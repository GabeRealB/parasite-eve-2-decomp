#ifndef INCLUDE_ROOMS_NEO_ARK_SUBMARINE_TUNNEL_H
#define INCLUDE_ROOMS_NEO_ARK_SUBMARINE_TUNNEL_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_neo_ark_submarine_tunnel_80181F94[4];

extern s16 D_neo_ark_submarine_tunnel_80181E90;

extern TaskDesc D_neo_ark_submarine_tunnel_801810E4;

extern AreaVariant D_neo_ark_submarine_tunnel_80187470[13];

// neo_ark_submarine_tunnel
extern WorldCoordRoomLighting D_neo_ark_submarine_tunnel_80181E00[];

extern WorldCollisionRoomResources D_neo_ark_submarine_tunnel_80181E08[];

extern u8* D_neo_ark_submarine_tunnel_80181E18[];

extern ViewCount D_neo_ark_submarine_tunnel_80181E1C[];

extern DirectionWarpEntry D_neo_ark_submarine_tunnel_80181E20[];

extern ViewCamera D_neo_ark_submarine_tunnel_80182500[];

extern SpriteView D_neo_ark_submarine_tunnel_80186B78[];

extern WorldCollisionSurfaceProperties* D_neo_ark_submarine_tunnel_801878EC[];

void func_neo_ark_submarine_tunnel_8017F4DC(Task* arg0);

/// Runs the tunnel's animated spark along a step fixed from a target's initial position.
///
/// Requires a composed coordinate body and a counted, zero-initialized
/// `EffectWork` owned through `spawnArg2.pointer`. `spawnArg1.pointer` borrows
/// a target `GfxCoord` until the first active tick, when both cached matrices
/// must be current in the same view space. That tick derives a parent-space
/// step of 204/4096 of the initial separation, with signed 16-bit intermediate
/// components; later ticks move by that step without resampling the target.
/// Drawing occurs on odd ages after initialization, and age 20 releases the
/// work and task. A live `gRoomEffectState` pauses at nonzero effect control
/// and cancels at four or above. The tunnel overlay must remain loaded.
void neoArkSubmarineTunnelRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs the tunnel's expanding orange disc, layered glow and fading outer ring.
///
/// Requires a coordinate body, a live `gRoomEffectState` and a counted,
/// zero-initialized `EffectWork` owned through `spawnArg2.pointer`. Active ticks
/// grow the glow half-extent by 16 world units and refresh a transient orange
/// light. The ring fades first, then the central brightness falls until the
/// work and task are released. Nonzero effect control pauses the task; four
/// or above cancels it. The tunnel overlay must remain loaded.
void neoArkSubmarineTunnelRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Selects the tunnel's glow-disc, flying-spark and projectile-burst effects once.
///
/// Bank-6 slot 0x152 installs the three room effect IDs in state 0 and advances
/// to state 1. Later ticks leave them unchanged and keep the task alive. The
/// tunnel overlay must stay loaded while its selected effects can spawn or run.
void neoArkSubmarineTunnelConfigureEffectsTask(Task* task);

/// Runs the Neo Ark Submarine Tunnel room controller for one tick.
///
/// `task` must be live with state 0..2 and this room overlay loaded.
/// Initialize room messages and ambience, remain idle between messages, then kill.
/// State 0 registers the borrowed task in `GAME_TASK_SLOT_ROOM`; state 2
/// requests teardown. The controller allocates no work or body of its own.
void neoArkSubmarineTunnelRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SUBMARINE_TUNNEL_H
