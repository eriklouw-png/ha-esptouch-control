"use strict";
const test = require("node:test");
const { loadTypescriptTest } = require("./helpers/load_typescript_test");

test("subpage grid size token, resize and standard colours", () => {
  loadTypescriptTest("tests/web/subpage_grid_size.test.ts").runSubpageGridSizeTests();
});
