/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (FELUCCA_LIVEUI=1): SAVE keeps the focused part's sound in the user bank (Felucca's 32 slots, upreset.c: the
 * record and its format as they are, up_store_cat). Two presses, so a stray tap never writes:
 *   SAVE          the question on the foot for NS_ASK_MS: "OVERWRITE? HOLD: NEW" on a user sound, else (a factory,
 *                 NCC or DX7 sound: nothing of its own to overwrite) "HOLD SAVE: NEW SOUND"
 *   SAVE again    a tap (on release): the user sound overwritten in its slot, its name and category kept (on another
 *                 sound the question again); held LV_PAGE_HOLD_MS: a new sound in the first free slot, named after
 *                 the one it came from ("SOFT PAD" -> "SOFT PAD 2", a copy of that "SOFT PAD 3"), with the category
 *                 the sound has now (nl entry's), so it is listed there and under SELECT's USER (ncc_library.c)
 *   another button, or the question left NS_ASK_MS: no save. The keys play on meanwhile.
 * The part then plays the saved sound (t->user), so the next save overwrites it. Written at once, also while the
 * Circuit plays (Remco's choice, 2026-11-15: up_anytime): the erase stops the audio for ~40 ms per sector (the record's
 * bank, an FM6 sound's patch store too), the clock catches up after. Renaming and erasing: the Manager.
 * Main loop only. */
#define NS_ASK_MS 4000u
static int up_store_cat(uint32_t k, const char *name, uint32_t cat);

static struct {
    uint8_t ask, down;                  /* the question is open / SAVE down again (1, 2: held, done) */
    uint8_t shown;                      /* lv.say_n of the question on the foot */
    const char *q;                      /* .. its text */
    uint32_t ask_ms, down_ms;
} ns;

/* the NC_* category -> the record's CAT_* (category.c, stored); ALL (none known) and USER as OTHER */
static const uint8_t NS_CAT[NC_N] = {CAT_OTHER, CAT_BASS, CAT_PAD, CAT_LEAD, CAT_KEYS, CAT_PLUCK, CAT_DRUM, CAT_OTHER,
                                     CAT_OTHER};

static int ns_name_used(const char *s)                  /* a user slot already named s (upper case) */
{
    char b[16];
    uint32_t k;
    for (k = 0; k < UP_SLOTS; k++)
        if (up_used(k)) {
            up_name(k, b);
            if (str_eq(b, s))
                return 1;
        }
    return 0;
}

/* a new name from the sound's (base, any case): a copy's number off ("SOFT PAD 2" -> "SOFT PAD"; one or two digits
 * after a space: "ACID 303" keeps its 303), then the first " 2" .. " 99" no slot has, the base cut to fit 12; b holds
 * 16 */
static void ns_new_name(char *b, const char *base)
{
    char s[16], num[4];
    uint32_t n, i, len, cut;
    for (i = 0; i < 12u && base[i]; i++)
        s[i] = (char)nl_up((uint8_t)base[i]);
    s[i] = 0;
    for (len = i; len && s[len - 1u] >= '0' && s[len - 1u] <= '9'; len--)
        ;
    if (len < i && i - len <= 2u && len && s[len - 1u] == ' ')   /* a copy's number: off */
        s[len - 1u] = 0;
    for (len = str_len(s); len && s[len - 1u] == ' '; len--)
        s[len - 1u] = 0;
    if (!s[0])
        str_cpy(s, "SOUND", sizeof s);
    for (n = 2; n < 100u; n++) {
        num[0] = ' ';
        if (n >= 10u)
            num[1] = (char)('0' + n / 10u), num[2] = (char)('0' + n % 10u), num[3] = 0;
        else
            num[1] = (char)('0' + n), num[2] = 0;
        cut = 12u - str_len(num);
        str_cpy(b, s, cut + 1u);
        for (len = str_len(b); len && b[len - 1u] == ' '; len--)
            b[len - 1u] = 0;
        str_cpy(b + str_len(b), num, 4);
        if (!ns_name_used(b))
            return;
    }
}

static void ns_ask(const char *q)                       /* the question on the foot, NS_ASK_MS from now */
{
    lv_say(q, "");
    ns.q = q;
    ns.shown = lv.say_n;
    ns.ask = 1;
    ns.ask_ms = fm1_ms;
}

/* the focused part's sound -> its own slot (fresh 0) or a new one (fresh 1) */
static void ns_save(uint32_t fresh)
{
    track_t *t = TSEL;
    uint32_t part = song.sel, u = user_of(t), k, cat;
    char name[16], base[16];
    int32_t f;
    int rc;
    nl_build();
    f = nl_find(part, lv_juno(part));
    cat = f >= 0 ? nl.s[f].cat : NC_OTHER;
    if (!fresh && u < UP_SLOTS) {
        k = u;
        up_name(k, name);
        cat = up_cat(k) ? up_cat(k) : NS_CAT[cat % NC_N];
    } else {
        for (k = 0; k < UP_SLOTS && up_used(k); k++)
            ;
        if (k >= UP_SLOTS) {
            lv_say("USER BANK FULL", "");
            return;
        }
        if (f >= 0)
            nl_name((uint32_t)f, base);
        else
            base[0] = 0;
        ns_new_name(name, base);
        cat = NS_CAT[cat % NC_N];
    }
    up_anytime = 1;
    rc = up_store_cat(k, name, cat);
    up_anytime = 0;
    if (rc == 0 || rc == 3) {
        t->user = (uint8_t)(k + 1u);                    /* (the part plays it now: the next save overwrites it) */
        lv_say("SAVED: ", name);
    } else
        lv_say("SAVE FAILED", "");
}

/* SAVE's two presses (live_input, every pass) */
static void ncc_save_input(uint32_t pressed, uint32_t buttons)
{
    uint32_t m = 1u << panel.btn[B_SAVE];
    if (ns.ask && (pressed & ~m))                       /* another button: no save */
        ns.ask = ns.down = 0;
    if ((pressed & m) && song.sel < LV_PARTS) {
        if (!ns.ask) {
            ns_ask(user_of(TSEL) < UP_SLOTS ? "OVERWRITE? HOLD: NEW" : "HOLD SAVE: NEW SOUND");
            return;
        }
        ns.down = 1;
        ns.down_ms = fm1_ms;
    }
    if (ns.down == 1u && fm1_ms - ns.down_ms >= LV_PAGE_HOLD_MS) {
        ns.down = 2;                                    /* held: a new sound, once */
        ns.ask = 0;
        ns_save(1);
    }
    if (ns.down && !(buttons & m)) {
        if (ns.down == 1u) {
            if (user_of(TSEL) < UP_SLOTS) {
                ns.ask = 0;
                ns_save(0);
            } else
                ns_ask("HOLD SAVE: NEW SOUND");         /* (nothing of its own to overwrite: ask again) */
        }
        ns.down = 0;
    }
    if (!ns.ask)
        return;
    if (!ns.down && fm1_ms - ns.ask_ms >= NS_ASK_MS) {
        ns.ask = 0;
        lv_say("SAVE CANCELLED", "");
    } else if (lv.say && lv.say_n == ns.shown)
        lv.pop_ms = fm1_ms | 1u;                        /* the question stays on the foot while it is open */
    else if (!lv.pop_ms) {                              /* (a knob's popup came over it and went: back) */
        lv_say(ns.q, "");
        ns.shown = lv.say_n;
    }
}
