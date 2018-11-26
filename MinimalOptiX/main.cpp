#include "MinimalOptiX.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MinimalOptiX w;
    w.show();
    return a.exec();
}
