#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QLoggingCategory>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <iostream>
#endif

#include "app/AppController.h"
#include "auth/AuthController.h"

using namespace std;

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // Включаем вывод в консоль для Windows GUI приложения
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    } else {
        AllocConsole();
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
        cout.clear();
        cerr.clear();
        cin.clear();
    }
    // Настраиваем Qt для вывода отладочных сообщений
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true\n*.info=true\n*.warning=true\n*.critical=true"));
#endif

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("offline-edm"));
    QGuiApplication::setOrganizationName(QStringLiteral("offline-edm"));

    QQmlApplicationEngine engine;

    AuthController authController;
    AppController appController;
    engine.rootContext()->setContextProperty(QStringLiteral("authController"), &authController);
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &appController);

    const QUrl url(QStringLiteral("qrc:/ui/qml/App.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    engine.load(url);
    return app.exec();
}
