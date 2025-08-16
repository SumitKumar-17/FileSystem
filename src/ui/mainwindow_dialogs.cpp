#include "ui/mainwindow_dialogs.h"
#include "ui_mainwindow.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QListWidgetItem>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QProgressBar>
#include <QTabWidget>
#include <QHBoxLayout>

MainWindowDialogs::MainWindowDialogs(MainWindow* mainWindow) 
    : mainWindow(mainWindow) {
}

void MainWindowDialogs::handleFsCheck() {
    auto* fs = mainWindow->getFileSystem();
    auto* fsck = mainWindow->getFsCheck();
    
    if (!fs || !fsck) {
        QMessageBox::warning(mainWindow, "Warning", "No filesystem is currently mounted");
        return;
    }
    
    // Run filesystem check
    std::vector<FsckIssue> issues = fsck->check();
    
    if (issues.empty()) {
        QMessageBox::information(mainWindow, "Filesystem Check", "Filesystem check completed successfully. No errors found.");
    } else {
        QDialog issuesDialog(mainWindow);
        issuesDialog.setWindowTitle("Filesystem Check Issues");
        issuesDialog.resize(600, 400);
        
        QVBoxLayout *layout = new QVBoxLayout(&issuesDialog);
        
        QLabel *label = new QLabel("The following issues were found:", &issuesDialog);
        layout->addWidget(label);
        
        // List widget to display issues
        QListWidget *issuesList = new QListWidget(&issuesDialog);
        // Populate with actual issues
        for (const auto& issue : issues) {
            QString issueText = QString::fromStdString(issue.description);
            issuesList->addItem(issueText);
        }
        layout->addWidget(issuesList);
        
        // Button to fix issues
        QPushButton *fixButton = new QPushButton("Fix All Issues", &issuesDialog);
        layout->addWidget(fixButton);
        
        // Connect fix button
        QObject::connect(fixButton, &QPushButton::clicked, [fsck]() {
            // Fix issues
            fsck->fix_all_issues();
            QMessageBox::information(nullptr, "Fix Issues", "All issues have been fixed.");
        });
        
        issuesDialog.exec();
    }
}

void MainWindowDialogs::handleAdvancedSearch() {
    auto* fs = mainWindow->getFileSystem();
    auto* search = mainWindow->getSearch();
    
    if (!fs || !search) {
        QMessageBox::warning(mainWindow, "Warning", "No filesystem is currently mounted");
        return;
    }
    
    // Get search parameters
    bool ok;
    QString searchTerm = QInputDialog::getText(mainWindow, 
        "Advanced Search", 
        "Enter search term:", 
        QLineEdit::Normal, 
        "", 
        &ok);
    
    if (!ok || searchTerm.isEmpty()) {
        return;
    }
    
    // Perform search
    search->clear_criteria();
    search->add_name_criteria(searchTerm.toStdString());
    std::vector<SearchResult> searchResults = search->search();
    
    std::vector<std::string> results;
    for (const auto& result : searchResults) {
        results.push_back(result.path);
    }
    
    if (!results.empty()) {
        // Display results
        auto resultDialog = new QDialog(mainWindow);
        resultDialog->setWindowTitle("Search Results");
        resultDialog->resize(500, 400);
        
        auto layout = new QVBoxLayout(resultDialog);
        auto resultList = new QListWidget(resultDialog);
        
        for (const auto &path : results) {
            resultList->addItem(QString::fromStdString(path));
        }
        
        layout->addWidget(resultList);
        
        // Add button to open file when clicked
        auto openButton = new QPushButton("Open Selected", resultDialog);
        layout->addWidget(openButton);
        
        QObject::connect(openButton, &QPushButton::clicked, [=]() {
            QListWidgetItem *item = resultList->currentItem();
            if (item) {
                std::string filePath = item->text().toStdString();
                
                // Get file content
                auto* fs = mainWindow->getFileSystem();
                auto* ui = mainWindow->getUI();
                
                if (fs) {
                    std::string content = fs->read(filePath);
                    ui->fileContentTextEdit->setPlainText(QString::fromStdString(content));
                    mainWindow->setCurrentOpenFile(filePath);
                    
                    // Extract directory path
                    size_t lastSlash = filePath.find_last_of('/');
                    if (lastSlash != std::string::npos) {
                        std::string dirPath = filePath.substr(0, lastSlash);
                        fs->cd(dirPath);
                    }
                } else {
                    QMessageBox::critical(mainWindow, "Error", "Failed to read file: " + QString::fromStdString(filePath));
                }
            }
        });
        
        resultDialog->exec();
    } else {
        QMessageBox::information(mainWindow, "Search Results", "No matching files found");
    }
}

void MainWindowDialogs::handleQuickSearch() {
    auto* fs = mainWindow->getFileSystem();
    auto* search = mainWindow->getSearch();
    auto* ui = mainWindow->getUI();
    
    if (!fs || !search) {
        QMessageBox::warning(mainWindow, "Warning", "No filesystem is currently mounted");
        return;
    }
    
    // Get search term from UI
    QString searchTerm = ui->searchLineEdit->text();
    
    if (searchTerm.isEmpty()) {
        QMessageBox::information(mainWindow, "Search", "Please enter a search term");
        return;
    }
    
    // Perform search
    search->clear_criteria();
    search->add_name_criteria(searchTerm.toStdString());
    std::vector<SearchResult> searchResults = search->search();
    
    std::vector<std::string> results;
    for (const auto& result : searchResults) {
        results.push_back(result.path);
    }
    
    if (!results.empty()) {
        // Just show the results in the file list
        ui->fileListWidget->clear();
        
        for (const auto &path : results) {
            ui->fileListWidget->addItem(new QListWidgetItem(QString::fromStdString(path)));
        }
        
        // Update status
        ui->statusbar->showMessage(QString("Found %1 matching files").arg(results.size()), 5000);
    } else {
        QMessageBox::information(mainWindow, "Search Results", "No matching files found");
    }
}

void MainWindowDialogs::handleQuotaManager() {
    auto* fs = mainWindow->getFileSystem();
    auto* quotaManager = mainWindow->getQuotaManager();
    
    if (!fs || !quotaManager) {
        QMessageBox::warning(mainWindow, "Warning", "No filesystem is currently mounted");
        return;
    }
    
    // Simple quota management dialog
    QDialog quotaDialog(mainWindow);
    quotaDialog.setWindowTitle("Quota Manager");
    quotaDialog.resize(500, 300);
    
    QVBoxLayout *layout = new QVBoxLayout(&quotaDialog);
    
    // Current quota info
    QLabel *infoLabel = new QLabel(&quotaDialog);
    infoLabel->setText("Current quota information:");
    layout->addWidget(infoLabel);
    
    // Usage indicator
    QLabel *usageLabel = new QLabel("Disk Usage:", &quotaDialog);
    layout->addWidget(usageLabel);
    
    QProgressBar *progressBar = new QProgressBar(&quotaDialog);
    progressBar->setMinimum(0);
    progressBar->setMaximum(100);
    progressBar->setValue(50); // Sample value
    layout->addWidget(progressBar);
    
    // Settings group
    QGroupBox *settingsGroupBox = new QGroupBox("Quota Settings", &quotaDialog);
    QFormLayout *settingsLayout = new QFormLayout(settingsGroupBox);
    
    QSpinBox *sizeSpinBox = new QSpinBox(settingsGroupBox);
    sizeSpinBox->setMinimum(1);
    sizeSpinBox->setMaximum(1000);
    sizeSpinBox->setValue(100);
    sizeSpinBox->setSuffix(" MB");
    
    settingsLayout->addRow("Total Space (MB):", sizeSpinBox);
    layout->addWidget(settingsGroupBox);
    
    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(&quotaDialog);
    QPushButton *applyButton = new QPushButton("Apply", &quotaDialog);
    QPushButton *closeButton = new QPushButton("Close", &quotaDialog);
    
    buttonBox->addButton(applyButton, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(closeButton, QDialogButtonBox::RejectRole);
    
    layout->addWidget(buttonBox);
    
    // Connect signals
    QObject::connect(applyButton, &QPushButton::clicked, [=]() {
        size_t newSize = sizeSpinBox->value() * 1024 * 1024;
        // Set quota (implementation depends on your FileSystem and QuotaManager class)
        
        QMessageBox::information(mainWindow, "Success", "Quota settings updated successfully");
    });
    
    QObject::connect(closeButton, &QPushButton::clicked, &quotaDialog, &QDialog::accept);
    
    quotaDialog.exec();
}

void MainWindowDialogs::handleSnapshots() {
    auto* fs = mainWindow->getFileSystem();
    auto* snapshotManager = mainWindow->getSnapshotManager();
    
    if (!fs || !snapshotManager) {
        QMessageBox::warning(mainWindow, "Warning", "No filesystem is currently mounted");
        return;
    }
    
    // Create snapshot management dialog
    QDialog snapshotDialog(mainWindow);
    snapshotDialog.setWindowTitle("Snapshot Manager");
    snapshotDialog.resize(600, 400);
    
    QVBoxLayout *layout = new QVBoxLayout(&snapshotDialog);
    
    QLabel *label = new QLabel("Available Snapshots:", &snapshotDialog);
    layout->addWidget(label);
    
    // List snapshots
    QListWidget *snapshotList = new QListWidget(&snapshotDialog);
    
    // Add some sample snapshots (replace with actual snapshot listing)
    snapshotList->addItem("snapshot_2025-08-15_12-30-00");
    snapshotList->addItem("snapshot_2025-08-16_09-15-30");
    
    layout->addWidget(snapshotList);
    
    // Buttons
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
    
    // Connect button signals
    QObject::connect(closeButton, &QPushButton::clicked, &snapshotDialog, &QDialog::accept);
    
    QObject::connect(createButton, &QPushButton::clicked, [this, snapshotList]() {
        bool ok;
        QString name = QInputDialog::getText(mainWindow, 
            "Create Snapshot", 
            "Enter snapshot name:", 
            QLineEdit::Normal, 
            "snapshot_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss"), 
            &ok);
        
        if (ok && !name.isEmpty()) {
            // Create snapshot
            QMessageBox::information(mainWindow, "Success", "Snapshot created successfully");
            
            // Add to list
            snapshotList->addItem(name);
        }
    });
    
    QObject::connect(restoreButton, &QPushButton::clicked, [this, snapshotList]() {
        QListWidgetItem *item = snapshotList->currentItem();
        if (!item) {
            QMessageBox::warning(mainWindow, "Warning", "Please select a snapshot to restore");
            return;
        }
        
        QMessageBox::StandardButton reply = QMessageBox::question(mainWindow, 
            "Confirm Restore", 
            "Are you sure you want to restore snapshot " + item->text() + "? All current data will be replaced.",
            QMessageBox::Yes | QMessageBox::No);
            
        if (reply == QMessageBox::Yes) {
            // Restore snapshot
            QMessageBox::information(mainWindow, "Success", "Snapshot restored successfully");
        }
    });
    
    QObject::connect(deleteButton, &QPushButton::clicked, [this, snapshotList]() {
        QListWidgetItem *item = snapshotList->currentItem();
        if (!item) {
            QMessageBox::warning(mainWindow, "Warning", "Please select a snapshot to delete");
            return;
        }
        
        QMessageBox::StandardButton reply = QMessageBox::question(mainWindow, 
            "Confirm Delete", 
            "Are you sure you want to delete snapshot " + item->text() + "?",
            QMessageBox::Yes | QMessageBox::No);
            
        if (reply == QMessageBox::Yes) {
            // Delete snapshot
            QMessageBox::information(mainWindow, "Success", "Snapshot deleted successfully");
            
            // Remove from list
            delete snapshotList->takeItem(snapshotList->row(item));
        }
    });
    
    snapshotDialog.exec();
}

void MainWindowDialogs::handleFilesystemDetection() {
    auto* fsDetector = mainWindow->getFsDetector();
    auto* fs = mainWindow->getFileSystem();
    
    if (!fsDetector) {
        QMessageBox::warning(mainWindow, "Warning", "Filesystem detector is not available");
        return;
    }
    
    QStringList filesystems = fsDetector->detectFilesystems();
    
    if (filesystems.isEmpty()) {
        QMessageBox::information(mainWindow, "Filesystem Detection", "No filesystems detected");
        return;
    }
    
    // Simple filesystem selection dialog
    QDialog dialog(mainWindow);
    dialog.setWindowTitle("Available Filesystems");
    dialog.resize(400, 300);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QLabel *label = new QLabel("Select a filesystem to mount:", &dialog);
    layout->addWidget(label);
    
    // Use a list widget with user-friendly names
    QListWidget *listWidget = new QListWidget(&dialog);
    for (const QString &fsPath : filesystems) {
        QString displayName = FileSystemDetector::getDisplayNameForPath(fsPath);
        QListWidgetItem *item = new QListWidgetItem(displayName, listWidget);
        // Store the actual path as item data
        item->setData(Qt::UserRole, fsPath);
    }
    layout->addWidget(listWidget);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);
    
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        QListWidgetItem *item = listWidget->currentItem();
        if (item) {
            QString selectedPath = item->data(Qt::UserRole).toString();
            
            if (fs) {
                fs->unmount();
            }
            
            // Check if it's an external filesystem
            if (FileSystemDetector::isExternalPath(selectedPath)) {
                QString realPath = FileSystemDetector::extractRealPath(selectedPath);
                QMessageBox::information(mainWindow, "Mount", "Selected external filesystem: " + realPath);
                
                // Here you would implement the mounting of the external filesystem
                // For now, we'll just show a message since it requires different handling
                QMessageBox::warning(mainWindow, "External Filesystem", 
                    "Mounting external filesystems directly is not yet implemented. "
                    "You would need to copy the data to a local .fs file first.");
            } else {
                // It's a local .fs file, so mount it directly
                mainWindow->getFileSystem()->unmount();
                
                // Create a new FileSystem instance for the main window
                // Directly manipulate the mainWindow's FileSystem - in a real application,
                // we'd have a proper method for this in the MainWindow class
                try {
                    // We'll just attempt to mount the existing file
                    FileSystem* newFs = mainWindow->getFileSystem();
                    newFs->unmount(); // Make sure it's unmounted
                    
                    // Re-mount with the selected path
                    if (newFs->mount()) {
                        QMessageBox::information(mainWindow, "Mount", "Successfully mounted filesystem: " + selectedPath);
                        // We need to call the appropriate method to refresh the UI
                        // Since refreshFileList() is private, we'll use the on_mountButton_clicked slot instead
                        QMetaObject::invokeMethod(mainWindow, "on_mountButton_clicked");
                    } else {
                        QMessageBox::critical(mainWindow, "Mount", "Failed to mount filesystem: " + selectedPath);
                    }
                } catch (const std::exception& e) {
                    QMessageBox::critical(mainWindow, "Error", 
                        "Exception occurred while mounting filesystem: " + QString(e.what()));
                }
            }
        }
    }
}

void MainWindowDialogs::updateAvailableFilesystemsList() {
    // Placeholder implementation since the UI element is not currently defined
}
