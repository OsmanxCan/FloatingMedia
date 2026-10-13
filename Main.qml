import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtMultimedia

Window {
    id: root

    visible: true
    width: 640
    height: 420
    minimumWidth: 240
    minimumHeight: 160

    color: "#111318"
    title: "FloatingMedia"

    flags: Qt.Window
         | Qt.FramelessWindowHint
         | Qt.WindowStaysOnTopHint

    Component.onCompleted: {
        visibility = Window.Windowed
        flags = Qt.Window
              | Qt.FramelessWindowHint
              | Qt.WindowStaysOnTopHint
        root.requestActivate()

        if (typeof AppBridge !== "undefined")
            AppBridge.openInitial()
    }

    property url mediaSource: ""
    property bool isVideo: false
    property bool imageError: false
    property bool chromeVisible: true
    property bool seeking: false
    property bool hasMedia: mediaSource.toString() !== ""
    property string statusMessage: ""
    property real volumeValue: 1.0
    readonly property bool resolving: typeof AppBridge !== "undefined" && AppBridge.resolving

    readonly property int headerHeight: 42
    readonly property int controlsHeight: 78
    readonly property int edgeSize: 6

    function classifyMedia(fileUrl) {
        const path = fileUrl.toString().toLowerCase()
        if (/\.(png|jpg|jpeg|webp|bmp|gif)(\?.*)?$/.test(path))
            return "image"
        if (/\.(mp4|mkv|webm|mov|avi|m4v|mpeg|mpg|ogv)(\?.*)?$/.test(path))
            return "video"
        if (/^https?:\/\//.test(path))
            return "video"
        return "unknown"
    }

    function openMedia(fileUrl, kindHint) {
        const kind = (kindHint === "image" || kindHint === "video")
                     ? kindHint : classifyMedia(fileUrl)
        if (kind === "unknown") {
            statusMessage = "Desteklenmeyen medya türü veya adres."
            return
        }

        imageError = false
        statusMessage = ""
        isVideo = (kind === "video")
        mediaSource = fileUrl
        chromeVisible = true
        hideChromeTimer.restart()
    }

    function formatTime(ms) {
        if (ms < 0 || !isFinite(ms))
            return "0:00"
        const totalSec = Math.floor(ms / 1000)
        const m = Math.floor(totalSec / 60)
        const s = totalSec % 60
        return m + ":" + ("0" + s).slice(-2)
    }

    function togglePlayback() {
        if (!root.isVideo || root.mediaSource.toString() === "")
            return
        if (player.playbackState === MediaPlayer.PlayingState)
            player.pause()
        else
            player.play()
    }

    function seekBy(deltaMs) {
        if (!root.isVideo || player.duration <= 0)
            return
        const next = Math.min(Math.max(player.position + deltaMs, 0), player.duration)
        player.setPosition(next)
    }

    function toggleMute() {
        audioOutput.muted = !audioOutput.muted
    }

    onMediaSourceChanged: {
        player.stop()
        if (isVideo && mediaSource.toString() !== "")
            player.play()
    }

    onVolumeValueChanged: {
        audioOutput.volume = volumeValue
    }

    property bool pointerInside: false

    onPointerInsideChanged: {
        if (!hasMedia) {
            chromeVisible = true
            return
        }
        if (pointerInside) {
            chromeVisible = true
            hideChromeTimer.stop()
        } else {
            hideChromeTimer.restart()
        }
    }

    onHasMediaChanged: {
        if (!hasMedia)
            chromeVisible = true
        else if (!pointerInside)
            hideChromeTimer.restart()
    }

    Connections {
        target: (typeof AppBridge !== "undefined") ? AppBridge : null
        function onOpenRequested(url, kind) {
            root.openMedia(url, kind)
            root.requestActivate()
            root.raise()
        }
        function onLastErrorChanged() {
            if (AppBridge.lastError && AppBridge.lastError.length > 0)
                root.statusMessage = AppBridge.lastError
        }
        function onResolvingChanged() {
            if (AppBridge.resolving) {
                // Yeni sayfa videosu hazırlanıyor: eskisini kapat
                player.stop()
                root.mediaSource = ""
                root.statusMessage = ""
                root.requestActivate()
                root.raise()
            }
        }
    }

    Timer {
        id: hideChromeTimer
        interval: 1200
        onTriggered: {
            if (root.hasMedia && !root.pointerInside)
                root.chromeVisible = false
        }
    }

    HoverHandler {
        onHoveredChanged: root.pointerInside = hovered
    }

    FileDialog {
        id: fileDialog
        title: "Fotoğraf veya video seç"
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "Medya dosyaları (*.png *.jpg *.jpeg *.webp *.bmp *.gif *.mp4 *.mkv *.webm *.mov *.avi *.m4v)",
            "Tüm dosyalar (*)"
        ]
        onAccepted: root.openMedia(selectedFile)
    }

    AudioOutput {
        id: audioOutput
        volume: root.volumeValue
    }

    MediaPlayer {
        id: player
        source: root.isVideo ? root.mediaSource : ""
        audioOutput: audioOutput
        videoOutput: videoOutputItem

        onErrorOccurred: function(error, errorString) {
            if (error === MediaPlayer.NoError)
                return
            let msg = errorString && errorString.length > 0
                ? ("Video açılamadı: " + errorString)
                : "Video açılamadı veya desteklenmiyor."
            if (/^https?:/.test(root.mediaSource.toString())
                    && typeof AppBridge !== "undefined" && !AppBridge.ytDlpAvailable)
                msg += "\n\nWeb sayfalarındaki videolar için yt-dlp gerekir:\nsudo pacman -S yt-dlp"
            root.statusMessage = msg
        }
    }

    Shortcut { sequence: "Space"; onActivated: root.togglePlayback() }
    Shortcut { sequence: "Left"; onActivated: root.seekBy(-5000) }
    Shortcut { sequence: "Right"; onActivated: root.seekBy(5000) }
    Shortcut { sequence: "M"; onActivated: root.toggleMute() }
    Shortcut { sequence: "O"; onActivated: fileDialog.open() }
    Shortcut { sequence: "Escape"; onActivated: root.close() }
    Shortcut {
        sequence: "Up"
        onActivated: root.volumeValue = Math.min(1.0, root.volumeValue + 0.05)
    }
    Shortcut {
        sequence: "Down"
        onActivated: root.volumeValue = Math.max(0.0, root.volumeValue - 0.05)
    }

    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (!drop.hasUrls || drop.urls.length === 0)
                return
            const u = drop.urls[0]
            // Tarayıcıdan sürüklenen bağlantılar yt-dlp çözümünden geçsin
            if (/^https?:/.test(u.toString()) && typeof AppBridge !== "undefined")
                AppBridge.requestOpen(u.toString(), "")
            else
                root.openMedia(u)
        }
    }

    Item {
        id: contentRoot
        anchors.fill: parent

        Rectangle {
            id: header
            z: 5
            width: parent.width
            height: root.chromeVisible ? root.headerHeight : 0
            color: "#20232b"
            clip: true
            opacity: root.chromeVisible ? 1 : 0

            Behavior on height { NumberAnimation { duration: 120 } }
            Behavior on opacity { NumberAnimation { duration: 120 } }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onPressed: root.startSystemMove()
            }

            Row {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                spacing: 8
                visible: root.chromeVisible

                Text {
                    height: parent.height
                    width: Math.max(40, parent.width - 108)
                    text: {
                        if (!root.hasMedia)
                            return "FloatingMedia"
                        if (root.isVideo) {
                            if (player.playbackState === MediaPlayer.PlayingState)
                                return "Video — oynatılıyor"
                            if (player.playbackState === MediaPlayer.PausedState)
                                return "Video — duraklatıldı"
                            return "Video"
                        }
                        return "Fotoğraf"
                    }
                    color: "#f1f3f5"
                    font.pixelSize: 13
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                Button {
                    width: 42
                    height: 34
                    anchors.verticalCenter: parent.verticalCenter
                    text: "＋"
                    onClicked: fileDialog.open()
                }

                Button {
                    width: 42
                    height: 34
                    anchors.verticalCenter: parent.verticalCenter
                    text: "×"
                    onClicked: root.close()
                }
            }
        }

        Item {
            id: mediaArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.bottom: controls.top
            clip: true

            VideoOutput {
                id: videoOutputItem
                anchors.fill: parent
                visible: root.isVideo && root.hasMedia && player.error === MediaPlayer.NoError
                fillMode: VideoOutput.PreserveAspectFit
            }

            Image {
                id: photoItem
                anchors.fill: parent
                source: (!root.isVideo && root.hasMedia) ? root.mediaSource : ""
                visible: !root.isVideo && root.hasMedia && status === Image.Ready
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false

                onStatusChanged: {
                    if (status === Image.Error) {
                        root.imageError = true
                        root.statusMessage = "Fotoğraf yüklenemedi veya desteklenmiyor."
                    } else if (status === Image.Ready) {
                        root.imageError = false
                    }
                }

                onSourceChanged: root.imageError = false
            }

            MouseArea {
                id: mediaMouse
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                hoverEnabled: true

                property real pressX: 0
                property real pressY: 0
                property bool dragged: false
                readonly property real dragThreshold: 6

                onPressed: (mouse) => {
                    pressX = mouse.x
                    pressY = mouse.y
                    dragged = false
                }

                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return
                    const dx = mouse.x - pressX
                    const dy = mouse.y - pressY
                    if (!dragged && (dx * dx + dy * dy) >= dragThreshold * dragThreshold) {
                        dragged = true
                        root.startSystemMove()
                    }
                }

                onClicked: {
                    if (!dragged && root.isVideo && root.hasMedia)
                        root.togglePlayback()
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 8
                visible: !root.hasMedia && root.statusMessage === "" && !root.resolving

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "FloatingMedia"
                    color: "#f1f3f5"
                    font.pixelSize: root.width < 320 ? 18 : 24
                    font.bold: true
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.parent.width - 24, 320)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: "Dosya aç, sürükle-bırak veya tarayıcıdan gönder"
                    color: "#a0a5b1"
                    font.pixelSize: 12
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Dosya aç"
                    onClicked: fileDialog.open()
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 10
                visible: root.resolving

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: root.resolving
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Video hazırlanıyor…"
                    color: "#d8dbe3"
                    font.pixelSize: 13
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(mediaArea.width - 24, 420)
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    visible: text !== ""
                    text: typeof AppBridge !== "undefined" ? AppBridge.resolveStatus : ""
                    color: "#8a90a0"
                    font.pixelSize: 11
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "İptal"
                    onClicked: AppBridge.cancelResolve()
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 24
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: root.statusMessage !== ""
                    && ((root.isVideo && player.error !== MediaPlayer.NoError)
                        || root.imageError
                        || !root.hasMedia)
                text: root.statusMessage
                color: "#ff8585"
                font.pixelSize: 13
            }
        }

        Rectangle {
            id: controls
            z: 5
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: root.chromeVisible ? root.controlsHeight : 0
            color: "#20232b"
            clip: true
            opacity: root.chromeVisible ? 1 : 0

            Behavior on height { NumberAnimation { duration: 120 } }
            Behavior on opacity { NumberAnimation { duration: 120 } }

            Column {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4
                visible: root.chromeVisible

                Row {
                    width: parent.width
                    height: 34
                    spacing: 6

                    Button {
                        width: root.width < 360 ? 40 : 68
                        height: 32
                        anchors.verticalCenter: parent.verticalCenter
                        enabled: root.isVideo && root.hasMedia
                        text: root.width < 360
                              ? (player.playbackState === MediaPlayer.PlayingState ? "||" : ">")
                              : (player.playbackState === MediaPlayer.PlayingState ? "Duraklat" : "Oynat")
                        onClicked: root.togglePlayback()
                    }

                    Slider {
                        id: seekSlider
                        width: Math.max(40, parent.width - (root.width < 360 ? 40 : 68) - 100 - 12)
                        anchors.verticalCenter: parent.verticalCenter
                        from: 0
                        to: Math.max(player.duration, 1)
                        enabled: root.isVideo && root.hasMedia && player.duration > 0

                        Binding on value {
                            when: !root.seeking
                            value: player.position
                        }

                        onPressedChanged: {
                            if (pressed) {
                                root.seeking = true
                            } else {
                                player.setPosition(value)
                                root.seeking = false
                            }
                        }

                        onMoved: {
                            if (root.seeking)
                                player.setPosition(value)
                        }
                    }

                    Text {
                        width: 92
                        height: parent.height
                        text: root.isVideo && root.hasMedia && player.duration > 0
                              ? (root.formatTime(root.seeking ? seekSlider.value : player.position)
                                 + " / " + root.formatTime(player.duration))
                              : "0:00 / 0:00"
                        color: "#d8dbe3"
                        font.pixelSize: 11
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                Row {
                    width: parent.width
                    height: 28
                    spacing: 6

                    Button {
                        width: root.width < 360 ? 44 : 56
                        height: 26
                        anchors.verticalCenter: parent.verticalCenter
                        enabled: root.isVideo && root.hasMedia
                        text: audioOutput.muted ? "Sessiz" : "Ses"
                        font.pixelSize: 10
                        onClicked: root.toggleMute()
                    }

                    Slider {
                        id: volumeSlider
                        width: Math.max(40, parent.width - (root.width < 360 ? 44 : 56) - 40 - 12)
                        anchors.verticalCenter: parent.verticalCenter
                        from: 0
                        to: 1
                        value: root.volumeValue
                        enabled: root.isVideo && root.hasMedia
                        onMoved: root.volumeValue = value
                    }

                    Text {
                        width: 36
                        height: parent.height
                        text: Math.round(root.volumeValue * 100) + "%"
                        color: "#d8dbe3"
                        font.pixelSize: 10
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    component ResizeEdge: MouseArea {
        required property int edges
        property int cursor: Qt.ArrowCursor
        z: 20
        hoverEnabled: true
        cursorShape: cursor
        onPressed: root.startSystemResize(edges)
    }

    ResizeEdge {
        edges: Qt.LeftEdge
        cursor: Qt.SizeHorCursor
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.edgeSize
        anchors.bottomMargin: root.edgeSize
        width: root.edgeSize
    }
    ResizeEdge {
        edges: Qt.RightEdge
        cursor: Qt.SizeHorCursor
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.edgeSize
        anchors.bottomMargin: root.edgeSize
        width: root.edgeSize
    }
    ResizeEdge {
        edges: Qt.TopEdge
        cursor: Qt.SizeVerCursor
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.edgeSize
        anchors.rightMargin: root.edgeSize
        height: root.edgeSize
    }
    ResizeEdge {
        edges: Qt.BottomEdge
        cursor: Qt.SizeVerCursor
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.edgeSize
        anchors.rightMargin: root.edgeSize
        height: root.edgeSize
    }
    ResizeEdge {
        edges: Qt.LeftEdge | Qt.TopEdge
        cursor: Qt.SizeFDiagCursor
        anchors.left: parent.left
        anchors.top: parent.top
        width: root.edgeSize
        height: root.edgeSize
    }
    ResizeEdge {
        edges: Qt.RightEdge | Qt.TopEdge
        cursor: Qt.SizeBDiagCursor
        anchors.right: parent.right
        anchors.top: parent.top
        width: root.edgeSize
        height: root.edgeSize
    }
    ResizeEdge {
        edges: Qt.LeftEdge | Qt.BottomEdge
        cursor: Qt.SizeBDiagCursor
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: root.edgeSize
        height: root.edgeSize
    }

    Rectangle {
        z: 21
        width: 18
        height: 18
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: "#454b59"

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeFDiagCursor
            onPressed: root.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
        }
    }
}
