#ifndef MAINWINDOW_DIALOGS_H
#define MAINWINDOW_DIALOGS_H

#include "ui/mainwindow.h"

/**
 * @brief Dialog handling for the MainWindow
 * This class handles various dialogs such as search, quota, and snapshots
 */
class MainWindowDialogs {
public:
    /**
     * @brief Constructor
     * @param mainWindow Pointer to the main window
     */
    MainWindowDialogs(MainWindow* mainWindow);
    
    /**
     * @brief Handle filesystem check action
     */
    void handleFsCheck();
    
    /**
     * @brief Handle advanced search action
     */
    void handleAdvancedSearch();
    
    /**
     * @brief Handle quick search action
     */
    void handleQuickSearch();
    
    /**
     * @brief Handle quota manager action
     */
    void handleQuotaManager();
    
    /**
     * @brief Handle snapshots action
     */
    void handleSnapshots();
    
    /**
     * @brief Handle filesystem detection
     */
    void handleFilesystemDetection();
    
    /**
     * @brief Update the list of available filesystems
     */
    void updateAvailableFilesystemsList();

private:
    MainWindow* mainWindow;
};

#endif // MAINWINDOW_DIALOGS_H
