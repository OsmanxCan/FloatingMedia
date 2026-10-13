#!/usr/bin/env bash
# FloatingMedia — kullanıcı düzeyinde kaldırma (sudo yok).
# Ana geliştirme makinesinde çalıştırmayın; sanal makinede test edin.
set -euo pipefail

BIN_DIR="${XDG_BIN_HOME:-$HOME/.local/bin}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
APP_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
APP_BIN="$BIN_DIR/FloatingMediaApp"
NATIVE_WRAPPER="$BIN_DIR/floatingmedia-native-host"
DESKTOP_FILE="$APP_DIR/floatingmedia.desktop"
SCRIPT_ID="floatingmedia-keep-above"
HOST_NAME="com.floatingmedia.host"
KWIN_SCRIPT_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/kwin/scripts/$SCRIPT_ID"

DRY_RUN=0
ASSUME_YES=0
PURGE_QT_CONFIG=0
PURGE_BROWSER=0

usage() {
    cat <<EOF
Kullanım: $(basename "$0") [seçenekler]

  --dry-run           Silme; listele
  -y, --yes           Onaysız kaldır
  --purge-qt-config   Qt config dizinlerini de sil
  --purge-browser     Native Messaging host JSON + sarmalayıcıyı sil
  -h, --help          Yardım

Kaldırılacaklar:
  $APP_BIN
  $DESKTOP_FILE
  KWin betiği $SCRIPT_ID
  kwinrc Plugins/${SCRIPT_ID}Enabled (yalnızca bu anahtar)

--purge-browser ile:
  $NATIVE_WRAPPER
  Chromium/Chrome/Brave/Edge + Firefox native messaging JSON

Dokunulmayanlar: diğer kwinrc ayarları, tarayıcı eklenti profili, build/.

pacman kurulumu için: sudo pacman -R floatingmedia-kwin floatingmedia
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run) DRY_RUN=1 ;;
        -y|--yes) ASSUME_YES=1 ;;
        --purge-qt-config) PURGE_QT_CONFIG=1 ;;
        --purge-browser) PURGE_BROWSER=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Bilinmeyen: $1" >&2; usage >&2; exit 1 ;;
    esac
    shift
done

run() {
    if [[ "$DRY_RUN" -eq 1 ]]; then
        printf '[dry-run]'; printf ' %q' "$@"; printf '\n'
    else
        "$@"
    fi
}

remove_file() {
    local path="$1"
    if [[ -e "$path" || -L "$path" ]]; then
        run rm -f "$path"
        echo "  kaldırıldı: $path"
    else
        echo "  yok (atlandı): $path"
    fi
}

echo "=== FloatingMedia kaldırma ==="
[[ "$DRY_RUN" -eq 1 ]] && echo "(dry-run)"
echo
echo "Kaldırılacaklar: ikili, desktop, KWin betiği, kwinrc anahtarı"
[[ "$PURGE_BROWSER" -eq 1 ]] && echo "  + native messaging dosyaları"
[[ "$PURGE_QT_CONFIG" -eq 1 ]] && echo "  + Qt config"
echo

if [[ "$ASSUME_YES" -ne 1 && "$DRY_RUN" -ne 1 ]]; then
    read -r -p "Devam? [y/N] " reply
    case "$reply" in y|Y|yes|YES) ;; *) echo "İptal."; exit 0 ;; esac
fi

echo "[1/5] İkili..."
remove_file "$APP_BIN"

echo "[2/5] Masaüstü..."
remove_file "$DESKTOP_FILE"
command -v update-desktop-database >/dev/null 2>&1 \
    && run update-desktop-database "$APP_DIR" 2>/dev/null || true

echo "[3/5] KWin..."
if command -v kpackagetool6 >/dev/null 2>&1; then
    if kpackagetool6 --type=KWin/Script --list 2>/dev/null | grep -Fqx "$SCRIPT_ID"; then
        run kpackagetool6 --type=KWin/Script --remove "$SCRIPT_ID"
        echo "  betik kaldırıldı"
    else
        echo "  listede yok"
        [[ -d "$KWIN_SCRIPT_DIR" ]] && run rm -rf "$KWIN_SCRIPT_DIR"
    fi
elif [[ -d "$KWIN_SCRIPT_DIR" ]]; then
    run rm -rf "$KWIN_SCRIPT_DIR"
fi

echo "[4/5] kwinrc anahtarı..."
if command -v kwriteconfig6 >/dev/null 2>&1; then
    run kwriteconfig6 --file kwinrc --group Plugins --key "${SCRIPT_ID}Enabled" --delete
    echo "  anahtar silindi (veya yoktu)"
else
    echo "  kwriteconfig6 yok"
fi
if command -v qdbus6 >/dev/null 2>&1; then
    [[ "$DRY_RUN" -eq 1 ]] && echo "[dry-run] qdbus6 reconfigure" \
        || qdbus6 org.kde.KWin /KWin reconfigure 2>/dev/null || true
fi

echo "[5/5] İsteğe bağlı temizlik..."
if [[ "$PURGE_BROWSER" -eq 1 ]]; then
    remove_file "$NATIVE_WRAPPER"
    for f in \
        "$CONFIG_HOME/chromium/NativeMessagingHosts/${HOST_NAME}.json" \
        "$CONFIG_HOME/google-chrome/NativeMessagingHosts/${HOST_NAME}.json" \
        "$CONFIG_HOME/BraveSoftware/Brave-Browser/NativeMessagingHosts/${HOST_NAME}.json" \
        "$CONFIG_HOME/microsoft-edge/NativeMessagingHosts/${HOST_NAME}.json" \
        "$HOME/.mozilla/native-messaging-hosts/${HOST_NAME}.json"
    do
        remove_file "$f"
    done
else
    echo "  tarayıcı host dosyaları bırakıldı (--purge-browser ile silinir)"
fi

if [[ "$PURGE_QT_CONFIG" -eq 1 ]]; then
    for d in "$CONFIG_HOME/FloatingMedia" "$CONFIG_HOME/Floating Media"; do
        if [[ -d "$d" ]]; then
            run rm -rf "$d"
            echo "  kaldırıldı: $d"
        fi
    done
fi

echo
[[ "$DRY_RUN" -eq 1 ]] && echo "Dry-run tamamlandı." || echo "Kaldırma tamamlandı."
