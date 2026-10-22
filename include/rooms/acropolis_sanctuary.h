#ifndef INCLUDE_ROOMS_ACROPOLIS_SANCTUARY_H
#define INCLUDE_ROOMS_ACROPOLIS_SANCTUARY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Sanctuary layouts indexed by the saved placement variant (0..11).
///
/// Variant 1 contains No. 9 GOLEM and Rupert; variant 2 contains Rupert alone.
/// Slots 0 and 3..11 are absent. Gameplay borrows the placement and resource
/// tables while this room and the Acropolis map overlay remain loaded.
extern AreaVariant gAcropolisSanctuaryAreaVariants[12];

extern TmdSource gAcropolisSanctuaryModel090F0;

extern TmdSource gAkropolisSanctuaryAcropolisSanctuaryModel09584;

/// Collision resources for the sanctuary's single room.
///
/// Room 1 borrows the live grid, 18 view boundaries and 17 action triggers;
/// it has no sight occluders. Keep this overlay loaded until they are unlinked.
extern WorldCollisionRoomResources gAcropolisSanctuaryRoomCollisionResources[1];

/// Logical-view maps for the sanctuary's single room.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..16 select matching
/// camera, image and sprite indices. Keep both room and gameplay overlays loaded.
extern u8* gAcropolisSanctuaryViewMaps[1];

/// Logical-view counts for the sanctuary's single room.
///
/// Room 1 has 16 identity-mapped views, matching its sprite and camera arrays.
/// The map overlay borrows this table while the sanctuary overlay is loaded.
extern ViewCount gAcropolisSanctuaryViewCounts[1];

/// Lighting for the sanctuary's single room, with default ambient minima.
///
/// Room 1 borrows nine mutable point lights and has no directional or cone
/// lights. The descriptor and light storage are live only while the room is loaded.
extern WorldCoordRoomLighting gAcropolisSanctuaryRoomLightingTable[1];

/// Sanctuary warp endpoints, indexed by the 1-based warp ID minus one.
///
/// Slots 1 and 3 use the promenade doorway and view 2; slot 2 uses the roof-garden
/// doorway and view 7. Slots 1 and 3 intentionally retain identical records.
/// Player and companion placements use room coordinates and 4096 units/turn.
/// The map overlay borrows all three entries while this room is loaded.
extern DirectionWarpEntry gAcropolisSanctuaryWarpEntries[3];

/// Sprite resources for sanctuary views 1..16, indexed by mapped view minus one.
///
/// Every view retains decoded background strips and has no clip list. Views
/// 1, 10 and 15 have no source sprites. Scripts change batch visibility in
/// views 12 and 16; keep the overlay loaded while sources or batches are used.
extern SpriteView gAcropolisSanctuarySpriteViews[16];

/// Cameras for sanctuary views 1..16, indexed by mapped view minus one.
///
/// Rotations use 4096 for 1.0, translations are negated camera origins in room
/// units, and projection distances are pixels. The map overlay borrows these
/// records; keep this room loaded until queued camera application completes.
extern ViewCamera gAcropolisSanctuaryViewCameras[16];

/// Sanctuary collision policies indexed by surface class 0..7.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Classes 1 and 2 have separate footstep cue pairs; classes 0 and 3..7 share
/// a record without footstep cues. Borrowed records require this room to stay loaded.
extern WorldCollisionSurfaceProperties* gAcropolisSanctuarySurfacePropertiesByClass[8];

/// Creates the sanctuary's twelve flames and maintains the mosaic cleanup latch.
///
/// Bank-6 slot 0x77 requires a live coordinate body. State 0 spawns six dim
/// variant-2 flames at default size and six variant-0 flames at size 160, then
/// registers this receiver in `GAME_TASK_SLOT_ROOM_EFFECT`. Later calls update
/// the latch too: view 16 sets it, view 12 retains it, other views clear it.
/// Keep this overlay and the placement coordinate live through task teardown.
void acropolisSanctuaryRoomEffectTask(Task* task);

/// Spawns the mosaic's 72 tiles and a second copy of 16 selected tiles.
///
/// Bank-6 slot 0x78 requires a live coordinate body and owned, zeroed
/// `EffectWork` in `spawnArg2.pointer`. State 0 uses `move` as a transient
/// local offset: texel V scales Y by 1145/128, texel U scales negative Z by
/// 2147/256, then the size-class corner is subtracted. Each child snapshots
/// this placement. The next call frees the work and kills this counted task.
/// Keep the sanctuary overlay loaded until all its child effects end.
void acropolisSanctuaryMosaicTask(Task* task);

/// Draws and advances one intact mosaic tile, spawning shards when it breaks.
///
/// Bank-6 slot 0x79 requires an owned, zeroed `EffectWork`, a live coordinate
/// body and a tile index 0..71 in `spawnArg1.value`. `scale` caches the tile's
/// thrown flag, `angle` its hold in frames, `move` its velocity in whole
/// coordinate units/frame, and `pos` its spin in angle units/frame (4096/turn).
/// Motion is seeded only when age zero projects at depth 17 or greater.
/// The tile drifts after the hold, accelerates downward by 3 per frame and
/// expires more than 60 frames after the hold; splitting advances its age.
///
/// Uses four signed-halfword world corners, one `POLY_FT4` packet per call
/// (also consumed below depth 17), the mosaic's 8-bit texture and palette,
/// and one `_AcropolisSanctuaryMosaicTileScratch` block released before return.
/// Keep the overlay, projection settings and GPU resources live throughout;
/// packets remain live through GPU completion. Teardown frees the effect work.
void acropolisSanctuaryMosaicTileTask(Task* task);

/// Draws and advances a scaled mosaic shard that can bounce or split again.
///
/// Bank-6 slot 0x7A requires an owned, zeroed `EffectWork` and live coordinate
/// body. On age zero, spawn bits 0..11 select a tile (0..71), bits 12..15
/// select drift (zero gives Y velocity -31..0; nonzero gives 0..15),
/// and the signed high halfword gives size in 4.12 units (zero means 4096).
/// Room spawns use positive sizes through 4096. Only the tile index is retained.
/// `angle` holds size, `scale` drift selection, `index` bounce count, `move`
/// velocity in coordinate units/frame and `pos` spin in angle units/frame (4096/turn).
/// Size exceeds 1024 to split; children encode half the parent size. Kills the
/// task at age 61 or two bounces, freeing its counted effect work.
///
/// Requires the mosaic's 8-bit texture/palette, initialized projection, arena
/// room for one `POLY_FT3` (also consumed below depth 17), and a scratch block
/// for three halfword world corners, released before return. Keep the overlay
/// and GPU resources live; queued packets survive through GPU completion.
void acropolisSanctuaryMosaicShardTask(Task* task);

/// Draws the sanctuary's view-gated, flickering additive flame billboard.
///
/// Bank-6 slot 0x8B requires a live coordinate body and the spawner's owned
/// `EffectWork` in `spawnArg2.pointer`. In `spawnArg1.value`, bits 0..3 select
/// a placement (0..11), bits 8..9 a texture/palette variant (0..2), and bits
/// 16..27 a size factor (1..4095, or 0 for 640). Other bits are ignored. The
/// texture math addresses four 40-by-40 cells, but the grey tables cover only
/// variants 0..2; the room's spawns select 0 and 2, never 1 or 3. The current
/// view is 1..16 and each placement's mask determines whether it is drawn.
///
/// Initialization waits for the first visible update. It retains the placement
/// in the spawn word and caches size, variant, base grey and odd-frame flicker
/// in the work's `scale`, `angle`, `period` and `step`. Composed position is
/// narrowed to signed 16-bit coordinates and projected through `GsWSMATRIX`.
/// Half-extent is size factor * 39 / depth pixels, with depth SZ3 / 4; depths
/// below 17 reserve a packet but queue nothing. GTE projection flags are not tested.
///
/// Requires initialized projection settings, the flame texture and palettes,
/// one aligned `RoomGlowSpriteScratch` block and arena room for one `POLY_FT4`
/// on each visible update. Scaled depth wraps into the current ordering table;
/// queued packets live through GPU completion. Scratch is released every call;
/// the task and effect work persist until external teardown. Keep this room
/// overlay and the task's coordinate alive through every callback.
void acropolisSanctuaryFlameTask(Task* task);

/// Dispatches the sanctuary room's setup, per-frame update and exit states.
///
/// The map's room descriptor creates this no-body task. `state` must index
/// its three handlers (0 setup, 1 update, 2 kill); each call copies that table
/// before dispatch. Keep this room overlay loaded for the task's lifetime.
void acropolisSanctuaryRoomTask(Task* task);

/// Updates the sanctuary placed item's model visibility from its saved state and view.
///
/// Requires the live `Enemy` supplied in `spawnArg2.pointer` by area placement
/// and a TMD body. The place key's low byte selects the current area's 2-bit
/// object state. State 2 or mapped views 11/13 suppress active drawing;
/// otherwise enables the flagged pass and clears ordering-table bias.
void acropolisSanctuaryItemVisibilityTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_SANCTUARY_H
