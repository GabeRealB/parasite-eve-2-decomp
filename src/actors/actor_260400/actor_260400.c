#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        GpAnimSet* sets[10];
        GpCopyArg  copy;
        GpAnimArg  arguments[2];
        GpXformArg placements[2];
    } data;
    s32 words[34];
} Actor260400AnimStorageC668;
STATIC_ASSERT_SIZEOF(Actor260400AnimStorageC668, 136);

extern Actor260400AnimStorageC668 D_actor_260400_8014C668;

/// Work block of the overlay's actor, allocated zeroed by its spawn routine
/// and kept both at `Task::work` and in `D_actor_260400_80154C70`: the light
/// and colour matrices the model is drawn under, its rig and animation state,
/// and the frames of turning left while animation 3 plays. `helper` is the
/// helper task the spawn routine starts and the exit callback kills, and
/// `helperShown` is nonzero once the helper's model may be shown.
typedef struct Actor260400Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig20  rig;
    ActorEnemyState st;
    s16             turnFrames;
    byte            pad_4EE[0x2];
    Task*           helper;
    s8              helperShown;
    byte            pad_4F5[0x3];
} Actor260400Work;
STATIC_ASSERT_SIZEOF(Actor260400Work, 0x4F8);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern GpEvsCmd D_actor_260400_8014C788[];
extern GpEvsCmd D_actor_260400_8014CF38[];
extern GpEvsCmd D_actor_260400_8014D118[];
extern GpEvsCmd D_actor_260400_8014D208[];
extern GpEvsCmd D_actor_260400_8014D340[];
extern GpEvsCmd D_actor_260400_8014D4A8[];
extern GpEvsCmd D_actor_260400_8014D610[];
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, GpAnimArg*);
        s32 (*call1)(Task*, s32, GpCmdArg*);
        s32 (*call2)(Task*, s32, GpXformArg*);
        s32 (*call3)(Task*, s32, VECTOR*, s32);
        s32 (*call4)(Task*, s32, s32);
    } handler;
} Actor260400MessageEntry;
STATIC_ASSERT_SIZEOF(Actor260400MessageEntry, 8);

extern Actor260400MessageEntry D_actor_260400_80154BE8[6];
extern TaskDesc                D_actor_260400_80154C18[];
extern u8                      D_actor_260400_80154C30[];

/// Reset argument the blended reseed forwards: the play-animation handler
/// latches the preset's `field_C` here, and the update sets it to 10 when a
/// walk ends.
extern s16 D_actor_260400_80154BE4;

/// The work block, published by the spawn routine and by the task handler
/// `func_actor_260400_8014A550` on every frame, so the message handlers and
/// the animation loops reach it without the task.
extern Actor260400Work* D_actor_260400_80154C70;

/// The actor's own task, published by the spawn routine: the helper task
/// hangs its model off this task's model parts, the play-animation handler runs
/// the update on it, and the visibility handler reaches its model.
extern Task* D_actor_260400_80154C74;

/// Approach mode the last `func_actor_260400_8014AB50` call selected; the
/// update picks its step length from it. It is the image's trailing halfword,
/// which the split covers as padding, so it has no symbol-file declaration.
extern s16 D_actor_260400_80154C78;

static void func_actor_260400_8014A200(Task* task);
static void func_actor_260400_8014A5AC(GpEnemy* enemy, Task* task);
static void func_actor_260400_8014A630(Task* task);
static void func_actor_260400_8014A66C(Task* task);
static void func_actor_260400_8014A7AC(void);
static void func_actor_260400_8014A7F8(void);
static void func_actor_260400_8014A888(void);

extern TmdSource D_actor_260400_8014F7F0;
extern TmdSource D_actor_260400_80154BC0;
void             func_actor_260400_8014A550(Task*);
void             func_actor_260400_8014A6F8(Task*);

s32 func_actor_260400_8014A908(Task*, s32, GpAnimArg*);
s32 func_actor_260400_8014A998(Task*, s32, s32);
s32 func_actor_260400_8014AA28(Task*, s32, GpXformArg*);
s32 func_actor_260400_8014AAA4(Task*, s32, GpCmdArg*);
s32 func_actor_260400_8014AB50(Task*, s32, VECTOR*, s32);

extern GpAnimArg D_actor_260400_8014C4D8;
extern GpAnimArg D_actor_260400_8014C4EC;
extern GpAnimArg D_actor_260400_8014C500;
extern GpAnimArg D_actor_260400_8014C514;
extern GpAnimArg D_actor_260400_8014C528;
extern GpAnimArg D_actor_260400_8014C53C;
extern GpAnimArg D_actor_260400_8014C550;
extern GpAnimArg D_actor_260400_8014C564;
extern GpAnimArg D_actor_260400_8014C578;
extern GpAnimArg D_actor_260400_8014C58C;
extern GpAnimArg D_actor_260400_8014C5A0;
extern GpAnimArg D_actor_260400_8014C5B4;
extern GpAnimArg D_actor_260400_8014C5DC;
extern GpAnimArg D_actor_260400_8014C5F0;
extern GpAnimArg D_actor_260400_8014C604;
extern GpAnimArg D_actor_260400_8014C618;
extern GpAnimArg D_actor_260400_8014C62C;
extern GpAnimArg D_actor_260400_8014C640;
extern GpAnimArg D_actor_260400_8014C654;
void             func_actor_260400_80149F5C(s32);

extern Actor260400AnimStorageC668 D_actor_260400_8014C668;
extern GpAnimSet                  D_actor_260400_8014AE7C;
extern GpAnimSet                  D_actor_260400_8014B120;
extern GpAnimSet                  D_actor_260400_8014B5CC;
extern GpAnimSet                  D_actor_260400_8014B984;
extern GpAnimSet                  D_actor_260400_8014BE6C;
extern GpAnimSet                  D_actor_260400_8014C170;
extern GpAnimSet                  D_actor_260400_8014C49C;

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor260400PoseBankE68;

Actor260400PoseBankE68 D_actor_260400_8014AC88 = { .poses = {
#include "assets/actor_260400_animation_0105C_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014ACA0[37] = {
#include "assets/actor_260400_animation_0105C_bank4.inc"
};

GpAnimRec D_actor_260400_8014AD34[72] = {
#include "assets/actor_260400_animation_0105C_records.inc"
};

u16 D_actor_260400_8014AE54[20] = {
#include "assets/actor_260400_animation_0105C_indices.inc"
};

GpAnimSet D_actor_260400_8014AE7C = {
    D_actor_260400_8014AD34, D_actor_260400_8014AE54,
    { NULL, D_actor_260400_8014AC88.words, NULL, NULL, D_actor_260400_8014ACA0, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor260400PoseBank1084;

Actor260400PoseBank1084 D_actor_260400_8014AEA4 = { .poses = {
#include "assets/actor_260400_animation_01300_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014AEBC[49] = {
#include "assets/actor_260400_animation_01300_bank4.inc"
};

GpAnimRec D_actor_260400_8014AF80[94] = {
#include "assets/actor_260400_animation_01300_records.inc"
};

u16 D_actor_260400_8014B0F8[20] = {
#include "assets/actor_260400_animation_01300_indices.inc"
};

GpAnimSet D_actor_260400_8014B120 = {
    D_actor_260400_8014AF80, D_actor_260400_8014B0F8,
    { NULL, D_actor_260400_8014AEA4.words, NULL, NULL, D_actor_260400_8014AEBC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} Actor260400PoseBank1328;

Actor260400PoseBank1328 D_actor_260400_8014B148 = { .poses = {
#include "assets/actor_260400_animation_017AC_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014B178[81] = {
#include "assets/actor_260400_animation_017AC_bank4.inc"
};

GpAnimRec D_actor_260400_8014B2BC[186] = {
#include "assets/actor_260400_animation_017AC_records.inc"
};

u16 D_actor_260400_8014B5A4[20] = {
#include "assets/actor_260400_animation_017AC_indices.inc"
};

GpAnimSet D_actor_260400_8014B5CC = {
    D_actor_260400_8014B2BC, D_actor_260400_8014B5A4,
    { NULL, D_actor_260400_8014B148.words, NULL, NULL, D_actor_260400_8014B178, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} Actor260400PoseBank17D4;

Actor260400PoseBank17D4 D_actor_260400_8014B5F4 = { .poses = {
#include "assets/actor_260400_animation_01B64_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014B618[81] = {
#include "assets/actor_260400_animation_01B64_bank4.inc"
};

GpAnimRec D_actor_260400_8014B75C[128] = {
#include "assets/actor_260400_animation_01B64_records.inc"
};

u16 D_actor_260400_8014B95C[20] = {
#include "assets/actor_260400_animation_01B64_indices.inc"
};

GpAnimSet D_actor_260400_8014B984 = {
    D_actor_260400_8014B75C, D_actor_260400_8014B95C,
    { NULL, D_actor_260400_8014B5F4.words, NULL, NULL, D_actor_260400_8014B618, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[8];
    GpPackedSvec words[24];
} Actor260400PoseBank1B8C;

Actor260400PoseBank1B8C D_actor_260400_8014B9AC = { .poses = {
#include "assets/actor_260400_animation_0204C_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014BA0C[109] = {
#include "assets/actor_260400_animation_0204C_bank4.inc"
};

GpAnimRec D_actor_260400_8014BBC0[161] = {
#include "assets/actor_260400_animation_0204C_records.inc"
};

u16 D_actor_260400_8014BE44[20] = {
#include "assets/actor_260400_animation_0204C_indices.inc"
};

GpAnimSet D_actor_260400_8014BE6C = {
    D_actor_260400_8014BBC0, D_actor_260400_8014BE44,
    { NULL, D_actor_260400_8014B9AC.words, NULL, NULL, D_actor_260400_8014BA0C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} Actor260400PoseBank2074;

Actor260400PoseBank2074 D_actor_260400_8014BE94 = { .poses = {
#include "assets/actor_260400_animation_02350_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014BEDC[46] = {
#include "assets/actor_260400_animation_02350_bank4.inc"
};

GpAnimRec D_actor_260400_8014BF94[109] = {
#include "assets/actor_260400_animation_02350_records.inc"
};

u16 D_actor_260400_8014C148[20] = {
#include "assets/actor_260400_animation_02350_indices.inc"
};

GpAnimSet D_actor_260400_8014C170 = {
    D_actor_260400_8014BF94, D_actor_260400_8014C148,
    { NULL, D_actor_260400_8014BE94.words, NULL, NULL, D_actor_260400_8014BEDC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} Actor260400PoseBank2378;

Actor260400PoseBank2378 D_actor_260400_8014C198 = { .poses = {
#include "assets/actor_260400_animation_0267C_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014C1EC[56] = {
#include "assets/actor_260400_animation_0267C_bank4.inc"
};

GpAnimRec D_actor_260400_8014C2CC[106] = {
#include "assets/actor_260400_animation_0267C_records.inc"
};

u16 D_actor_260400_8014C474[20] = {
#include "assets/actor_260400_animation_0267C_indices.inc"
};

GpAnimSet D_actor_260400_8014C49C = {
    D_actor_260400_8014C2CC, D_actor_260400_8014C474,
    { NULL, D_actor_260400_8014C198.words, NULL, NULL, D_actor_260400_8014C1EC, NULL, NULL, NULL },
};

GpAnimArg D_actor_260400_8014C4C4 = { { .index = 1 }, 0, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C4D8 = { { .index = 1 }, 1, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C4EC = { { .index = 1 }, 2, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C500 = { { .index = 1 }, 3, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C514 = { { .index = 1 }, 4, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C528 = { { .index = 1 }, 5, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C53C = { { .index = 1 }, 6, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C550 = { { .index = 1 }, 7, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C564 = { { .index = 1 }, 8, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C578 = { { .index = 1 }, 9, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C58C = { { .index = 1 }, 10, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C5A0 = { { .index = 1 }, 11, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C5B4 = { { .index = 1 }, 12, 0, 0, 0 };

// Retained parameter record; layout follows the adjacent script arguments.
GpAnimArg D_actor_260400_8014C5C8 = { { .index = 1 }, 47, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C5DC = { { .index = 1 }, 47, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C5F0 = { { .index = 1 }, 48, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C604 = { { .index = 1 }, 49, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C618 = { { .index = 1 }, 53, 0, 0, 0 };

GpAnimArg D_actor_260400_8014C62C = { { .index = 1 }, 54, 1, 8, 0 };

GpAnimArg D_actor_260400_8014C640 = { { .index = 1 }, 55, 1, 8, 0 };

GpAnimArg D_actor_260400_8014C654 = { { .index = 1 }, 56, 1, 8, 0 };

Actor260400AnimStorageC668 D_actor_260400_8014C668 = { .data = { { &D_actor_260400_8014AE7C, &D_actor_260400_8014B120, &D_actor_260400_8014B5CC, NULL, NULL, NULL, &D_actor_260400_8014B984, &D_actor_260400_8014BE6C, &D_actor_260400_8014C170, &D_actor_260400_8014C49C }, { { .words = D_actor_260400_8014C668.words }, 32 }, { { { .index = 1 }, 1, 0, 0, 0 }, { { .index = 1 }, 1, 1, 8, 0 } }, { { { 860, 0, 6730, 0 }, { 0, -2048, 0, 0 } }, { { 860, 0, 6730, 0 }, { 0, -2048, 0, 0 } } } } };

GpXformArg D_actor_260400_8014C6F0 = { { 860, 0, 6910, 0 }, { 0, -2161, 0, 0 } };

GpXformArg D_actor_260400_8014C708 = { { 860, 0, 6640, 0 }, { 0, -2275, 0, 0 } };

GpXformArg D_actor_260400_8014C720 = { { 920, 0, 6000, 0 }, { 0, -113, 0, 0 } };

GpXformArg D_actor_260400_8014C738 = { { 1010, 0, 5840, 0 }, { 0, -113, 0, 0 } };

GpXformArg D_actor_260400_8014C750 = { { 1210, 0, 5610, 0 }, { 0, -227, 0, 0 } };

GpXformArg D_actor_260400_8014C768 = { { 860, 0, 6180, 0 }, { 0, 0, 0, 0 } };

GpCmdArg D_actor_260400_8014C780 = { { .loc = { 5, 4 } }, 1 };

GpCmdArg D_actor_260400_8014C784 = { { .loc = { 5, 4 } }, 2 };

GpEvsCmd D_actor_260400_8014C788[82] = {
    { 47, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_260400_80149F5C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C668.data.arguments[0] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_260400_8014C668.data.copy }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_260400_8014C720 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260400_8014C668.data.placements[1] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C4EC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_260400_8014C720 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260400_8014C6F0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C618 }, { .value = 0 } },
    { 4, { .value = 62 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C5B4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_actor_260400_8014C780 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_260400_8014C738 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260400_8014C708 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C500 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C5DC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_actor_260400_8014C784 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_260400_8014C750 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C5F0 }, { .value = 0 } },
    { 4, { .value = 65 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260400_8014C668.data.placements[1] }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C4D8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C514 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C528 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C53C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_260400_8014C750 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C604 }, { .value = 0 } },
    { 4, { .value = 72 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260400_8014C668.data.placements[1] }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C550 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C564 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C618 }, { .value = 0 } },
    { 4, { .value = 62 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260400_8014C668.data.placements[0].pos.vx }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C4D8 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_260400_8014C768 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C668.data.arguments[0] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_260400_80149F5C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_260400_8014CF38[20] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C668.data.arguments[0] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260400_8014C668.data.placements[0].pos.vx }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C4D8 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_260400_8014C768 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_actor_260400_8014C784 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_260400_80149F5C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_260400_8014D118[10] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_260400_8014C668.data.copy }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 26 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C668.data.arguments[1] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C528 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C53C }, { .value = 0 } },
    { 4, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C4D8 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_260400_8014D208[13] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_260400_8014C668.data.copy }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C528 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C654 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C53C }, { .value = 0 } },
    { 4, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C4D8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_260400_8014D340[15] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_260400_8014C668.data.copy }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C528 }, { .value = 0 } },
    { 4, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C53C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C578 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C4D8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C618 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_260400_8014D4A8[15] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_260400_8014C668.data.copy }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C528 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C53C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C58C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C4D8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C62C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_260400_8014D610[9] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_260400_8014C668.data.copy }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C668.data.arguments[1] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C5A0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260400_8014C4D8 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} Actor260400PoseBank38C8;

Actor260400PoseBank38C8 D_actor_260400_8014D6E8 = { .poses = {
#include "assets/actor_260400_animation_03B24_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014D70C[26] = {
#include "assets/actor_260400_animation_03B24_bank4.inc"
};

GpAnimRec D_actor_260400_8014D774[106] = {
#include "assets/actor_260400_animation_03B24_records.inc"
};

u16 D_actor_260400_8014D91C[20] = {
#include "assets/actor_260400_animation_03B24_indices.inc"
};

GpAnimSet D_actor_260400_8014D944 = {
    D_actor_260400_8014D774, D_actor_260400_8014D91C,
    { NULL, D_actor_260400_8014D6E8.words, NULL, NULL, D_actor_260400_8014D70C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor260400PoseBank3B4C;

Actor260400PoseBank3B4C D_actor_260400_8014D96C = { .poses = {
#include "assets/actor_260400_animation_03CCC_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014D984[16] = {
#include "assets/actor_260400_animation_03CCC_bank4.inc"
};

GpAnimRec D_actor_260400_8014D9C4[64] = {
#include "assets/actor_260400_animation_03CCC_records.inc"
};

u16 D_actor_260400_8014DAC4[20] = {
#include "assets/actor_260400_animation_03CCC_indices.inc"
};

GpAnimSet D_actor_260400_8014DAEC = {
    D_actor_260400_8014D9C4, D_actor_260400_8014DAC4,
    { NULL, D_actor_260400_8014D96C.words, NULL, NULL, D_actor_260400_8014D984, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} Actor260400PoseBank3CF4;

Actor260400PoseBank3CF4 D_actor_260400_8014DB14 = { .poses = {
#include "assets/actor_260400_animation_03EDC_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014DB44[32] = {
#include "assets/actor_260400_animation_03EDC_bank4.inc"
};

GpAnimRec D_actor_260400_8014DBC4[68] = {
#include "assets/actor_260400_animation_03EDC_records.inc"
};

u16 D_actor_260400_8014DCD4[20] = {
#include "assets/actor_260400_animation_03EDC_indices.inc"
};

GpAnimSet D_actor_260400_8014DCFC = {
    D_actor_260400_8014DBC4, D_actor_260400_8014DCD4,
    { NULL, D_actor_260400_8014DB14.words, NULL, NULL, D_actor_260400_8014DB44, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor260400PoseBank3F04;

Actor260400PoseBank3F04 D_actor_260400_8014DD24 = { .poses = {
#include "assets/actor_260400_animation_0409C_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014DD3C[18] = {
#include "assets/actor_260400_animation_0409C_bank4.inc"
};

GpAnimRec D_actor_260400_8014DD84[68] = {
#include "assets/actor_260400_animation_0409C_records.inc"
};

u16 D_actor_260400_8014DE94[20] = {
#include "assets/actor_260400_animation_0409C_indices.inc"
};

GpAnimSet D_actor_260400_8014DEBC = {
    D_actor_260400_8014DD84, D_actor_260400_8014DE94,
    { NULL, D_actor_260400_8014DD24.words, NULL, NULL, D_actor_260400_8014DD3C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor260400PoseBank40C4;

Actor260400PoseBank40C4 D_actor_260400_8014DEE4 = { .poses = {
#include "assets/actor_260400_animation_0434C_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014DEFC[33] = {
#include "assets/actor_260400_animation_0434C_bank4.inc"
};

GpAnimRec D_actor_260400_8014DF80[113] = {
#include "assets/actor_260400_animation_0434C_records.inc"
};

u16 D_actor_260400_8014E144[20] = {
#include "assets/actor_260400_animation_0434C_indices.inc"
};

GpAnimSet D_actor_260400_8014E16C = {
    D_actor_260400_8014DF80, D_actor_260400_8014E144,
    { NULL, D_actor_260400_8014DEE4.words, NULL, NULL, D_actor_260400_8014DEFC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor260400PoseBank4374;

Actor260400PoseBank4374 D_actor_260400_8014E194 = { .poses = {
#include "assets/actor_260400_animation_044F4_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014E1AC[20] = {
#include "assets/actor_260400_animation_044F4_bank4.inc"
};

GpAnimRec D_actor_260400_8014E1FC[60] = {
#include "assets/actor_260400_animation_044F4_records.inc"
};

u16 D_actor_260400_8014E2EC[20] = {
#include "assets/actor_260400_animation_044F4_indices.inc"
};

GpAnimSet D_actor_260400_8014E314 = {
    D_actor_260400_8014E1FC, D_actor_260400_8014E2EC,
    { NULL, D_actor_260400_8014E194.words, NULL, NULL, D_actor_260400_8014E1AC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} Actor260400PoseBank451C;

Actor260400PoseBank451C D_actor_260400_8014E33C = { .poses = {
#include "assets/actor_260400_animation_047F0_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014E36C[54] = {
#include "assets/actor_260400_animation_047F0_bank4.inc"
};

GpAnimRec D_actor_260400_8014E444[105] = {
#include "assets/actor_260400_animation_047F0_records.inc"
};

u16 D_actor_260400_8014E5E8[20] = {
#include "assets/actor_260400_animation_047F0_indices.inc"
};

GpAnimSet D_actor_260400_8014E610 = {
    D_actor_260400_8014E444, D_actor_260400_8014E5E8,
    { NULL, D_actor_260400_8014E33C.words, NULL, NULL, D_actor_260400_8014E36C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} Actor260400PoseBank4818;

Actor260400PoseBank4818 D_actor_260400_8014E638 = { .poses = {
#include "assets/actor_260400_animation_04A88_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014E668[28] = {
#include "assets/actor_260400_animation_04A88_bank4.inc"
};

GpAnimRec D_actor_260400_8014E6D8[106] = {
#include "assets/actor_260400_animation_04A88_records.inc"
};

u16 D_actor_260400_8014E880[20] = {
#include "assets/actor_260400_animation_04A88_indices.inc"
};

GpAnimSet D_actor_260400_8014E8A8 = {
    D_actor_260400_8014E6D8, D_actor_260400_8014E880,
    { NULL, D_actor_260400_8014E638.words, NULL, NULL, D_actor_260400_8014E668, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} Actor260400PoseBank4AB0;

Actor260400PoseBank4AB0 D_actor_260400_8014E8D0 = { .poses = {
#include "assets/actor_260400_animation_04D14_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014E900[34] = {
#include "assets/actor_260400_animation_04D14_bank4.inc"
};

GpAnimRec D_actor_260400_8014E988[97] = {
#include "assets/actor_260400_animation_04D14_records.inc"
};

u16 D_actor_260400_8014EB0C[20] = {
#include "assets/actor_260400_animation_04D14_indices.inc"
};

GpAnimSet D_actor_260400_8014EB34 = {
    D_actor_260400_8014E988, D_actor_260400_8014EB0C,
    { NULL, D_actor_260400_8014E8D0.words, NULL, NULL, D_actor_260400_8014E900, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} Actor260400PoseBank4D3C;

Actor260400PoseBank4D3C D_actor_260400_8014EB5C = { .poses = {
#include "assets/actor_260400_animation_05094_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014EB80[66] = {
#include "assets/actor_260400_animation_05094_bank4.inc"
};

GpAnimRec D_actor_260400_8014EC88[129] = {
#include "assets/actor_260400_animation_05094_records.inc"
};

u16 D_actor_260400_8014EE8C[20] = {
#include "assets/actor_260400_animation_05094_indices.inc"
};

GpAnimSet D_actor_260400_8014EEB4 = {
    D_actor_260400_8014EC88, D_actor_260400_8014EE8C,
    { NULL, D_actor_260400_8014EB5C.words, NULL, NULL, D_actor_260400_8014EB80, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} Actor260400PoseBank50BC;

Actor260400PoseBank50BC D_actor_260400_8014EEDC = { .poses = {
#include "assets/actor_260400_animation_0526C_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014EF00[19] = {
#include "assets/actor_260400_animation_0526C_bank4.inc"
};

GpAnimRec D_actor_260400_8014EF4C[70] = {
#include "assets/actor_260400_animation_0526C_records.inc"
};

u16 D_actor_260400_8014F064[20] = {
#include "assets/actor_260400_animation_0526C_indices.inc"
};

GpAnimSet D_actor_260400_8014F08C = {
    D_actor_260400_8014EF4C, D_actor_260400_8014F064,
    { NULL, D_actor_260400_8014EEDC.words, NULL, NULL, D_actor_260400_8014EF00, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} Actor260400PoseBank5294;

Actor260400PoseBank5294 D_actor_260400_8014F0B4 = { .poses = {
#include "assets/actor_260400_animation_05474_bank1.inc"
} };

GpPackedSvec D_actor_260400_8014F0CC[26] = {
#include "assets/actor_260400_animation_05474_bank4.inc"
};

GpAnimRec D_actor_260400_8014F134[78] = {
#include "assets/actor_260400_animation_05474_records.inc"
};

u16 D_actor_260400_8014F26C[20] = {
#include "assets/actor_260400_animation_05474_indices.inc"
};

GpAnimSet D_actor_260400_8014F294 = {
    D_actor_260400_8014F134, D_actor_260400_8014F26C,
    { NULL, D_actor_260400_8014F0B4.words, NULL, NULL, D_actor_260400_8014F0CC, NULL, NULL, NULL },
};

TmdBone D_actor_260400_8014F2BC[1] = {
#include "assets/actor_260400_model_059D0_skeleton.inc"
};

u32 D_actor_260400_8014F2E0[1] = {
#include "assets/actor_260400_model_059D0_partVerts.inc"
};

SVECTOR D_actor_260400_8014F2E4[28] = {
#include "assets/actor_260400_model_059D0_verts.inc"
};

SVECTOR D_actor_260400_8014F3C4[28] = {
#include "assets/actor_260400_model_059D0_normals.inc"
};

u32 D_actor_260400_8014F4A4[211] = {
#include "assets/actor_260400_model_059D0_stream.inc"
};

TmdSource D_actor_260400_8014F7F0 = {
    0, 1464, 0, 1,
    D_actor_260400_8014F2E0, D_actor_260400_8014F2E4, D_actor_260400_8014F3C4, D_actor_260400_8014F2BC, D_actor_260400_8014F4A4,
};

TmdBone D_actor_260400_8014F814[20] = {
#include "assets/actor_260400_model_0ADA0_skeleton.inc"
};

u32 D_actor_260400_8014FAE4[20] = {
#include "assets/actor_260400_model_0ADA0_partVerts.inc"
};

SVECTOR D_actor_260400_8014FB34[343] = {
#include "assets/actor_260400_model_0ADA0_verts.inc"
};

SVECTOR D_actor_260400_801505EC[334] = {
#include "assets/actor_260400_model_0ADA0_normals.inc"
};

u32 D_actor_260400_8015105C[3801] = {
#include "assets/actor_260400_model_0ADA0_stream.inc"
};

TmdSource D_actor_260400_80154BC0 = {
    0, 20988, 5348, 20,
    D_actor_260400_8014FAE4, D_actor_260400_8014FB34, D_actor_260400_801505EC, D_actor_260400_8014F814, D_actor_260400_8015105C,
};

s16 D_actor_260400_80154BE4 = 8;

Actor260400MessageEntry D_actor_260400_80154BE8[6] = {
    { 2003, { .call0 = func_actor_260400_8014A908 } },
    { 2005, { .call4 = func_actor_260400_8014A998 } },
    { 2004, { .call2 = func_actor_260400_8014AA28 } },
    { 2011, { .call1 = func_actor_260400_8014AAA4 } },
    { 2013, { .call3 = func_actor_260400_8014AB50 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_260400_80154C18[2] = {
    { 1, 192, func_actor_260400_8014A550, { .model = &D_actor_260400_80154BC0 } },
    { 1, 192, func_actor_260400_8014A6F8, { .model = &D_actor_260400_8014F7F0 } },
};

u8 D_actor_260400_80154C30[64] = {
    0, 0, 0, 0, 68, 217, 20, 128, 236, 218, 20, 128, 252, 220, 20, 128,
    188, 222, 20, 128, 108, 225, 20, 128, 20, 227, 20, 128, 16, 230, 20, 128,
    168, 232, 20, 128, 52, 235, 20, 128, 180, 238, 20, 128, 140, 240, 20, 128,
    148, 242, 20, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

Actor260400Work * D_actor_260400_80154C70 = NULL;

Task * D_actor_260400_80154C74 = NULL;

s16 D_actor_260400_80154C78 = 0;

static void func_actor_260400_80149E38(void);
static void func_actor_260400_80149FA4(void);
static void func_actor_260400_80149FE0(GpEnemy* enemy, Task* task);

static void func_actor_260400_80149E38(void)
{
    switch (GameFlag_GetNibble(0xE3)) {
        case 0:
            func_800E8634(D_actor_260400_8014C788, 0, D_actor_260400_8014CF38);
            GameFlag_SetNibble(0xE3, 1);
            break;
        case 1:
            if ((Gp_GetCurBit2Flag(4) == 1) || (Gp_GetCurBit2Flag(5) == 1)) {
                func_800E8614(D_actor_260400_8014D118, 0);
            } else {
                func_800E8614(D_actor_260400_8014D208, 0);
                GameFlag_SetNibble(0xE3, 2);
            }
            break;
        case 2:
            func_800E8614(D_actor_260400_8014D340, 0);
            GameFlag_SetNibble(0xE3, 3);
            break;
        case 3:
            func_800E8614(D_actor_260400_8014D4A8, 0);
            GameFlag_SetNibble(0xE3, 4);
            break;
        case 4:
            func_800E8614(D_actor_260400_8014D610, 0);
            break;
    }
}

void func_actor_260400_80149F5C(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

static void func_actor_260400_80149FA4(void)
{
    Task* slot;

    slot = Gp_LookupSlot4(0);
    if (slot != 0) {
        Gp_DispatchMsgPtr(slot, 0x7D4, &D_actor_260400_8014C668.data.placements[0].pos.vx, 0);
    }
}

/// Spawn routine (state 0 of `func_actor_260400_8014A550`): allocates the work
/// block and publishes it in `D_actor_260400_80154C70` and the task's `work`
/// slot, binds the model to the view and hands it the block's light and colour
/// matrices, publishes the task in `D_actor_260400_80154C74`, relights the
/// model from a point 0x320 above its translation and binds the animation
/// stream. It then starts the helper task and textures the helper's model from
/// the area placement record the spawning enemy names, before running the
/// first update with the reset mode 2 / id 1 it seeds.
static void func_actor_260400_80149FE0(GpEnemy* enemy, Task* task)
{
    VECTOR     vec;
    GpCoord*   coord;
    TmdObject* obj;
    Task*      spawned;
    void*      work;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(0x4F8, 0);
    D_actor_260400_80154C70 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_260400_8014A630;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->flags                   = 0;
    obj->lightMtx                = &D_actor_260400_80154C70->light;
    obj->colorMtx                = &D_actor_260400_80154C70->color;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    D_actor_260400_80154C74      = task;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_260400_80154C70->rig.anim, D_actor_260400_80154C30, obj,
                  D_actor_260400_80154C70->rig.poses, D_actor_260400_80154C70->rig.slots);
    D_actor_260400_80154C70->st.animId = 1;
    D_actor_260400_80154C70->st.state  = 2;
    spawned                            = Task_SpawnFromTable(D_actor_260400_80154C18, 1, 8, 0);
    if (spawned != NULL) {
        D_actor_260400_80154C70->helper = spawned;
        actorTintTask(spawned, (GpEnemy*)task->spawnArg2.pointer);
    }
    D_actor_260400_80154C70->st.travel   = 0;
    D_actor_260400_80154C70->turnFrames  = 0;
    D_actor_260400_80154C70->helperShown = 0;
    task->msgTable                       = D_actor_260400_80154BE8;
    func_actor_260400_8014A200(task);
    task->state++;
}

/// Per-frame update: reset modes 1 and 2 run their one-shot reseed (blended or
/// plain) and switch to mode 3. In mode 3 the walking animations 2, 0xE and 0xF
/// step the model forward while `st.travel` counts down, by a distance the
/// approach mode in `D_actor_260400_80154C78` picks, and blend into animation
/// 0xD with reset argument 10 when the walk ends; animation 3 turns the model
/// while `turnFrames` counts down. Mode 3 then ticks the animation.
static void func_actor_260400_8014A200(Task* task)
{
    GpCoord*         coord = task->extra.tmd->coords;
    Actor260400Work* work  = (Actor260400Work*)task->work;

    if (D_actor_260400_80154C70->st.state == 1) {
        func_actor_260400_8014A888();
        D_actor_260400_80154C70->st.state = 3;
    } else if (D_actor_260400_80154C70->st.state == 2) {
        func_actor_260400_8014A7F8();
        D_actor_260400_80154C70->st.state = 3;
    } else if (D_actor_260400_80154C70->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (D_actor_260400_80154C78) {
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
                    D_actor_260400_80154BE4 = 10;
                    work->st.animId         = 0xD;
                }
            }
        }
        if (work->st.animId == 3 && work->turnFrames != 0) {
            work->st.yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, (s16)work->st.yaw, 1);
            coord->flg = 0;
            work->turnFrames--;
        }
        func_actor_260400_8014A7AC();
    }
}

/// Two-state task handler: publishes the task's work block in
/// `D_actor_260400_80154C70` on the way through, then calls the spawn routine
/// or the per-frame state, whichever `Task::state` selects from a table built
/// on the stack.
void func_actor_260400_8014A550(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_260400_80149FE0,
        func_actor_260400_8014A5AC,
    };

    D_actor_260400_80154C70 = (Actor260400Work*)task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Per-frame state (state 1 of `func_actor_260400_8014A550`): refreshes the
/// model root's world matrix, relights the model from a point 0x320 above its
/// translation, then runs the update and draws the ground shadow.
static void func_actor_260400_8014A5AC(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR     pos;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_260400_8014A200(task);
    func_actor_260400_8014A66C(task);
}

/// `Task::exitCallback` the spawn routine installs: hands the task's `GpEnemy`
/// back to `Gp_DestroyEnemy` and kills the helper task.
static void func_actor_260400_8014A630(Task* task)
{
    Actor260400Work* work = (Actor260400Work*)task->work;

    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
    taskKill(work->helper);
}

/// Draws the actor's ground shadow quad under the model root, unless the model
/// is hidden (`flags & 0x80`) or has no buffer yet. The root's world
/// translation is staged in a scratchpad `VECTOR3` rather than on the stack,
/// and the quad is drawn at a fixed brightness of 0xC0.
static void func_actor_260400_8014A66C(Task* task)
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
        Gp_DrawEffGroundQuad(vec, 0x200, 0xC0);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Task handler of the helper the spawn routine starts: the first tick hangs
/// the task's coordinate frame off the actor's model part `spawnArg1`, shows
/// its model and steps to state 1; every later tick relights the model from a
/// point 0x320 above the actor's root translation.
void func_actor_260400_8014A6F8(Task* task)
{
    TmdObject* extra = task->extra.tmd;
    GpCoord*   coord = extra->coords;
    GpCoord*   parts = D_actor_260400_80154C74->extra.tmd->coords;
    GpCoord*   part  = parts + task->spawnArg1.value;
    VECTOR     vec;

    switch (task->state) {
        case 0:
            coord->flg   = 0;
            extra->flags = 0;
            coord->sub   = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

/// Ticks animation slots 1..0x13 of the work block's animation context.
static void func_actor_260400_8014A7AC(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_260400_80154C70->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Plain reseed: marks animation slots 1..0x13 of the work block reset-pending
/// and reseeds each of them from the current animation id, then records that id
/// as the one now playing.
static void func_actor_260400_8014A7F8(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_260400_80154C70->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_260400_80154C70->rig.anim, i, D_actor_260400_80154C70->st.animId);
        i++;
    } while (i < 0x14);
    D_actor_260400_80154C70->st.appliedAnimId = D_actor_260400_80154C70->st.animId;
}

/// Blended reseed: reseeds animation slots 1..0x13 of the work block from the
/// current animation id with the latched reset argument
/// `D_actor_260400_80154BE4`, and records that id as the one now playing.
static void func_actor_260400_8014A888(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_260400_80154C70->rig.anim, i, D_actor_260400_80154C70->st.animId, 0,
                      D_actor_260400_80154BE4);
        i++;
    } while (i < 0x14);
    D_actor_260400_80154C70->st.appliedAnimId = D_actor_260400_80154C70->st.animId;
}

/// Message 0x7D3 (play animation): adopts the preset's animation id when it is
/// one of the first 0x10, latching the reset mode -- 1 for the blended reseed,
/// 2 for the plain one -- and the reset argument the blended reseed forwards,
/// then runs the update on the actor's task. Ids past the range are rejected
/// with -1 and leave the work block untouched.
s32 func_actor_260400_8014A908(Task* task, s32 arg1, GpAnimArg* preset)
{
    if (preset->field_4 < 0x10) {
        D_actor_260400_80154C70->st.animId = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_260400_80154C70->st.state = 1;
            D_actor_260400_80154BE4           = preset->field_C;
        } else {
            D_actor_260400_80154C70->st.state = 2;
        }
        D_actor_260400_80154C70->st.field_6 = 0;
        func_actor_260400_8014A200(D_actor_260400_80154C74);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 (visibility): bit 0 of `arg2` shows both the actor's and the
/// helper's model (flags 0) or hides them (0x80), and bit 1 ORs in 0x4. Until
/// message 0x7DB has enabled the helper (`helperShown`), its model is kept
/// hidden at 0x84 whatever the mask says.
s32 func_actor_260400_8014A998(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;
    TmdObject* helperObj;

    obj       = D_actor_260400_80154C74->extra.tmd;
    helperObj = D_actor_260400_80154C70->helper->extra.tmd;

    if (arg2 & 1) {
        obj->flags       = 0;
        helperObj->flags = 0;
    } else {
        obj->flags       = 0x80;
        helperObj->flags = 0x80;
    }
    if (arg2 & 2) {
        obj->flags       |= 4;
        helperObj->flags |= 4;
    }
    if ((u8)D_actor_260400_80154C70->helperShown == 0) {
        helperObj->flags = 0x84;
    }
    return 0;
}

/// Message 0x7D4 (placement): turns the model to the placement's yaw, keeping
/// that yaw in the work block, and moves it to the placement's position. Only
/// the Y rotation is applied.
s32 func_actor_260400_8014AA28(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord* coord;
    u16      yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_260400_80154C70->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message 0x7DB: the payload's halfword at 0x2 selects the action. Case 0
/// starts a turn of 0x14 steps; case 1 enables and shows the helper's model,
/// but only while `func_800B7420(0x88)` returns 0; case 2 disables it and
/// hides the model again (flags 0x84).
s32 func_actor_260400_8014AAA4(Task* task, s32 arg1, GpCmdArg* msg)
{
    TmdObject* obj;
    s32        mode;

    obj  = D_actor_260400_80154C70->helper->extra.tmd;
    mode = msg->command;

    switch (mode) {
        case 0:
            D_actor_260400_80154C70->turnFrames = 0x14;
            break;
        case 1:
            if (func_800B7420(0x88) == 0) {
                D_actor_260400_80154C70->helperShown = mode;
                obj->flags                           = 0;
            }
            break;
        case 2:
            D_actor_260400_80154C70->helperShown = 0;
            obj->flags                           = 0x84;
            break;
    }
    return 0;
}

/// Message 0x7DD (approach): turns the model to face `target` -- away from it
/// in mode 1, where the update then walks it backwards -- keeps the mode in
/// `D_actor_260400_80154C78`, and stores the number of steps the walk takes:
/// the planar distance over the mode's step length, 60 in mode 0, 15 in mode 1
/// and 25 otherwise.
s32 func_actor_260400_8014AB50(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GpCoord*         coord;
    Actor260400Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = task->extra.tmd->coords;
    work                    = (Actor260400Work*)task->work;
    D_actor_260400_80154C78 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->st.yaw            = angle;
    if (D_actor_260400_80154C78 == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, (s16)work->st.yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (D_actor_260400_80154C78) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            break;
    }
    work->st.travel = dist / steps;
    return 0;
}
