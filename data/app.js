function stateRow(key, val, cls = "") {
  return `<div class='row'><span class='k'>${key}</span><span class='v ${cls}'>${val}</span></div>`;
}

async function loadHome() {
  if (document.activeElement && document.activeElement.name === "pin") return;
  const statusEl = document.getElementById("status");

  try {
    const r = await fetch("/api/home");
    if (!r.ok) throw new Error("HTTP hiba");
    const d = await r.json();

    let html = "";

    if (!d.pinSaved) {
      html += `
        <div class='card' style='border: 2px solid var(--warn);'>
          <h2 style='color: var(--warn);'>⚠️ SIM PIN kód szükséges</h2>
          <p class='hint' style='margin-bottom: 15px;'>A hálózati csatlakozáshoz meg kell adnod a SIM kártya PIN kódját. Ha nincs rajta PIN, hagyd üresen és úgy mentsd el.</p>
          <form action='/savepin' method='POST'>
            <input type='password' name='pin' placeholder='4-8 számjegy (pl. 1234)' pattern='[0-9]{0,8}' style='max-width: 200px;'>
            <button type='submit' class='warn'>PIN Mentése és Csatlakozás</button>
          </form>
        </div>`;
    }

    html += "<div class='card'><h2>📡 Modem</h2>";
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
    statusEl.innerHTML = `<div class='card' style='text-align:center;'><div class='msg warn' style='margin-bottom:0;'>⏳ Várakozás a szerverre...<br><span style='font-size:11px;font-weight:normal;'>A szerver jelenleg elfoglalt.</span></div></div>`;
  }
}

loadHome();
setInterval(loadHome, 3000);
