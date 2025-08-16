#include "ui/mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QListWidgetItem>
#include <QDir>
#include <QHeaderView>
#include <QDateTime>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QCalendarWidget>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QTableWidget>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
                                          ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    // Create filesystem in the current directory
    fs = std::make_unique<FileSystem>("my_virtual_disk.fs");
    
    // Force format the filesystem initially to ensure it exists
    fs->format();

    // Initialize UI components
    ui->fileListWidget->setEnabled(false);
    ui->fileContentTextEdit->setEnabled(false);
    ui->saveButton->setEnabled(false);
    ui->mkdirButton->setEnabled(false);
    ui->createFileButton->setEnabled(false);
    ui->searchLineEdit->setEnabled(false);
    ui->searchButton->setEnabled(false);
    ui->fsDetectButton->setEnabled(true);

    // Set up file list drag and drop
    ui->fileListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->fileListWidget->setDragEnabled(true);
    ui->fileListWidget->setAcceptDrops(true);
    ui->fileListWidget->setDropIndicatorShown(true);
    ui->fileListWidget->setDefaultDropAction(Qt::MoveAction);
    setAcceptDrops(true);

    // Connect signals and slots
    connect(ui->fileListWidget, &QListWidget::customContextMenuRequested, this, &MainWindow::on_fileListWidget_customContextMenuRequested);
    connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::on_searchButton_clicked);
    connect(ui->fsDetectButton, &QPushButton::clicked, this, &MainWindow::on_actionDetectFilesystems_triggered);
    connect(ui->actionFsCheck, &QAction::triggered, this, &MainWindow::on_actionFsCheck_triggered);
    connect(ui->actionSearch, &QAction::triggered, this, &MainWindow::on_actionSearch_triggered);
    connect(ui->actionQuotaManager, &QAction::triggered, this, &MainWindow::on_actionQuotaManager_triggered);
    connect(ui->actionSnapshots, &QAction::triggered, this, &MainWindow::on_actionSnapshots_triggered);
    connect(ui->actionTreeView, &QAction::triggered, this, &MainWindow::on_actionTreeView_triggered);
    connect(ui->actionDetect_Filesystems, &QAction::triggered, this, &MainWindow::on_actionDetectFilesystems_triggered);
    
    // Initialize tree view
    setupTreeView();
    
    // Setup filesystem detection timer
    fsDetectionTimer = new QTimer(this);
    connect(fsDetectionTimer, &QTimer::timeout, this, &MainWindow::checkAvailableFilesystems);
    fsDetectionTimer->start(10000); // Check every 10 seconds
    
    // Initial filesystem detection
    checkAvailableFilesystems();
    
    // Set up toolbar
    setupFsToolbar();
    
    // Set window properties
    setWindowTitle("FileSystem Explorer");
    resize(900, 700);
}

MainWindow::~MainWindow()
{
    if (fs) {
        fs->unmount();
    }
    delete ui;
}

void MainWindow::setupMenus()
{
    // Already done in the UI file
}

void MainWindow::setupFsToolbar()
{
    ui->fsToolBar->setIconSize(QSize(24, 24));
    ui->fsToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
}

void MainWindow::setupTreeView()
{
    // Create tree view as a dock widget
    treeDock = new QDockWidget("Directory Tree", this);
    treeDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    treeView = new QTreeView(treeDock);
    treeDock->setWidget(treeView);
    
    // Create the model
    directoryModel = new QStandardItemModel(this);
    directoryModel->setHorizontalHeaderLabels(QStringList() << "Name");
    
    // Configure tree view
    treeView->setModel(directoryModel);
    treeView->setHeaderHidden(false);
    treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    treeView->setDragEnabled(true);
    treeView->setAcceptDrops(true);
    treeView->setDropIndicatorShown(true);
    
    // Connect signals
    connect(treeView, &QTreeView::clicked, this, &MainWindow::on_treeView_clicked);
    
    // Add to main window (initially hidden)
    addDockWidget(Qt::LeftDockWidgetArea, treeDock);
    treeDock->hide();
}

void MainWindow::refreshFileList()
{
    ui->fileListWidget->clear();
    auto entries = fs->ls();

    QListWidgetItem *up_item = new QListWidgetItem("..");
    up_item->setData(Qt::UserRole, -1); // Special inode number for 'up'
    ui->fileListWidget->addItem(up_item);

    for (const auto &entry : entries)
    {
        if (std::string(entry.name) == "." || std::string(entry.name) == "..")
            continue;

        Inode inode = fs->get_inode(entry.inode_num);
        QString prefix = (inode.mode == 2) ? "[D] " : "[F] ";
        QListWidgetItem *item = new QListWidgetItem(prefix + QString::fromStdString(entry.name));
        item->setData(Qt::UserRole, entry.inode_num);
        
        // Set icon based on file type for better visualization
        if (inode.mode == 2) { // Directory
            item->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
        } else {
            item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
        }
        
        ui->fileListWidget->addItem(item);
    }
    
    // Also refresh tree view if it's active
    if (treeDock && treeDock->isVisible()) {
        refreshTreeView();
    }
}

void MainWindow::refreshTreeView()
{
    if (!fs) return;
    
    // Save the current directory path
    std::string current_path = "/";
    
    // Clear the tree
    directoryModel->clear();
    directoryModel->setHorizontalHeaderLabels(QStringList() << "Name");
    
    // Create root item
    QStandardItem *rootItem = new QStandardItem(QIcon(style()->standardIcon(QStyle::SP_DirIcon)), "/");
    rootItem->setData(0, Qt::UserRole); // Root inode is always 0
    directoryModel->appendRow(rootItem);
    
    // Build tree recursively - starting from root
    buildDirectoryTree(rootItem, 0, "/");
    
    // Expand root by default
    treeView->expand(directoryModel->index(0, 0));
}

void MainWindow::buildDirectoryTree(QStandardItem *parentItem, int parent_inode, const std::string &parent_path)
{
    if (!fs) return;
    
    // Restore original directory
    int original_dir_inode = fs->find_inode_by_path(".");
    
    // Get entries
    auto entries = fs->ls();
    
    for (const auto &entry : entries)
    {
        if (std::string(entry.name) == "." || std::string(entry.name) == "..")
            continue;
            
        Inode inode = fs->get_inode(entry.inode_num);
        
        // Only add directories to the tree
        if (inode.mode == 2) {
            QStandardItem *item = new QStandardItem(QIcon(style()->standardIcon(QStyle::SP_DirIcon)), 
                                                    QString::fromStdString(entry.name));
            item->setData(entry.inode_num, Qt::UserRole);
            parentItem->appendRow(item);
            
            // Recursively build subdirectories
            std::string full_path = parent_path;
            if (parent_path != "/") full_path += "/";
            full_path += entry.name;
            
            buildDirectoryTree(item, entry.inode_num, full_path);
        }
    }
    
    // Restore original directory
    fs->cd(".");
}

void MainWindow::on_formatButton_clicked()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::warning(this, "Format Disk", "Are you sure you want to format the disk? All data will be lost.",
                                 QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
    {
        fs->format();
        QMessageBox::information(this, "Success", "Virtual disk formatted.");
        refreshFileList();
    }
}

void MainWindow::on_mountButton_clicked()
{
    if (fs->mount())
    {
        QMessageBox::information(this, "Success", "File system mounted.");
        ui->fileListWidget->setEnabled(true);
        ui->fileContentTextEdit->setEnabled(true);
        ui->saveButton->setEnabled(true);
        ui->mkdirButton->setEnabled(true);
        ui->createFileButton->setEnabled(true);
        ui->searchLineEdit->setEnabled(true);
        ui->searchButton->setEnabled(true);
        refreshFileList();
        
        // Initialize the utility classes
        fsck = std::make_unique<FileSystemCheck>(fs.get());
        search = std::make_unique<FileSystemSearch>(fs.get());
        quotaManager = std::make_unique<QuotaManager>(fs.get());
        snapshotManager = std::make_unique<SnapshotManager>(fs.get());
    }
    else
    {
        QMessageBox::critical(this, "Error", "Could not mount file system. Have you formatted it?");
    }
}

void MainWindow::on_fileListWidget_itemDoubleClicked(QListWidgetItem *item)
{
    int inode_num = item->data(Qt::UserRole).toInt();

    if (inode_num == -1)
    {
        fs->cd("..");
        refreshFileList();
        return;
    }

    Inode inode = fs->get_inode(inode_num);
    std::string name = item->text().mid(4).toStdString(); // remove prefix

    if (inode.mode == 2)
    {
        fs->cd(name);
        refreshFileList();
    }
    else
    {
        current_open_file = name;
        std::string content = fs->read(name);
        ui->fileContentTextEdit->setPlainText(QString::fromStdString(content));
    }
}

void MainWindow::on_saveButton_clicked()
{
    if (!current_open_file.empty())
    {
        std::string content = ui->fileContentTextEdit->toPlainText().toStdString();
        fs->write(current_open_file, content);
        QMessageBox::information(this, "Success", "File saved.");
    }
}

void MainWindow::on_mkdirButton_clicked()
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Create Directory"),
                                         tr("Directory name:"), QLineEdit::Normal,
                                         "", &ok);
    if (ok && !text.isEmpty())
    {
        fs->mkdir(text.toStdString());
        refreshFileList();
    }
}

void MainWindow::on_createFileButton_clicked()
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Create File"),
                                         tr("File name:"), QLineEdit::Normal,
                                         "", &ok);
    if (ok && !text.isEmpty())
    {
        fs->create(text.toStdString());
        refreshFileList();
    }
}

void MainWindow::on_fileListWidget_customContextMenuRequested(const QPoint &pos)
{
    QListWidgetItem* item = ui->fileListWidget->itemAt(pos);
    if (!item) return;

    int inode_num = item->data(Qt::UserRole).toInt();
    if (inode_num == -1) return; // No context menu for ".."

    QMenu contextMenu(this);
    QAction *deleteAction = contextMenu.addAction("Delete");
    QAction *renameAction = contextMenu.addAction("Rename");
    QAction *propertiesAction = contextMenu.addAction("Properties");

    connect(deleteAction, &QAction::triggered, [this, item]() {
        std::string name = item->text().mid(4).toStdString();
        fs->unlink(name);
        refreshFileList();
    });

    connect(renameAction, &QAction::triggered, [this, item]() {
        // Simple rename, does not move files yet
        bool ok;
        QString newName = QInputDialog::getText(this, "Rename", "New name:", QLineEdit::Normal, item->text().mid(4), &ok);
        if (ok && !newName.isEmpty()) {
            // This is a simplified rename. A real implementation would need a `rename` method in the filesystem
            // that can also move files between directories.
            std::string oldName = item->text().mid(4).toStdString();
            fs->unlink(oldName); // Unlink old name
            // Re-create with new name. This is not an atomic rename.
            // For hard links, we would just add a new directory entry and remove the old one.
            // For files, we'd need to decide if we are creating a new file or just renaming the link.
            // This is a placeholder for a proper rename implementation.
            fs->create(newName.toStdString()); // This is a simplification
            refreshFileList();
        }
    });

    connect(propertiesAction, &QAction::triggered, [this, inode_num]() {
        Inode inode = fs->get_inode(inode_num);
        QString props;
        props += "Inode: " + QString::number(inode_num) + "\n";
        props += "Size: " + QString::number(inode.size) + " bytes\n";
        props += "Links: " + QString::number(inode.link_count) + "\n";
        props += "Mode: " + QString::number(inode.mode, 8) + "\n"; // Octal
        props += "UID: " + QString::number(inode.uid) + "\n";
        props += "GID: " + QString::number(inode.gid) + "\n";
        
        char time_buf[80];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&inode.creation_time));
        props += "Created: " + QString(time_buf) + "\n";
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&inode.modification_time));
        props += "Modified: " + QString(time_buf) + "\n";
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&inode.access_time));
        props += "Accessed: " + QString(time_buf) + "\n";

        QMessageBox::information(this, "Properties", props);
    });

    contextMenu.exec(ui->fileListWidget->mapToGlobal(pos));
}

void MainWindow::checkAvailableFilesystems()
{
    // In a real implementation, this would scan the system for available filesystem images
    // For this demo, we'll look for *.fs files in the current directory
    QDir dir(".");
    QStringList filters;
    filters << "*.fs";
    QStringList fileList = dir.entryList(filters, QDir::Files);
    
    // Update only if the list has changed
    if (fileList != availableFilesystems) {
        availableFilesystems = fileList;
        updateAvailableFilesystemsList();
    }
}

void MainWindow::updateAvailableFilesystemsList()
{
    // If we have detected new filesystems, show a notification
    if (!availableFilesystems.isEmpty() && fs && !fs->mount()) {
        QMessageBox::information(this, "Filesystems Detected", 
                               "New filesystem images have been detected.\n"
                               "Would you like to mount one of them?",
                               QMessageBox::Yes | QMessageBox::No);
    }
}

void MainWindow::on_actionDetectFilesystems_triggered()
{
    checkAvailableFilesystems();
    
    if (availableFilesystems.isEmpty()) {
        QMessageBox::information(this, "No Filesystems Found", 
                               "No filesystem images (*.fs) were found in the current directory.");
        return;
    }
    
    // Show dialog to select filesystem
    bool ok;
    QString item = QInputDialog::getItem(this, "Select Filesystem", 
                                       "Available Filesystems:", 
                                       availableFilesystems, 0, false, &ok);
    if (ok && !item.isEmpty()) {
        // Unmount current if mounted
        if (fs) {
            fs->unmount();
        }
        
        // Create new filesystem with selected image
        fs = std::make_unique<FileSystem>(item.toStdString());
        
        // Try to mount
        if (fs->mount()) {
            QMessageBox::information(this, "Success", "File system mounted.");
            ui->fileListWidget->setEnabled(true);
            ui->fileContentTextEdit->setEnabled(true);
            ui->saveButton->setEnabled(true);
            ui->mkdirButton->setEnabled(true);
            ui->createFileButton->setEnabled(true);
            ui->searchLineEdit->setEnabled(true);
            ui->searchButton->setEnabled(true);
            refreshFileList();
            
            // Initialize the utility classes after mounting
            fsck = std::make_unique<FileSystemCheck>(fs.get());
            search = std::make_unique<FileSystemSearch>(fs.get());
            quotaManager = std::make_unique<QuotaManager>(fs.get());
            snapshotManager = std::make_unique<SnapshotManager>(fs.get());
        } else {
            QMessageBox::critical(this, "Error", "Could not mount file system. It may be corrupted or not formatted.");
        }
    }
}

void MainWindow::on_actionFsCheck_triggered()
{
    if (!fs || !fsck) {
        QMessageBox::warning(this, "Not Available", "File system must be mounted first.");
        return;
    }
    
    // Run filesystem check
    auto issues = fsck->check();
    
    if (issues.empty()) {
        QMessageBox::information(this, "Filesystem Check", "No issues found. Filesystem is clean.");
        return;
    }
    
    // Create a dialog to display issues
    QDialog dialog(this);
    dialog.setWindowTitle("Filesystem Check Results");
    dialog.setMinimumSize(600, 400);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QLabel *label = new QLabel(QString("Found %1 issues:").arg(issues.size()));
    layout->addWidget(label);
    
    // Create a table to display issues
    QTableWidget *table = new QTableWidget(&dialog);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels(QStringList() << "Type" << "Inode" << "Block" << "Description" << "Fixable");
    table->setRowCount(issues.size());
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    
    for (int i = 0; i < issues.size(); i++) {
        const auto &issue = issues[i];
        
        // Convert issue type to string
        QString typeStr;
        switch (issue.type) {
            case FsckIssueType::INVALID_INODE: typeStr = "Invalid Inode"; break;
            case FsckIssueType::ORPHANED_INODE: typeStr = "Orphaned Inode"; break;
            case FsckIssueType::DUPLICATE_BLOCK: typeStr = "Duplicate Block"; break;
            case FsckIssueType::UNREFERENCED_BLOCK: typeStr = "Unreferenced Block"; break;
            case FsckIssueType::DIRECTORY_LOOP: typeStr = "Directory Loop"; break;
            case FsckIssueType::INCORRECT_LINK_COUNT: typeStr = "Incorrect Link Count"; break;
            case FsckIssueType::INVALID_BLOCK_POINTER: typeStr = "Invalid Block Pointer"; break;
            default: typeStr = "Unknown Issue"; break;
        }
        
        table->setItem(i, 0, new QTableWidgetItem(typeStr));
        table->setItem(i, 1, new QTableWidgetItem(QString::number(issue.inode_num)));
        table->setItem(i, 2, new QTableWidgetItem(QString::number(issue.block_num)));
        table->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(issue.description)));
        table->setItem(i, 4, new QTableWidgetItem(issue.can_fix ? "Yes" : "No"));
    }
    
    layout->addWidget(table);
    
    // Add buttons to fix issues
    QDialogButtonBox *buttonBox = new QDialogButtonBox(&dialog);
    QPushButton *fixAllButton = new QPushButton("Fix All Issues");
    QPushButton *closeButton = new QPushButton("Close");
    buttonBox->addButton(fixAllButton, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(closeButton, QDialogButtonBox::RejectRole);
    
    layout->addWidget(buttonBox);
    
    connect(fixAllButton, &QPushButton::clicked, [&]() {
        int fixable = 0;
        for (const auto &issue : issues) {
            if (issue.can_fix) fixable++;
        }
        
        if (fixable == 0) {
            QMessageBox::information(&dialog, "No Fixable Issues", "None of the issues can be automatically fixed.");
            return;
        }
        
        QMessageBox::StandardButton reply = QMessageBox::question(&dialog, "Fix Issues", 
                            QString("Fix all %1 fixable issues?").arg(fixable),
                            QMessageBox::Yes | QMessageBox::No);
                            
        if (reply == QMessageBox::Yes) {
            fsck->fix_all_issues();
            QMessageBox::information(&dialog, "Fix Complete", "All fixable issues have been addressed.");
            dialog.accept();
        }
    });
    
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    dialog.exec();
}

void MainWindow::on_actionSearch_triggered()
{
    if (!fs || !search) {
        QMessageBox::warning(this, "Not Available", "File system must be mounted first.");
        return;
    }
    
    // Create advanced search dialog
    QDialog dialog(this);
    dialog.setWindowTitle("Advanced Search");
    dialog.setMinimumWidth(400);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    // Name filter
    QHBoxLayout *nameLayout = new QHBoxLayout();
    QLabel *nameLabel = new QLabel("Name contains:");
    QLineEdit *nameEdit = new QLineEdit();
    nameLayout->addWidget(nameLabel);
    nameLayout->addWidget(nameEdit);
    layout->addLayout(nameLayout);
    
    // Size filters
    QHBoxLayout *sizeLayout = new QHBoxLayout();
    QLabel *sizeMinLabel = new QLabel("Min size (bytes):");
    QSpinBox *sizeMinBox = new QSpinBox();
    sizeMinBox->setRange(0, 1000000);
    sizeMinBox->setValue(0);
    
    QLabel *sizeMaxLabel = new QLabel("Max size (bytes):");
    QSpinBox *sizeMaxBox = new QSpinBox();
    sizeMaxBox->setRange(0, 1000000);
    sizeMaxBox->setValue(0);
    sizeMaxBox->setSpecialValueText("Any");
    
    sizeLayout->addWidget(sizeMinLabel);
    sizeLayout->addWidget(sizeMinBox);
    sizeLayout->addWidget(sizeMaxLabel);
    sizeLayout->addWidget(sizeMaxBox);
    layout->addLayout(sizeLayout);
    
    // File type
    QHBoxLayout *typeLayout = new QHBoxLayout();
    QLabel *typeLabel = new QLabel("File type:");
    QComboBox *typeBox = new QComboBox();
    typeBox->addItems(QStringList() << "Any" << "File" << "Directory" << "Symlink");
    typeLayout->addWidget(typeLabel);
    typeLayout->addWidget(typeBox);
    layout->addLayout(typeLayout);
    
    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        // Clear previous search criteria
        search->clear_criteria();
        
        // Apply search criteria
        if (!nameEdit->text().isEmpty()) {
            search->add_name_criteria(nameEdit->text().toStdString());
        }
        
        if (sizeMinBox->value() > 0) {
            search->add_size_greater_than(sizeMinBox->value());
        }
        
        if (sizeMaxBox->value() > 0) {
            search->add_size_less_than(sizeMaxBox->value());
        }
        
        if (typeBox->currentIndex() > 0) {
            QString type;
            switch (typeBox->currentIndex()) {
                case 1: type = "file"; break;
                case 2: type = "dir"; break;
                case 3: type = "symlink"; break;
            }
            search->add_file_type(type.toStdString());
        }
        
        // Execute search
        auto results = search->search();
        
        if (results.empty()) {
            QMessageBox::information(this, "Search Results", "No matching files or directories found.");
            return;
        }
        
        // Show results in a dialog
        QDialog resultsDialog(this);
        resultsDialog.setWindowTitle("Search Results");
        resultsDialog.setMinimumSize(500, 300);
        
        QVBoxLayout *resultsLayout = new QVBoxLayout(&resultsDialog);
        
        QLabel *resultsLabel = new QLabel(QString("Found %1 results:").arg(results.size()));
        resultsLayout->addWidget(resultsLabel);
        
        QListWidget *resultsList = new QListWidget(&resultsDialog);
        for (const auto &result : results) {
            QString icon_prefix = result.is_dir ? "[D] " : "[F] ";
            QListWidgetItem *item = new QListWidgetItem(icon_prefix + QString::fromStdString(result.path));
            if (result.is_dir) {
                item->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
            } else {
                item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
            }
            resultsList->addItem(item);
        }
        
        resultsLayout->addWidget(resultsList);
        
        QPushButton *closeButton = new QPushButton("Close");
        resultsLayout->addWidget(closeButton);
        connect(closeButton, &QPushButton::clicked, &resultsDialog, &QDialog::accept);
        
        resultsDialog.exec();
    }
}

void MainWindow::on_searchButton_clicked()
{
    if (!fs || !search) {
        QMessageBox::warning(this, "Not Available", "File system must be mounted first.");
        return;
    }
    
    QString searchText = ui->searchLineEdit->text().trimmed();
    if (searchText.isEmpty()) {
        return;
    }
    
    // Quick search by name only
    search->clear_criteria();
    search->add_name_criteria(searchText.toStdString());
    
    auto results = search->search();
    
    if (results.empty()) {
        QMessageBox::information(this, "Search Results", "No matching files or directories found.");
        return;
    }
    
    // Show results in a dialog
    QDialog resultsDialog(this);
    resultsDialog.setWindowTitle("Search Results");
    resultsDialog.setMinimumSize(500, 300);
    
    QVBoxLayout *resultsLayout = new QVBoxLayout(&resultsDialog);
    
    QLabel *resultsLabel = new QLabel(QString("Found %1 results:").arg(results.size()));
    resultsLayout->addWidget(resultsLabel);
    
    QListWidget *resultsList = new QListWidget(&resultsDialog);
    for (const auto &result : results) {
        QString icon_prefix = result.is_dir ? "[D] " : "[F] ";
        QListWidgetItem *item = new QListWidgetItem(icon_prefix + QString::fromStdString(result.path));
        if (result.is_dir) {
            item->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
        } else {
            item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
        }
        resultsList->addItem(item);
    }
    
    resultsLayout->addWidget(resultsList);
    
    QPushButton *closeButton = new QPushButton("Close");
    resultsLayout->addWidget(closeButton);
    connect(closeButton, &QPushButton::clicked, &resultsDialog, &QDialog::accept);
    
    resultsDialog.exec();
}

void MainWindow::on_actionQuotaManager_triggered()
{
    if (!fs || !quotaManager) {
        QMessageBox::warning(this, "Not Available", "File system must be mounted first.");
        return;
    }
    
    // Create quota management dialog
    QDialog dialog(this);
    dialog.setWindowTitle("Quota Manager");
    dialog.setMinimumSize(600, 400);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    // Create tab widget for user and group quotas
    QTabWidget *tabWidget = new QTabWidget(&dialog);
    
    // User quotas tab
    QWidget *userTab = new QWidget();
    QVBoxLayout *userLayout = new QVBoxLayout(userTab);
    
    QTableWidget *userTable = new QTableWidget(userTab);
    userTable->setColumnCount(7);
    userTable->setHorizontalHeaderLabels(QStringList() 
        << "UID" << "Blocks Used" << "Blocks Soft Limit" << "Blocks Hard Limit" 
        << "Inodes Used" << "Inodes Soft Limit" << "Inodes Hard Limit");
    userTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    userLayout->addWidget(userTable);
    
    // Add user quota buttons
    QHBoxLayout *userButtonLayout = new QHBoxLayout();
    QPushButton *addUserButton = new QPushButton("Add User Quota");
    QPushButton *editUserButton = new QPushButton("Edit User Quota");
    QPushButton *removeUserButton = new QPushButton("Remove User Quota");
    userButtonLayout->addWidget(addUserButton);
    userButtonLayout->addWidget(editUserButton);
    userButtonLayout->addWidget(removeUserButton);
    userLayout->addLayout(userButtonLayout);
    
    // Group quotas tab
    QWidget *groupTab = new QWidget();
    QVBoxLayout *groupLayout = new QVBoxLayout(groupTab);
    
    QTableWidget *groupTable = new QTableWidget(groupTab);
    groupTable->setColumnCount(7);
    groupTable->setHorizontalHeaderLabels(QStringList() 
        << "GID" << "Blocks Used" << "Blocks Soft Limit" << "Blocks Hard Limit" 
        << "Inodes Used" << "Inodes Soft Limit" << "Inodes Hard Limit");
    groupTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    groupLayout->addWidget(groupTable);
    
    // Add group quota buttons
    QHBoxLayout *groupButtonLayout = new QHBoxLayout();
    QPushButton *addGroupButton = new QPushButton("Add Group Quota");
    QPushButton *editGroupButton = new QPushButton("Edit Group Quota");
    QPushButton *removeGroupButton = new QPushButton("Remove Group Quota");
    groupButtonLayout->addWidget(addGroupButton);
    groupButtonLayout->addWidget(editGroupButton);
    groupButtonLayout->addWidget(removeGroupButton);
    groupLayout->addLayout(groupButtonLayout);
    
    tabWidget->addTab(userTab, "User Quotas");
    tabWidget->addTab(groupTab, "Group Quotas");
    
    layout->addWidget(tabWidget);
    
    // Add close button
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    // Add user quota
    connect(addUserButton, &QPushButton::clicked, [&]() {
        bool ok;
        int uid = QInputDialog::getInt(&dialog, "Add User Quota", "User ID (UID):", 0, 0, 99999, 1, &ok);
        if (!ok) return;
        
        QDialog quotaDialog(&dialog);
        quotaDialog.setWindowTitle("Set Quota for User " + QString::number(uid));
        QVBoxLayout *quotaLayout = new QVBoxLayout(&quotaDialog);
        
        QFormLayout *formLayout = new QFormLayout();
        QSpinBox *blocksSoft = new QSpinBox();
        blocksSoft->setRange(0, 999999);
        blocksSoft->setValue(0);
        blocksSoft->setSuffix(" blocks");
        
        QSpinBox *blocksHard = new QSpinBox();
        blocksHard->setRange(0, 999999);
        blocksHard->setValue(0);
        blocksHard->setSuffix(" blocks");
        
        QSpinBox *inodesSoft = new QSpinBox();
        inodesSoft->setRange(0, 999999);
        inodesSoft->setValue(0);
        inodesSoft->setSuffix(" inodes");
        
        QSpinBox *inodesHard = new QSpinBox();
        inodesHard->setRange(0, 999999);
        inodesHard->setValue(0);
        inodesHard->setSuffix(" inodes");
        
        formLayout->addRow("Blocks Soft Limit:", blocksSoft);
        formLayout->addRow("Blocks Hard Limit:", blocksHard);
        formLayout->addRow("Inodes Soft Limit:", inodesSoft);
        formLayout->addRow("Inodes Hard Limit:", inodesHard);
        
        quotaLayout->addLayout(formLayout);
        
        QDialogButtonBox *quotaButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        quotaLayout->addWidget(quotaButtons);
        
        connect(quotaButtons, &QDialogButtonBox::accepted, &quotaDialog, &QDialog::accept);
        connect(quotaButtons, &QDialogButtonBox::rejected, &quotaDialog, &QDialog::reject);
        
        if (quotaDialog.exec() == QDialog::Accepted) {
            quotaManager->set_user_quota(uid, blocksSoft->value(), blocksHard->value(), 
                                         inodesSoft->value(), inodesHard->value());
            
            // Update table
            QuotaEntry quota = quotaManager->get_user_quota(uid);
            int row = userTable->rowCount();
            userTable->insertRow(row);
            userTable->setItem(row, 0, new QTableWidgetItem(QString::number(uid)));
            userTable->setItem(row, 1, new QTableWidgetItem(QString::number(quota.blocks_used)));
            userTable->setItem(row, 2, new QTableWidgetItem(QString::number(quota.blocks_soft_limit)));
            userTable->setItem(row, 3, new QTableWidgetItem(QString::number(quota.blocks_hard_limit)));
            userTable->setItem(row, 4, new QTableWidgetItem(QString::number(quota.inodes_used)));
            userTable->setItem(row, 5, new QTableWidgetItem(QString::number(quota.inodes_soft_limit)));
            userTable->setItem(row, 6, new QTableWidgetItem(QString::number(quota.inodes_hard_limit)));
        }
    });
    
    // Similar handlers could be added for group quotas and edit/remove functions
    
    dialog.exec();
}

void MainWindow::on_actionSnapshots_triggered()
{
    if (!fs || !snapshotManager) {
        QMessageBox::warning(this, "Not Available", "File system must be mounted first.");
        return;
    }
    
    // Create snapshots dialog
    QDialog dialog(this);
    dialog.setWindowTitle("Snapshot Manager");
    dialog.setMinimumSize(500, 400);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QLabel *label = new QLabel("Available Snapshots:");
    layout->addWidget(label);
    
    // Create table for snapshots
    QTableWidget *table = new QTableWidget(&dialog);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels(QStringList() << "Name" << "Created" << "Size (blocks)");
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(table);
    
    // Fill table with available snapshots
    auto snapshots = snapshotManager->list_snapshots();
    table->setRowCount(snapshots.size());
    
    for (int i = 0; i < snapshots.size(); i++) {
        const auto &snapshot = snapshots[i];
        table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(snapshot.name)));
        
        // Format timestamp
        QDateTime timestamp;
        timestamp.setSecsSinceEpoch(snapshot.creation_time);
        table->setItem(i, 1, new QTableWidgetItem(timestamp.toString("yyyy-MM-dd hh:mm:ss")));
        
        table->setItem(i, 2, new QTableWidgetItem(QString::number(snapshot.blocks_used)));
    }
    
    // Add buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *createButton = new QPushButton("Create Snapshot");
    QPushButton *restoreButton = new QPushButton("Restore Snapshot");
    QPushButton *deleteButton = new QPushButton("Delete Snapshot");
    QPushButton *closeButton = new QPushButton("Close");
    
    buttonLayout->addWidget(createButton);
    buttonLayout->addWidget(restoreButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addWidget(closeButton);
    
    layout->addLayout(buttonLayout);
    
    // Connect signals
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    
    connect(createButton, &QPushButton::clicked, [&]() {
        bool ok;
        QString name = QInputDialog::getText(&dialog, "Create Snapshot", 
                                           "Snapshot Name:", QLineEdit::Normal, 
                                           "snapshot_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"), 
                                           &ok);
        if (ok && !name.isEmpty()) {
            if (snapshotManager->create_snapshot(name.toStdString())) {
                QMessageBox::information(&dialog, "Success", "Snapshot created successfully.");
                
                // Refresh the table
                auto snapshots = snapshotManager->list_snapshots();
                table->setRowCount(snapshots.size());
                
                for (int i = 0; i < snapshots.size(); i++) {
                    const auto &snapshot = snapshots[i];
                    table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(snapshot.name)));
                    
                    // Format timestamp
                    QDateTime timestamp;
                    timestamp.setSecsSinceEpoch(snapshot.creation_time);
                    table->setItem(i, 1, new QTableWidgetItem(timestamp.toString("yyyy-MM-dd hh:mm:ss")));
                    
                    table->setItem(i, 2, new QTableWidgetItem(QString::number(snapshot.blocks_used)));
                }
            } else {
                QMessageBox::critical(&dialog, "Error", "Failed to create snapshot.");
            }
        }
    });
    
    connect(restoreButton, &QPushButton::clicked, [&]() {
        QModelIndexList selection = table->selectionModel()->selectedRows();
        if (selection.isEmpty()) {
            QMessageBox::warning(&dialog, "No Selection", "Please select a snapshot to restore.");
            return;
        }
        
        int row = selection.first().row();
        QString name = table->item(row, 0)->text();
        
        QMessageBox::StandardButton reply = QMessageBox::question(&dialog, "Confirm Restore", 
                                          "Are you sure you want to restore the snapshot '" + name + "'?\n"
                                          "This will replace the current filesystem state.",
                                          QMessageBox::Yes | QMessageBox::No);
                                          
        if (reply == QMessageBox::Yes) {
            if (snapshotManager->restore_snapshot(name.toStdString())) {
                QMessageBox::information(&dialog, "Success", "Snapshot restored successfully.");
                dialog.accept();
                refreshFileList(); // Refresh the main window file list
            } else {
                QMessageBox::critical(&dialog, "Error", "Failed to restore snapshot.");
            }
        }
    });
    
    connect(deleteButton, &QPushButton::clicked, [&]() {
        QModelIndexList selection = table->selectionModel()->selectedRows();
        if (selection.isEmpty()) {
            QMessageBox::warning(&dialog, "No Selection", "Please select a snapshot to delete.");
            return;
        }
        
        int row = selection.first().row();
        QString name = table->item(row, 0)->text();
        
        QMessageBox::StandardButton reply = QMessageBox::question(&dialog, "Confirm Delete", 
                                          "Are you sure you want to delete the snapshot '" + name + "'?",
                                          QMessageBox::Yes | QMessageBox::No);
                                          
        if (reply == QMessageBox::Yes) {
            if (snapshotManager->delete_snapshot(name.toStdString())) {
                QMessageBox::information(&dialog, "Success", "Snapshot deleted successfully.");
                
                // Remove the row from the table
                table->removeRow(row);
            } else {
                QMessageBox::critical(&dialog, "Error", "Failed to delete snapshot.");
            }
        }
    });
    
    dialog.exec();
}

void MainWindow::on_actionTreeView_triggered()
{
    // Toggle tree view visibility
    if (treeDock->isVisible()) {
        treeDock->hide();
    } else {
        treeDock->show();
        refreshTreeView();
    }
}

void MainWindow::on_treeView_clicked(const QModelIndex &index)
{
    if (!fs) return;
    
    // Get the inode number from the item data
    QStandardItem *item = directoryModel->itemFromIndex(index);
    if (!item) return;
    
    int inode_num = item->data(Qt::UserRole).toInt();
    
    // Build the full path by walking up the tree
    QString path;
    QStandardItem *current = item;
    while (current) {
        if (current->parent() == nullptr) {
            // Root item
            path = "/" + path;
            break;
        } else {
            if (!path.isEmpty()) {
                path = "/" + path;
            }
            path = current->text() + path;
            current = current->parent();
        }
    }
    
    // Navigate to this directory
    if (path == "/") {
        fs->cd("/");
    } else {
        fs->cd(path.toStdString());
    }
    
    // Refresh the file list to show the contents of this directory
    refreshFileList();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!fs) return;
    
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        
        for (const QUrl &url : urlList) {
            // Only import local files
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                QFileInfo fileInfo(filePath);
                
                if (fileInfo.isFile()) {
                    // Import the file into our filesystem
                    QFile file(filePath);
                    if (file.open(QIODevice::ReadOnly)) {
                        QByteArray fileData = file.readAll();
                        file.close();
                        
                        // Create the file in our filesystem
                        fs->create(fileInfo.fileName().toStdString());
                        fs->write(fileInfo.fileName().toStdString(), std::string(fileData.constData(), fileData.size()));
                    }
                }
                // Could add directory import support here
            }
        }
        
        // Refresh to show new files
        refreshFileList();
    }
}
