/*
 * Mango Compositor Type Definitions for Plugins
 * 
 * This header provides the necessary type definitions for plugins to access
 * compositor structures. These types must remain ABI-compatible with mango.c.
 */

#ifndef MANGO_TYPES_H
#define MANGO_TYPES_H

#include <stdint.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/box.h>

/* Forward declarations */
typedef struct Client Client;
typedef struct Monitor Monitor;
typedef struct Layout Layout;

/* Client structure - must match the one in mango.c */
struct Client {
	/* Must keep these three elements in this order */
	uint32_t type;
	struct wlr_box geom, pending, float_geom, animainit_geom,
		overview_backup_geom, current, drag_begin_geom;
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border;
	struct wlr_scene_shadow *shadow;
	struct wlr_scene_tree *scene_surface;
	struct wl_list link;
	struct wl_list flink;
	struct wl_list fadeout_link;
	union {
		struct wlr_xdg_surface *xdg;
		struct wlr_xwayland_surface *xwayland;
	} surface;
	struct wl_listener commit;
	struct wl_listener map;
	struct wl_listener maximize;
	struct wl_listener minimize;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener set_title;
	struct wl_listener fullscreen;
#ifdef XWAYLAND
	struct wl_listener activate;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener configure;
	struct wl_listener set_hints;
	struct wl_listener set_geometry;
#endif
	uint32_t bw;
	uint32_t tags, oldtags, mini_restore_tag;
	bool dirty;
	uint32_t configure_serial;
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel;
	int32_t isfloating, isurgent, isfullscreen, isfakefullscreen,
		need_float_size_reduce, isminimized, isoverlay, isnosizehint,
		ignore_maximize, ignore_minimize;
	int32_t ismaximizescreen;
	int32_t overview_backup_bw;
	int32_t fullscreen_backup_x, fullscreen_backup_y, fullscreen_backup_w,
		fullscreen_backup_h;
	int32_t overview_isfullscreenbak, overview_ismaximizescreenbak,
		overview_isfloatingbak;

	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct wl_listener foreign_activate_request;
	struct wl_listener foreign_fullscreen_request;
	struct wl_listener foreign_close_request;
	struct wl_listener foreign_destroy;
	struct wl_listener foreign_minimize_request;
	struct wl_listener foreign_maximize_request;
	struct wl_listener set_decoration_mode;
	struct wl_listener destroy_decoration;

	const char *animation_type_open;
	const char *animation_type_close;
	int32_t is_in_scratchpad;
	int32_t iscustomsize;
	int32_t iscustompos;
	int32_t is_scratchpad_show;
	int32_t isglobal;
	int32_t isnoborder;
	int32_t isnoshadow;
	int32_t isnoradius;
	int32_t isnoanimation;
	int32_t isopensilent;
	int32_t istagsilent;
	int32_t iskilling;
	int32_t istagswitching;
	int32_t isnamedscratchpad;
	bool is_pending_open_animation;
	bool is_restoring_from_ov;
	float scroller_proportion;
	float stack_proportion;
	float old_stack_proportion;
	bool need_output_flush;
	uint32_t animation_reserved;  /* Placeholder for animation struct */
	uint32_t opacity_animation_reserved;  /* Placeholder */
	int32_t isterm, noswallow;
	int32_t allow_csd;
	int32_t force_maximize;
	int32_t force_tiled_state;
	int32_t pid;
	Client *swallowing, *swallowedby;
	bool is_clip_to_hide;
	bool drag_to_tile;
	bool scratchpad_switching_mon;
	bool fake_no_border;
	int32_t nofocus;
	int32_t nofadein;
	int32_t nofadeout;
	int32_t no_force_center;
	int32_t isunglobal;
	float focused_opacity;
	float unfocused_opacity;
	char oldmonname[128];
	int32_t noblur;
	double master_mfact_per, master_inner_per, stack_inner_per;
	double old_master_mfact_per, old_master_inner_per, old_stack_inner_per;
	double old_scroller_pproportion;
	bool ismaster;
	bool cursor_in_upper_half, cursor_in_left_half;
	bool isleftstack;
	int32_t tearing_hint;
	int32_t force_tearing;
	int32_t allow_shortcuts_inhibit;
	float scroller_proportion_single;
	bool isfocusing;
	struct Client *next_in_stack;
	struct Client *prev_in_stack;
};

/* Monitor structure - must match the one in mango.c */
struct Monitor {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_output_state pending;
	struct wl_listener frame;
	struct wl_listener destroy;
	struct wl_listener request_state;
	struct wl_listener destroy_lock_surface;
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wlr_box m;		  /* monitor area, layout-relative */
	struct wlr_box w;		  /* window area, layout-relative */
	struct wl_list layers[4]; /* LayerSurface::link */
	uint32_t seltags;
	uint32_t tagset[2];

	struct wl_list dwl_ipc_outputs;
	int32_t gappih; /* horizontal gap between windows */
	int32_t gappiv; /* vertical gap between windows */
	int32_t gappoh; /* horizontal outer gaps */
	int32_t gappov; /* vertical outer gaps */
	void *pertag;  /* Opaque Pertag structure */
	Client *sel, *prevsel;
	int32_t isoverview;
	int32_t is_in_hotarea;
	int32_t asleep;
	uint32_t visible_clients;
	uint32_t visible_tiling_clients;
	uint32_t visible_scroll_tiling_clients;
	struct wlr_scene_optimized_blur *blur;
	char last_surface_ws_name[256];
	struct wlr_ext_workspace_group_handle_v1 *ext_group;
};

#endif /* MANGO_TYPES_H */
