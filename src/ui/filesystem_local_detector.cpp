#include "ui/filesystem_local_detector.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

FilesystemLocalDetector::FilesystemLocalDetector(QObject *parent) : QObject(parent) {
}

QStringList FilesystemLocalDetector::scanLocalFilesystems() {
    QStringList result;
    QSet<QString> scannedPaths; // To avoid duplicates

    // Scan common directories where .fs files might be stored
    QStringList dirsToScan;

    // Add home directory and its subdirectories
    dirsToScan << QDir::homePath();
    dirsToScan << QDir::homePath() + "/Documents";
    dirsToScan << QDir::homePath() + "/Downloads";
    dirsToScan << QDir::homePath() + "/Desktop";

    // Add desktop directory from standard paths
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (!desktopPath.isEmpty() && !dirsToScan.contains(desktopPath)) {
        dirsToScan << desktopPath;
    }

    // Add current directory
    dirsToScan << QDir::currentPath();

    // Add parent directory of current path (often useful)
    QDir currentDir = QDir::current();
    if (currentDir.cdUp()) {
        dirsToScan << currentDir.absolutePath();
    }

    // Scan each directory
    for (const QString &dirPath : dirsToScan) {
        if (scannedPaths.contains(dirPath))
            continue;

        scannedPaths.insert(dirPath);
        result.append(scanDirectory(dirPath));
    }

    qDebug() << "Local filesystems found:" << result;
    return result;
}

QStringList FilesystemLocalDetector::scanDirectory(const QString &dirPath) {
    QStringList result;

    QDir dir(dirPath);
    if (!dir.exists()) {
        qDebug() << "Directory does not exist:" << dirPath;
        return result;
    }

    // Look for .fs files in the current directory
    QStringList filters;
    filters << "*.fs" << "*.FS" << "*.img" << "*.IMG" << "*.image";

    dir.setNameFilters(filters);
    QStringList entries = dir.entryList(QDir::Files);

    for (const QString &entry : entries) {
        QString fullPath = dir.absoluteFilePath(entry);
        QFileInfo fileInfo(fullPath);

        // Check if it's a valid filesystem image
        // This is a basic check - just verifying it's a file of reasonable size
        if (fileInfo.size() > 1024) { // Assume a valid filesystem is at least 1KB
            qDebug() << "Found potential filesystem image:" << fullPath;

            // Try to read the first few bytes to check if it looks like a filesystem
            QFile file(fullPath);
            if (file.open(QIODevice::ReadOnly)) {
                QByteArray header = file.read(512); // Read first sector
                file.close();

                // Very basic check for a filesystem signature
                // In a real implementation, you would check for specific filesystem signatures
                if (header.size() == 512) {
                    result.append(fullPath);
                }
            }
        }
    }

    // Scan first-level subdirectories too
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &subdir : subdirs) {
        // Skip deep scanning - just scan one level down
        // This prevents hanging on directories with many nested subdirectories
        QString subdirPath = dir.absoluteFilePath(subdir);

        // Skip system directories and hidden directories
        if (subdir.startsWith(".") || subdirPath.contains("/proc/") ||
            subdirPath.contains("/sys/") || subdirPath.contains("/dev/") ||
            subdirPath.contains("/run/") || subdirPath.contains("/tmp/") ||
            subdirPath.contains("/var/tmp/") || subdirPath.contains("/lib/") ||
            subdirPath.contains("/lib64/") || subdirPath.contains("/usr/lib/") ||
            subdirPath.contains("/var/lib/") || subdirPath.contains("/snap/") ||
            subdirPath.contains("/opt/")) {
            continue;
        }

        QDir subDir(subdirPath);
        subDir.setNameFilters(filters);
        QStringList subEntries = subDir.entryList(QDir::Files);

        for (const QString &entry : subEntries) {
            QString fullPath = subDir.absoluteFilePath(entry);
            QFileInfo fileInfo(fullPath);

            // Check if it's a valid filesystem image
            if (fileInfo.size() > 1024) {
                // Try to read the first few bytes to check if it looks like a filesystem
                QFile file(fullPath);
                if (file.open(QIODevice::ReadOnly)) {
                    QByteArray header = file.read(512); // Read first sector
                    file.close();

                    // Very basic check for a filesystem signature
                    if (header.size() == 512) {
                        result.append(fullPath);
                    }
                }
            }
        }
    }

    return result;
}
