# NCC mijlpaal 2: plan (nog geen code)

Datum: 20 oktober 2026. Hoort bij `NC-companion-ontwerp.md` (v0.7), bouwplan stap 2: **Juno-smaak op ANALOG** en de
**stereo/split-uitgang**. Status: **akkoord (20-10-2026, met de voorgestelde antwoorden); gebouwd en getest, nog niet geflasht.** Zie "Stand van zaken" onderaan.

Uitgangspunt na mijlpaal 1 (echte build): RAM 58.696 van 73.728 B (±15 KB vrij), pool 325.632 van 368.640 B (±42 KB
vrij), image 387 KB. Er is dus ruimte.

## Wat Felucca al heeft (verkend)
- **ANALOG** (`firmware/engines/src/eng_analog.c`, de submodule van upstream): 2 polyBLEP-oscillators, sub, noise,
  drive, SVF LP/BP/HP, ADSR. Die engine laat ik ongemoeid (de submodule is van upstream).
- **INSERT CHOR** per part (`fx.c`): mono, één modulerende tap (rond 3,2 ms). Klinkt als een pedaal, niet als een Juno:
  een Juno-chorus is **stereo** (twee tappen met tegengestelde LFO) en heeft vaste standen.
- De **gedeelde chorus-bus**, delay (128 KB) en reverb zijn mono bussen; de droge mix is stereo (`mix_part` /
  `mix_spread` in `fx.c` tellen elk part met PAN op in `mix_l` / `mix_r`).
- De **modulatiematrix** heeft de bronnen RAND (per noot), S&H, SLEW en LFO, en het doel PITCH: daarmee kan
  "drift" zonder de engine aan te passen.

## A. Juno-chorus (nieuw bestand `firmware/src/juno_chorus.c`)
- Per part een korte vertraging (256 samples int16 = 5,8 ms; 512 B per part), gevoed met het mono signaal van het part
  ná DIST/INSERT. Twee tappen met een driehoeks-LFO, 180° uit fase: **links = droog + tap A, rechts = droog + tap B**.
  Een eenpolig laagdoorlaatfilter op het natte signaal (het BBD-karakter).
- Standen zoals op de Juno-60 (getallen uit analyses van het originele circuit; bij het bouwen naluisteren):
  | Stand | LFO | Vertraging |
  |---|---|---|
  | I | 0,51 Hz | ±1,7–5,3 ms |
  | II | 0,86 Hz | ±1,7–5,3 ms |
  | I+II | 9,75 Hz | ±3,3–3,7 ms (snel, ondiep: vibrato-achtig) |
- **Haakje in `fx.c`** (`/* NC: */`, achter de vlag): in `mix_part` en `mix_spread`, waar het part met PAN in
  `mix_l`/`mix_r` gaat: staat de Juno-chorus van dat part aan, dan telt `jc_mix()` het stereo-paar op in plaats van de
  gewone pan-stap. Staat hij uit, dan is alles bit voor bit als nu (goldens ongewijzigd).
- **Bediening**: op part R (ANALOG) is de ALGORITHM-encoder in mijlpaal 1 nog vrij. Voorstel: **ALGORITHM op R = chorus
  UIT / I / II / I+II** (zoals de knoppen op een Juno). Ook op L (FM6) bruikbaar als je wilt (daar zit SLOT al op
  ALGORITHM; dan via een andere knop, zie de vragen).
- Kosten (schatting): ±30–40 instructies per sample per part als hij aan staat; 0 als hij uit staat.

## B. Drift
- Geen engine-wijziging: een Juno-preset zet twee matrix-slots: **RAND → PITCH** (elke noot een paar cent anders,
  zoals de oscillators van een echte Juno) en **SLEW (of LFO 2, langzaam) → PITCH** (een trage zweving).
- De hoeveelheid zit in de preset; later eventueel op een macro.

## C. Juno-presets (nieuw bestand `firmware/src/ncc_presets.c`)
- Een eigen tabel met ±12 Juno-achtige ANALOG-sounds: complete instellingen (E1..E8, ENV, filter-env, LFO, de twee
  drift-slots, chorus-stand, sends, voice-modus). Voorstel voor de lijst: STRINGS, PAD, BRASS, POLY SYNTH, ORGAN,
  BASS, SUB BASS, PLUCK, LEAD, HOOVER, BELLS, SWEEP. Klinken ze niet goed, dan stem ik ze bij het luisteren af.
- PRESETS op R toont eerst de Juno-sounds, dan ANALOG's eigen presets, dan de user presets.
- Laden gebeurt via `apply_preset_to(t, 0)` als basis (alles netjes op standaard), daarna de Juno-waarden erover.
  De macrolaag krijgt een expliciete "nieuwe sound"-melding (`ncm` reset), want de preset-index zegt hier niets.
- Open punt: de naam van een Juno-sound is na uit/aan niet bekend (Felucca bewaart wel alle waarden, niet "welke
  NCC-preset"). Opties: de naam in een vrije byte van het project bewaren, of de Juno-sounds als Felucca-user presets
  meeleveren (dan staan ze in flash, met naam, en zijn ze in de web-editor te bewerken). Voorstel: eerst de simpele
  weg (na uit/aan toont de kolom "JUNO"), de nette weg in mijlpaal 5.

## D. Stereo / split-uitgang
- **ST** (standaard): zoals nu.
- **SPLIT**: part L helemaal links, part R helemaal rechts (PAN genegeerd), mono per kant. De sends naar delay,
  reverb en de gedeelde chorus-bus staan dan **uit** (het zijn mono bussen die beide kanten zouden vullen); een
  Juno-chorus van een part wordt in split mono op zijn eigen kant opgeteld. Drive en insert per part blijven.
  De PERFORM-effecten op de master werken op beide kanten (in split dus op beide parts: later bekijken).
- **Haakje in `fx.c`**: dezelfde plek als A: in split vervangen de pan-versterkingen door 1/0 of 0/1 en gaan de
  sends op 0.
- **Bediening**: voorstel GLO vasthouden + SEQ = ST ⇄ SPLIT (één toggle, popup "SPLIT"); de bovenbalk toont ST of
  SPLIT. Bewaren: met de instellingen (nog uitzoeken welke vrije byte; geen nieuw projectformaat).
- Nog te checken bij jou: mixt de Circuit Tracks zijn twee ingangen als losse kanalen? (Y-kabel 3,5 mm TRS → 2×
  mono jack.)

## Bestanden die ik aanraak
| Bestand | Wijziging |
|---|---|
| `firmware/src/fx.c` | Include van `juno_chorus.c`; in `mix_part` en `mix_spread` één haakje voor chorus/split (achter de vlag). |
| `firmware/src/live_ui.c`, `perf_layer.c` (van ons) | ALGORITHM op R, Juno-lijst bij PRESETS, split-toggle, "ST/SPLIT" in de kop, `ncm` reset-verzoek. |
| `tools/size_fns.py` | `ncc_presets.c` compact (main loop); `juno_chorus.c` niet (audio-ISR). |
Niet aangeraakt: de engines-submodule, de opslagformaten (tenzij de split-instelling een vrije byte nodig heeft).

## Testen
- `tests/ncc_test.c` uitbreiden:
  - chorus UIT = bit voor bit gelijk;
  - chorus aan: links ≠ rechts, mono-som zonder uitdovingen;
  - LFO-snelheid per stand (nuldoorgangen tellen);
  - geen klik bij het wisselen van stand;
  - split: een noot op L geeft rechts exact 0 (en andersom), delay/reverb-bussen leeg;
  - Juno-preset laden zet alle waarden en reset de macro's.
- Regressie: goldens ongewijzigd met en zonder de vlag (chorus uit, ST).
- CPU: kosten van de chorus per part meten in de regressie.
- Echte build (Colima): size-report.
- Emulator: luisteren, met de Circuit via Web MIDI.

## Geheugen en CPU (schatting)
- RAM: 2 × 512 B chorus-lijnen + ±64 B status = ±1,1 KB (van ±15 KB vrij). Pool: 0. Flash: ±6 KB (code + presettabel).
- CPU: 0 als de chorus uit staat; ±30–40 instructies per sample per part als hij aan staat (±2–3 %).

## Vragen aan Remco
1. ALGORITHM op R (ANALOG) = chorus-stand: goed? En wil je de Juno-chorus ook op L (FM6), en zo ja met welke knop?
2. De lijst met Juno-sounds: goed zo, of wil je bepaalde klanken (bijv. een bekende Juno-preset als voorbeeld)?
3. Split-toggle met GLO + SEQ: goed?
4. Naam van Juno-sounds na uit/aan: eerst "JUNO" en later netjes (mijlpaal 5), akkoord?

## Besloten (Remco, 20-10-2026): de voorgestelde antwoorden
1. ALGORITHM op R = chorus-stand. (De chorus werkt technisch ook op L; daar is nog geen knop voor.)
2. De voorgestelde lijst met Juno-sounds.
3. Split-toggle met GLO + SEQ.
4. Na uit/aan toont de kolom "JUNO"; de nette naam later (mijlpaal 5).

## Stand van zaken (20-10-2026)
Gebouwd (branch `nc/setup`):
- `firmware/src/juno_chorus.c`: stereo Juno-chorus per part (I / II / I+II, fade bij wisselen) en SPLIT, als eigen
  mixfunctie `ncc_mix` (haakje in `fx.c` `mix_part`; staat beide uit, dan Felucca's eigen lus, bit voor bit).
- Het Juno-karakter van een part zit in `P_ED_FX` (een opgeslagen parameter die Felucca niet gebruikt): bit 0–1 de
  chorus, bit 2–5 de drift. Het hoort dus bij de sound en wordt bewaard in projecten en user presets.
- Drift: haakje in `voice.c` (`poly_play`): elke POLY-noot een willekeurige fijnstemming, tot ±7,5 cent bij 15.
- SPLIT in `G_ENGGO` (een opgeslagen globale waarde die Felucca niet gebruikt): bewaard met het project.
- `firmware/src/ncc_presets.c`: 12 Juno-sounds (WAVE SUB = zaagtand + sub-blokgolf, of PWM met LFO op de breedte),
  geladen als één sound-load; de macro's gaan terug naar het midden (`ncm_gen`).
- `live_ui.c`: ALGORITHM op R = chorus (popup "R CHORUS II"), PRESETS op R eerst de Juno-lijst, GLO + SEQ =
  ST/SPLIT (popup, "ST"/"SPLIT" in de kop), de kolom toont "CHORUS ..." en de Juno-naam.

Getest:
- `tests/ncc_test.c` (+ UBSan): chorus (uit = Felucca's lus; II stereo en de mono-som blijft; de LFO-snelheden;
  geen klik bij wisselen, gemeten op een sinus), split (een noot op L: rechts en de bussen exact 0, en andersom),
  Juno-sounds (waarden, chorus, drift, macro's terug), drift per noot (en geen drift onder 4), UI (ALGORITHM, de
  Juno-lijst, GLO + SEQ, bewaard en teruggeladen met het project). Alles groen.
- Regressie met en zonder vlag: 98 golden renders, 0 gewijzigd. Idle-CPU 336 i.p.v. 326 instructies/sample.
- Kosten op de Mac: de chorus aan = +15 % op een blok met 4 ANALOG-noten (±150 ns per blok, ±7 % van het zwaarste
  geval); SPLIT +2,6 %. Hoger dan de geschatte 2–3 %; versnellen kan (de vertraging per blok interpoleren) als het op
  de chip krap wordt.
- Echte build (Colima): image 390.092 B (+2,6 KB t.o.v. mijlpaal 1), RAM 58.808 B (+112), pool 326.696 B (+1.064, de
  chorus-lijnen). Plain build nog steeds exact Felucca v1.6.1.
- Emulator: chorus via ALGORITHM, SPLIT via GLO + SEQ (kop en popup) en de Juno-lijst bij PRESETS gecontroleerd.

Nog open:
- Luisteren: klinken de Juno-sounds en de chorus goed? (emulator of FM-1). De presetwaarden zijn een eerste gok.
- De PERFORM-throws (REV/ECHO THROW) sturen in SPLIT nog naar de mono bussen (later, mijlpaal 4).
- Of de Circuit Tracks zijn twee ingangen als losse kanalen mixt (Remco).
