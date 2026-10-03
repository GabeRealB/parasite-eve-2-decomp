#ifndef GAMEPLAY_MAP_H
#define GAMEPLAY_MAP_H

#include "common.h"

#include "main/tmd_types.h"

/// `MenuMapArea.page` of an area index the stage's map does not show.
#define MENU_MAP_AREA_PAGE_NONE 0xF000

/// `MenuMapArea.page` of the record closing a stage's table.
#define MENU_MAP_AREA_PAGE_END 0xFFFF

/// Where one area sits on the map screen.
///
/// Each stage has a table of these, indexed by `GameSession.location.loc.area`.
/// A record names the map page the area is drawn on and ties a point in the
/// area's own coordinates to a point on that page, which is what places the
/// player cursor: one map pixel covers `scaleX` units of X and `scaleZ` units
/// of Z, and the page's Y runs against Z.
typedef struct {
    s16 originX; // Area-space X of the reference point
    s16 originZ; // Area-space Z of the reference point
    u16 mapX;    // Map-screen X of the reference point; wraps as 16-bit, so 0xFFFD is -3
    u16 mapY;    // Map-screen Y of the reference point; wraps the same way
    s16 scaleX;  // Area units per map pixel along X
    s16 scaleZ;  // Area units per map pixel along Z
    u16 page;    // Map page showing the area, counted from 1, or a `MENU_MAP_AREA_PAGE_*` marker
} MenuMapArea;
STATIC_ASSERT_SIZEOF(MenuMapArea, 0xE);

/// The name the map screen shows for one area.
///
/// Each stage has a table of these with one record for every area id up to
/// the stage's highest, indexed by `GameSession.location.loc.area - 1`; area 0
/// has no record. The map screen shows the name of the area the player is in,
/// in a panel sized to fit it. An id the stage has no room for, or a room it
/// leaves unnamed, holds the empty string.
typedef struct {
    u8 text[0x20]; // Name as one NUL-terminated line of UI text
} MenuMapAreaName;
STATIC_ASSERT_SIZEOF(MenuMapAreaName, 0x20);

/// `MenuMapAreaShape.page` of an area index the map screen draws no shape for.
#define MENU_MAP_AREA_SHAPE_PAGE_NONE 0xFF

/// `MenuMapAreaShape.pairedArea` of an area that shares its place with no other.
#define MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE 0xFF

/// The shape one area takes on its page of the map screen.
///
/// Each stage has a table of these, indexed by `GameSession.location.loc.area`.
/// A page's picture shows every area on it, and the map screen draws an area's
/// shape over the picture to change how that area reads: covered with a
/// pattern while the area is unvisited and unknown, dimmed while it is
/// unvisited but known, and tinted while it is visited and marked. The model
/// is flat, in its X/Z plane, and is scaled onto the page by a factor the stage
/// sets.
///
/// An area with a page but no model has no shape of its own, and only its
/// icons are drawn. Two areas that stand at the same place on the map name
/// each other in `pairedArea`, and the shape then counts as visited, or as
/// marked, when either area is. Both are looked up in the 32-area word that
/// holds the record's own area, so a pair works only within areas 1..32 or
/// within areas 33..64.
typedef struct {
    TmdSource* model;      // Shape in the map picture package of `page`, or NULL for an area drawn without one
    u8         page;       // Map page the area is on, counted from 1, or `MENU_MAP_AREA_SHAPE_PAGE_NONE`
    u8         pairedArea; // Area sharing this one's place, as `GameSession.location.loc.area`, or `MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE`
} MenuMapAreaShape;
STATIC_ASSERT_SIZEOF(MenuMapAreaShape, 8);

/// `MenuMapIcon.kind` of a telephone.
#define MENU_MAP_ICON_KIND_TELEPHONE 1

/// `MenuMapIcon.kind` of the marker for where the current objective is.
#define MENU_MAP_ICON_KIND_OBJECTIVE 2

/// A 16x16 icon the map screen draws over one area of a stage's map.
///
/// Each stage has a table of these, closed by a record whose `page` is 0. An
/// icon is drawn with its area, while its page is the one on screen and its
/// `condition` holds. A telephone and the kind-0 picture are fixed and appear
/// only once the area has been visited; an objective marker pulses, and shows
/// in an area not yet visited too. What the kind-0 picture depicts is unproven:
/// it sits in rooms with a shop, behind the flag that opens the shop, and in
/// other rooms unconditionally.
typedef struct {
    u8  page;      // Map page the icon is on, counted from 1; the page of its area's `MenuMapArea`
    u8  area;      // Area the icon belongs to, as `GameSession.location.loc.area`
    u8  kind;      // Picture (0 fixed picture, 1 telephone, 2 objective marker)
    u8  condition; // Objective marker: the objective number it marks. Other kinds: a game flag that must be set, or 0 for always
    s16 x;         // Map-screen X of the icon's centre
    s16 y;         // Map-screen Y of the icon's centre
} MenuMapIcon;
STATIC_ASSERT_SIZEOF(MenuMapIcon, 8);

/// `MenuMapMarker.area` of a marker that needs no area to have been visited.
#define MENU_MAP_MARKER_AREA_ANY 0

/// `MenuMapMarker.area` of a marker the map screen never draws.
#define MENU_MAP_MARKER_AREA_NEVER 0xFF

/// A 16x16 marker the map screen draws at a fixed point while a game flag asks for it.
///
/// Each stage has a table of these, closed by a record whose `page` is 0, beside
/// a table of marker flags of the same order and count: record `n` is drawn
/// while flag `n` holds 2, in one of two pictures that the flag's table entry
/// selects. What each picture depicts is unproven. Most of the flags belong to
/// a warp, which writes 1 on an arrival through it, and its room writes 2 when
/// it refuses a transition there; a few are written by a room's own events
/// instead. A flag may have several records, one for each page its place shows on.
typedef struct {
    u8  page; // Map page the marker is on, counted from 1; 0 closes the table
    u8  area; // Area, as `GameSession.location.loc.area`, that must have been visited for the marker to show, or a `MENU_MAP_MARKER_AREA_*` value
    s16 x;    // Map-screen X of the marker's centre
    s16 y;    // Map-screen Y of the marker's centre
} MenuMapMarker;
STATIC_ASSERT_SIZEOF(MenuMapMarker, 6);

#endif // GAMEPLAY_MAP_H
