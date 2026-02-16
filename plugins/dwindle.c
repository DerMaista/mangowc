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

typedef struct DwindleNode {
	struct wlr_box box;
	Client *client;                     // NULL if internal node
	struct DwindleNode *children[2];    // NULL for leaf nodes
	bool split_horizontal;              // true = left/right, false = top/bottom
} DwindleNode;

/* ============================================================================
 * PLUGIN CONFIGURATION
 * ============================================================================
 * 
 * TODO: Define your plugin-specific configuration variables here.
 * These can be modified by dispatch functions or from configuration files.
 */

/* Example: configuration for split ratio */
static float example_config_split = 0.5f;  /* Default: 50% split */

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
static int32_t example_action(const Arg *arg) {
	fprintf(stderr, "[EXAMPLE] Action dispatch called\n");
	
	/* TODO: Implement your custom action here
	 * 
	 * Example: count clients on focused monitor
	 * {
	 *     int count = 0;
	 *     Client *c;
	 *     wl_list_for_each(c, &clients, link) {
	 *         if (ISTILED(c))
	 *             count++;
	 *     }
	 *     fprintf(stderr, "[EXAMPLE] Tiled clients: %d\n", count);
	 * }
	 */
	
	return 0;
}

/* ============================================================================
 * MAIN LAYOUT ARRANGEMENT FUNCTION
 * ============================================================================
 */

// Dwindle layout
// Each new window splits the space in half, alternating between
// horizontal and vertical splits based on aspect ratio

static void dwindle(Monitor *m) {
	int32_t i, n = 0;
	Client *c = NULL;
	Client **tempClients = NULL;
	DwindleNode *nodes = NULL;
	int32_t nodeCount = 0;

	
	int32_t cur_gappiv = enablegaps ? m->gappiv : 0;
	int32_t cur_gappih = enablegaps ? m->gappih : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;

	
	cur_gappiv = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappiv;
	cur_gappih = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappih;
	cur_gappov = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappov;
	cur_gappoh = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappoh;

	const float split_width_multiplier = 1.0f;

	n = m->visible_tiling_clients;

	if (n == 0)
		return;

	if (n == 1) {
		wl_list_for_each(c, &clients, link) {
			if (!VISIBLEON(c, m) || !ISTILED(c))
				continue;

			resize(c,
				   (struct wlr_box){.x = m->w.x + cur_gappoh,
									.y = m->w.y + cur_gappov,
									.width = m->w.width - 2 * cur_gappoh,
									.height = m->w.height - 2 * cur_gappov},
				   0);
			return;
		}
	}

	tempClients = malloc(n * sizeof(Client *));
	if (!tempClients)
		return;

	i = 0;
	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || !ISTILED(c))
			continue;
		tempClients[i++] = c;
	}

	nodes = calloc(2 * n - 1, sizeof(DwindleNode));
	if (!nodes) {
		free(tempClients);
		return;
	}

	nodes[0].box.x = m->w.x + cur_gappoh;
	nodes[0].box.y = m->w.y + cur_gappov;
	nodes[0].box.width = m->w.width - 2 * cur_gappoh;
	nodes[0].box.height = m->w.height - 2 * cur_gappov;
	nodes[0].client = tempClients[0];
	nodes[0].children[0] = NULL;
	nodes[0].children[1] = NULL;
	nodeCount = 1;

	for (i = 1; i < n; i++) {
		DwindleNode *leafToSplit = NULL;
		for (int32_t j = nodeCount - 1; j >= 0; j--) {
			if (nodes[j].client != NULL) {
				leafToSplit = &nodes[j];
				break;
			}
		}

		if (!leafToSplit)
			break;

		bool splitHorizontal =
			leafToSplit->box.width > leafToSplit->box.height * split_width_multiplier;

		DwindleNode *child0 = &nodes[nodeCount++];
		DwindleNode *child1 = &nodes[nodeCount++];

		if (splitHorizontal) {
			int32_t halfWidth = (leafToSplit->box.width - cur_gappih) / 2;

			child0->box.x = leafToSplit->box.x;
			child0->box.y = leafToSplit->box.y;
			child0->box.width = halfWidth;
			child0->box.height = leafToSplit->box.height;

			child1->box.x = leafToSplit->box.x + halfWidth + cur_gappih;
			child1->box.y = leafToSplit->box.y;
			child1->box.width = leafToSplit->box.width - halfWidth - cur_gappih;
			child1->box.height = leafToSplit->box.height;
		} else {
			int32_t halfHeight = (leafToSplit->box.height - cur_gappiv) / 2;

			child0->box.x = leafToSplit->box.x;
			child0->box.y = leafToSplit->box.y;
			child0->box.width = leafToSplit->box.width;
			child0->box.height = halfHeight;

			child1->box.x = leafToSplit->box.x;
			child1->box.y = leafToSplit->box.y + halfHeight + cur_gappiv;
			child1->box.width = leafToSplit->box.width;
			child1->box.height = leafToSplit->box.height - halfHeight - cur_gappiv;
		}

		child0->client = leafToSplit->client;
		child0->children[0] = NULL;
		child0->children[1] = NULL;

		child1->client = tempClients[i];
		child1->children[0] = NULL;
		child1->children[1] = NULL;

		leafToSplit->client = NULL;
		leafToSplit->children[0] = child0;
		leafToSplit->children[1] = child1;
		leafToSplit->split_horizontal = splitHorizontal;
	}

	for (i = 0; i < nodeCount; i++) {
		if (nodes[i].client != NULL) {
			resize(nodes[i].client, nodes[i].box, 0);
		}
	}

	free(nodes);
	free(tempClients);
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
			.symbol = "DW",                  /* Layout symbol */
			.arrange = dwindle,      /* Your arrange function */
			.name = "dwindle",               /* Configuration identifier */
			.id = 1200,                      /* Unique layout ID */
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
		.name = "dwindle_plugin",
		.version = "0.1.0",
		.description = "Dwindle layout plugin - a simple tiling layout",
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
	plugin_register_dispatch("action", (PluginFuncType)example_action);

	fprintf(stderr, "[EXAMPLE] Plugin initialization complete\n");
	return &info;
}
