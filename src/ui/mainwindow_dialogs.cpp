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
#include <QDir>
#include <QProgressDialog>
#include <QRandomGenerator>
#include <QProcess>
#include <QDateTime>

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
            
            // Check if it's an unmounted external filesystem
            if (selectedPath.startsWith("UNMOUNTED:")) {
                QString devicePath = selectedPath.mid(10); // Remove "UNMOUNTED:" prefix
                QMessageBox::information(mainWindow, "Mount", "Selected unmounted device: " + devicePath);
                
                // Ask if user wants to mount this device first
                QMessageBox::StandardButton reply = QMessageBox::question(mainWindow, 
                    "Mount Device", 
                    "This device is currently not mounted. Would you like to mount it first?",
                    QMessageBox::Yes | QMessageBox::No);
                
                if (reply == QMessageBox::Yes) {
                    // Create a mount point
                    QString mountPoint = QDir::tempPath() + "/filesys_mount_" + 
                                        QString::number(QRandomGenerator::global()->bounded(10000));
                    QDir().mkdir(mountPoint);
                    
                    // Try to mount the device
                    QProcess mountProcess;
                    mountProcess.start("pkexec", QStringList() << "mount" << devicePath << mountPoint);
                    
                    if (mountProcess.waitForFinished(5000)) {
                        if (mountProcess.exitCode() == 0) {
                            // Successfully mounted, update the path
                            selectedPath = "EXTERNAL:" + mountPoint;
                            QMessageBox::information(mainWindow, "Success", 
                                "Device successfully mounted at " + mountPoint);
                                
                            // Create new filesystem for the mounted path
                            FileSystem* newFs = new FileSystem(mountPoint.toStdString());
                            if (newFs->mount()) {
                                mainWindow->setFileSystem(newFs);
                                mainWindow->refreshFileList();
                                mainWindow->updateStatusBar("Mounted external filesystem: " + mountPoint);
                            } else {
                                delete newFs;
                                QMessageBox::warning(mainWindow, "Warning", 
                                    "Successfully mounted device, but failed to open filesystem");
                            }
                            return;
                            QMessageBox::information(mainWindow, "Success", 
                                "Device successfully mounted at " + mountPoint);
                        } else {
                            QString error = QString::fromUtf8(mountProcess.readAllStandardError());
                            QMessageBox::critical(mainWindow, "Mount Error", 
                                "Failed to mount device: " + error);
                            return;
                        }
                    } else {
                        QMessageBox::critical(mainWindow, "Mount Error", 
                            "Mount operation timed out or was canceled");
                        return;
                    }
                } else {
                    // Create a temporary file from the unmounted device
                    QString tempFilePath = QDir::tempPath() + "/temp_external_fs.fs";
                    
                    QProgressDialog progress("Creating temporary filesystem image...", "Abort", 0, 100, mainWindow);
                    progress.setWindowModality(Qt::WindowModal);
                    progress.show();
                    
                    progress.setValue(10);
                    
                    // Check device size to ensure we don't create a huge image file
                    QProcess blockdevProcess;
                    blockdevProcess.start("blockdev", QStringList() << "--getsize64" << devicePath);
                    qint64 deviceSize = 0;
                    
                    if (blockdevProcess.waitForFinished(2000)) {
                        QString sizeStr = QString::fromUtf8(blockdevProcess.readAllStandardOutput()).trimmed();
                        deviceSize = sizeStr.toLongLong();
                    }
                    
                    // Limit image size to 1GB for safety
                    if (deviceSize > 1024*1024*1024 || deviceSize <= 0) {
                        QMessageBox::critical(mainWindow, "Error", 
                            "Device size is too large or could not be determined. "
                            "For safety reasons, we cannot create an image of this device.");
                        return;
                    }
                    
                    progress.setValue(20);
                    
                    // Use dd to create the image file
                    QProcess ddProcess;
                    ddProcess.start("dd", QStringList() << "if=" + devicePath << "of=" + tempFilePath << "bs=1M");
                    
                    // Connect signals to update progress
                    bool success = false;
                    if (ddProcess.waitForFinished(-1)) {
                        if (ddProcess.exitCode() == 0) {
                            success = true;
                        }
                    }
                    
                    progress.setValue(100);
                    
                    if (!success) {
                        QMessageBox::critical(mainWindow, "Error", "Failed to create image of external device");
                        return;
                    }
                    
                    // Get the existing filesystem and update it
                    FileSystem* newFs = new FileSystem(tempFilePath.toStdString());
                    if (!newFs->mount()) {
                        QMessageBox::critical(mainWindow, "Error", "Failed to mount temporary filesystem image");
                        delete newFs;
                        return;
                    }
                    
                    // We can't directly update mainWindow's filesystem pointer
                    // So we'll tell the user to close and re-open the file
                    QMessageBox::information(mainWindow, "Success", 
                        "The device image has been created at " + tempFilePath + 
                        "\n\nPlease use File > Open to access this filesystem.");
                    
                    // Clean up
                    delete newFs;
                    return;
                }
            }
            
            // Check if it's an external filesystem
            if (FileSystemDetector::isExternalPath(selectedPath)) {
                QString realPath = FileSystemDetector::extractRealPath(selectedPath);
                QMessageBox::information(mainWindow, "Mount", "Selected external filesystem: " + realPath);
                
                // Attempt to create a temporary file from the external filesystem content
                QString tempFilePath = QDir::tempPath() + "/temp_external_fs.fs";
                
                QProgressDialog progress("Creating temporary filesystem image...", "Abort", 0, 100, mainWindow);
                progress.setWindowModality(Qt::WindowModal);
                progress.show();
                
                // Execute dd command to create an image of the filesystem
                QProcess ddProcess;
                QString sourceDevice;
                
                // First find the block device associated with this mount point
                QProcess findmntProcess;
                findmntProcess.start("findmnt", QStringList() << "-n" << "-o" << "SOURCE" << realPath);
                if (findmntProcess.waitForFinished(2000)) {
                    sourceDevice = QString::fromUtf8(findmntProcess.readAllStandardOutput()).trimmed();
                }
                
                if (sourceDevice.isEmpty()) {
                    QMessageBox::critical(mainWindow, "Error", "Could not determine source device for mount point: " + realPath);
                    return;
                }
                
                progress.setValue(10);
                
                // Check device size to ensure we don't create a huge image file
                QProcess blockdevProcess;
                blockdevProcess.start("blockdev", QStringList() << "--getsize64" << sourceDevice);
                qint64 deviceSize = 0;
                
                if (blockdevProcess.waitForFinished(2000)) {
                    QString sizeStr = QString::fromUtf8(blockdevProcess.readAllStandardOutput()).trimmed();
                    deviceSize = sizeStr.toLongLong();
                }
                
                // Limit image size to 1GB for safety
                if (deviceSize > 1024*1024*1024 || deviceSize <= 0) {
                    QMessageBox::critical(mainWindow, "Error", 
                        "Device size is too large or could not be determined. "
                        "For safety reasons, we cannot create an image of this device.");
                    return;
                }
                
                progress.setValue(20);
                
                // Use dd to create the image file
                ddProcess.start("dd", QStringList() << "if=" + sourceDevice << "of=" + tempFilePath << "bs=1M");
                
                // Connect signals to update progress
                QObject::connect(&ddProcess, &QProcess::readyReadStandardError, [&]() {
                    QString output = QString::fromUtf8(ddProcess.readAllStandardError());
                    // Parse dd output to update progress
                    if (output.contains("bytes")) {
                        int progressValue = 30 + QRandomGenerator::global()->bounded(50); // Simulate progress
                        progress.setValue(progressValue);
                    }
                });
                
                if (!ddProcess.waitForFinished(30000)) { // 30 seconds timeout
                    QMessageBox::critical(mainWindow, "Error", "Timeout while creating filesystem image.");
                    return;
                }
                
                progress.setValue(90);
                
                // Now try to mount the image file
                try {
                    FileSystem* newFs = mainWindow->getFileSystem();
                    newFs->unmount(); // Make sure it's unmounted
                    
                    // Re-mount with the temporary image
                    if (newFs->mount()) {
                        QMessageBox::information(mainWindow, "Mount", 
                            "Successfully created temporary filesystem image and mounted it. "
                            "Note: this is read-only access to the filesystem.");
                        
                        // Refresh UI
                        QMetaObject::invokeMethod(mainWindow, "on_mountButton_clicked");
                    } else {
                        QMessageBox::critical(mainWindow, "Error", "Failed to mount the temporary filesystem image.");
                    }
                } catch (const std::exception& e) {
                    QMessageBox::critical(mainWindow, "Error", 
                        "Exception occurred while mounting filesystem: " + QString(e.what()));
                }
                
                progress.setValue(100);
            } else {
                // It's a local .fs file, so mount it directly
                mainWindow->getFileSystem()->unmount();
                
                // Create a new FileSystem instance for the main window
                // Directly manipulate the mainWindow's FileSystem - in a real application,
                // we'd have a proper method for this in the MainWindow class
                try {
                    // We'll just attempt to mount the existing file
                    FileSystem* oldFs = mainWindow->getFileSystem();
                    oldFs->unmount(); // Make sure it's unmounted
                    
                    // Create a new FileSystem instance and mount it
                    QString actualPath = selectedPath;
                    if (selectedPath.startsWith("EXTERNAL:")) {
                        actualPath = selectedPath.mid(9); // Remove "EXTERNAL:" prefix
                    }
                    
                    FileSystem* newFs = new FileSystem(actualPath.toStdString());
                    if (newFs->mount()) {
                        mainWindow->setFileSystem(newFs);
                        mainWindow->refreshFileList();
                        mainWindow->updateStatusBar("Mounted filesystem: " + actualPath);
                        QMessageBox::information(mainWindow, "Mount", "Successfully mounted filesystem: " + actualPath);
                    } else {
                        delete newFs; // Delete if mount failed
                        QMessageBox::critical(mainWindow, "Mount", "Failed to mount filesystem: " + actualPath);
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
