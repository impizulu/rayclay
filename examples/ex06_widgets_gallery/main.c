/*  main.c - RayClay widgets gallery

    Every widget RayClay ships, live, in one file meant to be copied from. STORIES is the whole
    catalog: a row gives a widget's page, name, a line of code and the function that draws it,
    and the rail, the search and the grid all read that table. To see how a widget is spelled,
    find its story_ function; nothing stands between you and the calls.

    The Appearance panel rewrites RC_Style on the spot: a theme is plain data, and the widgets
    here take their colors and corners from it, save where a class names its own. A class
    styles a box or a control as CSS does, with a look for each state (hover:, active:) and a
    control's mark in accent-<color>; one that names the theme's roles (bg-surface-alt,
    hover:bg-border) follows the theme too.

    Build target: rayclay_ex06_widgets_gallery.  */

#include "rayclay.h"

#include "icons/rc_icons_bell.h"
#include "icons/rc_icons_chart_column.h"
#include "icons/rc_icons_check.h"
#include "icons/rc_icons_circle_play.h"
#include "icons/rc_icons_code.h"
#include "icons/rc_icons_download.h"
#include "icons/rc_icons_copy.h"
#include "icons/rc_icons_file_text.h"
#include "icons/rc_icons_folder.h"
#include "icons/rc_icons_image.h"
#include "icons/rc_icons_moon.h"
#include "icons/rc_icons_panel_left.h"
#include "icons/rc_icons_panel_right.h"
#include "icons/rc_icons_rayclay_logo.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_square_pen.h"
#include "icons/rc_icons_sun.h"
#include "icons/rc_icons_trash_2.h"
#include "icons/rc_icons_upload.h"
#include "icons/rc_icons_user.h"
#include "icons/rc_icons_users.h"

/* Both resolve against the working directory: the repository root, or the program's folder. */
#ifndef RC_DEMO_LOGO
#define RC_DEMO_LOGO "examples/assets/logos/rayclay-logo-1024.png"
#endif
#ifndef RC_DEMO_MONO
#define RC_DEMO_MONO "examples/assets/fonts/AdwaitaMono-Regular.ttf"
#endif

enum { P_CONTROLS, P_OVERLAYS, P_DATA, P_LAYOUT, P_STYLE, PAGE_COUNT };
enum { RAIL_W = 232, BAND_H = 52, GAP = 16, ZOOM_N = 96 };

/* One app, one state: the stories below read and write it directly. */
static struct {
    RC_App *app;
    RC_Window *win;
    RC_FileDialogToken pick;   /* the one file dialog while it is open, else 0 */
    uint16_t mono;
    bool dark, drawer, alerts, digest, wifi, sync, modal, popover, uploading;
    bool brushing, deleted, noDialog;
    int page, accent, jump, current, copied, plan, region, tags, rowPick, listPick, sent, pickFor;
    double now, copiedUntil, linkedUntil, upStart;
    char query[32], name[48], email[64], password[32], invite[64], invited[64], notes[256];
    char upName[64], picked[256];   /* the file the progress story sends; the dialogs' answer */
    const char *pressed, *menuPick, *fileAct, *tipPick;
    float radius, volume, quality, split, lo, hi, brushA, zoomY[ZOOM_N];
    RC_Image logo;
} st = { .alerts = true, .wifi = true, .jump = -1, .current = -1, .copied = -1,
         .plan = 1, .region = -1, .rowPick = -1, .listPick = -1, .sent = 42,
         .notes = "The editor works over your buffer: a plain char array.\n\n"
                  "Drag to select, double-click a word; select all, copy and paste work.",
         .upName = "report-q3.pdf", .picked = "Nothing chosen yet", .pressed = "nothing yet",
         .menuPick = "nothing yet", .fileAct = "nothing yet", .tipPick = "nothing yet",
         .radius = 8.0f, .volume = 65.0f, .quality = 40.0f, .split = 0.35f, .hi = ZOOM_N - 1 };

static const RC_Style *s;   /* the active theme, set each frame */
static RC_Color accentInk;  /* the accent as text, lines and outlines draw it, set each frame */

/* Each accent's primary, its hover and its ink on the dark theme. A white label reads at 4.5:1
   or more on the primary and on the hover, so the hover is the darker step (teal and orange
   start one step darker still). Those read dim on a dark surface, so there the ink is light. */
static const uint32_t ACCENTS[5][3] = {
    { 0x7c3aed, 0x6d28d9, 0xa78bfa }, { 0x2563eb, 0x1d4ed8, 0x60a5fa },
    { 0x0f766e, 0x115e59, 0x2dd4bf }, { 0xdb2777, 0xbe185d, 0xf472b6 },
    { 0xc2410c, 0x9a3412, 0xfb923c } };

static RC_Style app_style(bool dark, int accent, float radius) {
    RC_Style t = dark ? rcStyleDark() : rcStyleLight();

    t.background = dark ? rcHex(0x101013) : RC_ZINC_100;
    t.chrome = dark ? RC_ZINC_950 : RC_WHITE;
    t.surface = dark ? RC_ZINC_900 : RC_WHITE;
    t.surfaceAlt = dark ? RC_ZINC_800 : RC_ZINC_100;
    t.border = dark ? RC_ZINC_700 : RC_ZINC_300;
    t.text = dark ? RC_ZINC_50 : RC_ZINC_900;
    t.textMuted = dark ? RC_ZINC_400 : RC_ZINC_600;
    t.primary = rcHex(ACCENTS[accent][0]);
    t.primaryHover = rcHex(ACCENTS[accent][1]);
    t.radius = radius;
    return t;
}

/* printf into the frame arena, so the text lives until the frame is drawn. RC_PRINTF_FMT has
   the compiler check each call's arguments against its format, as it does rcFormat's. */
static RC_PRINTF_FMT(1, 2) const char *fmt(const char *format, ...) {
    va_list args;

    va_start(args, format);
    RC_String text = rcFormatV(rcAppArena(st.app), format, args);
    va_end(args);
    return text.chars;
}

/* A note may hold a path (the file dialogs' answer), which has no space to wrap at: "w" breaks
   such a word at the card's edge and leaves every other word whole. */
static void note(const char *t) { rcTextC(t, .color = s->textMuted, .size = 12, .wordBreak = "w"); }

/* A label and a value on one line; the values of a stack of these read as a column. A label may
   be a file's name, one long word with no space to wrap at: "a" lets the row squeeze it and
   break it anywhere, so the value keeps its place at the end of the line. */
static void readout(const char *label, const char *value) {
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcTextC(label, .color = s->textMuted, .size = 13, .wordBreak = "a");
        rcSpacer() {}
        rcTextC(value, .color = s->text, .size = 13);
    }
}

static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

static bool contains(const char *text, const char *query) {
    for (; *text; text++) {
        int i = 0;
        while (query[i] && lower(text[i]) == lower(query[i])) i++;
        if (!query[i]) return true;
    }
    return !query[0];
}

/* The system's file dialogs, asked from the click's own frame as the web requires. One is open at a
   time, so one token serves the app; who asked: -1 an Upload, 0 to 3 the dialogs story. A save
   hands its bytes to a place the user picks. collect() reads the answer. */
static void ask(int who) {
    static const char NOTE[] = "Saved by the RayClay widgets gallery.\n";

    if (st.pick) return;
    st.pickFor = who;
    st.pick = who == 3 ? rcFileDialogSave("gallery-note.txt", NOTE, sizeof NOTE - 1)
            : rcFileDialogOpen(who == 1 ? RC_FILE_DIALOG_FILES
                               : who == 2 ? RC_FILE_DIALOG_FOLDER : RC_FILE_DIALOG_FILE, NULL);
}

static void upload(void) { ask(-1); }

static const char *base_name(const char *path) {
    const char *name = path;

    for (const char *c = path; *c; c++) if (*c == '/' || *c == '\\') name = c + 1;
    return name;
}

/* Each answer is handed over once. A chosen file names the upload, or the story says what came
   back; then its paths are released (on a desktop they are the user's own, left alone; on the
   web and phones RayClay's copies). UNSUPPORTED: this host has no dialog, so the asks go. */
static void collect(void) {
    static const char *const SAID[] = { "", "", "", "Canceled: nothing changed",
                                        "Refused: ask again from a click",
                                        "Failed: the log says why" };
    RC_FileDialogResult r = rcFileDialogPoll(st.pick);

    if (!st.pick || r.status == RC_FILE_DIALOG_PENDING) return;
    st.pick = 0;
    st.noDialog = r.status == RC_FILE_DIALOG_UNSUPPORTED;
    if (st.pickFor < 0 && r.count > 0) {
        rcStrCopy(st.upName, base_name(r.paths[0]), sizeof st.upName);
        st.uploading = true;
        st.upStart = st.now;
    } else if (st.pickFor >= 0 && r.status != RC_FILE_DIALOG_UNSUPPORTED) {
        rcStrCopy(st.picked, r.status != RC_FILE_DIALOG_DONE ? SAID[r.status]
                  : r.folder ? fmt("%d files in %s", r.count, r.folder)
                  : r.count > 1 ? fmt("%d files, first %s", r.count, r.paths[0])
                  : r.count ? r.paths[0] : "Saved where you chose", sizeof st.picked);
    }
    if (r.folder) rcReleaseFile(r.folder);
    else for (int i = 0; i < r.count; i++) rcReleaseFile(r.paths[i]);
}

/* A button's .className is its look, read in each of its states. Merge is a primary button
   whose one string recolors it: fill, corners and a shade for hover and press. Upload and the
   bell write "soft", a look the gallery names once at start-up (update()), as an app names a
   look it repeats; a default button draws a hairline, and border-transparent hides it and keeps
   its room, so the rows line up. The bell's px-2 makes its icon-only box square. */
static void story_button(void) {
    rcRow(.gap = 8, .align = "cl", .className = "flex-wrap") {
        if (rcButton("b1", "Save", .variant = RC_BTN_PRIMARY)) st.pressed = "Save";
        if (rcButton("b2", "Edit")) st.pressed = "Edit";
        if (rcButton("b3", "Delete", .variant = RC_BTN_DANGER)) st.pressed = "Delete";
    }
    rcRow(.gap = 8, .align = "cl", .className = "flex-wrap") {
        if (rcButton("b5", "Upload", .disabled = st.noDialog, .className = "soft",
                     .leadingIcon = rcIconUpload))
            upload();
        if (rcButton("b6", "", .className = "soft px-2", .leadingIcon = rcIconBell))
            st.pressed = "Bell";
        if (rcButton("b8", "Merge", .className = "rounded-full bg-emerald-700 "
                     "hover:bg-emerald-600 active:bg-emerald-800", .variant = RC_BTN_PRIMARY))
            st.pressed = "Merge";
    }
    rcRow(.gap = 8, .align = "cl", .className = "flex-wrap") {
        if (rcButton("b4", "Cancel", .variant = RC_BTN_GHOST)) st.pressed = "Cancel";
        rcButton("b7", "Disabled", .disabled = true, .variant = RC_BTN_PRIMARY);
    }
    note(fmt("Last pressed: %s", st.pressed));
}

static void story_field(void) {
    bool bad = st.email[0] && !contains(st.email, "@");

    rcTextInput("f1", st.name, sizeof st.name, .placeholder = "Full name", .clearButton = true);
    rcTextInput("f2", st.email, sizeof st.email, .placeholder = "Email",
                .inputKind = RC_INPUT_EMAIL, .clearButton = true);
    rcTextInput("f3", st.password, sizeof st.password, .placeholder = "Password",
                .inputKind = RC_INPUT_PASSWORD);
    rcTextC(bad ? "An email address needs an @" : "Tab moves between the fields.",
            .color = bad ? s->danger : s->textMuted, .size = 12);
}

static void story_area(void) { rcTextArea("n", st.notes, sizeof st.notes, .rows = 6); }

/* A disabled control shows a value nobody can change, so it gets its own, never a live one.
   accent-<color> in a control's class colors its mark, as CSS's accent-color does: the first
   box's checked fill here (the tick stays white). The rest take the theme's primary, and a
   disabled control keeps its muted mark, accent or not. */
static void story_check(void) {
    static bool managed = true;

    rcCheckbox("c1", "Email me about new sign-ins", &st.alerts, .className = "accent-emerald-600");
    rcCheckbox("c2", "Send a weekly digest", &st.digest);
    rcCheckbox("c3", "Share usage data (managed)", &managed, .disabled = true);
}

static void story_toggle(void) {
    static const char *const L[] = { "Wi-Fi", "Sync over cellular", "Airplane mode" };
    static bool airplane = false;
    bool *v[] = { &st.wifi, &st.sync, &airplane };

    for (int i = 0; i < 3; i++) rcToggle(fmt("t%d", i), L[i], v[i], .disabled = i == 2);
}

static void story_radio(void) {
    rcRadio("r1", "Starter, 3 seats", &st.plan, 0);
    rcRadio("r2", "Team, 20 seats", &st.plan, 1);
    rcRadio("r3", "Enterprise, ask sales", &st.plan, 2, .disabled = true);
}

static void story_slider(void) {
    readout("Volume", fmt("%.0f%%", st.volume));
    rcSlider("s1", &st.volume, 0.0f, 100.0f);
    readout("Quality, in steps of 5", fmt("%.0f", st.quality));
    rcSlider("s2", &st.quality, 0.0f, 100.0f, .step = 5.0f);
}

static void story_combo(void) {
    static const char *const R[] = { "Frankfurt", "Oregon", "Singapore", "Sao Paulo" };

    rcCombo("co1", &st.region, R, 4, .placeholder = "Choose a region");
    rcCombo("co2", NULL, R, 4, .disabled = true, .placeholder = "Managed by your admin");
    note(st.region >= 0 ? fmt("Deploying to %s", R[st.region]) : "Nothing chosen yet");
}

/* The pretend upload reads its percent off the clock, 60 points a second, so the frames that
   input or a resize add do not hurry it, and a late last frame still lands on 100. Nothing
   RayClay sees changes while it runs, so it asks for a frame every 60th of a second. */
static void story_progress(void) {
    if (st.uploading) {
        st.uploading = st.now - st.upStart < 100.0 / 60.0;
        st.sent = st.uploading ? (int)((st.now - st.upStart) * 60.0) : 100;
        rcWindowRequestFrameAfter(st.win, 1.0 / 60.0);
    }
    readout(st.upName, fmt("%d%%", st.sent));
    rcProgress("p1", (float)st.sent / 100.0f);
    if (rcButton("p2", st.uploading ? "Uploading (simulated)" : "Simulate an upload",
                 .disabled = st.uploading || st.noDialog,
                 .className = "self-start", .leadingIcon = rcIconUpload))
        upload();
}

static void story_menu(void) {
    rcRow(.gap = 8, .align = "cc") {
        if (rcBeginMenu("m1", "Sort", .caret = true)) {
            if (rcMenuItem("Newest")) st.menuPick = "Newest";
            if (rcMenuItem("Name")) st.menuPick = "Name";
            rcEndMenu();
        }
        if (rcBeginMenu("m2", "File", .leadingIcon = rcIconFolder)) {
            if (rcMenuItem("Upload", .disabled = st.noDialog, .leadingIcon = rcIconUpload))
                upload();
            rcMenuItem("Delete", .disabled = true, .className = "destructive",
                       .leadingIcon = rcIconTrash2);
            rcEndMenu();
        }
    }
    rcRow(.align = "cc") { note(fmt("Last chosen: %s", st.menuPick)); }
}

static void story_context(void) {
    rcRow(.id = "cx", .gap = 8, .align = "cc", .h = "grow", .hMin = RC_VAL(72),
          .className = "rounded-md bg-surface-alt") {
        rcIconFileText(16.0f, s->textMuted);
        rcTextC(rcPointerIsCoarse() ? "Long-press this file" : "Right-click this file",
                .select = RC_SELECT_NONE);
    }
    if (rcBeginContextMenu("cx_m", "cx")) {
        if (rcMenuItem("Rename", .leadingIcon = rcIconSquarePen)) st.fileAct = "Rename";
        if (rcMenuItem("Delete", .className = "destructive", .leadingIcon = rcIconTrash2))
            st.fileAct = "Delete";
        rcEndContextMenu();
    }
    rcRow(.align = "cc") { note(fmt("Last action: %s", st.fileAct)); }
}

static void story_modal(void) {
    rcRow(.align = "cc") {
        if (rcButton("d1", "Delete project", .variant = RC_BTN_DANGER)) st.modal = true;
    }
    rcRow(.align = "cc") { note(st.deleted ? "Deleted." : "Esc or a click outside cancels."); }
    if (rcBeginModal("dlg", &st.modal)) {
        rcTextL("Delete this project?", .font = rcFontSemibold(20.0f));
        rcTextL("Northwind Billing and its 12 environments go with it.", .color = s->textMuted);
        rcRow(.gap = 8, .align = "cr", .w = "grow") {
            if (rcButton("d3", "Keep it")) st.modal = false;
            if (rcButton("d4", "Delete", .variant = RC_BTN_DANGER)) {
                st.modal = false;
                st.deleted = true;
            }
        }
        rcEndModal();
    }
}

static void story_tooltip(void) {
    const RC_IconCallback icon[] = { rcIconBell, rcIconUser, rcIconSettings };
    static const char *const tip[] = { "3 unread mentions", "Billing admin", "Preferences" };

    rcRow(.gap = 12, .align = "cc") {
        for (int i = 0; i < 3; i++) {
            rcBox(.tooltip = tip[i]) {
                if (rcButton(fmt("tp%d", i), "", .leadingIcon = icon[i])) st.tipPick = tip[i];
            }
        }
    }
    rcRow(.align = "cc") {
        note(fmt("%s. Last clicked: %s", rcPointerIsCoarse() ? "Touch shows none"
                                                             : "Hover and wait", st.tipPick));
    }
}

/* A popover is a floating box attached to its button by id: out of the flow, above the page.
   RC_CLIP_NONE draws it over every clip, the clipping card's too, as the library's menus are. */
static void story_popover(void) {
    bool linked = st.now < st.linkedUntil;

    if (linked) rcWindowRequestFrameAfter(st.win, st.linkedUntil - st.now);   /* see card() */

    /* It opens on a click, never on load. The space under the header is kept for it, so it
       opens inside the card, over the document's text; Share, Invite and Esc close it. */
    rcColumn(.gap = 12, .w = "grow", .h = "grow", .hMin = RC_VAL(200)) {
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            rcIconFileText(20.0f, s->textMuted);
            rcColumn(.gap = 2, .w = "grow") {
                rcTextL("Q3 planning", .font = rcFontSemibold(14.0f));
                note(st.invited[0] ? fmt("Invited %s", st.invited)
                                   : "Edited 12 minutes ago by Dana Whitfield");
            }
            if (rcButton("po", "Share", .variant = RC_BTN_PRIMARY, .leadingIcon = rcIconUsers))
                st.popover = !st.popover;
        }
        rcTextL("1. Move billing to the new ledger\n2. Open the Singapore region\n"
                "3. Bring p95 latency under 200 ms", .size = 13);
    }
    if (st.popover) {
        rcColumn(.id = "po_p", .gap = 10, .p = 16, .w = "300px",
                 .shadow = { rcWithAlpha(st.dark ? RC_BLACK : RC_SLATE_400, 0.5f), 0, 8, 24, 0 },
                 .floating = { .to = RC_ATTACH_ELEMENT, .toId = "po",
                               .parent = RC_ANCHOR_BOTTOM_RIGHT, .element = RC_ANCHOR_TOP_RIGHT,
                               .offset = { 0, 8 }, .zIndex = 100, .clip = RC_CLIP_NONE },
                 .className = "rounded-lg border border-border bg-surface") {
            rcTextL("Share Q3 planning", .font = rcFontSemibold(14.0f));
            rcTextInput("po_mail", st.invite, sizeof st.invite,
                        .placeholder = "Add people by email", .clearButton = true);
            note("Anyone at Northwind with the link can view.");
            rcRow(.gap = 8, .align = "cr", .w = "grow") {
                if (rcButton("po_copy", linked ? "Copied" : "Copy link",
                             .leadingIcon = linked ? rcIconCheck : NULL)) {
                    rcClipboardSet("https://northwind.example/d/q3-planning");
                    st.linkedUntil = st.now + 2.5;
                }
                if (rcButton("po_send", "Invite", .disabled = !st.invite[0],
                             .variant = RC_BTN_PRIMARY)) {
                    rcStrCopy(st.invited, st.invite, sizeof st.invited);
                    st.invite[0] = '\0';
                    st.popover = false;
                }
            }
        }
    }
}

/* The dialogs a desktop app is expected to have: Open (one file, several, a folder) and Save as.
   Each button asks from its own click; the line under them is the answer. */
static void story_files(void) {
    static const char *const LABEL[] = { "Open file", "Open files", "Open folder", "Save as" };
    const RC_IconCallback icon[] = { rcIconFileText, rcIconFileText, rcIconFolder, rcIconDownload };

    if (!st.noDialog) {
        rcRow(.gap = 8, .align = "cl", .className = "flex-wrap") {
            for (int i = 0; i < 4; i++)
                if (rcButton(fmt("fd%d", i), LABEL[i], .disabled = st.pick != 0,
                             .leadingIcon = icon[i]))
                    ask(i);
        }
    }
    note(st.noDialog ? "This host shows no file dialog." : st.picked);
}

static const char *const HOURS[12] = { "09:00", "10:00", "11:00", "12:00", "13:00", "14:00",
                                       "15:00", "16:00", "17:00", "18:00", "19:00", "20:00" };
static const char *const MONTHS[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static const float D_P95[12] = { 182, 171, 196, 240, 318, 286, 251, 224, 209, 233, 262, 214 };
static const float D_P50[12] = { 64, 61, 72, 88, 121, 104, 92, 81, 76, 84, 97, 78 };
static const float D_IN[12] = { 12, 18, 26, 41, 38, 52, 61, 48, 44, 57, 34, 22 };
static const float D_OUT[12] = { 6, 9, 14, 19, 22, 28, 33, 26, 21, 24, 17, 11 };
static const float D_NEW[12] = { 412, 468, 530, 610, 584, 702, 760, 738, 815, 902, 968, 1040 };
static const float D_LOST[12] = { 96, 88, 104, 112, 98, 121, 118, 126, 109, 131, 140, 128 };
static const float D_RPS[12] = { 150, 200, 260, 320, 380, 450, 510, 570, 640, 700, 780, 850 };
static const float D_EU[12] = { 41, 44, 47, 52, 58, 66, 73, 88, 101, 128, 171, 240 };
static const float D_US[12] = { 38, 39, 42, 45, 49, 52, 58, 64, 71, 83, 102, 131 };

/* One recipe, four kinds. .unit turns a raw 14.019 into "14 ms" in the ticks and the readout.
   A numeric x axis fits its data exactly and the plot cuts a dot on its edge in half, so the
   scatter pins its load axis to round hundreds past its first and last reading. At 100 to 900
   its ticks, every 200 rps, all fall inside the plot: four labels that fit a narrow card, and
   none in the corner under the y labels. */
static void chart(const char *id, RC_SeriesKind kind, const float *a, const float *b,
                  const char *la, const char *lb, const char *unit, const float *x,
                  const char *const *labels) {
    RC_Series series[2] = {
        { .y = a, .x = x, .count = 12, .kind = kind, .color = accentInk, .label = la },
        { .y = b, .x = x, .count = 12, .kind = kind, .color = RC_CYAN_400, .label = lb } };

    rcBox(.w = "grow", .h = "216px") {
        rcChart(id, series, 2, .className = "text-[12px]",
                .x = { .min = x ? RC_VAL(100.0f) : RC_UNSET_VAL,
                       .max = x ? RC_VAL(900.0f) : RC_UNSET_VAL,
                       .unit = x ? " rps" : NULL, .labels = labels },
                .y = { .grid = true, .decimals = RC_VAL(0), .unit = unit }, .legend = true,
                .tooltip = true);
    }
}

static void story_line(void) {
    chart("ch1", RC_SERIES_LINE, D_P95, D_P50, "p95", "p50", " ms", NULL, HOURS);
}
static void story_areas(void) {
    chart("ch2", RC_SERIES_AREA, D_IN, D_OUT, "Inbound", "Outbound", " MB/s", NULL, HOURS);
}
static void story_bars(void) {
    chart("ch3", RC_SERIES_BAR, D_NEW, D_LOST, "New", "Churned", " users", NULL, MONTHS);
}
static void story_scatter(void) {
    chart("ch4", RC_SERIES_SCATTER, D_EU, D_US, "EU", "US", " ms", D_RPS, NULL);
}

static void story_spark(void) {
    static const char *const K[3][3] = { { "Sign-ups", "8,529", "+12.4%" },
                                         { "Latency", "214 ms", "+3.1%" },
                                         { "Traffic", "61 MB/s", "+8.0%" } };
    const bool good[3] = { true, false, true };   /* rising latency is bad news */
    const float *const y[3] = { D_NEW, D_P95, D_IN };
    const RC_SeriesKind kind[3] = { RC_SERIES_AREA, RC_SERIES_LINE, RC_SERIES_BAR };

    /* Each row takes a third of the card, so beside a taller chart the three fill it evenly
       instead of bunching in its middle. */
    for (int i = 0; i < 3; i++) {
        rcRow(.gap = 12, .align = "cl", .h = "grow") {
            rcColumn(.gap = 2, .w = "grow") {
                note(K[i][0]);
                rcTextC(K[i][1], .font = rcFontSemibold(20.0f));
            }
            rcTextC(K[i][2], .color = good[i] ? s->successHover : s->danger, .size = 12);
            rcBox(.w = "96px", .h = "40px") {
                rcSparkline(fmt("sp%d", i), y[i], 12, .kind = kind[i]);
            }
        }
    }
}

/* The zoom chart's x counts minutes from 09:00. Printed as a clock, the ticks and the
   readout's header both read "09:40", with no unit to explain. */
static int clock(double v, char *out, int cap, void *user) {
    int m = 9 * 60 + (int)v;
    RC_String text = rcFormat(rcAppArena(st.app), "%02d:%02d", m / 60, m % 60);

    (void)user;
    rcStrCopy(out, text.chars, (size_t)cap);
    return (int)text.length;
}

/* Drag to zoom is yours to write: the chart says where its plot is, and you own the x range.
   The box claims a finger's sideways drag (.captureTouch), as a slider does, so the scrolling
   page does not take it. */
static void story_brush(void) {
    RC_Series line = { .y = st.zoomY, .count = ZOOM_N, .kind = RC_SERIES_AREA,
                       .color = accentInk, .label = "Sample CPU" };
    RC_Box plot = rcChartPlotRect("zc");
    RC_Vec2 p = st.brushing || rcPointerPressed(RC_POINTER_LEFT) ? rcPointer()
                                                                 : RC_LIT(RC_Vec2){ 0 };
    float span = st.hi - st.lo;
    float x = plot.found ? st.lo + (p.x - plot.x) / plot.width * span : 0.0f;

    x = x < st.lo ? st.lo : x > st.hi ? st.hi : x;
    float a = st.brushA < x ? st.brushA : x, b = st.brushA < x ? x : st.brushA;
    /* A drag lives only while its chart is drawn: a chart the last frame did not lay out (a
       search or Esc hid the card) ends it, so no later release commits a zoom from an anchor
       the reader cannot see. */
    if (!plot.found) st.brushing = false;
    if (rcIsHovered("zb") && rcPointerPressed(RC_POINTER_LEFT)) {
        st.brushing = true;
        st.brushA = x;
    } else if (st.brushing && rcPointerReleased(RC_POINTER_LEFT)) {
        st.brushing = false;
        if (b - a > 2.0f) { st.lo = a; st.hi = b; }
    }
    rcBox(.id = "zb", .captureTouch = RC_TOUCH_AXIS_X, .w = "grow", .h = "176px") {
        RC_Box me = rcGetElementBox("zb");

        rcChart("zc", &line, 1, .className = "text-[12px]",
                .x = { .min = RC_VAL(st.lo), .max = RC_VAL(st.hi), .format = clock },
                .y = { .grid = true, .decimals = RC_VAL(0), .unit = "%" }, .tooltip = !st.brushing);
        if (st.brushing) {
            rcBox(.bg = rcWithAlpha(s->primary, 0.3f), .wType = RC_PX((b - a) / span * plot.width),
                  .hType = RC_PX(plot.height), .floating = { .to = RC_ATTACH_PARENT,
                                .offset = { plot.x - me.x + (a - st.lo) / span * plot.width,
                                            plot.y - me.y }, .capture = RC_CAPTURE_PASSTHROUGH }) {}
        }
    }
    rcRow(.gap = 8, .align = "cl") {
        note("Drag across the plot to zoom in");
        rcSpacer() {}
        if (rcButton("zr", "Reset", .disabled = span >= (float)ZOOM_N - 1.0f,
                     .variant = RC_BTN_GHOST)) {
            st.lo = 0.0f;
            st.hi = (float)ZOOM_N - 1.0f;
        }
    }
}

/* A table never scrolls sideways. Customer keeps 160 px of text and each other column has a
   .priority, so a narrow card hides Region first, then Status, and the tip says how many. */
static void story_table(void) {
    static const char *const ROWS[8][6] = {
        { "Northwind Traders", "$4,280", "Enterprise", "128", "Active", "Frankfurt" },
        { "Blue Harbor Logistics", "$1,960", "Team", "42", "Active", "Oregon" },
        { "Lumen Health", "$3,415", "Enterprise", "96", "Past due", "Singapore" },
        { "Fernway Studio", "$240", "Starter", "5", "Active", "Oregon" },
        { "Kestrel Robotics", "$2,720", "Team", "64", "Trial", "Frankfurt" },
        { "Altamira Foods", "$1,180", "Team", "27", "Active", "Sao Paulo" },
        { "Orchard Lane Press", "$96", "Starter", "3", "Canceled", "Frankfurt" },
        { "Tessera Labs", "$5,640", "Enterprise", "210", "Active", "Singapore" } };
    enum { PAD = 8 };
    const RC_TableColumn C[6] = {
        { .header = "Customer", .align = "cl", .w = "grow", .wMin = RC_VAL(160 + 2 * PAD) },
        { .header = "MRR", .wType = RC_PX(84), .align = "cr", .priority = 1 },
        { .header = "Plan", .wType = RC_PX(100), .align = "cl", .priority = 2 },
        { .header = "Seats", .wType = RC_PX(64), .align = "cr", .priority = 3 },
        { .header = "Status", .wType = RC_PX(88), .align = "cl", .priority = 4 },
        { .header = "Region", .wType = RC_PX(96), .align = "cl", .priority = 5 } };
    int hidden = 0;

    for (uint64_t bits = rcTableHiddenColumns("tb_t"); bits; bits &= bits - 1) hidden++;
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        note(st.rowPick >= 0 ? fmt("Selected %s", ROWS[st.rowPick][0]) : "Click a row");
        rcSpacer() {}
        if (hidden) note(fmt("+%d column%s", hidden, hidden == 1 ? "" : "s"));
    }
    rcBox(.w = "grow", .h = "272px") {
        if (rcBeginTable("tb_t", C, 6, .cellPadding = RC_VAL(PAD))) {
            for (int r = 0; r < 8; r++) {
                const char *id = fmt("tr%d", r);

                /* A focusable row is a Tab stop that no key clicks, so Enter is read here. The
                   scrolling body would clip a ring drawn outside it: ring-inset draws it inside. */
                rcTableRow(.id = id, .className = st.rowPick == r
                           ? "ring-inset bg-primary/15" : "ring-inset", .focusable = true);
                if (rcClicked(id) || rcKeyPressedOn(id, RC_KEY_ENTER)) st.rowPick = r;
                for (int c = 0; c < 6; c++) {
                    if (c) rcTableNext();
                    rcTextC(ROWS[r][c],
                            .color = st.rowPick == r || c == 0 || rcShowsHover(id) ? s->text
                                   : s->textMuted, .size = 13, .wrap = "n");
                }
            }
            rcEndTable();
        }
        rcScrollbar("tb_t");   /* the header and 8 rows pass 272 px, so the body scrolls */
    }
}

static void story_list(void) {
    rcBox(.w = "grow", .h = "272px") {
        rcColumn(.id = "vl", .scroll = "v", .w = "grow", .h = "grow") {
            rcVirtualList(row, "vl", 5000, 32) {
                const char *id = fmt("vl%d", row.index);
                unsigned cents = (unsigned)row.index * 2654435761u % 48000u + 1200u;

                if (rcClicked(id)) st.listPick = row.index;
                /* A row is a Tab stop as wide as the list, whose clip would cut a focus ring
                   drawn outside it: ring-inset draws the ring inside the row. */
                rcRow(.id = id, .focusable = true, .px = 8, .align = "cl", .w = "grow", .h = "32",
                      .className = st.listPick == row.index ? "ring-inset bg-primary/25"
                                                            : "ring-inset hover:bg-surface-alt") {
                    rcTextC(fmt("Order #%d", 10000 + row.index), .size = 13);
                    rcSpacer() {}
                    rcTextC(fmt("$%u.%02u", cents / 100, cents % 100),
                            .color = s->textMuted, .size = 13);
                }
            }
        }
        rcScrollbar("vl");
    }
    note("5,000 rows; only the visible ones are declared");
}

/* A file list beside the open file, the shape a split pane is usually for. A pane grows to
   hold what it cannot wrap, so the editor clips its lines as an editor does. */
static void story_split(void) {
    static const char *const F[] = { "main.c", "theme.h", "layout.c", "assets", "README.md" };

    rcBox(.w = "grow", .h = "grow", .hMin = RC_VAL(176), .className = "bg-surface-alt") {
        if (rcBeginSplitPane("sp", RC_SPLIT_ROW, &st.split,
                             .minFraction = 0.2f, .maxFraction = 0.8f)) {
            rcColumn(.gap = 8, .p = 12, .w = "grow", .h = "grow") {
                for (int i = 0; i < 5; i++) rcTextC(F[i], .size = 13);
            }
            rcSplitHandle();
            rcColumn(.gap = 8, .p = 12, .overflow = "hidden", .w = "grow", .h = "grow") {
                rcTextL("main.c", .font = rcFontSemibold(14.0f));
                rcTextL("#include \"rayclay.h\"\n\nint main(void) {\n    return rcRunApp(NULL);\n"
                        "}", .font = st.mono, .size = 12, .wrap = "l");
                note(fmt("Drag the divider: this pane has %.0f%%.", (1.0f - st.split) * 100.0f));
            }
            rcEndSplitPane();
        }
    }
}

static void story_scroll(void) {
    RC_ScrollInfo info = rcGetScrollInfo("sl");

    rcBox(.w = "grow", .h = "224px") {
        rcColumn(.id = "sl", .gap = 6, .scroll = "v", .w = "grow", .h = "grow") {
            for (int i = 0; i < 24; i++)
                rcTextC(fmt("12:%02d:%02d  worker-%d  job %d  done in %d ms", i * 2, i * 7 % 60,
                            i % 4, 4100 + i, 180 + i * 37 % 400), .font = st.mono, .size = 12);
        }
    }
    rcRow(.gap = 8, .align = "cl") {
        if (rcButton("sl1", "Top")) rcScrollToTop("sl");
        if (rcButton("sl2", "Down")) rcScrollBy("sl", 0.0f, 60.0f);
        if (rcButton("sl3", "Bottom")) rcScrollToBottom("sl");
        rcSpacer() {}
        note(fmt("%.0f of %.0f px", info.offsetY, info.maxOffsetY));
    }
    rcScrollbar("sl");   /* after the scroll calls on "sl", so its thumb reads their offset */
}

static void story_wrap(void) {
    static const char *const T[] = { "Design", "Billing", "Infrastructure", "Mobile", "Support",
                                     "Security", "Research", "Docs", "Growth" };

    rcRow(.gap = 8, .className = "flex-wrap") {
        for (int i = 0; i < 9; i++) {
            const char *id = fmt("wr%d", i);
            bool on = st.tags >> i & 1;

            if (rcClicked(id)) st.tags ^= 1 << i;
            rcBox(.id = id, .focusable = true, .px = 12, .py = 4, .borderRadius = "all-full",
                  .className = on ? "bg-primary hover:bg-primary-hover"
                                  : "bg-surface-alt hover:bg-border") {
                rcTextC(T[i], .color = on ? RC_WHITE : s->text, .size = 13,
                        .select = RC_SELECT_NONE);
            }
        }
    }
    note("Pick some. A wrapping row breaks wherever its width runs out.");
}

/* One tile per box property, so each style story below is a table and one call. The class
   string is the row's, so a story can stand its tiles on a stage of its own. */
typedef struct {
    const char *label, *cls;
    RC_Gradient g;
    RC_Shadow sh;
    RC_Color ov;
} Tile;

static void tiles(const Tile *t, int count, const char *cls) {
    rcRow(.gap = 16, .align = "cl", .className = cls) {
        for (int i = 0; i < count; i++) {
            rcColumn(.gap = 8, .align = "tc", .w = "72px") {
                rcBox(.bg = s->surfaceAlt, .w = "64px", .h = "48px", .gradient = t[i].g,
                      .shadow = t[i].sh, .overlay = t[i].ov, .className = t[i].cls) {}
                note(t[i].label);
            }
        }
    }
}

static void story_gradient(void) {
    const Tile T[] = {
        { .label = "Vertical", .cls = "rounded-lg", .g = { RC_VIOLET_600, RC_ROSE_500, "v" } },
        { .label = "Across", .cls = "rounded-lg", .g = { RC_EMERALD_500, RC_SKY_500, "h" } },
        { .label = "Diagonal", .cls = "rounded-lg", .g = { RC_AMBER_400, RC_ROSE_600, "d" } },
        { .label = "Rising", .cls = "rounded-lg", .g = { RC_INDIGO_600, RC_CYAN_400, "u" } },
        { .label = "Pill", .cls = "rounded-full", .g = { RC_FUCHSIA_500, RC_ORANGE_400, "h" } },
        { .label = "Round", .cls = "rounded-2xl", .g = { RC_SKY_400, RC_INDIGO_600, "v" } } };
    tiles(T, 6, "flex-wrap");
}

/* A shadow darkens what it falls on, and a dark card leaves it next to nothing to darken, so on
   dark the tiles stand on a lighter stage, lifted to a lighter fill again; a glow reads on both.
   The stage's padding stays in light, so a theme switch moves no tile.
   Each row names its color and offsets in RC_Shadow's order: color, x, y, blur, spread. */
static void story_shadow(void) {
    const RC_Color ink = st.dark ? RC_BLACK : RC_SLATE_400;
    const RC_Color fill = st.dark ? RC_ZINC_500 : RC_WHITE;
    const RC_Gradient g = { fill, fill, "v" };
    const Tile T[] = {
        { "Small", "rounded-lg", g, { rcWithAlpha(ink, 0.6f), 0, 2, 6, 0 }, RC_UNSET_COLOR },
        { "Medium", "rounded-lg", g, { rcWithAlpha(ink, 0.7f), 0, 8, 20, 0 }, RC_UNSET_COLOR },
        { "Offset", "rounded-lg", g, { rcWithAlpha(ink, 0.8f), 8, 8, 10, 0 }, RC_UNSET_COLOR },
        { "Tight", "rounded-lg", g, { rcWithAlpha(ink, 0.8f), 0, 6, 8, -3 }, RC_UNSET_COLOR },
        { "Glow", "rounded-lg", g, { accentInk, 0, 0, 20, 2 }, RC_UNSET_COLOR },
        { "Ring", "rounded-lg", g, { accentInk, 0, 0, 0, 3 }, RC_UNSET_COLOR } };
    tiles(T, 6, st.dark ? "flex-wrap p-3 rounded-lg bg-zinc-700" : "flex-wrap p-3");
}

static void story_border(void) {
    const Tile T[] = {
        { .label = "Hairline", .cls = "border border-zinc-500 rounded-md" },
        { .label = "2 px", .cls = "border-2 border-sky-500 rounded-md" },
        { .label = "4 px", .cls = "border-4 border-violet-500 rounded-lg" },
        { .label = "Square", .cls = "border border-zinc-500" },
        { .label = "Top corners", .cls = "border border-zinc-500 rounded-t-2xl" },
        { .label = "Pill", .cls = "border-2 border-emerald-500 rounded-full" } };
    tiles(T, 6, "flex-wrap");
}

/* .overlay mixes over the box and everything in it, so one color dims a whole subtree, and it
   takes the box's own corners, as these tiles show. */
static void story_overlay(void) {
    const RC_Gradient g = { RC_VIOLET_500, RC_SKY_400, "h" };
    const Tile T[] = {
        { .label = "None", .cls = "rounded-lg", .g = g },
        { .label = "White 40%", .cls = "rounded-lg", .g = g, .ov = rcWithAlpha(RC_WHITE, 0.4f) },
        { .label = "Black 50%", .cls = "rounded-lg", .g = g, .ov = rcWithAlpha(RC_BLACK, 0.5f) },
        { .label = "Rose 50%", .cls = "rounded-lg", .g = g, .ov = rcWithAlpha(RC_ROSE_500, 0.5f) },
        { .label = "Teal 50%", .cls = "rounded-lg", .g = g, .ov = rcWithAlpha(RC_TEAL_500, 0.5f) },
        { .label = "Amber 50%", .cls = "rounded-lg", .g = g,
          .ov = rcWithAlpha(RC_AMBER_400, 0.5f) } };
    tiles(T, 6, "flex-wrap");
}

static void story_type(void) {
    const uint16_t size[] = { 32, 24, 20, 16, 14, 13 };
    static const char *const L[] = { "Display 32", "Title 24", "Heading 20", "Lead 16", "Body 14",
                                     "Data 13" };

    for (int i = 0; i < 6; i++)
        rcTextC(L[i], .font = (uint16_t)(i < 3 ? rcFontSemibold((float)size[i]) : 0),
                .size = size[i]);
    note("Caption 12. Each size is the CSS number.");
    rcTextL("const char *mono = \"Adwaita Mono 12\";", .font = st.mono, .color = accentInk,
            .size = 12);
}

static void story_color(void) {
    static const char *const N[] = { "primary", "success", "warning", "danger", "surface",
                                     "surface-alt", "border", "foreground", "muted-foreground" };
    const RC_Color c[] = { s->primary, s->success, s->warning, s->danger, s->surface,
                           s->surfaceAlt, s->border, s->text, s->textMuted };

    /* Each role is named on itself as a class spells it (bg-surface-alt,
       text-muted-foreground), and filled from its RC_Style field, the one beside it in c[]:
       foreground is .text and muted-foreground .textMuted. The name is in the ink that reads
       there, as Material lays its roles out: the surface swatch is the card's own color, and
       its name shows it is a fill. The four accents take white, the surfaces and border the
       text color, the two text roles the surface. A swatch is as wide as the longest name. */
    rcRow(.gap = 8, .className = "flex-wrap") {
        for (int i = 0; i < 9; i++) {
            rcBox(.bg = c[i], .px = 8, .align = "cl", .borderRadius = "all-md",
                  .border = { .color = s->border, .width = 1 }, .w = "120px", .h = "32px") {
                rcTextC(N[i], .color = i < 4 ? RC_WHITE : i < 7 ? s->text : s->surface, .size = 12);
            }
        }
    }
}

/* The logo is square, so a square box draws Cover and Contain alike; a tall one shows Cover
   cropping its sides and Contain leaving bands above and below. */
static void story_image(void) {
    static const char *const L[] = { "Cover", "Contain", "Tinted" };
    const RC_ImageFit fit[] = { RC_IMAGE_COVER, RC_IMAGE_CONTAIN, RC_IMAGE_CONTAIN };

    if (!st.logo.handle) note("Run from the repository root to load the logo PNG.");
    rcRow(.gap = 12, .align = "cl") {
        for (int i = 0; i < 3 && st.logo.handle; i++) {
            rcColumn(.gap = 6, .align = "tc") {
                rcBox(.w = "56px", .h = "80px", .image = &st.logo, .imageFit = fit[i],
                      .imageTint = i == 2 ? accentInk : RC_UNSET_COLOR,
                      .className = "rounded-lg bg-surface-alt") {}
                note(L[i]);
            }
        }
    }
}

/* A triangle wave for a sine: demo data needs no libm. */
static int wave(int n) {
    n &= 255;
    return n < 128 ? n * 2 : (255 - n) * 2;
}

static void story_icons(void) {
    const RC_IconCallback I[] = { rcIconBell, rcIconCode, rcIconFolder, rcIconImage, rcIconSearch,
                                  rcIconSettings, rcIconTrash2, rcIconUpload, rcIconUser };
    static const char *const NAME[] = { "Bell", "Code", "Folder", "Image", "Search", "Settings",
                                        "Trash2", "Upload", "User" };

    rcRow(.gap = 4, .align = "cl", .className = "flex-wrap") {
        for (int i = 0; i < 9; i++) {
            const char *id = fmt("ic%d", i), *call = fmt("rcIcon%s", NAME[i]);

            /* As an icon site does, a click copies the name the tip shows; a tile is a Tab stop,
               so Enter on it copies too. */
            if (rcClicked(id)) rcClipboardSet(call);
            /* hover: fills the tile; a glyph is a call with no state of its own, so
               rcShowsHover picks its color, as it does a text run's. */
            rcBox(.id = id, .focusable = true, .align = "cc", .tooltip = call,
                  .className = "size-9 rounded-md hover:bg-surface-alt") {
                I[i](20.0f, rcShowsHover(id) ? accentInk : s->text);
            }
        }
    }
}

static const struct {
    int page;
    const char *name, *call;
    void (*draw)(void);
    int span;
} STORIES[] = {
    { P_CONTROLS, "Button", "rcButton(id, \"OK\")", story_button, 1 },
    { P_CONTROLS, "Text field", ".clearButton = true", story_field, 1 },
    { P_CONTROLS, "Text area", "rcTextArea(\"n\", b, sizeof b)", story_area, 1 },
    { P_CONTROLS, "Checkbox", "rcCheckbox(id, \"Alerts\", &on)", story_check, 1 },
    { P_CONTROLS, "Toggle", "rcToggle(id, \"Wi-Fi\", &on)", story_toggle, 1 },
    { P_CONTROLS, "Radio", "rcRadio(id, \"Small\", &sz, 0)", story_radio, 1 },
    { P_CONTROLS, "Slider", "rcSlider(\"v\", &vol, 0, 100)", story_slider, 1 },
    { P_CONTROLS, "Combo", "rcCombo(\"r\", &sel, items, n)", story_combo, 1 },
    { P_CONTROLS, "Progress", "rcProgress(\"up\", 0.4f)", story_progress, 1 },
    { P_OVERLAYS, "Menu", "rcBeginMenu(\"m\", \"Sort\")", story_menu, 1 },
    { P_OVERLAYS, "Context menu", "rcBeginContextMenu(\"m\", \"cx\")", story_context, 1 },
    { P_OVERLAYS, "Modal dialog", "rcBeginModal(\"ask\", &open)", story_modal, 1 },
    { P_OVERLAYS, "Tooltip", ".tooltip = \"Preferences\"", story_tooltip, 1 },
    { P_OVERLAYS, "Popover", ".floating = { .toId = \"po\" }", story_popover, 2 },
    { P_OVERLAYS, "File dialogs", "rcFileDialogOpen(RC_FILE_DIALOG_FILE, NULL)", story_files, 3 },
    { P_DATA, "Line chart", "rcChart(\"c\", series, 2)", story_line, 1 },
    { P_DATA, "Area chart", ".kind = RC_SERIES_AREA", story_areas, 1 },
    { P_DATA, "Bar chart", ".kind = RC_SERIES_BAR", story_bars, 1 },
    { P_DATA, "Scatter chart", ".kind = RC_SERIES_SCATTER", story_scatter, 1 },
    { P_DATA, "Sparkline", "rcSparkline(\"s\", y, n)", story_spark, 1 },
    { P_DATA, "Drag to zoom", "rcChartPlotRect(\"c\")", story_brush, 1 },
    { P_DATA, "Table", "rcBeginTable(\"t\", cols, 6)", story_table, 2 },
    { P_DATA, "Virtual list", "rcVirtualList(r, \"l\", n, 32)", story_list, 1 },
    { P_LAYOUT, "Split pane", "rcBeginSplitPane(\"p\", axis, &f)", story_split, 2 },
    { P_LAYOUT, "Wrapping row", ".className = \"flex-wrap\"", story_wrap, 1 },
    { P_LAYOUT, "Scroll area", "rcScrollbar(\"list\")", story_scroll, 3 },
    { P_STYLE, "Type", ".font = rcFontSemibold(24.0f)", story_type, 1 },
    { P_STYLE, "Color roles", "rcGetStylePtr()->primary", story_color, 1 },
    { P_STYLE, "Gradient", ".gradient = { a, b, \"h\" }", story_gradient, 1 },
    { P_STYLE, "Shadow", ".shadow = { c, 0, 8, 20, 0 }", story_shadow, 1 },
    { P_STYLE, "Border", ".className = \"border-2\"", story_border, 1 },
    { P_STYLE, "Overlay tint", ".overlay = rcWithAlpha(c, a)", story_overlay, 1 },
    { P_STYLE, "Image", ".image = &logo", story_image, 1 },
    { P_STYLE, "Icons", "rcIconSearch(20.0f, color)", story_icons, 2 } };
enum { STORY_COUNT = sizeof STORIES / sizeof STORIES[0] };

static const struct { const char *name, *lede; RC_IconCallback icon; } PAGES[PAGE_COUNT] = {
    { "Controls", "Buttons, fields and pickers, each bound to a value you own.", rcIconCirclePlay },
    { "Overlays", "Menus, dialogs and whatever else floats above the page.", rcIconPanelRight },
    { "Data", "Charts, tables and long lists, with readouts in real units.", rcIconChartColumn },
    { "Layout", "Panes, scrolling and wrapping: the boxes the rest sits in.", rcIconPanelLeft },
    { "Style", "Type, color and imagery, drawn from plain values.", rcIconImage } };

static const char *const CORNERS[5] = { "rounded-none", "rounded-sm", "rounded-lg", "rounded-xl",
                                        "rounded-2xl" };

/* With a query the grid shows every match; without one, the open page. */
static bool listed(int i) {
    if (!st.query[0]) return STORIES[i].page == st.page;
    return contains(STORIES[i].name, st.query) || contains(STORIES[i].call, st.query);
}

/* A rail row is a box read with rcClicked, and a Tab stop (.focusable): Enter or Space on it
   reads as a click. */
static void rail_row(const char *id, const char *label, RC_IconCallback icon, int count, bool on) {
    rcRow(.id = id, .focusable = true, .gap = 10, .pl = (float)(icon ? 10 : 36), .pr = 10,
          .align = "cl", .w = "grow", .h = rcPointerIsCoarse() ? "44px" : icon ? "36px" : "30px",
          .className = on ? "rounded-md bg-primary/25" : "rounded-md hover:bg-surface-alt") {
        if (icon) icon(16.0f, on ? accentInk : s->textMuted);
        rcTextC(label, .font = on ? rcFontSemibold(14.0f) : (uint16_t)0,
                .color = on || icon ? s->text : s->textMuted, .size = (uint16_t)(icon ? 14 : 13),
                .select = RC_SELECT_NONE);
        if (icon) {
            rcSpacer() {}
            note(fmt("%d", count));
        }
    }
}

/* Every page lists its stories, open or not, so a click never moves the row under the pointer;
   a search lists the matches, as many as the count says. One row is marked with a fill: the
   page, or the story last jumped to, which its card's outline marks too. */
static void rail_nav(void) {
    for (int p = 0; p < PAGE_COUNT; p++) {
        const char *id = fmt("nav%d", p);
        int n = 0;

        for (int i = 0; i < STORY_COUNT; i++)
            n += STORIES[i].page == p && (!st.query[0] || listed(i));
        if (rcClicked(id)) {
            st.page = p;
            st.current = -1;
            st.query[0] = '\0';
            st.drawer = false;
            rcScrollToTop("page");
        }
        rail_row(id, PAGES[p].name, PAGES[p].icon, n,
                 st.page == p && st.current < 0 && !st.query[0]);
        for (int i = 0; i < STORY_COUNT; i++) {
            if (STORIES[i].page != p || (st.query[0] && !listed(i))) continue;
            id = fmt("go%d", i);
            if (rcClicked(id)) {
                st.page = p;
                st.current = i;
                st.jump = i;
                st.drawer = false;
            }
            rail_row(id, STORIES[i].name, NULL, 0, st.current == i);
        }
    }
}

/* A theme control repaints from the next frame's first element. The slider's write asks for
   that frame itself; a swatch is a plain click, so it asks. */
static void rail(bool compact) {
    rcColumn(.h = "grow", .wType = compact ? RC_GROW : RC_PX(RAIL_W),
             .className = "bg-chrome border-r border-border") {
        rcColumn(.id = "nav", .gap = 2, .p = 12, .scroll = "v", .w = "grow", .h = "grow") {
            rail_nav();
        }
        rcScrollbar("nav");
        rcColumn(.gap = 10, .p = 16, .w = "grow", .className = "border-t border-border") {
            rcTextL("Appearance", .font = rcFontSemibold(13.0f));
            rcRow(.gap = 8, .align = "cl") {
                for (int i = 0; i < 5; i++) {
                    const char *id = fmt("ac%d", i);

                    if (rcClicked(id)) {
                        st.accent = i;
                        rcWindowRequestFrame(st.win);
                    }
                    rcBox(.id = id, .focusable = true, .bg = rcHex(ACCENTS[i][0]),
                          .w = "24px", .h = "24px",
                          .className = i == st.accent ? "rounded-full border-2 border-foreground"
                                                      : "rounded-full border-2 border-chrome") {}
                }
            }
            readout("Corners", fmt("%.0f px", st.radius));
            rcSlider("rad", &st.radius, 0.0f, 16.0f, .step = 4.0f);
        }
    }
}

/* The band is the titlebar too: it drags the window, except inside .noDrag boxes. */
static void band(bool compact) {
    rcRow(.id = RC_ID_WINDOW_DRAG, .gap = 12, .align = "cl", .w = "grow", .hType = RC_PX(BAND_H),
          .className = "bg-chrome border-b border-border") {
        rcRow(.gap = 10, .pl = 16, .align = "cl", .wType = compact ? RC_FIT : RC_PX(RAIL_W - 12)) {
            if (compact) {
                rcBox(.noDrag = true, .tooltip = st.drawer ? "Hide the pages" : "Show the pages") {
                    if (rcButton("tb_sidebar", "", .variant = RC_BTN_GHOST,
                                 .leadingIcon = rcIconPanelLeft))
                        st.drawer = !st.drawer;
                }
            } else {
                rcIconRayClayLogo(24.0f);
                rcTextL("RayClay", .font = rcFontSemibold(16.0f), .select = RC_SELECT_NONE);
                rcTextL("Widgets", .color = s->textMuted, .size = 13, .select = RC_SELECT_NONE);
            }
        }
        rcRow(.align = "cl", .w = "grow") {
            rcBox(.noDrag = true, .w = "grow", .wMax = RC_VAL(320)) {
                rcTextInput("search", st.query, sizeof st.query,
                            .placeholder = compact ? "Search" : "Search widgets",
                            .leadingIcon = rcIconSearch, .clearButton = true);
            }
        }
        rcBox(.noDrag = true, .tooltip = st.dark ? "Light theme" : "Dark theme") {
            if (rcButton("tb_theme", "", .variant = RC_BTN_GHOST,
                         .leadingIcon = st.dark ? rcIconSun : rcIconMoon)) {
                st.dark = !st.dark;
                rcWindowRequestFrame(st.win);
            }
        }
        rcWindowControls();
    }
}

/* The story the rail marks keeps its outline until another page or story is picked, as a
   page keeps its :target. */
static void card(int i, RC_Size width) {
    bool copied = st.copied == i && st.now < st.copiedUntil;
    RC_Color line = rcWithAlpha(s->border, 0.6f);

    /* A window keeps only its earliest one-shot timer (rcWindowRequestFrameAfter), so each
       frame that shows the check asks again for the frame that takes it down. */
    if (copied) rcWindowRequestFrameAfter(st.win, st.copiedUntil - st.now);
    rcColumn(.id = fmt("card%d", i), .bg = s->surface, .overflow = "clip",
             .border = { .color = st.current == i ? accentInk : line, .width = 1 },
             .h = "grow", .wType = width, .className = CORNERS[(int)st.radius / 4]) {
        rcBox(.pt = 12, .px = 16) { rcTextC(STORIES[i].name, .font = rcFontSemibold(14.0f)); }
        rcColumn(.gap = 12, .p = 16, .w = "grow", .h = "grow", .className = "items-stretch") {
            STORIES[i].draw();
        }
        rcRow(.gap = 8, .pl = 16, .pr = 4, .align = "cl", .w = "grow", .hMin = RC_VAL(36.0f),
              .className = "bg-zinc-950 py-2") {
            rcBox(.overflow = "hidden", .w = "grow") {
                rcTextC(STORIES[i].call, .font = st.mono, .color = RC_ZINC_300, .size = 12,
                        .wordBreak = "w");
            }
            /* py-1.5 makes it 28 px under a mouse and leaves the 44 px a finger's row takes;
               min-h-0 would hold it at 28 px on a phone too. */
            if (rcButton(fmt("copy%d", i), "", .className = "py-1.5 text-zinc-300 "
                         "hover:bg-zinc-800 active:bg-zinc-700", .variant = RC_BTN_GHOST,
                         .leadingIcon = copied ? rcIconCheck : rcIconCopy)) {
                rcClipboardSet(STORIES[i].call);
                st.copied = i;
                st.copiedUntil = st.now + 2.5;
            }
        }
    }
}

static int span_of(int i, int cols) { return STORIES[i].span < cols ? STORIES[i].span : cols; }

/* Rows, not a wrap: a row stretches its cards to the tallest one, and a span is plain
   arithmetic. A row's last card grows into the room left over, so no row ends in a hole: the
   columns no card took, and under a mouse the 12 px band the page's scrollbar takes inside its
   edge, which a card of fixed width would push into the right-hand padding. */
static void grid(float inner, int cols) {
    float unit = (inner - (float)((cols - 1) * GAP)) / (float)cols;
    int list[STORY_COUNT], n = 0;

    for (int i = 0; i < STORY_COUNT; i++) if (listed(i)) list[n++] = i;
    for (int i = 0; i < n;) {
        int end = i, used = 0;

        while (end < n && used + span_of(list[end], cols) <= cols)
            used += span_of(list[end++], cols);
        rcRow(.gap = GAP, .w = "grow", .h = "fit", .className = "items-stretch") {
            for (; i < end; i++) {
                int span = span_of(list[i], cols);
                float px = unit * (float)span + (float)((span - 1) * GAP);

                card(list[i], i == end - 1 ? RC_GROW : RC_PX(px));
            }
        }
    }
    if (n == 0) {
        rcColumn(.gap = 12, .pt = 96, .align = "cc", .w = "grow") {
            rcIconSearch(32.0f, s->textMuted);
            rcTextC(fmt("No widget matches \"%s\"", st.query), .font = rcFontSemibold(16.0f));
            if (rcButton("none", "Clear the search")) st.query[0] = '\0';
        }
    }
}

static void page(bool compact) {
    RC_Viewport v = rcViewport();
    float pad = compact ? 16.0f : 24.0f;
    float inner = v.width - v.safe.left - v.safe.right - (compact ? 0.0f : (float)RAIL_W) -
                  2.0f * pad;

    /* The jump lands after this frame's layout, on the cards it declares, so a card on another
       page, or one the phone's drawer hid until now, is found on the frame that lists it. */
    if (st.jump >= 0) {
        rcScrollIntoView("page", fmt("card%d", st.jump), RC_SCROLL_CENTER);
        st.jump = -1;
    }
    rcBox(.w = "grow", .h = "grow") {
        rcColumn(.id = "page", .gap = GAP, .p = pad, .scroll = "v", .w = "grow", .h = "grow") {
            rcColumn(.gap = 4, .pb = 8) {
                rcTextC(st.query[0] ? "Search results" : PAGES[st.page].name,
                        .font = rcFontSemibold(24.0f));
                rcTextC(st.query[0] ? fmt("Matching \"%s\"", st.query) : PAGES[st.page].lede,
                        .color = s->textMuted);
            }
            grid(inner, inner >= 960.0f ? 3 : inner >= 600.0f ? 2 : 1);
        }
        rcScrollbar("page");
    }
}

static void update(RC_App *app, void *userData) {
    (void)userData;
    st.now = rcAppTime(app);
    if (!st.app) {
        st.app = app;
        st.win = rcAppMainWindow(app);
        st.mono = rcLoadFont(RC_DEMO_MONO, 12.0f);
        st.logo = rcLoadImage(RC_DEMO_LOGO);
        /* A look more than one control wears gets a name, once (story_button, the menus). Its
           roles follow the theme, as a class's do. A class color stands on a disabled control,
           so each names its disabled: look: a disabled Upload drops its tint, a Delete reads
           muted. */
        rcDefineClass("soft", "border-transparent bg-primary/15 hover:bg-primary/25 "
                              "active:bg-primary/35 disabled:bg-transparent");
        rcDefineClass("destructive", "text-danger hover:bg-danger/10 "   /* rcMenuItem's recipe */
                                     "disabled:text-muted-foreground");
        for (int i = 0; i < ZOOM_N; i++)
            st.zoomY[i] = (float)(24 + wave(i * 9) / 6 + wave(i * 37) / 14);
    }
    if (rcKeyPressed(RC_KEY_K) && rcModDown(RC_MOD_PRIMARY)) rcSetFocus("search");
    if ((st.query[0] || st.popover) && rcKeyPressed(RC_KEY_ESCAPE)) {
        st.query[0] = '\0';
        st.popover = false;
    }
    collect();
    rcSetStyle(app_style(st.dark, st.accent, st.radius));
    s = rcGetStylePtr();
    accentInk = st.dark ? rcHex(ACCENTS[st.accent][2]) : s->primary;
    rcWindowSetClearColor(st.win, s->background);
}

static void layout(RC_App *app, void *userData) {
    RC_Viewport v = rcViewport();
    bool compact = v.width < 760.0f || v.height < 440.0f;   /* a phone either way up */

    (void)userData;
    rcColumn(.bg = s->background, .pt = v.safe.top, .pb = v.safe.bottom,
             .pl = v.safe.left, .pr = v.safe.right, .w = "grow", .h = "grow") {
        /* Unzoomed, the band keeps the on-screen height the OS drag strip was given. */
        rcUnzoomed() { band(compact); }
        /* A tap in the drawer closes it from inside rail_nav: read once, the frame draws the
           drawer or the page, never both side by side, and the page comes on the next. */
        rcRow(.w = "grow", .h = "grow") {
            bool drawer = st.drawer;

            if (!compact || drawer) rail(compact);
            if (!compact || !drawer) page(compact);
            if (drawer != st.drawer) rcWindowRequestFrame(rcAppMainWindow(app));
        }
    }
}

int main(void) {
    static const float sizes[] = { 14.0f };   /* slot 0: what every widget draws its label at */
    RC_AppOptions opts = {
        .width = 1280, .height = 820, .title = "RayClay Widgets", .fontSizes = sizes,
        .fontCount = 1, .scratchArenaBytes = 256 * 1024, .startLayoutElements = 4096,
        .frame = RC_WINDOW_FRAME_NONE, .updateCallback = update,
        .layoutCallback = layout, .titlebar = { .height = BAND_H }, };

    return rcRunApp(&opts);
}
