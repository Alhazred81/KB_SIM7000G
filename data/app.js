// Segédfüggvény a szép sorok rendereléséhez (mint a C++ stateRow)
function stateRow(key, val, cls = "") {
  return `<div class='row'><span class='k'>${key}</span><span class='v ${cls}'>${val}</span></div>`;
}

async function loadHome() {
  const statusEl = document.getElementById("status");

  try {
    const r = await fetch("/api/home");
    if (!r.ok) throw new Error("HTTP hiba");
    const d = await r.json();

    let html = "<div class='card'><h2>📡 Modem</h2>";
    html += stateRow(
      "Állapot",
      d.modemReady ? "Kész" : "Inicializálás...",
      d.modemReady ? "g" : "y",
    );
    html += stateRow(
      "Hálózat",
      d.registered ? "Felcsatlakozva" : "Keresés...",
      d.registered ? "g" : "y",
    );
    html += stateRow("Szolgáltató", d.operator || "N/A");
    html += stateRow("Jelerősség", d.signal ? `${d.signal} (0-31)` : "N/A");
    html += stateRow("Típus", d.netType || "N/A");
    html += "</div>";

    html += "<div class='card'><h2>🌍 GNSS (GPS)</h2>";
    html += stateRow(
      "Pozíció Fix",
      d.gnssFix ? "OK" : "Nincs fix",
      d.gnssFix ? "g" : "r",
    );
    if (d.gnssFix) {
      html += stateRow("Szélesség", `${d.lat.toFixed(6)}°`);
      html += stateRow("Hosszúság", `${d.lon.toFixed(6)}°`);
    } else {
      html += stateRow("Kiinduló Széles.", `${d.lat.toFixed(6)}°`);
      html += stateRow("Kiinduló Hossz.", `${d.lon.toFixed(6)}°`);
    }
    html += stateRow("Használt Műholdak", d.sat);
    html += stateRow("NTP Helyi idő", d.time || "-");
    html += "</div>";

    statusEl.innerHTML = html;
  } catch (err) {
    console.error("Adatlekérési hiba:", err);
    // Ha még töltés képernyő van, jelezzük a hibát
    if (statusEl.innerHTML.includes("⏳")) {
      statusEl.innerHTML = `<div class='card'><div class='msg err'>Hiba a kapcsolatban az ESP32-vel. Újrapróbálkozás...</div></div>`;
    }
  }
}

// Azonnali betöltés, majd 3 másodpercenkénti frissítés
loadHome();
setInterval(loadHome, 3000);
