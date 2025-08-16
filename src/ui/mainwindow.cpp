#include "ui/mainwindow.h"
#include "core/fsck_fixes.h"
#include "ui/filesystem_mount_dialog.h"
#include "ui/mainwindow_dialogs.h"
#include "ui/mainwindow_file_ops.h"
#include "ui_mainwindow.h"
#include <QCalendarWidget>
#include <QComboBox>
#include <QDateTime>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

// Forward declarations for the file operations and dialogs handlers
static std::unique_ptr<MainWindowFileOps> fileOps;
static std::unique_ptr<MainWindowDialogs> dialogHandler;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
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
    connect(ui->fileListWidget, &QListWidget::customContextMenuRequested, this,
            &MainWindow::on_fileListWidget_customContextMenuRequested);
    connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::on_searchButton_clicked);
    connect(ui->fsDetectButton, &QPushButton::clicked, this,
            &MainWindow::on_actionDetectFilesystems_triggered);
    connect(ui->actionFsCheck, &QAction::triggered, this, &MainWindow::on_actionFsCheck_triggered);
    connect(ui->actionSearch, &QAction::triggered, this, &MainWindow::on_actionSearch_triggered);
    connect(ui->actionQuotaManager, &QAction::triggered, this,
            &MainWindow::on_actionQuotaManager_triggered);
    connect(ui->actionSnapshots, &QAction::triggered, this,
            &MainWindow::on_actionSnapshots_triggered);
    connect(ui->actionTreeView, &QAction::triggered, this,
            &MainWindow::on_actionTreeView_triggered);
    connect(ui->actionDetect_Filesystems, &QAction::triggered, this,
            &MainWindow::on_actionDetectFilesystems_triggered);

    // Fix for QMetaObject::connectSlotsByName warning - manually connect the actions with different
    // names
    connect(ui->actionDetect_Filesystems, SIGNAL(triggered()), this,
            SLOT(on_actionDetectFilesystems_triggered()));

    // Initialize our modular components
    fsDetector = std::make_unique<FileSystemDetector>(this);
    treeViewManager = std::make_unique<TreeViewManager>(this, this);
    addDockWidget(Qt::LeftDockWidgetArea, treeViewManager->getDockWidget());
    connect(treeViewManager.get(), &TreeViewManager::directorySelected, this,
            &MainWindow::onDirectorySelected);

    // Initialize file operations and dialog handlers
    fileOps = std::make_unique<MainWindowFileOps>(this);
    dialogHandler = std::make_unique<MainWindowDialogs>(this);

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

MainWindow::~MainWindow() {
    if (fs) {
        fs->unmount();
    }
    delete ui;
}

void MainWindow::setupMenus() {
    // Already done in the UI file
}

void MainWindow::setupFsToolbar() {
    ui->fsToolBar->setIconSize(QSize(24, 24));
    ui->fsToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    // Setup Tools menu
    QMenu *toolsMenu = ui->menubar->addMenu("Tools");

    QAction *fsCheckAction = new QAction("Check Filesystem", this);
    connect(fsCheckAction, &QAction::triggered, this, &MainWindow::on_actionFsCheck_triggered);
    toolsMenu->addAction(fsCheckAction);

    QAction *fsCheckFixAction = new QAction("Check and Fix Filesystem", this);
    connect(fsCheckFixAction, &QAction::triggered, this,
            &MainWindow::on_actionFsCheckAndFix_triggered);
    toolsMenu->addAction(fsCheckFixAction);

    QAction *lostFoundAction = new QAction("Create lost+found Directory", this);
    connect(lostFoundAction, &QAction::triggered, this,
            &MainWindow::on_actionCreateLostFound_triggered);
    toolsMenu->addAction(lostFoundAction);

    toolsMenu->addSeparator();

    QAction *searchAction = new QAction("Advanced Search", this);
    connect(searchAction, &QAction::triggered, this, &MainWindow::on_actionSearch_triggered);
    toolsMenu->addAction(searchAction);

    QAction *quotaAction = new QAction("Quota Manager", this);
    connect(quotaAction, &QAction::triggered, this, &MainWindow::on_actionQuotaManager_triggered);
    toolsMenu->addAction(quotaAction);

    QAction *snapshotAction = new QAction("Snapshot Manager", this);
    connect(snapshotAction, &QAction::triggered, this, &MainWindow::on_actionSnapshots_triggered);
    toolsMenu->addAction(snapshotAction);
}

void MainWindow::refreshFileList() {
    fileOps->refreshFileList();
}

void MainWindow::refreshTreeView() {
    if (!fs || !treeViewManager)
        return;

    treeViewManager->refreshTreeView();
}

void MainWindow::on_formatButton_clicked() {
    if (!fs)
        return;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Format Confirmation",
        "Are you sure you want to format the filesystem? All data will be lost.",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        fs->format();
        QMessageBox::information(this, "Format", "Filesystem formatted successfully.");

        // Reset UI
        ui->fileContentTextEdit->clear();
        refreshFileList();
    }
}

void MainWindow::on_mountButton_clicked() {
    if (!fs)
        return;

    bool result = fs->mount();

    if (result) {
        QMessageBox::information(this, "Mount", "Filesystem mounted successfully.");

        // Enable UI elements
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
    } else {
        QMessageBox::critical(this, "Error", "Could not mount file system. Have you formatted it?");
    }
}

void MainWindow::on_fileListWidget_itemDoubleClicked(QListWidgetItem *item) {
    fileOps->fileDoubleClicked(item);
}

void MainWindow::on_saveButton_clicked() {
    fileOps->saveFile();
}

void MainWindow::on_mkdirButton_clicked() {
    fileOps->createDirectory();
}

void MainWindow::on_createFileButton_clicked() {
    fileOps->createFile();
}

void MainWindow::on_fileListWidget_customContextMenuRequested(const QPoint &pos) {
    fileOps->fileContextMenu(pos);
}

void MainWindow::on_actionFsCheck_triggered() {
    dialogHandler->handleFsCheck();
}

void MainWindow::on_actionSearch_triggered() {
    dialogHandler->handleAdvancedSearch();
}

void MainWindow::on_searchButton_clicked() {
    dialogHandler->handleQuickSearch();
}

void MainWindow::on_actionQuotaManager_triggered() {
    dialogHandler->handleQuotaManager();
}

void MainWindow::on_actionSnapshots_triggered() {
    dialogHandler->handleSnapshots();
}

void MainWindow::on_actionTreeView_triggered() {
    if (treeViewManager) {
        bool isVisible = treeViewManager->getDockWidget()->isVisible();
        treeViewManager->getDockWidget()->setVisible(!isVisible);
    }
}

void MainWindow::on_actionDetectFilesystems_triggered() {
    dialogHandler->handleFilesystemDetection();
}

void MainWindow::checkAvailableFilesystems() {
    if (fsDetector) {
        fsDetector->detectFilesystems();
    }
}

void MainWindow::updateAvailableFilesystemsList() {
    if (fsDetector) {
        availableFilesystems = fsDetector->detectFilesystems();
    }
}

void MainWindow::onDirectorySelected(const std::string &path) {
    if (!fs)
        return;

    current_open_file = "";
    refreshFileList();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    fileOps->handleDragEnterEvent(event);
}

void MainWindow::dropEvent(QDropEvent *event) {
    fileOps->handleDropEvent(event);
}

void MainWindow::updateStatusBar(const QString &message) {
    ui->statusbar->showMessage(message, 5000); // Show for 5 seconds
}

void MainWindow::on_actionFsCheckAndFix_triggered() {
    if (!fs) {
        QMessageBox::warning(this, "Error", "No filesystem is mounted.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Check and Fix Filesystem",
        "This will check the filesystem for errors and attempt to fix them. Continue?",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // Create a progress dialog
        QProgressDialog progress("Checking filesystem...", "Cancel", 0, 100, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();

        // Run the check
        fsck = std::make_unique<FileSystemCheck>(fs.get());
        std::vector<FsckIssue> issues = fsck->check();

        progress.setValue(50);

        // Fix issues if any were found
        if (!issues.empty()) {
            progress.setLabelText("Fixing filesystem issues...");

            // Fix all issues
            fsck->fix_all_issues();

            // Format report message
            QString report = QString("Fixed %1 filesystem issues:\n").arg(issues.size());
            for (const auto &issue : issues) {
                QString type;
                switch (issue.type) {
                    case FsckIssueType::INVALID_INODE:
                        type = "Invalid inode";
                        break;
                    case FsckIssueType::ORPHANED_INODE:
                        type = "Orphaned inode";
                        break;
                    case FsckIssueType::DUPLICATE_BLOCK:
                        type = "Duplicate block";
                        break;
                    case FsckIssueType::UNREFERENCED_BLOCK:
                        type = "Unreferenced block";
                        break;
                    case FsckIssueType::DIRECTORY_LOOP:
                        type = "Directory loop";
                        break;
                    case FsckIssueType::INCORRECT_LINK_COUNT:
                        type = "Incorrect link count";
                        break;
                    case FsckIssueType::INVALID_BLOCK_POINTER:
                        type = "Invalid block pointer";
                        break;
                }
                report +=
                    QString("- %1: %2\n").arg(type).arg(QString::fromStdString(issue.description));
            }

            progress.setValue(100);

            // Show the report
            QMessageBox::information(this, "Filesystem Fixed", report);
        } else {
            progress.setValue(100);
            QMessageBox::information(this, "Filesystem Check",
                                     "No issues found in the filesystem.");
        }
    }
}

void MainWindow::on_actionCreateLostFound_triggered() {
    if (!fs) {
        QMessageBox::warning(this, "Error", "No filesystem is mounted.");
        return;
    }

    int lostFoundInode = fs->create_lost_found();

    if (lostFoundInode != -1) {
        QMessageBox::information(this, "Success", "Created or verified lost+found directory.");
        refreshFileList();
    } else {
        QMessageBox::warning(this, "Error", "Failed to create lost+found directory.");
    }
}
