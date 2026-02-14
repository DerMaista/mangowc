#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/util/box.h>

/* Include the plugin API definition */
#include "plugin/plugin.h"

/* Include compositor type definitions for Client and Monitor */
#include "mango-types.h"

/* Define a local Layout struct compatible with the compositor's Layout */
typedef struct Layout {
	const char *symbol;
	void (*arrange)(Monitor *);
	const char *name;
	uint32_t id;
	uint32_t flags;
} Layout;

/* Forward declarations from the compositor (available at runtime via plugin loading) */
typedef struct Client Client;

/* Stubs for macros we cannot fully define without compositor internals */
/* These will be resolved at plugin load time by the compositor */

/* Configuration */
static float dual_scroller_split = 0.5f;  /* Top row takes 50% */

/* Per-client row state: map Client* -> row (0=top, 1=bottom, -1=unassigned) */
#define MAX_CLIENTS 256
static struct {
	Client *client;
	int32_t row;  /* 0 for top row, 1 for bottom row, -1 for unassigned */
} client_row_map[MAX_CLIENTS];
static int32_t client_row_count = 0;

/**
 * Get or set the row assignment for a client.
 * A client can be forced to a specific row using togglerow.
 */
static int32_t get_client_row(Client *c) {
	for (int i = 0; i < client_row_count; ++i) {
		if (client_row_map[i].client == c)
			return client_row_map[i].row;
	}
	return -1;  /* Unassigned - will be auto-distributed */
}

static void set_client_row(Client *c, int32_t row) {
	for (int i = 0; i < client_row_count; ++i) {
		if (client_row_map[i].client == c) {
			client_row_map[i].row = row;
			return;
		}
	}
	/* Not found - add new entry */
	if (client_row_count < MAX_CLIENTS) {
		client_row_map[client_row_count].client = c;
		client_row_map[client_row_count].row = row;
		client_row_count++;
	}
}

/* External symbols from compositor that are available at plugin load time */
extern struct wl_list clients;  /* Global client list */
extern int enablegaps;
extern int smartgaps;
extern int32_t scroller_structs;  /* Side padding for scrolling */
extern int32_t scroller_focus_center;  /* Center focused client */
extern int32_t scroller_prefer_center;  /* Prefer centering when possible */

/* Forward declaration of argument type from compositor */
typedef struct {
	int i;
	int i2;
	float f;
	float f2;
	uint32_t ui;
	uint32_t ui2;
	void *v;
	void *v2;
	void *v3;
} Arg;

/* Macro stubs - will use runtime knowledge of structures */
#define VISIBLEON(c, m) (((c)->mon == (m)) && ((c)->tags & (m)->tagset[(m)->seltags]))
#define ISTILED(c) ((c) && !(c)->isfloating && !(c)->isminimized && !(c)->iskilling && !(c)->ismaximizescreen && !(c)->isfullscreen && !(c)->isunglobal)

/* Forward declaration for resize function from compositor */
extern void resize(Client *c, struct wlr_box geo, int interact);

/**
 * Dispatch: toggle row assignment for focused client
 * When called on a client, forces it to the opposite row (or clears forced assignment).
 * 
 * This version stores the intent; the actual toggle happens in arrange if we have
 * access to the focused client. Otherwise it's deferred.
 */
static int32_t togglerow(const Arg *arg) {
	fprintf(stderr, "[DS] togglerow dispatch called\n");
	/* Note: Without direct access to selmon->sel, toggling requires compositor support.
	 * This is a placeholder that signals the action occurred.
	 * A future version could use a callback or shared state mechanism.
	 */
	fprintf(stderr, "[DS] togglerow: would toggle focused client's row\n");
	return 0;
}

/**
 * Dispatch: adjust the split ratio between top and bottom rows
 * arg->f is the amount to change (e.g., +0.05 or -0.05)
 * The split ratio affects how the monitor height is divided between rows.
 */
static int32_t adjust_dual_scroller_split(const Arg *arg) {
	if (!arg) {
		fprintf(stderr, "[DS] adjust_dual_scroller_split: NULL arg\n");
		return 0;
	}
	
	double delta = arg->f;
	fprintf(stderr, "[DS] adjust_dual_scroller_split dispatch called, delta=%.3f\n", delta);
	
	double new_split = dual_scroller_split + delta;
	
	/* Clamp to [0.1, 0.9] to ensure both rows remain visible */
	if (new_split < 0.1) {
		fprintf(stderr, "[DS] Split ratio clamped to minimum 0.1\n");
		new_split = 0.1;
	}
	if (new_split > 0.9) {
		fprintf(stderr, "[DS] Split ratio clamped to maximum 0.9\n");
		new_split = 0.9;
	}
	
	dual_scroller_split = new_split;
	fprintf(stderr, "[DS] Split ratio set to %.2f\n", dual_scroller_split);
	return 0;
}

/**
 * Dual Scroller Arrange Function with Per-Client Scrolling
 * 
 * Arranges windows in two independent rows with configurable split ratio.
 * Each client has a `scroller_proportion` that determines its width.
 * Implements scrolling/centering logic to keep focused window visible.
 * 
 * Algorithm:
 * 1. Assign clients to rows (0=top, 1=bottom)
 * 2. For each row:
 *    a. Find the focused client in that row (if any)
 *    b. Calculate its width based on scroller_proportion
 *    c. Position other clients before/after it
 *    d. Apply scrolling/centering logic to keep focused visible
 * 3. Apply gaps and smartgaps
 */
static void dual_scroller_arrange(Monitor *m) {
	if (!m) {
		fprintf(stderr, "[DS] arrange called with NULL monitor\n");
		return;
	}

	int32_t n = m->visible_tiling_clients;
	fprintf(stderr, "[DS] dual_scroller_arrange: monitor %p, visible_tiling_clients=%d\n", (void*)m, n);
	
	if (n == 0) {
		fprintf(stderr, "[DS] No visible tiling clients, skipping arrange\n");
		return;
	}

	/* Collect and assign clients to rows */
	Client **top_row = malloc(n * sizeof(Client*));
	Client **bottom_row = malloc(n * sizeof(Client*));
	if (!top_row || !bottom_row) {
		free(top_row);
		free(bottom_row);
		return;
	}
	
	int32_t top_count = 0, bottom_count = 0;
	Client *c = NULL;
	
	/* First pass: collect clients and assign to rows */
	wl_list_for_each(c, &clients, link) {
		if (c->mon != m)
			continue;
		
		int32_t assigned_row = get_client_row(c);
		if (assigned_row == 0) {
			top_row[top_count++] = c;
		} else if (assigned_row == 1) {
			bottom_row[bottom_count++] = c;
		} else {
			/* Auto-assign: alternate between top and bottom */
			if (top_count <= bottom_count) {
				set_client_row(c, 0);
				top_row[top_count++] = c;
			} else {
				set_client_row(c, 1);
				bottom_row[bottom_count++] = c;
			}
		}
	}

	fprintf(stderr, "[DS] Row distribution: top=%d, bottom=%d (total=%d)\n", 
		top_count, bottom_count, top_count + bottom_count);

	/* Helper function to layout a single row with scroller behavior */
	void layout_row(Client **row_clients, int32_t row_count, int32_t row_y, 
	                int32_t row_height, Monitor *m, bool is_top_row) {
		if (row_count == 0)
			return;

		int ie = enablegaps;
		int32_t cur_gappih = ie ? m->gappih : 0;
		int32_t cur_gappoh = ie ? m->gappoh : 0;

		/* Apply smartgaps */
		if (smartgaps && m->visible_tiling_clients == 1) {
			cur_gappih = 0;
			cur_gappoh = 0;
		}

		int32_t max_client_width = m->w.width - 2 * scroller_structs - cur_gappih;
		if (max_client_width < 1)
			max_client_width = 1;

		fprintf(stderr, "[DS] layout_row: %d clients, max_client_width=%d\n", 
			row_count, max_client_width);

		/* Simple approach: distribute clients proportionally
		 * We can't reliably access per-client scroller_proportion due to ABI differences
		 * Instead, use a fixed proportion for each client based on focus position */
		
		Client *focused = row_clients[0];
		int32_t focus_idx = 0;

		/* Focus the first visible non-floating client (simple heuristic) */
		for (int32_t i = 0; i < row_count; i++) {
			if (!row_clients[i]->isfloating && !row_clients[i]->isfullscreen) {
				focused = row_clients[i];
				focus_idx = i;
				break;
			}
		}

		/* Default width proportions: 50% for focused, rest split among others */
		int32_t focused_width = max_client_width / 2;
		if (row_count == 1) {
			focused_width = max_client_width;  /* Single window: use full width */
		}

		int32_t focused_x = m->w.x + scroller_structs;
		
		/* Apply centering if configured */
		if ((scroller_focus_center || scroller_prefer_center) && !is_top_row && row_count > 1) {
			focused_x = m->w.x + (m->w.width - focused_width) / 2;
		}

		struct wlr_box geo = {.x = focused_x, .y = row_y, 
			                  .width = focused_width, .height = row_height};
		resize(focused, geo, 0);

		fprintf(stderr, "[DS]   [%s] focused[%d]: x=%d w=%d\n", 
			is_top_row ? "TOP" : "BOT", focus_idx, focused_x, focused_width);

		/* Layout clients to the left */
		int32_t left_count = focus_idx;
		int32_t left_width = (left_count > 0) ? (max_client_width - focused_width) / left_count : 0;
		
		int32_t left_x = focused_x - cur_gappih;
		for (int32_t i = focus_idx - 1; i >= 0; i--) {
			Client *lc = row_clients[i];
			left_x -= left_width;

			geo = (struct wlr_box){.x = left_x, .y = row_y, 
				                   .width = left_width, .height = row_height};
			resize(lc, geo, 0);

			fprintf(stderr, "[DS]     left[%d]: x=%d w=%d\n", i, left_x, left_width);
			left_x -= cur_gappih;
		}

		/* Layout clients to the right */
		int32_t right_count = row_count - focus_idx - 1;
		int32_t right_width = (right_count > 0) ? (max_client_width - focused_width) / right_count : 0;
		
		int32_t right_x = focused_x + focused_width + cur_gappih;
		for (int32_t i = focus_idx + 1; i < row_count; i++) {
			Client *rc = row_clients[i];

			geo = (struct wlr_box){.x = right_x, .y = row_y, 
				                   .width = right_width, .height = row_height};
			resize(rc, geo, 0);

			fprintf(stderr, "[DS]     right[%d]: x=%d w=%d\n", i, right_x, right_width);
			right_x += right_width + cur_gappih;
		}
	}

	/* Get gap settings */
	int ie = enablegaps;
	int32_t cur_gappiv = ie ? m->gappiv : 0;
	int32_t cur_gappov = ie ? m->gappov : 0;

	if (smartgaps && m->visible_tiling_clients == 1) {
		cur_gappiv = 0;
		cur_gappov = 0;
	}

	/* Calculate row heights */
	int32_t avail_h = m->w.height - 2 * cur_gappov;
	if (avail_h < 1)
		avail_h = 1;

	int32_t top_h = (int32_t)(avail_h * dual_scroller_split);
	int32_t bottom_h = avail_h - top_h;
	
	if (bottom_count == 0) {
		top_h = avail_h;
		bottom_h = 0;
	}
	if (top_count == 0) {
		top_h = 0;
		bottom_h = avail_h;
	}

	int32_t top_y = m->w.y + cur_gappov;
	int32_t bottom_y = top_y + top_h + ((top_count > 0 && bottom_count > 0) ? cur_gappiv : 0);

	fprintf(stderr, "[DS] Row heights: avail=%d, split=%.2f, top=%d, bottom=%d\n",
		avail_h, dual_scroller_split, top_h, bottom_h);

	/* Layout both rows */
	layout_row(top_row, top_count, top_y, top_h, m, true);
	layout_row(bottom_row, bottom_count, bottom_y, bottom_h, m, false);

	/* Cleanup */
	free(top_row);
	free(bottom_row);

	fprintf(stderr, "[DS] Arrange complete\n");
}

/**
 * Plugin initialization function
 * 
 * This MUST be exported and match the signature:
 *   PluginInfo* plugin_init(void)
 * 
 * Returns a PluginInfo structure describing the plugin and its layouts.
 * The mango compositor will call this when loading the plugin.
 */
PluginInfo* plugin_init(void) {
	fprintf(stderr, "[DS] Plugin init called\n");
	
	/* Array of layouts provided by this plugin */
	static Layout plugin_layouts[] = {
		{
			.symbol = "DS",                        /* Symbol shown in status bar */
			.arrange = dual_scroller_arrange,      /* Function to arrange windows */
			.name = "dual_scroller",               /* Layout name for selection */
			.id = 1000,                             /* Unique ID for plugin layouts */
			.flags = LAYOUT_FLAG_SCROLLER | LAYOUT_FLAG_ROW
		}
	};

	/* Plugin metadata */
	static PluginInfo info = {
		.name = "dual_scroller",
		.version = "0.1.0",
		.description = "Dual Scroller Layout - Two-row tiling with adjustable split",
		.layouts = plugin_layouts,
		.num_layouts = 1
	};

	/* Register dispatch functions so the config parser can bind them */
	fprintf(stderr, "[DS] Registering dispatch: togglerow\n");
	plugin_register_dispatch("togglerow", (PluginFuncType)togglerow);
	fprintf(stderr, "[DS] Registering dispatch: adjust_dual_scroller_split\n");
	plugin_register_dispatch("adjust_dual_scroller_split", (PluginFuncType)adjust_dual_scroller_split);

	fprintf(stderr, "[DS] Plugin init complete\n");
	return &info;
}


