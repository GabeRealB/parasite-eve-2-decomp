#ifndef INCLUDE_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H
#define INCLUDE_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Two-step vibration cue for the landing-pad boss's lethal strike.
///
/// The binary motor stops immediately; the variable motor plays segment 0
/// once, then stops. Both STOP operands are ignored. Actor scripts borrow
/// the commands and segment until completion; keep this room overlay loaded.
extern PadScriptCmd gAcropolisHelicopterLandingPadLethalStrikeVibrationCommands[2];

/// Variable-motor segment for the lethal-strike cue: intensity 120 toward 70 over 20 frames.
///
/// Selected only by PLAY index 0. Intensities are unattenuated bytes; the
/// final stored byte is 1 and has no interpreter read. The script borrows
/// this record through playback while the landing-pad overlay remains loaded.
extern PadScriptVibrationSegment gAcropolisHelicopterLandingPadLethalStrikeVibrationSegment;

/// Actor layout variants for the Acropolis helicopter landing pad.
///
/// Indexed directly by `GameLocationKey.variant` (0..12); no terminator.
/// Slots 1 and 2 publish placement/resource pairs; all other slots are NULL pairs.
/// The room owns recipes and the stage map owns placements. Keep the map
/// loaded while actors borrow placements.
/// Keep the room loaded during loading, spawning and model-policy updates,
/// and each selected actor overlay loaded throughout its tasks' lifetime.
extern AreaVariant gAcropolisHelicopterLandingPadAreaVariants[13];

extern TmdSource gAkropolisHelicopterLandingPadAcropolisHelicopterLandingPadModel0547C;

extern TmdSource gAcropolisHelicopterLandingPadModel0A8E8;

// acropolis_helicopter_landing_pad
/// Collision resources for the landing pad's single room in Acropolis area 16.
///
/// The stage-map directory selects this array at area minus one; room 1 selects
/// its sole record. It borrows the grid, sixteen view boundaries, nine action
/// triggers and two sight occluders. Their own LAST flags terminate the lists;
/// this one-record array has no terminator. Keep the room loaded while any of
/// its mutable resources are bound or queried.
extern WorldCollisionRoomResources gAcropolisHelicopterLandingPadCollisionRoomResources[1];

/// Logical-view map directory for the landing pad's single room in area 16.
///
/// Entry `room - 1` borrows the room's identity map: logical views 1..27 select
/// the same camera/image/sprite indices. Keep the room overlay loaded while
/// the Acropolis map or view lookups borrow this directory and its map.
extern u8* gAcropolisHelicopterLandingPadViewMaps[1];

/// Reverse-view-search limits for the landing pad's single room in area 16.
///
/// Entry `room - 1` counts 27 logical map entries. It is an element count,
/// independent of the map's extra stored byte, and has no terminator.
extern ViewCount gAcropolisHelicopterLandingPadViewCounts[1];

/// Lighting descriptors for the landing pad's single room in area 16.
///
/// Entry `room - 1` borrows its mutable authored lights and uses the default
/// ambient minima for every view. The descriptor and light collection belong
/// to the loaded room overlay; borrowed pointers must not survive unloading it.
extern WorldCoordRoomLighting gAcropolisHelicopterLandingPadRoomLightingTable[1];

/// Three arrival endpoints for Acropolis area 16, indexed by `warp - 1`.
///
/// Warps 1 and 3 start in view 2 at the fire escape; warp 3 starts the player
/// in scripted mode. Warp 2 starts in view 11 by the lift. Player and companion
/// placements use whole room coordinates and 4096 angle units per turn. No
/// endpoint posts transition sounds or updates a map marker. The Acropolis map
/// borrows these complete records while the room overlay remains loaded.
extern DirectionWarpEntry gAcropolisHelicopterLandingPadWarps[3];

/// Grid-local Q12 normals, with four leading entries reserved for the encounter actor.
///
/// The No. 9 actor writes these through the active collision grid. The fifteen
/// entries remain writable and loaded while either room or actor uses them.
extern SVECTOR gAcropolisHelicopterLandingPadCollisionGridNormals[15];

/// Grid-local signed game-coordinate vertices, including sixteen encounter vertices.
///
/// The No. 9 actor rewrites the first sixteen of ninety-two entries through the
/// active grid. Keep the complete mutable pool loaded through collision use.
extern SVECTOR gAcropolisHelicopterLandingPadCollisionGridVerts[92];

/// Thirty-seven indexed room faces with four reserved leading encounter faces.
///
/// The No. 9 actor replaces the first four entries through the active grid.
/// Vertex and normal indices select this room's corresponding mutable pools;
/// the room's cell lists keep referring to these same face slots.
extern WorldCollisionGridFace gAcropolisHelicopterLandingPadCollisionGridFaces[37];

/// Sprite sources, drawing batches and clipping lists for the landing pad's 27 views.
///
/// The Acropolis map's area-16 entry borrows this array at mapped view minus one.
/// The room's identity map selects views 1..27; views 15 and 27 share their lists.
/// Batch and clipping sentinels terminate the referenced lists, not this directory.
/// All records belong to the loaded room overlay and must remain live while used.
extern SpriteView gAcropolisHelicopterLandingPadSpriteViews[27];

/// Authored camera transforms and projection distances for the landing pad's 27 views.
///
/// The Acropolis map's area-16 entry borrows this array at mapped view minus one.
/// Rotation coefficients have 12 fractional bits; translation is the negated
/// camera origin in world units, applied before rotation. Projection distances
/// are pixels. Views 15 and 27 have identical cameras. No terminator is stored;
/// keep the room loaded until any queued camera application has completed.
extern ViewCamera gAcropolisHelicopterLandingPadViewCameras[27];

/// The landing pad's collision and footstep policies indexed by surface class (0..7).
///
/// The Acropolis map publishes this directory for area 16. Classes 0, 1 and 7
/// share a silent solid policy; 2 selects the upper floor and scripted exit
/// stairs; 3 selects the lower floor. Classes 4 and 5 pass probes and suppress
/// weapon impacts while retaining pushback and floor footsteps. Class 6 is a
/// silent outer boundary that blocks probes and suppresses weapon impacts.
/// The directory, records and sound tables belong to the loaded room overlay;
/// borrowed pointers must not survive unloading it.
extern WorldCollisionSurfaceProperties* gAcropolisHelicopterLandingPadSurfacePropertiesByClass[8];

/// Draws one blue spark on the coordinate's lower local-Y side.
///
/// Borrows and composes a live coordinate chain. Endpoint 0 has local x/z in
/// [-32,31], y in [0,127]; endpoint 1 has x/z in [-32,31], y in [0,255].
/// Positive local Y points down for an upright light. Each composed endpoint
/// narrows to signed 16-bit view coordinates, then projects through `GsWSMATRIX`.
/// Only endpoint 1's GTE flags reject the line; its unbiased SZ3 / 4 selects
/// ordering. Queues one opaque `LINE_F2` with blue 255, a random green byte and
/// red at half green. Advances the LCG seven times, borrows 32 scratch bytes
/// and consumes one packet on success. Caller controls drawing while effects are
/// paused. Leaves GTE state changed; retains no pointer.
void acropolisHelicopterLandingPadDrawLowerSparkLine(GfxCoord* coord);

/// Pulses the twelve red perimeter lights and selects the view's ground-shadow shade.
///
/// The bank-6 effect task at slot 0x1D requires a live `EffectWork` in `spawnArg2`.
/// Stores brightness in its signed `scale` field, driven by the global animation
/// frame: a 64-frame triangle wave rises 0,8,...,248 and falls 254,246,...,6.
/// Disables ground shadows in mapped view 18 and selects the unmodulated
/// shadow texture in other views. Each visible light can refresh shared slots 6/7 and
/// draw twenty additive `POLY_G4` packets; room-effect control gates those draws.
/// Borrows the work and authored position tables without allocating or freeing them.
void acropolisHelicopterLandingPadPerimeterLightsTask(Task* task);

/// Animates one drifting helipad ember from the six-cell 32-pixel sprite row.
///
/// Bank-6 slot 0x5A owns zeroed counted `EffectWork` in `spawnArg2` and borrows
/// its coordinate body. `spawnArg1.value` 0 chooses small random drift; nonzero
/// chooses a larger sprite rising 24 local units per update. Only value 1 flickers
/// blue-white and can emit flash bursts; value 2 keeps the raw texture colours.
/// State 0 chooses the random size, angle, growth and frame duration, then state 1
/// renders ages 0..6*step-1 (step 1..4 for small, 2..5 for large). Growth and drift
/// occur only while effects run. Frozen effects still draw; hidden effects wait;
/// cancellation or expiry releases the work and task.
/// Projects the narrowed cached translation through `GsWSMATRIX`; accepted projections
/// require nonzero depth. Uses one 28-byte scratch block and one semitransparent
/// `POLY_FT4` per accepted projection. The room overlay and atlas must stay loaded.
void acropolisHelicopterLandingPadEmberTask(Task* task);

/// Animates one drifting helipad flare from the six-cell 40-pixel sprite row.
///
/// Bank-6 slot 0x5E owns zeroed counted `EffectWork` in `spawnArg2` and borrows
/// its coordinate body. State 0 seeds the size, angle, step 1..4 and drift, then
/// state 1 renders ages 0..6*step-1. It drifts along local negative X and fades
/// during the final seven ages (the whole lifetime when step is 1). Before that,
/// nonzero `spawnArg1.value` permits blue-white flicker and flash bursts; either
/// variant can emit embers with argument 2 minus this value (authored inputs 0/1).
/// Frozen effects still draw and consume random samples; movement and child
/// spawns require running effects. Hidden effects wait, cancellation or expiry
/// releases the work and task. Projects the narrowed cached translation through
/// `GsWSMATRIX`, requiring nonzero accepted depth; consumes a 28-byte scratch
/// block and one semitransparent `POLY_FT4`. Keep the room overlay and atlas loaded.
void acropolisHelicopterLandingPadLensFlareTask(Task* task);

/// Emits flares and embers at the shot helipad blast source under actor control.
///
/// Bank-6 slot 0x5F owns counted `EffectWork` in `spawnArg2` and borrows its
/// view-parented coordinate body. The actor selects states without automatic
/// advancement: 0 emits two flickering flares per running update and refreshes
/// transient light 4 for four frames; 1 emits two raw-colour flares per update;
/// 2 has a 1-in-16 ember chance each frame while animation-frame bit 6 is set.
/// State 3 releases work and task even while effects are paused. Other states
/// wait while effects are not running. Shares light 4 with the damaged-light
/// effect; emitted effects keep their own lifetimes. Keep the room overlay loaded.
void acropolisHelicopterLandingPadFlareEmitterTask(Task* task);

/// Queues the landing pad's fire-escape exit from a direction action.
///
/// Both action words are ignored. Requires the player, room and its overlay
/// to remain live through the asynchronous turn, descent and saved-location reload.
/// Allocation failure leaves the current room unchanged.
void acropolisHelicopterLandingPadStartExit(s32 unusedArgument, s32 unusedActionId);

/// Follows a damaged helipad light and drives its spark lines, bursts and transient lights.
///
/// Bank-6 slot 0x5B owns zeroed counted `EffectWork` in `spawnArg2` and borrows
/// its coordinate body and retained parent coordinate. The first running update
/// parents the coordinate to `EffectWork::parent` at its copied local offset.
/// Actor-selected state 0 draws random lines and refreshes light 4, then runs
/// state 1's burst logic; state 1 emits intermittent sound, impact and six pixel
/// sparks and refreshes light 5. Both lights expire after four gameplay frames.
/// Emitted tasks join this controller's teardown tree. State 2 releases work and
/// task even while effects are paused; other states wait while not running.
/// The parent chain and room overlay must remain live until teardown.
void acropolisHelicopterLandingPadDamagedLightSparksTask(Task* task);

/// Turns the player, descends three steps and reloads the resolved fire-escape room.
///
/// Bank-2 slot 15 is bodyless and requires state 0..4: resolve the destination,
/// start the turn, wait for the turn, start descent, then wait and commit.
/// Every tick selects surface 2, the landing pad's stair footstep entry; collision
/// response does not select the surface during scripted stair motion. Requires
/// a live player and room receiver and this overlay throughout the sequence.
void acropolisHelicopterLandingPadExitTask(Task* task);

/// Runs the landing pad lift's initialization, travel and teardown states.
///
/// Area object kind 0x204 uses this task for scene child 0x28. Requires an
/// attached TMD body, live scene-owned enemy bookkeeping and `Task::state`
/// in 0..2 (0 initialize, 1 update, 2 release). Initialization allocates owned
/// work and installs play-animation run requests and Euler placement. Travel
/// takes 120 ticks between Y=120 and Y=-2880; ready camera views gate drawing.
/// The room overlay must remain loaded while the task runs.
void acropolisHelicopterLandingPadLiftTask(Task* task);

/// Runs the landing pad room's initialization, encounter-phase update and teardown states.
///
/// The Acropolis map registers this for area 16. `Task::state` must be 0..2:
/// 0 installs the room message table, resets encounter gates, starts ambient
/// sound and retains the player pitch task; 1 updates encounter progress;
/// 2 kills the room task. The room overlay and its callback tables must stay
/// loaded while this task or its spawned room tasks run.
void acropolisHelicopterLandingPadRoomTask(Task* task);

/// Selects the placed pickup model's draw and buffer policy from its saved object state.
///
/// Area object kind 0xA4 requires a TMD body and live `Enemy` placement data in
/// `spawnArg2`. The place key's low byte must select object index 0..63 in the
/// current stage. Collected state 2 sets skip-auto-buffer without freeing an
/// existing buffer; other states select the flagged draw pass, clear its ordering
/// offset and allocate missing primitive buffers. Retains the unused mapped-view
/// lookup, which requires valid loaded view maps. The task keeps its state and
/// owns no additional work here.
void acropolisHelicopterLandingPadPickupModelTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H
