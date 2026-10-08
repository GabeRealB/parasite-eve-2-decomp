#ifndef INCLUDE_ROOMS_ACROPOLIS_SQUARE_H
#define INCLUDE_ROOMS_ACROPOLIS_SQUARE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// The copied room-event handler uses this table only in stage 1, area 1.
extern AreaApplyRec D_acropolis_square_80188888[4];

extern AreaVariant D_acropolis_square_80185E50[3];

// acropolis_square
extern WorldCollisionRoomResources D_acropolis_square_80183B9C[];

extern u8* D_acropolis_square_80183BAC[];

extern ViewCount D_acropolis_square_80183BB0[];

extern WorldCoordRoomLighting D_acropolis_square_80183BB4[];

extern DirectionWarpEntry D_acropolis_square_80183BBC[];

extern SpriteView D_acropolis_square_8018857C[];

extern ViewCamera D_acropolis_square_80188630[];

extern WorldCollisionSurfaceProperties* D_acropolis_square_80188868[];

s32 func_acropolis_square_80182360(s32 unused);

/// Registers the square's room effects and emits view-dependent beacon glows.
///
/// Requires a counted effect with a single-coordinate body and zeroed
/// `EffectWork` in `spawnArg2.pointer`; its exit callback owns that allocation.
/// State 0 publishes the effect receiver, resets beacon
/// mode and spawns floor/plane player reflections. State 1 emits glows in
/// logical views 4, 6, 7, 10, 14 and 9. Views must be valid before the mask
/// shift. Mode 1 speeds/enlarges the glows and adds streaks in the front views;
/// full signed-word modes are retained. Work and overlay resources must remain
/// live while the task and its spawned effects use them.
void acropolisSquareRoomEffectTask(Task* task);

/// Draws one frame of the square's pulsing red or cyan beacon glow, then ends the effect.
///
/// Bank-6 slot 0x47 requires a live coordinate body and a counted room effect.
/// `spawnArg1.value` packs the pulse phase advance per animation frame in bits
/// 0..7, radius scale in bits 8..15 and color in bit 16 (0 red, 1 cyan).
/// Bit 31 selects a round fan with rays; otherwise the glow is a diamond,
/// with crossing streaks when bit 28 is set. Other bits are ignored.
/// The 256-step triangular pulse gives center intensity 0..254.
///
/// Composed world coordinates narrow to signed 16-bit before projection through
/// `GsWSMATRIX`. Only SZ3 / 4 depths at least 17 draw; GTE FLAG is not tested.
/// The word-aligned packet arena must hold up to twenty `POLY_G4` packets or
/// two `POLY_G4` plus two `LINE_G3`, and one blend-mode packet per primitive.
/// Packets remain live through GPU drawing; scratch storage is released here.
/// Every call frees `spawnArg2.pointer` and tears down the task, even when
/// nothing is drawn. That pointer must be NULL or owned primary-heap storage,
/// distinct from `Task::work`, with nested resources already released.
/// Keep this overlay loaded through the callback.
void acropolisSquareBeaconGlowTask(Task* task);

/// Updates the square telephone's save menu and optional play statistics.
///
/// `spawnArg2.pointer` borrows the live UI object owned by this task. Save,
/// notice and statistics children must stay linked until their answers are
/// consumed. Requires this overlay and its telephone UI resources to stay loaded.
void acropolisSquareTelephoneMenuTask(Task* task);

/// Initializes and updates the player's floor or mirror-plane reflection in the square.
///
/// State must be 0 (start bodyless) or 1 (initialized model and work).
/// `spawnArg1.value` selects the floor (0) or room mirror plane (1).
/// Requires a live player with a TMD body. The task owns its cloned model and
/// work, borrows the player's geometry, and becomes a child of the player.
/// Keep this overlay and the player live while the reflection and its
/// attachment reflections run.
void acropolisSquarePlayerReflectionTask(Task* reflectionTask);

/// Registers the square's room task and checks its arrival cutscene each tick.
///
/// State 0 installs message handling and publishes `GAME_TASK_SLOT_ROOM`;
/// state 1 starts the warp-7 event once per overlay load; state 2 kills the task.
/// Only states 0..2 are valid. The task has no body or work requirements.
/// The Akropolis map UI spawns it for the square. Keep this overlay loaded
/// while the task, its message handlers and its event scripts can run.
void acropolisSquareRoomTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_SQUARE_H
