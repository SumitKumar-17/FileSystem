#include "ui/mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QListWidgetItem>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
                                          ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    fs = std::make_unique<FileSystem>("my_virtual_disk.fs");

    ui->fileListWidget->setEnabled(false);
    ui->fileContentTextEdit->setEnabled(false);
    ui->saveButton->setEnabled(false);
    ui->mkdirButton->setEnabled(false);
    ui->createFileButton->setEnabled(false);

    ui->fileListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->fileListWidget, &QListWidget::customContextMenuRequested, this, &MainWindow::on_fileListWidget_customContextMenuRequested);
}

MainWindow::~MainWindow()
{
    delete ui;
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
        ui->fileListWidget->addItem(item);
    }
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
        refreshFileList();
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
