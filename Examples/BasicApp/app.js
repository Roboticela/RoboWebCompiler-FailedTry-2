// Behaviour for the BasicApp example.
//
// JavaScript is its own language with its own frontend: it is never embedded in
// the markup. The compiler reads this module to build a typed interaction table
// for the native target.

const taskInput = document.getElementById("task");
const taskList = document.getElementById("tasks");
const saveButton = document.getElementById("save");
const cancelButton = document.getElementById("cancel");

let nextTaskId = 1;

function onSave(event) {
  event.preventDefault();

  const value = taskInput.value.trim();
  if (value.length === 0) {
    return;
  }

  const item = document.createElement("li");
  item.textContent = value;
  taskList.appendChild(item);
  nextTaskId = nextTaskId + 1;
  taskInput.value = "";
}

function onCancel(event) {
  event.preventDefault();
  taskInput.value = "";
}

saveButton.addEventListener("click", onSave);
cancelButton.addEventListener("click", onCancel);
