/* Layout flags exposed to plugins and core for feature detection */
#ifndef LAYOUT_FLAGS_H
#define LAYOUT_FLAGS_H

#include <stdint.h>

/* Layout behaviour flags */
#define LAYOUT_FLAG_NONE      0u
#define LAYOUT_FLAG_SCROLLER  (1u << 0) /* layout arranges scroll-stacks */
#define LAYOUT_FLAG_ROW       (1u << 1) /* layout organizes windows into rows */
#define LAYOUT_FLAG_VERTICAL  (1u << 2) /* scroller arranged vertically */

#endif /* LAYOUT_FLAGS_H */
