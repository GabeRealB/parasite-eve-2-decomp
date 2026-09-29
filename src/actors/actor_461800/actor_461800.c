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
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/neo_ark_r31.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s16 D_actor_461800_8014389C[2];
// Scalar symbol view preserves the original byte/halfword address formation.
extern s16 D_actor_461800_8014389C_value __asm__("D_actor_461800_8014389C");

extern Actor461800Work* D_actor_461800_80143894;

/// The task the first variant's work block above belongs to, published by
/// `func_actor_461800_80132390` alongside it.
extern Task* D_actor_461800_80143898;

extern Actor151000Work* D_actor_461800_801438A0;

/// The second variant's task, published by its spawn routine
/// `func_actor_461800_8013307C` so the handlers can reach its model.
extern Task* D_actor_461800_801438A4;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern Task* D_actor_461800_80133EB8;

extern Task*    D_actor_461800_80133EB4;
extern TaskDesc D_actor_461800_80133EBC[];

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call1)(Task*, s32, GpCmdArg*, s32);
        s32 (*call2)(Task*, s32, GpXformArg*);
        s32 (*call3)(Task*, s32, VECTOR*, s32);
        s32 (*call4)(Task*, s32, s32);
    } handler;
} Actor461800MessageEntry;
STATIC_ASSERT_SIZEOF(Actor461800MessageEntry, 8);

extern Actor461800MessageEntry D_actor_461800_80139F5C[6];
extern TaskDesc                D_actor_461800_80139F8C[];
extern GpAnimSet*              D_actor_461800_80139FB0[6];

extern s32 D_actor_461800_80143884;
extern s32 D_actor_461800_80143888;
extern s32 D_actor_461800_8014388C;
extern s32 D_actor_461800_80143890;

extern Actor461800MessageEntry D_actor_461800_801437BC[6];
extern GpAnimSet*              D_actor_461800_801437F8[35];

/// Reset argument the first variant forwards to every reseeded slot.
extern s16 D_actor_461800_80139F58;

/// Reset argument the second variant forwards to every reseeded slot.
extern s16 D_actor_461800_801437B8;

/// Approach mode the last `func_actor_461800_80132F44` call selected.

/// Approach mode the last `func_actor_461800_80133A3C` call selected.
extern s16 D_actor_461800_801438A8;

static void func_actor_461800_80132660(Task* task);
static void func_actor_461800_80132A0C(GpEnemy* enemy, Task* task);
static void func_actor_461800_80132A90(Task* task);
static void func_actor_461800_80132AD8(Task* task);
static void func_actor_461800_80132C28(void);
static void func_actor_461800_80132C74(void);
static void func_actor_461800_80132D04(void);
static void func_actor_461800_801331E4(Task* task);
static void func_actor_461800_801335B0(GpEnemy* enemy, Task* task);
static void func_actor_461800_80133634(Task* task);
static void func_actor_461800_8013365C(Task* task);
static void func_actor_461800_80133724(void);
static void func_actor_461800_80133770(void);
static void func_actor_461800_8013380C(void);
static void func_actor_461800_80133B98(Task* task);

s32  func_actor_461800_80132D84(Task*, s32, AnimationPlayRequest*, s32);
s32  func_actor_461800_80132E14(Task*, s32, s32);
s32  func_actor_461800_80132EA4(Task*, s32, GpXformArg*);
s32  func_actor_461800_80132F20(Task*, s32, GpCmdArg*, s32);
s32  func_actor_461800_80132F44(Task*, s32, VECTOR*, s32);
s32  func_actor_461800_80133898(Task*, s32, AnimationPlayRequest*, s32);
s32  func_actor_461800_80133928(Task*, s32, s32);
s32  func_actor_461800_80133970(Task*, s32, GpXformArg*);
s32  func_actor_461800_801339EC(Task*, s32, GpCmdArg*, s32);
s32  func_actor_461800_80133A3C(Task*, s32, VECTOR*, s32);
void func_actor_461800_801329B0(Task*);
void func_actor_461800_80132B74(Task*);
void func_actor_461800_80133554(Task*);

void func_actor_461800_80131E38(Task*);
void func_actor_461800_80132048(Task*);
void func_actor_461800_801321DC(s32);
void func_actor_461800_8013223C(s32);
void func_actor_461800_8013229C(void);

AnimationPackedPose D_actor_461800_80133C34[3] = {
#include "assets/actor_461800_animation_0206C_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80133C58[27] = {
#include "assets/actor_461800_animation_0206C_bank4.inc"
};

AnimationRecord D_actor_461800_80133CC4[104] = {
#include "assets/actor_461800_animation_0206C_records.inc"
};

u16 D_actor_461800_80133E64[20] = {
#include "assets/actor_461800_animation_0206C_indices.inc"
};

GpAnimSet D_actor_461800_80133E8C = {
    D_actor_461800_80133CC4,
    D_actor_461800_80133E64,
    { NULL, D_actor_461800_80133C34, NULL, NULL, D_actor_461800_80133C58, NULL, NULL, NULL },
};

Task* D_actor_461800_80133EB4 = NULL;

Task* D_actor_461800_80133EB8 = NULL;

TaskDesc D_actor_461800_80133EBC[2] = {
    { 0, 32, func_actor_461800_80132048, { .model = NULL } },
    { 0, 32, func_actor_461800_80131E38, { .model = NULL } },
};

GpXformArg D_actor_461800_80133ED4 = { { 7710, 980, 6290, 0 }, { 0, -1479, 0, 0 } };

GpXformArg D_actor_461800_80133EEC = { { 7030, 980, 7530, 0 }, { 0, -1820, 0, 0 } };

AnimationPlayRequest D_actor_461800_80133F04 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F18 = { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F2C = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F40 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F54 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F68 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpAnimSet* D_actor_461800_80133F7C[1] = {
    &D_actor_461800_80133E8C,
};

GpCopyArg D_actor_461800_80133F80 = { { .sets = D_actor_461800_80133F7C }, 1 };

GpOverlayIds D_actor_461800_80133F88 = { 6, 18, 11 };

GpEvsCmd D_actor_461800_80133F90[52] = {
    { 12, { .overlays = &D_actor_461800_80133F88 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 31, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_461800_80133F80 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_461800_80133ED4 }, { .value = 0 } },
    { 13, { .callback = func_actor_461800_801321DC }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_461800_8013223C }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_461800_80133EEC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_461800_80133F40 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_461800_80133F04 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 30, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_461800_801321DC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_461800_8013223C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_461800_80133F54 }, { .value = 0 } },
    { 4, { .value = 41 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_461800_80133F40 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_461800_80133F68 }, { .value = 0 } },
    { 4, { .value = 55 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_461800_80133F40 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_461800_80133F54 }, { .value = 0 } },
    { 4, { .value = 41 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_461800_80133F40 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_461800_801321DC }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_461800_8013223C }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_461800_80133F18 }, { .value = 0 } },
    { 13, { .callback = func_actor_461800_801321DC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_461800_801321DC }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_461800_8013229C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 46, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0xF4240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_461800_80134470[8] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_461800_8013229C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0xF4240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_actor_461800_80134530[20] = {
#include "assets/actor_461800_model_07230_skeleton.inc"
};

u32 D_actor_461800_80134800[20] = {
#include "assets/actor_461800_model_07230_partVerts.inc"
};

SVECTOR D_actor_461800_80134850[300] = {
#include "assets/actor_461800_model_07230_verts.inc"
};

SVECTOR D_actor_461800_801351B0[298] = {
#include "assets/actor_461800_model_07230_normals.inc"
};

u32 D_actor_461800_80135B00[3412] = {
#include "assets/actor_461800_model_07230_stream.inc"
};

TmdSource D_actor_461800_80139050 = {
    0,
    18224,
    5696,
    20,
    D_actor_461800_80134800,
    D_actor_461800_80134850,
    D_actor_461800_801351B0,
    D_actor_461800_80134530,
    D_actor_461800_80135B00,
};

TmdBone D_actor_461800_80139074[1] = {
#include "assets/actor_461800_model_075F8_skeleton.inc"
};

u32 D_actor_461800_80139098[1] = {
#include "assets/actor_461800_model_075F8_partVerts.inc"
};

SVECTOR D_actor_461800_8013909C[18] = {
#include "assets/actor_461800_model_075F8_verts.inc"
};

SVECTOR D_actor_461800_8013912C[18] = {
#include "assets/actor_461800_model_075F8_normals.inc"
};

u32 D_actor_461800_801391BC[151] = {
#include "assets/actor_461800_model_075F8_stream.inc"
};

TmdSource D_actor_461800_80139418 = {
    0,
    1000,
    0,
    1,
    D_actor_461800_80139098,
    D_actor_461800_8013909C,
    D_actor_461800_8013912C,
    D_actor_461800_80139074,
    D_actor_461800_801391BC,
};

TmdBone D_actor_461800_8013943C[1] = {
#include "assets/actor_461800_model_07AE8_skeleton.inc"
};

u32 D_actor_461800_80139460[1] = {
#include "assets/actor_461800_model_07AE8_partVerts.inc"
};

SVECTOR D_actor_461800_80139464[27] = {
#include "assets/actor_461800_model_07AE8_verts.inc"
};

SVECTOR D_actor_461800_8013953C[27] = {
#include "assets/actor_461800_model_07AE8_normals.inc"
};

u32 D_actor_461800_80139614[189] = {
#include "assets/actor_461800_model_07AE8_stream.inc"
};

TmdSource D_actor_461800_80139908 = {
    0,
    1328,
    0,
    1,
    D_actor_461800_80139460,
    D_actor_461800_80139464,
    D_actor_461800_8013953C,
    D_actor_461800_8013943C,
    D_actor_461800_80139614,
};

AnimationPackedPose D_actor_461800_8013992C[2] = {
#include "assets/actor_461800_animation_07D34_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80139944[26] = {
#include "assets/actor_461800_animation_07D34_bank4.inc"
};

AnimationRecord D_actor_461800_801399AC[96] = {
#include "assets/actor_461800_animation_07D34_records.inc"
};

u16 D_actor_461800_80139B2C[20] = {
#include "assets/actor_461800_animation_07D34_indices.inc"
};

GpAnimSet D_actor_461800_80139B54 = {
    D_actor_461800_801399AC,
    D_actor_461800_80139B2C,
    { NULL, D_actor_461800_8013992C, NULL, NULL, D_actor_461800_80139944, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80139B7C[2] = {
#include "assets/actor_461800_animation_07F58_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80139B94[31] = {
#include "assets/actor_461800_animation_07F58_bank4.inc"
};

AnimationRecord D_actor_461800_80139C10[80] = {
#include "assets/actor_461800_animation_07F58_records.inc"
};

u16 D_actor_461800_80139D50[20] = {
#include "assets/actor_461800_animation_07F58_indices.inc"
};

GpAnimSet D_actor_461800_80139D78 = {
    D_actor_461800_80139C10,
    D_actor_461800_80139D50,
    { NULL, D_actor_461800_80139B7C, NULL, NULL, D_actor_461800_80139B94, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80139DA0[2] = {
#include "assets/actor_461800_animation_08110_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80139DB8[20] = {
#include "assets/actor_461800_animation_08110_bank4.inc"
};

AnimationRecord D_actor_461800_80139E08[64] = {
#include "assets/actor_461800_animation_08110_records.inc"
};

u16 D_actor_461800_80139F08[20] = {
#include "assets/actor_461800_animation_08110_indices.inc"
};

GpAnimSet D_actor_461800_80139F30 = {
    D_actor_461800_80139E08,
    D_actor_461800_80139F08,
    { NULL, D_actor_461800_80139DA0, NULL, NULL, D_actor_461800_80139DB8, NULL, NULL, NULL },
};

s16 D_actor_461800_80139F58 = 8;

Actor461800MessageEntry D_actor_461800_80139F5C[6] = {
    { 2003, { .call0 = func_actor_461800_80132D84 } },
    { 2005, { .call4 = func_actor_461800_80132E14 } },
    { 2004, { .call2 = func_actor_461800_80132EA4 } },
    { 2011, { .call1 = func_actor_461800_80132F20 } },
    { 2013, { .call3 = func_actor_461800_80132F44 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_461800_80139F8C[3] = {
    { 1, 192, func_actor_461800_801329B0, { .model = &D_actor_461800_80139050 } },
    { 1, 192, func_actor_461800_80132B74, { .model = &D_actor_461800_80139908 } },
    { 1, 192, func_actor_461800_80132B74, { .model = &D_actor_461800_80139418 } },
};

GpAnimSet* D_actor_461800_80139FB0[6] = {
    NULL,
    &D_actor_461800_80139B54,
    &D_actor_461800_80139D78,
    &D_actor_461800_80139F30,
    NULL,
    NULL,
};

TmdBone D_actor_461800_80139FC8[19] = {
#include "assets/actor_461800_model_0D95C_skeleton.inc"
};

u32 D_actor_461800_8013A274[19] = {
#include "assets/actor_461800_model_0D95C_partVerts.inc"
};

SVECTOR D_actor_461800_8013A2C0[365] = {
#include "assets/actor_461800_model_0D95C_verts.inc"
};

SVECTOR D_actor_461800_8013AE28[385] = {
#include "assets/actor_461800_model_0D95C_normals.inc"
};

u32 D_actor_461800_8013BA30[3923] = {
#include "assets/actor_461800_model_0D95C_stream.inc"
};

TmdSource D_actor_461800_8013F77C = {
    0,
    21760,
    5992,
    19,
    D_actor_461800_8013A274,
    D_actor_461800_8013A2C0,
    D_actor_461800_8013AE28,
    D_actor_461800_80139FC8,
    D_actor_461800_8013BA30,
};

AnimationPackedPose D_actor_461800_8013F7A0[2] = {
#include "assets/actor_461800_animation_0DB6C_bank1.inc"
};

AnimationPackedRotation D_actor_461800_8013F7B8[23] = {
#include "assets/actor_461800_animation_0DB6C_bank4.inc"
};

AnimationRecord D_actor_461800_8013F814[84] = {
#include "assets/actor_461800_animation_0DB6C_records.inc"
};

u16 D_actor_461800_8013F964[20] = {
#include "assets/actor_461800_animation_0DB6C_indices.inc"
};

GpAnimSet D_actor_461800_8013F98C = {
    D_actor_461800_8013F814,
    D_actor_461800_8013F964,
    { NULL, D_actor_461800_8013F7A0, NULL, NULL, D_actor_461800_8013F7B8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_8013F9B4[7] = {
#include "assets/actor_461800_animation_0DF64_bank1.inc"
};

AnimationPackedRotation D_actor_461800_8013FA08[75] = {
#include "assets/actor_461800_animation_0DF64_bank4.inc"
};

AnimationRecord D_actor_461800_8013FB34[138] = {
#include "assets/actor_461800_animation_0DF64_records.inc"
};

u16 D_actor_461800_8013FD5C[20] = {
#include "assets/actor_461800_animation_0DF64_indices.inc"
};

GpAnimSet D_actor_461800_8013FD84 = {
    D_actor_461800_8013FB34,
    D_actor_461800_8013FD5C,
    { NULL, D_actor_461800_8013F9B4, NULL, NULL, D_actor_461800_8013FA08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_8013FDAC[22] = {
#include "assets/actor_461800_animation_0EB48_bank1.inc"
};

AnimationPackedRotation D_actor_461800_8013FEB4[298] = {
#include "assets/actor_461800_animation_0EB48_bank4.inc"
};

AnimationRecord D_actor_461800_8014035C[377] = {
#include "assets/actor_461800_animation_0EB48_records.inc"
};

u16 D_actor_461800_80140940[20] = {
#include "assets/actor_461800_animation_0EB48_indices.inc"
};

GpAnimSet D_actor_461800_80140968 = {
    D_actor_461800_8014035C,
    D_actor_461800_80140940,
    { NULL, D_actor_461800_8013FDAC, NULL, NULL, D_actor_461800_8013FEB4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80140990[4] = {
#include "assets/actor_461800_animation_0EDB0_bank1.inc"
};

AnimationPackedRotation D_actor_461800_801409C0[48] = {
#include "assets/actor_461800_animation_0EDB0_bank4.inc"
};

AnimationRecord D_actor_461800_80140A80[74] = {
#include "assets/actor_461800_animation_0EDB0_records.inc"
};

u16 D_actor_461800_80140BA8[20] = {
#include "assets/actor_461800_animation_0EDB0_indices.inc"
};

GpAnimSet D_actor_461800_80140BD0 = {
    D_actor_461800_80140A80,
    D_actor_461800_80140BA8,
    { NULL, D_actor_461800_80140990, NULL, NULL, D_actor_461800_801409C0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80140BF8[5] = {
#include "assets/actor_461800_animation_0F1B8_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80140C34[76] = {
#include "assets/actor_461800_animation_0F1B8_bank4.inc"
};

AnimationRecord D_actor_461800_80140D64[147] = {
#include "assets/actor_461800_animation_0F1B8_records.inc"
};

u16 D_actor_461800_80140FB0[20] = {
#include "assets/actor_461800_animation_0F1B8_indices.inc"
};

GpAnimSet D_actor_461800_80140FD8 = {
    D_actor_461800_80140D64,
    D_actor_461800_80140FB0,
    { NULL, D_actor_461800_80140BF8, NULL, NULL, D_actor_461800_80140C34, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80141000[5] = {
#include "assets/actor_461800_animation_0F5C4_bank1.inc"
};

AnimationPackedRotation D_actor_461800_8014103C[89] = {
#include "assets/actor_461800_animation_0F5C4_bank4.inc"
};

AnimationRecord D_actor_461800_801411A0[135] = {
#include "assets/actor_461800_animation_0F5C4_records.inc"
};

u16 D_actor_461800_801413BC[20] = {
#include "assets/actor_461800_animation_0F5C4_indices.inc"
};

GpAnimSet D_actor_461800_801413E4 = {
    D_actor_461800_801411A0,
    D_actor_461800_801413BC,
    { NULL, D_actor_461800_80141000, NULL, NULL, D_actor_461800_8014103C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_8014140C[4] = {
#include "assets/actor_461800_animation_0F8D4_bank1.inc"
};

AnimationPackedRotation D_actor_461800_8014143C[59] = {
#include "assets/actor_461800_animation_0F8D4_bank4.inc"
};

AnimationRecord D_actor_461800_80141528[105] = {
#include "assets/actor_461800_animation_0F8D4_records.inc"
};

u16 D_actor_461800_801416CC[20] = {
#include "assets/actor_461800_animation_0F8D4_indices.inc"
};

GpAnimSet D_actor_461800_801416F4 = {
    D_actor_461800_80141528,
    D_actor_461800_801416CC,
    { NULL, D_actor_461800_8014140C, NULL, NULL, D_actor_461800_8014143C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_8014171C[2] = {
#include "assets/actor_461800_animation_0FB2C_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80141734[30] = {
#include "assets/actor_461800_animation_0FB2C_bank4.inc"
};

AnimationRecord D_actor_461800_801417AC[94] = {
#include "assets/actor_461800_animation_0FB2C_records.inc"
};

u16 D_actor_461800_80141924[20] = {
#include "assets/actor_461800_animation_0FB2C_indices.inc"
};

GpAnimSet D_actor_461800_8014194C = {
    D_actor_461800_801417AC,
    D_actor_461800_80141924,
    { NULL, D_actor_461800_8014171C, NULL, NULL, D_actor_461800_80141734, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80141974[2] = {
#include "assets/actor_461800_animation_0FDBC_bank1.inc"
};

AnimationPackedRotation D_actor_461800_8014198C[48] = {
#include "assets/actor_461800_animation_0FDBC_bank4.inc"
};

AnimationRecord D_actor_461800_80141A4C[90] = {
#include "assets/actor_461800_animation_0FDBC_records.inc"
};

u16 D_actor_461800_80141BB4[20] = {
#include "assets/actor_461800_animation_0FDBC_indices.inc"
};

GpAnimSet D_actor_461800_80141BDC = {
    D_actor_461800_80141A4C,
    D_actor_461800_80141BB4,
    { NULL, D_actor_461800_80141974, NULL, NULL, D_actor_461800_8014198C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80141C04[5] = {
#include "assets/actor_461800_animation_101A4_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80141C40[59] = {
#include "assets/actor_461800_animation_101A4_bank4.inc"
};

AnimationRecord D_actor_461800_80141D2C[156] = {
#include "assets/actor_461800_animation_101A4_records.inc"
};

u16 D_actor_461800_80141F9C[20] = {
#include "assets/actor_461800_animation_101A4_indices.inc"
};

GpAnimSet D_actor_461800_80141FC4 = {
    D_actor_461800_80141D2C,
    D_actor_461800_80141F9C,
    { NULL, D_actor_461800_80141C04, NULL, NULL, D_actor_461800_80141C40, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80141FEC[3] = {
#include "assets/actor_461800_animation_10454_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80142010[29] = {
#include "assets/actor_461800_animation_10454_bank4.inc"
};

AnimationRecord D_actor_461800_80142084[114] = {
#include "assets/actor_461800_animation_10454_records.inc"
};

u16 D_actor_461800_8014224C[20] = {
#include "assets/actor_461800_animation_10454_indices.inc"
};

GpAnimSet D_actor_461800_80142274 = {
    D_actor_461800_80142084,
    D_actor_461800_8014224C,
    { NULL, D_actor_461800_80141FEC, NULL, NULL, D_actor_461800_80142010, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_8014229C[2] = {
#include "assets/actor_461800_animation_10610_bank1.inc"
};

AnimationPackedRotation D_actor_461800_801422B4[20] = {
#include "assets/actor_461800_animation_10610_bank4.inc"
};

AnimationRecord D_actor_461800_80142304[65] = {
#include "assets/actor_461800_animation_10610_records.inc"
};

u16 D_actor_461800_80142408[20] = {
#include "assets/actor_461800_animation_10610_indices.inc"
};

GpAnimSet D_actor_461800_80142430 = {
    D_actor_461800_80142304,
    D_actor_461800_80142408,
    { NULL, D_actor_461800_8014229C, NULL, NULL, D_actor_461800_801422B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80142458[3] = {
#include "assets/actor_461800_animation_109F8_bank1.inc"
};

AnimationPackedRotation D_actor_461800_8014247C[65] = {
#include "assets/actor_461800_animation_109F8_bank4.inc"
};

AnimationRecord D_actor_461800_80142580[156] = {
#include "assets/actor_461800_animation_109F8_records.inc"
};

u16 D_actor_461800_801427F0[20] = {
#include "assets/actor_461800_animation_109F8_indices.inc"
};

GpAnimSet D_actor_461800_80142818 = {
    D_actor_461800_80142580,
    D_actor_461800_801427F0,
    { NULL, D_actor_461800_80142458, NULL, NULL, D_actor_461800_8014247C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_80142840[18] = {
#include "assets/actor_461800_animation_113A8_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80142918[232] = {
#include "assets/actor_461800_animation_113A8_bank4.inc"
};

AnimationRecord D_actor_461800_80142CB8[314] = {
#include "assets/actor_461800_animation_113A8_records.inc"
};

u16 D_actor_461800_801431A0[20] = {
#include "assets/actor_461800_animation_113A8_indices.inc"
};

GpAnimSet D_actor_461800_801431C8 = {
    D_actor_461800_80142CB8,
    D_actor_461800_801431A0,
    { NULL, D_actor_461800_80142840, NULL, NULL, D_actor_461800_80142918, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_801431F0[4] = {
#include "assets/actor_461800_animation_11724_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80143220[68] = {
#include "assets/actor_461800_animation_11724_bank4.inc"
};

AnimationRecord D_actor_461800_80143330[123] = {
#include "assets/actor_461800_animation_11724_records.inc"
};

u16 D_actor_461800_8014351C[20] = {
#include "assets/actor_461800_animation_11724_indices.inc"
};

GpAnimSet D_actor_461800_80143544 = {
    D_actor_461800_80143330,
    D_actor_461800_8014351C,
    { NULL, D_actor_461800_801431F0, NULL, NULL, D_actor_461800_80143220, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_461800_8014356C[2] = {
#include "assets/actor_461800_animation_11970_bank1.inc"
};

AnimationPackedRotation D_actor_461800_80143584[27] = {
#include "assets/actor_461800_animation_11970_bank4.inc"
};

AnimationRecord D_actor_461800_801435F0[94] = {
#include "assets/actor_461800_animation_11970_records.inc"
};

u16 D_actor_461800_80143768[20] = {
#include "assets/actor_461800_animation_11970_indices.inc"
};

GpAnimSet D_actor_461800_80143790 = {
    D_actor_461800_801435F0,
    D_actor_461800_80143768,
    { NULL, D_actor_461800_8014356C, NULL, NULL, D_actor_461800_80143584, NULL, NULL, NULL },
};

s16 D_actor_461800_801437B8 = 8;

Actor461800MessageEntry D_actor_461800_801437BC[6] = {
    { 2003, { .call0 = func_actor_461800_80133898 } },
    { 2005, { .call4 = func_actor_461800_80133928 } },
    { 2004, { .call2 = func_actor_461800_80133970 } },
    { 2011, { .call1 = func_actor_461800_801339EC } },
    { 2013, { .call3 = func_actor_461800_80133A3C } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_461800_801437EC = { 1, 192, func_actor_461800_80133554, { .model = &D_actor_461800_8013F77C } };

GpAnimSet* D_actor_461800_801437F8[35] = {
    NULL,
    &D_actor_461800_80140BD0,
    &D_actor_461800_80140FD8,
    &D_actor_461800_801413E4,
    &D_actor_461800_801416F4,
    &D_actor_461800_8014194C,
    &D_actor_461800_80141BDC,
    &D_actor_461800_80141FC4,
    &D_actor_461800_80142274,
    NULL,
    &D_actor_461800_80142430,
    NULL,
    NULL,
    &D_actor_461800_8013F98C,
    &D_actor_461800_8013FD84,
    &D_actor_461800_80140968,
    &D_actor_461800_80142818,
    &D_actor_461800_801431C8,
    &D_actor_461800_80143790,
    &D_actor_461800_80143544,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

s32 D_actor_461800_80143884 = 0;

s32 D_actor_461800_80143888 = 0;

s32 D_actor_461800_8014388C = 0;

s32 D_actor_461800_80143890 = 0;

Actor461800Work* D_actor_461800_80143894 = NULL;

Task* D_actor_461800_80143898 = NULL;

s16 D_actor_461800_8014389C[2] = {
    0,
    -0x3658,
};

Actor151000Work* D_actor_461800_801438A0;

Task* D_actor_461800_801438A4;

s16 D_actor_461800_801438A8;

static void func_actor_461800_80132390(GpEnemy* enemy, Task* task);
static void func_actor_461800_8013307C(GpEnemy* enemy, Task* task);

void func_actor_461800_80131E38(Task* task)
{
    switch (task->state) {
        case 0:
            D_actor_461800_80143884 = 0x10;
            break;
        case 1:
            if (D_actor_461800_80143888 != 0) {
                if (D_actor_461800_80143884 < 0x10) {
                    D_actor_461800_80143884++;
                } else {
                    D_actor_461800_80143888 = 0;
                }
            } else if (D_actor_461800_80143884 > 0) {
                D_actor_461800_80143884--;
            } else {
                D_actor_461800_80143888 = 1;
            }
            if (D_actor_461800_8014388C != 0) {
                if (D_actor_461800_80143890 < 0x13) {
                    D_actor_461800_80143890++;
                } else {
                    D_actor_461800_8014388C = 0;
                }
            } else if (D_actor_461800_80143890 > 0) {
                D_actor_461800_80143890--;
            } else {
                D_actor_461800_8014388C = 1;
            }
            break;
        case 2:
            if (D_actor_461800_80143884 < 0x10) {
                D_actor_461800_80143884++;
            }
            if (D_actor_461800_80143890 < 0x13) {
                D_actor_461800_80143890++;
            }
            if (D_actor_461800_80143884 == 0x10 && D_actor_461800_80143890 == 0x13) {
                task->state = 3;
            }
            break;
        case 3:
            if (!(task->killCountdown & 3)) {
                if (D_actor_461800_80143884 > 0) {
                    D_actor_461800_80143884--;
                }
                if (D_actor_461800_80143890 > 0) {
                    D_actor_461800_80143890--;
                }
            }
            break;
        case 4:
            displaySetShakeY(0);
            D_neo_ark_r31_8017DC54 = -1;
            return;
    }
    task->killCountdown++;
    D_neo_ark_r31_8017DC54 = D_actor_461800_80143884 / 3 + 3;
}

/// Full-screen fade overlay: `Task::state` picks the ramp (0 snaps it to 90,
/// 1 clears it, 2 counts down, 3 counts up) held in `Task::killCountdown`, which
/// then scales a grey semi-transparent `TILE` linked with its `DR_TPAGE` into
/// OT slot 5.
void func_actor_461800_80132048(Task* task)
{
    TILE*     tile;
    DR_TPAGE* dr;
    u8        c;

    switch (task->state) {
        case 1:
            task->killCountdown = 0;
            break;
        case 3:
            if (task->killCountdown < 90) {
                task->killCountdown++;
            }
            break;
        case 2:
            if (task->killCountdown > 0) {
                task->killCountdown--;
            }
            break;
        case 0:
            task->killCountdown = 90;
            break;
    }
    tile           = (TILE*)gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    c              = (task->killCountdown * 0xFF) / 90;
    setlen(tile, 3);
    setcode(tile, 0x62);
    tile->x0 = -0xA0;
    tile->y0 = -0x80;
    tile->w  = 0x140;
    tile->h  = 0x100;
    tile->r0 = c;
    tile->g0 = c;
    tile->b0 = c;
    addPrim(gGpuCurrentOt + 5, tile);

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000240;
    addPrim(gGpuCurrentOt + 5, dr);
}

void func_actor_461800_801321DC(s32 arg0)
{
    if (arg0 < 0) {
        if (D_actor_461800_80133EB4 == NULL) {
            D_actor_461800_80133EB4 = Task_SpawnFromTable(D_actor_461800_80133EBC, 0, 0, 0);
        }
    } else {
        D_actor_461800_80133EB4->state = arg0;
    }
}

void func_actor_461800_8013223C(s32 arg0)
{
    if (arg0 < 0) {
        if (D_actor_461800_80133EB8 == NULL) {
            D_actor_461800_80133EB8 = Task_SpawnFromTable(D_actor_461800_80133EBC, 1, 0, 0);
        }
    } else {
        D_actor_461800_80133EB8->state = arg0;
    }
}

/// Exit path taken when the player leaves through this actor: two flag awards
/// first, then one of two endings depending on whether the two event flags have
/// been seen. With neither seen the session bails out (`field_128` / `field_12E`
/// are the stage-load sentinels); otherwise the save header is primed and the
/// boot loader started, with the stream RNG restored behind it. Skipped whole
/// when `Mc_SaveData[0].state.demoScene` (the current screen id) is 9.
void func_actor_461800_8013229C(void)
{
    if (Mc_SaveData[0].state.demoScene != 9) {
        if (GameFlag_GetNibble(0xEA) == 2) {
            Gp_SetCollectedBit(0x130);
        }
        if (GameFlag_GetNibble(0x113) != 0) {
            Gp_SetCollectedBit(0x12F);
        }
        if (GameFlag_GetNibble(0x112) == 0 && GameFlag_GetNibble(0x113) == 0) {
            gGameSession->restartMode = 0xFF;
            gGameSession->field_12E   = 0xF;
            return;
        }
        Mc_SaveData[0].state.at4.loc.stage = 4;
        Mc_SaveData[0].state.at4.loc.area  = 0x24;
        Mc_SaveData[0].state.at4.loc.warp  = 1;
        Mc_SaveData[0].state.at4.loc.room  = 1;
        gDisplayState.spriteVariant        = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Fs_BeginBootLoad((u8*)&Mc_SaveData[0].state.at4.loc, 0);
        Gp_RestoreStreamRng();
    }
}

/// Spawn tick of the first actor variant: allocates the work block, hangs the
/// model off the view, seeds the animation context and starts the two helper
/// tasks. Each helper takes its texture page and CLUT row from the nested area
/// record the actor's spawn index selects, and is streamed twice once its aux
/// buffer exists.
static void func_actor_461800_80132390(GpEnemy* enemy, Task* task)
{
    VECTOR     vec;
    GfxCoord*  coord;
    TmdObject* obj;
    Task*      spawned1;
    Task*      spawned2;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    task->work = (TaskIdMap*)(D_actor_461800_80143894 = memCalloc(0x4F8, false));
    if (D_actor_461800_80143894 == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_461800_80132A90;
    coord->parent                = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->flags                   = 0;
    obj->lightMtx                = &D_actor_461800_80143894->light;
    obj->colorMtx                = &D_actor_461800_80143894->color;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    D_actor_461800_80143898      = task;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_461800_80143894->rig.anim, D_actor_461800_80139FB0, obj,
                  D_actor_461800_80143894->rig.poses, D_actor_461800_80143894->rig.slots);
    D_actor_461800_80143894->st.animId = 1;
    D_actor_461800_80143894->st.state  = 2;

    spawned1 = Task_SpawnFromTable(D_actor_461800_80139F8C, 1, 8, 0);
    if (spawned1 != NULL) {
        D_actor_461800_80143894->helper1 = spawned1;
        actorTintModel(spawned1->extra.tmd, (GpEnemy*)task->spawnArg2.pointer);
    }

    spawned2 = Task_SpawnFromTable(D_actor_461800_80139F8C, 2, 0xC, 0);
    if (spawned2 != NULL) {
        D_actor_461800_80143894->helper2 = spawned2;
        actorTintModel(spawned2->extra.tmd, (GpEnemy*)task->spawnArg2.pointer);
    }

    D_actor_461800_80143894->st.travel  = 0;
    D_actor_461800_80143894->turnFrames = 0;
    task->msgTable                      = D_actor_461800_80139F5C;
    func_actor_461800_80132660(task);
    task->state++;
}

/// Per-frame update of the first variant: modes 1 and 2 run their one-shot
/// setup and switch to mode 3; mode 3 walks the model while `st.travel` counts
/// down (distance picked by `D_actor_461800_8014389C_value`), turns it while
/// `turnFrames` counts down in animation 3, then ticks the animation.
static void func_actor_461800_80132660(Task* task)
{
    GfxCoord*        coord = task->extra.tmd->coords;
    Actor461800Work* work  = (Actor461800Work*)task->work;

    if (D_actor_461800_80143894->st.state == 1) {
        func_actor_461800_80132D04();
        D_actor_461800_80143894->st.state = 3;
    } else if (D_actor_461800_80143894->st.state == 2) {
        func_actor_461800_80132C74();
        D_actor_461800_80143894->st.state = 3;
    } else if (D_actor_461800_80143894->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (D_actor_461800_8014389C_value) {
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
                    D_actor_461800_80139F58 = 10;
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
        func_actor_461800_80132C28();
    }
}

/// Two-state dispatcher: publishes the task's work block in
/// `D_actor_461800_80143894` on the way through, then calls the handler its
/// state selects.
void func_actor_461800_801329B0(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_461800_80132390,
        func_actor_461800_80132A0C,
    };

    D_actor_461800_80143894 = (Actor461800Work*)task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Second state of the first variant's task: refreshes the model root's world
/// matrix, relights the model from a point 0x320 above its translation, then
/// runs the per-frame update and draws the ground shadow.
static void func_actor_461800_80132A0C(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_461800_80132660(task);
    func_actor_461800_80132AD8(task);
}

/// `Task::exitCallback` of the first variant: hands the task's `GpEnemy`
/// (parked in `Task::spawnArg2` by the spawn descriptor) back to
/// `Gp_DestroyEnemy`, then kills the two helper tasks the spawn routine
/// started.
static void func_actor_461800_80132A90(Task* task)
{
    Actor461800Work* work = (Actor461800Work*)task->work;

    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
    taskKill(work->helper1);
    taskKill(work->helper2);
}

/// Draws the ground shadow quad under the model root, unless the model is
/// hidden (`flags & 0x80`) or has no buffer yet. The root's world translation
/// is staged in a scratchpad `VECTOR3`, and the quad's brightness follows the
/// room's current ground shade.
static void func_actor_461800_80132AD8(Task* task)
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
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// State handler of the actor's model task: the spawn tick hangs the task's own
/// coordinate frame off the actor's part `spawnArg1` and steps to state 1, and
/// every later tick hands that part's world translation, dropped by 0x320 in y,
/// to `func_800D7A9C` for the part colour matrix.
void func_actor_461800_80132B74(Task* task)
{
    TmdObject* extra = task->extra.tmd;
    GfxCoord*  coord = extra->coords;
    GfxCoord*  parts = D_actor_461800_80143898->extra.tmd->coords;
    GfxCoord*  part  = parts + task->spawnArg1.value;
    VECTOR     vec;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            extra->flags        = 0;
            coord->parent       = part;
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

/// Ticks animation slots 1..0x13 of the actor's animation context.
static void func_actor_461800_80132C28(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_461800_80143894->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Marks animation slots 1..0x13 reset-pending and reseeds each of them from
/// the current animation id, then records that id as the one now playing.
static void func_actor_461800_80132C74(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_461800_80143894->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_461800_80143894->rig.anim, i, D_actor_461800_80143894->st.animId);
        i++;
    } while (i < 0x14);
    D_actor_461800_80143894->st.appliedAnimId = D_actor_461800_80143894->st.animId;
}

/// Reseeds animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
static void func_actor_461800_80132D04(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_461800_80143894->rig.anim, i, D_actor_461800_80143894->st.animId, 0,
                      D_actor_461800_80139F58);
        i++;
    } while (i < 0x14);
    D_actor_461800_80143894->st.appliedAnimId = D_actor_461800_80143894->st.animId;
}

/// Applies an animation preset: the id is copied into the work block, the reset
/// mode is picked by the preset's blend flag and the reset argument is either
/// taken from the preset or left at 2, then the whole slot array is re-seeded.
/// Only the six known animation ids are accepted; anything else leaves the work
/// block untouched and reports the failure.
s32 func_actor_461800_80132D84(Task* task, s32 arg1, AnimationPlayRequest* preset, s32 arg3)
{
    if (preset->animationId < 6) {
        D_actor_461800_80143894->st.animId = preset->animationId;
        if (preset->blend != ANIMATION_BLEND_RESET) {
            D_actor_461800_80143894->st.state = 1;
            D_actor_461800_80139F58           = preset->blendFrames;
        } else {
            D_actor_461800_80143894->st.state = 2;
        }
        D_actor_461800_80143894->st.field_6 = 0;
        func_actor_461800_80132660(D_actor_461800_80143898);
        return 0;
    }
    return -1;
}

/// Applies a `Tmd_Create` flag word to the three model objects this actor owns:
/// the one on its own task and the two helper tasks' models in the work block.
/// `arg2 & 1` picks the base value -- 0x80 normally, 0 when set -- and
/// `arg2 & 2` ORs bit 0x4 in on top of it.
s32 func_actor_461800_80132E14(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject* own    = D_actor_461800_80143898->extra.tmd;
    TmdObject* first  = D_actor_461800_80143894->helper1->extra.tmd;
    TmdObject* second = D_actor_461800_80143894->helper2->extra.tmd;

    if (arg2 & 1) {
        own->flags    = 0;
        first->flags  = 0;
        second->flags = 0;
    } else {
        own->flags    = 0x80;
        first->flags  = 0x80;
        second->flags = 0x80;
    }
    if (arg2 & 2) {
        own->flags    |= 4;
        first->flags  |= 4;
        second->flags |= 4;
    }
    return 0;
}

/// Seeds the task's `TmdObject` coordinate frame from `placement`: only the yaw
/// is used, remembered in the work block and applied with `Gfx_RotMatrixY`,
/// then the three longs become the coordinate's translation.
s32 func_actor_461800_80132EA4(Task* task, s32 arg1, GpXformArg* placement)
{
    GfxCoord* coord;
    u16       yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_461800_80143894->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

s32 func_actor_461800_80132F20(Task* arg0, s32 arg1, GpCmdArg* arg2, s32 arg3)
{
    if (arg2->command == 0) {
        D_actor_461800_80143894->turnFrames = 0x14;
    }
    return 0;
}

/// Turns the model to face `target` -- away from it in mode 1 -- and stores the
/// per-step distance: the planar distance over 60 steps in mode 0, 15 in
/// mode 1 and 25 otherwise.
s32 func_actor_461800_80132F44(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GfxCoord*        coord;
    Actor461800Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                         = task->extra.tmd->coords;
    work                          = (Actor461800Work*)task->work;
    D_actor_461800_8014389C_value = mode;
    dx                            = target->vx - coord->coord.t[0];
    dz                            = target->vz - coord->coord.t[2];
    angle                         = ratan2(dx, dz);
    work->st.yaw                  = angle;
    if (D_actor_461800_8014389C_value == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (D_actor_461800_8014389C_value) {
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

static void func_actor_461800_8013307C(GpEnemy* enemy, Task* task)
{
    VECTOR     vec;
    GfxCoord*  coord;
    TmdObject* obj;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    task->work = (TaskIdMap*)(D_actor_461800_801438A0 = memCalloc(0x4C0, false));
    if (D_actor_461800_801438A0 == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_461800_80133634;
    coord->parent                = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->lightMtx                = &D_actor_461800_801438A0->light;
    obj->colorMtx                = &D_actor_461800_801438A0->color;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    D_actor_461800_801438A4      = task;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_461800_801438A0->rig.anim, D_actor_461800_801437F8, obj,
                  D_actor_461800_801438A0->rig.poses, D_actor_461800_801438A0->rig.slots);
    D_actor_461800_801438A0->st.animId  = 1;
    D_actor_461800_801438A0->st.state   = 2;
    D_actor_461800_801438A0->st.travel  = 0;
    D_actor_461800_801438A0->turnFrames = 0;
    D_actor_461800_801438A0->stepRec    = 0;
    D_actor_461800_801438A0->footsteps  = 0;
    task->msgTable                      = D_actor_461800_801437BC;
    func_actor_461800_801331E4(task);
    task->state++;
}

/// Per-frame update of the second variant: modes 1 and 2 run their one-shot
/// setup and switch to mode 3 for the next frame; mode 3 walks the model while `st.travel` counts
/// down (distance picked by `D_actor_461800_801438A8`), turns it while
/// `turnFrames` counts down in animation 3, then ticks the animation.
static void func_actor_461800_801331E4(Task* task)
{
    GfxCoord*        coord = task->extra.tmd->coords;
    Actor151000Work* work  = (Actor151000Work*)task->work;

    if (D_actor_461800_801438A0->st.state == 1) {
        func_actor_461800_8013380C();
        D_actor_461800_801438A0->st.state = 3;
    } else if (D_actor_461800_801438A0->st.state == 2) {
        func_actor_461800_80133770();
        D_actor_461800_801438A0->st.state = 3;
    } else if (D_actor_461800_801438A0->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (D_actor_461800_801438A8) {
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
                    D_actor_461800_801437B8 = 10;
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
        func_actor_461800_80133724();
        if (work->footsteps != 0) {
            func_actor_461800_8013365C(task);
        }
    }
}

/// Two-state dispatcher whose handler table is built on the stack, publishing
/// the task's work block in `D_actor_461800_801438A0` on the way through so the
/// rest of the overlay can reach it without the task.
void func_actor_461800_80133554(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_461800_8013307C,
        func_actor_461800_801335B0,
    };

    D_actor_461800_801438A0 = (Actor151000Work*)task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Second state of the second variant's task: refreshes the model root's world
/// matrix, relights the model from a point 0x320 above its translation, then
/// runs the per-frame update and draws the ground shadow.
static void func_actor_461800_801335B0(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_461800_801331E4(task);
    func_actor_461800_80133B98(task);
}

/// `Task::exitCallback` of the second variant: hands the task's `GpEnemy`
/// (parked in `Task::spawnArg2` by the spawn descriptor) back to
/// `Gp_DestroyEnemy`.
static void func_actor_461800_80133634(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Footstep sounds of the second variant: whenever animation slot 1 rolls onto
/// a new record whose flags nibble is 0x10 or 0x20, plays the matching step
/// sound, panned and attenuated from the model's second coordinate. The record
/// is latched in `stepRec` so each one fires once.
static void func_actor_461800_8013365C(Task* task)
{
    Actor151000Work* work;
    GfxCoord*        obj;
    AnimationRecord* rec;
    s32              cueBits;
    s32              id;
    s32              pan;

    work = (Actor151000Work*)task->work;
    obj  = task->extra.tmd->coords + 1;
    rec  = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
    if (rec == NULL || rec == work->stepRec) {
        return;
    }
    work->stepRec = rec;
    cueBits       = rec->flags & ANIMATION_RECORD_CUE_MASK;
    if (cueBits != ANIMATION_RECORD_CUE_1 && cueBits != ANIMATION_RECORD_CUE_2) {
        return;
    }
    id = 0x1000000F;
    if (cueBits == ANIMATION_RECORD_CUE_1) {
        id = 0x10000010;
    }
    id += 0x64;
    pan = (s8)Gp_GetObjPan(obj);
    SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(obj));
}

/// Ticks animation slots 1..0x12 of the second variant's animation context.
static void func_actor_461800_80133724(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_461800_801438A0->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Restarts animation slots 1..0x12 from `st.animId`, flagging each slot's
/// `field_9` before the reset so it replays from the top, and latches that id
/// into `st.appliedAnimId` as the one now playing.
static void func_actor_461800_80133770(void)
{
    s32 i;

    D_actor_461800_801438A0->stepRec = 0;
    i                                = 1;
    do {
        D_actor_461800_801438A0->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_461800_801438A0->rig.anim, i, D_actor_461800_801438A0->st.animId);
        i++;
    } while (i < 0x13);
    D_actor_461800_801438A0->st.appliedAnimId = D_actor_461800_801438A0->st.animId;
}

/// Reseeds animation slots 1..0x12 from `st.animId` and latches that id into
/// `st.appliedAnimId` as the one now playing.
static void func_actor_461800_8013380C(void)
{
    s32 i;

    D_actor_461800_801438A0->stepRec = 0;
    i                                = 1;
    do {
        func_800B4114(&D_actor_461800_801438A0->rig.anim, i, D_actor_461800_801438A0->st.animId, 0,
                      D_actor_461800_801437B8);
        i++;
    } while (i < 0x13);
    D_actor_461800_801438A0->st.appliedAnimId = D_actor_461800_801438A0->st.animId;
}

/// Applies an animation preset to the second variant's work block: the id is
/// copied in, the reset mode is picked by the preset's blend flag and the reset
/// argument is either taken from the preset or left at 2, then the work block's
/// animation is restarted through `func_actor_461800_801331E4`. Only the ids
/// this variant owns are accepted; anything else leaves the work block
/// untouched and reports the failure.
s32 func_actor_461800_80133898(Task* task, s32 arg1, AnimationPlayRequest* preset, s32 arg3)
{
    if (preset->animationId < 0x23) {
        D_actor_461800_801438A0->st.animId = preset->animationId;
        if (preset->blend != ANIMATION_BLEND_RESET) {
            D_actor_461800_801438A0->st.state = 1;
            D_actor_461800_801437B8           = preset->blendFrames;
        } else {
            D_actor_461800_801438A0->st.state = 2;
        }
        D_actor_461800_801438A0->st.field_6 = 0;
        func_actor_461800_801331E4(D_actor_461800_801438A4);
        return 0;
    }
    return -1;
}

/// Visibility message of the second variant: applies `arg2` to the model of
/// the task published in `D_actor_461800_801438A4` - bit 0 selects
/// `TmdObject.flags` 0 (shown) vs 0x80 (hidden), bit 1 ORs in 0x4.
s32 func_actor_461800_80133928(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = D_actor_461800_801438A4->extra.tmd;
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

/// Seeds the task's `TmdObject` coordinate frame from `placement`: only the yaw
/// is used, remembered in the work block and applied with `Gfx_RotMatrixY`,
/// then the three longs become the coordinate's translation.
s32 func_actor_461800_80133970(Task* task, s32 arg1, GpXformArg* placement)
{
    GfxCoord* coord;
    u16       yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_461800_801438A0->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Message handler: the message id selects how the second work block is
/// reseeded -- 0 arms the reset argument, 1 remembers the id in the byte the
/// seeding loop reads. Anything else does nothing.
s32 func_actor_461800_801339EC(Task* task, s32 arg1, GpCmdArg* msg, s32 arg3)
{
    s32 id;

    id = msg->command;
    switch (id) {
        case 0:
            D_actor_461800_801438A0->turnFrames = 0x14;
            break;
        case 1:
            D_actor_461800_801438A0->footsteps = id;
            break;
    }
    return 0;
}

/// Turns the model to face `target` -- away from it in mode 1 -- and stores the
/// per-step distance in the second work block: the planar distance over 60
/// steps in mode 0, 15 in mode 1 and 25 in mode 2.
s32 func_actor_461800_80133A3C(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GfxCoord*        coord;
    Actor151000Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = task->extra.tmd->coords;
    work                    = (Actor151000Work*)task->work;
    D_actor_461800_801438A8 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->st.yaw            = angle;
    if (D_actor_461800_801438A8 == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (D_actor_461800_801438A8) {
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

/// Draws the ground shadow quad under the model root, unless the model is
/// hidden (`flags & 0x80`) or has no buffer yet. The root's world translation
/// is staged in a scratchpad `VECTOR3`, and the quad's brightness follows the
/// room's current ground shade.
static void func_actor_461800_80133B98(Task* task)
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
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_POP_BYTES(0x18);
    }
}
