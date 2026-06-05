#include <QApplication>
#include "GpaWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("GPA 计算器");

    GpaWindow window;
    window.show();

    return app.exec();
}
