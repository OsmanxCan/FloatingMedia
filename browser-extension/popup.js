const statusEl = document.getElementById("status");
const urlEl = document.getElementById("url");

function setStatus(text, ok) {
  statusEl.textContent = text;
  statusEl.className = ok ? "ok" : "err";
}

function openUrl(url) {
  chrome.runtime.sendMessage({ type: "openUrl", url }, (response) => {
    const err = chrome.runtime.lastError;
    if (err) {
      setStatus(err.message, false);
      return;
    }
    if (response && response.ok) {
      setStatus("FloatingMedia'ya gönderildi.", true);
    } else {
      setStatus((response && response.error) || "Gönderilemedi.", false);
    }
  });
}

document.getElementById("send").addEventListener("click", () => {
  const url = urlEl.value.trim();
  if (!url) {
    setStatus("URL girin.", false);
    return;
  }
  openUrl(url);
});

document.getElementById("page").addEventListener("click", () => {
  chrome.tabs.query({ active: true, currentWindow: true }, (tabs) => {
    const tab = tabs && tabs[0];
    if (!tab || !tab.url) {
      setStatus("Sekme adresi alınamadı.", false);
      return;
    }
    urlEl.value = tab.url;
    openUrl(tab.url);
  });
});
