#include "ui/filesystem_detector.h"
#include "ui/filesystem_local_detector.h"
#include "ui/filesystem_external_detector.h"
#include <QDebug>
#include <QFileInfo>

FileSystemDetector::FileSystemDetector(QObject *parent)
    : QObject(parent)
{
}

QStringList FileSystemDetector::detectFilesystems()
{
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
                qDebug() << "Added external filesystem:" << fs;
            }
        } else {
            result.append(fs);
            qDebug() << "Added filesystem:" << fs;
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
                qDebug() << "Added external filesystem from /proc/mounts:" << fs;
            }
        } else {
            result.append(fs);
            qDebug() << "Added filesystem from /proc/mounts:" << fs;
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
                qDebug() << "Added external filesystem from lsblk:" << fs;
            }
        } else {
            result.append(fs);
            qDebug() << "Added filesystem from lsblk:" << fs;
        }
    }
    qDebug() << "Found" << lsblkFs.size() << "filesystems using lsblk";
    
    // Try additional manual detection methods if we haven't found any external drives
    if (addedExternalPaths.isEmpty()) {
        qDebug() << "No external filesystems found with standard methods. Trying manual block device detection...";
        QStringList manualFs = externalDetector.scanManualBlockDevices();
        for (const QString &fs : manualFs) {
            if (isExternalPath(fs)) {
                QString realPath = extractRealPath(fs);
                if (!addedExternalPaths.contains(realPath)) {
                    addedExternalPaths.insert(realPath);
                    result.append(fs);
                    qDebug() << "Added external filesystem from manual detection:" << fs;
                }
            } else {
                result.append(fs);
                qDebug() << "Added filesystem from manual detection:" << fs;
            }
        }
        qDebug() << "Found" << manualFs.size() << "filesystems using manual detection";
    }
    
    // Try specialized methods for USB drives
    qDebug() << "Scanning specifically for USB drives...";
    QStringList usbFs = externalDetector.scanForUsbDrives();
    for (const QString &fs : usbFs) {
        if (isExternalPath(fs)) {
            QString realPath = extractRealPath(fs);
            if (!addedExternalPaths.contains(realPath)) {
                addedExternalPaths.insert(realPath);
                result.append(fs);
                qDebug() << "Added USB drive:" << fs;
            }
        }
    }
    qDebug() << "Found" << usbFs.size() << "USB drives";
    
    // Try specialized methods for external hard drives
    qDebug() << "Scanning specifically for external hard drives...";
    QStringList hdFs = externalDetector.scanForHardDrives();
    for (const QString &fs : hdFs) {
        if (isExternalPath(fs)) {
            QString realPath = extractRealPath(fs);
            if (!addedExternalPaths.contains(realPath)) {
                addedExternalPaths.insert(realPath);
                result.append(fs);
                qDebug() << "Added external hard drive:" << fs;
            }
        }
    }
    qDebug() << "Found" << hdFs.size() << "external hard drives";
    
    qDebug() << "==== Filesystem detection complete ====";
    qDebug() << "Total filesystems found:" << result.size();
    
    // Print all found filesystems for debugging
    for (const QString &fs : result) {
        qDebug() << "  - " << fs << " -> " << getDisplayNameForPath(fs);
    }
    
    return result;
}

bool FileSystemDetector::isExternalPath(const QString &path)
{
    return path.startsWith("EXTERNAL:") || path.startsWith("UNMOUNTED:");
}

QString FileSystemDetector::getDisplayNameForPath(const QString &path)
{
    if (path.startsWith("UNMOUNTED:")) {
        // This is an unmounted external drive
        QString devicePath = extractRealPath(path);
        
        // Get device name from path
        QFileInfo fileInfo(devicePath);
        QString deviceName = fileInfo.fileName();
        
        // Try to get filesystem type
        QProcess blkidProcess;
        blkidProcess.start("blkid", QStringList() << "-o" << "value" << "-s" << "TYPE" << devicePath);
        QString fsType;
        
        if (blkidProcess.waitForFinished(1000)) {
            fsType = QString::fromUtf8(blkidProcess.readAllStandardOutput()).trimmed();
        }
        
        // Try to get size
        QProcess sizeProcess;
        sizeProcess.start("blockdev", QStringList() << "--getsize64" << devicePath);
        QString sizeStr;
        
        if (sizeProcess.waitForFinished(1000)) {
            QString sizeOutput = QString::fromUtf8(sizeProcess.readAllStandardOutput()).trimmed();
            bool ok;
            qint64 size = sizeOutput.toLongLong(&ok);
            
            if (ok) {
                if (size > 1024*1024*1024*1024LL) {
                    sizeStr = QString::number(size / (1024.0*1024*1024*1024), 'f', 2) + " TB";
                } else if (size > 1024*1024*1024) {
                    sizeStr = QString::number(size / (1024.0*1024*1024), 'f', 2) + " GB";
                } else if (size > 1024*1024) {
                    sizeStr = QString::number(size / (1024.0*1024), 'f', 2) + " MB";
                } else {
                    sizeStr = QString::number(size / 1024.0, 'f', 2) + " KB";
                }
            }
        }
        
        // Format the display name
        QString displayName = "Unmounted Drive: " + deviceName;
        
        if (!sizeStr.isEmpty()) {
            displayName += " (" + sizeStr + ")";
        }
        
        if (!fsType.isEmpty()) {
            displayName += " [" + fsType + "]";
        }
        
        return displayName;
    } else if (isExternalPath(path)) {
        QString realPath = extractRealPath(path);
        
        // First try to get vendor, model and size information
        QProcess processDetails;
        processDetails.start("lsblk", QStringList() << "-n" << "-o" << "VENDOR,MODEL,LABEL,SIZE,TYPE,FSTYPE" << realPath);
        
        if (processDetails.waitForFinished(1000)) {
            QString output = QString::fromUtf8(processDetails.readAllStandardOutput()).trimmed();
            if (!output.isEmpty()) {
                QStringList parts = output.split(" ", Qt::SkipEmptyParts);
                
                // Try to assemble a descriptive name
                QString vendorModel;
                QString label;
                QString size;
                QString fsType;
                
                // Extract components from the output
                for (const QString &part : parts) {
                    if ((part.contains("G") || part.contains("M") || part.contains("T")) && 
                        part.contains("B") && size.isEmpty()) {
                        size = part; // Size like 32G, 1T, etc.
                    } else if ((part == "vfat" || part == "ntfs" || part == "exfat" || 
                              part == "ext4" || part == "ext3" || part == "xfs") && 
                              fsType.isEmpty()) {
                        fsType = part;
                    } else if (part.length() > 1 && !part.contains("/")) {
                        // Could be a label or vendor/model
                        if (label.isEmpty()) {
                            label = part;
                        } else if (vendorModel.isEmpty()) {
                            vendorModel = part;
                        } else {
                            vendorModel += " " + part;
                        }
                    }
                }
                
                // Check for USB device type
                bool isUsb = false;
                QProcess udevProcess;
                udevProcess.start("udevadm", QStringList() << "info" << "--query=all" << "--name=" + realPath);
                if (udevProcess.waitForFinished(1000)) {
                    QString udevOutput = QString::fromUtf8(udevProcess.readAllStandardOutput());
                    isUsb = udevOutput.contains("ID_BUS=usb", Qt::CaseInsensitive);
                }
                
                // Assemble the display name
                QString displayName;
                
                if (!label.isEmpty()) {
                    // If we have a label, use it as the primary identifier
                    displayName = isUsb ? "USB Drive: " : "External Drive: ";
                    displayName += label;
                    
                    if (!size.isEmpty()) {
                        displayName += " (" + size + ")";
                    }
                    
                    if (!fsType.isEmpty()) {
                        displayName += " [" + fsType + "]";
                    }
                } else if (!vendorModel.isEmpty()) {
                    // If no label but we have vendor/model, use that
                    displayName = isUsb ? "USB Drive: " : "External Drive: ";
                    displayName += vendorModel;
                    
                    if (!size.isEmpty()) {
                        displayName += " (" + size + ")";
                    }
                } else {
                    // Fallback if we only have size
                    displayName = isUsb ? "USB Drive" : "External Drive";
                    
                    if (!size.isEmpty()) {
                        displayName += " (" + size + ")";
                    }
                    
                    if (!fsType.isEmpty()) {
                        displayName += " [" + fsType + "]";
                    }
                }
                
                return displayName;
            }
        }
        
        // Fall back to lsblk for basic label and size if the detailed approach failed
        QProcess process;
        process.start("lsblk", QStringList() << "-n" << "-o" << "LABEL,SIZE" << realPath);
        
        if (process.waitForFinished(1000)) {
            QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
            if (!output.isEmpty()) {
                QStringList parts = output.split(" ", Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    // We have a label and size
                    return "External: " + parts[0] + " (" + parts[1] + ")";
                } else if (parts.size() == 1) {
                    // Try to determine if it's a size or label
                    if (parts[0].contains("G") || parts[0].contains("M") || parts[0].contains("K")) {
                        // It's likely a size
                        return "External Drive (" + parts[0] + ")";
                    } else {
                        // It's likely a label
                        return "External: " + parts[0];
                    }
                }
            }
        }
        
        // Extract a user-friendly name from the path as a last resort
        int lastSlash = realPath.lastIndexOf('/');
        if (lastSlash != -1 && lastSlash < realPath.length() - 1) {
            QString name = realPath.mid(lastSlash + 1);
            // Check if it's in a standard mount location
            if (realPath.startsWith("/media/") || realPath.startsWith("/run/media/")) {
                return "External: " + name;
            } else if (realPath.startsWith("/mnt/")) {
                return "Mounted: " + name;
            } else {
                return "External Drive: " + name;
            }
        } else {
            return "External Drive";
        }
    } else {
        // For local .fs files, use the filename
        QFileInfo fileInfo(path);
        QString filename = fileInfo.fileName();
        QString sizeStr;
        
        // Get the file size for display
        qint64 size = fileInfo.size();
        if (size > 1024*1024*1024) {
            sizeStr = QString::number(size / (1024.0*1024*1024), 'f', 2) + " GB";
        } else if (size > 1024*1024) {
            sizeStr = QString::number(size / (1024.0*1024), 'f', 2) + " MB";
        } else if (size > 1024) {
            sizeStr = QString::number(size / 1024.0, 'f', 2) + " KB";
        } else {
            sizeStr = QString::number(size) + " bytes";
        }
        
        return "Local: " + filename + " (" + sizeStr + ")";
    }
}

QString FileSystemDetector::extractRealPath(const QString &path)
{
    if (path.startsWith("EXTERNAL:")) {
        return path.mid(9); // Remove "EXTERNAL:" prefix
    } else if (path.startsWith("UNMOUNTED:")) {
        return path.mid(10); // Remove "UNMOUNTED:" prefix
    } else {
        return path;
    }
}

bool FileSystemDetector::isExternalDevice(const QString &path)
{
    return FilesystemExternalDetector::isExternalDevice(path);
}
