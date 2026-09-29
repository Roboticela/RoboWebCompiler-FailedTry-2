// Behaviour for the MultiPage sign-in page.

const email = document.getElementById("email");
const signIn = document.getElementById("sign-in");

function onSignIn(event) {
  event.preventDefault();
  email.focus();
}

signIn.addEventListener("click", onSignIn);
