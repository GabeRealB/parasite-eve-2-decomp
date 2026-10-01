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

extern WorldCoordRoomAmbientEntry D_mist_shooting_gallery_8018DFD4[19];

extern s32 D_mist_shooting_gallery_8018E0BC;

extern s32 D_mist_shooting_gallery_8018E0C0;

s32 func_mist_shooting_gallery_80184470(s32 score);

s32 func_mist_shooting_gallery_80184970(s32 arg0);

#endif // SRC_ROOMS_MIST_SHOOTING_GALLERY_MIST_SHOOTING_GALLERY_PRIVATE_H
