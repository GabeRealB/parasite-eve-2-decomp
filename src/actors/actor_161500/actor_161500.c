#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/shelter_1f_heliport.h"

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        AnimationSet*        sets[6];
        AnimationPlayRequest arguments[6];
    } data;
    s32 words[36];
} Actor161500AnimStorage3F90;
STATIC_ASSERT_SIZEOF(Actor161500AnimStorage3F90, 144);

extern Actor161500AnimStorage3F90 D_actor_161500_80133F90;

// The engine copies words across the exported animation bank and its
// following argument records. Both views cover the complete backing object.
typedef union {
    struct {
        AnimationSet*        sets[7];
        AnimationPlayRequest arguments[1];
    } data;
    s32 words[12];
} Actor161500AnimCopy6D60;
STATIC_ASSERT_SIZEOF(Actor161500AnimCopy6D60, 48);

extern Actor161500AnimCopy6D60 D_actor_161500_80136D60;

extern TaskDesc      D_actor_161500_801401B0[];
extern AnimationSet* D_actor_161500_801401C8[12];
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*);
        s32 (*call1)(Task*, s32, ActorCommand* request);
        s32 (*call2)(Task*, s32, ActorTransform*);
        s32 (*call3)(Task*, s32, s32);
    } handler;
} Actor161500MessageEntry;
STATIC_ASSERT_SIZEOF(Actor161500MessageEntry, 8);

extern Actor161500MessageEntry D_actor_161500_80140180[6];

extern GpEvsCmd*      D_actor_161500_80134920[8];
extern GpEvsCmd*      D_actor_161500_80135288[8];
extern GpEvsCmd       D_actor_161500_801352A8[];
extern GpEvsCmd       D_actor_161500_801354B8[];
extern GpEvsCmd       D_actor_161500_80135668[];
extern GpEvsCmd       D_actor_161500_801357E8[];
extern GpEvsCmd       D_actor_161500_80135968[];
extern GpEvsCmd       D_actor_161500_80135AE8[];
extern GpEvsCmd       D_actor_161500_80135C68[];
extern GpEvsCmd       D_actor_161500_80136E88[];
extern GpEvsCmd       D_actor_161500_80137080[];
extern GpEvsCmd       D_actor_161500_80137650[];
extern ActorTransform D_actor_161500_801376E0;
extern GpEvsCmd       D_actor_161500_801376F8[];
extern GpEvsCmd       D_actor_161500_801378D8[];
extern GpEvsCmd       D_actor_161500_80137AB8[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

static void func_actor_161500_8013252C(Task* task);
static void func_actor_161500_8013273C(GpEnemy* enemy, Task* task);
static void func_actor_161500_8013284C(Task* task);
static void func_actor_161500_80132874(Task* task);
static void func_actor_161500_80132900(Task* task);
static void func_actor_161500_8013294C(Task* task);
static void func_actor_161500_801329C4(Task* task);

extern AnimationPlayRequest D_actor_161500_80133F7C;
extern AnimationPlayRequest D_actor_161500_80134020;
extern AnimationPlayRequest D_actor_161500_80134034;
extern ActorCommand         D_actor_161500_80133F74;
extern ActorCommand         D_actor_161500_80133F78;
extern GpCopyArg            D_actor_161500_80134048;
void                        func_actor_161500_80131F50(s32);
void                        func_actor_161500_801320B4(void);
void                        func_actor_161500_801320F0(s32);
void                        func_actor_161500_80132150(void);

void func_actor_161500_801321B4(Task*);

extern AnimationSet D_actor_161500_80136124;
extern AnimationSet D_actor_161500_80136338;
extern AnimationSet D_actor_161500_80136618;
extern AnimationSet D_actor_161500_801368A8;
extern AnimationSet D_actor_161500_80136A80;
extern AnimationSet D_actor_161500_80136CB4;

extern Actor161500AnimCopy6D60 D_actor_161500_80136D60;
extern ActorTransform          D_actor_161500_80136CE8;
extern ActorTransform          D_actor_161500_80136D00;
extern ActorTransform          D_actor_161500_80136D18;
extern ActorTransform          D_actor_161500_80136D30;
extern ActorTransform          D_actor_161500_80136D48;

void func_actor_161500_80131F50(s32);

extern AnimationPlayRequest D_actor_161500_80136D90;
extern AnimationPlayRequest D_actor_161500_80136DA4;
extern AnimationPlayRequest D_actor_161500_80136DB8;
extern AnimationPlayRequest D_actor_161500_80136E38;
extern AnimationPlayRequest D_actor_161500_80136E60;
extern AnimationPlayRequest D_actor_161500_80136E74;
extern GpCopyArg            D_actor_161500_80136E08;
s32                         func_actor_161500_80132A28(Task*, s32, AnimationPlayRequest*);
s32                         func_actor_161500_80132A94(Task*, s32, s32);
s32                         func_actor_161500_80132B10(Task*, s32, ActorTransform* placement);
s32                         func_actor_161500_80132B88(Task*, s32, ActorCommand* args);
s32                         func_actor_161500_80132BA0(Task*, s32, ActorTransform* target);
void                        func_actor_161500_80132210(void);
void                        func_actor_161500_80132294(u8);
void                        func_actor_161500_801326E8(Task*);
void                        func_actor_161500_80132C6C(Task*);

AnimationPackedPose D_actor_161500_80132CD4[3] = {
#include "assets/actor_161500_animation_010D0_bank1.inc"
};

AnimationPackedRotation D_actor_161500_80132CF8[36] = {
#include "assets/actor_161500_animation_010D0_bank4.inc"
};

AnimationRecord D_actor_161500_80132D88[80] = {
#include "assets/actor_161500_animation_010D0_records.inc"
};

u16 D_actor_161500_80132EC8[20] = {
#include "assets/actor_161500_animation_010D0_indices.inc"
};

AnimationSet D_actor_161500_80132EF0 = {
    D_actor_161500_80132D88,
    D_actor_161500_80132EC8,
    { NULL, D_actor_161500_80132CD4, NULL, NULL, D_actor_161500_80132CF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_80132F18[3] = {
#include "assets/actor_161500_animation_0129C_bank1.inc"
};

AnimationPackedRotation D_actor_161500_80132F3C[29] = {
#include "assets/actor_161500_animation_0129C_bank4.inc"
};

AnimationRecord D_actor_161500_80132FB0[57] = {
#include "assets/actor_161500_animation_0129C_records.inc"
};

u16 D_actor_161500_80133094[20] = {
#include "assets/actor_161500_animation_0129C_indices.inc"
};

AnimationSet D_actor_161500_801330BC = {
    D_actor_161500_80132FB0,
    D_actor_161500_80133094,
    { NULL, D_actor_161500_80132F18, NULL, NULL, D_actor_161500_80132F3C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_801330E4[5] = {
#include "assets/actor_161500_animation_016C4_bank1.inc"
};

AnimationPackedRotation D_actor_161500_80133120[90] = {
#include "assets/actor_161500_animation_016C4_bank4.inc"
};

AnimationRecord D_actor_161500_80133288[141] = {
#include "assets/actor_161500_animation_016C4_records.inc"
};

u16 D_actor_161500_801334BC[20] = {
#include "assets/actor_161500_animation_016C4_indices.inc"
};

AnimationSet D_actor_161500_801334E4 = {
    D_actor_161500_80133288,
    D_actor_161500_801334BC,
    { NULL, D_actor_161500_801330E4, NULL, NULL, D_actor_161500_80133120, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_8013350C[6] = {
#include "assets/actor_161500_animation_01A48_bank1.inc"
};

AnimationPackedRotation D_actor_161500_80133554[69] = {
#include "assets/actor_161500_animation_01A48_bank4.inc"
};

AnimationRecord D_actor_161500_80133668[118] = {
#include "assets/actor_161500_animation_01A48_records.inc"
};

u16 D_actor_161500_80133840[20] = {
#include "assets/actor_161500_animation_01A48_indices.inc"
};

AnimationSet D_actor_161500_80133868 = {
    D_actor_161500_80133668,
    D_actor_161500_80133840,
    { NULL, D_actor_161500_8013350C, NULL, NULL, D_actor_161500_80133554, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_80133890[7] = {
#include "assets/actor_161500_animation_01D74_bank1.inc"
};

AnimationPackedRotation D_actor_161500_801338E4[56] = {
#include "assets/actor_161500_animation_01D74_bank4.inc"
};

AnimationRecord D_actor_161500_801339C4[106] = {
#include "assets/actor_161500_animation_01D74_records.inc"
};

u16 D_actor_161500_80133B6C[20] = {
#include "assets/actor_161500_animation_01D74_indices.inc"
};

AnimationSet D_actor_161500_80133B94 = {
    D_actor_161500_801339C4,
    D_actor_161500_80133B6C,
    { NULL, D_actor_161500_80133890, NULL, NULL, D_actor_161500_801338E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_80133BBC[3] = {
#include "assets/actor_161500_animation_0212C_bank1.inc"
};

AnimationPackedRotation D_actor_161500_80133BE0[81] = {
#include "assets/actor_161500_animation_0212C_bank4.inc"
};

AnimationRecord D_actor_161500_80133D24[128] = {
#include "assets/actor_161500_animation_0212C_records.inc"
};

u16 D_actor_161500_80133F24[20] = {
#include "assets/actor_161500_animation_0212C_indices.inc"
};

AnimationSet D_actor_161500_80133F4C = {
    D_actor_161500_80133D24,
    D_actor_161500_80133F24,
    { NULL, D_actor_161500_80133BBC, NULL, NULL, D_actor_161500_80133BE0, NULL, NULL, NULL },
};

ActorCommand D_actor_161500_80133F74 = { { .loc = { 5, 4 } }, 1 };

ActorCommand D_actor_161500_80133F78 = { { .loc = { 5, 4 } }, 0 };

AnimationPlayRequest D_actor_161500_80133F7C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

Actor161500AnimStorage3F90 D_actor_161500_80133F90 = { .data = { { &D_actor_161500_80132EF0, &D_actor_161500_801330BC, &D_actor_161500_801334E4, &D_actor_161500_80133868, &D_actor_161500_80133B94, &D_actor_161500_80133F4C }, { { { .index = 1 }, 47, 1, 8, 1 }, { { .index = 1 }, 48, 1, 8, 1 }, { { .index = 1 }, 49, 1, 8, 1 }, { { .index = 1 }, 50, 1, 8, 1 }, { { .index = 1 }, 51, 1, 8, 1 }, { { .index = 1 }, 52, 1, 8, 1 } } } };

AnimationPlayRequest D_actor_161500_80134020 = { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80134034 = { { .index = 0 }, 11, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

GpCopyArg D_actor_161500_80134048 = { { .words = D_actor_161500_80133F90.words }, 32 };

GpEvsCmd D_actor_161500_80134050[13] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.arguments[2] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80134188[11] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 11 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80134290[11] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80134398[11] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_801344A0[15] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 34 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.arguments[2] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.arguments[5] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80134608[11] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 35 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80134710[11] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 36 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80134818[11] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 37 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd* D_actor_161500_80134920[8] = {
    D_actor_161500_80134050,
    D_actor_161500_80134188,
    D_actor_161500_80134290,
    D_actor_161500_80134398,
    D_actor_161500_801344A0,
    D_actor_161500_80134608,
    D_actor_161500_80134710,
    D_actor_161500_80134818,
};

GpEvsCmd D_actor_161500_80134940[15] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 14 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_161500_80133F90.data.arguments }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.arguments[1] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80134AA8[12] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 15 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80134BC8[13] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 16 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80134D00[11] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 17 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80134E08[12] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 38 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80134F28[12] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 39 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80135048[13] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 40 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.arguments[4] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80135180[11] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 41 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd* D_actor_161500_80135288[8] = {
    D_actor_161500_80134940,
    D_actor_161500_80134AA8,
    D_actor_161500_80134BC8,
    D_actor_161500_80134D00,
    D_actor_161500_80134E08,
    D_actor_161500_80134F28,
    D_actor_161500_80135048,
    D_actor_161500_80135180,
};

GpEvsCmd D_actor_161500_801352A8[22] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 50 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2003 }, { .storage = &D_actor_161500_80134020 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2003 }, { .storage = &D_actor_161500_80134020 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_161500_801320B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_801320F0 }, { .value = 52 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2003 }, { .storage = &D_actor_161500_80134020 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_801354B8[18] = {
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 51 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2003 }, { .storage = &D_actor_161500_80134020 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_161500_801320B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_801320F0 }, { .value = 52 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2003 }, { .storage = &D_actor_161500_80134020 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_80131F50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80135668[16] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 24 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_161500_80134020 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_161500_80132150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_801320F0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_161500_80134020 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_801357E8[16] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 25 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_161500_80134020 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_161500_80134034 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_161500_80132150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_801320F0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_161500_80134020 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80135968[16] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 26 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_161500_80134034 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_161500_80132150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_801320F0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_161500_80134020 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80135AE8[16] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 27 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_161500_80134020 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_161500_80132150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_801320F0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_161500_80134020 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80135C68[16] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 28 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80134048 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_161500_80134034 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F90.data.arguments[3] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_161500_80132150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_161500_801320F0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_161500_80134020 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

AnimationPackedPose D_actor_161500_80135DE8[6] = {
#include "assets/actor_161500_animation_04304_bank1.inc"
};

AnimationPackedRotation D_actor_161500_80135E30[75] = {
#include "assets/actor_161500_animation_04304_bank4.inc"
};

AnimationRecord D_actor_161500_80135F5C[104] = {
#include "assets/actor_161500_animation_04304_records.inc"
};

u16 D_actor_161500_801360FC[20] = {
#include "assets/actor_161500_animation_04304_indices.inc"
};

AnimationSet D_actor_161500_80136124 = {
    D_actor_161500_80135F5C,
    D_actor_161500_801360FC,
    { NULL, D_actor_161500_80135DE8, NULL, NULL, D_actor_161500_80135E30, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_8013614C[2] = {
#include "assets/actor_161500_animation_04518_bank1.inc"
};

AnimationPackedRotation D_actor_161500_80136164[24] = {
#include "assets/actor_161500_animation_04518_bank4.inc"
};

AnimationRecord D_actor_161500_801361C4[83] = {
#include "assets/actor_161500_animation_04518_records.inc"
};

u16 D_actor_161500_80136310[20] = {
#include "assets/actor_161500_animation_04518_indices.inc"
};

AnimationSet D_actor_161500_80136338 = {
    D_actor_161500_801361C4,
    D_actor_161500_80136310,
    { NULL, D_actor_161500_8013614C, NULL, NULL, D_actor_161500_80136164, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_80136360[5] = {
#include "assets/actor_161500_animation_047F8_bank1.inc"
};

AnimationPackedRotation D_actor_161500_8013639C[60] = {
#include "assets/actor_161500_animation_047F8_bank4.inc"
};

AnimationRecord D_actor_161500_8013648C[89] = {
#include "assets/actor_161500_animation_047F8_records.inc"
};

u16 D_actor_161500_801365F0[20] = {
#include "assets/actor_161500_animation_047F8_indices.inc"
};

AnimationSet D_actor_161500_80136618 = {
    D_actor_161500_8013648C,
    D_actor_161500_801365F0,
    { NULL, D_actor_161500_80136360, NULL, NULL, D_actor_161500_8013639C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_80136640[3] = {
#include "assets/actor_161500_animation_04A88_bank1.inc"
};

AnimationPackedRotation D_actor_161500_80136664[36] = {
#include "assets/actor_161500_animation_04A88_bank4.inc"
};

AnimationRecord D_actor_161500_801366F4[99] = {
#include "assets/actor_161500_animation_04A88_records.inc"
};

u16 D_actor_161500_80136880[20] = {
#include "assets/actor_161500_animation_04A88_indices.inc"
};

AnimationSet D_actor_161500_801368A8 = {
    D_actor_161500_801366F4,
    D_actor_161500_80136880,
    { NULL, D_actor_161500_80136640, NULL, NULL, D_actor_161500_80136664, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_801368D0[3] = {
#include "assets/actor_161500_animation_04C60_bank1.inc"
};

AnimationPackedRotation D_actor_161500_801368F4[32] = {
#include "assets/actor_161500_animation_04C60_bank4.inc"
};

AnimationRecord D_actor_161500_80136974[57] = {
#include "assets/actor_161500_animation_04C60_records.inc"
};

u16 D_actor_161500_80136A58[20] = {
#include "assets/actor_161500_animation_04C60_indices.inc"
};

AnimationSet D_actor_161500_80136A80 = {
    D_actor_161500_80136974,
    D_actor_161500_80136A58,
    { NULL, D_actor_161500_801368D0, NULL, NULL, D_actor_161500_801368F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_80136AA8[3] = {
#include "assets/actor_161500_animation_04E94_bank1.inc"
};

AnimationPackedRotation D_actor_161500_80136ACC[29] = {
#include "assets/actor_161500_animation_04E94_bank4.inc"
};

AnimationRecord D_actor_161500_80136B40[83] = {
#include "assets/actor_161500_animation_04E94_records.inc"
};

u16 D_actor_161500_80136C8C[20] = {
#include "assets/actor_161500_animation_04E94_indices.inc"
};

AnimationSet D_actor_161500_80136CB4 = {
    D_actor_161500_80136B40,
    D_actor_161500_80136C8C,
    { NULL, D_actor_161500_80136AA8, NULL, NULL, D_actor_161500_80136ACC, NULL, NULL, NULL },
};

TaskDesc D_actor_161500_80136CDC = { 0, 32, func_actor_161500_801321B4, { .model = NULL } };

ActorTransform D_actor_161500_80136CE8 = { { 850, 0, 4400, 0 }, { 0, 568, 0, 0 } };

ActorTransform D_actor_161500_80136D00 = { { 850, 0, 4400, 0 }, { 0, 568, 0, 0 } };

ActorTransform D_actor_161500_80136D18 = { { 2510, 0, 5950, 0 }, { 0, 2616, 0, 0 } };

ActorTransform D_actor_161500_80136D30 = { { 1330, 0, 4780, 0 }, { 0, 2616, 0, 0 } };

ActorTransform D_actor_161500_80136D48 = { { 4224, 0, 5209, 0 }, { 0, 2048, 0, 0 } };

Actor161500AnimCopy6D60 D_actor_161500_80136D60 = { .data = { { &D_actor_161500_80136124, &D_actor_161500_80136338, &D_actor_161500_80136618, &D_actor_161500_801368A8, &D_actor_161500_80136A80, &D_actor_161500_80136CB4, NULL }, { { { .index = 1 }, 47, 0, 0, 1 } } } };

AnimationPlayRequest D_actor_161500_80136D90 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DA4 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DB8 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DCC = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DE0 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DF4 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

GpCopyArg D_actor_161500_80136E08 = { { .words = D_actor_161500_80136D60.words }, 10 };

AnimationPlayRequest D_actor_161500_80136E10 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E24 = { { .index = 6 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E38 = { { .index = 6 }, 38, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E4C = { { .index = 6 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E60 = { { .index = 6 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E74 = { { .index = 6 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpEvsCmd D_actor_161500_80136E88[21] = {
    { 16, { .value = 0x55040003 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_161500_80136D00 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_161500_80136D48 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E10 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 23 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_1f_heliport_801802AC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80137080[62] = {
    { 47, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 22 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80136E08 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E10 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E24 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_161500_80136CE8 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_161500_80136D18 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x55040003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_161500_80136D30 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136D90 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { 29, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_161500_80136D30 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E24 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E4C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DCC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DE0 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DF4 }, { .value = 0 } },
    { 4, { .value = 46 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { 15, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_161500_80136D48 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_1f_heliport_801802AC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80137650[6] = {
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_161500_80136D48 }, { .value = 0 } },
    { 13, { .callback = func_shelter_1f_heliport_801802AC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

ActorTransform D_actor_161500_801376E0 = { { 0, 0, 0, 0 }, { 0, -1024, 0, 0 } };

GpEvsCmd D_actor_161500_801376F8[20] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80136E08 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 27 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_161500_80132210 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_161500_801376E0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136D90 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DB8 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_801378D8[20] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80136E08 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 29 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_161500_80132210 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_161500_801376E0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136D90 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DB8 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_161500_80137AB8[29] = {
    { 13, { .callback = func_shelter_1f_heliport_801802AC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_161500_80136E08 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 28 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_actor_161500_80132294 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_161500_80132210 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_actor_161500_801376E0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136D90 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E60 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DB8 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 8, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_actor_161500_80137D70[1] = {
#include "assets/actor_161500_model_06970_skeleton.inc"
};

u32 D_actor_161500_80137D94[1] = {
#include "assets/actor_161500_model_06970_partVerts.inc"
};

SVECTOR D_actor_161500_80137D98[58] = {
#include "assets/actor_161500_model_06970_verts.inc"
};

SVECTOR D_actor_161500_80137F68[58] = {
#include "assets/actor_161500_model_06970_normals.inc"
};

u32 D_actor_161500_80138138[406] = {
#include "assets/actor_161500_model_06970_stream.inc"
};

TmdSource D_actor_161500_80138790 = {
    0,
    2940,
    0,
    1,
    D_actor_161500_80137D94,
    D_actor_161500_80137D98,
    D_actor_161500_80137F68,
    D_actor_161500_80137D70,
    D_actor_161500_80138138,
};

TmdBone D_actor_161500_801387B4[20] = {
#include "assets/actor_161500_model_0C0A0_skeleton.inc"
};

u32 D_actor_161500_80138A84[20] = {
#include "assets/actor_161500_model_0C0A0_partVerts.inc"
};

SVECTOR D_actor_161500_80138AD4[366] = {
#include "assets/actor_161500_model_0C0A0_verts.inc"
};

SVECTOR D_actor_161500_80139644[363] = {
#include "assets/actor_161500_model_0C0A0_normals.inc"
};

u32 D_actor_161500_8013A19C[3913] = {
#include "assets/actor_161500_model_0C0A0_stream.inc"
};

TmdSource D_actor_161500_8013DEC0 = {
    0,
    21132,
    6448,
    20,
    D_actor_161500_80138A84,
    D_actor_161500_80138AD4,
    D_actor_161500_80139644,
    D_actor_161500_801387B4,
    D_actor_161500_8013A19C,
};

AnimationPackedPose D_actor_161500_8013DEE4[2] = {
#include "assets/actor_161500_animation_0C318_bank1.inc"
};

AnimationPackedRotation D_actor_161500_8013DEFC[32] = {
#include "assets/actor_161500_animation_0C318_bank4.inc"
};

AnimationRecord D_actor_161500_8013DF7C[101] = {
#include "assets/actor_161500_animation_0C318_records.inc"
};

u16 D_actor_161500_8013E110[20] = {
#include "assets/actor_161500_animation_0C318_indices.inc"
};

AnimationSet D_actor_161500_8013E138 = {
    D_actor_161500_8013DF7C,
    D_actor_161500_8013E110,
    { NULL, D_actor_161500_8013DEE4, NULL, NULL, D_actor_161500_8013DEFC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_8013E160[2] = {
#include "assets/actor_161500_animation_0C88C_bank1.inc"
};

AnimationPackedRotation D_actor_161500_8013E178[135] = {
#include "assets/actor_161500_animation_0C88C_bank4.inc"
};

AnimationRecord D_actor_161500_8013E394[188] = {
#include "assets/actor_161500_animation_0C88C_records.inc"
};

u16 D_actor_161500_8013E684[20] = {
#include "assets/actor_161500_animation_0C88C_indices.inc"
};

AnimationSet D_actor_161500_8013E6AC = {
    D_actor_161500_8013E394,
    D_actor_161500_8013E684,
    { NULL, D_actor_161500_8013E160, NULL, NULL, D_actor_161500_8013E178, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_8013E6D4[3] = {
#include "assets/actor_161500_animation_0CB1C_bank1.inc"
};

AnimationPackedRotation D_actor_161500_8013E6F8[27] = {
#include "assets/actor_161500_animation_0CB1C_bank4.inc"
};

AnimationRecord D_actor_161500_8013E764[108] = {
#include "assets/actor_161500_animation_0CB1C_records.inc"
};

u16 D_actor_161500_8013E914[20] = {
#include "assets/actor_161500_animation_0CB1C_indices.inc"
};

AnimationSet D_actor_161500_8013E93C = {
    D_actor_161500_8013E764,
    D_actor_161500_8013E914,
    { NULL, D_actor_161500_8013E6D4, NULL, NULL, D_actor_161500_8013E6F8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_8013E964[21] = {
#include "assets/actor_161500_animation_0D2B0_bank1.inc"
};

AnimationPackedRotation D_actor_161500_8013EA60[156] = {
#include "assets/actor_161500_animation_0D2B0_bank4.inc"
};

AnimationRecord D_actor_161500_8013ECD0[246] = {
#include "assets/actor_161500_animation_0D2B0_records.inc"
};

u16 D_actor_161500_8013F0A8[20] = {
#include "assets/actor_161500_animation_0D2B0_indices.inc"
};

AnimationSet D_actor_161500_8013F0D0 = {
    D_actor_161500_8013ECD0,
    D_actor_161500_8013F0A8,
    { NULL, D_actor_161500_8013E964, NULL, NULL, D_actor_161500_8013EA60, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_8013F0F8[5] = {
#include "assets/actor_161500_animation_0D544_bank1.inc"
};

AnimationPackedRotation D_actor_161500_8013F134[36] = {
#include "assets/actor_161500_animation_0D544_bank4.inc"
};

AnimationRecord D_actor_161500_8013F1C4[94] = {
#include "assets/actor_161500_animation_0D544_records.inc"
};

u16 D_actor_161500_8013F33C[20] = {
#include "assets/actor_161500_animation_0D544_indices.inc"
};

AnimationSet D_actor_161500_8013F364 = {
    D_actor_161500_8013F1C4,
    D_actor_161500_8013F33C,
    { NULL, D_actor_161500_8013F0F8, NULL, NULL, D_actor_161500_8013F134, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_8013F38C[2] = {
#include "assets/actor_161500_animation_0D834_bank1.inc"
};

AnimationPackedRotation D_actor_161500_8013F3A4[55] = {
#include "assets/actor_161500_animation_0D834_bank4.inc"
};

AnimationRecord D_actor_161500_8013F480[107] = {
#include "assets/actor_161500_animation_0D834_records.inc"
};

u16 D_actor_161500_8013F62C[20] = {
#include "assets/actor_161500_animation_0D834_indices.inc"
};

AnimationSet D_actor_161500_8013F654 = {
    D_actor_161500_8013F480,
    D_actor_161500_8013F62C,
    { NULL, D_actor_161500_8013F38C, NULL, NULL, D_actor_161500_8013F3A4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_8013F67C[2] = {
#include "assets/actor_161500_animation_0D9E8_bank1.inc"
};

AnimationPackedRotation D_actor_161500_8013F694[19] = {
#include "assets/actor_161500_animation_0D9E8_bank4.inc"
};

AnimationRecord D_actor_161500_8013F6E0[64] = {
#include "assets/actor_161500_animation_0D9E8_records.inc"
};

u16 D_actor_161500_8013F7E0[20] = {
#include "assets/actor_161500_animation_0D9E8_indices.inc"
};

AnimationSet D_actor_161500_8013F808 = {
    D_actor_161500_8013F6E0,
    D_actor_161500_8013F7E0,
    { NULL, D_actor_161500_8013F67C, NULL, NULL, D_actor_161500_8013F694, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_8013F830[2] = {
#include "assets/actor_161500_animation_0DB9C_bank1.inc"
};

AnimationPackedRotation D_actor_161500_8013F848[19] = {
#include "assets/actor_161500_animation_0DB9C_bank4.inc"
};

AnimationRecord D_actor_161500_8013F894[64] = {
#include "assets/actor_161500_animation_0DB9C_records.inc"
};

u16 D_actor_161500_8013F994[20] = {
#include "assets/actor_161500_animation_0DB9C_indices.inc"
};

AnimationSet D_actor_161500_8013F9BC = {
    D_actor_161500_8013F894,
    D_actor_161500_8013F994,
    { NULL, D_actor_161500_8013F830, NULL, NULL, D_actor_161500_8013F848, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_8013F9E4[3] = {
#include "assets/actor_161500_animation_0DE54_bank1.inc"
};

AnimationPackedRotation D_actor_161500_8013FA08[29] = {
#include "assets/actor_161500_animation_0DE54_bank4.inc"
};

AnimationRecord D_actor_161500_8013FA7C[116] = {
#include "assets/actor_161500_animation_0DE54_records.inc"
};

u16 D_actor_161500_8013FC4C[20] = {
#include "assets/actor_161500_animation_0DE54_indices.inc"
};

AnimationSet D_actor_161500_8013FC74 = {
    D_actor_161500_8013FA7C,
    D_actor_161500_8013FC4C,
    { NULL, D_actor_161500_8013F9E4, NULL, NULL, D_actor_161500_8013FA08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_8013FC9C[2] = {
#include "assets/actor_161500_animation_0E148_bank1.inc"
};

AnimationPackedRotation D_actor_161500_8013FCB4[43] = {
#include "assets/actor_161500_animation_0E148_bank4.inc"
};

AnimationRecord D_actor_161500_8013FD60[120] = {
#include "assets/actor_161500_animation_0E148_records.inc"
};

u16 D_actor_161500_8013FF40[20] = {
#include "assets/actor_161500_animation_0E148_indices.inc"
};

AnimationSet D_actor_161500_8013FF68 = {
    D_actor_161500_8013FD60,
    D_actor_161500_8013FF40,
    { NULL, D_actor_161500_8013FC9C, NULL, NULL, D_actor_161500_8013FCB4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_161500_8013FF90[2] = {
#include "assets/actor_161500_animation_0E338_bank1.inc"
};

AnimationPackedRotation D_actor_161500_8013FFA8[30] = {
#include "assets/actor_161500_animation_0E338_bank4.inc"
};

AnimationRecord D_actor_161500_80140020[68] = {
#include "assets/actor_161500_animation_0E338_records.inc"
};

u16 D_actor_161500_80140130[20] = {
#include "assets/actor_161500_animation_0E338_indices.inc"
};

AnimationSet D_actor_161500_80140158 = {
    D_actor_161500_80140020,
    D_actor_161500_80140130,
    { NULL, D_actor_161500_8013FF90, NULL, NULL, D_actor_161500_8013FFA8, NULL, NULL, NULL },
};

Actor161500MessageEntry D_actor_161500_80140180[6] = {
    { 2003, { .call0 = func_actor_161500_80132A28 } },
    { 2005, { .call3 = func_actor_161500_80132A94 } },
    { 2004, { .call2 = func_actor_161500_80132B10 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = func_actor_161500_80132B88 } },
    { 2013, { .call2 = func_actor_161500_80132BA0 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_161500_801401B0[2] = {
    { 257, 96, func_actor_161500_801326E8, { .model = &D_actor_161500_8013DEC0 } },
    { 257, 96, func_actor_161500_80132C6C, { .model = &D_actor_161500_80138790 } },
};

AnimationSet* D_actor_161500_801401C8[12] = {
    NULL,
    &D_actor_161500_8013E138,
    &D_actor_161500_8013E93C,
    &D_actor_161500_8013E6AC,
    &D_actor_161500_8013F0D0,
    &D_actor_161500_8013F654,
    &D_actor_161500_8013F808,
    &D_actor_161500_8013F9BC,
    &D_actor_161500_8013FC74,
    &D_actor_161500_8013FF68,
    &D_actor_161500_80140158,
    &D_actor_161500_8013F364,
};

static void func_actor_161500_80131E38(void);
static void func_actor_161500_80131FBC(void);
static void func_actor_161500_80132038(void);
static void func_actor_161500_80132110(void);
static void func_actor_161500_801322A0(void);
static void func_actor_161500_8013230C(void);
static void func_actor_161500_80132394(GpEnemy* enemy, Task* task);

static void func_actor_161500_80131E38(void)
{
    if ((GameFlag_GetNibble(0x116) != 1) && (GameFlag_GetNibble(0x116) != 2) && (GameFlag_GetNibble(0x113) == 4)) {
        GameFlag_SetNibble(0x113, 5);
        GameFlag_SetNibble(0x116, 4);
    }

    switch (GameFlag_GetNibble(0x116)) {
        case 0:
            func_800E8614(D_actor_161500_80135668, 0);
            break;
        case 1:
            func_800E8614(D_actor_161500_801357E8, 0);
            GameFlag_SetNibble(0x116, 3);
            break;
        case 2:
            func_800E8614(D_actor_161500_80135968, 0);
            GameFlag_SetNibble(0x116, 3);
            break;
        case 3:
            func_800E8614(D_actor_161500_80135AE8, 0);
            GameFlag_SetNibble(0x116, 0);
            break;
        case 4:
            func_800E8614(D_actor_161500_80135C68, 0);
            GameFlag_SetNibble(0x116, 0);
            break;
    }
}

void func_actor_161500_80131F50(s32 arg0)
{
    s8 capFile;

    if (arg0 != 0) {
        Gp_CapFile = 0;
        if (arg0 <= 0) {
            capFile = 1;
            if (gGameSession->at4.loc.variant == 1) {
                capFile = 2;
            }
            arg0 = capFile;
        }
        Gp_LoadCapFile(arg0);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

static void func_actor_161500_80131FBC(void)
{
    s32 temp_s0;
    s32 temp_v0;

    temp_s0 = (gGameSession->at4.loc.variant == 1) * 4;
    temp_v0 = GameFlag_GetNibble(0x103);
    func_800E8614(D_actor_161500_80134920[temp_v0 + temp_s0], 0);
    if (temp_v0 < 3) {
        GameFlag_SetNibble(0x103, temp_v0 + 1);
    }
}

static void func_actor_161500_80132038(void)
{
    s32 temp_s0;
    s32 temp_v0;

    temp_s0 = (gGameSession->at4.loc.variant == 1) * 4;
    temp_v0 = GameFlag_GetNibble(0x104);
    func_800E8614(D_actor_161500_80135288[temp_v0 + temp_s0], 0);
    if (temp_v0 < 3) {
        GameFlag_SetNibble(0x104, temp_v0 + 1);
    }
}

void func_actor_161500_801320B4(void)
{
    GameSession* session;

    session = gGameSession;
    do {
        func_800D4D2C((session->at4.loc.variant == 1) ? 0x31 : 0x30);
    } while (0);
}

void func_actor_161500_801320F0(s32 arg0)
{
    Gp_RunCapCmd(arg0, 0);
}

static void func_actor_161500_80132110(void)
{
    if (GameFlag_GetNibble(0x105) == 0) {
        func_800E8614(D_actor_161500_801352A8, 0);
    } else {
        func_800E8614(D_actor_161500_801354B8, 0);
    }
}

void func_actor_161500_80132150(void)
{
    if (GameFlag_GetNibble(0x112) != 0) {
        func_800D4D2C((GameFlag_GetNibble(0xEA) != 2) ? 0x31 : 0x33);
    } else {
        func_800D4D2C((GameFlag_GetNibble(0xEA) == 2) ? 0x32 : 0x30);
    }
}

void func_actor_161500_801321B4(Task* arg0)
{
    D_80115768 = 1;
    Gp_SetItemSeenBit(0x124, 1);
    GameFlag_SetNibble(0xE4, 2);
    func_800E8614(D_actor_161500_80137AB8, 0);
    taskKill(arg0);
}

void func_actor_161500_80132210(void)
{
    GfxCoord* target;
    GfxCoord* player;

    target = (gameGetPtrSlot(0xA))->extra.tmd->coords;
    player = (gameGetPtrSlot(3))->extra.tmd->coords;
    Gp_UpdateCoord(target);
    Gp_UpdateCoord(player);
    D_actor_161500_801376E0.rot.vy =
        ratan2(target->coord.t[0] - player->coord.t[0], target->coord.t[2] - player->coord.t[2]) & 0xFFF;
}

void func_actor_161500_80132294(u8 arg0)
{
    D_80115768 = arg0;
}

static void func_actor_161500_801322A0(void)
{
    s32 temp_v0;

    if (gameGetPtrSlot(0xA) != NULL) {
        temp_v0 = GameFlag_GetNibble(0xE4);
        if (temp_v0 == 1) {
            if (Gp_GetCurBit2Flag(3) == temp_v0) {
                func_800E8614(D_actor_161500_801378D8, 0);
            } else {
                func_800E8614(D_actor_161500_801376F8, 0);
            }
        }
    }
}

static void func_actor_161500_8013230C(void)
{
    s32 temp_v0;

    if (gameGetPtrSlot(0xA) != NULL) {
        temp_v0 = GameFlag_GetNibble(0xE4);
        switch (temp_v0) {
            case 0:
                func_800E8634(D_actor_161500_80137080, 0, D_actor_161500_80136E88);
                GameFlag_SetNibble(0xE4, 1);
                break;
            case 1:
                func_800E8614(D_actor_161500_80137650, 1);
                break;
            case 2:
                break;
        }
    }
}

/// The actor's spawn routine: allocates the work block, destroying the enemy
/// if that fails, and installs the exit callback. With `Task::spawnArg1` set it
/// spawns the paired enemy, reparents its own task under the pair's and starts
/// on clip 2, otherwise on clip 1. It then lights the model, sets up the
/// animation context and the task's message table, and runs the step body
/// once with the plain reseed queued.
static void func_actor_161500_80132394(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor161500Work* work;
    GfxCoord*        coord;
    TmdObject*       obj;
    GpEnemy*         spawned;

    coord      = task->extra.tmd->coords;
    obj        = task->extra.tmd;
    work       = (Actor161500Work*)memCalloc(0x4FC, false);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_161500_8013284C;
    coord->parent                = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    work->enemy                  = enemy;
    if (task->spawnArg1.value != 0) {
        spawned = Gp_SpawnEnemyFromTable(D_actor_161500_801401B0, 1, 0, enemy);
        Task_Reparent(task, spawned->task);
        work->pairTask  = spawned->task;
        work->st.animId = 2;
    } else {
        work->st.animId = 1;
    }
    work->turnUp     = 0;
    work->turnWeight = 0;
    obj->lightMtx    = &work->light;
    obj->colorMtx    = &work->color;
    vec.vx           = coord->workm.t[0];
    vec.vy           = coord->workm.t[1] - 0x320;
    vec.vz           = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, D_actor_161500_801401C8, obj,
                  work->rig.poses, work->rig.slots);
    work->st.state = 2;
    task->msgTable = D_actor_161500_80140180;
    func_actor_161500_8013252C(task);
    task->state += 1;
}

/// The actor's step body. States 1 and 2 reseed the animation slots (with and
/// without `animArg`) and advance to 3; state 3 walks the root coordinate 30
/// units per frame while the walk clip has `travel` left, and when it runs out
/// queues a reseed into clip 1 with argument 0xA, then ticks the slots.
static void func_actor_161500_8013252C(Task* task)
{
    Actor161500Work* work;
    s16              animId;

    work = (Actor161500Work*)task->work;
    if (work->st.state == 1) {
        func_actor_161500_801329C4(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_161500_8013294C(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveForward(task->extra.tmd->coords, 0x1E);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->st.state  = 1;
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        func_actor_161500_80132900(task);
        return;
    }
}

/// The actor's task body: dispatches on `Task::state` to the spawn routine
/// (state 0) or the per-frame body (state 1), handing each the task's
/// `GpEnemy` from `Task::spawnArg2`. The handler table is built on the stack.
void func_actor_161500_801326E8(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_161500_80132394,
        func_actor_161500_8013273C,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// The actor's draw body: refreshes the model root's coordinate, lights the
/// model at its world translation raised by 800 on y, then runs the step body.
/// `turnWeight` is the head-tracking blend rate handed to `func_800B0928`,
/// ramped toward 0x1000 in 0x200 steps while `turnUp` is 1 and back down to
/// 0 otherwise, so the actor turns its head to the player and away again
/// smoothly instead of snapping.
static void func_actor_161500_8013273C(GpEnemy* enemy, Task* task)
{
    TmdObject*       obj;
    GfxCoord*        coord;
    Actor161500Work* work;
    VECTOR           pos;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = (Actor161500Work*)task->work;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_161500_8013252C(task);
    if (work->turnUp == 1) {
        work->turnWeight += 0x200;
        if (work->turnWeight > 0x1000) {
            work->turnWeight = 0x1000;
        }
    } else {
        work->turnWeight -= 0x200;
        if (work->turnWeight < 0) {
            work->turnWeight = 0;
        }
    }
    func_800B0928(task, gameGetPtrSlot(3), 0x200, 0x100, work->turnWeight);
    func_actor_161500_80132874(task);
}

/// The actor's `Task::exitCallback`: hands the task's `GpEnemy`, parked in
/// `Task::spawnArg2`, back to `Gp_DestroyEnemy`.
static void func_actor_161500_8013284C(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Draws the actor's ground shadow quad under the model root, unless the model
/// is hidden (`TmdObject::flags` bit 0x80) or has no buffer yet. The world
/// position is the translation of the root coordinate's `workm`, staged in a
/// scratchpad VECTOR3 rather than on the stack.
static void func_actor_161500_80132874(Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR3*   vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & TMD_OBJECT_HIDDEN) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, 0xC0);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Ticks animation slots 1..0x13 of the actor's animation context.
static void func_actor_161500_80132900(Task* task)
{
    Actor161500Work* work;
    s32              i;

    work = (Actor161500Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Reseeds animation slots 1..0x13 with `animId`, each at rate 1, and records
/// that id as the one applied.
static void func_actor_161500_8013294C(Task* task)
{
    Actor161500Work* work;
    s32              i;

    work = (Actor161500Work*)task->work;
    i    = 1;
    do {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Reseeds animation slots 1..0x13 with `animId`, passing `animArg` through,
/// and records that id as the one applied.
static void func_actor_161500_801329C4(Task* task)
{
    Actor161500Work* work;
    s32              i;

    work = (Actor161500Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 0xC and above before changing playback state.
/// The blend path carries the requested duration in whole frames.
s32 func_actor_161500_80132A28(Task* task, s32 arg1, AnimationPlayRequest* args)
{
    Actor161500Work* work;

    work = (Actor161500Work*)task->work;
    if (args->animationId < 0xC) {
        work->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state = 1;
            work->animArg  = args->blendFrames;
        } else {
            work->st.state = 2;
        }
        work->st.field_6 = 0;
        func_actor_161500_8013252C(task);
        return 0;
    }
    return -1;
}

/// Script opcode: hides or shows this actor's model and the model of the pair
/// task its spawn routine parked in `pairTask`. Without `flags` bit 0 both
/// models get `TmdObject::flags` 0x80, which hides them; with it the flags are
/// cleared. Bit 1 additionally ORs in 0x4. With no pair spawned
/// (`Task::spawnArg1` == 0) the actor drives its own model twice.
s32 func_actor_161500_80132A94(Task* task, s32 arg1, s32 flags)
{
    Actor161500Work* work;
    TmdObject*       self;
    TmdObject*       other;

    self = task->extra.tmd;
    work = (Actor161500Work*)task->work;
    if (task->spawnArg1.value != 0) {
        other = work->pairTask->extra.tmd;
    } else {
        other = self;
    }
    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = TMD_OBJECT_HIDDEN;
        other->flags = TMD_OBJECT_HIDDEN;
    }
    if (flags & 2) {
        self->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        other->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Script opcode "place at": yaws the actor's root coordinate to
/// `placement->rot.vy`, caching that yaw in the work block, then drops the
/// placement translation into the matrix and marks it dirty.
s32 func_actor_161500_80132B10(Task* task, s32 arg1, ActorTransform* placement)
{
    GfxCoord*        coord;
    Actor161500Work* work;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor161500Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Script opcode: sets the work block's `turnUp`, which selects whether the
/// per-frame body turns the actor's head toward the player or away, to the
/// payload.
s32 func_actor_161500_80132B88(Task* task, s32 arg1, ActorCommand* args)
{
    ((Actor161500Work*)task->work)->turnUp = args->command;
    return 0;
}

/// Script opcode "walk to": aims the actor's root coordinate at `target` by
/// taking the yaw of the horizontal offset from the coordinate's own
/// translation, caches that yaw in the work block and rebuilds the local
/// matrix from it, then records the distance, in steps of 30, for the walk
/// that follows.
s32 func_actor_161500_80132BA0(Task* task, s32 arg1, ActorTransform* target)
{
    GfxCoord*        coord;
    Actor161500Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor161500Work*)task->work;
    dx           = target->pos.vx - coord->coord.t[0];
    dz           = target->pos.vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 30;
    return 0;
}

/// Per-frame task of the actor's sub-model, with the sub-model's own
/// `TmdObject` in `Task::extra` and the actor as `Task::parent`. The first
/// frame lights the sub-model with the matrix pair at the front of the
/// parent's work block and hangs its coordinate off the parent model's eighth
/// coordinate; every frame marks the coordinate dirty.
void func_actor_161500_80132C6C(Task* task)
{
    Task*      parent = task->parent;
    TmdObject* obj    = task->extra.tmd;
    GfxCoord*  coord  = obj->coords;
    GfxCoord*  sub    = &parent->extra.tmd->coords[7];
    MATRIX*    work   = (MATRIX*)parent->work;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            obj->lightMtx       = work;
            obj->colorMtx       = work + 1;
            coord->parent       = sub;
            task->state++;
            break;
        case 1:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
