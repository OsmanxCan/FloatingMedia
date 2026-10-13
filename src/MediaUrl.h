#pragma once

#include <QString>
#include <QUrl>

namespace MediaUrl {

enum class Kind {
    Unknown,
    Image,
    Video
};

inline bool isAllowedScheme(const QUrl &url)
{
    const QString s = url.scheme().toLower();
    return s == QStringLiteral("file")
        || s == QStringLiteral("http")
        || s == QStringLiteral("https");
}

// Yalnızca dosya uzantısına bakar; uzantı tanınmıyorsa Unknown döner.
inline Kind classifyByExtension(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty() || !isAllowedScheme(url))
        return Kind::Unknown;

    const QString path = url.path().toLower();
    static const char *const imageExts[] = {
        ".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif", ".avif", ".svg"
    };
    static const char *const videoExts[] = {
        ".mp4", ".mkv", ".webm", ".mov", ".avi", ".m4v", ".mpeg", ".mpg", ".ogv",
        ".m3u8", ".ts"
    };
    for (const char *ext : imageExts) {
        if (path.endsWith(QLatin1String(ext)))
            return Kind::Image;
    }
    for (const char *ext : videoExts) {
        if (path.endsWith(QLatin1String(ext)))
            return Kind::Video;
    }
    return Kind::Unknown;
}

inline Kind classify(const QUrl &url)
{
    const Kind byExt = classifyByExtension(url);
    if (byExt != Kind::Unknown)
        return byExt;
    if (!url.isValid() || url.isEmpty() || !isAllowedScheme(url))
        return Kind::Unknown;

    // Uzantısız http(s): sayfa veya akış olabilir (yt-dlp ile çözülür)
    const QString scheme = url.scheme().toLower();
    if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https"))
        return Kind::Video;

    return Kind::Unknown;
}

inline bool isAcceptable(const QUrl &url, QString *errorOut = nullptr)
{
    if (!url.isValid() || url.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Geçersiz veya boş adres.");
        return false;
    }
    if (!isAllowedScheme(url)) {
        if (errorOut)
            *errorOut = QStringLiteral("Yalnızca file, http ve https adresleri desteklenir.");
        return false;
    }
    if (classify(url) == Kind::Unknown) {
        if (errorOut)
            *errorOut = QStringLiteral("Desteklenmeyen medya türü veya uzantı.");
        return false;
    }
    return true;
}

inline QUrl fromUserInput(const QString &input)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty())
        return {};

    QUrl url = QUrl::fromUserInput(trimmed);
    if (url.isLocalFile() || url.scheme().compare(QLatin1String("file"), Qt::CaseInsensitive) == 0)
        return url;

    if (url.scheme().isEmpty())
        return QUrl::fromLocalFile(trimmed);

    return url;
}

} // namespace MediaUrl
