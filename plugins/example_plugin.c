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
 * 
 * This is the core function that defines how you arrange windows on a monitor.
 * Called whenever the layout needs to be applied (windows added/removed,
 * configuration changes, etc.)
 * 
 * Function signature:
 *   static void layout_name_arrange(Monitor *m)
 * 
 * Parameters:
 *   @m: Pointer to the Monitor to arrange
 * 
 * Monitor structure provides:
 *   m->w                   - Monitor geometry (wlr_box with x, y, width, height)
 *   m->w.x, m->w.y         - Monitor position
 *   m->w.width, m->w.height - Monitor dimensions
 *   m->visible_tiling_clients - Count of visible tiled windows
 *   m->tagset[]            - Tag filtering information
 *   m->gappih, m->gappoh   - Horizontal gap sizes (inside, outside)
 *   m->gappiv, m->gappov   - Vertical gap sizes (inside, outside)
 *   (and other fields accessible via mango-types.h)
 * 
 * Client structure provides:
 *   c->mon                 - Monitor pointer (NULL if floating)
 *   c->tags                - Tag membership bitmask
 *   c->isfloating          - Whether window is floating
 *   c->isfullscreen        - Whether in fullscreen mode
 *   c->ismaximizescreen    - Maximized state
 *   c->isminimized         - Minimized state
 *   (and other fields accessible via mango-types.h)
 * 
 * Core functions available:
 *   resize(client, wlr_box_geo, interact_flag);
 *     - Resizes and positions a client to the given geometry
 *     - interact_flag: 0 for automatic resize, non-zero for interactive
 * 
 * Typical layout steps:
 * 1. Validate the monitor parameter
 * 2. Get the count of visible tiling clients on this monitor
 * 3. Handle the empty case (no clients to arrange)
 * 4. Allocate space to collect clients (if needed)
 * 5. Iterate clients and collect those on this monitor that are tiled
 * 6. Compute geometry based on your layout algorithm
 * 7. Call resize() to apply geometry to each client
 * 8. Handle gaps and smartgaps configuration
 */

static void example_arrange(Monitor *m) {
	if (!m) {
		fprintf(stderr, "[EXAMPLE] Error: arrange called with NULL monitor\n");
		return;
	}

	/* Get the number of visible tiling clients on this monitor */
	int32_t n = m->visible_tiling_clients;
	fprintf(stderr, "[EXAMPLE] Arranging %d clients on monitor\n", n);
	
	/* Handle empty case early */
	if (n == 0) {
		fprintf(stderr, "[EXAMPLE] No visible tiling clients\n");
		return;
	}

	/**
	 * TODO: Implement your layout algorithm here
	 * 
	 * STEP 1: Allocate arrays to collect clients (if needed)
	 * ------
	 * Client **clients_list = malloc(n * sizeof(Client*));
	 * if (!clients_list) return;  // Handle allocation failure
	 * 
	 * 
	 * STEP 2: Collect visible tiled clients on this monitor
	 * ------
	 * int32_t count = 0;
	 * Client *c;
	 * wl_list_for_each(c, &clients, link) {
	 *     // Skip clients on other monitors
	 *     if (c->mon != m)
	 *         continue;
	 *     // Skip non-tiled windows
	 *     if (!ISTILED(c))
	 *         continue;
	 *     // Skip hidden/untagged windows
	 *     if (!VISIBLEON(c, m))
	 *         continue;
	 *     // c is a valid tiled, visible client on this monitor
	 *     clients_list[count++] = c;
	 * }
	 * 
	 * 
	 * STEP 3: Get gap configuration
	 * ------
	 * int32_t gappx = enablegaps ? m->gappih : 0;  // Horizontal gap
	 * int32_t gappy = enablegaps ? m->gappiv : 0;  // Vertical gap
	 * if (smartgaps && n == 1) {
	 *     gappx = gappy = 0;  // No gaps for single window
	 * }
	 * 
	 * 
	 * STEP 4: Calculate window geometries
	 * ------
	 * Example for master-slave layout:
	 * {
	 *     int32_t mw = m->w.width / 2;  // Master width
	 *     int32_t x = m->w.x + gappx;
	 *     int32_t y = m->w.y + gappy;
	 *     int32_t h = m->w.height - 2*gappy;
	 *     int32_t w = mw - 2*gappx;
	 *     
	 *     // First client is the master
	 *     struct wlr_box geo = {.x = x, .y = y, .width = w, .height = h};
	 *     resize(clients_list[0], geo, 0);
	 *     
	 *     // Remaining clients in slave column
	 *     int32_t slave_w = (m->w.width - mw) / (n-1) - gappx;
	 *     int32_t slave_x = mw + gappx;
	 *     for (int i = 1; i < n; i++) {
	 *         geo = (struct wlr_box){
	 *             .x = slave_x,
	 *             .y = y + (i-1)*(h+gappy)/(n-1),
	 *             .width = slave_w,
	 *             .height = h/(n-1)
	 *         };
	 *         resize(clients_list[i], geo, 0);
	 *         slave_x += slave_w + gappx;
	 *     }
	 * }
	 * 
	 * 
	 * STEP 5: Cleanup and logging
	 * ------
	 * free(clients_list);
	 * fprintf(stderr, "[EXAMPLE] Arrange complete\n");
	 */

	/* MINIMAL EXAMPLE: Simple fullscreen layout
	 * Place all clients fullscreen on top of each other
	 * (Only the last one in the list is visible)
	 */

	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (c->mon != m)
			continue;

		/* Fullscreen geometry */
		struct wlr_box geo = {
			.x = m->w.x,
			.y = m->w.y,
			.width = m->w.width,
			.height = m->w.height
		};
		resize(c, geo, 0);
	}

	fprintf(stderr, "[EXAMPLE] Arrange complete\n");
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
			.arrange = example_arrange,      /* Your arrange function */
			.name = "example",               /* Configuration identifier */
			.id = 1000                       /* Unique layout ID */
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
	plugin_register_dispatch("action", (PluginFuncType)example_action);

	fprintf(stderr, "[EXAMPLE] Plugin initialization complete\n");
	return &info;
}
