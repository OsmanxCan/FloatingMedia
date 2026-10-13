#include "KWinKeepAbove.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <cstdio>

namespace {

const QString kService = QStringLiteral("org.kde.KWin");
const QString kScriptingPath = QStringLiteral("/Scripting");
const QString kScriptingIface = QStringLiteral("org.kde.kwin.Scripting");
const QString kBrowserPipPlugin = QStringLiteral("floatingmedia-browser-pip");

QString buildScript(qint64 pid)
{
    return QStringLiteral(R"JS(
var FM_PID = %1;

function fmApply(w) {
    if (!w || w.pid !== FM_PID)
        return;
    try {
        w.keepAbove = true;
    } catch (e) {
        print("FloatingMedia keepAbove hatası: " + e);
    }
}

if (workspace.windowAdded)
    workspace.windowAdded.connect(fmApply);
else if (workspace.clientAdded)
    workspace.clientAdded.connect(fmApply);

var fmList = workspace.windowList ? workspace.windowList() : workspace.clientList();
for (var i = 0; i < fmList.length; i++)
    fmApply(fmList[i]);
)JS").arg(pid);
}

// Chrome/Chromium/Brave/Edge ve Firefox PiP pencere başlıkları (TR/EN)
QString buildBrowserPipScript()
{
    return QStringLiteral(R"JS(
var FM_PIP_RE = /picture.?in.?picture|pencere i[çc]inde pencere|resim i[çc]inde resim/i;

function fmPipCheck(w) {
    if (!w)
        return;
    try {
        if (FM_PIP_RE.test(w.caption || "") && !w.keepAbove)
            w.keepAbove = true;
    } catch (e) {
        print("FloatingMedia PiP hatası: " + e);
    }
}

function fmPipWatch(w) {
    if (!w)
        return;
    fmPipCheck(w);
    try {
        w.captionChanged.connect(function () { fmPipCheck(w); });
    } catch (e) {}
}

if (workspace.windowAdded)
    workspace.windowAdded.connect(fmPipWatch);
else if (workspace.clientAdded)
    workspace.clientAdded.connect(fmPipWatch);

var fmPipList = workspace.windowList ? workspace.windowList() : workspace.clientList();
for (var i = 0; i < fmPipList.length; i++)
    fmPipWatch(fmPipList[i]);
)JS");
}

QDBusMessage callKWin(const QString &path, const QString &iface, const QString &method,
                      const QVariantList &args = {})
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, path, iface, method);
    msg.setArguments(args);
    return QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
}

bool kwinAvailable()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface())
        return false;
    return bus.interface()->isServiceRegistered(kService).value();
}

QString scriptPathFor(const QString &pluginName)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath();
    return dir + QLatin1Char('/') + pluginName + QStringLiteral(".js");
}

bool writeFile(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        std::fprintf(stderr, "FloatingMedia: KWin betiği yazılamadı: %s\n", qPrintable(path));
        return false;
    }
    file.write(content.toUtf8());
    return true;
}

// Betiği yükleyip çalıştırır; başarıda true
bool loadAndRun(const QString &path, const QString &pluginName)
{
    const QDBusMessage loadReply = callKWin(kScriptingPath, kScriptingIface,
                                            QStringLiteral("loadScript"),
                                            {path, pluginName});
    if (loadReply.type() != QDBusMessage::ReplyMessage || loadReply.arguments().isEmpty()) {
        std::fprintf(stderr, "FloatingMedia: KWin betiği yüklenemedi: %s\n",
                     qPrintable(loadReply.errorMessage()));
        return false;
    }

    const int id = loadReply.arguments().constFirst().toInt();
    if (id < 0) {
        std::fprintf(stderr, "FloatingMedia: KWin betiği reddedildi (id=%d).\n", id);
        return false;
    }

    // Plasma 6: /Scripting/Script<N>, Plasma 5: /<N>
    QDBusMessage runReply = callKWin(QStringLiteral("/Scripting/Script%1").arg(id),
                                     QStringLiteral("org.kde.kwin.Script"),
                                     QStringLiteral("run"));
    if (runReply.type() != QDBusMessage::ReplyMessage)
        runReply = callKWin(QStringLiteral("/%1").arg(id),
                            QStringLiteral("org.kde.kwin.Script"),
                            QStringLiteral("run"));
    if (runReply.type() != QDBusMessage::ReplyMessage)
        callKWin(kScriptingPath, kScriptingIface, QStringLiteral("start"));
    return true;
}

} // namespace

KWinKeepAbove::KWinKeepAbove(QObject *parent)
    : QObject(parent)
{
}

KWinKeepAbove::~KWinKeepAbove()
{
    release();
}

bool KWinKeepAbove::apply()
{
    if (m_loaded)
        return true;
    if (qEnvironmentVariableIsSet("FLOATINGMEDIA_NO_KWIN"))
        return false;
    if (!kwinAvailable())
        return false; // KWin yok (GNOME vb.) — sessizce geç

    const qint64 pid = QCoreApplication::applicationPid();
    m_pluginName = QStringLiteral("floatingmedia-runtime-%1").arg(pid);
    m_scriptPath = scriptPathFor(m_pluginName);

    if (!writeFile(m_scriptPath, buildScript(pid)))
        return false;

    // Aynı adla kalmış eski bir kayıt varsa temizle
    callKWin(kScriptingPath, kScriptingIface, QStringLiteral("unloadScript"), {m_pluginName});

    if (!loadAndRun(m_scriptPath, m_pluginName)) {
        QFile::remove(m_scriptPath);
        return false;
    }

    m_loaded = true;
    return true;
}

void KWinKeepAbove::release()
{
    if (!m_loaded)
        return;
    m_loaded = false;

    if (QDBusConnection::sessionBus().isConnected())
        callKWin(kScriptingPath, kScriptingIface, QStringLiteral("unloadScript"), {m_pluginName});
    QFile::remove(m_scriptPath);
}

bool KWinKeepAbove::ensureBrowserPipKeepAbove()
{
    if (qEnvironmentVariableIsSet("FLOATINGMEDIA_NO_KWIN") || !kwinAvailable())
        return false;

    const QDBusMessage loaded = callKWin(kScriptingPath, kScriptingIface,
                                         QStringLiteral("isScriptLoaded"), {kBrowserPipPlugin});
    if (loaded.type() == QDBusMessage::ReplyMessage && !loaded.arguments().isEmpty()
        && loaded.arguments().constFirst().toBool()) {
        return true; // bu oturumda zaten etkin
    }

    const QString path = scriptPathFor(kBrowserPipPlugin);
    if (!writeFile(path, buildBrowserPipScript()))
        return false;
    // Betik dosyası oturum boyunca yerinde kalır (KWin yeniden okuyabilir)
    return loadAndRun(path, kBrowserPipPlugin);
}
