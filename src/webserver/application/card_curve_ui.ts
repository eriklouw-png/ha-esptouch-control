import type { AppState } from "../state/types";
import type { UiRuntimeState } from "./state";

export function syncColorPaletteUi(state: AppState, runtime: UiRuntimeState): void {
  const render = (runtime.els as any).renderColorPalette;
  if (typeof render === "function") render();
}

export function syncCardStyleExtrasUi(state: AppState, runtime: UiRuntimeState): void {
  const els = runtime.els;
  if (els.setCardStyleExtras) els.setCardStyleExtras.hidden = !(state.cardGapSupported || state.labelSizeSupported || state.cardAlignmentSupported);
  const alignButtons = els.setCardAlignmentButtons || {};
  for (const align of ["Left", "Centered"]) {
    const button = alignButtons[align];
    if (!button) continue;
    const active = state.cardAlignment === align;
    button.classList.toggle("active", active);
    button.setAttribute("aria-pressed", String(active));
    button.disabled = !state.cardAlignmentSupported;
  }
  document.documentElement.classList.toggle("sp-align-center", state.cardAlignmentSupported && state.cardAlignment === "Centered");
  const buttons = els.setLabelSizeButtons || {};
  for (const size of ["Small", "Medium", "Large"]) {
    const button = buttons[size];
    if (!button) continue;
    const active = state.labelSize === size;
    button.classList.toggle("active", active);
    button.setAttribute("aria-pressed", String(active));
    button.disabled = !state.labelSizeSupported;
  }
  const gapInput = els.setCardGap;
  if (gapInput) {
    gapInput.disabled = !state.cardGapSupported;
    if (document.activeElement !== gapInput) gapInput.value = String(state.cardGap);
  }
}

export function syncCardCurveUi(state: AppState, runtime: UiRuntimeState): void {
  const els = runtime.els;
  const buttons = els.setCardCurveButtons || {};
  for (const preset of ["Square", "Small", "Medium", "Large"]) {
    const button = buttons[preset];
    if (!button) continue;
    const active = state.cardCurve === preset;
    button.classList.toggle("active", active);
    button.setAttribute("aria-pressed", String(active));
    button.disabled = !state.cardCurveSupported;
  }
}
