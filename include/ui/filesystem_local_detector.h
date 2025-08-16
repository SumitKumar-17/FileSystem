#ifndef FILESYSTEM_LOCAL_DETECTOR_H
#define FILESYSTEM_LOCAL_DETECTOR_H

#include <QString>
#include <QStringList>
#include <QObject>
#include <QSet>

/**
 * @brief Class for detecting local filesystem images
 */
class FilesystemLocalDetector : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Constructor
     * @param parent Parent object
     */
    explicit FilesystemLocalDetector(QObject* parent = nullptr);
    
    /**
     * @brief Scan for local .fs files in multiple directories
     * @return List of .fs files found
     */
    QStringList scanLocalFilesystems();
    
private:
    /**
     * @brief Check if a directory contains filesystem images
     * @param dirPath Path to the directory
     * @return List of filesystem images found
     */
    QStringList scanDirectory(const QString& dirPath);
};

#endif // FILESYSTEM_LOCAL_DETECTOR_H
