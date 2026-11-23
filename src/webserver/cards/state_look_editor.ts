import { createColorSwatches } from "../components/color_swatches";
import { resolveColorRef } from "../model/config_primitives";

// "Look per state" of an action card, edited as rows: the state text, its icon and its colour.
// Stored as one text: "State:Icon:Colour;State:Icon:Colour;@entity:unit". Colour is a hex value or a
// standard colour ("@Name"); the last entry ("@entity:unit") is the optional value shown on the left
// half of a wide card.

export interface StateLookRow { state: string; icon: string; color: string }
export interface StateLookInfo { entity: string; unit: string }
export interface StateLookModel { rows: StateLookRow[]; info: StateLookInfo }

export function parseStateLook(spec: string): StateLookModel {
  const rows: StateLookRow[] = [];
  const info: StateLookInfo = { entity: "", unit: "" };
  for (const raw of String(spec || "").split(";")) {
    const item = raw.trim();
    if (!item) continue;
    if (item[0] === "@") {
      const colon = item.indexOf(":");
      info.entity = (colon < 0 ? item.slice(1) : item.slice(1, colon)).trim();
      info.unit = colon < 0 ? "" : item.slice(colon + 1).trim();
      continue;
    }
    const first = item.indexOf(":");
    if (first < 0) { rows.push({ state: item, icon: "", color: "" }); continue; }
    const second = item.indexOf(":", first + 1);
    rows.push({
      state: item.slice(0, first).trim(),
      icon: (second < 0 ? item.slice(first + 1) : item.slice(first + 1, second)).trim(),
      color: second < 0 ? "" : item.slice(second + 1).trim(),
    });
  }
  return { rows, info };
}

export function serializeStateLook(model: StateLookModel): string {
  const parts = model.rows
    .filter((row) => row.state.trim())
    .map((row) => {
      const cleanState = row.state.replace(/[;:]/g, "").trim();
      const icon = row.icon && row.icon !== "Auto" ? row.icon : "";
      const color = row.color || "";
      return cleanState + (icon || color ? ":" + icon : "") + (color ? ":" + color : "");
    });
  const entity = model.info.entity.trim();
  if (entity) parts.push("@" + entity + (model.info.unit.trim() ? ":" + model.info.unit.trim() : ""));
  return parts.join(";");
}

export function renderStateLookEditor(panel: HTMLElement, b: any, helpers: any, getSpec: () => string, save: (spec: string) => void): void {
  const model = parseStateLook(getSpec());
  const section = document.createElement("div");
  section.className = "sp-look-editor";
  const title = document.createElement("div");
  title.className = "sp-look-title";
  title.textContent = "Look per state";
  section.appendChild(title);
  const hint = document.createElement("div");
  hint.className = "sp-hint";
  hint.textContent = "Choose an icon and colour for each state of the sensor entity.";
  section.appendChild(hint);
  const rowsBox = document.createElement("div");
  section.appendChild(rowsBox);

  function commit() { save(serializeStateLook(model)); }

  function renderRows() {
    rowsBox.textContent = "";
    model.rows.forEach((row, index) => {
      const card = document.createElement("div");
      card.className = "sp-look-row";
      const head = document.createElement("div");
      head.className = "sp-look-head";
      const stateInput = helpers.textInput(helpers.idPrefix + "look-state-" + index, row.state, "State, e.g. Eco");
      stateInput.classList.add("sp-look-state");
      const onState = () => { row.state = stateInput.value; commit(); };
      stateInput.addEventListener("input", onState);
      stateInput.addEventListener("change", onState);
      const dot = document.createElement("button");
      dot.type = "button";
      dot.className = "sp-look-color";
      dot.title = "Colour";
      function paintDot() {
        const hex = resolveColorRef(row.color);
        dot.style.backgroundColor = /^[0-9A-Fa-f]{6}$/.test(hex) ? "#" + hex : "transparent";
      }
      paintDot();
      const remove = document.createElement("button");
      remove.type = "button";
      remove.className = "sp-icon-button";
      remove.title = "Remove state";
      remove.innerHTML = '<span class="mdi mdi-delete"></span>';
      remove.addEventListener("click", () => { model.rows.splice(index, 1); commit(); renderRows(); });
      head.appendChild(stateInput);
      head.appendChild(dot);
      head.appendChild(remove);
      card.appendChild(head);
      const iconPicker = helpers.makeIconPicker(helpers.idPrefix + "look-icon-" + index, helpers.idPrefix + "look-icon-input-" + index,
        row.icon || "", (opt: any) => { row.icon = opt && opt !== "Auto" ? opt : ""; commit(); }, "Icon");
      card.appendChild(iconPicker);
      const swatches: any = createColorSwatches(() => row.color, (value: string) => {
        row.color = value;
        paintDot();
        commit();
      }, "Colour for state " + (row.state || index + 1));
      swatches.hidden = true;
      swatches.classList.add("sp-look-swatches");
      dot.addEventListener("click", () => { swatches.hidden = !swatches.hidden; });
      card.appendChild(swatches);
      rowsBox.appendChild(card);
    });
  }
  renderRows();

  const add = document.createElement("button");
  add.type = "button";
  add.className = "sp-action-btn";
  add.textContent = "Add state";
  add.addEventListener("click", () => { model.rows.push({ state: "", icon: "", color: "" }); renderRows(); });
  section.appendChild(add);

  // Optional value in the left half of a wide card (e.g. the room temperature or the vacuum status).
  const infoTitle = document.createElement("div");
  infoTitle.className = "sp-look-title";
  infoTitle.textContent = "Left value (wide cards)";
  section.appendChild(infoTitle);
  const infoEntity = helpers.entityField("Entity", helpers.idPrefix + "look-info-entity", model.info.entity,
    "e.g. sensor.living_room_temperature", ["sensor", "vacuum", "climate", "input_select", "binary_sensor"], null, false);
  const infoUnit = helpers.textField("Unit (numbers only)", helpers.idPrefix + "look-info-unit", model.info.unit, "e.g. °", null, false);
  const onInfo = () => { model.info.entity = infoEntity.input.value; model.info.unit = infoUnit.input.value; commit(); };
  for (const input of [infoEntity.input, infoUnit.input]) {
    input.addEventListener("input", onInfo);
    input.addEventListener("change", onInfo);
  }
  section.appendChild(infoEntity.field);
  section.appendChild(infoUnit.field);
  panel.appendChild(section);
}
