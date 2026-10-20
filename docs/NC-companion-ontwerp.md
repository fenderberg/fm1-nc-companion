# FM-1 als hulpje van de Novation Circuit Tracks: ontwerp v0.6

Status: ontwerp, 20 oktober 2026. Verkenning (stap 0) klaar, bouwen begint in Claude Code.
- v0.2: geen sequencer, split-uitgang als optie, interface voor live gebruik.
- v0.3: rolverdeling. **NC = noten + klankvorming (8 macro's). FM-1-paneel = sounds kiezen, arp bedienen, effecten.** Ontworpen voor een live jam.
- v0.4: arp altijd vast aan de klok, met Arp-hold. Losse Drone-hold voor drones.
- v0.5: basis gekozen: fork van Felucca, met eigen interface. Architectuur in lagen; live tweaken is de belangrijkste eis.
- v0.6: **verkenning verwerkt** (zie `NC-companion-verkenning.md`): Felucca bevestigd, open vragen over code en FM6 beantwoord, split aangepast aan het geheugen, hergebruik per onderdeel aangegeven.

## Doel
De FM-1 wordt een klankmodule met twee engines naast de Circuit Tracks (NC). De NC speelt de noten via zijn twee MIDI-tracks, levert de MIDI-klok en vormt de klank met zijn 8 macroknoppen. Op de FM-1 kies je live de sounds, speel je met de arp, zet je drones vast en gooi je effecten erop. **Geen eigen sequencer**: de NC is de baas. De engines zijn vervangbaar; de interface en het live tweaken zijn het product.

## Basis: fork van Felucca (bevestigd)
- Felucca v1.6.1 bouwt en alle tests slagen (op één upstream-randgeval na, zie de verkenning).
- De UI is in de code een **aparte laag bovenop** de kern. Die vervangen we door een eigen `live_ui` achter een build-vlag `FELUCCA_LIVEUI=1`.
- De engine-interface (`felucca_engine.h`, MIT) is al het adaptercontract uit dit ontwerp.
- Licentie GPL-3.0-only: broncode delen als we een build delen. **Eigen package identity** kiezen.

## Architectuur (4 lagen)
```
┌───────────────────────────────────────────┐
│ 4. UI-laag (nieuw: live_ui.c)              │  pagina's SOUND/ARP/FX, scherm, popups, overlays
├───────────────────────────────────────────┤
│ 3. Performance-laag (nieuw: perf_layer.c)  │  macro's + smoothing, klok & arp, holds,
│    + Felucca's seq.c / perform.c / fx.c    │  FX-sends & throws, 2 parts, stemverdeling
├───────────────────────────────────────────┤
│ 2. Engine-laag (Felucca-engines + adapter) │  FM6 → "DX7", ANALOG → "Juno", later meer
├───────────────────────────────────────────┤
│ 1. Platform (Felucca, ongewijzigd)         │  USB, MIDI, audio, scherm, paneel, flash, rescue
└───────────────────────────────────────────┘
```

### Engine-adapter
Basis is Felucca's `engine_t`: 8 parameters met namen en bereik, presets, `note_on`/`render` en optionele hooks. Per engine voegen we een **macrotabel** toe: welke van de 8 NC-macro's welke parameters sturen, met curve en bereik. Een nieuwe engine = een `engine_t` + een macrotabel; de interface verandert niet.

### Eisen voor live tweaken (belangrijkste)
1. **Geen zipper-ruis**: macro's glijden per audioblok naar de doelwaarde (± 5–20 ms, instelbaar). Felucca zet CC-waarden nu direct; de macrolaag voegt de slew toe.
2. **Snelle CC-stromen**: per ronde alleen de laatste waarde per CC verwerken.
3. **CPU-marge**: worst case onder een vast budget (voorstel ≤ 75%), bewaakt met Felucca's CPU-tests. Bij te veel stemmen netjes stelen (Felucca doet dat al).
4. **Geen klikken bij sound-wissel**: Felucca heeft al fades bij engine- en presetwissel.
5. **Scherm nooit in de weg**: alleen gewijzigde stukjes tekenen; MIDI en klok gaan voor.
6. **Nooit flash schrijven tijdens het spelen**: Felucca weigert dat al.
7. **Delers bewaken**: nooit delen door een tempo of lengte die 0 kan zijn (zie de verkenning: `beat_samples`).

## Opzet
| | Engine L | Engine R |
|---|---|---|
| Type | **FM6** (msfa/Dexed-port, 6-op, 32 algoritmes) | **ANALOG** (2 polyBLEP-osc, sub, noise, drive, SVF) + Juno-chorus en -presets |
| Bestuurd door | NC MIDI-track 1 | NC MIDI-track 2 |
| MIDI-kanaal | Felucca's **TRACK**-routing (elk part een eigen kanaal), instelbaar | idem |
| Klanken | **32-voice DX7 .syx-banks**: FM6 laadt die al (via de web-editor); eigen patch per part | ANALOG-presets; Juno-106-import later uitzoeken |
| Stemmen | Max 6 per part | |
| Totaal | **8 stemmen gedeeld** door beide parts (Felucca-budget); later meten of meer kan, bijv. met de tweede CPU-core | |

## Audio-uitgang (instelling in GLO)
- **Stereo mix** (standaard): beide parts in stereo, met de gedeelde chorus, delay en reverb van Felucca.
- **Split**: part L helemaal links, part R helemaal rechts (allebei mono). Y-kabel (3,5 mm TRS → 2× mono jack) naar de twee audio-ingangen van de NC.
  - **Beperking**: de delay (128 KB) en reverb zijn gedeelde mono-bussen, en er is geen geheugen voor een tweede set.
  - **v1 van split**: de delay- en reverb-sends van de FM-1 staan uit; je gebruikt de effecten van de NC per kanaal. Drive, insert en chorus per part kunnen blijven.
  - Later eventueel: de delaybuffer in tweeën (2 × 0,74 s) of een lichte reverb per kant.
  - Nog checken: mixt de NC zijn twee ingangen als losse kanalen?

## De 8 macroknoppen (alleen op de NC)
Beide engines gebruiken dezelfde CC-nummers. Eén Components-template werkt dan voor beide MIDI-tracks. In `FELUCCA_LIVEUI` vervangt de macrolaag Felucca's vaste CC-map voor deze nummers; MIDI Learn kan blijven bestaan.

| Knop | CC | Engine L: FM6 | Engine R: ANALOG/Juno |
|---|---|---|---|
| 1 | 74 | Kleur → FM6-macro **MLVL** (level van alle modulators) | Cutoff |
| 2 | 71 | Karakter → **MRAT** (ratio's van de modulators) | Resonance (met lichte saturatie) |
| 3 | 70 | Grom → **FB** (feedback) + drive | Env → filter |
| 4 | 73 | Aanslag → **MEG** (sneller/trager) | Attack |
| 5 | 72 | Staart → MEG + release | Decay/release |
| 6 | 76 | Beweging: LFO-diepte (klok-gesynct) | LFO: snelheid + diepte |
| 7 | 77 | Breedte → **DTUN** (carriers uit elkaar) + spread | Chorus: uit / I / II / I+II + drift |
| 8 | 78 | Arp: uit · 1/8 · 1/16 · 1/16T · 1/32 | idem |

- FM6 heeft deze macro's al op zijn patch (ALG, FB, MLVL, MRAT, MEG, VMOD, DTUN, SLOT). Wij sturen ze aan met slew.
- Macro's werken **relatief t.o.v. de preset**. Bij een nieuwe preset gaan ze terug naar de middenstand (instelbaar).

## Arpeggiator (per part)
Basis: Felucca's arp in `seq.c`: 14 modes (o.a. up, down, up/down, random, as played, chord), OCT, GATE, SWING, PROB, HOLD; volgt de externe klok.

### Altijd vast aan de klok (aan te passen)
- Felucca start de arp nu **meteen** bij de eerste noot en telt vanaf daar. Nieuw: start **gekwantiseerd** op de eerstvolgende stap van het klokraster (GLO-optie: tel of maat).
- **Start van de NC** → arp-fase reset, beide arps samen op tel 1. **Stop** → arps stoppen; een vastgehouden arp onthoudt zijn noten.
- **Geen klok** → arp speelt niet, scherm "GEEN KLOK". GLO-optie (standaard uit): interne oefenklok.
- Wisselen van verdeling, swing of ratchet gaat in op de volgende stap.

### Arp-hold
- Bestaat al (`P_AHOLD`): een nieuwe aanslag na loslaten vervangt het akkoord, bijspelen terwijl je vasthoudt voegt toe. Precies zoals gewenst.

### Instellingen
- Modes: Felucca's lijst; op de ARP-pagina tonen we eerst up, down, up/down, random, as played, chord
- Verdeling: 1/4 · 1/8 · 1/8T · 1/16 · 1/16T · 1/32
- 1–4 octaven, gate, swing, ratchet (1–4×), probability, velocity

## Drone-hold (los van de arp, nieuw)
- Per part. Gespeelde noten blijven klinken; note-offs worden genegeerd. Voortbouwen op Felucca's sustain-pedaal (CC64), maar als vergrendelde schakelaar.
- Extra noten = **opstapelen** tot het stemmenmaximum. Een klinkende noot nog eens spelen = die noot loslaten.
- Drone-hold uit = alle noten netjes uit via hun release.
- Arp over een drone op hetzelfde part: let op het budget van 8 stemmen; anders per part of-of.
- De NC-macro's blijven werken op de drone. Panic wist alle holds.

## Live-interface (FM-1-paneel)

### Principes
1. **Drie live-pagina's: SOUND, ARP, FX.** Eén druk om te wisselen, HOME = terug naar SOUND.
2. **Focus op L of R** met OCT−/OCT+ (LED + gemarkeerde kolom).
3. **Vaste plekken**: arp aan/uit, arp-hold, drone-hold en de twee throws werken op elke pagina.
4. **Eén knop = één ding** op de live-pagina's; fijnere instellingen één SELECT-draai verder.
5. **Shift-overlays**: houd je een functieknop vast, dan toont het scherm wat de toetsen nu doen.
6. **Niets kan per ongeluk kapot**: lange druk voor opslaan, panic, paneelslot.

### Altijd beschikbaar (op elke pagina)
| Bediening | Functie |
|---|---|
| OCT− / OCT+ | Focus part L / part R (LED) |
| PLAY/STOP | Arp aan/uit voor het gefocuste part (LED) |
| REC | **Arp-hold** aan/uit (LED) |
| SEQ | **Drone-hold** aan/uit (LED) |
| HOME + PLAY | Arp retrigger bij de volgende maat |
| ENV (vasthouden) | **Delay-throw** (Felucca's ECHO THROW) |
| LFO (vasthouden) | **Reverb-freeze/throw** (Felucca's REV THROW / FREEZE) |
| HOME | Naar SOUND. **2 s vasthouden = panic** (noten uit, holds gewist, arps stop) |
| HOME + SAVE | Paneel vergrendelen/ontgrendelen |
| 27 toetsen | Spelen het gefocuste part; via de arp als die aan staat; vastgezet bij drone-hold |

Felucca gebruikt OCT− bij het aanzetten voor USB-rescue. Dat blijft zo; tijdens het spelen is OCT− focus L.

### Pagina SOUND (HOME)
| Bediening | Functie |
|---|---|
| ALGORITHM (encoder) | Bank kiezen (FM6: factory F1–F8, bank B1–B32, user presets) |
| PRESETS (encoder) | Sound kiezen; laadt na een korte pauze, zonder klikken |
| KNOB 1 | Level van het part |
| KNOB 2 | Octaaf (−2 … +2) |
| KNOB 3 | Voice-modus: poly / mono / unison |
| KNOB 4 | Glide |
| SAVE (vasthouden) + toets | Huidige sound als favoriet op die toets (Felucca heeft favorieten) |
| SEL (vasthouden) + toets | Favoriet direct laden (overlay toont de namen) |

### Pagina ARP (knop ARP; nog eens = terug)
| Bediening | Pagina 1 | Pagina 2 (SELECT) |
|---|---|---|
| KNOB 1 | Mode | Swing |
| KNOB 2 | Verdeling | Ratchet (1–4×) |
| KNOB 3 | Octaven 1–4 | Probability (100 → 25%) |
| KNOB 4 | Gate | Velocity |

### Pagina FX (knop FX; nog eens = terug)
Per part: **Drive → Insert/Mod → sends naar chorus, delay en reverb** (Felucca's FX-keten).

| Bediening | Pagina 1: hoeveel | Pagina 2 (SELECT): karakter |
|---|---|---|
| KNOB 1 | Drive | Insert-type |
| KNOB 2 | Chorus-send | Chorus/phaser + snelheid |
| KNOB 3 | Delay-send | Tijd (gesynct) + feedback |
| KNOB 4 | Reverb-send | Type ROOM/SPRING/HALL + grootte |
- **FX (vasthouden) + toets** = momentane effecten, Felucca's PERFORM-laag: REPEAT 1/8–1/32, REVERSE, filter, TAPE STOP, FREEZE, OCT, FLANGER, PHASER, REV/ECHO THROW.

### Hoofdscherm (1,54", 240×240)
- Twee kolommen **L** en **R**; de gefocuste licht, de andere gedimd.
- Per kolom: sound-naam, arp-icoon, **A-HOLD**- en **DRONE**-teken, FX-lampjes, MIDI-activiteitsstipje.
- Bovenbalk: pagina, BPM (amber = externe klok), beat-knipper, ST/SPLIT, accu.
- Bij draaien: grote popup met naam + waarde (± 1 s), ook voor NC-macro's.
- Tekenen met Felucca's `gfx.c` (fonts, icons, alleen wat verandert).

### Off-stage
| Knop | Functie |
|---|---|
| EDIT | Diep editen van de sound (FM6-operatorpagina's bestaan al in Felucca) |
| GLO | MIDI-kanalen, klok (startkwantisatie, oefenklok), ST/SPLIT, arp-link, hold-opties, smoothing, CC-map |
| SAVE (lang) op SOUND | Sound opslaan |

### Betrouwbaarheid
- Bij aanzetten de laatste toestand; holds bewust **uit**.
- Autosave alleen als het stil is (Felucca's autosave).

## Nog uitzoeken
- Standaard MIDI-kanalen van de NC MIDI-tracks; stuurt de NC klok + start/stop via MIDI out?
- Program Change per patroon vanaf de NC? (Felucca laadt bij PC al een sound op het part)
- Mixt de NC de twee audio-ingangen als losse kanalen? (split)
- Zijn de FM-1-encoders indrukbaar? Vasthouden en kort drukken: Felucca heeft al een instelbare HOLD-drempel voor lagen.
- Juno-106 SysEx-patchformaat (voor import op ANALOG)
- Stemmenbudget: is 8 gedeeld genoeg met arp + drone? Kan het meer met de tweede core?
- Eigen package identity: hoe herkennen installers en de Firmware Hub die?

## Bouwplan
0. ✔ **Verkenning** (20-10-2026): Felucca bouwt en test in de cloud, code en engines doorgelicht. Zie `NC-companion-verkenning.md`.
1. **Mijlpaal 1**: `FELUCCA_LIVEUI=1` met minimale live-UI (twee parts FM6/ANALOG, focus, presets bladeren, hoofdscherm), TRACK-routing, macrolaag met slew. Hosttests groen + nieuwe tests.
2. Juno-smaak op ANALOG; stereo/split-uitgang (v1: zonder FM-1-delay/reverb in split)
3. Arp op het raster, Arp-hold, Drone-hold (+ guard op `beat_samples`)
4. FX-pagina + throws (PERFORM hergebruiken)
5. Live-interface af (favorieten, overlays, popups, panic, slot)
6. .syx-bank laden zonder web-editor (optioneel), Components-template voor de NC, eerste release met eigen package identity
