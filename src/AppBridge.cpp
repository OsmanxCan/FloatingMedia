#include "AppBridge.h"
#include "MediaUrl.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QVariant>

namespace {

const QString kImage = QStringLiteral("image");
const QString kVideo = QStringLiteral("video");
const QString kPage = QStringLiteral("page");

QString ytDlpPath()
{
    return QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
}

QString lastErrorLine(const QString &stderrText)
{
    const QStringList lines = stderrText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        if (it->startsWith(QLatin1String("ERROR:")))
            return it->mid(6).trimmed();
    }
    return lines.isEmpty() ? QString() : lines.constLast().trimmed();
}

} // namespace

AppBridge::AppBridge(QObject *parent)
    : QObject(parent)
{
}

AppBridge::~AppBridge()
{
    cancelResolve();
    if (m_server)
        m_server->close();
}

bool AppBridge::ytDlpAvailable() const
{
    return !ytDlpPath().isEmpty();
}

void AppBridge::setInitialUrl(const QUrl &url, const QString &kind)
{
    m_initialUrl = url;
    m_initialKind = isValidKind(kind) ? kind : QString();
}

void AppBridge::setError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

bool AppBridge::isValidKind(const QString &kind)
{
    return kind.isEmpty() || kind == kImage || kind == kVideo || kind == kPage;
}

QByteArray AppBridge::encodeIpc(const QUrl &url, const QString &kind)
{
    return (isValidKind(kind) ? kind : QString()).toUtf8() + '\t'
        + url.toString(QUrl::FullyEncoded).toUtf8() + '\n';
}

bool AppBridge::decodeIpc(const QByteArray &line, QUrl *url, QString *kind)
{
    const QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return false;

    QString k;
    QByteArray urlPart = trimmed;
    const int tab = line.indexOf('\t');
    if (tab >= 0) {
        k = QString::fromUtf8(line.left(tab)).trimmed();
        urlPart = line.mid(tab + 1).trimmed();
        if (!isValidKind(k))
            k.clear();
    }
    *url = MediaUrl::fromUserInput(QString::fromUtf8(urlPart));
    *kind = k;
    return !url->isEmpty();
}

bool AppBridge::forwardToPrimary(const QUrl &url, const QString &kind)
{
    QLocalSocket socket;
    socket.connectToServer(QString::fromUtf8(kServerName));
    if (!socket.waitForConnected(800))
        return false;
    socket.write(encodeIpc(url, kind));
    socket.flush();
    socket.waitForBytesWritten(800);
    socket.waitForReadyRead(800);
    socket.disconnectFromServer();
    return true;
}

bool AppBridge::becomePrimaryInstance()
{
    // Önce çalışan birincil var mı bak
    {
        QLocalSocket probe;
        probe.connectToServer(QString::fromUtf8(kServerName));
        if (probe.waitForConnected(200)) {
            probe.disconnectFromServer();
            return false;
        }
    }

    m_server = new QLocalServer(this);
    QLocalServer::removeServer(QString::fromUtf8(kServerName));
    if (!m_server->listen(QString::fromUtf8(kServerName))) {
        delete m_server;
        m_server = nullptr;
        return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, &AppBridge::handleIpcConnection);
    return true;
}

void AppBridge::handleIpcConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QLocalSocket *socket = m_server->nextPendingConnection();
        if (!socket)
            continue;

        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            QUrl url;
            QString kind;
            QString error;
            if (!decodeIpc(socket->readAll(), &url, &kind) || !MediaUrl::isAcceptable(url, &error)) {
                setError(error.isEmpty() ? QStringLiteral("Geçersiz adres.") : error);
                socket->write("ERR\n");
            } else {
                dispatch(url, kind);
                socket->write("OK\n");
            }
            socket->flush();
            socket->disconnectFromServer();
        });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    }
}

bool AppBridge::requestOpen(const QString &input, const QString &kindHint)
{
    const QUrl url = MediaUrl::fromUserInput(input);
    QString error;
    if (!MediaUrl::isAcceptable(url, &error)) {
        setError(error);
        return false;
    }
    dispatch(url, isValidKind(kindHint) ? kindHint : QString());
    return true;
}

void AppBridge::openInitial()
{
    if (!m_initialUrl.isEmpty())
        dispatch(m_initialUrl, m_initialKind);
}

void AppBridge::dispatch(const QUrl &url, const QString &kindHint)
{
    cancelResolve();
    setError({});

    const MediaUrl::Kind byExt = MediaUrl::classifyByExtension(url);

    if (kindHint == kImage || (kindHint.isEmpty() && byExt == MediaUrl::Kind::Image)) {
        emit openRequested(url, kImage);
        return;
    }
    if (url.isLocalFile()) {
        emit openRequested(url, byExt == MediaUrl::Kind::Image ? kImage : kVideo);
        return;
    }
    if (kindHint == kVideo || (kindHint.isEmpty() && byExt == MediaUrl::Kind::Video)) {
        emit openRequested(url, kVideo);
        return;
    }

    // Uzantısız http(s) adresi veya video içeren sayfa → yt-dlp ile gerçek akışı bul
    resolveWithYtDlp(url);
}

void AppBridge::setResolveStatus(const QString &status)
{
    if (m_resolveStatus == status)
        return;
    m_resolveStatus = status;
    emit resolveStatusChanged();
}

void AppBridge::resolveWithYtDlp(const QUrl &pageUrl)
{
    const QString exe = ytDlpPath();
    if (exe.isEmpty()) {
        // yt-dlp yok: doğrudan oynatmayı dene (doğrudan akışsa çalışır)
        emit openRequested(pageUrl, kVideo);
        return;
    }

    auto *proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    // Bazı ortamlarda yt-dlp girdi bekleyebilir; kapat
    proc->setStandardInputFile(QProcess::nullDevice());
    proc->setProperty("fmOutput", QString());
    m_resolver = proc;
    setResolveStatus(QStringLiteral("yt-dlp başlatılıyor…"));
    emit resolvingChanged();

    // Canlı durum: yt-dlp'nin "[site] ...: Downloading ..." satırlarını göster
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        const QString chunk = QString::fromUtf8(proc->readAllStandardOutput());
        proc->setProperty("fmOutput", proc->property("fmOutput").toString() + chunk);
        if (proc != m_resolver)
            return;
        const QStringList lines = chunk.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
            const QString t = it->trimmed();
            if (t.startsWith(QLatin1Char('['))) {
                setResolveStatus(t.left(90));
                break;
            }
        }
    });

    connect(proc, &QProcess::finished, this,
            [this, proc, pageUrl](int exitCode, QProcess::ExitStatus status) {
        const bool current = (proc == m_resolver);
        const QString out = proc->property("fmOutput").toString()
            + QString::fromUtf8(proc->readAllStandardOutput());
        const bool timedOut = proc->property("fmTimedOut").toBool();
        proc->deleteLater();
        if (!current)
            return; // iptal edildi / yerine yenisi geldi

        const QString lastStep = m_resolveStatus;
        m_resolver = nullptr;
        setResolveStatus({});
        emit resolvingChanged();

        QUrl direct;
        const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QString t = line.trimmed();
            if (t.startsWith(QLatin1String("http://")) || t.startsWith(QLatin1String("https://"))) {
                direct = QUrl(t);
                break;
            }
        }

        if (status == QProcess::NormalExit && exitCode == 0 && direct.isValid()) {
            emit openRequested(direct, kVideo);
            return;
        }
        if (timedOut || status != QProcess::NormalExit) {
            QString msg = QStringLiteral("Video adresi çözülemedi (zaman aşımı).");
            if (!lastStep.isEmpty())
                msg += QStringLiteral("\nSon adım: ") + lastStep;
            msg += QStringLiteral("\n\nBu site için eklentideki \"Yüzen pencereye al\" seçeneğini kullanın.");
            setError(msg);
            return;
        }
        if (out.contains(QLatin1String("Unsupported URL"))) {
            // yt-dlp tanımıyor: belki uzantısız doğrudan bir akıştır
            emit openRequested(pageUrl, kVideo);
            return;
        }
        const QString reason = lastErrorLine(out);
        setError((reason.isEmpty()
                     ? QStringLiteral("Bu sayfadaki video alınamadı.")
                     : QStringLiteral("Bu sayfadaki video alınamadı: ") + reason)
                 + QStringLiteral("\n\nBu site için eklentideki \"Yüzen pencereye al\" seçeneğini kullanın."));
    });

    connect(proc, &QProcess::errorOccurred, this, [this, proc, pageUrl](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart || proc != m_resolver)
            return;
        m_resolver = nullptr;
        setResolveStatus({});
        emit resolvingChanged();
        proc->deleteLater();
        emit openRequested(pageUrl, kVideo);
    });

    // Çok uzun sürerse durdur
    QTimer::singleShot(90000, proc, [proc]() {
        if (proc->state() != QProcess::NotRunning) {
            proc->setProperty("fmTimedOut", true);
            proc->kill();
        }
    });

    // "b": hem görüntü hem ses içeren en iyi tek akış (Qt ayrı akışları birleştiremez)
    // --force-ipv4: sanal makinelerde/bazı ağlarda bozuk IPv6 yüzünden takılmayı önler
    proc->start(exe, {
        QStringLiteral("--no-playlist"),
        QStringLiteral("--no-quiet"),
        QStringLiteral("--force-ipv4"),
        QStringLiteral("--socket-timeout"), QStringLiteral("15"),
        QStringLiteral("-f"), QStringLiteral("b/best"),
        QStringLiteral("-g"),
        pageUrl.toString(QUrl::FullyEncoded)
    });
}

void AppBridge::cancelResolve()
{
    if (!m_resolver)
        return;
    QProcess *proc = m_resolver;
    m_resolver = nullptr;
    if (proc->state() != QProcess::NotRunning)
        proc->kill(); // finished geldiğinde kendini siler
    else
        proc->deleteLater();
    setResolveStatus({});
    emit resolvingChanged();
}
