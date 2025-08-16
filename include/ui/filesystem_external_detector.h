#ifndef FILESYSTEM_EXTERNAL_DETECTOR_H
#define FILESYSTEM_EXTERNAL_DETECTOR_H

#include <QString>
#include <QStringList>
#include <QObject>
#include <QSet>

/**
 * @brief Class for detecting external drives and filesystems
 */
class FilesystemExternalDetector : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Constructor
     * @param parent Parent object
     */
    explicit FilesystemExternalDetector(QObject* parent = nullptr);
    
    /**
     * @brief Scan standard mount points for external drives
     * @return List of external drives found, prefixed with "EXTERNAL:"
     */
    QStringList scanMountPoints();
    
    /**
     * @brief Scan /proc/mounts file for mounted filesystems
     * @return List of mounted filesystems, prefixed with "EXTERNAL:"
     */
    QStringList scanProcMounts();
    
    /**
     * @brief Use lsblk command to detect block devices
     * @return List of block devices found, prefixed with "EXTERNAL:"
     */
    QStringList scanWithLsblk();
    
    /**
     * @brief Manually scan for block devices if lsblk fails
     * @return List of block devices found, prefixed with "EXTERNAL:"
     */
    QStringList scanManualBlockDevices();
    
    /**
     * @brief Check if a path is likely an external device
     * @param path The path to check
     * @return True if it's likely an external device
     */
    static bool isExternalDevice(const QString &path);
    
private:
    /**
     * @brief Helper method to check a device path and add it to the result if it's mounted
     * @param devicePath The device path to check
     * @param result The result list to add to
     * @param addedMountPoints Set of already added mount points to avoid duplicates
     */
    void checkAndAddDevice(const QString &devicePath, QStringList &result, QSet<QString> &addedMountPoints);
};

#endif // FILESYSTEM_EXTERNAL_DETECTOR_H
