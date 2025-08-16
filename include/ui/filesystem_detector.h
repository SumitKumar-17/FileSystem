#ifndef FILESYSTEM_DETECTOR_H
#define FILESYSTEM_DETECTOR_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QStringList>
#include <sys/statfs.h>
#include <unistd.h>

/**
 * @brief The FileSystemDetector class handles detection of available filesystem images and external
 * drives
 */
class FileSystemDetector : public QObject {
    Q_OBJECT

  public:
    explicit FileSystemDetector(QObject *parent = nullptr);

    /**
     * @brief Scan for available filesystem images and external drives
     * @return List of available filesystems, external drives are prefixed with "EXTERNAL:"
     */
    QStringList detectFilesystems();

    /**
     * @brief Check if a path is an external filesystem
     * @param path The path to check
     * @return True if it's an external filesystem
     */
    static bool isExternalPath(const QString &path);

    /**
     * @brief Get a human-readable name for an external filesystem path
     * @param path The path to the external filesystem
     * @return A user-friendly name for the filesystem
     */
    static QString getDisplayNameForPath(const QString &path);

    /**
     * @brief Extract the actual path from an EXTERNAL: prefixed path
     * @param path The path with EXTERNAL: prefix
     * @return The actual filesystem path
     */
    static QString extractRealPath(const QString &path);

  private:
    /**
     * @brief Scan for local .fs files in multiple directories
     * @return List of .fs files found
     */
    QStringList scanLocalFilesystems();

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
     * @brief Helper method to check a device path and add it to the result if it's mounted
     * @param devicePath The device path to check
     * @param result The result list to add to
     * @param addedMountPoints Set of already added mount points to avoid duplicates
     */
    void checkAndAddDevice(const QString &devicePath, QStringList &result,
                           QSet<QString> &addedMountPoints);

    /**
     * @brief Helper method to determine if a path is likely an external device
     * @param path The path to check
     * @return True if it's likely an external device
     */
    static bool isExternalDevice(const QString &path);
};

#endif // FILESYSTEM_DETECTOR_H
