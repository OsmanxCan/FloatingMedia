#!/usr/bin/env bash
# FloatingMedia — kullanıcı düzeyinde kurulum (sudo yok).
# Ana geliştirme makinesinde çalıştırmayın; sanal makinede test edin.
set -euo pipefail

PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${XDG_BIN_HOME:-$HOME/.local/bin}"
DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
APP_DIR="$DATA_HOME/applications"
APP_BIN="$BIN_DIR/FloatingMediaApp"
DESKTOP_FILE="$APP_DIR/floatingmedia.desktop"
DESKTOP_TEMPLATE="$PROJECT_DIR/resources/floatingmedia.desktop.in"
SCRIPT_ID="floatingmedia-keep-above"
HOST_NAME="com.floatingmedia.host"
BUILD_DIR="$PROJECT_DIR/build"
BUILT_BIN="$BUILD_DIR/FloatingMediaApp"
EXT_DIR="$PROJECT_DIR/browser-extension"

DRY_RUN=0
SKIP_BUILD=0
ASSUME_YES=0
WITH_KDE=1
WITH_BROWSER=0
CHROME_EXTENSION_ID="REPLACE_WITH_UNPACKED_EXTENSION_ID"
AUTO_KDE=1

usage() {
    cat <<EOF
Kullanım: $(basename "$0") [seçenekler]

  --dry-run                     Değişiklik yapma; yapılacakları listele
  --skip-build                  Mevcut build/FloatingMediaApp kullan
  --no-kde                      KWin kurma/etkinleştirme
  --with-kde                    KWin kur (varsayılan; Plasma yoksa uyarır)
  --with-browser                Native Messaging host JSON'larını yaz
  --chrome-extension-id=ID      Chromium allowed_origins için eklenti kimliği
  -y, --yes                     Onay sorma
  -h, --help                    Yardım

Kurulum hedefleri:
  $APP_BIN
  $DESKTOP_FILE
  KWin: $SCRIPT_ID  (--no-kde ile atlanır)
  Native Messaging (--with-browser):
    $CONFIG_HOME/chromium/NativeMessagingHosts/${HOST_NAME}.json
    $CONFIG_HOME/google-chrome/NativeMessagingHosts/${HOST_NAME}.json
    $HOME/.mozilla/native-messaging-hosts/${HOST_NAME}.json
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run) DRY_RUN=1 ;;
        --skip-build) SKIP_BUILD=1 ;;
        --no-kde) WITH_KDE=0; AUTO_KDE=0 ;;
        --with-kde) WITH_KDE=1; AUTO_KDE=0 ;;
        --with-browser) WITH_BROWSER=1 ;;
        --chrome-extension-id=*) CHROME_EXTENSION_ID="${1#*=}" ;;
        -y|--yes) ASSUME_YES=1 ;;
        -h|--help) usage; exit 0 ;;
        *)
            echo "Bilinmeyen seçenek: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
    shift
done

# Plasma yoksa KDE katmanını zorunlu kılma
if [[ "$AUTO_KDE" -eq 1 ]]; then
    if [[ "${XDG_CURRENT_DESKTOP:-}" != *KDE* ]] && ! command -v kpackagetool6 >/dev/null 2>&1; then
        WITH_KDE=0
        echo "Bilgi: KDE Plasma araçları yok; KWin katmanı atlanacak (--with-kde ile zorlanabilir)."
    fi
fi

run() {
    if [[ "$DRY_RUN" -eq 1 ]]; then
        printf '[dry-run]'
        printf ' %q' "$@"
        printf '\n'
    else
        "$@"
    fi
}

write_desktop() {
    local dest="$1"
    if [[ -f "$DESKTOP_TEMPLATE" ]]; then
        sed "s|^Exec=.*|Exec=$APP_BIN|" "$DESKTOP_TEMPLATE" > "$dest"
    else
        cat > "$dest" <<DESKTOP
[Desktop Entry]
Type=Application
Name=FloatingMedia
Comment=Fotoğraf ve videoları diğer pencerelerin üzerinde görüntüle
Exec=$APP_BIN
Icon=video-x-generic
Terminal=false
Categories=AudioVideo;Video;
StartupWMClass=FloatingMediaApp
DESKTOP
    fi
    chmod 644 "$dest"
}

install_native_host_json() {
    local template="$1"
    local dest="$2"
    local dir
    dir="$(dirname "$dest")"
    run mkdir -p "$dir"
    if [[ "$DRY_RUN" -eq 1 ]]; then
        echo "[dry-run] native host → $dest"
        return
    fi
    sed -e "s|@HOST_PATH@|$APP_BIN --native-messaging-host|" \
        -e "s|@CHROME_EXTENSION_ID@|$CHROME_EXTENSION_ID|" \
        "$template" > "$dest"
    # path alanı tek string olmalı; bayrak için sarmalayıcı betik daha doğru
    # Chrome path'te argüman kabul etmeyebilir — sarmalayıcı yaz
}

# Native host path genelde argümansız ikili ister; sarmalayıcı kullan
write_native_wrapper() {
    local wrapper="$BIN_DIR/floatingmedia-native-host"
    if [[ "$DRY_RUN" -eq 1 ]]; then
        echo "[dry-run] wrapper → $wrapper"
        return
    fi
    cat > "$wrapper" <<WRAP
#!/usr/bin/env bash
exec "$APP_BIN" --native-messaging-host
WRAP
    chmod 755 "$wrapper"
    echo "$wrapper"
}

echo "=== FloatingMedia kurulumu ==="
[[ "$DRY_RUN" -eq 1 ]] && echo "(dry-run: sistem değiştirilmeyecek)"
echo
echo "Yapılacaklar:"
echo "  - Derleme / ikili: $APP_BIN"
echo "  - Masaüstü: $DESKTOP_FILE"
[[ "$WITH_KDE" -eq 1 ]] && echo "  - KWin betiği: $SCRIPT_ID (+ etkinleştirme)"
[[ "$WITH_BROWSER" -eq 1 ]] && echo "  - Native Messaging host dosyaları"
echo

if [[ "$ASSUME_YES" -ne 1 && "$DRY_RUN" -ne 1 ]]; then
    read -r -p "Devam edilsin mi? [y/N] " reply
    case "$reply" in y|Y|yes|YES) ;; *) echo "İptal."; exit 0 ;; esac
fi

need_cmds=()
[[ "$SKIP_BUILD" -eq 0 ]] && need_cmds+=(cmake)
[[ "$WITH_KDE" -eq 1 ]] && need_cmds+=(kpackagetool6 kwriteconfig6)

for cmd in "${need_cmds[@]}"; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Hata: '$cmd' bulunamadı." >&2
        [[ "$WITH_KDE" -eq 1 ]] && echo "Plasma yoksa --no-kde kullanın." >&2
        exit 1
    fi
done

STEP=1
TOTAL=5
[[ "$WITH_BROWSER" -eq 1 ]] && TOTAL=6

echo "[$STEP/$TOTAL] Derleme..."
STEP=$((STEP + 1))
if [[ "$SKIP_BUILD" -eq 0 ]]; then
    run cmake -S "$PROJECT_DIR" -B "$BUILD_DIR"
    run cmake --build "$BUILD_DIR" --parallel
else
    echo "  (atlandı)"
fi

if [[ "$DRY_RUN" -eq 0 && ! -x "$BUILT_BIN" ]]; then
    echo "Hata: $BUILT_BIN bulunamadı." >&2
    exit 1
fi

echo "[$STEP/$TOTAL] Uygulama → $APP_BIN"
STEP=$((STEP + 1))
run mkdir -p "$BIN_DIR" "$APP_DIR"
run install -m 755 "$BUILT_BIN" "$APP_BIN"

if [[ "$WITH_KDE" -eq 1 ]]; then
    echo "[$STEP/$TOTAL] KWin betiği ($SCRIPT_ID)..."
    STEP=$((STEP + 1))
    if [[ "$DRY_RUN" -eq 1 ]]; then
        echo "[dry-run] kpackagetool6 install/upgrade kwin-script"
    else
        if kpackagetool6 --type=KWin/Script --list 2>/dev/null | grep -Fqx "$SCRIPT_ID"; then
            if kpackagetool6 --help 2>&1 | grep -q -- '--upgrade'; then
                kpackagetool6 --type=KWin/Script --upgrade "$PROJECT_DIR/kwin-script"
            else
                kpackagetool6 --type=KWin/Script --remove "$SCRIPT_ID"
                kpackagetool6 --type=KWin/Script --install "$PROJECT_DIR/kwin-script"
            fi
        else
            kpackagetool6 --type=KWin/Script --install "$PROJECT_DIR/kwin-script"
        fi
    fi

    echo "[$STEP/$TOTAL] KWin etkinleştirme..."
    STEP=$((STEP + 1))
    run kwriteconfig6 --file kwinrc --group Plugins --key "${SCRIPT_ID}Enabled" true
    if command -v qdbus6 >/dev/null 2>&1; then
        if [[ "$DRY_RUN" -eq 1 ]]; then
            echo "[dry-run] qdbus6 org.kde.KWin /KWin reconfigure"
        else
            qdbus6 org.kde.KWin /KWin reconfigure 2>/dev/null \
                || echo "Uyarı: KWin reconfigure başarısız (oturum dışı olabilir)."
        fi
    fi
else
    echo "[$STEP/$TOTAL] KDE atlandı"
    STEP=$((STEP + 1))
fi

echo "[$STEP/$TOTAL] Masaüstü girdisi..."
STEP=$((STEP + 1))
if [[ "$DRY_RUN" -eq 1 ]]; then
    echo "[dry-run] $DESKTOP_FILE"
else
    write_desktop "$DESKTOP_FILE"
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    run update-desktop-database "$APP_DIR" 2>/dev/null || true
fi

if [[ "$WITH_BROWSER" -eq 1 ]]; then
    echo "[$STEP/$TOTAL] Native Messaging..."
    WRAPPER_PATH="$BIN_DIR/floatingmedia-native-host"
    if [[ "$DRY_RUN" -eq 1 ]]; then
        echo "[dry-run] $WRAPPER_PATH"
    else
        write_native_wrapper >/dev/null
    fi

    FF_TEMPLATE="$EXT_DIR/native-messaging/com.floatingmedia.host-firefox.json.in"
    CH_TEMPLATE="$EXT_DIR/native-messaging/com.floatingmedia.host-chrome.json.in"
    FF_DEST="$HOME/.mozilla/native-messaging-hosts/${HOST_NAME}.json"

    for chrome_dir in \
        "$CONFIG_HOME/chromium/NativeMessagingHosts" \
        "$CONFIG_HOME/google-chrome/NativeMessagingHosts" \
        "$CONFIG_HOME/BraveSoftware/Brave-Browser/NativeMessagingHosts" \
        "$CONFIG_HOME/microsoft-edge/NativeMessagingHosts"
    do
        dest="$chrome_dir/${HOST_NAME}.json"
        run mkdir -p "$chrome_dir"
        if [[ "$DRY_RUN" -eq 1 ]]; then
            echo "[dry-run] $dest"
        else
            sed -e "s|@HOST_PATH@|$WRAPPER_PATH|" \
                -e "s|@CHROME_EXTENSION_ID@|$CHROME_EXTENSION_ID|" \
                "$CH_TEMPLATE" > "$dest"
        fi
    done

    run mkdir -p "$(dirname "$FF_DEST")"
    if [[ "$DRY_RUN" -eq 1 ]]; then
        echo "[dry-run] $FF_DEST"
    else
        sed -e "s|@HOST_PATH@|$WRAPPER_PATH|" "$FF_TEMPLATE" > "$FF_DEST"
    fi

    echo
    echo "Tarayıcı eklentisi: $EXT_DIR"
    echo "  Firefox: about:debugging → Bu Firefox → Geçici eklenti yükle → manifest.json"
    echo "  Chromium: chrome://extensions → Geliştirici modu → Paketlenmemiş öğe yükle"
    echo "  Chromium ID sonrası: ./install.sh --skip-build --with-browser --chrome-extension-id=ID -y"
fi

# Doğrulama
echo
echo "Doğrulama:"
if [[ "$DRY_RUN" -eq 1 ]]; then
    echo "  (dry-run — atlandı)"
else
    ok=1
    if [[ -x "$APP_BIN" ]]; then
        echo "  [ok] ikili: $APP_BIN"
    else
        echo "  [hata] ikili yok"; ok=0
    fi
    if [[ -f "$DESKTOP_FILE" ]]; then
        echo "  [ok] desktop: $DESKTOP_FILE"
    else
        echo "  [hata] desktop yok"; ok=0
    fi
    if [[ "$WITH_KDE" -eq 1 ]]; then
        if kpackagetool6 --type=KWin/Script --list 2>/dev/null | grep -Fqx "$SCRIPT_ID"; then
            echo "  [ok] KWin betiği listede"
        else
            echo "  [uyarı] KWin betiği listede görünmüyor"
        fi
    fi
    if [[ "$ok" -ne 1 ]]; then
        echo "Kurulum doğrulaması başarısız." >&2
        exit 1
    fi
fi

echo
echo "FloatingMedia kurulum adımları tamamlandı."
echo "Kaldırmak için: ./uninstall.sh"
