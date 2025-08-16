#include "ui/mainwindow_file_ops.h"
#include "ui_mainwindow.h"
#include <QListWidgetItem>
#include <QInputDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QStyle>
#include <string.h>  // For strnlen

MainWindowFileOps::MainWindowFileOps(MainWindow* mainWindow) 
    : mainWindow(mainWindow) {
}

void MainWindowFileOps::createFile() {
    bool ok;
    QString fileName = QInputDialog::getText(mainWindow, 
        "Create File", 
        "Enter file name:", 
        QLineEdit::Normal, 
        "new_file.txt", 
        &ok);
    
    if (ok && !fileName.isEmpty()) {
        auto* fs = mainWindow->getFileSystem();
        
        if (fs) {
            fs->create(fileName.toStdString());
            refreshFileList();
        }
    }
}

void MainWindowFileOps::createDirectory() {
    bool ok;
    QString dirName = QInputDialog::getText(mainWindow, 
        "Create Directory", 
        "Enter directory name:", 
        QLineEdit::Normal, 
        "new_directory", 
        &ok);
    
    if (ok && !dirName.isEmpty()) {
        auto* fs = mainWindow->getFileSystem();
        
        if (fs) {
            fs->mkdir(dirName.toStdString());
            refreshFileList();
        }
    }
}

void MainWindowFileOps::saveFile() {
    auto* ui = mainWindow->getUI();
    auto* fs = mainWindow->getFileSystem();
    std::string current_open_file = mainWindow->getCurrentOpenFile();
    
    if (!fs || current_open_file.empty()) return;
    
    std::string content = ui->fileContentTextEdit->toPlainText().toStdString();
    fs->write(current_open_file, content);
    
    ui->statusbar->showMessage("File saved: " + QString::fromStdString(current_open_file), 3000);
}

void MainWindowFileOps::fileDoubleClicked(QListWidgetItem *item) {
    if (!item) return;
    
    auto* ui = mainWindow->getUI();
    auto* fs = mainWindow->getFileSystem();
    if (!fs) return;
    
    // Get file name (removing prefix if present)
    std::string file_name = item->text().toStdString();
    
    // Remove prefix if present
    if (file_name.substr(0, 4) == "[D] ") {
        file_name = file_name.substr(4);
    } else if (file_name.substr(0, 4) == "[F] ") {
        file_name = file_name.substr(4);
    }
    
    // Get inode information to determine if it's a directory or file
    int inode_num = item->data(Qt::UserRole).toInt();
    Inode inode = fs->get_inode(inode_num);
    
    if (inode.mode == 2) {
        // It's a directory, navigate to it
        fs->cd(file_name);
        refreshFileList();
    } else {
        // It's a file, open it for editing
        std::string content = fs->read(file_name);
        ui->fileContentTextEdit->setPlainText(QString::fromStdString(content));
        mainWindow->setCurrentOpenFile(file_name);
        ui->saveButton->setEnabled(true);
    }
}

void MainWindowFileOps::fileContextMenu(const QPoint &pos) {
    auto* ui = mainWindow->getUI();
    QListWidgetItem *item = ui->fileListWidget->itemAt(pos);
    if (!item) return;
    
    QMenu contextMenu(mainWindow);
    
    QAction *openAction = contextMenu.addAction("Open");
    QAction *renameAction = contextMenu.addAction("Rename");
    QAction *deleteAction = contextMenu.addAction("Delete");
    
    QAction *selectedAction = contextMenu.exec(ui->fileListWidget->mapToGlobal(pos));
    
    if (!selectedAction) return;
    
    auto* fs = mainWindow->getFileSystem();
    if (!fs) return;
    
    // Get file name (removing prefix if present)
    std::string file_name = item->text().toStdString();
    
    // Remove prefix if present
    if (file_name.substr(0, 4) == "[D] ") {
        file_name = file_name.substr(4);
    } else if (file_name.substr(0, 4) == "[F] ") {
        file_name = file_name.substr(4);
    }
    
    if (selectedAction == openAction) {
        fileDoubleClicked(item);
    } else if (selectedAction == renameAction) {
        bool ok;
        QString newName = QInputDialog::getText(mainWindow, 
            "Rename", 
            "Enter new name:", 
            QLineEdit::Normal, 
            QString::fromStdString(file_name), 
            &ok);
        
        if (ok && !newName.isEmpty()) {
            // Implement rename using temporary file copy and unlink
            // First read the original file
            std::string content = fs->read(file_name);
            
            // Create the new file
            fs->create(newName.toStdString());
            
            // Write the content to the new file
            fs->write(newName.toStdString(), content);
            
            // Delete the old file
            fs->unlink(file_name);
            
            refreshFileList();
        }
    } else if (selectedAction == deleteAction) {
        QMessageBox::StandardButton reply = QMessageBox::question(mainWindow, 
            "Confirm Delete", 
            "Are you sure you want to delete " + QString::fromStdString(file_name) + "?",
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::Yes) {
            fs->unlink(file_name);
            refreshFileList();
        }
    }
}

void MainWindowFileOps::refreshFileList() {
    auto* ui = mainWindow->getUI();
    ui->fileListWidget->clear();
    
    auto* fs = mainWindow->getFileSystem();
    if (!fs) return;
    
    // Get directory entries
    std::vector<DirEntry> entries = fs->ls();
    
    // Add entries to the list
    for (const auto &entry : entries) {
        // Create a std::string using the entry name and ensure it's properly terminated
        std::string nameStr(entry.name, strnlen(entry.name, MAX_FILENAME_LENGTH));
        
        if (nameStr == "." || nameStr == "..") 
            continue;
        
        // Check if the name contains only valid UTF-8 characters
        QString entryName = QString::fromUtf8(nameStr.c_str());
        if (entryName.isEmpty() && !nameStr.empty()) {
            // If conversion fails, fallback to Latin1 encoding
            entryName = QString::fromLatin1(nameStr.c_str());
            // If still problematic, provide a placeholder name with the inode number
            if (entryName.contains(QChar(QChar::ReplacementCharacter))) {
                entryName = QString("File-%1").arg(entry.inode_num);
            }
        }
            
        // Get the inode and determine if it's a directory or file
        if (!fs->is_valid_inode(entry.inode_num)) {
            QListWidgetItem *item = new QListWidgetItem("[?] " + entryName + " (invalid inode)");
            item->setData(Qt::UserRole, entry.inode_num);
            item->setIcon(ui->fileListWidget->style()->standardIcon(QStyle::SP_FileIcon));
            ui->fileListWidget->addItem(item);
            continue;
        }
        
        Inode inode = fs->get_inode(entry.inode_num);
        bool isDirectory = (inode.mode == 2);
        QString prefix = isDirectory ? "[D] " : "[F] ";
        
        QListWidgetItem *item = new QListWidgetItem(prefix + entryName);
        item->setData(Qt::UserRole, entry.inode_num);
        
        // Set icon based on file type
        if (isDirectory) {
            item->setIcon(ui->fileListWidget->style()->standardIcon(QStyle::SP_DirIcon));
        } else {
            item->setIcon(ui->fileListWidget->style()->standardIcon(QStyle::SP_FileIcon));
        }
        
        ui->fileListWidget->addItem(item);
    }
}

void MainWindowFileOps::handleDragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindowFileOps::handleDropEvent(QDropEvent *event) {
    auto* fs = mainWindow->getFileSystem();
    if (!fs) return;
    
    for (const QUrl &url : event->mimeData()->urls()) {
        QString filePath = url.toLocalFile();
        QFileInfo fileInfo(filePath);
        
        // Import the file into the filesystem
        std::string content;
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            content = file.readAll().toStdString();
            file.close();
            
            // Create the file in our filesystem
            std::string target_path = fileInfo.fileName().toStdString();
            fs->create(target_path);
            fs->write(target_path, content);
        } else {
            QMessageBox::critical(mainWindow, "Error", "Failed to open file: " + filePath);
        }
    }
    
    // Refresh the file list to show the new files
    refreshFileList();
}
