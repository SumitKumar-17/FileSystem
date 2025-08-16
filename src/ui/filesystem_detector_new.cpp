#include "ui/filesystem_detector.h"
#include "ui/filesystem_external_detector.h"
#include "ui/filesystem_local_detector.h"
#include <QDebug>
#include <QFileInfo>

FileSystemDetector::FileSystemDetector(QObject *parent) : QObject(parent) {
}

QStringList FileSystemDetector::detectFilesystems() {
    qDebug() << "==== Starting filesystem detection ====";
    QStringList result;
    QSet<QString> addedExternalPaths;

    // Create local and external detectors
    FilesystemLocalDetector localDetector;
    FilesystemExternalDetector externalDetector;

    // Detect local filesystem images
    qDebug() << "Scanning for local .fs files...";
    QStringList localFs = localDetector.scanLocalFilesystems();
    result.append(localFs);
    qDebug() << "Found" << localFs.size() << "local filesystem images";

    // Detect external drives using multiple methods
    qDebug() << "Scanning standard mount points for external drives...";
    QStringList mountPointFs = externalDetector.scanMountPoints();
    for (const QString &fs : mountPointFs) {
        // Keep track of external paths we've already added
        if (isExternalPath(fs)) {
            QString realPath = extractRealPath(fs);
            if (!addedExternalPaths.contains(realPath)) {
                addedExternalPaths.insert(realPath);
                result.append(fs);
            }
        } else {
            result.append(fs);
        }
    }
    qDebug() << "Found" << mountPointFs.size() << "external drives in standard mount points";

    // Scan /proc/mounts for additional filesystems
    qDebug() << "Scanning /proc/mounts for additional filesystems...";
    QStringList procMountsFs = externalDetector.scanProcMounts();
    for (const QString &fs : procMountsFs) {
        if (isExternalPath(fs)) {
            QString realPath = extractRealPath(fs);
            if (!addedExternalPaths.contains(realPath)) {
                addedExternalPaths.insert(realPath);
                result.append(fs);
            }
        } else {
            result.append(fs);
        }
    }
    qDebug() << "Found" << procMountsFs.size() << "filesystems from /proc/mounts";

    // Run additional detection using lsblk for USB drives
    qDebug() << "Running additional detection using lsblk...";
    QStringList lsblkFs = externalDetector.scanWithLsblk();
    for (const QString &fs : lsblkFs) {
        if (isExternalPath(fs)) {
            QString realPath = extractRealPath(fs);
            if (!addedExternalPaths.contains(realPath)) {
                addedExternalPaths.insert(realPath);
                result.append(fs);
            }
        } else {
            result.append(fs);
        }
    }
    qDebug() << "Found" << lsblkFs.size() << "filesystems using lsblk";

    // Try additional manual detection methods if we haven't found any external drives
    if (addedExternalPaths.isEmpty()) {
        qDebug() << "No external filesystems found with standard methods. Trying manual block "
                    "device detection...";
        QStringList manualFs = externalDetector.scanManualBlockDevices();
        for (const QString &fs : manualFs) {
            if (isExternalPath(fs)) {
                QString realPath = extractRealPath(fs);
                if (!addedExternalPaths.contains(realPath)) {
                    addedExternalPaths.insert(realPath);
                    result.append(fs);
                }
            } else {
                result.append(fs);
            }
        }
        qDebug() << "Found" << manualFs.size() << "filesystems using manual detection";
    }

    qDebug() << "==== Filesystem detection complete ====";
    qDebug() << "Total filesystems found:" << result.size();

    return result;
}

bool FileSystemDetector::isExternalPath(const QString &path) {
    return path.startsWith("EXTERNAL:");
}

QString FileSystemDetector::getDisplayNameForPath(const QString &path) {
    if (isExternalPath(path)) {
        QString realPath = extractRealPath(path);

        // Extract a user-friendly name from the path
        int lastSlash = realPath.lastIndexOf('/');
        if (lastSlash != -1 && lastSlash < realPath.length() - 1) {
            return "External: " + realPath.mid(lastSlash + 1);
        } else {
            return "External Drive";
        }
    } else {
        // For local .fs files, use the filename
        QFileInfo fileInfo(path);
        return fileInfo.fileName();
    }
}

QString FileSystemDetector::extractRealPath(const QString &path) {
    if (isExternalPath(path)) {
        return path.mid(9); // Remove "EXTERNAL:" prefix
    } else {
        return path;
    }
}

bool FileSystemDetector::isExternalDevice(const QString &path) {
    return FilesystemExternalDetector::isExternalDevice(path);
}
