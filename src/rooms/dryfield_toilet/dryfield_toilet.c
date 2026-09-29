#include "rooms/dryfield_toilet.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "dryfield_toilet_private.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_targets.h"

#include "main/fs.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

/// The room task's message table (published in `Task::msgTable` for
/// `Gp_DispatchMsg` to walk) and the four-byte payload `func_dryfield_toilet_8017D940`
/// hands that call as `arg2`.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, s32, s32);
        s32 (*call2)(s32, s32, RoomEventMsg*, RoomEventMsg*);
    } handler;
} DryfieldToiletMessageEntry;
STATIC_ASSERT_SIZEOF(DryfieldToiletMessageEntry, 8);

extern DryfieldToiletMessageEntry D_dryfield_toilet_801802A4[6];
extern s32                        D_dryfield_toilet_801802D4;

/// The template the room's collision grid is restored from, and the grid
/// itself.
extern GpGridParams D_dryfield_toilet_80180314;

static void func_dryfield_toilet_8017D940(Task* arg0);
static void func_dryfield_toilet_8017D9D4(Task* task);

/// The room task's three states: entry, idle and `taskKill`.
static const TaskFuncTable3 D_dryfield_toilet_8017D5C4 = {
    { func_dryfield_toilet_8017D940, func_dryfield_toilet_8017D9D4, taskKill },
};

s32 func_dryfield_toilet_8017D810(s32, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_toilet_8017D884(Task*, s32, s32, s32);
s32 func_dryfield_toilet_8017D8B8(void);
s32 func_dryfield_toilet_8017D8C0(void);
s32 func_dryfield_toilet_8017D8C8(s32, s32, RoomEventMsg*, RoomEventMsg*);

DryfieldToiletMessageEntry D_dryfield_toilet_801802A4[6] = {
    { 5102, { .call2 = func_dryfield_toilet_8017D810 } },
    { 5105, { .call0 = func_dryfield_toilet_8017D8B8 } },
    { 5103, { .call2 = func_dryfield_toilet_8017D8C8 } },
    { 5104, { .call0 = func_dryfield_toilet_8017D8C0 } },
    { 5106, { .call1 = func_dryfield_toilet_8017D884 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

s32 D_dryfield_toilet_801802D4 = 4098;

SVECTOR D_dryfield_toilet_801802D8[1] = {
#include "assets/dryfield_toilet_collision_02D54_normals.inc"
};

SVECTOR D_dryfield_toilet_801802E0[4] = {
#include "assets/dryfield_toilet_collision_02D54_verts.inc"
};

GpGridFace D_dryfield_toilet_80180300[1] = {
#include "assets/dryfield_toilet_collision_02D54_faces.inc"
};

s16 D_dryfield_toilet_8018030C[2] = {
#include "assets/dryfield_toilet_collision_02D54_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_toilet_8018030C[i])
s16* D_dryfield_toilet_80180310[1] = {
#include "assets/dryfield_toilet_collision_02D54_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_toilet_80180314 = { NULL, D_dryfield_toilet_801802D8, D_dryfield_toilet_801802E0, D_dryfield_toilet_80180300, D_dryfield_toilet_80180310, 2250, 590, 1, 1, 4000, 1 };

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[6];
    AnimationPackedRotation        words[18];
} DryfieldToiletPoseBank2D78;

DryfieldToiletPoseBank2D78 D_dryfield_toilet_80180338 = { .poses = {
#include "assets/dryfield_toilet_animation_03054_bank1.inc"
};

AnimationPackedRotation D_dryfield_toilet_80180380[46] = {
#include "assets/dryfield_toilet_animation_03054_bank4.inc"
};

GpAnimRec D_dryfield_toilet_80180438[109] = {
#include "assets/dryfield_toilet_animation_03054_records.inc"
};

u16 D_dryfield_toilet_801805EC[20] = {
#include "assets/dryfield_toilet_animation_03054_indices.inc"
};

GpAnimSet D_dryfield_toilet_80180614 = {
    D_dryfield_toilet_80180438,
    D_dryfield_toilet_801805EC,
    { NULL, D_dryfield_toilet_80180338, NULL, NULL, D_dryfield_toilet_80180380, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[9];
    AnimationPackedRotation        words[27];
} DryfieldToiletPoseBank307C;

DryfieldToiletPoseBank307C D_dryfield_toilet_8018063C = { .poses = {
#include "assets/dryfield_toilet_animation_035A4_bank1.inc"
};

AnimationPackedRotation D_dryfield_toilet_801806A8[131] = {
#include "assets/dryfield_toilet_animation_035A4_bank4.inc"
};

GpAnimRec D_dryfield_toilet_801808B4[162] = {
#include "assets/dryfield_toilet_animation_035A4_records.inc"
};

u16 D_dryfield_toilet_80180B3C[20] = {
#include "assets/dryfield_toilet_animation_035A4_indices.inc"
};

GpAnimSet D_dryfield_toilet_80180B64 = {
    D_dryfield_toilet_801808B4,
    D_dryfield_toilet_80180B3C,
    { NULL, D_dryfield_toilet_8018063C, NULL, NULL, D_dryfield_toilet_801806A8, NULL, NULL, NULL },
};

static void func_dryfield_toilet_8017D5E4(void);

/// Restores one face of the room's collision grid (its normal, four corners and
/// face record) from the template, then slides the four corners 2000 units toward
/// negative x once game flag nibble 0x60 is set.
static void func_dryfield_toilet_8017D5E4(void)
{
    GpGridParams* geom = &D_dryfield_toilet_80181404;
    GpGridParams* src  = &D_dryfield_toilet_80180314;
    s32           i;

    for (i = 0; i < 1; i++) {
        geom->field_4[i].vx         = src->field_4[i].vx;
        geom->field_4[i].vy         = src->field_4[i].vy;
        geom->field_4[i].vz         = src->field_4[i].vz;
        geom->field_8[i * 4 + 0].vx = src->field_8[i * 4 + 0].vx;
        geom->field_8[i * 4 + 0].vy = src->field_8[i * 4 + 0].vy;
        geom->field_8[i * 4 + 0].vz = src->field_8[i * 4 + 0].vz;
        geom->field_8[i * 4 + 1].vx = src->field_8[i * 4 + 1].vx;
        geom->field_8[i * 4 + 1].vy = src->field_8[i * 4 + 1].vy;
        geom->field_8[i * 4 + 1].vz = src->field_8[i * 4 + 1].vz;
        geom->field_8[i * 4 + 2].vx = src->field_8[i * 4 + 2].vx;
        geom->field_8[i * 4 + 2].vy = src->field_8[i * 4 + 2].vy;
        geom->field_8[i * 4 + 2].vz = src->field_8[i * 4 + 2].vz;
        geom->field_8[i * 4 + 3].vx = src->field_8[i * 4 + 3].vx;
        geom->field_8[i * 4 + 3].vy = src->field_8[i * 4 + 3].vy;
        geom->field_8[i * 4 + 3].vz = src->field_8[i * 4 + 3].vz;
        geom->field_C[i]            = src->field_C[i];
    }
    if (GameFlag_GetNibble(0x60) != 0) {
        for (i = 0; i < 4; i++) {
            geom->field_8[i].vx -= 2000;
        }
    }
}

/// Message handler that answers query 0xF: copies the incoming record onto the
/// outgoing one and, unless the query is report-only (`field_5` set), replies
/// with game flag nibble 0x61 plus one. Always returns 1, leaving the message
/// unconsumed.
s32 func_dryfield_toilet_8017D810(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->prefix.packed == 0xF && in->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    return 1;
}

/// Queues stage sound `0x52100005` when `arg2` is 5; otherwise does nothing.
s32 func_dryfield_toilet_8017D884(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 5) {
        Gp_EnqueueStageSnd6(0x52100000 | 5, 0, 0);
    }
    return 0;
}

s32 func_dryfield_toilet_8017D8B8(void)
{
    return 0;
}

s32 func_dryfield_toilet_8017D8C0(void)
{
    return 0;
}

/// Handler for message `0x13EF` in the room's `(msgId, handler)` table - the
/// direction record `Gp_PostMsg13EF` posts. On the visit whose sub-id
/// (`field_2`) is 1, that agrees with the session's own sub-id
/// (`gGameSession::at4.loc.place`) and that has not yet latched nibble 0x60, the
/// toilet starts its cutscene pair and latches the nibble. The outgoing record
/// is never written: this handler only consumes the message.
s32 func_dryfield_toilet_8017D8C8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 subId = in->field_2;

    if (subId == 1 && GameFlag_GetNibble(0x60) == 0 && gGameSession->at4.loc.place == subId) {
        func_800E8634(D_dryfield_toilet_80180C58, 1, D_dryfield_toilet_80180F40);
        GameFlag_SetNibble(0x60, 1);
    }
    return 0;
}

/// The room task's entry state: publish the message table, claim game pointer
/// slot 7, and on the visit that agrees with the session's sub-id
/// (`gGameSession::at4.loc.place` == 1) and has not yet latched nibble 0x60, post
/// message `0x7DA` with the room's payload and run the scene setup. Advance to
/// the next state either way.
static void func_dryfield_toilet_8017D940(Task* arg0)
{
    arg0->msgTable = D_dryfield_toilet_801802A4;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x60) == 0 && gGameSession->at4.loc.place == 1) {
        Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &D_dryfield_toilet_801802D4, 0x7DB);
        func_dryfield_toilet_8017D5E4();
    }
    arg0->state = arg0->state + 1;
}

/// The room task's idle state, entry 1 of its state table: does nothing but
/// open and close a stack frame.
static void func_dryfield_toilet_8017D9D4(Task* task)
{
    char pad[0x10];
}

/// The room task's update: runs the handler for its current state from a stack
/// copy of the room's state table.
void func_dryfield_toilet_8017D9E4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_toilet_8017D5C4;
    sp.funcs[task->state](task);
}

/// With a zero argument, restores one face of the room's collision grid (its
/// normal, four corners and face record) from the template; otherwise slides
/// the grid's four corners 2000 units toward negative x.
void func_dryfield_toilet_8017DA3C(s32 arg0)
{
    GpGridParams* geom = &D_dryfield_toilet_80181404;
    GpGridParams* src  = &D_dryfield_toilet_80180314;
    s32           i;

    if (arg0 == 0) {
        for (i = 0; i < 1; i++) {
            geom->field_4[i].vx         = src->field_4[i].vx;
            geom->field_4[i].vy         = src->field_4[i].vy;
            geom->field_4[i].vz         = src->field_4[i].vz;
            geom->field_8[i * 4 + 0].vx = src->field_8[i * 4 + 0].vx;
            geom->field_8[i * 4 + 0].vy = src->field_8[i * 4 + 0].vy;
            geom->field_8[i * 4 + 0].vz = src->field_8[i * 4 + 0].vz;
            geom->field_8[i * 4 + 1].vx = src->field_8[i * 4 + 1].vx;
            geom->field_8[i * 4 + 1].vy = src->field_8[i * 4 + 1].vy;
            geom->field_8[i * 4 + 1].vz = src->field_8[i * 4 + 1].vz;
            geom->field_8[i * 4 + 2].vx = src->field_8[i * 4 + 2].vx;
            geom->field_8[i * 4 + 2].vy = src->field_8[i * 4 + 2].vy;
            geom->field_8[i * 4 + 2].vz = src->field_8[i * 4 + 2].vz;
            geom->field_8[i * 4 + 3].vx = src->field_8[i * 4 + 3].vx;
            geom->field_8[i * 4 + 3].vy = src->field_8[i * 4 + 3].vy;
            geom->field_8[i * 4 + 3].vz = src->field_8[i * 4 + 3].vz;
            geom->field_C[i]            = src->field_C[i];
        }
    } else {
        for (i = 0; i < 4; i++) {
            geom->field_8[i].vx -= 2000;
        }
    }
}

void func_dryfield_toilet_8017DC50(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

void func_dryfield_toilet_8017DC70(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_dryfield_toilet_8017DC90(void)
{
    Gp_RestoreStreamRng();
}

void func_dryfield_toilet_8017DCB0(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_dryfield_toilet_8017DCD0(s32 arg0)
{
    Gp_ArmStateF0(arg0);
}
