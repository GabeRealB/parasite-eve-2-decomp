#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/rand.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/paced_walk.h"
#include "../../shared/walker.h"

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*  sets[3];
        GpCopyArg      copies[2];
        ActorTransform placements[1];
    } data;
    s32 words[13];
} Actor160600AnimStorage506C;
STATIC_ASSERT_SIZEOF(Actor160600AnimStorage506C, 52);

extern Actor160600AnimStorage506C D_actor_160600_8013506C;

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*        sets[15];
        AnimationPlayRequest arguments[4];
    } data;
    s32 words[35];
} Actor160600AnimStorage4E00;
STATIC_ASSERT_SIZEOF(Actor160600AnimStorage4E00, 140);

extern Actor160600AnimStorage4E00 D_actor_160600_80134E00;

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*);
        s32 (*call1)(Task*, s32, ActorCommand* request);
        s32 (*call2)(Task*, s32, ActorTransform*);
        s32 (*call3)(Task*, s32, s32);
    } handler;
} Actor160600MessageEntry;
STATIC_ASSERT_SIZEOF(Actor160600MessageEntry, 8);

extern Actor160600MessageEntry gPacedWalkMsgTable[6];
extern u8                      gPacedWalkAnimBank[];
extern u8                      gPacedWalkEffectParts[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern TmdSource D_actor_160600_8013BA9C;
void             func_actor_160600_801321B4(Task*);

s32 func_actor_160600_8013268C(Task*, s32, ActorCommand* args);

extern AnimationPlayRequest D_actor_160600_80134E8C;
extern AnimationPlayRequest D_actor_160600_80134EA0;
extern AnimationPlayRequest D_actor_160600_80134EB4;
extern AnimationPlayRequest D_actor_160600_80134EC8;
extern AnimationPlayRequest D_actor_160600_80134EDC;
extern AnimationPlayRequest D_actor_160600_80134EF0;
extern AnimationPlayRequest D_actor_160600_80134F04;
extern AnimationPlayRequest D_actor_160600_80134F18;
extern AnimationPlayRequest D_actor_160600_80134F2C;
extern AnimationPlayRequest D_actor_160600_80134F68;
extern AnimationPlayRequest D_actor_160600_80134F7C;
extern AnimationPlayRequest D_actor_160600_80134F90;
extern AnimationPlayRequest D_actor_160600_80134FA4;
extern AnimationPlayRequest D_actor_160600_80134FB8;
extern AnimationPlayRequest D_actor_160600_80134FCC;
extern AnimationPlayRequest D_actor_160600_80134FE0;
extern AnimationPlayRequest D_actor_160600_80134FF4;
extern AnimationPlayRequest D_actor_160600_80135008;
extern AnimationPlayRequest D_actor_160600_8013501C;
extern AnimationPlayRequest D_actor_160600_80135030;
extern AnimationPlayRequest D_actor_160600_80135044;
extern AnimationPlayRequest D_actor_160600_80135058;
extern AnimationPlayRequest D_actor_160600_801351A8;
extern AnimationPlayRequest D_actor_160600_801351BC;
extern AnimationPlayRequest D_actor_160600_801351D0;
extern AnimationPlayRequest D_actor_160600_801351E4;
extern GpOverrideArg        D_actor_160600_801351F8;
extern GpOverrideArg        D_actor_160600_80135208;
extern ActorTransform       D_actor_160600_801350A0;
extern ActorTransform       D_actor_160600_801350B8;
extern ActorTransform       D_actor_160600_801350D0;
extern ActorTransform       D_actor_160600_801350E8;
extern ActorTransform       D_actor_160600_80135100;
extern ActorTransform       D_actor_160600_80135118;
extern ActorTransform       D_actor_160600_80135130;
extern ActorTransform       D_actor_160600_80135148;
extern ActorTransform       D_actor_160600_80135160;
extern ActorTransform       D_actor_160600_80135178;
extern ActorTransform       D_actor_160600_80135190;
void                        func_actor_160600_80131E24(void);

extern Actor160600AnimStorage506C D_actor_160600_8013506C;
extern AnimationSet               D_actor_160600_80134A00;
extern AnimationSet               D_actor_160600_80134BC0;
extern AnimationSet               D_actor_160600_80134DD8;

AnimationPackedPose D_actor_160600_80132774[3] = {
#include "assets/actor_160600_animation_00AF4_bank1.inc"
};

AnimationPackedRotation D_actor_160600_80132798[28] = {
#include "assets/actor_160600_animation_00AF4_bank4.inc"
};

AnimationRecord D_actor_160600_80132808[57] = {
#include "assets/actor_160600_animation_00AF4_records.inc"
};

u16 D_actor_160600_801328EC[20] = {
#include "assets/actor_160600_animation_00AF4_indices.inc"
};

AnimationSet D_actor_160600_80132914 = {
    D_actor_160600_80132808,
    D_actor_160600_801328EC,
    { NULL, D_actor_160600_80132774, NULL, NULL, D_actor_160600_80132798, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013293C[3] = {
#include "assets/actor_160600_animation_00CC0_bank1.inc"
};

AnimationPackedRotation D_actor_160600_80132960[29] = {
#include "assets/actor_160600_animation_00CC0_bank4.inc"
};

AnimationRecord D_actor_160600_801329D4[57] = {
#include "assets/actor_160600_animation_00CC0_records.inc"
};

u16 D_actor_160600_80132AB8[20] = {
#include "assets/actor_160600_animation_00CC0_indices.inc"
};

AnimationSet D_actor_160600_80132AE0 = {
    D_actor_160600_801329D4,
    D_actor_160600_80132AB8,
    { NULL, D_actor_160600_8013293C, NULL, NULL, D_actor_160600_80132960, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_80132B08[3] = {
#include "assets/actor_160600_animation_00E88_bank1.inc"
};

AnimationPackedRotation D_actor_160600_80132B2C[28] = {
#include "assets/actor_160600_animation_00E88_bank4.inc"
};

AnimationRecord D_actor_160600_80132B9C[57] = {
#include "assets/actor_160600_animation_00E88_records.inc"
};

u16 D_actor_160600_80132C80[20] = {
#include "assets/actor_160600_animation_00E88_indices.inc"
};

AnimationSet D_actor_160600_80132CA8 = {
    D_actor_160600_80132B9C,
    D_actor_160600_80132C80,
    { NULL, D_actor_160600_80132B08, NULL, NULL, D_actor_160600_80132B2C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_80132CD0[4] = {
#include "assets/actor_160600_animation_010F0_bank1.inc"
};

AnimationPackedRotation D_actor_160600_80132D00[39] = {
#include "assets/actor_160600_animation_010F0_bank4.inc"
};

AnimationRecord D_actor_160600_80132D9C[83] = {
#include "assets/actor_160600_animation_010F0_records.inc"
};

u16 D_actor_160600_80132EE8[20] = {
#include "assets/actor_160600_animation_010F0_indices.inc"
};

AnimationSet D_actor_160600_80132F10 = {
    D_actor_160600_80132D9C,
    D_actor_160600_80132EE8,
    { NULL, D_actor_160600_80132CD0, NULL, NULL, D_actor_160600_80132D00, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_80132F38[4] = {
#include "assets/actor_160600_animation_01368_bank1.inc"
};

AnimationPackedRotation D_actor_160600_80132F68[48] = {
#include "assets/actor_160600_animation_01368_bank4.inc"
};

AnimationRecord D_actor_160600_80133028[78] = {
#include "assets/actor_160600_animation_01368_records.inc"
};

u16 D_actor_160600_80133160[20] = {
#include "assets/actor_160600_animation_01368_indices.inc"
};

AnimationSet D_actor_160600_80133188 = {
    D_actor_160600_80133028,
    D_actor_160600_80133160,
    { NULL, D_actor_160600_80132F38, NULL, NULL, D_actor_160600_80132F68, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_801331B0[2] = {
#include "assets/actor_160600_animation_01504_bank1.inc"
};

AnimationPackedRotation D_actor_160600_801331C8[20] = {
#include "assets/actor_160600_animation_01504_bank4.inc"
};

AnimationRecord D_actor_160600_80133218[57] = {
#include "assets/actor_160600_animation_01504_records.inc"
};

u16 D_actor_160600_801332FC[20] = {
#include "assets/actor_160600_animation_01504_indices.inc"
};

AnimationSet D_actor_160600_80133324 = {
    D_actor_160600_80133218,
    D_actor_160600_801332FC,
    { NULL, D_actor_160600_801331B0, NULL, NULL, D_actor_160600_801331C8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013334C[2] = {
#include "assets/actor_160600_animation_017AC_bank1.inc"
};

AnimationPackedRotation D_actor_160600_80133364[41] = {
#include "assets/actor_160600_animation_017AC_bank4.inc"
};

AnimationRecord D_actor_160600_80133408[103] = {
#include "assets/actor_160600_animation_017AC_records.inc"
};

u16 D_actor_160600_801335A4[20] = {
#include "assets/actor_160600_animation_017AC_indices.inc"
};

AnimationSet D_actor_160600_801335CC = {
    D_actor_160600_80133408,
    D_actor_160600_801335A4,
    { NULL, D_actor_160600_8013334C, NULL, NULL, D_actor_160600_80133364, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_801335F4[2] = {
#include "assets/actor_160600_animation_01948_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013360C[20] = {
#include "assets/actor_160600_animation_01948_bank4.inc"
};

AnimationRecord D_actor_160600_8013365C[57] = {
#include "assets/actor_160600_animation_01948_records.inc"
};

u16 D_actor_160600_80133740[20] = {
#include "assets/actor_160600_animation_01948_indices.inc"
};

AnimationSet D_actor_160600_80133768 = {
    D_actor_160600_8013365C,
    D_actor_160600_80133740,
    { NULL, D_actor_160600_801335F4, NULL, NULL, D_actor_160600_8013360C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_80133790[12] = {
#include "assets/actor_160600_animation_01F1C_bank1.inc"
};

AnimationPackedRotation D_actor_160600_80133820[138] = {
#include "assets/actor_160600_animation_01F1C_bank4.inc"
};

AnimationRecord D_actor_160600_80133A48[179] = {
#include "assets/actor_160600_animation_01F1C_records.inc"
};

u16 D_actor_160600_80133D14[20] = {
#include "assets/actor_160600_animation_01F1C_indices.inc"
};

AnimationSet D_actor_160600_80133D3C = {
    D_actor_160600_80133A48,
    D_actor_160600_80133D14,
    { NULL, D_actor_160600_80133790, NULL, NULL, D_actor_160600_80133820, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_80133D64[19] = {
#include "assets/actor_160600_animation_02734_bank1.inc"
};

AnimationPackedRotation D_actor_160600_80133E48[189] = {
#include "assets/actor_160600_animation_02734_bank4.inc"
};

AnimationRecord D_actor_160600_8013413C[252] = {
#include "assets/actor_160600_animation_02734_records.inc"
};

u16 D_actor_160600_8013452C[20] = {
#include "assets/actor_160600_animation_02734_indices.inc"
};

AnimationSet D_actor_160600_80134554 = {
    D_actor_160600_8013413C,
    D_actor_160600_8013452C,
    { NULL, D_actor_160600_80133D64, NULL, NULL, D_actor_160600_80133E48, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013457C[6] = {
#include "assets/actor_160600_animation_02A38_bank1.inc"
};

AnimationPackedRotation D_actor_160600_801345C4[46] = {
#include "assets/actor_160600_animation_02A38_bank4.inc"
};

AnimationRecord D_actor_160600_8013467C[109] = {
#include "assets/actor_160600_animation_02A38_records.inc"
};

u16 D_actor_160600_80134830[20] = {
#include "assets/actor_160600_animation_02A38_indices.inc"
};

AnimationSet D_actor_160600_80134858 = {
    D_actor_160600_8013467C,
    D_actor_160600_80134830,
    { NULL, D_actor_160600_8013457C, NULL, NULL, D_actor_160600_801345C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_80134880[2] = {
#include "assets/actor_160600_animation_02BE0_bank1.inc"
};

AnimationPackedRotation D_actor_160600_80134898[23] = {
#include "assets/actor_160600_animation_02BE0_bank4.inc"
};

AnimationRecord D_actor_160600_801348F4[57] = {
#include "assets/actor_160600_animation_02BE0_records.inc"
};

u16 D_actor_160600_801349D8[20] = {
#include "assets/actor_160600_animation_02BE0_indices.inc"
};

AnimationSet D_actor_160600_80134A00 = {
    D_actor_160600_801348F4,
    D_actor_160600_801349D8,
    { NULL, D_actor_160600_80134880, NULL, NULL, D_actor_160600_80134898, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_80134A28[2] = {
#include "assets/actor_160600_animation_02DA0_bank1.inc"
};

AnimationPackedRotation D_actor_160600_80134A40[26] = {
#include "assets/actor_160600_animation_02DA0_bank4.inc"
};

AnimationRecord D_actor_160600_80134AA8[60] = {
#include "assets/actor_160600_animation_02DA0_records.inc"
};

u16 D_actor_160600_80134B98[20] = {
#include "assets/actor_160600_animation_02DA0_indices.inc"
};

AnimationSet D_actor_160600_80134BC0 = {
    D_actor_160600_80134AA8,
    D_actor_160600_80134B98,
    { NULL, D_actor_160600_80134A28, NULL, NULL, D_actor_160600_80134A40, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_80134BE8[2] = {
#include "assets/actor_160600_animation_02FB8_bank1.inc"
};

AnimationPackedRotation D_actor_160600_80134C00[28] = {
#include "assets/actor_160600_animation_02FB8_bank4.inc"
};

AnimationRecord D_actor_160600_80134C70[80] = {
#include "assets/actor_160600_animation_02FB8_records.inc"
};

u16 D_actor_160600_80134DB0[20] = {
#include "assets/actor_160600_animation_02FB8_indices.inc"
};

AnimationSet D_actor_160600_80134DD8 = {
    D_actor_160600_80134C70,
    D_actor_160600_80134DB0,
    { NULL, D_actor_160600_80134BE8, NULL, NULL, D_actor_160600_80134C00, NULL, NULL, NULL },
};

Actor160600AnimStorage4E00 D_actor_160600_80134E00 = { .data = { { &D_actor_160600_80132914, &D_actor_160600_80132AE0, &D_actor_160600_80132CA8, &D_actor_160600_80132F10, &D_actor_160600_80133188, NULL, NULL, NULL, NULL, &D_actor_160600_80133324, &D_actor_160600_801335CC, &D_actor_160600_80133768, &D_actor_160600_80133D3C, &D_actor_160600_80134554, &D_actor_160600_80134858 }, { { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_160600_80134E8C = { { .index = 1 }, 4, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134EA0 = { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134EB4 = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134EC8 = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134EDC = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134EF0 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134F04 = { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134F18 = { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134F2C = { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134F40[2] = {
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_160600_80134F68 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134F7C = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134F90 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134FA4 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134FB8 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134FCC = { { .index = 1 }, 57, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134FE0 = { { .index = 1 }, 58, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134FF4 = { { .index = 1 }, 59, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80135008 = { { .index = 1 }, 60, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_8013501C = { { .index = 1 }, 61, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80135030 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_160600_80135044 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_160600_80135058 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

Actor160600AnimStorage506C D_actor_160600_8013506C = { .data = { { &D_actor_160600_80134A00, &D_actor_160600_80134BC0, &D_actor_160600_80134DD8 }, { { { .words = D_actor_160600_8013506C.words }, 10 }, { { .words = D_actor_160600_80134E00.words }, 32 } }, { { { 2350, 0, 1980, 0 }, { 0, 682, 0, 0 } } } } };

ActorTransform D_actor_160600_801350A0 = { { 4870, 0, 3400, 0 }, { 0, 682, 0, 0 } };

ActorTransform D_actor_160600_801350B8 = { { 5250, 0, 3520, 0 }, { 0, 682, 0, 0 } };

ActorTransform D_actor_160600_801350D0 = { { 6080, 0, 3850, 0 }, { 0, -1251, 0, 0 } };

ActorTransform D_actor_160600_801350E8 = { { 6080, 0, 4000, 0 }, { 0, -967, 0, 0 } };

ActorTransform D_actor_160600_80135100 = { { 2300, 0, 1170, 0 }, { 0, -113, 0, 0 } };

ActorTransform D_actor_160600_80135118 = { { 2300, 0, 2600, 0 }, { 0, -113, 0, 0 } };

ActorTransform D_actor_160600_80135130 = { { 4360, 0, 1890, 0 }, { 0, 455, 0, 0 } };

ActorTransform D_actor_160600_80135148 = { { 5080, 0, 2830, 0 }, { 0, 455, 0, 0 } };

ActorTransform D_actor_160600_80135160 = { { 5280, 0, 3010, 0 }, { 0, 455, 0, 0 } };

ActorTransform D_actor_160600_80135178 = { { 2360, 0, 5040, 0 }, { 0, 3470, 0, 0 } };

ActorTransform D_actor_160600_80135190 = { { 2066, 0, 6626, 0 }, { 0, 0, 0, 0 } };

AnimationPlayRequest D_actor_160600_801351A8 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_801351BC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_801351D0 = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_801351E4 = { { .index = 1 }, 38, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

GpOverrideArg D_actor_160600_801351F8 = { 19, 47 };

GpOverrideArg D_actor_160600_80135200 = { 19, 59 };

GpOverrideArg D_actor_160600_80135208 = { 19, 61 };

GpOverrideArg D_actor_160600_80135210 = { 19, 7 };

GpOverrideArg D_actor_160600_80135218 = { 4, 7 };

EvsCommand D_actor_160600_80135220[20] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 11 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_801351A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_160600_8013506C.data.copies[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_160600_8013506C.data.copies[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_160600_80135100 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_160600_801350D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134E00.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134E00.data.arguments[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_160600_80135118 }, { .storage = &D_actor_160600_801351F8 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_160600_801350E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134E00.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_160600_80135400[56] = {
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FCC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134E8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FCC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134F18 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134F2C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FCC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FCC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134EA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FCC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134E00.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_160600_801350B8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_160600_80135160 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80135008 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_160600_80135178 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_8013501C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134EB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 85 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134EC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_160600_80135940[16] = {
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5410000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_801351A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_160600_80135190 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_160600_80131E24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134F04 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5410000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_160600_80135AC0[29] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54100017 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_160600_80135220 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_160600_80135130 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_160600_80135148 }, { .storage = &D_actor_160600_80135208 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FF4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FCC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134E00.data.arguments[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_160600_80135400 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134F68 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_160600_80135178 }, { .storage = &D_actor_160600_80135208 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134EDC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_160600_80135940 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_160600_80135D78[52] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54100017 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_801351BC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_160600_80135220 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_160600_8013506C.data.placements[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1019 }, { .storage = &D_actor_160600_801350A0 }, { .storage = &D_actor_160600_80135218 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_801351D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_801351E4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x40720009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_801351D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134E00.data.arguments[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_160600_80135130 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_160600_80135148 }, { .storage = &D_actor_160600_80135208 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FF4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FCC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_160600_80135400 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80135030 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80135058 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x40720009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134EF0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80135044 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_801351D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134F90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_160600_80135178 }, { .storage = &D_actor_160600_80135208 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134EDC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_160600_80135940 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_160600_80136258[16] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_160600_80131E24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_801351A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_160600_80135190 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160600_80134F04 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_160600_801350E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_actor_160600_801363D8[20] = {
#include "assets/actor_160600_model_09C7C_skeleton.inc"
};

u32 D_actor_160600_801366A8[20] = {
#include "assets/actor_160600_model_09C7C_partVerts.inc"
};

SVECTOR D_actor_160600_801366F8[363] = {
#include "assets/actor_160600_model_09C7C_verts.inc"
};

SVECTOR D_actor_160600_80137250[360] = {
#include "assets/actor_160600_model_09C7C_normals.inc"
};

u32 D_actor_160600_80137D90[3907] = {
#include "assets/actor_160600_model_09C7C_stream.inc"
};

TmdSource D_actor_160600_8013BA9C = {
    0,
    21060,
    6448,
    20,
    D_actor_160600_801366A8,
    D_actor_160600_801366F8,
    D_actor_160600_80137250,
    D_actor_160600_801363D8,
    D_actor_160600_80137D90,
};

AnimationPackedPose D_actor_160600_8013BAC0[2] = {
#include "assets/actor_160600_animation_09ED0_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013BAD8[28] = {
#include "assets/actor_160600_animation_09ED0_bank4.inc"
};

AnimationRecord D_actor_160600_8013BB48[96] = {
#include "assets/actor_160600_animation_09ED0_records.inc"
};

u16 D_actor_160600_8013BCC8[20] = {
#include "assets/actor_160600_animation_09ED0_indices.inc"
};

AnimationSet D_actor_160600_8013BCF0 = {
    D_actor_160600_8013BB48,
    D_actor_160600_8013BCC8,
    { NULL, D_actor_160600_8013BAC0, NULL, NULL, D_actor_160600_8013BAD8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013BD18[2] = {
#include "assets/actor_160600_animation_0A168_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013BD30[46] = {
#include "assets/actor_160600_animation_0A168_bank4.inc"
};

AnimationRecord D_actor_160600_8013BDE8[94] = {
#include "assets/actor_160600_animation_0A168_records.inc"
};

u16 D_actor_160600_8013BF60[20] = {
#include "assets/actor_160600_animation_0A168_indices.inc"
};

AnimationSet D_actor_160600_8013BF88 = {
    D_actor_160600_8013BDE8,
    D_actor_160600_8013BF60,
    { NULL, D_actor_160600_8013BD18, NULL, NULL, D_actor_160600_8013BD30, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013BFB0[2] = {
#include "assets/actor_160600_animation_0A4B0_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013BFC8[58] = {
#include "assets/actor_160600_animation_0A4B0_bank4.inc"
};

AnimationRecord D_actor_160600_8013C0B0[126] = {
#include "assets/actor_160600_animation_0A4B0_records.inc"
};

u16 D_actor_160600_8013C2A8[20] = {
#include "assets/actor_160600_animation_0A4B0_indices.inc"
};

AnimationSet D_actor_160600_8013C2D0 = {
    D_actor_160600_8013C0B0,
    D_actor_160600_8013C2A8,
    { NULL, D_actor_160600_8013BFB0, NULL, NULL, D_actor_160600_8013BFC8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013C2F8[2] = {
#include "assets/actor_160600_animation_0A7E8_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013C310[65] = {
#include "assets/actor_160600_animation_0A7E8_bank4.inc"
};

AnimationRecord D_actor_160600_8013C414[115] = {
#include "assets/actor_160600_animation_0A7E8_records.inc"
};

u16 D_actor_160600_8013C5E0[20] = {
#include "assets/actor_160600_animation_0A7E8_indices.inc"
};

AnimationSet D_actor_160600_8013C608 = {
    D_actor_160600_8013C414,
    D_actor_160600_8013C5E0,
    { NULL, D_actor_160600_8013C2F8, NULL, NULL, D_actor_160600_8013C310, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013C630[2] = {
#include "assets/actor_160600_animation_0AA00_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013C648[28] = {
#include "assets/actor_160600_animation_0AA00_bank4.inc"
};

AnimationRecord D_actor_160600_8013C6B8[80] = {
#include "assets/actor_160600_animation_0AA00_records.inc"
};

u16 D_actor_160600_8013C7F8[20] = {
#include "assets/actor_160600_animation_0AA00_indices.inc"
};

AnimationSet D_actor_160600_8013C820 = {
    D_actor_160600_8013C6B8,
    D_actor_160600_8013C7F8,
    { NULL, D_actor_160600_8013C630, NULL, NULL, D_actor_160600_8013C648, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013C848[2] = {
#include "assets/actor_160600_animation_0AE94_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013C860[107] = {
#include "assets/actor_160600_animation_0AE94_bank4.inc"
};

AnimationRecord D_actor_160600_8013CA0C[160] = {
#include "assets/actor_160600_animation_0AE94_records.inc"
};

u16 D_actor_160600_8013CC8C[20] = {
#include "assets/actor_160600_animation_0AE94_indices.inc"
};

AnimationSet D_actor_160600_8013CCB4 = {
    D_actor_160600_8013CA0C,
    D_actor_160600_8013CC8C,
    { NULL, D_actor_160600_8013C848, NULL, NULL, D_actor_160600_8013C860, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013CCDC[2] = {
#include "assets/actor_160600_animation_0B10C_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013CCF4[28] = {
#include "assets/actor_160600_animation_0B10C_bank4.inc"
};

AnimationRecord D_actor_160600_8013CD64[104] = {
#include "assets/actor_160600_animation_0B10C_records.inc"
};

u16 D_actor_160600_8013CF04[20] = {
#include "assets/actor_160600_animation_0B10C_indices.inc"
};

AnimationSet D_actor_160600_8013CF2C = {
    D_actor_160600_8013CD64,
    D_actor_160600_8013CF04,
    { NULL, D_actor_160600_8013CCDC, NULL, NULL, D_actor_160600_8013CCF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013CF54[2] = {
#include "assets/actor_160600_animation_0B360_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013CF6C[45] = {
#include "assets/actor_160600_animation_0B360_bank4.inc"
};

AnimationRecord D_actor_160600_8013D020[78] = {
#include "assets/actor_160600_animation_0B360_records.inc"
};

u16 D_actor_160600_8013D158[20] = {
#include "assets/actor_160600_animation_0B360_indices.inc"
};

AnimationSet D_actor_160600_8013D180 = {
    D_actor_160600_8013D020,
    D_actor_160600_8013D158,
    { NULL, D_actor_160600_8013CF54, NULL, NULL, D_actor_160600_8013CF6C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013D1A8[2] = {
#include "assets/actor_160600_animation_0B534_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013D1C0[23] = {
#include "assets/actor_160600_animation_0B534_bank4.inc"
};

AnimationRecord D_actor_160600_8013D21C[68] = {
#include "assets/actor_160600_animation_0B534_records.inc"
};

u16 D_actor_160600_8013D32C[20] = {
#include "assets/actor_160600_animation_0B534_indices.inc"
};

AnimationSet D_actor_160600_8013D354 = {
    D_actor_160600_8013D21C,
    D_actor_160600_8013D32C,
    { NULL, D_actor_160600_8013D1A8, NULL, NULL, D_actor_160600_8013D1C0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013D37C[2] = {
#include "assets/actor_160600_animation_0B6D4_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013D394[18] = {
#include "assets/actor_160600_animation_0B6D4_bank4.inc"
};

AnimationRecord D_actor_160600_8013D3DC[60] = {
#include "assets/actor_160600_animation_0B6D4_records.inc"
};

u16 D_actor_160600_8013D4CC[20] = {
#include "assets/actor_160600_animation_0B6D4_indices.inc"
};

AnimationSet D_actor_160600_8013D4F4 = {
    D_actor_160600_8013D3DC,
    D_actor_160600_8013D4CC,
    { NULL, D_actor_160600_8013D37C, NULL, NULL, D_actor_160600_8013D394, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013D51C[2] = {
#include "assets/actor_160600_animation_0B8B0_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013D534[29] = {
#include "assets/actor_160600_animation_0B8B0_bank4.inc"
};

AnimationRecord D_actor_160600_8013D5A8[64] = {
#include "assets/actor_160600_animation_0B8B0_records.inc"
};

u16 D_actor_160600_8013D6A8[20] = {
#include "assets/actor_160600_animation_0B8B0_indices.inc"
};

AnimationSet D_actor_160600_8013D6D0 = {
    D_actor_160600_8013D5A8,
    D_actor_160600_8013D6A8,
    { NULL, D_actor_160600_8013D51C, NULL, NULL, D_actor_160600_8013D534, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013D6F8[2] = {
#include "assets/actor_160600_animation_0BAD8_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013D710[24] = {
#include "assets/actor_160600_animation_0BAD8_bank4.inc"
};

AnimationRecord D_actor_160600_8013D770[88] = {
#include "assets/actor_160600_animation_0BAD8_records.inc"
};

u16 D_actor_160600_8013D8D0[20] = {
#include "assets/actor_160600_animation_0BAD8_indices.inc"
};

AnimationSet D_actor_160600_8013D8F8 = {
    D_actor_160600_8013D770,
    D_actor_160600_8013D8D0,
    { NULL, D_actor_160600_8013D6F8, NULL, NULL, D_actor_160600_8013D710, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013D920[2] = {
#include "assets/actor_160600_animation_0BD04_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013D938[25] = {
#include "assets/actor_160600_animation_0BD04_bank4.inc"
};

AnimationRecord D_actor_160600_8013D99C[88] = {
#include "assets/actor_160600_animation_0BD04_records.inc"
};

u16 D_actor_160600_8013DAFC[20] = {
#include "assets/actor_160600_animation_0BD04_indices.inc"
};

AnimationSet D_actor_160600_8013DB24 = {
    D_actor_160600_8013D99C,
    D_actor_160600_8013DAFC,
    { NULL, D_actor_160600_8013D920, NULL, NULL, D_actor_160600_8013D938, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013DB4C[2] = {
#include "assets/actor_160600_animation_0BF6C_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013DB64[31] = {
#include "assets/actor_160600_animation_0BF6C_bank4.inc"
};

AnimationRecord D_actor_160600_8013DBE0[97] = {
#include "assets/actor_160600_animation_0BF6C_records.inc"
};

u16 D_actor_160600_8013DD64[20] = {
#include "assets/actor_160600_animation_0BF6C_indices.inc"
};

AnimationSet D_actor_160600_8013DD8C = {
    D_actor_160600_8013DBE0,
    D_actor_160600_8013DD64,
    { NULL, D_actor_160600_8013DB4C, NULL, NULL, D_actor_160600_8013DB64, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160600_8013DDB4[2] = {
#include "assets/actor_160600_animation_0C128_bank1.inc"
};

AnimationPackedRotation D_actor_160600_8013DDCC[25] = {
#include "assets/actor_160600_animation_0C128_bank4.inc"
};

AnimationRecord D_actor_160600_8013DE30[60] = {
#include "assets/actor_160600_animation_0C128_records.inc"
};

u16 D_actor_160600_8013DF20[20] = {
#include "assets/actor_160600_animation_0C128_indices.inc"
};

AnimationSet D_actor_160600_8013DF48 = {
    D_actor_160600_8013DE30,
    D_actor_160600_8013DF20,
    { NULL, D_actor_160600_8013DDB4, NULL, NULL, D_actor_160600_8013DDCC, NULL, NULL, NULL },
};

Actor160600MessageEntry gPacedWalkMsgTable[6] = {
    { 2003, { .call0 = pacedWalkPlayAnim } },
    { 2005, { .call3 = pacedWalkShowPair } },
    { 2004, { .call2 = pacedWalkPlace } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = func_actor_160600_8013268C } },
    { 2013, { .call2 = pacedWalkTo } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_160600_8013DFA0 = { { { TASK_BODY_TMD, 96 } }, func_actor_160600_801321B4, { .model = &D_actor_160600_8013BA9C } };

u8 gPacedWalkAnimBank[64] = {
    0,
    0,
    0,
    0,
    240,
    188,
    19,
    128,
    136,
    191,
    19,
    128,
    208,
    194,
    19,
    128,
    8,
    198,
    19,
    128,
    32,
    200,
    19,
    128,
    180,
    204,
    19,
    128,
    44,
    207,
    19,
    128,
    128,
    209,
    19,
    128,
    84,
    211,
    19,
    128,
    244,
    212,
    19,
    128,
    208,
    214,
    19,
    128,
    248,
    216,
    19,
    128,
    36,
    219,
    19,
    128,
    140,
    221,
    19,
    128,
    72,
    223,
    19,
    128,
};

u8 gPacedWalkEffectParts[11] = { 1, 3, 5, 6, 9, 14, 15, 16, 17, 18, 19 };

/// Passes the task filed in the session's pointer slot 0xA, if any, to
/// `Task_CallExit` and empties the slot.
void func_actor_160600_80131E24(void)
{
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        Task_CallExit(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION));
        Game_SetPtrSlot(NULL, 0xA);
    }
}

#include "../../shared/paced_walk_frame.inc.c"

#include "../../shared/paced_walk_update.inc.c"

/// The actor's task body: dispatches on `Task::state` to the spawn routine
/// (state 0) or the per-frame body (state 1), handing each the task's
/// `Enemy` from `Task::spawnArg2`. The handler table is built on the stack.
void func_actor_160600_801321B4(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        pacedWalkSpawn,
        pacedWalkFrame,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

#include "../../shared/paced_walk_spawn.inc.c"

/// The actor's `Task::exitCallback`: hands the task's `Enemy`, parked in
/// `Task::spawnArg2`, back to `Gp_DestroyEnemy`.
void pacedWalkExit(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

#include "../../shared/walker_shadow.inc.c"

#include "../../shared/paced_walk_tick_anim.inc.c"

#include "../../shared/paced_walk_reset_anim.inc.c"

#include "../../shared/paced_walk_blend_anim.inc.c"

#include "../../shared/paced_walk_play_anim.inc.c"

#include "../../shared/paced_walk_show_pair.inc.c"

#include "../../shared/paced_walk_place.inc.c"

/// Script opcode: sets the work block's `effects`, which enables the
/// per-frame effect spawns, when the payload is exactly 1; any other payload
/// is ignored.
s32 func_actor_160600_8013268C(Task* task, s32 arg1, ActorCommand* args)
{
    Actor160600Work* work;
    u16              value;

    value = args->command;
    work  = (Actor160600Work*)task->work;
    if (value == 1) {
        work->effects = value;
    }
    return 0;
}

#include "../../shared/paced_walk_to.inc.c"
