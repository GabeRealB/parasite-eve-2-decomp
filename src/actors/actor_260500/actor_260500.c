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
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*        sets[10];
        GpCopyArg            copy;
        AnimationPlayRequest arguments[1];
        ActorTransform       placements[3];
    } data;
    s32 words[35];
} Actor260500AnimStorageCAF4;
STATIC_ASSERT_SIZEOF(Actor260500AnimStorageCAF4, 140);

extern Actor260500AnimStorageCAF4 D_actor_260500_8014CAF4;

/// The work block, published by the spawn routine and by the task handler
/// `func_actor_260500_8014A460` on every frame, so the message handlers and
/// the animation loops reach it without the task.
extern Actor260500Work* D_actor_260500_80159E4C;

/// The actor's own task, published by the spawn routine: the play-animation
/// handler runs the update on it and the visibility handler reaches its model.
extern Task* D_actor_260500_80159E50;

/// Reset argument the blended reseed forwards: the play-animation handler
/// latches the preset's `field_C` here, and the update sets it to 10 when a
/// walk ends.
extern s16 D_actor_260500_80159D7C;

/// Approach mode the last `func_actor_260500_8014A83C` call selected; the
/// update picks its step length from it.
extern s16 D_actor_260500_80159E54;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern GpEvsCmd D_actor_260500_8014CBF8[];
extern GpEvsCmd D_actor_260500_8014D630[];
extern GpEvsCmd D_actor_260500_8014D7C8[];
extern GpEvsCmd D_actor_260500_8014D948[];
extern GpEvsCmd D_actor_260500_8014DAB0[];
extern GpEvsCmd D_actor_260500_8014DCC0[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*);
        s32 (*call1)(Task*, s32, ActorCommand* request);
        s32 (*call2)(Task*, s32, ActorTransform*);
        s32 (*call3)(Task*, s32, VECTOR*, s32);
        s32 (*call4)(Task*, s32, s32);
    } handler;
} Actor260500MsgEntry;
STATIC_ASSERT_SIZEOF(Actor260500MsgEntry, 8);

extern Actor260500MsgEntry D_actor_260500_80159D80[];
extern u8                  D_actor_260500_80159DBC[];

static void func_actor_260500_8014A110(Task* task);
static void func_actor_260500_8014A4BC(GpEnemy* enemy, Task* task);
static void func_actor_260500_8014A540(Task* task);
static void func_actor_260500_8014A568(void);
static void func_actor_260500_8014A5B4(void);
static void func_actor_260500_8014A644(void);
static void func_actor_260500_8014A99C(Task* task);

extern TmdSource D_actor_260500_80159D58;
void             func_actor_260500_8014A460(Task*);

s32 func_actor_260500_8014A6C4(Task*, s32, AnimationPlayRequest*);
s32 func_actor_260500_8014A754(Task*, s32, s32);
s32 func_actor_260500_8014A79C(Task*, s32, ActorTransform* placement);
s32 func_actor_260500_8014A818(Task*, s32, ActorCommand* msg);
s32 func_actor_260500_8014A83C(Task*, s32, VECTOR*, s32);

extern AnimationPlayRequest D_actor_260500_8014C874;
extern AnimationPlayRequest D_actor_260500_8014C888;
extern AnimationPlayRequest D_actor_260500_8014C89C;
extern AnimationPlayRequest D_actor_260500_8014C8B0;
extern AnimationPlayRequest D_actor_260500_8014C8C4;
extern AnimationPlayRequest D_actor_260500_8014C8D8;
extern AnimationPlayRequest D_actor_260500_8014C8EC;
extern AnimationPlayRequest D_actor_260500_8014C900;
extern AnimationPlayRequest D_actor_260500_8014C914;
extern AnimationPlayRequest D_actor_260500_8014C928;
extern AnimationPlayRequest D_actor_260500_8014C93C;
extern AnimationPlayRequest D_actor_260500_8014C950;
extern AnimationPlayRequest D_actor_260500_8014C964;
extern AnimationPlayRequest D_actor_260500_8014C978;
extern AnimationPlayRequest D_actor_260500_8014C98C;
extern AnimationPlayRequest D_actor_260500_8014C9A0;
extern AnimationPlayRequest D_actor_260500_8014C9B4;
extern AnimationPlayRequest D_actor_260500_8014C9C8;
extern AnimationPlayRequest D_actor_260500_8014C9DC;
extern AnimationPlayRequest D_actor_260500_8014C9F0;
extern AnimationPlayRequest D_actor_260500_8014CA04;
extern AnimationPlayRequest D_actor_260500_8014CA18;
extern AnimationPlayRequest D_actor_260500_8014CA2C;
extern AnimationPlayRequest D_actor_260500_8014CA54;
extern AnimationPlayRequest D_actor_260500_8014CA68;
extern AnimationPlayRequest D_actor_260500_8014CA7C;
extern AnimationPlayRequest D_actor_260500_8014CA90;
extern AnimationPlayRequest D_actor_260500_8014CAA4;
extern AnimationPlayRequest D_actor_260500_8014CAB8;
extern AnimationPlayRequest D_actor_260500_8014CACC;
extern AnimationPlayRequest D_actor_260500_8014CAE0;
void                        func_actor_260500_80149E38(s32);

extern Actor260500AnimStorageCAF4 D_actor_260500_8014CAF4;
extern AnimationSet               D_actor_260500_8014AE44;
extern AnimationSet               D_actor_260500_8014B134;
extern AnimationSet               D_actor_260500_8014B830;
extern AnimationSet               D_actor_260500_8014BACC;
extern AnimationSet               D_actor_260500_8014BE84;
extern AnimationSet               D_actor_260500_8014C208;
extern AnimationSet               D_actor_260500_8014C50C;
extern AnimationSet               D_actor_260500_8014C838;

AnimationPackedPose D_actor_260500_8014AA28[3] = {
#include "assets/actor_260500_animation_01024_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014AA4C[81] = {
#include "assets/actor_260500_animation_01024_bank4.inc"
};

AnimationRecord D_actor_260500_8014AB90[163] = {
#include "assets/actor_260500_animation_01024_records.inc"
};

u16 D_actor_260500_8014AE1C[20] = {
#include "assets/actor_260500_animation_01024_indices.inc"
};

AnimationSet D_actor_260500_8014AE44 = {
    D_actor_260500_8014AB90,
    D_actor_260500_8014AE1C,
    { NULL, D_actor_260500_8014AA28, NULL, NULL, D_actor_260500_8014AA4C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014AE6C[4] = {
#include "assets/actor_260500_animation_01314_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014AE9C[40] = {
#include "assets/actor_260500_animation_01314_bank4.inc"
};

AnimationRecord D_actor_260500_8014AF3C[116] = {
#include "assets/actor_260500_animation_01314_records.inc"
};

u16 D_actor_260500_8014B10C[20] = {
#include "assets/actor_260500_animation_01314_indices.inc"
};

AnimationSet D_actor_260500_8014B134 = {
    D_actor_260500_8014AF3C,
    D_actor_260500_8014B10C,
    { NULL, D_actor_260500_8014AE6C, NULL, NULL, D_actor_260500_8014AE9C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014B15C[8] = {
#include "assets/actor_260500_animation_01A10_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014B1BC[157] = {
#include "assets/actor_260500_animation_01A10_bank4.inc"
};

AnimationRecord D_actor_260500_8014B430[246] = {
#include "assets/actor_260500_animation_01A10_records.inc"
};

u16 D_actor_260500_8014B808[20] = {
#include "assets/actor_260500_animation_01A10_indices.inc"
};

AnimationSet D_actor_260500_8014B830 = {
    D_actor_260500_8014B430,
    D_actor_260500_8014B808,
    { NULL, D_actor_260500_8014B15C, NULL, NULL, D_actor_260500_8014B1BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014B858[2] = {
#include "assets/actor_260500_animation_01CAC_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014B870[51] = {
#include "assets/actor_260500_animation_01CAC_bank4.inc"
};

AnimationRecord D_actor_260500_8014B93C[90] = {
#include "assets/actor_260500_animation_01CAC_records.inc"
};

u16 D_actor_260500_8014BAA4[20] = {
#include "assets/actor_260500_animation_01CAC_indices.inc"
};

AnimationSet D_actor_260500_8014BACC = {
    D_actor_260500_8014B93C,
    D_actor_260500_8014BAA4,
    { NULL, D_actor_260500_8014B858, NULL, NULL, D_actor_260500_8014B870, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014BAF4[3] = {
#include "assets/actor_260500_animation_02064_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014BB18[81] = {
#include "assets/actor_260500_animation_02064_bank4.inc"
};

AnimationRecord D_actor_260500_8014BC5C[128] = {
#include "assets/actor_260500_animation_02064_records.inc"
};

u16 D_actor_260500_8014BE5C[20] = {
#include "assets/actor_260500_animation_02064_indices.inc"
};

AnimationSet D_actor_260500_8014BE84 = {
    D_actor_260500_8014BC5C,
    D_actor_260500_8014BE5C,
    { NULL, D_actor_260500_8014BAF4, NULL, NULL, D_actor_260500_8014BB18, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014BEAC[6] = {
#include "assets/actor_260500_animation_023E8_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014BEF4[69] = {
#include "assets/actor_260500_animation_023E8_bank4.inc"
};

AnimationRecord D_actor_260500_8014C008[118] = {
#include "assets/actor_260500_animation_023E8_records.inc"
};

u16 D_actor_260500_8014C1E0[20] = {
#include "assets/actor_260500_animation_023E8_indices.inc"
};

AnimationSet D_actor_260500_8014C208 = {
    D_actor_260500_8014C008,
    D_actor_260500_8014C1E0,
    { NULL, D_actor_260500_8014BEAC, NULL, NULL, D_actor_260500_8014BEF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014C230[6] = {
#include "assets/actor_260500_animation_026EC_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014C278[46] = {
#include "assets/actor_260500_animation_026EC_bank4.inc"
};

AnimationRecord D_actor_260500_8014C330[109] = {
#include "assets/actor_260500_animation_026EC_records.inc"
};

u16 D_actor_260500_8014C4E4[20] = {
#include "assets/actor_260500_animation_026EC_indices.inc"
};

AnimationSet D_actor_260500_8014C50C = {
    D_actor_260500_8014C330,
    D_actor_260500_8014C4E4,
    { NULL, D_actor_260500_8014C230, NULL, NULL, D_actor_260500_8014C278, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014C534[7] = {
#include "assets/actor_260500_animation_02A18_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014C588[56] = {
#include "assets/actor_260500_animation_02A18_bank4.inc"
};

AnimationRecord D_actor_260500_8014C668[106] = {
#include "assets/actor_260500_animation_02A18_records.inc"
};

u16 D_actor_260500_8014C810[20] = {
#include "assets/actor_260500_animation_02A18_indices.inc"
};

AnimationSet D_actor_260500_8014C838 = {
    D_actor_260500_8014C668,
    D_actor_260500_8014C810,
    { NULL, D_actor_260500_8014C534, NULL, NULL, D_actor_260500_8014C588, NULL, NULL, NULL },
};

AnimationPlayRequest D_actor_260500_8014C860 = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C874 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C888 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C89C = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C8B0 = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C8C4 = { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C8D8 = { { .index = 1 }, 6, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C8EC = { { .index = 1 }, 7, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C900 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C914 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C928 = { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C93C = { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C950 = { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C964 = { { .index = 1 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C978 = { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C98C = { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9A0 = { { .index = 1 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9B4 = { { .index = 1 }, 17, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9C8 = { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9DC = { { .index = 1 }, 19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9F0 = { { .index = 1 }, 20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA04 = { { .index = 1 }, 21, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA18 = { { .index = 1 }, 22, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA2C = { { .index = 1 }, 23, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_260500_8014CA40 = { { .index = 1 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA54 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA68 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA7C = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA90 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CAA4 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CAB8 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CACC = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CAE0 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

Actor260500AnimStorageCAF4 D_actor_260500_8014CAF4 = { .data = { { &D_actor_260500_8014AE44, &D_actor_260500_8014B134, &D_actor_260500_8014B830, &D_actor_260500_8014BACC, NULL, NULL, &D_actor_260500_8014BE84, &D_actor_260500_8014C208, &D_actor_260500_8014C50C, &D_actor_260500_8014C838 }, { { .words = D_actor_260500_8014CAF4.words }, 32 }, { { { .index = 1 }, 1, 0, 0, 0 } }, { { { 860, 0, 6730, 0 }, { 0, -2161, 0, 0 } }, { { 860, 0, 6730, 0 }, { 0, -2161, 0, 0 } }, { { 860, 0, 6910, 0 }, { 0, -2048, 0, 0 } } } } };

ActorTransform D_actor_260500_8014CB80 = { { 860, 0, 6640, 0 }, { 0, -2161, 0, 0 } };

ActorTransform D_actor_260500_8014CB98 = { { 920, 0, 6000, 0 }, { 0, -113, 0, 0 } };

ActorTransform D_actor_260500_8014CBB0 = { { 1210, 0, 5610, 0 }, { 0, -227, 0, 0 } };

ActorTransform D_actor_260500_8014CBC8 = { { 1010, 0, 5840, 0 }, { 0, -113, 0, 0 } };

ActorTransform D_actor_260500_8014CBE0 = { { 860, 0, 6180, 0 }, { 0, 0, 0, 0 } };

GpEvsCmd D_actor_260500_8014CBF8[109] = {
    { 47, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_260500_80149E38 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAF4.data.arguments[0] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_260500_8014CAF4.data.copy }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_260500_8014CB98 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260500_8014CAF4.data.placements[1] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C874 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260500_8014CAF4.data.placements[2] }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C888 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C89C }, { .value = 0 } },
    { 4, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8B0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAA4 }, { .value = 0 } },
    { 4, { .value = 62 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8C4 }, { .value = 0 } },
    { 4, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8D8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_260500_8014CBB0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260500_8014CB80 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_260500_8014CBC8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8EC }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAB8 }, { .value = 0 } },
    { 4, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8D8 }, { .value = 0 } },
    { 4, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8EC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C900 }, { .value = 0 } },
    { 4, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C914 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C928 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAA4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA54 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA90 }, { .value = 0 } },
    { 4, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C93C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260500_8014CAF4.data.placements[1] }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C950 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_260500_8014CBB0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA68 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260500_8014CAF4.data.placements[1] }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C964 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260500_8014CAF4.data.placements[2] }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_260500_8014CB98 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C978 }, { .value = 0 } },
    { 4, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C98C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA7C }, { .value = 0 } },
    { 4, { .value = 62 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C9A0 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014CA2C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260500_8014CAF4.data.placements[0] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8B0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_260500_8014CBE0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAF4.data.arguments[0] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_260500_80149E38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_260500_8014D630[17] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAF4.data.arguments[0] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_260500_8014CAF4.data.placements[0] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8B0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_260500_8014CBE0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_260500_80149E38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_260500_8014D7C8[16] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_260500_8014CAF4.data.copy }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8D8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8EC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C9B4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAA4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8B0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_260500_8014D948[15] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_260500_8014CAF4.data.copy }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8D8 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8EC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C9C8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAA4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8B0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_260500_8014DAB0[22] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_260500_8014CAF4.data.copy }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8D8 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8EC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C9DC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA68 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C9F0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAE0 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014CA04 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8B0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_260500_8014DCC0[9] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_260500_8014CAF4.data.copy }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014CA18 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_260500_8014C8B0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

AnimationPackedPose D_actor_260500_8014DD98[4] = {
#include "assets/actor_260500_animation_042F8_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014DDC8[55] = {
#include "assets/actor_260500_animation_042F8_bank4.inc"
};

AnimationRecord D_actor_260500_8014DEA4[147] = {
#include "assets/actor_260500_animation_042F8_records.inc"
};

u16 D_actor_260500_8014E0F0[20] = {
#include "assets/actor_260500_animation_042F8_indices.inc"
};

AnimationSet D_actor_260500_8014E118 = {
    D_actor_260500_8014DEA4,
    D_actor_260500_8014E0F0,
    { NULL, D_actor_260500_8014DD98, NULL, NULL, D_actor_260500_8014DDC8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014E140[5] = {
#include "assets/actor_260500_animation_0483C_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014E17C[103] = {
#include "assets/actor_260500_animation_0483C_bank4.inc"
};

AnimationRecord D_actor_260500_8014E318[199] = {
#include "assets/actor_260500_animation_0483C_records.inc"
};

u16 D_actor_260500_8014E634[20] = {
#include "assets/actor_260500_animation_0483C_indices.inc"
};

AnimationSet D_actor_260500_8014E65C = {
    D_actor_260500_8014E318,
    D_actor_260500_8014E634,
    { NULL, D_actor_260500_8014E140, NULL, NULL, D_actor_260500_8014E17C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014E684[2] = {
#include "assets/actor_260500_animation_04AC4_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014E69C[53] = {
#include "assets/actor_260500_animation_04AC4_bank4.inc"
};

AnimationRecord D_actor_260500_8014E770[83] = {
#include "assets/actor_260500_animation_04AC4_records.inc"
};

u16 D_actor_260500_8014E8BC[20] = {
#include "assets/actor_260500_animation_04AC4_indices.inc"
};

AnimationSet D_actor_260500_8014E8E4 = {
    D_actor_260500_8014E770,
    D_actor_260500_8014E8BC,
    { NULL, D_actor_260500_8014E684, NULL, NULL, D_actor_260500_8014E69C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014E90C[3] = {
#include "assets/actor_260500_animation_04D18_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014E930[27] = {
#include "assets/actor_260500_animation_04D18_bank4.inc"
};

AnimationRecord D_actor_260500_8014E99C[93] = {
#include "assets/actor_260500_animation_04D18_records.inc"
};

u16 D_actor_260500_8014EB10[20] = {
#include "assets/actor_260500_animation_04D18_indices.inc"
};

AnimationSet D_actor_260500_8014EB38 = {
    D_actor_260500_8014E99C,
    D_actor_260500_8014EB10,
    { NULL, D_actor_260500_8014E90C, NULL, NULL, D_actor_260500_8014E930, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014EB60[3] = {
#include "assets/actor_260500_animation_04ED8_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014EB84[20] = {
#include "assets/actor_260500_animation_04ED8_bank4.inc"
};

AnimationRecord D_actor_260500_8014EBD4[63] = {
#include "assets/actor_260500_animation_04ED8_records.inc"
};

u16 D_actor_260500_8014ECD0[20] = {
#include "assets/actor_260500_animation_04ED8_indices.inc"
};

AnimationSet D_actor_260500_8014ECF8 = {
    D_actor_260500_8014EBD4,
    D_actor_260500_8014ECD0,
    { NULL, D_actor_260500_8014EB60, NULL, NULL, D_actor_260500_8014EB84, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014ED20[3] = {
#include "assets/actor_260500_animation_05234_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014ED44[49] = {
#include "assets/actor_260500_animation_05234_bank4.inc"
};

AnimationRecord D_actor_260500_8014EE08[137] = {
#include "assets/actor_260500_animation_05234_records.inc"
};

u16 D_actor_260500_8014F02C[20] = {
#include "assets/actor_260500_animation_05234_indices.inc"
};

AnimationSet D_actor_260500_8014F054 = {
    D_actor_260500_8014EE08,
    D_actor_260500_8014F02C,
    { NULL, D_actor_260500_8014ED20, NULL, NULL, D_actor_260500_8014ED44, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014F07C[3] = {
#include "assets/actor_260500_animation_05400_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014F0A0[29] = {
#include "assets/actor_260500_animation_05400_bank4.inc"
};

AnimationRecord D_actor_260500_8014F114[57] = {
#include "assets/actor_260500_animation_05400_records.inc"
};

u16 D_actor_260500_8014F1F8[20] = {
#include "assets/actor_260500_animation_05400_indices.inc"
};

AnimationSet D_actor_260500_8014F220 = {
    D_actor_260500_8014F114,
    D_actor_260500_8014F1F8,
    { NULL, D_actor_260500_8014F07C, NULL, NULL, D_actor_260500_8014F0A0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014F248[4] = {
#include "assets/actor_260500_animation_057B0_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014F278[82] = {
#include "assets/actor_260500_animation_057B0_bank4.inc"
};

AnimationRecord D_actor_260500_8014F3C0[122] = {
#include "assets/actor_260500_animation_057B0_records.inc"
};

u16 D_actor_260500_8014F5A8[20] = {
#include "assets/actor_260500_animation_057B0_indices.inc"
};

AnimationSet D_actor_260500_8014F5D0 = {
    D_actor_260500_8014F3C0,
    D_actor_260500_8014F5A8,
    { NULL, D_actor_260500_8014F248, NULL, NULL, D_actor_260500_8014F278, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014F5F8[2] = {
#include "assets/actor_260500_animation_059D4_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014F610[26] = {
#include "assets/actor_260500_animation_059D4_bank4.inc"
};

AnimationRecord D_actor_260500_8014F678[85] = {
#include "assets/actor_260500_animation_059D4_records.inc"
};

u16 D_actor_260500_8014F7CC[20] = {
#include "assets/actor_260500_animation_059D4_indices.inc"
};

AnimationSet D_actor_260500_8014F7F4 = {
    D_actor_260500_8014F678,
    D_actor_260500_8014F7CC,
    { NULL, D_actor_260500_8014F5F8, NULL, NULL, D_actor_260500_8014F610, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014F81C[6] = {
#include "assets/actor_260500_animation_05DB8_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014F864[63] = {
#include "assets/actor_260500_animation_05DB8_bank4.inc"
};

AnimationRecord D_actor_260500_8014F960[148] = {
#include "assets/actor_260500_animation_05DB8_records.inc"
};

u16 D_actor_260500_8014FBB0[20] = {
#include "assets/actor_260500_animation_05DB8_indices.inc"
};

AnimationSet D_actor_260500_8014FBD8 = {
    D_actor_260500_8014F960,
    D_actor_260500_8014FBB0,
    { NULL, D_actor_260500_8014F81C, NULL, NULL, D_actor_260500_8014F864, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014FC00[3] = {
#include "assets/actor_260500_animation_060B8_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014FC24[62] = {
#include "assets/actor_260500_animation_060B8_bank4.inc"
};

AnimationRecord D_actor_260500_8014FD1C[101] = {
#include "assets/actor_260500_animation_060B8_records.inc"
};

u16 D_actor_260500_8014FEB0[20] = {
#include "assets/actor_260500_animation_060B8_indices.inc"
};

AnimationSet D_actor_260500_8014FED8 = {
    D_actor_260500_8014FD1C,
    D_actor_260500_8014FEB0,
    { NULL, D_actor_260500_8014FC00, NULL, NULL, D_actor_260500_8014FC24, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8014FF00[5] = {
#include "assets/actor_260500_animation_06474_bank1.inc"
};

AnimationPackedRotation D_actor_260500_8014FF3C[76] = {
#include "assets/actor_260500_animation_06474_bank4.inc"
};

AnimationRecord D_actor_260500_8015006C[128] = {
#include "assets/actor_260500_animation_06474_records.inc"
};

u16 D_actor_260500_8015026C[20] = {
#include "assets/actor_260500_animation_06474_indices.inc"
};

AnimationSet D_actor_260500_80150294 = {
    D_actor_260500_8015006C,
    D_actor_260500_8015026C,
    { NULL, D_actor_260500_8014FF00, NULL, NULL, D_actor_260500_8014FF3C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_801502BC[3] = {
#include "assets/actor_260500_animation_06640_bank1.inc"
};

AnimationPackedRotation D_actor_260500_801502E0[29] = {
#include "assets/actor_260500_animation_06640_bank4.inc"
};

AnimationRecord D_actor_260500_80150354[57] = {
#include "assets/actor_260500_animation_06640_records.inc"
};

u16 D_actor_260500_80150438[20] = {
#include "assets/actor_260500_animation_06640_indices.inc"
};

AnimationSet D_actor_260500_80150460 = {
    D_actor_260500_80150354,
    D_actor_260500_80150438,
    { NULL, D_actor_260500_801502BC, NULL, NULL, D_actor_260500_801502E0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_80150488[4] = {
#include "assets/actor_260500_animation_06878_bank1.inc"
};

AnimationPackedRotation D_actor_260500_801504B8[41] = {
#include "assets/actor_260500_animation_06878_bank4.inc"
};

AnimationRecord D_actor_260500_8015055C[69] = {
#include "assets/actor_260500_animation_06878_records.inc"
};

u16 D_actor_260500_80150670[20] = {
#include "assets/actor_260500_animation_06878_indices.inc"
};

AnimationSet D_actor_260500_80150698 = {
    D_actor_260500_8015055C,
    D_actor_260500_80150670,
    { NULL, D_actor_260500_80150488, NULL, NULL, D_actor_260500_801504B8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_801506C0[3] = {
#include "assets/actor_260500_animation_06AF0_bank1.inc"
};

AnimationPackedRotation D_actor_260500_801506E4[28] = {
#include "assets/actor_260500_animation_06AF0_bank4.inc"
};

AnimationRecord D_actor_260500_80150754[101] = {
#include "assets/actor_260500_animation_06AF0_records.inc"
};

u16 D_actor_260500_801508E8[20] = {
#include "assets/actor_260500_animation_06AF0_indices.inc"
};

AnimationSet D_actor_260500_80150910 = {
    D_actor_260500_80150754,
    D_actor_260500_801508E8,
    { NULL, D_actor_260500_801506C0, NULL, NULL, D_actor_260500_801506E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_80150938[6] = {
#include "assets/actor_260500_animation_06DF0_bank1.inc"
};

AnimationPackedRotation D_actor_260500_80150980[61] = {
#include "assets/actor_260500_animation_06DF0_bank4.inc"
};

AnimationRecord D_actor_260500_80150A74[93] = {
#include "assets/actor_260500_animation_06DF0_records.inc"
};

u16 D_actor_260500_80150BE8[20] = {
#include "assets/actor_260500_animation_06DF0_indices.inc"
};

AnimationSet D_actor_260500_80150C10 = {
    D_actor_260500_80150A74,
    D_actor_260500_80150BE8,
    { NULL, D_actor_260500_80150938, NULL, NULL, D_actor_260500_80150980, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_80150C38[4] = {
#include "assets/actor_260500_animation_07118_bank1.inc"
};

AnimationPackedRotation D_actor_260500_80150C68[57] = {
#include "assets/actor_260500_animation_07118_bank4.inc"
};

AnimationRecord D_actor_260500_80150D4C[113] = {
#include "assets/actor_260500_animation_07118_records.inc"
};

u16 D_actor_260500_80150F10[20] = {
#include "assets/actor_260500_animation_07118_indices.inc"
};

AnimationSet D_actor_260500_80150F38 = {
    D_actor_260500_80150D4C,
    D_actor_260500_80150F10,
    { NULL, D_actor_260500_80150C38, NULL, NULL, D_actor_260500_80150C68, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_80150F60[8] = {
#include "assets/actor_260500_animation_07698_bank1.inc"
};

AnimationPackedRotation D_actor_260500_80150FC0[119] = {
#include "assets/actor_260500_animation_07698_bank4.inc"
};

AnimationRecord D_actor_260500_8015119C[189] = {
#include "assets/actor_260500_animation_07698_records.inc"
};

u16 D_actor_260500_80151490[20] = {
#include "assets/actor_260500_animation_07698_indices.inc"
};

AnimationSet D_actor_260500_801514B8 = {
    D_actor_260500_8015119C,
    D_actor_260500_80151490,
    { NULL, D_actor_260500_80150F60, NULL, NULL, D_actor_260500_80150FC0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_801514E0[3] = {
#include "assets/actor_260500_animation_07870_bank1.inc"
};

AnimationPackedRotation D_actor_260500_80151504[32] = {
#include "assets/actor_260500_animation_07870_bank4.inc"
};

AnimationRecord D_actor_260500_80151584[57] = {
#include "assets/actor_260500_animation_07870_records.inc"
};

u16 D_actor_260500_80151668[20] = {
#include "assets/actor_260500_animation_07870_indices.inc"
};

AnimationSet D_actor_260500_80151690 = {
    D_actor_260500_80151584,
    D_actor_260500_80151668,
    { NULL, D_actor_260500_801514E0, NULL, NULL, D_actor_260500_80151504, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_801516B8[4] = {
#include "assets/actor_260500_animation_07B3C_bank1.inc"
};

AnimationPackedRotation D_actor_260500_801516E8[51] = {
#include "assets/actor_260500_animation_07B3C_bank4.inc"
};

AnimationRecord D_actor_260500_801517B4[96] = {
#include "assets/actor_260500_animation_07B3C_records.inc"
};

u16 D_actor_260500_80151934[20] = {
#include "assets/actor_260500_animation_07B3C_indices.inc"
};

AnimationSet D_actor_260500_8015195C = {
    D_actor_260500_801517B4,
    D_actor_260500_80151934,
    { NULL, D_actor_260500_801516B8, NULL, NULL, D_actor_260500_801516E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_80151984[3] = {
#include "assets/actor_260500_animation_07D1C_bank1.inc"
};

AnimationPackedRotation D_actor_260500_801519A8[34] = {
#include "assets/actor_260500_animation_07D1C_bank4.inc"
};

AnimationRecord D_actor_260500_80151A30[57] = {
#include "assets/actor_260500_animation_07D1C_records.inc"
};

u16 D_actor_260500_80151B14[20] = {
#include "assets/actor_260500_animation_07D1C_indices.inc"
};

AnimationSet D_actor_260500_80151B3C = {
    D_actor_260500_80151A30,
    D_actor_260500_80151B14,
    { NULL, D_actor_260500_80151984, NULL, NULL, D_actor_260500_801519A8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_80151B64[3] = {
#include "assets/actor_260500_animation_07FC8_bank1.inc"
};

AnimationPackedRotation D_actor_260500_80151B88[41] = {
#include "assets/actor_260500_animation_07FC8_bank4.inc"
};

AnimationRecord D_actor_260500_80151C2C[101] = {
#include "assets/actor_260500_animation_07FC8_records.inc"
};

u16 D_actor_260500_80151DC0[20] = {
#include "assets/actor_260500_animation_07FC8_indices.inc"
};

AnimationSet D_actor_260500_80151DE8 = {
    D_actor_260500_80151C2C,
    D_actor_260500_80151DC0,
    { NULL, D_actor_260500_80151B64, NULL, NULL, D_actor_260500_80151B88, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_80151E10[2] = {
#include "assets/actor_260500_animation_081E4_bank1.inc"
};

AnimationPackedRotation D_actor_260500_80151E28[24] = {
#include "assets/actor_260500_animation_081E4_bank4.inc"
};

AnimationRecord D_actor_260500_80151E88[85] = {
#include "assets/actor_260500_animation_081E4_records.inc"
};

u16 D_actor_260500_80151FDC[20] = {
#include "assets/actor_260500_animation_081E4_indices.inc"
};

AnimationSet D_actor_260500_80152004 = {
    D_actor_260500_80151E88,
    D_actor_260500_80151FDC,
    { NULL, D_actor_260500_80151E10, NULL, NULL, D_actor_260500_80151E28, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8015202C[6] = {
#include "assets/actor_260500_animation_084E4_bank1.inc"
};

AnimationPackedRotation D_actor_260500_80152074[61] = {
#include "assets/actor_260500_animation_084E4_bank4.inc"
};

AnimationRecord D_actor_260500_80152168[93] = {
#include "assets/actor_260500_animation_084E4_records.inc"
};

u16 D_actor_260500_801522DC[20] = {
#include "assets/actor_260500_animation_084E4_indices.inc"
};

AnimationSet D_actor_260500_80152304 = {
    D_actor_260500_80152168,
    D_actor_260500_801522DC,
    { NULL, D_actor_260500_8015202C, NULL, NULL, D_actor_260500_80152074, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_8015232C[6] = {
#include "assets/actor_260500_animation_087E8_bank1.inc"
};

AnimationPackedRotation D_actor_260500_80152374[46] = {
#include "assets/actor_260500_animation_087E8_bank4.inc"
};

AnimationRecord D_actor_260500_8015242C[109] = {
#include "assets/actor_260500_animation_087E8_records.inc"
};

u16 D_actor_260500_801525E0[20] = {
#include "assets/actor_260500_animation_087E8_indices.inc"
};

AnimationSet D_actor_260500_80152608 = {
    D_actor_260500_8015242C,
    D_actor_260500_801525E0,
    { NULL, D_actor_260500_8015232C, NULL, NULL, D_actor_260500_80152374, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_80152630[10] = {
#include "assets/actor_260500_animation_08D68_bank1.inc"
};

AnimationPackedRotation D_actor_260500_801526A8[104] = {
#include "assets/actor_260500_animation_08D68_bank4.inc"
};

AnimationRecord D_actor_260500_80152848[198] = {
#include "assets/actor_260500_animation_08D68_records.inc"
};

u16 D_actor_260500_80152B60[20] = {
#include "assets/actor_260500_animation_08D68_indices.inc"
};

AnimationSet D_actor_260500_80152B88 = {
    D_actor_260500_80152848,
    D_actor_260500_80152B60,
    { NULL, D_actor_260500_80152630, NULL, NULL, D_actor_260500_801526A8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_80152BB0[8] = {
#include "assets/actor_260500_animation_09148_bank1.inc"
};

AnimationPackedRotation D_actor_260500_80152C10[85] = {
#include "assets/actor_260500_animation_09148_bank4.inc"
};

AnimationRecord D_actor_260500_80152D64[119] = {
#include "assets/actor_260500_animation_09148_records.inc"
};

u16 D_actor_260500_80152F40[20] = {
#include "assets/actor_260500_animation_09148_indices.inc"
};

AnimationSet D_actor_260500_80152F68 = {
    D_actor_260500_80152D64,
    D_actor_260500_80152F40,
    { NULL, D_actor_260500_80152BB0, NULL, NULL, D_actor_260500_80152C10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_80152F90[5] = {
#include "assets/actor_260500_animation_094B0_bank1.inc"
};

AnimationPackedRotation D_actor_260500_80152FCC[74] = {
#include "assets/actor_260500_animation_094B0_bank4.inc"
};

AnimationRecord D_actor_260500_801530F4[109] = {
#include "assets/actor_260500_animation_094B0_records.inc"
};

u16 D_actor_260500_801532A8[20] = {
#include "assets/actor_260500_animation_094B0_indices.inc"
};

AnimationSet D_actor_260500_801532D0 = {
    D_actor_260500_801530F4,
    D_actor_260500_801532A8,
    { NULL, D_actor_260500_80152F90, NULL, NULL, D_actor_260500_80152FCC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_801532F8[5] = {
#include "assets/actor_260500_animation_097C8_bank1.inc"
};

AnimationPackedRotation D_actor_260500_80153334[66] = {
#include "assets/actor_260500_animation_097C8_bank4.inc"
};

AnimationRecord D_actor_260500_8015343C[97] = {
#include "assets/actor_260500_animation_097C8_records.inc"
};

u16 D_actor_260500_801535C0[20] = {
#include "assets/actor_260500_animation_097C8_indices.inc"
};

AnimationSet D_actor_260500_801535E8 = {
    D_actor_260500_8015343C,
    D_actor_260500_801535C0,
    { NULL, D_actor_260500_801532F8, NULL, NULL, D_actor_260500_80153334, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_80153610[8] = {
#include "assets/actor_260500_animation_09B9C_bank1.inc"
};

AnimationPackedRotation D_actor_260500_80153670[84] = {
#include "assets/actor_260500_animation_09B9C_bank4.inc"
};

AnimationRecord D_actor_260500_801537C0[117] = {
#include "assets/actor_260500_animation_09B9C_records.inc"
};

u16 D_actor_260500_80153994[20] = {
#include "assets/actor_260500_animation_09B9C_indices.inc"
};

AnimationSet D_actor_260500_801539BC = {
    D_actor_260500_801537C0,
    D_actor_260500_80153994,
    { NULL, D_actor_260500_80153610, NULL, NULL, D_actor_260500_80153670, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_801539E4[7] = {
#include "assets/actor_260500_animation_09F8C_bank1.inc"
};

AnimationPackedRotation D_actor_260500_80153A38[62] = {
#include "assets/actor_260500_animation_09F8C_bank4.inc"
};

AnimationRecord D_actor_260500_80153B30[149] = {
#include "assets/actor_260500_animation_09F8C_records.inc"
};

u16 D_actor_260500_80153D84[20] = {
#include "assets/actor_260500_animation_09F8C_indices.inc"
};

AnimationSet D_actor_260500_80153DAC = {
    D_actor_260500_80153B30,
    D_actor_260500_80153D84,
    { NULL, D_actor_260500_801539E4, NULL, NULL, D_actor_260500_80153A38, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_260500_80153DD4[7] = {
#include "assets/actor_260500_animation_0A2F8_bank1.inc"
};

AnimationPackedRotation D_actor_260500_80153E28[74] = {
#include "assets/actor_260500_animation_0A2F8_bank4.inc"
};

AnimationRecord D_actor_260500_80153F50[104] = {
#include "assets/actor_260500_animation_0A2F8_records.inc"
};

u16 D_actor_260500_801540F0[20] = {
#include "assets/actor_260500_animation_0A2F8_indices.inc"
};

AnimationSet D_actor_260500_80154118 = {
    D_actor_260500_80153F50,
    D_actor_260500_801540F0,
    { NULL, D_actor_260500_80153DD4, NULL, NULL, D_actor_260500_80153E28, NULL, NULL, NULL },
};

TmdBone D_actor_260500_80154140[19] = {
#include "assets/actor_260500_model_0FF38_skeleton.inc"
};

u32 D_actor_260500_801543EC[19] = {
#include "assets/actor_260500_model_0FF38_partVerts.inc"
};

SVECTOR D_actor_260500_80154438[369] = {
#include "assets/actor_260500_model_0FF38_verts.inc"
};

SVECTOR D_actor_260500_80154FC0[369] = {
#include "assets/actor_260500_model_0FF38_normals.inc"
};

u32 D_actor_260500_80155B48[4228] = {
#include "assets/actor_260500_model_0FF38_stream.inc"
};

TmdSource D_actor_260500_80159D58 = {
    0,
    22752,
    6928,
    19,
    D_actor_260500_801543EC,
    D_actor_260500_80154438,
    D_actor_260500_80154FC0,
    D_actor_260500_80154140,
    D_actor_260500_80155B48,
};

s16 D_actor_260500_80159D7C = 8;

Actor260500MsgEntry D_actor_260500_80159D80[6] = {
    { 2003, { .call0 = func_actor_260500_8014A6C4 } },
    { 2005, { .call4 = func_actor_260500_8014A754 } },
    { 2004, { .call2 = func_actor_260500_8014A79C } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = func_actor_260500_8014A818 } },
    { 2013, { .call3 = func_actor_260500_8014A83C } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_260500_80159DB0 = { 1, 192, func_actor_260500_8014A460, { .model = &D_actor_260500_80159D58 } };

u8 D_actor_260500_80159DBC[144] = {
    0,
    0,
    0,
    0,
    24,
    225,
    20,
    128,
    92,
    230,
    20,
    128,
    228,
    232,
    20,
    128,
    56,
    235,
    20,
    128,
    248,
    236,
    20,
    128,
    84,
    240,
    20,
    128,
    32,
    242,
    20,
    128,
    208,
    245,
    20,
    128,
    244,
    247,
    20,
    128,
    216,
    251,
    20,
    128,
    216,
    254,
    20,
    128,
    148,
    2,
    21,
    128,
    96,
    4,
    21,
    128,
    152,
    6,
    21,
    128,
    16,
    9,
    21,
    128,
    16,
    12,
    21,
    128,
    56,
    15,
    21,
    128,
    184,
    20,
    21,
    128,
    144,
    22,
    21,
    128,
    92,
    25,
    21,
    128,
    60,
    27,
    21,
    128,
    232,
    29,
    21,
    128,
    4,
    32,
    21,
    128,
    4,
    35,
    21,
    128,
    0,
    0,
    0,
    0,
    8,
    38,
    21,
    128,
    136,
    43,
    21,
    128,
    104,
    47,
    21,
    128,
    208,
    50,
    21,
    128,
    232,
    53,
    21,
    128,
    188,
    57,
    21,
    128,
    172,
    61,
    21,
    128,
    24,
    65,
    21,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

Actor260500Work* D_actor_260500_80159E4C = NULL;

Task* D_actor_260500_80159E50;

s16 D_actor_260500_80159E54;

static void func_actor_260500_80149E80(void);
static void func_actor_260500_80149EBC(void);
static void func_actor_260500_80149FB0(GpEnemy* enemy, Task* task);

/// Loads cap file 2 and starts it (`func_800E6D4C(0x340, 0)`) when `arg0` is
/// non-zero, otherwise resets the cap state.
void func_actor_260500_80149E38(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(2);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

/// Sends message 0x7D4 (placement) with the record at
/// `D_actor_260500_8014CAF4.data.placements[0]` to the task in lookup slot 4, when there is one.
static void func_actor_260500_80149E80(void)
{
    Task* slot;

    slot = Gp_LookupSlot4(0);
    if (slot != 0) {
        Gp_DispatchMsgPtr(slot, 0x7D4, &D_actor_260500_8014CAF4.data.placements[0], 0);
    }
}

static void func_actor_260500_80149EBC(void)
{
    switch (GameFlag_GetNibble(0xE3)) {
        case 0:
            func_800E8634(D_actor_260500_8014CBF8, 0, D_actor_260500_8014D630);
            GameFlag_SetNibble(0xE3, 1);
            break;
        case 1:
            func_800E8614(D_actor_260500_8014D7C8, 0);
            GameFlag_SetNibble(0xE3, 2);
            break;
        case 2:
            func_800E8614(D_actor_260500_8014D948, 0);
            GameFlag_SetNibble(0xE3, 3);
            break;
        case 3:
            func_800E8614(D_actor_260500_8014DAB0, 0);
            GameFlag_SetNibble(0xE3, 4);
            break;
        case 4:
            func_800E8614(D_actor_260500_8014DCC0, 0);
            break;
    }
}

/// Spawn routine (state 0 of `func_actor_260500_8014A460`): allocates the work
/// block and publishes it in `D_actor_260500_80159E4C` and the task's `work`
/// slot, binds the model to the view and hands it the block's light and colour
/// matrices, publishes the task in `D_actor_260500_80159E50`, relights the
/// model from a point 0x320 above its translation and binds the animation
/// stream. It then installs the message table and runs the first update with
/// the reset mode 2 / id 4 it seeds.
static void func_actor_260500_80149FB0(GpEnemy* enemy, Task* task)
{
    VECTOR     vec;
    GfxCoord*  coord;
    TmdObject* obj;
    void*      work;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(sizeof(Actor260500Work), 0);
    D_actor_260500_80159E4C = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_260500_8014A540;
    coord->parent                = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->lightMtx                = &D_actor_260500_80159E4C->light;
    obj->colorMtx                = &D_actor_260500_80159E4C->color;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    D_actor_260500_80159E50      = task;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_260500_80159E4C->rig.anim, D_actor_260500_80159DBC, obj,
                  D_actor_260500_80159E4C->rig.poses, D_actor_260500_80159E4C->rig.slots);
    D_actor_260500_80159E4C->st.animId  = 4;
    D_actor_260500_80159E4C->st.state   = 2;
    D_actor_260500_80159E4C->st.travel  = 0;
    D_actor_260500_80159E4C->turnFrames = 0;
    task->msgTable                      = D_actor_260500_80159D80;
    func_actor_260500_8014A110(task);
    task->state++;
}

/// Per-frame update: reset modes 1 and 2 run their one-shot reseed (blended or
/// plain) and switch to mode 3. In mode 3 the walking animations 2, 0xE and 0xF
/// step the model forward while `st.travel` counts down, by a distance the
/// approach mode in `D_actor_260500_80159E54` picks, and blend into animation
/// 0xD with reset argument 10 when the walk ends; animation 3 turns the model
/// while `turnFrames` counts down. Mode 3 then ticks the animation.
static void func_actor_260500_8014A110(Task* task)
{
    GfxCoord*        coord = task->extra.tmd->coords;
    Actor260500Work* work  = (Actor260500Work*)task->work;

    if (D_actor_260500_80159E4C->st.state == 1) {
        func_actor_260500_8014A644();
        D_actor_260500_80159E4C->st.state = 3;
    } else if (D_actor_260500_80159E4C->st.state == 2) {
        func_actor_260500_8014A5B4();
        D_actor_260500_80159E4C->st.state = 3;
    } else if (D_actor_260500_80159E4C->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (D_actor_260500_80159E54) {
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
                    D_actor_260500_80159D7C = 10;
                    work->st.animId         = 0xD;
                }
            }
        }
        if (work->st.animId == 3 && work->turnFrames != 0) {
            work->st.yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->turnFrames--;
        }
        func_actor_260500_8014A568();
    }
}

/// Two-state task handler: publishes the task's work block in
/// `D_actor_260500_80159E4C` on the way through, then calls the spawn routine
/// or the per-frame state, whichever `Task::state` selects from a table built
/// on the stack.
void func_actor_260500_8014A460(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_260500_80149FB0,
        func_actor_260500_8014A4BC,
    };

    D_actor_260500_80159E4C = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Per-frame state (state 1 of `func_actor_260500_8014A460`): refreshes the
/// model root's world matrix, relights the model from a point 0x320 above its
/// translation, then runs the update and draws the ground shadow.
static void func_actor_260500_8014A4BC(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     pos;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_260500_8014A110(task);
    func_actor_260500_8014A99C(task);
}

/// `Task::exitCallback` the spawn routine installs: hands the task's `GpEnemy`
/// back to `Gp_DestroyEnemy`.
static void func_actor_260500_8014A540(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Ticks animation slots 1..0x12 of the work block's animation context.
static void func_actor_260500_8014A568(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_260500_80159E4C->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Plain reseed: marks animation slots 1..0x12 of the work block reset-pending
/// and reseeds each of them from the current animation id, then records that id
/// as the one now playing.
static void func_actor_260500_8014A5B4(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_260500_80159E4C->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_260500_80159E4C->rig.anim, i, D_actor_260500_80159E4C->st.animId);
        i++;
    } while (i < 0x13);
    D_actor_260500_80159E4C->st.appliedAnimId = D_actor_260500_80159E4C->st.animId;
}

/// Blended reseed: reseeds animation slots 1..0x12 of the work block from the
/// current animation id with the latched reset argument
/// `D_actor_260500_80159D7C`, and records that id as the one now playing.
static void func_actor_260500_8014A644(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_260500_80159E4C->rig.anim, i, D_actor_260500_80159E4C->st.animId, 0,
                      D_actor_260500_80159D7C);
        i++;
    } while (i < 0x13);
    D_actor_260500_80159E4C->st.appliedAnimId = D_actor_260500_80159E4C->st.animId;
}

/// Play-animation message handler: adopts the preset's animation id when it is
/// one of the first 0x24, latching the reset mode -- 1 for the blended reseed,
/// 2 for the plain one -- and the reset argument the blended reseed forwards,
/// then runs the update on the actor's task. Ids past the range are rejected
/// with -1 and leave the work block untouched.
s32 func_actor_260500_8014A6C4(Task* task, s32 arg1, AnimationPlayRequest* preset)
{
    if (preset->animationId < 0x24) {
        D_actor_260500_80159E4C->st.animId = preset->animationId;
        if (preset->blend != ANIMATION_BLEND_RESET) {
            D_actor_260500_80159E4C->st.state = 1;
            D_actor_260500_80159D7C           = preset->blendFrames;
        } else {
            D_actor_260500_80159E4C->st.state = 2;
        }
        D_actor_260500_80159E4C->st.field_6 = 0;
        func_actor_260500_8014A110(D_actor_260500_80159E50);
        return 0;
    }
    return -1;
}

/// Visibility handler: bit 0 of `arg2` shows the actor's model (flags 0) or
/// hides it (0x80), and bit 1 ORs in 0x4.
s32 func_actor_260500_8014A754(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = D_actor_260500_80159E50->extra.tmd;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (arg2 & 2) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Placement handler: turns the model to the placement's yaw, keeping that yaw
/// in the work block, and moves it to the placement's position. Only the Y
/// rotation is applied.
s32 func_actor_260500_8014A79C(Task* task, s32 arg1, ActorTransform* placement)
{
    GfxCoord* coord;
    u16       yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_260500_80159E4C->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Message 0x7DB: a zero payload halfword at 0x2 arms the work block's
/// `turnFrames` at 0x14.
s32 func_actor_260500_8014A818(Task* task, s32 arg1, ActorCommand* msg)
{
    if (msg->command == 0) {
        D_actor_260500_80159E4C->turnFrames = 0x14;
    }
    return 0;
}

/// Approach handler: turns the model to face `target` -- away from it in mode
/// 1, where the update then walks it backwards -- keeps the mode in
/// `D_actor_260500_80159E54`, and stores the number of steps the walk takes:
/// the planar distance over the mode's step length, 60 in mode 0, 15 in mode 1
/// and 25 in mode 2.
s32 func_actor_260500_8014A83C(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GfxCoord*        coord;
    Actor260500Work* work;
    s32              steps;
    s32              dx;
    s32              dz;
    s32              dist;
    s32              angle;

    steps                   = 0;
    coord                   = task->extra.tmd->coords;
    work                    = (Actor260500Work*)task->work;
    D_actor_260500_80159E54 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->st.yaw            = angle;
    if (D_actor_260500_80159E54 == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (D_actor_260500_80159E54) {
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

/// Draws the actor's ground shadow quad under the model root, unless the model
/// is hidden (`flags & 0x80`) or has no buffer yet. The root's world
/// translation is staged in a scratchpad `VECTOR3` rather than on the stack,
/// and the quad is drawn at a fixed brightness of 0xC0.
static void func_actor_260500_8014A99C(Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR3*   vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, 0xC0);
        SCRATCH_POP_BYTES(0x18);
    }
}
