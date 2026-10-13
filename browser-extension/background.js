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
  if (message && message.type === "openUrl" && message.url) {
    sendToNative(message.url, message.kind || "").then(sendResponse);
    return true;
  }
  return false;
});
