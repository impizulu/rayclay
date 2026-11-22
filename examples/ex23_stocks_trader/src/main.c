/*
    main.c - RayClay Markets: a charting terminal with a simulated brokerage account.

    Shows: a custom titlebar that carries real controls (.noDrag), a candlestick chart
    with volume, indicators, drawings and a crosshair in one custom-draw element, tables
    that drop columns instead of scrolling sideways, a live order book, a working order
    ticket with a confirm dialog, and one source that folds from a trading desk to a
    phone. The market is simulated; the clock under the chart is the device's, through
    rcLocalTime, and the Account tab's other currencies are real: the European Central
    Bank's daily reference rates, read through rcFetch and rcJsonParse from Frankfurter
    (frankfurter.dev, no key), and only once a currency is picked.

    No assets to ship: the bundled font and generated icons. Build and run the
    rayclay_ex23_stocks_trader target, or `cmake --preset web` for the browser.
    RAYCLAY_MAX_FRAMES=N draws N frames headless and exits.
*/
#include "app/trader_app.h"

/* What the app core keeps, beside the one request only this file makes. */
typedef struct {
    AppState state;
    RC_Fetch fx;        /* the rates request in flight, 0 when there is none */
} Runner;

static bool same_text(RC_String a, const char *b) {
    int i = 0;

    while (i < a.length && b[i] && a.chars[i] == b[i])
        i++;
    return i == a.length && !b[i];
}

/* Fill the core's TrFx from the answer: one {date, base, quote, rate} object per currency.
   Every currency asked for must come back with a positive rate, or none is shown. */
static void fx_read(TrFx *fx, RC_FetchResult r) {
    RC_Json doc = rcJsonParse(r.text, r.length);
    int found = 0;

    /* The answer is the network's word. A rate counts only finite and under a million to the
       dollar (1e999 parses as infinity, and the account in it would overflow its integer), and
       a currency counts once however many rows name it. */
    for (int c = 1; c < TR_FX_COUNT; c++)
        fx->rate[c] = 0.0;
    for (int i = 0; i < rcJsonCount(doc); i++) {
        RC_Json row = rcJsonAt(doc, i);
        RC_String quote = rcJsonText(row, "quote");
        double rate = rcJsonNumber(row, "rate", 0.0);

        for (int c = 1; c < TR_FX_COUNT; c++)
            if (rate > 0.0 && rate < 1e6 && same_text(quote, TR_FX_CODES[c]))
                fx->rate[c] = rate;
        rcStrCopy(fx->date, rcJsonString(row, "date", ""), sizeof fx->date);
    }
    rcJsonFree(doc);
    for (int c = 1; c < TR_FX_COUNT; c++)
        found += fx->rate[c] > 0.0;
    fx->state = r.ok && found == TR_FX_COUNT - 1 ? TR_FX_READY : TR_FX_FAILED;
}

/* One request when the core asks, polled every frame until it ends, then freed. The ECB
   publishes once a working day, so one answer serves the whole session. */
static void fx_poll(Runner *runner, RC_App *app) {
    TrFx *fx = &runner->state.fx;
    RC_FetchResult r;

    if (fx->want && !runner->fx) {
        const char *url = rcFormat(rcAppArena(app),
            "https://api.frankfurter.dev/v2/providers/ecb/rates?base=%s&quotes=%s,%s,%s,%s,%s",
            TR_FX_CODES[0], TR_FX_CODES[1], TR_FX_CODES[2], TR_FX_CODES[3], TR_FX_CODES[4],
            TR_FX_CODES[5]).chars;

        runner->fx = rcFetch(url, .timeoutSeconds = 10);
        fx->state = TR_FX_LOADING;
    }
    fx->want = false;
    r = rcFetchResult(runner->fx);
    if (r.state == RC_FETCH_PENDING || r.state == RC_FETCH_NONE)
        return;
    if (r.state == RC_FETCH_DONE)
        fx_read(fx, r);
    else
        fx->state = TR_FX_FAILED;
    rcFetchFree(runner->fx);
    runner->fx = 0;
}

static void demo_update(RC_App *app, void *userData) {
    Runner *runner = (Runner *)userData;
    AppState *st = &runner->state;
    AppCtx ctx;

    if (!st->seeded)
        trader_seed(st, 0x517A7Eu);
    /* The range bar's clock is the device's own: the market is simulated, the time is not.
       rcLocalTime zeroes it when the system cannot answer, and the bar shows the market's. */
    rcLocalTime(rcUnixTimeSeconds(), &st->localNow);
    fx_poll(runner, app);
    /* ctx.dt is the true delta, park included. A window back from minimized resumes the
       simulated market where it stopped rather than fast-forwarding it in one jump; a slow
       frame still counts in full. */
    ctx = app_demo_ctx(app);
    if (ctx.dt > 0.25f)
        ctx.dt = 0.25f;
    trader_update(st, &ctx);
    /* The feed moves TR_TICKS_PER_SEC times a second of market time; between ticks only the
       quote flashes and the newest close's ring move, and 30 frames a second carry them.
       So the window draws on its own clock at most that often and sleeps in between, where
       drawing every display refresh would cost a whole desk per vsync for no new picture. */
    const double q = st->store.clock * TR_TICKS_PER_SEC;
    const double tick = (1.0 - (q - (double)(int64_t)q)) / (TR_TICKS_PER_SEC * st->store.speed);
    rcWindowRequestFrameAfter(rcAppMainWindow(app), tick < 1.0 / 30.0 ? tick : 1.0 / 30.0);
}

static void demo_layout(RC_App *app, void *userData) {
    AppState *st = &((Runner *)userData)->state;
    AppCtx ctx = app_demo_ctx(app);

    trader_layout(st, &ctx);
    /* The window's clear color is a snapshot from create, not a live link to the style,
       so a theme switch needs this line too. It is change-gated inside the library, and
       the toggle flips inside the layout, so call it every frame. */
    rcWindowSetClearColor(rcAppMainWindow(app), rcGetStyle().background);
}

int main(void) {
    static Runner runner;
    /* Slot 0 is what controls, menus, tooltips and chart text draw in, so it is the
       app's body size. Every other size is written where it is used. */
    static const float bodySize[] = { (float)TR_BODY_SIZE };

    rcSetStyle(trader_style(true));

    RC_AppOptions opts = {
        .width               = 1280,
        .height              = 820,
        .title               = "RayClay Markets",
        .clearColor          = rcGetStyle().background,
        .fontSizes           = bodySize,
        .fontCount           = 1,
        .startLayoutElements = 8192,
        .frame               = RC_WINDOW_FRAME_NONE,
        .updateCallback      = demo_update,
        .layoutCallback      = demo_layout,
        .userData            = &runner,
        .titlebar            = { .height = TR_TOPBAR_H },
    };

    return rcRunApp(&opts);
}
