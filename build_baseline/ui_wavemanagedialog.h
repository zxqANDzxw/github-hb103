/********************************************************************************
** Form generated from reading UI file 'wavemanagedialog.ui'
**
** Created by: Qt User Interface Compiler version 5.9.9
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WAVEMANAGEDIALOG_H
#define UI_WAVEMANAGEDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDateTimeEdit>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_WaveManageDialog
{
public:
    QHBoxLayout *horizontalLayout_2;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QDateTimeEdit *dtStart;
    QLabel *label_2;
    QDateTimeEdit *dtEnd;
    QSpacerItem *horizontalSpacer;
    QPushButton *pushButton;
    QTableWidget *tableWidget;
    QProgressBar *progressBar;

    void setupUi(QDialog *WaveManageDialog)
    {
        if (WaveManageDialog->objectName().isEmpty())
            WaveManageDialog->setObjectName(QStringLiteral("WaveManageDialog"));
        WaveManageDialog->resize(789, 409);
        horizontalLayout_2 = new QHBoxLayout(WaveManageDialog);
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        label = new QLabel(WaveManageDialog);
        label->setObjectName(QStringLiteral("label"));

        horizontalLayout->addWidget(label);

        dtStart = new QDateTimeEdit(WaveManageDialog);
        dtStart->setObjectName(QStringLiteral("dtStart"));

        horizontalLayout->addWidget(dtStart);

        label_2 = new QLabel(WaveManageDialog);
        label_2->setObjectName(QStringLiteral("label_2"));

        horizontalLayout->addWidget(label_2);

        dtEnd = new QDateTimeEdit(WaveManageDialog);
        dtEnd->setObjectName(QStringLiteral("dtEnd"));

        horizontalLayout->addWidget(dtEnd);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        pushButton = new QPushButton(WaveManageDialog);
        pushButton->setObjectName(QStringLiteral("pushButton"));

        horizontalLayout->addWidget(pushButton);


        verticalLayout->addLayout(horizontalLayout);

        tableWidget = new QTableWidget(WaveManageDialog);
        tableWidget->setObjectName(QStringLiteral("tableWidget"));

        verticalLayout->addWidget(tableWidget);

        progressBar = new QProgressBar(WaveManageDialog);
        progressBar->setObjectName(QStringLiteral("progressBar"));
        progressBar->setValue(0);

        verticalLayout->addWidget(progressBar);


        horizontalLayout_2->addLayout(verticalLayout);


        retranslateUi(WaveManageDialog);

        QMetaObject::connectSlotsByName(WaveManageDialog);
    } // setupUi

    void retranslateUi(QDialog *WaveManageDialog)
    {
        WaveManageDialog->setWindowTitle(QApplication::translate("WaveManageDialog", "Dialog", Q_NULLPTR));
        label->setText(QApplication::translate("WaveManageDialog", "\345\274\200\345\247\213\346\227\266\351\227\264", Q_NULLPTR));
        label_2->setText(QApplication::translate("WaveManageDialog", "\347\273\223\346\235\237\346\227\266\351\227\264", Q_NULLPTR));
        pushButton->setText(QApplication::translate("WaveManageDialog", "\345\217\254\345\224\244\346\263\242\345\275\242\345\210\227\350\241\250", Q_NULLPTR));
    } // retranslateUi

};

namespace Ui {
    class WaveManageDialog: public Ui_WaveManageDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WAVEMANAGEDIALOG_H
