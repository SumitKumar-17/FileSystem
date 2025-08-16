#include "ui/tree_view_manager.h"
#include <QHeaderView>
#include <string.h> // For strnlen

TreeViewManager::TreeViewManager(QObject *parent, QWidget *parentWidget)
    : QObject(parent), treeDock(nullptr), treeView(nullptr), directoryModel(nullptr), fs(nullptr) {
    setupTreeView(parentWidget);
}

void TreeViewManager::setupTreeView(QWidget *parentWidget) {
    // Create tree view as a dock widget
    treeDock = new QDockWidget("Directory Tree", parentWidget);
    treeDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    treeView = new QTreeView(treeDock);
    treeDock->setWidget(treeView);

    // Create the model
    directoryModel = new QStandardItemModel(this);
    directoryModel->setHorizontalHeaderLabels(QStringList() << "Name");

    // Configure tree view
    treeView->setModel(directoryModel);
    treeView->setHeaderHidden(false);
    treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    treeView->setDragEnabled(true);
    treeView->setAcceptDrops(true);
    treeView->setDropIndicatorShown(true);

    // Connect signals
    connect(treeView, &QTreeView::clicked, this, &TreeViewManager::onTreeViewClicked);

    // Initially hidden
    treeDock->hide();
}

void TreeViewManager::setFileSystem(FileSystem *filesystem) {
    fs = filesystem;
}

void TreeViewManager::refreshTreeView() {
    if (!fs)
        return;

    // Clear the tree
    directoryModel->clear();
    directoryModel->setHorizontalHeaderLabels(QStringList() << "Name");

    // Create root item
    QStandardItem *rootItem =
        new QStandardItem(QIcon(treeView->style()->standardIcon(QStyle::SP_DirIcon)), "/");
    rootItem->setData(0, Qt::UserRole); // Root inode is always 0
    directoryModel->appendRow(rootItem);

    // Build tree recursively - starting from root
    buildDirectoryTree(rootItem, 0, "/");

    // Expand root by default
    treeView->expand(directoryModel->index(0, 0));
}

void TreeViewManager::buildDirectoryTree(QStandardItem *parentItem, int parent_inode,
                                         const std::string &parent_path) {
    if (!fs)
        return;

    // Restore original directory
    int original_dir_inode = fs->find_inode_by_path(".");

    // Get entries
    auto entries = fs->ls();

    for (const auto &entry : entries) {
        // Create a std::string using the entry name and ensure it's properly terminated
        std::string nameStr(entry.name, strnlen(entry.name, MAX_FILENAME_LENGTH));

        if (nameStr == "." || nameStr == "..")
            continue;

        Inode inode = fs->get_inode(entry.inode_num);

        // Only add directories to the tree - mode 2 indicates directory
        bool isDirectory = (inode.mode == 2);
        if (isDirectory) {
            // Check if the name contains only valid UTF-8 characters
            QString entryName = QString::fromUtf8(nameStr.c_str());
            if (entryName.isEmpty() && !nameStr.empty()) {
                // If conversion fails, fallback to Latin1 encoding
                entryName = QString::fromLatin1(nameStr.c_str());
                // If still problematic, provide a placeholder name with the inode number
                if (entryName.contains(QChar(QChar::ReplacementCharacter))) {
                    entryName = QString("Dir-%1").arg(entry.inode_num);
                }
            }

            QStandardItem *item = new QStandardItem(
                QIcon(treeView->style()->standardIcon(QStyle::SP_DirIcon)), entryName);
            item->setData(entry.inode_num, Qt::UserRole);
            parentItem->appendRow(item);

            // Recursively build subdirectories
            std::string full_path = parent_path;
            if (parent_path != "/")
                full_path += "/";
            full_path += nameStr;

            buildDirectoryTree(item, entry.inode_num, full_path);
        }
    }

    // Restore original directory
    fs->cd(".");
}

void TreeViewManager::toggleVisibility() {
    if (treeDock->isVisible()) {
        treeDock->hide();
    } else {
        treeDock->show();
        refreshTreeView();
    }
}

QDockWidget *TreeViewManager::getDockWidget() const {
    return treeDock;
}

void TreeViewManager::onTreeViewClicked(const QModelIndex &index) {
    if (!fs)
        return;

    // Get the inode number from the item data
    QStandardItem *item = directoryModel->itemFromIndex(index);
    if (!item)
        return;

    int inode_num = item->data(Qt::UserRole).toInt();

    // Build the full path by walking up the tree
    QString path;
    QStandardItem *current = item;
    while (current) {
        if (current->parent() == nullptr) {
            // Root item
            path = "/" + path;
            break;
        } else {
            if (!path.isEmpty()) {
                path = "/" + path;
            }
            path = current->text() + path;
            current = current->parent();
        }
    }

    // Navigate to this directory and emit signal
    emit directorySelected(path.toStdString());
}
