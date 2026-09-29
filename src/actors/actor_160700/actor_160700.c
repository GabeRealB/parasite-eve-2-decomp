#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
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
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        GpAnimSet* sets[23];
        GpCopyArg  copy;
        GpAnimArg  arguments[2];
    } data;
    s32 words[35];
} Actor160700AnimStorage51E8;
STATIC_ASSERT_SIZEOF(Actor160700AnimStorage51E8, 140);

extern Actor160700AnimStorage51E8 D_actor_160700_801351E8;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern TaskDesc D_actor_160700_801416A8[];
extern u8       D_actor_160700_801416C0[];
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, GpAnimArg*);
        s32 (*call2)(Task*, s32, GpXformArg*);
        s32 (*call3)(Task*, s32, s32);
    } handler;
} Actor160700MessageEntry;
STATIC_ASSERT_SIZEOF(Actor160700MessageEntry, 8);

extern Actor160700MessageEntry D_actor_160700_80141678[6];

extern GpAnimArg D_actor_160700_801354CC;
extern GpEvsCmd  D_actor_160700_80135664[];
extern GpEvsCmd  D_actor_160700_80135ACC[];
extern GpEvsCmd  D_actor_160700_80135BD4[];
extern GpEvsCmd  D_actor_160700_801362F4[];
extern GpEvsCmd  D_actor_160700_80136414[];

static void func_actor_160700_80132184(Task* task);
static void func_actor_160700_80132390(GpEnemy* enemy, Task* task);
static void func_actor_160700_80132414(Task* task);
static void func_actor_160700_8013243C(Task* task);
static void func_actor_160700_801324C8(Task* task);
static void func_actor_160700_80132514(Task* task);
static void func_actor_160700_8013258C(Task* task);

extern GpAnimArg D_actor_160700_80135288;
extern GpAnimArg D_actor_160700_8013529C;
extern GpAnimArg D_actor_160700_801352B0;
extern GpAnimArg D_actor_160700_801352C4;
extern GpAnimArg D_actor_160700_801352D8;
extern GpAnimArg D_actor_160700_801352EC;
extern GpAnimArg D_actor_160700_80135300;
extern GpAnimArg D_actor_160700_80135314;
extern GpAnimArg D_actor_160700_80135328;
extern GpAnimArg D_actor_160700_8013533C;
extern GpAnimArg D_actor_160700_80135350;
extern GpAnimArg D_actor_160700_80135364;
extern GpAnimArg D_actor_160700_80135378;
extern GpAnimArg D_actor_160700_8013538C;
extern GpAnimArg D_actor_160700_801353A0;
extern GpAnimArg D_actor_160700_801353B4;
extern GpAnimArg D_actor_160700_801353C8;
extern GpAnimArg D_actor_160700_801353DC;
extern GpAnimArg D_actor_160700_801353F0;
extern GpAnimArg D_actor_160700_8013547C;
extern GpAnimArg D_actor_160700_80135490;
extern GpAnimArg D_actor_160700_801354A4;
extern GpAnimArg D_actor_160700_801354B8;

extern TmdSource D_actor_160700_8013C6FC;
extern TmdSource D_actor_160700_8013C8F8;
s32              func_actor_160700_801325F0(Task*, s32, GpAnimArg*);
s32              func_actor_160700_8013265C(Task*, s32, s32);
s32              func_actor_160700_801326C0(Task*, s32, GpXformArg*);
s32              func_actor_160700_80132738(void);
s32              func_actor_160700_80132740(Task*, s32, GpXformArg*);
void             func_actor_160700_8013233C(Task*);
void             func_actor_160700_80132808(Task*);

extern Actor160700AnimStorage51E8 D_actor_160700_801351E8;

AnimationPackedPose D_actor_160700_8013287C[2] = {
#include "assets/actor_160700_animation_00C90_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80132894[26] = {
#include "assets/actor_160700_animation_00C90_bank4.inc"
};

AnimationRecord D_actor_160700_801328FC[99] = {
#include "assets/actor_160700_animation_00C90_records.inc"
};

u16 D_actor_160700_80132A88[20] = {
#include "assets/actor_160700_animation_00C90_indices.inc"
};

GpAnimSet D_actor_160700_80132AB0 = {
    D_actor_160700_801328FC,
    D_actor_160700_80132A88,
    { NULL, D_actor_160700_8013287C, NULL, NULL, D_actor_160700_80132894, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80132AD8[3] = {
#include "assets/actor_160700_animation_00EC0_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80132AFC[30] = {
#include "assets/actor_160700_animation_00EC0_bank4.inc"
};

AnimationRecord D_actor_160700_80132B74[81] = {
#include "assets/actor_160700_animation_00EC0_records.inc"
};

u16 D_actor_160700_80132CB8[20] = {
#include "assets/actor_160700_animation_00EC0_indices.inc"
};

GpAnimSet D_actor_160700_80132CE0 = {
    D_actor_160700_80132B74,
    D_actor_160700_80132CB8,
    { NULL, D_actor_160700_80132AD8, NULL, NULL, D_actor_160700_80132AFC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80132D08[3] = {
#include "assets/actor_160700_animation_01184_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80132D2C[36] = {
#include "assets/actor_160700_animation_01184_bank4.inc"
};

AnimationRecord D_actor_160700_80132DBC[112] = {
#include "assets/actor_160700_animation_01184_records.inc"
};

u16 D_actor_160700_80132F7C[20] = {
#include "assets/actor_160700_animation_01184_indices.inc"
};

GpAnimSet D_actor_160700_80132FA4 = {
    D_actor_160700_80132DBC,
    D_actor_160700_80132F7C,
    { NULL, D_actor_160700_80132D08, NULL, NULL, D_actor_160700_80132D2C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80132FCC[3] = {
#include "assets/actor_160700_animation_0135C_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80132FF0[32] = {
#include "assets/actor_160700_animation_0135C_bank4.inc"
};

AnimationRecord D_actor_160700_80133070[57] = {
#include "assets/actor_160700_animation_0135C_records.inc"
};

u16 D_actor_160700_80133154[20] = {
#include "assets/actor_160700_animation_0135C_indices.inc"
};

GpAnimSet D_actor_160700_8013317C = {
    D_actor_160700_80133070,
    D_actor_160700_80133154,
    { NULL, D_actor_160700_80132FCC, NULL, NULL, D_actor_160700_80132FF0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_801331A4[3] = {
#include "assets/actor_160700_animation_01530_bank1.inc"
};

AnimationPackedRotation D_actor_160700_801331C8[31] = {
#include "assets/actor_160700_animation_01530_bank4.inc"
};

AnimationRecord D_actor_160700_80133244[57] = {
#include "assets/actor_160700_animation_01530_records.inc"
};

u16 D_actor_160700_80133328[20] = {
#include "assets/actor_160700_animation_01530_indices.inc"
};

GpAnimSet D_actor_160700_80133350 = {
    D_actor_160700_80133244,
    D_actor_160700_80133328,
    { NULL, D_actor_160700_801331A4, NULL, NULL, D_actor_160700_801331C8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80133378[3] = {
#include "assets/actor_160700_animation_017A0_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013339C[29] = {
#include "assets/actor_160700_animation_017A0_bank4.inc"
};

AnimationRecord D_actor_160700_80133410[98] = {
#include "assets/actor_160700_animation_017A0_records.inc"
};

u16 D_actor_160700_80133598[20] = {
#include "assets/actor_160700_animation_017A0_indices.inc"
};

GpAnimSet D_actor_160700_801335C0 = {
    D_actor_160700_80133410,
    D_actor_160700_80133598,
    { NULL, D_actor_160700_80133378, NULL, NULL, D_actor_160700_8013339C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_801335E8[3] = {
#include "assets/actor_160700_animation_01A4C_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013360C[32] = {
#include "assets/actor_160700_animation_01A4C_bank4.inc"
};

AnimationRecord D_actor_160700_8013368C[110] = {
#include "assets/actor_160700_animation_01A4C_records.inc"
};

u16 D_actor_160700_80133844[20] = {
#include "assets/actor_160700_animation_01A4C_indices.inc"
};

GpAnimSet D_actor_160700_8013386C = {
    D_actor_160700_8013368C,
    D_actor_160700_80133844,
    { NULL, D_actor_160700_801335E8, NULL, NULL, D_actor_160700_8013360C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80133894[3] = {
#include "assets/actor_160700_animation_01C18_bank1.inc"
};

AnimationPackedRotation D_actor_160700_801338B8[29] = {
#include "assets/actor_160700_animation_01C18_bank4.inc"
};

AnimationRecord D_actor_160700_8013392C[57] = {
#include "assets/actor_160700_animation_01C18_records.inc"
};

u16 D_actor_160700_80133A10[20] = {
#include "assets/actor_160700_animation_01C18_indices.inc"
};

GpAnimSet D_actor_160700_80133A38 = {
    D_actor_160700_8013392C,
    D_actor_160700_80133A10,
    { NULL, D_actor_160700_80133894, NULL, NULL, D_actor_160700_801338B8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80133A60[2] = {
#include "assets/actor_160700_animation_01E54_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80133A78[35] = {
#include "assets/actor_160700_animation_01E54_bank4.inc"
};

AnimationRecord D_actor_160700_80133B04[82] = {
#include "assets/actor_160700_animation_01E54_records.inc"
};

u16 D_actor_160700_80133C4C[20] = {
#include "assets/actor_160700_animation_01E54_indices.inc"
};

GpAnimSet D_actor_160700_80133C74 = {
    D_actor_160700_80133B04,
    D_actor_160700_80133C4C,
    { NULL, D_actor_160700_80133A60, NULL, NULL, D_actor_160700_80133A78, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80133C9C[3] = {
#include "assets/actor_160700_animation_02024_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80133CC0[30] = {
#include "assets/actor_160700_animation_02024_bank4.inc"
};

AnimationRecord D_actor_160700_80133D38[57] = {
#include "assets/actor_160700_animation_02024_records.inc"
};

u16 D_actor_160700_80133E1C[20] = {
#include "assets/actor_160700_animation_02024_indices.inc"
};

GpAnimSet D_actor_160700_80133E44 = {
    D_actor_160700_80133D38,
    D_actor_160700_80133E1C,
    { NULL, D_actor_160700_80133C9C, NULL, NULL, D_actor_160700_80133CC0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80133E6C[3] = {
#include "assets/actor_160700_animation_02284_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80133E90[26] = {
#include "assets/actor_160700_animation_02284_bank4.inc"
};

AnimationRecord D_actor_160700_80133EF8[97] = {
#include "assets/actor_160700_animation_02284_records.inc"
};

u16 D_actor_160700_8013407C[20] = {
#include "assets/actor_160700_animation_02284_indices.inc"
};

GpAnimSet D_actor_160700_801340A4 = {
    D_actor_160700_80133EF8,
    D_actor_160700_8013407C,
    { NULL, D_actor_160700_80133E6C, NULL, NULL, D_actor_160700_80133E90, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_801340CC[3] = {
#include "assets/actor_160700_animation_02454_bank1.inc"
};

AnimationPackedRotation D_actor_160700_801340F0[30] = {
#include "assets/actor_160700_animation_02454_bank4.inc"
};

AnimationRecord D_actor_160700_80134168[57] = {
#include "assets/actor_160700_animation_02454_records.inc"
};

u16 D_actor_160700_8013424C[20] = {
#include "assets/actor_160700_animation_02454_indices.inc"
};

GpAnimSet D_actor_160700_80134274 = {
    D_actor_160700_80134168,
    D_actor_160700_8013424C,
    { NULL, D_actor_160700_801340CC, NULL, NULL, D_actor_160700_801340F0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013429C[5] = {
#include "assets/actor_160700_animation_02714_bank1.inc"
};

AnimationPackedRotation D_actor_160700_801342D8[54] = {
#include "assets/actor_160700_animation_02714_bank4.inc"
};

AnimationRecord D_actor_160700_801343B0[87] = {
#include "assets/actor_160700_animation_02714_records.inc"
};

u16 D_actor_160700_8013450C[20] = {
#include "assets/actor_160700_animation_02714_indices.inc"
};

GpAnimSet D_actor_160700_80134534 = {
    D_actor_160700_801343B0,
    D_actor_160700_8013450C,
    { NULL, D_actor_160700_8013429C, NULL, NULL, D_actor_160700_801342D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013455C[3] = {
#include "assets/actor_160700_animation_029AC_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80134580[28] = {
#include "assets/actor_160700_animation_029AC_bank4.inc"
};

AnimationRecord D_actor_160700_801345F0[109] = {
#include "assets/actor_160700_animation_029AC_records.inc"
};

u16 D_actor_160700_801347A4[20] = {
#include "assets/actor_160700_animation_029AC_indices.inc"
};

GpAnimSet D_actor_160700_801347CC = {
    D_actor_160700_801345F0,
    D_actor_160700_801347A4,
    { NULL, D_actor_160700_8013455C, NULL, NULL, D_actor_160700_80134580, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_801347F4[2] = {
#include "assets/actor_160700_animation_02BD4_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013480C[23] = {
#include "assets/actor_160700_animation_02BD4_bank4.inc"
};

AnimationRecord D_actor_160700_80134868[89] = {
#include "assets/actor_160700_animation_02BD4_records.inc"
};

u16 D_actor_160700_801349CC[20] = {
#include "assets/actor_160700_animation_02BD4_indices.inc"
};

GpAnimSet D_actor_160700_801349F4 = {
    D_actor_160700_80134868,
    D_actor_160700_801349CC,
    { NULL, D_actor_160700_801347F4, NULL, NULL, D_actor_160700_8013480C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80134A1C[2] = {
#include "assets/actor_160700_animation_02D7C_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80134A34[23] = {
#include "assets/actor_160700_animation_02D7C_bank4.inc"
};

AnimationRecord D_actor_160700_80134A90[57] = {
#include "assets/actor_160700_animation_02D7C_records.inc"
};

u16 D_actor_160700_80134B74[20] = {
#include "assets/actor_160700_animation_02D7C_indices.inc"
};

GpAnimSet D_actor_160700_80134B9C = {
    D_actor_160700_80134A90,
    D_actor_160700_80134B74,
    { NULL, D_actor_160700_80134A1C, NULL, NULL, D_actor_160700_80134A34, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80134BC4[3] = {
#include "assets/actor_160700_animation_02F38_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80134BE8[19] = {
#include "assets/actor_160700_animation_02F38_bank4.inc"
};

AnimationRecord D_actor_160700_80134C34[63] = {
#include "assets/actor_160700_animation_02F38_records.inc"
};

u16 D_actor_160700_80134D30[20] = {
#include "assets/actor_160700_animation_02F38_indices.inc"
};

GpAnimSet D_actor_160700_80134D58 = {
    D_actor_160700_80134C34,
    D_actor_160700_80134D30,
    { NULL, D_actor_160700_80134BC4, NULL, NULL, D_actor_160700_80134BE8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80134D80[3] = {
#include "assets/actor_160700_animation_030F4_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80134DA4[19] = {
#include "assets/actor_160700_animation_030F4_bank4.inc"
};

AnimationRecord D_actor_160700_80134DF0[63] = {
#include "assets/actor_160700_animation_030F4_records.inc"
};

u16 D_actor_160700_80134EEC[20] = {
#include "assets/actor_160700_animation_030F4_indices.inc"
};

GpAnimSet D_actor_160700_80134F14 = {
    D_actor_160700_80134DF0,
    D_actor_160700_80134EEC,
    { NULL, D_actor_160700_80134D80, NULL, NULL, D_actor_160700_80134DA4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80134F3C[3] = {
#include "assets/actor_160700_animation_033A0_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80134F60[55] = {
#include "assets/actor_160700_animation_033A0_bank4.inc"
};

AnimationRecord D_actor_160700_8013503C[87] = {
#include "assets/actor_160700_animation_033A0_records.inc"
};

u16 D_actor_160700_80135198[20] = {
#include "assets/actor_160700_animation_033A0_indices.inc"
};

GpAnimSet D_actor_160700_801351C0 = {
    D_actor_160700_8013503C,
    D_actor_160700_80135198,
    { NULL, D_actor_160700_80134F3C, NULL, NULL, D_actor_160700_80134F60, NULL, NULL, NULL },
};

Actor160700AnimStorage51E8 D_actor_160700_801351E8 = { .data = { { &D_actor_160700_80132AB0, &D_actor_160700_80132CE0, &D_actor_160700_80132FA4, &D_actor_160700_8013317C, &D_actor_160700_80133350, &D_actor_160700_801335C0, &D_actor_160700_8013386C, &D_actor_160700_80133A38, &D_actor_160700_80133C74, &D_actor_160700_80133E44, &D_actor_160700_801340A4, &D_actor_160700_80134274, &D_actor_160700_80134534, &D_actor_160700_801347CC, &D_actor_160700_801349F4, &D_actor_160700_80134B9C, &D_actor_160700_80134D58, &D_actor_160700_80134F14, &D_actor_160700_801351C0, NULL, NULL, NULL, NULL }, { { .words = D_actor_160700_801351E8.words }, 32 }, { { { .index = 1 }, 1, 0, 0, 0 }, { { .index = 1 }, 1, 1, 15, 0 } } } };

GpAnimArg D_actor_160700_80135274 = { { .index = 1 }, 47, 0, 0, 0 };

GpAnimArg D_actor_160700_80135288 = { { .index = 1 }, 47, 0, 0, 0 };

GpAnimArg D_actor_160700_8013529C = { { .index = 1 }, 48, 0, 0, 0 };

GpAnimArg D_actor_160700_801352B0 = { { .index = 1 }, 49, 0, 0, 0 };

GpAnimArg D_actor_160700_801352C4 = { { .index = 1 }, 50, 0, 0, 0 };

GpAnimArg D_actor_160700_801352D8 = { { .index = 1 }, 51, 0, 0, 0 };

GpAnimArg D_actor_160700_801352EC = { { .index = 1 }, 52, 0, 0, 0 };

GpAnimArg D_actor_160700_80135300 = { { .index = 1 }, 53, 0, 0, 0 };

GpAnimArg D_actor_160700_80135314 = { { .index = 1 }, 54, 0, 0, 0 };

GpAnimArg D_actor_160700_80135328 = { { .index = 1 }, 55, 0, 0, 0 };

GpAnimArg D_actor_160700_8013533C = { { .index = 1 }, 56, 0, 0, 0 };

GpAnimArg D_actor_160700_80135350 = { { .index = 1 }, 57, 0, 0, 0 };

GpAnimArg D_actor_160700_80135364 = { { .index = 1 }, 58, 0, 0, 0 };

GpAnimArg D_actor_160700_80135378 = { { .index = 1 }, 59, 0, 0, 0 };

GpAnimArg D_actor_160700_8013538C = { { .index = 1 }, 60, 0, 0, 0 };

GpAnimArg D_actor_160700_801353A0 = { { .index = 1 }, 61, 0, 0, 0 };

GpAnimArg D_actor_160700_801353B4 = { { .index = 1 }, 62, 0, 0, 0 };

GpAnimArg D_actor_160700_801353C8 = { { .index = 1 }, 63, 0, 0, 0 };

GpAnimArg D_actor_160700_801353DC = { { .index = 1 }, 64, 0, 0, 0 };

GpAnimArg D_actor_160700_801353F0 = { { .index = 1 }, 65, 0, 0, 0 };

GpAnimArg D_actor_160700_80135404[6] = {
    { { .index = 1 }, 66, 0, 0, 0 },
    { { .index = 1 }, 67, 0, 0, 0 },
    { { .index = 1 }, 68, 0, 0, 0 },
    { { .index = 1 }, 69, 0, 0, 0 },
    { { .index = 1 }, 0, 0, 0, 0 },
    { { .index = 1 }, 1, 0, 0, 0 },
};

GpAnimArg D_actor_160700_8013547C = { { .index = 1 }, 2, 0, 0, 0 };

GpAnimArg D_actor_160700_80135490 = { { .index = 1 }, 3, 0, 0, 0 };

GpAnimArg D_actor_160700_801354A4 = { { .index = 1 }, 4, 0, 0, 0 };

GpAnimArg D_actor_160700_801354B8 = { { .index = 1 }, 5, 0, 0, 0 };

GpAnimArg D_actor_160700_801354CC = { { .index = 1 }, 6, 0, 0, 0 };

GpAnimArg D_actor_160700_801354E0 = { { .index = 1 }, 7, 0, 0, 0 };

GpAnimArg D_actor_160700_801354F4 = { { .index = 1 }, 8, 0, 0, 0 };

GpAnimArg D_actor_160700_80135508 = { { .index = 1 }, 9, 0, 0, 0 };

GpAnimArg D_actor_160700_8013551C = { { .index = 1 }, 10, 0, 0, 0 };

GpAnimArg D_actor_160700_80135530 = { { .index = 1 }, 11, 0, 0, 0 };

GpAnimArg D_actor_160700_80135544 = { { .index = 1 }, 12, 0, 0, 0 };

GpAnimArg D_actor_160700_80135558 = { { .index = 1 }, 13, 0, 0, 0 };

GpAnimArg D_actor_160700_8013556C = { { .index = 1 }, 14, 0, 0, 0 };

GpAnimArg D_actor_160700_80135580 = { { .index = 1 }, 15, 0, 0, 0 };

GpAnimArg D_actor_160700_80135594 = { { .index = 1 }, 16, 0, 0, 0 };

GpAnimArg D_actor_160700_801355A8 = { { .index = 1 }, 17, 0, 0, 0 };

GpAnimArg D_actor_160700_801355BC = { { .index = 1 }, 18, 0, 0, 0 };

GpAnimArg D_actor_160700_801355D0 = { { .index = 1 }, 19, 0, 0, 0 };

GpAnimArg D_actor_160700_801355E4 = { { .index = 1 }, 20, 0, 0, 0 };

GpAnimArg D_actor_160700_801355F8 = { { .index = 1 }, 21, 0, 0, 0 };

GpAnimArg D_actor_160700_8013560C = { { .index = 1 }, 22, 0, 0, 0 };

GpAnimArg D_actor_160700_80135620 = { { .index = 1 }, 23, 0, 0, 0 };

GpXformArg D_actor_160700_80135634 = { { 2250, 0, 790, 0 }, { 0, -853, 0, 0 } };

GpXformArg D_actor_160700_8013564C = { { 2000, 0, 750, 0 }, { 0, -739, 0, 0 } };

GpEvsCmd D_actor_160700_80135664[47] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_160700_801351E8.data.copy }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801351E8.data.arguments[0] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_160700_80135634 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_8013529C }, { .value = 0 } },
    { 4, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135288 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_8013547C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_8013560C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_80135620 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801352B0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801352C4 }, { .value = 0 } },
    { 4, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135288 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_80135490 }, { .value = 0 } },
    { 4, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801352D8 }, { .value = 0 } },
    { 4, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801352EC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354A4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354B8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135300 }, { .value = 0 } },
    { 4, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354CC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135314 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354E0 }, { .value = 0 } },
    { 4, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801352EC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354F4 }, { .value = 0 } },
    { 4, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354CC }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801351E8.data.arguments[0] }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_160700_80135ACC[11] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_160700_80135634 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801351E8.data.arguments[0] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354CC }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_160700_80135BD4[76] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_160700_801351E8.data.copy }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801351E8.data.arguments[0] }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_160700_80135634 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801352EC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_80135508 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_8013551C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135328 }, { .value = 0 } },
    { 4, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354CC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_80135530 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_8013533C }, { .value = 0 } },
    { 4, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135350 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_80135544 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_80135558 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_8013556C }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354CC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_80135580 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135364 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_80135594 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801355A8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135378 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354CC }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_160700_8013564C }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801353A0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801353B4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_80135530 }, { .value = 0 } },
    { 4, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_8013538C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801355BC }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354CC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354E0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801355D0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801355E4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801353C8 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354CC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801355F8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801353DC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801353F0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354CC }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801351E8.data.arguments[0] }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_160700_801362F4[12] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_160700_801351E8.data.copy }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801351E8.data.arguments[1] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354E0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801355D0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801355E4 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354CC }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_160700_80136414[10] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_160700_801351E8.data.copy }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801351E8.data.arguments[1] }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_80135530 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801355BC }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_160700_801354CC }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_actor_160700_80136504[20] = {
#include "assets/actor_160700_model_0A8DC_skeleton.inc"
};

u32 D_actor_160700_801367D4[20] = {
#include "assets/actor_160700_model_0A8DC_partVerts.inc"
};

SVECTOR D_actor_160700_80136824[390] = {
#include "assets/actor_160700_model_0A8DC_verts.inc"
};

SVECTOR D_actor_160700_80137454[407] = {
#include "assets/actor_160700_model_0A8DC_normals.inc"
};

u32 D_actor_160700_8013810C[4476] = {
#include "assets/actor_160700_model_0A8DC_stream.inc"
};

TmdSource D_actor_160700_8013C6FC = {
    0,
    24444,
    6776,
    20,
    D_actor_160700_801367D4,
    D_actor_160700_80136824,
    D_actor_160700_80137454,
    D_actor_160700_80136504,
    D_actor_160700_8013810C,
};

TmdBone D_actor_160700_8013C720[1] = {
#include "assets/actor_160700_model_0AAD8_skeleton.inc"
};

u32 D_actor_160700_8013C744[1] = {
#include "assets/actor_160700_model_0AAD8_partVerts.inc"
};

SVECTOR D_actor_160700_8013C748[14] = {
#include "assets/actor_160700_model_0AAD8_verts.inc"
};

SVECTOR D_actor_160700_8013C7B8[12] = {
#include "assets/actor_160700_model_0AAD8_normals.inc"
};

u32 D_actor_160700_8013C818[56] = {
#include "assets/actor_160700_model_0AAD8_stream.inc"
};

TmdSource D_actor_160700_8013C8F8 = {
    0,
    340,
    0,
    1,
    D_actor_160700_8013C744,
    D_actor_160700_8013C748,
    D_actor_160700_8013C7B8,
    D_actor_160700_8013C720,
    D_actor_160700_8013C818,
};

AnimationPackedPose D_actor_160700_8013C91C[2] = {
#include "assets/actor_160700_animation_0AD1C_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013C934[22] = {
#include "assets/actor_160700_animation_0AD1C_bank4.inc"
};

AnimationRecord D_actor_160700_8013C98C[98] = {
#include "assets/actor_160700_animation_0AD1C_records.inc"
};

u16 D_actor_160700_8013CB14[20] = {
#include "assets/actor_160700_animation_0AD1C_indices.inc"
};

GpAnimSet D_actor_160700_8013CB3C = {
    D_actor_160700_8013C98C,
    D_actor_160700_8013CB14,
    { NULL, D_actor_160700_8013C91C, NULL, NULL, D_actor_160700_8013C934, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013CB64[2] = {
#include "assets/actor_160700_animation_0AFEC_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013CB7C[48] = {
#include "assets/actor_160700_animation_0AFEC_bank4.inc"
};

AnimationRecord D_actor_160700_8013CC3C[106] = {
#include "assets/actor_160700_animation_0AFEC_records.inc"
};

u16 D_actor_160700_8013CDE4[20] = {
#include "assets/actor_160700_animation_0AFEC_indices.inc"
};

GpAnimSet D_actor_160700_8013CE0C = {
    D_actor_160700_8013CC3C,
    D_actor_160700_8013CDE4,
    { NULL, D_actor_160700_8013CB64, NULL, NULL, D_actor_160700_8013CB7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013CE34[30] = {
#include "assets/actor_160700_animation_0C104_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013CF9C[439] = {
#include "assets/actor_160700_animation_0C104_bank4.inc"
};

AnimationRecord D_actor_160700_8013D678[545] = {
#include "assets/actor_160700_animation_0C104_records.inc"
};

u16 D_actor_160700_8013DEFC[20] = {
#include "assets/actor_160700_animation_0C104_indices.inc"
};

GpAnimSet D_actor_160700_8013DF24 = {
    D_actor_160700_8013D678,
    D_actor_160700_8013DEFC,
    { NULL, D_actor_160700_8013CE34, NULL, NULL, D_actor_160700_8013CF9C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013DF4C[4] = {
#include "assets/actor_160700_animation_0C3F4_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013DF7C[34] = {
#include "assets/actor_160700_animation_0C3F4_bank4.inc"
};

AnimationRecord D_actor_160700_8013E004[122] = {
#include "assets/actor_160700_animation_0C3F4_records.inc"
};

u16 D_actor_160700_8013E1EC[20] = {
#include "assets/actor_160700_animation_0C3F4_indices.inc"
};

GpAnimSet D_actor_160700_8013E214 = {
    D_actor_160700_8013E004,
    D_actor_160700_8013E1EC,
    { NULL, D_actor_160700_8013DF4C, NULL, NULL, D_actor_160700_8013DF7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013E23C[3] = {
#include "assets/actor_160700_animation_0C5D0_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013E260[30] = {
#include "assets/actor_160700_animation_0C5D0_bank4.inc"
};

AnimationRecord D_actor_160700_8013E2D8[60] = {
#include "assets/actor_160700_animation_0C5D0_records.inc"
};

u16 D_actor_160700_8013E3C8[20] = {
#include "assets/actor_160700_animation_0C5D0_indices.inc"
};

GpAnimSet D_actor_160700_8013E3F0 = {
    D_actor_160700_8013E2D8,
    D_actor_160700_8013E3C8,
    { NULL, D_actor_160700_8013E23C, NULL, NULL, D_actor_160700_8013E260, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013E418[3] = {
#include "assets/actor_160700_animation_0C858_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013E43C[29] = {
#include "assets/actor_160700_animation_0C858_bank4.inc"
};

AnimationRecord D_actor_160700_8013E4B0[104] = {
#include "assets/actor_160700_animation_0C858_records.inc"
};

u16 D_actor_160700_8013E650[20] = {
#include "assets/actor_160700_animation_0C858_indices.inc"
};

GpAnimSet D_actor_160700_8013E678 = {
    D_actor_160700_8013E4B0,
    D_actor_160700_8013E650,
    { NULL, D_actor_160700_8013E418, NULL, NULL, D_actor_160700_8013E43C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013E6A0[3] = {
#include "assets/actor_160700_animation_0CA8C_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013E6C4[36] = {
#include "assets/actor_160700_animation_0CA8C_bank4.inc"
};

AnimationRecord D_actor_160700_8013E754[76] = {
#include "assets/actor_160700_animation_0CA8C_records.inc"
};

u16 D_actor_160700_8013E884[20] = {
#include "assets/actor_160700_animation_0CA8C_indices.inc"
};

GpAnimSet D_actor_160700_8013E8AC = {
    D_actor_160700_8013E754,
    D_actor_160700_8013E884,
    { NULL, D_actor_160700_8013E6A0, NULL, NULL, D_actor_160700_8013E6C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013E8D4[3] = {
#include "assets/actor_160700_animation_0CC70_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013E8F8[32] = {
#include "assets/actor_160700_animation_0CC70_bank4.inc"
};

AnimationRecord D_actor_160700_8013E978[60] = {
#include "assets/actor_160700_animation_0CC70_records.inc"
};

u16 D_actor_160700_8013EA68[20] = {
#include "assets/actor_160700_animation_0CC70_indices.inc"
};

GpAnimSet D_actor_160700_8013EA90 = {
    D_actor_160700_8013E978,
    D_actor_160700_8013EA68,
    { NULL, D_actor_160700_8013E8D4, NULL, NULL, D_actor_160700_8013E8F8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013EAB8[8] = {
#include "assets/actor_160700_animation_0D310_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013EB18[120] = {
#include "assets/actor_160700_animation_0D310_bank4.inc"
};

AnimationRecord D_actor_160700_8013ECF8[260] = {
#include "assets/actor_160700_animation_0D310_records.inc"
};

u16 D_actor_160700_8013F108[20] = {
#include "assets/actor_160700_animation_0D310_indices.inc"
};

GpAnimSet D_actor_160700_8013F130 = {
    D_actor_160700_8013ECF8,
    D_actor_160700_8013F108,
    { NULL, D_actor_160700_8013EAB8, NULL, NULL, D_actor_160700_8013EB18, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013F158[5] = {
#include "assets/actor_160700_animation_0D638_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013F194[70] = {
#include "assets/actor_160700_animation_0D638_bank4.inc"
};

AnimationRecord D_actor_160700_8013F2AC[97] = {
#include "assets/actor_160700_animation_0D638_records.inc"
};

u16 D_actor_160700_8013F430[20] = {
#include "assets/actor_160700_animation_0D638_indices.inc"
};

GpAnimSet D_actor_160700_8013F458 = {
    D_actor_160700_8013F2AC,
    D_actor_160700_8013F430,
    { NULL, D_actor_160700_8013F158, NULL, NULL, D_actor_160700_8013F194, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013F480[3] = {
#include "assets/actor_160700_animation_0D980_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013F4A4[47] = {
#include "assets/actor_160700_animation_0D980_bank4.inc"
};

AnimationRecord D_actor_160700_8013F560[134] = {
#include "assets/actor_160700_animation_0D980_records.inc"
};

u16 D_actor_160700_8013F778[20] = {
#include "assets/actor_160700_animation_0D980_indices.inc"
};

GpAnimSet D_actor_160700_8013F7A0 = {
    D_actor_160700_8013F560,
    D_actor_160700_8013F778,
    { NULL, D_actor_160700_8013F480, NULL, NULL, D_actor_160700_8013F4A4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013F7C8[3] = {
#include "assets/actor_160700_animation_0DC18_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013F7EC[38] = {
#include "assets/actor_160700_animation_0DC18_bank4.inc"
};

AnimationRecord D_actor_160700_8013F884[99] = {
#include "assets/actor_160700_animation_0DC18_records.inc"
};

u16 D_actor_160700_8013FA10[20] = {
#include "assets/actor_160700_animation_0DC18_indices.inc"
};

GpAnimSet D_actor_160700_8013FA38 = {
    D_actor_160700_8013F884,
    D_actor_160700_8013FA10,
    { NULL, D_actor_160700_8013F7C8, NULL, NULL, D_actor_160700_8013F7EC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013FA60[4] = {
#include "assets/actor_160700_animation_0DF38_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013FA90[38] = {
#include "assets/actor_160700_animation_0DF38_bank4.inc"
};

AnimationRecord D_actor_160700_8013FB28[130] = {
#include "assets/actor_160700_animation_0DF38_records.inc"
};

u16 D_actor_160700_8013FD30[20] = {
#include "assets/actor_160700_animation_0DF38_indices.inc"
};

GpAnimSet D_actor_160700_8013FD58 = {
    D_actor_160700_8013FB28,
    D_actor_160700_8013FD30,
    { NULL, D_actor_160700_8013FA60, NULL, NULL, D_actor_160700_8013FA90, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8013FD80[2] = {
#include "assets/actor_160700_animation_0E1D8_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8013FD98[54] = {
#include "assets/actor_160700_animation_0E1D8_bank4.inc"
};

AnimationRecord D_actor_160700_8013FE70[88] = {
#include "assets/actor_160700_animation_0E1D8_records.inc"
};

u16 D_actor_160700_8013FFD0[20] = {
#include "assets/actor_160700_animation_0E1D8_indices.inc"
};

GpAnimSet D_actor_160700_8013FFF8 = {
    D_actor_160700_8013FE70,
    D_actor_160700_8013FFD0,
    { NULL, D_actor_160700_8013FD80, NULL, NULL, D_actor_160700_8013FD98, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80140020[6] = {
#include "assets/actor_160700_animation_0E4E4_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80140068[61] = {
#include "assets/actor_160700_animation_0E4E4_bank4.inc"
};

AnimationRecord D_actor_160700_8014015C[96] = {
#include "assets/actor_160700_animation_0E4E4_records.inc"
};

u16 D_actor_160700_801402DC[20] = {
#include "assets/actor_160700_animation_0E4E4_indices.inc"
};

GpAnimSet D_actor_160700_80140304 = {
    D_actor_160700_8014015C,
    D_actor_160700_801402DC,
    { NULL, D_actor_160700_80140020, NULL, NULL, D_actor_160700_80140068, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_8014032C[4] = {
#include "assets/actor_160700_animation_0E8BC_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8014035C[60] = {
#include "assets/actor_160700_animation_0E8BC_bank4.inc"
};

AnimationRecord D_actor_160700_8014044C[154] = {
#include "assets/actor_160700_animation_0E8BC_records.inc"
};

u16 D_actor_160700_801406B4[20] = {
#include "assets/actor_160700_animation_0E8BC_indices.inc"
};

GpAnimSet D_actor_160700_801406DC = {
    D_actor_160700_8014044C,
    D_actor_160700_801406B4,
    { NULL, D_actor_160700_8014032C, NULL, NULL, D_actor_160700_8014035C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80140704[2] = {
#include "assets/actor_160700_animation_0EA74_bank1.inc"
};

AnimationPackedRotation D_actor_160700_8014071C[24] = {
#include "assets/actor_160700_animation_0EA74_bank4.inc"
};

AnimationRecord D_actor_160700_8014077C[60] = {
#include "assets/actor_160700_animation_0EA74_records.inc"
};

u16 D_actor_160700_8014086C[20] = {
#include "assets/actor_160700_animation_0EA74_indices.inc"
};

GpAnimSet D_actor_160700_80140894 = {
    D_actor_160700_8014077C,
    D_actor_160700_8014086C,
    { NULL, D_actor_160700_80140704, NULL, NULL, D_actor_160700_8014071C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_801408BC[3] = {
#include "assets/actor_160700_animation_0EC58_bank1.inc"
};

AnimationPackedRotation D_actor_160700_801408E0[32] = {
#include "assets/actor_160700_animation_0EC58_bank4.inc"
};

AnimationRecord D_actor_160700_80140960[60] = {
#include "assets/actor_160700_animation_0EC58_records.inc"
};

u16 D_actor_160700_80140A50[20] = {
#include "assets/actor_160700_animation_0EC58_indices.inc"
};

GpAnimSet D_actor_160700_80140A78 = {
    D_actor_160700_80140960,
    D_actor_160700_80140A50,
    { NULL, D_actor_160700_801408BC, NULL, NULL, D_actor_160700_801408E0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80140AA0[3] = {
#include "assets/actor_160700_animation_0EF34_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80140AC4[35] = {
#include "assets/actor_160700_animation_0EF34_bank4.inc"
};

AnimationRecord D_actor_160700_80140B50[119] = {
#include "assets/actor_160700_animation_0EF34_records.inc"
};

u16 D_actor_160700_80140D2C[20] = {
#include "assets/actor_160700_animation_0EF34_indices.inc"
};

GpAnimSet D_actor_160700_80140D54 = {
    D_actor_160700_80140B50,
    D_actor_160700_80140D2C,
    { NULL, D_actor_160700_80140AA0, NULL, NULL, D_actor_160700_80140AC4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80140D7C[3] = {
#include "assets/actor_160700_animation_0F118_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80140DA0[32] = {
#include "assets/actor_160700_animation_0F118_bank4.inc"
};

AnimationRecord D_actor_160700_80140E20[60] = {
#include "assets/actor_160700_animation_0F118_records.inc"
};

u16 D_actor_160700_80140F10[20] = {
#include "assets/actor_160700_animation_0F118_indices.inc"
};

GpAnimSet D_actor_160700_80140F38 = {
    D_actor_160700_80140E20,
    D_actor_160700_80140F10,
    { NULL, D_actor_160700_80140D7C, NULL, NULL, D_actor_160700_80140DA0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80140F60[2] = {
#include "assets/actor_160700_animation_0F3D0_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80140F78[56] = {
#include "assets/actor_160700_animation_0F3D0_bank4.inc"
};

AnimationRecord D_actor_160700_80141058[92] = {
#include "assets/actor_160700_animation_0F3D0_records.inc"
};

u16 D_actor_160700_801411C8[20] = {
#include "assets/actor_160700_animation_0F3D0_indices.inc"
};

GpAnimSet D_actor_160700_801411F0 = {
    D_actor_160700_80141058,
    D_actor_160700_801411C8,
    { NULL, D_actor_160700_80140F60, NULL, NULL, D_actor_160700_80140F78, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_80141218[2] = {
#include "assets/actor_160700_animation_0F65C_bank1.inc"
};

AnimationPackedRotation D_actor_160700_80141230[33] = {
#include "assets/actor_160700_animation_0F65C_bank4.inc"
};

AnimationRecord D_actor_160700_801412B4[104] = {
#include "assets/actor_160700_animation_0F65C_records.inc"
};

u16 D_actor_160700_80141454[20] = {
#include "assets/actor_160700_animation_0F65C_indices.inc"
};

GpAnimSet D_actor_160700_8014147C = {
    D_actor_160700_801412B4,
    D_actor_160700_80141454,
    { NULL, D_actor_160700_80141218, NULL, NULL, D_actor_160700_80141230, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_160700_801414A4[2] = {
#include "assets/actor_160700_animation_0F830_bank1.inc"
};

AnimationPackedRotation D_actor_160700_801414BC[23] = {
#include "assets/actor_160700_animation_0F830_bank4.inc"
};

AnimationRecord D_actor_160700_80141518[68] = {
#include "assets/actor_160700_animation_0F830_records.inc"
};

u16 D_actor_160700_80141628[20] = {
#include "assets/actor_160700_animation_0F830_indices.inc"
};

GpAnimSet D_actor_160700_80141650 = {
    D_actor_160700_80141518,
    D_actor_160700_80141628,
    { NULL, D_actor_160700_801414A4, NULL, NULL, D_actor_160700_801414BC, NULL, NULL, NULL },
};

Actor160700MessageEntry D_actor_160700_80141678[6] = {
    { 2003, { .call1 = func_actor_160700_801325F0 } },
    { 2005, { .call3 = func_actor_160700_8013265C } },
    { 2004, { .call2 = func_actor_160700_801326C0 } },
    { 2011, { .call0 = func_actor_160700_80132738 } },
    { 2013, { .call2 = func_actor_160700_80132740 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_160700_801416A8[2] = {
    { 1, 96, func_actor_160700_8013233C, { .model = &D_actor_160700_8013C6FC } },
    { 1, 192, func_actor_160700_80132808, { .model = &D_actor_160700_8013C8F8 } },
};

u8 D_actor_160700_801416C0[100] = {
    0,
    0,
    0,
    0,
    60,
    203,
    19,
    128,
    12,
    206,
    19,
    128,
    36,
    223,
    19,
    128,
    20,
    226,
    19,
    128,
    240,
    227,
    19,
    128,
    120,
    230,
    19,
    128,
    172,
    232,
    19,
    128,
    144,
    234,
    19,
    128,
    48,
    241,
    19,
    128,
    88,
    244,
    19,
    128,
    160,
    247,
    19,
    128,
    56,
    250,
    19,
    128,
    88,
    253,
    19,
    128,
    248,
    255,
    19,
    128,
    4,
    3,
    20,
    128,
    220,
    6,
    20,
    128,
    148,
    8,
    20,
    128,
    120,
    10,
    20,
    128,
    84,
    13,
    20,
    128,
    56,
    15,
    20,
    128,
    240,
    17,
    20,
    128,
    124,
    20,
    20,
    128,
    80,
    22,
    20,
    128,
    0,
    0,
    0,
    0,
};

static void func_actor_160700_80131E24(void);
static void func_actor_160700_80131E70(void);
static void func_actor_160700_80131F70(GpEnemy* enemy, Task* task);

static void func_actor_160700_80131E24(void)
{
    Task* slot;

    if (GameFlag_GetNibble(0x113) != 0) {
        slot = Gp_LookupSlot4(0);
        if (slot != 0) {
            Gp_DispatchMsgPtr(slot, 0x7D3, &D_actor_160700_801354CC, 0);
        }
    }
}

static void func_actor_160700_80131E70(void)
{
    switch (GameFlag_GetNibble(0x113)) {
        case 0:
            func_800E8634(D_actor_160700_80135664, 0, D_actor_160700_80135ACC);
            GameFlag_SetNibble(0x113, 1);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 0xC);
            break;
        case 1:
            func_800E8634(D_actor_160700_80135BD4, 0, D_actor_160700_80135ACC);
            GameFlag_SetNibble(0x113, 2);
            break;
        case 2:
            func_800E8614(D_actor_160700_801362F4, 0);
            GameFlag_SetNibble(0x113, 3);
            break;
        case 3:
            func_800E8614(D_actor_160700_80136414, 0);
            break;
    }
}

/// State-0 handler of the actor's dispatcher: allocates the work block, spawns
/// the sub-model and adopts it as a child, takes the model's texture page and
/// CLUT from the area placement the enemy's `placeKey` selects, sets up the
/// animation context on clip 1, installs the message table whose handlers are
/// the actor's script opcodes, and starts the animation.
static void func_actor_160700_80131F70(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor160600Work* work;
    Actor160600Work* mem;
    GfxCoord*        coord;
    TmdObject*       obj;
    GpEnemy*         spawned;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = (Actor160600Work*)memCalloc(0x4F8, false);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_160700_80132414;
    coord->parent                = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->flags                   = 0;
    obj->otOffset                = 1;
    work->enemy                  = enemy;
    spawned                      = Gp_SpawnEnemyFromTable(D_actor_160700_801416A8, 1, 0, enemy);
    actorTintModel(spawned->task->extra.tmd, enemy);
    Task_Reparent(task, spawned->task);
    work->pairTask  = spawned->task;
    work->st.animId = 1;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    vec.vx          = coord->workm.t[0];
    vec.vy          = coord->workm.t[1] - 0x320;
    vec.vz          = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, D_actor_160700_801416C0, obj,
                  work->rig.poses, work->rig.slots);
    work->st.state = 2;
    task->msgTable = D_actor_160700_80141678;
    func_actor_160700_80132184(task);
    task->state += 1;
}

/// The actor's animation step. State 1 reseeds the slots with `animArg` and
/// state 2 resets them, each then moving on to state 3; state 3 walks the
/// root coordinate 12 units forward per frame while clip 4 still has `travel`
/// left, switching to clip 1 when it runs out, and ticks the slots.
static void func_actor_160700_80132184(Task* task)
{
    Actor160600Work* work;
    s16              animId;

    work = (Actor160600Work*)task->work;
    if (work->st.state == 1) {
        func_actor_160700_8013258C(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_160700_80132514(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveForward(task->extra.tmd->coords, 0xC);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        func_actor_160700_801324C8(task);
        return;
    }
}

/// Two-state dispatcher, its handler table built on the stack: state 0 spawns
/// the actor, state 1 runs it. Both handlers take the task's `GpEnemy` as
/// well as the task.
void func_actor_160700_8013233C(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_160700_80131F70,
        func_actor_160700_80132390,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// State-1 handler of the actor's dispatcher: recomputes the root part's
/// world matrix, hands the position 800 units above it to the model's
/// light/colour step, then runs the animation step and draws the shadow.
static void func_actor_160700_80132390(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 800;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_160700_80132184(task);
    func_actor_160700_8013243C(task);
}

/// Exit callback: hands the task's `GpEnemy` back to `Gp_DestroyEnemy`.
static void func_actor_160700_80132414(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Draws the actor's ground shadow under its root part, unless the model's
/// `flags` bit 0x80 (hidden) is set or it has no buffer. The position is the
/// root part's world translation, staged on the scratchpad stack.
static void func_actor_160700_8013243C(Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
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

/// Ticks animation slots 1..0x13.
static void func_actor_160700_801324C8(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Resets animation slots 1..0x13 to clip `animId` and records it as the
/// applied clip.
static void func_actor_160700_80132514(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Reseeds animation slots 1..0x13 with clip `animId` and argument `animArg`,
/// and records the clip as the applied one.
static void func_actor_160700_8013258C(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Script opcode: plays clip `args->field_4` (ids from 0x19 up are refused
/// with -1). With `args->field_8` set the slots are reseeded with
/// `args->field_C`, otherwise they are reset; the step body then applies it
/// straight away.
s32 func_actor_160700_801325F0(Task* task, s32 arg1, GpAnimArg* args)
{
    Actor160600Work* work;

    work = (Actor160600Work*)task->work;
    if (args->field_4 < 0x19) {
        work->st.animId = args->field_4;
        if (args->field_8 != 0) {
            work->st.state = 1;
            work->animArg  = args->field_C;
        } else {
            work->st.state = 2;
        }
        work->st.field_6 = 0;
        func_actor_160700_80132184(task);
        return 0;
    }
    return -1;
}

/// Script opcode: sets the visibility flags of the actor's model and of the
/// model of the enemy spawned alongside it. `flags` bit 0 makes both visible
/// (`TmdObject::flags` 0) and its absence hides them (0x80); bit 1 also sets
/// 0x4. The middle argument is the one every opcode of the table receives.
s32 func_actor_160700_8013265C(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = task->extra.tmd;
    other = ((Actor160600Work*)task->work)->pairTask->extra.tmd;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = 0x80;
        other->flags = 0x80;
    }

    if (flags & 2) {
        self->flags  |= 4;
        other->flags |= 4;
    }
    return 0;
}

/// Script opcode: yaws the actor's root coordinate to `placement->rot.vy`,
/// caching the yaw in the work block, and moves it to `placement->pos`.
s32 func_actor_160700_801326C0(Task* task, s32 arg1, GpXformArg* placement)
{
    GfxCoord*        coord;
    Actor160600Work* work;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor160600Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Script opcode (message 0x7DB) that does nothing.
s32 func_actor_160700_80132738(void)
{
    return 0;
}

/// Script opcode "walk to": turns the actor's root coordinate to face
/// `target` horizontally, caching the yaw, and stores the horizontal distance
/// in steps of 12 as `travel` for the step body to walk off.
s32 func_actor_160700_80132740(Task* task, s32 arg1, GpXformArg* target)
{
    GfxCoord*        coord;
    Actor160600Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor160600Work*)task->work;
    dx           = target->pos.vx - coord->coord.t[0];
    dz           = target->pos.vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}

/// Handler of the sub-model the actor spawns and adopts as its child. On the
/// first frame it points the sub-model's light and colour matrices at the
/// parent's, makes it visible with `flags` 0 and parents its root coordinate
/// to part 4 of the parent's model; every frame it clears the coordinate's
/// `composeStamp` so it is recomputed from that part.
void func_actor_160700_80132808(Task* task)
{
    char             pad[0x10];
    Task*            parent = task->parent;
    TmdObject*       obj    = task->extra.tmd;
    GfxCoord*        coord  = obj->coords;
    GfxCoord*        sub    = &parent->extra.tmd->coords[4];
    Actor160600Work* work   = (Actor160600Work*)parent->work;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            obj->lightMtx       = &work->light;
            obj->flags          = 0;
            obj->colorMtx       = &work->color;
            coord->parent       = sub;
            task->state++;
            break;
        case 1:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
