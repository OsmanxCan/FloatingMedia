const HOST_NAME = "com.floatingmedia.host";

// kind: "image" | "video" | "page" | "" (otomatik)
function sendToNative(url, kind) {
  return new Promise((resolve) => {
    try {
      chrome.runtime.sendNativeMessage(
        HOST_NAME,
        { action: "open", url, kind: kind || "" },
        (response) => {
          const err = chrome.runtime.lastError;
          if (err) {
            resolve({ ok: false, error: err.message });
            return;
          }
          resolve(response || { ok: false, error: "Boş yanıt" });
        }
      );
    } catch (e) {
      resolve({ ok: false, error: String(e) });
    }
  });
}

function sendNativeRaw(message) {
  return new Promise((resolve) => {
    try {
      chrome.runtime.sendNativeMessage(HOST_NAME, message, (response) => {
        const err = chrome.runtime.lastError;
        resolve(err ? { ok: false, error: err.message } : (response || { ok: false }));
      });
    } catch (e) {
      resolve({ ok: false, error: String(e) });
    }
  });
}

// ---- Tarayıcının kendi "pencere içinde pencere" modu (her sitede çalışır) ----
// Sayfanın her çerçevesinde çalışır; en uygun videoyu PiP'e alır.
function pagePictureInPicture() {
  if (!("pictureInPictureEnabled" in document) || !document.pictureInPictureEnabled)
    return { ok: false, unsupported: true };
  if (document.pictureInPictureElement) {
    return document.exitPictureInPicture()
      .then(() => ({ ok: true, closed: true }))
      .catch((e) => ({ ok: false, error: String(e) }));
  }
  const videos = Array.from(document.querySelectorAll("video")).filter((v) => {
    const r = v.getBoundingClientRect();
    return v.readyState > 0 && r.width * r.height > 0;
  });
  if (videos.length === 0)
    return { ok: false, none: true };
  const score = (v) => {
    const r = v.getBoundingClientRect();
    return (v.paused ? 0 : 1e9) + r.width * r.height;
  };
  videos.sort((a, b) => score(b) - score(a));
  const v = videos[0];
  v.removeAttribute("disablepictureinpicture");
  try { v.disablePictureInPicture = false; } catch (e) {}
  return v.requestPictureInPicture()
    .then(() => ({ ok: true }))
    .catch((e) => ({ ok: false, error: String(e) }));
}

async function togglePictureInPicture(tabId) {
  let results;
  try {
    results = await chrome.scripting.executeScript({
      target: { tabId, allFrames: true },
      func: pagePictureInPicture
    });
  } catch (e) {
    return { ok: false, error: "Bu sayfada çalıştırılamıyor." };
  }
  const rs = (results || []).map((r) => r && r.result).filter(Boolean);
  const okResult = rs.find((r) => r.ok);
  if (okResult) {
    if (!okResult.closed)
      await sendNativeRaw({ action: "pip" }); // KWin: PiP penceresi her zaman üstte
    return okResult;
  }
  if (rs.length > 0 && rs.every((r) => r.unsupported))
    return { ok: false, error: "Bu tarayıcı bu özelliği eklentiye açmıyor. Firefox'ta videonun üzerindeki PiP düğmesini kullanın." };
  const withError = rs.find((r) => r.error);
  if (withError)
    return { ok: false, error: withError.error };
  return { ok: false, error: "Sayfada oynatılabilir video bulunamadı." };
}

chrome.commands.onCommand.addListener(async (command, tab) => {
  if (command !== "toggle-pip")
    return;
  let target = tab;
  if (!target) {
    const tabs = await chrome.tabs.query({ active: true, currentWindow: true });
    target = tabs[0];
  }
  if (target && target.id) {
    const r = await togglePictureInPicture(target.id);
    if (!r.ok)
      console.warn("FloatingMedia PiP:", r.error);
  }
});

function isStreamSrc(src) {
  return !src || /^(blob:|data:|mediasource:)/i.test(src);
}

chrome.runtime.onInstalled.addListener(() => {
  chrome.contextMenus.removeAll(() => {
    chrome.contextMenus.create({
      id: "fm-open-media",
      title: "FloatingMedia'da aç",
      contexts: ["image", "video", "audio"]
    });
    chrome.contextMenus.create({
      id: "fm-pip",
      title: "Videoyu yüzen pencereye al (her sitede)",
      contexts: ["video", "page"]
    });
    chrome.contextMenus.create({
      id: "fm-open-link",
      title: "Bağlantıyı FloatingMedia'da aç",
      contexts: ["link"]
    });
    chrome.contextMenus.create({
      id: "fm-open-page",
      title: "Bu sayfadaki videoyu FloatingMedia'da aç",
      contexts: ["page"]
    });
  });
});

chrome.contextMenus.onClicked.addListener(async (info, tab) => {
  if (info.menuItemId === "fm-pip") {
    if (tab && tab.id) {
      const r = await togglePictureInPicture(tab.id);
      if (!r.ok)
        console.warn("FloatingMedia PiP:", r.error);
    }
    return;
  }

  const pageUrl = info.frameUrl || info.pageUrl || (tab && tab.url);
  let url = null;
  let kind = "";

  if (info.menuItemId === "fm-open-media") {
    if (info.mediaType === "image") {
      url = info.srcUrl;
      kind = "image";
    } else if (isStreamSrc(info.srcUrl)) {
      // blob:/MSE videoları dışarıdan açılamaz → sayfayı yt-dlp çözsün
      url = pageUrl;
      kind = "page";
    } else {
      url = info.srcUrl;
      kind = "video";
    }
  } else if (info.menuItemId === "fm-open-link") {
    url = info.linkUrl;
    kind = "";
  } else {
    url = pageUrl;
    kind = "page";
  }

  if (!url)
    return;
  const result = await sendToNative(url, kind);
  if (!result.ok)
    console.warn("FloatingMedia:", result.error || "gönderilemedi");
});

chrome.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  if (message && message.type === "pipReady") {
    sendNativeRaw({ action: "pip" }).then(sendResponse);
    return true;
  }
  if (message && message.type === "openUrl" && message.url) {
    sendToNative(message.url, message.kind || "").then(sendResponse);
    return true;
  }
  return false;
});
