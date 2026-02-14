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

/* LoadedPlugin struct - used internally but needs to be visible to compositor */
typedef struct {
	void *handle;			  /* dlopen handle */
	PluginInfo *info;		  /* Plugin info structure */
	char path[256];			  /* Path to the .so file */
} LoadedPlugin;

/* Minimal FuncType used by plugin dispatch registration.
 * Plugins may register functions matching the core signature:
 *   int32_t fn(const Arg *);
 * We use a void pointer here to avoid depending on parse_config.h in plugins.
 */
typedef int32_t (*PluginFuncType)(const void *);

/**
 * Register a dispatch function provided by a plugin. This is called by the
 * plugin during its `plugin_init()` to make custom dispatch names available
 * to the compositor's binding/parser.
 */
void plugin_register_dispatch(const char *name, PluginFuncType func);

/**
 * Lookup a dispatch registered by any plugin. Returns NULL if not found.
 */
PluginFuncType plugin_lookup_dispatch(const char *name);

/**
 * Load all plugins from a directory.
 * If plugin_dir is NULL or empty, auto-detects standard plugin directories.
 * Returns the number of successfully loaded plugins.
 */
int32_t plugin_load_all(const char *plugin_dir);

/**
 * Get all loaded plugins and their count.
 */
const LoadedPlugin *plugin_get_loaded(int32_t *out_count);

/**
 * Get total number of layouts provided by all loaded plugins.
 */
int32_t plugin_get_total_layouts(void);

/**
 * Unload all plugins (call at compositor shutdown).
 */
void plugin_unload_all(void);

/**
 * All plugins must export this function with this exact signature:
 * PluginInfo* plugin_init(void)
 * 
 * This function is called when the plugin is loaded and should return
 * a PluginInfo structure with the plugin's layouts and other extensions.
 * 
 * The returned PluginInfo should have a lifetime for the entire compositor session.
 */

#endif /* PLUGIN_H */
