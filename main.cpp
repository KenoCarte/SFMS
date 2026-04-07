#include <QApplication>
#include <QIcon>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.ico")));
    MainWindow window;
    window.setWindowIcon(QIcon(QStringLiteral(":/icons/app.ico")));
    window.show();
    return app.exec();
}
