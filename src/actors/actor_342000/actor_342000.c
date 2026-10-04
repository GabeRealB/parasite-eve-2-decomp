#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "actors/actor_444000.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_garbage_incinerator.h"
#include "../../shared/actor_messages.h"

/// Per-instance work block for the overlay's model actor.
///
/// `func_actor_342000_80162158` allocates it with `memMalloc(0x2AC, 0)`,
/// `memFillBytes`s it to zero over the same 0x2AC bytes and stores it in the
/// `Task::work` slot (0x1C), so the size below is the allocation, not a
/// guess: the actor reuses that pointer field for its own work block. Reach it
/// with `(Actor342000Work*)task->work`.
///
/// `field_2A4` is the coordinate node the actor's model is re-parented to:
/// `func_actor_342000_80162158` seeds it with `&gGfxViewCoord`, and the exit
/// callback `func_actor_342000_80163F88` writes it back into
/// `task->extra.tmd->coords->parent`.
///
/// `coord` is the actor's own rotation node. `func_actor_342000_801628C8`
/// builds `coord.coord` from the euler angles below it (`gfxRotMatrixY` of
/// `field_278`, then `X` of `field_274`, then `Z` of `field_27C`, word loads),
/// scales each of its columns by the matching `field_264` component through
/// `gpf 12` and clears `coord.composeStamp`; `func_actor_342000_801640C0` writes all of
/// it from an `ActorTransform`.
///
/// `field_264` holds that per-axis scale, 1.12 fixed point like the matrix it
/// multiplies: each column `j` is gathered into a scratchpad `SVECTOR`, run
/// through `GPF` against `field_264[j]` and written back.
///
/// `field_298` is the parent actor task a child model display handler
/// (`func_actor_342000_801625D8`) mirrors its flags and column scale from.
///
/// `field_29C` / `field_2A0` are the actor's two child tasks; the per-frame tail
/// of `func_actor_342000_801628C8` ticks them with `func_actor_342000_80161EA4`.
///
/// `field_2AA` latches the `ActorCommand::command` the id 0x7DB handler was
/// last called with; command 0xA additionally refills `field_264` from the
/// handler's second payload.
///
/// The block opens with the actor's playback rig -- `rig`, whose context
/// `animationInitContext` binds to its eight slots and pose entries with this
/// overlay's banks: `func_actor_342000_80161EA4` ticks it and passes the id bank
/// `field_288` indexes. Slot 0 is the child slot that function skips. `light` /
/// `color` at 0x1D4 / 0x1F4 are the pair `func_actor_342000_80162158`
/// republishes onto the model's `TmdObject::lightMtx` / `colorMtx`, exactly as
/// the neighbouring actor overlays lay out theirs.
typedef struct Actor342000Work {
    /* 0x000 */ ActorAnimRig8 rig;
    /* 0x1D4 */ MATRIX        light;
    /* 0x1F4 */ MATRIX        color;
    /* 0x214 */ GfxCoord      coord;
    /* 0x264 */ VECTOR        field_264;
    /* 0x274 */ s32           field_274;
    /* 0x278 */ s32           field_278;
    /* 0x27C */ s32           field_27C;
    /* 0x280 */ byte          pad_280[0x8];
    /* 0x288 */ s32           field_288;
    /* 0x28C */ byte          pad_28C[0xC];
    /* 0x298 */ Task*         field_298;
    /* 0x29C */ Task*         field_29C;
    /* 0x2A0 */ Task*         field_2A0;
    /* 0x2A4 */ GfxCoord*     field_2A4;
    /* 0x2A8 */ byte          pad_2A8[0x2];
    /* 0x2AA */ u16           field_2AA;
} Actor342000Work;
STATIC_ASSERT_SIZEOF(Actor342000Work, 0x2AC);

/// Work block of the overlay's event/sequence task -- the one
/// `D_actor_342000_80165070` points at.
///
/// `func_actor_342000_8016382C` allocates it with `memCalloc(0x80, 0)`,
/// `memFillBytes`s 0x80 bytes and stores it in that task's `Task::work` slot, so
/// the size is anchored. The same function publishes its owning task in
/// `D_actor_342000_80165070`, which is how the leaf helpers below reach it:
/// `(Actor342000EventWork*)D_actor_342000_80165070->work`.
///
/// `field_48` is the `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` task every `taskMessageDispatch` in the
/// overlay is aimed at; `field_50` / `field_5C` / `field_60` / `field_64` are
/// spawned child tasks the teardown helpers kill. `field_7A` and `field_7C`
/// are once-only latches guarding a sound cue and the fade-out setup.
typedef struct Actor342000EventWork {
    /* 0x00 */ ActorTransform field_0[2];
    /* 0x30 */ ActorTransform field_30;
    /* 0x48 */ Task*          field_48;
    /* 0x4C */ Task*          field_4C;
    /* 0x50 */ Task*          field_50;
    /* 0x54 */ Task*          field_54;
    /* 0x58 */ Task*          field_58;
    /* 0x5C */ Task*          field_5C;
    /* 0x60 */ Task*          field_60;
    /* 0x64 */ Task*          field_64;
    /* 0x68 */ u16            field_68;
    /* 0x6A */ u16            field_6A;
    /* 0x6C */ u16            field_6C;
    /* 0x6E */ byte           pad_6E[0x2];
    /* 0x70 */ s16            field_70;
    /* 0x72 */ s16            field_72;
    /* 0x74 */ u16            field_74;
    /* 0x76 */ byte           pad_76[0x2];
    /* 0x78 */ s16            field_78;
    /* 0x7A */ u16            field_7A;
    /* 0x7C */ u16            field_7C;
    /* 0x7E */ u16            field_7E;
} Actor342000EventWork;
STATIC_ASSERT_SIZEOF(Actor342000EventWork, 0x80);

/// The task owning the `Actor342000EventWork` block, published by
/// `func_actor_342000_8016382C`.
extern Task* D_actor_342000_80165070;

/// Message 0x7D4's static payload, handed to `taskMessageDispatch` by the actor's
/// spawn tick. The same record the handler takes.
extern ActorTransform D_actor_342000_801648B8;

/// Fixed placement `func_actor_342000_8016439C` warps slot 3 to, sent as
/// message 0x3E9 and again as 0x3F2 by `func_actor_342000_80162BBC`.
extern ActorTransform D_actor_342000_80164948;

// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_342000_801648A8[2];

/// Animation-id bank `Actor342000Work::field_288` indexes; a negative entry
/// means the bank is empty and the slots are left alone.
extern s16 D_actor_342000_80164810[];

/// Per-`spawnArg1` translation seeds for the child model's part coordinate.
extern SVECTOR D_actor_342000_80164900[];

void func_actor_342000_8016201C(Task*);
void func_actor_342000_801625D8(Task*);
void func_actor_342000_801628C8(Task*);
void func_actor_342000_8016382C(Task*);
void func_actor_342000_80163EAC(Task*);
s32  func_actor_342000_801640C0(Task* task, s32 msgId, ActorTransform* transform, s32 arg3);
s32  func_actor_342000_80164110(Task* task, s32 msgId, ActorCommand* request, ActorTransform* transform);
void func_actor_342000_80164154(void);
void func_actor_342000_801641B4(void);
void func_actor_342000_801641FC(void);
void func_actor_342000_80164260(void);
void func_actor_342000_801642B4(s16);
void func_actor_342000_801642D4(s16);
void func_actor_342000_801642F4(void);
void func_actor_342000_80164364(s32);
void func_actor_342000_8016439C(void);
void func_actor_342000_8016447C(void);
void func_actor_342000_8016449C(void);
void func_actor_342000_801644BC(void);

static AnimationPackedPose _gActor342000Animation029A0Bank1[6] = {
#include "assets/actor_342000_animation_029A0_bank1.inc"
};

static AnimationPackedRotation _gActor342000Animation029A0Bank4[46] = {
#include "assets/actor_342000_animation_029A0_bank4.inc"
};

static AnimationRecord _gActor342000Animation029A0Records[109] = {
#include "assets/actor_342000_animation_029A0_records.inc"
};

static u16 _gActor342000Animation029A0Indices[20] = {
#include "assets/actor_342000_animation_029A0_indices.inc"
};

static AnimationSet _gActor342000Animation029A0 = {
    _gActor342000Animation029A0Records,
    _gActor342000Animation029A0Indices,
    { NULL, _gActor342000Animation029A0Bank1, NULL, NULL, _gActor342000Animation029A0Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_342000_801647E8[4] = {
    &_gActor342000Animation029A0,
    &gActor444000Animation2E548,
    &gActor444000Animation2EA80,
    &gActor444000Animation2EE14,
};

AnimationSet* D_actor_342000_801647F8[2] = {
    &gActor444000Animation20BD4,
    &gActor444000Animation25BE0,
};

AnimationSet* D_actor_342000_80164800[2] = {
    &gActor444000Animation20C80,
    &gActor444000Animation25F14,
};

AnimationSet* D_actor_342000_80164808[2] = {
    &gActor444000Animation20D2C,
    &gActor444000Animation26240,
};

s16 D_actor_342000_80164810[4] = {
    -1,
    -1,
    -1,
    -1,
};

ActorTransform D_actor_342000_80164818[2] = {
    { { 0x2CEC, 4500, -0x4650, 0 }, { 0, 0, 0, 0 } },
    { { 0x4074, 4500, -0x4650, 0 }, { 0, 0, 0, 0 } },
};

ActorTransform D_actor_342000_80164848[2] = {
    { { 0x30D4, 4500, -0x4650, 0 }, { 0, 0, 0, 0 } },
    { { 0x3C8C, 4500, -0x4650, 0 }, { 0, 0, 0, 0 } },
};

ActorTransform D_actor_342000_80164878[2] = {
    { { 0x364C, 4500, -0x4650, 0 }, { 0, 0, 0, 0 } },
    { { 0x3714, 4500, -0x4650, 0 }, { 0, 0, 0, 0 } },
};

TaskMessageEntry D_actor_342000_801648A8[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetDrawMode },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
};

ActorTransform D_actor_342000_801648B8 = { { 0x36B0, 2000, -0x40D8, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_342000_801648D0 = { { 0x36B0, 2000, -0x3E80, 0 }, { 0, 2048, 0, 0 } };

TaskMessageEntry D_actor_342000_801648E8[3] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetDrawMode },
    { ACTOR_MESSAGE_PLACE, func_actor_342000_801640C0 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_342000_80164110 },
};

SVECTOR D_actor_342000_80164900[6] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 89, 100, 4 },
    { 0, 0, 0, 3 },
    { 0, 1660, 200, 2 },
};

ActorTransform D_actor_342000_80164930 = { { 0x3A98, 0, -0x5FB4, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_342000_80164948 = { { 0x3584, 0, -0x5460, 0 }, { 0, 0, 0, 0 } };

EvsSceneKey D_actor_342000_80164960 = { 4, 20, 11 };

EvsCommand D_actor_342000_80164968[51] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_342000_80164960 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_8016447C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_8016449C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_801641B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_801644BC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_801641FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_801642F4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_342000_80164364 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_342000_80164364 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_80164154 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_garbage_incinerator_8018507C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_342000_80164E30[19] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_80164154 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_801641B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_801641FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_8016439C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_801642F4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_80164260 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_342000_80164FF8[10] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_342000_8016382C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_342000_80163EAC, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_801628C8, { .model = &gActor444000Actor403200Model10824 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_801625D8, { .model = &gActor444000GluttonLegRight } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_801625D8, { .model = &gActor444000GluttonLegLeft } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_801625D8, { .model = &gActor444000Actor403200Model12884 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_801625D8, { .model = &gActor444000Actor403200Model13774 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_801625D8, { .model = &gActor444000Actor403200Model18BE4 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_8016201C, { .model = &gActor444000Model1C814 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_8016201C, { .model = &gActor444000Model1D36C } },
};

Task* D_actor_342000_80165070;

static void func_actor_342000_80163F88(Task* arg0);

extern AnimationSet* D_actor_342000_801647F8[];

extern AnimationSet* D_actor_342000_80164800[];

extern AnimationSet* D_actor_342000_80164808[];

extern TaskMessageEntry D_actor_342000_801648E8[3];

/// Animation payload of the 0x3F4 messages sent to the slot-3 task.
extern AnimationSet* D_actor_342000_801647E8[4];

/// Placement sent as message 0x3E9 by sequence step 1.
extern ActorTransform D_actor_342000_80164930;

extern ActorTransform D_actor_342000_80164818[2];

extern ActorTransform D_actor_342000_80164848[2];

extern ActorTransform D_actor_342000_80164878[2];

extern ActorTransform D_actor_342000_801648D0;

extern PadScriptCmd D_80144A74[2];

extern PadScriptVibrationSegment D_80144A7C[2];

void func_80143490(s32 arg0);

/// Spawn table of the event task's children: entry 2 is the script parent,
/// 3..7 its five script tasks and 8/9 the two effect actors.
extern TaskDesc D_actor_342000_80164FF8[];

extern EvsCommand D_actor_342000_80164968[];

extern EvsCommand D_actor_342000_80164E30[];

static s32         func_actor_342000_80161EA4(Task* arg0, u16 arg1);
static inline void Actor342000_InitCoord(Task* arg0, Actor342000Work* w);
static void        func_actor_342000_80162158(Task* arg0);
static void        func_actor_342000_80162BBC(Task* arg0);
static inline void Actor342000_CopyMove(ActorTransform* dst, ActorTransform* src);
static inline void Actor342000_SetAnim(Task* task, u16 anim, u16 blend, u16 n);
static inline s32  Actor342000_Sway(s32 x, s32 d);
static inline void Actor342000_Add(long* value, s32 delta);
static inline void Actor342000_Store(long* dst, s32 value);
static void        func_actor_342000_80162F28(Task* arg0);
static inline void Actor342000_KillFx(void);
static inline void Actor342000_SetAction(s16 arg0);
static inline void Actor342000_SetMode(s16 arg0);
static inline void Actor342000_EnterArea(void);

/// Ticks slots `(arg1 == 8)..arg1-1` of the task's animation context (slot 0 is
/// skipped for the eight-slot actor). If every one of them then has
/// `ANIMATION_SLOT_SETTLED` set, passes them the
/// `D_actor_342000_80164810` id and returns 1; otherwise returns 0. The gotos
/// reproduce retail's block layout.
static s32 func_actor_342000_80161EA4(Task* arg0, u16 arg1)
{
    Actor342000Work* work;
    Actor342000Work* ctx;
    u16              i;
    u16              done;
    u16              start;
    u16              anim;
    s32              first;

    anim  = arg1 == 8;
    start = anim;
    work  = (Actor342000Work*)arg0->work;
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
        if (D_actor_342000_80164810[work->field_288] >= 0) {
            anim  = D_actor_342000_80164810[work->field_288];
            ctx   = (Actor342000Work*)arg0->work;
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

void func_actor_342000_8016201C(Task* arg0)
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
            mtx->field_40 = (Task*)arg0->spawnArg2.pointer;
            extra->flags  = 0;
            if (arg0->spawnArg1.value != 0) {
                extra->otOffset = 0x1F;
            }
            arg0->extra.tmd->coords->parent = &gGfxViewCoord;
            extra->colorMtx                 = &mtx->color;
            extra->lightMtx                 = &mtx->light;
            arg0->msgTable                  = D_actor_342000_801648A8;
            taskReparent(mtx->field_40, arg0);
        }
        arg0->state += 1;
    }

    mdl    = arg0->extra.tmd;
    pos.vx = arg0->extra.tmd->coords->workm.t[0];
    pos.vy = arg0->extra.tmd->coords->workm.t[1];
    pos.vz = arg0->extra.tmd->coords->workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

/// Parents the work block's own coordinate to `Actor342000Work::field_2A4`,
/// hangs the model's part coordinate off it and resets it to an identity
/// matrix with no translation. Every `func_actor_342000_80162158` case repeats
/// it; as a function its address pseudos are born at their first use instead
/// of being hoisted to the top of each case.
static inline void Actor342000_InitCoord(Task* arg0, Actor342000Work* w)
{
    GfxCoord*  coord;
    GfxMatrix* mtx;

    coord                                 = &w->coord;
    coord->parent                         = ((Actor342000Work*)arg0->work)->field_2A4;
    arg0->extra.tmd->coords->parent       = coord;
    coord->coord.t[0]                     = 0;
    coord->coord.t[1]                     = 0;
    coord->coord.t[2]                     = 0;
    mtx                                   = (GfxMatrix*)&w->coord.coord;
    mtx->rotationWords.m00M01             = ONE;
    mtx->rotationWords.m02M10             = 0;
    mtx->rotationWords.m11M12             = ONE;
    mtx->rotationWords.m20M21             = 0;
    mtx->rotationWords.m22                = ONE;
    w->coord.composeStamp                 = GRAPHICS_COORD_DIRTY;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Spawn tick shared by the actor and its child model tasks: allocates and
/// zeroes the work block, republishes its light/colour matrices onto the model,
/// applies the area record 0x20's TMD bytes and, per `Task::spawnArg1`, parents
/// the coordinate (view, parent model, or the parent part
/// `D_actor_342000_80164900` names) and binds the animation bank. Cases 1 and 2
/// register themselves on the parent as `field_29C` / `field_2A0`.
///
/// The empty loops before `case 1:` / `case 2:` make reorg fill the dispatch
/// delay slots from those arms; one `ctx` per case keeps each short-lived so
/// the work pointer outranks it for `$s1`.
static void func_actor_342000_80162158(Task* arg0)
{
    TmdObject*       extra;
    Actor342000Work* work;
    Actor342000Work* ctx;
    Actor342000Work* ctx2;
    Actor342000Work* ctx3;
    Actor342000Work* w;
    AreaPlacement*   rec;
    u16              i;

    extra      = arg0->extra.tmd;
    work       = memMalloc(sizeof(*work), false);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    w = work;
    memFillBytes(w, 0, sizeof(*w));
    w->field_298    = (Task*)arg0->spawnArg2.pointer;
    extra->lightMtx = &w->light;
    extra->colorMtx = &w->color;
    arg0->msgTable  = D_actor_342000_801648E8;
    rec             = (Gp_GetNestedAreaRec(&gGameSession->location.loc))->placements;
    for (; rec->entryId != AREA_PLACEMENT_END; rec++) {
        if (rec->entryId == 0x20) {
            break;
        }
    }
    Gp_SetTmdBytes(extra, rec->texturePageOffset, rec->clutRowOffset);
    switch (arg0->spawnArg1.value) {
        case 0:
            w->field_2A4 = &gGfxViewCoord;
            Actor342000_InitCoord(arg0, w);
            animationInitContext(&w->rig.anim, D_actor_342000_801647F8, extra, w->rig.poses, w->rig.slots);
            ctx = (Actor342000Work*)arg0->work;
            for (i = 1; i < 8; i++) {
                ctx->rig.slots[i].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&ctx->rig.anim, i, 0);
            }
            break;
            do {
            } while (0);
        case 1:
            w->field_2A4 = w->field_298->extra.tmd->coords;
            Actor342000_InitCoord(arg0, w);
            ((Actor342000Work*)w->field_298->work)->field_29C = arg0;
            animationInitContext(&w->rig.anim, D_actor_342000_80164800, extra, w->rig.poses, w->rig.slots);
            ctx2 = (Actor342000Work*)arg0->work;
            for (i = 0; i < 4; i++) {
                ctx2->rig.slots[i].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&ctx2->rig.anim, i, 0);
            }
            break;
            do {
            } while (0);
        case 2:
            w->field_2A4 = w->field_298->extra.tmd->coords;
            Actor342000_InitCoord(arg0, w);
            ((Actor342000Work*)w->field_298->work)->field_2A0 = arg0;
            animationInitContext(&w->rig.anim, D_actor_342000_80164808, extra, w->rig.poses, w->rig.slots);
            ctx3 = (Actor342000Work*)arg0->work;
            for (i = 0; i < 4; i++) {
                ctx3->rig.slots[i].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&ctx3->rig.anim, i, 0);
            }
            break;
        default:
            w->field_2A4 = &w->field_298->extra.tmd->coords[D_actor_342000_80164900[arg0->spawnArg1.value].pad];
            Actor342000_InitCoord(arg0, w);
            break;
    }
    taskReparent(w->field_298, arg0);
    arg0->exitCallback = func_actor_342000_80163F88;
}

/// Display handler of the actor's child model. The spawn tick seeds the
/// model's part coordinate translation from the `D_actor_342000_80164900` entry
/// `Task::spawnArg1` selects; state 1 resets the work block's coordinate to
/// identity and scales each column by the parent's `Actor342000Work::field_264`.
/// Every tick then mirrors the parent model's `TmdObject::flags` and hands the
/// second part translation to `func_800D7A9C`.
void func_actor_342000_801625D8(Task* arg0)
{
    Actor342000Work* work;
    GfxMatrix*       mtx;
    VECTOR*          sc;
    GfxCoord*        coord;
    TmdObject*       extra;
    VECTOR           pos;

    work = (Actor342000Work*)arg0->work;

    switch (arg0->state) {
        case 0:
            func_actor_342000_80162158(arg0);
            work                = (Actor342000Work*)arg0->work;
            coord               = arg0->extra.tmd->coords;
            coord->coord.t[0]   = D_actor_342000_80164900[arg0->spawnArg1.value].vx;
            coord->coord.t[1]   = D_actor_342000_80164900[arg0->spawnArg1.value].vy;
            coord->coord.t[2]   = D_actor_342000_80164900[arg0->spawnArg1.value].vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            arg0->state        += 1;
            break;
        case 1:
            sc                        = &((Actor342000Work*)work->field_298->work)->field_264;
            mtx                       = (GfxMatrix*)&work->coord.coord;
            mtx->rotationWords.m00M01 = ONE;
            mtx->rotationWords.m02M10 = 0;
            mtx->rotationWords.m11M12 = ONE;
            mtx->rotationWords.m20M21 = 0;
            mtx->rotationWords.m22    = ONE;
            gfxScaleMatrixColumns(&mtx->mat, sc);
            work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
    arg0->extra.tmd->flags = work->field_298->extra.tmd->flags;
    extra                  = arg0->extra.tmd;
    pos.vx                 = arg0->extra.tmd->coords[1].workm.t[0];
    pos.vy                 = arg0->extra.tmd->coords[1].workm.t[1];
    pos.vz                 = arg0->extra.tmd->coords[1].workm.t[2];
    func_800D7A9C(extra, &pos, 0, 3);
}

/// Display handler of the actor itself. The spawn tick initialises the work
/// block and sends message 0x7D4. State 1 rebuilds the actor coordinate: an
/// identity rotation, the euler angles below it composed onto it (Y, then X,
/// then Z), and every column scaled by the matching component of
/// `Actor342000Work::field_264`. Every later tick ticks the actor's own
/// animation bank and the two child tasks and hands the model's second part
/// translation to `func_800D7A9C`.
void func_actor_342000_801628C8(Task* arg0)
{
    Actor342000Work* work;
    Actor342000Work* data;
    GfxMatrix*       mtx;
    s32*             ang;
    TmdObject*       extra;
    VECTOR           pos;

    work = (Actor342000Work*)arg0->work;

    switch (arg0->state) {
        case 0:
            func_actor_342000_80162158(arg0);
            TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_342000_801648B8, 0);
            arg0->state += 1;
            return;
        case 1:
            mtx                       = (GfxMatrix*)&work->coord.coord;
            mtx->rotationWords.m00M01 = ONE;
            mtx->rotationWords.m02M10 = 0;
            mtx->rotationWords.m11M12 = ONE;
            mtx->rotationWords.m20M21 = 0;
            mtx->rotationWords.m22    = ONE;
            ang                       = &work->field_274;
            gfxRotMatrixY(&mtx->mat, ang[1], 1);
            gfxRotMatrixX(&mtx->mat, ang[0], GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixZ(&mtx->mat, ang[2], GRAPHICS_ROTATION_COMPOSE);
            gfxScaleMatrixColumns(&mtx->mat, &work->field_264);
            work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
            /* fallthrough */
        default:
            data = (Actor342000Work*)arg0->work;
            func_actor_342000_80161EA4(arg0, 8);
            func_actor_342000_80161EA4(data->field_29C, 4);
            func_actor_342000_80161EA4(data->field_2A0, 4);
            extra  = arg0->extra.tmd;
            pos.vx = arg0->extra.tmd->coords[1].workm.t[0];
            pos.vy = arg0->extra.tmd->coords[1].workm.t[1];
            pos.vz = arg0->extra.tmd->coords[1].workm.t[2];
            func_800D7A9C(extra, &pos, 0, 3);
    }
}

/// `gPlayerStatus.weapon` is the
/// base weapon id, `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` selects the alternate animation block.

/// Per-tick sequence driver of the event task: raises 0x3ED on `field_48`,
/// then runs the one-shot step latched in `field_68` (warps, animation
/// changes for the slot-3 task, the step-2 wait on 0x3F0 plus an 11-tick
/// delay, and step 8's sound cue) and clears it. Cases 5 and 7 keep their
/// weapon id locals block-scoped; sharing one pseudo across both cases moves
/// the `gPlayerStatus.weapon` load ahead of the flag load.
static void func_actor_342000_80162BBC(Task* arg0)
{
    Actor342000EventWork* work;
    Actor342000EventWork* ev;
    AnimationPlayRequest  msg;

    work = (Actor342000EventWork*)arg0->work;
    if (work->field_48 != NULL) {
        taskMessageDispatch(work->field_48, ANIMATION_MESSAGE_IS_PLAYING, 0, 0);
    }
    switch (work->field_68) {
        case 0:
            break;
        case 1:
            Gp_PulseState1C();
            Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
            TASK_MESSAGE_DISPATCH_POINTER(work->field_48, 0x3E9, &D_actor_342000_80164930, 0);
            break;
        case 2:
            switch (work->field_6A) {
                case 0:
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_48, 0x3F2, &D_actor_342000_80164948, 0);
                    work->field_6A++;
                    return;
                case 1:
                    if (taskMessageDispatch(work->field_48, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                        work->field_6C = 0;
                        work->field_6A++;
                    }
                    return;
                case 2:
                    if (++work->field_6C > 10) {
                        work->field_68           = 0;
                        msg.source.sets          = D_actor_342000_801647E8;
                        msg.animationId          = 0;
                        msg.blend                = ANIMATION_BLEND_RESET;
                        msg.blendFrames          = 0;
                        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
                    }
                    return;
            }
            return;
        case 3:
            TASK_MESSAGE_DISPATCH_POINTER(work->field_48, 0x3E9, &D_actor_342000_80164948, 0);
            msg.source.sets          = D_actor_342000_801647E8;
            msg.animationId          = 0;
            msg.blend                = ANIMATION_BLEND_RESET;
            msg.blendFrames          = 0;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            break;
        case 4:
            msg.source.sets          = D_actor_342000_801647E8;
            msg.animationId          = 1;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 10;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            break;
        case 5: {
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
            break;
        }
        case 6:
            msg.source.sets          = D_actor_342000_801647E8;
            msg.animationId          = 2;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 10;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            break;
        case 7: {
            s32 weaponId;
            s32 anim;

            weaponId                 = gPlayerStatus.weapon;
            anim                     = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.source.index         = anim;
            msg.animationId          = 1;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 10;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &msg, 0);
            break;
        }
        case 8:
            msg.source.sets          = D_actor_342000_801647E8;
            msg.animationId          = 3;
            msg.blend                = ANIMATION_BLEND_RESET;
            msg.blendFrames          = 0;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            ev = (Actor342000EventWork*)D_actor_342000_80165070->work;
            if (ev->field_7A == 0) {
                SndEvt_EnqueueType6(SOUND_SHELTER_B3_INCINERATOR_ALERT, 0, 0);
                ev->field_7A = 1;
            }
            break;
    }
    work->field_68 = 0;
}

static inline void Actor342000_CopyMove(ActorTransform* dst, ActorTransform* src)
{
    dst->pos.vx = src->pos.vx;
    dst->pos.vy = src->pos.vy;
    dst->pos.vz = src->pos.vz;
    dst->rot.vx = src->rot.vx;
    dst->rot.vy = src->rot.vy;
    dst->rot.vz = src->rot.vz;
}

static inline void Actor342000_SetAnim(Task* task, u16 anim, u16 blend, u16 n)
{
    Actor342000Work* ctx;
    u16              i;
    u16              first;

    first = n == 8;
    ctx   = (Actor342000Work*)task->work;
    if (blend == 0) {
        for (i = first; i < n; i++) {
            ctx->rig.slots[i].rate = ANIMATION_RATE_ONE;
            animationResetSlot(&ctx->rig.anim, i, anim);
        }
    } else {
        for (i = first; i < n; i++) {
            animationSeekSlotWithBlend(&ctx->rig.anim, i, anim, 0, blend);
        }
    }
}

static inline s32 Actor342000_Sway(s32 x, s32 d)
{
    if (gDisplayState.animFrame & 1) {
        return x + d;
    }
    return x - d;
}

static inline void Actor342000_Add(long* value, s32 delta)
{
    *value += delta;
}

static inline void Actor342000_Store(long* dst, s32 value)
{
    *dst = value;
}

static void func_actor_342000_80162F28(Task* arg0)
{
    Actor342000EventWork* work;
    Actor342000Work*      actor;
    ActorTransform*       src;
    s32                   v;

    work  = (Actor342000EventWork*)arg0->work;
    actor = (Actor342000Work*)work->field_50->work;
    switch ((u16)work->field_70) {
        case 1:
            switch ((u16)work->field_72) {
                case 0:
                    taskMessageDispatch(work->field_50, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    taskMessageDispatch(work->field_5C, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    taskMessageDispatch(work->field_60, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    Actor342000_CopyMove(&work->field_0[0], &D_actor_342000_80164818[0]);
                    Actor342000_CopyMove(&work->field_0[1], &D_actor_342000_80164818[1]);
                    actor->field_264.vx   = 0x1000;
                    actor->field_264.vy   = 0x1000;
                    actor->field_264.vz   = 0x1000;
                    src                   = &D_actor_342000_801648B8;
                    work->field_30.pos.vx = src->pos.vx;
                    work->field_30.pos.vy = src->pos.vy;
                    work->field_30.pos.vz = src->pos.vz;
                    work->field_30.rot.vx = src->rot.vx;
                    work->field_30.rot.vy = src->rot.vy;
                    work->field_30.rot.vz = src->rot.vz;
                    work->field_74        = 0;
                    work->field_72++;
                case 1:
                    if (++work->field_74 == 60) {
                        Actor342000_SetAnim(work->field_50, 1, 10, 8);
                        Actor342000_SetAnim(work->field_54, 1, 10, 4);
                        Actor342000_SetAnim(work->field_58, 1, 10, 4);
                    }
                    actor->field_264.vx     -= 4;
                    work->field_0[0].pos.vx += 5;
                    work->field_0[1].pos.vx -= 5;
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_5C, 0x7D4, &work->field_0[0], 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_60, 0x7D4, &work->field_0[1], 0);
                    work->field_30.pos.vy += 3;
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_50, 0x7D4, &work->field_30, 0);
                    break;
            }
            return;
        case 2:
            switch ((u16)work->field_72) {
                case 0:
                    taskMessageDispatch(work->field_5C, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
                    taskMessageDispatch(work->field_60, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_50, 0x7D4, &D_actor_342000_801648D0, 0);
                    Actor342000_SetAnim(work->field_50, 0, 0, 8);
                    Actor342000_SetAnim(work->field_54, 0, 0, 4);
                    Actor342000_SetAnim(work->field_58, 0, 0, 4);
                    work->field_72++;
                case 1:
                    actor->field_264.vx -= 4;
                    break;
            }
            return;
        case 3:
            switch ((u16)work->field_72) {
                case 0:
                    taskMessageDispatch(work->field_5C, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    taskMessageDispatch(work->field_60, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_50, 0x7D4, &D_actor_342000_801648B8, 0);
                    Actor342000_CopyMove(&work->field_0[0], &D_actor_342000_80164848[0]);
                    Actor342000_CopyMove(&work->field_0[1], &D_actor_342000_80164848[1]);
                    work->field_72++;
                case 1:
                    actor->field_264.vx -= 4;
                    Actor342000_Add(&work->field_0[0].pos.vx, 5);
                    Actor342000_Add(&work->field_0[1].pos.vx, -5);
                    v = Actor342000_Sway(work->field_0[0].pos.vz, -20);
                    Actor342000_Store(&work->field_0[0].pos.vz, v);
                    v = Actor342000_Sway(work->field_0[1].pos.vz, 20);
                    Actor342000_Store(&work->field_0[1].pos.vz, v);
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_5C, 0x7D4, &work->field_0[0], 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_60, 0x7D4, &work->field_0[1], 0);
                    break;
            }
            return;
        case 0:
            break;
        case 4:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 8;
            work->field_64                                             = Task_Spawn(1, 0x2D, 0x10, 0);
            break;
        case 5:
            if (work->field_64 != NULL) {
                taskKill(work->field_64);
            }
            break;
        case 6:
            if ((u16)work->field_72 == 0) {
                SndEvt_EnqueueType6(SOUND_SHELTER_B3_INCINERATOR_DOORS_CLOSING, 0, 0);
                work->field_7E = 1;
                work->field_72++;
            }
            if (gGameSession->location.loc.view == 0xF) {
                taskMessageDispatch(work->field_5C, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
                taskMessageDispatch(work->field_60, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
            } else {
                taskMessageDispatch(work->field_5C, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                taskMessageDispatch(work->field_60, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            }
            work->field_0[0].pos.vx += 5;
            work->field_0[1].pos.vx -= 5;
            TASK_MESSAGE_DISPATCH_POINTER(work->field_5C, 0x7D4, &work->field_0[0], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->field_60, 0x7D4, &work->field_0[1], 0);
            return;
        case 7:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = work->field_78;
            break;
        case 8:
            switch ((u16)work->field_72) {
                case 0:
                    taskMessageDispatch(work->field_5C, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    taskMessageDispatch(work->field_60, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    Actor342000_CopyMove(&work->field_0[0], &D_actor_342000_80164878[0]);
                    Actor342000_CopyMove(&work->field_0[1], &D_actor_342000_80164878[1]);
                    work->field_72++;
                case 1:
                    work->field_0[0].pos.vx += 5;
                    work->field_0[1].pos.vx -= 5;
                    if (work->field_0[0].pos.vx >= 0x36B0) {
                        work->field_0[0].pos.vx = 0x36B0;
                        work->field_0[1].pos.vx = 0x36B0;
                        taskReparent(arg0, Gp_SpawnScript18(D_80144A74, D_80144A7C));
                        func_80143490(3);
                        work->field_70 = 0;
                    }
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_5C, 0x7D4, &work->field_0[0], 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->field_60, 0x7D4, &work->field_0[1], 0);
                    break;
            }
            return;
        case 9:
            SndEvt_EnqueueType7(SOUND_SHELTER_B3_INCINERATOR_DOORS_CLOSING, 1);
            SndEvt_EnqueueType6(SOUND_SHELTER_B3_INCINERATOR_DOORS_SHUT, 0, 0);
            taskReparent(arg0, Gp_SpawnScript18(D_80144A74, D_80144A7C));
            func_80143490(3);
            break;
        default:
            break;
    }
    work->field_70 = 0;
}

/// The event task's leaf steps, inlined here; `actor_342000_3.c` carries the
/// same bodies as out-of-line functions (`func_actor_342000_801641FC`,
/// `801642B4`, `801642D4`, `80164154`).
static inline void Actor342000_KillFx(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    if (work->field_5C != NULL) {
        taskKill(work->field_5C);
    }
    if (work->field_60 != NULL) {
        taskKill(work->field_60);
    }
    work->field_5C = NULL;
    work->field_60 = NULL;
}

static inline void Actor342000_SetAction(s16 arg0)
{
    Actor342000EventWork* work;

    work           = (Actor342000EventWork*)D_actor_342000_80165070->work;
    work->field_68 = arg0;
    work->field_6A = 0;
}

static inline void Actor342000_SetMode(s16 arg0)
{
    Actor342000EventWork* work;

    work           = (Actor342000EventWork*)D_actor_342000_80165070->work;
    work->field_70 = arg0;
    work->field_72 = 0;
}

static inline void Actor342000_EnterArea(void)
{
    gGameSession->location.loc.room                            = 7;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 7;
    gGameSession->eventRoomIndex                               = 6;
    gGameSession->incineratorRoomGroup                         = 1;
    gGameSession->roomObjsDirty                                = 1;
    Gp_ApplyAreaRecs(D_shelter_b3_garbage_incinerator_8018FB6C);
}

/// Event/sequence task body, idle while a cutscene, pause or mode switch is up.
/// State 0 allocates the `Actor342000EventWork` block and spawns the effect
/// actors (a spawn with `GameSession::skipEventIntro` set skips to state 4);
/// states 1..10 spawn the script tasks, seed the placements and run the timed
/// hand-off to area 0x21, and state 11 kills the task. `SOFT_BARRIER()` keeps
/// state 7's `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` store ahead of the state load, as in retail.
void func_actor_342000_8016382C(Task* arg0)
{
    ActorCommand          msg;
    Actor342000EventWork* work;
    Actor342000EventWork* ev;
    Actor342000EventWork* alloc;
    Actor342000EventWork* seq;
    ActorTransform*       src;
    ActorTransform*       dst;
    Task*                 child;
    u16                   i;
    s16                   timer;

    work = (Actor342000EventWork*)arg0->work;
    if (D_shelter_b3_garbage_incinerator_801855DE != 0 || gGameSession->sceneUpdatesPaused != 0 || Gp_StateC08.menuOpen != ATTACHMENT_MENU_CLOSED || gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
        return;
    }
    if (gGameSession->enemyCullZone != 0) {
        if (work->field_7E != 0) {
            SndEvt_EnqueueType7(SOUND_SHELTER_B3_INCINERATOR_DOORS_CLOSING, 0xA);
            work->field_7E = 0;
        }
        return;
    }
    switch (arg0->state) {
        case 0:
            alloc      = memCalloc(0x80U, false);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
            } else {
                memFillBytes(alloc, 0U, sizeof(*alloc));
                alloc->field_48         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_actor_342000_80165070 = arg0;
                alloc->field_4C         = Gp_FindWorkById(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8))->task;
            }
            work = (Actor342000EventWork*)arg0->work;
            if (gGameSession->skipEventIntro == 0) {
                msg.context.loc.stage = gGameSession->location.loc.stage;
                msg.context.loc.area  = gGameSession->location.loc.area;
                msg.command           = 0;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
                work->field_5C = Task_SpawnFromTable(D_actor_342000_80164FF8, 8, 0, arg0);
                work->field_60 = Task_SpawnFromTable(D_actor_342000_80164FF8, 9, 0, arg0);
                goto next;
            }
            work->field_5C = Task_SpawnFromTable(D_actor_342000_80164FF8, 8, 1, arg0);
            work->field_60 = Task_SpawnFromTable(D_actor_342000_80164FF8, 9, 1, arg0);
            func_shelter_b3_garbage_incinerator_80180FE4(0x17, 0, 0x3C);
            arg0->state = 4;
            break;
        case 1:
            work->field_50 = Task_SpawnFromTable(D_actor_342000_80164FF8, 2, 0, arg0);
            for (i = 0; i < 5; i++) {
                child = Task_SpawnFromTable(D_actor_342000_80164FF8, i + 3, i + 1, work->field_50);
                if (i == 0) {
                    work->field_54 = child;
                }
                if (i == 1) {
                    work->field_58 = child;
                }
            }
            goto next;
        case 2:
            Gp_MsgPlayerWeapon(0);
            func_800E8634(D_actor_342000_80164968, 0, D_actor_342000_80164E30);
            goto next;
        case 3:
            if (gGameSession->eventState == 0) {
                gGameSession->sceneClock = D_shelter_b3_garbage_incinerator_8018FBC8[0];
                Task_SpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 0, 1, 0);
                gGameSession->incineratorExitPhase = GAME_SESSION_INCINERATOR_EXIT_ENCOUNTER;
                Task_RequestKill(arg0, 0);
                return;
            }
            func_actor_342000_80162BBC(arg0);
            func_actor_342000_80162F28(arg0);
            break;
        case 4:
            Actor342000_CopyMove(&work->field_0[0], &D_actor_342000_80164818[0]);
            Actor342000_CopyMove(&work->field_0[1], &D_actor_342000_80164818[1]);
            work->field_70      = 6;
            arg0->killCountdown = 0;
            arg0->state++;
            break;
        case 5:
            timer               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = timer;
            if (timer >= 0x1A5) {
                work->field_78 = gGameSession->location.loc.view;
                Actor342000_SetAction(7);
                Actor342000_SetMode(6);
                arg0->killCountdown = 0;
                arg0->state++;
            }
            func_actor_342000_80162F28(arg0);
            break;
        case 6:
            timer               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = timer;
            if (timer >= 2) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x21;
                goto next;
            }
            break;
        case 7:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x21;
            arg0->killCountdown                                        = 0;
            arg0->state++;
            break;
        case 8:
            timer               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = timer;
            if (timer >= 0x3C) {
                Actor342000_KillFx();
                Actor342000_SetMode(7);
                Actor342000_EnterArea();
                msg.context.loc.stage = gGameSession->location.loc.stage;
                msg.context.loc.area  = gGameSession->location.loc.area;
                msg.command           = 0;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
                arg0->killCountdown = 0;
                arg0->state++;
                break;
            }
            break;
        case 9:
            timer               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = timer;
            if (timer >= 2) {
                Actor342000_SetMode(9);
                func_shelter_b3_garbage_incinerator_8018507C();
                taskMessageDispatch(work->field_48, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                gGameSession->incineratorExitPhase = GAME_SESSION_INCINERATOR_EXIT_ENCOUNTER;
                goto next;
            }
            break;
        case 10:
        next:
            arg0->state++;
            break;
        case 11:
            Task_RequestKill(arg0, 0);
            return;
    }
    if ((u32)(arg0->state - 6) < 5U) {
        func_actor_342000_80162BBC(arg0);
        func_actor_342000_80162F28(arg0);
    }
}

/// Fade task of the overlay's task table: its first tick allocates the
/// channel block and seeds every channel at 0xFF; each tick then draws the
/// full-screen fade overlay and steps the channels down by `spawnArg1`, killing
/// the task once `r` has gone negative.
void func_actor_342000_80163EAC(Task* arg0)
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

static void func_actor_342000_80163F88(Task* task)
{
    Actor342000Work* work;
    GfxCoord*        coord;

    coord = task->extra.tmd->coords;
    work  = (Actor342000Work*)task->work;

    coord->parent = work->field_2A4;
    taskKill(task);
}

#include "../../shared/actor_messages_draw_mode.inc.c"

#include "../../shared/actor_messages_place_ypr.inc.c"

s32 func_actor_342000_801640C0(Task* arg0, s32 arg1, ActorTransform* transform, s32 arg3)
{
    Actor342000Work* work;
    GfxCoord*        coord;

    work                     = (Actor342000Work*)arg0->work;
    coord                    = &work->coord;
    coord->coord.t[0]        = transform->pos.vx;
    coord->coord.t[1]        = transform->pos.vy;
    coord->coord.t[2]        = transform->pos.vz;
    work->field_274          = transform->rot.vx;
    work->field_278          = transform->rot.vy;
    work->field_27C          = transform->rot.vz;
    work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
}

s32 func_actor_342000_80164110(Task* arg0, s32 arg1, ActorCommand* request, ActorTransform* transform)
{
    Actor342000Work* work;

    work = (Actor342000Work*)arg0->work;
    if (request->command == 0xA) {
        work->field_264.vx = transform->pos.vx;
        work->field_264.vy = transform->pos.vy;
        work->field_264.vz = transform->pos.vz;
    }
    work->field_2AA = request->command;
}

void func_actor_342000_80164154(void)
{
    gGameSession->location.loc.room                            = 7;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 7;
    gGameSession->eventRoomIndex                               = 6;
    gGameSession->incineratorRoomGroup                         = 1;
    gGameSession->roomObjsDirty                                = 1;
    Gp_ApplyAreaRecs(D_shelter_b3_garbage_incinerator_8018FB6C);
}

void func_actor_342000_801641B4(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    if (work->field_50 != NULL) {
        Task_CallExit(work->field_50);
    }
    work->field_50 = NULL;
}

void func_actor_342000_801641FC(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    if (work->field_5C != NULL) {
        taskKill(work->field_5C);
    }
    if (work->field_60 != NULL) {
        taskKill(work->field_60);
    }
    work->field_5C = NULL;
    work->field_60 = NULL;
}

void func_actor_342000_80164260(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    if (work->field_7A == 0) {
        SndEvt_EnqueueType6(SOUND_SHELTER_B3_INCINERATOR_ALERT, 0, 0);
        work->field_7A = 1;
    }
}

void func_actor_342000_801642B4(s16 arg0)
{
    Actor342000EventWork* work;

    work           = (Actor342000EventWork*)D_actor_342000_80165070->work;
    work->field_68 = arg0;
    work->field_6A = 0;
}

void func_actor_342000_801642D4(s16 arg0)
{
    Actor342000EventWork* work;

    work           = (Actor342000EventWork*)D_actor_342000_80165070->work;
    work->field_70 = arg0;
    work->field_72 = 0;
}

void func_actor_342000_801642F4(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    if (work->field_7C == 0) {
        gSceneCombatState.battleRefs                        = 0;
        gSceneCombatState.signals.bytes.endDelayFrames      = 0xF;
        gSceneCombatState.signals.bytes.battlePhase         = SCENE_COMBAT_BATTLE_IDLE;
        gSceneCombatState.signals.bytes.actionFlags         = 0;
        gSceneCombatState.signals.bytes.enemyAlert          = 0;
        gGameSession->flowFlags                            |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0xD;
        work->field_7C                                      = 1;
    }
}

void func_actor_342000_80164364(s32 arg0)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    taskMessageDispatch(work->field_48, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, arg0, 0);
}

/// Warps the slot-3 task to the overlay's fixed placement (0x3E9), installs
/// the animation set the current weapon selects (`gPlayerStatus.weapon + 1` for the
/// alternate block, `+ 0x22` for the base one, sent as 0x3E8 to the slot
/// `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` returns), raises 0x3F3, kills the child in
/// `field_64`, and cancels any pending CD command replacement.
void func_actor_342000_8016439C(void)
{
    Actor342000EventWork* work;
    AnimationPlayRequest  msg;
    s32                   weaponId;
    s32                   anim;

    work = (Actor342000EventWork*)D_actor_342000_80165070->work;
    TASK_MESSAGE_DISPATCH_POINTER(work->field_48, 0x3E9, &D_actor_342000_80164948, 0);
    func_shelter_b3_garbage_incinerator_8018507C();
    weaponId                 = gPlayerStatus.weapon;
    anim                     = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.source.index         = anim;
    msg.animationId          = 1;
    msg.blend                = ANIMATION_BLEND_RESET;
    msg.blendFrames          = 0;
    msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &msg, 0);
    taskMessageDispatch(((Actor342000EventWork*)D_actor_342000_80165070->work)->field_48, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
    if (work->field_64 != NULL) {
        taskKill(work->field_64);
        work->field_64 = NULL;
    }
    CdCmd_CancelReplaceAndActivate();
}

/// Script callback: queues the replacement overlay load.
void func_actor_342000_8016447C(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Script callback: queues the overlay load.
void func_actor_342000_8016449C(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Script callback: restores the stream random state, then cancels the
/// pending overlay replacement and activates the loaded one.
void func_actor_342000_801644BC(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}
