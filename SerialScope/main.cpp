#include "SerialScope.h"
#include <QtWidgets/QApplication>
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>


int main(int argc, char *argv[])
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    QApplication app(argc, argv);
    SerialScope window;
    window.show();
    return app.exec();
}
