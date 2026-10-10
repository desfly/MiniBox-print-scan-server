(function () {
  "use strict";
  var target = document.getElementById("status");

  function refresh() {
    fetch("/cgi-bin/minibox-status", { cache: "no-store" })
      .then(function (response) {
        if (!response.ok) throw new Error("HTTP " + response.status);
        return response.text();
      })
      .then(function (text) {
        target.textContent = text.trim();
        target.className = "ok";
      })
      .catch(function (error) {
        target.textContent = "Вебсервер відповідає, але стан служб недоступний: " + error.message;
        target.className = "warn";
      });
  }

  refresh();
  window.setInterval(refresh, 10000);
}());
