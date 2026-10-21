# NCC mijlpaal 4: meer engines, een 303, en per part de engine kiezen (plan, nog geen code)

Datum: 21 oktober 2026. Vraag van Remco: "ik wil meer engines, bv. de 303, en een manier om te kiezen welke engine
synth 1 (L) en welke synth 2 (R) heeft. Kies alleen engines waarmee we de knoppen van de NC kunnen toewijzen."
Status: **akkoord (21-10-2026); gebouwd en getest, nog niet geflasht.** Zie "Stand van zaken" onderaan.

## Welke engines (verkend)
Felucca heeft 13 engines. Op de 8 Circuit-labels (Oscillator, Osc Mod, Amp Env, Filter Env, Filter Freq, Resonance,
Modulation, FX) passen er vier zinnig, plus de nieuwe 303:

| Engine | Karakter | Waarom het past |
|---|---|---|
| **FM6** | DX7-FM | al in gebruik (helderheid = MLVL, enz.) |
| **ANALOG / JUNO** | 2 oscillators, SVF-filter, Juno-chorus | al in gebruik |
| **PHASE** | Casio CZ-achtige phase distortion | DCW is het "filter", ENV de filter-envelope, DTN/SUB/FOLD |
| **TRIO** | SID-achtig (Commodore 64): 3 oscillators, ring/sync, 12 dB multimode-filter | CUT, RES, PW, intervals |
| **ACID (nieuw)** | een 303: 1 oscillator, 24 dB-filter, accent, slide | precies de knoppen van een 303 |

Niet gekozen: LOFI (geen resonantie), PHYS, WHEEL (orgel), VOICE (formant), GRAIN, NOISE, DRUM, SAMPLE, SLICE: hun
parameters passen niet op de Circuit-labels.

## De 303: een eigen engine `ACID` (nieuw bestand `firmware/src/eng_acid.c`)
Felucca heeft alleen een "ACID"-preset op ANALOG (12 dB-SVF, geen accent, geen slide). Een echte 303-smaak vraagt:
- **Eén oscillator**: zaagtand of blokgolf (band-limited, polyBLEP), mono.
- **24 dB/oct-ladderfilter** (vier polen, integer, met de typische 303-resonantie die bij hoge RES "piept"), met
  lichte saturatie.
- **Filter-envelope** met ENV MOD en DECAY, een snelle VCA-envelope.
- **Accent**: een noot met hoge velocity (≥ 100) krijgt meer filter-sweep en volume, en opeenvolgende accenten
  stapelen zoals op het origineel. De Circuit stuurt velocity mee, dus accenten komen uit je patroon.
- **Slide**: overlappende noten (legato) glijden naar elkaar, met een vaste 303-achtige glijtijd.
- Alles fixed point (geen float, geen 64-bit deling), één stem: lichte CPU.
- Parameters (de 8 E-waarden): WAVE, CUT, RES, ENV MOD, DECAY, ACCENT, SLIDE, DRIVE. Met 6–8 presets (klassiek
  acid, squelch, bass, …).
- Registratie als **engine 14**, achteraan in Felucca's tabel (die is "append-only": opgeslagen nummers blijven
  geldig), alleen in de NCC-build (achter de vlag). Een paar kleine haakjes in `engines.c`, `core.h` (NENGINES),
  `fx.c` (CPU-schatting per engine) en `midi_control.c` (de CC-tabel per engine).

## De macro's per engine (`perf_layer.c`, NCM_MAP)
| Knop | Label | PHASE | TRIO | ACID |
|---|---|---|---|---|
| 1 | Oscillator | WAVE (stapt) | INT2 (interval osc 2, stapt) | WAVE (saw ↔ square) |
| 2 | Osc Mod | DTN | PW (pulsbreedte) | ACCENT |
| 3 | Amp Env | ATK + REL | ATK + REL | DECAY (VCA) |
| 4 | Filter Env | ENV (env → DCW) | ENV → FLT | ENV MOD |
| 5 | Filter Freq | DCW | CUT | CUT |
| 6 | Resonance | FOLD | RES | RES |
| 7 | Modulation | LFO → DCW + snelheid | LFO → filter + snelheid | SLIDE-tijd |
| 8 | FX | drive + chorus-send | drive + chorus-send | DRIVE + delay-send |
FM6 en ANALOG houden hun huidige tabel.

## Kiezen welke engine L en R hebben
- Voorstel: op de **SOUND-pagina** draait **SELECT** door de engines van het gefocuste part: FM6 → JUNO (ANALOG) →
  PHASE → TRIO → ACID. De kolom toont tijdens het draaien "ENGINE: TRIO >"; na een korte pauze (zoals PRESETS) wordt
  de engine gewisseld met zijn eerste preset, klikvrij (Felucca's fade bij een engine-wissel). De macro's gaan terug
  naar het midden.
- Daarna bladert PRESETS door de sounds van die engine (op ANALOG eerst de Juno-sounds, zoals nu).
- ALGORITHM: bij FM6 de SLOT, bij alle andere engines de Juno-chorus (de chorus werkt op elke engine).
- Wordt bewaard met het project (de engine zit in Felucca's projectformaat). Na uit/aan blijft je keuze staan;
  `lv_setup` dwingt dus niet meer FM6/ANALOG af, alleen bij een leeg project of een engine die NCC niet kent.

## Testen
- `tests/ncc_test.c`:
  - de engine-keuze met SELECT (pauze, wissel, eerste preset, macro's terug, bewaard);
  - de macrotabellen per engine (geen sprongen, binnen het bereik);
  - ACID: zaagtand en blokgolf, filter-sweep, de resonantie stabiel (geen ontploffing bij RES 127), accent (meer
    sweep en level bij velocity ≥ 100), slide (legato glijdt, los niet), geen klik bij noot-aan, CPU per stem.
- Regressie: goldens van Felucca ongewijzigd (zonder vlag sowieso; met de vlag ook voor de bestaande engines). ACID
  krijgt eigen golden renders in `tests/ncc_golden.txt` (NCC-eigen, los van Felucca's `tests/golden.txt`).
- Echte build: size-report. Emulator: engine kiezen, ACID spelen.

## Geheugen en CPU (schatting)
- ACID: per part ±64 B status in Felucca's engine-geheugen (geen extra RAM); flash ±4–6 KB; CPU één stem met
  ladderfilter ±60–90 instructies per sample (minder dan een FM6-stem).
- Engine-keuze: ±50 B RAM, ±1 KB flash.

## Vragen
1. ACID als **eigen engine** met ladderfilter, accent en slide (voorstel, het meest 303), of eerst snel **acid-presets
   op ANALOG** (12 dB-filter, zonder echte accent/slide)?
2. Engine kiezen met **SELECT op de SOUND-pagina** (voorstel), of liever met een knop (bijv. EDIT)?
3. De lijst **FM6, JUNO, PHASE, TRIO, ACID**: goed, of wil je LOFI (8-bit) of PHYS (snaren/plaat) er toch bij?

## Besloten (Remco, 21-10-2026)
1. ACID als **eigen engine** (ladderfilter, accent, slide).
2. Engine kiezen met **SELECT op de SOUND-pagina**.
3. De lijst **FM6, JUNO, PHASE, TRIO, ACID**.

## Stand van zaken (21-10-2026)
Gebouwd (branch `nc/setup`):
- `firmware/src/eng_acid.c`: **ACID**, engine 14. Zaagtand/blokgolf (polyBLEP), vierpolig ladderfilter met
  teruggekoppelde resonantie door een soft clip (RES 127 piept maar blijft begrensd), filter-envelope met ENV MOD en
  DECAY (DECAY 0 ≈ 70 ms, 60 ≈ 250 ms, 127 ≈ 3 s), drive, keytracking. **Accent** bij velocity ≥ 100 (meer sweep, kortere
  decay, luider; opeenvolgende accenten stapelen). **Slide**: legato + glide in TIME-modus (een nieuwe ACID-sound krijgt
  dat vanzelf). 8 presets (ACID 303, SQUELCH, SQR ACID, DEEP BASS, RUBBER, SCREAM, DUB ACID, HOOVERISH).
- Registratie als engine 14 achter de vlag (`/* NC: */` in `core.h` NENGINES = 15, `engines.c` tabel/volgorde/eng_ok,
  `fx.c` CPU-schatting, `midi_control.c` CC-tabel). Favorieten weigeren engine 14+ (rij 14/15 zijn MIDI Learn en de
  voorkeuren; `favorites.c`, `midi_learn.c`).
- `perf_layer.c`: macrotabellen voor PHASE, TRIO en ACID (zie de tabel hierboven).
- `live_ui.c`/`live_draw.c`: **SELECT op SOUND** kiest de engine (FM6, JUNO, PHASE, TRIO, ACID; wissel na 300 ms
  stilstand, popup "L ENGINE ACID", de pad toont "ENGINE …" tijdens het draaien en de enginenaam rechtsboven).
  ALGORITHM = chorus op alle engines behalve FM6. Een project dat NCC voor het eerst opent, krijgt L FM6 en R JUNO;
  daarna blijft je keuze bewaard.

Getest:
- `tests/ncc_test.c` (+ UBSan): ENGINES (kiezen, rondje, terug, R apart, chorus op TRIO, bewaard met het project,
  SELECT op ARP blijft pagina 2), FILTER FREQ op PHASE/TRIO/ACID, ACID (klinkt, envelope sluit, RES 127 begrensd,
  accent helderder en luider, slide alleen bij overlap, zaagtand ≠ blokgolf). Alles groen.
- Volume: ACID 303 rms ≈ 23, gelijk aan ANALOG SUB en PHASE. CPU: 170–234 instructies per sample per preset (minder
  dan een FM6-stem).
- Regressie: zonder vlag Felucca's 98 goldens 0 gewijzigd; met de vlag `tests/ncc_golden.txt` = Felucca's 98 + 8 van
  ACID, 0 gewijzigd (`run_tests.sh` gebruikt dat bestand voor de NCC-build). Let op: verandert upstream een golden,
  dan ook `tests/ncc_golden.txt` bijwerken.
- `midi_control_test` met de vlag: 19 checks van Felucca's eigen CC- en klokgedrag (bedoeld: de macro-CC's, en de arp
  die zonder klok op de BPM doorloopt). Zonder vlag groen.
- Echte build: image 400.040 B (+2,1 KB), RAM 58.904 B (+16), pool 326.696 B. Plain = Felucca v1.6.1.
- Emulator: engine-keuze met SELECT (FM6 → ACID), ACID 303 met LEG en slide.

Nog open:
- Luisteren: klinken ACID-presets en de ladder als een 303? (filtergevoel, accent, slide-tijd: 24 ≈ 50 ms).
- De GLIDE-ring toont de waarde als tijd in RATE-stijl ("5.7ms"), ook in TIME-modus.
