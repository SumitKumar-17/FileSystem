#include "ui/filesystem_mount_dialog.h"
#include "ui/filesystem_detector.h"
#include <QStyle>
#include <QFileInfo>

FileSystemMountDialog::FileSystemMountDialog(const QStringList &availableFilesystems, QWidget *parent)
    : QDialog(parent), fsListWidget(nullptr), selectedFilesystem("")
{
    setupUI();
    populateFilesystemList(availableFilesystems);
}

void FileSystemMountDialog::setupUI()
{
    setWindowTitle("Select Filesystem");
    setMinimumWidth(400);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    QLabel *label = new QLabel("Available Filesystems:", this);
    layout->addWidget(label);
    
    fsListWidget = new QListWidget(this);
    layout->addWidget(fsListWidget);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, this, &FileSystemMountDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void FileSystemMountDialog::populateFilesystemList(const QStringList &availableFilesystems)
{
    // Add items with readable names
    for (const QString &fs : availableFilesystems) {
        QListWidgetItem *item = nullptr;
        
        if (FileSystemDetector::isExternalPath(fs)) {
            QString path = FileSystemDetector::extractRealPath(fs);
            QString displayName = FileSystemDetector::getDisplayNameForPath(fs);
            item = new QListWidgetItem(QIcon(style()->standardIcon(QStyle::SP_DriveHDIcon)), displayName);
            item->setData(Qt::UserRole, fs);
        } else {
            item = new QListWidgetItem(QIcon(style()->standardIcon(QStyle::SP_FileIcon)), fs);
            item->setData(Qt::UserRole, fs);
        }
        
        fsListWidget->addItem(item);
    }
}

void FileSystemMountDialog::onAccepted()
{
    if (fsListWidget->currentItem()) {
        selectedFilesystem = fsListWidget->currentItem()->data(Qt::UserRole).toString();
        accept();
    } else {
        reject();
    }
}

QString FileSystemMountDialog::getSelectedFilesystem() const
{
    return selectedFilesystem;
}
