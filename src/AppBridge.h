#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>

class QLocalServer;
class QProcess;

// Medya türü ipuçları (tarayıcıdan / komut satırından gelir):
//   "image" — fotoğraf, "video" — doğrudan video dosyası/akışı,
//   "page"  — video içeren web sayfası (yt-dlp ile çözülür), ""  — otomatik
class AppBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl initialUrl READ initialUrl CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool resolving READ resolving NOTIFY resolvingChanged)
    Q_PROPERTY(bool ytDlpAvailable READ ytDlpAvailable CONSTANT)

public:
    explicit AppBridge(QObject *parent = nullptr);
    ~AppBridge() override;

    QUrl initialUrl() const { return m_initialUrl; }
    QString lastError() const { return m_lastError; }
    bool resolving() const { return m_resolver != nullptr; }
    bool ytDlpAvailable() const;

    void setInitialUrl(const QUrl &url, const QString &kind = QString());

    // true = bu süreç birincil örnek
    bool becomePrimaryInstance();
    bool forwardToPrimary(const QUrl &url, const QString &kind = QString());

    Q_INVOKABLE bool requestOpen(const QString &input, const QString &kindHint = QString());
    Q_INVOKABLE void openInitial();

    static bool isValidKind(const QString &kind);
    // IPC satırı: "<tür>\t<url>" (tür boş olabilir; eski biçim yalnızca "<url>")
    static QByteArray encodeIpc(const QUrl &url, const QString &kind);
    static bool decodeIpc(const QByteArray &line, QUrl *url, QString *kind);

signals:
    void openRequested(const QUrl &url, const QString &kind);
    void lastErrorChanged();
    void resolvingChanged();

private:
    void setError(const QString &message);
    void handleIpcConnection();
    void dispatch(const QUrl &url, const QString &kindHint);
    void resolveWithYtDlp(const QUrl &pageUrl);
    void cancelResolve();

    QUrl m_initialUrl;
    QString m_initialKind;
    QString m_lastError;
    QLocalServer *m_server = nullptr;
    QProcess *m_resolver = nullptr;
    static constexpr const char *kServerName = "floatingmedia-ipc";
};
