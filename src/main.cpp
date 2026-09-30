#include <QApplication>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("patlite_lr6_qt"));
    QApplication::setOrganizationName(QStringLiteral("local"));

    MainWindow w;
    w.show();

    return app.exec();
}
