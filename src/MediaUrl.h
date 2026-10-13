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

inline Kind classify(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty() || !isAllowedScheme(url))
        return Kind::Unknown;

    const QString path = url.path().toLower();
    if (path.endsWith(QLatin1String(".png"))
        || path.endsWith(QLatin1String(".jpg"))
        || path.endsWith(QLatin1String(".jpeg"))
        || path.endsWith(QLatin1String(".webp"))
        || path.endsWith(QLatin1String(".bmp"))
        || path.endsWith(QLatin1String(".gif"))) {
        return Kind::Image;
    }

    if (path.endsWith(QLatin1String(".mp4"))
        || path.endsWith(QLatin1String(".mkv"))
        || path.endsWith(QLatin1String(".webm"))
        || path.endsWith(QLatin1String(".mov"))
        || path.endsWith(QLatin1String(".avi"))
        || path.endsWith(QLatin1String(".m4v"))
        || path.endsWith(QLatin1String(".mpeg"))
        || path.endsWith(QLatin1String(".mpg"))
        || path.endsWith(QLatin1String(".ogv"))) {
        return Kind::Video;
    }

    // Uzantısız http(s): oynatıcıya bırak (DRM vb. başarısız olabilir)
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
