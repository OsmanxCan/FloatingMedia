#include "NativeMessagingHost.h"
#include "MediaUrl.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
#include <QtEndian>

#include <cstdio>
#include <cstring>

namespace {

bool readExact(FILE *in, char *buf, size_t n)
{
    size_t got = 0;
    while (got < n) {
        const size_t r = fread(buf + got, 1, n - got, in);
        if (r == 0)
            return false;
        got += r;
    }
    return true;
}

bool writeMessage(FILE *out, const QJsonObject &obj)
{
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    quint32 len = static_cast<quint32>(payload.size());
    quint32 le = qToLittleEndian(len);
    if (fwrite(&le, 1, 4, out) != 4)
        return false;
    if (fwrite(payload.constData(), 1, payload.size(), out) != static_cast<size_t>(payload.size()))
        return false;
    fflush(out);
    return true;
}

bool forwardOrLaunch(const QUrl &url)
{
    QLocalSocket socket;
    socket.connectToServer(QStringLiteral("floatingmedia-ipc"));
    if (socket.waitForConnected(400)) {
        socket.write(url.toString(QUrl::FullyEncoded).toUtf8());
        socket.write("\n");
        socket.flush();
        socket.waitForBytesWritten(400);
        socket.disconnectFromServer();
        return true;
    }

    const QString app = QCoreApplication::applicationFilePath();
    return QProcess::startDetached(app, {url.toString(QUrl::FullyEncoded)});
}

} // namespace

int runNativeMessagingHost(QCoreApplication &app)
{
    Q_UNUSED(app);
    FILE *in = stdin;
    FILE *out = stdout;

    while (true) {
        quint32 lenLe = 0;
        if (!readExact(in, reinterpret_cast<char *>(&lenLe), 4))
            return 0;

        const quint32 len = qFromLittleEndian(lenLe);
        if (len == 0 || len > 1024 * 1024) {
            writeMessage(out, QJsonObject{
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Geçersiz mesaj boyutu.")}
            });
            continue;
        }

        QByteArray raw(static_cast<int>(len), Qt::Uninitialized);
        if (!readExact(in, raw.data(), len))
            return 0;

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            writeMessage(out, QJsonObject{
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("JSON ayrıştırılamadı.")}
            });
            continue;
        }

        const QJsonObject msg = doc.object();
        const QString action = msg.value(QStringLiteral("action")).toString(QStringLiteral("open"));
        if (action != QStringLiteral("open") && action != QStringLiteral("ping")) {
            writeMessage(out, QJsonObject{
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Bilinmeyen action.")}
            });
            continue;
        }

        if (action == QStringLiteral("ping")) {
            writeMessage(out, QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("pong"), true}
            });
            continue;
        }

        const QString urlText = msg.value(QStringLiteral("url")).toString();
        const QUrl url = MediaUrl::fromUserInput(urlText);
        QString error;
        if (!MediaUrl::isAcceptable(url, &error)) {
            writeMessage(out, QJsonObject{
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), error}
            });
            continue;
        }

        if (!forwardOrLaunch(url)) {
            writeMessage(out, QJsonObject{
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("FloatingMedia başlatılamadı.")}
            });
            continue;
        }

        writeMessage(out, QJsonObject{
            {QStringLiteral("ok"), true},
            {QStringLiteral("opened"), url.toString()}
        });
    }
}
