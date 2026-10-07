#ifndef INCLUDE_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H
#define INCLUDE_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

extern PadScriptCmd D_acropolis_helicopter_landing_pad_80187D34[2];

extern PadScriptVibrationSegment D_acropolis_helicopter_landing_pad_80187D3C;

extern AreaVariant D_acropolis_helicopter_landing_pad_801861E8[13];

extern TmdSource gAcropolisHelicopterLandingPadModel0547C;

extern TmdSource gAcropolisHelicopterLandingPadModel0A8E8;

// acropolis_helicopter_landing_pad
extern WorldCollisionRoomResources D_acropolis_helicopter_landing_pad_80184F10[];

extern u8* D_acropolis_helicopter_landing_pad_80184F3C[];

extern ViewCount D_acropolis_helicopter_landing_pad_80184F40[];

extern WorldCoordRoomLighting D_acropolis_helicopter_landing_pad_80184F44[];

extern DirectionWarpEntry D_acropolis_helicopter_landing_pad_80184F4C[];

extern SpriteView D_acropolis_helicopter_landing_pad_80187824[];

extern ViewCamera D_acropolis_helicopter_landing_pad_80187968[];

extern WorldCollisionSurfaceProperties* D_acropolis_helicopter_landing_pad_80187DC8[];

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

void func_acropolis_helicopter_landing_pad_8017EF60(s32 unused0, s32 unused1);

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

void func_acropolis_helicopter_landing_pad_8017EF8C(Task* arg0);

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
