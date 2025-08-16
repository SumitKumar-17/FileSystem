#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include "core/filesystem.h"
namespace Ui {
class MainWindow;
}

class QListWidgetItem;

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

private:
    void refreshFileList();

    Ui::MainWindow *ui;
    
    std::unique_ptr<FileSystem> fs;

    std::string current_open_file;
};

#endif // MAINWINDOW_H
