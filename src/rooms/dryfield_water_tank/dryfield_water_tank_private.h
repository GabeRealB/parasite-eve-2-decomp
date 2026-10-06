#ifndef SRC_ROOMS_DRYFIELD_WATER_TANK_DRYFIELD_WATER_TANK_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_WATER_TANK_DRYFIELD_WATER_TANK_PRIVATE_H

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// Requests the prop scene's event script posts to the scene's driver task.
///
/// The driver carries a request out on its next frame and clears it, so each
/// one lasts a single frame.
enum {
    DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_NONE        = 0, // Nothing posted
    DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_START_SLIDE = 1, // Hide the player's model, show the prop and start its slide
    DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_SHOW_PLAYER = 2, // Switch to the room's view 3 and show the player's model again
    DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_PLAY_SOUNDS = 3, // Queue the scene's two sound events
};

extern TaskDesc D_dryfield_water_tank_80184DF4[2];

extern u16 D_dryfield_water_tank_801868CC[10];

extern Task* D_dryfield_water_tank_80188D50;

extern EvsCommand D_dryfield_water_tank_8017F114[11];

extern EvsCommand D_dryfield_water_tank_8017F21C[11];

extern TaskMessageEntry D_dryfield_water_tank_8017F324[5];

extern TaskDesc D_dryfield_water_tank_8017F34C[2];

extern ActorTransform D_dryfield_water_tank_8017FD60[2];

extern u16 D_dryfield_water_tank_8017FDA8[12];

extern EvsCommand D_dryfield_water_tank_8017FDC0[11];

extern EvsCommand D_dryfield_water_tank_8017FEC8[8];

extern TaskDesc D_dryfield_water_tank_8017FF88[2];

extern TaskDesc D_dryfield_water_tank_80180794;

extern AnimationSet gDryfieldWaterTankAnimation034BC;

extern AnimationSet gDryfieldWaterTankAnimation0377C;

extern AnimationSet gDryfieldWaterTankAnimation03A60;

extern AnimationSet gDryfieldWaterTankAnimation03CB4;

extern AnimationSet gDryfieldWaterTankAnimation04000;

extern AnimationSet gDryfieldWaterTankAnimation047BC;

extern AnimationSet gDryfieldWaterTankAnimation04AA0;

extern AnimationSet gDryfieldWaterTankAnimation04C98;

extern AnimationSet gDryfieldWaterTankAnimation04FEC;

extern AnimationSet gDryfieldWaterTankAnimation051E4;

extern AnimationSet gDryfieldWaterTankAnimation05704;

extern AnimationSet gDryfieldWaterTankAnimation059E4;

extern AnimationSet gDryfieldWaterTankAnimation05DB8;

extern AnimationSet gDryfieldWaterTankAnimation061A8;

extern AnimationSet gDryfieldWaterTankAnimation06514;

extern AnimationSet gDryfieldWaterTankAnimation06840;

extern AnimationSet gDryfieldWaterTankAnimation06C68;

extern AnimationSet gDryfieldWaterTankAnimation06F48;

extern TaskMessageEntry D_dryfield_water_tank_8017FD90[3];

/// Selects the room sprites shown before or after operating the tank mechanism.
///
/// Nonzero shows the pre-operation sprite in mapped view 8 and hides the
/// post-operation sprite in mapped view 3; zero reverses them. The caller uses
/// nonzero for mechanism states 0..2 and zero for state 3. Only the argument's
/// low byte is significant. No-op outside `GAME_STAGE_DRYFIELD`.
///
/// In that stage, the active area's loaded sprite directory must be the water
/// tank's ten-view array, with at least four batches in view 3 and two in view 8.
/// Mutates the room-owned batches without allocating or releasing resources.
void dryfieldWaterTankSetPreOperationSprites(u8 beforeOperation);

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_water_tank_8017DD20(Task*);

void func_dryfield_water_tank_8017DEA4(Task*);

void func_dryfield_water_tank_8017E194(s16);

void func_dryfield_water_tank_8017E1B4(void);

void func_dryfield_water_tank_8017EC38(u32);

void func_dryfield_water_tank_8017EC6C(Task*);

void func_dryfield_water_tank_8017ED30(Task*);

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_water_tank_8017D618(Task*);

s32 func_dryfield_water_tank_8017D7BC(Task*, s32, s32, s32);

s32 func_dryfield_water_tank_8017D7C4(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_dryfield_water_tank_8017D7EC(Task* task, s32 msgId, const void* firstArg, s32 arg3);

s32 func_dryfield_water_tank_8017D910(Task*, s32, s32, s32);

void func_dryfield_water_tank_8017D948(Task*);

void func_dryfield_water_tank_8017E0B4(Task*, s32, s32, s32);

s32 func_dryfield_water_tank_8017E174(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);

#endif // SRC_ROOMS_DRYFIELD_WATER_TANK_DRYFIELD_WATER_TANK_PRIVATE_H
