export function trimConfigFields(fields: string[]): string[] {
  while (fields.length > 1 && !fields[fields.length - 1]) fields.pop();
  return fields;
}

export function encodeConfigField(value: string | number | boolean | null | undefined): string {
  return String(value || "").replace(/[%,;|:]/g, (ch) => {
    const hex = ch.charCodeAt(0).toString(16).toUpperCase();
    return "%" + (hex.length < 2 ? "0" : "") + hex;
  });
}

export function decodeConfigField(value: string | number | boolean | null | undefined): string {
  const str = String(value || "");
  // Decode complete UTF-8 runs so multi-byte sequences such as %C2%B0 become °.
  return str.replace(/(%[0-9a-fA-F]{2})+/g, (run) => {
    try {
      return decodeURIComponent(run);
    } catch {
      return run;
    }
  });
}

export function legacyButtonConfigSafe(fields: readonly string[]): boolean {
  return fields.join(";").charAt(0) !== "~" && fields.every((field) => {
    return String(field || "").indexOf(";") < 0;
  });
}

export function configOptionEnabled(options: unknown, name: string): boolean {
  return String(options || "").split(",").some((part) => part === name);
}

export function setConfigOption(options: unknown, name: string, enabled: boolean): string {
  const out: string[] = [];
  let found = false;
  for (const part of String(options || "").split(",")) {
    if (!part) continue;
    if (part === name) {
      found = true;
      if (enabled) out.push(part);
    } else if (out.indexOf(part) < 0) {
      out.push(part);
    }
  }
  if (enabled && !found) out.push(name);
  return out.join(",");
}

export function configOptionValue(options: unknown, name: string): string {
  const prefix = name + "=";
  for (const part of String(options || "").split(",")) {
    if (part.indexOf(prefix) === 0) return decodeConfigField(part.substring(prefix.length));
  }
  return "";
}

export function setConfigOptionValue(options: unknown, name: string, value: unknown): string {
  const prefix = name + "=";
  const out: string[] = [];
  for (const part of String(options || "").split(",")) {
    if (!part || part.indexOf(prefix) === 0) continue;
    if (out.indexOf(part) < 0) out.push(part);
  }
  const text = String(value || "").trim();
  if (text) out.push(prefix + encodeConfigField(text));
  return out.join(",");
}

const CARD_COLOR_OPTIONS = ["card_on_color", "card_off_color", "card_active_color"] as const;

// ── Standard colours ("palette") ─────────────────────────────────────────
// A card colour written as "@Name" follows the palette, so changing a palette colour changes
// every card that uses that name. The palette is stored as "Name=RRGGBB;Name=RRGGBB".
export interface PaletteColor { name: string; hex: string }
export const DEFAULT_COLOR_PALETTE = "Aan=FF8C00;Uit=313131;Geel=FFB300;Groen=66BB6A;Blauw=1E88E5;Lichtblauw=4FC3F7;Rood=E64A19;Paars=8F28AB;Grijs=7B797F";
let activePalette: PaletteColor[] = [];

export function cleanPaletteName(value: unknown): string {
  return String(value || "").replace(/[@,;=:#]/g, "").trim().slice(0, 20);
}

export function parseColorPalette(spec: unknown): PaletteColor[] {
  const out: PaletteColor[] = [];
  for (const part of String(spec || "").split(";")) {
    const eq = part.indexOf("=");
    if (eq < 1) continue;
    const name = cleanPaletteName(part.slice(0, eq));
    const hex = part.slice(eq + 1).replace(/^#/, "").trim().toUpperCase();
    if (!name || !/^[0-9A-F]{6}$/.test(hex)) continue;
    if (out.some((entry) => entry.name.toLowerCase() === name.toLowerCase())) continue;
    out.push({ name, hex });
  }
  return out;
}

export function serializeColorPalette(colors: PaletteColor[]): string {
  return colors.map((entry) => entry.name + "=" + entry.hex).join(";");
}

export function setColorPalette(spec: unknown): void {
  activePalette = parseColorPalette(spec);
}

export function getColorPalette(): PaletteColor[] {
  return activePalette.slice();
}

export function isColorRef(value: unknown): boolean {
  return /^@[^@,;=:#]{1,20}$/.test(String(value || ""));
}

// "@Name" -> hex of that palette colour ("" when unknown); anything else is returned unchanged.
export function resolveColorRef(value: unknown): string {
  const text = String(value || "");
  if (!isColorRef(text)) return text;
  const wanted = text.slice(1).toLowerCase();
  const found = activePalette.find((entry) => entry.name.toLowerCase() === wanted);
  return found ? found.hex : "";
}

function normalizedColor(value: unknown): string {
  const raw = String(value || "").trim();
  if (isColorRef(raw)) return raw;  // palette reference: kept as written
  const hex = raw.replace(/^#/, "").toUpperCase();
  return /^[0-9A-F]{6}$/.test(hex) ? hex : "";
}

export function cardActiveColorMode(options: unknown): "matching" | "theme" | "custom" {
  const mode = configOptionValue(options, "card_active_color");
  return mode === "theme" || mode === "custom" ? mode : "matching";
}

export function normalizedCardColorOptions(options: unknown): string {
  const mode = cardActiveColorMode(options);
  const on = normalizedColor(configOptionValue(options, "card_on_color"));
  // Unmarked legacy active-only customisations still migrate to the card colour.
  const off = normalizedColor(configOptionValue(options, "card_off_color")) || (mode === "matching" ? on : "");
  const out = off ? ["card_off_color=" + off] : [];
  if (mode !== "matching") out.push("card_active_color=" + mode);
  if (mode === "custom" && on) out.push("card_on_color=" + on);
  return out.join(",");
}

export function withoutCardColorOptions(options: unknown): string {
  return String(options || "").split(",").filter((part) =>
    !CARD_COLOR_OPTIONS.some((name) => part.indexOf(name + "=") === 0),
  ).filter(Boolean).join(",");
}

export function preserveCardColorOptions(options: unknown, normalized: unknown): string {
  return [withoutCardColorOptions(normalized), normalizedCardColorOptions(options)].filter(Boolean).join(",");
}

export function setCardColor(options: unknown, value: unknown): string {
  const normalized = preserveCardColorOptions(options, options);
  return setConfigOptionValue(normalized, "card_off_color", normalizedColor(String(value || "").replace(/^#/, "").trim()));
}

export function setCardActiveColor(options: unknown, mode: "matching" | "theme" | "custom", color?: string): string {
  let out = preserveCardColorOptions(options, options);
  out = setConfigOptionValue(out, "card_active_color", mode === "matching" ? "" : mode);
  return setConfigOptionValue(out, "card_on_color", mode === "custom" ? normalizedColor(color) : "");
}

export function cardActiveColor(options: unknown, themeColor: string): string {
  const mode = cardActiveColorMode(options);
  if (mode === "custom") {
    const color = normalizedColor(resolveColorRef(configOptionValue(options, "card_on_color")));
    if (color) return color;
  }
  if (mode !== "theme") {
    const matching = activeCardColor(resolveColorRef(configOptionValue(options, "card_off_color")));
    if (matching) return matching;
  }
  return normalizedColor(themeColor) || "FF8C00";
}

export function activeCardColor(color: string): string {
  const hex = color.replace(/^#/, "");
  if (!/^[0-9A-F]{6}$/i.test(hex)) return "";
  const channels = [parseInt(hex.slice(0, 2), 16), parseInt(hex.slice(2, 4), 16), parseInt(hex.slice(4, 6), 16)] as const;
  // Match active_card_color in the firmware, including round-to-nearest channels.
  const pale = channels[0] * 299 + channels[1] * 587 + channels[2] * 114 >= 160000;
  return channels.map(channel => {
    const active = pale ? Math.floor((channel * 75 + 50) / 100)
      : channel + Math.floor(((255 - channel) * 40 + 50) / 100);
    return active.toString(16).padStart(2, "0").toUpperCase();
  }).join("");
}

export function lighterCardColor(color: string): string {
  const hex = color.replace(/^#/, "");
  if (!/^[0-9A-F]{6}$/i.test(hex)) return "";
  // Match lighter_card_color in the firmware, including round-to-nearest channels.
  return [0, 2, 4].map((offset) => {
    const channel = parseInt(hex.slice(offset, offset + 2), 16);
    const lighter = channel + Math.floor(((255 - channel) * 30 + 50) / 100);
    return ("0" + lighter.toString(16)).slice(-2).toUpperCase();
  }).join("");
}
