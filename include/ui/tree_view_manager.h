#ifndef TREE_VIEW_MANAGER_H
#define TREE_VIEW_MANAGER_H

#include "core/filesystem.h"
#include <QDockWidget>
#include <QObject>
#include <QStandardItemModel>
#include <QTreeView>
#include <memory>

/**
 * @brief Manager for the filesystem tree view
 */
class TreeViewManager : public QObject {
    Q_OBJECT

  public:
    /**
     * @brief Constructor
     * @param parent Parent object
     * @param parentWidget Widget that will contain the dock
     */
    explicit TreeViewManager(QObject *parent = nullptr, QWidget *parentWidget = nullptr);

    /**
     * @brief Set the filesystem to use
     * @param filesystem Pointer to the filesystem
     */
    void setFileSystem(FileSystem *filesystem);

    /**
     * @brief Refresh the tree view
     */
    void refreshTreeView();

    /**
     * @brief Toggle the visibility of the tree view
     */
    void toggleVisibility();

    /**
     * @brief Get the dock widget containing the tree view
     * @return The dock widget
     */
    QDockWidget *getDockWidget() const;

  signals:
    /**
     * @brief Signal emitted when a directory is selected in the tree
     * @param path The path of the selected directory
     */
    void directorySelected(const std::string &path);

  private:
    QDockWidget *treeDock;
    QTreeView *treeView;
    QStandardItemModel *directoryModel;
    FileSystem *fs;

    void setupTreeView(QWidget *parentWidget);
    void buildDirectoryTree(QStandardItem *parentItem, int parent_inode,
                            const std::string &parent_path);

  private slots:
    void onTreeViewClicked(const QModelIndex &index);
};

#endif // TREE_VIEW_MANAGER_H
