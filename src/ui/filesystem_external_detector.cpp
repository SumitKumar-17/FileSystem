#include "ui/filesystem_external_detector.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>

// Implementation of ExternalDeviceInfo::getDisplayName
QString ExternalDeviceInfo::getDisplayName() const {
    QString displayName;

    // Determine device type prefix
    if (isUsb) {
        displayName = "USB Drive: ";
    } else if (devicePath.contains("sr") || devicePath.contains("cdrom")) {
        displayName = "Optical Drive: ";
    } else if (isRemovable) {
        displayName = "Removable Drive: ";
    } else {
        displayName = "External Drive: ";
    }

    // Add label if available
    if (!label.isEmpty()) {
        displayName += label;
    } else if (!model.isEmpty()) {
        // Add vendor and model if available
        if (!vendor.isEmpty()) {
            displayName += vendor + " ";
        }
        displayName += model;
    } else {
        // Fallback to device path basename
        int lastSlash = devicePath.lastIndexOf('/');
        if (lastSlash != -1 && lastSlash < devicePath.length() - 1) {
            displayName += devicePath.mid(lastSlash + 1);
        } else {
            displayName += "Unknown";
        }
    }

    // Add size if available
    if (!size.isEmpty()) {
        displayName += " (" + size + ")";
    }

    // Add filesystem type if available
    if (!fsType.isEmpty()) {
        displayName += " [" + fsType + "]";
    }

    return displayName;
}

FilesystemExternalDetector::FilesystemExternalDetector(QObject *parent) : QObject(parent) {
}

QStringList FilesystemExternalDetector::scanMountPoints() {
    QStringList result;
    QSet<QString> addedMountPoints;

    // Standard mount points for external drives
    QStringList standardMountPoints = {"/media", "/media/" + qgetenv("USER"),
                                       "/run/media/" + qgetenv("USER"), "/mnt"};

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

        // Check if this is an external device
        if (isExternalDevice(mountPoint)) {
            result.append("EXTERNAL:" + mountPoint);
            addedMountPoints.insert(mountPoint);
        }
    }

    return result;
}

QStringList FilesystemExternalDetector::scanWithLsblk() {
    QStringList result;
    QSet<QString> addedMountPoints;

    // Run lsblk to get all block devices with mount points
    QProcess process;
    process.start("lsblk", QStringList()
                               << "-o" << "NAME,MOUNTPOINT,HOTPLUG,RM,TYPE" << "-n" << "-p");

    if (!process.waitForFinished(3000)) {
        qDebug() << "lsblk command failed or timed out";
        return result;
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QStringList parts = line.simplified().split(' ');
        if (parts.isEmpty())
            continue;

        QString devicePath = parts[0];

        // Check if this is a disk or partition (skip loops, etc.)
        if (line.contains("disk") || line.contains("part")) {
            // Find mount point if present
            QString mountPoint;
            bool isRemovable = false;

            // Check if there's a mountpoint in the output
            for (int i = 1; i < parts.size(); ++i) {
                if (parts[i].startsWith("/")) {
                    mountPoint = parts[i];
                    break;
                }
            }

            // Check if it's removable/hotplug
            isRemovable =
                line.contains("1") && (line.contains("hotplug") || line.contains("removable"));

            if (!mountPoint.isEmpty() && isRemovable && !addedMountPoints.contains(mountPoint)) {
                if (isExternalDevice(mountPoint)) {
                    result.append("EXTERNAL:" + mountPoint);
                    addedMountPoints.insert(mountPoint);
                }
            } else if (isRemovable) {
                // It's removable but not mounted - check the device
                QProcess blkidProcess;
                blkidProcess.start("blkid", QStringList()
                                                << "-o" << "value" << "-s" << "TYPE" << devicePath);

                if (blkidProcess.waitForFinished(1000)) {
                    QString fsType =
                        QString::fromUtf8(blkidProcess.readAllStandardOutput()).trimmed();

                    if (!fsType.isEmpty() && !addedMountPoints.contains(devicePath)) {
                        result.append("UNMOUNTED:" + devicePath);
                        addedMountPoints.insert(devicePath);
                    }
                }
            }
        }
    }

    return result;
}

QStringList FilesystemExternalDetector::scanManualBlockDevices() {
    QStringList result;
    QSet<QString> addedMountPoints;

    // Check common block device patterns
    QStringList devicePatterns = {
        "/dev/sd[a-z][1-9]*",          // SATA/USB drives
        "/dev/nvme[0-9]n[0-9]p[0-9]*", // NVMe drives
        "/dev/mmcblk[0-9]p[0-9]*"      // SD cards
    };

    for (const QString &pattern : devicePatterns) {
        QStringList globResults;

        // Simple glob implementation
        QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(pattern));
        QDir devDir("/dev");
        QStringList entries = devDir.entryList(QDir::System);

        for (const QString &entry : entries) {
            QString path = "/dev/" + entry;
            if (regex.match(path).hasMatch()) {
                globResults.append(path);
            }
        }

        for (const QString &devicePath : globResults) {
            checkAndAddDevice(devicePath, result, addedMountPoints);
        }
    }

    return result;
}

QStringList FilesystemExternalDetector::scanForUsbDrives() {
    QStringList result;
    QSet<QString> addedMountPoints;

    qDebug() << "Scanning specifically for USB drives...";

    // Method 1: Direct check for USB block devices through sysfs
    QDir sysBlockDir("/sys/block");
    QStringList blockDevices =
        sysBlockDir.entryList(QStringList() << "sd*" << "nvme*n*", QDir::Dirs);

    for (const QString &block : blockDevices) {
        // Check if this is a USB device by looking at the device path
        QString usbPath = QString("/sys/block/%1/device/../../usb").arg(block);
        QFileInfo usbInfo(usbPath);
        if (usbInfo.exists() || usbInfo.isSymLink()) {
            qDebug() << "Found USB storage device:" << block;

            // Check for all partitions of this device
            QDir blockDir(QString("/sys/block/%1").arg(block));
            QStringList partitions =
                blockDir.entryList(QStringList() << QString("%1*").arg(block), QDir::Dirs);

            // If no partitions are found, check the device itself
            if (partitions.isEmpty()) {
                QString devPath = QString("/dev/%1").arg(block);
                checkAndAddDevice(devPath, result, addedMountPoints);
            } else {
                for (const QString &partition : partitions) {
                    // Skip the base device which we already checked
                    if (partition == block)
                        continue;

                    QString devPath = QString("/dev/%1").arg(partition);
                    checkAndAddDevice(devPath, result, addedMountPoints);
                }
            }
        }
    }

    // Method 2: Look directly in /dev/disk/by-id/ for USB devices
    QDir diskByIdDir("/dev/disk/by-id");
    if (diskByIdDir.exists()) {
        QStringList usbEntries = diskByIdDir.entryList(QStringList() << "usb-*", QDir::System);

        for (const QString &entry : usbEntries) {
            QString devicePath = "/dev/disk/by-id/" + entry;
            QString realPath = QFileInfo(devicePath).symLinkTarget();

            // Only add partitions to avoid duplicates
            if (realPath.contains("sd") && QChar(realPath.at(realPath.length() - 1)).isDigit()) {
                checkAndAddDevice(realPath, result, addedMountPoints);
            }
        }
    }

    // Method 3: Use lsblk with JSON output to find USB devices
    QProcess lsblkProcess;
    lsblkProcess.start("lsblk", QStringList() << "-J" << "-o" << "NAME,MOUNTPOINT,TRAN,HOTPLUG,RM");

    if (lsblkProcess.waitForFinished(3000)) {
        QByteArray output = lsblkProcess.readAllStandardOutput();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(output);

        if (!jsonDoc.isNull() && jsonDoc.isObject()) {
            QJsonArray devices = jsonDoc.object()["blockdevices"].toArray();

            for (const QJsonValue &deviceVal : devices) {
                QJsonObject device = deviceVal.toObject();
                QString name = device["name"].toString();
                QString tran = device["tran"].toString();
                int hotplug = device["hotplug"].toInt();
                int removable = device["rm"].toInt();

                // Check if this is a USB device
                if (tran == "usb" || (hotplug == 1 && removable == 1)) {
                    qDebug() << "Found USB device via lsblk:" << name;

                    // Check for partitions
                    if (device.contains("children")) {
                        QJsonArray children = device["children"].toArray();

                        for (const QJsonValue &childVal : children) {
                            QJsonObject child = childVal.toObject();
                            QString childName = child["name"].toString();
                            QString mountpoint = child["mountpoint"].toString();

                            // Add both mounted and unmounted partitions
                            if (!mountpoint.isNull() && !mountpoint.isEmpty()) {
                                if (!addedMountPoints.contains(mountpoint)) {
                                    result.append("EXTERNAL:" + mountpoint);
                                    addedMountPoints.insert(mountpoint);
                                    qDebug() << "Added USB drive mount point:" << mountpoint;
                                }
                            } else {
                                // Add unmounted device
                                QString devicePath = QString("/dev/%1").arg(childName);
                                if (!addedMountPoints.contains(devicePath)) {
                                    result.append("UNMOUNTED:" + devicePath);
                                    addedMountPoints.insert(devicePath);
                                    qDebug() << "Added unmounted USB drive:" << devicePath;
                                }
                            }
                        }
                    } else {
                        // No partitions, check if the device itself is mounted
                        QString mountpoint = device["mountpoint"].toString();
                        if (!mountpoint.isNull() && !mountpoint.isEmpty()) {
                            if (!addedMountPoints.contains(mountpoint)) {
                                result.append("EXTERNAL:" + mountpoint);
                                addedMountPoints.insert(mountpoint);
                                qDebug() << "Added USB drive mount point:" << mountpoint;
                            }
                        } else {
                            // Device is not mounted, add it as unmounted
                            QString devicePath = QString("/dev/%1").arg(name);
                            if (!addedMountPoints.contains(devicePath)) {
                                result.append("UNMOUNTED:" + devicePath);
                                addedMountPoints.insert(devicePath);
                                qDebug() << "Added unmounted USB drive:" << devicePath;
                            }
                        }
                    }
                }
            }
        }
    }

    qDebug() << "Found" << result.size() << "USB drives";
    return result;
}

QStringList FilesystemExternalDetector::scanForHardDrives() {
    QStringList result;
    QSet<QString> addedMountPoints;

    qDebug() << "Scanning for external hard drives...";

    // Method 1: Use lsblk to find non-USB removable drives
    QProcess lsblkProcess;
    lsblkProcess.start("lsblk", QStringList()
                                    << "-J" << "-o" << "NAME,MOUNTPOINT,TRAN,HOTPLUG,RM,TYPE");

    if (lsblkProcess.waitForFinished(3000)) {
        QByteArray output = lsblkProcess.readAllStandardOutput();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(output);

        if (!jsonDoc.isNull() && jsonDoc.isObject()) {
            QJsonArray devices = jsonDoc.object()["blockdevices"].toArray();

            for (const QJsonValue &deviceVal : devices) {
                QJsonObject device = deviceVal.toObject();
                QString name = device["name"].toString();
                QString tran = device["tran"].toString();
                QString type = device["type"].toString();
                int hotplug = device["hotplug"].toInt();
                int removable = device["rm"].toInt();

                // Check if this is a hard drive (not USB, not internal)
                if (type == "disk" && tran != "usb" && hotplug == 1) {
                    qDebug() << "Found potential external hard drive:" << name;

                    // Process similar to USB drives
                    if (device.contains("children")) {
                        QJsonArray children = device["children"].toArray();

                        for (const QJsonValue &childVal : children) {
                            QJsonObject child = childVal.toObject();
                            QString childName = child["name"].toString();
                            QString mountpoint = child["mountpoint"].toString();

                            if (!mountpoint.isNull() && !mountpoint.isEmpty()) {
                                if (!addedMountPoints.contains(mountpoint)) {
                                    result.append("EXTERNAL:" + mountpoint);
                                    addedMountPoints.insert(mountpoint);
                                }
                            } else {
                                QString devicePath = QString("/dev/%1").arg(childName);
                                checkAndAddDevice(devicePath, result, addedMountPoints);
                            }
                        }
                    } else {
                        QString mountpoint = device["mountpoint"].toString();
                        if (!mountpoint.isNull() && !mountpoint.isEmpty()) {
                            if (!addedMountPoints.contains(mountpoint)) {
                                result.append("EXTERNAL:" + mountpoint);
                                addedMountPoints.insert(mountpoint);
                            }
                        } else {
                            QString devicePath = QString("/dev/%1").arg(name);
                            checkAndAddDevice(devicePath, result, addedMountPoints);
                        }
                    }
                }
            }
        }
    }

    // Method 2: Check for eSATA and external drives via /sys
    QDir sysBlockDir("/sys/block");
    QStringList blockDevices = sysBlockDir.entryList(QStringList() << "sd*", QDir::Dirs);

    for (const QString &block : blockDevices) {
        // Skip devices we've already identified as USB
        QString usbPath = QString("/sys/block/%1/device/../../usb").arg(block);
        QFileInfo usbInfo(usbPath);
        if (usbInfo.exists() || usbInfo.isSymLink()) {
            continue; // Skip USB devices
        }

        // Check if it's removable
        QFile removableFile(QString("/sys/block/%1/removable").arg(block));
        if (removableFile.open(QIODevice::ReadOnly)) {
            QString removable = QString::fromUtf8(removableFile.readAll()).trimmed();
            if (removable == "1") {
                qDebug() << "Found removable non-USB device:" << block;

                // Add the device and its partitions
                QDir blockDir(QString("/sys/block/%1").arg(block));
                QStringList partitions =
                    blockDir.entryList(QStringList() << QString("%1*").arg(block), QDir::Dirs);

                if (partitions.isEmpty()) {
                    QString devicePath = QString("/dev/%1").arg(block);
                    checkAndAddDevice(devicePath, result, addedMountPoints);
                } else {
                    for (const QString &partition : partitions) {
                        if (partition == block)
                            continue;

                        QString devicePath = QString("/dev/%1").arg(partition);
                        checkAndAddDevice(devicePath, result, addedMountPoints);
                    }
                }
            }
        }
    }

    qDebug() << "Found" << result.size() << "external hard drives";
    return result;
}

QStringList FilesystemExternalDetector::scanForSdCards() {
    QStringList result;
    QSet<QString> addedMountPoints;

    // Method 1: Check for mmcblk devices which are typically SD cards
    QDir sysBlockDir("/sys/block");
    QStringList mmcDevices = sysBlockDir.entryList(QStringList() << "mmcblk*", QDir::Dirs);

    for (const QString &device : mmcDevices) {
        // Skip internal eMMC storage
        QFile ueventFile(QString("/sys/block/%1/device/uevent").arg(device));
        bool isInternal = false;

        if (ueventFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString ueventContent = QString::fromUtf8(ueventFile.readAll());
            isInternal = ueventContent.contains("MMC_TYPE=MMC") ||
                         ueventContent.contains("DRIVER=mmcblk") ||
                         !ueventContent.contains("DRIVER=mmc_host");
            ueventFile.close();
        }

        if (!isInternal) {
            qDebug() << "Found potential SD card:" << device;

            // Check for partitions
            QDir blockDir(QString("/sys/block/%1").arg(device));
            QStringList partitions =
                blockDir.entryList(QStringList() << QString("%1p*").arg(device), QDir::Dirs);

            if (partitions.isEmpty()) {
                QString devicePath = QString("/dev/%1").arg(device);
                checkAndAddDevice(devicePath, result, addedMountPoints);
            } else {
                for (const QString &partition : partitions) {
                    QString devicePath = QString("/dev/%1").arg(partition);
                    checkAndAddDevice(devicePath, result, addedMountPoints);
                }
            }
        }
    }

    // Method 2: Use lsblk to identify SD cards
    QProcess lsblkProcess;
    lsblkProcess.start("lsblk", QStringList()
                                    << "-J" << "-o" << "NAME,MOUNTPOINT,TRAN,HOTPLUG,RM,TYPE");

    if (lsblkProcess.waitForFinished(3000)) {
        QByteArray output = lsblkProcess.readAllStandardOutput();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(output);

        if (!jsonDoc.isNull() && jsonDoc.isObject()) {
            QJsonArray devices = jsonDoc.object()["blockdevices"].toArray();

            for (const QJsonValue &deviceVal : devices) {
                QJsonObject device = deviceVal.toObject();
                QString name = device["name"].toString();

                // SD cards typically have mmcblk in their name
                if (name.startsWith("mmcblk")) {
                    // Similar processing to what we've done for other device types
                    if (device.contains("children")) {
                        QJsonArray children = device["children"].toArray();

                        for (const QJsonValue &childVal : children) {
                            QJsonObject child = childVal.toObject();
                            QString childName = child["name"].toString();
                            QString mountpoint = child["mountpoint"].toString();

                            if (!mountpoint.isNull() && !mountpoint.isEmpty()) {
                                if (!addedMountPoints.contains(mountpoint)) {
                                    result.append("EXTERNAL:" + mountpoint);
                                    addedMountPoints.insert(mountpoint);
                                }
                            } else {
                                QString devicePath = QString("/dev/%1").arg(childName);
                                if (!addedMountPoints.contains(devicePath)) {
                                    result.append("UNMOUNTED:" + devicePath);
                                    addedMountPoints.insert(devicePath);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return result;
}

QStringList FilesystemExternalDetector::scanForOpticalDrives() {
    QStringList result;
    QSet<QString> addedMountPoints;

    // Method 1: Check for sr* devices which are typically optical drives
    QDir sysBlockDir("/sys/block");
    QStringList opticalDevices = sysBlockDir.entryList(QStringList() << "sr*", QDir::Dirs);

    for (const QString &device : opticalDevices) {
        qDebug() << "Found optical drive:" << device;

        QString devicePath = QString("/dev/%1").arg(device);

        // Check if mounted
        QString mountPoint = findMountPointForDevice(devicePath);

        if (!mountPoint.isEmpty()) {
            if (!addedMountPoints.contains(mountPoint)) {
                result.append("EXTERNAL:" + mountPoint);
                addedMountPoints.insert(mountPoint);
            }
        } else {
            // Add unmounted optical drive
            if (!addedMountPoints.contains(devicePath)) {
                result.append("UNMOUNTED:" + devicePath);
                addedMountPoints.insert(devicePath);
            }
        }
    }

    // Method 2: Check for common optical drive mount points
    QStringList opticalMountPoints = {"/media/cdrom", "/media/cdrom0", "/media/dvd", "/media/dvdrw",
                                      "/mnt/cdrom"};

    for (const QString &mountPoint : opticalMountPoints) {
        QFileInfo info(mountPoint);
        if (info.exists() && info.isDir() && !addedMountPoints.contains(mountPoint)) {
            result.append("EXTERNAL:" + mountPoint);
            addedMountPoints.insert(mountPoint);
        }
    }

    return result;
}

QMap<QString, ExternalDeviceInfo>
FilesystemExternalDetector::getDetailedDeviceInfo(DeviceType types) {
    QMap<QString, ExternalDeviceInfo> result;

    // Use lsblk with JSON output to get detailed device info
    QProcess lsblkProcess;
    lsblkProcess.start(
        "lsblk",
        QStringList() << "-J" << "-o"
                      << "NAME,MOUNTPOINT,TRAN,HOTPLUG,RM,TYPE,SIZE,FSTYPE,LABEL,VENDOR,MODEL");

    if (lsblkProcess.waitForFinished(3000)) {
        QByteArray output = lsblkProcess.readAllStandardOutput();
        result = parseDeviceInfoFromLsblk(output);

        // Enrich device info with udev information
        for (auto it = result.begin(); it != result.end(); ++it) {
            enrichDeviceInfoWithUdev(it.value());
        }

        // Filter based on device type if needed
        if (types != All) {
            QMap<QString, ExternalDeviceInfo> filteredResult;

            for (auto it = result.begin(); it != result.end(); ++it) {
                bool include = false;

                switch (types) {
                    case UsbDrives:
                        include = it.value().isUsb;
                        break;
                    case HardDrives:
                        include = !it.value().isUsb && !it.value().devicePath.contains("sr") &&
                                  !it.value().devicePath.contains("mmcblk");
                        break;
                    case OpticalDrives:
                        include = it.value().devicePath.contains("sr") ||
                                  it.value().devicePath.contains("cdrom");
                        break;
                    case SdCards:
                        include = it.value().devicePath.contains("mmcblk");
                        break;
                    default:
                        include = true;
                }

                if (include) {
                    filteredResult.insert(it.key(), it.value());
                }
            }

            return filteredResult;
        }
    }

    return result;
}

ExternalDeviceInfo FilesystemExternalDetector::getDeviceInfo(const QString &devicePath) {
    // Get all device info and then filter for the specific one we want
    QMap<QString, ExternalDeviceInfo> allDevices = getDetailedDeviceInfo();

    // First check if the exact device path is in the map
    if (allDevices.contains(devicePath)) {
        return allDevices[devicePath];
    }

    // Next check if it's a mount point for a device
    for (auto it = allDevices.begin(); it != allDevices.end(); ++it) {
        if (it.value().mountPoint == devicePath) {
            return it.value();
        }
    }

    // If still not found, create a new entry with basic info from the path
    ExternalDeviceInfo info;
    info.devicePath = devicePath;

    // Try to get more info using blkid
    QProcess blkidProcess;
    blkidProcess.start("blkid", QStringList() << devicePath);

    if (blkidProcess.waitForFinished(1000)) {
        QString output = blkidProcess.readAllStandardOutput();

        if (!output.isEmpty()) {
            // Parse blkid output for basic information
            QRegularExpression labelRegex("LABEL=\"([^\"]+)\"");
            QRegularExpression fsTypeRegex("TYPE=\"([^\"]+)\"");
            QRegularExpression uuidRegex("UUID=\"([^\"]+)\"");

            auto labelMatch = labelRegex.match(output);
            auto fsTypeMatch = fsTypeRegex.match(output);
            auto uuidMatch = uuidRegex.match(output);

            if (labelMatch.hasMatch()) {
                info.label = labelMatch.captured(1);
            }

            if (fsTypeMatch.hasMatch()) {
                info.fsType = fsTypeMatch.captured(1);
            }
        }
    }

    // Try to determine if it's a USB device
    QString sysPath;
    if (devicePath.startsWith("/dev/sd")) {
        QString block = devicePath.mid(5, 3);
        sysPath = QString("/sys/block/%1/device/../../usb").arg(block);
        QFileInfo usbInfo(sysPath);
        info.isUsb = usbInfo.exists() || usbInfo.isSymLink();
    }

    // Check if device is removable
    if (!sysPath.isEmpty()) {
        QString removablePath = sysPath.left(sysPath.indexOf("/device/")) + "/removable";
        QFile removableFile(removablePath);
        if (removableFile.open(QIODevice::ReadOnly)) {
            QString removable = QString::fromUtf8(removableFile.readAll()).trimmed();
            info.isRemovable = (removable == "1");
        }
    }

    return info;
}

QString FilesystemExternalDetector::mountExternalDevice(const QString &devicePath,
                                                        const QString &mountPoint) {
    QString actualMountPoint = mountPoint;

    // Create a mount point if not specified
    if (actualMountPoint.isEmpty()) {
        QTemporaryDir tempDir;
        if (tempDir.isValid()) {
            tempDir.setAutoRemove(false); // Don't auto-remove when object is destroyed
            actualMountPoint = tempDir.path();
        } else {
            // Fallback to a generated temp dir name in /mnt
            QString randomSuffix = QString::number(QRandomGenerator::global()->bounded(100000));
            actualMountPoint = "/mnt/temp_mount_" + randomSuffix;
            QDir().mkpath(actualMountPoint);
        }
    }

    // Determine filesystem type
    QString fsType;
    QProcess blkidProcess;
    blkidProcess.start("blkid", QStringList() << "-o" << "value" << "-s" << "TYPE" << devicePath);

    if (blkidProcess.waitForFinished(1000)) {
        fsType = QString::fromUtf8(blkidProcess.readAllStandardOutput()).trimmed();
    }

    // Build mount command
    QStringList args;
    args << devicePath << actualMountPoint;

    if (!fsType.isEmpty()) {
        args << "-t" << fsType;
    }

    // Add some common mount options
    args << "-o" << "defaults,noatime";

    // Execute mount command
    QProcess mountProcess;
    mountProcess.start("mount", args);

    if (mountProcess.waitForFinished(5000)) {
        if (mountProcess.exitCode() == 0) {
            return actualMountPoint;
        } else {
            qDebug() << "Mount failed:" << QString::fromUtf8(mountProcess.readAllStandardError());

            // Cleanup temp directory if we created one
            if (mountPoint.isEmpty()) {
                QDir().rmpath(actualMountPoint);
            }

            return QString();
        }
    }

    return QString();
}

bool FilesystemExternalDetector::unmountExternalDevice(const QString &mountPoint) {
    QProcess umountProcess;
    umountProcess.start("umount", QStringList() << mountPoint);

    if (umountProcess.waitForFinished(5000)) {
        if (umountProcess.exitCode() == 0) {
            // Check if the mount point was a temporary one and remove it
            if (mountPoint.contains("/temp_mount_") || mountPoint.contains("/tmp/")) {
                QDir().rmpath(mountPoint);
            }
            return true;
        } else {
            qDebug() << "Unmount failed:"
                     << QString::fromUtf8(umountProcess.readAllStandardError());
        }
    }

    return false;
}

bool FilesystemExternalDetector::isExternalDevice(const QString &path) {
    // Skip system directories
    if (path == "/" || path.startsWith("/boot") || path.startsWith("/usr") ||
        path.startsWith("/var") || path.startsWith("/etc") || path.startsWith("/bin") ||
        path.startsWith("/sbin") || path.startsWith("/lib") || path.startsWith("/opt") ||
        path.startsWith("/proc") || path.startsWith("/sys") || path.startsWith("/dev") ||
        path.startsWith("/run") || path.startsWith("/tmp") || path.startsWith("/home")) {
        return false;
    }

    // Check for removable or usb in the output
    QProcess process;
    process.start("findmnt", QStringList() << "-n" << "-o" << "SOURCE" << path);

    if (process.waitForFinished(1000)) {
        QString devicePath = QString::fromUtf8(process.readAllStandardOutput()).trimmed();

        if (!devicePath.isEmpty()) {
            // Check device properties using lsblk
            QProcess lsblkProcess;
            lsblkProcess.start("lsblk", QStringList()
                                            << "-n" << "-o" << "HOTPLUG,RM,TRAN" << devicePath);

            if (lsblkProcess.waitForFinished(1000)) {
                QString output = QString::fromUtf8(lsblkProcess.readAllStandardOutput()).trimmed();

                // If it has hotplug=1, removable=1, or is USB, it's external
                if (output.contains("1") || output.contains("usb", Qt::CaseInsensitive)) {
                    return true;
                }
            }
        }
    }

    // Check common external media patterns
    if (path.contains("/media/") || path.contains("/mnt/") || path.contains("/run/media/")) {
        return true;
    }

    return false;
}

void FilesystemExternalDetector::checkAndAddDevice(const QString &devicePath, QStringList &result,
                                                   QSet<QString> &addedMountPoints) {
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
            qDebug() << "Added already mounted device:" << devicePath
                     << "at mount point:" << mountPoint;
        }
    } else {
        // Device is not mounted yet - let's add it as a device path
        // Get filesystem type to determine if it's a usable filesystem
        QProcess blkidProcess;
        blkidProcess.start("blkid", QStringList()
                                        << "-o" << "value" << "-s" << "TYPE" << devicePath);

        if (blkidProcess.waitForFinished(1000)) {
            QString fsType = QString::fromUtf8(blkidProcess.readAllStandardOutput()).trimmed();

            if (!fsType.isEmpty() &&
                (fsType == "vfat" || fsType == "exfat" || fsType == "ntfs" ||
                 fsType.startsWith("ext") || fsType == "btrfs" || fsType == "xfs" ||
                 fsType == "jfs" || fsType == "hfs" || fsType == "hfsplus" || fsType == "apfs")) {
                // Add as an unmounted device that needs to be mounted
                QString deviceName = QFileInfo(devicePath).fileName();
                QString mountId = "UNMOUNTED:" + devicePath;

                if (!addedMountPoints.contains(mountId)) {
                    result.append(mountId);
                    addedMountPoints.insert(mountId);
                    qDebug() << "Added unmounted device:" << devicePath
                             << "with filesystem:" << fsType;
                }
            }
        }
    }
}

QMap<QString, ExternalDeviceInfo>
FilesystemExternalDetector::parseDeviceInfoFromLsblk(const QByteArray &jsonOutput) {
    QMap<QString, ExternalDeviceInfo> result;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonOutput);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        return result;
    }

    QJsonArray devices = jsonDoc.object()["blockdevices"].toArray();

    for (const QJsonValue &deviceVal : devices) {
        QJsonObject device = deviceVal.toObject();
        QString name = device["name"].toString();
        QString devicePath = QString("/dev/%1").arg(name);

        // Process device info
        ExternalDeviceInfo info;
        info.devicePath = devicePath;
        info.mountPoint = device["mountpoint"].toString();
        info.fsType = device["fstype"].toString();
        info.label = device["label"].toString();
        info.size = device["size"].toString();
        info.vendor = device["vendor"].toString();
        info.model = device["model"].toString();

        // Determine if removable
        if (device.contains("rm")) {
            info.isRemovable = (device["rm"].toInt() == 1);
        }

        // Determine if USB
        if (device.contains("tran")) {
            info.isUsb = (device["tran"].toString() == "usb");
        }

        // Only add if it seems like an external device
        if (info.isRemovable || info.isUsb ||
            (device.contains("hotplug") && device["hotplug"].toInt() == 1)) {
            result.insert(devicePath, info);
        }

        // Process partitions (children)
        if (device.contains("children")) {
            QJsonArray children = device["children"].toArray();

            for (const QJsonValue &childVal : children) {
                QJsonObject child = childVal.toObject();
                QString childName = child["name"].toString();
                QString childPath = QString("/dev/%1").arg(childName);

                ExternalDeviceInfo partInfo;
                partInfo.devicePath = childPath;
                partInfo.mountPoint = child["mountpoint"].toString();
                partInfo.fsType = child["fstype"].toString();
                partInfo.label = child["label"].toString();
                partInfo.size = child["size"].toString();

                // Inherit some properties from parent
                partInfo.vendor = info.vendor;
                partInfo.model = info.model;
                partInfo.isRemovable = info.isRemovable;
                partInfo.isUsb = info.isUsb;

                // Only add if it has a filesystem type
                if (!partInfo.fsType.isEmpty()) {
                    result.insert(childPath, partInfo);
                }
            }
        }
    }

    return result;
}

bool FilesystemExternalDetector::enrichDeviceInfoWithUdev(ExternalDeviceInfo &info) {
    QProcess udevProcess;
    udevProcess.start("udevadm",
                      QStringList() << "info" << "--query=property" << "--name=" + info.devicePath);

    if (!udevProcess.waitForFinished(2000)) {
        return false;
    }

    QString output = QString::fromUtf8(udevProcess.readAllStandardOutput());
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        if (line.startsWith("ID_VENDOR=")) {
            info.vendor = line.mid(10);
        } else if (line.startsWith("ID_MODEL=")) {
            info.model = line.mid(9);
        } else if (line.startsWith("ID_SERIAL=") || line.startsWith("ID_SERIAL_SHORT=")) {
            info.serialNumber = line.contains("ID_SERIAL=") ? line.mid(10) : line.mid(16);
        } else if (line.startsWith("ID_BUS=usb")) {
            info.isUsb = true;
        } else if (line.startsWith("ID_TYPE=cd") || line.contains("CDROM")) {
            // Set info as optical drive
        } else if (line.startsWith("ID_FS_TYPE=")) {
            if (info.fsType.isEmpty()) {
                info.fsType = line.mid(11);
            }
        } else if (line.startsWith("ID_FS_LABEL=")) {
            if (info.label.isEmpty()) {
                info.label = line.mid(12);
            }
        }
    }

    return true;
}

QString FilesystemExternalDetector::findMountPointForDevice(const QString &devicePath) {
    QProcess process;
    process.start("findmnt", QStringList() << "-n" << "-o" << "TARGET" << devicePath);

    if (process.waitForFinished(1000)) {
        return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    }

    return QString();
}
