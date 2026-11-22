// The icon font draws each glyph inside a box that is wider than the drawing,
// and not always centred in it. Centering the box would therefore leave the
// visible icon off-centre. Measure where the glyph really is and store the
// correction in --ink-dx, which the centred-card CSS uses.
let measureCanvas: HTMLCanvasElement | null = null;

export function centerPreviewIcons(root: ParentNode): void {
  if (typeof document === "undefined") return;
  const icons = root.querySelectorAll<HTMLElement>(".sp-btn-icon.mdi");
  for (const icon of Array.from(icons)) {
    const content = getComputedStyle(icon, "::before").content.replace(/^["']|["']$/g, "");
    const size = parseFloat(getComputedStyle(icon).fontSize);
    if (!content || content === "none" || !(size > 0)) continue;
    measureCanvas = measureCanvas || document.createElement("canvas");
    const context = measureCanvas.getContext("2d");
    if (!context) continue;
    context.font = `${size}px "EspControl Icons"`;
    const metrics = context.measureText(content);
    if (!(metrics.width > 0) || !Number.isFinite(metrics.actualBoundingBoxRight) ||
        !Number.isFinite(metrics.actualBoundingBoxLeft)) continue;
    const inkCenter = (metrics.actualBoundingBoxRight - metrics.actualBoundingBoxLeft) / 2;
    const dx = Math.max(-size, Math.min(size, metrics.width / 2 - inkCenter));
    icon.style.setProperty("--ink-dx", dx.toFixed(2) + "px");
  }
}

let fontsRechecked = false;

/** Re-run once when the icon font has finished loading; before that the measurement uses a fallback font. */
export function centerPreviewIconsAfterFonts(root: ParentNode): void {
  centerPreviewIcons(root);
  const fonts = typeof document !== "undefined" ? (document as Document & { fonts?: { ready?: Promise<unknown> } }).fonts : undefined;
  if (fontsRechecked || !fonts || !fonts.ready) return;
  fontsRechecked = true;
  fonts.ready.then(() => centerPreviewIcons(root), () => undefined);
}
