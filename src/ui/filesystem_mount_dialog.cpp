#include "ui/filesystem_mount_dialog.h"
#include "ui/filesystem_detector.h"
#include <QFileInfo>
#include <QProcess>
#include <QPushButton>
#include <QStyle>

FileSystemMountDialog::FileSystemMountDialog(const QStringList &availableFilesystems,
                                             QWidget *parent)
    : QDialog(parent), fsListWidget(nullptr), selectedFilesystem("") {
    setupUI();
    populateFilesystemList(availableFilesystems);
}

void FileSystemMountDialog::setupUI() {
    setWindowTitle("Select Filesystem");
    setMinimumWidth(500);

    QVBoxLayout *layout = new QVBoxLayout(this);
    QLabel *label = new QLabel("Available Filesystems:", this);
    layout->addWidget(label);

    // Add refresh button at the top
    QPushButton *refreshButton = new QPushButton(
        QIcon(style()->standardIcon(QStyle::SP_BrowserReload)), "Refresh Filesystem List");
    layout->addWidget(refreshButton);
    connect(refreshButton, &QPushButton::clicked, this, [this]() {
        // Create a new FileSystemDetector to refresh the list
        FileSystemDetector detector;
        QStringList refreshedFs = detector.detectFilesystems();
        fsListWidget->clear();
        populateFilesystemList(refreshedFs);
    });

    fsListWidget = new QListWidget(this);
    layout->addWidget(fsListWidget);

    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &FileSystemMountDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void FileSystemMountDialog::populateFilesystemList(const QStringList &availableFilesystems) {
    int numExternal = 0;
    int numLocal = 0;

    // Group the filesystems by type
    QListWidgetItem *headerExternal = new QListWidgetItem("--- EXTERNAL DEVICES ---");
    headerExternal->setFlags(Qt::NoItemFlags);
    headerExternal->setBackground(QBrush(QColor(240, 240, 240)));
    headerExternal->setForeground(QBrush(QColor(0, 0, 0)));

    QListWidgetItem *headerLocal = new QListWidgetItem("--- LOCAL FILESYSTEM IMAGES ---");
    headerLocal->setFlags(Qt::NoItemFlags);
    headerLocal->setBackground(QBrush(QColor(240, 240, 240)));
    headerLocal->setForeground(QBrush(QColor(0, 0, 0)));

    // First, count the types
    for (const QString &fs : availableFilesystems) {
        if (FileSystemDetector::isExternalPath(fs)) {
            numExternal++;
        } else {
            numLocal++;
        }
    }

    // Add the headers and items
    if (numExternal > 0) {
        fsListWidget->addItem(headerExternal);
    }

    // Add external devices
    for (const QString &fs : availableFilesystems) {
        if (FileSystemDetector::isExternalPath(fs)) {
            QString path = FileSystemDetector::extractRealPath(fs);
            QString displayName = FileSystemDetector::getDisplayNameForPath(fs);

            // Try to get size information
            QString sizeInfo = getDeviceSizeInfo(path);
            if (!sizeInfo.isEmpty()) {
                displayName += " (" + sizeInfo + ")";
            }

            QListWidgetItem *item = new QListWidgetItem(
                QIcon(style()->standardIcon(QStyle::SP_DriveHDIcon)), displayName);
            item->setData(Qt::UserRole, fs);
            item->setToolTip("Mount point: " + path);
            fsListWidget->addItem(item);
        }
    }

    // Add local filesystem images
    if (numLocal > 0) {
        fsListWidget->addItem(headerLocal);
    }

    for (const QString &fs : availableFilesystems) {
        if (!FileSystemDetector::isExternalPath(fs)) {
            QFileInfo fileInfo(fs);
            QString displayName = fs;

            // Add file size for local filesystem images
            qint64 size = fileInfo.size();
            if (size > 0) {
                displayName += " (" + formatSize(size) + ")";
            }

            QListWidgetItem *item =
                new QListWidgetItem(QIcon(style()->standardIcon(QStyle::SP_FileIcon)), displayName);
            item->setData(Qt::UserRole, fs);
            item->setToolTip("Path: " + fileInfo.absoluteFilePath());
            fsListWidget->addItem(item);
        }
    }

    // Select the first actual item (not a header)
    for (int i = 0; i < fsListWidget->count(); i++) {
        QListWidgetItem *item = fsListWidget->item(i);
        if (item->flags() & Qt::ItemIsSelectable) {
            fsListWidget->setCurrentItem(item);
            break;
        }
    }
}

void FileSystemMountDialog::onAccepted() {
    if (fsListWidget->currentItem()) {
        selectedFilesystem = fsListWidget->currentItem()->data(Qt::UserRole).toString();
        accept();
    } else {
        reject();
    }
}

QString FileSystemMountDialog::getSelectedFilesystem() const {
    return selectedFilesystem;
}

QString FileSystemMountDialog::formatSize(qint64 bytes) {
    const qint64 kb = 1024;
    const qint64 mb = kb * 1024;
    const qint64 gb = mb * 1024;

    if (bytes >= gb) {
        return QString::number(bytes / gb, 'f', 2) + " GB";
    } else if (bytes >= mb) {
        return QString::number(bytes / mb, 'f', 2) + " MB";
    } else if (bytes >= kb) {
        return QString::number(bytes / kb, 'f', 2) + " KB";
    } else {
        return QString::number(bytes) + " bytes";
    }
}

QString FileSystemMountDialog::getDeviceSizeInfo(const QString &mountPoint) {
    // Get size information using df command
    QProcess process;
    process.start("df", QStringList() << "-h" << mountPoint);
    process.waitForFinished();
    QString output = process.readAllStandardOutput();

    QStringList lines = output.split('\n');
    if (lines.size() >= 2) {
        QString infoLine = lines[1];
        QStringList parts = infoLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        if (parts.size() >= 4) {
            QString size = parts[1];
            QString used = parts[2];
            QString available = parts[3];
            QString usedPercentage = parts[4];

            return size + ", " + usedPercentage + " used";
        }
    }

    return QString();
}
