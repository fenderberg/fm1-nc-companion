/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (Novation Circuit Companion, FELUCCA_LIVEUI=1): the sound library. Every sound of NCC's five engines in one list
 * (LV_ENGS' order: FM6's presets and its voice bank, NCC's Juno sounds and ANALOG's presets, PHASE, TRIO, ACID; each
 * engine's user presets after its own), each with a category by what it is for: BASS, PAD, LEAD, KEYS, PLUCK (OTHER: a
 * user sound or a bank voice whose name does not tell). On SOUND, SELECT filters by category and ALGORITHM by engine
 * (each with ALL), PRESETS steps through what is left; a sound of another engine switches the part to it (live_ui.c).
 *
 * The category comes from the sound's name (nl_cat: a keyword at the start of a word, BASS / SUB / 303 / ACID ->
 * BASS, PAD / STR / CHOIR -> PAD, LEAD / HOOVER / BRASS -> LEAD, EP / KEY / ORGAN -> KEYS, PLUCK / BELL / MARIMBA ->
 * PLUCK ..), else the engine's own (ACID: BASS), else OTHER: one rule for the factory sounds (tests/ncc_test.c: none of
 * them OTHER), the user presets and a loaded DX7 bank alike, and no table to keep in step with the engines.
 *
 * Main loop only. The list is built when it is used (nl_build: ~100 entries, from the presets in flash and the user
 * presets' RAM mirror); the bank voices' names are read from flash once per bank (nl.bsig). */

enum { NC_ALL, NC_BASS, NC_PAD, NC_LEAD, NC_KEYS, NC_PLUCK, NC_OTHER, NC_N };
static const char *const NC_NAME[NC_N] = {"ALL", "BASS", "PAD", "LEAD", "KEYS", "PLUCK", "OTHER"};
enum { NLK_PRESET, NLK_JUNO, NLK_BANK, NLK_USER };
typedef struct {
    uint8_t kind, eng, k, cat;          /* NLK_*, ENGINES[] index, the preset / Juno sound / bank voice / user slot */
} nl_snd_t;
#define NL_MAX 128u                     /* (factory 51 + bank 32 + user presets of these engines) */
#define NL_NAME 11u                     /* a bank voice's name: 10 ASCII + 0 */

static const struct {
    const char *kw;
    uint8_t cat;
} NL_KW[] = {                           /* the first that starts a word of the name wins */
    {"BASS", NC_BASS}, {"SUB", NC_BASS}, {"303", NC_BASS}, {"ACID", NC_BASS},
    {"PAD", NC_PAD}, {"STR", NC_PAD}, {"CHOIR", NC_PAD}, {"SWEEP", NC_PAD}, {"WIND", NC_PAD}, {"ATMO", NC_PAD},
    {"VOX", NC_PAD},
    {"LEAD", NC_LEAD}, {"HOOVER", NC_LEAD}, {"SCREAM", NC_LEAD}, {"RAVE", NC_LEAD}, {"WIRE", NC_LEAD},
    {"SYNC", NC_LEAD}, {"SOLO", NC_LEAD}, {"BRASS", NC_LEAD}, {"BRS", NC_LEAD}, {"HORN", NC_LEAD}, {"RESO", NC_LEAD},
    {"EP", NC_KEYS}, {"KEY", NC_KEYS}, {"TINE", NC_KEYS}, {"ORGAN", NC_KEYS}, {"POLY", NC_KEYS}, {"PIANO", NC_KEYS},
    {"RHODE", NC_KEYS}, {"CLAV", NC_KEYS},
    {"PLUCK", NC_PLUCK}, {"BELL", NC_PLUCK}, {"MARIMBA", NC_PLUCK}, {"CHIME", NC_PLUCK}, {"HARP", NC_PLUCK},
    {"MALLET", NC_PLUCK}, {"VIBE", NC_PLUCK}, {"GLOCK", NC_PLUCK}, {"KALIMBA", NC_PLUCK}, {"GUITAR", NC_PLUCK},
    {"GTR", NC_PLUCK},
};

static struct {
    uint32_t n;                         /* entries in s (nl_build) */
    nl_snd_t s[NL_MAX];
    uint32_t bsig;                      /* the bank whose names bname holds (fm6_bank_used + 1; 0: none read) */
    char bname[FM6_NBANK][NL_NAME];
} nl;

static uint32_t nl_up(uint32_t c) { return c >= 'a' && c <= 'z' ? c - 32u : c; }
static int nl_letter(uint32_t c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }

/* the category of a sound named s on engine e */
static uint32_t nl_cat(const char *s, uint32_t e)
{
    uint32_t r, i, j;
    for (r = 0; r < NELEM(NL_KW); r++)
        for (i = 0; s[i]; i++) {
            const char *kw = NL_KW[r].kw;
            if (i && nl_letter(nl_up((uint8_t)s[i - 1])))
                continue;                               /* (only where a word starts) */
            for (j = 0; kw[j] && nl_up((uint8_t)s[i + j]) == (uint8_t)kw[j]; j++)
                ;
            if (!kw[j])
                return NL_KW[r].cat;
        }
    return e == ENGI_ACID ? NC_BASS : NC_OTHER;
}

/* the names of the bank's voices (once per bank: a flash read each) */
static void nl_bank_names(void)
{
    uint8_t pk[FM6_PACKED];
    uint32_t k, i;
    if (nl.bsig == fm6_bank_used + 1u)
        return;
    nl.bsig = fm6_bank_used + 1u;
    for (k = 0; k < FM6_NBANK; k++) {
        nl.bname[k][0] = 0;
        if (!((fm6_bank_used >> k) & 1u) || !fm6_bank_read || fm6_bank_read(k, pk))
            continue;
        for (i = 0; i < 10u; i++)
            nl.bname[k][i] = (char)(pk[118 + i] >= 32u && pk[118 + i] < 127u ? pk[118 + i] : ' ');
        for (i = 10u; i && nl.bname[k][i - 1u] == ' '; i--)
            ;
        nl.bname[k][i] = 0;
    }
}

static void nl_add(uint32_t kind, uint32_t e, uint32_t k, const char *name)
{
    if (nl.n < NL_MAX)
        nl.s[nl.n++] = (nl_snd_t){(uint8_t)kind, (uint8_t)e, (uint8_t)k, (uint8_t)nl_cat(name, e)};
}

/* the list, as it is now */
static void nl_build(void)
{
    uint32_t i, k;
    char b[16];
    nl.n = 0;
    nl_bank_names();
    for (i = 0; i < NELEM(LV_ENGS); i++) {
        uint32_t e = LV_ENGS[i];
        const engine_t *en = ENGINES[e];
        if (e == ENGI_ANALOG)
            for (k = 0; k < NCC_NJUNO; k++)
                nl_add(NLK_JUNO, e, k, NCC_JUNO[k].name);
        for (k = 0; k < en->npresets; k++)
            if (preset_orig(en, k) == k)
                nl_add(NLK_PRESET, e, k, en->presets[k].name);
        if (e == ENGI_FM6)
            for (k = 0; k < FM6_NBANK; k++)
                if ((fm6_bank_used >> k) & 1u)
                    nl_add(NLK_BANK, e, k, nl.bname[k]);
        for (k = 0; k < UP_SLOTS; k++)
            if (up_used(k) && up_engine(k) == e) {
                up_name(k, b);
                nl_add(NLK_USER, e, k, b);
            }
    }
}

/* entry i passes the filters: category c (NC_ALL any), engine position ep in LV_ENGS + 1 (0 any) */
static int nl_fits(uint32_t i, uint32_t c, uint32_t ep)
{
    return i < nl.n && (!c || nl.s[i].cat == c) && (!ep || nl.s[i].eng == LV_ENGS[ep - 1u]);
}
static uint32_t nl_count(uint32_t c, uint32_t ep)
{
    uint32_t i, n = 0;
    for (i = 0; i < nl.n; i++)
        n += (uint32_t)nl_fits(i, c, ep);
    return n;
}

/* the entry part's sound is (nl_build first), -1 none (an engine NCC does not offer) */
static int32_t nl_find(uint32_t part, uint32_t juno)
{
    const track_t *t = &trk[part];
    uint32_t i, e = eng_idx(t->eng_req), u = user_of(t), kind, k;
    int32_t bank = e == ENGI_FM6 ? (int32_t)t->p[P_E7] - (int32_t)FM6_OWN - 1 : -1;
    if (juno)
        kind = NLK_JUNO, k = juno - 1u;
    else if (u < UP_SLOTS)
        kind = NLK_USER, k = u;
    else if (bank >= 0 && bank < (int32_t)FM6_NBANK)
        kind = NLK_BANK, k = (uint32_t)bank;
    else
        kind = NLK_PRESET, k = ENGINES[e]->npresets ? preset_orig(ENGINES[e], t->preset % ENGINES[e]->npresets) : 0u;
    for (i = 0; i < nl.n; i++)
        if (nl.s[i].kind == kind && nl.s[i].eng == e && nl.s[i].k == k)
            return (int32_t)i;
    return -1;
}

/* entry i on the focused part (its engine first when it is another's) */
static void nl_load(uint32_t i)
{
    const nl_snd_t *s = &nl.s[i];
    track_t *t = TSEL;
    if (s->kind == NLK_JUNO) {
        ncc_juno_load(t, s->k);
        return;
    }
    if (s->kind == NLK_USER) {
        up_load(s->k);
        return;
    }
    if (t->eng_req != s->eng)
        set_engine_of(t, s->eng);                       /* (Felucca's fade) */
    if (s->kind == NLK_BANK) {                          /* FM6's preset 0 (the patch as it is), the bank voice */
        apply_preset_to(t, 0);
        t->p[P_E7] = (int16_t)(FM6_OWN + 1u + s->k);
    } else
        apply_preset_to(t, s->k);
}
