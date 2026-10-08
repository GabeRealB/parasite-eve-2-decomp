#ifndef SRC_ROOMS_MIST_SHOOTING_GALLERY_MIST_SHOOTING_GALLERY_PRIVATE_H
#define SRC_ROOMS_MIST_SHOOTING_GALLERY_MIST_SHOOTING_GALLERY_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/room.h"

extern WorldCollisionGrid D_mist_shooting_gallery_80189968;

extern WorldCollisionTrigger D_mist_shooting_gallery_8018BDE8[28];

extern WorldCollisionTrigger D_mist_shooting_gallery_8018C638[21];

/// Default gallery room lighting: fourteen point lights contributing in every view.
///
/// Selected initially by the room table and restored by the lighting selector's
/// zero option. There are no directional or cone lights. The loaded room overlay
/// owns the collection and its mutable light array; borrowed pointers must not
/// outlive the overlay. Coordinate updates and shading queries modify the lights.
extern WorldCoordRoomLights gMistShootingGalleryDefaultRoomLights;

extern WorldCoordRoomLights D_mist_shooting_gallery_8018DF38;

/// Number of per-view colour entries in the gallery's ambient-light table.
///
/// Excludes entry zero, which stores this count; valid view indices are 1..18.
enum { MIST_SHOOTING_GALLERY_AMBIENT_VIEW_COUNT = 18 };

/// Minimum ambient model-shading colours for the gallery's 18 views.
///
/// Entry zero stores the view count; entries 1..18 use the room's 1-based view
/// index. RGB components use 16 units per 8-bit colour level; stored brightness
/// in the fourth component is unused by lighting. Both room light collections
/// share this table. Borrowed pointers are valid only while the overlay is loaded.
extern const WorldCoordRoomAmbientEntry gMistShootingGalleryViewAmbientTable[MIST_SHOOTING_GALLERY_AMBIENT_VIEW_COUNT + 1];

extern s32 D_mist_shooting_gallery_8018E0BC;

extern s32 D_mist_shooting_gallery_8018E0C0;

/// Returns the bonus BP earned by a score in the active gallery course.
///
/// Requires the live gallery controller and its initialized work. Returns
/// 0, 100, 200 or 300 BP, without crediting rewards or changing course flags.
/// Minimum scores for 100/200/300 BP by zero-based course are:
/// 0: 8000/9000/10000; 1: 16000/16800/17400; 2: 39000/46000/50000;
/// 3: 52000/56000/60000; 4: 50000/53000/55000. Other courses return 0.
s32 mistShootingGalleryGetBonusBp(s32 score);

s32 func_mist_shooting_gallery_80184970(s32 arg0);

#endif // SRC_ROOMS_MIST_SHOOTING_GALLERY_MIST_SHOOTING_GALLERY_PRIVATE_H
