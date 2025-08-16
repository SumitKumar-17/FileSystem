#ifndef FILESYSTEM_MOUNT_DIALOG_H
#define FILESYSTEM_MOUNT_DIALOG_H

#include <QDialog>
#include <QStringList>
#include <QString>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>

/**
 * @brief Dialog for selecting and mounting a filesystem
 */
class FileSystemMountDialog : public QDialog
{
    Q_OBJECT
    
public:
    /**
     * @brief Constructor
     * @param availableFilesystems List of available filesystems
     * @param parent Parent widget
     */
    explicit FileSystemMountDialog(const QStringList &availableFilesystems, QWidget *parent = nullptr);
    
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
    
private slots:
    void onAccepted();
};

#endif // FILESYSTEM_MOUNT_DIALOG_H
