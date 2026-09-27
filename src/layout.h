// Grid geometry and navigation for menus. Pure: plain integers, no SDL, no globals, so
// tests/test_layout.c can build it on its own. launcher.c does the drawing.
#ifndef LAYOUT_H
#define LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

#define LAYOUT_MIN_BUTTON 32   // Same as MIN_ICON_SIZE in launcher.h
#define LAYOUT_MAX_BUTTON 1024 // Same as MAX_ICON_SIZE in launcher.h

typedef enum {
    LAYOUT_UP,
    LAYOUT_DOWN,
    LAYOUT_LEFT,
    LAYOUT_RIGHT
} LayoutDirection;

// Rows, Columns and IconSize from a menu section or from [Layout]; 0 means "not set"
typedef struct {
    int rows;
    int columns;
    int icon_cap;
} LayoutOverrides;

// Everything layout_compute needs to size a menu's buttons
typedef struct {
    int rows;
    int columns;
    int icon_cap;    // Largest allowed button size; 0 = no cap
    int spacing;     // Gap between buttons in px, across and down
    int title_block; // Title padding + font height below each icon; 0 without titles
    int hpad;        // Requested highlight padding; negative counts as 0
    int vpad;
} LayoutParams;

// The part of the screen the buttons may use
typedef struct {
    int x;
    int y;
    int w;
    int h;
    int vcenter;     // Vertical centre of the button block, in px from the top of the screen
} LayoutArea;

// The computed layout of one menu
typedef struct {
    int rows;        // After any reduction to fit the screen
    int columns;
    int button;      // Square button size in px
    int x_advance;   // Distance between neighbouring buttons' x
    int y_advance;   // Distance between neighbouring rows' y
    int x_origin;    // Top-left of the first visible slot
    int y_origin;
    int hpad;        // Highlight padding after capping
    int vpad;
} LayoutGeometry;

// Where the highlight is, and what is scrolled into view
typedef struct {
    int selected;    // Entry index
    int first;       // Strip: first visible entry. Grid: first visible row
} LayoutPosition;

LayoutOverrides layout_resolve(LayoutOverrides menu, LayoutOverrides global, LayoutOverrides builtin);
bool layout_parse_count(const char *value, int *count);
int layout_compute(const LayoutParams *params, const LayoutArea *area, int entry_count,
                   LayoutGeometry *geometry, char *why, size_t why_size);
LayoutPosition layout_move(const LayoutGeometry *geometry, int entry_count, LayoutPosition position,
                           LayoutDirection direction, bool wrap);
LayoutPosition layout_clamp(const LayoutGeometry *geometry, int entry_count, LayoutPosition position);
bool layout_slot(const LayoutGeometry *geometry, LayoutPosition position, int index, int *x, int *y);
bool layout_can_scroll(const LayoutGeometry *geometry, int entry_count, LayoutPosition position,
                       LayoutDirection direction);

#endif
