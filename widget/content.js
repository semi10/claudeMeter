/**
 * ClaudeMeter — content script (isolated world).
 * Injects inject.js into the page main world and relays
 * intercepted usage data to the service worker.
 */

// Inject the fetch interceptor into the page's main world
const script = document.createElement("script");
script.src = chrome.runtime.getURL("inject.js");
script.onload = () => script.remove();
(document.head || document.documentElement).appendChild(script);

// Listen for messages from inject.js
window.addEventListener("message", (event) => {
  if (event.source !== window) return;
  if (event.data?.type !== "CLAUDEMETER_DATA") return;

  chrome.runtime.sendMessage({
    type: "USAGE_DATA",
    payload: event.data,
  });
});
