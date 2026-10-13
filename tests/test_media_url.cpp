#include "MediaUrl.h"

#include <cstdio>
#include <cstdlib>

static int failures = 0;

static void expect(bool cond, const char *msg)
{
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++failures;
    }
}

int main()
{
    {
        const QUrl u = MediaUrl::fromUserInput(QStringLiteral("/tmp/photo.jpg"));
        expect(MediaUrl::isAcceptable(u), "local jpg acceptable");
        expect(MediaUrl::classify(u) == MediaUrl::Kind::Image, "jpg is image");
    }
    {
        const QUrl u = MediaUrl::fromUserInput(QStringLiteral("https://example.com/v.mp4"));
        expect(MediaUrl::isAcceptable(u), "https mp4 acceptable");
        expect(MediaUrl::classify(u) == MediaUrl::Kind::Video, "mp4 is video");
    }
    {
        const QUrl u = MediaUrl::fromUserInput(QStringLiteral("https://example.com/stream"));
        expect(MediaUrl::isAcceptable(u), "https without ext treated as video attempt");
        expect(MediaUrl::classify(u) == MediaUrl::Kind::Video, "http unknown ext -> video");
    }
    {
        const QUrl u = MediaUrl::fromUserInput(QStringLiteral("javascript:alert(1)"));
        QString err;
        expect(!MediaUrl::isAcceptable(u, &err), "javascript rejected");
        expect(!err.isEmpty(), "error message set");
    }
    {
        const QUrl u = MediaUrl::fromUserInput(QStringLiteral(""));
        expect(!MediaUrl::isAcceptable(u), "empty rejected");
    }
    {
        const QUrl u = MediaUrl::fromUserInput(QStringLiteral("/tmp/file.xyz"));
        expect(!MediaUrl::isAcceptable(u), "unknown local ext rejected");
    }

    if (failures) {
        std::fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    std::printf("test_media_url: ok\n");
    return 0;
}
