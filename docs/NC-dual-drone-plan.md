# NCC: twee Drone-modi en Dual met L/R

Datum: 24 oktober 2026. Status: **nieuwe bediening en akkoord-REPLACE geïmplementeerd en met expliciet akkoord geflasht**.
De eerdere versie met GLO + HOME en één vervangende noot is al met expliciet akkoord geïnstalleerd.

## Afgesproken werking

Dual gebruikt de bestaande L- en R-part en koppelt **alleen noten**. Eén gekozen bron, MIDI-kanaal 3 of 4, speelt
beide parts. Het andere kanaal levert in Dual geen noten. Klanken, arp, Drone, knopbesturing en effecten houden
hun eigen werking. De FM-1-toetsen spelen beide parts. REPLACE stopt oude noten met een korte overgang.
Alle nieuw gekozen sounds beginnen zonder delay- of reverb-send; daarna kun je deze effecten gewoon bijdraaien.

| Instelling / actie | Part L | Part R |
|---|---|---|
| Sound | korte pluck | pad |
| Arp | UP, 1/16, eventueel HOLD | uit |
| Drone | uit | REPLACE |
| Dual CH3 | kanaal 3 speelt L | kanaal 3 speelt ook R |
| C aanslaan en loslaten | arp volgt eigen HOLD | C blijft klinken |
| Daarna G aanslaan | arp krijgt G | C stopt met korte fade; G blijft klinken |
| Paneelfocus L / R | bewerkt alleen de gekozen part | behoudt eigen instellingen |
| Circuit-macro's | kanaal 3 blijft L bedienen | kanaal 4 blijft R bedienen |

## Bediening

| Bediening | Functie |
|---|---|
| SEQ | Drone aan/uit voor de geselecteerde part |
| ARP-pagina 2, KNOB 4 | DRONE: STACK / REPLACE |
| SCL (naast FX, door Remco bevestigd) | Dual aan/uit; onthoudt CH3 / CH4 |
| GLO | instellingen, twee pagina’s via SELECT of GLO |
| GLO pagina 2, KNOB 1 | Dual OFF / CH3 / CH4 |
| EDIT | klankbewerking van de gekozen part; SELECT / EDIT wisselt pagina |
| HOME | terug naar SOUND |
| OCT- / OCT+ | L / R kiezen om te bewerken |
| GLO + SEQ | SPLIT / STEREO |
| Manager > Settings | Dual-bron en Drone per part rechtstreeks kiezen |

De kop toont `D3` / `D4` bij actieve Dual. Drone-badges zijn `DR+` (STACK) en `DR1` (REPLACE); HOLD is `H`, zodat
ARP, HOLD en Drone samen in het partvak passen. De volledige modusnaam staat bij de DRONE-knop op ARP-pagina 2.
De bestaande keuze van categorie, engine en sound blijft hetzelfde.

## Drone per part

| Actie | STACK | REPLACE |
|---|---|---|
| Nieuwe noot | toevoegen aan de drone | toevoegen zolang een fysieke toets vast is; na alles loslaten het vorige akkoord vervangen |
| C loslaten, G spelen | C en G klinken | alleen G blijft actief |
| Dezelfde noot opnieuw | die noot uit de drone halen | opnieuw aanslaan en vasthouden |
| Noot loslaten | blijft klinken | blijft klinken |
| Drone uit | drone loslaten; fysiek vastgehouden toetsen blijven spelen | hetzelfde |

REPLACE gebruikt sinds de vervolgwijziging een vloeiende overgang: circa 51 ms uitfaden en daarna circa 51 ms
infaden, ook bij lange RELEASE en MONO/LEGATO/UNISON. De eerste aanslag start direct. De oude stemmen blijven
renderen tot de gain nul bereikt; pas daarna worden hun plaatsen hergebruikt. Zo werkt dezelfde overgang ook voor
PHYS/GRAIN met drie stemmen, zonder extra audiobuffers of gelijktijdige oude/nieuwe stemmen. Nieuwe aanslagen tijdens
het uitfaden vervangen alleen het wachtende akkoord, zonder de aftelling opnieuw te beginnen.
Remco koos expliciet voor een fysieke toetsgroep: noten toevoegen mag zolang minstens één toets ingedrukt is.
Na alles loslaten blijft het akkoord dronen; de volgende aanslag vervangt het hele akkoord. Geen tijdvenster.
Dit werkt voor gewone MIDI-akkoorden, de lokale toetsen, Dual en CHORD-uitbreiding van één grondnoot.
Sustain telt niet als een fysiek ingedrukte toets. Gebruik POLY voor meerdere gelijktijdige tonen; stemmodi
blijven hun eigen werking houden.
Een bestaande delay/reverbstaart mag uitklinken; de effectbuffers worden niet gewist.

Met ARP aan geldt Drone voor de invoernoten van de arp. REPLACE verwijdert ook de oude ARP HOLD- en monostacknoten;
een late Note Off mag de nieuwe arp niet beïnvloeden. Snelheid, octaven en klokgrid blijven per part werken.
Een moduswissel geldt vanaf de volgende aanslag. SEQ uit laat fysiek vastgehouden toetsen gewoon doorspelen tot
hun eigen Note Off. Stop, panic en MIDI-overloop ruimen de betreffende invoer en wachtende aanslagen op.

## Instellingen en klankbewerking

GLO pagina 1: L MIDI-kanaal, R MIDI-kanaal, SPLIT/STEREO, klok INT/USB/TRS.
GLO pagina 2: Dual OFF/CH3/CH4, interne BPM, L Drone OFF/STACK/REPLACE, R Drone OFF/STACK/REPLACE.
Deze waarden zijn dezelfde als in de Manager. De actuele BPM blijft in de kop staan. SCL schakelt aan/uit en
onthoudt ook over projectherstel of herstart de laatste actieve bron.

EDIT werkt steeds op de gekozen part: ENGINE 1/2, AMP ENV, ENV DEST, LFO, LFO DEST, LFO SYNC, VOICE, VOICE 2 en
CHORD. JUNO krijgt FILTER. FM6 toont OPERATOR, OP ENV en OP MODE en gebruikt die in plaats van de algemene AMP ENV /
ENV DEST. De bestaande beschrijvingen bepalen labels, bereiken en eenheden; de bestaande FM6-patchschrijver maakt
operatorwijzigingen veilig naar SLOT OWN. Gewone edits behouden bewust opgedraaide sends. Een nieuwe FM6 SLOT
laden zet die wel op nul. De bestaande undo-opname wordt voor klankwijzigingen gebruikt.

## Dual-routing en controllers

| Stand | Noten kanaal 3 | Noten kanaal 4 | FM-1-toetsen |
|---|---|---|---|
| OFF | normale routing, standaard L | normale routing, standaard R | geselecteerde part |
| CH3 | naar L én R | genegeerd | naar L én R |
| CH4 | genegeerd | naar L én R | naar L én R |

CH3 en CH4 sluiten elkaar uit. De kanaalinstellingen voor normaal gebruik blijven bewaard. USB en TRS volgen
dezelfde notenfiltering; de klokbron blijft apart instelbaar. Er komen geen extra parts of stemmen bij.

Alleen Note On/Off wordt gekoppeld. Macro's, pitchbend, aftertouch, sustain en Program Change houden hun bestaande
bestemming, ook wanneer dat kanaal geen notenbron is. Sustain op kanaal 4 kan dus R vasthouden terwijl CH3 beide
parts speelt; de nootrouter kopieert geen pitchbend van L naar R.

Een bronwissel of Dual aan/uit wordt op een audioblokgrens toegepast. Bestaande live-noten, arp-latches en drones
op beide parts stoppen met een fade; de volgende aanslag gebruikt de nieuwe routing. Vastgehouden toetsen moeten
eerst los en opnieuw aangeslagen worden. Patches, controllers, arpinstellingen en Drone-keuzes blijven staan.

## Sounds en effecten

- Iedere nieuwe sound op L of R krijgt `P_DLY = 0` en `P_REV = 0`: NCC, fabriek, DX7-bank en gebruikerssounds.
  Dit geldt voor paneel, Manager, Program Change en nieuwe initialisatie, zowel normaal als in Dual.
- Daarna zijn de sends normaal bij te draaien. De volgende sound zet alleen de twee sends van de geladen part op nul.
  De andere part houdt zijn waarden. Bestaande staarten kunnen uitklinken.
- Delaytijd, feedback, reverbtype/-grootte en gedeelde effectbussen houden hun huidige werking. Chorus, Juno-karakter,
  drive, insert en throws blijven beschikbaar. Dual maakt globale effecten niet onafhankelijk.
- Presets en geïmporteerde banken worden niet herschreven. Een volledig projectherstel en undo herstellen hun
  bewust opgeslagen effectwaarden; die handelingen gelden niet als het kiezen van een nieuwe sound.

## Implementatie en opslag

Alles staat achter `FELUCCA_LIVEUI=1`. Engine-interface en opgeslagen structuren blijven gelijk.

| Bestand | Werk |
|---|---|
| `ncc_input.c` | Dual-filtering, twee eigen MIDI-eigenaars en snapshots voor lokale toetsen |
| `ncc_settings.c` | bestaande `G_SYNC`-placeholder hergebruiken voor gemarkeerde keuzes |
| `ncc_drone_fade.c` | vloeiende ~51 ms uit/in-overgang voor alle engines, zonder extra stemmen |
| `ncc_seq.c` | REPLACE-akkoorden, begrensde wachtrij per toon na de fade, oude arp/mono-invoer opruimen |
| `ncc_pages.c` | GLO-instellingen, engineafhankelijke EDIT-pagina’s en FM6-adapters |
| `ncc_sound_defaults.c` | nul-sends na buitenste soundload; hooks voor initialisatie en bank-PC in `ui.c` |
| `live_ui.c`, `live_draw.c` | bediening, modusnamen en badges |
| `editor_ncc.c`, `web/ncc/manager.*` | optionele protocoluitbreiding en instellingen |
| `tests/ncc_performance.c`, `ncc_panel_chords.c`, `ncc_drone_fade_test.c` | echte MIDI-, paneel-, opslag- en laadpaden in hosttests |
| `web/emu/ncc_test.mjs`, `web/ncc/manager_test.mjs` | echte WebAssembly-invoer en Manager-bediening met mock |

`G_SYNC` had geen actieve lezer. Gemarkeerde waarde: `64 | dual | (L_replace << 2) | (R_replace << 3) | (remember_CH4 << 4)`.
Dual is 0/1/2 voor OFF/CH3/CH4. Ongemarkeerde oude of ongeldige waarden geven OFF/STACK. Projectgrootte en
engineparameters groeien niet. Dual-bron en Drone-modi blijven bij een herstart bewaard; Drone aan/uit en
klinkende noten worden niet opgeslagen. Oudere gemarkeerde waarden zonder herinneringsbit blijven bruikbaar;
een actieve bron heeft voorrang op dat bit.

Twee private MIDI-invoerslots (16=L, 17=R) houden eigenaarschap apart van fysieke controllerkanalen. Chord-,
pedaal-, Note Off- en paniclogica gebruiken hun bestaande administratie; deze slots zijn geen externe MIDI-kanalen.
Lokale toetsen bewaren per part hun oorspronkelijke akkoord en MIDI-uitgangskanaal, zodat focus- of akkoordwissels
geen noten achterlaten. De ISR beheert de invoertabellen; er zijn geen effectbuffers bij gekomen. Een kleine teller per part houdt
fysiek ingedrukte MIDI-grondnoten bij, apart van pedaalhouders. Lokale snapshots tellen mee voor de akkoordgroep.
Elke fysieke aanslag begint één groepcontrole, zodat alle tonen van CHORD bij elkaar blijven. De fade-wachtrij
bewaart maximaal zestien toonhoogtes met hun eigen velocity en toets; de bestaande stemmenlimiet blijft gelden.

Protocol (alle bytes 7-bit, bestaande prefixes behouden):

- INFO eindigt optioneel met `1, 7`: versie 1; capabilitybits 0=Dual, 1=Drone-modi, 2=nul-sends.
- PARTS eindigt optioneel met `1, dual, L_mode, L_on, R_mode, R_on`.
- SET op 4: `[4, dual]`; op 5: `[5, part, mode, on]`, part 0/1, mode 0/1, on 0/1.
- Een oudere Manager negeert de trailers. De nieuwe Manager schakelt de nieuwe bediening uit bij oudere firmware.

## Verificatie en vervolg

De hosttests controleren beide NCC-varianten (LOOP verborgen en aan), bronuitsluiting, afzonderlijke CC/pitchbend/
sustain, eigenaarschap bij focuswissel, lokale en MIDI-overlap, akkoordsnapshots, TRS, panic/overloop/Stop, alle
stemmodi bij REPLACE, hele akkoorden, langzame toevoegingen, sustain zonder fysieke toetsen,
ARP HOLD, herhaalde noten en late offs. De GLO-bediening, EDIT-isolatie, FM6-operators, ongeldige slots en
het bewaren van CH4 terwijl Dual uitstaat hebben eigen tests. Ook iedere bibliotheeksound op beide parts,
gebruikersopslag, bank-PC, undo, projectherstel en oude/ongeldige opslag worden getest. Manager-tests bedienen de
echte handlers, inclusief terugval op oude firmware.

`sh tools/ncc_dev.sh emu-test` bouwt de echte WebAssembly en bedient Dual CH3/CH4, aparte sends, soundkeuze,
een arp op L en akkoord-REPLACE op R, GLO-pagina’s en EDIT. De audio blijft eindig en begrensd;
`build/emu-demo/performance*.png` toont de werkelijke schermen, inclusief instellingen en FM6-operator.
De bestaande 113 NCC- en 98 Felucca-goldenrenders zijn niet aangepast.

Voor de eerder geflashte versie is de volledige Felucca-suite ook afgerond (`SANITIZE=0`, uitvoer buiten Dropbox). Bestaande Mac-problemen blijven:
letterafstandtests, CPU-metingen en twee DRUM-stresstests die al tijdens ASan-initialisatie afbreken. De optionele
sanitiserronde is overgeslagen. De overige controles, waaronder schermlayout, invoer, opslag, engines, Manager-
protocol en emulator, slagen. Volledig log: `build/host/full-suite.log`.

De actuele build gebruikt **576728 B image**, **61704 / 73728 B RAM** en **359464 / 368640 B pool**.
Deze bevat ook de twaalf engines en 161 ingebouwde sounds; zie [extra engines](NC-extra-engines-macros.md).
De ontwikkelidentiteit blijft `FM-1_700`; het package is herkenbaar aan SHA-256
`f5b0b041a7906ed590e13e4c44fad83316cf337f1460da8e32885ee6e58f3ec2`.

Geïnstalleerd met de opdracht “doe dat en zet alles naar de fm1”. De installer bevestigt schrijven en herstart;
een losse identiteitscontrole en alleen-lezen NCC INFO/LIB/PARTS slagen: twaalf engines en 161 ingebouwde sounds,
193 entries inclusief de bestaande DX7-bank. Logs: `build/ncc-flash-smooth.log` en
`build/ncc-device-check-smooth.json`. De vorige paneelbuild (573408 B) is hiermee vervangen.

Nog niet zelf beluisterd: overgangen, nieuwe patches, fysieke knoppen en zware Dual-combinaties vragen een
praktijktest. De publieke release en webinstaller zijn nu NCC 0.4 (`FM-1_704`), met dezelfde firmwarelogica. Een volgende flash of publicatie vereist
opnieuw expliciet akkoord.
