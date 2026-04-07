/**
 * ClaudeMeter — content script (isolated world).
 * Injects inject.js into the page main world and relays
 * intercepted usage data to the service worker.
 */

console.log("ClaudeMeter: content script loaded on", window.location.href);

// Inject the fetch interceptor into the page's main world
const script = document.createElement("script");
script.src = chrome.runtime.getURL("inject.js");
script.onload = () => {
  console.log("ClaudeMeter: inject.js loaded into page");
  script.remove();
};
script.onerror = (e) => console.error("ClaudeMeter: inject.js failed to load", e);
(document.head || document.documentElement).appendChild(script);

// Listen for messages from inject.js
window.addEventListener("message", (event) => {
  if (event.source !== window) return;
  if (event.data?.type !== "CLAUDEMETER_DATA") return;

  console.log("ClaudeMeter: relaying data to service worker", event.data);
  chrome.runtime.sendMessage({
    type: "USAGE_DATA",
    payload: event.data,
  });
});
