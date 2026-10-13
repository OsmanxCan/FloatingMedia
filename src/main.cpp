#include "AppBridge.h"
#include "KWinKeepAbove.h"
#include "MediaUrl.h"
#include "NativeMessagingHost.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

#include <cstdio>
#include <cstring>

static void printUsage()
{
    std::fprintf(stderr,
        "FloatingMediaApp [seçenekler] [medya-yolu-veya-url]\n"
        "  --native-messaging-host   Tarayıcı Native Messaging host modu\n"
        "  --help                    Bu yardımı göster\n");
}

int main(int argc, char *argv[])
{
    // Native host GUI'siz çalışmalı
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--native-messaging-host") == 0) {
            QCoreApplication app(argc, argv);
            QCoreApplication::setApplicationName(QStringLiteral("FloatingMediaApp"));
            QCoreApplication::setOrganizationName(QStringLiteral("FloatingMedia"));
            return runNativeMessagingHost(app);
        }
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("FloatingMediaApp"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("FloatingMedia"));
    QGuiApplication::setOrganizationName(QStringLiteral("FloatingMedia"));
    QGuiApplication::setDesktopFileName(QStringLiteral("floatingmedia"));

    AppBridge bridge;
    QUrl startupUrl;
    const QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString &a = args.at(i);
        if (a == QLatin1String("--help") || a == QLatin1String("-h")) {
            printUsage();
            return 0;
        }
        if (a.startsWith(QLatin1Char('-')))
            continue;
        startupUrl = MediaUrl::fromUserInput(a);
        break;
    }

    if (!startupUrl.isEmpty()) {
        QString error;
        if (!MediaUrl::isAcceptable(startupUrl, &error)) {
            std::fprintf(stderr, "FloatingMedia: %s\n", qPrintable(error));
            return 1;
        }
    }

    // İkincil örnek: birincile ilet ve çık
    if (!bridge.becomePrimaryInstance()) {
        if (!startupUrl.isEmpty()) {
            if (!bridge.forwardToPrimary(startupUrl)) {
                std::fprintf(stderr, "FloatingMedia: çalışan örneğe bağlanılamadı.\n");
                return 1;
            }
            return 0;
        }
        // Boş argümanla ikinci örnek — yine de iletmeyi dene / çık
        return 0;
    }

    if (!startupUrl.isEmpty())
        bridge.setInitialUrl(startupUrl);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("AppBridge"), &bridge);
    engine.loadFromModule("FloatingMedia", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    // KDE Plasma (Wayland) için "her zaman üstte": KWin'e çalışma anında betik yükle
    KWinKeepAbove keepAbove;
    keepAbove.apply();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &keepAbove, &KWinKeepAbove::release);

    return app.exec();
}
