/**
 * ClaudeMeter — service worker.
 * Manages native messaging connection to the Python host,
 * periodic polling of the usage page, and keep-alive alarms.
 */

let port = null;

function connectNative() {
  if (port) return;
  try {
    port = chrome.runtime.connectNative("com.example.claudemeter");
    port.onDisconnect.addListener(() => {
      console.log("ClaudeMeter: native host disconnected", chrome.runtime.lastError?.message);
      port = null;
    });
    console.log("ClaudeMeter: connected to native host");
  } catch (e) {
    console.error("ClaudeMeter: failed to connect", e);
    port = null;
  }
}

// Receive usage data from content script
chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
  if (msg.type !== "USAGE_DATA") return;

  if (!port) connectNative();
  if (port) {
    port.postMessage(msg.payload);
  }
});

// Alarms: keepalive (reconnect) + poll (ensure usage page is hit)
chrome.alarms.onAlarm.addListener((alarm) => {
  if (alarm.name === "keepalive") {
    if (!port) connectNative();
  }

  if (alarm.name === "poll") {
    // Find or open a claude.ai tab to trigger the fetch interceptor
    chrome.tabs.query({ url: "https://claude.ai/*" }, (tabs) => {
      if (tabs.length > 0) {
        // Re-inject into first matching tab to trigger a fresh usage fetch
        chrome.scripting.executeScript({
          target: { tabId: tabs[0].id },
          world: "MAIN",
          func: () => {
            fetch("/api/usage").catch(() => {});
          },
        });
      }
    });
  }
});

function createAlarms() {
  chrome.alarms.create("keepalive", { periodInMinutes: 1 });
  chrome.alarms.create("poll", { periodInMinutes: 1 });
}

chrome.runtime.onInstalled.addListener(createAlarms);
chrome.runtime.onStartup.addListener(createAlarms);

// Initial connection
connectNative();
