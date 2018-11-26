/********************************************************************************
** Form generated from reading UI file 'MinimalOptiX.ui'
**
** Created by: Qt User Interface Compiler version 5.11.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MINIMALOPTIX_H
#define UI_MINIMALOPTIX_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MinimalOptiXClass
{
public:
    QWidget *centralWidget;
    QGraphicsView *view;

    void setupUi(QMainWindow *MinimalOptiXClass)
    {
        if (MinimalOptiXClass->objectName().isEmpty())
            MinimalOptiXClass->setObjectName(QStringLiteral("MinimalOptiXClass"));
        MinimalOptiXClass->resize(600, 400);
        centralWidget = new QWidget(MinimalOptiXClass);
        centralWidget->setObjectName(QStringLiteral("centralWidget"));
        view = new QGraphicsView(centralWidget);
        view->setObjectName(QStringLiteral("view"));
        view->setGeometry(QRect(0, 0, 601, 401));
        MinimalOptiXClass->setCentralWidget(centralWidget);

        retranslateUi(MinimalOptiXClass);

        QMetaObject::connectSlotsByName(MinimalOptiXClass);
    } // setupUi

    void retranslateUi(QMainWindow *MinimalOptiXClass)
    {
        MinimalOptiXClass->setWindowTitle(QApplication::translate("MinimalOptiXClass", "MinimalOptiX", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MinimalOptiXClass: public Ui_MinimalOptiXClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MINIMALOPTIX_H
