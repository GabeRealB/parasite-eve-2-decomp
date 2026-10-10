#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_3_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_3_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_motel_room_3_80180D24[13];

/// Night motel room 3's single room-lighting entry, borrowed by the stage map.
///
/// Room 1 uses seven point lights and one cone light in every view, with the
/// default ambient minima. Keep this overlay loaded while its lights are used;
/// coordinate composition and shading mutate the borrowed light records.
extern WorldCoordRoomLighting gDryfieldNightMotelRoom3RoomLighting[1];

extern WorldCollisionRoomResources D_dryfield_night_motel_room_3_8017DAA4[];

/// Night motel room 3's per-room logical-view maps, indexed by room minus one.
///
/// The sole entry borrows gameplay's `gViewIdentityMap`: logical views 1..11
/// select the same 1-based camera, image and sprite indices. The room directory
/// lives in this overlay; the map itself lives in gameplay and has no terminator.
extern u8* gDryfieldNightMotelRoom3ViewMaps[1];

extern ViewCount D_dryfield_night_motel_room_3_8017DAB8[];

extern DirectionWarpEntry D_dryfield_night_motel_room_3_8017DABC[];

extern ViewCamera D_dryfield_night_motel_room_3_8017E1A4[];

extern SpriteView D_dryfield_night_motel_room_3_80180118[];

/// Night motel room 3's collision and footstep policies, indexed by class 0..7.
///
/// Classes 0 and 4..7 block probes, allow weapon impacts and have no footstep
/// cues. Classes 1 and 2 have separate cue sets with those same policies.
/// Class 3 passes probes and suppresses weapon impacts, with no footstep cues.
/// Every class applies pushback. The directory and its records belong to this
/// overlay and remain borrowed only while it is loaded; there is no terminator.
extern WorldCollisionSurfaceProperties* gDryfieldNightMotelRoom3SurfacePropertiesByClass[8];

/// Draws the night motel room 3's flickering textured flares for the current view.
///
/// Bank-6 effect 0x109 uses the room-local camera view: view 3 draws one flare,
/// view 4 draws that flare then a second, views 10 and 11 draw only the second,
/// and view 8 draws a third with a different texture column and radius scale.
/// Other views reserve no packets. `task` supplies the callback signature and
/// is unused; this handler allocates no work and does not advance task state.
///
/// Requires the room overlay, live session, current view matrices and flare
/// textures, initialized scratch space, and frame arena/ordering-table storage.
/// Each selected flare reserves one packet even when rejected at camera Z / 4
/// below 17. Queued packets remain in the frame arena until GPU completion.
void dryfieldNightMotelRoom3DrawFlaresTask(Task* task);

/// Runs night motel room 3's room-message task for one update.
///
/// Requires a live task: state 0 initializes, 1 idles and 2 releases the task.
/// The state index is unchecked. Keep this room overlay loaded while the task
/// or its registered message handlers can run; the final state ends its lifetime.
void dryfieldNightMotelRoom3RoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_3_H
