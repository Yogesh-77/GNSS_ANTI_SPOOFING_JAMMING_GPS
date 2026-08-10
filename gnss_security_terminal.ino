#include <WiFi.h>
#include <WebServer.h>
#include <TinyGPSPlus.h>
#include <ArduinoJson.h>
#include <math.h>
#include "gnss_model.h"

/* ================= WIFI ================= */
const char *ssid = "Airtel_YOGI";
const char *password = "YSM123456789";

/* ================= OBJECTS ================= */
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);
WebServer server(80);
Eloquent::ML::Port::RandomForest model;

/* ================= STATE ================= */
// Position state
double currentLat = 0;
double currentLon = 0;
double lastLat = 0;
double lastLon = 0;
double displayLat = 0;
double displayLon = 0;
double predictedLat = 0;
double predictedLon = 0;

// Telemetry + engineered features
float speedKph = 0;
float satellites = 0;
float hdopValue = 0;
float driftMeters = 0;
float trustScore = 1.0;
float antiSpoofRisk = 0.0;
float zAnomalyScore = 0.0;
float satDropRate = 0.0;
float speedJump = 0.0;
float hdopSpike = 0.0;

float lastSpeedKph = 0.0;
float lastSatellites = 0.0;
float lastHdopValue = 0.0;

// EWMA baselines for z-score model
float driftMean = 8.0;
float driftVar = 12.0;
float hdopMean = 1.2;
float hdopVar = 0.6;
float satMean = 9.0;
float satVar = 2.0;

unsigned long lastFixMs = 0;

// Runtime flags
bool safeMode = false;
bool spoofSim = false;
bool jamSim = false;
bool stabilityGateBlocked = false;

// Simple 2-state Kalman-like tracker (lat/lon + velocity)
double kfLat = 0;
double kfLon = 0;
double kfVLat = 0;
double kfVLon = 0;
bool kalmanInitialized = false;

String aiStatus = "INITIALIZING";
String spoofReason = "CALIBRATING";

int spoofCounter = 0;
int heatBurnCounter = 0;
String lastLoggedAttack = "";
unsigned long lastAttackLogMs = 0;

struct AttackLog {
  unsigned long sec;
  double lat;
  double lon;
  float risk;
  String attack;
};

const int MAX_ATTACK_LOGS = 20;
AttackLog attackLogs[MAX_ATTACK_LOGS];
int attackLogCount = 0;
int attackLogStart = 0;

/* ================= DASHBOARD HTML ================= */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<meta name="viewport" content="width=device-width, initial-scale=1.0" />
<title>AirWatch Sentinel v3.0</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap" rel="stylesheet">
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"/>
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
<script src="https://unpkg.com/leaflet.heat@0.2.0/dist/leaflet-heat.js"></script>
<style>
:root {
  --panel: #ffffff;
  --border: #e2e8f0;
  --text: #0f172a;
  --muted: #475569;
  --good: #16a34a;
  --warn: #d97706;
  --bad: #dc2626;
  --neutral: #64748b;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  color: var(--text);
  font-family: "Inter", Arial, sans-serif;
  background: linear-gradient(180deg, #ffffff 0%, #eef2ff 35%, #e2e8f0 100%);
}
.container { width: min(1240px, 95vw); margin: 22px auto; display: grid; gap: 14px; }
.topbar { display: flex; justify-content: space-between; align-items: center; gap: 16px; flex-wrap: wrap; }
.brand h1 { margin: 0; font-size: clamp(1.35rem, 2.3vw, 2rem); font-weight: 800; }
.brand p { margin: 6px 0 0; color: var(--muted); }
.badge { border: 1px solid var(--border); background: #fff; color: #1e293b; padding: 10px 14px; border-radius: 10px; font-size: .85rem; font-weight: 600; }
.grid { display: grid; grid-template-columns: repeat(12, 1fr); gap: 12px; }
.card { background: var(--panel); border: 1px solid var(--border); border-radius: 14px; padding: 14px; box-shadow: 0 10px 24px rgba(15, 23, 42, 0.08); }
.status-card { grid-column: span 12; }
.metrics, .controls, .location { grid-column: span 4; }
.map-card { grid-column: span 8; padding: 0; overflow: hidden; }
.security-card { grid-column: span 4; }
.log-card { grid-column: span 12; }
.section-title { font-size: .8rem; text-transform: uppercase; letter-spacing: .08em; color: var(--muted); margin-bottom: 8px; font-weight: 700; }
.status-pill { display: inline-flex; align-items: center; gap: 10px; font-weight: 700; border-radius: 999px; padding: 9px 14px; border: 1px solid var(--border); background: #f8fafc; }
.dot { width: 10px; height: 10px; border-radius: 50%; background: var(--neutral); }
.metric-list { margin: 0; padding: 0; list-style: none; display: grid; gap: 9px; }
.metric-row { display: flex; justify-content: space-between; align-items: center; padding-bottom: 6px; border-bottom: 1px dashed #cbd5e1; }
.metric-row:last-child { border-bottom: none; padding-bottom: 0; }
.metric-label { color: var(--muted); }
.metric-value { font-weight: 700; font-variant-numeric: tabular-nums; }
#map { width: 100%; height: 500px; background: #f1f5f9; }
button { width: 100%; margin-bottom: 10px; border: 0; border-radius: 10px; padding: 11px 14px; font-size: .95rem; font-weight: 700; color: white; cursor: pointer; }
.btn-spoof { background: linear-gradient(90deg, #dc2626, #ef4444); }
.btn-jam { background: linear-gradient(90deg, #d97706, #f59e0b); }
.btn-reset { background: linear-gradient(90deg, #0284c7, #38bdf8); }
.helper { color: var(--muted); font-size: .82rem; line-height: 1.45; }
.progress { width: 100%; height: 12px; border-radius: 999px; background: #e2e8f0; overflow: hidden; margin-top: 8px; }
.progress > span { display: block; height: 100%; width: 0%; transition: width .2s ease; background: linear-gradient(90deg, #16a34a, #f59e0b, #dc2626); }
.reason { margin-top: 10px; border: 1px solid var(--border); background: #f8fafc; border-radius: 10px; padding: 10px; color: #334155; font-size: .86rem; line-height: 1.4; }
.kicker { margin-top: 10px; font-size: .78rem; color: #475569; }
.log-table-wrap { max-height: 220px; overflow: auto; border: 1px solid var(--border); border-radius: 10px; }
table { width: 100%; border-collapse: collapse; font-size: .82rem; }
th, td { padding: 8px; text-align: left; border-bottom: 1px solid #e2e8f0; }
th { position: sticky; top: 0; background: #f8fafc; z-index: 1; }
@media (max-width: 1060px) { .metrics, .controls, .location, .map-card, .security-card { grid-column: span 12; } }
</style>
</head>
<body>
  <div class="container">
    <div class="topbar">
      <div class="brand">
        <h1>AirWatch Sentinel v3.0</h1>
        <p>Dual-AI GNSS defense • White detailed map • Stability Gate and Attack Heatmap</p>
      </div>
      <div class="badge" id="uptime">Waiting for telemetry...</div>
    </div>

    <div class="grid">
      <div class="card status-card">
        <div class="section-title">System Status</div>
        <div class="status-pill" id="statusPill">
          <span class="dot" id="statusDot"></span>
          <span id="statusBox">INITIALIZING</span>
        </div>
      </div>

      <div class="card metrics">
        <div class="section-title">Signal Metrics</div>
        <ul class="metric-list">
          <li class="metric-row"><span class="metric-label">Satellites</span><span class="metric-value" id="sat">0</span></li>
          <li class="metric-row"><span class="metric-label">HDOP</span><span class="metric-value" id="hdop">0.00</span></li>
          <li class="metric-row"><span class="metric-label">Drift</span><span class="metric-value" id="drift">0.0 m</span></li>
          <li class="metric-row"><span class="metric-label">Trust Score</span><span class="metric-value" id="trust">0%</span></li>
          <li class="metric-row"><span class="metric-label">Speed</span><span class="metric-value" id="speed">0.0 km/h</span></li>
          <li class="metric-row"><span class="metric-label">Z-Anomaly</span><span class="metric-value" id="zscore">0.00</span></li>
          <li class="metric-row"><span class="metric-label">Satellite Drop</span><span class="metric-value" id="satdrop">0.00</span></li>
          <li class="metric-row"><span class="metric-label">Speed Jump</span><span class="metric-value" id="speedjump">0.0 km/h</span></li>
          <li class="metric-row"><span class="metric-label">HDOP Spike</span><span class="metric-value" id="hdopspike">0.00</span></li>
        </ul>
      </div>

      <div class="card controls">
        <div class="section-title">Simulation Controls</div>
        <button class="btn-spoof" onclick="fetch('/spoof')">Simulate Spoof Attack</button>
        <button class="btn-jam" onclick="fetch('/jam')">Simulate Jamming Event</button>
        <button class="btn-reset" onclick="fetch('/reset')">Deep Purge + Resync</button>
        <div class="helper">Deep purge resets detection memory, feature baselines, heat counters, and gate state.</div>
      </div>

      <div class="card location">
        <div class="section-title">Position & Gate</div>
        <ul class="metric-list">
          <li class="metric-row"><span class="metric-label">Latitude</span><span class="metric-value" id="lat">0.000000</span></li>
          <li class="metric-row"><span class="metric-label">Longitude</span><span class="metric-value" id="lon">0.000000</span></li>
          <li class="metric-row"><span class="metric-label">Safe Mode</span><span class="metric-value" id="safe">OFF</span></li>
          <li class="metric-row"><span class="metric-label">Stability Gate</span><span class="metric-value" id="gate">OPEN</span></li>
          <li class="metric-row"><span class="metric-label">Heat Burns</span><span class="metric-value" id="burns">0</span></li>
        </ul>
      </div>

      <div class="card map-card"><div id="map"></div></div>

      <div class="card security-card">
        <div class="section-title">Anti-Spoofing Intelligence</div>
        <div style="display:flex;justify-content:space-between;align-items:center;"><span class="metric-label">Fusion Risk</span><span class="metric-value" id="risk">0%</span></div>
        <div class="progress"><span id="riskBar"></span></div>
        <div class="reason" id="reason">Analyzing GNSS behavior...</div>
        <div class="kicker">Heatmap burns only when risk > 55% to preserve forensic hotspots.</div>
      </div>

      <div class="card log-card">
        <div class="section-title">Attack Forensic Log</div>
        <div class="log-table-wrap">
          <table>
            <thead>
              <tr><th>Time(s)</th><th>Type</th><th>Risk</th><th>Latitude</th><th>Longitude</th></tr>
            </thead>
            <tbody id="logRows">
              <tr><td colspan="5">No attack events yet.</td></tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>
  </div>

<script>
const map = L.map('map', { zoomControl: true }).setView([0, 0], 2);
const baseStreet = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', { maxZoom: 20, attribution: '&copy; OpenStreetMap contributors' });
const baseDetailedWhite = L.tileLayer('https://{s}.basemaps.cartocdn.com/light_all/{z}/{x}/{y}{r}.png', { maxZoom: 20, attribution: '&copy; OpenStreetMap & CARTO' });
const satellite = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', { maxZoom: 20, attribution: 'Tiles &copy; Esri' });
baseDetailedWhite.addTo(map);
L.control.layers({ 'Detailed White': baseDetailedWhite, 'Street': baseStreet, 'Satellite': satellite }, {}, { position: 'topright' }).addTo(map);

const marker = L.marker([0, 0]).addTo(map);
const trail = L.polyline([], { color: '#0284c7', weight: 3, opacity: 0.8 }).addTo(map);
let heatPoints = [];
const heat = L.heatLayer(heatPoints, {
  radius: 23,
  blur: 20,
  maxZoom: 18,
  gradient: {0.2: '#38bdf8', 0.45: '#f59e0b', 0.75: '#dc2626'}
}).addTo(map);

let tick = 0;
let lastBurnAt = 0;

function applyStatusTheme(status) {
  const pill = document.getElementById('statusPill');
  const dot = document.getElementById('statusDot');
  if (status.includes('NORMAL')) {
    pill.style.background = 'rgba(22,163,74,0.11)';
    pill.style.borderColor = 'rgba(22,163,74,0.45)';
    dot.style.background = 'var(--good)';
  } else if (status.includes('JAM')) {
    pill.style.background = 'rgba(217,119,6,0.12)';
    pill.style.borderColor = 'rgba(217,119,6,0.45)';
    dot.style.background = 'var(--warn)';
  } else if (status.includes('SPOOF')) {
    pill.style.background = 'rgba(220,38,38,0.12)';
    pill.style.borderColor = 'rgba(220,38,38,0.45)';
    dot.style.background = 'var(--bad)';
  } else {
    pill.style.background = '#f8fafc';
    pill.style.borderColor = 'var(--border)';
    dot.style.background = 'var(--neutral)';
  }
}

function burnHeatPoint(lat, lon, risk, gateBlocked) {
  const now = Date.now();
  if (risk < 0.55) return;
  if (!gateBlocked && now - lastBurnAt < 2500) return;

  heatPoints.push([lat, lon, Math.max(0.3, risk)]);
  if (heatPoints.length > 120) heatPoints.shift();
  heat.setLatLngs(heatPoints);
  lastBurnAt = now;
}

function renderLogs(items) {
  const body = document.getElementById('logRows');
  if (!items || !items.length) {
    body.innerHTML = '<tr><td colspan="5">No attack events yet.</td></tr>';
    return;
  }

  body.innerHTML = items.map(item => `
    <tr>
      <td>${Number(item.time || 0).toFixed(0)}</td>
      <td>${item.attack || 'Unknown'}</td>
      <td>${(Number(item.risk || 0) * 100).toFixed(0)}%</td>
      <td>${Number(item.lat || 0).toFixed(6)}</td>
      <td>${Number(item.lon || 0).toFixed(6)}</td>
    </tr>
  `).join('');
}

async function update() {
  try {
    const res = await fetch('/status');
    const d = await res.json();

    const lat = Number(d.lat || 0);
    const lon = Number(d.lon || 0);
    const risk = Math.max(0, Math.min(1, Number(d.risk || 0)));

    document.getElementById('sat').innerText = Number(d.sat || 0).toFixed(0);
    document.getElementById('hdop').innerText = Number(d.hdop || 0).toFixed(2);
    document.getElementById('drift').innerText = `${Number(d.drift || 0).toFixed(1)} m`;
    document.getElementById('trust').innerText = `${(Number(d.trust || 0) * 100).toFixed(0)}%`;
    document.getElementById('speed').innerText = `${Number(d.speed || 0).toFixed(1)} km/h`;
    document.getElementById('zscore').innerText = Number(d.zscore || 0).toFixed(2);
    document.getElementById('satdrop').innerText = Number(d.satdrop || 0).toFixed(2);
    document.getElementById('speedjump').innerText = `${Number(d.speedjump || 0).toFixed(1)} km/h`;
    document.getElementById('hdopspike').innerText = Number(d.hdopspike || 0).toFixed(2);

    document.getElementById('lat').innerText = lat.toFixed(6);
    document.getElementById('lon').innerText = lon.toFixed(6);
    document.getElementById('safe').innerText = d.safe ? 'ON' : 'OFF';
    document.getElementById('gate').innerText = d.gate ? 'BLOCKED' : 'OPEN';
    document.getElementById('burns').innerText = Number(d.burns || 0);

    const status = d.status || 'INITIALIZING';
    document.getElementById('statusBox').innerText = status;
    applyStatusTheme(status);

    document.getElementById('risk').innerText = `${(risk * 100).toFixed(0)}%`;
    document.getElementById('riskBar').style.width = `${(risk * 100).toFixed(0)}%`;
    document.getElementById('reason').innerText = d.reason || 'Analyzing GNSS behavior...';

    marker.setLatLng([lat, lon]);
    trail.addLatLng([lat, lon]);
    burnHeatPoint(lat, lon, risk, d.gate);

    if (tick % 2 === 0) {
      const logRes = await fetch('/logs');
      const logData = await logRes.json();
      renderLogs(logData.logs || []);
    }

    if (tick % 3 === 0) map.setView([lat, lon], 17);
    tick++;
    document.getElementById('uptime').innerText = `Live stream • update #${tick}`;
  } catch (_) {
    document.getElementById('uptime').innerText = 'Telemetry unavailable, retrying...';
  }
}

update();
setInterval(update, 1000);
</script>
</body>
</html>
)rawliteral";

/* ================= HELPERS ================= */
float calculateDrift(double lat1, double lon1, double lat2, double lon2) {
  if (lat1 == 0) return 0;

  const double dLat = radians(lat2 - lat1);
  const double dLon = radians(lon2 - lon1);

  const double a = sin(dLat / 2) * sin(dLat / 2) +
                   cos(radians(lat1)) * cos(radians(lat2)) *
                   sin(dLon / 2) * sin(dLon / 2);

  const double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return 6371000 * c;
}

void updateEWMA(float value, float alpha, float &mean, float &variance) {
  const float delta = value - mean;
  mean += alpha * delta;
  variance = (1.0f - alpha) * (variance + alpha * delta * delta);
  if (variance < 0.0001f) variance = 0.0001f;
}

float zScore(float value, float mean, float variance) {
  float stddev = sqrt(variance);
  if (stddev < 0.01f) stddev = 0.01f;
  return fabs(value - mean) / stddev;
}

void kalmanPredict(float dt) {
  kfLat += kfVLat * dt;
  kfLon += kfVLon * dt;
}

void kalmanUpdate(double measLat, double measLon, float dt) {
  const float kPos = 0.22f;
  const float kVel = 0.08f;

  const double errLat = measLat - kfLat;
  const double errLon = measLon - kfLon;

  kfLat += kPos * errLat;
  kfLon += kPos * errLon;

  if (dt > 0.01f) {
    kfVLat += kVel * (errLat / dt);
    kfVLon += kVel * (errLon / dt);
  }
}

void appendAttackLog(const String &attackType) {
  const unsigned long now = millis();
  if (attackType == lastLoggedAttack && (now - lastAttackLogMs) < 3000) return;

  AttackLog entry;
  entry.sec = now / 1000;
  entry.lat = displayLat;
  entry.lon = displayLon;
  entry.risk = antiSpoofRisk;
  entry.attack = attackType;

  int idx;
  if (attackLogCount < MAX_ATTACK_LOGS) {
    idx = (attackLogStart + attackLogCount) % MAX_ATTACK_LOGS;
    attackLogCount++;
  } else {
    idx = attackLogStart;
    attackLogStart = (attackLogStart + 1) % MAX_ATTACK_LOGS;
  }

  attackLogs[idx] = entry;
  lastLoggedAttack = attackType;
  lastAttackLogMs = now;
}

void deepPurgeResync() {
  spoofSim = false;
  jamSim = false;
  safeMode = false;
  stabilityGateBlocked = false;
  spoofCounter = 0;
  antiSpoofRisk = 0;
  zAnomalyScore = 0;
  heatBurnCounter = 0;
  attackLogCount = 0;
  attackLogStart = 0;
  lastLoggedAttack = "";
  lastAttackLogMs = 0;

  driftMean = 8.0;
  driftVar = 12.0;
  hdopMean = 1.2;
  hdopVar = 0.6;
  satMean = 9.0;
  satVar = 2.0;

  satDropRate = 0;
  speedJump = 0;
  hdopSpike = 0;
  lastSpeedKph = 0;
  lastSatellites = 0;
  lastHdopValue = 0;

  while (gpsSerial.available()) gpsSerial.read();

  if (gps.location.isValid()) {
    lastLat = gps.location.lat();
    lastLon = gps.location.lng();
    displayLat = lastLat;
    displayLon = lastLon;
  }

  kalmanInitialized = false;
  kfLat = displayLat;
  kfLon = displayLon;
  kfVLat = 0;
  kfVLon = 0;

  spoofReason = "Deep purge complete. Sensors and AI memory resynchronized.";
  aiStatus = "RESET COMPLETE";
}

/* ================= SECURITY LOGIC ================= */
void processSecurityLogic() {
  const unsigned long nowMs = millis();
  float dt = (lastFixMs == 0) ? 1.0f : (nowMs - lastFixMs) / 1000.0f;
  if (dt < 0.2f) dt = 0.2f;
  if (dt > 5.0f) dt = 5.0f;
  lastFixMs = nowMs;

  if (!gps.location.isValid() || gps.satellites.value() < 4) {
    aiStatus = "NO GPS LOCK";
    spoofReason = "Waiting for valid GNSS lock and satellite visibility.";
    antiSpoofRisk = 0.2;
    zAnomalyScore = 0;
    return;
  }

  currentLat = gps.location.lat();
  currentLon = gps.location.lng();
  speedKph = gps.speed.kmph();
  satellites = gps.satellites.value();
  hdopValue = gps.hdop.hdop();

  satDropRate = max(0.0f, lastSatellites - satellites) / dt;
  speedJump = fabs(speedKph - lastSpeedKph) / dt;
  hdopSpike = fabs(hdopValue - lastHdopValue) / dt;

  driftMeters = calculateDrift(lastLat, lastLon, currentLat, currentLon);

  if (!kalmanInitialized) {
    kfLat = currentLat;
    kfLon = currentLon;
    kfVLat = 0;
    kfVLon = 0;
    kalmanInitialized = true;
  }

  kalmanPredict(dt);
  const double kalmanDrift = calculateDrift(kfLat, kfLon, currentLat, currentLon);
  kalmanUpdate(currentLat, currentLon, dt);

  const float predictedTravelMeters = (speedKph / 3.6f) * dt;
  const float gateTolerance = max(35.0f, (predictedTravelMeters * 4.0f) + 25.0f);
  const float physicalLimitMps = 55.5f; // ~200 km/h
  const float observedMps = driftMeters / dt;
  const bool impossibleTrajectory = observedMps > physicalLimitMps;

  stabilityGateBlocked =
    (speedKph < 8.0f && driftMeters > gateTolerance) ||
    impossibleTrajectory ||
    (kalmanDrift > (gateTolerance * 1.2f));

  if (stabilityGateBlocked) {
    predictedLat = lastLat;
    predictedLon = lastLon;
  } else {
    predictedLat = currentLat;
    predictedLon = currentLon;
  }

  float features[5] = {
    (float) currentLat,
    (float) currentLon,
    speedKph,
    satellites,
    hdopValue
  };

  const int pred = model.predict(features);

  // Statistical layer
  updateEWMA(driftMeters, 0.07f, driftMean, driftVar);
  updateEWMA(hdopValue, 0.08f, hdopMean, hdopVar);
  updateEWMA(satellites, 0.08f, satMean, satVar);

  const float driftZ = zScore(driftMeters, driftMean, driftVar);
  const float hdopZ = zScore(hdopValue, hdopMean, hdopVar);
  const float satZ = zScore(satellites, satMean, satVar);

  const float suspiciousPerfect = (hdopValue < 0.7f && satellites >= 10 && driftMeters > 40) ? 1.0f : 0.0f;
  zAnomalyScore = (0.45f * driftZ) + (0.35f * hdopZ) + (0.20f * satZ) + suspiciousPerfect;
  if (zAnomalyScore > 8.0f) zAnomalyScore = 8.0f;

  // Multi-stage fusion risk
  const float satDropRisk = constrain(satDropRate / 3.0f, 0.0f, 1.0f);
  const float speedJumpRisk = constrain(speedJump / 50.0f, 0.0f, 1.0f);
  const float hdopSpikeRisk = constrain(hdopSpike / 2.5f, 0.0f, 1.0f);
  const float driftRisk = constrain(driftMeters / 300.0f, 0.0f, 1.0f);
  const float speedMismatchRisk = (speedKph < 4 && driftMeters > 180) ? 1.0f : 0.0f;
  const float hdopRisk = constrain((hdopValue - 1.2f) / 4.0f, 0.0f, 1.0f);
  const float satRisk = constrain((6.0f - satellites) / 6.0f, 0.0f, 1.0f);
  const float zRisk = constrain(zAnomalyScore / 4.0f, 0.0f, 1.0f);

  antiSpoofRisk = constrain(
    (0.20f * driftRisk) +
    (0.14f * speedMismatchRisk) +
    (0.11f * hdopRisk) +
    (0.07f * satRisk) +
    (0.20f * zRisk) +
    (0.10f * satDropRisk) +
    (0.09f * speedJumpRisk) +
    (0.09f * hdopSpikeRisk),
    0.0f,
    1.0f
  );

  // Reasoning layer
  if (impossibleTrajectory) {
    spoofCounter += 2;
    spoofReason = "Trajectory validation failed: impossible speed path detected.";
  } else if (speedJump > 50.0f) {
    spoofCounter++;
    spoofReason = "Sudden speed jump anomaly detected.";
  } else if (satDropRate > 2.5f) {
    spoofCounter++;
    spoofReason = "Rapid satellite drop indicates possible RF interference.";
  } else if (hdopSpike > 2.0f) {
    spoofCounter++;
    spoofReason = "HDOP spike anomaly detected in signal geometry.";
  } else if (stabilityGateBlocked) {
    spoofCounter += 2;
    spoofReason = "Inconsistent Motion vs Speed: Stability Gate rejected an impossible jump.";
  } else if (driftMeters > 250 && speedKph < 5) {
    spoofCounter++;
    spoofReason = "Large position jump detected at low speed.";
  } else if (hdopValue > 3.8f && satellites < 5) {
    spoofCounter++;
    spoofReason = "Weak geometry and unstable GNSS precision detected.";
  } else if (suspiciousPerfect > 0.5f) {
    spoofCounter++;
    spoofReason = "Suspiciously perfect GNSS fix pattern detected.";
  } else {
    spoofCounter = max(0, spoofCounter - 1);
    spoofReason = "Signal behavior currently within normal thresholds.";
  }

  // Spoofing decision
  if (spoofSim || spoofCounter > 3 || antiSpoofRisk > 0.80f) {
    aiStatus = "SPOOF DETECTED - SAFE MODE";
    spoofReason = spoofSim ? "Manual spoof simulation active." : spoofReason;
    safeMode = true;
    displayLat = predictedLat;
    displayLon = predictedLon;

    if (antiSpoofRisk > 0.55f) heatBurnCounter++;
    appendAttackLog("Spoofing");
    return;
  }

  // Jamming decision with hardware veto for AI output
  const bool hardwareDistress = (satellites < 6 || hdopValue > 2.5f);
  const bool aiFlagsJamming = (pred == 1 && hardwareDistress);

  if (jamSim || aiFlagsJamming || satellites < 4 || hdopValue > 4) {
    aiStatus = "JAMMING DETECTED";
    spoofReason = jamSim
      ? "Manual jamming simulation active."
      : (aiFlagsJamming ? "AI + hardware distress agree on jamming." : "Hardware signal quality indicates jamming risk.");

    safeMode = false;
    displayLat = predictedLat;
    displayLon = predictedLon;
    appendAttackLog("Jamming");
  } else {
    aiStatus = "NORMAL";
    safeMode = false;
    displayLat = predictedLat;
    displayLon = predictedLon;
  }

  trustScore = satellites / 12.0f;
  trustScore = constrain(trustScore, 0.0f, 1.0f);
  trustScore = constrain(trustScore * (1.0f - (antiSpoofRisk * 0.40f)), 0.0f, 1.0f);

  if (!stabilityGateBlocked) {
    lastLat = currentLat;
    lastLon = currentLon;
  }

  lastSpeedKph = speedKph;
  lastSatellites = satellites;
  lastHdopValue = hdopValue;
}

/* ================= API ================= */
void handleStatus() {
  StaticJsonDocument<384> doc;

  doc["lat"] = displayLat;
  doc["lon"] = displayLon;
  doc["status"] = aiStatus;
  doc["sat"] = satellites;
  doc["hdop"] = hdopValue;
  doc["drift"] = driftMeters;
  doc["trust"] = trustScore;
  doc["speed"] = speedKph;
  doc["risk"] = antiSpoofRisk;
  doc["safe"] = safeMode;
  doc["reason"] = spoofReason;
  doc["zscore"] = zAnomalyScore;
  doc["satdrop"] = satDropRate;
  doc["speedjump"] = speedJump;
  doc["hdopspike"] = hdopSpike;
  doc["gate"] = stabilityGateBlocked;
  doc["burns"] = heatBurnCounter;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleLogs() {
  StaticJsonDocument<2048> doc;
  JsonArray logs = doc.createNestedArray("logs");

  for (int i = 0; i < attackLogCount; i++) {
    const int idx = (attackLogStart + i) % MAX_ATTACK_LOGS;
    JsonObject row = logs.createNestedObject();
    row["time"] = attackLogs[idx].sec;
    row["lat"] = attackLogs[idx].lat;
    row["lon"] = attackLogs[idx].lon;
    row["risk"] = attackLogs[idx].risk;
    row["attack"] = attackLogs[idx].attack;
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println(WiFi.localIP());

  server.on("/", []() { server.send(200, "text/html", index_html); });
  server.on("/status", handleStatus);
  server.on("/logs", handleLogs);

  server.on("/spoof", []() {
    spoofSim = true;
    jamSim = false;
    server.send(200);
  });

  server.on("/jam", []() {
    jamSim = true;
    spoofSim = false;
    server.send(200);
  });

  server.on("/reset", []() {
    deepPurgeResync();
    server.send(200);
  });

  server.begin();
}

/* ================= LOOP ================= */
void loop() {
  server.handleClient();

  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  if (gps.location.isUpdated()) {
    processSecurityLogic();
    Serial.printf(
      "Sats:%.0f HDOP:%.2f Drift:%.1fm Risk:%.2f Z:%.2f Gate:%d Status:%s\n",
      satellites,
      hdopValue,
      driftMeters,
      antiSpoofRisk,
      zAnomalyScore,
      stabilityGateBlocked ? 1 : 0,
      aiStatus.c_str()
    );
  }
}
