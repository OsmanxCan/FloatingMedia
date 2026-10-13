const HOST_NAME = "com.floatingmedia.host";

function sendToNative(url) {
  return new Promise((resolve) => {
    try {
      chrome.runtime.sendNativeMessage(
        HOST_NAME,
        { action: "open", url },
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

chrome.runtime.onInstalled.addListener(() => {
  chrome.contextMenus.removeAll(() => {
    chrome.contextMenus.create({
      id: "fm-open-link",
      title: "FloatingMedia'da aç",
      contexts: ["link", "image", "video", "audio"]
    });
    chrome.contextMenus.create({
      id: "fm-open-page",
      title: "Sayfa adresini FloatingMedia'da aç",
      contexts: ["page"]
    });
  });
});

chrome.contextMenus.onClicked.addListener(async (info) => {
  let url = info.srcUrl || info.linkUrl || info.pageUrl;
  if (!url)
    return;
  const result = await sendToNative(url);
  if (!result.ok) {
    console.warn("FloatingMedia:", result.error || "gönderilemedi");
  }
});

chrome.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  if (message && message.type === "openUrl" && message.url) {
    sendToNative(message.url).then(sendResponse);
    return true;
  }
  return false;
});
