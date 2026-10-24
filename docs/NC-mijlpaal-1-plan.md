# NCC mijlpaal 1: plan (nog geen code)

**Historisch document.** Dit bewaart het ontwerp en de teststand van de vermelde mijlpaal; oude bediening en
“nog niet geflasht” zijn geen actuele status. Voor de huidige firmware: [handleiding](MANUAL.md),
[overdracht/status](HANDOFF.md), [Dual/Drone](NC-dual-drone-plan.md) en [extra engines](NC-extra-engines-macros.md).

Datum: 20 oktober 2026. Basis: Felucca v1.6.1 (fork `Fenderberg/fm1-nc-companion`, branch `nc/setup`).
Hoort bij `NC-companion-ontwerp.md`. Status: **v2, akkoord van Remco (20-10-2026); code af, gebouwd voor de chip en getest (host). Nog niet geflasht.** Zie "Stand van zaken" onderaan.

Besluiten van Remco (20-10-2026):
- De firmware heet **NCC** (Novation Circuit Companion).
- De arp komt later (niet in mijlpaal 1).
- De 8 macro's volgen zoveel mogelijk de **paneellabels van de Circuit Tracks**.
- De twee engines luisteren standaard op **kanaal 3 en 4** (de MIDI 1- en MIDI 2-track van de Circuit Tracks).

## Doel
Build-vlag `FELUCCA_LIVEUI=1`. Zonder de vlag is de firmware bit-voor-bit Felucca (alle tests en goldens gelijk).
Met de vlag:
- een minimale live-UI (`firmware/src/live_ui.c`): twee parts, **L = track 1 (FM6)**, **R = track 2 (ANALOG)**;
  focus met OCT− / OCT+; PRESETS bladert de sounds van het gefocuste part; hoofdscherm met twee kolommen; "NCC" op het
  opstartscherm.
- MIDI IN standaard op **TRACK** (`ROUTE_TRACK`): **L op kanaal 3, R op kanaal 4** (instelbaar via `song.mch`).
- een macrolaag (`firmware/src/perf_layer.c`): de 8 macro-CC's met slew naar de engine-parameters (tabel hieronder).

## Package identity en naam
- Identity **`FM-1_700`** (dev) en **`FM-1_7XY`** voor releases (v1.0 → `FM-1_710`). Al gezet in `tools/build.py`.
  - "FM-1" vóór de `_` moet blijven: beide installers weigeren een pakket waarvan het model niet gelijk is aan dat
    van het apparaat. Het nummer is vrij (officieel 0xx, Felucca 9xx). De Firmware Hub kijkt naar de SHA-256, niet
    naar de identity. `web/make_site.py` accepteert nu ook 7xx.
- Release-bestanden heten `ncc-X.Y.fwsc` en `ncc-X.Y-app.bin`. De dev-build blijft `build/felucca.fwsc` (de tests en
  scripts zoeken die naam).

## Wat de Circuit Tracks stuurt (uit de officiële handleiding, v3)
- Kanalen af fabriek: Synth 1 = 1, Synth 2 = 2, **MIDI 1 = 3, MIDI 2 = 4**, Drums = 10.
- Een MIDI-track stuurt met de 8 macroknoppen CC's volgens het gekozen **template** (8 per track, te bewerken in
  Components). Af fabriek: **CC 1, 2, 5, 11, 12, 13, 71, 74** voor knop 1..8, op het kanaal van die track.
- De paneellabels van de 8 knoppen: **1 Oscillator, 2 Oscillator Mod, 3 Amp Envelope, 4 Filter Envelope,
  5 Filter Frequency, 6 Resonance, 7 Modulation, 8 FX**.

Dus: met het fabriekstemplate werkt NCC meteen, zonder Components. In Felucca betekenen CC 1 (modwiel), 5 (glide),
11 (expressie/volume), 71 en 74 iets anders; met de vlag vangt de macrolaag deze 8 CC's op kanaal 3/4 af **vóór**
Felucca's eigen afhandeling. (Optioneel later: een eigen template met andere CC's, instelbaar in GLO.)

## De 8 macro's (volgens de Circuit-labels)
| Knop | Label NC | CC | L: FM6 | R: ANALOG |
|---|---|---|---|---|
| 1 | Oscillator | 1 | MRAT: ratio's van de modulators (stapt) | MIX: balans osc 1 / osc 2 |
| 2 | Oscillator Mod | 2 | FB: feedback (stapt) | DTN: detune osc 2 |
| 3 | Amp Envelope | 5 | **nieuw: CEG** (tijden van de carrier-envelopes), zie hieronder | ATK + REL samen langer |
| 4 | Filter Envelope | 11 | MEG: tijden van de modulator-envelopes (= de "filter-envelope" van FM) | ENV→FLT (P_ED_FLT) |
| 5 | Filter Frequency | 12 | MLVL: level van alle modulators (= helderheid; Felucca stuurt CC74 hier ook al heen) | CUT |
| 6 | Resonance | 13 | geen echte tegenhanger; voorstel **DTUN** (carriers uit elkaar: zweving, "glans") | RES |
| 7 | Modulation | 71 | LFO-diepte naar helderheid + vibrato (P_LD_FLT, P_LD_PIT) | LFO-diepte + snelheid (P_LD_FLT, P_LRATE) |
| 8 | FX | 74 | Drive + chorus-send (P_DIST, P_CHOR) | Drive + chorus-send (P_DRV/P_DIST, P_CHOR) |

Opmerkingen:
- **Amp Envelope bij FM6**: bij FM6 doen de gewone ADSR-knoppen niets; de envelopes van de operators zijn de
  amplitude. Er is wel een macro voor de modulators (MEG), niet voor de carriers. Voorstel: een kleine 9e
  "macro" CEG in `eng_fm6.c` (zelfde rekenwijze als MEG, op de carriers). Is dat te veel voor M1, dan doet knop 3
  bij FM6 in M1 nog niets.
- **FB en MRAT** zijn hele stappen. Slew maakt ze niet vloeiend, maar voorkomt tien stappen in één blok. Of het
  klikt, meet de test; zo nodig gaan die twee pas in bij een nieuwe noot.
- **CPU**: FM6 rekent zijn effectieve patch opnieuw uit in de ISR als een macro verandert. Tijdens het glijden
  gebeurt dat elk blok. Dat meet ik apart (worst case: alle macro's tegelijk draaien).
- **Relatief t.o.v. de preset**: FM6's macro's zijn al offsets op de patch. Bij ANALOG onthoudt de laag de
  presetwaarde en telt de macro erbij op. Bij laden van een sound springen alle macro's naar het midden.
  Let op: de Circuit stuurt absolute waarden (0..127) en weet niet dat wij resetten; de eerste draai na een
  presetwissel springt dus naar de knopstand. Opvangen met **"pick-up"** (pas volgen als de knop het midden
  passeert) of gewoon laten springen (met slew): keuze voor na de eerste test.

## Hoe het in Felucca past
- Alles is één compilatie-eenheid (`felucca.c`). De main loop (`main.c`) roept per frame `ui_input()`, `ui_leds()`,
  `ui_draw()` aan: dat zijn de haakjes voor de live-UI.
- MIDI en CC's worden in de audio-ISR verwerkt (`events_block()` in `seq.c`, per blok van 32 samples).
- `apply_preset_to(track, n)` (`ui.c`) laadt een sound met de bestaande fades: die hergebruik ik.
- De oude Felucca-UI blijft gecompileerd (settings, presets, opslag en MIDI Learn hangen eraan); met de vlag roept
  de main loop `live_input()` / `live_leds()` / `live_draw()` aan.

## Nieuwe bestanden
| Bestand | Inhoud |
|---|---|
| `firmware/src/perf_layer.c` | Macrotabel per engine, doel per (part, macro), slew per blok, laatste waarde wint. |
| `firmware/src/live_ui.c` | Focus, encoders, twee-kolommen-scherm (alleen gewijzigde stukjes, via `gfx.c`), LED's. |
| `tests/perf_test.c` | Hosttest macrolaag. |
| `tests/live_ui_test.c` | Hosttest live-UI met de stubs van `tests/ui_test.c`. |

## Upstream-bestanden die ik aanraak (klein, gemarkeerd met `/* NC: ... */`)
| Bestand | Wijziging |
|---|---|
| `firmware/src/felucca.c` | `FELUCCA_LIVEUI` (standaard 0); met de vlag `perf_layer.c` en `live_ui.c` includen. |
| `firmware/src/main.c` | De UI-aanroepen via `#if FELUCCA_LIVEUI` naar `live_*`; bij opstart tracks 1/2 = FM6/ANALOG, ROUTE_TRACK, kanalen 3/4 als het project nog niets heeft. |
| `firmware/src/midi_control.c` | Met de vlag in `midi_control()`: eerst `perf_cc(ch, cc, value)`; 1 = van de macrolaag. |
| `firmware/src/fx.c` | Eén aanroep `perf_block()` per audioblok, direct na `events_block(n)`. |
| `firmware/src/eng_fm6.c` | Alleen als je CEG wilt: een carrier-envelope-offset naast MEG. |
| `tools/build.py` | `FELUCCA_LIVEUI` aan de lijst met doorgegeven vlaggen (identity/naam: al gedaan). |
| `tests/run_tests.sh` | De twee nieuwe tests + de regressie ook met `-DFELUCCA_LIVEUI=1`. |

## De live-UI (minimaal)
- **OCT− / OCT+**: focus L / R (LED). OCT− bij het aanzetten blijft de USB-rescue (zit vóór de UI, blijft onaangeroerd).
- **PRESETS**: volgende/vorige sound van het gefocuste part, laden na ±150 ms stilstand (geen klikken bij doordraaien).
- **ALGORITHM**: bij FM6 de SLOT (F1–F8, B1–B32); bij ANALOG niets in M1.
- **27 toetsen**: spelen het gefocuste part.
- **Scherm**: bovenbalk (NCC, BPM, externe klok), twee kolommen L/R met engine, sound-naam, kanaal en MIDI-stip;
  focus licht, de andere gedimd. Popup met het **Circuit-label** + waarde ("FILTER FREQ 87") ±1 s bij een macro.
- De rest (EDIT, GLO, SAVE, FX, ARP) doet in M1 niets. Opslaan via Felucca's autosave (alleen als het stil is).

## Testen
1. Bestaande suite zonder vlag: volledig groen, goldens en CPU-budget ongewijzigd.
2. `tests/perf_test.c`: stap 0→127 loopt monotoon zonder grote sprongen en is binnen 10 ms ± 1 blok bij het doel;
   100 CC's in één blok → de laatste telt; sound laden → midden, waarde = preset; klemmen; render-check zonder klik
   (FM6 en ANALOG, sweep van elke macro); CC 1/5/11 op kanaal 3/4 doen niets meer met modwiel/glide/volume.
3. `tests/live_ui_test.c`: focus en LED; PRESETS op het gefocuste part; noot op kanaal 3 → L, kanaal 4 → R,
   kanaal 1/2 genegeerd; render-lint van het nieuwe scherm.
4. CPU: `perf_block` en de FM6-herberekening bij glijden in de regressiemeting.
5. Firmwarebuild (cloud) met en zonder de vlag; size-report vergelijken.
6. Op het apparaat (alleen na jouw akkoord, jij flasht): rescue, focus, presets, Circuit op kanaal 3/4, macro's
   zonder zipper, geen hangende noten.

## Geheugen (schatting, te bevestigen met het size-report)
- RAM: < 0,5 KB (van ±12,5 KB vrij). Pool: 0. Flash: ±4–8 KB. CPU: klein, behalve mogelijk FM6 tijdens glijden (meten).

## Besloten (Remco, 20-10-2026)
1. FM6 knop 3: **CEG toevoegen** in M1 (carrier-envelopes, naast MEG in `eng_fm6.c`).
2. FM6 knop 6: **DTUN**.
3. Na een presetwissel: **springen met glijden** (geen pick-up).
4. De `beat_samples()`-guard: **nu meenemen**.

## Stand van zaken (20-10-2026, eind van de dag)
Gebouwd (branch `nc/setup`, lokaal, niet gepusht):
- `firmware/src/perf_layer.c`: de macrolaag (CC 1 2 5 11 71 74 12 13 op part L/R, slew ±10 ms, relatief, reset bij
  nieuwe sound, snelle route als een knop stilstaat).
- `firmware/src/live_ui.c`: focus (OCT−/OCT+), PRESETS met pauze, ALGORITHM = FM6-SLOT, scherm met twee kolommen,
  popup met het Circuit-label, opstart: FM6/ANALOG, TRACK op kanaal 3/4, tracks 3/4 stil, geen patronen.
- `eng_fm6.c`: CEG (carrier-envelopes) achter de vlag. `fx.c`: BPM-guard (altijd, ook zonder vlag).
- Haakjes (`/* NC: */`): `felucca.c` (vlag + include), `main.c` (live_* i.p.v. ui_*), `fx.c` (include + `ncm_block`),
  `midi_control.c` (`ncm_cc` vóór `mod_midi`), `ui_draw.c` ("NCC" op het opstartscherm, Felucca en Hügelton blijven
  vermeld), `tools/build.py` (vlag), `tools/size_fns.py` (live_ui.c compact).
- `tests/ncc_test.c` + twee regels in `tests/run_tests.sh` (de test, en de regressie met de vlag).

Getest op de Mac (hosttests):
- `tests/ncc_test.c`: alles groen, ook met UBSan (ASan start niet op deze Mac: bekend macOS-probleem).
- Regressie zonder én met de vlag: 98 golden renders, 0 gewijzigd. CPU in rust: 334 i.p.v. 326 instructies/sample
  (+2,5 %). Twee CPU-posten staan op de Mac al boven budget in **ongewijzigd** Felucca (Apple-compiler; in de cloud
  op Linux opnieuw kijken).
- `ui_test`, `fm6_test`, `midi_control_test` zonder vlag: groen. Met de vlag faalt `midi_control_test` op 14 checks
  die Felucca's eigen CC-betekenissen (CC 1/5/11/71/74 op track 1/2, MIDI Learn met CC74) testen: dat is precies wat
  NCC bewust vervangt. Die test draait in `run_tests.sh` daarom alleen zonder vlag (zoals upstream).
- Zoveel mogelijk van de volledige `tests/run_tests.sh` lokaal (98 testgroepen, zonder de stappen die een firmwarebuild
  vragen): groen op drie na, die alle drie ook in **ongewijzigd** Felucca op deze Mac falen: M-UPGRADE (vraagt
  `build/felucca.fwsc` en de SDK), de letterafstand (fonts gegenereerd met de Mac-Pillow) en de PERFORM-kosten van
  FLANGER/PHASER (Apple-compiler; upstream-fx.c geeft hetzelfde getal).
- Syntax-check van de hele firmware (`felucca.c`) met en zonder vlag: in orde. **Nog geen echte build** (geen Docker
  op de Mac; cloudbuild nodig) en dus nog geen size-report.

Afwijkingen van het plan:
- Eén testbestand (`tests/ncc_test.c`) i.p.v. twee.
- De macro-CC's gaan vóór MIDI Learn én vóór Felucca's modwiel/expressie (CC 1/11 werden al vóór `midi_control`
  gebruikt, door `mod_midi`). MIDI Learn is in de live-UI toch niet bereikbaar.
- OCT+ heeft in Felucca's knoppenkaart geen LED. Focus R = OCT− donker (het scherm toont de focus met een balk).

Open punten voor de cloudbuild / het apparaat:
1. Echte build met `FELUCCA_LIVEUI=1` en zonder; size-report (RAM, pool, image) vergelijken.
2. CPU op de chip. Op de Mac gemeten: de macrolaag zelf kost in rust ±0,7 % van een blok, met alle 16 knoppen
   glijdend ±4,6 % (FM6 herrekent zijn patch tijdens het glijden nu maar elk 4e blok: gedaan). Een render met alle
   knoppen wild gedraaid is ±16–20 % duurder, maar dat is de klank zelf (feller, meer modulatie, drive/chorus aan),
   niet de laag. Op de chip nog meten met Felucca's CPU-meter.
3. De volledige `tests/run_tests.sh` (ook UI-renders, target-budget) in de cloud.
4. Op het apparaat: rescue, focus, PRESETS, Circuit op kanaal 3/4, macro's zonder zipper, FB/MRAT-stappen beluisteren.

## Echte build (20-10-2026, op de Mac via Colima + Rosetta)
| | Felucca (zonder vlag) | NCC (`FELUCCA_LIVEUI=1`) | Verschil |
|---|---|---|---|
| Image | 511.136 B | 387.428 B | −123,7 KB |
| RAM (.data+.bss) | 60.984 van 73.728 B | 58.696 B | −2.288 B (15 KB vrij) |
| Pool | 328.896 van 368.640 B | 325.632 B | −3.264 B |
| Identity | FM-1_700 | FM-1_700 | |

De build zonder vlag is exact gelijk aan Felucca v1.6.1 (zelfde toolchain). NCC is kleiner: de linker gooit de oude
UI-code weg die niet meer wordt aangeroepen (`size: 412 main-loop functions ... 500 not in this build`). Alle
chip-controles van `build.py` slagen (register-toegang alleen in hal/, .ram_text zonder calls, .c1_text).

Volledige `tests/run_tests.sh` tegen de echte build: 100 testgroepen. Groen, behalve twee die ook in ongewijzigd
Felucca op deze Mac falen (letterafstand met Mac-fonts; PERFORM FLANGER/PHASER-kosten met de Apple-compiler). Eén
webtest eiste `FM-1_9xx` als identity: aangepast naar `FM-1_[79]xx`. Doellussen op de chip (target budget): binnen
budget. M-UPGRADE (eigen loader): groen.

Bouwen op deze Mac, zonder build.sh (macOS haalt `DYLD_LIBRARY_PATH` weg zodra `/bin/sh` start):
```
colima start --vm-type vz --vz-rosetta        # (eenmalig per herstart)
DYLD_LIBRARY_PATH=/opt/homebrew/lib JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK \
  FELUCCA_LIVEUI=1 /Library/Frameworks/Python.framework/Versions/3.13/bin/python3 tools/build.py
```
Emulator met NCC: `EMU_CFLAGS=-DFELUCCA_LIVEUI=1 sh web/emu/build.sh`, dan `build/emu` serveren op localhost
(bijv. `/usr/bin/python3 -m http.server 8766 -d <kopie buiten Dropbox>`) en openen in Chrome (Web MIDI voor de
Circuit). In de emulator gecontroleerd: scherm, OCT−/OCT+ met LED, PRESETS (TINE EP → BELL), ALGORITHM (F1 → F2).
