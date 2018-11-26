#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MinimalOptiX.h"

class MinimalOptiX : public QMainWindow
{
    Q_OBJECT

public:
    MinimalOptiX(QWidget *parent = Q_NULLPTR);

private:
    Ui::MinimalOptiXClass ui;
};
