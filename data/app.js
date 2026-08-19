document.addEventListener("DOMContentLoaded", () => {
  // Adatok betöltése
  fetch("/api/config")
    .then((response) => response.json())
    .then((data) => {
      if (document.getElementById("apPass")) {
        document.getElementById("apPass").value = data.apPass || "";
      }
      if (document.getElementById("ntfyTopic")) {
        document.getElementById("ntfyTopic").value = data.ntfyTopic || "";
      }
      if (document.getElementById("ntfyNickname")) {
        document.getElementById("ntfyNickname").value = data.ntfyNickname || "";
      }
    })
    .catch((err) => console.error("Hiba a konfiguráció betöltésekor: ", err));

  // Mentés eseménykezelője
  const btnSave = document.getElementById("btnSave");
  if (btnSave) {
    btnSave.addEventListener("click", () => {
      btnSave.disabled = true;
      btnSave.innerText = "Mentés...";

      const payload = {
        apPass: document.getElementById("apPass")
          ? document.getElementById("apPass").value
          : "",
        ntfyTopic: document.getElementById("ntfyTopic")
          ? document.getElementById("ntfyTopic").value
          : "",
        ntfyNickname: document.getElementById("ntfyNickname")
          ? document.getElementById("ntfyNickname").value
          : "",
      };

      fetch("/api/config", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      })
        .then((res) => res.json())
        .then((data) => {
          if (data.status === "ok") {
            alert("Beállítások elmentve.");
          } else {
            alert("Hiba a mentés során.");
          }
        })
        .catch((err) => {
          console.error(err);
          alert("Hálózati hiba mentéskor.");
        })
        .finally(() => {
          btnSave.disabled = false;
          btnSave.innerText = "Mentés";
        });
    });
  }

  // Ntfy teszt eseménykezelője
  const btnTestNtfy = document.getElementById("btnTestNtfy");
  if (btnTestNtfy) {
    btnTestNtfy.addEventListener("click", () => {
      const prio = parseInt(document.getElementById("testPriority").value) || 3;
      btnTestNtfy.disabled = true;
      btnTestNtfy.innerText = "Küldés folyamatban...";

      fetch("/api/test-ntfy", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ priority: prio }),
      })
        .then((res) => res.json())
        .then((data) => {
          if (data.status === "ok") {
            alert("Az értesítés sikeresen átment a modemen.");
          } else {
            alert(
              "Hiba történt az elküldés során. Ellenőrizd a modem logokat.",
            );
          }
        })
        .catch((err) => {
          console.error(err);
          alert("Hálózati hiba a szerverrel való kommunikációban.");
        })
        .finally(() => {
          btnTestNtfy.disabled = false;
          btnTestNtfy.innerText = "Teszt Üzenet Küldése";
        });
    });
  }
});
