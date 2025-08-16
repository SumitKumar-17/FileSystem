#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include "core/filesystem.h"
#include "core/fsck.h"
#include "core/search.h"
#include "core/quota.h"
#include "core/snapshot.h"
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QTreeView>
#include <QFileSystemModel>
#include <QStandardItemModel>
#include <QDockWidget>
#include <QTimer>

namespace Ui {
class MainWindow;
}

class QListWidgetItem;
class QStandardItem;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

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
    void on_treeView_clicked(const QModelIndex &index);
    void on_searchButton_clicked();
    void checkAvailableFilesystems();

private:
    void refreshFileList();
    void setupTreeView();
    void refreshTreeView();
    void setupMenus();
    void updateAvailableFilesystemsList();
    void setupFsToolbar();
    void buildDirectoryTree(QStandardItem *parentItem, int parent_inode, const std::string &parent_path);
    
    // Override for drag and drop support
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    Ui::MainWindow *ui;
    
    std::unique_ptr<FileSystem> fs;
    std::unique_ptr<FileSystemCheck> fsck;
    std::unique_ptr<FileSystemSearch> search;
    std::unique_ptr<QuotaManager> quotaManager;
    std::unique_ptr<SnapshotManager> snapshotManager;

    QStandardItemModel *directoryModel;
    QTreeView *treeView;
    QDockWidget *treeDock;
    QTimer *fsDetectionTimer;
    QStringList availableFilesystems;

    std::string current_open_file;
};

#endif // MAINWINDOW_H
