#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/model_placement.h"
#include "../../shared/actor_messages.h"

/// Work block the spawn handler allocates (`memCalloc(0x4C4)`) and parks in
/// `Task::work`. It opens with the animation context the preset handler hands
/// `func_800B3F84` at the block's own address, the 0x14 0x28-byte slots
/// immediately above it and the 0x140-byte table at 0x334 that call also
/// takes; the slot walkers run to 0x14, the slot count. `field_474` latches
/// once a preset has started the slots and gates the per-frame tick;
/// `field_476` and `field_475` hold the current bank index and animation id,
/// seeded to -1 by the spawn handler. `field_477` is the countdown after which
/// the tick frees the model's buffers, -1 while idle. `light` / `color` are
/// the matrices published on the model as its light and colour matrices.
/// `field_4BC` / `field_4C0` hold the two children spawned from table entries
/// 1 and 2, whose models the 0x7DB handler shows and hides.
typedef struct Actor213000Work {
    /* 0x000 */ ActorAnimRig20 rig;
    /* 0x474 */ s8             field_474;
    /* 0x475 */ s8             field_475;
    /* 0x476 */ s8             field_476;
    /* 0x477 */ s8             field_477;
    /* 0x478 */ s32            field_478;
    /* 0x47C */ MATRIX         light;
    /* 0x49C */ MATRIX         color;
    /* 0x4BC */ Task*          field_4BC;
    /* 0x4C0 */ Task*          field_4C0;
} Actor213000Work;
STATIC_ASSERT_SIZEOF(Actor213000Work, 0x4C4);

/// The actor's spawn table: entry 0 is the actor itself, entries 1 to 4 the
/// children its spawn handler creates.
extern TaskDesc D_actor_213000_80157DE0[];

/// The actor's message table: `(message id, handler)` pairs for 0x7D3 / 0x7D4 /
/// 0x7D5 / 0x7DB, ended by `0x7FFFFFFF`. The spawn handler parks its address in
/// `Task::msgTable`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*);
        s32 (*call1)(Task*, s32, ActorCommand* request);
        s32 (*call2)(Task*, s32, ActorTransform*);
        s32 (*call3)(Task*, s32, s32);
    } handler;
} Actor213000MsgEntry;
STATIC_ASSERT_SIZEOF(Actor213000MsgEntry, 8);

extern Actor213000MsgEntry D_actor_213000_80157E1C[];

/// Animation bank table the 0x7D3 handler indexes with the preset's `field_0`.
extern AnimationSet*  D_actor_213000_80157DB0[11];
extern AnimationSet** D_actor_213000_80157DDC[1];

static void func_actor_213000_8014A158(Task* task);
static void func_actor_213000_8014A5D0(Task* task);
static void func_actor_213000_8014A6AC(Task* task);

extern TmdSource D_actor_213000_801505D0;
extern TmdSource D_actor_213000_80150C04;
extern TmdSource D_actor_213000_80150F88;
extern TmdSource D_actor_213000_80151254;
extern TmdSource D_actor_213000_8015144C;
s32              func_actor_213000_8014A70C(Task*, s32, AnimationPlayRequest*);
s32              func_actor_213000_8014A8A4(Task*, s32, s32);
s32              func_actor_213000_8014A980(Task*, s32, ActorCommand* msg);
void             func_actor_213000_8014A084(Task*);
void             func_actor_213000_8014A160(Task*);
void             func_actor_213000_8014A520(Task*);
void             func_actor_213000_8014A578(Task*);

TmdBone D_actor_213000_8014AA4C[20] = {
#include "assets/actor_213000_model_067B0_skeleton.inc"
};

u32 D_actor_213000_8014AD1C[20] = {
#include "assets/actor_213000_model_067B0_partVerts.inc"
};

SVECTOR D_actor_213000_8014AD6C[338] = {
#include "assets/actor_213000_model_067B0_verts.inc"
};

SVECTOR D_actor_213000_8014B7FC[413] = {
#include "assets/actor_213000_model_067B0_normals.inc"
};

u32 D_actor_213000_8014C4E4[4155] = {
#include "assets/actor_213000_model_067B0_stream.inc"
};

TmdSource D_actor_213000_801505D0 = {
    0,
    20860,
    8300,
    20,
    D_actor_213000_8014AD1C,
    D_actor_213000_8014AD6C,
    D_actor_213000_8014B7FC,
    D_actor_213000_8014AA4C,
    D_actor_213000_8014C4E4,
};

TmdBone D_actor_213000_801505F4[3] = {
#include "assets/actor_213000_model_06DE4_skeleton.inc"
};

u32 D_actor_213000_80150660[3] = {
#include "assets/actor_213000_model_06DE4_partVerts.inc"
};

SVECTOR D_actor_213000_8015066C[28] = {
#include "assets/actor_213000_model_06DE4_verts.inc"
};

SVECTOR D_actor_213000_8015074C[28] = {
#include "assets/actor_213000_model_06DE4_normals.inc"
};

u32 D_actor_213000_8015082C[246] = {
#include "assets/actor_213000_model_06DE4_stream.inc"
};

TmdSource D_actor_213000_80150C04 = {
    0,
    1312,
    312,
    3,
    D_actor_213000_80150660,
    D_actor_213000_8015066C,
    D_actor_213000_8015074C,
    D_actor_213000_801505F4,
    D_actor_213000_8015082C,
};

TmdBone D_actor_213000_80150C28[1] = {
#include "assets/actor_213000_model_07168_skeleton.inc"
};

u32 D_actor_213000_80150C4C[1] = {
#include "assets/actor_213000_model_07168_partVerts.inc"
};

SVECTOR D_actor_213000_80150C50[19] = {
#include "assets/actor_213000_model_07168_verts.inc"
};

SVECTOR D_actor_213000_80150CE8[19] = {
#include "assets/actor_213000_model_07168_normals.inc"
};

u32 D_actor_213000_80150D80[130] = {
#include "assets/actor_213000_model_07168_stream.inc"
};

TmdSource D_actor_213000_80150F88 = {
    0,
    876,
    0,
    1,
    D_actor_213000_80150C4C,
    D_actor_213000_80150C50,
    D_actor_213000_80150CE8,
    D_actor_213000_80150C28,
    D_actor_213000_80150D80,
};

TmdBone D_actor_213000_80150FAC[1] = {
#include "assets/actor_213000_model_07434_skeleton.inc"
};

u32 D_actor_213000_80150FD0[1] = {
#include "assets/actor_213000_model_07434_partVerts.inc"
};

SVECTOR D_actor_213000_80150FD4[14] = {
#include "assets/actor_213000_model_07434_verts.inc"
};

SVECTOR D_actor_213000_80151044[17] = {
#include "assets/actor_213000_model_07434_normals.inc"
};

u32 D_actor_213000_801510CC[98] = {
#include "assets/actor_213000_model_07434_stream.inc"
};

TmdSource D_actor_213000_80151254 = {
    0,
    652,
    0,
    1,
    D_actor_213000_80150FD0,
    D_actor_213000_80150FD4,
    D_actor_213000_80151044,
    D_actor_213000_80150FAC,
    D_actor_213000_801510CC,
};

TmdBone D_actor_213000_80151278[1] = {
#include "assets/actor_213000_model_0762C_skeleton.inc"
};

u32 D_actor_213000_8015129C[1] = {
#include "assets/actor_213000_model_0762C_partVerts.inc"
};

SVECTOR D_actor_213000_801512A0[14] = {
#include "assets/actor_213000_model_0762C_verts.inc"
};

u32 D_actor_213000_80151310[79] = {
#include "assets/actor_213000_model_0762C_stream.inc"
};

TmdSource D_actor_213000_8015144C = {
    0,
    528,
    0,
    1,
    D_actor_213000_8015129C,
    D_actor_213000_801512A0,
    &D_actor_213000_801512A0[14],
    D_actor_213000_80151278,
    D_actor_213000_80151310,
};

AnimationPackedPose D_actor_213000_80151470[11] = {
#include "assets/actor_213000_animation_08080_bank1.inc"
};

AnimationPackedRotation D_actor_213000_801514F4[277] = {
#include "assets/actor_213000_animation_08080_bank4.inc"
};

AnimationRecord D_actor_213000_80151948[332] = {
#include "assets/actor_213000_animation_08080_records.inc"
};

u16 D_actor_213000_80151E78[20] = {
#include "assets/actor_213000_animation_08080_indices.inc"
};

AnimationSet D_actor_213000_80151EA0 = {
    D_actor_213000_80151948,
    D_actor_213000_80151E78,
    { NULL, D_actor_213000_80151470, NULL, NULL, D_actor_213000_801514F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213000_80151EC8[2] = {
#include "assets/actor_213000_animation_0894C_bank1.inc"
};

AnimationPackedRotation D_actor_213000_80151EE0[242] = {
#include "assets/actor_213000_animation_0894C_bank4.inc"
};

AnimationRecord D_actor_213000_801522A8[295] = {
#include "assets/actor_213000_animation_0894C_records.inc"
};

u16 D_actor_213000_80152744[20] = {
#include "assets/actor_213000_animation_0894C_indices.inc"
};

AnimationSet D_actor_213000_8015276C = {
    D_actor_213000_801522A8,
    D_actor_213000_80152744,
    { NULL, D_actor_213000_80151EC8, NULL, NULL, D_actor_213000_80151EE0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213000_80152794[2] = {
#include "assets/actor_213000_animation_09DDC_bank1.inc"
};

AnimationPackedRotation D_actor_213000_801527AC[570] = {
#include "assets/actor_213000_animation_09DDC_bank4.inc"
};

AnimationRecord D_actor_213000_80153094[720] = {
#include "assets/actor_213000_animation_09DDC_records.inc"
};

u16 D_actor_213000_80153BD4[20] = {
#include "assets/actor_213000_animation_09DDC_indices.inc"
};

AnimationSet D_actor_213000_80153BFC = {
    D_actor_213000_80153094,
    D_actor_213000_80153BD4,
    { NULL, D_actor_213000_80152794, NULL, NULL, D_actor_213000_801527AC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213000_80153C24[2] = {
#include "assets/actor_213000_animation_0B0DC_bank1.inc"
};

AnimationPackedRotation D_actor_213000_80153C3C[555] = {
#include "assets/actor_213000_animation_0B0DC_bank4.inc"
};

AnimationRecord D_actor_213000_801544E8[635] = {
#include "assets/actor_213000_animation_0B0DC_records.inc"
};

u16 D_actor_213000_80154ED4[20] = {
#include "assets/actor_213000_animation_0B0DC_indices.inc"
};

AnimationSet D_actor_213000_80154EFC = {
    D_actor_213000_801544E8,
    D_actor_213000_80154ED4,
    { NULL, D_actor_213000_80153C24, NULL, NULL, D_actor_213000_80153C3C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213000_80154F24[2] = {
#include "assets/actor_213000_animation_0BA6C_bank1.inc"
};

AnimationPackedRotation D_actor_213000_80154F3C[255] = {
#include "assets/actor_213000_animation_0BA6C_bank4.inc"
};

AnimationRecord D_actor_213000_80155338[331] = {
#include "assets/actor_213000_animation_0BA6C_records.inc"
};

u16 D_actor_213000_80155864[20] = {
#include "assets/actor_213000_animation_0BA6C_indices.inc"
};

AnimationSet D_actor_213000_8015588C = {
    D_actor_213000_80155338,
    D_actor_213000_80155864,
    { NULL, D_actor_213000_80154F24, NULL, NULL, D_actor_213000_80154F3C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213000_801558B4[2] = {
#include "assets/actor_213000_animation_0C7C8_bank1.inc"
};

AnimationPackedRotation D_actor_213000_801558CC[383] = {
#include "assets/actor_213000_animation_0C7C8_bank4.inc"
};

AnimationRecord D_actor_213000_80155EC8[446] = {
#include "assets/actor_213000_animation_0C7C8_records.inc"
};

u16 D_actor_213000_801565C0[20] = {
#include "assets/actor_213000_animation_0C7C8_indices.inc"
};

AnimationSet D_actor_213000_801565E8 = {
    D_actor_213000_80155EC8,
    D_actor_213000_801565C0,
    { NULL, D_actor_213000_801558B4, NULL, NULL, D_actor_213000_801558CC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213000_80156610[3] = {
#include "assets/actor_213000_animation_0CBE8_bank1.inc"
};

AnimationPackedRotation D_actor_213000_80156634[96] = {
#include "assets/actor_213000_animation_0CBE8_bank4.inc"
};

AnimationRecord D_actor_213000_801567B4[139] = {
#include "assets/actor_213000_animation_0CBE8_records.inc"
};

u16 D_actor_213000_801569E0[20] = {
#include "assets/actor_213000_animation_0CBE8_indices.inc"
};

AnimationSet D_actor_213000_80156A08 = {
    D_actor_213000_801567B4,
    D_actor_213000_801569E0,
    { NULL, D_actor_213000_80156610, NULL, NULL, D_actor_213000_80156634, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213000_80156A30[3] = {
#include "assets/actor_213000_animation_0D3A4_bank1.inc"
};

AnimationPackedRotation D_actor_213000_80156A54[213] = {
#include "assets/actor_213000_animation_0D3A4_bank4.inc"
};

AnimationRecord D_actor_213000_80156DA8[253] = {
#include "assets/actor_213000_animation_0D3A4_records.inc"
};

u16 D_actor_213000_8015719C[20] = {
#include "assets/actor_213000_animation_0D3A4_indices.inc"
};

AnimationSet D_actor_213000_801571C4 = {
    D_actor_213000_80156DA8,
    D_actor_213000_8015719C,
    { NULL, D_actor_213000_80156A30, NULL, NULL, D_actor_213000_80156A54, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213000_801571EC[2] = {
#include "assets/actor_213000_animation_0D724_bank1.inc"
};

AnimationPackedRotation D_actor_213000_80157204[79] = {
#include "assets/actor_213000_animation_0D724_bank4.inc"
};

AnimationRecord D_actor_213000_80157340[119] = {
#include "assets/actor_213000_animation_0D724_records.inc"
};

u16 D_actor_213000_8015751C[20] = {
#include "assets/actor_213000_animation_0D724_indices.inc"
};

AnimationSet D_actor_213000_80157544 = {
    D_actor_213000_80157340,
    D_actor_213000_8015751C,
    { NULL, D_actor_213000_801571EC, NULL, NULL, D_actor_213000_80157204, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213000_8015756C[2] = {
#include "assets/actor_213000_animation_0DF68_bank1.inc"
};

AnimationPackedRotation D_actor_213000_80157584[222] = {
#include "assets/actor_213000_animation_0DF68_bank4.inc"
};

AnimationRecord D_actor_213000_801578FC[281] = {
#include "assets/actor_213000_animation_0DF68_records.inc"
};

u16 D_actor_213000_80157D60[20] = {
#include "assets/actor_213000_animation_0DF68_indices.inc"
};

AnimationSet D_actor_213000_80157D88 = {
    D_actor_213000_801578FC,
    D_actor_213000_80157D60,
    { NULL, D_actor_213000_8015756C, NULL, NULL, D_actor_213000_80157584, NULL, NULL, NULL },
};

AnimationSet* D_actor_213000_80157DB0[11] = {
    NULL,
    &D_actor_213000_80151EA0,
    &D_actor_213000_8015276C,
    &D_actor_213000_80153BFC,
    &D_actor_213000_80154EFC,
    &D_actor_213000_8015588C,
    &D_actor_213000_801565E8,
    &D_actor_213000_80156A08,
    &D_actor_213000_801571C4,
    &D_actor_213000_80157544,
    &D_actor_213000_80157D88,
};

AnimationSet** D_actor_213000_80157DDC[1] = {
    D_actor_213000_80157DB0,
};

TaskDesc D_actor_213000_80157DE0[5] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_213000_8014A578, { .model = &D_actor_213000_801505D0 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_213000_8014A084, { .model = &D_actor_213000_80151254 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_213000_8014A084, { .model = &D_actor_213000_8015144C } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_213000_8014A520, { .model = &D_actor_213000_80150C04 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_213000_8014A160, { .model = &D_actor_213000_80150F88 } },
};

Actor213000MsgEntry D_actor_213000_80157E1C[5] = {
    { 2003, { .call0 = func_actor_213000_8014A70C } },
    { 2004, { .call2 = actorMsgPlaceEuler } },
    { 2005, { .call3 = func_actor_213000_8014A8A4 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = func_actor_213000_8014A980 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
}; /// Spawn handler: allocates the work block, seeds its animation bytes and

static void func_actor_213000_80149E54(Task* task);
static void func_actor_213000_8014A35C(Task* task);
static void func_actor_213000_8014A488(Task* task);

/// countdown, hides the model, then spawns the four children of the spawn
/// table -- entries 1 and 2 attached to part 8 and parked at `field_4BC` /
/// `field_4C0`, entry 3 attached to part 9 and entry 4 to part 12. Each of the
/// last two has its model's `tpage` / `clut` loaded from the `AreaPlacement` of
/// the current area selected by the model id the parent's `spawnArg2` carries
/// at `Enemy::placeKey >> ENEMY_PLACE_INDEX_SHIFT`, and has its texture stream processed twice
/// when it has a buffer. It then publishes the work block's matrices on the
/// model, installs the message table and `Gp_EnemyTaskExit` as the exit
/// callback, and advances to the tick. A failed allocation exits the task
/// instead.
static void func_actor_213000_80149E54(Task* task)
{
    Actor213000Work* work;
    TmdObject*       obj;
    GameLocationKey  key;
    Task*            spawned1;
    Task*            spawned2;

    obj  = task->extra.tmd;
    work = (Actor213000Work*)memCalloc(0x4C4, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_478 = 0;
    work->field_477 = -1;
    obj->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->field_4BC = Task_SpawnFromTable(D_actor_213000_80157DE0, 1, 8, task);
    work->field_4C0 = Task_SpawnFromTable(D_actor_213000_80157DE0, 2, 8, task);
    spawned1        = Task_SpawnFromTable(D_actor_213000_80157DE0, 3, 9, task);
    spawned2        = Task_SpawnFromTable(D_actor_213000_80157DE0, 4, 0xC, task);
    if (spawned1 != NULL) {
        TmdObject*       model;
        GpAreaVariant*   rec;
        AreaPlacement*   place;
        GameLocationKey* sessionKey;
        s32              idx;

        idx        = ((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        model      = spawned1->extra.tmd;
        sessionKey = &gGameSession->location.loc;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        key.view   = sessionKey->view;
        areaSyncLocationVariant(&key);
        rec                      = Gp_GetNestedAreaRec(&key);
        place                    = gpAreaPlaceAt(rec->field_0, idx);
        model->texturePageOffset = place->texturePageOffset;
        model->clutRowOffset     = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    if (spawned2 != NULL) {
        TmdObject*       model;
        GpAreaVariant*   rec;
        AreaPlacement*   place;
        GameLocationKey* sessionKey;
        s32              idx;

        model      = spawned2->extra.tmd;
        idx        = ((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionKey = &gGameSession->location.loc;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        key.view   = sessionKey->view;
        areaSyncLocationVariant(&key);
        rec                      = Gp_GetNestedAreaRec(&key);
        place                    = gpAreaPlaceAt(rec->field_0, idx);
        model->texturePageOffset = place->texturePageOffset;
        model->clutRowOffset     = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    func_actor_213000_8014A6AC(task);
    task->msgTable     = D_actor_213000_80157E1C;
    task->exitCallback = Gp_EnemyTaskExit;
    task->state++;
}

/// State table of the children spawned from table entries 1 and 2: attach to
/// the parent, idle, kill.
static const TaskFuncTable3 D_actor_213000_80149E24 = {
    {
        modelPlacementAttachPart,
        func_actor_213000_8014A158,
        taskKill,
    },
};

/// Body of the children spawned from table entries 1 and 2: dispatches on
/// the state through `D_actor_213000_80149E24`.
void func_actor_213000_8014A084(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E24;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// The idle state of the children spawned from table entries 1 and 2: does
/// nothing.
static void func_actor_213000_8014A158(Task* task)
{
}

/// State table of the child spawned from table entry 4: setup, tick, kill.
static const TaskFuncTable3 D_actor_213000_80149E30 = {
    {
        modelPlacementAttachChild,
        modelPlacementMirrorParent,
        taskKill,
    },
};

/// Body of the child spawned from table entry 4: dispatches on the state
/// through `D_actor_213000_80149E30`.
void func_actor_213000_8014A160(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E30;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

/// Setup state of the child spawned from table entry 3: hangs each of the
/// child's three root coordinates off the parent's part nine slots above it
/// (parts 9 to 11) with a zero local transform, shares the parent's light and
/// colour matrices, mirrors the parent's model bits 0x80 (hidden) and 0x4 as
/// the tick state does, draws the model at order-table offset -4, reparents
/// the task under the parent and steps to the tick state.
static void func_actor_213000_8014A35C(Task* task)
{
    Task*      parent;
    TmdObject* obj;
    TmdObject* parentObj;
    GfxCoord*  coords;
    GfxCoord*  root;
    s32        i;
    u16        flags;

    parent    = task->spawnArg2.pointer;
    obj       = task->extra.tmd;
    parentObj = parent->extra.tmd;
    for (i = 0; i < 3; i++) {
        coords             = &(parent->extra.tmd->coords)[i + 9];
        root               = &(task->extra.tmd->coords)[i];
        root->parent       = coords;
        root->coord.t[0]   = 0;
        root->coord.t[1]   = 0;
        root->coord.t[2]   = 0;
        root->param.rot.vx = 0;
        root->param.rot.vy = 0;
        root->param.rot.vz = 0;
        root->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    obj->lightMtx = parentObj->lightMtx;
    obj->colorMtx = parentObj->colorMtx;
    flags         = obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    obj->flags    = flags;
    if (!(parentObj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        obj->flags = flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (!(parentObj->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        obj->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        Tmd_AllocBuffers(obj);
    } else {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    obj->otOffset = -4;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// A second copy, under this file's own name.
#define modelPlacementMirrorParent func_actor_213000_8014A488
#include "../../shared/model_placement_mirror_parent.inc.c"
#undef modelPlacementMirrorParent

/// State table of the child spawned from table entry 3: setup, tick, kill.
static const TaskFuncTable3 D_actor_213000_80149E3C = {
    {
        func_actor_213000_8014A35C,
        func_actor_213000_8014A488,
        taskKill,
    },
};

/// Body of the child spawned from table entry 3: dispatches on the state
/// through `D_actor_213000_80149E3C`.
void func_actor_213000_8014A520(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E3C;
    sp.funcs[task->state](task);
}

/// The actor's three states: spawn, per-frame tick and teardown.
static const TaskFuncTable3 D_actor_213000_80149E48 = {
    {
        func_actor_213000_80149E54,
        func_actor_213000_8014A5D0,
        Gp_EnemyTaskExit,
    },
};

/// Body of the actor's task (spawn table entry 0): dispatches on the state
/// through `D_actor_213000_80149E48`.
void func_actor_213000_8014A578(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E48;
    sp.funcs[task->state](task);
}

/// Per-frame tick: ticks the work block's animation slots once a preset has
/// started them, and once the view is ready rebuilds model part 1's world
/// matrix and hands its translation to `func_800D7A9C`. The work block's
/// countdown then frees the model's buffers as it reaches zero.
static void func_actor_213000_8014A5D0(Task* task)
{
    Actor213000Work* work;
    TmdObject*       extra;
    GfxCoord*        coords;
    s32              i;

    extra  = task->extra.tmd;
    work   = (Actor213000Work*)task->work;
    coords = &extra->coords[1];
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    if (gGameSession->viewReady != 0) {
        coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coords);
        func_800D7A9C(extra, (VECTOR*)coords->workm.t, 0, 3);
    }
    if (work->field_477 >= 0) {
        if (work->field_477 == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_477--;
    }
}

/// Points the model's light and colour matrices at the work block's own pair,
/// then rebuilds model part 1's world matrix and hands its translation to
/// `func_800D7A9C`.
static void func_actor_213000_8014A6AC(Task* task)
{
    Actor213000Work* work;
    GfxCoord*        coords;
    TmdObject*       extra;

    work                   = (Actor213000Work*)task->work;
    extra                  = task->extra.tmd;
    coords                 = extra->coords;
    extra->lightMtx        = &work->light;
    extra->colorMtx        = &work->color;
    coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

/// Applies the requested animation bank and clip to this actor's rig.
///
/// A changed bank installs its set table. The requested clip is applied to the slots.
/// Requested blending uses 6 frames; otherwise the slots reset.
s32 func_actor_213000_8014A70C(Task* task, s32 arg1, AnimationPlayRequest* msg)
{
    Actor213000Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor213000Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->field_476) {
        work->field_476 = msg->source.index;
        work->field_475 = -1;
        func_800B3F84(&work->rig.anim, D_actor_213000_80157DDC[work->field_476], ext, work->rig.poses,
                      work->rig.slots);
    }
    work->field_475 = msg->animationId;
    if (msg->blend != ANIMATION_BLEND_RESET) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->rig.anim, i, work->field_475, 0, 6);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->rig.anim, i, work->field_475);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    work->field_474 = 1;
    return 0;
}

#include "../../shared/actor_messages_place_euler.inc.c"

/// Message-0x7D5 display handler, switching on the message's mode word. Mode
/// 0 hides the model and clears `TMD_OBJECT_SKIP_AUTO_BUFFER`; 1 shows it, reallocates its buffers
/// through `Tmd_AllocBuffers` and clears `TMD_OBJECT_SKIP_AUTO_BUFFER`; 2 hides it, sets
/// `TMD_OBJECT_SKIP_AUTO_BUFFER` and starts
/// the work block's countdown at 2, after which the tick frees the buffers; 3
/// shows it and sets `TMD_OBJECT_SKIP_AUTO_BUFFER`. The handled modes return 0; any other mode changes
/// nothing and returns 1.
/// The handler reads `work` before the switch even though mode 2 is its only
/// use, so retail's `lw $v1,0x1C($a0)` sits in the entry block.
s32 func_actor_213000_8014A8A4(Task* task, s32 arg1, s32 mode)
{
    TmdObject*       obj;
    Actor213000Work* work;
    s32              ret;

    obj  = task->extra.tmd;
    work = (Actor213000Work*)task->work;
    ret  = 0;

    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->field_477 = mode;
            obj->flags     |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message-0x7DB handler: shows or hides the models of the two children the
/// work block parks at `field_4BC` / `field_4C0`. Mode 0 shows the first
/// (clears bit 0x80 of its `TmdObject::flags`) and 1 hides it; 2 and 3 show
/// and hide the second. A missing child or an unknown mode touches nothing.
/// Every path returns 0.
/// Both `|= 0x80` arms are written out in the source; the post-reload `jump2`
/// cross-jump folds mode 1's copy into mode 3's, which is why retail's mode-1
/// arm is only the `lw` plus a jump while modes 0 and 2 each keep their own
/// `& 0xFF7F` copy. Which tails jump2 merges is decided by which jumps share a
/// target label, not by how alike the bodies are.
s32 func_actor_213000_8014A980(Task* task, s32 arg1, ActorCommand* msg)
{
    Actor213000Work* work;
    Task*            child;
    u16              mode;

    mode = msg->command;
    work = (Actor213000Work*)task->work;

    switch (mode) {
        case 0:
            child = work->field_4BC;
            if (child != NULL) {
                child->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case 1:
            child = work->field_4BC;
            if (child != NULL) {
                child->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case 2:
            child = work->field_4C0;
            if (child != NULL) {
                child->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case 3:
            child = work->field_4C0;
            if (child != NULL) {
                child->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
    }
    return 0;
}
