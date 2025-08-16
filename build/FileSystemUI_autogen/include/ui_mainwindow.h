/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QPushButton *formatButton;
    QPushButton *mountButton;
    QSpacerItem *horizontalSpacer;
    QHBoxLayout *horizontalLayout_2;
    QListWidget *fileListWidget;
    QPlainTextEdit *fileContentTextEdit;
    QHBoxLayout *horizontalLayout_3;
    QPushButton *mkdirButton;
    QPushButton *createFileButton;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *saveButton;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(800, 600);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName("verticalLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        formatButton = new QPushButton(centralwidget);
        formatButton->setObjectName("formatButton");

        horizontalLayout->addWidget(formatButton);

        mountButton = new QPushButton(centralwidget);
        mountButton->setObjectName("mountButton");

        horizontalLayout->addWidget(mountButton);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        fileListWidget = new QListWidget(centralwidget);
        fileListWidget->setObjectName("fileListWidget");

        horizontalLayout_2->addWidget(fileListWidget);

        fileContentTextEdit = new QPlainTextEdit(centralwidget);
        fileContentTextEdit->setObjectName("fileContentTextEdit");

        horizontalLayout_2->addWidget(fileContentTextEdit);


        verticalLayout->addLayout(horizontalLayout_2);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        mkdirButton = new QPushButton(centralwidget);
        mkdirButton->setObjectName("mkdirButton");

        horizontalLayout_3->addWidget(mkdirButton);

        createFileButton = new QPushButton(centralwidget);
        createFileButton->setObjectName("createFileButton");

        horizontalLayout_3->addWidget(createFileButton);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_2);

        saveButton = new QPushButton(centralwidget);
        saveButton->setObjectName("saveButton");

        horizontalLayout_3->addWidget(saveButton);


        verticalLayout->addLayout(horizontalLayout_3);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 800, 22));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "Simple File System GUI", nullptr));
        formatButton->setText(QCoreApplication::translate("MainWindow", "Format Disk", nullptr));
        mountButton->setText(QCoreApplication::translate("MainWindow", "Mount File System", nullptr));
        mkdirButton->setText(QCoreApplication::translate("MainWindow", "Create Directory", nullptr));
        createFileButton->setText(QCoreApplication::translate("MainWindow", "Create File", nullptr));
        saveButton->setText(QCoreApplication::translate("MainWindow", "Save File Content", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
