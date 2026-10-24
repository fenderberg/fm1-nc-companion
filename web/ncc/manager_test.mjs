// SPDX-License-Identifier: GPL-3.0-only
// Run the real Manager and its mock MIDI link without a browser; exercise the bound controls and old replies.
import fs from "node:fs";
import vm from "node:vm";
import assert from "node:assert/strict";

class Element {
  constructor() { this.children = []; this.options = this.children; this.style = {}; this.value = "0"; }
  append(...items) { this.children.push(...items); }
  replaceChildren(...items) { this.children.length = 0; this.append(...items); }
  setAttribute(name, value) { this[name] = value; }
  remove() {}
}
const elements = new Map();
const document = { body: new Element(), createElement: () => new Element(), querySelectorAll: () => [],
  getElementById: (id) => { if (!elements.has(id)) elements.set(id, new Element()); return elements.get(id); } };
const html = fs.readFileSync(new URL("../editor.html", import.meta.url), "utf8");
const proto = html.slice(html.indexOf("/*PROTO-BEGIN*/"), html.indexOf("/*PROTO-END*/"));
const source = fs.readFileSync(new URL("manager.js", import.meta.url), "utf8");
const context = vm.createContext({ document, location: { search: "?mock=1" }, URLSearchParams, URL, console,
  setTimeout, clearTimeout, performance, TextDecoder, TextEncoder, Uint8Array, ArrayBuffer, confirm: () => true });
vm.runInContext(proto + "\n" + source + `
globalThis.api = { typeOf, connect, loadParts, parseNccInfo, renderSettings, get: () => dev, busy: () => busy,
  send: args => rq([NCMD.SET, args]),
  load: (part, index) => rq([NCMD.LOAD, [part, ...v14enc(index)]]),
  legacy: args => { dev.ncc = parseNccInfo(args); dev.link.request = async () =>
    [2, ...[0, 1].flatMap(p => [...v14enc(0), 0, 0, p + 2, 12, ...strEnc("TEST")]), 1, 1, 120, 0]; }
};`, context);
const api = context.api;
const plain = (v) => JSON.parse(JSON.stringify(v));
const el = (id) => document.getElementById(id);
async function change(id, value) {
  el(id).value = String(value);
  el(id).onchange({ target: el(id) });
  for (let i = 0; i < 200 && api.busy(); i++) await new Promise((resolve) => setTimeout(resolve, 5));
  assert.equal(api.busy(), false, "control action completed");
}

for (const [name, cat] of [["TOMS", 6], ["VOX LEAD", 3], ["FROZEN", 2], ["SHIMMER", 2],
  ["WHISPER", 2], ["BOWED METAL", 2], ["SAX", 3], ["GLITCH", 3], ["ARCADE", 3]])
  assert.equal(api.typeOf(name), cat, `category preview: ${name}`);
await api.connect();
assert.equal(api.get().ncc.perfCaps, 7);
assert.equal(api.get().ncc.engines.length, 12, "Manager demo advertises all current engines");
assert.ok(api.get().lib.length > 16, "demo library exercises pagination");
for (const engine of [9, 8, 5, 3, 7, 11, 4]) {
  const sound = api.get().lib.find(s => s.eng === engine);
  assert.ok(sound, `demo offers engine ${engine}`);
  const left = plain(api.get().parts[0]);
  assert.equal((await api.load(1, sound.i))[1], 0);
  await api.loadParts();
  assert.equal(api.get().parts[1].eng, engine, "extra sound loads on R");
  assert.deepEqual(plain(api.get().parts[0]), left, "loading R preserves L");
}
assert.equal(el("dual").disabled, false);
await change("dual", 1);
assert.equal(api.get().dual, 1);
await change("droneR", 2);
assert.deepEqual(plain(api.get().drone), [{ mode: 0, on: 0 }, { mode: 1, on: 1 }]);
await change("droneR", 0);
assert.deepEqual(plain(api.get().drone[1]), { mode: 1, on: 0 }, "off remembers REPLACE");
await change("droneL", 1);
await change("dual", 2);
assert.equal(api.get().dual, 2);
assert.deepEqual(plain(api.get().drone[0]), { mode: 0, on: 1 });
assert.equal((await api.send([4, 3]))[1], 1);
assert.equal((await api.send([5, 2, 0, 0]))[1], 1);

const str = (s) => [...s].map((c) => c.charCodeAt(0)).concat(0);
const oldInfo = [...str("NCC"), ...str("1.6.1"), 1, ...str("ALL"), 1, 12, ...str("FM6")];
assert.equal(api.parseNccInfo(oldInfo).perfCaps, 0, "old INFO has no performance capability");
api.legacy(oldInfo);
await api.loadParts(); api.renderSettings();
assert.equal(api.get().bpm, 120);
assert.equal(api.get().parts.length, 2);
assert.equal(el("dual").disabled, true);
assert.equal(el("droneL").disabled, true);
assert.equal(el("chL").disabled, false, "old firmware's existing settings remain usable");
console.log("ncc manager: controls, mock protocol and legacy firmware replies ok");
