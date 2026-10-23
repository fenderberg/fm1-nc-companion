#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""NC: NCC's site (GitHub Pages, the gh-pages branch): Felucca's site (web/make_site.py: the web installer, the
package and its licences, Felucca's editor) made NCC's, plus the NCC Manager and the browser emulator.

  index.html                  NCC's front page: install, manage, try
  webapp/installer/           the web installer (one click, Chrome or Edge, USB), NCC's words; Felucca's credits kept
  webapp/manager/             the NCC Manager (tools/ncc_manager.sh's build)
  webapp/try/                 the emulator with NCC and its demo Circuit (tools/ncc_demo.sh with DEMO=1)
  webapp/editor/              Felucca's editor (as make_site.py puts it)
  firmware/                   the package, LICENSE, LICENSING.md, LICENSES/

  tools/ncc_site.py build/ncc-X.Y.fwsc X.Y OUT_DIR

Needs the emulator's build (Emscripten) and build/gen. The page texts are English.
"""
import html
import re
import shutil
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
REPO = "https://github.com/fenderberg/fm1-nc-companion"


def sub1(text, old, new):
    """old must be there exactly once (the installer changed: update this script)"""
    if text.count(old) != 1:
        raise SystemExit(f"ncc_site: {old[:60]!r} is not in the installer once")
    return text.replace(old, new)


# the installer in the site's purple / pink (the front page's, the manager's): after Felucca's own style, so it wins
INSTALLER_CSS = """<style>
  :root, :root[data-theme="dark"], :root:not([data-theme="light"]) { --bg: #cbbbe0; --fg: #fdf0ff; --dim: #e2d4f3; }
  :root { --panel: #6a4c9c; --tray: #523a7e; --card: #7c5fae; --edge: #e39ad8; --ink: #2a1d40; color-scheme: light; }
  body { background: #cbbbe0; color: var(--ink); }
  main { max-width: 640px; padding: 24px 16px 40px; }
  main > * { position: relative; }
  .ncc-card { background: var(--panel); color: var(--fg); border-radius: 22px; padding: 22px 24px; }
  .ncc-back { display: inline-block; margin: 0 0 12px; color: #4b3a63; text-decoration: none; font-size: 14px; }
  .ncc-back:hover { text-decoration: underline; }
  h1 { font-size: 30px; margin: 0 0 8px; color: var(--fg); }
  .lang { color: var(--dim); }
  .small, pre { color: var(--dim); }
  a { color: inherit; }
  button { background: var(--card); color: var(--fg); border: 1px solid var(--edge); border-radius: 10px; padding: 8px 18px; }
  button:hover:not(:disabled) { filter: brightness(1.12); }
  #go { background: var(--edge); color: var(--ink); border-color: var(--edge); font-size: 18px; font-weight: 600;
        padding: 12px 34px; border-radius: 12px; }
  .lang { border: 0; background: none; }
  details { background: var(--tray); border-radius: 14px; padding: 10px 14px; }
  summary { cursor: pointer; }
  input[type=file] { color: var(--dim); }
  progress { accent-color: var(--edge); height: 8px; border-radius: 4px; }
  #status { font-weight: 600; }
</style>
"""


def installer(path, version):
    t = path.read_text(encoding="utf-8")
    t = sub1(t, "</head>", INSTALLER_CSS + "</head>")
    t = sub1(t, "<main>\n  <button class=\"lang\" id=\"lang\"></button>",
             '<main>\n  <a class="ncc-back" href="../../">← NCC</a>\n  <div class="ncc-card">\n  <button class="lang" id="lang"></button>')
    t = sub1(t, "</main>\n<script type=\"module\">", "  </div>\n</main>\n<script type=\"module\">")
    t = sub1(t, "<title>Felucca</title>", "<title>NCC installer</title>")
    t = sub1(t, "<h1>Felucca</h1>", f"<h1>NCC {html.escape(version)}</h1>\n  <p>Novation Circuit Companion: firmware that turns "
             "the M-VAVE FM-1 into a two-part synth for the Novation Circuit Tracks. Built on Felucca.</p>")
    t = sub1(t, 'beta: "Felucca 1.0 is a field-testing release; please report problems on GitHub.',
             'beta: "NCC is an early release built on Felucca; please report problems on GitHub (fenderberg/fm1-nc-companion).')
    t = sub1(t, 'editor: "Editor (after installing: edit sounds and sequences from the computer)"',
             'editor: "NCC Manager (after installing: sounds, your own sounds, DX7 banks, loops and backups from the computer)"')
    t = sub1(t, '<a href="../editor/"', '<a href="../manager/"')
    t = sub1(t, "say(`Felucca ${meta.version}`);", "say(`NCC ${meta.version}`);")
    t = sub1(t, "a.download = `Felucca-backup-", "a.download = `NCC-backup-")
    for old, new in (("Older firmware needs a Felucca update for complete backups. To restore an archive later, reinstall "
                      "Felucca and use the editor.", "Older firmware needs an NCC update for complete backups. To restore "
                      "an archive later, reinstall NCC and use the NCC Manager."),
                     ("Official V15 cannot open Felucca music.", "Official V15 cannot open NCC's music."),
                     ("This Felucca does not support complete backups. Install the current Felucca first.",
                      "This firmware does not support complete backups. Install the current NCC first.")):
        t = sub1(t, old, new)
    t = sub1(t, '<span data-t="license"></span> <a href="https://github.com/hugelton/Felucca"',
             f'<span data-t="license"></span> <a href="{REPO}" target="_blank" rel="noopener" style="color:inherit">'
             'github.com/fenderberg/fm1-nc-companion</a> · Felucca: <a href="https://github.com/hugelton/Felucca"')
    path.write_text(t, encoding="utf-8")


FRONT = """<!doctype html>
<!-- SPDX-License-Identifier: GPL-3.0-only -->
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>NCC: Novation Circuit Companion</title>
<meta name="description" content="Free firmware that turns the M-VAVE FM-1 into a two-part synth for the Novation Circuit Tracks.">
<style>
:root { --bg: #cbbbe0; --panel: #6a4c9c; --tray: #523a7e; --edge: #e39ad8; --text: #fdf0ff; --dim: #e2d4f3; --ink: #2a1d40; }
* { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--ink); font: 16px/1.55 -apple-system, BlinkMacSystemFont, "Helvetica Neue", Arial, sans-serif; }
main { max-width: 980px; margin: 0 auto; padding: 24px 16px 40px; }
.hero { background: var(--panel); color: var(--text); border-radius: 22px; padding: 26px 24px; display: grid; gap: 18px;
        grid-template-columns: 1.1fr 1fr; align-items: center; }
.hero h1 { margin: 0 0 6px; font-size: 34px; letter-spacing: .01em; }
.hero p { margin: 0 0 10px; color: var(--dim); }
.hero img { width: 100%; border-radius: 14px; display: block; }
.cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); gap: 14px; margin: 18px 0; }
a.card { display: block; background: var(--panel); color: var(--text); text-decoration: none; border-radius: 18px; padding: 18px;
         border: 2px solid transparent; }
a.card:hover { border-color: var(--edge); }
a.card strong { display: block; font-size: 21px; margin-bottom: 4px; }
a.card span { color: var(--dim); }
ul { padding-left: 20px; }
.box { background: var(--panel); color: var(--text); border-radius: 18px; padding: 16px 20px; }
.box a, footer a { color: inherit; }
footer { font-size: 13px; color: #4b3a63; margin-top: 18px; }
@media (max-width: 720px) { .hero { grid-template-columns: 1fr; } .hero h1 { font-size: 28px; } }
</style></head><body><main>
<div class="hero">
  <div><h1>NCC</h1>
    <p><strong>Novation Circuit Companion.</strong> Free firmware that turns the M-VAVE FM-1 into a two-part synth for the
      Novation Circuit Tracks: part L on its MIDI 1 track, part R on MIDI 2, split to its two audio inputs, every sound
      hands-on from the Circuit's eight knobs.</p>
    <p>Six engines (DX7-style FM, Juno, phase distortion, chip, a 303 and one-bar drum loops), sounds picked by role,
      a clock-locked arp, Drone, effects one tap away. Version @VERSION@.</p></div>
  <img src="ncc-emulator.jpg" alt="NCC on a purple FM-1, in the browser emulator">
</div>
<div class="cards">
  <a class="card" href="webapp/installer/"><strong>Install</strong><span>One click in Chrome or Edge, over USB. Back to the
    stock firmware any time.</span></a>
  <a class="card" href="webapp/manager/"><strong>Manage</strong><span>Sounds on L and R, your own sounds, DX7 banks, drum
    loops, backups and settings, from your computer.</span></a>
  <a class="card" href="webapp/try/"><strong>Try it</strong><span>The FM-1 in your browser with a built-in Circuit
    playing: hear NCC before you install.</span></a>
</div>
<div class="box">
  <p>Read the <a href="@REPO@/blob/main/docs/MANUAL.md">manual</a> · get the
    <a href="@REPO@/releases">releases</a> · see the <a href="@REPO@">source</a>.</p>
  <p>Installing firmware is at your own risk. NCC is built on <a href="https://github.com/hugelton/Felucca">Felucca</a> by
    Leo Kuroshita (Hügelton Instruments).</p>
</div>
<footer>GPL-3.0. Novation and Circuit Tracks are trademarks of Focusrite Audio Engineering Ltd; M-VAVE and FM-1 are
  trademarks of their respective owners. NCC is not affiliated with or endorsed by any of them.</footer>
</main></body></html>
"""


def main(pkg, version, out):
    out = Path(out)
    if out.exists():
        shutil.rmtree(out)
    subprocess.run([sys.executable, str(SRC / "web" / "make_site.py"), pkg, version, str(out)], check=True)
    for old in (out / "firmware").glob("felucca-*.fwsc"):   # (make_site's name) -> ncc-X.Y.fwsc
        new = old.with_name("ncc-" + old.name[len("felucca-"):])
        old.rename(new)
        inst = out / "webapp" / "installer" / "index.html"
        inst.write_text(inst.read_text(encoding="utf-8").replace("firmware/" + old.name, "firmware/" + new.name),
                        encoding="utf-8")
    installer(out / "webapp" / "installer" / "index.html", version)
    env = {"NO_SERVE": "1", "PATH": __import__("os").environ["PATH"]}
    subprocess.run(["sh", str(SRC / "tools" / "ncc_manager.sh")], check=True, env=env)
    shutil.copytree(SRC / "build" / "ncc-manager", out / "webapp" / "manager")
    subprocess.run(["sh", str(SRC / "tools" / "ncc_demo.sh")], check=True, env={**env, "DEMO": "1"})
    shutil.copytree(SRC / "build" / "emu-demo", out / "webapp" / "try")
    shutil.copy(SRC / "docs" / "ncc-emulator.jpg", out / "ncc-emulator.jpg")
    (out / "index.html").write_text(FRONT.replace("@VERSION@", html.escape(version)).replace("@REPO@", REPO),
                                    encoding="utf-8")
    (out / ".nojekyll").write_text("")
    print(f"ncc_site: {out}")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
