#ifndef INCLUDE_ROOMS_DRYFIELD_WATER_TOWER_H
#define INCLUDE_ROOMS_DRYFIELD_WATER_TOWER_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Commands broadcast to the room's Desert Chasers with `ACTOR_COMMAND_MESSAGE_APPLY`.
///
/// `ActorCommand.context` names the Dryfield Water Tower stage and area.
/// Commands are stored in a halfword by the sender and a byte by the actors.
enum {
    DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_BATTLE       = 0, // Restore health and release the first placed chaser from its scripted animation
    DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_RUN          = 1, // Restore health and stage the remaining chasers for the timed mechanism run
    DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_FINAL_BATTLE = 2, // Place the remaining chasers in the arena; report a room event when none remain
    DRYFIELD_WATER_TOWER_CHASER_COMMAND_END_RUN            = 3, // Release a held player and send eligible chasers into their retreat state
    DRYFIELD_WATER_TOWER_CHASER_COMMAND_HIDE               = 9, // Hide both chasers while the mechanism's opening script runs
};

extern u16 D_dryfield_water_tower_801876A8;

extern u16 D_dryfield_water_tower_801876AA;

extern AreaVariant D_dryfield_water_tower_8018757C[13];

// dryfield_water_tower
extern WorldCollisionRoomResources D_dryfield_water_tower_801827CC[];

extern u8* D_dryfield_water_tower_801827DC[];

extern ViewCount D_dryfield_water_tower_801827E0[];

extern WorldCoordRoomLighting D_dryfield_water_tower_801827E4[];

extern DirectionWarpEntry D_dryfield_water_tower_801827EC[];

extern ViewCamera D_dryfield_water_tower_801835E8[];

extern SpriteView D_dryfield_water_tower_80186560[];

extern WorldCollisionSurfaceProperties* D_dryfield_water_tower_80187608[];

/// Updates the room's ambient-effect gate for the current mapped view each frame.
///
/// Room-effect task 0xD2 ignores its task argument. The water-tower overlay and
/// current view mapping must be live, with a mapped view index in 1..21.
/// The per-view table supplies 2 for enabled effects and 0 for disabled effects.
void dryfieldWaterTowerUpdateViewEffectGateTask(Task* unused);

void func_dryfield_water_tower_8017DDD8(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_WATER_TOWER_H
