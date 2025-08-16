#include "ui/filesystem_external_detector.h"
#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>

FilesystemExternalDetector::FilesystemExternalDetector(QObject* parent)
    : QObject(parent) {
}

QStringList FilesystemExternalDetector::scanMountPoints() {
    QStringList result;
    QSet<QString> addedMountPoints;
    
    // Standard mount points for external drives
    QStringList standardMountPoints = {
        "/media",
        "/media/" + qgetenv("USER"),
        "/run/media/" + qgetenv("USER"),
        "/mnt"
    };
    
    for (const QString &basePath : standardMountPoints) {
        QDir baseDir(basePath);
        if (!baseDir.exists()) {
            continue;
        }
        
        QStringList mountPoints = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &mountPoint : mountPoints) {
            QString fullPath = baseDir.absoluteFilePath(mountPoint);
            
            // Skip if we've already added this path
            if (addedMountPoints.contains(fullPath)) {
                continue;
            }
            
            // Check if this is a valid mount point with a filesystem
            QFileInfo checkDir(fullPath);
            if (checkDir.isReadable() && isExternalDevice(fullPath)) {
                result.append("EXTERNAL:" + fullPath);
                addedMountPoints.insert(fullPath);
            }
        }
    }
    
    return result;
}

QStringList FilesystemExternalDetector::scanProcMounts() {
    QStringList result;
    QSet<QString> addedMountPoints;
    
    QFile mountsFile("/proc/mounts");
    if (!mountsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open /proc/mounts";
        return result;
    }
    
    QTextStream in(&mountsFile);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList parts = line.split(' ');
        
        if (parts.size() < 2) {
            continue;
        }
        
        QString devicePath = parts[0];
        QString mountPoint = parts[1];
        
        // Skip if we've already added this mount point
        if (addedMountPoints.contains(mountPoint)) {
            continue;
        }
        
        // Check if this is a valid external device
        if (isExternalDevice(mountPoint)) {
            result.append("EXTERNAL:" + mountPoint);
            addedMountPoints.insert(mountPoint);
        }
    }
    
    mountsFile.close();
    return result;
}

QStringList FilesystemExternalDetector::scanWithLsblk() {
    QStringList result;
    QSet<QString> addedMountPoints;
    
    QProcess process;
    process.start("lsblk", QStringList() << "-J" << "-o" << "NAME,MOUNTPOINT,TYPE");
    
    if (!process.waitForFinished(3000)) {
        qDebug() << "lsblk command timed out";
        return result;
    }
    
    QByteArray output = process.readAllStandardOutput();
    
    // Parse the JSON output
    QJsonDocument doc = QJsonDocument::fromJson(output);
    if (doc.isNull() || !doc.isObject()) {
        qDebug() << "Failed to parse lsblk JSON output";
        return result;
    }
    
    QJsonArray blockdevices = doc.object()["blockdevices"].toArray();
    
    for (const QJsonValue &value : blockdevices) {
        QJsonObject device = value.toObject();
        
        // Skip loop devices and internal drives
        if (device["type"].toString() == "loop") {
            continue;
        }
        
        // Check if this device has a mount point
        QString mountPoint = device["mountpoint"].toString();
        if (!mountPoint.isEmpty() && mountPoint != "null" && mountPoint != "[SWAP]") {
            // Skip if we've already added this mount point
            if (addedMountPoints.contains(mountPoint)) {
                continue;
            }
            
            // Check if this is a valid external device
            if (isExternalDevice(mountPoint)) {
                result.append("EXTERNAL:" + mountPoint);
                addedMountPoints.insert(mountPoint);
            }
        }
        
        // Check children (partitions)
        QJsonArray children = device["children"].toArray();
        for (const QJsonValue &childValue : children) {
            QJsonObject child = childValue.toObject();
            QString childMountPoint = child["mountpoint"].toString();
            
            if (!childMountPoint.isEmpty() && childMountPoint != "null" && childMountPoint != "[SWAP]") {
                // Skip if we've already added this mount point
                if (addedMountPoints.contains(childMountPoint)) {
                    continue;
                }
                
                // Check if this is a valid external device
                if (isExternalDevice(childMountPoint)) {
                    result.append("EXTERNAL:" + childMountPoint);
                    addedMountPoints.insert(childMountPoint);
                }
            }
        }
    }
    
    return result;
}

QStringList FilesystemExternalDetector::scanManualBlockDevices() {
    QStringList result;
    QSet<QString> addedMountPoints;
    
    // Scan /dev/sd* devices
    QDir devDir("/dev");
    QStringList filters;
    filters << "sd*" << "nvme*n*p*";
    
    QStringList entries = devDir.entryList(filters, QDir::System);
    
    for (const QString &entry : entries) {
        QString devicePath = "/dev/" + entry;
        
        // Skip non-partitions (sda vs sda1)
        if (entry.length() <= 3 && !entry.contains('p')) {
            continue;
        }
        
        // Check if the device is mounted
        checkAndAddDevice(devicePath, result, addedMountPoints);
    }
    
    // Scan /dev/disk/by-id/ for USB drives
    QDir diskByIdDir("/dev/disk/by-id");
    if (diskByIdDir.exists()) {
        QStringList usbEntries = diskByIdDir.entryList(QStringList() << "usb-*", QDir::System);
        
        for (const QString &entry : usbEntries) {
            QString devicePath = "/dev/disk/by-id/" + entry;
            
            // Skip non-partitions
            if (!entry.contains("part")) {
                continue;
            }
            
            // Check if the device is mounted
            checkAndAddDevice(devicePath, result, addedMountPoints);
        }
    }
    
    return result;
}

void FilesystemExternalDetector::checkAndAddDevice(const QString &devicePath, QStringList &result, QSet<QString> &addedMountPoints) {
    // Run findmnt to get mount point
    QProcess process;
    process.start("findmnt", QStringList() << "-n" << "-o" << "TARGET" << devicePath);
    
    if (!process.waitForFinished(1000)) {
        return;
    }
    
    QString mountPoint = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    
    if (!mountPoint.isEmpty() && !addedMountPoints.contains(mountPoint)) {
        // Check if this is a valid external device
        if (isExternalDevice(mountPoint)) {
            result.append("EXTERNAL:" + mountPoint);
            addedMountPoints.insert(mountPoint);
        }
    }
}

bool FilesystemExternalDetector::isExternalDevice(const QString &path) {
    // Skip system directories
    if (path == "/" || path.startsWith("/boot") || path.startsWith("/usr") ||
        path.startsWith("/var") || path.startsWith("/etc") || path.startsWith("/bin") ||
        path.startsWith("/sbin") || path.startsWith("/lib") || path.startsWith("/opt") ||
        path.startsWith("/proc") || path.startsWith("/sys") || path.startsWith("/dev") ||
        path.startsWith("/run") || path.startsWith("/tmp")) {
        return false;
    }
    
    // Check if path is under known external device mount points
    if (path.startsWith("/media") || path.startsWith("/mnt") || 
        path.startsWith("/run/media")) {
        return true;
    }
    
    // Check if the path is accessible
    QFileInfo pathInfo(path);
    if (!pathInfo.isReadable()) {
        return false;
    }
    
    // Additional check for device paths using udisksctl
    QProcess process;
    process.start("udisksctl", QStringList() << "info" << "-b" << path);
    
    if (process.waitForFinished(1000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        
        // Check for removable or usb in the output
        if (output.contains("Removable:", Qt::CaseInsensitive) && 
            output.contains("true", Qt::CaseInsensitive)) {
            return true;
        }
        
        if (output.contains("Connection Bus:", Qt::CaseInsensitive) && 
            output.contains("usb", Qt::CaseInsensitive)) {
            return true;
        }
    }
    
    // Try to determine by running findmnt
    process.start("findmnt", QStringList() << "-n" << "-o" << "SOURCE" << path);
    
    if (process.waitForFinished(1000)) {
        QString source = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        
        // If the source contains /dev/sd but not the root device, it's likely external
        if ((source.contains("/dev/sd") || source.contains("/dev/nvme")) && 
            !source.contains("sda1") && !source.contains("nvme0n1p1")) {
            
            // Check if it's actually the system drive
            QProcess lsblk;
            lsblk.start("lsblk", QStringList() << "-o" << "NAME,MOUNTPOINT" << source);
            
            if (lsblk.waitForFinished(1000)) {
                QString lsblkOutput = QString::fromUtf8(lsblk.readAllStandardOutput());
                
                // If this device has the root (/) mount point, it's not external
                if (!lsblkOutput.contains(" /\n")) {
                    return true;
                }
            } else {
                // If lsblk fails, assume it's external if it's not in /etc/fstab
                QFile fstab("/etc/fstab");
                if (fstab.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QString fstabContent = QString::fromUtf8(fstab.readAll());
                    fstab.close();
                    
                    if (!fstabContent.contains(source)) {
                        return true;
                    }
                }
            }
        }
    }
    
    // Another check: run the mount command and check if the filesystem type is removable
    process.start("mount");
    
    if (process.waitForFinished(1000)) {
        QString mountOutput = QString::fromUtf8(process.readAllStandardOutput());
        QStringList lines = mountOutput.split('\n');
        
        for (const QString &line : lines) {
            if (line.contains(path)) {
                // Check for vfat, exfat, ntfs - common on removable drives
                if (line.contains("vfat") || line.contains("exfat") || 
                    line.contains("ntfs") || line.contains("fuseblk") ||
                    line.contains("msdos")) {
                    return true;
                }
            }
        }
    }
    
    return false;
}
