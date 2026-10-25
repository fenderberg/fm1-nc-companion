// SPDX-License-Identifier: GPL-3.0-only
// NCC integration in the real WebAssembly emulator: panel gestures + incoming MIDI, with read-only observations.
import fs from "node:fs";
import assert from "node:assert/strict";

const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2]), {});
let ex = instance.exports, mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const B = { SCL: 1, EDIT: 4, GLO: 5, HOME: 6, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, L: 12, R: 13 };
let peak = 0;
function render(ms) {
  const frames = Math.round(ms * 44.1);
  for (let at = 0; at < frames; at += 128) {
    const n = Math.min(128, frames - at);
    ex.web_render(n);
    for (const ptr of [ex.web_out_l(), ex.web_out_r()])
      for (const sample of new Float32Array(mem.buffer, ptr, n)) {
        assert.ok(Number.isFinite(sample) && Math.abs(sample) <= 1, "finite bounded audio");
        peak = Math.max(peak, Math.abs(sample));
      }
  }
}
function tap(...buttons) {
  ex.web_buttons(buttons.reduce((mask, b) => mask | (1 << b), 0)); render(40);
  ex.web_buttons(0); render(40);
}
function turn(role, detents) { ex.web_enc(role, detents); render(40); }
function note(status, pitch, velocity = 0) { ex.web_midi(status, pitch, velocity); render(120); }
const gate = (p, n) => ex.web_test_ncc_gate(p, n);
render(1000);
assert.equal(ex.web_test_ncc_sends(0), 0, "boot L without delay/reverb");
assert.equal(ex.web_test_ncc_sends(1), 0, "boot R without delay/reverb");
tap(B.SCL);
assert.equal(ex.web_test_ncc_state() & 3, 1, "SCL enables Dual CH3");
note(0x92, 60, 100);
assert.ok(gate(0, 60) && gate(1, 60), "CH3 plays both parts");
note(0x93, 67, 100);
assert.equal(gate(0, 67) + gate(1, 67), 0, "other channel supplies no notes");
tap(B.R); note(0x82, 60);
assert.equal(gate(0, 60) + gate(1, 60), 0, "focus does not redirect note-offs");
tap(B.GLO);
if (process.argv[3]) screenshot(process.argv[3].replace(/\.ppm$/, "-settings-main.ppm"));
tap(B.PLAY); turn(3, 1);
assert.equal(ex.web_test_ncc_view(), 3 | (1 << 8), "GLO settings page 2");
assert.equal(ex.web_test_ncc_state() & 3, 2, "settings select CH4");
turn(0, 2);
assert.equal(ex.web_test_ncc_view(), 3 | (1 << 8), "SELECT does not navigate settings");
ex.web_buttons(1 << B.PLAY); render(450);
assert.equal(ex.web_test_ncc_view(), 3 | (1 << 8), "PLAY waits below the long-press threshold");
render(150);
assert.equal(ex.web_test_ncc_view(), 3, "long PLAY goes to the previous settings page");
ex.web_buttons(0); render(40);
assert.equal(ex.web_test_ncc_view(), 3, "long PLAY release does not advance again");
tap(B.PLAY);
if (process.argv[3]) screenshot(process.argv[3].replace(/\.ppm$/, "-settings.ppm"));
tap(B.SCL); assert.equal(ex.web_test_ncc_state() & 3, 0, "SCL turns Dual off");
tap(B.SCL); assert.equal(ex.web_test_ncc_state() & 3, 2, "SCL remembers CH4");
tap(B.HOME);
note(0x92, 69, 100);
assert.equal(gate(0, 69) + gate(1, 69), 0);
note(0x93, 72, 100);
assert.ok(gate(0, 72) && gate(1, 72));
note(0x83, 72);

ex.web_midi(0xB3, 91, 51); ex.web_midi(0xB2, 94, 47); render(30);
assert.equal(ex.web_test_ncc_sends(1) >> 7, 51, "R reverb can be turned up");
tap(B.L); turn(2, 1); render(200);
assert.equal(ex.web_test_ncc_sends(0), 0, "new sound resets L sends");
assert.equal(ex.web_test_ncc_sends(1) >> 7, 51, "other part keeps its send");

tap(B.R); tap(B.SEQ);
ex.web_keys(1 << 12); render(30);
const heldKey = Array.from({ length: 128 }, (_, n) => n).find(n => gate(1, n));
assert.notEqual(heldKey, undefined, "local key plays R in Dual");
tap(B.SEQ);
assert.ok(gate(1, heldKey), "Drone off keeps a physically held local key");
ex.web_keys(0); render(30);
assert.equal(gate(0, heldKey) + gate(1, heldKey), 0, "local release ends both layers");

tap(B.GLO); tap(B.PLAY); turn(3, -1); tap(B.HOME); // CH3
// Edit the selected part, and use its own POLY setting for the chord drone.
tap(B.R); tap(B.EDIT); tap(B.PLAY);
const cutoffR = ex.web_test_ncc_knob(0);
tap(B.L); const parameterL = ex.web_test_ncc_knob(0); tap(B.R); turn(3, -8);
assert.ok(ex.web_test_ncc_knob(0) < cutoffR, "EDIT changes R cutoff");
tap(B.L); assert.equal(ex.web_test_ncc_knob(0), parameterL, "EDIT leaves L unchanged"); tap(B.R);
if (process.argv[3]) screenshot(process.argv[3].replace(/\.ppm$/, "-edit.ppm"));
for (let i = 0; i < 6; i++) tap(B.PLAY); // ENGINE 2 -> VOICE
assert.equal(ex.web_test_ncc_view(), 4 | (7 << 8));
turn(3, -3); assert.equal(ex.web_test_ncc_knob(0), 0, "R POLY for chord drone");
tap(B.HOME);
tap(B.R); tap(B.ARP); tap(B.PLAY); turn(6, 1); tap(B.ARP); tap(B.SEQ);
assert.equal(ex.web_test_ncc_state() & 40, 40, "R REPLACE Drone enabled via the panel");
tap(B.L); tap(B.ARP); tap(B.REC);
note(0x92, 60, 100); note(0x82, 60); note(0x92, 67, 100); render(200);
assert.equal(gate(1, 60), 0, "R's previous drone stops");
assert.ok(gate(1, 67), "R's new drone rings");
assert.equal(ex.web_test_ncc_held(0), 1, "L arp follows its own HOLD settings");
note(0x82, 60); note(0x82, 67);
assert.ok(gate(1, 67), "note-off leaves REPLACE Drone ringing");
// Three separate Note Ons are one physical chord; no time-window limit while a key remains held.
ex.web_midi(0x92, 60, 100); ex.web_midi(0x92, 64, 100); ex.web_midi(0x92, 67, 100); render(20);
assert.ok(gate(1, 67) && gate(1, 60) && gate(1, 64), "old drone overlaps the new chord without muting the part");
render(300);
assert.ok(gate(1, 60) && gate(1, 64) && gate(1, 67), "REPLACE drones a whole MIDI chord");
for (const n of [60, 64, 67]) ex.web_midi(0x82, n, 0);
render(30); note(0x92, 62, 100); render(200); note(0x92, 65, 100);
assert.ok(!gate(1, 60) && !gate(1, 64) && !gate(1, 67), "next gesture replaces the old chord");
assert.ok(gate(1, 62) && gate(1, 65), "a later addition belongs to the held gesture");
note(0x82, 62); note(0x82, 65);
assert.ok(peak > 0.001, "both routed parts produce audio");
if (process.argv[3]) {
  screenshot(process.argv[3]);
  tap(B.HOME); tap(B.L); tap(B.EDIT);
  for (let i = 0; i < 8; i++) tap(B.PLAY);
  screenshot(process.argv[3].replace(/\.ppm$/, "-operator.ppm"));
}

// A fresh emulated boot; select each engine via the existing category/engine/sound knobs.
const fresh = await WebAssembly.instantiate(fs.readFileSync(process.argv[2]), {});
ex = fresh.instance.exports; mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot(); render(1000);
tap(B.R); tap(B.HOME);

const extraEngines = [9, 8, 5, 3, 7, 11, 4], macroCCs = [1, 2, 5, 11, 71, 74, 12, 13];
for (let index = 0; index < extraEngines.length; index++) {
  // ALL category; ALGORITHM's position is the existing five engines, then the seven extras.
  turn(1, index ? 1 : 6); render(400);
  assert.equal(ex.web_test_ncc_engine(1), extraEngines[index], "panel selects extra engine");
  assert.equal(ex.web_test_ncc_sends(1), 0, "extra sounds start dry");
  tap(B.SCL); // Dual CH3 for notes, separate CC channels.
  note(0x92, 60, 100);
  assert.ok(gate(1, 60), "extra engine receives Dual notes");
  for (let m = 0; m < 8; m++) {
    const other = Array.from({ length: 8 }, (_, k) => ex.web_test_ncc_macro(0, k));
    const base = ex.web_test_ncc_macro(1, m);
    ex.web_midi(0xB3, macroCCs[m], 127); render(40);
    let changed = ex.web_test_ncc_macro(1, m) !== base;
    ex.web_midi(0xB3, macroCCs[m], 0); render(40);
    changed ||= ex.web_test_ncc_macro(1, m) !== base;
    assert.ok(changed, `engine ${extraEngines[index]}, macro ${m + 1} moves`);
    assert.deepEqual(Array.from({ length: 8 }, (_, k) => ex.web_test_ncc_macro(0, k)), other,
      "CC4 edits R only, including in Dual");
  }
  note(0x82, 60); tap(B.SCL);
  tap(B.EDIT); turn(3, 1); // real descriptor and sound edit for this engine
  tap(B.HOME);
  if (index === 0 && process.argv[3]) screenshot(process.argv[3].replace(/\.ppm$/, "-phys.ppm"));
}

function screenshot(path) {
  const fb = new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240), rgb = Buffer.alloc(fb.length * 3);
  fb.forEach((raw, i) => {
    const v = (raw >>> 8) | ((raw & 255) << 8);
    rgb[3 * i] = Math.round((v >>> 11) * 255 / 31);
    rgb[3 * i + 1] = Math.round(((v >>> 5) & 63) * 255 / 63);
    rgb[3 * i + 2] = Math.round((v & 31) * 255 / 31);
  });
  fs.writeFileSync(path, Buffer.concat([Buffer.from("P6\n240 240\n255\n"), rgb]));
}
console.log(`ncc emulator: Dual sources, separate controls, GLO settings, EDIT, REPLACE chords, arp, sound defaults and seven extra engines / 56 macros ok (peak ${peak.toFixed(3)})`);
