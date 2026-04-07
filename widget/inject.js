/**
 * ClaudeMeter — fetch interceptor (runs in page main world).
 * Wraps window.fetch to capture /api/usage responses from claude.ai,
 * extracts utilization data, and posts it to the content script.
 */
(function () {
  if (window.__claudeMeterInjected) return;
  window.__claudeMeterInjected = true;

  const originalFetch = window.fetch;

  window.fetch = async function (...args) {
    const response = await originalFetch.apply(this, args);

    try {
      const url = typeof args[0] === "string" ? args[0] : args[0]?.url || "";
      if (url.includes("/api/usage")) {
        const clone = response.clone();
        clone.json().then((data) => {
          if (!data.five_hour) return;

          const sp = Math.round(data.five_hour.utilization || 0);
          const wp = Math.round(data.seven_day?.utilization || 0);

          // Session reset: delta from now to resets_at
          let sr = "";
          if (data.five_hour.resets_at) {
            const delta = new Date(data.five_hour.resets_at) - Date.now();
            if (delta > 0) {
              const mins = Math.floor(delta / 60000);
              const hours = Math.floor(mins / 60);
              const remMins = mins % 60;
              sr = hours > 0 ? `${hours}h ${remMins}m` : `${mins}m`;
            }
          }

          // Weekly reset: day-of-week + HH:MM local time
          let wr = "";
          if (data.seven_day?.resets_at) {
            const d = new Date(data.seven_day.resets_at);
            const days = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
            const hh = String(d.getHours()).padStart(2, "0");
            const mm = String(d.getMinutes()).padStart(2, "0");
            wr = `${days[d.getDay()]} ${hh}:${mm}`;
          }

          window.postMessage(
            { type: "CLAUDEMETER_DATA", sp, sr, wp, wr },
            "*"
          );
        }).catch(() => {});
      }
    } catch (_) {}

    return response;
  };
})();
