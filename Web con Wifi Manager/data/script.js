document.addEventListener("DOMContentLoaded", () => {
  const tempEl = document.getElementById("temp");
  const humEl = document.getElementById("hum");
  const datePicker = document.getElementById("datePicker");
  const themeToggle = document.getElementById("themeToggle");
  const themeIcon = document.getElementById("themeIcon");

  // --- 1. Manejo de modo oscuro/claro con persistencia ---
  function applyTheme(darkMode) {
    if (darkMode) {
      document.body.classList.add("dark");
      themeIcon.textContent = "🌙";
      localStorage.setItem("theme", "dark");
    } else {
      document.body.classList.remove("dark");
      themeIcon.textContent = "☀️";
      localStorage.setItem("theme", "light");
    }
  }

  // Cargar preferencia previa
  const savedTheme = localStorage.getItem("theme");
  applyTheme(savedTheme === "dark");

  // Botón para alternar tema
  themeToggle.addEventListener("click", () => {
    applyTheme(!document.body.classList.contains("dark"));
  });

  // --- 2. Datos en tiempo real ---
  async function fetchCurrentData() {
    try {
      const res = await fetch("/api/data");
      const json = await res.json();
      if (!json.error) {
        tempEl.textContent = `${json.temp} °C`;
        humEl.textContent = `${json.hum} %`;
      }
    } catch (err) {
      console.error("Error obteniendo datos actuales:", err);
    }
  }
  setInterval(fetchCurrentData, 3000);
  fetchCurrentData();

  // --- 3. Mostrar gráfico del día seleccionado ---
  document.getElementById("showChart").addEventListener("click", async () => {
    const selectedDate = datePicker.value;
    if (!selectedDate) {
      Swal.fire("Error", "Selecciona una fecha primero", "warning");
      return;
    }

    try {
      const res = await fetch(`/api/history?date=${selectedDate}`);
      const data = await res.json();

      if (!data.length) {
        Swal.fire("Sin datos", "No hay datos para esa fecha", "info");
        return;
      }

      const labels = data.map(d => d.time);
      const temps = data.map(d => d.temp);
      const hums = data.map(d => d.hum);

      Swal.fire({
        title: `Historial - ${selectedDate}`,
        html: '<canvas id="chart" width="400" height="200"></canvas>',
        didOpen: () => {
          const ctx = document.getElementById("chart").getContext("2d");
          new Chart(ctx, {
            type: "line",
            data: {
              labels: labels,
              datasets: [
                {
                  label: "Temperatura (°C)",
                  data: temps,
                  borderColor: "red",
                  fill: false
                },
                {
                  label: "Humedad (%)",
                  data: hums,
                  borderColor: "blue",
                  fill: false
                }
              ]
            },
            options: {
              responsive: true,
              animation: false,
              scales: {
                y: { beginAtZero: false }
              }
            }
          });
        }
      });
    } catch (err) {
      console.error("Error obteniendo historial:", err);
      Swal.fire("Error", "No se pudieron cargar los datos", "error");
    }
  });
});
