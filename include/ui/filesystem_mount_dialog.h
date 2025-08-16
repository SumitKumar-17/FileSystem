#ifndef FILESYSTEM_MOUNT_DIALOG_H
#define FILESYSTEM_MOUNT_DIALOG_H

#include <QBrush>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QProcess>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>

/**
 * @brief Dialog for selecting and mounting a filesystem
 */
class FileSystemMountDialog : public QDialog {
    Q_OBJECT

  public:
    /**
     * @brief Constructor
     * @param availableFilesystems List of available filesystems
     * @param parent Parent widget
     */
    explicit FileSystemMountDialog(const QStringList &availableFilesystems,
                                   QWidget *parent = nullptr);

    /**
     * @brief Get the selected filesystem path
     * @return The selected filesystem path
     */
    QString getSelectedFilesystem() const;

  private:
    QListWidget *fsListWidget;
    QString selectedFilesystem;

    void setupUI();
    void populateFilesystemList(const QStringList &availableFilesystems);

    /**
     * @brief Format a size in bytes to a human-readable string
     * @param bytes Size in bytes
     * @return Formatted size string (e.g., "1.23 MB")
     */
    QString formatSize(qint64 bytes);

    /**
     * @brief Get size information for a mounted device
     * @param mountPoint The mount point of the device
     * @return Size information string or empty string if not available
     */
    QString getDeviceSizeInfo(const QString &mountPoint);

  private slots:
    void onAccepted();
};

#endif // FILESYSTEM_MOUNT_DIALOG_H
