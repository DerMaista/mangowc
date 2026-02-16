/*
 * PLUGIN TEMPLATE
 * ===============
 * This file demonstrates the structure and required elements for creating
 * a window layout plugin for the mango compositor.
 *
 * To use this template:
 * 1. Copy this file and rename it to your_plugin_name.c
 * 2. Replace all occurrences of "example" with your plugin name
 * 3. Implement your layout algorithm in the arrange function
 * 4. Add/modify dispatch functions as needed
 * 5. Update the plugin metadata in plugin_init()
 *
 * Key components:
 * - Includes: plugin API and compositor types
 * - Type definitions: Layout struct and custom data structures
 * - External symbols: access compositor state (clients, globals)
 * - Configuration: plugin-specific settings
 * - Dispatch functions: entry points for keybindings/commands
 * - Arrange function: the main layout logic (called for each monitor)
 * - plugin_init(): initialization function (REQUIRED, must be exported)
 */

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

/* ============================================================================
 * TYPE DEFINITIONS
 * ============================================================================
 */

/**
 * Layout structure - describes a spatial layout arrangement.
 * Each layout provided by your plugin must have:
 *   - symbol: Single character/short string for status bar display
 *   - arrange: Function pointer to your arrangement algorithm
 *   - name: Human-readable layout identifier
 *   - id: Unique ID for this layout (should be >= 1000 for plugins)
 */
typedef struct Layout {
	const char *symbol;
	void (*arrange)(Monitor *);
	const char *name;
	uint32_t id;
    uint32_t flags;
} Layout;

/* Forward declarations from the compositor (available at runtime via plugin loading) */
typedef struct Client Client;

/**
 * Argument type passed to dispatch functions.
 * Dispatch functions receive configuration arguments through this union.
 * Fill only the fields your function needs.
 */
typedef struct {
	int i;           /* Integer argument */
	int i2;          /* Second integer */
	float f;         /* Float argument */
	float f2;        /* Second float */
	uint32_t ui;     /* Unsigned integer */
	uint32_t ui2;    /* Second unsigned integer */
	void *v;         /* Void pointer (for custom data) */
	void *v2;        /* Second void pointer */
	void *v3;        /* Third void pointer */
} Arg;

/* ============================================================================
 * PLUGIN CONFIGURATION
 * ============================================================================
 * 
 * TODO: Define your plugin-specific configuration variables here.
 * These can be modified by dispatch functions or from configuration files.
 */

/* Example: configuration for split ratio */
static float dual_scroller_default_split_ratio = 0.3f;

/* TODO: Add more configuration variables as needed */

/* ============================================================================
 * EXTERNAL SYMBOLS - Provided by compositor at plugin load
 * ============================================================================
 * 
 * The compositor makes these symbols available via dynamic linking.
 * You can use them directly in your code - they're resolved at plugin load time.
 *
 * Common external symbols:
 *   extern struct wl_list clients;     - Global list of all clients
 *   extern int enablegaps;             - Whether gaps are enabled
 *   extern int smartgaps;              - Smart gaps mode
 *   extern void resize(...);           - Function to resize a client
 *
 * Check the compositor source or documentation for additional available symbols.
 */

extern struct wl_list clients;  /* Global client list */
extern int enablegaps;
extern int smartgaps;
extern void resize(Client *c, struct wlr_box geo, int interact);
extern int scroller_prefer_center;
extern int scroller_focus_center;
extern int32_t scroller_structs;
extern bool is_row_layout(Monitor *m);
extern bool start_drag_window(Client *c, int edge);

/* ============================================================================
 * USEFUL MACROS
 * ============================================================================
 * 
 * These macros help check client and visibility properties.
 */

/**
 * VISIBLEON: Check if client is visible on this monitor
 * Returns true if the client is on the monitor and has visible tags
 */
#define VISIBLEON(c, m) (((c)->mon == (m)) && ((c)->tags & (m)->tagset[(m)->seltags]))

/**
 * ISTILED: Check if client should be tiled
 * Returns true if the client should participate in tiling layout
 * (not floating, fullscreen, minimized, etc.)
 */
#define ISTILED(c) ((c) && !(c)->isfloating && !(c)->isminimized && !(c)->iskilling && !(c)->ismaximizescreen && !(c)->isfullscreen && !(c)->isunglobal)

/* ============================================================================
 * DISPATCH FUNCTIONS - Entry points for keybindings and commands
 * ============================================================================
 * 
 * Dispatch functions are called when the user triggers a keybinding or command
 * that is bound to your plugin in the configuration.
 * 
 * Function signature:
 *   static int32_t function_name(const Arg *arg)
 * 
 * Parameters:
 *   @arg: Configuration arguments passed from config or keybinding
 *         Returns 0 on success, non-zero on error
 * 
 * Registration in plugin_init():
 *   plugin_register_dispatch("name", (PluginFuncType)function_name);
 * 
 * Usage via mmsg:
 *   mmsg -o <output> -s -d "dispatch_name"
 *   Example: mmsg -o WL-1 -s -d "adjust_split"
 * 
 * Usage in config (keybinding example):
 *   { MODKEY, XK_d, <action_function>, {"adjust_split"} },
 */

/**
 * Example dispatch: Adjust configuration value
 * 
 * Demonstrates how to handle float arguments and update plugin state.
 * 
 * @arg Configuration arguments (arg->f contains the delta)
 * 
 * Return: 0 on success, -1 on error
 */
static int32_t example_adjust_split(const Arg *arg) {
	if (!arg) {
		fprintf(stderr, "[EXAMPLE] Error: NULL argument\n");
		return -1;
	}
	
	/* Read the float delta from the argument */
	float delta = arg->f;
	fprintf(stderr, "[EXAMPLE] Adjusting split by %.3f\n", delta);
	
	/* Update the configuration */
	example_config_split += delta;
	
	/* Clamp to valid range [0.1, 0.9] */
	if (example_config_split < 0.1f)
		example_config_split = 0.1f;
	if (example_config_split > 0.9f)
		example_config_split = 0.9f;
	
	fprintf(stderr, "[EXAMPLE] New split ratio: %.2f\n", example_config_split);
	
	/* Note: To trigger a re-layout after changing configuration,
	 * you may need to signal the compositor to refresh layouts.
	 * This depends on what APIs are exposed to plugins.
	 */
	
	return 0;
}

/**
 * Example dispatch: Perform an action
 * 
 * Demonstrates how to iterate clients and perform operations.
 * 
 * @arg Configuration arguments (optional)
 * 
 * Return: 0 on success
 */
static int32_t togglerow(const Arg *arg) {
	if (!selmon || !selmon->sel || !is_row_layout(selmon))
		return 0;

	Client *c = selmon->sel;

	// Only toggle for tiled windows
	if (c->isfloating || !ISSCROLLTILED(c) || !VISIBLEON(c, selmon))
		return 0;

	// Toggle the row (0 <-> 1)
	if (c->dual_scroller_row == 0) {
		c->dual_scroller_row = 1;
	} else {
		c->dual_scroller_row = 0;
	}

	// Trigger a relayout
	arrange(selmon, false, false);
	return 0;
}

static int32_t adjust_dual_scroller_split(const Arg *arg) {
	float new_ratio;

	if (!arg || !selmon)
		return 0;

	// Check if we're in a dual-scroller layout
	if (!is_row_layout(selmon))
		return 0;

	// Calculate new ratio: if arg->f < 1.0, treat as relative adjustment, otherwise as absolute value
	new_ratio = arg->f < 1.0 ? dual_scroller_default_split_ratio + arg->f : arg->f - 1.0;

	// Clamp the ratio between 0.1 and 0.9
	if (new_ratio < 0.1 || new_ratio > 0.9)
		return 0;

	dual_scroller_default_split_ratio = new_ratio;
	arrange(selmon, false, false);
	return 0;
}



/* ============================================================================
 * MAIN LAYOUT ARRANGEMENT FUNCTION
 * ============================================================================
*/


// Dual-row scroller layout with independent scrolling
// Top row: 30% of screen height, Bottom row: 70% of screen height
void dual_scroller(Monitor *m) {
	unsigned int i, n_top = 0, n_bottom = 0, n_total = 0;

	Client *c = NULL;
	Client **top_row_clients = NULL;
	Client **bottom_row_clients = NULL;
	struct wlr_box target_geom;

	unsigned int cur_gappih = enablegaps ? m->gappih : 0;
	unsigned int cur_gappoh = enablegaps ? m->gappoh : 0;
	unsigned int cur_gappov = enablegaps ? m->gappov : 0;
	unsigned int cur_gappiv = enablegaps ? m->gappiv : 0;

	cur_gappih =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappih;
	cur_gappoh =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappoh;
	cur_gappov =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappov;
	cur_gappiv =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappiv;

	unsigned int max_client_width =
		m->w.width - 2 * scroller_structs - cur_gappih;

	n_total = m->visible_scroll_tiling_clients;

	if (n_total == 0) {
		return;
	}

	// First pass: count clients per row and assign unassigned clients
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) && ISSCROLLTILED(c)) {
			// Assign to bottom row by default if not assigned
			if (c->dual_scroller_row == -1) {
				c->dual_scroller_row = 1; // Default to bottom row
			}

			if (c->dual_scroller_row == 0) {
				n_top++;
			} else {
				n_bottom++;
			}
		}
	}

	// Allocate arrays for each row
	if (n_top > 0) {
		top_row_clients = malloc(n_top * sizeof(Client *));
		if (!top_row_clients) {
			return;
		}
	}

	if (n_bottom > 0) {
		bottom_row_clients = malloc(n_bottom * sizeof(Client *));
		if (!bottom_row_clients) {
			free(top_row_clients);
			return;
		}
	}

	// Fill row arrays
	unsigned int top_idx = 0, bottom_idx = 0;
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) && ISSCROLLTILED(c)) {
			if (c->dual_scroller_row == 0) {
				top_row_clients[top_idx++] = c;
			} else {
				bottom_row_clients[bottom_idx++] = c;
			}
		}
	}

	// Calculate row heights using configurable split ratio
	unsigned int top_row_height = (unsigned int)((m->w.height - 2 * cur_gappov - cur_gappiv) * dual_scroller_default_split_ratio);
	unsigned int bottom_row_height = m->w.height - 2 * cur_gappov - cur_gappiv - top_row_height;
	unsigned int top_row_y = m->w.y + cur_gappov;
	unsigned int bottom_row_y = top_row_y + top_row_height + cur_gappiv;

	// Helper function to layout a single row
	void layout_row(Client **row_clients, unsigned int n_row, unsigned int row_y,
	                unsigned int row_height, bool is_top_row) {
		if (n_row == 0) return;

		Client *root_client = NULL;
		int focus_index = -1;
		bool need_scroller = false;

		// Find focused client in this row
		for (i = 0; i < n_row; i++) {
			if (row_clients[i] == m->sel) {
				root_client = row_clients[i];
				focus_index = i;
				break;
			}
		}

		// If no focused client in this row, keep current scroll position
		if (!root_client && n_row > 0) {
			return;
		}

		// Check if scrolling is needed
		if (root_client && !root_client->is_pending_open_animation &&
			root_client->geom.x >= m->w.x + scroller_structs &&
			root_client->geom.x + root_client->geom.width <=
				m->w.x + m->w.width - scroller_structs) {
			need_scroller = false;
		} else {
			need_scroller = true;
		}

		if (start_drag_window)
			need_scroller = false;

		// Layout focused client
		if (focus_index >= 0 && root_client) {
			target_geom.height = row_height;
			target_geom.width = max_client_width * root_client->scroller_proportion;
			target_geom.y = row_y;

			// Handle fullscreen and maximize
			if (root_client->isfullscreen) {
				target_geom.height = m->m.height;
				target_geom.width = m->m.width;
				target_geom.y = m->m.y;
				target_geom.x = m->m.x;
				resize(root_client, target_geom, 0);
			} else if (root_client->ismaximizescreen) {
				target_geom.height = m->w.height - 2 * cur_gappov;
				target_geom.width = m->w.width - 2 * cur_gappoh;
				target_geom.y = m->w.y + cur_gappov;
				target_geom.x = m->w.x + cur_gappoh;
				resize(root_client, target_geom, 0);
			} else if (need_scroller) {
				// Determine if we should center
				bool should_center = (scroller_focus_center ||
					((!m->prevsel ||
					  (ISSCROLLTILED(m->prevsel) &&
					   (m->prevsel->scroller_proportion * max_client_width) +
							   (root_client->scroller_proportion * max_client_width) >
						   m->w.width - 2 * scroller_structs - cur_gappih)) &&
					scroller_prefer_center));

				// Top row: never center
				if (is_top_row) {
					should_center = false;
				}

				if (should_center) {
					target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
				} else {
					target_geom.x = root_client->geom.x > m->w.x + (m->w.width) / 2
										? m->w.x + (m->w.width -
													root_client->scroller_proportion *
														max_client_width -
													scroller_structs)
										: m->w.x + scroller_structs;
				}
				resize(root_client, target_geom, 0);
			} else {
				target_geom.x = root_client->geom.x;
				resize(root_client, target_geom, 0);
			}
		}

		// Layout clients to the left of focused
		for (i = focus_index - 1; i >= 0 && i < n_row; i--) {
			c = row_clients[i];
			target_geom.width = max_client_width * c->scroller_proportion;
			target_geom.height = row_height;
			target_geom.y = row_y;

			if (!c->isfullscreen && !c->ismaximizescreen) {
				target_geom.x = row_clients[i + 1]->geom.x - cur_gappih - target_geom.width;
				resize(c, target_geom, 0);
			}
		}

		// Layout clients to the right of focused
		for (i = focus_index + 1; i < n_row; i++) {
			c = row_clients[i];
			target_geom.width = max_client_width * c->scroller_proportion;
			target_geom.height = row_height;
			target_geom.y = row_y;

			if (!c->isfullscreen && !c->ismaximizescreen) {
				target_geom.x = row_clients[i - 1]->geom.x + cur_gappih + row_clients[i - 1]->geom.width;
				resize(c, target_geom, 0);
			}
		}
	}

	// Layout both rows independently
	layout_row(top_row_clients, n_top, top_row_y, top_row_height, true);
	layout_row(bottom_row_clients, n_bottom, bottom_row_y, bottom_row_height, false);

	// Cleanup
	free(top_row_clients);
	free(bottom_row_clients);
}

/* ============================================================================
 * PLUGIN INITIALIZATION - REQUIRED ENTRY POINT
 * ============================================================================
 * 
 * CRITICAL: Your plugin MUST export a function with this exact signature:
 * 
 *   PluginInfo* plugin_init(void)
 * 
 * This function is called by the compositor when the plugin is loaded.
 * It must return a PluginInfo structure describing your plugin's capabilities.
 * 
 * Return value:
 *   Must return a pointer to a PluginInfo structure. This structure should be
 *   static to ensure it persists after the function returns.
 * 
 * PluginInfo structure:
 *   .name         - Plugin name (e.g., "example_plugin")
 *   .version      - Version string (e.g., "0.1.0")
 *   .description  - Short description of what your plugin does
 *   .layouts      - Pointer to array of Layout structures
 *   .num_layouts  - Number of layouts in the array
 * 
 * Layout structure (for each layout you provide):
 *   .symbol       - 1-2 character symbol shown in status bar
 *   .arrange      - Function pointer to your arrange function
 *   .name         - Layout name (used in configuration, e.g., "my_layout")
 *   .id           - Unique ID (1000+ for plugins to avoid conflicts)
 * 
 * Dispatch registration:
 *   plugin_register_dispatch(name, function_pointer);
 *   - name: String identifier for the dispatch (e.g., "my_action")
 *   - function_pointer: Cast your function to (PluginFuncType)
 * 
 * Usage via mmsg (after registration):
 *   mmsg -o <output> -s -d "name"
 *   The registered name is looked up globally across all plugins.
 * 
 * Usage in config bindings (after registration):
 *   You can bind the dispatch to a key using the appropriateaction function.
 */

PluginInfo* plugin_init(void) {
	fprintf(stderr, "[EXAMPLE] Plugin initialization started\n");
	
	/*
	 * TODO: Define the layouts this plugin provides
	 * 
	 * Each layout has:
	 *   - symbol: Short identifier shown in status bar
	 *   - arrange: Your layout function
	 *   - name: Configuration name
	 *   - id: Unique ID (1000+ for plugins)
	 * 
	 * If you have multiple layouts, add more entries to this array.
	 */
	static Layout plugin_layouts[] = {
		{
			.symbol = "EX",                  /* Layout symbol */
			.arrange = dual_scroller,      /* Your arrange function */
			.name = "example",               /* Configuration identifier */
			.id = 1000,                      /* Unique layout ID */
			.flags = LAYOUT_FLAG_NONE
		}
		/*
		// Example of adding a second layout:
		// .
		// {
		//     .symbol = "E2",
		//     .arrange = example_arrange_alternative,
		//     .name = "example_alt",
		//     .id = 1001
		// },
		*/
	};

	/*
	 * Create and populate the plugin info structure.
	 * This is returned to the compositor and should be static.
	 */
	static PluginInfo info = {
		.name = "example_plugin",
		.version = "0.1.0",
		.description = "Example plugin template - customize this description",
		.layouts = plugin_layouts,
		.num_layouts = 1  /* Update if you add more layouts */
	};

	/*
	 * Register dispatch functions.
	 * These make custom commands available globally via mmsg and keybindings.
	 * 
	 * After registration, users can call them via mmsg:
	 *   mmsg -o <output> -s -d "adjust_split"
	 * Or bind them to keys in config.conf (exact syntax depends on your config format).
	 * 
	 * TODO: Customize dispatch names and functions for your plugin
	 */

	fprintf(stderr, "[EXAMPLE] Registering dispatch: adjust_split\n");
	plugin_register_dispatch("adjust_split", (PluginFuncType)example_adjust_split);

	fprintf(stderr, "[EXAMPLE] Registering dispatch: action\n");
	plugin_register_dispatch("togglerow", (PluginFuncType)togglerow);
	plugin_register_dispatch("adjust_dual_scroller_split", (PluginFuncType)adjust_dual_scroller_split);

	fprintf(stderr, "[EXAMPLE] Plugin initialization complete\n");
	return &info;
}
