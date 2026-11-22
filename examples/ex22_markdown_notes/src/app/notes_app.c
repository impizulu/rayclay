/*
    notes_app.c - the notes app's GUI: Bear's three panes, with Obsidian's links.

    Every pane is a pure function of AppState, and no pane holds a copy of a note: the
    text area edits the note's own buffer, the Preview renders that same buffer
    (notes_markdown.h), and the list reads what the text implies (notes_backend.h). So
    three views of one note can never disagree, which is what an immediate-mode UI
    gives you for free.
*/
#define NOTES_BACKEND_IMPLEMENTATION
#include "notes_app.h"
#include "notes_theme.h"
#include "notes_markdown.h"

#include "icons/rc_icons_archive.h"
#include "icons/rc_icons_arrow_left.h"
#include "icons/rc_icons_book_open.h"
#include "icons/rc_icons_bookmark.h"
#include "icons/rc_icons_briefcase.h"
#include "icons/rc_icons_calendar.h"
#include "icons/rc_icons_chevron_down.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_code.h"
#include "icons/rc_icons_compass.h"
#include "icons/rc_icons_download.h"
#include "icons/rc_icons_ellipsis_vertical.h"
#include "icons/rc_icons_file_text.h"
#include "icons/rc_icons_hash.h"
#include "icons/rc_icons_house.h"
#include "icons/rc_icons_inbox.h"
#include "icons/rc_icons_list_tree.h"
#include "icons/rc_icons_menu.h"
#include "icons/rc_icons_moon.h"
#include "icons/rc_icons_panel_left.h"
#include "icons/rc_icons_pin.h"
#include "icons/rc_icons_plus.h"
#include "icons/rc_icons_rotate_cw.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_square_arrow_out_up_right.h"
#include "icons/rc_icons_square_check.h"
#include "icons/rc_icons_square_pen.h"
#include "icons/rc_icons_sun.h"
#include "icons/rc_icons_trash_2.h"
#include "icons/rc_icons_x.h"

/* Warmup frames, then the scene HOLDS. */
#define NOTES_BENCH_WARMUP 64

#define NOTES_ARENA_BYTES (256u * 1024u)
#define NOTES_TOAST_SECONDS 6.0f
#define NOTES_READ_FOOT 96   /* the reader page's bottom padding: room, not text */

/* Which panes this frame's width allows, decided once in notes_layout and read by
   every pane, so the band above a column can never disagree with the column. */
typedef struct {
    bool  sidebar;     /* the graphite column is on screen                 */
    bool  list;
    bool  editor;
    bool  sidebarFits; /* three panes fit, so the sidebar has its own slot */
    bool  onePane;
    bool  coarse;      /* a finger: every row and button is at least 44 px */
    bool  shortView;   /* too little height to spend any on a status bar   */
    float sidebarW;
    float listW;
    float editorW;
    float zoom;
} NotesArms;

RC_Style notes_style(bool dark) {
    NotesPalette t = notes_palette(dark);
    RC_Style     s = dark ? rcStyleDark() : rcStyleLight();

    /* THE PAGE IS `surface` AND THE LIST IS `background`, because a text field fills
       itself with `surface`: a field on the page is the page, which is what lets the
       editor have no box around it, and the search field lifts off the list. */
    s.background   = t.list;
    s.surface      = t.paper;
    s.surfaceAlt   = t.rule;
    s.chrome       = t.list;
    s.text         = t.ink;
    s.textMuted    = t.inkMuted;
    s.primary      = t.accent;
    s.primaryHover = t.link;
    s.danger       = t.accent;
    s.dangerHover  = t.link;
    s.border       = t.rule;
    s.radius       = 6.0f;
    return s;
}

/* The theme, for every window and for the bench: the palette as RC_Style, and its named
   looks as classes. Defining a class makes every class string resolve again, so the
   classes are defined when the theme changes, never per frame; the class table is the
   process's, and so is the record of which theme it holds. */
static void notes_install_theme(bool dark) {
    static int classesFor = -1;

    rcSetStyle(notes_style(dark));
    if (classesFor != (int)dark) {
        notes_define_classes(dark);
        classesFor = (int)dark;
    }
}

/* The reading column's width: characters per line times the body face's average
   advance, read off the font rather than guessed. */
static float notes_measure_px(const AppState *st) {
    static const char sample[] = "abcdefghijklmnopqrstuvwxyz etaoinshrdlu";
    uint16_t size = NOTES_TYPE_STEPS[st->textStep].body;
    float    avg  = rcTextWidth(0, size, sample, -1) / (float)(sizeof sample - 1);

    return avg > 0.0f ? avg * NOTES_MEASURES[st->widthStep] : 640.0f;
}

/* -- commands: everything a button, a key or a page link can ask for --------------- */

static void notes_refresh_order(AppState *st) {
    st->orderCount = note_sorted(&st->store, st->section, st->tag, st->search, st->order);
}

/* The one writer of openNote, so the list, the editor and the reader follow together.
   Selecting a note is also what moves a one-pane window to the editor. */
static void notes_open(AppState *st, int index) {
    Note *n = note_at(&st->store, index);

    st->openNote       = n ? index : -1;
    st->backlinksStale = true;
    st->pendingHeading = -1;
    st->pickedHeading  = -1;
    st->activeHeading  = 0;
    st->readProgress   = 0.0f;
    st->pageY          = 0.0f;
    st->pageToEnd      = 0;
    if (n)
        st->pane = PANE_EDITOR;
}

/* The list changed under the open note: keep it if it is still listed, else show the
   first row, as a list with no selection reads as broken. */
static void notes_keep_selection(AppState *st) {
    notes_refresh_order(st);
    for (int i = 0; i < st->orderCount; i++)
        if (st->order[i] == st->openNote)
            return;
    {
        NotePane pane = st->pane;

        notes_open(st, st->orderCount ? st->order[0] : -1);
        st->pane = pane;
    }
}

static void notes_show_section(AppState *st, NoteSection section, const char *tag, int tagLen) {
    st->section = section;
    st->tag[0]  = '\0';
    if (tag && tagLen < NOTE_TAG_CAP) {
        for (int i = 0; i < tagLen; i++)
            st->tag[i] = tag[i];
        st->tag[tagLen] = '\0';
    }
    st->search[0] = '\0';
    st->reader    = false;
    st->pane      = PANE_LIST;
    notes_keep_selection(st);
}

static void notes_show_tag(AppState *st, const char *path, int length) {
    notes_show_section(st, NOTE_SEC_TAG, path, length);
}

static void notes_toast(AppState *st, const char *text, NoteUndo kind, int note) {
    rcStrCopy(st->toastText, text, sizeof st->toastText);
    st->toastLeft = NOTES_TOAST_SECONDS;
    st->undoKind  = kind;
    st->undoNote  = note;
}

void notes_notice(AppState *st, const char *text) {
    notes_toast(st, text, UNDO_NONE, -1);
}

/* A new note always lands somewhere it can be seen: leaving Trash or Archive, and
   clearing a search that would hide it. */
static void notes_new(AppState *st, const char *body) {
    int fresh = note_create(&st->store, body);

    if (fresh < 0) {
        notes_toast(st, "The notebook is full", UNDO_NONE, -1);
        return;
    }
    if (st->section != NOTE_SEC_ALL && st->section != NOTE_SEC_TODAY)
        notes_show_section(st, NOTE_SEC_ALL, NULL, 0);
    st->search[0] = '\0';
    st->reader    = false;
    notes_open(st, fresh);
    if (!body) {
        st->view       = VIEW_WRITE;
        st->focusField = true;
    }
}

static void notes_create_titled(AppState *st, const char *title, int length) {
    char body[NOTE_TITLE_CAP + 8];
    int  k = note_link_line(title, length, body);   /* the line note_find_title reads */

    body[k++] = '\n';
    body[k++] = '\n';
    body[k]   = '\0';
    notes_new(st, body);
}

static void notes_toggle_pin(AppState *st, Note *n) {
    n->pinned = !n->pinned;
    st->store.revision++;
}

static void notes_trash(AppState *st, int index) {
    Note *n = note_at(&st->store, index);

    if (!n)
        return;
    n->trashed = true;
    st->store.revision++;
    st->backlinksStale = true;
    note_tags_rebuild(&st->store);
    notes_keep_selection(st);
    notes_toast(st, "Moved to Trash", UNDO_TRASH, index);
}

static void notes_archive(AppState *st, int index, bool archived) {
    Note *n = note_at(&st->store, index);

    if (!n)
        return;
    n->archived = archived;
    st->store.revision++;
    st->backlinksStale = true;
    note_tags_rebuild(&st->store);
    notes_keep_selection(st);
    if (archived)
        notes_toast(st, "Archived", UNDO_ARCHIVE, index);
}

static void notes_restore(AppState *st, int index) {
    Note *n = note_at(&st->store, index);

    if (!n)
        return;
    n->trashed = false;
    st->store.revision++;
    st->backlinksStale = true;
    note_tags_rebuild(&st->store);
    notes_keep_selection(st);
    notes_toast(st, "Restored from Trash", UNDO_NONE, -1);
}

static void notes_destroy(AppState *st, int index) {
    note_destroy(&st->store, index);
    /* The toast's Undo names its note by slot, and a new note may take the slot: a note gone
       for good has nothing to take back. */
    if (st->undoNote == index)
        st->undoKind = UNDO_NONE;
    st->backlinksStale = true;
    notes_keep_selection(st);
}

/* Undo puts the note back and reopens it, so the user sees what came back. */
static void notes_undo(AppState *st) {
    Note *n = note_at(&st->store, st->undoNote);

    if (n && st->undoKind == UNDO_TRASH)
        n->trashed = false;
    else if (n && st->undoKind == UNDO_ARCHIVE)
        n->archived = false;
    st->store.revision++;
    st->backlinksStale = true;
    note_tags_rebuild(&st->store);
    st->undoKind  = UNDO_NONE;
    st->toastLeft = 0.0f;
    if (n) {
        notes_refresh_order(st);
        notes_open(st, st->undoNote);
    }
}

static void notes_open_search(AppState *st) {
    st->searchOpen = true;
    st->pane       = PANE_LIST;
    st->reader     = false;
    rcSetFocus("search");
}

/* Closing the search brings every note back, and the selection rule runs again because
   a search that found nothing left no note open. */
static void notes_close_search(AppState *st) {
    st->searchOpen = false;
    st->search[0]  = '\0';
    notes_keep_selection(st);
}

static void notes_enter_reader(AppState *st) {
    st->reader         = true;
    st->readerContents = false;
    st->pendingHeading = -1;
    st->pickedHeading  = -1;
}

/* Text the Append menu adds. A text field keeps its caret to itself, so a block goes
   to the end of the note, and focusing the field then brings that end into view. */
static const char *const APPEND_LABELS[] = {
    "Heading", "Task list", "Quote", "Callout", "Table", "Code block", "Divider",
};
static const char *const APPEND_TEXT[] = {
    "## Heading\n",
    "- [ ] First task\n- [ ] Second task\n",
    "> A line worth keeping.\n",
    "> [!tip] Worth knowing\n> The detail goes here.\n",
    "| Item | Note | Cost |\n|---|---|---:|\n| One | | $0 |\n| Two | | $0 |\n",
    "```c\nint main(void)\n{\n    return 0;\n}\n```\n",
    "---\n",
};

/* -- small controls ---------------------------------------------------------------- */

/* The ink of an icon in a band: the soft ink, the accent of a toggle that is on, or the
   muted ink of the region it sits in, which graphite_style makes the column's own. */
typedef enum { INK_SOFT = 0, INK_ON, INK_MUTED } IconInk;

/* The library's icon-only button, its look its class: the ink, and a mouse's 32 px or a
   finger's 44. A button has no tooltip of its own, so the box around it carries one,
   and .noDrag because it may sit in the window's drag band, where a press would
   otherwise move the window. */
static bool icon_button(const char *id, RC_IconCallback icon, IconInk ink, const char *tip,
                        bool coarse) {
    static const char *const LOOK[2][3] = {
        { "h-8 ink-soft", "h-8 text-primary", "h-8 text-muted-foreground" },
        { "h-11 ink-soft", "h-11 text-primary", "h-11 text-muted-foreground" },
    };
    bool hit = false;

    rcBox(.noDrag = true, .tooltip = tip) {
        hit = rcButton(id, "", .className = LOOK[coarse][ink], .variant = RC_BTN_GHOST,
                       .leadingIcon = icon);
    }
    return hit;
}

/* One segment of a segmented control, a Tab stop, a finger's 44 px tall. The chosen one
   lifts onto the page's color; the same box in both states, so nothing reflows when the
   choice moves. */
static bool segment(const NotesPalette *t, const char *id, const char *label, bool active,
                    uint16_t size) {
    rcBox(.id = id, .focusable = true, .px = 12, .align = "cc", .borderRadius = "all-sm",
          .h = rcPointerIsCoarse() ? "44px" : "29px",
          .className = active ? "bg-surface" : "hover:bg-surface-alt") {
        rcTextC(label, .color = active ? t->ink : t->inkSoft, .size = size,
                .select = RC_SELECT_NONE);
    }
    return rcClicked(id);
}

/* The menu of things one note can have done to it, shared by the toolbar's menu and
   the list row's right-click, so the two can never offer different actions. */
static void note_menu_items(AppState *st, int index) {
    Note *n = note_at(&st->store, index);

    if (!n)
        return;
    if (n->trashed) {
        if (rcMenuItem("Restore", .leadingIcon = rcIconRotateCw))
            notes_restore(st, index);
        if (rcMenuItem("Delete forever", .className = "text-danger hover:bg-danger/10",
                       .leadingIcon = rcIconTrash2))
            notes_destroy(st, index);
        return;
    }
    if (rcMenuItem(n->pinned ? "Unpin" : "Pin to top", .leadingIcon = rcIconPin))
        notes_toggle_pin(st, n);
    if (rcMenuItem("Duplicate", .leadingIcon = rcIconCopy)) {
        int fresh = note_duplicate(&st->store, index);

        if (fresh >= 0)
            notes_open(st, fresh);
    }
    if (st->canOpenWindows &&
        rcMenuItem("Open in new window", .leadingIcon = rcIconSquareArrowOutUpRight))
        st->requestWindow = index;
    if (rcMenuItem("Export Markdown", .leadingIcon = rcIconDownload))
        st->requestExport = index;
    if (rcMenuItem(n->archived ? "Unarchive" : "Archive", .leadingIcon = rcIconArchive))
        notes_archive(st, index, !n->archived);
    if (rcMenuItem("Move to Trash", .className = "text-danger hover:bg-danger/10",
                   .leadingIcon = rcIconTrash2))
        notes_trash(st, index);
}

/* -- the sidebar ------------------------------------------------------------------- */

/* One row of the graphite column. The chevron's slot is held on every row, so labels
   line up whether or not a row can expand. Returns true on a click on the row; a
   click on the chevron sets *toggled instead, and is polled first because an element
   and its parent both count as under the pointer. */
static bool side_row(const NotesPalette *t, AppState *st, const char *id, RC_IconCallback icon,
                     const char *label, int count, int depth, bool selected, bool coarse,
                     const char *chevronId, bool open, bool *toggled) {
    RC_Color ink   = selected ? RC_WHITE : t->sidebarInk;
    RC_Color muted = selected ? t->sidebarInk : t->sidebarMuted;

    rcRow(.id = id, .focusable = true, .gap = 6, .pl = (float)(4 + depth * 14), .pr = 10,
          .align = "cl", .borderRadius = "all-md", .w = "grow", .h = coarse ? "44px" : "30px",
          .className = selected ? "side-row-on" : "side-row") {
        rcBox(.id = chevronId, .focusable = chevronId != NULL, .align = "cc", .w = "16px",
              .h = "16px") {
            if (chevronId) {
                if (open)
                    rcIconChevronDown(14.0f, t->sidebarMuted);
                else
                    rcIconChevronRight(14.0f, t->sidebarMuted);
            }
        }
        icon(16.0f, muted);
        rcBox(.pl = 2, .overflow = "hidden", .w = "grow") {
            rcTextC(label, .color = ink, .size = 14, .wrap = "e", .select = RC_SELECT_NONE);
        }
        if (count > 0)
            rcText(rcFormat(&st->arena, "%d", count), .color = muted, .size = 13,
                   .select = RC_SELECT_NONE);
    }
    if (chevronId && rcClicked(chevronId)) {
        *toggled = true;
        return false;
    }
    return rcClicked(id);
}

/* A top-level tag wears its own icon, as Bear 2 draws them; a nested tag keeps the hash. */
static RC_IconCallback tag_icon(const NoteTagRow *row) {
    static const struct { const char *tag; RC_IconCallback icon; } icons[] = {
        { "dev", rcIconCode }, { "docs", rcIconBookOpen }, { "home", rcIconHouse },
        { "reading", rcIconBookmark }, { "travel", rcIconCompass }, { "work", rcIconBriefcase },
    };
    if (row->depth > 0)
        return rcIconHash;
    for (size_t i = 0; i < sizeof icons / sizeof icons[0]; i++)
        if (strcmp(row->path, icons[i].tag) == 0)
            return icons[i].icon;
    return rcIconHash;
}

static void notes_sidebar(AppState *st, const NotesPalette *t, const NotesArms *a) {
    NoteStore *store   = &st->store;
    bool       toggled = false;

    /* The scrollbar takes its own 12 px on the right while the rows overflow, under every
       pointer (it has a class), so the rows' rounded fills end clear of its track. */
    rcColumn(.id = "Sidebar", .bg = t->sidebar, .gap = 1, .pt = 4, .pb = 12, .pl = 8, .pr = 8,
             .scroll = "v", .h = "grow", .wType = a->onePane ? RC_GROW : RC_PX(a->sidebarW)) {
        if (side_row(t, st, "sec_all", rcIconFileText, "Notes",
                     note_section_count(store, NOTE_SEC_ALL, NULL), 0,
                     st->section == NOTE_SEC_ALL, a->coarse, "sec_all_c", st->notesOpen,
                     &toggled))
            notes_show_section(st, NOTE_SEC_ALL, NULL, 0);
        if (toggled)
            st->notesOpen = !st->notesOpen;
        if (st->notesOpen) {
            if (side_row(t, st, "sec_untagged", rcIconInbox, "Untagged",
                         note_section_count(store, NOTE_SEC_UNTAGGED, NULL), 1,
                         st->section == NOTE_SEC_UNTAGGED, a->coarse, NULL, false, &toggled))
                notes_show_section(st, NOTE_SEC_UNTAGGED, NULL, 0);
            if (side_row(t, st, "sec_todo", rcIconSquareCheck, "Todo",
                         note_section_count(store, NOTE_SEC_TODO, NULL), 1,
                         st->section == NOTE_SEC_TODO, a->coarse, NULL, false, &toggled))
                notes_show_section(st, NOTE_SEC_TODO, NULL, 0);
            if (side_row(t, st, "sec_today", rcIconCalendar, "Today",
                         note_section_count(store, NOTE_SEC_TODAY, NULL), 1,
                         st->section == NOTE_SEC_TODAY, a->coarse, NULL, false, &toggled))
                notes_show_section(st, NOTE_SEC_TODAY, NULL, 0);
        }
        if (side_row(t, st, "sec_archive", rcIconArchive, "Archive",
                     note_section_count(store, NOTE_SEC_ARCHIVE, NULL), 0,
                     st->section == NOTE_SEC_ARCHIVE, a->coarse, NULL, false, &toggled))
            notes_show_section(st, NOTE_SEC_ARCHIVE, NULL, 0);
        if (side_row(t, st, "sec_trash", rcIconTrash2, "Trash",
                     note_section_count(store, NOTE_SEC_TRASH, NULL), 0,
                     st->section == NOTE_SEC_TRASH, a->coarse, NULL, false, &toggled))
            notes_show_section(st, NOTE_SEC_TRASH, NULL, 0);

        rcBox(.h = "12px") {}

        /* The tag tree. A row under a collapsed ancestor is skipped by depth, so a
           branch folds without the tree having to know its own shape. */
        int foldedAt = -1;

        for (int r = 0; r < store->tagRowCount; r++) {
            NoteTagRow *row   = &store->tagRows[r];
            const char *leaf  = row->path;
            bool        hit   = false;

            if (foldedAt >= 0 && row->depth > foldedAt)
                continue;
            foldedAt = row->collapsed ? row->depth : -1;
            for (const char *c = row->path; *c; c++)
                if (*c == '/')
                    leaf = c + 1;

            toggled = false;
            hit = side_row(t, st, row->id, tag_icon(row), leaf, row->count, row->depth,
                           st->section == NOTE_SEC_TAG && strcmp(st->tag, row->path) == 0,
                           a->coarse, row->hasKids ? row->chevronId : NULL, !row->collapsed,
                           &toggled);
            if (toggled)
                row->collapsed = !row->collapsed;
            if (hit)
                notes_show_tag(st, row->path, (int)strlen(row->path));
        }
    }
    /* The column's own colors (side-scrollbar). A bar with a class is one bar under every
       pointer, so a finger gets this track and thumb too, not the fading indicator. */
    rcScrollbar("Sidebar", .className = "side-scrollbar");
}

/* -- the note list ----------------------------------------------------------------- */

/* A row as Bear draws it: title, two lines of preview, then the date with its pin.
   A selected row takes a tinted fill and a red bar on its left edge, a background AND
   a mark because a tint is the first channel a color-blind reader loses. The bar's
   slot is held in both states so nothing reflows when the selection moves. */
static void note_row(AppState *st, const NotesPalette *t, int index, bool coarse, bool last,
                     bool onePane) {
    Note *n      = &st->store.notes[index];
    bool  active = index == st->openNote && !onePane;   /* a phone's list has no open row */

    rcRow(.id = n->rowId, .focusable = true, .borderRadius = "all-md", .w = "grow",
          .className = active ? "list-row-on" : "list-row") {
        rcBox(.bg = active ? t->accent : RC_UNSET_COLOR, .w = "3px", .h = "grow",
              .className = "rounded-l-md") {}
        rcColumn(.gap = 2, .pt = 10, .pb = 8, .pl = 11, .pr = 12, .w = "grow",
                 .hMin = coarse ? RC_VAL(88.0f) : RC_UNSET_VAL) {
            rcBox(.overflow = "hidden", .w = "grow") {
                rcTextC(n->title, .font = st->faces.bold, .color = t->ink, .size = 14, .wrap = "e",
                        .select = RC_SELECT_NONE);
            }
            rcBox(.overflow = "hidden", .w = "grow", .className = "items-stretch") {
                rcTextC(n->snippet, .color = t->inkSoft, .size = 13, .lineHeight = 18,
                        .select = RC_SELECT_NONE, .lineClamp = 2);
            }
            rcRow(.gap = 4, .pt = 4, .align = "cl", .w = "grow") {
                if (n->pinned)
                    rcIconPin(12.0f, t->accent);
                rcTextC(n->ageText, .color = t->inkMuted, .size = 13, .select = RC_SELECT_NONE);
                rcSpacer() {}
                if (n->tasksTotal > 0) {
                    rcText(rcFormat(&st->arena, "%d/%d", n->tasksDone, n->tasksTotal),
                           .color = t->inkMuted, .size = 13, .select = RC_SELECT_NONE);
                    rcIconSquareCheck(12.0f, n->tasksDone == n->tasksTotal ? t->tipBar
                                                                           : t->inkMuted);
                }
            }
        }
    }
    if (rcClicked(n->rowId))
        notes_open(st, index);
    {
        const char *menuId = rcFormat(&st->arena, "ctx%d", index).chars;

        if (rcBeginContextMenu(menuId, n->rowId)) {
            note_menu_items(st, index);
            rcEndContextMenu();
        }
    }
    if (!last && !active) {
        rcBox(.pl = 14, .pr = 12, .w = "grow") {
            rcBox(.bg = t->rule, .w = "grow", .h = "1px") {}
        }
    }
}

/* What the empty list says depends on why it is empty: a list that says nothing reads
   as a list that failed to load. */
static void notes_list_empty(AppState *st, const NotesPalette *t) {
    const char *head = "No notes here";
    const char *hint = "Press the pen icon to write the first one.";

    if (st->search[0]) {
        head = "No results";
        hint = "No note contains that text.";
    } else if (st->section == NOTE_SEC_TRASH) {
        head = "Trash is empty";
        hint = "Notes you delete wait here until you empty it.";
    } else if (st->section == NOTE_SEC_ARCHIVE) {
        head = "Nothing archived";
        hint = "Archive a note to keep it out of the way.";
    } else if (st->section == NOTE_SEC_TODO) {
        head = "All done";
        hint = "Every task in every note is ticked.";
    }
    rcColumn(.gap = 6, .px = 24, .align = "cc", .w = "grow", .h = "grow") {
        rcTextC(head, .font = st->faces.bold, .color = t->ink, .size = 16);
        rcTextC(hint, .color = t->inkMuted, .size = 13, .textAlign = "c");
        if (st->search[0] && rcButton("search_reset", "Clear search", .variant = RC_BTN_GHOST)) {
            st->search[0] = '\0';
            notes_keep_selection(st);
        }
    }
}

static void notes_list(AppState *st, const NotesPalette *t, const NotesArms *a) {
    rcColumn(.id = "ListPane", .bg = t->list, .h = "grow",
             .wType = a->onePane ? RC_GROW : RC_PX(a->listW)) {
        if (st->searchOpen) {
            rcBox(.pb = 8, .pl = 12, .pr = 12, .w = "grow") {
                /* The results replace the list, so the open note must be one of them. */
                if (rcTextInput("search", st->search, sizeof st->search,
                                .className = a->coarse ? "h-11" : "h-8",
                                .placeholder = "Search all text", .leadingIcon = rcIconSearch,
                                .clearButton = true)) {
                    notes_keep_selection(st);
                    st->requestFrame = true;
                }
            }
        }
        if (st->orderCount == 0) {
            notes_list_empty(st, t);
        } else {
            rcColumn(.id = "NoteList", .pb = 8, .pl = 6, .pr = 6, .scroll = "v", .w = "grow",
                     .h = "grow") {
                for (int i = 0; i < st->orderCount; i++)
                    note_row(st, t, st->order[i], a->coarse, i + 1 == st->orderCount, a->onePane);
            }
            rcScrollbar("NoteList");
        }
        if (st->section == NOTE_SEC_TRASH && st->orderCount > 0) {
            rcBox(.p = 12, .w = "grow") {
                if (rcButton("empty_trash", "Empty Trash"))
                    st->modalEmptyTrash = true;
            }
        }
    }
}

/* -- the editor pane --------------------------------------------------------------- */

static MdPage notes_page(AppState *st, const NotesPalette *t, Note *n, const char *prefix,
                         float width, RC_Arena *arena, bool readOnly) {
    MdPage m;

    memset(&m, 0, sizeof m);
    m.st       = st;
    m.t        = t;
    m.rs       = &NOTES_TYPE_STEPS[st->textStep];
    m.arena    = arena;
    m.note     = n;
    m.prefix   = prefix;
    m.width    = width;
    m.readOnly = readOnly;
    return m;
}

/* The foot of the page: the notes that link here, as Obsidian's backlinks. */
static void notes_backlinks(AppState *st, const NotesPalette *t) {
    if (st->backlinkCount == 0)
        return;
    rcColumn(.gap = 4, .pt = 24, .w = "grow") {
        rcBox(.bg = t->rule, .w = "grow", .h = "1px") {}
        rcBox(.pt = 12, .pb = 4) {
            rcText(rcFormat(&st->arena, "Linked from %d note%s", st->backlinkCount,
                            st->backlinkCount == 1 ? "" : "s"),
                   .font = st->faces.bold, .color = t->inkMuted, .size = 14);
        }
        for (int b = 0; b < st->backlinkCount; b++) {
            int         from = st->backlinks[b];
            Note       *n    = note_at(&st->store, from);
            const char *id   = rcFormat(&st->arena, "bl%d", from).chars;

            if (n) {
                rcColumn(.id = id, .focusable = true, .gap = 2, .px = 8, .py = 6,
                         .borderRadius = "all-md", .w = "grow", .className = "list-row") {
                    rcTextC(n->title, .color = t->link, .size = 14, .select = RC_SELECT_NONE);
                    rcBox(.overflow = "hidden", .w = "grow") {
                        rcTextC(n->snippet, .color = t->inkMuted, .size = 13, .wrap = "e",
                                .select = RC_SELECT_NONE);
                    }
                }
                if (rcClicked(id))
                    notes_open(st, from);
            }
        }
    }
}

/* The rendered page in a scrolling pane, centered and held to the reading measure.
   Write and Reader drop this pane, and a pane dropped loses its offset, so the place is
   read back every live frame and handed over when the pane returns: that is what lets a
   writer flip to Write and back without losing the paragraph being checked. */
static void notes_preview(AppState *st, const NotesPalette *t, Note *n, const char *prefix,
                          float paneW) {
    float         measure = notes_measure_px(st);
    const char   *id      = rcFormat(&st->arena, "%s.page.%d", prefix, st->openNote).chars;
    const char   *colId   = rcFormat(&st->arena, "%s.col", prefix).chars;
    RC_Box        col     = rcGetElementBox(colId);
    RC_ScrollInfo place   = rcGetScrollInfo(id);
    float         width   = paneW - 64.0f < measure ? paneW - 64.0f : measure;
    MdPage        page    = notes_page(st, t, n, prefix, col.found ? col.width : width, &st->arena,
                                       n->trashed);

    if (place.found)
        st->pageY = place.offsetY;
    /* An append grows the page this frame, and a scroll acts on the page as last laid
       out, so the jump to its end waits one frame. */
    if (st->pageToEnd > 0) {
        if (--st->pageToEnd == 0)
            rcScrollToBottom(id);
        st->requestFrame = true;
    }
    rcColumn(.id = id, .pt = 36, .pb = 64, .pl = 32, .pr = 32, .align = "tc", .scroll = "v",
             .w = "grow", .h = "grow", .scrollOffset = { 0.0f, st->pageY }) {
        rcColumn(.id = colId, .w = "grow", .wMax = RC_VAL(measure)) {
            md_render(&page);
            notes_backlinks(st, t);
        }
    }
    rcScrollbar(id);
}

/* The source. The text area edits the note's own buffer in place; rcTextArea returns
   true on the frame it changed, and that one edit is what recomputes the title, the
   preview line, the tags and the counts. The id carries the note, so switching notes
   is a different field with its own caret and scroll rather than one field whose text
   was replaced under it. */
static void notes_source(AppState *st, Note *n, bool centered) {
    const char *id      = rcFormat(&st->arena, "src.%d", st->openNote).chars;
    float       measure = notes_measure_px(st) + 48.0f;
    const NotesTypeStep *rs = &NOTES_TYPE_STEPS[st->textStep];
    const char *look    = rcFormat(&st->arena, "border-0 flex-1 min-h-32 font-[%d] leading-[%dpx]",
                                   st->faces.source[st->textStep], rs->bodyLine).chars;

    rcColumn(.pt = 20, .pb = 8, .pl = 16, .pr = 16, .align = "tc", .w = "grow", .h = "grow") {
        rcColumn(.w = "grow", .h = "grow",
                 .wMax = centered ? RC_VAL(measure) : RC_UNSET_VAL) {
            if (rcTextArea(id, n->body, NOTE_BODY_CAP, .className = look,
                           .placeholder = "Start with a title")) {
                note_touch(&st->store, n);
                st->backlinksStale = true;
            }
        }
    }
    if (st->focusField) {
        rcSetFocus(id);
        st->focusField = false;
    }
}

/* A note in the Trash or the Archive says so at the top, with the way back. */
static void notes_banner(AppState *st, const NotesPalette *t, Note *n) {
    if (!n->trashed && !n->archived)
        return;
    rcRow(.bg = n->trashed ? t->dangerFill : t->tableHead, .gap = 8, .px = 16, .py = 8,
          .align = "cl", .w = "grow") {
        rcTextC(n->trashed ? "This note is in the Trash." : "This note is archived.",
                .color = t->ink, .size = 14);
        rcSpacer() {}
        if (n->trashed) {
            if (rcButton("banner_restore", "Restore"))
                notes_restore(st, st->openNote);
            if (rcButton("banner_delete", "Delete forever", .variant = RC_BTN_DANGER))
                notes_destroy(st, st->openNote);
        } else if (rcButton("banner_unarchive", "Unarchive")) {
            notes_archive(st, st->openNote, false);
        }
    }
}

/* The foot of the editor, as Obsidian's: the facts of the note, right-aligned, in
   the order a writer asks for them. */
static void notes_status(AppState *st, const NotesPalette *t, const Note *n, const NotesArms *a) {
    RC_Arena *ar    = &st->arena;
    bool      roomy = a->editorW >= 720.0f;   /* the facts win over the notice beside them */

    rcBox(.bg = t->rule, .w = "grow", .h = "1px") {}
    rcRow(.gap = 16, .px = 16, .align = "cl", .w = "grow", .h = "28px") {
        if (st->faces.failed && roomy) {
            rcIconTriangleAlert(14.0f, t->warnBar);
            rcTextL("Fonts not found: run from the repo root", .color = t->ink, .size = 13,
                    .wrap = "n");
        } else if (st->keepOnDevice && (st->saveFailed || st->storeRefused) && roomy) {
            /* Dirty until stored: a deliberate retry, never one every frame. */
            rcTextL("Not saved", .color = t->ink, .size = 13, .wrap = "n");
            if (rcButton("save_retry", "Retry", .variant = RC_BTN_GHOST))
                st->saveRetry = true;
        } else if (st->keepOnDevice && st->store.revision != st->savedRevision && roomy) {
            rcTextL("Saving", .color = t->inkMuted, .size = 13, .wrap = "n");
        } else if (st->keepOnDevice && st->store.revision != st->storedRevision && roomy) {
            /* The web: the file is saved in the page; the browser has not stored it yet. */
            rcTextL("Saved here, storing", .color = t->inkMuted, .size = 13, .wrap = "n");
        }
        rcSpacer() {}
        if (n->tasksTotal > 0)
            rcText(rcFormat(ar, "%d of %d tasks", n->tasksDone, n->tasksTotal),
                   .color = t->inkMuted, .size = 13, .wrap = "n");
        if (st->backlinkCount > 0 && !a->onePane)
            rcText(rcFormat(ar, "%d backlink%s", st->backlinkCount,
                            st->backlinkCount == 1 ? "" : "s"),
                   .color = t->inkMuted, .size = 13, .wrap = "n");
        rcText(rcFormat(ar, "%s word%s", rcFormatInt(ar, n->words, NULL).chars,
                        n->words == 1 ? "" : "s"),
               .color = t->inkMuted, .size = 13, .wrap = "n");
        if (!a->onePane)
            rcText(rcFormat(ar, "%s characters", rcFormatInt(ar, n->chars, NULL).chars),
                   .color = t->inkMuted, .size = 13, .wrap = "n");
        if (n->words > 0)
            rcText(rcFormat(ar, "%d min read", n->readMins), .color = t->inkMuted, .size = 13,
                   .wrap = "n");
    }
}

/* The view this frame shows. A trashed note only reads, and one pane has no room for
   Split, so it reads Preview there while st->view keeps the choice for a wider window.
   The band and the pane both ask here, so the highlighted segment is always the view. */
static NoteView notes_shown_view(const AppState *st, const Note *n, const NotesArms *a) {
    if ((n && n->trashed) || (a->onePane && st->view == VIEW_SPLIT))
        return VIEW_PREVIEW;
    return st->view;
}

static void notes_editor(AppState *st, const NotesPalette *t, const NotesArms *a) {
    Note *n = note_at(&st->store, st->openNote);

    rcColumn(.id = "Editor", .bg = t->paper, .w = "grow", .h = "grow") {
        if (!n) {
            rcColumn(.gap = 8, .align = "cc", .w = "grow", .h = "grow") {
                rcTextL("Select a note", .font = st->faces.bold, .color = t->ink, .size = 20);
                rcTextL("Or start a new one with the pen icon.", .color = t->inkMuted, .size = 14);
            }
        } else {
            NoteView view = notes_shown_view(st, n, a);

            notes_banner(st, t, n);
            /* notes_banner can delete the note it is drawn for. */
            n = note_at(&st->store, st->openNote);
            if (n) {
                if (view == VIEW_WRITE) {
                    notes_source(st, n, true);
                } else if (view == VIEW_PREVIEW) {
                    notes_preview(st, t, n, "pv", a->editorW);
                } else if (rcBeginSplitPane("split", RC_SPLIT_ROW, &st->splitFraction,
                                            .minFraction = 0.25f, .maxFraction = 0.75f,
                                            .handleThickness = 5.0f)) {
                    notes_source(st, n, false);
                    rcSplitHandle();
                    notes_preview(st, t, n, "sp", a->editorW * (1.0f - st->splitFraction));
                    rcEndSplitPane();
                }
                /* A phone's note app has no status bar, and a window this short has the
                   same reason: the page needs the line. A failed font load still says so. */
                if (!a->shortView || st->faces.failed)
                    notes_status(st, t, n, a);
            }
        }
    }
}

/* -- the docs reader --------------------------------------------------------------- */

/* A section matches on its title or anything in it, because the word a reader
   remembers is usually in the prose. A section owns the text up to the next heading
   of its level or higher. */
static bool notes_section_matches(const AppState *st, const Note *n, int h) {
    int stop;

    if (!st->tocSearch[0])
        return true;
    stop = (int)strlen(n->body);
    for (int k = h + 1; k < st->headingCount; k++) {
        if (st->headings[k].level <= st->headings[h].level) {
            stop = st->headings[k].offset;
            break;
        }
    }
    return note_text_contains(n->body + st->headings[h].offset, stop - st->headings[h].offset,
                              st->tocSearch);
}

/* The contents: a marker rail held on every entry so the list never shifts as the
   reader passes a heading, the entry being read marked, and the count in the label,
   because a filtered list that does not say so is a list with sections missing. */
static void notes_contents(AppState *st, const NotesPalette *t, Note *n, bool coarse) {
    int total = st->headingCount - st->firstSection, shown = 0;

    if (total == 0) {
        rcBox(.pt = 4, .pl = 8) {
            rcTextL("This note has no sections.", .color = t->inkMuted, .size = 14);
        }
        return;
    }
    rcBox(.pb = 8, .w = "grow") {
        rcTextInput("toc_search", st->tocSearch, sizeof st->tocSearch,
                    .className = coarse ? "h-11" : "h-7.5", .placeholder = "Search sections",
                    .leadingIcon = rcIconSearch, .clearButton = true);
    }
    /* Counted after the field, whose x may just have emptied the search. */
    for (int h = st->firstSection; h < st->headingCount; h++)
        if (notes_section_matches(st, n, h))
            shown++;
    rcBox(.pb = 6, .pl = 8) {
        if (st->tocSearch[0])
            rcText(rcFormat(&st->arena, "%d OF %d SECTIONS", shown, total), .color = t->inkMuted,
                   .size = 13, .letterSpacing = 1, .select = RC_SELECT_NONE);
        else
            rcTextL("CONTENTS", .color = t->inkMuted, .size = 13, .letterSpacing = 1,
                    .select = RC_SELECT_NONE);
    }
    for (int h = st->firstSection; h < st->headingCount; h++) {
        const NoteHeading *head = &st->headings[h];
        const char        *id;
        bool               current = h == st->activeHeading;

        if (!notes_section_matches(st, n, h))
            continue;
        id = rcFormat(&st->arena, "toc.%d", h).chars;
        rcRow(.id = id, .focusable = true, .gap = 6, .pl = (float)(head->level >= 3 ? 16 : 4),
              .pr = 8, .align = "cl", .borderRadius = "all-md", .w = "grow",
              .hMin = RC_VAL(coarse ? 44.0f : 30.0f), .className = "list-row") {
            rcBox(.bg = current ? t->accent : RC_UNSET_COLOR, .borderRadius = "all-full",
                  .w = "3px", .h = "16px") {}
            rcBox(.w = "grow") {
                rcText(md_slice(n->body + head->offset, head->length),
                       .font = current ? st->faces.bold : (uint16_t)0,
                       .color = current ? t->ink : t->inkSoft, .size = 14,
                       .select = RC_SELECT_NONE);
            }
        }
        if (rcClicked(id)) {
            st->pendingHeading = h;
            st->pickedHeading  = h;
            st->readerContents = false;
        }
    }
    if (shown == 0) {
        rcBox(.pl = 8) {
            rcTextL("No section matches that search.", .color = t->inkMuted, .size = 14);
        }
    }
}

/* WHICH SECTION THE READER IS IN: the last heading whose top has reached the upper
   part of the page, and the first section until the page has moved at all. Boxes
   describe the PREVIOUS frame, which is the frame the reader was looking at, so when
   the answer moves the runner is asked for one more. The last sections can run out of
   travel before their headings reach that band, so at the end the mark is the last
   heading, and a section picked from the contents keeps the mark until the page moves.
   THE END IS THE TEXT'S, NOT THE PAGE'S: the page's foot is padding, so a reader who
   can see the last line has reached the end, and the progress reads 100%. */
static void notes_scroll_spy(AppState *st, MdPage *page, const char *pageId) {
    RC_ScrollInfo info = rcGetScrollInfo(pageId);
    RC_Box        box  = rcGetElementBox(pageId);
    int           now  = st->headingCount > st->firstSection ? st->firstSection : 0;
    float         end;

    if (!info.found || !box.found)
        return;
    end = info.maxOffsetY - (float)NOTES_READ_FOOT;
    {
        float frac = end > 0.0f && info.offsetY < end ? info.offsetY / end : 1.0f;

        if (st->pendingHeading < 0 && info.offsetY != st->readOffset)
            st->pickedHeading = -1;
        st->readOffset = info.offsetY;
        if (frac != st->readProgress) {
            st->readProgress = frac;
            st->requestFrame = true;
        }
    }
    {
        float band  = box.y + box.height * 0.4f;
        bool  atEnd = info.maxOffsetY > 0.0f && info.offsetY >= end - 0.5f;

        for (int h = st->firstSection; info.offsetY > 0.5f && h < st->headingCount; h++) {
            RC_Box sec = rcGetElementBox(md_heading_id(page, h));

            if (sec.found && sec.height > 0.0f && (sec.y <= band || atEnd))
                now = h;
        }
    }
    if (st->pickedHeading >= 0)
        now = st->pickedHeading;
    if (now != st->activeHeading) {
        st->activeHeading = now;
        st->requestFrame  = true;
    }
}

/* THE SCROLL IS WRITTEN BEFORE THE PAGE IS DECLARED: a container takes its offset as
   it opens. rcScrollIntoView acts on last frame's boxes, so it waits until the page
   has been laid out at least once, and it fires once, as it asks to be. */
static void notes_reader_seek(AppState *st, MdPage *page, const char *pageId) {
    const char *target;

    if (st->pendingHeading < 0)
        return;
    target = md_heading_id(page, st->pendingHeading);
    if (rcGetElementBox(target).found) {
        rcScrollIntoView(pageId, target, RC_SCROLL_START);
        st->pendingHeading = -1;
    }
    st->requestFrame = true;
}

/* The same letter at the sizes it selects: a control that spells its options out costs
   three times the width and says less. */
static void reader_text_steps(AppState *st, const NotesPalette *t) {
    rcRow(.bg = t->control, .gap = 2, .p = 2, .align = "cl", .borderRadius = "all-md",
          .noDrag = true) {
        if (segment(t, "step_s", "A", st->textStep == 0, 13))
            st->textStep = 0;
        if (segment(t, "step_m", "A", st->textStep == 1, 16))
            st->textStep = 1;
        if (segment(t, "step_l", "A", st->textStep == 2, 18))
            st->textStep = 2;
    }
}

static void notes_reader(AppState *st, const NotesPalette *t, Note *n, float innerW, bool rail,
                         const NotesArms *a) {
    float       measure = notes_measure_px(st);
    float       paneW   = rail ? innerW - NOTES_TOC_W - 1.0f : innerW;
    float       width   = paneW - 48.0f < measure ? paneW - 48.0f : measure;
    const char *pageId  = "ReadPage";
    MdPage      page    = notes_page(st, t, n, "rd", width, &st->arena, n->trashed);

    md_outline(st, n->body);
    rcRow(.id = "ReaderBody", .bg = t->paper, .w = "grow", .h = "grow") {
        if (rail) {
            rcColumn(.id = "Toc", .bg = t->list, .gap = 1, .p = 12, .scroll = "v", .h = "grow",
                     .wType = RC_PX(NOTES_TOC_W)) {
                notes_contents(st, t, n, a->coarse);
            }
            rcScrollbar("Toc");
            rcBox(.bg = t->rule, .w = "1px", .h = "grow") {}
        }
        if (!rail && st->readerContents) {
            rcColumn(.id = "TocPane", .gap = 1, .p = 16, .scroll = "v", .w = "grow", .h = "grow") {
                /* One pane's band has no room for these, so they wait here. */
                if (a->onePane) {
                    rcRow(.pb = 12, .align = "cl", .w = "grow") {
                        rcTextL("Text size", .color = t->ink, .size = 14);
                        rcSpacer() {}
                        reader_text_steps(st, t);
                    }
                }
                notes_contents(st, t, n, a->coarse);
            }
            rcScrollbar("TocPane");
        } else {
            notes_reader_seek(st, &page, pageId);
            notes_scroll_spy(st, &page, pageId);
            rcColumn(.id = pageId, .pt = 40, .pb = NOTES_READ_FOOT, .pl = 24, .pr = 24,
                     .align = "tc", .scroll = "v", .w = "grow", .h = "grow") {
                rcColumn(.w = "grow", .wMax = RC_VAL(measure)) {
                    md_render(&page);
                }
            }
            rcScrollbar(pageId);
        }
    }
}

/* -- the band: one row across the top that is also the window's title bar ---------- */

static void notes_view_switch(AppState *st, const NotesPalette *t, const Note *n,
                              const NotesArms *a) {
    NoteView view = notes_shown_view(st, n, a);

    rcRow(.bg = t->control, .gap = 2, .p = 2, .align = "cl", .borderRadius = "all-md",
          .disabled = !n || n->trashed, .noDrag = true) {
        if (segment(t, "view_write", "Write", view == VIEW_WRITE, 14))
            st->view = VIEW_WRITE;
        if (!a->onePane && segment(t, "view_split", "Split", view == VIEW_SPLIT, 14))
            st->view = VIEW_SPLIT;
        if (segment(t, "view_preview", "Preview", view == VIEW_PREVIEW, 14))
            st->view = VIEW_PREVIEW;
    }
}

/* The band's list section: where you are, a pen and a magnifier. */
static void band_list(AppState *st, const NotesPalette *t, const NotesArms *a) {
    const char *title = "Notes";

    switch (st->section) {
    case NOTE_SEC_UNTAGGED: title = "Untagged"; break;
    case NOTE_SEC_TODO:     title = "Todo";     break;
    case NOTE_SEC_TODAY:    title = "Today";    break;
    case NOTE_SEC_ARCHIVE:  title = "Archive";  break;
    case NOTE_SEC_TRASH:    title = "Trash";    break;
    case NOTE_SEC_TAG:      title = st->tag;    break;
    default:                                    break;
    }
    if (!a->sidebar && icon_button("show_sidebar", rcIconMenu, INK_SOFT, "Show the sidebar",
                                   a->coarse)) {
        st->sidebarHidden = false;
        if (!a->sidebarFits)
            st->pane = PANE_SIDEBAR;
    }
    if (st->section == NOTE_SEC_TAG)
        rcIconHash(16.0f, t->inkMuted);
    rcBox(.overflow = "hidden", .w = "grow") {
        rcTextC(title, .font = st->faces.bold, .color = t->ink, .size = 16, .wrap = "e");
    }
    if (icon_button("new_note", rcIconSquarePen, INK_SOFT, "New note", a->coarse))
        notes_new(st, NULL);
    if (icon_button("toggle_search", rcIconSearch, st->searchOpen ? INK_ON : INK_SOFT,
                    "Search all text", a->coarse)) {
        if (st->searchOpen)
            notes_close_search(st);
        else
            notes_open_search(st);
    }
}

/* The band's sidebar section: the app's name, and settings. */
static void band_sidebar(AppState *st, const NotesPalette *t, const NotesArms *a) {
    rcBox(.bg = t->accent, .align = "cc", .borderRadius = "all-md", .w = "24px", .h = "24px") {
        rcIconSquarePen(14.0f, RC_WHITE);
    }
    rcBox(.overflow = "hidden", .w = "grow") {
        rcTextL("RayClay Notes", .font = st->faces.bold, .color = t->sidebarInk, .size = 14,
                .wrap = "e");
    }
    if (icon_button("open_settings", rcIconSettings, INK_MUTED, "Settings", a->coarse))
        st->modalSettings = true;
    if (icon_button("hide_sidebar", rcIconPanelLeft, INK_MUTED, "Hide the sidebar", a->coarse)) {
        st->sidebarHidden = a->sidebarFits;
        st->pane          = PANE_LIST;
    }
}

/* The band's editor section. The icons a note acts through sit at the right edge,
   where Bear keeps them. A menu is the library's, so it holds the actions a toolbar
   has no room for and every arm reaches them all. */
static void band_editor(AppState *st, const NotesPalette *t, const NotesArms *a, Note *n) {
    if (a->onePane && icon_button("back_to_list", rcIconArrowLeft, INK_SOFT, "Notes", a->coarse))
        st->pane = PANE_LIST;
    notes_view_switch(st, t, n, a);
    if (n && !n->trashed && !a->onePane && notes_shown_view(st, n, a) != VIEW_PREVIEW) {
        rcBox(.noDrag = true) {
            if (rcBeginMenu("append_menu", "Append", .caret = true)) {
                for (int i = 0; i < (int)(sizeof APPEND_LABELS / sizeof APPEND_LABELS[0]); i++) {
                    if (rcMenuItem(APPEND_LABELS[i])) {
                        note_append(&st->store, n, APPEND_TEXT[i]);
                        st->backlinksStale = true;
                        st->focusField     = true;
                        st->pageToEnd      = 2;
                    }
                }
                rcEndMenu();
            }
        }
    }
    rcSpacer() {}
    if (n) {
        if (!a->onePane && icon_button("open_reader", rcIconBookOpen, INK_SOFT, "Reader mode",
                                       a->coarse))
            notes_enter_reader(st);
        /* One pane's band has no room for it, and the menu beside it offers the same. */
        if (!a->onePane && icon_button("toggle_pin", rcIconPin, n->pinned ? INK_ON : INK_SOFT,
                                       n->pinned ? "Unpin" : "Pin to top", a->coarse))
            notes_toggle_pin(st, n);
        rcBox(.noDrag = true) {
            if (rcBeginMenu("note_menu", "", .className = a->coarse ? "h-11" : "h-8",
                            .variant = RC_BTN_GHOST, .leadingIcon = rcIconEllipsisVertical)) {
                if (a->onePane && rcMenuItem("Reader mode", .leadingIcon = rcIconBookOpen))
                    notes_enter_reader(st);
                note_menu_items(st, st->openNote);
                rcEndMenu();
            }
        }
    }
}

/* The graphite column is a dark region in a light app, so what sits on it takes a style
   of its own, a theme for one region. It is a style rather than classes because of the
   tooltips of the band's icon buttons: a tooltip takes the panel (surface), edge (border)
   and text of the style installed where its box is declared, and no class reaches it, so
   graphite ones keep the light ink off a paper panel. The buttons read the same style: a
   ghost button's hover (surface-alt) and the muted ink (text-muted-foreground). When the
   sidebar is the only pane the window controls sit on graphite and read it too, so their
   glyph lifts toward white over close's red as a bundled slab's does, where a class's
   text color would hold it. */
static RC_Style graphite_style(const NotesPalette *t, bool dark) {
    RC_Style s = notes_style(dark);

    s.text       = t->sidebarInk;
    s.textMuted  = t->sidebarMuted;
    s.surface    = t->sidebar;
    s.surfaceAlt = t->sidebarHover;
    s.border     = t->sidebarHover;
    return s;
}

/* CHROME, NOT CONTENT: the OS drag strip is pinned at .titlebar.height, so the band is
   counter-scaled with rcUnzoomed() and keeps its height at every zoom. Its sections
   sit over the columns below, which do zoom, so a section's width is the column's
   width times the zoom: the one multiplication this scope ever asks for. */
static void notes_band(AppState *st, const NotesPalette *t, const NotesArms *a) {
    Note    *n      = note_at(&st->store, st->openNote);
    RC_Color ground = a->editor ? t->paper : (a->list ? t->list : t->sidebar);

    rcUnzoomed()
    rcRow(.id = RC_ID_WINDOW_DRAG, .bg = ground, .align = "cl", .w = "grow", .h = "48px") {
        if (a->sidebar) {
            rcSetStyle(graphite_style(t, st->dark));
            rcRow(.bg = t->sidebar, .gap = 4, .pl = 12, .pr = 4, .align = "cl", .h = "grow",
                  .wType = a->onePane ? RC_GROW : RC_PX(a->sidebarW * a->zoom),
                  .className = "min-w-0") {
                band_sidebar(st, t, a);
            }
        }
        /* Only the sidebar's own arm ends on graphite; the window controls then read it. */
        if (a->list || a->editor)
            rcSetStyle(notes_style(st->dark));
        if (a->list) {
            /* With no sidebar button before it, the title starts where the rows' text does:
               NoteList's 6, the selection bar's 3 and the row's 11. */
            rcRow(.bg = t->list, .gap = 2, .pl = (float)(a->sidebar ? 20 : 12), .pr = 6,
                  .align = "cl", .h = "grow",
                  .wType = a->onePane ? RC_GROW : RC_PX(a->listW * a->zoom),
                  .className = "min-w-0") {
                band_list(st, t, a);
            }
            if (a->editor) {
                rcBox(.bg = t->rule, .w = "1px", .h = "grow") {}
            }
        }
        if (a->editor) {
            rcRow(.bg = t->paper, .gap = 6, .pl = 10, .pr = 6, .align = "cl", .w = "grow",
                  .h = "grow", .className = "min-w-0") {
                band_editor(st, t, a, n);
            }
        }
        /* Band-tall and flush to the corner, so the OS rounds the close slab with the
           window rather than leaving a strip beside it. */
        rcWindowControls();
        rcSetStyle(notes_style(st->dark));
    }
}

/* The reader's band: the way back, how the type is set, how far through you are. One
   pane keeps only what fits beside the window controls: the steps move to the contents
   pane, and the percentage goes, because the scrollbar already says it. */
static void notes_reader_band(AppState *st, const NotesPalette *t, const NotesArms *a, Note *n,
                              bool rail) {
    rcUnzoomed()
    rcRow(.id = RC_ID_WINDOW_DRAG, .bg = t->paper, .gap = 8, .pl = 10, .align = "cl", .w = "grow",
          .h = "48px") {
        rcBox(.noDrag = true) {
            if (rcButton("reader_back", a->onePane ? "" : "Notes",
                         .className = a->coarse ? "h-11 ink-soft" : "h-8 ink-soft",
                         .variant = RC_BTN_GHOST, .leadingIcon = rcIconArrowLeft))
                st->reader = false;
        }
        if (!rail && icon_button("reader_toc", rcIconListTree,
                                 st->readerContents ? INK_ON : INK_SOFT, "Contents", a->coarse))
            st->readerContents = !st->readerContents;
        rcBox(.px = 8, .align = "cc", .overflow = "hidden", .w = "grow") {
            if (!a->onePane)
                rcTextC(n->title, .font = st->faces.bold, .color = t->ink, .size = 14, .wrap = "e");
        }
        if (!a->onePane) {
            rcBox(.w = "96px") {
                rcProgress("read_progress", st->readProgress);
            }
            rcBox(.align = "cr", .w = "36px") {
                rcText(rcFormat(&st->arena, "%d%%", (int)(st->readProgress * 100.0f + 0.5f)),
                       .color = t->inkMuted, .size = 13);
            }
            reader_text_steps(st, t);
        }
        if (icon_button("reader_theme", st->dark ? rcIconSun : rcIconMoon, INK_SOFT,
                        st->dark ? "Paper" : "Night", a->coarse))
            st->dark = !st->dark;
        rcWindowControls();
    }
}

/* -- dialogs and the toast --------------------------------------------------------- */

static void settings_row(const NotesPalette *t, const char *label, const char *hint) {
    rcColumn(.gap = 1, .w = "grow") {
        rcTextC(label, .color = t->ink, .size = 14);
        rcTextC(hint, .color = t->inkMuted, .size = 13);
    }
}

static void shortcut_row(const NotesPalette *t, const AppState *st, const char *keys,
                         const char *what) {
    rcRow(.gap = 12, .align = "cl", .w = "grow") {
        rcBox(.w = "grow") {
            rcTextC(what, .color = t->inkSoft, .size = 13);
        }
        rcBox(.bg = t->codeFill, .px = 6, .py = 1, .borderRadius = "all-sm",
              .border = { .color = t->rule, .width = 1 }) {
            rcTextC(keys, .font = st->faces.mono, .color = t->inkSoft, .size = 13);
        }
    }
}

/* The rows scroll and the title and Done stay, so a short window still shows the way out.
   viewH less the panel's padding, the title, Done and the gaps between them. */
static void notes_modals(AppState *st, const NotesPalette *t, bool compact, float viewH) {
    if (rcBeginModal("modal_settings", &st->modalSettings)) {
        rcColumn(.gap = 16, .w = compact ? "84vw" : "440px") {
            rcTextL("Settings", .font = st->faces.bold, .color = t->ink, .size = 20);
            rcColumn(.id = "SettingsBody", .gap = 16, .scroll = "v", .w = "grow",
                     .hMax = RC_VAL(viewH - 190.0f)) {
                rcRow(.gap = 16, .align = "cl", .w = "grow") {
                    settings_row(t, "Theme", "Paper by day, Night for the dark");
                    rcRow(.bg = t->control, .gap = 2, .p = 2, .borderRadius = "all-md") {
                        if (segment(t, "set_paper", "Paper", !st->dark, 13))
                            st->dark = false;
                        if (segment(t, "set_night", "Night", st->dark, 13))
                            st->dark = true;
                    }
                }
                rcRow(.gap = 16, .align = "cl", .w = "grow") {
                    settings_row(t, "Text size", "Sets the whole heading ramp");
                    rcRow(.bg = t->control, .gap = 2, .p = 2, .borderRadius = "all-md") {
                        if (segment(t, "set_s", "Small", st->textStep == 0, 13))
                            st->textStep = 0;
                        if (segment(t, "set_m", "Medium", st->textStep == 1, 13))
                            st->textStep = 1;
                        if (segment(t, "set_l", "Large", st->textStep == 2, 13))
                            st->textStep = 2;
                    }
                }
                rcRow(.gap = 16, .align = "cl", .w = "grow") {
                    settings_row(t, "Line length", "Characters per line in the page");
                    rcRow(.bg = t->control, .gap = 2, .p = 2, .borderRadius = "all-md") {
                        if (segment(t, "set_w0", "56", st->widthStep == 0, 13))
                            st->widthStep = 0;
                        if (segment(t, "set_w1", "68", st->widthStep == 1, 13))
                            st->widthStep = 1;
                        if (segment(t, "set_w2", "75", st->widthStep == 2, 13))
                            st->widthStep = 2;
                    }
                }
                rcRow(.gap = 16, .align = "cl", .w = "grow") {
                    /* The one place the default, a notebook that lasts until you quit, is
                       stated: a standing line in the status bar reads as a warning. */
                    settings_row(t, "Keep notes on this device",
                                 "Saves after each edit. When off, edits last until you quit");
                    rcToggle("set_keep", "", &st->keepOnDevice);
                }
                rcBox(.bg = t->rule, .w = "grow", .h = "1px") {}
                rcColumn(.gap = 6, .w = "grow") {
                    rcTextL("Keyboard: Ctrl, or Cmd on a Mac", .font = st->faces.bold,
                            .color = t->ink, .size = 14);
                    shortcut_row(t, st, "Ctrl+N", "New note");
                    shortcut_row(t, st, "Ctrl+F", "Search all text");
                    shortcut_row(t, st, "Ctrl+G", "Find the search in the note");
                    shortcut_row(t, st, "Ctrl+1 2 3", "Write, Split, Preview");
                    shortcut_row(t, st, "Ctrl+Shift+E", "Reader mode");
                    shortcut_row(t, st, "Ctrl+\\", "Hide the sidebar");
                    shortcut_row(t, st, "Ctrl+Z", "Undo a delete, outside a field");
                }
            }
            rcScrollbar("SettingsBody");
            rcRow(.pr = 12, .align = "cr", .w = "grow") {
                if (rcButton("settings_done", "Done", .variant = RC_BTN_PRIMARY))
                    st->modalSettings = false;
            }
        }
        rcEndModal();
    }

    if (rcBeginModal("modal_empty_trash", &st->modalEmptyTrash)) {
        int count = note_section_count(&st->store, NOTE_SEC_TRASH, NULL);

        rcColumn(.gap = 12, .w = compact ? "84vw" : "380px") {
            rcTextL("Empty the Trash?", .font = st->faces.bold, .color = t->ink, .size = 20);
            rcText(rcFormat(&st->arena, "This deletes %d note%s for good. It cannot be undone.",
                            count, count == 1 ? "" : "s"),
                   .color = t->inkSoft, .size = 14);
            rcRow(.gap = 8, .align = "cr", .w = "grow") {
                if (rcButton("trash_cancel", "Cancel", .variant = RC_BTN_GHOST))
                    st->modalEmptyTrash = false;
                if (rcButton("trash_confirm", "Delete forever", .variant = RC_BTN_DANGER)) {
                    for (int i = 0; i < st->store.count; i++)
                        if (st->store.notes[i].used && st->store.notes[i].trashed)
                            notes_destroy(st, i);
                    st->modalEmptyTrash = false;
                }
            }
        }
        rcEndModal();
    }
}

static void notes_toast_view(AppState *st, const NotesPalette *t) {
    if (st->toastLeft <= 0.0f)
        return;
    rcRow(.id = "Toast", .bg = t->toast, .gap = 12, .pl = 16, .pr = 8, .py = 8, .align = "cl",
          .borderRadius = "all-lg",
          .shadow = { .color = rcWithAlpha(RC_BLACK, 0.275f), .y = 6, .blur = 18, .spread = -2 },
          .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_BOTTOM_CENTER,
                        .element = RC_ANCHOR_BOTTOM_CENTER, .offset = { 0.0f, -44.0f },
                        .zIndex = 800 }) {
        rcTextC(st->toastText, .color = t->toastInk, .size = 14);
        if (st->undoKind != UNDO_NONE &&
            rcButton("toast_undo", "Undo", .className = rcPointerIsCoarse() ? "h-11" : "h-7"))
            notes_undo(st);
    }
}

/* -- keys -------------------------------------------------------------------------- */

/* The next match of `query` in `body` at or after byte `from`, wrapping to the top;
   case-insensitive, as the list's search matches. -1 for none. */
static int notes_find_next(const char *body, int from, const char *query) {
    int len = (int)strlen(body), qlen = (int)strlen(query);

    for (int pass = 0; pass < 2; pass++) {
        for (int i = pass ? 0 : from; i + qlen <= len; i++) {
            int k = 0;

            while (k < qlen && note_fold(body[i + k]) == note_fold(query[k]))
                k++;
            if (k == qlen)
                return i;
        }
    }
    return -1;
}

static void notes_keys(AppState *st, bool onePane) {
    bool primary = rcModDown(RC_MOD_PRIMARY);
    bool shift   = rcModDown(RC_MOD_SHIFT);
    bool inField = rcIsFocused("search") || rcIsFocused("toc_search") ||
                   rcIsFocused(rcFormat(&st->arena, "src.%d", st->openNote).chars);

    if (rcIsModalOpen())
        return;
    if (primary && rcKeyPressed(RC_KEY_N))
        notes_new(st, NULL);
    if (primary && rcKeyPressed(RC_KEY_F))
        notes_open_search(st);
    if (primary && rcKeyPressed(RC_KEY_1))
        st->view = VIEW_WRITE;
    if (primary && rcKeyPressed(RC_KEY_2) && !onePane)
        st->view = VIEW_SPLIT;
    if (primary && rcKeyPressed(RC_KEY_3))
        st->view = VIEW_PREVIEW;
    if (primary && shift && rcKeyPressed(RC_KEY_E) && st->openNote >= 0) {
        if (st->reader)
            st->reader = false;
        else
            notes_enter_reader(st);
    }
    if (primary && rcKeyPressed(RC_KEY_BACKSLASH))
        st->sidebarHidden = !st->sidebarHidden;
    /* FIND NEXT: the list's search, in the open note's text, from its caret on. The
       field selects the match, focused if it was not, as a browser's find bar does. */
    if (primary && rcKeyPressed(RC_KEY_G) && st->search[0] && st->openNote >= 0) {
        const char       *src  = rcFormat(&st->arena, "src.%d", st->openNote).chars;
        const Note       *note = note_at(&st->store, st->openNote);
        RC_FieldSelection sel  = rcGetFieldSelection(src);
        int               at   = notes_find_next(note->body, sel.found ? sel.caret : 0, st->search);

        if (at >= 0) {
            rcSetFocus(src);
            rcSetFieldSelection(src, at, at + (int)strlen(st->search));
        }
    }
    if (primary && rcKeyPressed(RC_KEY_Z) && st->undoKind != UNDO_NONE && st->toastLeft > 0.0f &&
        !inField)
        notes_undo(st);
    /* A focused field takes the press to blur itself, so rcKeyPressed reads false. A
       search has nothing to keep focus for, so the one Esc also closes it. Read only with
       something to close, so Android's Back otherwise stays the system's. */
    if ((st->reader || st->searchOpen)
        && (rcKeyPressed(RC_KEY_ESCAPE) || rcKeyPressedOn("search", RC_KEY_ESCAPE))) {
        if (st->reader)
            st->reader = false;
        else if (st->searchOpen)
            notes_close_search(st);
    }
}

/* -- the four-function contract ---------------------------------------------------- */

void notes_seed(AppState *st, unsigned seed) {
    (void)seed;                         /* the notebook is the same on every run */
    note_memzero(st, sizeof *st);
    note_store_seed(&st->store);
    st->arena         = rcArenaInit(NOTES_ARENA_BYTES);
    st->section       = NOTE_SEC_ALL;
    st->view          = VIEW_PREVIEW;
    st->pane          = PANE_LIST;      /* a one-pane window opens on the list */
    st->notesOpen     = true;
    st->textStep      = 1;
    st->widthStep     = 1;
    st->splitFraction = 0.5f;
    st->requestExport = -1;
    st->requestWindow = -1;
    st->undoNote      = -1;
    st->seeded        = true;
    {
        NotePane pane = st->pane;

        notes_open(st, 2);              /* the note the first frame shows */
        st->pane = pane;
    }
}

void notes_set_clock(AppState *st, double unixSeconds, int utcOffset) {
    note_store_set_clock(&st->store, (uint32_t)unixSeconds, (int32_t)utcOffset);
}

void notes_update(AppState *st, const AppCtx *ctx) {
    note_store_step(&st->store, ctx->dt);   /* dt <= 0 is the freeze: a no-op */
    if (ctx->dt > 0.0f) {
        if (st->toastLeft > 0.0f)
            st->toastLeft -= ctx->dt;
        if (st->copiedLeft > 0.0f)
            st->copiedLeft -= ctx->dt;
    }
    if (st->backlinksStale) {
        st->backlinkCount  = note_backlinks(&st->store, st->openNote, st->backlinks,
                                            NOTES_MAX_BACKLINKS);
        st->backlinksStale = false;
    }
}

void notes_layout(AppState *st, const AppCtx *ctx) {
    NotesPalette t    = notes_palette(st->dark);
    RC_Insets    safe = app_safe(ctx);
    float        innerW = app_view_w(ctx) - safe.left - safe.right;
    NotesArms    a;
    Note        *open;

    /* A theme for every caller, so the demo and the bench render the same app. */
    notes_install_theme(st->dark);
    rcArenaReset(&st->arena);
    notes_keys(st, innerW < NOTES_ONE_PANE_W);
    notes_refresh_order(st);
    open = note_at(&st->store, st->openNote);

    /* THE ARMS ARE DECIDED ON THE SPACE, never the platform, off the measured viewport,
       so a desktop window dragged narrow takes the same branch a phone does. The pane
       that must not lose width is the editor: three panes while it keeps its minimum,
       then the sidebar shares the list's slot, then below NOTES_ONE_PANE_W the panes
       take turns, one at a time and never a squeezed version of both. */
    memset(&a, 0, sizeof a);
    a.coarse      = ctx->view.coarsePointer;
    a.shortView   = app_view_h(ctx) - safe.top - safe.bottom < NOTES_SHORT_H;
    a.zoom        = ctx->zoom > 0.0f ? ctx->zoom : 1.0f;
    a.sidebarW    = NOTES_SIDEBAR_W;
    a.listW       = innerW >= 1100.0f ? NOTES_LIST_W : NOTES_LIST_SLIM_W;
    a.onePane     = innerW < NOTES_ONE_PANE_W;
    a.sidebarFits = innerW - a.sidebarW - a.listW >= NOTES_EDITOR_MIN;
    if (a.onePane) {
        a.sidebar = st->pane == PANE_SIDEBAR;
        a.list    = st->pane == PANE_LIST;
        a.editor  = st->pane == PANE_EDITOR;
    } else if (a.sidebarFits) {
        a.sidebar = !st->sidebarHidden;
        a.list    = true;
        a.editor  = true;
    } else {
        a.sidebar = st->pane == PANE_SIDEBAR;
        a.list    = st->pane != PANE_SIDEBAR;
        a.editor  = true;
    }
    a.editorW = innerW - (a.sidebar && !a.onePane ? a.sidebarW : 0.0f) -
                (a.list && !a.onePane ? a.listW + 1.0f : 0.0f);

    rcColumn(.id = "Root", .bg = t.paper, .pt = safe.top, .pb = safe.bottom,
             .pl = safe.left, .pr = safe.right, .w = "grow", .h = "grow") {
        if (st->reader && open) {
            /* The rail stays only while the page beside it still holds its measure. */
            bool rail = innerW - NOTES_TOC_W - notes_measure_px(st) - 96.0f >= 0.0f;

            notes_reader_band(st, &t, &a, open, rail);
            notes_reader(st, &t, open, innerW, rail, &a);
        } else {
            notes_band(st, &t, &a);
            rcRow(.id = "Body", .w = "grow", .h = "grow") {
                if (a.sidebar)
                    notes_sidebar(st, &t, &a);
                if (a.list) {
                    notes_list(st, &t, &a);
                    if (a.editor) {
                        rcBox(.bg = t.rule, .w = "1px", .h = "grow") {}
                    }
                }
                if (a.editor)
                    notes_editor(st, &t, &a);
            }
        }
        notes_toast_view(st, &t);
    }
    notes_modals(st, &t, a.onePane, app_view_h(ctx) - safe.top - safe.bottom);

    /* Any command this frame may have left the backlinks to recompute in notes_update. */
    if (st->backlinksStale)
        st->requestFrame = true;
}

/* One note read-only, for a window of its own. It writes nothing to the notebook, so
   there is one editor over each buffer, and the title stays live, which is what makes
   it obviously ONE note in two places. */
void notes_note_window(AppState *st, int noteIndex, RC_Arena *arena) {
    NotesPalette t = notes_palette(st->dark);
    Note        *n = note_at(&st->store, noteIndex);

    notes_install_theme(st->dark);
    rcColumn(.id = "WindowRoot", .bg = t.paper, .w = "grow", .h = "grow") {
        rcUnzoomed()
        rcRow(.id = RC_ID_WINDOW_DRAG, .bg = t.list, .gap = 10, .pl = 16, .align = "cl",
              .w = "grow", .h = "44px") {
            rcBox(.overflow = "hidden", .w = "grow") {
                rcTextC(n ? n->title : "Note", .font = st->faces.bold, .color = t.ink, .size = 14,
                        .wrap = "e");
            }
            rcWindowControls();
        }
        if (n) {
            RC_Box col  = rcGetElementBox("WindowCol");
            MdPage page = notes_page(st, &t, n, "win", col.found ? col.width : 460.0f, arena, true);

            rcColumn(.id = "WindowPage", .p = 24, .scroll = "v", .w = "grow", .h = "grow") {
                rcColumn(.id = "WindowCol", .w = "grow") {
                    md_render(&page);
                }
            }
            rcScrollbar("WindowPage");
        } else {
            rcColumn(.align = "cc", .w = "grow", .h = "grow") {
                rcTextL("This note is not in the notebook.", .color = t.inkMuted, .size = 14);
            }
        }
    }
}

size_t notes_write_notebook(const AppState *st, char *out, size_t cap) {
    return note_store_write(&st->store, out, cap);
}

bool notes_read_notebook(AppState *st, const char *text) {
    if (!note_store_read(&st->store, text))
        return false;
    st->keepOnDevice   = true;
    st->triedRevision  = st->store.revision;
    st->savedRevision  = st->store.revision;
    st->storedRevision = st->store.revision;
    notes_keep_selection(st);
    return true;
}

/* -- the scripted scenario --------------------------------------------------------- */

/* Press an element by asking where it is: the same box the hit test uses, so the
   script survives a layout change and every action runs the real input path. */
static void bench_press(const AppInputSink *in, const char *id) {
    RC_Box box = rcGetElementBox(id);

    if (box.found) {
        in->move(in->ctx, box.x + box.width * 0.5f, box.y + box.height * 0.5f);
        in->button(in->ctx, APP_MBTN_LEFT, true);
    }
}

void notes_bench_step(AppState *st, const AppInputSink *in, int frame) {
    static const char TYPED[] = " looks great";
    (void)st;   /* every action here is synthetic input, never a direct state write */

    /* At and after NOTES_BENCH_WARMUP the app HOLDS, a strict no-op, so a rendered frame
       is byte-identical. The caret blink and tooltip dwell run on real time, so the
       frozen frame must carry no focused field and the pointer must rest over nothing:
       the off-canvas press below guarantees both. */
    if (!in || frame >= NOTES_BENCH_WARMUP)
        return;

    if (frame == 6) {
        RC_Box list = rcGetElementBox("NoteList");

        if (list.found)
            in->move(in->ctx, list.x + list.width * 0.5f, list.y + list.height * 0.5f);
    } else if (frame >= 10 && frame < 16) {
        in->wheel(in->ctx, 0.0f, -2.0f);
    } else if (frame == 18) {
        bench_press(in, "view_write");
    } else if (frame == 19 || frame == 23 || frame == 41 || frame == 45 || frame == 49 ||
               frame == 51) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame == 22) {
        /* Right of the title's text clamps the caret to its end. Short of the box's far
           edge, which is the text area's scrollbar. */
        RC_Box box = rcGetElementBox("src.2");

        if (box.found) {
            in->move(in->ctx, box.x + box.width - 48.0f, box.y + 16.0f);
            in->button(in->ctx, APP_MBTN_LEFT, true);
        }
    } else if (frame >= 26 && frame < 38) {
        in->text(in->ctx, (unsigned int)(unsigned char)TYPED[frame - 26]);
    } else if (frame == 40) {
        bench_press(in, "view_split");
    } else if (frame == 44) {
        bench_press(in, "toggle_pin");
    } else if (frame == 48) {
        bench_press(in, "view_preview");
    } else if (frame == 50) {
        /* Pin it back and scroll back up, so the frozen frame shows the selected row. */
        bench_press(in, "toggle_pin");
    } else if (frame == 52) {
        RC_Box list = rcGetElementBox("NoteList");

        if (list.found)
            in->move(in->ctx, list.x + list.width * 0.5f, list.y + list.height * 0.5f);
    } else if (frame >= 54 && frame < 60) {
        in->wheel(in->ctx, 0.0f, 2.0f);
    } else if (frame == NOTES_BENCH_WARMUP - 2) {
        in->move(in->ctx, -100.0f, -100.0f);
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == NOTES_BENCH_WARMUP - 1) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    }
}
