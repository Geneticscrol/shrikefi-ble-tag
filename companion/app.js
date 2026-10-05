/**
 * ShrikeFi BLE Tag — last-seen map companion
 *
 * Records the *phone's* GPS when the tag is nearby (Web Bluetooth + Geolocation).
 * Not Apple Find My. The tag has no onboard GPS.
 *
 * Primary target: Android Chrome (OnePlus). iOS Safari typically lacks Web Bluetooth.
 */
(() => {
  "use strict";

  const STORAGE_KEY = "shrikefi-lastseen-v1";
  const AUTO_PIN_MS = 8000;       // while connected / watching ads
  const MIN_MOVE_M = 8;           // skip near-duplicate pins
  const MAX_POINTS = 200;

  const el = {
    status: document.getElementById("statusChip"),
    banner: document.getElementById("compatBanner"),
    tagName: document.getElementById("tagName"),
    btnConnect: document.getElementById("btnConnect"),
    btnDisconnect: document.getElementById("btnDisconnect"),
    btnMark: document.getElementById("btnMark"),
    btnClear: document.getElementById("btnClear"),
    rssi: document.getElementById("rssiVal"),
    lastPin: document.getElementById("lastPin"),
    pointCount: document.getElementById("pointCount"),
  };

  /** @type {{lat:number,lng:number,rssi:number|null,timestamp:number,source:string}[]} */
  let points = loadPoints();
  /** @type {BluetoothDevice|null} */
  let device = null;
  /** @type {BluetoothRemoteGATTServer|null} */
  let gatt = null;
  let watchingAds = false;
  let autoTimer = null;
  let lastRssi = null;

  // ---- Map (Leaflet + OSM) -------------------------------------------------
  const map = L.map("map", { zoomControl: true }).setView([20.5937, 78.9629], 5);
  L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
    maxZoom: 19,
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OSM</a>',
  }).addTo(map);

  const trailLayer = L.layerGroup().addTo(map);
  /** @type {L.Marker|null} */
  let lastMarker = null;
  /** @type {L.Polyline|null} */
  let trailLine = null;

  const lastIcon = L.divIcon({
    className: "",
    html: '<div style="width:18px;height:18px;border-radius:50%;background:#ef4444;border:3px solid #fff;box-shadow:0 0 0 4px rgba(239,68,68,.35)"></div>',
    iconSize: [18, 18],
    iconAnchor: [9, 9],
  });
  const trailIcon = L.divIcon({
    className: "",
    html: '<div style="width:10px;height:10px;border-radius:50%;background:#38bdf8;border:2px solid #0f172a;opacity:.9"></div>',
    iconSize: [10, 10],
    iconAnchor: [5, 5],
  });

  function setStatus(state, text) {
    el.status.dataset.state = state;
    el.status.textContent = text;
  }

  function showBanner(html) {
    el.banner.hidden = false;
    el.banner.innerHTML = html;
  }

  function checkCompat() {
    const issues = [];
    if (!window.isSecureContext) {
      issues.push("This page is not a <strong>secure context</strong>. Web Bluetooth needs HTTPS or <code>http://localhost</code> (try <code>adb reverse</code> on OnePlus — see README).");
    }
    if (!navigator.bluetooth) {
      issues.push("<strong>Web Bluetooth</strong> is not available in this browser. Use <strong>Android Chrome</strong>. On iPhone, use LightBlue / nRF Connect for find-me instead.");
    }
    if (!navigator.geolocation) {
      issues.push("Geolocation is unavailable — cannot drop map pins.");
    }
    if (issues.length) showBanner(issues.join("<br>"));
  }

  function loadPoints() {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      if (!raw) return [];
      const parsed = JSON.parse(raw);
      return Array.isArray(parsed) ? parsed.slice(-MAX_POINTS) : [];
    } catch {
      return [];
    }
  }

  function savePoints() {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(points.slice(-MAX_POINTS)));
  }

  function haversineM(a, b) {
    const R = 6371000;
    const toRad = (d) => (d * Math.PI) / 180;
    const dLat = toRad(b.lat - a.lat);
    const dLng = toRad(b.lng - a.lng);
    const lat1 = toRad(a.lat);
    const lat2 = toRad(b.lat);
    const h =
      Math.sin(dLat / 2) ** 2 +
      Math.cos(lat1) * Math.cos(lat2) * Math.sin(dLng / 2) ** 2;
    return 2 * R * Math.asin(Math.sqrt(h));
  }

  function formatWhen(ts) {
    try {
      return new Date(ts).toLocaleString(undefined, {
        dateStyle: "medium",
        timeStyle: "medium",
      });
    } catch {
      return new Date(ts).toISOString();
    }
  }

  function renderMap(fit = false) {
    trailLayer.clearLayers();
    lastMarker = null;
    trailLine = null;

    el.pointCount.textContent = String(points.length);
    if (!points.length) {
      el.lastPin.textContent = "—";
      return;
    }

    const latlngs = points.map((p) => [p.lat, p.lng]);
    if (latlngs.length >= 2) {
      trailLine = L.polyline(latlngs, {
        color: "#38bdf8",
        weight: 3,
        opacity: 0.75,
      }).addTo(trailLayer);
    }

    points.forEach((p, i) => {
      const isLast = i === points.length - 1;
      if (!isLast) {
        L.marker([p.lat, p.lng], { icon: trailIcon, opacity: 0.85 })
          .bindPopup(
            `<strong>Sighting</strong><br>${formatWhen(p.timestamp)}<br>RSSI: ${
              p.rssi == null ? "n/a" : p.rssi + " dBm"
            }<br>${p.source || ""}`
          )
          .addTo(trailLayer);
      }
    });

    const last = points[points.length - 1];
    lastMarker = L.marker([last.lat, last.lng], { icon: lastIcon })
      .bindPopup(
        `<strong>Last seen</strong><br>${formatWhen(last.timestamp)}<br>RSSI: ${
          last.rssi == null ? "n/a" : last.rssi + " dBm"
        }<br>${last.source || ""}`
      )
      .addTo(trailLayer)
      .openPopup();

    el.lastPin.textContent = formatWhen(last.timestamp);
    el.rssi.textContent = lastRssi == null ? (last.rssi == null ? "—" : `${last.rssi} dBm`) : `${lastRssi} dBm`;

    if (fit) {
      const b = L.latLngBounds(latlngs);
      map.fitBounds(b.pad(0.35), { maxZoom: 17 });
    } else {
      map.setView([last.lat, last.lng], Math.max(map.getZoom(), 16));
    }
  }

  function getPosition() {
    return new Promise((resolve, reject) => {
      if (!navigator.geolocation) {
        reject(new Error("Geolocation unavailable"));
        return;
      }
      navigator.geolocation.getCurrentPosition(resolve, reject, {
        enableHighAccuracy: true,
        maximumAge: 5000,
        timeout: 15000,
      });
    });
  }

  /**
   * @param {{rssi?: number|null, source: string, force?: boolean}} opts
   */
  async function recordSighting(opts) {
    const { rssi = lastRssi, source, force = false } = opts;
    try {
      const pos = await getPosition();
      const lat = pos.coords.latitude;
      const lng = pos.coords.longitude;
      const point = {
        lat,
        lng,
        rssi: typeof rssi === "number" ? rssi : null,
        timestamp: Date.now(),
        source,
      };

      const prev = points[points.length - 1];
      if (
        !force &&
        prev &&
        haversineM(prev, point) < MIN_MOVE_M &&
        point.timestamp - prev.timestamp < AUTO_PIN_MS * 2
      ) {
        // Still update last RSSI display / bump timestamp lightly on last point
        prev.rssi = point.rssi ?? prev.rssi;
        prev.timestamp = point.timestamp;
        savePoints();
        renderMap(false);
        return prev;
      }

      points.push(point);
      if (points.length > MAX_POINTS) points = points.slice(-MAX_POINTS);
      savePoints();
      renderMap(false);
      return point;
    } catch (err) {
      console.warn("recordSighting failed", err);
      setStatus("error", "GPS denied / failed");
      throw err;
    }
  }

  function stopAuto() {
    if (autoTimer) {
      clearInterval(autoTimer);
      autoTimer = null;
    }
  }

  function startAuto() {
    stopAuto();
    autoTimer = setInterval(() => {
      if (!gatt || !gatt.connected) return;
      recordSighting({ source: "auto-connected", force: false }).catch(() => {});
    }, AUTO_PIN_MS);
  }

  async function tryWatchAdvertisements(dev) {
    if (typeof dev.watchAdvertisements !== "function") return false;
    try {
      await dev.watchAdvertisements();
      watchingAds = true;
      dev.addEventListener("advertisementreceived", onAdvertisement);
      return true;
    } catch (err) {
      console.info("watchAdvertisements unavailable", err);
      return false;
    }
  }

  function onAdvertisement(event) {
    if (typeof event.rssi === "number") {
      lastRssi = event.rssi;
      el.rssi.textContent = `${lastRssi} dBm`;
    }
    // Throttled pin on each advertisement burst while watching
    recordSighting({
      rssi: lastRssi,
      source: "advertisement",
      force: false,
    }).catch(() => {});
  }

  async function connect() {
    const name = (el.tagName.value || "ShrikeFi-Tag").trim();
    el.tagName.value = name;

    if (!navigator.bluetooth) {
      setStatus("error", "No Web Bluetooth");
      return;
    }

    setStatus("connecting", "Pick device…");
    el.btnConnect.disabled = true;

    try {
      device = await navigator.bluetooth.requestDevice({
        filters: [{ name }, { namePrefix: name.slice(0, Math.min(8, name.length)) }],
        optionalServices: [0x1802], // Immediate Alert (Find Me)
      });

      device.addEventListener("gattserverdisconnected", onDisconnected);

      setStatus("connecting", "Connecting…");
      gatt = await device.gatt.connect();

      // Optional: open Immediate Alert so the connection stays useful for Find Me
      try {
        await gatt.getPrimaryService(0x1802);
      } catch {
        /* service optional for mapping */
      }

      const adsOk = await tryWatchAdvertisements(device);
      startAuto();

      // First pin immediately
      await recordSighting({
        source: adsOk ? "connect+ads" : "connect",
        force: true,
      });

      setStatus("connected", adsOk ? "Connected · watching RSSI" : "Connected · auto pins");
      el.btnDisconnect.disabled = false;
      el.btnMark.disabled = false;
      el.btnConnect.disabled = false;
    } catch (err) {
      console.warn(err);
      const msg = err && err.name === "NotFoundError" ? "Cancelled" : "Connect failed";
      setStatus("error", msg);
      el.btnConnect.disabled = false;
      cleanupDevice(false);
    }
  }

  function onDisconnected() {
    setStatus("idle", "Disconnected");
    stopAuto();
    watchingAds = false;
    el.btnDisconnect.disabled = true;
    // Keep Mark enabled so user can still drop a manual pin if they want
    el.btnMark.disabled = false;
    gatt = null;
  }

  function cleanupDevice(forgetListener) {
    stopAuto();
    if (device) {
      try {
        if (watchingAds && typeof device.unwatchAdvertisements === "function") {
          device.unwatchAdvertisements();
        }
      } catch { /* ignore */ }
      device.removeEventListener("advertisementreceived", onAdvertisement);
      if (forgetListener) {
        device.removeEventListener("gattserverdisconnected", onDisconnected);
      }
      try {
        if (device.gatt && device.gatt.connected) device.gatt.disconnect();
      } catch { /* ignore */ }
    }
    watchingAds = false;
    device = null;
    gatt = null;
  }

  async function disconnect() {
    cleanupDevice(true);
    setStatus("idle", "Idle");
    el.btnDisconnect.disabled = true;
    el.btnConnect.disabled = false;
  }

  async function markNow() {
    el.btnMark.disabled = true;
    try {
      await recordSighting({ source: "manual", force: true });
      setStatus(gatt && gatt.connected ? "connected" : "idle", "Pin saved");
    } catch {
      /* status already set */
    } finally {
      el.btnMark.disabled = false;
    }
  }

  function clearTrail() {
    if (!points.length) return;
    if (!confirm("Clear all last-seen pins stored on this phone?")) return;
    points = [];
    savePoints();
    renderMap(true);
    el.rssi.textContent = "—";
    setStatus(gatt && gatt.connected ? "connected" : "idle", "Trail cleared");
  }

  // Wire UI
  el.btnConnect.addEventListener("click", () => connect());
  el.btnDisconnect.addEventListener("click", () => disconnect());
  el.btnMark.addEventListener("click", () => markNow());
  el.btnClear.addEventListener("click", () => clearTrail());

  // Allow Mark even before connect (user knows they are near the tag)
  el.btnMark.disabled = false;

  checkCompat();
  renderMap(true);

  // Service worker for offline shell (best-effort)
  if ("serviceWorker" in navigator) {
    navigator.serviceWorker.register("./sw.js").catch(() => {});
  }
})();
