# NCC mijlpaal 1: testlijst op de FM-1

Voor Remco. Pas gebruiken als er een echte build is (`FELUCCA_LIVEUI=1 ./build.sh`, in de cloud) en jij besluit te
flashen. Vink af en noteer bijzonderheden in je FM-1-logboek.

## Vooraf (één keer)
- [ ] FM-1 op een bekende firmware (officieel V15 of Felucca), accu vol, goede datakabel.
- [ ] Rescue getest: OCT− ingedrukt houden bij het aanzetten = updater. Dit is je terugweg.
- [ ] Back-up van eigen klanken en projecten.
- [ ] Na het flashen: `python3 tools/fm1_install.py --info` meldt `FM-1_700`.

## Opstarten
- [ ] Opstartscherm toont **NCC** (en Felucca + Hügelton).
- [ ] Hoofdscherm: links **L · CH 3 · FM6**, rechts **R · CH 4 · ANALOG**; L heeft de balk eronder.
- [ ] OCT− brandt (focus L).

## Paneel
- [ ] OCT+ → balk naar R, OCT− gaat uit. OCT− → terug naar L.
- [ ] Toetsen spelen het part met de focus.
- [ ] PRESETS draaien: kolom toont "NEXT >" of "< PREV"; na het loslaten laadt er één sound, zonder klik.
- [ ] ALGORITHM op L (FM6): SLOT wisselt (F1…F8, B1…), de klank verandert.
- [ ] OCT− + OCT+ samen 5 s vasthouden: update-aftelling verschijnt; loslaten = geannuleerd (scherm komt terug).

## Met de Circuit Tracks
Circuit: MIDI 1 op kanaal 3, MIDI 2 op kanaal 4 (fabrieksinstelling), fabrieks-MIDI-template.
- [ ] Noten van MIDI 1 → L (stipje links knippert), MIDI 2 → R.
- [ ] Synth 1/2 van de Circuit (kanaal 1/2) laten de FM-1 stil.
- [ ] Elke macroknop toont onderin een popup met het Circuit-label en de waarde (±1 s).
- [ ] Per knop luisteren (op L en R): 1 Oscillator, 2 Osc Mod, 3 Amp Env, 4 Filter Env, 5 Filter Freq, 6 Resonance,
      7 Modulation, 8 FX. Geen zipper-ruis bij snel draaien. Bij FM6 knop 1 en 2: stappen hoorbaar? Klikt het?
- [ ] Andere sound kiezen: de klank is weer precies de preset (macro's terug naar het midden); de eerste draai daarna
      glijdt naar de stand van de knop.
- [ ] Program Change vanaf de Circuit (als je die gebruikt) laadt een sound op het juiste part.
- [ ] Start/Stop en klok van de Circuit: BPM rechtsboven wordt amber; er gaat geen eigen patroon van de FM-1 spelen.
- [ ] Alle 16 knoppen tegelijk wild draaien met akkoorden op beide parts: geen haperingen of krakers
      (CPU-meting hoort hierbij).
- [ ] Geen hangende noten na Stop of na een sound-wissel tijdens het spelen.

## Na afloop
- [ ] Uit- en aanzetten: de laatste sounds staan er weer.
- [ ] Terug naar Felucca of V15 als dat nodig is (rescue of de web-installer).

## Mijlpaal 2 (Juno en split)
- [ ] OCT+ (focus R), ALGORITHM draaien: popup "R CHORUS I / II / I+II", de kolom toont de stand. Klinkt het als
      een Juno-chorus (breed, zwevend), geen klik bij wisselen?
- [ ] PRESETS op R: eerst de Juno-sounds (JUNO STRINGS … JUNO SWEEP), dan ANALOG's eigen sounds. Per sound luisteren.
- [ ] Drift: akkoorden op een Juno-sound klinken iets "levend" (niet vals).
- [ ] GLO vasthouden + SEQ: kop "SPLIT", popup "SPLIT: L LEFT, R RIGHT". Met een koptelefoon: L alleen links,
      R alleen rechts, geen galm/delay. Nog eens: "STEREO".
- [ ] Uit/aan: SPLIT en de chorus-stand staan er weer (de naam toont dan "JUNO").
- [ ] Y-kabel naar de twee ingangen van de Circuit Tracks: L en R los te mixen?

## Mijlpaal 3 (arp, effecten, Circuit-stijl)
- [ ] Het scherm: kop met paginablok in de kleur van het part (L blauw, R roze), twee pads, stappenrij, vier ringen.
- [ ] ARP: arp aan (popup "L ARP ON", LED, ARP-pagina). KNOB 1 mode, 2 rate, 3 octaven, 4 gate; SELECT = pagina 2
      (swing, kans, volgorde). ARP nog eens (op de ARP-pagina): uit.
- [ ] Met de Circuit (klok via USB, Play): de arp valt op de tellen van de Circuit, ook als je een akkoord iets te laat
      speelt. Start op de Circuit: beide arps vanaf de eerste stap.
- [ ] Zonder Circuit-klok: de arp loopt op het eigen tempo.
- [ ] REC = HOLD: akkoord spelen, loslaten, de arp loopt door; nieuw akkoord vervangt het. REC uit: stopt.
- [ ] SEQ = Drone: noten blijven klinken, bijspelen stapelt, dezelfde noot nog eens = los, SEQ uit = alles uit.
- [ ] FX: FX-pagina (drive, chorus, delay, reverb van het gefocuste part; pagina 2: insert, delaytijd, feedback,
      reverb-grootte).
- [ ] FX vasthouden + witte toets: het effect speelt (pad licht op), KNOB 1–4 = FILTER, CRUSH, THROW, DEPTH.
- [ ] ENV vasthouden = delay-throw, LFO vasthouden = reverb-throw.

## Mijlpaal 4 (engines, ACID)
- [ ] SOUND-pagina, SELECT draaien: de pad toont "ENGINE …"; na loslaten wisselt het part (popup "L ENGINE …"):
      FM6 → JUNO → PHASE → TRIO → ACID → FM6. Klikvrij?
- [ ] Per engine: PRESETS bladert, de 8 Circuit-knoppen doen iets logisch (Filter Freq, Resonance …).
- [ ] ACID: een acid-lijn vanaf de Circuit (MIDI 1 of 2): klinkt als een 303? Accenten (hoge velocity op de Circuit)
      knallen eruit, overlappende noten glijden, losse noten niet. Knop 7 (Modulation) = slide-tijd.
- [ ] ALGORITHM op PHASE/TRIO/ACID = chorus.
- [ ] Uit/aan: de engines van L en R staan er weer.
