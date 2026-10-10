#ifndef INCLUDE_ROOMS_ACROPOLIS_OBSERVATORY_H
#define INCLUDE_ROOMS_ACROPOLIS_OBSERVATORY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Extents of the observatory's room, mapped-view and surface-class tables.
enum {
    ACROPOLIS_OBSERVATORY_ROOM_COUNT          = 2,
    ACROPOLIS_OBSERVATORY_VIEW_COUNT          = 8,
    ACROPOLIS_OBSERVATORY_SURFACE_CLASS_COUNT = 8,
    ACROPOLIS_OBSERVATORY_WARP_COUNT          = 4,
};

extern AreaVariant D_acropolis_observatory_80181264[19];

/// Collision resources for rooms 1 and 2, indexed by room minus one.
///
/// Both entries borrow one grid, ten view boundaries and nine action triggers,
/// with no sight occluders. Setup mutates the shared grid and trigger records;
/// keep this overlay loaded while active and until its trigger lists are unlinked.
extern WorldCollisionRoomResources gAcropolisObservatoryRoomCollisionResources[ACROPOLIS_OBSERVATORY_ROOM_COUNT];

/// Logical-to-mapped view indices for rooms 1 and 2, indexed by room minus one.
///
/// Each room uses eight unsigned 1-based camera/image/sprite indices, indexed by
/// logical view minus one, without a terminator. Room 1 borrows gameplay's larger
/// identity map; room 2 swaps indices 2 and 8. Keep gameplay and this room
/// overlay loaded while using the borrowed maps.
extern u8* gAcropolisObservatoryViewMaps[ACROPOLIS_OBSERVATORY_ROOM_COUNT];

/// Signed logical-view counts for rooms 1 and 2, indexed by room minus one.
///
/// Both rooms have eight views; no terminator follows the two entries.
/// The stage-map directory borrows this array while the room overlay is loaded.
extern ViewCount gAcropolisObservatoryViewCounts[ACROPOLIS_OBSERVATORY_ROOM_COUNT];

/// Lighting descriptors for rooms 1 and 2, indexed by room minus one.
///
/// Both borrow the same twelve point lights and use gameplay's default ambient
/// minima. Light transforms and attenuation remain writable; keep this overlay
/// loaded while the descriptors or light arrays are in use.
extern WorldCoordRoomLighting gAcropolisObservatoryRoomLightingTable[ACROPOLIS_OBSERVATORY_ROOM_COUNT];

/// Four arrival/departure endpoints, indexed by 1-based warp ID minus one.
///
/// Slots 1 and 3 use the promenade-side origin with views 4 and 7; slots 2 and 4
/// use the forked-road-side origin with views 2 and 6. XYZ uses world game units
/// and yaw uses 4096 per turn. Slot 4 retains a zero companion transform.
/// Sounds and option flags are absent; slots 2 and 4 mark the security room.
/// No terminator is present. Keep this overlay loaded through endpoint lookup;
/// consumers copy a complete entry before applying it.
extern DirectionWarpEntry gAcropolisObservatoryWarpEntries[ACROPOLIS_OBSERVATORY_WARP_COUNT];

/// Foreground sprite resources for mapped views 1..8, indexed by view minus one.
///
/// Sources use pixel rectangles relative to the view centre and unscaled sorting
/// depths. Each batch list ends with `SPRITE_BATCH_END` and starts with a zero-count
/// record requesting decoded background strips. Views 1 and 5 have no source
/// sprites; all views have no clipping areas. Keep this overlay loaded during
/// packet allocation and drawing; the borrowed source and batch arrays are mutable.
extern SpriteView gAcropolisObservatorySpriteViews[ACROPOLIS_OBSERVATORY_VIEW_COUNT];

/// Camera transforms and projection distances for mapped views 1..8.
///
/// Indexed by mapped view minus one, with no terminator. Rotation uses `ONE`
/// (4096) for unity; translation is the negated origin in world units, applied
/// before rotation. Projection distance is in pixels (447 for view 1, 230 for
/// the others). Keep this overlay loaded until a queued camera request applies.
extern ViewCamera gAcropolisObservatoryViewCameras[ACROPOLIS_OBSERVATORY_VIEW_COUNT];

/// Collision policies for surface classes 0..7 in both observatory rooms.
///
/// Class 1 supplies paired walking/running footstep cues; the other classes have
/// no surface cues. Every class blocks probes, allows weapon impacts and applies
/// pushback. The directory has no terminator and borrows records from this overlay;
/// keep it loaded while collision or actor-footstep queries use those records.
extern WorldCollisionSurfaceProperties* gAcropolisObservatorySurfacePropertiesByClass[ACROPOLIS_OBSERVATORY_SURFACE_CLASS_COUNT];

/// Emits the observatory's lens flares at anchors enabled for the mapped view.
///
/// Requires a live coordinate body, loaded room anchor/mask arrays and mapped
/// view below 31 for the signed shift. Masks select views 0..15. Each tick
/// spawns one counted effect per enabled anchor, until
/// room-effect control reaches `ROOM_EFFECT_CONTROL_CANCEL_MIN`. The emitter
/// borrows its coordinate and anchor data, changes no state and remains live;
/// keep this room overlay loaded while gameplay's effect-bank callback runs.
void acropolisObservatoryLensFlareEmitterTask(Task* task);

/// Draws one frame of the observatory's additive ambient glow, then retires its effect.
///
/// Requires a live coordinate body and counted `EffectWork` in `spawnArg2.pointer`
/// from the effect spawner. The composed view position narrows to signed 16-bit
/// coordinates before projection through `GsWSMATRIX`. A negative
/// GTE FLAG suppresses drawing; otherwise the square uses a pixel half-extent
/// of 0x5D00 / max(SZ3 / 4 - 64, 16) and alternates two palettes each frame.
///
/// Requires an initialized scratch stack with room for one `EffectCentreScratch`,
/// a current depth ordering table, and room for one `POLY_FT4` in the frame arena.
/// Releases scratch and the counted work, and kills the task even when drawing
/// is suppressed. A queued packet remains live through GPU drawing.
void acropolisObservatoryAmbientGlowTask(Task* task);

/// Runs the observatory's room messages and arrival scene gates.
///
/// State 0 registers the receiver and activates the unseen room-2 encounter;
/// state 1 starts an arrival scene once for warp 3 or 4; state 2 releases the task.
/// Start with a live bodyless task in state 0. The state must remain in 0..2;
/// dispatch performs no bounds check. Keep the room overlay and gameplay
/// resources loaded through the selected handler, which may release the task.
void acropolisObservatoryRoomTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_OBSERVATORY_H
