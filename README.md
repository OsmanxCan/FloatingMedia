# FloatingMedia

Linux için kayan (picture-in-picture tarzı) fotoğraf ve video görüntüleyici.

Diğer pencerelerin üzerinde tutulabilen, taşınabilir ve yeniden boyutlandırılabilir küçük bir pencerede medya açmanızı sağlar.

## İndir ve kur

Hazır sürümler: **[Releases](https://github.com/OsmanxCan/FloatingMedia/releases/latest)**

**AppImage (tüm dağıtımlar):**

```bash
chmod +x FloatingMedia-*-x86_64.AppImage
./FloatingMedia-*-x86_64.AppImage
```

**Arch / CachyOS:**

```bash
sudo pacman -U floatingmedia-*.pkg.tar.zst
```

KDE Plasma'da pencere otomatik olarak her zaman üstte tutulur; ek bir kurulum gerekmez.

**Tarayıcı eklentisi (isteğe bağlı):** Arch paketi tarayıcı bağlantısını (Native Messaging) otomatik kurar; eklentinin kendisini tarayıcıya bir kez elle yüklemeniz gerekir (tarayıcılar, mağaza dışındaki eklentilerin paketle kurulmasına izin vermez). Eklenti dosyaları: `/usr/share/floatingmedia/browser-extension` (veya Releases'taki `floatingmedia-browser-extension-*.zip`).

- Chromium / Chrome: `chrome://extensions` → Geliştirici modu → **Paketlenmemiş öğe yükle** → `/usr/share/floatingmedia/browser-extension`
- Firefox: `about:debugging#/runtime/this-firefox` → **Geçici eklenti yükle** → `/usr/share/floatingmedia/browser-extension/manifest.json` (Firefox yeniden başlayınca kaldırılır)

Sonra bir videoya / görsele / bağlantıya sağ tıklayıp **FloatingMedia'da aç** deyin.

## Yeni sürüm yayınlama (geliştirici)

1. `CMakeLists.txt` içindeki `project(... VERSION x.y.z)` ve `packaging/arch/PKGBUILD` içindeki `pkgver` değerini güncelleyin.
2. Commit + etiket gönderin:
   ```bash
   git tag vX.Y.Z
   git push origin main --tags
   ```
3. GitHub Actions (`.github/workflows/release.yml`) paketleri derleyip Release sayfasına yükler.

## Özellikler

- Fotoğraf ve video açma (dosya seçici, sürükle-bırak, CLI, tarayıcı uzantısı)
- Qt Multimedia ile oynat / duraklat / seek / ses / sessize alma
- Çerçevesiz pencere; sürükleme; çok kenarlı boyutlandırma
- Küçük pencerede otomatik gizlenen kontroller
- KDE Plasma 6 (Wayland) için isteğe bağlı KWin `keepAbove` betiği
- Chromium / Firefox için Native Messaging tabanlı tarayıcı köprüsü (ilk sürüm)

## Teknolojiler

- C++17, Qt 6 (Quick, Quick Controls 2, Multimedia)
- QML arayüz
- CMake
- KWin JavaScript (Plasma)
- WebExtension + Native Messaging

## Hedef ortam

- Birincil: CachyOS + KDE Plasma 6 + Wayland
- Hyprland: kapsam dışı
- GNOME: henüz desteklenmiyor (mimari masaüstü katmanını ayırır)

## Bağımlılıklar (geliştirme)

- Qt 6.8+ (`qt6-base`, `qt6-declarative`, `qt6-multimedia`)
- CMake ≥ 3.21, Ninja (önerilir)
- Plasma entegrasyonu için: `kpackagetool6`, `kwriteconfig6`, KWin

## Derleme ve çalıştırma

```bash
cd ~/Masaüstü/Projeler/FloatingMedia
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/FloatingMediaApp
./build/FloatingMediaApp /yol/foto.jpg
./build/FloatingMediaApp https://example.com/video.mp4
```

## Kurulum / kaldırma

**Ana geliştirme bilgisayarında kalıcı kurulum yapmayın.** Testleri sanal makinede çalıştırın.

Kullanıcı dizini (sudo yok):

```bash
./install.sh -y                 # uygulama + (Plasma varsa) KWin
./install.sh -y --no-kde        # yalnızca uygulama
./install.sh -y --with-browser  # + Native Messaging host dosyaları
./uninstall.sh -y
./uninstall.sh -y --purge-browser
```

`--dry-run` ile değişiklik yapılmadan plan görülebilir.

Arch / CachyOS paketi:

```bash
./packaging/make-source-tarball.sh
cd packaging/arch && makepkg -s
# yalnızca VM: makepkg -i
```

Paketler: `floatingmedia` (uygulama), `floatingmedia-kwin` (isteğe bağlı).

Ayrıntılı VM kontrol listesi: [`tests/vm_install_checklist.txt`](tests/vm_install_checklist.txt)

## KDE Plasma entegrasyonu

Wayland’da Qt `WindowStaysOnTopHint` tek başına yeterli değildir. Bu yüzden uygulama, KWin çalışıyorsa her açılışta D-Bus üzerinden geçici bir betik yükleyip kendi pencerelerine `keepAbove` uygular ve kapanırken betiği kaldırır (kurulum gerektirmez; kapatmak için `FLOATINGMEDIA_NO_KWIN=1`).

Ayrıca kalıcı olarak kurulabilen KWin betiği `floatingmedia-keep-above`, `FloatingMediaApp` pencerelerine `keepAbove` uygular.

Kurulum betiği betiği yükler ve `kwinrc` içinde yalnızca `Plugins/floatingmedia-keep-aboveEnabled=true` yazar. Diğer KWin ayarlarına dokunmaz.

## Tarayıcı uzantısı

Dizin: [`browser-extension/`](browser-extension/)

1. `./install.sh -y --with-browser` (VM’de) — Native Messaging host JSON’ları yazar.
2. Firefox: `about:debugging` → geçici eklenti → `manifest.json`
3. Chromium: `chrome://extensions` → paketlenmemiş yükle → eklenti ID’sini alın:
   ```bash
   ./install.sh --skip-build --with-browser --chrome-extension-id=ID -y
   ```
4. Sağ tık menüsü veya popup ile URL gönderin.

Uygulama host modu: `FloatingMediaApp --native-messaging-host` (kurulumda sarmalayıcı kullanılır).

### Test durumu

- Native Messaging `ping` / URL doğrulama: derleme ortamında doğrulandı.
- Gerçek Firefox/Chromium eklenti yükleme ve uçtan uca gönderim: **bu ortamda tarayıcıya yazılmadığı için test edilmedi** (VM’de yapılmalı).
- Ana bilgisayara Native Messaging / KWin kurulumu yapılmadı.

## Klavye

| Tuş | İşlev |
|-----|--------|
| Space | Oynat / duraklat |
| ← / → | ±5 sn |
| ↑ / ↓ | Ses |
| M | Sessize al |
| O | Dosya aç |
| Esc | Kapat |

## Bilinen sınırlamalar

- DRM / geçici imzalı veya çerez gerektiren akışlar açılamayabilir.
- Uzantısız HTTP(S) adresler video olarak denenir; başarısız olursa hata gösterilir.
- Hyprland / GNOME keep-above entegrasyonu yok.
- `Failed to open VDPAU backend libvdpau_nvidia.so` yalnızca uyarıdır (NVIDIA donanım çözücüsü yok); yazılımsal çözmeye geçilir, oynatmayı engellemez.

## Lisans

Proje dosyalarında KWin betiği `GPL-3.0-or-later` olarak işaretlenmiştir. Kök dizinde ayrı bir `LICENSE` dosyası yoksa katkıdan önce lisans netleştirilmelidir; bu depoda yeni bir lisans dayatılmamıştır.

## Katkı

1. Küçük, test edilebilir değişiklikler yapın.
2. `cmake --build build && ctest --test-dir build` çalıştırın.
3. Ana makineye `install.sh` çalıştırmayın.
