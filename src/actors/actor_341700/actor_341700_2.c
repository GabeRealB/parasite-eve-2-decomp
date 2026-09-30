#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_dumping_hole.h"
#include "../../shared/coord_math.h"
#include "../../shared/actor_messages.h"

/// The overlay's *other* work block, for the task `func_actor_341700_8016D130`
/// starts: that function calls `memCalloc(0x80, 0)` and stores the result in
/// the same `Task::work` slot, so the two blocks never coexist on one task.
///
/// `field_0` is the state index `func_actor_341700_8016CC9C` dispatches
/// through its three-entry handler table; `field_2` holds the previous value
/// and `field_4` the state-change flag. `func_actor_341700_8016CEB4` copies
/// the three leading bytes of an incoming command over `field_18` .. `field_1A`.
///
/// `light` / `color` are the matrices this block is allocated for:
/// `func_actor_341700_8016D130` stores their addresses into the model's
/// `TmdObject.lightMtx` / `colorMtx` light and colour matrix slots, so the
/// actor rasterises through its own work block rather than a separate
/// `MATRIX` allocation.
typedef struct Actor341700SubWork {
    /* 0x00 */ s16    field_0;
    /* 0x02 */ s16    field_2;
    /* 0x04 */ s16    field_4;
    /* 0x06 */ s16    field_6;
    /* 0x08 */ byte   pad_8[0x10];
    /* 0x18 */ u8     field_18;
    /* 0x19 */ u8     field_19;
    /* 0x1A */ u8     field_1A;
    /* 0x1B */ byte   pad_1B[0x1];
    /* 0x1C */ MATRIX light;
    /* 0x3C */ MATRIX color;
    /* 0x5C */ byte   pad_5C[0x24];
} Actor341700SubWork;
STATIC_ASSERT_SIZEOF(Actor341700SubWork, 0x80);

/// Whole-unit part of the last movement step `func_actor_341700_8016B804`
/// applied, rounded away from zero when the step had a fraction.
static SVECTOR ActorContact_ScratchPosition;

extern SVECTOR D_actor_341700_80175F7C[];
extern u8      D_actor_341700_801760FC[];
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, ActorCommand* request);
        s32 (*call1)(Task*, s32, ActorTransform*);
        s32 (*call2)(Task*, s32, s32);
    } handler;
} Actor3417002MessageEntry;
STATIC_ASSERT_SIZEOF(Actor3417002MessageEntry, 8);

extern Actor3417002MessageEntry D_actor_341700_80175F5C[4]; // stored into `Task::msgTable` by func_actor_341700_8016D130

/// Psy-Q `RotMatrixY`.

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static void func_actor_341700_8016D130(Enemy* arg0, Task* arg1);
static void func_actor_341700_8016D2B8(Enemy* arg0, Task* arg1);
static void func_actor_341700_8016D2E8(Enemy* arg0, Task* arg1);

extern TmdSource D_actor_341700_80175F38;
static void      func_actor_341700_8016D32C(Task*);

s32 func_actor_341700_8016CE28(Task*, s32, s32);
s32 func_actor_341700_8016CEB4(Task*, s32, ActorCommand* cmd);

#include "../../shared/actor_contacts.h"

TmdBone D_actor_341700_80174DC4[11] = {
#include "assets/actor_341700_model_14118_skeleton.inc"
};

u32 D_actor_341700_80174F50[11] = {
#include "assets/actor_341700_model_14118_partVerts.inc"
};

SVECTOR D_actor_341700_80174F7C[81] = {
#include "assets/actor_341700_model_14118_verts.inc"
};

SVECTOR D_actor_341700_80175204[46] = {
#include "assets/actor_341700_model_14118_normals.inc"
};

u32 D_actor_341700_80175374[753] = {
#include "assets/actor_341700_model_14118_stream.inc"
};

TmdSource D_actor_341700_80175F38 = {
    0,
    3184,
    1720,
    11,
    D_actor_341700_80174F50,
    D_actor_341700_80174F7C,
    D_actor_341700_80175204,
    D_actor_341700_80174DC4,
    D_actor_341700_80175374,
};

Actor3417002MessageEntry D_actor_341700_80175F5C[4] = {
    { 2005, { .call2 = func_actor_341700_8016CE28 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = func_actor_341700_8016CEB4 } },
    { 2004, { .call1 = actorMsgPlace } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

SVECTOR D_actor_341700_80175F7C[48] = {
    { 0, 0, 0, 0 },
    { 0, -2500, 3000, 0 },
    { 200, -2400, 2500, 0 },
    { 0, -2500, 2000, 0 },
    { 200, -2400, 1500, 0 },
    { 0, -2500, 1000, 0 },
    { 200, -2400, 500, 0 },
    { 0, -2500, 0, 0 },
    { 200, -2400, -500, 0 },
    { 0, -2500, -1000, 0 },
    { 200, -2400, -1500, 0 },
    { 0, -2500, -2000, 0 },
    { 200, -2400, -2500, 0 },
    { 0, -2500, -3000, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 1500, -1500, 3000, 0 },
    { 1700, -1100, 2500, 0 },
    { 1500, -1500, 2000, 0 },
    { 1700, -1100, 1500, 0 },
    { 1500, -1500, 1000, 0 },
    { 1700, -1100, 500, 0 },
    { 1500, -1500, 0, 0 },
    { 1700, -1100, -500, 0 },
    { 1500, -1500, -1000, 0 },
    { 1700, -1100, -1500, 0 },
    { 1500, -1500, -2000, 0 },
    { 1700, -1100, -2500, 0 },
    { 1500, -1500, -3000, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 2500, -500, 3000, 0 },
    { 2700, -400, 2500, 0 },
    { 2500, -500, 2000, 0 },
    { 2700, -400, 1500, 0 },
    { 2500, -500, 1000, 0 },
    { 2700, -400, 500, 0 },
    { 2500, -500, 0, 0 },
    { 2700, -400, -500, 0 },
    { 2500, -500, -1000, 0 },
    { 2700, -400, -1500, 0 },
    { 2500, -500, -2000, 0 },
    { 2700, -400, -2500, 0 },
    { 2500, -500, -3000, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

u8 D_actor_341700_801760FC[600] = {
    2,
    0,
    0,
    0,
    19,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    22,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    23,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    33,
    0,
    0,
    0,
    19,
    0,
    0,
    0,
    36,
    0,
    0,
    0,
    21,
    0,
    0,
    0,
    40,
    0,
    0,
    0,
    27,
    9,
    0,
    0,
    45,
    0,
    0,
    0,
    44,
    0,
    0,
    0,
    19,
    0,
    0,
    0,
    33,
    0,
    0,
    0,
    27,
    0,
    0,
    0,
    39,
    0,
    0,
    0,
    21,
    0,
    0,
    0,
    45,
    0,
    0,
    0,
    21,
    0,
    0,
    0,
    8,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    3,
    7,
    0,
    0,
    20,
    0,
    0,
    0,
    41,
    25,
    0,
    0,
    22,
    0,
    0,
    0,
    27,
    9,
    0,
    0,
    34,
    0,
    0,
    0,
    42,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    37,
    0,
    0,
    0,
    27,
    0,
    0,
    0,
    41,
    0,
    0,
    0,
    22,
    0,
    0,
    0,
    33,
    0,
    0,
    0,
    37,
    0,
    0,
    0,
    40,
    0,
    0,
    0,
    34,
    0,
    0,
    0,
    3,
    7,
    0,
    0,
    18,
    0,
    0,
    0,
    35,
    41,
    0,
    0,
    26,
    9,
    0,
    0,
    38,
    0,
    0,
    0,
    23,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    40,
    0,
    0,
    0,
    28,
    0,
    0,
    0,
    35,
    0,
    0,
    0,
    22,
    0,
    0,
    0,
    42,
    0,
    0,
    0,
    23,
    0,
    0,
    0,
    40,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    3,
    7,
    0,
    0,
    36,
    0,
    0,
    0,
    38,
    0,
    0,
    0,
    27,
    9,
    0,
    0,
    34,
    0,
    0,
    0,
    42,
    45,
    0,
    0,
    36,
    0,
    0,
    0,
    37,
    40,
    0,
    0,
    27,
    42,
    0,
    0,
    41,
    0,
    0,
    0,
    22,
    39,
    0,
    0,
    33,
    0,
    0,
    0,
    21,
    33,
    0,
    0,
    8,
    40,
    0,
    0,
    2,
    45,
    0,
    0,
    3,
    7,
    0,
    0,
    18,
    0,
    0,
    0,
    35,
    25,
    0,
    0,
    26,
    9,
    0,
    0,
    38,
    0,
    0,
    0,
    23,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    40,
    0,
    0,
    0,
    28,
    0,
    0,
    0,
    35,
    0,
    0,
    0,
    22,
    0,
    0,
    0,
    42,
    0,
    0,
    0,
    23,
    0,
    0,
    0,
    8,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    3,
    7,
    0,
    0,
    20,
    0,
    0,
    0,
    22,
    0,
    0,
    0,
    27,
    9,
    0,
    0,
    34,
    0,
    0,
    0,
    42,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    37,
    0,
    0,
    0,
    27,
    0,
    0,
    0,
    41,
    0,
    0,
    0,
    22,
    0,
    0,
    0,
    33,
    0,
    0,
    0,
    21,
    0,
    0,
    0,
    8,
    0,
    0,
    0,
    34,
    0,
    0,
    0,
    3,
    7,
    0,
    0,
    50,
    0,
    0,
    0,
    35,
    25,
    0,
    0,
    58,
    9,
    0,
    0,
    38,
    0,
    0,
    0,
    23,
    0,
    0,
    0,
    52,
    0,
    0,
    0,
    40,
    0,
    0,
    0,
    28,
    0,
    0,
    0,
    35,
    0,
    0,
    0,
    38,
    42,
    0,
    0,
    42,
    0,
    0,
    0,
    23,
    45,
    0,
    0,
    40,
    36,
    0,
    0,
    34,
    0,
    0,
    0,
    3,
    7,
    0,
    0,
    20,
    40,
    0,
    0,
    38,
    41,
    0,
    0,
    43,
    9,
    0,
    0,
    34,
    43,
    0,
    0,
    42,
    0,
    0,
    0,
    20,
    45,
    0,
    0,
    37,
    0,
    0,
    0,
    27,
    45,
    0,
    0,
    41,
    33,
    0,
    0,
    22,
    0,
    0,
    0,
    33,
    0,
    0,
    0,
    21,
    36,
    0,
    0,
    8,
    0,
    0,
    0,
    34,
    0,
    0,
    0,
    3,
    7,
    0,
    0,
    18,
    43,
    0,
    0,
    35,
    25,
    0,
    0,
    26,
    42,
    0,
    0,
    38,
    0,
    0,
    0,
    39,
    0,
    0,
    0,
    36,
    45,
    0,
    0,
    40,
    45,
    0,
    0,
    44,
    0,
    0,
    0,
    35,
    0,
    0,
    0,
    22,
    0,
    0,
    0,
    42,
    0,
    0,
    0,
    23,
    0,
    0,
    0,
    8,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    35,
    39,
    0,
    0,
    20,
    0,
    0,
    0,
};

TaskDesc D_actor_341700_80176354 = { (TASK_BODY_TMD | 0x100), 96, func_actor_341700_8016D32C, { .model = &D_actor_341700_80175F38 } };

static SVECTOR ActorContact_ScratchPosition;

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

static void func_actor_341700_8016C0F4(Enemy* arg0, Task* arg1);
static void func_actor_341700_8016CC9C(Enemy* arg0, Task* arg1);

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

static void func_actor_341700_8016C0F4(Enemy* arg0, Task* arg1)
{
    SVECTOR             vec;
    Actor341700SubWork* work = (Actor341700SubWork*)arg1->work;
    s16                 i;
    s16                 r;
    u32                 t;
    SVECTOR*            table;
    u8*                 p;
    u32                 rnd;
    u32                 index;
    SVECTOR*            vecPtr;
    u8*                 indices;

    if (work->field_4 != 0) {
        work->field_6                       = 0;
        arg1->extra.tmd->coords->coord.t[1] = 0x708;
    }
    work->field_6++;
    if (arg1->extra.tmd->coords->coord.t[1] > 200) {
        if (work->field_6 >= 0xA6) {
            if (work->field_6 % 4 < 2) {
                arg1->extra.tmd->coords->coord.t[1] += 90;
            } else {
                arg1->extra.tmd->coords->coord.t[1] -= 100;
            }
            if (work->field_6 % 6 < 3) {
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[1].coord, 12, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[2].coord, 24, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[3].coord, -24, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[4].coord, 12, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[5].coord, -6, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[6].coord, 18, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[8].coord, -6, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[9].coord, -3, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[10].coord, -27, 0);
            } else {
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[1].coord, -12, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[2].coord, -24, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[3].coord, 24, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[4].coord, -12, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[5].coord, 6, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[6].coord, -18, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[8].coord, 6, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[9].coord, 3, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[10].coord, 27, 0);
            }
        } else if (work->field_6 >= 0x79) {
            if (work->field_6 % 4 < 2) {
                arg1->extra.tmd->coords->coord.t[1] += 50;
            } else {
                arg1->extra.tmd->coords->coord.t[1] -= 58;
            }
            if (work->field_6 % 6 < 3) {
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[1].coord, 8, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[2].coord, 16, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[3].coord, -16, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[4].coord, 8, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[5].coord, -4, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[6].coord, 12, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[8].coord, -4, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[9].coord, -2, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[10].coord, -18, 0);
            } else {
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[1].coord, -8, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[2].coord, -16, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[3].coord, 16, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[4].coord, -8, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[5].coord, 4, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[6].coord, -12, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[8].coord, 4, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[9].coord, 2, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[10].coord, 18, 0);
            }
        } else if (work->field_6 >= 11 && work->field_6 < 18) {
            if (work->field_6 % 4 < 2) {
                arg1->extra.tmd->coords->coord.t[1] += 30;
            } else {
                arg1->extra.tmd->coords->coord.t[1] -= 33;
            }
            if (work->field_6 % 6 < 3) {
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[1].coord, 4, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[2].coord, 8, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[3].coord, -8, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[4].coord, 4, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[5].coord, -2, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[6].coord, 6, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[8].coord, -2, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[9].coord, -1, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[10].coord, -9, 0);
            } else {
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[1].coord, -4, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[2].coord, -8, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[3].coord, 8, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[4].coord, -4, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[5].coord, 2, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[6].coord, -6, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[8].coord, 2, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[9].coord, 1, 0);
                Gfx_RotMatrixX(&arg1->extra.tmd->coords[10].coord, 9, 0);
            }
            switch ((work->field_6 - 11) % 8) {
                case 0:
                    vec = D_actor_341700_80175F7C[35];
                    func_shelter_b3_dumping_hole_8017FCF4(arg1->extra.tmd->coords, &vec);
                    break;
                case 1:
                    vec = D_actor_341700_80175F7C[8];
                    func_shelter_b3_dumping_hole_8017FCF4(arg1->extra.tmd->coords, &vec);
                    break;
                case 2:
                    vec = D_actor_341700_80175F7C[25];
                    func_shelter_b3_dumping_hole_8017FCF4(arg1->extra.tmd->coords, &vec);
                    break;
                case 4:
                    vec = D_actor_341700_80175F7C[24];
                    func_shelter_b3_dumping_hole_8017FCF4(arg1->extra.tmd->coords, &vec);
                    break;
                case 5:
                    vec = D_actor_341700_80175F7C[26];
                    func_shelter_b3_dumping_hole_8017FCF4(arg1->extra.tmd->coords, &vec);
                    break;
            }
        }
    } else {
        if (work->field_6 % 4 < 2) {
            arg1->extra.tmd->coords->coord.t[1] += 100;
        } else {
            arg1->extra.tmd->coords->coord.t[1] -= 100;
        }
        if (work->field_6 % 6 < 3) {
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[1].coord, 12, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[2].coord, 24, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[3].coord, -24, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[4].coord, 12, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[5].coord, -6, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[6].coord, 18, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[8].coord, -6, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[9].coord, -3, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[10].coord, -27, 0);
        } else {
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[1].coord, -12, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[2].coord, -24, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[3].coord, 24, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[4].coord, -12, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[5].coord, 6, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[6].coord, -18, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[8].coord, 6, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[9].coord, 3, 0);
            Gfx_RotMatrixX(&arg1->extra.tmd->coords[10].coord, 27, 0);
        }
    }
    arg1->extra.tmd->coords[0].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[2].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[3].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[4].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[5].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[6].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[7].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[8].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[9].composeStamp  = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[10].composeStamp = GRAPHICS_COORD_DIRTY;
    i                                        = 0;
    indices                                  = D_actor_341700_801760FC;
    table                                    = D_actor_341700_80175F7C;
    vecPtr                                   = &vec;
    for (; i < 4; i++) {
        if (work->field_6 > 240) {
            return;
        }
        if (work->field_6 < 120) {
            return;
        }
        p = (u8*)((i + (work->field_6 - 120) * 4) + (u32)indices);
        if (*p == 0) {
            return;
        }
        rnd         = Gp_LcgState * 5 + 0x71357911;
        t           = (rnd >> 16) & 0xFF;
        index       = *p;
        r           = t;
        Gp_LcgState = rnd;
        vec         = table[index];
        if (r % 2) {
            r = -t;
        }
        vec.vx += r / 2;
        vec.vy -= r;
        vec.vz += r;
        func_shelter_b3_dumping_hole_8017FCF4(arg1->extra.tmd->coords, vecPtr);
    }
}

/// Three state handlers, indexed by `Actor341700SubWork::field_0`; copied onto
/// the stack before dispatch.
static const GpEnemyTaskFuncTable3 D_actor_341700_80162058 = { {
    func_actor_341700_8016D2B8,
    func_actor_341700_8016D2E8,
    func_actor_341700_8016C0F4,
} };

/// Per-frame callback of the `func_actor_341700_8016D130` task. It colours the
/// model from the world position of its *second* attach coordinate and then,
/// unless `Gp_StateF0.field_4` hides the model, runs the handler `Actor341700SubWork::
/// field_0` names.
///
/// `case 0` is folded into `default` on purpose. The two bodies are the same,
/// so the case list keeps three nodes and GCC's tree tests `case 1` at the
/// root; dropping the case makes `case 2` the root and the emitted branches
/// come out with the wrong polarity and a stray low-bound test.
static void func_actor_341700_8016CC9C(Enemy* arg0, Task* arg1)
{
    VECTOR                block;
    Actor341700SubWork*   work = (Actor341700SubWork*)arg1->work;
    GpEnemyTaskFuncTable3 sp   = D_actor_341700_80162058;

    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg1->extra.tmd->coords[1]);
    block.vx = arg1->extra.tmd->coords[1].workm.t[0];
    block.vy = arg1->extra.tmd->coords[1].workm.t[1];
    block.vz = arg1->extra.tmd->coords[1].workm.t[2];
    Gp_UpdateActorColor(arg0, &block, 0, 0);
    switch (Gp_StateF0.field_4) {
        case 2:
            arg1->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case 1:
            return;
        case 0:
        default:
            if (work->field_2 != work->field_0) {
                work->field_4 = 1;
            } else {
                work->field_4 = 0;
            }
            work->field_2 = work->field_0;
            sp.funcs[work->field_0](arg0, arg1);
            if (gGameSession->viewReady != 0) {
                arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            return;
    }
}

/// Task-state handlers of the `func_actor_341700_8016D130` task: set-up, the
/// per-frame callback, teardown. `func_actor_341700_8016D32C` dispatches them
/// on `Task::state`.
static const GpEnemyTaskFuncTable3 D_actor_341700_80162064 = { {
    func_actor_341700_8016D130,
    func_actor_341700_8016CC9C,
    Gp_DestroyEnemy,
} };

s32 func_actor_341700_8016CE28(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj = task->extra.tmd;

    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            break;
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags = TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// The `0x2704` command handler, reached through the task's `Task::msgTable`
/// table (`D_actor_341700_80175F5C`): the three leading bytes of `cmd` are
/// copied over `Actor341700SubWork::field_18` .. `field_1A` and the second
/// halfword, when the opcode matches, picks the state the work block moves to.
///
/// `case 2` is folded into `default` on purpose. The two bodies are the same,
/// so the case list keeps three nodes and GCC's decision tree balances around
/// `case 1`; dropping `case 2` makes `case 0` the root and the emitted branches
/// come out in a different order.
s32 func_actor_341700_8016CEB4(Task* task, s32 arg1, ActorCommand* cmd)
{
    Actor341700SubWork* work = (Actor341700SubWork*)task->work;

    work->field_18 = cmd->context.loc.stage;
    work->field_19 = cmd->context.loc.area;
    work->field_1A = (u8)cmd->command;

    if (cmd->context.key == 0x2704) {
        switch (cmd->command) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 1:
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->field_0                         = 2;
                break;
            case 2:
            default:
                work->field_0 = 0;
                task->state   = 1;
                break;
        }
    }
    return 1;
}

#include "../../shared/actor_messages_place.inc.c"

#include "../../shared/coord_math_yaw_scale.inc.c"

static void func_actor_341700_8016D130(Enemy* arg0, Task* arg1)
{
    Actor341700SubWork* work;
    Actor341700SubWork* workAllocation;
    TmdObject*          model;
    GfxCoord*           coord;
    VECTOR              block;

    model          = arg1->extra.tmd;
    coord          = model->coords;
    workAllocation = memCalloc(sizeof(*workAllocation), false);
    work           = workAllocation;
    arg1->work     = workAllocation;
    if (workAllocation == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    coord->parent                         = &gGfxViewCoord;
    arg1->extra.tmd->coords->coord.t[1]   = 0;
    arg1->extra.tmd->coords->coord.t[0]   = 0x1388;
    arg1->extra.tmd->coords->coord.t[2]   = -0x1770;
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    arg1->msgTable                        = D_actor_341700_80175F5C;
    arg0->field_4                         = &coord->coord;
    arg0->field_48                        = 0;
    arg0->bodyPos.vx                      = 0;
    arg0->bodyPos.vy                      = 0;
    arg0->bodyPos.vz                      = 0;
    arg0->coord                           = &arg1->extra.tmd->coords[2];
    arg0->node.state.parts.flags          = WORLD_TARGET_NOT_LOCKABLE;
    arg0->reactionFlags                   = 0;
    arg0->hpMax                           = 0;
    arg0->hp                              = 0;
    model->lightMtx                       = &work->light;
    model->colorMtx                       = &work->color;
    coord->composeStamp                   = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    block.vx = coord->workm.t[0];
    block.vy = coord->workm.t[1];
    block.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &block, 0, 0);
    work->field_2 = -1;
    work->field_0 = 1;
    arg1->state  += 1;
}

static void func_actor_341700_8016D2B8(Enemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((Actor341700SubWork*)arg1->work)->field_4 != 0) {
        model                        = arg1->extra.tmd;
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                 = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    }
}

static void func_actor_341700_8016D2E8(Enemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((Actor341700SubWork*)arg1->work)->field_4 != 0) {
        model                        = arg1->extra.tmd;
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                 = 0;
        Tmd_AllocBuffers(model);
    }
}

/// Runs the controlled enemy's current state handler from
/// `D_actor_341700_80162064` - spawn/setup, per-frame tick or teardown -
/// copying the table onto the stack before the call.
static void func_actor_341700_8016D32C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_341700_80162064;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
