/* Minimal Dual Scroller plugin
 * Only contains what's necessary to declare the layout and related
 * dispatches. Implementation is adapted from dual_scroller.md.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/util/box.h>

/* Plugin API and compositor types */
#include "plugin/plugin.h"
#include "mango-types.h"

/* Local Layout type compatible with core Layout */
typedef struct Layout {
    const char *symbol;
    void (*arrange)(Monitor *);
    const char *name;
    uint32_t id;
    uint32_t flags;
} Layout;

/* External symbols provided by the compositor at runtime */
extern struct wl_list clients;
extern int enablegaps;
extern int smartgaps;
extern int32_t scroller_structs;
extern int32_t scroller_focus_center;
extern int32_t scroller_prefer_center;
/* plugin-local default split ratio to avoid relying on core symbols */
static float dual_scroller_default_split_ratio = 0.3f;
extern void resize(Client *c, struct wlr_box geo, int interact);
/* Use accessors exported by the core instead of referencing globals
 * or static functions directly. Prototypes live in mango-types.h */
extern bool is_row_layout(Monitor *m);

/* Helper macros (ABI-compatible assumptions) */
#define VISIBLEON(c, m) (((c)->mon == (m)) && ((c)->tags & (m)->tagset[(m)->seltags]))
#define ISSCROLLTILED(c) ((c) && !(c)->isfloating && !(c)->isminimized && !(c)->iskilling && !(c)->ismaximizescreen && !(c)->isfullscreen && !(c)->isunglobal)

/* Per-client row state kept inside plugin to avoid touching core Client struct */
#define MAX_CLIENTS 256
static struct {
    Client *client;
    int32_t row; /* 0 top, 1 bottom, -1 unassigned */
} client_row_map[MAX_CLIENTS];
static int32_t client_row_count = 0;

static int32_t get_client_row(Client *c) {
    for (int i = 0; i < client_row_count; ++i) {
        if (client_row_map[i].client == c)
            return client_row_map[i].row;
    }
    return -1;
}

static void set_client_row(Client *c, int32_t row) {
    for (int i = 0; i < client_row_count; ++i) {
        if (client_row_map[i].client == c) {
            client_row_map[i].row = row;
            return;
        }
    }
    if (client_row_count < MAX_CLIENTS) {
        client_row_map[client_row_count].client = c;
        client_row_map[client_row_count].row = row;
        client_row_count++;
    }
}

/* Dispatch: toggle row assignment for focused client */
static int32_t togglerow(const void *arg) {
    Client *c = NULL;
    Monitor *m_sel = get_selmon();
    if (!m_sel || !m_sel->sel || !is_row_layout(m_sel))
        return 0;
    c = m_sel->sel;
    if (c->isfloating || !ISSCROLLTILED(c) || !VISIBLEON(c, m_sel))
        return 0;
    int32_t row = get_client_row(c);
    if (row == 0)
        set_client_row(c, 1);
    else
        set_client_row(c, 0);
    arrange_mon(m_sel, false, false);
    return 0;
}

/* Dispatch: adjust dual scroller split ratio */
static int32_t adjust_dual_scroller_split(const void *arg) {
    const struct { float f; } *a = arg;
    float new_ratio;
    Monitor *m_sel = get_selmon();
    if (!a || !m_sel)
        return 0;
    if (!is_row_layout(m_sel))
        return 0;
    new_ratio = a->f < 1.0f ? dual_scroller_default_split_ratio + a->f : a->f - 1.0f;
    if (new_ratio < 0.1f || new_ratio > 0.9f)
        return 0;
    dual_scroller_default_split_ratio = new_ratio;
    arrange_mon(m_sel, false, false);
    return 0;
}

/* Compact dual scroller arrange implementation adapted from docs
 * - collects ISSCROLLTILED clients on the monitor
 * - assigns rows using Client::dual_scroller_row (defaults to bottom)
 * - lays out each row independently using per-client scroller_proportion
 */
static void dual_scroller_arrange(Monitor *m) {
    if (!m)
        return;

    printf("[DS] arrange: monitor=%p sel=%p tags=%u\n", (void *)m, (void *)m->sel, (unsigned)m->seltags);

    uint32_t n_total = m->visible_scroll_tiling_clients;
    printf("[DS] arrange: visible_scroll_tiling_clients=%u visible_tiling_clients=%u\n", n_total, m->visible_tiling_clients);
    if (n_total == 0) {
        printf("[DS] arrange: nothing to do (n_total==0)\n");
        return;
    }

    Client *c = NULL;
    uint32_t top_count = 0, bottom_count = 0;

    /* first pass: ensure clients have a row and count */
    wl_list_for_each(c, &clients, link) {
        uint32_t ctags = c->tags;
        uint32_t mtag = m->tagset[m->seltags];
        bool vis = VISIBLEON(c, m);
        bool scrolltiled = ISSCROLLTILED(c);
        printf("[DS] client %p mon=%p m=%p tags=0x%08x mon_tag=0x%08x seltags_idx=%d isfloating=%d ismin=%d iskilling=%d ismaxscr=%d isfs=%d isunglobal=%d\n",
               (void *)c, (void *)c->mon, (void *)m, (unsigned)ctags, (unsigned)mtag, (int)m->seltags,
               (int)c->isfloating, (int)c->isminimized, (int)c->iskilling, (int)c->ismaximizescreen, (int)c->isfullscreen, (int)c->isunglobal);
        if (!vis || !scrolltiled) {
            printf("[DS] skipping client %p vis=%d scrolltiled=%d\n", (void *)c, vis, scrolltiled);
            continue;
        }
        int32_t row = get_client_row(c);
        if (row < 0) {
            set_client_row(c, 1); /* default bottom */
            printf("[DS] set default row for client %p -> bottom\n", (void *)c);
        }
        row = get_client_row(c);
        if (row == 0)
            top_count++;
        else
            bottom_count++;
    }

    printf("[DS] counts: top=%u bottom=%u\n", top_count, bottom_count);

    Client **top = NULL, **bottom = NULL;
    top = top_count ? calloc(top_count, sizeof(Client *)) : NULL;
    bottom = bottom_count ? calloc(bottom_count, sizeof(Client *)) : NULL;
    if ((top_count && !top) || (bottom_count && !bottom)) {
        free(top); free(bottom); return;
    }

    /* fill arrays */
    uint32_t ti = 0, bi = 0;
    wl_list_for_each(c, &clients, link) {
        if (!VISIBLEON(c, m) || !ISSCROLLTILED(c))
            continue;
        int32_t row = get_client_row(c);
        if (row == 0)
            top[ti++] = c;
        else
            bottom[bi++] = c;
    }

    printf("[DS] filled arrays: ti=%u bi=%u expected top=%u bottom=%u\n", ti, bi, top_count, bottom_count);

    int ie = enablegaps;
    int32_t cur_gappih = ie ? m->gappih : 0;
    int32_t cur_gappov = ie ? m->gappov : 0;
    if (smartgaps && m->visible_scroll_tiling_clients == 1) {
        cur_gappih = cur_gappov = 0;
    }

    int32_t max_client_width = m->w.width - 2 * scroller_structs - cur_gappih;
    if (max_client_width < 1) max_client_width = 1;

    int32_t avail_h = m->w.height - 2 * cur_gappov;
    if (avail_h < 1) avail_h = 1;
    int32_t top_h = (int32_t)(avail_h * dual_scroller_default_split_ratio);
    int32_t bottom_h = avail_h - top_h;
    if (bottom_count == 0) { top_h = avail_h; bottom_h = 0; }
    if (top_count == 0) { top_h = 0; bottom_h = avail_h; }

    int32_t top_y = m->w.y + cur_gappov;
    int32_t bottom_y = top_y + top_h + (top_count && bottom_count ? m->gappiv : 0);

    /* helper to layout a row */
    void layout_row(Client **row, uint32_t nrow, int32_t row_y, int32_t row_h, bool is_top) {
        if (!nrow) return;
        /* choose focused client if any */
        int focus_idx = 0;
        for (uint32_t i = 0; i < nrow; ++i)
            if (row[i] == m->sel) { focus_idx = i; break; }

        Client *focused = row[focus_idx];
        int32_t focused_width = (nrow == 1) ? max_client_width : (max_client_width / 2);
        struct wlr_box geo = {0};
        geo.y = row_y; geo.height = row_h; geo.width = focused_width;

        /* center logic simplified per docs */
        if ((scroller_focus_center || scroller_prefer_center) && !is_top && nrow > 1)
            geo.x = m->w.x + (m->w.width - focused_width) / 2;
        else
            geo.x = m->w.x + scroller_structs;

        printf("[DS] resize focused %p at x=%d y=%d w=%d h=%d\n", (void *)focused, geo.x, geo.y, geo.width, geo.height);
        resize(focused, geo, 0);

        /* left of focus */
        int32_t left_x = geo.x;
        for (int i = focus_idx - 1; i >= 0; --i) {
            Client *lc = row[i];
            int32_t w = max_client_width / nrow;
            left_x -= (w + cur_gappih);
            geo.x = left_x; geo.width = w;
            printf("[DS] resize left %p at x=%d y=%d w=%d h=%d\n", (void *)lc, geo.x, geo.y, geo.width, geo.height);
            resize(lc, geo, 0);
        }

        /* right of focus */
        int32_t right_x = geo.x + geo.width + cur_gappih;
        for (uint32_t i = focus_idx + 1; i < nrow; ++i) {
            Client *rc = row[i];
            int32_t w = max_client_width / nrow;
            geo.x = right_x; geo.width = w;
            printf("[DS] resize right %p at x=%d y=%d w=%d h=%d\n", (void *)rc, geo.x, geo.y, geo.width, geo.height);
            resize(rc, geo, 0);
            right_x += w + cur_gappih;
        }
    }

    layout_row(top, top_count, top_y, top_h, true);
    layout_row(bottom, bottom_count, bottom_y, bottom_h, false);

    free(top); free(bottom);
}

/* Plugin init */
PluginInfo* plugin_init(void) {
    static Layout plugin_layouts[] = {
        {
            .symbol = "DS",
            .arrange = dual_scroller_arrange,
            .name = "dual_scroller",
            .id = 1000,
            .flags = LAYOUT_FLAG_SCROLLER | LAYOUT_FLAG_ROW
        }
    };

    static PluginInfo info = {
        .name = "dual_scroller",
        .version = "0.1.0",
        .description = "Dual Scroller Layout - Two-row tiling with adjustable split",
        .layouts = plugin_layouts,
        .num_layouts = 1
    };

    /* Register dispatches with the compositor */
    plugin_register_dispatch("togglerow", (PluginFuncType)togglerow);
    plugin_register_dispatch("adjust_dual_scroller_split", (PluginFuncType)adjust_dual_scroller_split);

    return &info;
}
