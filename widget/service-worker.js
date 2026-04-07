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
  console.log("ClaudeMeter: onMessage received", msg);
  if (msg.type !== "USAGE_DATA") return;

  console.log("ClaudeMeter: USAGE_DATA payload", msg.payload);
  if (!port) connectNative();
  if (port) {
    port.postMessage(msg.payload);
    console.log("ClaudeMeter: sent to native host");
  }
});

// Poll: find a claude.ai tab and trigger /api/usage fetch
function pollUsage() {
  console.log("ClaudeMeter: polling for claude.ai tab...");
  chrome.tabs.query({ url: "https://claude.ai/*", status: "complete" }, (tabs) => {
    console.log("ClaudeMeter: found tabs", tabs?.length);
    const tab = tabs?.find((t) => t.url && t.url.startsWith("https://claude.ai"));
    if (!tab) {
      console.log("ClaudeMeter: no claude.ai tab found, opening background tab");
      chrome.tabs.create({
        url: "https://claude.ai/settings/usage",
        active: false,
      });
      return;
    }
    console.log("ClaudeMeter: reloading tab", tab.id, tab.url);
    chrome.tabs.reload(tab.id).catch((e) =>
      console.warn("ClaudeMeter: tab reload failed", e.message)
    );
  });
}

// Alarms: keepalive (reconnect) + poll (ensure usage page is hit)
chrome.alarms.onAlarm.addListener((alarm) => {
  console.log("ClaudeMeter: alarm fired", alarm.name);
  if (alarm.name === "keepalive") {
    if (!port) connectNative();
  }
  if (alarm.name === "poll") {
    pollUsage();
  }
});

function createAlarms() {
  chrome.alarms.create("keepalive", { periodInMinutes: 1 });
  chrome.alarms.create("poll", { periodInMinutes: 1 });
}

chrome.runtime.onInstalled.addListener(() => {
  console.log("ClaudeMeter: onInstalled");
  createAlarms();
  setTimeout(pollUsage, 3000); // Poll immediately after install
});
chrome.runtime.onStartup.addListener(() => {
  console.log("ClaudeMeter: onStartup");
  createAlarms();
  setTimeout(pollUsage, 3000);
});

// Initial connection + immediate poll
connectNative();
setTimeout(pollUsage, 5000);
