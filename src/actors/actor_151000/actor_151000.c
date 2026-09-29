#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        GpAnimSet* sets[3];
        GpAnimArg  arguments[6];
    } data;
    s32 words[33];
} Actor151000AnimStorage336C;
STATIC_ASSERT_SIZEOF(Actor151000AnimStorage336C, 132);

extern Actor151000AnimStorage336C D_actor_151000_8013336C;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// Reset argument the "start animation" opcode leaves behind:
/// `func_actor_151000_801326AC` forwards it to every reseeded slot, and the
/// runner sets it to 10 when a walk ends.
extern s16 D_actor_151000_8013D2AC;

/// Fade countdown: `func_actor_151000_80131EE0` seeds it, and the fade task
/// `func_actor_151000_80131E24` draws while it is non-zero.
extern s32 D_actor_151000_8013D378;

/// The enemy's work block, published by its spawn handler and by its task
/// body.
extern Actor151000Work* D_actor_151000_8013D37C;

/// The enemy's task, published by its spawn handler so the visibility opcode
/// can reach its model.
extern Task* D_actor_151000_8013D380;

/// Picks the distance the runner walks the model each frame: 0 steps 0x3C
/// forward, 1 steps 0xF back, 2 steps 0x19 forward. Set by the "walk to"
/// opcode.
extern s16 D_actor_151000_8013D384;

/// Descriptor of the fade task `func_actor_151000_80131E24`.
extern TaskDesc D_actor_151000_80133360;

/// The enemy's message table and the animation data its work block's slots
/// are seeded from.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, GpAnimArg*, s32);
        s32 (*call1)(Task*, s32, GpCmdArg*);
        s32 (*call2)(Task*, s32, GpXformArg*);
        s32 (*call3)(Task*, s32, VECTOR*, s32);
        s32 (*call4)(Task*, s32, s32);
    } handler;
} Actor151000MsgEntry;
STATIC_ASSERT_SIZEOF(Actor151000MsgEntry, 8);

extern Actor151000MsgEntry D_actor_151000_8013D2B0[];
extern u8                  D_actor_151000_8013D2EC[];

static void func_actor_151000_80132084(Task* task);
static void func_actor_151000_80132450(GpEnemy* enemy, Task* task);
static void func_actor_151000_801324D4(Task* task);
static void func_actor_151000_801324FC(Task* task);
static void func_actor_151000_801325C4(void);
static void func_actor_151000_80132610(void);
static void func_actor_151000_801326AC(void);
static void func_actor_151000_80132A38(Task* task);

extern TmdSource D_actor_151000_80139270;
void             func_actor_151000_801323F4(Task*);

s32 func_actor_151000_80132738(Task*, s32, GpAnimArg*, s32);
s32 func_actor_151000_801327C8(Task*, s32, s32);
s32 func_actor_151000_80132810(Task*, s32, GpXformArg*);
s32 func_actor_151000_8013288C(Task*, s32, GpCmdArg*);
s32 func_actor_151000_801328DC(Task*, s32, VECTOR*, s32);

extern GpAnimArg D_actor_151000_801333F0;
extern GpAnimArg D_actor_151000_80133404;
void             func_actor_151000_80131EE0(s32);

void func_actor_151000_80131E24(Task*);

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor151000PoseBankCB4;

Actor151000PoseBankCB4 D_actor_151000_80132AD4 = { .poses = {
#include "assets/actor_151000_animation_00ED8_bank1.inc"
} };

GpPackedSvec D_actor_151000_80132AEC[25] = {
#include "assets/actor_151000_animation_00ED8_bank4.inc"
};

GpAnimRec D_actor_151000_80132B50[96] = {
#include "assets/actor_151000_animation_00ED8_records.inc"
};

u16 D_actor_151000_80132CD0[20] = {
#include "assets/actor_151000_animation_00ED8_indices.inc"
};

GpAnimSet D_actor_151000_80132CF8 = {
    D_actor_151000_80132B50, D_actor_151000_80132CD0,
    { NULL, D_actor_151000_80132AD4.words, NULL, NULL, D_actor_151000_80132AEC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} Actor151000PoseBankF00;

Actor151000PoseBankF00 D_actor_151000_80132D20 = { .poses = {
#include "assets/actor_151000_animation_012A0_bank1.inc"
} };

GpPackedSvec D_actor_151000_80132D68[76] = {
#include "assets/actor_151000_animation_012A0_bank4.inc"
};

GpAnimRec D_actor_151000_80132E98[128] = {
#include "assets/actor_151000_animation_012A0_records.inc"
};

u16 D_actor_151000_80133098[20] = {
#include "assets/actor_151000_animation_012A0_indices.inc"
};

GpAnimSet D_actor_151000_801330C0 = {
    D_actor_151000_80132E98, D_actor_151000_80133098,
    { NULL, D_actor_151000_80132D20.words, NULL, NULL, D_actor_151000_80132D68, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} Actor151000PoseBank12C8;

Actor151000PoseBank12C8 D_actor_151000_801330E8 = { .poses = {
#include "assets/actor_151000_animation_01518_bank1.inc"
} };

GpPackedSvec D_actor_151000_8013310C[45] = {
#include "assets/actor_151000_animation_01518_bank4.inc"
};

GpAnimRec D_actor_151000_801331C0[84] = {
#include "assets/actor_151000_animation_01518_records.inc"
};

u16 D_actor_151000_80133310[20] = {
#include "assets/actor_151000_animation_01518_indices.inc"
};

GpAnimSet D_actor_151000_80133338 = {
    D_actor_151000_801331C0, D_actor_151000_80133310,
    { NULL, D_actor_151000_801330E8.words, NULL, NULL, D_actor_151000_8013310C, NULL, NULL, NULL },
};

TaskDesc D_actor_151000_80133360 = { 0, 192, func_actor_151000_80131E24, { .model = NULL } };

Actor151000AnimStorage336C D_actor_151000_8013336C = { .data = { { &D_actor_151000_80132CF8, &D_actor_151000_801330C0, &D_actor_151000_80133338 }, { { { .index = 1 }, 47, 0, 0, 0 }, { { .index = 1 }, 47, 0, 0, 0 }, { { .index = 1 }, 48, 0, 0, 0 }, { { .index = 1 }, 49, 0, 0, 0 }, { { .index = 1 }, 0, 0, 0, 0 }, { { .index = 1 }, 16, 0, 0, 0 } } } };

GpAnimArg D_actor_151000_801333F0 = { { .index = 1 }, 17, 0, 0, 0 };

GpAnimArg D_actor_151000_80133404 = { { .index = 1 }, 19, 1, 8, 0 };

GpAnimArg D_actor_151000_80133418[2] = {
    { { .index = 0 }, 13, 0, 0, 0 },
    { { .index = 0 }, 14, 0, 0, 0 },
};

GpAnimArg D_actor_151000_80133440 = { { .index = 0 }, 15, 0, 0, 0 };

GpAnimArg D_actor_151000_80133454 = { { .index = 1 }, 1, 0, 0, 0 };

GpCmdArg D_actor_151000_80133468 = { { .loc = { 5, 15 } }, 1 };

GpXformArg D_actor_151000_8013346C = { { -7700, 0, -0x46B4, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_actor_151000_80133484 = { { -6300, 0, -0x4330, 0 }, { 0, 227, 0, 0 } };

GpXformArg D_actor_151000_8013349C = { { -7480, 0, -0x44B6, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_actor_151000_801334B4 = { { -4900, 0, -0x3CBE, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_actor_151000_801334CC = { { -3600, 0, -0x30D4, 0 }, { 0, 0, 0, 0 } };

GpCopyArg D_actor_151000_801334E4 = { { .words = D_actor_151000_8013336C.words }, 32 };

GpEvsCmd D_actor_151000_801334EC[47] = {
    { 47, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_151000_801334E4 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_151000_8013346C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_151000_8013349C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_actor_151000_80133468 }, { .value = 0 } },
    { 13, { .callback = func_actor_151000_80131EE0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_151000_8013336C.data.arguments[1] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_151000_8013336C.data.arguments[5] }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x550F0007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x550F0008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 1 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_151000_80131EE0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_151000_80133404 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_151000_801333F0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_151000_8013336C.data.arguments[2] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_151000_80133484 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_151000_8013336C.data.arguments[3] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_151000_801334B4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_151000_80133440 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_actor_151000_801334CC }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_151000_8013336C.data.arguments[5] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_151000_80133454 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_151000_80133954[15] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_151000_80131EE0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_151000_8013336C.data.arguments[5] }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_151000_80133454 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_151000_80133484 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_actor_151000_80133ABC[19] = {
#include "assets/actor_151000_model_07450_skeleton.inc"
};

u32 D_actor_151000_80133D68[19] = {
#include "assets/actor_151000_model_07450_partVerts.inc"
};

SVECTOR D_actor_151000_80133DB4[365] = {
#include "assets/actor_151000_model_07450_verts.inc"
};

SVECTOR D_actor_151000_8013491C[385] = {
#include "assets/actor_151000_model_07450_normals.inc"
};

u32 D_actor_151000_80135524[3923] = {
#include "assets/actor_151000_model_07450_stream.inc"
};

TmdSource D_actor_151000_80139270 = {
    0, 21760, 5992, 19,
    D_actor_151000_80133D68, D_actor_151000_80133DB4, D_actor_151000_8013491C, D_actor_151000_80133ABC, D_actor_151000_80135524,
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor151000PoseBank7474;

Actor151000PoseBank7474 D_actor_151000_80139294 = { .poses = {
#include "assets/actor_151000_animation_07660_bank1.inc"
} };

GpPackedSvec D_actor_151000_801392AC[23] = {
#include "assets/actor_151000_animation_07660_bank4.inc"
};

GpAnimRec D_actor_151000_80139308[84] = {
#include "assets/actor_151000_animation_07660_records.inc"
};

u16 D_actor_151000_80139458[20] = {
#include "assets/actor_151000_animation_07660_indices.inc"
};

GpAnimSet D_actor_151000_80139480 = {
    D_actor_151000_80139308, D_actor_151000_80139458,
    { NULL, D_actor_151000_80139294.words, NULL, NULL, D_actor_151000_801392AC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} Actor151000PoseBank7688;

Actor151000PoseBank7688 D_actor_151000_801394A8 = { .poses = {
#include "assets/actor_151000_animation_07A58_bank1.inc"
} };

GpPackedSvec D_actor_151000_801394FC[75] = {
#include "assets/actor_151000_animation_07A58_bank4.inc"
};

GpAnimRec D_actor_151000_80139628[138] = {
#include "assets/actor_151000_animation_07A58_records.inc"
};

u16 D_actor_151000_80139850[20] = {
#include "assets/actor_151000_animation_07A58_indices.inc"
};

GpAnimSet D_actor_151000_80139878 = {
    D_actor_151000_80139628, D_actor_151000_80139850,
    { NULL, D_actor_151000_801394A8.words, NULL, NULL, D_actor_151000_801394FC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[22];
    GpPackedSvec words[66];
} Actor151000PoseBank7A80;

Actor151000PoseBank7A80 D_actor_151000_801398A0 = { .poses = {
#include "assets/actor_151000_animation_0863C_bank1.inc"
} };

GpPackedSvec D_actor_151000_801399A8[298] = {
#include "assets/actor_151000_animation_0863C_bank4.inc"
};

GpAnimRec D_actor_151000_80139E50[377] = {
#include "assets/actor_151000_animation_0863C_records.inc"
};

u16 D_actor_151000_8013A434[20] = {
#include "assets/actor_151000_animation_0863C_indices.inc"
};

GpAnimSet D_actor_151000_8013A45C = {
    D_actor_151000_80139E50, D_actor_151000_8013A434,
    { NULL, D_actor_151000_801398A0.words, NULL, NULL, D_actor_151000_801399A8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} Actor151000PoseBank8664;

Actor151000PoseBank8664 D_actor_151000_8013A484 = { .poses = {
#include "assets/actor_151000_animation_088A4_bank1.inc"
} };

GpPackedSvec D_actor_151000_8013A4B4[48] = {
#include "assets/actor_151000_animation_088A4_bank4.inc"
};

GpAnimRec D_actor_151000_8013A574[74] = {
#include "assets/actor_151000_animation_088A4_records.inc"
};

u16 D_actor_151000_8013A69C[20] = {
#include "assets/actor_151000_animation_088A4_indices.inc"
};

GpAnimSet D_actor_151000_8013A6C4 = {
    D_actor_151000_8013A574, D_actor_151000_8013A69C,
    { NULL, D_actor_151000_8013A484.words, NULL, NULL, D_actor_151000_8013A4B4, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} Actor151000PoseBank88CC;

Actor151000PoseBank88CC D_actor_151000_8013A6EC = { .poses = {
#include "assets/actor_151000_animation_08CAC_bank1.inc"
} };

GpPackedSvec D_actor_151000_8013A728[76] = {
#include "assets/actor_151000_animation_08CAC_bank4.inc"
};

GpAnimRec D_actor_151000_8013A858[147] = {
#include "assets/actor_151000_animation_08CAC_records.inc"
};

u16 D_actor_151000_8013AAA4[20] = {
#include "assets/actor_151000_animation_08CAC_indices.inc"
};

GpAnimSet D_actor_151000_8013AACC = {
    D_actor_151000_8013A858, D_actor_151000_8013AAA4,
    { NULL, D_actor_151000_8013A6EC.words, NULL, NULL, D_actor_151000_8013A728, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} Actor151000PoseBank8CD4;

Actor151000PoseBank8CD4 D_actor_151000_8013AAF4 = { .poses = {
#include "assets/actor_151000_animation_090B8_bank1.inc"
} };

GpPackedSvec D_actor_151000_8013AB30[89] = {
#include "assets/actor_151000_animation_090B8_bank4.inc"
};

GpAnimRec D_actor_151000_8013AC94[135] = {
#include "assets/actor_151000_animation_090B8_records.inc"
};

u16 D_actor_151000_8013AEB0[20] = {
#include "assets/actor_151000_animation_090B8_indices.inc"
};

GpAnimSet D_actor_151000_8013AED8 = {
    D_actor_151000_8013AC94, D_actor_151000_8013AEB0,
    { NULL, D_actor_151000_8013AAF4.words, NULL, NULL, D_actor_151000_8013AB30, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} Actor151000PoseBank90E0;

Actor151000PoseBank90E0 D_actor_151000_8013AF00 = { .poses = {
#include "assets/actor_151000_animation_093C8_bank1.inc"
} };

GpPackedSvec D_actor_151000_8013AF30[59] = {
#include "assets/actor_151000_animation_093C8_bank4.inc"
};

GpAnimRec D_actor_151000_8013B01C[105] = {
#include "assets/actor_151000_animation_093C8_records.inc"
};

u16 D_actor_151000_8013B1C0[20] = {
#include "assets/actor_151000_animation_093C8_indices.inc"
};

GpAnimSet D_actor_151000_8013B1E8 = {
    D_actor_151000_8013B01C, D_actor_151000_8013B1C0,
    { NULL, D_actor_151000_8013AF00.words, NULL, NULL, D_actor_151000_8013AF30, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor151000PoseBank93F0;

Actor151000PoseBank93F0 D_actor_151000_8013B210 = { .poses = {
#include "assets/actor_151000_animation_09620_bank1.inc"
} };

GpPackedSvec D_actor_151000_8013B228[30] = {
#include "assets/actor_151000_animation_09620_bank4.inc"
};

GpAnimRec D_actor_151000_8013B2A0[94] = {
#include "assets/actor_151000_animation_09620_records.inc"
};

u16 D_actor_151000_8013B418[20] = {
#include "assets/actor_151000_animation_09620_indices.inc"
};

GpAnimSet D_actor_151000_8013B440 = {
    D_actor_151000_8013B2A0, D_actor_151000_8013B418,
    { NULL, D_actor_151000_8013B210.words, NULL, NULL, D_actor_151000_8013B228, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor151000PoseBank9648;

Actor151000PoseBank9648 D_actor_151000_8013B468 = { .poses = {
#include "assets/actor_151000_animation_098B0_bank1.inc"
} };

GpPackedSvec D_actor_151000_8013B480[48] = {
#include "assets/actor_151000_animation_098B0_bank4.inc"
};

GpAnimRec D_actor_151000_8013B540[90] = {
#include "assets/actor_151000_animation_098B0_records.inc"
};

u16 D_actor_151000_8013B6A8[20] = {
#include "assets/actor_151000_animation_098B0_indices.inc"
};

GpAnimSet D_actor_151000_8013B6D0 = {
    D_actor_151000_8013B540, D_actor_151000_8013B6A8,
    { NULL, D_actor_151000_8013B468.words, NULL, NULL, D_actor_151000_8013B480, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} Actor151000PoseBank98D8;

Actor151000PoseBank98D8 D_actor_151000_8013B6F8 = { .poses = {
#include "assets/actor_151000_animation_09C98_bank1.inc"
} };

GpPackedSvec D_actor_151000_8013B734[59] = {
#include "assets/actor_151000_animation_09C98_bank4.inc"
};

GpAnimRec D_actor_151000_8013B820[156] = {
#include "assets/actor_151000_animation_09C98_records.inc"
};

u16 D_actor_151000_8013BA90[20] = {
#include "assets/actor_151000_animation_09C98_indices.inc"
};

GpAnimSet D_actor_151000_8013BAB8 = {
    D_actor_151000_8013B820, D_actor_151000_8013BA90,
    { NULL, D_actor_151000_8013B6F8.words, NULL, NULL, D_actor_151000_8013B734, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} Actor151000PoseBank9CC0;

Actor151000PoseBank9CC0 D_actor_151000_8013BAE0 = { .poses = {
#include "assets/actor_151000_animation_09F48_bank1.inc"
} };

GpPackedSvec D_actor_151000_8013BB04[29] = {
#include "assets/actor_151000_animation_09F48_bank4.inc"
};

GpAnimRec D_actor_151000_8013BB78[114] = {
#include "assets/actor_151000_animation_09F48_records.inc"
};

u16 D_actor_151000_8013BD40[20] = {
#include "assets/actor_151000_animation_09F48_indices.inc"
};

GpAnimSet D_actor_151000_8013BD68 = {
    D_actor_151000_8013BB78, D_actor_151000_8013BD40,
    { NULL, D_actor_151000_8013BAE0.words, NULL, NULL, D_actor_151000_8013BB04, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor151000PoseBank9F70;

Actor151000PoseBank9F70 D_actor_151000_8013BD90 = { .poses = {
#include "assets/actor_151000_animation_0A104_bank1.inc"
} };

GpPackedSvec D_actor_151000_8013BDA8[20] = {
#include "assets/actor_151000_animation_0A104_bank4.inc"
};

GpAnimRec D_actor_151000_8013BDF8[65] = {
#include "assets/actor_151000_animation_0A104_records.inc"
};

u16 D_actor_151000_8013BEFC[20] = {
#include "assets/actor_151000_animation_0A104_indices.inc"
};

GpAnimSet D_actor_151000_8013BF24 = {
    D_actor_151000_8013BDF8, D_actor_151000_8013BEFC,
    { NULL, D_actor_151000_8013BD90.words, NULL, NULL, D_actor_151000_8013BDA8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} Actor151000PoseBankA12C;

Actor151000PoseBankA12C D_actor_151000_8013BF4C = { .poses = {
#include "assets/actor_151000_animation_0A4EC_bank1.inc"
} };

GpPackedSvec D_actor_151000_8013BF70[65] = {
#include "assets/actor_151000_animation_0A4EC_bank4.inc"
};

GpAnimRec D_actor_151000_8013C074[156] = {
#include "assets/actor_151000_animation_0A4EC_records.inc"
};

u16 D_actor_151000_8013C2E4[20] = {
#include "assets/actor_151000_animation_0A4EC_indices.inc"
};

GpAnimSet D_actor_151000_8013C30C = {
    D_actor_151000_8013C074, D_actor_151000_8013C2E4,
    { NULL, D_actor_151000_8013BF4C.words, NULL, NULL, D_actor_151000_8013BF70, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[18];
    GpPackedSvec words[54];
} Actor151000PoseBankA514;

Actor151000PoseBankA514 D_actor_151000_8013C334 = { .poses = {
#include "assets/actor_151000_animation_0AE9C_bank1.inc"
} };

GpPackedSvec D_actor_151000_8013C40C[232] = {
#include "assets/actor_151000_animation_0AE9C_bank4.inc"
};

GpAnimRec D_actor_151000_8013C7AC[314] = {
#include "assets/actor_151000_animation_0AE9C_records.inc"
};

u16 D_actor_151000_8013CC94[20] = {
#include "assets/actor_151000_animation_0AE9C_indices.inc"
};

GpAnimSet D_actor_151000_8013CCBC = {
    D_actor_151000_8013C7AC, D_actor_151000_8013CC94,
    { NULL, D_actor_151000_8013C334.words, NULL, NULL, D_actor_151000_8013C40C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} Actor151000PoseBankAEC4;

Actor151000PoseBankAEC4 D_actor_151000_8013CCE4 = { .poses = {
#include "assets/actor_151000_animation_0B218_bank1.inc"
} };

GpPackedSvec D_actor_151000_8013CD14[68] = {
#include "assets/actor_151000_animation_0B218_bank4.inc"
};

GpAnimRec D_actor_151000_8013CE24[123] = {
#include "assets/actor_151000_animation_0B218_records.inc"
};

u16 D_actor_151000_8013D010[20] = {
#include "assets/actor_151000_animation_0B218_indices.inc"
};

GpAnimSet D_actor_151000_8013D038 = {
    D_actor_151000_8013CE24, D_actor_151000_8013D010,
    { NULL, D_actor_151000_8013CCE4.words, NULL, NULL, D_actor_151000_8013CD14, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor151000PoseBankB240;

Actor151000PoseBankB240 D_actor_151000_8013D060 = { .poses = {
#include "assets/actor_151000_animation_0B464_bank1.inc"
} };

GpPackedSvec D_actor_151000_8013D078[27] = {
#include "assets/actor_151000_animation_0B464_bank4.inc"
};

GpAnimRec D_actor_151000_8013D0E4[94] = {
#include "assets/actor_151000_animation_0B464_records.inc"
};

u16 D_actor_151000_8013D25C[20] = {
#include "assets/actor_151000_animation_0B464_indices.inc"
};

GpAnimSet D_actor_151000_8013D284 = {
    D_actor_151000_8013D0E4, D_actor_151000_8013D25C,
    { NULL, D_actor_151000_8013D060.words, NULL, NULL, D_actor_151000_8013D078, NULL, NULL, NULL },
};

s16 D_actor_151000_8013D2AC = 8;

Actor151000MsgEntry D_actor_151000_8013D2B0[6] = {
    { 2003, { .call0 = func_actor_151000_80132738 } },
    { 2005, { .call4 = func_actor_151000_801327C8 } },
    { 2004, { .call2 = func_actor_151000_80132810 } },
    { 2011, { .call1 = func_actor_151000_8013288C } },
    { 2013, { .call3 = func_actor_151000_801328DC } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_151000_8013D2E0 = { 1, 192, func_actor_151000_801323F4, { .model = &D_actor_151000_80139270 } };

u8 D_actor_151000_8013D2EC[140] = {
    0, 0, 0, 0, 196, 166, 19, 128, 204, 170, 19, 128, 216, 174, 19, 128,
    232, 177, 19, 128, 64, 180, 19, 128, 208, 182, 19, 128, 184, 186, 19, 128,
    104, 189, 19, 128, 0, 0, 0, 0, 36, 191, 19, 128, 0, 0, 0, 0,
    0, 0, 0, 0, 128, 148, 19, 128, 120, 152, 19, 128, 92, 164, 19, 128,
    12, 195, 19, 128, 188, 204, 19, 128, 132, 210, 19, 128, 56, 208, 19, 128,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

s32 D_actor_151000_8013D378;

Actor151000Work * D_actor_151000_8013D37C;

Task* D_actor_151000_8013D380;

s16 D_actor_151000_8013D384;

static void func_actor_151000_80131F1C(GpEnemy* enemy, Task* task);

/// The fade task: while the countdown `D_actor_151000_8013D378` is non-zero,
/// draws a full-screen black `TILE` into ordering table slot 0xA; once it is
/// zero the task kills itself.
void func_actor_151000_80131E24(Task* task)
{
    TILE* tile;

    if (D_actor_151000_8013D378 != 0) {
        tile           = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        SetTile(tile);
        tile->r0 = 0;
        tile->g0 = 0;
        tile->b0 = 0;
        tile->x0 = -0xA0;
        tile->y0 = -0x80;
        tile->w  = 0x140;
        tile->h  = 0x100;
        addPrim(gGpuCurrentOt + 0xA, tile);
    } else {
        taskKill(task);
    }
}

/// Starts a fade to black lasting `frames` frames: seeds the countdown and,
/// unless it is zero, spawns the fade task.
void func_actor_151000_80131EE0(s32 frames)
{
    D_actor_151000_8013D378 = frames;
    if (frames != 0) {
        Task_SpawnFromTable(&D_actor_151000_80133360, 0, 0, 0);
    }
}

/// State 0 of the enemy's task: allocates the work block, publishes it in
/// `D_actor_151000_8013D37C` and on the task's work slot, points the model's
/// light and colour matrices and its animation context at it, publishes the
/// task in `D_actor_151000_8013D380`, then runs the runner once and advances
/// the task to state 1.
///
/// Every access to the block after the null check goes through the global
/// rather than the `memCalloc` result, which is why the pointer is reloaded at
/// each use.
static void func_actor_151000_80131F1C(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor151000Work* work;
    TmdObject*       obj;
    GpCoord*         coord;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(0x4C0, 0);
    D_actor_151000_8013D37C = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_151000_801324D4;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->lightMtx                = &D_actor_151000_8013D37C->light;
    obj->colorMtx                = &D_actor_151000_8013D37C->color;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    D_actor_151000_8013D380      = task;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_151000_8013D37C->rig.anim, D_actor_151000_8013D2EC, obj,
                  &D_actor_151000_8013D37C->rig.poses, D_actor_151000_8013D37C->rig.slots);
    D_actor_151000_8013D37C->st.animId  = 1;
    D_actor_151000_8013D37C->st.state   = 2;
    D_actor_151000_8013D37C->st.travel  = 0;
    D_actor_151000_8013D37C->turnFrames = 0;
    D_actor_151000_8013D37C->stepRec    = 0;
    D_actor_151000_8013D37C->footsteps  = 0;
    task->msgTable                      = D_actor_151000_8013D2B0;
    func_actor_151000_80132084(task);
    task->state += 1;
}

/// Per-frame update: states 1 and 2 run their one-shot animation restart and
/// leave the work block in state 3; state 3 walks the model while `travel`
/// counts down (distance picked by `D_actor_151000_8013D384`) and, when the
/// walk ends, queues clip 0xD through state 1; it turns the model while
/// `turnFrames` counts down in clip 3, then ticks the animation and, once
/// `footsteps` is set, plays the footsteps.
static void func_actor_151000_80132084(Task* task)
{
    GpCoord*         coord = task->extra.tmd->coords;
    Actor151000Work* work  = (Actor151000Work*)task->work;

    if (D_actor_151000_8013D37C->st.state == 1) {
        func_actor_151000_801326AC();
        D_actor_151000_8013D37C->st.state = 3;
    } else if (D_actor_151000_8013D37C->st.state == 2) {
        func_actor_151000_80132610();
        D_actor_151000_8013D37C->st.state = 3;
    } else if (D_actor_151000_8013D37C->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (D_actor_151000_8013D384) {
                    case 0:
                        actorMoveModelForward(task, 0x3C);
                        break;
                    case 1:
                        actorMoveModelForward(task, -0xF);
                        break;
                    case 2:
                        actorMoveModelForward(task, 0x19);
                        break;
                }
                if (--work->st.travel == 0) {
                    work->st.state          = 1;
                    D_actor_151000_8013D2AC = 10;
                    work->st.animId         = 0xD;
                }
            }
        }
        if (work->st.animId == 3 && work->turnFrames != 0) {
            work->st.yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
            coord->flg = 0;
            work->turnFrames--;
        }
        func_actor_151000_801325C4();
        if (work->footsteps != 0) {
            func_actor_151000_801324FC(task);
        }
    }
}

/// The enemy's task body: publishes the task's work block in
/// `D_actor_151000_8013D37C`, then runs the handler for the task's state from a
/// table built on the stack - the spawn handler `func_actor_151000_80131F1C`,
/// then the per-frame `func_actor_151000_80132450`.
void func_actor_151000_801323F4(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_151000_80131F1C,
        func_actor_151000_80132450,
    };

    D_actor_151000_8013D37C = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// State 1 of the enemy's task: refreshes the model root's coordinate, hands
/// `func_800D7A9C` the point 0x320 above it, then runs the runner and draws the
/// ground shadow.
static void func_actor_151000_80132450(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR     vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_151000_80132084(task);
    func_actor_151000_80132A38(task);
}

/// Exit callback the spawn handler installs on the enemy's task: tears down
/// the enemy the task was spawned for.
static void func_actor_151000_801324D4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Plays a step sound whenever animation slot 1 rolls onto a new record whose
/// flags nibble is 0x10 or 0x20 - the two feet - panned and attenuated from
/// the second coordinate of the task's model. The record is latched in
/// `stepRec` so each one fires once.
static void func_actor_151000_801324FC(Task* task)
{
    Actor151000Work* work;
    GpCoord*         obj;
    GpAnimRec*       rec;
    s32              kind;
    s32              id;
    s32              pan;

    work = (Actor151000Work*)task->work;
    obj  = task->extra.tmd->coords + 1;
    rec  = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
    if (rec == NULL || rec == work->stepRec) {
        return;
    }
    work->stepRec = rec;
    kind          = rec->flags & 0x30;
    if (kind != 0x10 && kind != 0x20) {
        return;
    }
    id = 0x1000000F;
    if (kind == 0x10) {
        id = 0x10000010;
    }
    id += 0x64;
    pan = (s8)Gp_GetObjPan(obj);
    SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(obj));
}

/// Ticks animation slots 1..0x12 of the enemy's animation context.
static void func_actor_151000_801325C4(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_151000_8013D37C->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Resets animation slots 1..0x12 to clip `animId` at rate 1, without a
/// reset argument, and latches the clip into `st.appliedAnimId`. Clears the footstep
/// check's record first.
static void func_actor_151000_80132610(void)
{
    s32 i;

    D_actor_151000_8013D37C->stepRec = NULL;
    i                                = 1;
    do {
        D_actor_151000_8013D37C->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_151000_8013D37C->rig.anim, i, D_actor_151000_8013D37C->st.animId);
        i++;
    } while (i < 0x13);
    D_actor_151000_8013D37C->st.appliedAnimId = D_actor_151000_8013D37C->st.animId;
}

/// Starts animation slots 1..0x12 on clip `animId`, forwarding
/// `D_actor_151000_8013D2AC` as the reset argument, and latches the clip into
/// `st.appliedAnimId`. Clears the footstep check's record first.
static void func_actor_151000_801326AC(void)
{
    s32 i;

    D_actor_151000_8013D37C->stepRec = NULL;
    i                                = 1;
    do {
        func_800B4114(&D_actor_151000_8013D37C->rig.anim, i, D_actor_151000_8013D37C->st.animId, 0,
                      D_actor_151000_8013D2AC);
        i++;
    } while (i < 0x13);
    D_actor_151000_8013D37C->st.appliedAnimId = D_actor_151000_8013D37C->st.animId;
}

/// "Start animation" opcode: `withArg` selects between the two start paths the
/// runner `func_actor_151000_80132084` dispatches on, and only the first carries
/// `animArg`, which it leaves in `D_actor_151000_8013D2AC`. The runner is then
/// run once on the task published in `D_actor_151000_8013D380`. Returns -1,
/// without touching the work block, when the clip id is 0x23 or more.
s32 func_actor_151000_80132738(Task* task, s32 arg1, GpAnimArg* args, s32 arg3)
{
    if (args->field_4 < 0x23) {
        D_actor_151000_8013D37C->st.animId = args->field_4;
        if (args->field_8 != 0) {
            D_actor_151000_8013D37C->st.state = 1;
            D_actor_151000_8013D2AC           = args->field_C;
        } else {
            D_actor_151000_8013D37C->st.state = 2;
        }
        D_actor_151000_8013D37C->st.field_6 = 0;
        func_actor_151000_80132084(D_actor_151000_8013D380);
        return 0;
    }
    return -1;
}

/// Visibility opcode: applies `arg2` to the model of the task published in
/// `D_actor_151000_8013D380` - bit 0 shows it (flags 0) rather than hiding it
/// (0x80), and bit 1 ORs in 0x4.
s32 func_actor_151000_801327C8(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = D_actor_151000_8013D380->extra.tmd;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = 0x80;
    }
    if (arg2 & 2) {
        obj->flags |= 4;
    }
    return 0;
}

/// Placement opcode: yaws the model's root coordinate to `placement->rot.vy`,
/// caching that yaw in the work block, then drops the placement translation
/// into the matrix and marks it dirty.
s32 func_actor_151000_80132810(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord* coord;
    u16      yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_151000_8013D37C->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message handler: message 0 arms the turn countdown `turnFrames` at 0x14
/// frames, message 1 sets `footsteps`, which turns the footsteps on. Anything else does nothing.
s32 func_actor_151000_8013288C(Task* task, s32 arg1, GpCmdArg* msg)
{
    s32 kind;

    kind = msg->command;
    switch (kind) {
        case 0:
            D_actor_151000_8013D37C->turnFrames = 0x14;
            break;
        case 1:
            D_actor_151000_8013D37C->footsteps = kind;
            break;
    }
    return 0;
}

/// "Walk to" opcode: records `mode` in `D_actor_151000_8013D384`, turns the
/// model to face `target` (away from it in mode 1) caching the yaw in the work
/// block, and leaves in `travel` the planar distance divided by the walk's
/// frame count: 0x3C in mode 0, 0xF in mode 1 and 0x19 in mode 2.
s32 func_actor_151000_801328DC(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GpCoord*         coord;
    Actor151000Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = task->extra.tmd->coords;
    work                    = (Actor151000Work*)task->work;
    D_actor_151000_8013D384 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->st.yaw            = angle;
    if (D_actor_151000_8013D384 == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (D_actor_151000_8013D384) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            steps = 0x19;
            break;
    }
    work->st.travel = dist / steps;
    return 0;
}

/// Draws the enemy's ground shadow quad under the model root, unless the model
/// is hidden (`flags & 0x80`) or has no buffer yet. The root part's `workm`
/// translation is staged in a scratchpad VECTOR3 rather than on the stack, and
/// the quad takes the room's current ground shade.
static void func_actor_151000_80132A38(Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR3*   vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_POP_BYTES(0x18);
    }
}
