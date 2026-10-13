# FloatingMedia — Görev listesi

## Aşama 1 — Sağlamlaştırma
- [x] WM sınıfı / desktopFileName hizalama
- [x] CLI ile medya açma
- [x] URL/dosya doğrulama (`MediaUrl.h`)
- [x] Derleme doğrulama

## Aşama 2 — Kayan medya penceresi
- [x] Ses seviyesi kontrolü
- [x] Küçük pencerede kullanılabilir kontroller
- [x] Medya durumu gösterimi

## Aşama 3 — KDE Plasma
- [x] KWin betiği gözden geçirme (resourceName / desktopFileName)
- [x] Kurulumda KDE algılama / --no-kde

## Aşama 4 — Kurulum / kaldırma
- [x] Doğrulama adımları
- [x] Native messaging kurulum yolları (--with-browser; host’a yazılmadan dry-run)
- [x] uninstall --purge-browser

## Aşama 5 — Tarayıcı entegrasyonu
- [x] WebExtension iskeleti
- [x] Native Messaging host (`--native-messaging-host`)
- [x] Uygulama tarafı URL açma / tek örnek IPC
- [x] Manifest / host JSON şablonları
- [ ] Uçtan uca tarayıcı testi (VM; ana makinede yapılmadı)

## Aşama 6 — Testler
- [x] Betik dry-run testleri
- [x] URL doğrulama testleri
- [x] Derleme smoke + native ping

## Aşama 7 — Dokümantasyon
- [x] README.md
- [x] Bilinen sınırlamalar / VM test prosedürü
