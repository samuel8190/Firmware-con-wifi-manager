document.addEventListener("DOMContentLoaded", () => {
  const tempEl = document.getElementById("temp");
  const humEl = document.getElementById("hum");
  const themeToggle = document.getElementById("themeToggle");
  const themeIcon = document.getElementById("themeIcon");

  // Toggle oscuro / claro
  themeToggle.addEventListener("click", () => {
    document.body.classList.toggle("dark");
    if (document.body.classList.contains("dark")) {
      themeIcon.textContent = "🌙";
    } else {
      themeIcon.textContent = "☀️";
    }
  });

  // Actualizar datos cada 3s
  async function fetchData() {
    try {
      const res = await fetch("/data");
      const json = await res.json();
      tempEl.textContent = `${json.temp} °C`;
      humEl.textContent = `${json.hum} %`;
    } catch (err) {
      console.error("Error obteniendo datos", err);
    }
  }
  setInterval(fetchData, 3000);
  fetchData();

  // Mostrar gráficos
  document.getElementById("showChart").addEventListener("click", () => {
    Swal.fire({
      title: "Historial de Datos",
      html: '<canvas id="chart" width="400" height="200"></canvas>',
      didOpen: () => {
        const ctx = document.getElementById("chart").getContext("2d");
        new Chart(ctx, {
          type: "line",
          data: {
            labels: ["10:00", "10:05", "10:10", "10:15", "10:20"],
            datasets: [
              {
                label: "Temperatura (°C)",
                data: [22, 23, 24, 23.5, 25],
                borderColor: "red",
                fill: false,
              },
              {
                label: "Humedad (%)",
                data: [45, 47, 50, 48, 46],
                borderColor: "blue",
                fill: false,
              },
            ],
          },
        });
      },
    });
  });
});
