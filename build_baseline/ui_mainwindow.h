/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.9.9
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QGridLayout *gridLayout;
    QSplitter *splitter_2;
    QSplitter *splitter;
    QTreeWidget *treeWidget;
    QPlainTextEdit *plainTextEdit;
    QTextEdit *textEdit;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QStringLiteral("MainWindow"));
        MainWindow->resize(640, 360);
        MainWindow->setContextMenuPolicy(Qt::CustomContextMenu);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QStringLiteral("centralwidget"));
        gridLayout = new QGridLayout(centralwidget);
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        splitter_2 = new QSplitter(centralwidget);
        splitter_2->setObjectName(QStringLiteral("splitter_2"));
        splitter_2->setOrientation(Qt::Vertical);
        splitter = new QSplitter(splitter_2);
        splitter->setObjectName(QStringLiteral("splitter"));
        splitter->setOrientation(Qt::Horizontal);
        treeWidget = new QTreeWidget(splitter);
        treeWidget->setObjectName(QStringLiteral("treeWidget"));
        treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        splitter->addWidget(treeWidget);
        treeWidget->header()->setVisible(true);
        plainTextEdit = new QPlainTextEdit(splitter);
        plainTextEdit->setObjectName(QStringLiteral("plainTextEdit"));
        splitter->addWidget(plainTextEdit);
        splitter_2->addWidget(splitter);
        textEdit = new QTextEdit(splitter_2);
        textEdit->setObjectName(QStringLiteral("textEdit"));
        splitter_2->addWidget(textEdit);

        gridLayout->addWidget(splitter_2, 0, 0, 1, 1);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName(QStringLiteral("menubar"));
        menubar->setGeometry(QRect(0, 0, 640, 21));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName(QStringLiteral("statusbar"));
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "MainWindow", Q_NULLPTR));
        QTreeWidgetItem *___qtreewidgetitem = treeWidget->headerItem();
        ___qtreewidgetitem->setText(0, QApplication::translate("MainWindow", "\350\256\276\345\244\207\345\210\227\350\241\250", Q_NULLPTR));
        plainTextEdit->setPlainText(QApplication::translate("MainWindow", "\346\212\245\346\226\207\346\224\266\345\217\221\345\261\225\347\244\272\347\252\227\345\217\243", Q_NULLPTR));
        textEdit->setHtml(QApplication::translate("MainWindow", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'SimSun'; font-size:9pt; font-weight:400; font-style:normal;\">\n"
"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-family:'SimSun';\">\346\212\245\346\226\207\350\247\243\346\236\220\347\252\227\345\217\243<br />\347\254\254\344\270\200\350\241\214\346\212\245\346\226\207\345\216\237\346\226\207<br />\347\254\254\344\272\214\350\241\214\346\212\245\346\226\207\345\255\227\346\256\265<br />\347\254\254\344\270\211\350\241\214\346\212\245\346\226\207\350\247\243\346\236\220</span></p></body></html>", Q_NULLPTR));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
