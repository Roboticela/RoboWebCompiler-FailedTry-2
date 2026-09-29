// Behaviour for the MultiPage dashboard page.

const total = document.getElementById("total");

function onTotalClick(event) {
  event.preventDefault();
  total.textContent = "39";
}

total.addEventListener("click", onTotalClick);
