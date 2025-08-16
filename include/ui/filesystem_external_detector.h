#ifndef FILESYSTEM_EXTERNAL_DETECTOR_H
#define FILESYSTEM_EXTERNAL_DETECTOR_H

#include <QString>
#include <QStringList>
#include <QObject>
#include <QSet>
#include <QPair>
#include <QMap>

/**
 * @brief Struct to hold device information
 */
struct ExternalDeviceInfo {
    QString devicePath;      ///< Path to the device (/dev/sdb1, etc.)
    QString mountPoint;      ///< Where the device is mounted
    QString label;           ///< Label of the device 
    QString size;            ///< Size of the device
    QString fsType;          ///< Filesystem type (vfat, ntfs, ext4, etc.)
    QString vendor;          ///< Device vendor if available
    QString model;           ///< Device model if available
    QString serialNumber;    ///< Serial number if available
    bool isRemovable;        ///< Whether device is marked as removable
    bool isUsb;              ///< Whether it's a USB device
    
    /**
     * @brief Get a user-friendly display name for the device
     * @return A formatted name including relevant device info
     */
    QString getDisplayName() const;
    
    /**
     * @brief Constructor with default values
     */
    ExternalDeviceInfo() : isRemovable(false), isUsb(false) {}
};

/**
 * @brief Class for detecting external drives and filesystems
 */
class FilesystemExternalDetector : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Device types that can be detected
     */
    enum DeviceType {
        All,            ///< All device types
        UsbDrives,      ///< Only USB drives
        HardDrives,     ///< Only external hard drives
        OpticalDrives,  ///< Only optical drives (CD/DVD)
        SdCards         ///< Only SD cards
    };
    
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
     * @brief Scan specifically for USB drives using various system commands
     * @return List of USB drives found, prefixed with "EXTERNAL:"
     */
    QStringList scanForUsbDrives();
    
    /**
     * @brief Scan for hard drives using direct device enumeration
     * @return List of hard drives found, prefixed with "EXTERNAL:"
     */
    QStringList scanForHardDrives();
    
    /**
     * @brief Scan for SD cards using specialized detection methods
     * @return List of SD cards found, prefixed with "EXTERNAL:"
     */
    QStringList scanForSdCards();
    
    /**
     * @brief Scan for optical drives (CD/DVD) 
     * @return List of optical drives found, prefixed with "EXTERNAL:"
     */
    QStringList scanForOpticalDrives();
    
    /**
     * @brief Get detailed information about all detected external devices
     * @param types Types of devices to detect (default: All)
     * @return Map of device paths to their detailed information
     */
    QMap<QString, ExternalDeviceInfo> getDetailedDeviceInfo(DeviceType types = All);
    
    /**
     * @brief Get information about a specific external device path
     * @param devicePath Path to the device or its mount point
     * @return Detailed information about the device
     */
    ExternalDeviceInfo getDeviceInfo(const QString &devicePath);
    
    /**
     * @brief Attempt to mount an unmounted external device
     * @param devicePath Path to the device (/dev/sdX)
     * @param mountPoint Optional mount point (if empty, system will choose)
     * @return Mount point if successful, empty string if failed
     */
    QString mountExternalDevice(const QString &devicePath, const QString &mountPoint = QString());
    
    /**
     * @brief Safely unmount an external device
     * @param mountPoint The mount point to unmount
     * @return True if unmounted successfully
     */
    bool unmountExternalDevice(const QString &mountPoint);
    
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
    
    /**
     * @brief Parse device information from lsblk JSON output
     * @param jsonOutput The JSON output from lsblk
     * @return Map of device paths to their information
     */
    QMap<QString, ExternalDeviceInfo> parseDeviceInfoFromLsblk(const QByteArray &jsonOutput);
    
    /**
     * @brief Get additional device information using udevadm
     * @param info Reference to device info structure to populate
     * @return True if information was successfully retrieved
     */
    bool enrichDeviceInfoWithUdev(ExternalDeviceInfo &info);
    
    /**
     * @brief Check if a device is currently mounted
     * @param devicePath Path to check
     * @return Mount point if mounted, empty string otherwise
     */
    QString findMountPointForDevice(const QString &devicePath);
};

#endif // FILESYSTEM_EXTERNAL_DETECTOR_H
