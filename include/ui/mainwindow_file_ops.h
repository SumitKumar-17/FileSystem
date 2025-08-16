#ifndef MAINWINDOW_FILE_OPS_H
#define MAINWINDOW_FILE_OPS_H

#include "ui/mainwindow.h"
#include <QListWidgetItem>

/**
 * @brief File operations for the MainWindow
 * This class handles file operations such as creating, opening, and saving files
 */
class MainWindowFileOps {
public:
    /**
     * @brief Constructor
     * @param mainWindow Pointer to the main window
     */
    MainWindowFileOps(MainWindow* mainWindow);
    
    /**
     * @brief Handle creating a new file
     */
    void createFile();
    
    /**
     * @brief Handle creating a new directory
     */
    void createDirectory();
    
    /**
     * @brief Handle saving the current file
     */
    void saveFile();
    
    /**
     * @brief Handle file double-clicked event
     * @param item The clicked list widget item
     */
    void fileDoubleClicked(QListWidgetItem *item);
    
    /**
     * @brief Handle file context menu
     * @param pos The position of the context menu
     */
    void fileContextMenu(const QPoint &pos);
    
    /**
     * @brief Refresh the file list
     */
    void refreshFileList();
    
    /**
     * @brief Drag enter event handler
     * @param event The drag enter event
     */
    void handleDragEnterEvent(QDragEnterEvent *event);
    
    /**
     * @brief Drop event handler
     * @param event The drop event
     */
    void handleDropEvent(QDropEvent *event);

private:
    MainWindow* mainWindow;
};

#endif // MAINWINDOW_FILE_OPS_H
