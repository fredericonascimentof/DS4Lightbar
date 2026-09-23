#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QWindow>

#include "LightbarController.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifndef DS4LIGHTBAR_SOURCE_DIR
#define DS4LIGHTBAR_SOURCE_DIR ""
#endif

static QIcon loadAppIcon()
{
    QIcon icon(QCoreApplication::applicationDirPath() + "/assets/icon.png");

    if (icon.isNull()) {
        icon = QIcon(":/qt/qml/DS4Lightbar/assets/icon.png");
    }

    return icon;
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");

    app.setApplicationName("DS4 Lightbar");
    app.setOrganizationName("DS4Lightbar");
    app.setQuitOnLastWindowClosed(false);

#ifdef Q_OS_WIN
    HANDLE singleInstanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\DS4LightbarSingleInstance");

    if (!singleInstanceMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (singleInstanceMutex) {
            CloseHandle(singleInstanceMutex);
        }

        return 0;
    }

    auto releaseSingleInstance = [&singleInstanceMutex]() {
        if (singleInstanceMutex) {
            ReleaseMutex(singleInstanceMutex);
            CloseHandle(singleInstanceMutex);
            singleInstanceMutex = nullptr;
        }
    };
#else
    auto releaseSingleInstance = []() {};
#endif

    const QIcon appIcon = loadAppIcon();
    app.setWindowIcon(appIcon);

    LightbarController lightbar;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("lightbar", &lightbar);

    const QString mainQmlPath = QStringLiteral(DS4LIGHTBAR_SOURCE_DIR) + "/qml/Main.qml";

    auto loadUi = [&engine, &mainQmlPath]() {
        const QList<QObject*> previousRoots = engine.rootObjects();
        engine.clearComponentCache();

#ifdef DS4LIGHTBAR_DEV_RELOAD
        engine.load(QUrl::fromLocalFile(mainQmlPath));
#else
        engine.loadFromModule("DS4Lightbar", "Main");
#endif

        const QList<QObject*> currentRoots = engine.rootObjects();
        QList<QObject*> createdRoots;

        for (QObject* object : currentRoots) {
            if (!previousRoots.contains(object)) {
                createdRoots.append(object);
            }
        }

        if (createdRoots.empty()) {
            qWarning() << "QML load failed.";
            return false;
        }

        for (QObject* object : previousRoots) {
            object->deleteLater();
        }

        return true;
    };

    if (!loadUi()) {
        releaseSingleInstance();
        return -1;
    }

#ifdef DS4LIGHTBAR_DEV_RELOAD
    QFileSystemWatcher qmlWatcher;
    QTimer qmlReloadTimer;
    qmlReloadTimer.setSingleShot(true);
    qmlReloadTimer.setInterval(150);

    if (QFileInfo::exists(mainQmlPath)) {
        qmlWatcher.addPath(mainQmlPath);
    }

    QObject::connect(&qmlWatcher, &QFileSystemWatcher::fileChanged, &app, [&](const QString& path) {
        if (QFileInfo::exists(path) && !qmlWatcher.files().contains(path)) {
            qmlWatcher.addPath(path);
        }

        qmlReloadTimer.start();
    });

    QObject::connect(&qmlReloadTimer, &QTimer::timeout, &app, [&]() {
        if (QFileInfo::exists(mainQmlPath) && !qmlWatcher.files().contains(mainQmlPath)) {
            qmlWatcher.addPath(mainQmlPath);
        }

        if (loadUi()) {
            qInfo() << "QML reloaded:" << mainQmlPath;
        }
    });
#endif

    auto showMainWindow = [&engine]() {
        const QList<QObject*> roots = engine.rootObjects();

        for (QObject* object : roots) {
            QWindow* window = qobject_cast<QWindow*>(object);

            if (!window) {
                continue;
            }

            window->show();
            window->raise();
            window->requestActivate();
            break;
        }
    };

    QSystemTrayIcon tray(appIcon);
    tray.setToolTip("DS4 Lightbar");

    QMenu trayMenu;
    QAction* openAction = trayMenu.addAction("Open window");
    QAction* logAction = trayMenu.addAction("Open log");
    trayMenu.addSeparator();
    QAction* quitAction = trayMenu.addAction("Exit");

    QObject::connect(openAction, &QAction::triggered, &app, showMainWindow);
    QObject::connect(logAction, &QAction::triggered, &lightbar, &LightbarController::showLogWindow);
    QObject::connect(quitAction, &QAction::triggered, &app, [&]() {
        lightbar.shutdown();
        app.quit();
    });

    QObject::connect(
        &tray,
        &QSystemTrayIcon::activated,
        &app,
        [&](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                showMainWindow();
            }
        }
    );

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        tray.setContextMenu(&trayMenu);
        tray.show();
    }

    const int exitCode = app.exec();
    releaseSingleInstance();
    return exitCode;
}
