# NCC mijlpaal 3: arp, effecten en Circuit-stijl (plan, nog geen code)

**Historisch document.** Dit bewaart het ontwerp en de teststand van de vermelde mijlpaal; oude bediening en
“nog niet geflasht” zijn geen actuele status. Voor de huidige firmware: [handleiding](MANUAL.md),
[overdracht/status](HANDOFF.md), [Dual/Drone](NC-dual-drone-plan.md) en [extra engines](NC-extra-engines-macros.md).

Datum: 21 oktober 2026. Vraag van Remco: "als je een sound selecteert en je drukt ARP in, dan moet de arp aanstaan;
met KNOB 1–4 pas je de arp aan; de arp moet een latch/hold hebben; werk de effecten uit; maak de UI in de stijl en
vibe van de Circuit Tracks." Dit voegt stap 3 (arp) en 4 (FX) van het bouwplan samen, plus een nieuwe look.
Status: **akkoord (21-10-2026); gebouwd en getest, nog niet geflasht.** Zie "Stand van zaken" onderaan.

## Wat Felucca al heeft (verkend)
- **Arp** (`seq.c`): 14 modes (UP, DN, UPDN, RND, ORD = zoals gespeeld, REPEAT, DNUP, UP+8, CONV, DIVG, PINKY,
  THUMB, WALK, CHORD), RATE (1/4, 1/8, 1/16, 1/32, 8T, 16T, 1/2, 1/1, 2BAR, 4BAR), OCT 1–4, GATE, SWING, PROB en
  **HOLD** (`P_AHOLD`): een nieuwe aanslag na loslaten vervangt het akkoord, bijspelen terwijl je vasthoudt voegt
  toe. De arp werkt op de toetsen **en op de MIDI-noten van de Circuit**. Hij volgt de externe klok, maar **start
  meteen bij de eerste noot** en telt vanaf daar: niet op het raster.
- **Effecten per part**: DIST (drive), sends naar CHORUS, DELAY en REVERB (gedeelde bussen), INSERT (o.a. phaser,
  flanger, crush). Globaal: delaytijd (gesynct), feedback, reverb-type (ROOM, SPRING, HALL) en -grootte.
- **PERFORM-laag** (`perform.c`): FX vasthouden + witte toets = momentaan effect (REPEAT 1/8, 1/16, 1/32, REVERSE,
  LPF, HPF, TAPE STOP, FREEZE, OCT UP, OCT DN, FLANGER, PHASER, REV THROW, ECHO THROW); KNOB 1–4 met FX vast =
  FILTER, CRUSH, THROW, DEPTH. In de live-UI nu nog niet bereikbaar (dat deed de oude UI).

## Bediening (voorstel)
Drie pagina's, zoals in het ontwerp: **SOUND** (HOME), **ARP** en **FX**. Steeds voor het part met de focus (L/R).

| Knop | Functie |
|---|---|
| **ARP** | Arp **aan** voor het gefocuste part en naar de ARP-pagina. Nog eens op de ARP-pagina: arp **uit**. LED = arp aan. |
| **REC** | **HOLD/latch** van de arp aan/uit (LED). Werkt op elke pagina. |
| **FX** | Naar de FX-pagina. **FX vasthouden + witte toets** = momentaan effect (PERFORM), het scherm toont welke toets wat doet. |
| **ENV vasthouden** | Delay-throw. **LFO vasthouden**: reverb-throw. Op elke pagina. |
| **HOME** | Terug naar SOUND. |
| OCT− / OCT+ | Focus L / R (zoals nu). |

**ARP-pagina** (KNOB 1–4; SELECT draaien = pagina 2):

| | Pagina 1 | Pagina 2 |
|---|---|---|
| KNOB 1 | Mode (UP, DN, UPDN, RND, ORD, CHORD …) | Swing |
| KNOB 2 | Rate (1/4, 1/8, 8T, 1/16, 16T, 1/32) | Probability |
| KNOB 3 | Octaven 1–4 | Volgorde (NOTE / PLAY) |
| KNOB 4 | Gate | — |

**FX-pagina**:

| | Pagina 1: hoeveel (per part) | Pagina 2: karakter (globaal) |
|---|---|---|
| KNOB 1 | Drive | Insert-type |
| KNOB 2 | Chorus-send | Delaytijd (gesynct: 1/16 … 1/2, gepunteerd) |
| KNOB 3 | Delay-send | Delay-feedback |
| KNOB 4 | Reverb-send | Reverb-type + grootte |

**SOUND-pagina** (KNOB 1–4): level, octaaf van de FM-1-toetsen, voice-modus (POLY / MONO / UNISON), glide.

## Arp altijd op het raster (nieuw, `perf_layer.c` + klein haakje in `seq.c`)
- De eerste noot start de arp op de **eerstvolgende stap van het klokraster** (een 1/16-arp op de volgende 1/16 van
  de maat), niet meteen. Een noot die een klein beetje te laat komt (binnen 1/8 van de stap), telt nog als "op
  tijd" en start meteen, zodat live spelen niet slepend voelt.
- **Start** van de Circuit: de arp-fase gaat terug naar tel 1 (beide parts samen). **Stop**: de arp stopt; een
  vastgehouden (HOLD) akkoord blijft bewaard.
- Wisselen van rate/swing: gaat in op de volgende stap.
- Zonder externe klok: de arp loopt op de interne BPM (zie de vragen).

## Circuit Tracks-stijl voor het scherm
- **Kleuren zoals op de Circuit**: L (gespeeld door MIDI 1) **blauw**, R (MIDI 2) **roze**, zoals de pads van die
  tracks op de Circuit. Zwarte achtergrond, felle kleurvlakken, witte playhead.
- **Kop**: paginanaam (SOUND / ARP / FX) in een blok in de kleur van het gefocuste part, NCC, ST/SPLIT, BPM.
- **Midden**: de twee parts als "pads": de gefocuste fel, de andere gedimd; sound-naam, ARP- en HOLD-badges,
  chorus, een MIDI-stip.
- **Onder**: **vier knop-ringen** (KNOB 1–4) als de macro-LED's van de Circuit: een boog in de kleur van het part
  met de waarde en het label eronder; de ring die je draait licht op.
- **Stappenrij**: 16 kleine pads met een witte playhead die op de klok meeloopt (zoals de step-pads van de Circuit),
  vooral op de ARP-pagina.
- **FX vast**: het scherm toont de toetsen als een pad-raster met de effectnamen (wat je ingedrukt houdt licht op).
- Tekenen met Felucca's `gfx.c` (fonts, knop-bogen), alleen wat verandert.

## Bestanden
| Bestand | Wijziging |
|---|---|
| `firmware/src/live_ui.c` (van ons) | Pagina's, ARP/REC/FX/ENV/LFO-knoppen, KNOB 1–4, het nieuwe scherm. Wordt groot: opsplitsen in `live_ui.c` (invoer) en `live_draw.c` (scherm). |
| `firmware/src/perf_layer.c` (van ons) | Arp op het raster: startfase, reset bij Start, de laatst gekozen arp-mode per part. |
| `firmware/src/seq.c` | Eén haakje (`/* NC: */`) in `arp_add`: de startfase vragen aan de perf-laag i.p.v. "meteen". |
| `tools/size_fns.py` | `live_draw.c` compact (main loop). |

De PERFORM-laag en de effecten zelf blijven zoals ze zijn; NCC maakt ze alleen bereikbaar.

## Testen
- `tests/ncc_test.c`:
  - ARP aan/uit en de pagina;
  - KNOB 1–4 per pagina naar de juiste parameter;
  - HOLD vervangt/voegt toe;
  - arp op het raster: de eerste noot valt op de volgende stapgrens, een licht late noot meteen, reset bij Start;
  - FX vast + toets zet het PERFORM-effect, loslaten stopt het;
  - ENV/LFO-throws;
  - FX-pagina-knoppen.
- Scherm: render-lint (geen tekst buiten beeld of over elkaar) in GREY en het eigen kleurenschema.
- Regressie: goldens ongewijzigd met en zonder vlag. Echte build: size-report. Emulator: kijken en spelen.

## Geheugen en CPU (schatting)
- RAM: < 0,5 KB (pagina, knop-ringen, arp-fase). Flash: ±10–15 KB (scherm en tabellen; ruim binnen de marge, image
  nu 390 KB). CPU: de arp-fase kost niets meetbaars; het scherm draait in de main loop.

## Vragen
1. **HOLD/latch op REC** (voorstel, zoals het ontwerp), of liever ARP lang indrukken?
2. **Zonder externe klok**: arp op de interne BPM (voorstel: dan kun je ook zonder de Circuit oefenen), of stil met
   "GEEN KLOK" zoals in het ontwerp stond?
3. **Drone-hold** (SEQ) nu meenemen, of later?
4. De kleuren blauw (L) en roze (R) zoals de MIDI-tracks op de Circuit: goed?

## Besloten (Remco, 21-10-2026)
1. HOLD/latch op **REC**.
2. Zonder externe klok: de arp loopt op de **interne BPM**.
3. **Drone-hold** (SEQ) gaat mee in deze mijlpaal: per part, gespeelde noten blijven klinken (note-offs genegeerd),
   bijspelen stapelt tot het stemmenmaximum, een klinkende noot nog eens spelen laat die los, Drone-hold uit laat
   alles los via de release. De NC-macro's blijven werken.
4. Kleuren: **L blauw, R roze** (de MIDI 1/2-tracks van de Circuit).

## Stand van zaken (21-10-2026)
Gebouwd (branch `nc/setup`):
- `firmware/src/ncc_seq.c` (in `seq.c` geïncludeerd): de **arp op het raster** (start op de volgende stap van de maat,
  een noot tot 1/8 stap te laat start meteen, Start = alle arps vanaf de eerste stap) en **Drone-hold** (note-offs
  genegeerd, stapelen, nog eens = loslaten, uit = alles via de release). **HOLD uit** zonder toets laat het
  vastgehouden akkoord los. Zonder klok loopt de arp op de BPM. Haakjes `/* NC: */` in `seq.c`: `arp_add`, `input_on`,
  `input_off`, de include en `events_block`.
- `perf_layer.c`: de throws (ENV = ECHO THROW, LFO = REV THROW) via de ISR (`ncm_throw`).
- `live_ui.c` (invoer): pagina's SOUND / ARP / FX met KNOB 1–4 (en pagina 2 via SELECT), ARP aan/uit + pagina,
  REC = HOLD, SEQ = Drone (GLO + SEQ = SPLIT), FX vast = PERFORM (toetsen en macro's FILTER/CRUSH/THROW/DEPTH), de
  LED's (ARP, REC, SEQ, FX, ENV/LFO tijdens een throw). De klok staat op USB (de Circuit).
- `live_draw.c` (scherm, Circuit-stijl): kop met paginablok in de partkleur (L blauw, R roze), twee pads, stappenrij met
  witte playhead, vier knop-ringen, voetregel met popups; FX vast toont de 16 effect-pads.

Getest:
- `tests/ncc_test.c` (+ UBSan): ARP UI, ARP GRID (een noot midden in een 1/16: de arp start op de volgende 1/16; een
  licht late noot meteen; geen klok = BPM), DRONE, FX UI (knoppen, PERFORM-toets en -macro, throws), SCREEN (kopkleur
  per part, labels passen). Alles groen.
- Regressie met en zonder vlag: 98 golden renders, 0 gewijzigd. Idle 340 i.p.v. 326 instructies/sample.
- Felucca's `ui_test` met de vlag: één check faalt ("the note the arp plays is recorded"), omdat de arp in NCC op het
  raster start in plaats van meteen; NCC neemt niets op. Zonder vlag groen.
- Echte build: image 397.964 B (+7,9 KB), RAM 58.888 B (+80), pool 326.696 B. Plain = Felucca v1.6.1.
  Let op: bouwen in Colima vanuit de Dropbox-map gaf vandaag "Resource deadlock avoided" (gelijktijdige containers op
  de Dropbox-koppeling); bouwen vanuit een spiegel buiten Dropbox (`~/.cache/ncc-build`) werkt.
- Emulator: SOUND-, ARP- en FX-pagina, de PERFORM-kaart (een vastgehouden effect licht op) en de kleuren bekeken.

Nog open / later:
- Bij Stop van de Circuit stopt de arp nu niet (hij blijft op de klok of de BPM lopen); in het ontwerp stond "Stop →
  arps stoppen". Bekijken na het spelen.
- KNOB 4 op FX-pagina 2 = reverb-grootte; het reverb-type (ROOM/SPRING/HALL) zit nog niet op een knop.
- Luisteren en bijstellen: arp-gevoel op het raster, de PERFORM-effecten, de throws.

## Aanpassing (21-10-2026, na de eerste keer op de FM-1)
Op verzoek van Remco:
- **FX is een aan/uit-knop**: één tik opent de FX-modus, nog een tik (of HOME, of ARP) sluit hem. In de FX-modus zijn
  de witte toetsen de PERFORM-effecten zonder dat je FX vasthoudt (Felucca's `kb_lock`), elk zolang je de toets houdt.
  **SELECT** bladert door de pagina's, elk met KNOB 1–4:
  | Pagina | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
  |---|---|---|---|---|
  | PERFORM (met de effect-pads) | FILTER | CRUSH | THROW | DEPTH |
  | SENDS (per part) | drive | chorus-send | delay-send | reverb-send |
  | DELAY | tijd | feedback | kleur | mix |
  | REVERB | type | grootte | demping | – |
  | CHORUS | snelheid (bus) | diepte (bus) | Juno-chorus (part) | drift (part) |
  | INSERT (per part) | type | A, B, C met de namen die het type geeft (bijv. DRIVE, TONE, LEVEL) |
- **SPLIT is nu echt 100 %**: in SPLIT gaat de retour van de mono-bussen (delay, reverb, chorus; ook throws, de
  THROW-macro en galmstaarten) niet meer naar de uitgang, en de BASS+-luidspreker-EQ mengt geen bas van de ene kant
  naar de andere. Getest tot aan de uitgang na de master: speelt alleen L (met sends, throws, THROW, FLANGER en de
  Juno-chorus aan), dan is rechts exact 0, en andersom.
- **SPLIT is de standaard** (ook in bestaande projecten); GLO + SEQ schakelt naar STEREO en terug. Technisch:
  `G_ENGGO = 1` betekent nu STEREO (`NCC_SPLIT()` in `core.h`; de regressietest van de NCC-build bouwt met
  `NCC_SPLIT_DEFAULT=0`, zodat Felucca's goldens in stereo blijven gelden).
