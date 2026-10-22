// SPDX-License-Identifier: GPL-3.0-only
// NC: the NCC Manager's logic (manager.html). It runs on proto.js, Felucca editor's own protocol code (web/editor.html
// between PROTO-BEGIN and PROTO-END: frame, Link, req, parse, pack7, crc32, parseWav, resample, buildSlot ..), copied
// in by tools/ncc_manager.sh, and talks NCC's commands 96..100 (firmware/src/editor_ncc.c) besides Felucca's.
"use strict";

const NCMD = { INFO: 96, LIB: 97, LOAD: 98, PARTS: 99, SET: 100 };
const FM6B = { BEGIN: 74, WRITE: 75, END: 76, LIST: 77 };
const PART = ["L", "R"];
const CH_NAMES = [...Array.from({ length: 16 }, (_, i) => `Channel ${i + 1}`), "OMNI", "OFF"];
const MOCK = new URLSearchParams(location.search).get("mock") === "1";
const $ = (id) => document.getElementById(id);
const el = (tag, props = {}, ...kids) => {
  const e = Object.assign(document.createElement(tag), props);
  for (const k of kids) e.append(k);
  return e;
};

let dev = null;            /* {link, input, info, ncc, lib, parts, up, smp, bank} */
let busy = false;
const ui = { tab: "parts", cat: 0, ep: 0, search: "", dx: null, loops: [] };

function say(text, kind = "") { const m = $("msg"); m.textContent = text; m.className = "msg " + kind; }
const rq = (r, opt = {}) => dev.link.request(r, opt);

/* ------------------------------------------------------------------ the type rules (ncc_library.c nl_cat) --- */
const KW = [["BEAT", 6], ["BREAK", 6], ["LOOP", 6], ["DRUM", 6], ["GROOVE", 6],
  ["BASS", 1], ["SUB", 1], ["FRETLESS", 1],
  ["PAD", 2], ["STR", 2], ["CHOIR", 2], ["SWEEP", 2], ["WIND", 2], ["ATMO", 2], ["VOX", 2], ["VOICE", 2],
  ["LEAD", 3], ["HOOVER", 3], ["SCREAM", 3], ["RAVE", 3], ["WIRE", 3], ["SYNC", 3], ["SOLO", 3], ["BRASS", 3],
  ["BRS", 3], ["HORN", 3], ["RESO", 3], ["SYNBRASS", 3], ["SYNTHBRASS", 3], ["HARMONICA", 3], ["FLUTE", 3],
  ["EP", 4], ["KEY", 4], ["TINE", 4], ["ORGAN", 4], ["POLY", 4], ["PIANO", 4], ["RHODE", 4], ["CLAV", 4],
  ["PLUCK", 5], ["BELL", 5], ["MARIMBA", 5], ["CHIME", 5], ["HARP", 5], ["MALLET", 5], ["VIBE", 5], ["GLOCK", 5],
  ["KALIMBA", 5], ["GUITAR", 5], ["GTR", 5], ["KOTO", 5], ["GLOK", 5], ["CELESTE", 5], ["SITAR", 5],
  ["303", 1], ["ACID", 1]];
function typeOf(name) {
  const s = String(name).toUpperCase();
  for (const [kw, c] of KW)
    for (let i = 0; i < s.length; i++)
      if ((!i || !/[A-Z0-9]/.test(s[i - 1])) && s.startsWith(kw, i)) return c;
  return 7;                                          /* OTHER */
}

/* ------------------------------------------------------------------ connect --- */
async function connect() {
  if (dev) { disconnect(); return; }
  if (!MOCK && !navigator.requestMIDIAccess) { say("This browser has no Web MIDI: use Chrome or Edge.", "bad"); return; }
  $("connect").disabled = true;
  say("Connecting…");
  try {
    const access = MOCK ? mockAccess() : await navigator.requestMIDIAccess({ sysex: true });
    const isFm1 = (p) => /felucca|fm-?1|x0x|ncc/i.test(p.name || "") && p.state !== "disconnected";
    const input = [...access.inputs.values()].find(isFm1), output = [...access.outputs.values()].find(isFm1);
    if (!input || !output) { say("No FM-1 found. Is it connected by USB and switched on?", "bad"); return; }
    const link = new Link((d) => output.send(d), { timeout: 400 });
    input.onmidimessage = (e) => link.receive(e.data);
    if (input.open) await input.open().catch(() => {});
    dev = { link, input };
    dev.info = parse[CMD.INFO](await rq(req.info(), { timeout: 800 }));
    try { dev.ncc = parseNccInfo(await rq([NCMD.INFO, []], { timeout: 800, retries: 0 })); }
    catch (e) { dev.ncc = null; }
    $("connect").textContent = "Disconnect";
    $("status").textContent = dev.ncc ? `Connected: NCC on ${dev.info.version.replace(/^FELUCCA /, "Felucca ")}${MOCK ? " (simulated)" : ""}`
      : `Connected to ${dev.info.version}, but it is not NCC firmware: only Backup and DX7 work.`;
    await refreshAll();
    say(dev.ncc ? "Connected." : "This FM-1 does not run NCC.", dev.ncc ? "ok" : "bad");
  } catch (e) {
    console.error(e);
    disconnect();
    say(e.name === "SecurityError" ? "Access to MIDI was not allowed." : "Could not talk to the FM-1: " + e.message, "bad");
  } finally {
    $("connect").disabled = false;
  }
}
function disconnect() {
  if (dev) { dev.link.close(); dev.input.onmidimessage = null; }
  dev = null;
  $("connect").textContent = "Connect";
  $("status").textContent = "Not connected.";
  renderAll();
}

/* ------------------------------------------------------------------ NCC replies --- */
function parseNccInfo(a) {
  const r = new Reader(a), o = { name: r.s(), version: r.s(), types: [], engines: [] };
  const nt = r.b();
  for (let i = 0; i < nt; i++) o.types.push(r.s());
  const ne = r.b();
  for (let i = 0; i < ne; i++) o.engines.push({ id: r.b(), name: r.s() });
  return o;
}
async function loadLib() {
  const lib = [];
  for (let start = 0, total = 1; start < total; start += 16) {
    const r = new Reader(await rq([NCMD.LIB, [...v14enc(start), 16]]));
    r.v(); const n = r.b(); total = r.v();
    for (let i = 0; i < n; i++) lib.push({ i: start + i, kind: r.b(), eng: r.b(), cat: r.b(), name: r.s() });
    if (!n) break;
  }
  dev.lib = lib;
}
async function loadParts() {
  const r = new Reader(await rq([NCMD.PARTS, []]));
  const n = r.b(), parts = [];
  for (let i = 0; i < n; i++)
    parts.push({ entry: r.v(), cat: r.b(), ep: r.b(), ch: r.b(), eng: r.b(), name: r.s() });
  dev.parts = parts;
  dev.split = r.b(); dev.clock = r.b(); dev.bpm = r.b() | r.b() << 7;
}
async function loadMine() {
  const up = [];
  for (let start = 0; start < 32; start += 16) up.push(...parse[CMD.UP_LIST](await rq(req.upList(start, 16))).slots);
  dev.up = up;
}
async function loadSmp() {
  try { dev.smp = parse[CMD.SMP_INFO](await rq(req.smpInfo())); } catch (e) { dev.smp = null; }
}
async function loadBank() {
  try {
    const r = new Reader(await rq([FM6B.LIST, []], { timeout: 1500, retries: 0 }));
    const n = r.b(), has = r.b(), name = r.s(), voices = [];
    for (let i = 0; i < n; i++) voices.push({ used: r.b(), name: r.s() });
    dev.bank = { has, name, voices };
  } catch (e) { dev.bank = null; }
}
async function refreshAll() {
  if (!dev) return;
  if (dev.ncc) { await loadLib(); await loadParts(); await loadMine(); }
  await loadSmp();
  await loadBank();
  renderAll();
}
/* run a device action (one at a time), then re-read what it changed */
async function act(what, fn, after = []) {
  if (!dev || busy) return;
  busy = true;
  document.body.style.cursor = "progress";
  try {
    await fn();
    for (const f of after) await f();
    renderAll();
  } catch (e) {
    console.error(e);
    say(`${what} failed: ${e.message}`, "bad");
    busy = false; document.body.style.cursor = ""; return;
  }
  busy = false; document.body.style.cursor = "";
}

/* ------------------------------------------------------------------ render --- */
function renderAll() { renderParts(); renderSounds(); renderMine(); renderDx(); renderLoops(); renderSettings(); }
const engName = (id) => (dev?.ncc?.engines.find((e) => e.id === id)?.name) || (dev?.info?.engines[id]) || "?";
const typeName = (c) => (dev?.ncc?.types[c]) || ["ALL", "BASS", "PAD", "LEAD", "KEYS", "PLUCK", "BEAT", "OTHER"][c] || "?";

function renderParts() {
  const box = $("partcards");
  box.replaceChildren();
  if (!dev?.parts) { box.append(el("p", { className: "hint", textContent: "Connect to see the parts." })); return; }
  dev.parts.forEach((p, i) => {
    const filt = `${typeName(p.cat)} / ${p.ep ? dev.ncc.engines[p.ep - 1]?.name : "ALL"}`;
    box.append(el("div", { className: "part " + PART[i] },
      el("div", { className: "tag", textContent: `${PART[i]} · ${CH_NAMES[p.ch]}` }),
      el("div", { className: "name", textContent: p.name || "-" }),
      el("div", {}, el("span", { className: "badge", textContent: engName(p.eng) }), " ",
        el("span", { className: "badge", textContent: "filter " + filt }))));
  });
}
function renderSounds() {
  const tc = $("typeChips"), ec = $("engChips"), rows = $("soundRows");
  tc.replaceChildren(); ec.replaceChildren(); rows.replaceChildren();
  if (!dev?.lib) { rows.append(el("tr", {}, el("td", { colSpan: 4, textContent: dev ? "This FM-1 does not run NCC." : "Connect to see the sounds." }))); return; }
  dev.ncc.types.forEach((t, c) => {
    if (c && !dev.lib.some((s) => s.cat === c)) return;
    tc.append(el("button", { textContent: t, ariaPressed: String(ui.cat === c), onclick: () => { ui.cat = c; renderSounds(); } }));
  });
  [{ id: -1, name: "ALL" }, ...dev.ncc.engines].forEach((e, k) =>
    ec.append(el("button", { textContent: e.name, ariaPressed: String(ui.ep === k), onclick: () => { ui.ep = k; renderSounds(); } })));
  const q = ui.search.trim().toUpperCase();
  for (const s of dev.lib) {
    if ((ui.cat && s.cat !== ui.cat) || (ui.ep && s.eng !== dev.ncc.engines[ui.ep - 1].id) || (q && !s.name.toUpperCase().includes(q))) continue;
    const mark = dev.parts ? dev.parts.map((p, i) => (p.entry === s.i ? PART[i] : "")).join("") : "";
    rows.append(el("tr", {},
      el("td", { textContent: s.name + (s.kind === 3 ? "  (mine)" : s.kind === 2 ? "  (DX7)" : "") + (mark ? `  ◀ ${mark}` : "") }),
      el("td", { textContent: typeName(s.cat) }), el("td", { textContent: engName(s.eng) }),
      el("td", { className: "act" },
        el("button", { className: "l", textContent: "L", title: "Play it on part L", onclick: () => loadOn(0, s) }), " ",
        el("button", { className: "r", textContent: "R", title: "Play it on part R", onclick: () => loadOn(1, s) }))));
  }
}
function loadOn(part, s) {
  act("Loading", async () => {
    const a = await rq([NCMD.LOAD, [part, ...v14enc(s.i)]], { timeout: 1000 });
    if (a[1]) throw new Error("the FM-1 said no (rc " + a[1] + ")");
    say(`${s.name} on part ${PART[part]}.`, "ok");
  }, [loadParts]);
}
function renderMine() {
  const rows = $("mineRows"), sel = $("storeSlot");
  rows.replaceChildren();
  if (!dev?.up) { rows.append(el("tr", {}, el("td", { colSpan: 4, textContent: "Connect to see your sounds." }))); return; }
  const keep = sel.value;
  sel.replaceChildren(...dev.up.map((u) => el("option", { value: u.slot, textContent: `${u.slot + 1}${u.used ? " · " + u.name : " · (empty)"}` })));
  sel.value = keep || String(dev.up.findIndex((u) => !u.used) >= 0 ? dev.up.findIndex((u) => !u.used) : 0);
  for (const u of dev.up) {
    rows.append(el("tr", {}, el("td", { textContent: u.slot + 1 }), el("td", { textContent: u.used ? u.name : "—" }),
      el("td", { textContent: u.used ? engName(u.engine) : "" }),
      el("td", { className: "act" }, ...(u.used ? [
        el("button", { className: "l", textContent: "L", title: "Load on part L", onclick: () => mineLoad(u, 0) }), " ",
        el("button", { className: "r", textContent: "R", title: "Load on part R", onclick: () => mineLoad(u, 1) }), " ",
        el("button", { textContent: "Rename", onclick: () => mineRename(u) }), " ",
        el("button", { className: "danger", textContent: "Erase", onclick: () => mineErase(u) })] : []))));
  }
}
/* UP_LOAD / UP_STORE act on the selected track: select the part, then give the focus back */
async function onPart(part, fn) {
  const was = parse[CMD.TRACK](await rq(req.track())).sel;
  await rq(req.track(part));
  try { await fn(); } finally { await rq(req.track(was)); }
}
function mineLoad(u, part) {
  act("Loading", () => onPart(part, async () => {
    const a = await rq(req.upLoad(u.slot), { timeout: 1500 });
    if (a[1]) throw new Error("empty slot");
    say(`${u.name} on part ${PART[part]}.`, "ok");
  }), [loadParts]);
}
function mineStore() {
  const part = +$("storePart").value, slot = +$("storeSlot").value, name = upName($("storeName").value || "");
  const old = dev?.up?.[slot];
  if (old?.used && !confirm(`Slot ${slot + 1} holds ${old.name}. Replace it?`)) return;
  act("Storing", () => onPart(part, async () => {
    const a = await rq(req.upStore(slot, $("storeName").value ? name : ""), FLASH_OPT);
    if (a[1]) throw new Error("rc " + a[1]);
    say(`Part ${PART[part]} stored in slot ${slot + 1}.`, "ok");
  }), [loadMine, loadLib]);
}
/* UP_GET's reply as UP_PUT's arguments (the same, less "used"), with another slot / name */
async function upRecord(slot) {
  const a = await rq(req.upGet(slot), { timeout: 1500 });
  if (!a[1]) return null;
  return a.slice(2);
}
function withName(rec, slot, name) {
  const end = rec.indexOf(0, 1);
  return [slot & 127, rec[0], ...strEnc(upName(name)), ...rec.slice(end + 1)];
}
function mineRename(u) {
  const name = prompt("New name (up to 12 characters)", u.name);
  if (!name) return;
  act("Renaming", async () => {
    const rec = await upRecord(u.slot);
    const a = await rq([CMD.UP_PUT, withName(rec, u.slot, name)], FLASH_OPT);
    if (a[1]) throw new Error("rc " + a[1]);
    say(`Renamed to ${upName(name)}.`, "ok");
  }, [loadMine, loadLib]);
}
function mineErase(u) {
  if (!confirm(`Erase ${u.name} from slot ${u.slot + 1}?`)) return;
  act("Erasing", async () => { await rq(req.upErase(u.slot), FLASH_OPT); say(`Slot ${u.slot + 1} is empty.`, "ok"); },
    [loadMine, loadLib]);
}
function mineExport() {
  act("Saving", async () => {
    const slots = [];
    for (const u of dev.up) if (u.used) slots.push({ slot: u.slot, name: u.name, record: await upRecord(u.slot) });
    download(`ncc-my-sounds-${today()}.json`, JSON.stringify({ format: "ncc-my-sounds", version: 1,
      firmware: dev.info.version, pcount: dev.info.pcount, slots }, null, 1));
    say(`${slots.length} sounds saved to a file.`, "ok");
  });
}
function mineImport(file) {
  file.text().then((txt) => {
    const f = JSON.parse(txt);
    if (f.format !== "ncc-my-sounds" || !Array.isArray(f.slots)) throw new Error("not an NCC sounds file");
    if (f.pcount !== dev.info.pcount) throw new Error("made with another firmware version (parameters differ)");
    if (!confirm(`Write ${f.slots.length} sounds into their slots? Sounds in those slots are replaced.`)) return;
    act("Loading the file", async () => {
      for (const s of f.slots) {
        const a = await rq([CMD.UP_PUT, withName(s.record, s.slot, s.name)], FLASH_OPT);
        if (a[1]) throw new Error(`slot ${s.slot + 1}: rc ${a[1]}`);
      }
      say(`${f.slots.length} sounds written.`, "ok");
    }, [loadMine, loadLib]);
  }).catch((e) => say("That file can't be used: " + e.message, "bad"));
}

/* ------------------------------------------------------------------ DX7 --- */
function readSyx(buf) {
  const d = new Uint8Array(buf), i = d.indexOf(0xF0);
  if (i < 0 || d[i + 1] !== 0x43 || d[i + 3] !== 0x09 || d.length < i + 6 + 4096) throw new Error("not a 32-voice DX7 bank");
  const body = d.subarray(i + 6, i + 6 + 4096), voices = [];
  for (let k = 0; k < 32; k++) {
    const rec = body.slice(k * 128, k * 128 + 128).map((b) => b & 127);
    const name = String.fromCharCode(...rec.subarray(118, 128)).replace(/[^\x20-\x7E]/g, " ").trimEnd();
    voices.push({ rec, name });
  }
  return voices;
}
function dxFile(file) {
  file.arrayBuffer().then((buf) => {
    ui.dx = { voices: readSyx(buf), file: file.name };
    $("dxName").value = file.name.replace(/\.syx$/i, "").toUpperCase().replace(/[^\x20-\x7E]/g, "").slice(0, 10);
    renderDx();
  }).catch((e) => say("That file can't be used: " + e.message, "bad"));
}
function renderDx() {
  $("bankNow").textContent = !dev ? "-" : !dev.bank ? "(this firmware has no voice bank)" :
    dev.bank.has ? `${dev.bank.name.trim() || "a bank"}, ${dev.bank.voices.filter((v) => v.used).length} voices` : "no bank yet";
  const box = $("dxPreview");
  box.replaceChildren();
  $("dxSend").disabled = !dev || !ui.dx;
  if (!ui.dx) return;
  const counts = {};
  ui.dx.voices.forEach((v) => { const t = typeName(typeOf(v.name)); counts[t] = (counts[t] || 0) + 1; });
  box.append(el("p", { className: "hint", textContent: `${ui.dx.file}: ` + Object.entries(counts).map(([t, n]) => `${n} ${t}`).join(", ") }),
    el("div", { className: "list" }, el("table", {}, el("tbody", {}, ...ui.dx.voices.map((v, k) =>
      el("tr", {}, el("td", { textContent: `B${k + 1}` }), el("td", { textContent: v.name }),
        el("td", { textContent: typeName(typeOf(v.name)) })))))));
}
function dxSend() {
  if (!ui.dx || !confirm("Replace the FM-1's DX7 bank with this one?")) return;
  const prog = $("dxProg");
  act("Sending the bank", async () => {
    prog.hidden = false; prog.value = 0;
    let a = await rq([FM6B.BEGIN, []], { timeout: 2500, retries: 0 });
    if (a[0]) throw new Error("begin rc " + a[0]);
    const all = new Uint8Array(32 * 128);
    for (let k = 0; k < 32; k++) {
      a = await rq([FM6B.WRITE, [k, ...ui.dx.voices[k].rec]], { timeout: 1500, retries: 1 });
      if (a[1]) throw new Error(`voice ${k + 1} rc ${a[1]}`);
      all.set(ui.dx.voices[k].rec, k * 128);
      prog.value = (k + 1) / 33;
    }
    const nm = ($("dxName").value || "BANK").toUpperCase().replace(/[^\x20-\x7E]/g, "").padEnd(10, " ").slice(0, 10);
    const c = crc32(all) >>> 0;
    a = await rq([FM6B.END, [...Array.from(nm, (ch) => ch.charCodeAt(0)), ...[0, 7, 14, 21, 28].map((s) => (c >>> s) & 127)]],
      { timeout: 3000, retries: 0 });
    if (a[0]) throw new Error("end rc " + a[0]);
    prog.value = 1;
    say(`Bank ${nm.trim()} is on the FM-1: its voices are in Sounds under FM6.`, "ok");
  }, [loadBank, ...(dev?.ncc ? [loadLib] : [])]).finally(() => { prog.hidden = true; });
}

/* ------------------------------------------------------------------ loops --- */
async function lpFiles(files) {
  for (const f of files) {
    try {
      const w = parseWav(await f.arrayBuffer());
      const s = Float64Array.from(resample(w.x, w.sr, SMP.RATE));
      ui.loops.push({ name: f.name.replace(/\.wav$/i, "").toUpperCase().slice(0, 12), raw: s, bpm: guessBpm(s.length) });
    } catch (e) { say(`${f.name}: ${e.message}`, "bad"); }
  }
  renderLoops();
}
/* the tempo at which the file is 1, 2 or 4 bars (the one in 70..180 BPM) */
function guessBpm(n) {
  for (const bars of [1, 2, 4, 8]) {
    const bpm = 60 * 4 * bars * SMP.RATE / n;
    if (bpm >= 70 && bpm <= 180) return Math.round(bpm * 10) / 10;
  }
  return 120;
}
/* one bar of loop l at its BPM, as int16 at SMP.RATE (editor's takeSample: a 2 ms fade at the cut, normalized) */
function lpBar(l) {
  return takeSample(l.raw, 0, Math.min(l.raw.length, Math.round(SMP.RATE * 60 * 4 / Math.max(40, l.bpm))));
}
function lpBuilt() {
  if (!ui.loops.length) return null;
  try { return buildSlot($("lpName").value || "LOOPS", ui.loops.map((l, k) => ({ s: lpBar(l), root: 60 + k }))); }
  catch (e) { return { error: e.message }; }
}
function renderLoops() {
  const info = $("slotInfo");
  info.replaceChildren();
  if (dev?.smp) dev.smp.slots.forEach((s, k) => info.append(el("span", { className: "badge",
    textContent: `USR${k + 1}: ${s.zones ? `${s.name} · ${s.zones} zone${s.zones > 1 ? "s" : ""} · ${s.kib} KiB` : "empty"}` })));
  const rows = $("lpRows");
  rows.replaceChildren();
  ui.loops.forEach((l, k) => {
    const bpm = el("input", { type: "number", min: 40, max: 300, step: 0.1, value: l.bpm, style: "width:6em",
      onchange: () => { l.bpm = +bpm.value || 120; renderLoops(); } });
    rows.append(el("tr", {}, el("td", { textContent: l.name }), el("td", {}, bpm),
      el("td", { textContent: `${(lpBar(l).length / SMP.RATE).toFixed(2)} s` }),
      el("td", { className: "act" }, el("button", { textContent: "▶", title: "Listen to the bar", onclick: () => play(lpBar(l)) }), " ",
        el("button", { className: "danger", textContent: "✕", onclick: () => { ui.loops.splice(k, 1); renderLoops(); } }))));
  });
  const b = lpBuilt();
  $("lpSize").textContent = !b ? "" : b.error ? "Too much for one slot: " + b.error : `${(b.data.length / 1024).toFixed(1)} KiB of ${(SMP.MAX_DATA / 1024).toFixed(0)}`;
  $("lpSend").disabled = !dev || !b || !!b.error;
}
let actx = null;
function play(s) {
  actx = actx || new AudioContext();
  const buf = actx.createBuffer(1, s.length, SMP.RATE), ch = buf.getChannelData(0);
  for (let i = 0; i < s.length; i++) ch[i] = s[i] / 32768;
  const src = actx.createBufferSource(); src.buffer = buf; src.connect(actx.destination); src.start();
}
function lpSend() {
  const b = lpBuilt(), k = +$("lpSlot").value;
  if (!b || b.error) return;
  if (dev.smp?.slots[k]?.zones && !confirm(`USR${k + 1} holds ${dev.smp.slots[k].name}. Replace it?`)) return;
  const prog = $("lpProg");
  act("Sending the loops", async () => {
    prog.hidden = false; prog.value = 0;
    let r = parse[CMD.SMP_BEGIN](await rq(req.smpBegin(k), { timeout: 1500, retries: 0 }));
    if (r.rc) throw new Error("begin rc " + r.rc);
    for (let off = 0; off < b.data.length; off += 256) {
      const w = parse[CMD.SMP_WRITE](await rq(req.smpWrite(k, SMP.DATA_OFF + off, b.data.subarray(off, off + 256)), { timeout: 1000, retries: 1 }));
      if (w.rc) throw new Error("write rc " + w.rc);
      prog.value = (off + 256) / b.data.length;
    }
    r = parse[CMD.SMP_END](await rq(req.smpEnd(k, b.hdr), { timeout: 2500, retries: 0 }));
    if (r.rc) throw new Error("end rc " + r.rc);
    say(`${ui.loops.length} loop${ui.loops.length > 1 ? "s" : ""} in USR${k + 1}.`, "ok");
  }, [loadSmp, ...(dev?.ncc ? [loadLib] : [])]).finally(() => { prog.hidden = true; });
}

/* ------------------------------------------------------------------ backup --- */
let bk = null;
async function backupModule() { return bk || (bk = await import("./fm1backup.js")); }
function bkSave() {
  const prog = $("bkProg");
  act("Backup", async () => {
    const m = await backupModule();
    prog.hidden = false;
    const file = await m.captureBackup((r, o) => rq(r, o), dev.info.version, (d, t) => { prog.value = t ? d / t : 0; });
    download(`ncc-backup-${today()}.json`, JSON.stringify(file));
    say("Backup saved.", "ok");
  }).finally(() => { prog.hidden = true; });
}
function bkRestore(file) {
  if (!confirm("Restore this backup? Everything on the FM-1 is replaced by it.")) return;
  const prog = $("bkProg");
  file.text().then((txt) => act("Restore", async () => {
    const m = await backupModule();
    prog.hidden = false;
    await m.restoreBackup((r, o) => rq(r, o), JSON.parse(txt), (d, t) => { prog.value = t ? d / t : 0; });
    say("Backup restored. Switch the FM-1 off and on.", "ok");
  }, [refreshAll])).finally(() => { prog.hidden = true; });
}

/* ------------------------------------------------------------------ settings --- */
function renderSettings() {
  for (const [id, p] of [["chL", 0], ["chR", 1]]) {
    const s = $(id);
    if (!s.options.length) s.append(...CH_NAMES.map((n, v) => el("option", { value: v, textContent: n })));
    s.disabled = !dev?.parts;
    if (dev?.parts) s.value = dev.parts[p].ch;
  }
  $("split").disabled = $("clock").disabled = !dev?.parts;
  if (dev?.parts) { $("split").value = dev.split; $("clock").value = dev.clock; }
  $("bpm").textContent = dev?.parts ? `${dev.bpm} BPM` : "-";
}
function setting(args, text) {
  act("Setting", async () => {
    const a = await rq([NCMD.SET, args]);
    if (a[1]) throw new Error("rc " + a[1]);
    say(text, "ok");
  }, [loadParts]);
}

/* ------------------------------------------------------------------ helpers --- */
function download(name, text) {
  const a = el("a", { href: URL.createObjectURL(new Blob([text], { type: "application/json" })), download: name });
  document.body.append(a); a.click(); a.remove();
}
const today = () => new Date().toISOString().slice(0, 10);
function dropZone(zone, pick, input, onFiles) {
  $(pick).onclick = () => $(input).click();
  $(input).onchange = (e) => { onFiles([...e.target.files]); e.target.value = ""; };
  const z = $(zone);
  z.ondragover = (e) => { e.preventDefault(); z.classList.add("over"); };
  z.ondragleave = () => z.classList.remove("over");
  z.ondrop = (e) => { e.preventDefault(); z.classList.remove("over"); onFiles([...e.dataTransfer.files]); };
}

/* ------------------------------------------------------------------ a simulated FM-1 with NCC (?mock=1) --- */
function mockAccess() {
  const types = ["ALL", "BASS", "PAD", "LEAD", "KEYS", "PLUCK", "BEAT", "OTHER"];
  const engs = [[12, "FM6"], [0, "JUNO"], [2, "PHASE"], [6, "TRIO"], [14, "ACID"], [15, "LOOP"]];
  const lib = [[0, 12, 4, "TINE EP"], [0, 12, 5, "BELL"], [0, 12, 1, "FM BASS"], [1, 0, 2, "JUNO STRINGS"],
    [1, 0, 2, "JUNO PAD"], [1, 0, 4, "FUNK CLAV"], [0, 0, 3, "SAW LEAD"], [1, 2, 4, "PD EP"], [1, 2, 5, "GLASS BELL"],
    [1, 6, 1, "CHIP BASS"], [0, 6, 3, "ARP LEAD"], [0, 14, 1, "ACID 303"], [0, 14, 1, "SQUELCH"],
    [0, 15, 6, "JUNGLE BREAK"], [0, 15, 6, "AMEN-ISH BREAK"], [0, 15, 6, "IDM GLITCH BEAT"]].map(([kind, eng, cat, name]) => ({ kind, eng, cat, name }));
  const up = Array.from({ length: 32 }, (_, k) => (k < 3 ? { used: 1, engine: [12, 0, 15][k], name: ["MY EP", "WARM PAD", "MY BREAK"][k] } : { used: 0, engine: 0, name: "" }));
  const st = { parts: [{ entry: 0, cat: 0, ep: 0, ch: 2 }, { entry: 3, cat: 0, ep: 0, ch: 3 }], split: 1, clock: 1, sel: 0,
    smp: [{ zones: 0, name: "", kib: 0 }, { zones: 0, name: "", kib: 0 }, { zones: 0, name: "", kib: 0 }],
    bank: { has: 0, name: "", voices: Array.from({ length: 32 }, () => ({ used: 0, name: "" })) } };
  const S = strEnc, V = v14enc;
  const input = { name: "NCC mock FM-1", state: "connected", onmidimessage: null };
  const reply = (cmd, args) => setTimeout(() => input.onmidimessage?.({ data: Uint8Array.from(frame(cmd, args)) }), 5);
  const output = { name: "NCC mock FM-1", state: "connected", send(d) {
    const f = unframe(d); if (!f) return;
    const a = f.a, c = f.cmd;
    if (c === CMD.INFO) reply(c, [...S("FELUCCA 1.6.1"), 16, 111, 30, 64, 103, ...Array.from({ length: 16 }, (_, i) => S(["ANALOG", "-", "PHASE", "LOFI", "SAMPLE", "VOICE", "TRIO", "WHEEL", "GRAIN", "PHYS", "DRUM", "NOISE", "FM6", "SLICE", "ACID", "LOOP"][i])).flat(), 4]);
    else if (c === NCMD.INFO) reply(c, [...S("NCC"), ...S("1.6.1"), types.length, ...types.flatMap(S), engs.length, ...engs.flatMap(([id, n]) => [id, ...S(n)])]);
    else if (c === NCMD.LIB) { const s0 = v14dec(a[0], a[1]), n = Math.max(0, Math.min(a[2], lib.length - s0));
      reply(c, [...V(s0), n, ...V(lib.length), ...lib.slice(s0, s0 + n).flatMap((e) => [e.kind, e.eng, e.cat, ...S(e.name)])]); }
    else if (c === NCMD.LOAD) { const i = v14dec(a[1], a[2]); if (i < lib.length) st.parts[a[0]].entry = i; reply(c, [a[0], i < lib.length ? 0 : 1]); }
    else if (c === NCMD.PARTS) reply(c, [2, ...st.parts.flatMap((p) => [...V(p.entry), p.cat, p.ep, p.ch, lib[p.entry].eng, ...S(lib[p.entry].name)]), st.split, st.clock, 120, 0]);
    else if (c === NCMD.SET) { if (a[0] === 0) st.parts[a[1]].ch = a[2]; else if (a[0] === 1) st.split = a[1]; else if (a[0] === 2) st.clock = a[1]; reply(c, [a[0], 0]); }
    else if (c === CMD.UP_LIST) reply(c, [a[0], a[1], 32, ...up.slice(a[0], a[0] + a[1]).flatMap((u) => [u.used, u.engine, ...S(u.name)])]);
    else if (c === CMD.UP_GET) { const u = up[a[0]]; reply(c, [a[0], u.used, u.engine, ...S(u.name), ...Array(111).fill(0).flatMap(() => V(0)), ...Array(32).fill(0)]); }
    else if (c === CMD.UP_PUT) { const e = a.indexOf(0, 2); up[a[0]] = { used: 1, engine: a[1], name: String.fromCharCode(...a.slice(2, e)) }; reply(c, [a[0], 0]); }
    else if (c === CMD.UP_STORE) { const e = a.indexOf(0, 1); up[a[0]] = { used: 1, engine: lib[st.parts[st.sel].entry].eng, name: String.fromCharCode(...a.slice(1, e)) || "STORED" }; reply(c, [a[0], 0]); }
    else if (c === CMD.UP_LOAD || c === CMD.UP_ERASE) { if (c === CMD.UP_ERASE) up[a[0]] = { used: 0, engine: 0, name: "" }; reply(c, [a[0], 0]); }
    else if (c === CMD.TRACK) { if (a.length) st.sel = a[0]; reply(c, [st.sel, 4, ...Array(4).fill(0).flatMap(() => [0, 0, ...V(100), 0, 0])]); }
    else if (c === CMD.SMP_INFO) reply(c, [3, 80, ...st.smp.flatMap((s) => [s.zones, ...S(s.name), s.kib])]);
    else if (c === CMD.SMP_BEGIN) reply(c, [a[0], 0]);
    else if (c === CMD.SMP_WRITE) reply(c, [a[0], a[1], a[2], a[3], 0]);
    else if (c === CMD.SMP_END) { const h = unpack7(a.slice(1)); st.smp[a[0]] = { zones: h[6], name: String.fromCharCode(...h.slice(8, 16)).replace(/\0+$/, ""), kib: 20 }; reply(c, [a[0], 0]); }
    else if (c === FM6B.BEGIN) reply(c, [0]);
    else if (c === FM6B.WRITE) { st.bank.voices[a[0]] = { used: 1, name: String.fromCharCode(...a.slice(119, 129)).trimEnd() }; reply(c, [a[0], 0]); }
    else if (c === FM6B.END) { st.bank.has = 1; st.bank.name = String.fromCharCode(...a.slice(0, 10)); reply(c, [0, 32]); }
    else if (c === FM6B.LIST) reply(c, [32, st.bank.has, ...S(st.bank.name), ...st.bank.voices.flatMap((v) => [v.used, ...S(v.name)])]);
  } };
  return { inputs: new Map([["i", input]]), outputs: new Map([["o", output]]) };
}

/* ------------------------------------------------------------------ wire up --- */
$("connect").onclick = connect;
$("tabs").onclick = (e) => {
  const t = e.target.closest("button")?.dataset.tab;
  if (!t) return;
  ui.tab = t;
  for (const b of $("tabs").children) b.setAttribute("aria-selected", String(b.dataset.tab === t));
  for (const s of document.querySelectorAll("main > section")) s.hidden = s.id !== t;
};
$("partsRefresh").onclick = () => act("Refreshing", async () => {}, [loadParts]);
$("soundSearch").oninput = (e) => { ui.search = e.target.value; renderSounds(); };
$("storeBtn").onclick = mineStore;
$("mineExport").onclick = mineExport;
$("mineImportBtn").onclick = () => $("mineImport").click();
$("mineImport").onchange = (e) => { if (e.target.files[0]) mineImport(e.target.files[0]); e.target.value = ""; };
dropZone("dxDrop", "dxPick", "dxFile", (fs) => fs[0] && dxFile(fs[0]));
$("dxSend").onclick = dxSend;
dropZone("lpDrop", "lpPick", "lpFile", lpFiles);
$("lpName").oninput = renderLoops;
$("lpSend").onclick = lpSend;
$("bkSave").onclick = bkSave;
$("bkLoadBtn").onclick = () => $("bkLoad").click();
$("bkLoad").onchange = (e) => { if (e.target.files[0]) bkRestore(e.target.files[0]); e.target.value = ""; };
$("chL").onchange = (e) => setting([0, 0, +e.target.value], "Part L's channel set.");
$("chR").onchange = (e) => setting([0, 1, +e.target.value], "Part R's channel set.");
$("split").onchange = (e) => setting([1, +e.target.value], +e.target.value ? "SPLIT: L left, R right." : "STEREO.");
$("clock").onchange = (e) => setting([2, +e.target.value], "Clock set.");
renderAll();
if (MOCK) say("Simulated FM-1: press Connect.");
