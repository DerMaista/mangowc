/*
 * Dual Scroller Layout Plugin for Mango Wayland Compositor
 * 
 * This plugin implements a dual_scroller layout that splits the screen into
 * two rows with an adjustable split ratio. Windows are tiled in two rows,
 * with the top row using the configured split ratio and the bottom row
 * using the remaining space.
 *
 * Features:
 * - Two-row layout with configurable split ratio (default 0.5 = 50/50)
 * - Windows distributed evenly across rows
 * - Gaps between windows configurable
 *
 * Usage:
 *   In config.conf:
 *     bind=SUPER,s,setlayout,dual_scroller
 *
 * Compile with:
 *   gcc -I./src -shared -fPIC -o plugins/dual_scroller.so plugins/dual_scroller.c
 */

#include <stdint.h>
#include <string.h>

/* Include the plugin API definition */
#include "plugin/plugin.h"

/* Use compositor-provided helper to arrange a dual-scroller layout */
extern void plugin_arrange_dual_scroller(struct Monitor *m, float split, int32_t gap);

/* Define a local Layout struct compatible with the compositor's Layout
 * so this plugin can be compiled as a standalone shared object. This
 * must match the layout definition in the core (symbol, arrange, name, id).
 */
typedef struct Layout {
	const char *symbol;
	void (*arrange)(Monitor *);
	const char *name;
	uint32_t id;
} Layout;

/* Configuration for dual scroller */
static float dual_scroller_split = 0.5f;  /* Top row takes 50% */
static int32_t dual_scroller_gap = 10;    /* 10px gap between windows */

/**
 * Dual Scroller Arrange Function
 * 
 * This is a placeholder that demonstrates the plugin structure.
 * In production, it would:
 * 1. Count tiled clients visible on monitor
 * 2. Split them across two rows based on split_ratio
 * 3. Arrange windows within each row horizontally
 * 4. Handle window sizing and gaps
 *
 * Note: The actual implementation requires access to:
 * - VISIBLEON, ISTILED macros from mango
 * - Full Client and Monitor structure definitions
 * - Various utility functions from mango
 * 
 * For now, this returns early to avoid errors when linking
 * against incomplete structure definitions at compile time.
 */
static void dual_scroller_arrange(Monitor *m) {
	/* Delegate to compositor helper so plugin doesn't depend on internal symbols */
	if (!m)
		return;

	plugin_arrange_dual_scroller(m, dual_scroller_split, dual_scroller_gap);
	
	/* Full implementation would:
	 * 1. Get all visible tiled clients
	 * 2. Calculate top and bottom row heights based on split ratio
	 * 3. Distribute clients between rows
	 * 4. Position and resize each client in their row
	 * 
	 * This matches the scroller layout pattern but with two rows instead
	 */
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
	/* Array of layouts provided by this plugin */
	static Layout plugin_layouts[] = {
		{
			.symbol = "DS",                        /* Symbol shown in status bar */
			.arrange = dual_scroller_arrange,      /* Function to arrange windows */
			.name = "dual_scroller",               /* Layout name for selection */
			.id = 1000                             /* Unique ID for plugin layouts */
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

	/* Silence "defined but not used" warnings for config variables */
	(void)dual_scroller_gap;
	(void)dual_scroller_split;

	return &info;
}


