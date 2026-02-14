/*
 * Plugin system for Mango Wayland Compositor
 * This allows runtime loading of custom layouts and extensions without recompilation
 */

#ifndef PLUGIN_H
#define PLUGIN_H

#include <stdint.h>

/* Forward declarations - these are defined in mango.c */
typedef struct Monitor Monitor;
typedef struct Client Client;

/* Opaque type - Layout is defined in layout.h and will be included before this */
/* We only use it as a pointer here, so we don't need the full definition */
struct Layout;  /* Forward declaration, will be fully defined later */

/**
 * Plugin information structure
 * Plugins should export a plugin_init() function that returns this structure
 */
typedef struct {
	const char *name;				/* Plugin name */
	const char *version;			/* Plugin version */
	const char *description;		/* Plugin description */
	struct Layout *layouts;			/* Array of layout structures (opaque type) */
	int32_t num_layouts;			/* Number of layouts provided */
	/* Future extensions for dispatch functions, config options, etc. */
} PluginInfo;

/**
 * All plugins must export this function with this exact signature:
 * PluginInfo* plugin_init(void)
 * 
 * This function is called when the plugin is loaded and should return
 * a PluginInfo structure with the plugin's layouts and other extensions.
 * 
 * The returned PluginInfo should have a lifetime for the entire compositor session.
 */

/* Helper for plugins: arrange monitor in dual-scroller two-row layout.
 * Implemented in the compositor so plugins don't need internal headers.
 * `split` is fraction for top row (0..1), `gap` is preferred gap in pixels.
 */
void plugin_arrange_dual_scroller(struct Monitor *m, float split, int32_t gap);

#endif /* PLUGIN_H */
