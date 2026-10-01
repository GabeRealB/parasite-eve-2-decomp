#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/rand.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/ending.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/screen_negative.h"
#include "../../shared/paced_walk.h"
#include "../../shared/walker.h"
#include "../../shared/stride_walk.h"

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*        sets[15];
        GpCopyArg            copy;
        AnimationPlayRequest arguments[3];
    } data;
    s32 words[32];
} Actor460200AnimStorage5E30;
STATIC_ASSERT_SIZEOF(Actor460200AnimStorage5E30, 128);

extern Actor460200AnimStorage5E30 D_actor_460200_80135E30;

s32 func_actor_460200_80133C64(Task* task, s32 arg1, AnimationPlayRequest* args);

s32 func_actor_460200_80133CD0(Task* task, s32 arg1, s32 flags);

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, AnimationPlayRequest*);
        s32 (*call2)(Task*, s32, ActorCommand* request);
        s32 (*call3)(Task*, s32, ActorTransform*);
        s32 (*call4)(Task*, s32, s32);
    } handler;
} Actor460200MessageEntry;
STATIC_ASSERT_SIZEOF(Actor460200MessageEntry, 8);

extern Actor460200MessageEntry gPacedWalkMsgTable[6];
extern AnimationSet*           gPacedWalkAnimBank[16];

extern u8 gPacedWalkEffectParts[];

extern AnimationPlayRequest D_actor_460200_80135F14;
extern AnimationPlayRequest D_actor_460200_8013607C;
extern ActorTransform       D_actor_460200_80136234;
extern EvsCommand           D_actor_460200_80137AA0[];
extern EvsCommand           D_actor_460200_80137BA8[];
extern EvsCommand           D_actor_460200_80137CB0[];
extern EvsCommand           D_actor_460200_80137DA0[];
extern EvsCommand           D_actor_460200_80137F98[];
extern EvsCommand           D_actor_460200_80137FE0[];
extern EvsCommand           D_actor_460200_80138028[];
extern EvsCommand           D_actor_460200_80138070[];

extern RECT D_actor_460200_80135E0C;
extern RECT D_actor_460200_80135E14;

extern TaskDesc                gStrideWalkTasks[];
extern u8                      gStrideWalkAnimParams[];
extern Actor460200MessageEntry gStrideWalkMessages[6];
extern Actor460200MessageEntry D_actor_460200_801514FC[6];
extern s32                     D_actor_460200_80151538;

/// Scratchpad stack pointer the per-frame helpers carve temporary frames off.

static void func_actor_460200_80133254(Task* task);
static void func_actor_460200_801332E0(Task* task);
static void func_actor_460200_8013332C(Task* task);
static void func_actor_460200_801333A4(Task* task);
static void func_actor_460200_801338C0(Enemy* enemy, Task* task);
static void func_actor_460200_80133A04(Enemy* enemy, Task* task);
static void func_actor_460200_80133A88(Task* task);
static void func_actor_460200_80133AB0(Task* task);
static void func_actor_460200_80133B3C(Task* task);
static void func_actor_460200_80133B88(Task* task);
static void func_actor_460200_80133C00(Task* task);

extern TmdSource D_actor_460200_801406F8;
extern TmdSource D_actor_460200_80145E28;
void             func_actor_460200_801330C8(Task*);

s32 func_actor_460200_801334F0(Task*, s32, ActorTransform* placement);
s32 func_actor_460200_80133568(Task*, s32, ActorCommand* args);

extern TmdSource D_actor_460200_8014DB94;
s32              func_actor_460200_80133C64(Task*, s32, AnimationPlayRequest*);
s32              func_actor_460200_80133CD0(Task*, s32, s32);
s32              func_actor_460200_80133D4C(Task*, s32, ActorTransform* placement);
s32              func_actor_460200_80133DC4(void);
s32              func_actor_460200_80133DCC(Task*, s32, ActorTransform* target);
void             func_actor_460200_8013386C(Task*);

extern AnimationPlayRequest D_actor_460200_80135E1C;
extern AnimationPlayRequest D_actor_460200_80135F28;
extern AnimationPlayRequest D_actor_460200_80135F3C;
extern AnimationPlayRequest D_actor_460200_80135F50;
extern AnimationPlayRequest D_actor_460200_80136090;
extern AnimationPlayRequest D_actor_460200_801360A4;
extern AnimationPlayRequest D_actor_460200_801360B8;
extern AnimationPlayRequest D_actor_460200_801360CC;
s32                         func_actor_460200_80132C8C(Task*, s32, ActorCommand* args);
void                        func_actor_460200_801327B4(Task*);

extern AnimationPlayRequest D_actor_460200_80135EB0;
extern AnimationPlayRequest D_actor_460200_80135EC4;
extern AnimationPlayRequest D_actor_460200_80135ED8;
extern AnimationPlayRequest D_actor_460200_80135EEC;
extern AnimationPlayRequest D_actor_460200_80135F00;
extern AnimationPlayRequest D_actor_460200_80135F78;
extern AnimationPlayRequest D_actor_460200_80135F8C;
extern AnimationPlayRequest D_actor_460200_80135FA0;
extern AnimationPlayRequest D_actor_460200_80135FB4;
extern AnimationPlayRequest D_actor_460200_80135FC8;
extern AnimationPlayRequest D_actor_460200_80135FDC;
extern AnimationPlayRequest D_actor_460200_80135FF0;
extern AnimationPlayRequest D_actor_460200_80136004;
extern AnimationPlayRequest D_actor_460200_80136018;
extern AnimationPlayRequest D_actor_460200_8013602C;
extern AnimationPlayRequest D_actor_460200_80136040;
extern AnimationPlayRequest D_actor_460200_801360E0;
extern AnimationPlayRequest D_actor_460200_801360F4;
extern AnimationPlayRequest D_actor_460200_80136108;
extern AnimationPlayRequest D_actor_460200_8013611C;
extern AnimationPlayRequest D_actor_460200_80136130;
extern AnimationPlayRequest D_actor_460200_80136144;
extern AnimationPlayRequest D_actor_460200_80136158;
extern AnimationPlayRequest D_actor_460200_8013616C;
extern AnimationPlayRequest D_actor_460200_80136180;
extern AnimationPlayRequest D_actor_460200_80136194;
extern AnimationPlayRequest D_actor_460200_801361A8;
extern ActorTransform       D_actor_460200_801361BC;
extern ActorTransform       D_actor_460200_801361D4;
extern ActorTransform       D_actor_460200_801361EC;
extern ActorTransform       D_actor_460200_80136204;
extern ActorTransform       D_actor_460200_8013621C;
void                        func_actor_460200_80132090(Task*);
void                        func_actor_460200_801320E0(s32);
static void                 func_actor_460200_80132124(void);
void                        func_actor_460200_80132204(s8);

extern Actor460200AnimStorage5E30 D_actor_460200_80135E30;
void                              func_actor_460200_80131E24(Task*);

extern AnimationSet D_actor_460200_8014DE58;
extern AnimationSet D_actor_460200_8014E124;
extern AnimationSet D_actor_460200_8014E470;
extern AnimationSet D_actor_460200_8014E64C;
extern AnimationSet D_actor_460200_8014EB78;
extern AnimationSet D_actor_460200_8014ED60;
extern AnimationSet D_actor_460200_8014F120;
extern AnimationSet D_actor_460200_8014F2F8;
extern AnimationSet D_actor_460200_8014F6F4;
extern AnimationSet D_actor_460200_8014F8E0;
extern AnimationSet D_actor_460200_8014FC40;
extern AnimationSet D_actor_460200_80150844;
extern AnimationSet D_actor_460200_80150A84;
extern AnimationSet D_actor_460200_80150C7C;
extern AnimationSet D_actor_460200_80150F78;
extern AnimationSet D_actor_460200_80151298;
extern AnimationSet D_actor_460200_801514D4;

AnimationPackedPose D_actor_460200_80133E94[2] = {
#include "assets/actor_460200_animation_022B4_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80133EAC[26] = {
#include "assets/actor_460200_animation_022B4_bank4.inc"
};

AnimationRecord D_actor_460200_80133F14[102] = {
#include "assets/actor_460200_animation_022B4_records.inc"
};

u16 D_actor_460200_801340AC[20] = {
#include "assets/actor_460200_animation_022B4_indices.inc"
};

AnimationSet D_actor_460200_801340D4 = {
    D_actor_460200_80133F14,
    D_actor_460200_801340AC,
    { NULL, D_actor_460200_80133E94, NULL, NULL, D_actor_460200_80133EAC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_801340FC[2] = {
#include "assets/actor_460200_animation_02520_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80134114[25] = {
#include "assets/actor_460200_animation_02520_bank4.inc"
};

AnimationRecord D_actor_460200_80134178[104] = {
#include "assets/actor_460200_animation_02520_records.inc"
};

u16 D_actor_460200_80134318[20] = {
#include "assets/actor_460200_animation_02520_indices.inc"
};

AnimationSet D_actor_460200_80134340 = {
    D_actor_460200_80134178,
    D_actor_460200_80134318,
    { NULL, D_actor_460200_801340FC, NULL, NULL, D_actor_460200_80134114, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80134368[2] = {
#include "assets/actor_460200_animation_026C0_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80134380[21] = {
#include "assets/actor_460200_animation_026C0_bank4.inc"
};

AnimationRecord D_actor_460200_801343D4[57] = {
#include "assets/actor_460200_animation_026C0_records.inc"
};

u16 D_actor_460200_801344B8[20] = {
#include "assets/actor_460200_animation_026C0_indices.inc"
};

AnimationSet D_actor_460200_801344E0 = {
    D_actor_460200_801343D4,
    D_actor_460200_801344B8,
    { NULL, D_actor_460200_80134368, NULL, NULL, D_actor_460200_80134380, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80134508[2] = {
#include "assets/actor_460200_animation_029E0_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80134520[44] = {
#include "assets/actor_460200_animation_029E0_bank4.inc"
};

AnimationRecord D_actor_460200_801345D0[130] = {
#include "assets/actor_460200_animation_029E0_records.inc"
};

u16 D_actor_460200_801347D8[20] = {
#include "assets/actor_460200_animation_029E0_indices.inc"
};

AnimationSet D_actor_460200_80134800 = {
    D_actor_460200_801345D0,
    D_actor_460200_801347D8,
    { NULL, D_actor_460200_80134508, NULL, NULL, D_actor_460200_80134520, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80134828[2] = {
#include "assets/actor_460200_animation_02B8C_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80134840[24] = {
#include "assets/actor_460200_animation_02B8C_bank4.inc"
};

AnimationRecord D_actor_460200_801348A0[57] = {
#include "assets/actor_460200_animation_02B8C_records.inc"
};

u16 D_actor_460200_80134984[20] = {
#include "assets/actor_460200_animation_02B8C_indices.inc"
};

AnimationSet D_actor_460200_801349AC = {
    D_actor_460200_801348A0,
    D_actor_460200_80134984,
    { NULL, D_actor_460200_80134828, NULL, NULL, D_actor_460200_80134840, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_801349D4[2] = {
#include "assets/actor_460200_animation_02DE0_bank1.inc"
};

AnimationPackedRotation D_actor_460200_801349EC[25] = {
#include "assets/actor_460200_animation_02DE0_bank4.inc"
};

AnimationRecord D_actor_460200_80134A50[98] = {
#include "assets/actor_460200_animation_02DE0_records.inc"
};

u16 D_actor_460200_80134BD8[20] = {
#include "assets/actor_460200_animation_02DE0_indices.inc"
};

AnimationSet D_actor_460200_80134C00 = {
    D_actor_460200_80134A50,
    D_actor_460200_80134BD8,
    { NULL, D_actor_460200_801349D4, NULL, NULL, D_actor_460200_801349EC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80134C28[2] = {
#include "assets/actor_460200_animation_03020_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80134C40[26] = {
#include "assets/actor_460200_animation_03020_bank4.inc"
};

AnimationRecord D_actor_460200_80134CA8[92] = {
#include "assets/actor_460200_animation_03020_records.inc"
};

u16 D_actor_460200_80134E18[20] = {
#include "assets/actor_460200_animation_03020_indices.inc"
};

AnimationSet D_actor_460200_80134E40 = {
    D_actor_460200_80134CA8,
    D_actor_460200_80134E18,
    { NULL, D_actor_460200_80134C28, NULL, NULL, D_actor_460200_80134C40, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80134E68[2] = {
#include "assets/actor_460200_animation_031D0_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80134E80[25] = {
#include "assets/actor_460200_animation_031D0_bank4.inc"
};

AnimationRecord D_actor_460200_80134EE4[57] = {
#include "assets/actor_460200_animation_031D0_records.inc"
};

u16 D_actor_460200_80134FC8[20] = {
#include "assets/actor_460200_animation_031D0_indices.inc"
};

AnimationSet D_actor_460200_80134FF0 = {
    D_actor_460200_80134EE4,
    D_actor_460200_80134FC8,
    { NULL, D_actor_460200_80134E68, NULL, NULL, D_actor_460200_80134E80, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80135018[2] = {
#include "assets/actor_460200_animation_033B8_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80135030[25] = {
#include "assets/actor_460200_animation_033B8_bank4.inc"
};

AnimationRecord D_actor_460200_80135094[71] = {
#include "assets/actor_460200_animation_033B8_records.inc"
};

u16 D_actor_460200_801351B0[20] = {
#include "assets/actor_460200_animation_033B8_indices.inc"
};

AnimationSet D_actor_460200_801351D8 = {
    D_actor_460200_80135094,
    D_actor_460200_801351B0,
    { NULL, D_actor_460200_80135018, NULL, NULL, D_actor_460200_80135030, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80135200[2] = {
#include "assets/actor_460200_animation_03584_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80135218[22] = {
#include "assets/actor_460200_animation_03584_bank4.inc"
};

AnimationRecord D_actor_460200_80135270[67] = {
#include "assets/actor_460200_animation_03584_records.inc"
};

u16 D_actor_460200_8013537C[20] = {
#include "assets/actor_460200_animation_03584_indices.inc"
};

AnimationSet D_actor_460200_801353A4 = {
    D_actor_460200_80135270,
    D_actor_460200_8013537C,
    { NULL, D_actor_460200_80135200, NULL, NULL, D_actor_460200_80135218, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_801353CC[12] = {
#include "assets/actor_460200_animation_03BF4_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013545C[159] = {
#include "assets/actor_460200_animation_03BF4_bank4.inc"
};

AnimationRecord D_actor_460200_801356D8[197] = {
#include "assets/actor_460200_animation_03BF4_records.inc"
};

u16 D_actor_460200_801359EC[20] = {
#include "assets/actor_460200_animation_03BF4_indices.inc"
};

AnimationSet D_actor_460200_80135A14 = {
    D_actor_460200_801356D8,
    D_actor_460200_801359EC,
    { NULL, D_actor_460200_801353CC, NULL, NULL, D_actor_460200_8013545C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80135A3C[3] = {
#include "assets/actor_460200_animation_03FAC_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80135A60[81] = {
#include "assets/actor_460200_animation_03FAC_bank4.inc"
};

AnimationRecord D_actor_460200_80135BA4[128] = {
#include "assets/actor_460200_animation_03FAC_records.inc"
};

u16 D_actor_460200_80135DA4[20] = {
#include "assets/actor_460200_animation_03FAC_indices.inc"
};

AnimationSet D_actor_460200_80135DCC = {
    D_actor_460200_80135BA4,
    D_actor_460200_80135DA4,
    { NULL, D_actor_460200_80135A3C, NULL, NULL, D_actor_460200_80135A60, NULL, NULL, NULL },
};

TaskDesc D_actor_460200_80135DF4[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_actor_460200_80131E24, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

RECT D_actor_460200_80135E0C = { 0, 0, 320, 240 };

RECT D_actor_460200_80135E14 = { 0, 0, 16, 240 };

AnimationPlayRequest D_actor_460200_80135E1C = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

Actor460200AnimStorage5E30 D_actor_460200_80135E30 = { .data = { { &D_actor_460200_801340D4, &D_actor_460200_80134340, &D_actor_460200_801344E0, &D_actor_460200_80134800, &D_actor_460200_801349AC, &D_actor_460200_80134C00, &D_actor_460200_80134E40, &D_actor_460200_80134FF0, &D_actor_460200_801351D8, &D_actor_460200_801353A4, &D_actor_460200_80135A14, NULL, NULL, NULL, &D_actor_460200_80135DCC }, { { .words = D_actor_460200_80135E30.words }, 32 }, { { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_460200_80135EB0 = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135EC4 = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135ED8 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135EEC = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F00 = { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F14 = { { .index = 1 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F28 = { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F3C = { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F50 = { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F64 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F78 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F8C = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135FA0 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135FB4 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135FC8 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135FDC = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135FF0 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136004 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136018 = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_8013602C = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136040 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136054[2] = {
    { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_460200_8013607C = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136090 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_801360A4 = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_801360B8 = { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_801360CC = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_801360E0 = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_801360F4 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136108 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_8013611C = { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136130 = { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136144 = { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136158 = { { .index = 1 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_8013616C = { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136180 = { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136194 = { { .index = 1 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_801361A8 = { { .index = 1 }, 17, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_460200_801361BC = { { -910, 0, 5650, 0 }, { 0, -1592, 0, 0 } };

ActorTransform D_actor_460200_801361D4 = { { -1050, 0, 5418, 0 }, { 0, -1592, 0, 0 } };

ActorTransform D_actor_460200_801361EC = { { -2100, 0, 4600, 0 }, { 0, 398, 0, 0 } };

ActorTransform D_actor_460200_80136204 = { { -2100, 0, 4600, 0 }, { 0, 341, 0, 0 } };

ActorTransform D_actor_460200_8013621C = { { -2000, 0, 3800, 0 }, { 0, 1479, 0, 0 } };

ActorTransform D_actor_460200_80136234 = { { -2000, 0, 4700, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_460200_8013624C = { { 800, 0, 4490, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_460200_80136264 = { { -250, 0, 4490, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_460200_8013627C = { { -1000, 0, 4490, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_460200_80136294 = { { -1300, 0, 3300, 0 }, { 0, -568, 0, 0 } };

TaskDesc D_actor_460200_801362AC = { { { TASK_BODY_NONE, 32 } }, func_actor_460200_80132090, { .value = 0 } };

EvsCommand D_actor_460200_801362B8[233] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_460200_801320E0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1015 }, { .message = { .pointer = &D_actor_460200_80135E30.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 6 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_460200_801361BC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_801361EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_80136204 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136158 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136108 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x551C0007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013611C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_8013624C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135E30.data.arguments[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2013 }, { .message = { .pointer = &D_actor_460200_80136264 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135E30.data.arguments[2] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136130 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135EB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135E30.data.arguments[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2013 }, { .message = { .pointer = &D_actor_460200_8013627C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_801361EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136144 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x551C0008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_8013621C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_80136294 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FDC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135EEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135ED8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135EC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135EEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135ED8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_460200_80132124 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_80114A24 }, { .vibrationSegments = D_80114A34 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_801361EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_80136204 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013616C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FF0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80136004 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136180 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136194 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80136018 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801361A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_8013602C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_460200_801361D4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80136040 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_80136234 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_460200_801320E0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x551C0009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x551C000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_460200_80132204 }, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80137890[22] = {
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x551C0007 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x551C0008 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_460200_80132204 }, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_80136234 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_460200_801361D4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_460200_801320E0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x551C0009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x551C000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80137AA0[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1015 }, { .message = { .pointer = &D_actor_460200_80135E30.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80137BA8[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1015 }, { .message = { .pointer = &D_actor_460200_80135E30.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80137CB0[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1015 }, { .message = { .pointer = &D_actor_460200_80135E30.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135E1C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80137DA0[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1015 }, { .message = { .pointer = &D_actor_460200_80135E30.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135E1C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80137E90[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1015 }, { .message = { .pointer = &D_actor_460200_80135E30.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135F28 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135E1C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135F3C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135F14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_460200_80137F98[3] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 11 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_460200_80137E90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80137FE0[3] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_460200_80137E90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80138028[3] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_460200_80137E90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80138070[3] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 14 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_460200_80137E90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TmdBone D_actor_460200_801380B8[20] = {
#include "assets/actor_460200_model_0B95C_skeleton.inc"
};

u32 D_actor_460200_80138388[20] = {
#include "assets/actor_460200_model_0B95C_partVerts.inc"
};

SVECTOR D_actor_460200_801383D8[363] = {
#include "assets/actor_460200_model_0B95C_verts.inc"
};

SVECTOR D_actor_460200_80138F30[360] = {
#include "assets/actor_460200_model_0B95C_normals.inc"
};

u32 D_actor_460200_80139A70[3907] = {
#include "assets/actor_460200_model_0B95C_stream.inc"
};

TmdSource D_actor_460200_8013D77C = {
    0,
    21060,
    6448,
    20,
    D_actor_460200_80138388,
    D_actor_460200_801383D8,
    D_actor_460200_80138F30,
    D_actor_460200_801380B8,
    D_actor_460200_80139A70,
};

AnimationPackedPose D_actor_460200_8013D7A0[2] = {
#include "assets/actor_460200_animation_0BBB0_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013D7B8[28] = {
#include "assets/actor_460200_animation_0BBB0_bank4.inc"
};

AnimationRecord D_actor_460200_8013D828[96] = {
#include "assets/actor_460200_animation_0BBB0_records.inc"
};

u16 D_actor_460200_8013D9A8[20] = {
#include "assets/actor_460200_animation_0BBB0_indices.inc"
};

AnimationSet D_actor_460200_8013D9D0 = {
    D_actor_460200_8013D828,
    D_actor_460200_8013D9A8,
    { NULL, D_actor_460200_8013D7A0, NULL, NULL, D_actor_460200_8013D7B8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8013D9F8[2] = {
#include "assets/actor_460200_animation_0BE48_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013DA10[46] = {
#include "assets/actor_460200_animation_0BE48_bank4.inc"
};

AnimationRecord D_actor_460200_8013DAC8[94] = {
#include "assets/actor_460200_animation_0BE48_records.inc"
};

u16 D_actor_460200_8013DC40[20] = {
#include "assets/actor_460200_animation_0BE48_indices.inc"
};

AnimationSet D_actor_460200_8013DC68 = {
    D_actor_460200_8013DAC8,
    D_actor_460200_8013DC40,
    { NULL, D_actor_460200_8013D9F8, NULL, NULL, D_actor_460200_8013DA10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8013DC90[2] = {
#include "assets/actor_460200_animation_0C190_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013DCA8[58] = {
#include "assets/actor_460200_animation_0C190_bank4.inc"
};

AnimationRecord D_actor_460200_8013DD90[126] = {
#include "assets/actor_460200_animation_0C190_records.inc"
};

u16 D_actor_460200_8013DF88[20] = {
#include "assets/actor_460200_animation_0C190_indices.inc"
};

AnimationSet D_actor_460200_8013DFB0 = {
    D_actor_460200_8013DD90,
    D_actor_460200_8013DF88,
    { NULL, D_actor_460200_8013DC90, NULL, NULL, D_actor_460200_8013DCA8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8013DFD8[2] = {
#include "assets/actor_460200_animation_0C4C8_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013DFF0[65] = {
#include "assets/actor_460200_animation_0C4C8_bank4.inc"
};

AnimationRecord D_actor_460200_8013E0F4[115] = {
#include "assets/actor_460200_animation_0C4C8_records.inc"
};

u16 D_actor_460200_8013E2C0[20] = {
#include "assets/actor_460200_animation_0C4C8_indices.inc"
};

AnimationSet D_actor_460200_8013E2E8 = {
    D_actor_460200_8013E0F4,
    D_actor_460200_8013E2C0,
    { NULL, D_actor_460200_8013DFD8, NULL, NULL, D_actor_460200_8013DFF0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8013E310[2] = {
#include "assets/actor_460200_animation_0C6E0_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013E328[28] = {
#include "assets/actor_460200_animation_0C6E0_bank4.inc"
};

AnimationRecord D_actor_460200_8013E398[80] = {
#include "assets/actor_460200_animation_0C6E0_records.inc"
};

u16 D_actor_460200_8013E4D8[20] = {
#include "assets/actor_460200_animation_0C6E0_indices.inc"
};

AnimationSet D_actor_460200_8013E500 = {
    D_actor_460200_8013E398,
    D_actor_460200_8013E4D8,
    { NULL, D_actor_460200_8013E310, NULL, NULL, D_actor_460200_8013E328, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8013E528[2] = {
#include "assets/actor_460200_animation_0CB74_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013E540[107] = {
#include "assets/actor_460200_animation_0CB74_bank4.inc"
};

AnimationRecord D_actor_460200_8013E6EC[160] = {
#include "assets/actor_460200_animation_0CB74_records.inc"
};

u16 D_actor_460200_8013E96C[20] = {
#include "assets/actor_460200_animation_0CB74_indices.inc"
};

AnimationSet D_actor_460200_8013E994 = {
    D_actor_460200_8013E6EC,
    D_actor_460200_8013E96C,
    { NULL, D_actor_460200_8013E528, NULL, NULL, D_actor_460200_8013E540, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8013E9BC[2] = {
#include "assets/actor_460200_animation_0CDEC_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013E9D4[28] = {
#include "assets/actor_460200_animation_0CDEC_bank4.inc"
};

AnimationRecord D_actor_460200_8013EA44[104] = {
#include "assets/actor_460200_animation_0CDEC_records.inc"
};

u16 D_actor_460200_8013EBE4[20] = {
#include "assets/actor_460200_animation_0CDEC_indices.inc"
};

AnimationSet D_actor_460200_8013EC0C = {
    D_actor_460200_8013EA44,
    D_actor_460200_8013EBE4,
    { NULL, D_actor_460200_8013E9BC, NULL, NULL, D_actor_460200_8013E9D4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8013EC34[2] = {
#include "assets/actor_460200_animation_0D040_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013EC4C[45] = {
#include "assets/actor_460200_animation_0D040_bank4.inc"
};

AnimationRecord D_actor_460200_8013ED00[78] = {
#include "assets/actor_460200_animation_0D040_records.inc"
};

u16 D_actor_460200_8013EE38[20] = {
#include "assets/actor_460200_animation_0D040_indices.inc"
};

AnimationSet D_actor_460200_8013EE60 = {
    D_actor_460200_8013ED00,
    D_actor_460200_8013EE38,
    { NULL, D_actor_460200_8013EC34, NULL, NULL, D_actor_460200_8013EC4C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8013EE88[2] = {
#include "assets/actor_460200_animation_0D214_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013EEA0[23] = {
#include "assets/actor_460200_animation_0D214_bank4.inc"
};

AnimationRecord D_actor_460200_8013EEFC[68] = {
#include "assets/actor_460200_animation_0D214_records.inc"
};

u16 D_actor_460200_8013F00C[20] = {
#include "assets/actor_460200_animation_0D214_indices.inc"
};

AnimationSet D_actor_460200_8013F034 = {
    D_actor_460200_8013EEFC,
    D_actor_460200_8013F00C,
    { NULL, D_actor_460200_8013EE88, NULL, NULL, D_actor_460200_8013EEA0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8013F05C[2] = {
#include "assets/actor_460200_animation_0D3B4_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013F074[18] = {
#include "assets/actor_460200_animation_0D3B4_bank4.inc"
};

AnimationRecord D_actor_460200_8013F0BC[60] = {
#include "assets/actor_460200_animation_0D3B4_records.inc"
};

u16 D_actor_460200_8013F1AC[20] = {
#include "assets/actor_460200_animation_0D3B4_indices.inc"
};

AnimationSet D_actor_460200_8013F1D4 = {
    D_actor_460200_8013F0BC,
    D_actor_460200_8013F1AC,
    { NULL, D_actor_460200_8013F05C, NULL, NULL, D_actor_460200_8013F074, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8013F1FC[2] = {
#include "assets/actor_460200_animation_0D590_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013F214[29] = {
#include "assets/actor_460200_animation_0D590_bank4.inc"
};

AnimationRecord D_actor_460200_8013F288[64] = {
#include "assets/actor_460200_animation_0D590_records.inc"
};

u16 D_actor_460200_8013F388[20] = {
#include "assets/actor_460200_animation_0D590_indices.inc"
};

AnimationSet D_actor_460200_8013F3B0 = {
    D_actor_460200_8013F288,
    D_actor_460200_8013F388,
    { NULL, D_actor_460200_8013F1FC, NULL, NULL, D_actor_460200_8013F214, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8013F3D8[2] = {
#include "assets/actor_460200_animation_0D7B8_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013F3F0[24] = {
#include "assets/actor_460200_animation_0D7B8_bank4.inc"
};

AnimationRecord D_actor_460200_8013F450[88] = {
#include "assets/actor_460200_animation_0D7B8_records.inc"
};

u16 D_actor_460200_8013F5B0[20] = {
#include "assets/actor_460200_animation_0D7B8_indices.inc"
};

AnimationSet D_actor_460200_8013F5D8 = {
    D_actor_460200_8013F450,
    D_actor_460200_8013F5B0,
    { NULL, D_actor_460200_8013F3D8, NULL, NULL, D_actor_460200_8013F3F0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8013F600[2] = {
#include "assets/actor_460200_animation_0D9E4_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013F618[25] = {
#include "assets/actor_460200_animation_0D9E4_bank4.inc"
};

AnimationRecord D_actor_460200_8013F67C[88] = {
#include "assets/actor_460200_animation_0D9E4_records.inc"
};

u16 D_actor_460200_8013F7DC[20] = {
#include "assets/actor_460200_animation_0D9E4_indices.inc"
};

AnimationSet D_actor_460200_8013F804 = {
    D_actor_460200_8013F67C,
    D_actor_460200_8013F7DC,
    { NULL, D_actor_460200_8013F600, NULL, NULL, D_actor_460200_8013F618, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8013F82C[2] = {
#include "assets/actor_460200_animation_0DC4C_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013F844[31] = {
#include "assets/actor_460200_animation_0DC4C_bank4.inc"
};

AnimationRecord D_actor_460200_8013F8C0[97] = {
#include "assets/actor_460200_animation_0DC4C_records.inc"
};

u16 D_actor_460200_8013FA44[20] = {
#include "assets/actor_460200_animation_0DC4C_indices.inc"
};

AnimationSet D_actor_460200_8013FA6C = {
    D_actor_460200_8013F8C0,
    D_actor_460200_8013FA44,
    { NULL, D_actor_460200_8013F82C, NULL, NULL, D_actor_460200_8013F844, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8013FA94[2] = {
#include "assets/actor_460200_animation_0DE08_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8013FAAC[25] = {
#include "assets/actor_460200_animation_0DE08_bank4.inc"
};

AnimationRecord D_actor_460200_8013FB10[60] = {
#include "assets/actor_460200_animation_0DE08_records.inc"
};

u16 D_actor_460200_8013FC00[20] = {
#include "assets/actor_460200_animation_0DE08_indices.inc"
};

AnimationSet D_actor_460200_8013FC28 = {
    D_actor_460200_8013FB10,
    D_actor_460200_8013FC00,
    { NULL, D_actor_460200_8013FA94, NULL, NULL, D_actor_460200_8013FAAC, NULL, NULL, NULL },
};

Actor460200MessageEntry gPacedWalkMsgTable[6] = {
    { 2003, { .call1 = pacedWalkPlayAnim } },
    { 2005, { .call4 = pacedWalkShowPair } },
    { 2004, { .call3 = pacedWalkPlace } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call2 = func_actor_460200_80132C8C } },
    { 2013, { .call3 = pacedWalkTo } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

TaskDesc D_actor_460200_8013FC80 = { { { TASK_BODY_TMD, 96 } }, func_actor_460200_801327B4, { .model = &D_actor_460200_8013D77C } };

AnimationSet* gPacedWalkAnimBank[16] = {
    NULL,
    &D_actor_460200_8013D9D0,
    &D_actor_460200_8013DC68,
    &D_actor_460200_8013DFB0,
    &D_actor_460200_8013E2E8,
    &D_actor_460200_8013E500,
    &D_actor_460200_8013E994,
    &D_actor_460200_8013EC0C,
    &D_actor_460200_8013EE60,
    &D_actor_460200_8013F034,
    &D_actor_460200_8013F1D4,
    &D_actor_460200_8013F3B0,
    &D_actor_460200_8013F5D8,
    &D_actor_460200_8013F804,
    &D_actor_460200_8013FA6C,
    &D_actor_460200_8013FC28,
};

u8 gPacedWalkEffectParts[12] = {
    1,
    3,
    5,
    6,
    9,
    14,
    15,
    16,
    17,
    18,
    19,
    0,
};

TmdBone D_actor_460200_8013FCD8[1] = {
#include "assets/actor_460200_model_0E8D8_skeleton.inc"
};

u32 D_actor_460200_8013FCFC[1] = {
#include "assets/actor_460200_model_0E8D8_partVerts.inc"
};

SVECTOR D_actor_460200_8013FD00[58] = {
#include "assets/actor_460200_model_0E8D8_verts.inc"
};

SVECTOR D_actor_460200_8013FED0[58] = {
#include "assets/actor_460200_model_0E8D8_normals.inc"
};

u32 D_actor_460200_801400A0[406] = {
#include "assets/actor_460200_model_0E8D8_stream.inc"
};

TmdSource D_actor_460200_801406F8 = {
    0,
    2940,
    0,
    1,
    D_actor_460200_8013FCFC,
    D_actor_460200_8013FD00,
    D_actor_460200_8013FED0,
    D_actor_460200_8013FCD8,
    D_actor_460200_801400A0,
};

TmdBone D_actor_460200_8014071C[20] = {
#include "assets/actor_460200_model_14008_skeleton.inc"
};

u32 D_actor_460200_801409EC[20] = {
#include "assets/actor_460200_model_14008_partVerts.inc"
};

SVECTOR D_actor_460200_80140A3C[366] = {
#include "assets/actor_460200_model_14008_verts.inc"
};

SVECTOR D_actor_460200_801415AC[363] = {
#include "assets/actor_460200_model_14008_normals.inc"
};

u32 D_actor_460200_80142104[3913] = {
#include "assets/actor_460200_model_14008_stream.inc"
};

TmdSource D_actor_460200_80145E28 = {
    0,
    21132,
    6448,
    20,
    D_actor_460200_801409EC,
    D_actor_460200_80140A3C,
    D_actor_460200_801415AC,
    D_actor_460200_8014071C,
    D_actor_460200_80142104,
};

AnimationPackedPose D_actor_460200_80145E4C[2] = {
#include "assets/actor_460200_animation_14280_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80145E64[32] = {
#include "assets/actor_460200_animation_14280_bank4.inc"
};

AnimationRecord D_actor_460200_80145EE4[101] = {
#include "assets/actor_460200_animation_14280_records.inc"
};

u16 D_actor_460200_80146078[20] = {
#include "assets/actor_460200_animation_14280_indices.inc"
};

AnimationSet D_actor_460200_801460A0 = {
    D_actor_460200_80145EE4,
    D_actor_460200_80146078,
    { NULL, D_actor_460200_80145E4C, NULL, NULL, D_actor_460200_80145E64, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_801460C8[2] = {
#include "assets/actor_460200_animation_147F4_bank1.inc"
};

AnimationPackedRotation D_actor_460200_801460E0[135] = {
#include "assets/actor_460200_animation_147F4_bank4.inc"
};

AnimationRecord D_actor_460200_801462FC[188] = {
#include "assets/actor_460200_animation_147F4_records.inc"
};

u16 D_actor_460200_801465EC[20] = {
#include "assets/actor_460200_animation_147F4_indices.inc"
};

AnimationSet D_actor_460200_80146614 = {
    D_actor_460200_801462FC,
    D_actor_460200_801465EC,
    { NULL, D_actor_460200_801460C8, NULL, NULL, D_actor_460200_801460E0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8014663C[3] = {
#include "assets/actor_460200_animation_14A84_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80146660[27] = {
#include "assets/actor_460200_animation_14A84_bank4.inc"
};

AnimationRecord D_actor_460200_801466CC[108] = {
#include "assets/actor_460200_animation_14A84_records.inc"
};

u16 D_actor_460200_8014687C[20] = {
#include "assets/actor_460200_animation_14A84_indices.inc"
};

AnimationSet D_actor_460200_801468A4 = {
    D_actor_460200_801466CC,
    D_actor_460200_8014687C,
    { NULL, D_actor_460200_8014663C, NULL, NULL, D_actor_460200_80146660, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_801468CC[21] = {
#include "assets/actor_460200_animation_15218_bank1.inc"
};

AnimationPackedRotation D_actor_460200_801469C8[156] = {
#include "assets/actor_460200_animation_15218_bank4.inc"
};

AnimationRecord D_actor_460200_80146C38[246] = {
#include "assets/actor_460200_animation_15218_records.inc"
};

u16 D_actor_460200_80147010[20] = {
#include "assets/actor_460200_animation_15218_indices.inc"
};

AnimationSet D_actor_460200_80147038 = {
    D_actor_460200_80146C38,
    D_actor_460200_80147010,
    { NULL, D_actor_460200_801468CC, NULL, NULL, D_actor_460200_801469C8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80147060[5] = {
#include "assets/actor_460200_animation_154AC_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8014709C[36] = {
#include "assets/actor_460200_animation_154AC_bank4.inc"
};

AnimationRecord D_actor_460200_8014712C[94] = {
#include "assets/actor_460200_animation_154AC_records.inc"
};

u16 D_actor_460200_801472A4[20] = {
#include "assets/actor_460200_animation_154AC_indices.inc"
};

AnimationSet D_actor_460200_801472CC = {
    D_actor_460200_8014712C,
    D_actor_460200_801472A4,
    { NULL, D_actor_460200_80147060, NULL, NULL, D_actor_460200_8014709C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_801472F4[2] = {
#include "assets/actor_460200_animation_1579C_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8014730C[55] = {
#include "assets/actor_460200_animation_1579C_bank4.inc"
};

AnimationRecord D_actor_460200_801473E8[107] = {
#include "assets/actor_460200_animation_1579C_records.inc"
};

u16 D_actor_460200_80147594[20] = {
#include "assets/actor_460200_animation_1579C_indices.inc"
};

AnimationSet D_actor_460200_801475BC = {
    D_actor_460200_801473E8,
    D_actor_460200_80147594,
    { NULL, D_actor_460200_801472F4, NULL, NULL, D_actor_460200_8014730C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_801475E4[2] = {
#include "assets/actor_460200_animation_15950_bank1.inc"
};

AnimationPackedRotation D_actor_460200_801475FC[19] = {
#include "assets/actor_460200_animation_15950_bank4.inc"
};

AnimationRecord D_actor_460200_80147648[64] = {
#include "assets/actor_460200_animation_15950_records.inc"
};

u16 D_actor_460200_80147748[20] = {
#include "assets/actor_460200_animation_15950_indices.inc"
};

AnimationSet D_actor_460200_80147770 = {
    D_actor_460200_80147648,
    D_actor_460200_80147748,
    { NULL, D_actor_460200_801475E4, NULL, NULL, D_actor_460200_801475FC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80147798[2] = {
#include "assets/actor_460200_animation_15B04_bank1.inc"
};

AnimationPackedRotation D_actor_460200_801477B0[19] = {
#include "assets/actor_460200_animation_15B04_bank4.inc"
};

AnimationRecord D_actor_460200_801477FC[64] = {
#include "assets/actor_460200_animation_15B04_records.inc"
};

u16 D_actor_460200_801478FC[20] = {
#include "assets/actor_460200_animation_15B04_indices.inc"
};

AnimationSet D_actor_460200_80147924 = {
    D_actor_460200_801477FC,
    D_actor_460200_801478FC,
    { NULL, D_actor_460200_80147798, NULL, NULL, D_actor_460200_801477B0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8014794C[3] = {
#include "assets/actor_460200_animation_15DBC_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80147970[29] = {
#include "assets/actor_460200_animation_15DBC_bank4.inc"
};

AnimationRecord D_actor_460200_801479E4[116] = {
#include "assets/actor_460200_animation_15DBC_records.inc"
};

u16 D_actor_460200_80147BB4[20] = {
#include "assets/actor_460200_animation_15DBC_indices.inc"
};

AnimationSet D_actor_460200_80147BDC = {
    D_actor_460200_801479E4,
    D_actor_460200_80147BB4,
    { NULL, D_actor_460200_8014794C, NULL, NULL, D_actor_460200_80147970, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80147C04[2] = {
#include "assets/actor_460200_animation_160B0_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80147C1C[43] = {
#include "assets/actor_460200_animation_160B0_bank4.inc"
};

AnimationRecord D_actor_460200_80147CC8[120] = {
#include "assets/actor_460200_animation_160B0_records.inc"
};

u16 D_actor_460200_80147EA8[20] = {
#include "assets/actor_460200_animation_160B0_indices.inc"
};

AnimationSet D_actor_460200_80147ED0 = {
    D_actor_460200_80147CC8,
    D_actor_460200_80147EA8,
    { NULL, D_actor_460200_80147C04, NULL, NULL, D_actor_460200_80147C1C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80147EF8[2] = {
#include "assets/actor_460200_animation_162A0_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80147F10[30] = {
#include "assets/actor_460200_animation_162A0_bank4.inc"
};

AnimationRecord D_actor_460200_80147F88[68] = {
#include "assets/actor_460200_animation_162A0_records.inc"
};

u16 D_actor_460200_80148098[20] = {
#include "assets/actor_460200_animation_162A0_indices.inc"
};

AnimationSet D_actor_460200_801480C0 = {
    D_actor_460200_80147F88,
    D_actor_460200_80148098,
    { NULL, D_actor_460200_80147EF8, NULL, NULL, D_actor_460200_80147F10, NULL, NULL, NULL },
};

Actor460200MessageEntry gStrideWalkMessages[6] = {
    { 2003, { .call1 = strideWalkPlay } },
    { 2005, { .call4 = strideWalkSetVisibility } },
    { 2004, { .call3 = func_actor_460200_801334F0 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call2 = func_actor_460200_80133568 } },
    { 2013, { .call3 = strideWalkTo } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

TaskDesc gStrideWalkTasks[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_460200_801330C8, { .model = &D_actor_460200_80145E28 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, strideWalkSubModelTask, { .model = &D_actor_460200_801406F8 } },
};

u8 gStrideWalkAnimParams[48] = {
    0,
    0,
    0,
    0,
    160,
    96,
    20,
    128,
    164,
    104,
    20,
    128,
    20,
    102,
    20,
    128,
    56,
    112,
    20,
    128,
    188,
    117,
    20,
    128,
    112,
    119,
    20,
    128,
    36,
    121,
    20,
    128,
    220,
    123,
    20,
    128,
    208,
    126,
    20,
    128,
    192,
    128,
    20,
    128,
    204,
    114,
    20,
    128,
};

TmdBone D_actor_460200_80148160[20] = {
#include "assets/actor_460200_model_1BD74_skeleton.inc"
};

u32 D_actor_460200_80148430[20] = {
#include "assets/actor_460200_model_1BD74_partVerts.inc"
};

SVECTOR D_actor_460200_80148480[373] = {
#include "assets/actor_460200_model_1BD74_verts.inc"
};

SVECTOR D_actor_460200_80149028[398] = {
#include "assets/actor_460200_model_1BD74_normals.inc"
};

u32 D_actor_460200_80149C98[4031] = {
#include "assets/actor_460200_model_1BD74_stream.inc"
};

TmdSource D_actor_460200_8014DB94 = {
    0,
    22164,
    6060,
    20,
    D_actor_460200_80148430,
    D_actor_460200_80148480,
    D_actor_460200_80149028,
    D_actor_460200_80148160,
    D_actor_460200_80149C98,
};

AnimationPackedPose D_actor_460200_8014DBB8[5] = {
#include "assets/actor_460200_animation_1C038_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8014DBF4[56] = {
#include "assets/actor_460200_animation_1C038_bank4.inc"
};

AnimationRecord D_actor_460200_8014DCD4[87] = {
#include "assets/actor_460200_animation_1C038_records.inc"
};

u16 D_actor_460200_8014DE30[20] = {
#include "assets/actor_460200_animation_1C038_indices.inc"
};

AnimationSet D_actor_460200_8014DE58 = {
    D_actor_460200_8014DCD4,
    D_actor_460200_8014DE30,
    { NULL, D_actor_460200_8014DBB8, NULL, NULL, D_actor_460200_8014DBF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8014DE80[3] = {
#include "assets/actor_460200_animation_1C304_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8014DEA4[33] = {
#include "assets/actor_460200_animation_1C304_bank4.inc"
};

AnimationRecord D_actor_460200_8014DF28[117] = {
#include "assets/actor_460200_animation_1C304_records.inc"
};

u16 D_actor_460200_8014E0FC[20] = {
#include "assets/actor_460200_animation_1C304_indices.inc"
};

AnimationSet D_actor_460200_8014E124 = {
    D_actor_460200_8014DF28,
    D_actor_460200_8014E0FC,
    { NULL, D_actor_460200_8014DE80, NULL, NULL, D_actor_460200_8014DEA4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8014E14C[4] = {
#include "assets/actor_460200_animation_1C650_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8014E17C[41] = {
#include "assets/actor_460200_animation_1C650_bank4.inc"
};

AnimationRecord D_actor_460200_8014E220[138] = {
#include "assets/actor_460200_animation_1C650_records.inc"
};

u16 D_actor_460200_8014E448[20] = {
#include "assets/actor_460200_animation_1C650_indices.inc"
};

AnimationSet D_actor_460200_8014E470 = {
    D_actor_460200_8014E220,
    D_actor_460200_8014E448,
    { NULL, D_actor_460200_8014E14C, NULL, NULL, D_actor_460200_8014E17C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8014E498[3] = {
#include "assets/actor_460200_animation_1C82C_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8014E4BC[30] = {
#include "assets/actor_460200_animation_1C82C_bank4.inc"
};

AnimationRecord D_actor_460200_8014E534[60] = {
#include "assets/actor_460200_animation_1C82C_records.inc"
};

u16 D_actor_460200_8014E624[20] = {
#include "assets/actor_460200_animation_1C82C_indices.inc"
};

AnimationSet D_actor_460200_8014E64C = {
    D_actor_460200_8014E534,
    D_actor_460200_8014E624,
    { NULL, D_actor_460200_8014E498, NULL, NULL, D_actor_460200_8014E4BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8014E674[4] = {
#include "assets/actor_460200_animation_1CD58_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8014E6A4[79] = {
#include "assets/actor_460200_animation_1CD58_bank4.inc"
};

AnimationRecord D_actor_460200_8014E7E0[220] = {
#include "assets/actor_460200_animation_1CD58_records.inc"
};

u16 D_actor_460200_8014EB50[20] = {
#include "assets/actor_460200_animation_1CD58_indices.inc"
};

AnimationSet D_actor_460200_8014EB78 = {
    D_actor_460200_8014E7E0,
    D_actor_460200_8014EB50,
    { NULL, D_actor_460200_8014E674, NULL, NULL, D_actor_460200_8014E6A4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8014EBA0[3] = {
#include "assets/actor_460200_animation_1CF40_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8014EBC4[33] = {
#include "assets/actor_460200_animation_1CF40_bank4.inc"
};

AnimationRecord D_actor_460200_8014EC48[60] = {
#include "assets/actor_460200_animation_1CF40_records.inc"
};

u16 D_actor_460200_8014ED38[20] = {
#include "assets/actor_460200_animation_1CF40_indices.inc"
};

AnimationSet D_actor_460200_8014ED60 = {
    D_actor_460200_8014EC48,
    D_actor_460200_8014ED38,
    { NULL, D_actor_460200_8014EBA0, NULL, NULL, D_actor_460200_8014EBC4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8014ED88[5] = {
#include "assets/actor_460200_animation_1D300_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8014EDC4[63] = {
#include "assets/actor_460200_animation_1D300_bank4.inc"
};

AnimationRecord D_actor_460200_8014EEC0[142] = {
#include "assets/actor_460200_animation_1D300_records.inc"
};

u16 D_actor_460200_8014F0F8[20] = {
#include "assets/actor_460200_animation_1D300_indices.inc"
};

AnimationSet D_actor_460200_8014F120 = {
    D_actor_460200_8014EEC0,
    D_actor_460200_8014F0F8,
    { NULL, D_actor_460200_8014ED88, NULL, NULL, D_actor_460200_8014EDC4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8014F148[2] = {
#include "assets/actor_460200_animation_1D4D8_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8014F160[23] = {
#include "assets/actor_460200_animation_1D4D8_bank4.inc"
};

AnimationRecord D_actor_460200_8014F1BC[69] = {
#include "assets/actor_460200_animation_1D4D8_records.inc"
};

u16 D_actor_460200_8014F2D0[20] = {
#include "assets/actor_460200_animation_1D4D8_indices.inc"
};

AnimationSet D_actor_460200_8014F2F8 = {
    D_actor_460200_8014F1BC,
    D_actor_460200_8014F2D0,
    { NULL, D_actor_460200_8014F148, NULL, NULL, D_actor_460200_8014F160, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8014F320[3] = {
#include "assets/actor_460200_animation_1D8D4_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8014F344[72] = {
#include "assets/actor_460200_animation_1D8D4_bank4.inc"
};

AnimationRecord D_actor_460200_8014F464[154] = {
#include "assets/actor_460200_animation_1D8D4_records.inc"
};

u16 D_actor_460200_8014F6CC[20] = {
#include "assets/actor_460200_animation_1D8D4_indices.inc"
};

AnimationSet D_actor_460200_8014F6F4 = {
    D_actor_460200_8014F464,
    D_actor_460200_8014F6CC,
    { NULL, D_actor_460200_8014F320, NULL, NULL, D_actor_460200_8014F344, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8014F71C[3] = {
#include "assets/actor_460200_animation_1DAC0_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8014F740[34] = {
#include "assets/actor_460200_animation_1DAC0_bank4.inc"
};

AnimationRecord D_actor_460200_8014F7C8[60] = {
#include "assets/actor_460200_animation_1DAC0_records.inc"
};

u16 D_actor_460200_8014F8B8[20] = {
#include "assets/actor_460200_animation_1DAC0_indices.inc"
};

AnimationSet D_actor_460200_8014F8E0 = {
    D_actor_460200_8014F7C8,
    D_actor_460200_8014F8B8,
    { NULL, D_actor_460200_8014F71C, NULL, NULL, D_actor_460200_8014F740, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8014F908[4] = {
#include "assets/actor_460200_animation_1DE20_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8014F938[49] = {
#include "assets/actor_460200_animation_1DE20_bank4.inc"
};

AnimationRecord D_actor_460200_8014F9FC[135] = {
#include "assets/actor_460200_animation_1DE20_records.inc"
};

u16 D_actor_460200_8014FC18[20] = {
#include "assets/actor_460200_animation_1DE20_indices.inc"
};

AnimationSet D_actor_460200_8014FC40 = {
    D_actor_460200_8014F9FC,
    D_actor_460200_8014FC18,
    { NULL, D_actor_460200_8014F908, NULL, NULL, D_actor_460200_8014F938, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8014FC68[29] = {
#include "assets/actor_460200_animation_1EA24_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8014FDC4[279] = {
#include "assets/actor_460200_animation_1EA24_bank4.inc"
};

AnimationRecord D_actor_460200_80150220[383] = {
#include "assets/actor_460200_animation_1EA24_records.inc"
};

u16 D_actor_460200_8015081C[20] = {
#include "assets/actor_460200_animation_1EA24_indices.inc"
};

AnimationSet D_actor_460200_80150844 = {
    D_actor_460200_80150220,
    D_actor_460200_8015081C,
    { NULL, D_actor_460200_8014FC68, NULL, NULL, D_actor_460200_8014FDC4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_8015086C[4] = {
#include "assets/actor_460200_animation_1EC64_bank1.inc"
};

AnimationPackedRotation D_actor_460200_8015089C[42] = {
#include "assets/actor_460200_animation_1EC64_bank4.inc"
};

AnimationRecord D_actor_460200_80150944[70] = {
#include "assets/actor_460200_animation_1EC64_records.inc"
};

u16 D_actor_460200_80150A5C[20] = {
#include "assets/actor_460200_animation_1EC64_indices.inc"
};

AnimationSet D_actor_460200_80150A84 = {
    D_actor_460200_80150944,
    D_actor_460200_80150A5C,
    { NULL, D_actor_460200_8015086C, NULL, NULL, D_actor_460200_8015089C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80150AAC[2] = {
#include "assets/actor_460200_animation_1EE5C_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80150AC4[26] = {
#include "assets/actor_460200_animation_1EE5C_bank4.inc"
};

AnimationRecord D_actor_460200_80150B2C[74] = {
#include "assets/actor_460200_animation_1EE5C_records.inc"
};

u16 D_actor_460200_80150C54[20] = {
#include "assets/actor_460200_animation_1EE5C_indices.inc"
};

AnimationSet D_actor_460200_80150C7C = {
    D_actor_460200_80150B2C,
    D_actor_460200_80150C54,
    { NULL, D_actor_460200_80150AAC, NULL, NULL, D_actor_460200_80150AC4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80150CA4[3] = {
#include "assets/actor_460200_animation_1F158_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80150CC8[39] = {
#include "assets/actor_460200_animation_1F158_bank4.inc"
};

AnimationRecord D_actor_460200_80150D64[123] = {
#include "assets/actor_460200_animation_1F158_records.inc"
};

u16 D_actor_460200_80150F50[20] = {
#include "assets/actor_460200_animation_1F158_indices.inc"
};

AnimationSet D_actor_460200_80150F78 = {
    D_actor_460200_80150D64,
    D_actor_460200_80150F50,
    { NULL, D_actor_460200_80150CA4, NULL, NULL, D_actor_460200_80150CC8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_80150FA0[3] = {
#include "assets/actor_460200_animation_1F478_bank1.inc"
};

AnimationPackedRotation D_actor_460200_80150FC4[52] = {
#include "assets/actor_460200_animation_1F478_bank4.inc"
};

AnimationRecord D_actor_460200_80151094[119] = {
#include "assets/actor_460200_animation_1F478_records.inc"
};

u16 D_actor_460200_80151270[20] = {
#include "assets/actor_460200_animation_1F478_indices.inc"
};

AnimationSet D_actor_460200_80151298 = {
    D_actor_460200_80151094,
    D_actor_460200_80151270,
    { NULL, D_actor_460200_80150FA0, NULL, NULL, D_actor_460200_80150FC4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_460200_801512C0[2] = {
#include "assets/actor_460200_animation_1F6B4_bank1.inc"
};

AnimationPackedRotation D_actor_460200_801512D8[34] = {
#include "assets/actor_460200_animation_1F6B4_bank4.inc"
};

AnimationRecord D_actor_460200_80151360[83] = {
#include "assets/actor_460200_animation_1F6B4_records.inc"
};

u16 D_actor_460200_801514AC[20] = {
#include "assets/actor_460200_animation_1F6B4_indices.inc"
};

AnimationSet D_actor_460200_801514D4 = {
    D_actor_460200_80151360,
    D_actor_460200_801514AC,
    { NULL, D_actor_460200_801512C0, NULL, NULL, D_actor_460200_801512D8, NULL, NULL, NULL },
};

Actor460200MessageEntry D_actor_460200_801514FC[6] = {
    { 2003, { .call1 = func_actor_460200_80133C64 } },
    { 2005, { .call4 = func_actor_460200_80133CD0 } },
    { 2004, { .call3 = func_actor_460200_80133D4C } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = func_actor_460200_80133DC4 } },
    { 2013, { .call3 = func_actor_460200_80133DCC } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

TaskDesc D_actor_460200_8015152C = { { { TASK_BODY_TMD, 96 } }, func_actor_460200_8013386C, { .model = &D_actor_460200_8014DB94 } };

s32 D_actor_460200_80151538 = 0;

AnimationSet* D_actor_460200_8015153C[17] = {
    &D_actor_460200_8014DE58,
    &D_actor_460200_8014E124,
    &D_actor_460200_8014E470,
    &D_actor_460200_8014E64C,
    &D_actor_460200_8014EB78,
    &D_actor_460200_8014ED60,
    &D_actor_460200_8014F120,
    &D_actor_460200_8014F2F8,
    &D_actor_460200_8014F6F4,
    &D_actor_460200_8014F8E0,
    &D_actor_460200_8014FC40,
    &D_actor_460200_80150844,
    &D_actor_460200_80150A84,
    &D_actor_460200_80150C7C,
    &D_actor_460200_80150F78,
    &D_actor_460200_80151298,
    &D_actor_460200_801514D4,
};

static void func_actor_460200_80132210(void);
static void func_actor_460200_801322B8(void);
static void func_actor_460200_80132390(void);
static void func_actor_460200_801336B4(Task* task);

void func_actor_460200_80131E24(Task* task)
{
    OverlayCaptureArgs* args;
    s32                 i;
    u_long*             strip;

    args = task->spawnArg2.pointer;
    if (D_801156F9 == 0) {
        switch (task->state) {
            case 0:
                args->done          = 0;
                task->killCountdown = args->duration;
                if (gDisplayState.drawBuffer != 0) {
                    D_actor_460200_80135E14.y = 0;
                } else {
                    D_actor_460200_80135E14.y = 0x110;
                }
                if (gDisplayState.debugMode < 0) {
                    StoreImage(&D_actor_460200_80135E0C, Fs_ImgBuffers->words);
                } else {
                    strip = Fs_ImgBuffers->words;
                    for (i = 0; i < 20; i++) {
                        D_actor_460200_80135E14.x = i * 16;
                        StoreImage(&D_actor_460200_80135E14, strip);
                        strip += 1920;
                    }
                }
                gDisplayState.skipDraw = 1;
                goto advance;
            case 1:
                DrawSync(0);
                screenNegativeFilter();
            advance:
                task->state++;
                break;
            case 2:
                if (--task->killCountdown <= 0) {
                    args->done = 1;
                }
                if (args->done != 0) {
                    taskKill(task);
                    gDisplayState.skipDraw = 0;
                }
                break;
        }
    }
}

#include "../../shared/screen_negative_filter.inc.c"

void func_actor_460200_80132090(Task* arg0)
{
    s32 var_v0;

    var_v0 = arg0->spawnArg1.value;
    if (var_v0 < 0) {
        Stage_SetEndingFlag();
        taskKill(arg0);
        var_v0 = arg0->spawnArg1.value;
    }
    var_v0                = var_v0 - 1;
    arg0->spawnArg1.value = var_v0;
}

void func_actor_460200_801320E0(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(arg0);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

/// The same filter under the name the cutscene script's command 13 calls.
#define screenNegativeFilter func_actor_460200_80132124
#include "../../shared/screen_negative_filter.inc.c"
#undef screenNegativeFilter

void func_actor_460200_80132204(s8 arg0)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = arg0;
}

static void func_actor_460200_80132210(void)
{
    Task* slot;

    slot = Gp_LookupSlot4(0);
    if (slot != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(slot, 0x7D4, &D_actor_460200_80136234, 0);
        TASK_MESSAGE_DISPATCH_POINTER(slot, 0x7D3, &D_actor_460200_8013607C, 0);
    }
    if (Gp_LookupSlot4(1) != 0) {
        Gp_MsgSlot4Chain(1, 2);
    }
    slot = Gp_LookupSlot4(2);
    if (slot != NULL) {
        Gp_MsgSlot4Chain(2, 1);
        TASK_MESSAGE_DISPATCH_POINTER(slot, 0x7D3, &D_actor_460200_80135F14, 0);
    }
}

static void func_actor_460200_801322B8(void)
{
    switch (GameFlag_GetNibble(0x114)) {
        case 0:
            func_800E8614(D_actor_460200_80137AA0, 0);
            GameFlag_SetNibble(0x114, 1);
            break;
        case 1:
            func_800E8614(D_actor_460200_80137BA8, 0);
            GameFlag_SetNibble(0x114, 2);
            break;
        case 2:
            func_800E8614(D_actor_460200_80137CB0, 0);
            GameFlag_SetNibble(0x114, 3);
            break;
        case 3:
            func_800E8614(D_actor_460200_80137DA0, 0);
            break;
    }
}

static void func_actor_460200_80132390(void)
{
    switch (GameFlag_GetNibble(0x115)) {
        case 0:
            func_800E8614(D_actor_460200_80137F98, 0);
            GameFlag_SetNibble(0x115, 1);
            break;
        case 1:
            func_800E8614(D_actor_460200_80137FE0, 0);
            GameFlag_SetNibble(0x115, 2);
            break;
        case 2:
            func_800E8614(D_actor_460200_80138028, 0);
            GameFlag_SetNibble(0x115, 3);
            break;
        case 3:
            func_800E8614(D_actor_460200_80138070, 0);
            break;
    }
}

#include "../../shared/paced_walk_frame.inc.c"

#include "../../shared/paced_walk_update.inc.c"

void func_actor_460200_801327B4(Task* task)
{
    GpEnemyTaskFunc fns[2] = { pacedWalkSpawn, pacedWalkFrame };

    fns[task->state](task->spawnArg2.pointer, task);
}

#include "../../shared/paced_walk_spawn.inc.c"

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

/// Script opcode: raise the work block's `effects`, which lets the per-frame
/// state spawn its effect, when the payload is exactly 1. Any other payload is
/// ignored and leaves the flag as it was.
s32 func_actor_460200_80132C8C(Task* task, s32 arg1, ActorCommand* args)
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

#include "../../shared/stride_walk_spawn.inc.c"

#define pacedWalkBlendAnim func_actor_460200_801333A4
#define pacedWalkResetAnim func_actor_460200_8013332C
#define pacedWalkTickAnim  func_actor_460200_801332E0
#include "../../shared/stride_walk_update.inc.c"
#undef pacedWalkBlendAnim
#undef pacedWalkResetAnim
#undef pacedWalkTickAnim

void func_actor_460200_801330C8(Task* task)
{
    GpEnemyTaskFunc fns[2] = { strideWalkSpawn, strideWalkFrame };

    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerDrawShadow func_actor_460200_80133254
#include "../../shared/stride_walk_frame.inc.c"
#undef walkerDrawShadow

void strideWalkExit(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// A further copy of the shadow, under this file's own name.
#define walkerDrawShadow func_actor_460200_80133254
#include "../../shared/walker_shadow.inc.c"
#undef walkerDrawShadow

/// The second walker's copy.
#define pacedWalkTickAnim func_actor_460200_801332E0
#include "../../shared/paced_walk_tick_anim.inc.c"
#undef pacedWalkTickAnim

/// The second walker's copy.
#define pacedWalkResetAnim func_actor_460200_8013332C
#include "../../shared/paced_walk_reset_anim.inc.c"
#undef pacedWalkResetAnim

/// The second walker's copy.
#define pacedWalkBlendAnim func_actor_460200_801333A4
#include "../../shared/paced_walk_blend_anim.inc.c"
#undef pacedWalkBlendAnim

#include "../../shared/stride_walk_play.inc.c"

#include "../../shared/stride_walk_visibility.inc.c"

/// The second walker's copy.
#define pacedWalkPlace func_actor_460200_801334F0
#include "../../shared/paced_walk_place.inc.c"
#undef pacedWalkPlace

/// Script opcode: set the work block's `turnUp`, which selects whether the
/// per-frame state blends the model toward the `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` task or away
/// from it, to the payload.
s32 func_actor_460200_80133568(Task* task, s32 arg1, ActorCommand* args)
{
    ((Actor161500Work*)task->work)->turnUp = args->command;
    return 0;
}

#include "../../shared/stride_walk_to.inc.c"

#include "../../shared/stride_walk_sub_model.inc.c"

/// The third walker's copy.
#define pacedWalkUpdate    func_actor_460200_801336B4
#define pacedWalkTickAnim  func_actor_460200_80133B3C
#define pacedWalkResetAnim func_actor_460200_80133B88
#define pacedWalkBlendAnim func_actor_460200_80133C00
#include "../../shared/paced_walk_update.inc.c"
#undef pacedWalkUpdate
#undef pacedWalkTickAnim
#undef pacedWalkResetAnim
#undef pacedWalkBlendAnim

void func_actor_460200_8013386C(Task* task)
{
    GpEnemyTaskFunc fns[2] = { func_actor_460200_801338C0, func_actor_460200_80133A04 };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Spawn routine of the actor whose `func_actor_460200_80133A88` exit path
/// hands it back to `Gp_DestroyEnemy`: it allocates the 0x4F8 work block (the
/// matrix pair its sub-model reads through `TmdObject::lightMtx`/`colorMtx`
/// plus the animation state below), parks the enemy in `Actor160600Work::enemy`
/// and runs the step body `func_actor_460200_801336B4` once in state 2.
static void func_actor_460200_801338C0(Enemy* enemy, Task* task)
{
    Actor160600Work* work;
    void*            workMem;
    TmdObject*       obj;
    GfxCoord*        coord;
    MATRIX*          mtx;
    VECTOR           vec;

    obj     = task->extra.tmd;
    coord   = obj->coords;
    workMem = memCalloc(0x4F8, 0);
    work    = (Actor160600Work*)workMem;
    if ((task->work = work) == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_460200_80133A88;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->flags                       = 0;
    obj->otOffset                    = 1;
    work->enemy                      = enemy;
    work->st.animId                  = 2;
    mtx                              = &work->light;
    obj->lightMtx                    = mtx;
    obj->colorMtx                    = mtx + 1;
    vec.vx                           = coord->workm.t[0];
    vec.vy                           = coord->workm.t[1] - 0x320;
    vec.vz                           = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, &D_actor_460200_80151538, obj, work->rig.poses, work->rig.slots);
    work->st.state = 2;
    task->msgTable = D_actor_460200_801514FC;
    func_actor_460200_801336B4(task);
    task->state += 1;
}

#define walkerFrame      func_actor_460200_80133A04
#define walkerUpdate     func_actor_460200_801336B4
#define walkerDrawShadow func_actor_460200_80133AB0
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

static void func_actor_460200_80133A88(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// A further copy of the shadow, under this file's own name.
#define walkerDrawShadow func_actor_460200_80133AB0
#include "../../shared/walker_shadow.inc.c"
#undef walkerDrawShadow

/// The third walker's copy.
#define pacedWalkTickAnim func_actor_460200_80133B3C
#include "../../shared/paced_walk_tick_anim.inc.c"
#undef pacedWalkTickAnim

/// The third walker's copy.
#define pacedWalkResetAnim func_actor_460200_80133B88
#include "../../shared/paced_walk_reset_anim.inc.c"
#undef pacedWalkResetAnim

/// The third walker's copy.
#define pacedWalkBlendAnim func_actor_460200_80133C00
#include "../../shared/paced_walk_blend_anim.inc.c"
#undef pacedWalkBlendAnim

s32 func_actor_460200_80133C64(Task* task, s32 arg1, AnimationPlayRequest* args)
{
    Actor160600Work* work;

    work = (Actor160600Work*)task->work;
    if (args->animationId < 0x12) {
        work->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state = 1;
            work->animArg  = args->blendFrames;
        } else {
            work->st.state = 2;
        }
        work->st.field_6 = 0;
        func_actor_460200_801336B4(task);
        return 0;
    }
    return -1;
}

/// A further copy, under this file's own name.
#define pacedWalkShowPair func_actor_460200_80133CD0
#include "../../shared/paced_walk_show_pair.inc.c"
#undef pacedWalkShowPair

/// The third walker's copy.
#define pacedWalkPlace func_actor_460200_80133D4C
#include "../../shared/paced_walk_place.inc.c"
#undef pacedWalkPlace

s32 func_actor_460200_80133DC4(void)
{
    return 0;
}

/// The third walker's copy.
#define pacedWalkTo func_actor_460200_80133DCC
#include "../../shared/paced_walk_to.inc.c"
#undef pacedWalkTo
