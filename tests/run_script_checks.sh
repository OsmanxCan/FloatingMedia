#!/usr/bin/env bash
# Host'a kurulum yapmaz; yalnızca betik sözdizimi ve dry-run kontrolü.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

bash -n install.sh
bash -n uninstall.sh
bash -n packaging/make-source-tarball.sh

./install.sh --help >/dev/null
./uninstall.sh --help >/dev/null

# Dry-run: kullanıcı dizinine yazmamalı
./install.sh --dry-run --skip-build --no-kde -y >/tmp/fm-install-dry.out
grep -q 'dry-run' /tmp/fm-install-dry.out

./uninstall.sh --dry-run -y >/tmp/fm-uninstall-dry.out
grep -q 'dry-run' /tmp/fm-uninstall-dry.out

# KWin metadata
python3 - <<'PY'
import json, pathlib
p = pathlib.Path("kwin-script/metadata.json")
data = json.loads(p.read_text())
assert data["KPlugin"]["Id"] == "floatingmedia-keep-above"
assert data["KPackageStructure"] == "KWin/Script"
print("kwin metadata: ok")
PY

# Extension manifest
python3 - <<'PY'
import json, pathlib
data = json.loads(pathlib.Path("browser-extension/manifest.json").read_text())
assert data["manifest_version"] == 3
assert "nativeMessaging" in data["permissions"]
print("extension manifest: ok")
PY

# Desktop template
grep -q 'StartupWMClass=FloatingMediaApp' resources/floatingmedia.desktop.in
grep -q 'Exec=FloatingMediaApp' resources/floatingmedia.desktop.in

echo "run_script_checks: ok"
