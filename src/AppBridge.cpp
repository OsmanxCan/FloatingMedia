#include "AppBridge.h"
#include "MediaUrl.h"

#include <QLocalServer>
#include <QLocalSocket>

AppBridge::AppBridge(QObject *parent)
    : QObject(parent)
{
}

AppBridge::~AppBridge()
{
    if (m_server)
        m_server->close();
}

void AppBridge::setInitialUrl(const QUrl &url)
{
    m_initialUrl = url;
}

void AppBridge::setError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

bool AppBridge::forwardToPrimary(const QUrl &url)
{
    QLocalSocket socket;
    socket.connectToServer(QString::fromUtf8(kServerName));
    if (!socket.waitForConnected(800))
        return false;
    socket.write(url.toString(QUrl::FullyEncoded).toUtf8());
    socket.write("\n");
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
            const QByteArray data = socket->readAll().trimmed();
            const QUrl url = MediaUrl::fromUserInput(QString::fromUtf8(data));
            QString error;
            if (!MediaUrl::isAcceptable(url, &error)) {
                setError(error);
                socket->write("ERR\n");
            } else {
                setError({});
                emit openRequested(url);
                socket->write("OK\n");
            }
            socket->flush();
            socket->disconnectFromServer();
        });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    }
}

bool AppBridge::requestOpen(const QString &input)
{
    const QUrl url = MediaUrl::fromUserInput(input);
    QString error;
    if (!MediaUrl::isAcceptable(url, &error)) {
        setError(error);
        return false;
    }
    setError({});
    emit openRequested(url);
    return true;
}
