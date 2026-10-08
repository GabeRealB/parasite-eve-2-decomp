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

/// Posts a single-frame prop-scene request for the driver's next update.
///
/// `request` is one of `DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_*`; a later post
/// overwrites an earlier pending request. Requires the live scene driver and
/// its allocated work. Also clears an unread work halfword of unproven purpose.
void dryfieldWaterTankPostPropSceneRequest(s16 request);

/// Restores the player and final view when the prop scene is skipped.
///
/// Requires the live driver and its player task. Selects logical view 3, shows
/// the player's model and requests a view reload, then queues a fade over ten
/// audio updates of scene sound entry 2. Scene-task cleanup belongs to the skip script.
void dryfieldWaterTankSkipPropScene(void);

void func_dryfield_water_tank_8017EC38(u32);

void func_dryfield_water_tank_8017EC6C(Task*);

void func_dryfield_water_tank_8017ED30(Task*);

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_water_tank_8017D618(Task*);

/// Refuses every `ROOM_MESSAGE_USE_KEY_ITEM` request without consuming an item.
///
/// All arguments are unused; returns `ROOM_KEY_ITEM_USE_REFUSED`.
s32 dryfieldWaterTankRefuseKeyItem(Task* task, s32 messageId, s32 itemId, s32 secondArg);

/// Accepts a room-transition request unchanged, returning 1.
///
/// `ROOM_EVENT_MESSAGE_RESOLVE` borrows an initialized eight-byte request and
/// a writable reply, which may be the same object. Copies the complete record
/// even in query mode; retains neither pointer and performs no departure effects.
s32 dryfieldWaterTankResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

s32 func_dryfield_water_tank_8017D7EC(Task* task, s32 msgId, const void* firstArg, s32 arg3);

s32 func_dryfield_water_tank_8017D910(Task*, s32, s32, s32);

void func_dryfield_water_tank_8017D948(Task*);

/// Applies `ACTOR_MESSAGE_SET_MODEL_DRAW` to the scene prop's live model.
///
/// Zero `visible` sets `TMD_OBJECT_SKIP_ACTIVE_DRAW`; nonzero clears it, leaving
/// other flags intact. `messageId` and `secondArg` are unused. No reply is defined.
void dryfieldWaterTankSetPropModelDraw(Task* task, s32 messageId, s32 visible, s32 secondArg);

/// Restarts the prop's movement phase and applies the command's task state.
///
/// `ACTOR_COMMAND_MESSAGE_APPLY` borrows `command` through dispatch and reads
/// only its command word: 0 initializes, 1 idles, 2 slides. The scene driver sends
/// 2. Requires allocated prop work; resets the dust index and an unread halfword
/// of unproven purpose, retaining the settle count and current model position.
/// `messageId` and `secondArg` are unused. No reply is defined.
void dryfieldWaterTankRestartPropSlide(Task* task, s32 messageId, const ActorCommand* command, s32 secondArg);

#endif // SRC_ROOMS_DRYFIELD_WATER_TANK_DRYFIELD_WATER_TANK_PRIVATE_H
