#pragma once

#include <QObject>
#include <QString>

// KDE Plasma (özellikle Wayland) üzerinde Qt::WindowStaysOnTopHint dikkate
// alınmaz. Bu sınıf, uygulama çalışırken KWin'e D-Bus üzerinden küçük bir
// betik yükler; betik bu sürecin (PID) pencerelerine keepAbove uygular.
// Hiçbir şey kalıcı olarak kurulmaz; uygulama kapanınca betik kaldırılır.
class KWinKeepAbove : public QObject
{
    Q_OBJECT

public:
    explicit KWinKeepAbove(QObject *parent = nullptr);
    ~KWinKeepAbove() override;

    // true: betik KWin'e yüklendi ve çalıştırıldı
    bool apply();
    void release();

    // Tarayıcıların "pencere içinde pencere" (PiP) pencerelerini her zaman
    // üstte tutan betiği oturum boyunca yükler (zaten yüklüyse dokunmaz).
    // Native Messaging host'u tarafından çağrılır.
    static bool ensureBrowserPipKeepAbove();

private:
    QString m_pluginName;
    QString m_scriptPath;
    bool m_loaded = false;
};
