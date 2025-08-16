#ifndef FILESYSTEM_DETECTOR_H
#define FILESYSTEM_DETECTOR_H

#include <QString>
#include <QStringList>
#include <QObject>

/**
 * @brief The FileSystemDetector class handles detection of available filesystem images and external drives
 */
class FileSystemDetector : public QObject
{
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
    QStringList scanLocalFilesystems();
    QStringList scanMountPoints();
    QStringList scanProcMounts();
};

#endif // FILESYSTEM_DETECTOR_H
