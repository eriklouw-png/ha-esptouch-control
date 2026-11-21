import type { AppState } from "../state/types";
import type { UiRuntimeState } from "./state";

export function syncCardStyleExtrasUi(state: AppState, runtime: UiRuntimeState): void {
  const els = runtime.els;
  if (els.setCardStyleExtras) els.setCardStyleExtras.hidden = !(state.cardGapSupported || state.labelSizeSupported);
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
