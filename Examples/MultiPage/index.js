// Behaviour for the MultiPage start page.
//
// JavaScript is its own language: it is never embedded in the markup. The
// compiler reads this module to build a typed interaction table for the native
// target of index.html only.

const heading = document.getElementById("workshop-heading");

function onHeadingClick(event) {
  event.preventDefault();
  heading.textContent = "Workshop";
}

heading.addEventListener("click", onHeadingClick);
