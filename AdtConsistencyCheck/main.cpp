#include "AdtConsistencyCheckApp.h"
#include "logger.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    Logger::Init("");
    QApplication a(argc, argv);

    //在退出时调用Logger::Shutdown();
    QObject::connect(&a, &QApplication::aboutToQuit, [] {
        Logger::Shutdown();
    });

    AdtConsistencyCheckApp w;
    w.show();
    return QApplication::exec();
}
