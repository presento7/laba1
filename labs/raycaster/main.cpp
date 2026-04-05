#include "main_window.h"

#include <QtWidgets/QApplication>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("BSU");
    QApplication::setApplicationName("RaycasterLab");

    MainWindow window;
    window.show();

    return QApplication::exec();
}
