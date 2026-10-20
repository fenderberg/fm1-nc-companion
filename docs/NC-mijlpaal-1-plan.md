# Mijlpaal 1: plan (nog geen code)

Datum: 20 oktober 2026. Basis: Felucca v1.6.1 (fork `Fenderberg/fm1-nc-companion`, branch `nc/setup`).
Hoort bij `NC-companion-ontwerp.md` (v0.6). Status: **voorstel, wacht op akkoord van Remco.**

## Doel
Build-vlag `FELUCCA_LIVEUI=1`. Zonder de vlag is de firmware bit-voor-bit Felucca (alle tests en goldens gelijk).
Met de vlag:
- een minimale live-UI (`firmware/src/live_ui.c`): twee parts, **L = track 1 (FM6)**, **R = track 2 (ANALOG)**;
  focus met OCT− / OCT+; PRESETS bladert de sounds van het gefocuste part; hoofdscherm met twee kolommen.
- MIDI IN standaard op **TRACK** (`ROUTE_TRACK`): L op kanaal 1, R op kanaal 2 (instelbaar via `song.mch`).
- een macrolaag (`firmware/src/perf_layer.c`): CC 70–77 met slew naar de engine-parameters (tabel hieronder).

## Hoe het in Felucca past (wat ik vond)
- Alles is één compilatie-eenheid (`felucca.c` includet de rest in vaste volgorde). De UI komt als laatste.
- De main loop (`main.c`) roept per frame `ui_input()`, `ui_leds()`, `ui_draw()` aan. Dat zijn de drie haakjes.
- MIDI en CC's worden in de audio-ISR verwerkt (`events_block()` in `seq.c`, per blok van 32 samples).
  CC's gaan via `midi_control()` → `midi_cc_map()` (`midi_control.c`) en zetten `t->p[id]` **direct**.
- FM6's 8 macro's (ALG FB MLVL MRAT MEG VMOD DTUN SLOT) zijn al **offsets op de patch** (0 = de patch zelf).
  "Relatief t.o.v. de preset" is daar dus gratis.
- `apply_preset_to(track, n)` (`ui.c`) laadt een sound op een track, met de bestaande fades: die hergebruik ik.

## Aanpak: oude UI blijft ingebouwd, live-UI neemt de haakjes over
De Felucca-UI blijft gecompileerd (settings, presets, opslag en de MIDI-Learn-tabel hangen eraan), maar met
`FELUCCA_LIVEUI=1` roept de main loop `live_input()` / `live_leds()` / `live_draw()` aan in plaats van de `ui_*`-versies.
Zo is de ingreep klein en blijft de fork mergebaar. Pas als het RAM knelt, gaan delen van de oude UI eruit (later).

## Nieuwe bestanden
| Bestand | Inhoud |
|---|---|
| `firmware/src/perf_layer.c` | Macrotabel per engine, doelwaarde per (part, macro), slew per blok, "laatste waarde wint". Geen UI-code. |
| `firmware/src/live_ui.c` | Focus, PRESETS/ALGORITHM-encoders, twee-kolommen-scherm (alleen gewijzigde stukjes tekenen, via `gfx.c`), LED's van OCT−/OCT+. |
| `tests/perf_test.c` | Hosttest macrolaag (zie Testen). |
| `tests/live_ui_test.c` | Hosttest live-UI met de stubs van `tests/ui_test.c` (knoppen, encoders, scherm). |

Beide nieuwe bronbestanden krijgen `SPDX-License-Identifier: GPL-3.0-only`.

## Upstream-bestanden die ik moet aanraken (klein, gemarkeerd met `/* NC: ... */`)
| Bestand | Wijziging |
|---|---|
| `firmware/src/felucca.c` | `#ifndef FELUCCA_LIVEUI` → `0`; met de vlag `#include "perf_layer.c"` (na `seq.c`) en `#include "live_ui.c"` (na `ui_layer.c`). |
| `firmware/src/main.c` | De drie UI-aanroepen (2 plekken) via `#if FELUCCA_LIVEUI` naar `live_*`. Bij opstart: tracks 1/2 op FM6/ANALOG en ROUTE_TRACK zetten als het project dat nog niet heeft. |
| `firmware/src/midi_control.c` | In `midi_cc_map()`: met de vlag eerst `perf_cc(t, cc, value)`; geeft die 1 terug, dan is de CC van de macrolaag (CC 70–77). MIDI Learn blijft voorgaan. |
| `firmware/src/fx.c` of `seq.c` | Eén aanroep `perf_block()` per audioblok, direct na `events_block(n)` (de slew). |
| `tools/build.py` | `FELUCCA_LIVEUI` doorgeven zoals de andere `FELUCCA_*`-vlaggen (als dat niet al generiek gaat) + de eigen identity (apart voorstel). |
| `tests/run_tests.sh` | De twee nieuwe tests + een compile van de regressie met `-DFELUCCA_LIVEUI=1`. |

Niet aangeraakt: de engines-submodule (`felucca_engine.h`), `voice.c`, de opslagformaten.

## De macrolaag
- **CC-afhandeling (ISR, `events_block`)**: CC 70–77 schrijft alleen de doelwaarde `perf.tgt[part][m]` (0..127).
  Veel CC's in één blok = alleen de laatste telt (gratis, want we zetten alleen het doel).
- **Slew (ISR, `perf_block`, 1× per blok van 32 samples ≈ 0,73 ms)**: `cur += (tgt − cur) >> k` in Q8 (integer, geen
  deling), met k zó gekozen dat ±10 ms het doel bijna bereikt (instelbaar 5–20 ms; standaard 10 ms). Pas als de
  afgeronde waarde verandert, schrijft hij `t->p[id]`. Eén schrijver per variabele: alleen de ISR schrijft `t->p` van
  gemapte parameters zolang een macro actief is.
- **Relatief t.o.v. de preset**: bij laden van een sound (`apply_preset_to`) springen de macro's naar het midden (64)
  en onthoudt de laag de basiswaarde van elke doelparameter. Waarde = basis + (macro − 64) × bereik / 64, geklemd.
  (De deling door 64 is een shift; geen deling door een variabele.)
- **Tabel (const, in flash)**, per engine een rij van 8 macro's → tot 2 doelen elk, met bereik:

| Knop | CC | L: FM6 | R: ANALOG |
|---|---|---|---|
| 1 | 74 | MLVL (P_E2) | CUT (P_E4) |
| 2 | 71 | MRAT (P_E3) — **stapt** (integer) | RES (P_E5) |
| 3 | 70 | FB (P_E1, stapt) + DIST (P_DIST) | ENV→FLT (P_ED_FLT) |
| 4 | 73 | MEG (P_E4) | ATK (P_ATK) |
| 5 | 72 | MEG + REL (P_REL) | DEC + REL (P_DEC, P_REL) |
| 6 | 76 | LFO-diepte (P_LD_PIT/P_LD_AMP; sync bestaat: P_LSYNC) | LFO-snelheid + diepte (P_LRATE, P_LD_FLT) |
| 7 | 77 | DTUN (P_E6) + SPREAD (P_SPRD) | Chorus-send (P_CHOR) — Juno-chorus I/II komt in mijlpaal 2 |
| 8 | 78 | **niet in M1** (arp-verdeling hoort bij mijlpaal 3: arp op het raster) | idem |

Let op: FB en MRAT zijn gehele stappen (−7..7, −16..16). Slew maakt die niet vloeiend; het zorgt alleen dat een snelle
draai niet tien stappen in één blok zet. Of dat hoorbaar klikt, meet ik in de test (en op het apparaat).

- **Standaard-CC-map**: met de vlag nemen de macro's CC 70–77 over (CC 71/72/73/74 zaten in Felucca's vaste map).
  CC 5, 7, 10, 91, 93, 94 blijven zoals Felucca ze doet.

## De live-UI (minimaal)
- **OCT− / OCT+**: focus L / R (LED van de knop aan). Geen octaafwissel meer op die knoppen met de vlag.
  OCT− bij het aanzetten blijft de USB-rescue (dat zit vóór de UI in `main.c`, raak ik niet aan).
- **PRESETS-encoder**: volgende/vorige sound van het gefocuste part; laden na ±150 ms stilstand (zoals Felucca's
  `preset_go`), dus draaien door de lijst klikt niet.
- **ALGORITHM-encoder**: bij FM6 de SLOT (F1–F8, B1–B32); bij ANALOG niets in M1.
- **27 toetsen**: spelen het gefocuste part (via Felucca's toetsenroute naar de geselecteerde track: `song.sel` = focus).
- **Scherm 240×240**: bovenbalk (BPM, ext. klok ja/nee), twee kolommen L/R met engine-naam, sound-naam en een
  MIDI-activiteitsstip; de gefocuste kolom licht, de andere gedimd. Popup "CUT 87" ±1 s bij een NC-macro.
  Alleen tekenen wat verandert (vlaggen per veld), nooit een heel scherm per frame.
- Alles anders (EDIT, GLO, SAVE, FX, ARP-pagina's) doet in M1 **niets**. Opslaan gebeurt via Felucca's autosave
  (alleen als het stil is).

## Testen
1. **Bestaande suite zonder vlag**: `sh tests/run_tests.sh` moet volledig groen blijven, goldens en CPU-budget
   ongewijzigd (de vlag staat standaard uit, dus dat moet vanzelf).
2. **`tests/perf_test.c`** (nieuw, host):
   - stap van 0 → 127 op CC74: de parameter loopt monotoon op, geen sprong groter dan X per blok, doel bereikt
     binnen 10 ms ± 1 blok;
   - 100 CC's in één blok → alleen de laatste telt;
   - sound laden → macro's terug naar midden, waarde = preset;
   - geklemd aan het bereik van elke parameter (ook bipolaire FM6-macro's);
   - render-check: een FM6- en ANALOG-noot met een macro-sweep geeft geen klik (max. sample-sprong onder een drempel),
     met de bestaande render-helpers van `tests/regress.c`.
3. **`tests/live_ui_test.c`** (nieuw): OCT−/OCT+ wisselt focus en LED; PRESETS laadt op het gefocuste part en niet op
   het andere; TRACK-routing: noot op kanaal 1 → track 1, kanaal 2 → track 2; UI-render-lint van het nieuwe scherm
   (geen tekst buiten beeld), zoals `tests/ui_render.c` dat voor de oude schermen doet.
4. **CPU**: `perf_block` kost per blok max. 16 vergelijkingen + een paar schrijfacties; ik voeg het toe aan de
   CPU-meting van de regressie (budget +25 % mag niet geraakt worden; verwacht < 1 %).
5. **Firmwarebuild** (cloud, want lokaal geen Docker): build met en zonder `FELUCCA_LIVEUI=1`, size-report vergelijken.
6. **Op het apparaat** (alleen na jouw akkoord, jij flasht): testlijst — rescue werkt nog, focus, presets, NC op
   kanaal 1/2, macro's draaien zonder zipper, geen hangende noten.

## Wat het kost aan geheugen (schatting, te bevestigen met het size-report)
- **RAM (.data+.bss)**: macrolaag ±100 B (per part 8 × doel/huidig/basis + een paar vlaggen), live-UI ±100 B
  (focus, popup-timer, "wat is vuil"-vlaggen). Totaal **< 0,5 KB** van de ±12,5 KB vrij. De oude UI blijft, dus
  daar winnen we nog niets terug.
- **Pool**: 0 (geen buffers).
- **Flash/image**: ±4–8 KB code + tabellen (image nu 511 KB; de app-slot heeft ruimte, check in het size-report).
- **CPU**: verwaarloosbaar (zie boven).

## Risico's / open punten
- FB en MRAT stappen (integer): mogelijk hoorbaar bij snel draaien. Alternatief: die twee alleen bij een nieuwe noot
  laten ingaan. Beslissen na de test.
- Felucca's autosave bewaart `t->p` mét macro-offset. Voor M1 acceptabel; later basis en offset apart bewaren.
- `beat_samples()`-guard (deling door BPM 0) hoort bij mijlpaal 3, maar is één regel: mag ik die meteen meenemen?
- De standaard MIDI-kanalen van de NC-tracks (1 en 2?) nog bevestigen.
