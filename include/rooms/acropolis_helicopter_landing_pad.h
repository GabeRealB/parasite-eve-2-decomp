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

void func_acropolis_helicopter_landing_pad_8017FA30(Task* arg0);

void func_acropolis_helicopter_landing_pad_80181064(Task* arg0);

void func_acropolis_helicopter_landing_pad_80180E40(Task* arg0);

void func_acropolis_helicopter_landing_pad_8017EF60(s32 unused0, s32 unused1);

void func_acropolis_helicopter_landing_pad_801802E0(Task* arg0);

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

void func_acropolis_helicopter_landing_pad_8017EB00(Task* task);

void func_acropolis_helicopter_landing_pad_801822B0(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H
