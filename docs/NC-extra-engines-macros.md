# Extra engines: koppeling met de Circuit Tracks

24 oktober 2026. Status: lokaal uitgevoerd na Remco’s “voer uit”. Zeven engines met acht NC-macro’s,
46 nieuwe patches en 161 ingebouwde sounds. Hosttests, emulator en firmwarebuild slagen; deze uitbreiding is met
Remco’s vervolgopdracht geflasht, samen met de vloeiendere REPLACE-overgang. Nog niet op het apparaat beluisterd. Remco’s voorwaarde blijft: extra engines moeten via de NC te bedienen zijn.

## Conclusie

PHYS, GRAIN, VOICE, LOFI, WHEEL, NOISE en SAMPLE zijn toegevoegd aan dezelfde ALGORITHM-/soundkeuze.
Elke engine heeft acht macrokoppelingen via het bestaande MIDI-template. De tests controleren echte CC’s, afzonderlijke
L/R-parts en veranderingen in gerenderde audio; modelafhankelijke functies worden bij LOFI omgeleid waar nodig.

PHYS, GRAIN en VOICE voegen voor arp/dronegebruik het meest onderscheidende materiaal toe. LOFI, WHEEL en NOISE
bieden respectievelijk chipklanken, orgel en texturen. SAMPLE levert piano/fluit/sax en gebruikerssamples.

## Geïmplementeerd: alle acht knoppen

Deze koppelingen staan in `firmware/src/ncc_extra_macros.c`. De knoppen blijven de bestaande CC’s sturen:
1, 2, 5, 11, 71, 74, 12, 13. Hun middenstand blijft de geladen patch; bewegingen gebruiken dezelfde slew.
Een engine zonder resonant filter krijgt bij RESONANCE een expliciet benoemde andere klankfunctie, zoals nu
FM6 daar DETUNE heeft. Er komen geen knoppen die alleen een ongebruikt getal veranderen.

| NC-knop | PHYS | GRAIN | VOICE |
|---|---|---|---|
| OSCILLATOR | structuur / harmonischen (`E1`) | positie in de sample (`E1`) | klinker A + B (`E0`, `E1`) |
| OSC MOD | aanslagpositie / sympathische hoeveelheid (`E4`) | korrelgrootte (`E2`) | stembron BUZZ (`E4`) |
| AMP ENV | attack + release | attack + release | attack + release |
| FILTER ENV | envelope → helderheid (`P_ED_FLT`) | envelope → toonfilter (`P_ED_FLT`) | envelope → klinkers (`P_ED_FLT`) |
| FILTER FREQ | helderheid BRIT (`E2`) | TONE (`E7`) | formantverschuiving SHIFT (`E3`) |
| RESONANCE | uitklinkduur DAMP (`E3`; hoog = langer) | korreldichtheid DENS (`E3`) | formantresonantie Q (`E6`) |
| MODULATION | BOW / BEND / BUZZ (`E6`, afhankelijk van model) | willekeur + spreiding (`E6`, `E5`) | LFO → klinkers + snelheid |
| FX | drive + chorus-send | drive + chorus-send | drive + chorus-send |

| NC-knop | LOFI | WHEEL | NOISE | SAMPLE |
|---|---|---|---|---|
| OSCILLATOR | waveform (`E1`) | orgelregistratie (`E0`) | klankkleur (`E1`) | sample-stemming (`E1`) |
| OSC MOD | pulsbreedte / wavetable (`E2`); TRI/SAW/NOIS: pitch-sweep (`E4`) | lage drawbars SUB (`E1`) | dichtheid / register (`E5`) | bitreductie (`E2`) |
| AMP ENV | attack + release; STEP: release | attack + release | attack + release | attack + release |
| FILTER ENV | envelope → toonfilter | orgelpercussie (`E4`) | envelope → filter | envelope → toonfilter |
| FILTER FREQ | TONE (`E7`) | hoge drawbars TOP (`E3`) | FREQ (`E2`) | CUT (`E4`) |
| RESONANCE | CRSH / STEP-envelope (`E3`) | middelste drawbars BODY (`E2`) | RES (`E3`) | overdrive (`E6`) |
| MODULATION | vibrato (`E5`) | rotor OFF/SLOW/FAST (`E7`) | LFO → filter + snelheid | vibrato + snelheid |
| FX | drive + chorus-send | orgeldrive (`E6`) | drive + chorus-send | chorus-send |

Model, chipvariant en samplebron blijven via soundkeuze/EDIT instelbaar. Daarmee hoeft een live macro niet door
allerlei model- of samplewissels heen te draaien. WHEEL’s registratie, percussie en rotor zijn wel bewust standen.
De popup toont bij PHYS, LOFI en NOISE de modelafhankelijke betekenis, bijvoorbeeld BEND in plaats van BOW.
Bij SAMPLE houdt iedere ingedrukte noot zijn oorspronkelijke samplezone; stemming en CUT blijven live instelbaar.

## Wat de code bevestigt

- [perf_layer.c](../firmware/src/perf_layer.c): `NCM_MAP` koppelt acht macro’s aan maximaal twee parameters elk.
  De voorstellen passen binnen die bestaande interface; `ncm_cc` houdt de CC’s op hun eigen part en `ncm_block`
  verzorgt middenstand, slew, clamping en nieuwe bases na een soundload. De nieuwe engines gebruiken nu dezelfde macro-interface.
- [PHYS](../firmware/src/eng_phys.c): BRIT leest werkelijk `m->cutoff`; de structuur, positie, DAMP en modelafhankelijke
  BOW/BEND/BUZZ worden bij renderen gebruikt. Vier modellen en negen bestaande factorysounds. Drie stemmen per part.
- [GRAIN](../firmware/src/eng_grain.c): POS gebruikt shape-modulatie, TONE gebruikt cutoff-modulatie; SIZE, DENS,
  SPRD en RAND hebben eigen DSP-functies. Vier factorysounds en drie stemmen per part. Verandering kan bij nieuwe
  korrels ingaan; niet ieder bestaand korreltje wordt halverwege omgebouwd.
- [VOICE](../firmware/src/eng_formant.c): cutoff verschuift beide klinkers; BUZZ gebruikt shape-modulatie en Q stuurt
  de bandbreedte van de formantfilters. Vier factorysounds, maximaal vier stemmen per part.
- [LOFI](../firmware/engines/src/eng_lofi.c): waveform, duty/wavetable, crush/STEP, vibrato en toonfilter zijn bruikbaar.
  Vijf factorysounds. De betekenis van E2/E3 verschilt per chipvariant; de adapter kiest SWP of RELEASE als DUTY/ATK niet wordt gebruikt.
- [WHEEL](../firmware/engines/src/eng_wheel.c): SUB/BODY/TOP sturen de echte drawbars; rotor heeft versnellende/
  vertragende snelheden en uitfaden. Drawbargains worden per audioblok gerampt. Vijf factorysounds.
  Deze engine leest cutoff niet als filter; FILTER ENV daar aan `P_ED_FLT` koppelen zou dus een lege bediening geven.
- [NOISE](../firmware/engines/src/eng_noise.c): FREQ/RES zijn een filter; cutoff- en shape-modulatie worden gebruikt.
  Vier factorysounds. De betekenis van dichtheid/register verschilt per MODE.
- [SAMPLE](../firmware/src/eng_sample.c): TUNE, BITS, CUT en DRIVE worden bij renderen gebruikt. Geen eigen resonantie:
  daarom een andere betekenis voor knop 6. Drie unieke ingebouwde sounds plus eigen samplemateriaal.

## Uitvoering en verificatie

- Nieuwe bestanden: `ncc_extra_macros.c`, `ncc_extra_presets.c`, `ncc_formant_math.c`, `ncc_noise.c`,
  `tests/ncc_extra_engines.c` en `tests/ncc_extra_cpu.c`. De engine-interface en submodule zijn ongewijzigd.
- 46 nieuwe NCC-patches: PHYS/GRAIN/VOICE elk 8, LOFI/WHEEL/NOISE elk 6, SAMPLE 4. Samen met 34 extra bestaande
  factorysounds brengt dit de lijst van 81 op 161. Bestaande categorieën gebruikt; geen nieuwe FX-categorie.
  Nieuwe sounds laden nog steeds met delay- en reverb-send op nul. Beschikbare samplebronnen blijven dezelfde.
- Bibliotheekruimte: 384 entries, met compilecontrole voor de reserve en hostcontrole met 32 bankvoices en alle
  gebruikersslots. Ook de ruimte voor alle 48 gebruikersloops is meegenomen, voor wanneer LOOP terugkomt.
- VOICE: de eerste formant gebruikt nu twee 32-bit delingen met exact dezelfde gehele uitkomst; 100.000 vergelijkingen
  met de oorspronkelijke berekening slagen. Geen 64-bit deling op het NCC-pad.
- NOISE: de nieuwe RADIO PAD bracht een bestaande signed overflow in de roze-ruispolen aan het licht. NCC gebruikt
  bredere vermenigvuldigingen in die polen, kleurmenging en filter-/gainberekeningen. De originele submodule blijft
  intact; alle 113 NCC- en 98 Felucca-audiogoldens blijven gelijk. Tests belasten alle vier bronnen en kleurgrenzen.
- `sh tools/ncc_dev.sh test`: beide NCC-varianten, Manager, editor, runner en regressie slagen. De bestaande twee
  Mac CPU-afwijkingen worden zichtbaar en binnen de eerder afgesproken grens geaccepteerd. Signed DSP-left-shifts
  blijven de bestaande UBSan-uitzondering; overige sanitizerfouten zijn fataal. Geen goldens/baselines bijgewerkt.
- Alle 56 macro’s veranderen audio tegenover een deterministisch herhaalde referentie. CC’s blijven per part, ook
  met Dual CH4. Alle 49 nieuwe-engineparen leveren begrensde audio bij live CC’s, houden de PHYS/GRAIN-stemlimiet
  aan en stoppen op panic. LOFI’s niet-pulsgolven en STEP hebben aparte controles.
- `sh tools/ncc_dev.sh emu-test`: de echte Wasm-panel-/MIDI-paden selecteren alle zeven engines en controleren alle
  acht macro’s, EDIT, Dual en droge soundloads. Screenshot: `build/emu-demo/performance-phys.png`.
- CPU-log `build/host/ncc_extra_cpu.log` vergelijkt zware paren op de Mac. Inclusief idle: FM6+FM6 circa 1809
  instructies/sample, PHYS+GRAIN 1525, PHYS+PHYS 1809, GRAIN+GRAIN 1224, FM6+PHYS 1829. Dit zijn hostmetingen;
  ze bewijzen geen deadline op de AC791N.
- Firmware: `build/ncc.fwsc`, FM-1_700; image **576728 B**, RAM **61704 / 73728 B**, pool **359464 / 368640 B**.
  Buildlog: `build/ncc-build.log`. SHA-256: `f5b0b041a7906ed590e13e4c44fad83316cf337f1460da8e32885ee6e58f3ec2`.
- Nog op de echte FM-1: luisteren naar patches/overgangen, Circuit-knoppen en zware Dual-combinaties uitproberen.
  De firmware is met expliciet akkoord geflasht; alleen-lezen controle op de FM-1 bevestigt alle twaalf engines en
  161 ingebouwde sounds (193 entries inclusief de DX7-bank). Log: `build/ncc-device-check-smooth.json`.
  Broncode en documentatie staan op `main`. De publieke versie is [NCC 0.4](https://github.com/fenderberg/fm1-nc-companion/releases/tag/v0.4),
  met release-identiteit `FM-1_704`; de bovenstaande devbuild blijft op Remco’s apparaat staan.
  Releasepakket: 576724 B image, zelfde RAM/pool; details in [HANDOFF.md](HANDOFF.md).
  Een volgende flash of publicatie vraagt opnieuw expliciet OK.

DRUM en SLICE zijn eveneens te koppelen, maar blijven hier buiten het voorstel: de Circuit heeft al drums en
SLICE vraagt extra samplevoorbereiding. LOOP blijft zoals afgesproken verborgen; DIGITAL is een vervangen engine.
