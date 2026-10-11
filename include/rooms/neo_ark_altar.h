#ifndef INCLUDE_ROOMS_NEO_ARK_ALTAR_H
#define INCLUDE_ROOMS_NEO_ARK_ALTAR_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Collision resources for room selectors 1..3, indexed by room minus one.
///
/// All three room slots borrow the same grid and four action triggers; there are no
/// view-boundary triggers or sight occluders.
///
/// The stage map borrows this directory while the room overlay is loaded. Keep the grid
/// and its pools live while active, and the mutable trigger/occluder lists live until
/// unlinked. This directory has no terminator or bounds check.
extern WorldCollisionRoomResources gNeoArkAltarRoomCollisionResources[3];

/// Extents of the altar's progress variants and shared mapped-view resources.
enum {
    NEO_ARK_ALTAR_ROOM_VARIANT_COUNT  = 3,
    NEO_ARK_ALTAR_VIEW_RESOURCE_COUNT = 8,
};

/// Lighting for room variants 1..3, all borrowing the same directional light.
///
/// Indexed by room minus one. No per-view ambient table is supplied, so the
/// default ambient minima apply. The altar overlay must remain loaded while
/// the records or their mutable light collection are used.
extern WorldCoordRoomLighting gNeoArkAltarRoomLighting[NEO_ARK_ALTAR_ROOM_VARIANT_COUNT];

/// Logical-view maps for the altar's three tile-sequence progress variants.
///
/// Indexed by room minus one: 1 uses the identity map, 2 swaps mapped files
/// 2 and 4 after sequence 2, and 3 swaps files 2 and 7 after both sequences.
/// Each map accepts logical views 1..8 and returns a 1-based index into the
/// area's camera, image and sprite resources. Maps have no terminator and
/// require the altar and gameplay overlays to remain loaded.
extern u8* gNeoArkAltarViewMaps[NEO_ARK_ALTAR_ROOM_VARIANT_COUNT];

/// Reverse-lookup limits for altar room variants 1 and 2, eight entries each.
///
/// Indexed by room minus one; this array contains no count for variant 3.
/// Forward view selection in all three variants uses `gNeoArkAltarViewMaps`
/// independently. Requires the altar overlay to remain loaded.
extern ViewCount gNeoArkAltarViewCounts[2];

/// A single warp endpoint record, selected by warp ID 1.
///
/// Slot 1 serves the Pyramid connection, with arrival view 2 and scripted player
/// control. Player and companion have distinct world positions and facing. It
/// has no sounds or map marker.
///
/// Positions use signed whole world coordinates; yaw uses 4096 units per turn. The two
/// byte spans have unproven roles and remain zero. The stage map borrows this array;
/// keep the room overlay loaded through departure or arrival setup. Consumers copy
/// complete entries and actor spawns borrow their transforms synchronously. There is
/// no terminator or bounds check.
extern DirectionWarpEntry gNeoArkAltarWarpEntries[1];

/// Cameras for the altar's eight mapped view files, indexed by file minus one.
///
/// File 1 looks down on the altar, file 5 is the switch close-up, and files
/// 2/4/6 and 7/8 share their respective shots. Rotation uses ONE (4096) for
/// unit scale; translations are negated world-space camera origins and
/// projection distances are pixels. Requires the altar overlay to stay loaded.
extern ViewCamera gNeoArkAltarViewCameras[NEO_ARK_ALTAR_VIEW_RESOURCE_COUNT];

/// Foreground and switch sprites for the altar's eight mapped view files.
///
/// Indexed by mapped file minus one. Files 1, 3, 6 and 8 have no sprites;
/// files 2, 4 and 7 provide foreground layers, with switch indicators in
/// batch 1 of files 4 and 7. File 5 has six switch frames in batches 1..6,
/// with one visible at a time. Every batch list ends with SPRITE_BATCH_END;
/// no view supplies a draw-area list. The room owns the mutable lists and
/// sources, which remain valid only while the altar overlay is loaded.
extern SpriteView gNeoArkAltarSpriteViews[NEO_ARK_ALTAR_VIEW_RESOURCE_COUNT];

/// Collision policies for the altar's surface classes 0..7.
///
/// All classes block probes, allow weapon impacts and apply pushback. Class 4
/// supplies paired walking/running/jump footstep cues; the others have none.
/// Grid faces use classes 0, 1 and 4. The pointers borrow room-owned records
/// and require the altar overlay to remain loaded.
extern WorldCollisionSurfaceProperties* gNeoArkAltarSurfaceProperties[8];

/// Leaves the altar's bank-6 effect task (slot 0x15A) unchanged.
///
/// `unusedTask` is ignored: this callback draws nothing and does not end the task.
void neoArkAltarEffectNoopTask(Task* unusedTask);

/// Initializes the altar's room services, then keeps its message task alive.
///
/// `task` must be live with state 0..2: 0 installs the room message table,
/// updates the switch sprites from the saved choice and starts the tile
/// sequence controller; 1 waits for messages; 2 releases the task. The altar
/// overlay must remain loaded.
/// The map overlay selects this callback for Neo Ark area 20.
void neoArkAltarRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_ALTAR_H
