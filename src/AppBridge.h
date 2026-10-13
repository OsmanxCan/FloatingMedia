#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QLocalServer;

class AppBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl initialUrl READ initialUrl CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit AppBridge(QObject *parent = nullptr);
    ~AppBridge() override;

    QUrl initialUrl() const { return m_initialUrl; }
    QString lastError() const { return m_lastError; }

    void setInitialUrl(const QUrl &url);

    // true = bu süreç birincil örnek
    bool becomePrimaryInstance();
    bool forwardToPrimary(const QUrl &url);

    Q_INVOKABLE bool requestOpen(const QString &input);

signals:
    void openRequested(const QUrl &url);
    void lastErrorChanged();

private:
    void setError(const QString &message);
    void handleIpcConnection();

    QUrl m_initialUrl;
    QString m_lastError;
    QLocalServer *m_server = nullptr;
    static constexpr const char *kServerName = "floatingmedia-ipc";
};
