const statusEl = document.getElementById("status");
const urlEl = document.getElementById("url");
const listEl = document.getElementById("list");

let mediaItems = [];
let currentFilter = "all";

function setStatus(text, ok) {
  statusEl.textContent = text;
  statusEl.className = ok ? "ok" : "err";
}

function openUrl(url, kind) {
  setStatus("Gönderiliyor…", true);
  chrome.runtime.sendMessage({ type: "openUrl", url, kind: kind || "" }, (response) => {
    const err = chrome.runtime.lastError;
    if (err) {
      setStatus(err.message, false);
      return;
    }
    if (response && response.ok)
      setStatus("FloatingMedia'ya gönderildi.", true);
    else
      setStatus((response && response.error) || "Gönderilemedi.", false);
  });
}

// ---- Sayfada çalışan tarayıcı (her çerçevede ayrı çalışır) ----
function collectPageMedia() {
  const out = [];
  const seen = new Set();
  const abs = (u) => {
    try { return new URL(u, location.href).href; } catch (e) { return null; }
  };
  const add = (url, kind, thumb, w, h) => {
    if (!url || !/^https?:/i.test(url))
      return;
    const key = kind + "|" + url;
    if (seen.has(key))
      return;
    seen.add(key);
    out.push({ url, kind, thumb: thumb || null, w: Math.round(w || 0), h: Math.round(h || 0) });
  };
  const isStream = (src) => !src || /^(blob:|data:|mediasource:)/i.test(src);

  // Videolar
  document.querySelectorAll("video").forEach((v) => {
    let src = v.currentSrc || v.src;
    if (!src) {
      const s = v.querySelector("source[src]");
      if (s) src = s.src;
    }
    const r = v.getBoundingClientRect();
    if (r.width < 40 && r.height < 40 && !v.videoWidth)
      return; // gizli/izleme amaçlı küçük videolar
    let thumb = v.poster ? abs(v.poster) : null;
    if (!thumb && v.readyState >= 2) {
      try {
        const c = document.createElement("canvas");
        c.width = 160;
        c.height = 90;
        c.getContext("2d").drawImage(v, 0, 0, 160, 90);
        thumb = c.toDataURL("image/jpeg", 0.6);
      } catch (e) { /* farklı kaynak: önizleme alınamaz */ }
    }
    const w = v.videoWidth || r.width;
    const h = v.videoHeight || r.height;
    if (isStream(src))
      add(location.href, "page", thumb, w, h);   // yt-dlp çözer
    else
      add(abs(src), "video", thumb, w, h);
  });

  // Sayfa meta etiketleri
  document.querySelectorAll(
    'meta[property="og:video"], meta[property="og:video:url"], meta[property="og:video:secure_url"]'
  ).forEach((m) => {
    const u = abs(m.content);
    if (u && /\.(mp4|webm|m3u8|mov|m4v|ogv)(\?|$)/i.test(u))
      add(u, "video", null, 0, 0);
  });

  // Fotoğraflar
  document.querySelectorAll("img").forEach((img) => {
    const src = img.currentSrc || img.src;
    if (!src || /^data:/i.test(src))
      return;
    if (img.naturalWidth < 150 || img.naturalHeight < 150)
      return; // simge/küçük resim
    add(abs(src), "image", abs(src), img.naturalWidth, img.naturalHeight);
  });
  const og = document.querySelector('meta[property="og:image"]');
  if (og && og.content)
    add(abs(og.content), "image", abs(og.content), 0, 0);

  // Medya dosyalarına giden bağlantılar
  document.querySelectorAll("a[href]").forEach((a) => {
    const u = abs(a.getAttribute("href"));
    if (!u)
      return;
    if (/\.(mp4|webm|mkv|mov|m4v|ogv|m3u8)(\?|$)/i.test(u))
      add(u, "video", null, 0, 0);
    else if (/\.(jpe?g|png|webp|gif|avif)(\?|$)/i.test(u))
      add(u, "image", u, 0, 0);
  });

  return out.slice(0, 100);
}

function labelFor(item) {
  if (item.kind === "image")
    return "Fotoğraf";
  if (item.kind === "page")
    return "Sayfa videosu";
  return "Video";
}

function shortUrl(u) {
  try {
    const x = new URL(u);
    const path = x.pathname.length > 40 ? x.pathname.slice(0, 40) + "…" : x.pathname;
    return x.hostname + path;
  } catch (e) {
    return u;
  }
}

function render() {
  const items = mediaItems.filter((it) =>
    currentFilter === "all"
      || (currentFilter === "image" ? it.kind === "image" : it.kind !== "image"));

  listEl.textContent = "";
  if (items.length === 0) {
    const d = document.createElement("div");
    d.className = "empty";
    d.textContent = mediaItems.length === 0
      ? "Bu sayfada video veya fotoğraf bulunamadı."
      : "Bu filtrede öğe yok.";
    listEl.appendChild(d);
    return;
  }

  for (const it of items) {
    const btn = document.createElement("button");
    btn.className = "item";
    btn.title = it.url;

    const th = document.createElement("div");
    th.className = "thumb";
    if (it.thumb)
      th.style.backgroundImage = `url("${it.thumb.replace(/"/g, "%22")}")`;
    else
      th.textContent = it.kind === "image" ? "🖼" : "▶";

    const meta = document.createElement("div");
    meta.className = "meta";
    const title = document.createElement("div");
    title.className = "title";
    title.textContent = labelFor(it) + (it.w && it.h ? ` · ${it.w}×${it.h}` : "");
    const url = document.createElement("div");
    url.className = "url";
    url.textContent = shortUrl(it.url);
    meta.append(title, url);

    btn.append(th, meta);
    btn.addEventListener("click", () => openUrl(it.url, it.kind));
    listEl.appendChild(btn);
  }
}

async function scanActiveTab() {
  try {
    const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
    if (!tab || !tab.id || !/^https?:/i.test(tab.url || "")) {
      mediaItems = [];
      listEl.innerHTML = '<div class="empty">Bu sayfa taranamıyor.</div>';
      return;
    }
    const results = await chrome.scripting.executeScript({
      target: { tabId: tab.id, allFrames: true },
      func: collectPageMedia
    });
    const seen = new Set();
    mediaItems = [];
    for (const r of results || []) {
      for (const it of (r && r.result) || []) {
        const key = it.kind + "|" + it.url;
        if (seen.has(key))
          continue;
        seen.add(key);
        mediaItems.push(it);
      }
    }
    // Videolar önce, sonra büyük fotoğraflar
    mediaItems.sort((a, b) => {
      const av = a.kind === "image" ? 1 : 0;
      const bv = b.kind === "image" ? 1 : 0;
      if (av !== bv)
        return av - bv;
      return (b.w * b.h) - (a.w * a.h);
    });
    render();
  } catch (e) {
    mediaItems = [];
    listEl.innerHTML = '<div class="empty">Bu sayfa taranamıyor.</div>';
    console.warn("FloatingMedia tarama:", e);
  }
}

document.querySelectorAll(".tabs button").forEach((b) => {
  b.addEventListener("click", () => {
    document.querySelectorAll(".tabs button").forEach((x) => x.classList.remove("active"));
    b.classList.add("active");
    currentFilter = b.dataset.filter;
    render();
  });
});

document.getElementById("send").addEventListener("click", () => {
  const url = urlEl.value.trim();
  if (!url) {
    setStatus("Adres girin.", false);
    return;
  }
  openUrl(url, "");
});

urlEl.addEventListener("keydown", (e) => {
  if (e.key === "Enter")
    document.getElementById("send").click();
});

document.getElementById("page").addEventListener("click", async () => {
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  if (!tab || !tab.url) {
    setStatus("Sekme adresi alınamadı.", false);
    return;
  }
  openUrl(tab.url, "page");
});

scanActiveTab();
