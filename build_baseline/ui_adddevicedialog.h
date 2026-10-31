/********************************************************************************
** Form generated from reading UI file 'adddevicedialog.ui'
**
** Created by: Qt User Interface Compiler version 5.9.9
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ADDDEVICEDIALOG_H
#define UI_ADDDEVICEDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AddDeviceDialog
{
public:
    QWidget *verticalLayoutWidget;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QSpacerItem *horizontalSpacer_2;
    QLineEdit *lineEditName;
    QHBoxLayout *horizontalLayout_2;
    QLabel *label_2;
    QSpacerItem *horizontalSpacer;
    QLineEdit *lineEditIp;
    QHBoxLayout *horizontalLayout_3;
    QLabel *label_3;
    QSpacerItem *horizontalSpacer_3;
    QSpinBox *spinBoxPort;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *AddDeviceDialog)
    {
        if (AddDeviceDialog->objectName().isEmpty())
            AddDeviceDialog->setObjectName(QStringLiteral("AddDeviceDialog"));
        AddDeviceDialog->resize(859, 500);
        verticalLayoutWidget = new QWidget(AddDeviceDialog);
        verticalLayoutWidget->setObjectName(QStringLiteral("verticalLayoutWidget"));
        verticalLayoutWidget->setGeometry(QRect(10, 20, 801, 431));
        verticalLayout = new QVBoxLayout(verticalLayoutWidget);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        label = new QLabel(verticalLayoutWidget);
        label->setObjectName(QStringLiteral("label"));

        horizontalLayout->addWidget(label);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);

        lineEditName = new QLineEdit(verticalLayoutWidget);
        lineEditName->setObjectName(QStringLiteral("lineEditName"));

        horizontalLayout->addWidget(lineEditName);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        label_2 = new QLabel(verticalLayoutWidget);
        label_2->setObjectName(QStringLiteral("label_2"));

        horizontalLayout_2->addWidget(label_2);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);

        lineEditIp = new QLineEdit(verticalLayoutWidget);
        lineEditIp->setObjectName(QStringLiteral("lineEditIp"));

        horizontalLayout_2->addWidget(lineEditIp);


        verticalLayout->addLayout(horizontalLayout_2);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QStringLiteral("horizontalLayout_3"));
        label_3 = new QLabel(verticalLayoutWidget);
        label_3->setObjectName(QStringLiteral("label_3"));

        horizontalLayout_3->addWidget(label_3);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_3);

        spinBoxPort = new QSpinBox(verticalLayoutWidget);
        spinBoxPort->setObjectName(QStringLiteral("spinBoxPort"));
        spinBoxPort->setMinimum(0);
        spinBoxPort->setMaximum(777777);

        horizontalLayout_3->addWidget(spinBoxPort);


        verticalLayout->addLayout(horizontalLayout_3);

        buttonBox = new QDialogButtonBox(verticalLayoutWidget);
        buttonBox->setObjectName(QStringLiteral("buttonBox"));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(AddDeviceDialog);
        QObject::connect(buttonBox, SIGNAL(accepted()), AddDeviceDialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), AddDeviceDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(AddDeviceDialog);
    } // setupUi

    void retranslateUi(QDialog *AddDeviceDialog)
    {
        AddDeviceDialog->setWindowTitle(QApplication::translate("AddDeviceDialog", "Dialog", Q_NULLPTR));
        label->setText(QApplication::translate("AddDeviceDialog", "\350\256\276\345\244\207\345\220\215\347\247\260", Q_NULLPTR));
        lineEditName->setText(QApplication::translate("AddDeviceDialog", "192.168.124.18", Q_NULLPTR));
        label_2->setText(QApplication::translate("AddDeviceDialog", "ip\345\234\260\345\235\200", Q_NULLPTR));
        lineEditIp->setText(QApplication::translate("AddDeviceDialog", "192.168.124.18", Q_NULLPTR));
        label_3->setText(QApplication::translate("AddDeviceDialog", "\347\253\257\345\217\243", Q_NULLPTR));
    } // retranslateUi

};

namespace Ui {
    class AddDeviceDialog: public Ui_AddDeviceDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ADDDEVICEDIALOG_H
