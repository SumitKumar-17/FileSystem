#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include "core/filesystem.h"
#include "core/fsck.h"
#include "core/search.h"
#include "core/quota.h"
#include "core/snapshot.h"
#include "ui/filesystem_detector.h"
#include "ui/tree_view_manager.h"
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>

namespace Ui {
class MainWindow;
}

class QListWidgetItem;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    // Accessor for the FileSystem object
    FileSystem* getFileSystem() { return fs.get(); }
    
    // Method to set a new FileSystem
    void setFileSystem(FileSystem* newFs) { 
        fs.reset(newFs); 
    }
    
    // Update status bar with a message
    void updateStatusBar(const QString &message);
    
    // Accessor for the UI
    Ui::MainWindow* getUI() { return ui; }
    
    // Accessors for other components
    FileSystemCheck* getFsCheck() { return fsck.get(); }
    FileSystemSearch* getSearch() { return search.get(); }
    QuotaManager* getQuotaManager() { return quotaManager.get(); }
    SnapshotManager* getSnapshotManager() { return snapshotManager.get(); }
    FileSystemDetector* getFsDetector() { return fsDetector.get(); }
    
    // Accessor for current open file
    std::string getCurrentOpenFile() { return current_open_file; }
    void setCurrentOpenFile(const std::string& path) { current_open_file = path; }
    
    // UI operations that need to be accessible to other classes
    void refreshFileList();

private slots:
    void on_formatButton_clicked();
    void on_mountButton_clicked();
    void on_fileListWidget_itemDoubleClicked(QListWidgetItem *item);
    void on_saveButton_clicked();
    void on_mkdirButton_clicked();
    void on_createFileButton_clicked();
    void on_fileListWidget_customContextMenuRequested(const QPoint &pos);
    void on_actionFsCheck_triggered();
    void on_actionSearch_triggered();
    void on_actionQuotaManager_triggered();
    void on_actionSnapshots_triggered();
    void on_actionTreeView_triggered();
    void on_actionDetectFilesystems_triggered();
    void on_searchButton_clicked();
    void checkAvailableFilesystems();
    void updateAvailableFilesystemsList();
    void onDirectorySelected(const std::string &path);
    void refreshTreeView();
    void on_actionFsCheckAndFix_triggered();
    void on_actionCreateLostFound_triggered();

    // Override for drag and drop support
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void setupMenus();
    void setupFsToolbar();

    Ui::MainWindow *ui;
    
    std::unique_ptr<FileSystem> fs;
    std::unique_ptr<FileSystemCheck> fsck;
    std::unique_ptr<FileSystemSearch> search;
    std::unique_ptr<QuotaManager> quotaManager;
    std::unique_ptr<SnapshotManager> snapshotManager;
    
    std::unique_ptr<FileSystemDetector> fsDetector;
    std::unique_ptr<TreeViewManager> treeViewManager;

    QTimer *fsDetectionTimer;
    QStringList availableFilesystems;

    std::string current_open_file;
};

#endif // MAINWINDOW_H
