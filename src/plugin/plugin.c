/*
 * Plugin manager for Mango Wayland Compositor
 * Handles dynamic loading of plugins from the plugins/ directory
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "plugin.h"

/* Maximum number of plugins that can be loaded */
#define MAX_PLUGINS 32

/* Plugin load information */
typedef struct {
	void *handle;			  /* dlopen handle */
	PluginInfo *info;		  /* Plugin info structure */
	char path[256];			  /* Path to the .so file */
} LoadedPlugin;

/**
 * Find the plugins directory by checking multiple standard locations
 * Returns the first directory that exists, or NULL if none found
 */
static char* find_plugins_dir(void) {
	static char plugin_path[512];
	const char *home = getenv("HOME");
	
	/* Try paths in order of preference */
	const char *search_paths[] = {
		"./plugins",                          /* Current directory (for development) */
		"../plugins",                         /* Parent directory (for running from build/) */
		"plugins",                            /* Relative to cwd */
		NULL  /* Sentinel - will add expandable paths below */
	};
	
	/* Check simple paths first */
	for (int i = 0; search_paths[i] != NULL; i++) {
		if (access(search_paths[i], F_OK) == 0) {
			return (char*)search_paths[i];
		}
	}
	
	/* Try user config directory */
	if (home) {
		snprintf(plugin_path, sizeof(plugin_path), "%s/.config/mango/plugins", home);
		if (access(plugin_path, F_OK) == 0)
			return plugin_path;
	}
	
	/* Try system directories */
	if (access("/usr/local/share/mango/plugins", F_OK) == 0)
		return "/usr/local/share/mango/plugins";
	
	if (access("/usr/share/mango/plugins", F_OK) == 0)
		return "/usr/share/mango/plugins";
	
	return NULL;
}

/* Global plugin registry */
static LoadedPlugin loaded_plugins[MAX_PLUGINS];
static int32_t num_loaded_plugins = 0;

/**
 * Load a single plugin from a .so file
 * Returns 0 on success, -1 on failure
 */
static int32_t plugin_load_file(const char *path) {
	void *handle;
	PluginInfo *(*plugin_init)(void);
	PluginInfo *info;

	if (num_loaded_plugins >= MAX_PLUGINS) {
		fprintf(stderr, "Plugin limit (%d) reached\n", MAX_PLUGINS);
		return -1;
	}

	/* Clear any existing errors */
	dlerror();

	/* Open the shared object. Use RTLD_GLOBAL so plugin symbols can
	 * resolve references to symbols exported by the main compositor
	 * process (e.g., layout functions like `tile`). */
	handle = dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
	if (!handle) {
		const char *err = dlerror();
		fprintf(stderr, "Failed to load plugin %s: %s\n", path, err ? err : "unknown error");
		return -1;
	}

	/* Get the plugin_init function */
	dlerror();
	plugin_init = (PluginInfo *(*)(void))dlsym(handle, "plugin_init");
	if (!plugin_init) {
		const char *err = dlerror();
		fprintf(stderr, "Plugin %s missing plugin_init() function: %s\n", path, err ? err : "unknown error");
		dlclose(handle);
		return -1;
	}

	/* Call the plugin initialization function */
	info = plugin_init();
	if (!info) {
		fprintf(stderr, "Plugin %s plugin_init() returned NULL\n", path);
		dlclose(handle);
		return -1;
	}

	/* Store the loaded plugin */
	loaded_plugins[num_loaded_plugins].handle = handle;
	loaded_plugins[num_loaded_plugins].info = info;
	strncpy(loaded_plugins[num_loaded_plugins].path, path, sizeof(loaded_plugins[0].path) - 1);
	loaded_plugins[num_loaded_plugins].path[sizeof(loaded_plugins[0].path) - 1] = '\0';

	fprintf(stderr, "Loaded plugin: %s (v%s) - %s\n", info->name, info->version, info->description);
	if (info->num_layouts > 0) {
		fprintf(stderr, "  Registered %d layout(s)\n", info->num_layouts);
	}

	num_loaded_plugins++;
	return 0;
}

/**
 * Load all plugins from the plugins directory
 * Automatically finds the plugins directory
 * Returns the number of successfully loaded plugins
 */
int32_t plugin_load_all(const char *plugin_dir) {
	DIR *dir;
	struct dirent *entry;
	char path[512];
	int32_t count = 0;
	const char *actual_dir = plugin_dir;
	char auto_dir[512];

	/* If no directory provided, try to find it automatically */
	if (!plugin_dir || plugin_dir[0] == '\0') {
		char *found = find_plugins_dir();
		if (!found) {
			fprintf(stderr, "No plugin directory found. Checked: ./plugins ../plugins and standard locations\n");
			return 0;
		}
		if (strlen(found) >= sizeof(auto_dir)) {
			fprintf(stderr, "Plugin directory path too long\n");
			return 0;
		}
		strcpy(auto_dir, found);
		actual_dir = auto_dir;
		fprintf(stderr, "Found plugin directory: %s\n", actual_dir);
	}

	/* Check if directory exists */
	if (access(actual_dir, F_OK) == -1) {
		fprintf(stderr, "Plugin directory does not exist: %s\n", actual_dir);
		return 0;
	}

	dir = opendir(actual_dir);
	if (!dir) {
		fprintf(stderr, "Failed to open plugin directory: %s\n", actual_dir);
		return 0;
	}

	fprintf(stderr, "Loading plugins from %s\n", actual_dir);

	while ((entry = readdir(dir)) != NULL) {
		/* Skip . and .. */
		if (entry->d_name[0] == '.')
			continue;

		/* Only load .so files */
		const char *ext = strrchr(entry->d_name, '.');
		if (!ext || strcmp(ext, ".so") != 0)
			continue;

		/* Build full path safely and skip if it would overflow */
		int ret = snprintf(path, sizeof(path), "%s/%s", actual_dir, entry->d_name);
		if (ret < 0 || ret >= (int)sizeof(path)) {
			fprintf(stderr, "Plugin path too long, skipping: %s/%s\n", actual_dir, entry->d_name);
			continue;
		}

		/* Try to load the plugin */
		if (plugin_load_file(path) == 0)
			count++;
	}

	closedir(dir);

	if (count == 0) {
		fprintf(stderr, "No plugins were loaded from %s\n", actual_dir);
	} else {
		fprintf(stderr, "Successfully loaded %d plugin(s)\n", count);
	}

	return count;
}

/**
 * Get all loaded plugins
 */
const LoadedPlugin *plugin_get_loaded(int32_t *out_count) {
	if (out_count)
		*out_count = num_loaded_plugins;
	return loaded_plugins;
}

/**
 * Get the total number of loaded plugins
 */
int32_t plugin_get_count(void) {
	return num_loaded_plugins;
}

/**
 * Get a specific loaded plugin by index
 */
const PluginInfo *plugin_get_info(int32_t index) {
	if (index < 0 || index >= num_loaded_plugins)
		return NULL;
	return loaded_plugins[index].info;
}

/**
 * Count total layouts provided by all loaded plugins
 */
int32_t plugin_get_total_layouts(void) {
	int32_t total = 0;
	for (int32_t i = 0; i < num_loaded_plugins; i++) {
		if (loaded_plugins[i].info)
			total += loaded_plugins[i].info->num_layouts;
	}
	return total;
}

/**
 * Unload all plugins (cleanup on compositor shutdown)
 */
void plugin_unload_all(void) {
	for (int32_t i = 0; i < num_loaded_plugins; i++) {
		if (loaded_plugins[i].handle) {
			dlclose(loaded_plugins[i].handle);
			loaded_plugins[i].handle = NULL;
		}
	}
	num_loaded_plugins = 0;
}
