# NC-Companion: verkenning van Felucca (stap 0)

Datum: 20 oktober 2026. Onderzocht: hugelton/Felucca `main` (laatste commit 16-10-2026, versie v1.6.1) met submodule Felucca-Engines.
Hoort bij: `NC-companion-ontwerp.md` (v0.6).

## Conclusie in het kort
- **Felucca als basis is bevestigd.** Plan B (Dinghy) is niet nodig.
- De interface is een **aparte laag bovenop** de rest van de code, en daardoor goed te vervangen.
- **Veel van ons ontwerp bestaat al**: FM6 met DX7-banks en macro's, een arp met hold, MIDI-klok, Program Change, MIDI Learn en effect-throws.
- **Wat we zelf moeten bouwen**:
  - de live-interface
  - een arp die altijd op het raster van de klok valt
  - drone-hold
  - een macrolaag met smoothing
  - Juno-chorus/-klanken
  - de split-uitgang
- **Grootste beperking: het geheugen zit bijna vol.** Daarom kan split niet met twee eigen delays.

## 1. Bouwen werkt
- De JieLi-toolchain plus de drie SDK-bestanden (uboot.boot, cfg_tool.bin, cfg/eq_cfg_hw.bin, hashes gecontroleerd) bouwen op Linux x86-64 zonder Docker. Docker is alleen nodig op macOS.
- Resultaat: `felucca.fwsc` (610 KB), identity FM-1_900.
- **Geheugen**: image 511 KB; RAM 60.984 van 73.728 B (± 12,5 KB vrij); pool 328.896 van 368.640 B (± 39 KB vrij).
- Tests: zie §7.

## 2. Hoe de code in elkaar zit
- Eén compilatie-eenheid (`firmware/src/felucca.c`) in vaste volgorde: platform → engines → voices → FX → USB/MIDI → sequencer/arp (`seq.c`) → audio → **UI als laatste** (`panel.c`, `ui*.c`, `menu_items.c`, `icons.c`).
- De UI zit los van de klank- en MIDI-kern: de kern praat met de UI via een paar pointers (`mpc_io`, `ml_tab`). Een build zonder UI is in de code al voorzien.
- De UI bestaat uit ± 10.000 regels (ui.c, ui_input.c, ui_layer.c, ui_draw.c, ui_graph.c, ui_menu.c …). Die vervangen we door een eigen `live_ui`. Teken- en fontcode (`gfx.c`, `lcd.c`) kunnen we hergebruiken.

## 3. Engine-interface
`firmware/engines/src/felucca_engine.h` (MIT) definieert een helder `engine_t`:
- 8 parameters (P_E0..E7) met naam en bereik, presets, `note_on`, `render`, plus optionele hooks (`block`, `amp`, `desc`, `keys`, `done`)
- `knob[4]`: welke 4 parameters op het hoofdscherm staan
- `poly`: maximum aantal stemmen

Dat is in feite de adapter uit ons ontwerp. Onze macrotabel (8 NC-macro's → parameters met curve) komt erbovenop.

## 4. Wat er al is (hergebruiken)
| Ontwerp | In Felucca | Opmerking |
|---|---|---|
| DX7-engine | **FM6**: msfa/Dexed-port, 32 algoritmes, laadt **32-voice DX7 .syx-banks** (via de web-editor) | Heeft al 8 macro's op het patch: ALG, FB, **MLVL** (helderheid), **MRAT** (ratio), MEG (env-tijden modulators), VMOD, **DTUN**, SLOT. Onze Kleur/Karakter/Grom/Staart/Breedte passen er bijna 1-op-1 op. Max 6 stemmen per part |
| Juno-engine | **ANALOG**: 2 polyBLEP-oscillators, sub, noise, drive, SVF LP/BP/HP | Juno-chorus en Juno-presets moeten erbij |
| Arp | `seq.c`: 14 modes (up, down, up/down, random, as played, chord …), OCT, GATE, SWING, PROB, **HOLD** | Hold vervangt het akkoord bij een nieuwe aanslag: precies zoals ontworpen. Volgt externe klok. **Maar**: de arp start meteen bij de eerste noot en telt vanaf daar → niet op het raster. Moet aangepast |
| MIDI-klok | USB + TRS, jitter-afvang, Start/Stop | Klaar |
| Kanalen | MIDI IN per part: blokken CH1-4 enz., of **TRACK** (elk part zijn eigen kanaal) | TRACK-modus = onze L/R op eigen kanalen |
| CC's | Vaste CC-map + MIDI Learn (16), CC64-sustain, CC120/123 | CC zet de parameter direct, zonder slew. Engines werken per blok van 32 samples (± 0,7 ms). Onze macrolaag voegt smoothing toe |
| Program Change | Laadt een sound op het part | Presetwissel per NC-patroon mogelijk (als de NC PC stuurt) |
| Effecten | Per part DIST + INSERT; gedeelde chorus, tempo-delay (1,49 s), reverb (ROOM/SPRING/HALL) | |
| Throws/live-FX | PERFORM-laag: REPEAT 1/8–1/32, REVERSE, filter, TAPE STOP, FREEZE, OCT, FLANGER, PHASER, **REV THROW, ECHO THROW** (zonder klik) | Onze throws en momentane effecten bestaan al |
| Favorieten, user presets, categorieën | Ja | |
| Stemmen | **8 stemmen totaal**, gedeeld door alle parts; tweede CPU-core kan parts renderen | Met 2 parts: 8 stemmen samen. Later meten of meer kan |

## 5. Wat we moeten bouwen
1. **Live-UI** (vervangt de Felucca-UI): SOUND/ARP/FX, focus L/R, overlays, popups, panic, slot.
2. **Arp op het raster**: start gekwantiseerd op de klok-stappen, fase-reset bij Start, wijzigingen op de volgende stap.
3. **Drone-hold**: kan voortbouwen op de sustain-pedaallogica (CC64), maar dan als vergrendelde schakelaar per part.
4. **Macrolaag**: CC 70–78 → per engine een tabel → parameters, met slew (5–20 ms).
5. **Juno-smaak**: chorus I/II/I+II, drift, Juno-achtige presets op ANALOG.
6. **Split-uitgang**: de delay en reverb zijn gedeelde mono-bussen. Voor twee eigen delays is **geen geheugen** (de delay alleen is al 128 KB). Opties:
   - in split de effecten alleen op één kant
   - de delaybuffer in tweeën delen (2 × 0,74 s)
   - in split de effecten van de NC gebruiken (aanbevolen voor v1)
7. **Sequencer uit de interface halen.** De code laten we eerst slapend staan (minder risico). Later eventueel weg uit de build om RAM terug te winnen.

## 6. Emulator
- Felucca heeft een browser-emulator (WebAssembly) die **Web MIDI** accepteert: noten, en CC 20–27 als paneelknoppen. De NC via USB op de Mac → emulator in Chrome werkt dus.
- Bouwen vraagt Emscripten (`emcc`). Dat lukte in deze cloudomgeving niet; op een Mac gaat het met `brew install emscripten`.
- De originele Felucca-emulator om nu al mee te spelen: https://hugelton.github.io/Felucca/webapp/try/

## 7. Testresultaten
`tests/run_tests.sh` is volledig gedraaid op Linux (106 testgroepen, ± 5.700 afzonderlijke checks):
- **Alles groen, op één punt na.** Er waren 98 golden renders met 0 afwijkingen. De health-checks, de stem- en routingchecks, het CPU-budget, de UI-renders (233 schermen × 10 paletten) en de opslag- en persistence-tests hadden 0 fouten.
- **Portabiliteit:** de stresstest `drumfuzz` compileert op Linux pas met `-include stdint.h`. Dat is een upstream-probleempje, want op de Mac van de maker werkt het. In onze fork aanpassen in `tests/run_tests.sh`.
- **Gevonden randgeval (upstream):** in de drumfuzz-run met een SONG die speelt, ving UBSan één keer een **deling door nul** in `fx.c` `beat_samples()`: `FS * 60 / song.g[G_BPM]` terwijl BPM even 0 was. Bij een herhaling met dezelfde seed kwam het niet terug, dus het is waarschijnlijk een timing-race tijdens het laden van een song. Op de FM-1 geeft zo'n deling geen crash maar een verkeerde waarde. Voor ons relevant omdat de arp op `beat_samples`/`div_samples` leunt: **in de fork een guard toevoegen** (BPM nooit 0) en het upstream melden.
- De emulator-stap is overgeslagen (geen Emscripten in deze omgeving).

## 8. Aanbeveling: verder in Claude Code
Bouwen en testen gaat prima in een cloudomgeving. Flashen gaat vanaf de Mac: de web-installer in Chrome/Edge, of `tools/fm1_install.py`. Voor het echte werk is een git-repo in Claude Code het handigst.

### Opzet
1. Fork `hugelton/Felucca` naar je eigen GitHub (Fenderberg), met de submodule `firmware/engines`.
2. Eigen **package identity** kiezen (niet die van Felucca), en een werknaam voor de firmware.
3. Een `CLAUDE.md` in de repo met de chipregels (alleen integers, geen 64-bit deling, niets zwaars in de audio-ISR, alleen tekenen wat verandert, flash alleen schrijven als het stil is) en een verwijzing naar het ontwerpdocument.
4. Rescue testen op de echte FM-1 vóór de eerste eigen build: OCT− ingedrukt bij het aanzetten = updater.

### Stap 1 (eerste mijlpaal)
- Build-vlag `FELUCCA_LIVEUI=1` die de Felucca-UI vervangt door een minimale `live_ui.c`: twee kolommen L/R, focus via OCT−/OCT+, PRESETS bladert sounds van het gefocuste part.
- Part 1 = FM6, part 2 = ANALOG; MIDI IN op TRACK-modus met twee instelbare kanalen.
- Macrolaag: CC 70–78 met slew naar de FM6-macro's (MLVL, MRAT, FB, MEG, DTUN …) en naar de ANALOG-parameters.
- Hosttests blijven groen; nieuwe test voor de macrolaag (geen sprongen, slew-tijd).

De startprompt en het volledige stappenplan staan in `OVERDRACHT.md`.
