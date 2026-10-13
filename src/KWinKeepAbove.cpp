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

QDBusMessage callKWin(const QString &path, const QString &iface, const QString &method,
                      const QVariantList &args = {})
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, path, iface, method);
    msg.setArguments(args);
    return QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
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

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface())
        return false;
    if (!bus.interface()->isServiceRegistered(kService).value())
        return false; // KWin yok (GNOME vb.) — sessizce geç

    const qint64 pid = QCoreApplication::applicationPid();
    m_pluginName = QStringLiteral("floatingmedia-runtime-%1").arg(pid);

    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath();
    m_scriptPath = dir + QLatin1Char('/') + m_pluginName + QStringLiteral(".js");

    QFile file(m_scriptPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        std::fprintf(stderr, "FloatingMedia: KWin betiği yazılamadı: %s\n",
                     qPrintable(m_scriptPath));
        return false;
    }
    file.write(buildScript(pid).toUtf8());
    file.close();

    // Aynı adla kalmış eski bir kayıt varsa temizle
    callKWin(kScriptingPath, kScriptingIface, QStringLiteral("unloadScript"), {m_pluginName});

    const QDBusMessage loadReply = callKWin(kScriptingPath, kScriptingIface,
                                            QStringLiteral("loadScript"),
                                            {m_scriptPath, m_pluginName});
    if (loadReply.type() != QDBusMessage::ReplyMessage || loadReply.arguments().isEmpty()) {
        std::fprintf(stderr, "FloatingMedia: KWin betiği yüklenemedi: %s\n",
                     qPrintable(loadReply.errorMessage()));
        QFile::remove(m_scriptPath);
        return false;
    }

    const int id = loadReply.arguments().constFirst().toInt();
    if (id < 0) {
        std::fprintf(stderr, "FloatingMedia: KWin betiği reddedildi (id=%d).\n", id);
        QFile::remove(m_scriptPath);
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
