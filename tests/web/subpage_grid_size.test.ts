import {
  buildSubpageGrid,
  parseColorPalette,
  resizeSubpageGrid,
  resolveColorRef,
  serializeSubpageGrid,
  setColorPalette,
  subpageDimsFromSize,
  subpageSizeFromOrder,
} from "../../src/webserver/model";

function equal(actual: unknown, expected: unknown, message: string): void {
  if (JSON.stringify(actual) !== JSON.stringify(expected)) {
    throw new Error(`${message}: expected ${JSON.stringify(expected)}, received ${JSON.stringify(actual)}`);
  }
}

export function runSubpageGridSizeTests(): void {
  // The size token is read from the order and takes no cell.
  const order = ["G3x4", "B", "1", "2"];
  equal(subpageSizeFromOrder(order), "3x4", "size token is read from the subpage order");
  const built = buildSubpageGrid({ order, buttons: [{}, {}] }, 12, 3);
  equal(built.grid.slice(0, 3), [-2, 1, 2], "size token does not occupy a cell");

  // The token is written first, and only when a size is set.
  const grid = [-2, 1, 2, 0, 0, 0];
  equal(serializeSubpageGrid(grid, {}, "Back", "", "2x3"), ["G2x3", "B", "1", "2"], "size token is serialized first");
  equal(serializeSubpageGrid(grid, {}, "Back", "", ""), ["B", "1", "2"], "no token without a size");
  equal(subpageSizeFromOrder(["B", "1"]), "", "no size without a token");

  // A size must fit the device's largest grid.
  equal(subpageDimsFromSize("3x4", 3, 4), { cols: 3, rows: 4, slots: 12 }, "size fits the device");
  equal(subpageDimsFromSize("4x4", 3, 4), null, "size larger than the device is ignored");
  equal(subpageDimsFromSize("", 3, 4), null, "empty size follows the main page");

  // Cards keep their row and column when the grid is resized.
  const resized = resizeSubpageGrid([-2, 1, 0, 2, 0, 3, 0, 0], 4, 3, 9);
  equal(resized[0], -2, "back button stays first");
  equal(resized[1], 1, "card in the first row stays");
  equal(resized.indexOf(2), -1, "card outside the narrower grid is dropped");
  equal(resized[4], 3, "card keeps its row and column");

  // Standard colours resolve "@Name" against the palette.
  setColorPalette("Groen=66BB6A;Rood=E64A19");
  equal(parseColorPalette("Groen=66BB6A;Rood=E64A19").length, 2, "palette parses");
  equal(resolveColorRef("@groen"), "66BB6A", "palette name resolves, ignoring case");
  equal(resolveColorRef("@Onbekend"), "", "unknown palette name resolves to nothing");
  equal(resolveColorRef("FF8C00"), "FF8C00", "plain hex is unchanged");
  setColorPalette("");
}
