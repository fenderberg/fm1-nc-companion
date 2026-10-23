#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""NC: NCC's site (GitHub Pages, the gh-pages branch), dark like a Circuit Tracks (web/site/style.css), every page with
the same bar: Home, Manual, Install, Manager, Try it.

  index.html                  the front page: the story, the features with the emulator's pictures, a video, the FAQ,
                              what's new (CHANGELOG.md)
  manual/index.html           docs/MANUAL.md as a page, with its contents on the side
  webapp/installer/           Felucca's web installer (web/make_site.py) in NCC's words and style; Felucca's credits kept
  webapp/manager/             the NCC Manager (tools/ncc_manager.sh's build), in the site's style
  webapp/try/                 the emulator with NCC and its demo Circuit (tools/ncc_demo.sh, DEMO=1): the purple FM-1
  webapp/editor/              Felucca's editor (as make_site.py puts it; not in the bar)
  firmware/                   the package, LICENSE, LICENSING.md, LICENSES/
  style.css, img/             web/site's

  tools/ncc_site.py build/ncc-X.Y.fwsc X.Y OUT_DIR

Needs the emulator's build (Emscripten) and build/gen. The pages are English.
"""
import html
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
REPO = "https://github.com/fenderberg/fm1-nc-companion"
VIDEO = ""                              # a YouTube embed URL once there is a demo video ("" = the placeholder)
PAGES = [("", "Home"), ("manual/", "Manual"), ("webapp/installer/", "Install"), ("webapp/manager/", "Manager"),
         ("webapp/try/", "Try it")]


def nav(root, cur):
    links = "".join(f'<a href="{root}{p}"{" aria-current=page" if p == cur else ""}>{n}</a>' for p, n in PAGES)
    return (f'<header class="ncc-nav"><div class="in"><a class="brand" href="{root}"><i>{"<b></b>" * 8}</i>NCC</a>'
            f'<nav>{links}<a href="{REPO}">GitHub</a></nav></div></header>\n')


def foot():
    return ('<footer class="ncc-foot"><div class="in">NCC is free software (GPL-3.0), built on '
            '<a href="https://github.com/hugelton/Felucca">Felucca</a> by Leo Kuroshita (Hügelton Instruments). '
            'Novation and Circuit Tracks are trademarks of Focusrite Audio Engineering Ltd; M-VAVE and FM-1 are trademarks '
            'of their respective owners. NCC is not affiliated with or endorsed by any of them. Installing firmware is at '
            'your own risk.</div></footer>\n')


def page(root, cur, title, body, desc=""):
    return (f'<!doctype html>\n<!-- SPDX-License-Identifier: GPL-3.0-only -->\n<html lang="en"><head><meta charset="utf-8">'
            f'<meta name="viewport" content="width=device-width, initial-scale=1"><title>{html.escape(title)}</title>'
            f'<meta name="description" content="{html.escape(desc)}"><link rel="stylesheet" href="{root}style.css">'
            f'</head><body>\n{nav(root, cur)}<main class="wrap">\n{body}\n</main>\n{foot()}</body></html>\n')


# ------------------------------------------------------------------------------------------- a little Markdown ---
def slug(s):
    return re.sub(r"[^a-z0-9 -]", "", s.lower()).strip().replace(" ", "-")


def inline(s, rel):
    s = html.escape(s, quote=False)
    s = re.sub(r"`([^`]+)`", lambda m: "<code>" + m.group(1) + "</code>", s)
    s = re.sub(r"!\[([^\]]*)\]\(([^)]+)\)", lambda m: f'<img src="{rel(m.group(2))}" alt="{m.group(1)}">', s)
    s = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", lambda m: f'<a href="{rel(m.group(2))}">{m.group(1)}</a>', s)
    s = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", s)
    return s


LIST = re.compile(r"\s*([-*]|\d+\.) ")


def markdown(text, rel):
    """the subset docs/MANUAL.md and CHANGELOG.md use: headings, paragraphs, lists (with a code block in an item),
    tables, code blocks, images, links, code, bold -> (html, [(level, id, title)])"""
    out, toc, lines, i = [], [], text.split("\n"), 0
    while i < len(lines):
        ln = lines[i]
        if ln.startswith("```"):
            j = i + 1
            while j < len(lines) and not lines[j].startswith("```"):
                j += 1
            out.append("<pre><code>" + html.escape("\n".join(lines[i + 1:j])) + "</code></pre>")
            i = j + 1
        elif m := re.match(r"(#{1,3}) (.*)", ln):
            lvl, t = len(m.group(1)), m.group(2)
            hid = slug(t)
            toc.append((lvl, hid, t))
            out.append(f'<h{lvl} id="{hid}">{inline(t, rel)}</h{lvl}>')
            i += 1
        elif ln.startswith("|"):
            rows = []
            while i < len(lines) and lines[i].startswith("|"):
                rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")])
                i += 1
            head, body = rows[0], [r for r in rows[1:] if not all(re.fullmatch(r":?-+:?", c) for c in r)]
            out.append("<table><thead><tr>" + "".join(f"<th>{inline(c, rel)}</th>" for c in head) + "</tr></thead><tbody>"
                       + "".join("<tr>" + "".join(f"<td>{inline(c, rel)}</td>" for c in r) + "</tr>" for r in body)
                       + "</tbody></table>")
        elif LIST.match(ln):
            tag = "ol" if re.match(r"\s*\d+\. ", ln) else "ul"
            items = []
            while i < len(lines) and (LIST.match(lines[i]) or (items and lines[i].startswith("  ")) or
                                      (items and not lines[i].strip() and i + 1 < len(lines) and lines[i + 1].startswith("   "))):
                if LIST.match(lines[i]):
                    items.append([LIST.sub("", lines[i], count=1), ""])
                elif lines[i].strip().startswith("```"):     # a code block inside an item
                    j = i + 1
                    while j < len(lines) and not lines[j].strip().startswith("```"):
                        j += 1
                    items[-1][1] = "\n".join(x.strip() for x in lines[i + 1:j])
                    i = j
                elif lines[i].strip():
                    items[-1][0] += " " + lines[i].strip()
                i += 1
            out.append(f"<{tag}>" + "".join("<li>" + inline(t, rel) + (f"<pre><code>{html.escape(c)}</code></pre>" if c else "")
                                            + "</li>" for t, c in items) + f"</{tag}>")
        elif ln.strip():
            para = []
            while i < len(lines) and lines[i].strip() and not re.match(r"(#{1,3} |\||```)", lines[i]) and not LIST.match(lines[i]):
                para.append(lines[i].strip())
                i += 1
            out.append("<p>" + inline(" ".join(para), rel) + "</p>")
        else:
            i += 1
    return "\n".join(out), toc


# ------------------------------------------------------------------------------------------------- the pages ---
def home(version, changelog):
    img = "img/"
    feats = [
        ("green", "Two parts, two tracks", "Part L listens to the Circuit's MIDI 1 track, part R to MIDI 2. The Circuit "
         "plays the notes and sends the clock; NCC has no sequencer of its own.", ""),
        ("blue", "Split to its two inputs", "L comes out on the left, R on the right: mix, mute and effect each part on the "
         "Circuit itself.", ""),
        ("pink", "The Circuit's knobs just work", "Its eight macros are mapped to what their labels mean on every engine, "
         "smoothly, and the screen shows what each one does.", "screen-sound.png"),
        ("amber", "Sounds by role", "SELECT picks the type (bass, pad, lead, keys, pluck, beat), ALGORITHM the engine, "
         "PRESETS walks through what's left. 88 sounds, and your own.", ""),
        ("cyan", "Six engines", "DX7-style FM (loads real DX7 banks), Juno with its chorus, phase distortion, a chip synth, "
         "a 303, and drum loops.", ""),
        ("red", "Drum loops that fit the bar", "Any note opens a one-bar loop where the bar is; the Circuit's steps chop it. "
         "Shuffle, reverse, stutter, crush and a DJ filter on the knobs.", "screen-loop.png"),
        ("purple", "Built for jamming", "OCT- / OCT+ switch between your two sounds, an arp locked to the clock with HOLD, "
         "and Drone to keep a pad ringing while you play over it.", "screen-arp.png"),
        ("green", "Effects one tap away", "Sends, delay, reverb, chorus, an insert and performance effects on the keys, plus "
         "echo and reverb throws.", "screen-sends.png"),
        ("blue", "NCC Manager", "A web page over USB: sounds on L and R, your own sounds, DX7 banks, your drum loops, "
         "backups and settings.", ""),
    ]
    cards = "\n".join(f'<div class="card" style="--c:var(--{c})"><h3>{t}</h3><p>{d}</p></div>' for c, t, d, _ in feats)
    shots = "\n".join(f'<figure><img src="{img}{f}" alt="{html.escape(cap)}"><figcaption>{cap}</figcaption></figure>'
                       for f, cap in (("screen-sound.png", "SOUND: both parts, their filters, a Circuit knob's popup"),
                                      ("screen-loop.png", "A drum loop on part L (type BEAT, engine LOOP)"),
                                      ("screen-arp.png", "The arp, locked to the Circuit's clock"),
                                      ("screen-sends.png", "FX: one tap and you're on the sends")))
    faq = [
        ("What do I need?", "An M-VAVE FM-1, a Novation Circuit Tracks, a way to get the Circuit's MIDI to the FM-1's USB "
         "(a USB MIDI host adapter, or a computer passing it on), and a stereo-to-two-mono cable into the Circuit's audio "
         "inputs."),
        ("Does it work without a Circuit Tracks?", "Yes. The FM-1's keys play the parts, the arp runs on its own tempo, and "
         "any MIDI keyboard or sequencer on channels 3 and 4 works too. Or try it in your browser first."),
        ("How do I go back to the original firmware?", "On the Install page, open “Return to official V15” and pick M-VAVE's "
         "FM-1.fwsc, or use M-VAVE's own updater. Your music is backed up first."),
        ("Which browser?", "Chrome or Edge on a computer. The installer and the manager need Web MIDI with SysEx, which "
         "Safari and Firefox don't offer."),
        ("My loop runs out of time with the Circuit.", "Press Stop and Start on the Circuit once: NCC counts its bar from the "
         "Circuit's Start. Since 0.3 it follows the Circuit's clock pulses, so it can't drift away afterwards."),
        ("Can I use my own sounds and loops?", "Yes: send DX7 banks and your own one-bar WAV drum loops with the manager, "
         "and store your own sounds in the 32 user slots."),
        ("Is it safe?", "It installs like M-VAVE's own updates, and the way back is always there. Still, installing "
         "firmware is at your own risk; if an install is cut off, run it again (hold OCT- while switching on for the "
         "update mode)."),
        ("What is Felucca?", "Felucca is Leo Kuroshita's open-source firmware for the FM-1. NCC is a fork of it, focused on "
         "being the Circuit Tracks' companion. Both are free software (GPL-3.0)."),
    ]
    faqs = "\n".join(f"<details><summary>{q}</summary><p>{a}</p></details>" for q, a in faq)
    log, _ = markdown(re.sub(r"\A# .*\n", "", changelog), lambda u: u)
    log = re.sub(r'<h2 id="[^"]*">([^<]*)</h2>', r'<h3><span class="pad lit green">\1</span></h3>', log)
    video = (f'<div class="video"><iframe src="{VIDEO}" title="NCC demo" allowfullscreen></iframe></div>' if VIDEO else
             '<div class="video"><div><div class="label">Demo video</div><p>Coming soon. Meanwhile, hear it in your '
             'browser.</p><a class="pad lit pink" href="webapp/try/">Try it</a></div></div>')
    grid = "".join(f'<span style="--i:{i % 16}"></span>' for i in range(32))
    body = f"""
<section class="hero">
  <div>
    <h1>NCC<small>Novation Circuit Companion</small></h1>
    <p class="lead">Free firmware that turns the M-VAVE FM-1 into a two-part synth for the Novation Circuit Tracks:
      every sound hands-on from the Circuit's eight knobs.</p>
    <div class="btns"><a class="pad lit green" href="webapp/installer/">Install {html.escape(version)}</a>
      <a class="pad lit pink" href="webapp/try/">Try it in your browser</a><a class="pad" href="manual/">Read the manual</a></div>
    <div class="grid8" aria-hidden="true">{grid}</div>
  </div>
  <img class="shot" src="{img}fm1-purple.png" alt="NCC on a purple FM-1, in the browser emulator">
</section>

<h2><span class="label">Why</span>The Circuit's spare pair</h2>
<div class="two">
  <p class="lead">The Circuit Tracks has two synth tracks inside, plus two MIDI tracks and two audio inputs that
    often sit unused. That spare pair is an open invitation: plug in a second instrument and give each MIDI track its
    own sound.</p>
  <p class="lead">The FM-1 is small, cheap and runs Felucca, a brilliant multi-engine firmware, but Felucca is a whole
    groovebox. NCC strips it down to one job: being the Circuit's companion. Less to learn, more to play.</p>
</div>

<h2><span class="label">What it does</span>Features</h2>
<div class="cards">
{cards}
</div>

<h2><span class="label">On the screen</span>The FM-1 in Circuit style</h2>
<div class="shots">
{shots}
</div>

<h2><span class="label">Watch</span>See it play</h2>
{video}

<h2><span class="label">Questions</span>FAQ</h2>
{faqs}

<h2><span class="label">Releases</span>What's new</h2>
<div class="case log">{log}<p><a href="{REPO}/releases">All releases on GitHub</a></p></div>
"""
    return page("", "", "NCC: Novation Circuit Companion", body,
                "Free firmware that turns the M-VAVE FM-1 into a two-part synth for the Novation Circuit Tracks.")


def manual(text):
    def rel(u):
        if u.startswith(("http", "#")):
            return u
        if u == "ncc-emulator.jpg":                     # (the site's picture of the FM-1: cut out, the page dark)
            return "../img/fm1-purple.png"
        if u.endswith((".jpg", ".png")):
            return "../img/" + Path(u).name
        return f"{REPO}/blob/main/{os.path.normpath('docs/' + u)}"
    text = re.sub(r"(?m)^- \[[^\]]+\]\(#[^)]+\)\n", "", text)   # (its own contents list: the side has one)
    body, toc = markdown(text, rel)
    side = "".join(f'<a href="#{i}"{" class=sub" if lvl == 3 else ""}>{html.escape(t)}</a>' for lvl, i, t in toc if lvl > 1)
    return page("../", "manual/", "NCC manual", f'<div class="manual"><aside class="toc"><div class="label">Contents</div>'
                f'{side}</aside><article class="doc">{body}</article></div>', "The NCC manual: hooking up, sounds, loops, "
                "the Circuit's knobs, arp, Drone, effects, the manager, installing.")


# --------------------------------------------------------------------------------------- the apps, in the style ---
APP_CSS = """<style>
  html, body { background: #0d0d0f !important; }
  /* the bar as on the site's pages, whatever the app's own header / nav / button rules say */
  header.ncc-nav { display: block; position: sticky; top: 0; margin: 0; padding: 0; border: 0; border-radius: 0;
                   border-bottom: 1px solid #2c2c31; background: rgba(13, 13, 15, .92); color: #ececf0;
                   font: 16px/1.6 -apple-system, BlinkMacSystemFont, "Helvetica Neue", Arial, sans-serif; }
  header.ncc-nav, header.ncc-nav * { box-sizing: border-box; }
  header.ncc-nav .in { max-width: 1100px; margin: 0 auto; padding: 10px 16px; display: flex; align-items: center;
                       justify-content: flex-start; gap: 18px; flex-wrap: wrap; }
  header.ncc-nav nav { display: flex; gap: 4px; margin: 0 0 0 auto; padding: 0; background: none; border: 0; }
</style>
"""
INSTALLER_CSS = """<style>
  :root, :root[data-theme="dark"], :root:not([data-theme="light"]) { --bg: #0d0d0f; --fg: #ececf0; --dim: #9a9aa6; }
  body { background: #0d0d0f; }
  main { max-width: 680px; padding: 28px 16px 48px; }
  .ncc-card { background: linear-gradient(180deg, #202024, #18181b); border: 1px solid #2c2c31; border-radius: 18px;
              padding: 24px 26px; }
  h1 { font-size: 34px; margin: 0 0 8px; letter-spacing: .04em; }
  a { color: inherit; }
  button { background: #202024; color: #ececf0; border: 1px solid #2c2c31; border-radius: 9px; padding: 8px 18px; }
  button:hover:not(:disabled) { filter: brightness(1.15); }
  #go { background: #3ee07a; color: #08080a; border: 0; font-size: 18px; font-weight: 700; padding: 12px 36px;
        box-shadow: 0 0 24px -4px #3ee07a; }
  .lang { border: 0; background: none; }
  details { background: #141417; border: 1px solid #2c2c31; border-radius: 12px; padding: 10px 14px; }
  summary { cursor: pointer; }
  progress { accent-color: #3ee07a; height: 8px; }
  #status { font-weight: 600; }
</style>
"""
MANAGER_CSS = """<style>
  :root { --bg: #0d0d0f; --panel: #18181b; --tray: #202024; --card: #2a2a30; --edge: #3a3a42; --text: #ececf0;
          --dim: #9a9aa6; --ink: #08080a; --l: #3aa4ff; --r: #ff5fd2; color-scheme: dark; }
  html, body { background: #0d0d0f; color: #ececf0; }
  main > header, main > section { border: 1px solid #2c2c31; background: linear-gradient(180deg, #202024, #18181b); }
  nav#tabs button[aria-selected="true"], .chips button[aria-pressed="true"] { background: #3ee07a; color: #08080a;
          border-color: #3ee07a; box-shadow: 0 0 16px -4px #3ee07a; }
  main > footer { color: #5c5c66; }
  .msg.ok { color: #3ee07a; } .msg.bad { color: #ff4d5e; }
</style>
"""
TRY_CSS = """<style>
  :root { --bg: #0d0d0f; --fg: #ececf0; }
  main { color: #9a9aa6; }
  main a { color: #ececf0; }
</style>
"""


def sub1(text, old, new):
    """old must be there exactly once (the page changed: update this script)"""
    if text.count(old) != 1:
        raise SystemExit(f"ncc_site: {old[:60]!r} is not in the page once")
    return text.replace(old, new)


def installer(path, version):
    t = path.read_text(encoding="utf-8")
    t = sub1(t, "</head>", '<link rel="stylesheet" href="../../style.css">' + INSTALLER_CSS + APP_CSS + "</head>")
    t = sub1(t, "<body>", "<body>\n" + nav("../../", "webapp/installer/"))
    t = sub1(t, "<main>\n  <button class=\"lang\" id=\"lang\"></button>",
             '<main>\n  <div class="ncc-card">\n  <button class="lang" id="lang"></button>')
    t = sub1(t, "</main>\n<script type=\"module\">", "  </div>\n</main>\n<script type=\"module\">")
    t = sub1(t, "<title>Felucca</title>", "<title>Install NCC</title>")
    t = sub1(t, "<h1>Felucca</h1>", f"<h1>Install NCC {html.escape(version)}</h1>\n  <p>Novation Circuit Companion: firmware "
             "that turns the M-VAVE FM-1 into a two-part synth for the Novation Circuit Tracks. Built on Felucca.</p>")
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


def themed(path, cur, css):
    """an app's page (the manager, the emulator): the site's bar and dark page around it"""
    t = path.read_text(encoding="utf-8")
    t = sub1(t, "</head>", '<link rel="stylesheet" href="../../style.css">' + css + APP_CSS + "</head>")
    t = sub1(t, "<body>", "<body>\n" + nav("../../", cur))
    path.write_text(t, encoding="utf-8")


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
    env = {"NO_SERVE": "1", "PATH": os.environ["PATH"]}
    subprocess.run(["sh", str(SRC / "tools" / "ncc_manager.sh")], check=True, env=env)
    shutil.copytree(SRC / "build" / "ncc-manager", out / "webapp" / "manager")
    themed(out / "webapp" / "manager" / "index.html", "webapp/manager/", MANAGER_CSS)
    subprocess.run(["sh", str(SRC / "tools" / "ncc_demo.sh")], check=True, env={**env, "DEMO": "1"})
    shutil.copytree(SRC / "build" / "emu-demo", out / "webapp" / "try")
    themed(out / "webapp" / "try" / "index.html", "webapp/try/", TRY_CSS)
    shutil.copy(SRC / "web" / "site" / "style.css", out / "style.css")
    shutil.copytree(SRC / "web" / "site" / "img", out / "img")
    shutil.copy(SRC / "docs" / "ncc-emulator.jpg", out / "img" / "ncc-emulator.jpg")
    (out / "index.html").write_text(home(version, (SRC / "CHANGELOG.md").read_text(encoding="utf-8")), encoding="utf-8")
    (out / "manual").mkdir()
    (out / "manual" / "index.html").write_text(manual((SRC / "docs" / "MANUAL.md").read_text(encoding="utf-8")),
                                               encoding="utf-8")
    (out / ".nojekyll").write_text("")
    print(f"ncc_site: {out}")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
