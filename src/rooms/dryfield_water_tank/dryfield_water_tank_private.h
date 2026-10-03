#ifndef SRC_ROOMS_DRYFIELD_WATER_TANK_DRYFIELD_WATER_TANK_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_WATER_TANK_DRYFIELD_WATER_TANK_PRIVATE_H

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// One row of the message table the prop scene's model task installs in
/// `Task::msgTable`: a message id and the callback that handles it.
///
/// The row has the layout the task-message dispatcher walks, but not
/// `TaskMessageEntry`'s callback type: these handlers return nothing and take
/// only the first argument word, so `handler` holds one view per message. The
/// prop's table has a row for each of the three messages below and no
/// `TASK_MESSAGE_TABLE_END` row, so the prop may be sent no other id.
typedef struct {
    s32 messageId;                                                              // Message this row answers
    union {
        void (*place)(Task* task, s32 messageId, ActorTransform* placement);    // ACTOR_MESSAGE_PLACE: sets the model's position and yaw/pitch/roll from a borrowed transform
        void (*applyCommand)(Task* task, s32 messageId, ActorCommand* command); // ACTOR_COMMAND_MESSAGE_APPLY: restarts the slide and enters the task state `ActorCommand::command` holds
        void (*setModelDraw)(Task* task, s32 messageId, s32 visible);           // ACTOR_MESSAGE_SET_MODEL_DRAW: draws the model when nonzero, hides it when zero
    } handler;                                                                  // Callback for `messageId`, through the view its message selects
} DryfieldWaterTankMessageEntry;
STATIC_ASSERT_SIZEOF(DryfieldWaterTankMessageEntry, 8);

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

extern DryfieldWaterTankMessageEntry D_dryfield_water_tank_8017FD90[3];

/// Toggle the room's cutscene-“watched” state over two of the area's sprite
/// commands: `arg0 != 0` hides the first and shows the second by setting and
/// clearing their `SpriteBatch::hidden`, `arg0 == 0` does the opposite. No-op unless `GameSession.location.loc.stage` is 2, i.e. only for the stage
/// whose sprite table has a record for the current room.
/// `func_dryfield_water_tank_8017DB48` passes the game-flag `0x55` nibble
/// through it, one way per value.
void func_dryfield_water_tank_8017EFF4(s32 arg0);

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

s32 func_dryfield_water_tank_8017D7BC(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_dryfield_water_tank_8017D7C4(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_dryfield_water_tank_8017D7EC(Task*, s32, TaskMessageArg firstArg, TaskMessageArg);

s32 func_dryfield_water_tank_8017D910(Task*, s32, s32, TaskMessageArg);

void func_dryfield_water_tank_8017D948(Task*);

void func_dryfield_water_tank_8017E0B4(Task*, s32, s32);

void func_dryfield_water_tank_8017E174(Task*, s32, ActorCommand* msg);

#endif // SRC_ROOMS_DRYFIELD_WATER_TANK_DRYFIELD_WATER_TANK_PRIVATE_H
