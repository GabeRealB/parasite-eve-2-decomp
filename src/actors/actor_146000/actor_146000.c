#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/dryfield_night_water_hole.h"

// Retained task-shaped record; preserve the callback's actual ABI.
typedef struct {
    u16   flags;
    u16   priority;
    void  (*callback)(GpEnemy*, Task*);
    void* arg;
} Actor146000RetainedTaskSeed;
STATIC_ASSERT_SIZEOF(Actor146000RetainedTaskSeed, 12);
extern Actor146000RetainedTaskSeed D_actor_146000_801351FC;

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*        sets[8];
        AnimationPlayRequest arguments[5];
    } data;
    s32 words[33];
} Actor146000AnimStorage52BC;
STATIC_ASSERT_SIZEOF(Actor146000AnimStorage52BC, 132);

extern Actor146000AnimStorage52BC D_actor_146000_801352BC;

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*        sets[7];
        AnimationPlayRequest arguments[5];
    } data;
    s32 words[32];
} Actor146000AnimStorage5214;
STATIC_ASSERT_SIZEOF(Actor146000AnimStorage5214, 128);

extern Actor146000AnimStorage5214 D_actor_146000_80135214;

extern GpEvsCmd D_actor_146000_80135428[];
extern GpEvsCmd D_actor_146000_80135980[];
extern GpEvsCmd D_actor_146000_80135BD8[];

extern AnimationPlayRequest D_actor_146000_80135294;
extern AnimationPlayRequest D_actor_146000_801352A8;

// Retained parameter record; layout follows the adjacent script arguments.

// Retained parameter record; layout follows the adjacent script arguments.

extern AnimationSet D_actor_146000_80132324;
extern AnimationSet D_actor_146000_80132770;
extern AnimationSet D_actor_146000_80132A98;
extern AnimationSet D_actor_146000_80133018;
extern AnimationSet D_actor_146000_801333F8;
extern AnimationSet D_actor_146000_801336B8;
extern AnimationSet D_actor_146000_8013390C;
extern AnimationSet D_actor_146000_80133CC4;
extern AnimationSet D_actor_146000_80133E9C;
extern AnimationSet D_actor_146000_801342B8;
extern AnimationSet D_actor_146000_80134474;
extern AnimationSet D_actor_146000_8013476C;
extern AnimationSet D_actor_146000_80134A40;
extern AnimationSet D_actor_146000_80134C60;
extern AnimationSet D_actor_146000_801351D4;
void                func_actor_146000_80131E24(Task*);

AnimationPackedPose D_actor_146000_80131F80[6] = {
#include "assets/actor_146000_animation_00504_bank1.inc"
};

AnimationPackedRotation D_actor_146000_80131FC8[64] = {
#include "assets/actor_146000_animation_00504_bank4.inc"
};

AnimationRecord D_actor_146000_801320C8[141] = {
#include "assets/actor_146000_animation_00504_records.inc"
};

u16 D_actor_146000_801322FC[20] = {
#include "assets/actor_146000_animation_00504_indices.inc"
};

AnimationSet D_actor_146000_80132324 = {
    D_actor_146000_801320C8,
    D_actor_146000_801322FC,
    { NULL, D_actor_146000_80131F80, NULL, NULL, D_actor_146000_80131FC8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146000_8013234C[6] = {
#include "assets/actor_146000_animation_00950_bank1.inc"
};

AnimationPackedRotation D_actor_146000_80132394[78] = {
#include "assets/actor_146000_animation_00950_bank4.inc"
};

AnimationRecord D_actor_146000_801324CC[159] = {
#include "assets/actor_146000_animation_00950_records.inc"
};

u16 D_actor_146000_80132748[20] = {
#include "assets/actor_146000_animation_00950_indices.inc"
};

AnimationSet D_actor_146000_80132770 = {
    D_actor_146000_801324CC,
    D_actor_146000_80132748,
    { NULL, D_actor_146000_8013234C, NULL, NULL, D_actor_146000_80132394, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146000_80132798[5] = {
#include "assets/actor_146000_animation_00C78_bank1.inc"
};

AnimationPackedRotation D_actor_146000_801327D4[69] = {
#include "assets/actor_146000_animation_00C78_bank4.inc"
};

AnimationRecord D_actor_146000_801328E8[98] = {
#include "assets/actor_146000_animation_00C78_records.inc"
};

u16 D_actor_146000_80132A70[20] = {
#include "assets/actor_146000_animation_00C78_indices.inc"
};

AnimationSet D_actor_146000_80132A98 = {
    D_actor_146000_801328E8,
    D_actor_146000_80132A70,
    { NULL, D_actor_146000_80132798, NULL, NULL, D_actor_146000_801327D4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146000_80132AC0[10] = {
#include "assets/actor_146000_animation_011F8_bank1.inc"
};

AnimationPackedRotation D_actor_146000_80132B38[104] = {
#include "assets/actor_146000_animation_011F8_bank4.inc"
};

AnimationRecord D_actor_146000_80132CD8[198] = {
#include "assets/actor_146000_animation_011F8_records.inc"
};

u16 D_actor_146000_80132FF0[20] = {
#include "assets/actor_146000_animation_011F8_indices.inc"
};

AnimationSet D_actor_146000_80133018 = {
    D_actor_146000_80132CD8,
    D_actor_146000_80132FF0,
    { NULL, D_actor_146000_80132AC0, NULL, NULL, D_actor_146000_80132B38, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146000_80133040[8] = {
#include "assets/actor_146000_animation_015D8_bank1.inc"
};

AnimationPackedRotation D_actor_146000_801330A0[85] = {
#include "assets/actor_146000_animation_015D8_bank4.inc"
};

AnimationRecord D_actor_146000_801331F4[119] = {
#include "assets/actor_146000_animation_015D8_records.inc"
};

u16 D_actor_146000_801333D0[20] = {
#include "assets/actor_146000_animation_015D8_indices.inc"
};

AnimationSet D_actor_146000_801333F8 = {
    D_actor_146000_801331F4,
    D_actor_146000_801333D0,
    { NULL, D_actor_146000_80133040, NULL, NULL, D_actor_146000_801330A0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146000_80133420[4] = {
#include "assets/actor_146000_animation_01898_bank1.inc"
};

AnimationPackedRotation D_actor_146000_80133450[55] = {
#include "assets/actor_146000_animation_01898_bank4.inc"
};

AnimationRecord D_actor_146000_8013352C[89] = {
#include "assets/actor_146000_animation_01898_records.inc"
};

u16 D_actor_146000_80133690[20] = {
#include "assets/actor_146000_animation_01898_indices.inc"
};

AnimationSet D_actor_146000_801336B8 = {
    D_actor_146000_8013352C,
    D_actor_146000_80133690,
    { NULL, D_actor_146000_80133420, NULL, NULL, D_actor_146000_80133450, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146000_801336E0[2] = {
#include "assets/actor_146000_animation_01AEC_bank1.inc"
};

AnimationPackedRotation D_actor_146000_801336F8[30] = {
#include "assets/actor_146000_animation_01AEC_bank4.inc"
};

AnimationRecord D_actor_146000_80133770[93] = {
#include "assets/actor_146000_animation_01AEC_records.inc"
};

u16 D_actor_146000_801338E4[20] = {
#include "assets/actor_146000_animation_01AEC_indices.inc"
};

AnimationSet D_actor_146000_8013390C = {
    D_actor_146000_80133770,
    D_actor_146000_801338E4,
    { NULL, D_actor_146000_801336E0, NULL, NULL, D_actor_146000_801336F8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146000_80133934[3] = {
#include "assets/actor_146000_animation_01EA4_bank1.inc"
};

AnimationPackedRotation D_actor_146000_80133958[81] = {
#include "assets/actor_146000_animation_01EA4_bank4.inc"
};

AnimationRecord D_actor_146000_80133A9C[128] = {
#include "assets/actor_146000_animation_01EA4_records.inc"
};

u16 D_actor_146000_80133C9C[20] = {
#include "assets/actor_146000_animation_01EA4_indices.inc"
};

AnimationSet D_actor_146000_80133CC4 = {
    D_actor_146000_80133A9C,
    D_actor_146000_80133C9C,
    { NULL, D_actor_146000_80133934, NULL, NULL, D_actor_146000_80133958, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146000_80133CEC[3] = {
#include "assets/actor_146000_animation_0207C_bank1.inc"
};

AnimationPackedRotation D_actor_146000_80133D10[29] = {
#include "assets/actor_146000_animation_0207C_bank4.inc"
};

AnimationRecord D_actor_146000_80133D84[60] = {
#include "assets/actor_146000_animation_0207C_records.inc"
};

u16 D_actor_146000_80133E74[20] = {
#include "assets/actor_146000_animation_0207C_indices.inc"
};

AnimationSet D_actor_146000_80133E9C = {
    D_actor_146000_80133D84,
    D_actor_146000_80133E74,
    { NULL, D_actor_146000_80133CEC, NULL, NULL, D_actor_146000_80133D10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146000_80133EC4[3] = {
#include "assets/actor_146000_animation_02498_bank1.inc"
};

AnimationPackedRotation D_actor_146000_80133EE8[80] = {
#include "assets/actor_146000_animation_02498_bank4.inc"
};

AnimationRecord D_actor_146000_80134028[154] = {
#include "assets/actor_146000_animation_02498_records.inc"
};

u16 D_actor_146000_80134290[20] = {
#include "assets/actor_146000_animation_02498_indices.inc"
};

AnimationSet D_actor_146000_801342B8 = {
    D_actor_146000_80134028,
    D_actor_146000_80134290,
    { NULL, D_actor_146000_80133EC4, NULL, NULL, D_actor_146000_80133EE8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146000_801342E0[2] = {
#include "assets/actor_146000_animation_02654_bank1.inc"
};

AnimationPackedRotation D_actor_146000_801342F8[25] = {
#include "assets/actor_146000_animation_02654_bank4.inc"
};

AnimationRecord D_actor_146000_8013435C[60] = {
#include "assets/actor_146000_animation_02654_records.inc"
};

u16 D_actor_146000_8013444C[20] = {
#include "assets/actor_146000_animation_02654_indices.inc"
};

AnimationSet D_actor_146000_80134474 = {
    D_actor_146000_8013435C,
    D_actor_146000_8013444C,
    { NULL, D_actor_146000_801342E0, NULL, NULL, D_actor_146000_801342F8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146000_8013449C[4] = {
#include "assets/actor_146000_animation_0294C_bank1.inc"
};

AnimationPackedRotation D_actor_146000_801344CC[62] = {
#include "assets/actor_146000_animation_0294C_bank4.inc"
};

AnimationRecord D_actor_146000_801345C4[96] = {
#include "assets/actor_146000_animation_0294C_records.inc"
};

u16 D_actor_146000_80134744[20] = {
#include "assets/actor_146000_animation_0294C_indices.inc"
};

AnimationSet D_actor_146000_8013476C = {
    D_actor_146000_801345C4,
    D_actor_146000_80134744,
    { NULL, D_actor_146000_8013449C, NULL, NULL, D_actor_146000_801344CC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146000_80134794[4] = {
#include "assets/actor_146000_animation_02C20_bank1.inc"
};

AnimationPackedRotation D_actor_146000_801347C4[58] = {
#include "assets/actor_146000_animation_02C20_bank4.inc"
};

AnimationRecord D_actor_146000_801348AC[91] = {
#include "assets/actor_146000_animation_02C20_records.inc"
};

u16 D_actor_146000_80134A18[20] = {
#include "assets/actor_146000_animation_02C20_indices.inc"
};

AnimationSet D_actor_146000_80134A40 = {
    D_actor_146000_801348AC,
    D_actor_146000_80134A18,
    { NULL, D_actor_146000_80134794, NULL, NULL, D_actor_146000_801347C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146000_80134A68[2] = {
#include "assets/actor_146000_animation_02E40_bank1.inc"
};

AnimationPackedRotation D_actor_146000_80134A80[25] = {
#include "assets/actor_146000_animation_02E40_bank4.inc"
};

AnimationRecord D_actor_146000_80134AE4[85] = {
#include "assets/actor_146000_animation_02E40_records.inc"
};

u16 D_actor_146000_80134C38[20] = {
#include "assets/actor_146000_animation_02E40_indices.inc"
};

AnimationSet D_actor_146000_80134C60 = {
    D_actor_146000_80134AE4,
    D_actor_146000_80134C38,
    { NULL, D_actor_146000_80134A68, NULL, NULL, D_actor_146000_80134A80, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_146000_80134C88[2] = {
#include "assets/actor_146000_animation_033B4_bank1.inc"
};

AnimationPackedRotation D_actor_146000_80134CA0[135] = {
#include "assets/actor_146000_animation_033B4_bank4.inc"
};

AnimationRecord D_actor_146000_80134EBC[188] = {
#include "assets/actor_146000_animation_033B4_records.inc"
};

u16 D_actor_146000_801351AC[20] = {
#include "assets/actor_146000_animation_033B4_indices.inc"
};

AnimationSet D_actor_146000_801351D4 = {
    D_actor_146000_80134EBC,
    D_actor_146000_801351AC,
    { NULL, D_actor_146000_80134C88, NULL, NULL, D_actor_146000_80134CA0, NULL, NULL, NULL },
};

Actor146000RetainedTaskSeed D_actor_146000_801351FC = { 0, 192, Gp_DestroyEnemy, NULL };

TaskDesc D_actor_146000_80135208 = { 0, 32, func_actor_146000_80131E24, { .model = NULL } };

Actor146000AnimStorage5214 D_actor_146000_80135214 = { .data = { { &D_actor_146000_80133E9C, &D_actor_146000_801342B8, &D_actor_146000_80134474, &D_actor_146000_8013476C, &D_actor_146000_80134A40, &D_actor_146000_80134C60, &D_actor_146000_801351D4 }, { { { .index = 1 }, 47, 0, 0, 1 }, { { .index = 1 }, 48, 0, 0, 1 }, { { .index = 1 }, 49, 1, 10, 1 }, { { .index = 1 }, 50, 0, 0, 1 }, { { .index = 1 }, 51, 0, 0, 1 } } } };

AnimationPlayRequest D_actor_146000_80135294 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_146000_801352A8 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

Actor146000AnimStorage52BC D_actor_146000_801352BC = { .data = { { &D_actor_146000_80132324, &D_actor_146000_80132770, &D_actor_146000_80132A98, &D_actor_146000_80133018, &D_actor_146000_801333F8, &D_actor_146000_801336B8, &D_actor_146000_8013390C, &D_actor_146000_80133CC4 }, { { { .index = 1 }, 47, 0, 0, 1 }, { { .index = 1 }, 48, 0, 0, 1 }, { { .index = 1 }, 49, 0, 0, 1 }, { { .index = 1 }, 50, 0, 0, 1 }, { { .index = 1 }, 51, 0, 0, 1 } } } };

AnimationPlayRequest D_actor_146000_80135340 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_146000_80135354 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_146000_80135368 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_146000_8013537C = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_146000_80135390 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_146000_801353A4 = { { .index = 6 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

GpCopyArg D_actor_146000_801353B8 = { { .words = D_actor_146000_801352BC.words }, 32 };

GpCopyArg D_actor_146000_801353C0 = { { .words = D_actor_146000_80135214.words }, 32 };

ActorTransform D_actor_146000_801353C8 = { { 4668, 0, -1337, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_146000_801353E0 = { { 5885, 0, -1300, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_146000_801353F8 = { { 0x281E, 0, -1000, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_146000_80135410 = { { 9020, 0, -1000, 0 }, { 0, 1024, 0, 0 } };

GpEvsCmd D_actor_146000_80135428[57] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_146000_801353B8 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_146000_801353C0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135390 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801353A4 }, { .value = 0 } },
    { 41, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_146000_801353F8 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_146000_80135410 }, { .value = 0 } },
    { 36, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135390 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801352A8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_8013537C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135214.data.arguments[0] }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135214.data.arguments[1] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135214.data.arguments[2] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_8013537C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135214.data.arguments[0] }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135214.data.arguments[1] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135214.data.arguments[2] }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801352BC.data.arguments[1] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801352BC.data.arguments[2] }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135214.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135294 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135340 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135354 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 7, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135390 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_146000_80135410 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801353A4 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_146000_80135980[25] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_146000_801353B8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135390 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801353A4 }, { .value = 0 } },
    { 41, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_146000_801353C8 }, { .value = 0 } },
    { 39, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135390 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x53200006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_146000_801353E0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801352BC.data.arguments[0] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 43, { .commands = D_actor_146000_80135428 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_146000_80135BD8[16] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 7, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801353A4 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_146000_80135410 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_146000_801353F8 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

void func_actor_146000_80131E24(Task* arg0)
{
    s32 state;
    s8  session;

    state = arg0->state;
    switch (state) {
        case 0:
            if (GameFlag_GetNibble(0x73) != 0) {
                func_800E8634(D_actor_146000_80135980, 0, D_actor_146000_80135BD8);
                GameFlag_SetNibble(0x4B, 7);
                Mc_SaveData[0].state.at4.loc.warp = 4;
            } else {
                func_800E8634(D_actor_146000_80135428, 1, D_actor_146000_80135BD8);
                Mc_SaveData[0].state.at4.loc.warp = 2;
            }
            arg0->state++;
            return;
        case 1:
            session = gGameSession->eventState;
            if (session == 2) {
                arg0->state = session;
            }
            return;
        case 2:
            SndEvt_EnqueueType7(0x80000000, 0);
            GameFlag_SetNibble(0x4C, 0);
            Gp_ApplyAreaRecs(D_dryfield_night_water_hole_80183618);
            Mc_SaveData[0].state.at4.loc.area = 0x19;
            Mc_SaveData[0].state.at4.loc.room = state;
            gDisplayState.spriteVariant       = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}
