#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_UNDERPASS_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_UNDERPASS_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_underpass_801802DC[12];

// dryfield_night_underpass
extern WorldCollisionRoomResources D_dryfield_night_underpass_8017DD70[];

/// Model lighting for nighttime underpass room IDs 1..6.
///
/// Rooms 1, 3, 5 and 6 share nine point lights, including eight ceiling lamps;
/// rooms 2 and 4 share six dimmer point lights. All use the default ambient
/// minima. Keep this overlay loaded while its mutable lights are in use.
extern WorldCoordRoomLighting gDryfieldNightUnderpassRoomLighting[6];

/// Logical-view maps for nighttime underpass room IDs 1..6.
///
/// Indexed by room minus one, then logical view minus one. Room 1 borrows
/// `gViewIdentityMap` for views 1..26; rooms 2..6 each map eleven logical views
/// to camera/image/sprite indices 1..26. Their stored twelfth zero byte is
/// outside the live range, not a terminator. Keep the room overlay loaded.
extern u8* gDryfieldNightUnderpassViewMaps[6];

extern ViewCount D_dryfield_night_underpass_8017DE54[];

extern DirectionWarpEntry D_dryfield_night_underpass_8017DE60[];

/// Number of mapped camera/image/sprite slots in the nighttime underpass.
enum { DRYFIELD_NIGHT_UNDERPASS_MAPPED_VIEW_COUNT = 26 };

/// Cameras for nighttime underpass mapped view indices 1..26.
///
/// Index by `viewGetMappedIndex() - 1`, after the room's logical-view mapping.
/// Rotations use Q12, translations are negated world-space camera origins,
/// and projection distances are pixels. The table has no terminator;
/// keep this overlay loaded until a queued camera application has finished.
extern ViewCamera gDryfieldNightUnderpassViewCameras[DRYFIELD_NIGHT_UNDERPASS_MAPPED_VIEW_COUNT];

/// Sprite descriptors for nighttime underpass mapped view indices 1..26.
///
/// Index by `viewGetMappedIndex() - 1`. Each batch list keeps decoded background
/// strips and ends in SPRITE_BATCH_END; views 3, 4, 5, 9, 13, 14, 15 and 19
/// additionally select source sprites. All draw-area pointers are NULL.
/// Descriptors and their mutable source/batch arrays borrow this overlay's
/// lifetime and must stay loaded during packet allocation and drawing.
extern SpriteView gDryfieldNightUnderpassSpriteViews[DRYFIELD_NIGHT_UNDERPASS_MAPPED_VIEW_COUNT];

/// Nighttime underpass collision policies indexed directly by surface class 0..7.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Classes 2 and 4 share footstep cue bases; class 3 has its own set;
/// classes 0, 1 and 5..7 have no footstep table. The records belong to this
/// overlay and remain valid only while it is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightUnderpassSurfacePropertiesByClass[8];

/// Draws the nighttime underpass's world-space flares for the current camera view.
///
/// Called each frame while this room overlay is loaded. `unusedTask` supplies no
/// position or state. Nonzero `GAME_FLAG_053` suppresses drawing; otherwise each
/// of eight fixed points is selected by bit `location.loc.view` of its view mask.
/// The room's view IDs are 1..26, and only views 2..11 select any points.
/// Each selected point uses flare texture column zero and a pixel half-extent of
/// `640 * 39 / depth`, where depth is camera Z / 4 and must be nonzero for an
/// accepted projection. Requires the current view matrices, scratch stack and
/// frame's GPU packet arena and ordering table; queued packets live for the frame.
void dryfieldNightUnderpassDrawFlaresTask(Task* unusedTask);

/// Runs the night underpass's room initialization, message wait and teardown.
///
/// State 0 installs the room handlers; state 1 waits for messages;
/// state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldNightUnderpassRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_UNDERPASS_H
