#include "ui/filesystem_detector.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDebug>
#include <QStandardPaths>
#include <QProcess>

FileSystemDetector::FileSystemDetector(QObject *parent)
    : QObject(parent)
{
}

QStringList FileSystemDetector::detectFilesystems()
{
    QStringList fileList;
    
    // Scan for local .fs files
    fileList.append(scanLocalFilesystems());
    
    // Scan mount points for external drives
    fileList.append(scanMountPoints());
    
    // Scan /proc/mounts for additional filesystems
    fileList.append(scanProcMounts());
    
    return fileList;
}

QStringList FileSystemDetector::scanLocalFilesystems()
{
    QStringList result;
    
    // Look for *.fs files in the current directory
    QDir dir(".");
    QStringList filters;
    filters << "*.fs";
    result = dir.entryList(filters, QDir::Files);
    
    // Log the current directory and files found
    qDebug() << "Checking for filesystems in directory: " << QDir::currentPath();
    qDebug() << "Found local fs files: " << result;
    
    return result;
}

QStringList FileSystemDetector::scanMountPoints()
{
    QStringList result;
    QStringList externalDrives;
    
    // Common mount points for external drives on Linux
    QStringList mountPaths = {"/media", "/mnt", "/run/media/" + QString(qgetenv("USER"))};
    
    // Check each mount point for drives
    for (const QString &mountPath : mountPaths) {
        QDir mountDir(mountPath);
        if (mountDir.exists()) {
            // First level - usually user name on some distros
            QStringList entries = mountDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &entry : entries) {
                QDir subDir(mountPath + "/" + entry);
                // Second level - the actual devices on some distros
                QStringList devices = subDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QString &device : devices) {
                    externalDrives << mountPath + "/" + entry + "/" + device;
                }
                // Also add first level (for systems that mount directly to /media/devicename)
                externalDrives << mountPath + "/" + entry;
            }
        }
    }
    
    // Add to the file list with a special prefix to identify as external drive
    for (const QString &drive : externalDrives) {
        QFileInfo driveInfo(drive);
        if (driveInfo.exists() && driveInfo.isReadable()) {
            result << "EXTERNAL:" + drive;
            qDebug() << "Found external drive: " << drive;
        }
    }
    
    return result;
}

QStringList FileSystemDetector::scanProcMounts()
{
    QStringList result;
    
    // Check /proc/mounts to find all mounted filesystems
    QFile mountsFile("/proc/mounts");
    if (mountsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&mountsFile);
        QString line;
        while (in.readLineInto(&line)) {
            QStringList parts = line.split(" ");
            if (parts.size() >= 2) {
                QString devicePath = parts[0];
                QString mountPoint = parts[1];
                
                // Skip system mounts, only interested in user-mountable devices
                if (devicePath.startsWith("/dev/sd") || 
                    devicePath.startsWith("/dev/nvme") || 
                    devicePath.startsWith("/dev/usb") ||
                    devicePath.contains("UUID=")) {
                    
                    // Add to the list with a special prefix
                    if (!mountPoint.startsWith("/boot") &&
                        !mountPoint.startsWith("/snap") &&
                        !mountPoint.startsWith("/var") &&
                        mountPoint != "/" && 
                        QFileInfo(mountPoint).exists()) {
                        result << "EXTERNAL:" + mountPoint;
                        qDebug() << "Found mounted filesystem: " << devicePath << "at" << mountPoint;
                    }
                }
            }
        }
        mountsFile.close();
    }
    
    return result;
}

bool FileSystemDetector::isExternalPath(const QString &path)
{
    return path.startsWith("EXTERNAL:");
}

QString FileSystemDetector::getDisplayNameForPath(const QString &path)
{
    if (isExternalPath(path)) {
        QString realPath = extractRealPath(path);
        return "External: " + QFileInfo(realPath).fileName();
    }
    return path;
}

QString FileSystemDetector::extractRealPath(const QString &path)
{
    if (isExternalPath(path)) {
        return path.mid(9); // Remove "EXTERNAL:" prefix
    }
    return path;
}
