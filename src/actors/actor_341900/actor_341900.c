#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "actors/actor_444000.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/actor_messages.h"

/// Work block of the overlay's sequence/event task -- the one
/// `D_actor_341900_80164208` points at.
///
/// `func_actor_341900_80162EFC` allocates it with `memCalloc(0x70, 0)`,
/// `memFillBytes`s the same 0x70 bytes over it and stores it in its own task's
/// `Task::work` slot (0x1C), then publishes
/// that task in `D_actor_341900_80164208`. The script callbacks from
/// `func_actor_341900_80163388` on reach the block that way,
/// `(Actor341900Work*)D_actor_341900_80164208->work`; the two dispatchers
/// `func_actor_341900_801628B8` / `func_actor_341900_80162AD4` are handed the
/// same task as their argument and index it identically.
///
/// `field_0` is the `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` task the overlay's messages are aimed
/// at (0x3E8 and 0x3F3), and `field_8` / `field_C` / `field_10` are child tasks
/// the senders null-check first (0x7D5 goes to `field_8`);
/// `func_actor_341900_80163488` disposes of `field_8` by killing it and clearing
/// the slot, and `func_actor_341900_801634D0` does the same for `field_C` and
/// `field_10` in turn.
///
/// `field_5C` and `field_64` are one-shot request states: a dispatcher switches
/// on the state through a jump table and clears it back to 0 on the way out, so
/// writing it runs that state once. `func_actor_341900_80163564` requests state
/// `field_5C`, `func_actor_341900_80163584` state `field_64`, and each also
/// resets the halfword beside it -- `field_5E` / `field_66` -- the step within
/// the state, which the dispatcher compares against 0 and 1 and increments.
/// `field_68` is cleared as that step advances, and `field_6C` is a 0/1 latch
/// shared by `func_actor_341900_801633F8` (sets it, then calls
/// `Gp_KillPlayerEffs`) and `func_actor_341900_80163438` (calls
/// `Gp_SpawnWeaponEff` while it is set, then clears it).
typedef struct Actor341900Work {
    /* 0x00 */ Task*          field_0; // gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)
    /* 0x04 */ Task*          field_4; // Gp_FindWorkById(session slot)->task
    /* 0x08 */ Task*          field_8;
    /* 0x0C */ Task*          field_C;
    /* 0x10 */ Task*          field_10;
    /* 0x14 */ ActorTransform field_14;
    /* 0x2C */ ActorTransform field_2C;
    /* 0x44 */ ActorTransform field_44;
    /* 0x5C */ s16            field_5C;
    /* 0x5E */ s16            field_5E;
    /* 0x60 */ byte           pad_60[0x4];
    /* 0x64 */ s16            field_64;
    /* 0x66 */ s16            field_66;
    /* 0x68 */ s16            field_68;
    /* 0x6A */ byte           pad_6A[0x2];
    /* 0x6C */ u16            field_6C;
    /* 0x6E */ byte           pad_6E[0x2];
} Actor341900Work;
STATIC_ASSERT_SIZEOF(Actor341900Work, 0x70);

/// Controller task of this overlay, published by `func_actor_341900_80162EFC`
/// and read by the sequence helpers that hang their work off its `Task::work`.
extern Task* D_actor_341900_80164208;

/// 8-byte record of `D_actor_341900_80163A98`, indexed by `Task::spawnArg1`.
/// `func_actor_341900_801625B4` copies the first three halves onto part 0's
/// `GfxCoord::coord.t` and hangs that part off entry `field_6` of the
/// spawner model's own coordinate array, so a record is a spawn offset plus the
/// bone the actor is attached to. The first three records are all zero and only
/// `field_6` is under 9 in the rest, which is what sizes a model's part array.
typedef struct Actor341900SpawnPos {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} Actor341900SpawnPos;
STATIC_ASSERT_SIZEOF(Actor341900SpawnPos, 0x8);

extern Actor341900SpawnPos D_actor_341900_80163A98[6];

/// Work block `func_actor_341900_80162330` allocates with `memMalloc(0x258, 0)`
/// and parks in its own task's opaque `Task::work` slot. `field_248` is the
/// task that spawned this actor, copied there from
/// `Task::spawnArg2`; `func_actor_341900_801625B4` walks it to the spawner's
/// model to inherit its spawn position and its colour flag.
///
/// `field_66` is the animation frame, masked to 10 bits, and
/// `func_actor_341900_80162708` acts on two of its values: at 0x12 and 0x18 it
/// reparents the actor to a freshly spawned script and clears its message
/// state, recording each in `field_230` so a frame fires once rather than
/// every tick it is current. That whole check runs behind `field_254`, which
/// is matched against `Task::state` and so gates it to the one state the
/// actor's dispatcher handles it in. `field_24C` and `field_250` are the
/// actor's second and third child tasks, refreshed every tick alongside the
/// model.
typedef struct Actor341900TaskWork {
    /* 0x000 */ byte  pad_0[0x66];
    /* 0x066 */ u16   field_66;
    /* 0x068 */ byte  pad_68[0x1C8];
    /* 0x230 */ s32   field_230;
    /* 0x234 */ byte  pad_234[0x14];
    /* 0x248 */ Task* field_248;
    /* 0x24C */ Task* field_24C;
    /* 0x250 */ Task* field_250;
    /* 0x254 */ u16   field_254;
    /* 0x256 */ byte  pad_256[0x2];
} Actor341900TaskWork;
STATIC_ASSERT_SIZEOF(Actor341900TaskWork, 0x258);

/// The same 0x258-byte block as `Actor341900TaskWork`, seen from
/// `func_actor_341900_80162330`, which fills it: the playback rig of the
/// part's model and the light/colour matrix pair the model draws with. The
/// body drives slots 1 to 7 of `rig`; each of the two children allocates the
/// same block and drives slots 0 to 3.
typedef struct Actor341900AnimWork {
    /* 0x000 */ ActorAnimRig8 rig;
    /* 0x1D4 */ MATRIX        light;
    /* 0x1F4 */ MATRIX        color;
    /* 0x214 */ s32           field_214;
    /* 0x218 */ s32           field_218;
    /* 0x21C */ s32           field_21C;
    /* 0x220 */ s32           field_220;
    /* 0x224 */ s32           field_224;
    /* 0x228 */ byte          pad_228[0x20];
    /* 0x248 */ Task*         field_248;
    /* 0x24C */ Task*         field_24C;
    /* 0x250 */ Task*         field_250;
    /* 0x254 */ u16           field_254;
    /* 0x256 */ byte          pad_256[0x2];
} Actor341900AnimWork;
STATIC_ASSERT_SIZEOF(Actor341900AnimWork, 0x258);

/// Animation command `func_actor_341900_80161FD0` copies into
/// `Actor341900AnimWork::field_214..field_224`: `field_4` is the animation id
/// and the low half of `field_C` the blend handed to `animationSeekSlotWithBlend` (0 resets
/// the slots instead).
typedef struct Actor341900AnimCmd {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u16 field_4;
    /* 0x06 */ u16 pad_6;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor341900AnimCmd;
STATIC_ASSERT_SIZEOF(Actor341900AnimCmd, 0x14);

/// Main-executable globals with no module header yet: `gPlayerStatus.weapon` is the
/// base weapon id records are numbered from, and `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` selects the
/// alternate set -- 1 means the second block, anything else the `+0x22` one.
/// Byte the other actor overlays' one-argument setters write; set to 0xC here
/// beside `gStageSceneMusicEntry`.

extern void                      func_80143490(s32 arg0);
extern PadScriptCmd              D_80144A74[2];
extern PadScriptVibrationSegment D_80144A7C[2];

/// Parameter record `func_actor_341900_801628B8` sends with message 0x3F4.
extern AnimationSet* D_actor_341900_801639A4[2];
extern AnimationSet* D_actor_341900_801639AC[3];
extern AnimationSet* D_actor_341900_801639B8[3];
extern AnimationSet* D_actor_341900_801639C4[3];
/// Animation id `func_actor_341900_80161E58` hands every slot to
/// `animationSeekSlotWithBlend`, indexed by `Actor341900AnimWork::field_218`; a negative
/// entry skips the call.
extern s16 D_actor_341900_801639D0[];
/// Placements sent to the two effect children (`field_C` / `field_10`) as
/// message 0x7D4 (states 3 and 4), and to `field_8` (states 1 and 2).
extern ActorTransform D_actor_341900_801639D8[2];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_341900_80163A38[2];
extern ActorTransform   D_actor_341900_80163A48;
extern ActorTransform   D_actor_341900_80163A60;
extern TaskMessageEntry D_actor_341900_80163A78[4];
/// Slot-3 placements and payloads sent by `func_actor_341900_801628B8`;
/// `func_actor_341900_801635A4` also warps slot 3 to the last one.
extern ActorTransform D_actor_341900_80163AC8;
extern ActorTransform D_actor_341900_80163AE0;
extern ActorTransform D_actor_341900_80163AF8;
extern ActorTransform D_actor_341900_80163B10;
extern ActorTransform D_actor_341900_80163B28;
/// Event scripts in the overlay's `.data`, handed to `func_800E8634` (which
/// forwards the first to `Task_Spawn`).
extern EvsCommand D_actor_341900_80163B48[];
extern EvsCommand D_actor_341900_80163FB0[];
extern TaskDesc   D_actor_341900_80164190[];

extern EvsSceneKey D_actor_341900_80163B40;
void               func_actor_341900_80162200(Task*);
void               func_actor_341900_801625B4(Task*);
void               func_actor_341900_80162708(Task*);
void               func_actor_341900_80162EFC(Task*);
void               func_actor_341900_80163148(Task*);
void               func_actor_341900_80163334(s16);
void               func_actor_341900_80163388(s32);
void               func_actor_341900_801633C0(s32);
void               func_actor_341900_801633F8(void);
void               func_actor_341900_80163438(void);
void               func_actor_341900_80163488(void);
void               func_actor_341900_801634D0(void);
void               func_actor_341900_80163534(void);
void               func_actor_341900_80163564(s16);
void               func_actor_341900_80163584(s16);
void               func_actor_341900_801635A4(void);
void               func_actor_341900_80163638(void);
void               func_actor_341900_80163658(void);
void               func_actor_341900_80163678(void);

s32 func_actor_341900_80161FD0(Task*, s32, Actor341900AnimCmd*, s32);
s32 func_actor_341900_8016332C(Task*, s32, s32, s32);

static AnimationPackedPose _gActor341900Animation01B5CBank1[6] = {
#include "assets/actor_341900_animation_01B5C_bank1.inc"
};

static AnimationPackedRotation _gActor341900Animation01B5CBank4[46] = {
#include "assets/actor_341900_animation_01B5C_bank4.inc"
};

static AnimationRecord _gActor341900Animation01B5CRecords[109] = {
#include "assets/actor_341900_animation_01B5C_records.inc"
};

static u16 _gActor341900Animation01B5CIndices[20] = {
#include "assets/actor_341900_animation_01B5C_indices.inc"
};

static AnimationSet _gActor341900Animation01B5C = {
    _gActor341900Animation01B5CRecords,
    _gActor341900Animation01B5CIndices,
    { NULL, _gActor341900Animation01B5CBank1, NULL, NULL, _gActor341900Animation01B5CBank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_341900_801639A4[2] = {
    &_gActor341900Animation01B5C,
    &gActor444000Animation2E198,
};

AnimationSet* D_actor_341900_801639AC[3] = {
    &gActor444000Animation20BD4,
    &gActor444000Animation21014,
    &gActor444000Animation2C240,
};

AnimationSet* D_actor_341900_801639B8[3] = {
    &gActor444000Animation20C80,
    &gActor444000Animation212D8,
    &gActor444000Animation2C2CC,
};

AnimationSet* D_actor_341900_801639C4[3] = {
    &gActor444000Animation20D2C,
    &gActor444000Animation215B4,
    &gActor444000Animation2C358,
};

s16 D_actor_341900_801639D0[4] = {
    -1,
    -1,
    -1,
    0,
};

ActorTransform D_actor_341900_801639D8[2] = {
    { { -60, 150, -2650, 0 }, { 0, 0, 0, 0 } },
    { { -60, 150, -2650, 0 }, { 0, 0, 0, 0 } },
};

ActorTransform D_actor_341900_80163A08[2] = {
    { { -60, 150, -5300, 0 }, { 0, 0, 0, 0 } },
    { { -60, 150, 0, 0 }, { 0, 0, 0, 0 } },
};

TaskMessageEntry D_actor_341900_80163A38[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetDrawMode },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
};

ActorTransform D_actor_341900_80163A48 = { { -5000, 0, -2450, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_341900_80163A60 = { { -3000, 0, -2450, 0 }, { 0, 1024, 0, 0 } };

TaskMessageEntry D_actor_341900_80163A78[4] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetDrawMode },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_341900_8016332C },
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_341900_80161FD0 },
};

Actor341900SpawnPos D_actor_341900_80163A98[6] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 89, 100, 4 },
    { 0, 0, 0, 3 },
    { 0, 1660, 200, 2 },
};

ActorTransform D_actor_341900_80163AC8 = { { 500, 0, -1000, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_341900_80163AE0 = { { 3000, 0, -1000, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_341900_80163AF8 = { { 3200, 0, -2500, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_341900_80163B10 = { { 1000, 0, -1900, 0 }, { 0, 3072, 0, 0 } };

ActorTransform D_actor_341900_80163B28 = { { 4000, 0, -1900, 0 }, { 0, 3072, 0, 0 } };

EvsSceneKey D_actor_341900_80163B40 = { 4, 19, 11 };

EvsCommand D_actor_341900_80163B48[47] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163534 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163584 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_341900_80163B40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163638 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163658 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163584 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_801633F8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163584 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_801634D0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_341900_801633C0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_341900_80163388 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163584 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_341900_801633C0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163438 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163584 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163488 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163334 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163678 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163584 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_341900_80163FB0[20] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163584 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163334 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163488 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_801634D0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163438 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_801635A4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_341900_80164190[10] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_341900_80162EFC, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_341900_80163148, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_80162708, { .model = &gActor444000Actor403200Model10824 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_801625B4, { .model = &gActor444000GluttonLegRight } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_801625B4, { .model = &gActor444000GluttonLegLeft } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_801625B4, { .model = &gActor444000Actor403200Model12884 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_801625B4, { .model = &gActor444000Actor403200Model13774 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_801625B4, { .model = &gActor444000Actor403200Model18BE4 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_80162200, { .model = &gActor444000Model1DC9C } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_80162200, { .model = &gActor444000Model1E14C } },
};

Task* D_actor_341900_80164208;

static s32         func_actor_341900_80161E58(Task* arg0, u16 arg1);
static inline void Actor341900_SetAnim(Task* task, u16 anim, u16 blend, u16 n);
static void        func_actor_341900_80162330(Task* arg0);
static void        func_actor_341900_801628B8(Task* arg0);
static void        func_actor_341900_80162AD4(Task* arg0);

/// Ticks slots `(arg1 == 8)..arg1-1` of the task's animation context (slot 0
/// is skipped for the eight-slot actor). If every one of them then has
/// `ANIMATION_SLOT_SETTLED` set, passes them the `D_actor_341900_801639D0` id and
/// returns 1; otherwise returns 0. The gotos reproduce retail's block layout.
static s32 func_actor_341900_80161E58(Task* arg0, u16 arg1)
{
    Actor341900AnimWork* work;
    Actor341900AnimWork* ctx;
    u16                  i;
    u16                  done;
    u16                  start;
    u16                  anim;
    s32                  first;

    anim  = arg1 == 8;
    start = anim;
    work  = (Actor341900AnimWork*)arg0->work;
    for (i = start; i < arg1; i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    i    = start;
    done = 1;
    for (; i < arg1; i++) {
        if (!(work->rig.slots[i].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
            goto fail;
        }
    }
check:
    if (done) {
        if (D_actor_341900_801639D0[work->field_218] >= 0) {
            anim  = D_actor_341900_801639D0[work->field_218];
            ctx   = (Actor341900AnimWork*)arg0->work;
            first = arg1 == 8;
            goto loop;
        fail:
            done = 0;
            goto check;
        loop:
            for (i = first; i < arg1; i++) {
                animationSeekSlotWithBlend(&ctx->rig.anim, i, anim, 0, 10);
            }
        }
        return 1;
    }
    return 0;
}

/// Points `n` slots of a task's animation context at `anim`, skipping slot 0
/// on the eight-slot actor: a zero `blend` resets each slot, otherwise
/// `animationSeekSlotWithBlend` blends into it.
static inline void Actor341900_SetAnim(Task* task, u16 anim, u16 blend, u16 n)
{
    Actor341900AnimWork* ctx;
    u16                  i;

    ctx = (Actor341900AnimWork*)task->work;
    if (blend == 0) {
        for (i = n == 8; i < n; i++) {
            ctx->rig.slots[i].rate = ANIMATION_RATE_ONE;
            animationResetSlot(&ctx->rig.anim, i, anim);
        }
    } else {
        for (i = n == 8; i < n; i++) {
            animationSeekSlotWithBlend(&ctx->rig.anim, i, anim, 0, blend);
        }
    }
}

/// Records an animation command in the work block and applies it to the
/// actor and both of its child tasks.
s32 func_actor_341900_80161FD0(Task* arg0, s32 arg1, Actor341900AnimCmd* cmd, s32 arg3)
{
    Actor341900AnimWork* work;

    work            = (Actor341900AnimWork*)arg0->work;
    work->field_214 = cmd->field_0;
    work->field_218 = work->field_254 = cmd->field_4;
    work->field_21C                   = cmd->field_8;
    work->field_220                   = cmd->field_C;
    work->field_224                   = cmd->field_10;
    Actor341900_SetAnim(arg0, cmd->field_4, cmd->field_C, 8);
    Actor341900_SetAnim(work->field_24C, cmd->field_4, cmd->field_C, 4);
    Actor341900_SetAnim(work->field_250, cmd->field_4, cmd->field_C, 4);
}

/// Turns the model's world translation into the light/colour matrix pair the
/// actor draws with, allocating that pair on the first frame.
void func_actor_341900_80162200(Task* arg0)
{
    TmdObject*    extra;
    TmdObject*    mdl;
    ActorLitWork* mtx;
    VECTOR        pos;

    if (arg0->state == 0) {
        extra      = arg0->extra.tmd;
        mtx        = memMalloc(sizeof(*mtx), false);
        arg0->work = mtx;
        if (mtx == NULL) {
            taskKill(arg0);
        } else {
            memFillBytes(mtx, 0, sizeof(*mtx));
            mtx->field_40                   = (Task*)arg0->spawnArg2.pointer;
            extra->flags                    = 0;
            arg0->extra.tmd->coords->parent = &gGfxViewCoord;
            extra->lightMtx                 = &mtx->light;
            extra->colorMtx                 = &mtx->color;
            extra->otOffset                 = 0x1F;
            arg0->msgTable                  = D_actor_341900_80163A38;
            taskReparent(mtx->field_40, arg0);
        }
        arg0->state++;
    }

    mdl    = arg0->extra.tmd;
    pos.vx = arg0->extra.tmd->coords->workm.t[0];
    pos.vy = arg0->extra.tmd->coords->workm.t[1];
    pos.vz = arg0->extra.tmd->coords->workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

/// Shared first tick of the actor's three parts, selected by `spawnArg1`:
/// allocates and clears the `Actor341900AnimWork` block, binds its matrices to
/// the model, applies the area's tpage/clut, sets up the part's animation
/// slots (eight for the body, four for each of the two children, which also
/// register themselves with the spawner) and reparents the spawner to it.
static void func_actor_341900_80162330(Task* arg0)
{
    TmdObject*           extra;
    Actor341900AnimWork* work;
    Actor341900AnimWork* ctx;
    Actor341900AnimWork* w;
    AreaPlacement*       rec;
    u16                  i;

    extra      = arg0->extra.tmd;
    work       = memMalloc(sizeof(*work), false);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    w = work;
    memFillBytes(w, 0, sizeof(*w));
    w->field_248    = (Task*)arg0->spawnArg2.pointer;
    extra->lightMtx = &w->light;
    extra->colorMtx = &w->color;
    arg0->msgTable  = D_actor_341900_80163A78;
    rec             = (Gp_GetNestedAreaRec(&gGameSession->location.loc))->placements;
    for (; rec->entryId != AREA_PLACEMENT_END; rec++) {
        if (rec->entryId == 0x20) {
            break;
        }
    }
    Gp_SetTmdBytes(extra, rec->texturePageOffset, rec->clutRowOffset);
    switch (arg0->spawnArg1.value) {
        case 0:
            animationInitContext(&w->rig.anim, D_actor_341900_801639AC, extra, w->rig.poses, w->rig.slots);
            ctx = (Actor341900AnimWork*)arg0->work;
            for (i = 1; i < 8; i++) {
                ctx->rig.slots[i].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&ctx->rig.anim, i, 0);
            }
            break;
            /* The empty loop's notes before `case 1:` make reorg predict the
             * dispatch branch taken and fill its delay slot from that arm. */
            do {
            } while (0);
        case 1:
            ((Actor341900AnimWork*)w->field_248->work)->field_24C = arg0;
            animationInitContext(&w->rig.anim, D_actor_341900_801639B8, extra, w->rig.poses, w->rig.slots);
            ctx = (Actor341900AnimWork*)arg0->work;
            for (i = 0; i < 4; i++) {
                ctx->rig.slots[i].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&ctx->rig.anim, i, 0);
            }
            break;
        case 2:
            ((Actor341900AnimWork*)w->field_248->work)->field_250 = arg0;
            animationInitContext(&w->rig.anim, D_actor_341900_801639C4, extra, w->rig.poses, w->rig.slots);
            ctx = (Actor341900AnimWork*)arg0->work;
            for (i = 0; i < 4; i++) {
                ctx->rig.slots[i].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&ctx->rig.anim, i, 0);
            }
            break;
    }
    taskReparent(w->field_248, arg0);
}

/// Attaches the actor to the bone its spawn record names, copies that record's
/// offset onto the part's coordinate, inherits the spawner's colour flag and
/// pushes the part's translation through the draw matrix.
void func_actor_341900_801625B4(Task* arg0)
{
    Actor341900TaskWork* work = (Actor341900TaskWork*)arg0->work;
    TmdObject*           extra;
    TmdObject*           mdl;
    GfxCoord*            coord;
    VECTOR               pos;

    if (arg0->state == 0) {
        func_actor_341900_80162330(arg0);
        work = (Actor341900TaskWork*)arg0->work;

        extra               = arg0->extra.tmd;
        coord               = extra->coords;
        coord->parent       = &(work->field_248)->extra.tmd->coords[D_actor_341900_80163A98[arg0->spawnArg1.value].field_6];
        coord->coord.t[0]   = D_actor_341900_80163A98[arg0->spawnArg1.value].field_0;
        coord->coord.t[1]   = D_actor_341900_80163A98[arg0->spawnArg1.value].field_2;
        coord->coord.t[2]   = D_actor_341900_80163A98[arg0->spawnArg1.value].field_4;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->state++;
    }

    arg0->extra.tmd->flags =
        (work->field_248)->extra.tmd->flags;

    mdl    = arg0->extra.tmd;
    pos.vx = arg0->extra.tmd->coords[1].workm.t[0];
    pos.vy = arg0->extra.tmd->coords[1].workm.t[1];
    pos.vz = arg0->extra.tmd->coords[1].workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

/// Per-state body of the actor task. State 0 publishes the part's draw
/// matrix, state 1 watches the work block's frame counter for the two frames
/// that respawn the actor's script, and every state but 0 then refreshes the
/// three child tasks and pushes the translation of the model's second
/// coordinate through the draw matrix.
void func_actor_341900_80162708(Task* arg0)
{
    Actor341900TaskWork* work;
    TmdObject*           mdl;
    VECTOR               pos;
    s32                  frame;

    work = (Actor341900TaskWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            func_actor_341900_80162330(arg0);
            arg0->extra.tmd->coords->parent = &gGfxViewCoord;
            arg0->state++;
            return;
        case 1:
            if (work->field_254 == arg0->state) {
                frame = work->field_66 & 0x3FF;
                if ((frame == 0x12) && (work->field_230 != frame)) {
                    taskReparent(arg0,
                                 Gp_SpawnScript18(D_80144A74, D_80144A7C));
                    func_80143490(3);
                }
                frame = work->field_66 & 0x3FF;
                if ((frame == 0x18) && (work->field_230 != frame)) {
                    taskReparent(arg0,
                                 Gp_SpawnScript18(D_80144A74, D_80144A7C));
                    func_80143490(3);
                }
                work->field_230 = work->field_66 & 0x3FF;
            }
            work = (Actor341900TaskWork*)arg0->work;
            break;
    }

    work = (Actor341900TaskWork*)arg0->work;
    func_actor_341900_80161E58(arg0, 8);
    func_actor_341900_80161E58(work->field_24C, 4);
    func_actor_341900_80161E58(work->field_250, 4);

    mdl    = arg0->extra.tmd;
    pos.vx = arg0->extra.tmd->coords[1].workm.t[0];
    pos.vy = arg0->extra.tmd->coords[1].workm.t[1];
    pos.vz = arg0->extra.tmd->coords[1].workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

/// Runs the one-shot request in `Actor341900Work::field_5C` against the slot-3
/// task after pinging it with message 0x3ED, then clears the request. States 1
/// and 5 install an animation set (message 0x3E8) around a placement (0x3E9),
/// 2 sends 0x3F2, 3 and 4 send 0x3F4 and 6 is a bare placement.
static void func_actor_341900_801628B8(Task* arg0)
{
    Actor341900Work*     work;
    Actor341900Work*     w;
    AnimationPlayRequest msg;

    work = (Actor341900Work*)arg0->work;
    if (work->field_0 != NULL) {
        taskMessageDispatch(work->field_0, ANIMATION_MESSAGE_IS_PLAYING, 0, 0);
    }
    switch ((u16)work->field_5C) {
        case 0:
            break;
        case 1:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_0, 0x3E9, &D_actor_341900_80163AC8, 0);
            {
                s32 weaponId;
                s32 anim;

                weaponId                 = gPlayerStatus.weapon;
                anim                     = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                msg.source.index         = anim;
                msg.animationId          = 1;
                msg.blend                = ANIMATION_BLEND_RESET;
                msg.blendFrames          = 0;
                msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &msg, 0);
            }
            break;
        case 2:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_0, 0x3F2, &D_actor_341900_80163AE0, 0);
            break;
        case 3:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_0, 0x3E9, &D_actor_341900_80163AF8, 0);
            w = (Actor341900Work*)arg0->work;
            if (w->field_0 != NULL) {
                msg.source.sets          = D_actor_341900_801639A4;
                msg.animationId          = 0;
                msg.blend                = ANIMATION_BLEND_RESET;
                msg.blendFrames          = 0;
                msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(w->field_0, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            }
            break;
        case 4:
            w = (Actor341900Work*)arg0->work;
            if (w->field_0 != NULL) {
                msg.source.sets          = D_actor_341900_801639A4;
                msg.animationId          = 1;
                msg.blend                = ANIMATION_BLEND_INTERPOLATE;
                msg.blendFrames          = 10;
                msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(w->field_0, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            }
            break;
        case 5: {
            s32 weaponId;
            s32 anim;

            weaponId                 = gPlayerStatus.weapon;
            anim                     = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.source.index         = anim;
            msg.animationId          = 9;
            msg.blend                = ANIMATION_BLEND_RESET;
            msg.blendFrames          = 0;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &msg, 0);
        }
            TASK_MESSAGE_DISPATCH_POINTER(work->field_0, 0x3E9, &D_actor_341900_80163B10, 0);
            break;
        case 6:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_0, 0x3E9, &D_actor_341900_80163B28, 0);
            break;
    }
    work->field_5C = 0;
}

/// Runs the one-shot request in `Actor341900Work::field_64`, stepping through
/// `field_66`. States 1 and 2 hand `field_8` an animation (0x7D3) and a
/// placement (0x7D4); state 1 then slides it along x once `field_68` reaches
/// 0x10, state 2 spawns four effects on its third coordinate after 0x3C ticks.
/// State 3 places both effect children and sends `field_4` 0x7D5; state 4
/// places them and moves them apart along z every tick.
static void func_actor_341900_80162AD4(Task* arg0)
{
    Actor341900Work*     work;
    AnimationPlayRequest msg;
    AnimationPlayRequest msg2;
    SVECTOR              ofs;

    work = (Actor341900Work*)arg0->work;
    switch ((u16)work->field_64) {
        case 1:
            switch ((u16)work->field_66) {
                case 0:
                    msg.animationId  = 1;
                    msg.blend        = ANIMATION_BLEND_INTERPOLATE;
                    msg.source.index = 0;
                    msg.blendFrames  = 10;
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_8, 0x7D3, &msg, 0);
                    work->field_14.pos.vx = D_actor_341900_80163A48.pos.vx;
                    work->field_14.pos.vy = D_actor_341900_80163A48.pos.vy;
                    work->field_14.pos.vz = D_actor_341900_80163A48.pos.vz;
                    work->field_14.rot.vx = D_actor_341900_80163A48.rot.vx;
                    work->field_14.rot.vy = D_actor_341900_80163A48.rot.vy;
                    work->field_14.rot.vz = D_actor_341900_80163A48.rot.vz;
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_8, 0x7D4, &work->field_14, 0);
                    work->field_68 = 0;
                    work->field_66++;
                    break;
                case 1:
                    if ((u16)work->field_68 >= 0x10) {
                        work->field_14.pos.vx += 0x1E;
                        TASK_MESSAGE_DISPATCH_POINTER(work->field_8, 0x7D4, &work->field_14, 0);
                    } else {
                        work->field_68++;
                    }
                    break;
            }
            break;
        case 2:
            switch ((u16)work->field_66) {
                case 0:
                    msg2.animationId  = 2;
                    msg2.blend        = ANIMATION_BLEND_INTERPOLATE;
                    msg2.source.index = 0;
                    msg2.blendFrames  = 10;
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_8, 0x7D3, &msg2, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_8, 0x7D4, &D_actor_341900_80163A60, 0);
                    work->field_68 = 0;
                    work->field_66++;
                    break;
                case 1:
                    work->field_68++;
                    if ((u16)work->field_68 > 0x3C) {
                        ofs.vx = 0;
                        ofs.vy = -0x64;
                        ofs.vz = 0x1194;
                        Gp_SpawnEff(EFFECT_196, &work->field_8->extra.tmd->coords[2], 0x04402800, &ofs);
                        ofs.vx = 0xC8;
                        ofs.vy = 0xC8;
                        ofs.vz = 0x1194;
                        Gp_SpawnEff(EFFECT_196, &work->field_8->extra.tmd->coords[2], 0x04402800, &ofs);
                        ofs.vx = -0xC8;
                        ofs.vy = 0xC8;
                        ofs.vz = 0x1194;
                        Gp_SpawnEff(EFFECT_196, &work->field_8->extra.tmd->coords[2], 0x04402800, &ofs);
                        ofs.vx = 0;
                        ofs.vy = 0xC8;
                        ofs.vz = 0x1194;
                        Gp_SpawnEff(EFFECT_196, &work->field_8->extra.tmd->coords[2], 0x04402800, &ofs);
                        work->field_64 = 0;
                    }
                    break;
            }
            break;
        case 3:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_C, 0x7D4, &D_actor_341900_801639D8[0], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->field_10, 0x7D4, &D_actor_341900_801639D8[1], 0);
            taskMessageDispatch(work->field_4, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            work->field_64 = 0;
            break;
        case 4:
            switch ((u16)work->field_66) {
                case 0:
                    work->field_2C.pos.vx = D_actor_341900_801639D8[0].pos.vx;
                    work->field_2C.pos.vy = D_actor_341900_801639D8[0].pos.vy;
                    work->field_2C.pos.vz = D_actor_341900_801639D8[0].pos.vz;
                    work->field_2C.rot.vx = D_actor_341900_801639D8[0].rot.vx;
                    work->field_2C.rot.vy = D_actor_341900_801639D8[0].rot.vy;
                    work->field_2C.rot.vz = D_actor_341900_801639D8[0].rot.vz;
                    work->field_44.pos.vx = D_actor_341900_801639D8[0].pos.vx;
                    work->field_44.pos.vy = D_actor_341900_801639D8[0].pos.vy;
                    work->field_44.pos.vz = D_actor_341900_801639D8[0].pos.vz;
                    work->field_44.rot.vx = D_actor_341900_801639D8[0].rot.vx;
                    work->field_44.rot.vy = D_actor_341900_801639D8[0].rot.vy;
                    work->field_44.rot.vz = D_actor_341900_801639D8[0].rot.vz;
                    work->field_66++;
                case 1:
                    work->field_2C.pos.vz -= 0x14;
                    work->field_44.pos.vz += 0x14;
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_C, 0x7D4, &work->field_2C, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_10, 0x7D4, &work->field_44, 0);
                    break;
            }
            break;
        case 0:
        default:
            work->field_64 = 0;
            break;
    }
}

/// Controller task of the overlay's script sequence, the one published in
/// `D_actor_341900_80164208`. State 0 clears and publishes the work block,
/// points it at the slot-3 task and at the work object of the current session
/// id, hands that id to slot 4 as message 0x7DA, spawns the five child script
/// tasks (table entries 3..7, spawn arguments 1..5) under `field_8` and the
/// two effect actors (entries 8 and 9) under the task itself, then sets the
/// two `GameSession.flowFlags` flags that suppress the bank-load spawn of the
/// ending and area-enter tasks. State 1 arms the stage-3 sound byte and spawns
/// the two blob tasks. State 2 waits for `GameSession.eventState` to clear -- it
/// sets game flag nibble 0x11D and kills the task when it does -- and
/// otherwise runs the two child dispatchers.
void func_actor_341900_80162EFC(Task* arg0)
{
    ActorCommand     request;
    Actor341900Work* work;
    Actor341900Work* seqWork;
    u8               sessionIdLo;
    s32              temp_a2;
    u16              var_s0;

    switch (arg0->state) {
        case 0:
            work       = memCalloc(0x70U, false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                memFillBytes(work, 0U, sizeof(*work));
                work->field_0           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_actor_341900_80164208 = arg0;
                work->field_4           = Gp_FindWorkById(
                                    gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8))
                                    ->task;
            }
            request.context.loc.stage = gGameSession->location.loc.stage;
            sessionIdLo               = gGameSession->location.loc.area;
            request.command           = 0;
            request.context.loc.area  = sessionIdLo;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &request, ACTOR_COMMAND_MESSAGE_APPLY);
            seqWork          = (Actor341900Work*)arg0->work;
            seqWork->field_8 = Task_SpawnFromTable(D_actor_341900_80164190, 2, 0, arg0);
            for (var_s0 = 0; (u32)(var_s0 & 0xFFFF) < 5U; var_s0++) {
                temp_a2 = var_s0 & 0xFFFF;
                Task_SpawnFromTable(D_actor_341900_80164190, temp_a2 + 3, temp_a2 + 1, seqWork->field_8);
            }
            seqWork->field_C         = Task_SpawnFromTable(D_actor_341900_80164190, 8, 0, arg0);
            seqWork->field_10        = Task_SpawnFromTable(D_actor_341900_80164190, 9, 0, arg0);
            gGameSession->flowFlags |= (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
            goto next;
        case 1:
            gStageSceneMusicEntry                               = 4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0xC;
            func_800E8634(D_actor_341900_80163B48, 0, D_actor_341900_80163FB0);
        next:
            arg0->state += 1;
            return;
        case 2:
            if (gGameSession->eventState == 0) {
                GameFlag_SetNibble(GAME_FLAG_11D, 2);
                Task_RequestKill(arg0, 0);
                return;
            }
            func_actor_341900_801628B8(arg0);
            func_actor_341900_80162AD4(arg0);
            return;
    }
}

/// Fade task, entry 1 of the overlay's task table: its first tick allocates
/// the channel block and seeds every channel at 0xFF; each tick then draws the
/// full-screen fade overlay and steps the channels down by `spawnArg1`,
/// killing the task once `r` has gone negative.
void func_actor_341900_80163148(Task* arg0)
{
    ScreenFadeWork* fade;
    ScreenFadeWork* alloc;

    fade = arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = memCalloc(8, 0);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, GPU_BLEND_SUBTRACT);
            fade->r -= (u16)arg0->spawnArg1.value;
            fade->g -= (u16)arg0->spawnArg1.value;
            fade->b -= (u16)arg0->spawnArg1.value;
            if (fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

#include "../../shared/actor_messages_draw_mode.inc.c"

#include "../../shared/actor_messages_place_ypr.inc.c"

/// Message 0x7DB handler of the actor's second message table; ignores it.
s32 func_actor_341900_8016332C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

/// Script callback: sends message 0x7DA to the slot-4 task, tagged with the
/// current session's two id bytes and the script's selector, asking for the
/// 0x7DB reply.
void func_actor_341900_80163334(s16 arg0)
{
    ActorCommand msg;

    msg.context.loc.stage = gGameSession->location.loc.stage;
    msg.context.loc.area  = gGameSession->location.loc.area;
    msg.command           = arg0;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
}

void func_actor_341900_80163388(s32 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    taskMessageDispatch(work->field_8, ACTOR_MESSAGE_SET_MODEL_DRAW, arg0, 0);
}

void func_actor_341900_801633C0(s32 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    taskMessageDispatch(work->field_0, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, arg0, 0);
}

void func_actor_341900_801633F8(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    if (work->field_6C == 0) {
        work->field_6C = 1;
        Gp_KillPlayerEffs();
    }
}

void func_actor_341900_80163438(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    if (work->field_6C != 0) {
        Gp_SpawnWeaponEff();
        work->field_6C = 0;
        Gp_MsgPlayerWeapon(0);
    }
}

void func_actor_341900_80163488(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    if (work->field_8 != NULL) {
        taskKill(work->field_8);
        work->field_8 = NULL;
    }
}

void func_actor_341900_801634D0(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    if (work->field_C != NULL) {
        taskKill(work->field_C);
        work->field_C = NULL;
    }
    if (work->field_10 != NULL) {
        taskKill(work->field_10);
        work->field_10 = NULL;
    }
}

void func_actor_341900_80163534(void)
{
    Task_SpawnFromTable(D_actor_341900_80164190, 1, 9, 0);
}

void func_actor_341900_80163564(s16 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    work->field_5C = arg0;
    work->field_5E = 0;
}

void func_actor_341900_80163584(s16 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    work->field_64 = arg0;
    work->field_66 = 0;
}

/// Installs one animation set on slot 3 (message 0x3E8) and then warps it to
/// the overlay's fixed placement (message 0x3E9), cancelling any pending CD
/// command replacement on the way out. The set is `gPlayerStatus.weapon + 1` for the
/// alternate weapon block and `gPlayerStatus.weapon + 0x22` for the base one; its
/// `field_4` is 9, the rest of the frame is zero.
void func_actor_341900_801635A4(void)
{
    Actor341900Work*     work;
    AnimationPlayRequest msg;
    s32                  weaponId;
    s32                  anim;

    work                     = (Actor341900Work*)D_actor_341900_80164208->work;
    weaponId                 = gPlayerStatus.weapon;
    anim                     = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.source.index         = anim;
    msg.animationId          = 9;
    msg.blend                = ANIMATION_BLEND_RESET;
    msg.blendFrames          = 0;
    msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(work->field_0, ANIMATION_MESSAGE_PLAY, &msg, 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->field_0, 0x3E9, &D_actor_341900_80163B28, 0);
    CdCmd_CancelReplaceAndActivate();
}

/// Script callback: queues the replacement overlay load.
void func_actor_341900_80163638(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Script callback: queues the overlay load.
void func_actor_341900_80163658(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Script callback: restores the stream random state, then cancels the
/// pending overlay replacement and activates the loaded one.
void func_actor_341900_80163678(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}
