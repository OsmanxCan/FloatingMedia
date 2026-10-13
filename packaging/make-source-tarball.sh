#!/usr/bin/env bash
# Proje kökünden kaynak tarball üretir (build/ hariç). Host'a kurulum yapmaz.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="$(grep -E '^\s*project\(FloatingMedia VERSION' "$ROOT/CMakeLists.txt" \
  | sed -E 's/.*VERSION ([0-9.]+).*/\1/')"
NAME="floatingmedia-${VERSION}"
OUT_DIR="$ROOT/packaging/arch"
OUT_TAR="$OUT_DIR/${NAME}.tar.gz"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

mkdir -p "$STAGE/$NAME" "$OUT_DIR"

# İzlenen kaynaklar; build ve paket çıktıları hariç
tar -C "$ROOT" -cf - \
  --exclude='./build' \
  --exclude='./packaging/arch/*.pkg.tar*' \
  --exclude='./packaging/arch/*.tar.gz' \
  --exclude='./.git' \
  CMakeLists.txt \
  Main.qml \
  src \
  kwin-script \
  resources \
  browser-extension \
  tests \
  install.sh \
  uninstall.sh \
  packaging \
  README.md \
  TODO.md \
  .gitignore \
  | tar -C "$STAGE/$NAME" -xf -

tar -C "$STAGE" -czf "$OUT_TAR" "$NAME"
echo "Oluşturuldu: $OUT_TAR"
echo "VM'de: cd packaging/arch && makepkg -s"
