#include <psyq/sys/types.h>
#include <psyq/libgte.h>

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
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/shelter_r49.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s16 D_actor_143900_801496C0[2];
// Scalar symbol view preserves the original byte/halfword address formation.
extern s16 D_actor_143900_801496C0_value __asm__("D_actor_143900_801496C0");

/// Work block of the overlay's first actor variant, allocated zeroed by its
/// spawn routine and kept both in `D_actor_143900_801496B8` and at
/// `Task::work`; every other function of the variant reaches it through the
/// global.
///
/// `light` and `color` are the two matrices the block supplies to the model:
/// the spawn routine points the object's `lightMtx` / `colorMtx` at them.
/// `rig` and `st` are the model's animation rig and state, and `turnFrames`
/// the frames of turning left while animation 3 plays, which the 0x7DB
/// handler latches.
typedef struct Actor143900Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig20  rig;
    ActorEnemyState st;
    s16             turnFrames;
    byte            pad_4EE[0x2];
} Actor143900Work;
STATIC_ASSERT_SIZEOF(Actor143900Work, 0x4F0);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// Spawn table this overlay hands to `Task_SpawnFromTable`. It sits at an
/// absolute address outside the actor slot - offset 0x440 into the loaded room
/// overlay, whose base is 0x8017D5C0 - so splat cannot name it and it keeps its
/// raw `D_` form.

/// The first variant's work block, published by its dispatcher
/// `func_actor_143900_80132324` and its spawn routine.
extern Actor143900Work* D_actor_143900_801496B8;

/// The first variant's task, published by its spawn routine so the
/// visibility and play-animation handlers can reach it.
extern Task* D_actor_143900_801496BC;

/// Approach mode the last `func_actor_143900_8013279C` call selected; the first
/// variant's update picks its walk distance from it.

/// Reset argument the first variant forwards to the reseed: its play-animation
/// handler latches the preset's `field_C` here, and the update sets it to 10
/// when a walk ends.
extern s16 D_actor_143900_801413B8;

/// The first variant's message table; its spawn routine publishes it as
/// `Task::msgTable`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*);
        s32 (*call1)(Task*, s32, ActorCommand* request);
        s32 (*call2)(Task*, s32, GpXformArg*);
        s32 (*call3)(Task*, s32, VECTOR*, s32);
        s32 (*call4)(Task*, s32, s32);
    } handler;
} Actor143900MsgEntry;
STATIC_ASSERT_SIZEOF(Actor143900MsgEntry, 8);

extern Actor143900MsgEntry D_actor_143900_801413BC[];

/// Animation stream the first variant's spawn routine binds into its work
/// block's animation context with `func_800B3F84`.
extern u8 D_actor_143900_801413F8[];

/// The second variant's work block, published by its dispatcher
/// `func_actor_143900_80132DEC` and its spawn routine.
extern Actor461800Work* D_actor_143900_801496C4;

/// The second variant's task, published by its spawn routine so the placement,
/// visibility and play-animation handlers can reach it.
extern Task* D_actor_143900_801496C8;

/// Approach mode the last `func_actor_143900_801333C4` call selected; the
/// second variant's update picks its walk distance from it.
extern s16 D_actor_143900_801496CC;

/// Reset argument the second variant forwards to the reseed: its
/// play-animation handler latches the preset's `field_C` here, and the update
/// sets it to 10 when a walk ends.
extern s16 D_actor_143900_80149630;

/// The second variant's message table; its spawn routine publishes it as
/// `Task::msgTable`.
extern Actor143900MsgEntry D_actor_143900_80149634[];

/// Spawn table the second variant's spawn routine starts its two helper tasks
/// from, indices 1 and 2; the tasks are parked in `helper1` / `helper2`.
extern TaskDesc D_actor_143900_80149664[];

/// Animation stream the second variant's spawn routine binds into its work
/// block's animation context with `func_800B3F84`.
extern u8 D_actor_143900_80149688[];

static void func_actor_143900_80131FD4(Task* task);
static void func_actor_143900_80132380(GpEnemy* enemy, Task* task);
static void func_actor_143900_80132404(Task* task);
static void func_actor_143900_8013242C(Task* task);
static void func_actor_143900_801324C8(void);
static void func_actor_143900_80132514(void);
static void func_actor_143900_801325A4(void);
static void func_actor_143900_80132A9C(Task* task);
static void func_actor_143900_80132E48(GpEnemy* enemy, Task* task);
static void func_actor_143900_80132ECC(Task* task);
static void func_actor_143900_80132F14(Task* task);
static void func_actor_143900_80133068(void);
static void func_actor_143900_801330B4(void);
static void func_actor_143900_80133144(void);

extern TmdSource D_actor_143900_80146F40;
extern TmdSource D_actor_143900_801493AC;
extern TmdSource D_actor_143900_8014960C;
s32              func_actor_143900_801331C4(Task*, s32, AnimationPlayRequest*);
s32              func_actor_143900_80133254(Task*, s32, s32);
s32              func_actor_143900_801332E4(Task*, s32, GpXformArg*);
s32              func_actor_143900_80133360(Task*, s32, ActorCommand* msg);
s32              func_actor_143900_801333C4(Task*, s32, VECTOR*, s32);
void             func_actor_143900_80132DEC(Task*);
void             func_actor_143900_80132FB0(Task*);

s32  func_actor_143900_80132624(Task*, s32, AnimationPlayRequest*);
s32  func_actor_143900_801326B4(Task*, s32, s32);
s32  func_actor_143900_801326FC(Task*, s32, GpXformArg*);
s32  func_actor_143900_80132778(Task*, s32, ActorCommand* msg);
s32  func_actor_143900_8013279C(Task*, s32, VECTOR*, s32);
void func_actor_143900_80132324(Task*);

void func_actor_143900_80131E24(void);

void func_actor_143900_80131E24(void);

AnimationPlayRequest D_actor_143900_801334FC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143900_80133510 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143900_80133524 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143900_80133538 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143900_8013354C = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpEvsCmd D_actor_143900_80133560[32] = {
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x54310002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143900_801334FC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_143900_80133510 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x54310001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x54310003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_143900_80133524 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_143900_80133538 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_143900_8013354C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_143900_80131E24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 46, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_143900_80133860[12] = {
    { 19, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_143900_80131E24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_actor_143900_80133980[20] = {
#include "assets/actor_143900_model_079E0_skeleton.inc"
};

u32 D_actor_143900_80133C50[20] = {
#include "assets/actor_143900_model_079E0_partVerts.inc"
};

SVECTOR D_actor_143900_80133CA0[342] = {
#include "assets/actor_143900_model_079E0_verts.inc"
};

SVECTOR D_actor_143900_80134750[442] = {
#include "assets/actor_143900_model_079E0_normals.inc"
};

u32 D_actor_143900_80135520[4280] = {
#include "assets/actor_143900_model_079E0_stream.inc"
};

TmdSource D_actor_143900_80139800 = {
    0,
    21544,
    8516,
    20,
    D_actor_143900_80133C50,
    D_actor_143900_80133CA0,
    D_actor_143900_80134750,
    D_actor_143900_80133980,
    D_actor_143900_80135520,
};

AnimationPackedPose D_actor_143900_80139824[2] = {
#include "assets/actor_143900_animation_07C70_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013983C[31] = {
#include "assets/actor_143900_animation_07C70_bank4.inc"
};

AnimationRecord D_actor_143900_801398B8[108] = {
#include "assets/actor_143900_animation_07C70_records.inc"
};

u16 D_actor_143900_80139A68[20] = {
#include "assets/actor_143900_animation_07C70_indices.inc"
};

AnimationSet D_actor_143900_80139A90 = {
    D_actor_143900_801398B8,
    D_actor_143900_80139A68,
    { NULL, D_actor_143900_80139824, NULL, NULL, D_actor_143900_8013983C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_80139AB8[2] = {
#include "assets/actor_143900_animation_0800C_bank1.inc"
};

AnimationPackedRotation D_actor_143900_80139AD0[56] = {
#include "assets/actor_143900_animation_0800C_bank4.inc"
};

AnimationRecord D_actor_143900_80139BB0[149] = {
#include "assets/actor_143900_animation_0800C_records.inc"
};

u16 D_actor_143900_80139E04[20] = {
#include "assets/actor_143900_animation_0800C_indices.inc"
};

AnimationSet D_actor_143900_80139E2C = {
    D_actor_143900_80139BB0,
    D_actor_143900_80139E04,
    { NULL, D_actor_143900_80139AB8, NULL, NULL, D_actor_143900_80139AD0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_80139E54[2] = {
#include "assets/actor_143900_animation_08398_bank1.inc"
};

AnimationPackedRotation D_actor_143900_80139E6C[58] = {
#include "assets/actor_143900_animation_08398_bank4.inc"
};

AnimationRecord D_actor_143900_80139F54[143] = {
#include "assets/actor_143900_animation_08398_records.inc"
};

u16 D_actor_143900_8013A190[20] = {
#include "assets/actor_143900_animation_08398_indices.inc"
};

AnimationSet D_actor_143900_8013A1B8 = {
    D_actor_143900_80139F54,
    D_actor_143900_8013A190,
    { NULL, D_actor_143900_80139E54, NULL, NULL, D_actor_143900_80139E6C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013A1E0[2] = {
#include "assets/actor_143900_animation_087A4_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013A1F8[59] = {
#include "assets/actor_143900_animation_087A4_bank4.inc"
};

AnimationRecord D_actor_143900_8013A2E4[174] = {
#include "assets/actor_143900_animation_087A4_records.inc"
};

u16 D_actor_143900_8013A59C[20] = {
#include "assets/actor_143900_animation_087A4_indices.inc"
};

AnimationSet D_actor_143900_8013A5C4 = {
    D_actor_143900_8013A2E4,
    D_actor_143900_8013A59C,
    { NULL, D_actor_143900_8013A1E0, NULL, NULL, D_actor_143900_8013A1F8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013A5EC[11] = {
#include "assets/actor_143900_animation_09138_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013A670[245] = {
#include "assets/actor_143900_animation_09138_bank4.inc"
};

AnimationRecord D_actor_143900_8013AA44[315] = {
#include "assets/actor_143900_animation_09138_records.inc"
};

u16 D_actor_143900_8013AF30[20] = {
#include "assets/actor_143900_animation_09138_indices.inc"
};

AnimationSet D_actor_143900_8013AF58 = {
    D_actor_143900_8013AA44,
    D_actor_143900_8013AF30,
    { NULL, D_actor_143900_8013A5EC, NULL, NULL, D_actor_143900_8013A670, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013AF80[2] = {
#include "assets/actor_143900_animation_09364_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013AF98[33] = {
#include "assets/actor_143900_animation_09364_bank4.inc"
};

AnimationRecord D_actor_143900_8013B01C[80] = {
#include "assets/actor_143900_animation_09364_records.inc"
};

u16 D_actor_143900_8013B15C[20] = {
#include "assets/actor_143900_animation_09364_indices.inc"
};

AnimationSet D_actor_143900_8013B184 = {
    D_actor_143900_8013B01C,
    D_actor_143900_8013B15C,
    { NULL, D_actor_143900_8013AF80, NULL, NULL, D_actor_143900_8013AF98, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013B1AC[5] = {
#include "assets/actor_143900_animation_099B4_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013B1E8[147] = {
#include "assets/actor_143900_animation_099B4_bank4.inc"
};

AnimationRecord D_actor_143900_8013B434[222] = {
#include "assets/actor_143900_animation_099B4_records.inc"
};

u16 D_actor_143900_8013B7AC[20] = {
#include "assets/actor_143900_animation_099B4_indices.inc"
};

AnimationSet D_actor_143900_8013B7D4 = {
    D_actor_143900_8013B434,
    D_actor_143900_8013B7AC,
    { NULL, D_actor_143900_8013B1AC, NULL, NULL, D_actor_143900_8013B1E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013B7FC[12] = {
#include "assets/actor_143900_animation_0A040_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013B88C[152] = {
#include "assets/actor_143900_animation_0A040_bank4.inc"
};

AnimationRecord D_actor_143900_8013BAEC[211] = {
#include "assets/actor_143900_animation_0A040_records.inc"
};

u16 D_actor_143900_8013BE38[20] = {
#include "assets/actor_143900_animation_0A040_indices.inc"
};

AnimationSet D_actor_143900_8013BE60 = {
    D_actor_143900_8013BAEC,
    D_actor_143900_8013BE38,
    { NULL, D_actor_143900_8013B7FC, NULL, NULL, D_actor_143900_8013B88C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013BE88[2] = {
#include "assets/actor_143900_animation_0A21C_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013BEA0[25] = {
#include "assets/actor_143900_animation_0A21C_bank4.inc"
};

AnimationRecord D_actor_143900_8013BF04[68] = {
#include "assets/actor_143900_animation_0A21C_records.inc"
};

u16 D_actor_143900_8013C014[20] = {
#include "assets/actor_143900_animation_0A21C_indices.inc"
};

AnimationSet D_actor_143900_8013C03C = {
    D_actor_143900_8013BF04,
    D_actor_143900_8013C014,
    { NULL, D_actor_143900_8013BE88, NULL, NULL, D_actor_143900_8013BEA0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013C064[2] = {
#include "assets/actor_143900_animation_0A738_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013C07C[123] = {
#include "assets/actor_143900_animation_0A738_bank4.inc"
};

AnimationRecord D_actor_143900_8013C268[178] = {
#include "assets/actor_143900_animation_0A738_records.inc"
};

u16 D_actor_143900_8013C530[20] = {
#include "assets/actor_143900_animation_0A738_indices.inc"
};

AnimationSet D_actor_143900_8013C558 = {
    D_actor_143900_8013C268,
    D_actor_143900_8013C530,
    { NULL, D_actor_143900_8013C064, NULL, NULL, D_actor_143900_8013C07C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013C580[2] = {
#include "assets/actor_143900_animation_0A8D0_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013C598[16] = {
#include "assets/actor_143900_animation_0A8D0_bank4.inc"
};

AnimationRecord D_actor_143900_8013C5D8[60] = {
#include "assets/actor_143900_animation_0A8D0_records.inc"
};

u16 D_actor_143900_8013C6C8[20] = {
#include "assets/actor_143900_animation_0A8D0_indices.inc"
};

AnimationSet D_actor_143900_8013C6F0 = {
    D_actor_143900_8013C5D8,
    D_actor_143900_8013C6C8,
    { NULL, D_actor_143900_8013C580, NULL, NULL, D_actor_143900_8013C598, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013C718[2] = {
#include "assets/actor_143900_animation_0AB6C_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013C730[40] = {
#include "assets/actor_143900_animation_0AB6C_bank4.inc"
};

AnimationRecord D_actor_143900_8013C7D0[101] = {
#include "assets/actor_143900_animation_0AB6C_records.inc"
};

u16 D_actor_143900_8013C964[20] = {
#include "assets/actor_143900_animation_0AB6C_indices.inc"
};

AnimationSet D_actor_143900_8013C98C = {
    D_actor_143900_8013C7D0,
    D_actor_143900_8013C964,
    { NULL, D_actor_143900_8013C718, NULL, NULL, D_actor_143900_8013C730, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013C9B4[11] = {
#include "assets/actor_143900_animation_0B2B4_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013CA38[175] = {
#include "assets/actor_143900_animation_0B2B4_bank4.inc"
};

AnimationRecord D_actor_143900_8013CCF4[238] = {
#include "assets/actor_143900_animation_0B2B4_records.inc"
};

u16 D_actor_143900_8013D0AC[20] = {
#include "assets/actor_143900_animation_0B2B4_indices.inc"
};

AnimationSet D_actor_143900_8013D0D4 = {
    D_actor_143900_8013CCF4,
    D_actor_143900_8013D0AC,
    { NULL, D_actor_143900_8013C9B4, NULL, NULL, D_actor_143900_8013CA38, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013D0FC[2] = {
#include "assets/actor_143900_animation_0B4A0_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013D114[24] = {
#include "assets/actor_143900_animation_0B4A0_bank4.inc"
};

AnimationRecord D_actor_143900_8013D174[73] = {
#include "assets/actor_143900_animation_0B4A0_records.inc"
};

u16 D_actor_143900_8013D298[20] = {
#include "assets/actor_143900_animation_0B4A0_indices.inc"
};

AnimationSet D_actor_143900_8013D2C0 = {
    D_actor_143900_8013D174,
    D_actor_143900_8013D298,
    { NULL, D_actor_143900_8013D0FC, NULL, NULL, D_actor_143900_8013D114, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013D2E8[15] = {
#include "assets/actor_143900_animation_0BC24_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013D39C[177] = {
#include "assets/actor_143900_animation_0BC24_bank4.inc"
};

AnimationRecord D_actor_143900_8013D660[239] = {
#include "assets/actor_143900_animation_0BC24_records.inc"
};

u16 D_actor_143900_8013DA1C[20] = {
#include "assets/actor_143900_animation_0BC24_indices.inc"
};

AnimationSet D_actor_143900_8013DA44 = {
    D_actor_143900_8013D660,
    D_actor_143900_8013DA1C,
    { NULL, D_actor_143900_8013D2E8, NULL, NULL, D_actor_143900_8013D39C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013DA6C[12] = {
#include "assets/actor_143900_animation_0C718_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013DAFC[255] = {
#include "assets/actor_143900_animation_0C718_bank4.inc"
};

AnimationRecord D_actor_143900_8013DEF8[390] = {
#include "assets/actor_143900_animation_0C718_records.inc"
};

u16 D_actor_143900_8013E510[20] = {
#include "assets/actor_143900_animation_0C718_indices.inc"
};

AnimationSet D_actor_143900_8013E538 = {
    D_actor_143900_8013DEF8,
    D_actor_143900_8013E510,
    { NULL, D_actor_143900_8013DA6C, NULL, NULL, D_actor_143900_8013DAFC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013E560[14] = {
#include "assets/actor_143900_animation_0D1C4_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013E608[263] = {
#include "assets/actor_143900_animation_0D1C4_bank4.inc"
};

AnimationRecord D_actor_143900_8013EA24[358] = {
#include "assets/actor_143900_animation_0D1C4_records.inc"
};

u16 D_actor_143900_8013EFBC[20] = {
#include "assets/actor_143900_animation_0D1C4_indices.inc"
};

AnimationSet D_actor_143900_8013EFE4 = {
    D_actor_143900_8013EA24,
    D_actor_143900_8013EFBC,
    { NULL, D_actor_143900_8013E560, NULL, NULL, D_actor_143900_8013E608, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8013F00C[103] = {
#include "assets/actor_143900_animation_0ED80_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8013F4E0[620] = {
#include "assets/actor_143900_animation_0ED80_bank4.inc"
};

AnimationRecord D_actor_143900_8013FE90[826] = {
#include "assets/actor_143900_animation_0ED80_records.inc"
};

u16 D_actor_143900_80140B78[20] = {
#include "assets/actor_143900_animation_0ED80_indices.inc"
};

AnimationSet D_actor_143900_80140BA0 = {
    D_actor_143900_8013FE90,
    D_actor_143900_80140B78,
    { NULL, D_actor_143900_8013F00C, NULL, NULL, D_actor_143900_8013F4E0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_80140BC8[2] = {
#include "assets/actor_143900_animation_0F188_bank1.inc"
};

AnimationPackedRotation D_actor_143900_80140BE0[93] = {
#include "assets/actor_143900_animation_0F188_bank4.inc"
};

AnimationRecord D_actor_143900_80140D54[139] = {
#include "assets/actor_143900_animation_0F188_records.inc"
};

u16 D_actor_143900_80140F80[20] = {
#include "assets/actor_143900_animation_0F188_indices.inc"
};

AnimationSet D_actor_143900_80140FA8 = {
    D_actor_143900_80140D54,
    D_actor_143900_80140F80,
    { NULL, D_actor_143900_80140BC8, NULL, NULL, D_actor_143900_80140BE0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_80140FD0[2] = {
#include "assets/actor_143900_animation_0F570_bank1.inc"
};

AnimationPackedRotation D_actor_143900_80140FE8[89] = {
#include "assets/actor_143900_animation_0F570_bank4.inc"
};

AnimationRecord D_actor_143900_8014114C[135] = {
#include "assets/actor_143900_animation_0F570_records.inc"
};

u16 D_actor_143900_80141368[20] = {
#include "assets/actor_143900_animation_0F570_indices.inc"
};

AnimationSet D_actor_143900_80141390 = {
    D_actor_143900_8014114C,
    D_actor_143900_80141368,
    { NULL, D_actor_143900_80140FD0, NULL, NULL, D_actor_143900_80140FE8, NULL, NULL, NULL },
};

s16 D_actor_143900_801413B8 = 8;

Actor143900MsgEntry D_actor_143900_801413BC[6] = {
    { 2003, { .call0 = func_actor_143900_80132624 } },
    { 2005, { .call4 = func_actor_143900_801326B4 } },
    { 2004, { .call2 = func_actor_143900_801326FC } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = func_actor_143900_80132778 } },
    { 2013, { .call3 = func_actor_143900_8013279C } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_143900_801413EC = { 1, 192, func_actor_143900_80132324, { .model = &D_actor_143900_80139800 } };

u8 D_actor_143900_801413F8[80] = {
    0,
    0,
    0,
    0,
    144,
    154,
    19,
    128,
    44,
    158,
    19,
    128,
    184,
    161,
    19,
    128,
    196,
    165,
    19,
    128,
    88,
    175,
    19,
    128,
    132,
    177,
    19,
    128,
    212,
    183,
    19,
    128,
    96,
    190,
    19,
    128,
    240,
    198,
    19,
    128,
    140,
    201,
    19,
    128,
    212,
    208,
    19,
    128,
    192,
    210,
    19,
    128,
    68,
    218,
    19,
    128,
    56,
    229,
    19,
    128,
    228,
    239,
    19,
    128,
    160,
    11,
    20,
    128,
    60,
    192,
    19,
    128,
    88,
    197,
    19,
    128,
    168,
    15,
    20,
    128,
};

TmdBone D_actor_143900_80141448[20] = {
#include "assets/actor_143900_model_15120_skeleton.inc"
};

u32 D_actor_143900_80141718[20] = {
#include "assets/actor_143900_model_15120_partVerts.inc"
};

SVECTOR D_actor_143900_80141768[339] = {
#include "assets/actor_143900_model_15120_verts.inc"
};

SVECTOR D_actor_143900_80142200[389] = {
#include "assets/actor_143900_model_15120_normals.inc"
};

u32 D_actor_143900_80142E28[4166] = {
#include "assets/actor_143900_model_15120_stream.inc"
};

TmdSource D_actor_143900_80146F40 = {
    0,
    21036,
    8232,
    20,
    D_actor_143900_80141718,
    D_actor_143900_80141768,
    D_actor_143900_80142200,
    D_actor_143900_80141448,
    D_actor_143900_80142E28,
};

AnimationPackedPose D_actor_143900_80146F64[2] = {
#include "assets/actor_143900_animation_15330_bank1.inc"
};

AnimationPackedRotation D_actor_143900_80146F7C[30] = {
#include "assets/actor_143900_animation_15330_bank4.inc"
};

AnimationRecord D_actor_143900_80146FF4[77] = {
#include "assets/actor_143900_animation_15330_records.inc"
};

u16 D_actor_143900_80147128[20] = {
#include "assets/actor_143900_animation_15330_indices.inc"
};

AnimationSet D_actor_143900_80147150 = {
    D_actor_143900_80146FF4,
    D_actor_143900_80147128,
    { NULL, D_actor_143900_80146F64, NULL, NULL, D_actor_143900_80146F7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_80147178[2] = {
#include "assets/actor_143900_animation_157D4_bank1.inc"
};

AnimationPackedRotation D_actor_143900_80147190[107] = {
#include "assets/actor_143900_animation_157D4_bank4.inc"
};

AnimationRecord D_actor_143900_8014733C[164] = {
#include "assets/actor_143900_animation_157D4_records.inc"
};

u16 D_actor_143900_801475CC[20] = {
#include "assets/actor_143900_animation_157D4_indices.inc"
};

AnimationSet D_actor_143900_801475F4 = {
    D_actor_143900_8014733C,
    D_actor_143900_801475CC,
    { NULL, D_actor_143900_80147178, NULL, NULL, D_actor_143900_80147190, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8014761C[2] = {
#include "assets/actor_143900_animation_15B1C_bank1.inc"
};

AnimationPackedRotation D_actor_143900_80147634[41] = {
#include "assets/actor_143900_animation_15B1C_bank4.inc"
};

AnimationRecord D_actor_143900_801476D8[143] = {
#include "assets/actor_143900_animation_15B1C_records.inc"
};

u16 D_actor_143900_80147914[20] = {
#include "assets/actor_143900_animation_15B1C_indices.inc"
};

AnimationSet D_actor_143900_8014793C = {
    D_actor_143900_801476D8,
    D_actor_143900_80147914,
    { NULL, D_actor_143900_8014761C, NULL, NULL, D_actor_143900_80147634, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_80147964[7] = {
#include "assets/actor_143900_animation_15DEC_bank1.inc"
};

AnimationPackedRotation D_actor_143900_801479B8[26] = {
#include "assets/actor_143900_animation_15DEC_bank4.inc"
};

AnimationRecord D_actor_143900_80147A20[113] = {
#include "assets/actor_143900_animation_15DEC_records.inc"
};

u16 D_actor_143900_80147BE4[20] = {
#include "assets/actor_143900_animation_15DEC_indices.inc"
};

AnimationSet D_actor_143900_80147C0C = {
    D_actor_143900_80147A20,
    D_actor_143900_80147BE4,
    { NULL, D_actor_143900_80147964, NULL, NULL, D_actor_143900_801479B8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_80147C34[2] = {
#include "assets/actor_143900_animation_162D4_bank1.inc"
};

AnimationPackedRotation D_actor_143900_80147C4C[120] = {
#include "assets/actor_143900_animation_162D4_bank4.inc"
};

AnimationRecord D_actor_143900_80147E2C[168] = {
#include "assets/actor_143900_animation_162D4_records.inc"
};

u16 D_actor_143900_801480CC[20] = {
#include "assets/actor_143900_animation_162D4_indices.inc"
};

AnimationSet D_actor_143900_801480F4 = {
    D_actor_143900_80147E2C,
    D_actor_143900_801480CC,
    { NULL, D_actor_143900_80147C34, NULL, NULL, D_actor_143900_80147C4C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_8014811C[2] = {
#include "assets/actor_143900_animation_16564_bank1.inc"
};

AnimationPackedRotation D_actor_143900_80148134[39] = {
#include "assets/actor_143900_animation_16564_bank4.inc"
};

AnimationRecord D_actor_143900_801481D0[99] = {
#include "assets/actor_143900_animation_16564_records.inc"
};

u16 D_actor_143900_8014835C[20] = {
#include "assets/actor_143900_animation_16564_indices.inc"
};

AnimationSet D_actor_143900_80148384 = {
    D_actor_143900_801481D0,
    D_actor_143900_8014835C,
    { NULL, D_actor_143900_8014811C, NULL, NULL, D_actor_143900_80148134, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_801483AC[2] = {
#include "assets/actor_143900_animation_166AC_bank1.inc"
};

AnimationPackedRotation D_actor_143900_801483C4[16] = {
#include "assets/actor_143900_animation_166AC_bank4.inc"
};

AnimationRecord D_actor_143900_80148404[40] = {
#include "assets/actor_143900_animation_166AC_records.inc"
};

u16 D_actor_143900_801484A4[20] = {
#include "assets/actor_143900_animation_166AC_indices.inc"
};

AnimationSet D_actor_143900_801484CC = {
    D_actor_143900_80148404,
    D_actor_143900_801484A4,
    { NULL, D_actor_143900_801483AC, NULL, NULL, D_actor_143900_801483C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_801484F4[2] = {
#include "assets/actor_143900_animation_168EC_bank1.inc"
};

AnimationPackedRotation D_actor_143900_8014850C[33] = {
#include "assets/actor_143900_animation_168EC_bank4.inc"
};

AnimationRecord D_actor_143900_80148590[85] = {
#include "assets/actor_143900_animation_168EC_records.inc"
};

u16 D_actor_143900_801486E4[20] = {
#include "assets/actor_143900_animation_168EC_indices.inc"
};

AnimationSet D_actor_143900_8014870C = {
    D_actor_143900_80148590,
    D_actor_143900_801486E4,
    { NULL, D_actor_143900_801484F4, NULL, NULL, D_actor_143900_8014850C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_80148734[12] = {
#include "assets/actor_143900_animation_16E10_bank1.inc"
};

AnimationPackedRotation D_actor_143900_801487C4[104] = {
#include "assets/actor_143900_animation_16E10_bank4.inc"
};

AnimationRecord D_actor_143900_80148964[169] = {
#include "assets/actor_143900_animation_16E10_records.inc"
};

u16 D_actor_143900_80148C08[20] = {
#include "assets/actor_143900_animation_16E10_indices.inc"
};

AnimationSet D_actor_143900_80148C30 = {
    D_actor_143900_80148964,
    D_actor_143900_80148C08,
    { NULL, D_actor_143900_80148734, NULL, NULL, D_actor_143900_801487C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_143900_80148C58[9] = {
#include "assets/actor_143900_animation_17280_bank1.inc"
};

AnimationPackedRotation D_actor_143900_80148CC4[95] = {
#include "assets/actor_143900_animation_17280_bank4.inc"
};

AnimationRecord D_actor_143900_80148E40[142] = {
#include "assets/actor_143900_animation_17280_records.inc"
};

u16 D_actor_143900_80149078[20] = {
#include "assets/actor_143900_animation_17280_indices.inc"
};

AnimationSet D_actor_143900_801490A0 = {
    D_actor_143900_80148E40,
    D_actor_143900_80149078,
    { NULL, D_actor_143900_80148C58, NULL, NULL, D_actor_143900_80148CC4, NULL, NULL, NULL },
};

TmdBone D_actor_143900_801490C8[1] = {
#include "assets/actor_143900_model_1758C_skeleton.inc"
};

u32 D_actor_143900_801490EC[1] = {
#include "assets/actor_143900_model_1758C_partVerts.inc"
};

SVECTOR D_actor_143900_801490F0[18] = {
#include "assets/actor_143900_model_1758C_verts.inc"
};

SVECTOR D_actor_143900_80149180[9] = {
#include "assets/actor_143900_model_1758C_normals.inc"
};

u32 D_actor_143900_801491C8[121] = {
#include "assets/actor_143900_model_1758C_stream.inc"
};

TmdSource D_actor_143900_801493AC = {
    0,
    944,
    0,
    1,
    D_actor_143900_801490EC,
    D_actor_143900_801490F0,
    D_actor_143900_80149180,
    D_actor_143900_801490C8,
    D_actor_143900_801491C8,
};

TmdBone D_actor_143900_801493D0[1] = {
#include "assets/actor_143900_model_177EC_skeleton.inc"
};

u32 D_actor_143900_801493F4[1] = {
#include "assets/actor_143900_model_177EC_partVerts.inc"
};

SVECTOR D_actor_143900_801493F8[14] = {
#include "assets/actor_143900_model_177EC_verts.inc"
};

SVECTOR D_actor_143900_80149468[8] = {
#include "assets/actor_143900_model_177EC_normals.inc"
};

u32 D_actor_143900_801494A8[89] = {
#include "assets/actor_143900_model_177EC_stream.inc"
};

TmdSource D_actor_143900_8014960C = {
    0,
    680,
    0,
    1,
    D_actor_143900_801493F4,
    D_actor_143900_801493F8,
    D_actor_143900_80149468,
    D_actor_143900_801493D0,
    D_actor_143900_801494A8,
};

s16 D_actor_143900_80149630 = 8;

Actor143900MsgEntry D_actor_143900_80149634[6] = {
    { 2003, { .call0 = func_actor_143900_801331C4 } },
    { 2005, { .call4 = func_actor_143900_80133254 } },
    { 2004, { .call2 = func_actor_143900_801332E4 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = func_actor_143900_80133360 } },
    { 2013, { .call3 = func_actor_143900_801333C4 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_143900_80149664[3] = {
    { 1, 192, func_actor_143900_80132DEC, { .model = &D_actor_143900_80146F40 } },
    { 1, 192, func_actor_143900_80132FB0, { .model = &D_actor_143900_801493AC } },
    { 1, 192, func_actor_143900_80132FB0, { .model = &D_actor_143900_8014960C } },
};

u8 D_actor_143900_80149688[48] = {
    0,
    0,
    0,
    0,
    60,
    121,
    20,
    128,
    12,
    124,
    20,
    128,
    244,
    128,
    20,
    128,
    132,
    131,
    20,
    128,
    204,
    132,
    20,
    128,
    12,
    135,
    20,
    128,
    48,
    140,
    20,
    128,
    160,
    144,
    20,
    128,
    80,
    113,
    20,
    128,
    244,
    117,
    20,
    128,
    0,
    0,
    0,
    0,
};

Actor143900Work* D_actor_143900_801496B8 = NULL;

Task* D_actor_143900_801496BC = NULL;

s16 D_actor_143900_801496C0[2] = {
    0,
    0x49E7,
};

Actor461800Work* D_actor_143900_801496C4 = NULL;

Task* D_actor_143900_801496C8;

s16 D_actor_143900_801496CC;

static void func_actor_143900_80131E70(GpEnemy* enemy, Task* task);
static void func_actor_143900_801328D4(GpEnemy* enemy, Task* task);

/// Arms `sceneEvent` and starts the room's spawn-table task, unless
/// `demoScene` is 9, so this story trigger is skipped while the attract demo
/// plays.
void func_actor_143900_80131E24(void)
{
    if (Mc_SaveData[0].state.demoScene != 9) {
        Mc_SaveData[0].state.sceneEvent = 0x14;
        Task_SpawnFromTable(D_shelter_r49_8017DA00, 0, 0, 0);
    }
}

/// Spawn routine of the first variant (state 0 of `func_actor_143900_80132324`):
/// allocates the work block and publishes it in `D_actor_143900_801496B8` and
/// the task's `work` slot, binds the model's coordinate to the view and hands
/// the object its light and colour matrices out of the block, publishes the
/// task in `D_actor_143900_801496BC`, relights the model from a point 0x320
/// above its translation, binds the animation stream and runs the first update
/// with the reset mode 2 / id 1 it seeds.
///
/// Every access to the block after the allocation goes through the global
/// rather than the `memCalloc` result, which is why the pointer is reloaded at
/// each use instead of staying in a callee-saved register.
static void func_actor_143900_80131E70(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor143900Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(0x4F0, 0);
    D_actor_143900_801496B8 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_143900_80132404;
    coord->parent                = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->flags                   = 0;
    obj->lightMtx                = &D_actor_143900_801496B8->light;
    obj->colorMtx                = &D_actor_143900_801496B8->color;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    D_actor_143900_801496BC      = task;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_143900_801496B8->rig.anim, D_actor_143900_801413F8, obj,
                  &D_actor_143900_801496B8->rig.poses, D_actor_143900_801496B8->rig.slots);
    D_actor_143900_801496B8->st.animId  = 1;
    D_actor_143900_801496B8->st.state   = 2;
    D_actor_143900_801496B8->st.travel  = 0;
    D_actor_143900_801496B8->turnFrames = 0;
    task->msgTable                      = D_actor_143900_801413BC;
    func_actor_143900_80131FD4(task);
    task->state += 1;
}

/// Per-frame update of the first variant: modes 1 and 2 run their one-shot
/// reseed and switch to mode 3; mode 3 walks the model while `st.travel`
/// counts down (distance picked by `D_actor_143900_801496C0_value`), turns it while
/// `turnFrames` counts down in animation 3, then ticks the animation.
static void func_actor_143900_80131FD4(Task* task)
{
    GfxCoord*        coord = task->extra.tmd->coords;
    Actor143900Work* work  = (Actor143900Work*)task->work;

    if (D_actor_143900_801496B8->st.state == 1) {
        func_actor_143900_801325A4();
        D_actor_143900_801496B8->st.state = 3;
    } else if (D_actor_143900_801496B8->st.state == 2) {
        func_actor_143900_80132514();
        D_actor_143900_801496B8->st.state = 3;
    } else if (D_actor_143900_801496B8->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (D_actor_143900_801496C0_value) {
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
                    D_actor_143900_801413B8 = 10;
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
        func_actor_143900_801324C8();
    }
}

/// Two-state dispatcher of the first variant: publishes the task's work block
/// in `D_actor_143900_801496B8` on the way through, then calls the handler its
/// state selects from a table built on the stack.
void func_actor_143900_80132324(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_143900_80131E70,
        func_actor_143900_80132380,
    };

    D_actor_143900_801496B8 = (Actor143900Work*)task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Second state of the first variant's task: refreshes the model root's world
/// matrix, relights the model from a point 0x320 above its translation, then
/// runs the per-frame update and draws the ground shadow.
static void func_actor_143900_80132380(GpEnemy* enemy, Task* task)
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
    func_actor_143900_80131FD4(task);
    func_actor_143900_8013242C(task);
}

/// `Task::exitCallback` of the first variant: hands the task's `GpEnemy`
/// (parked in `Task::spawnArg2`) back to `Gp_DestroyEnemy`.
static void func_actor_143900_80132404(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Draws the first variant's ground shadow quad under the model root, unless
/// the model is hidden (`flags & 0x80`) or has no buffer yet. The root's world
/// translation is staged in a scratchpad `VECTOR3`, and the quad's brightness
/// follows the room's current ground shade.
static void func_actor_143900_8013242C(Task* task)
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
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Ticks animation slots 1..0x13 of the first variant's animation context.
static void func_actor_143900_801324C8(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_143900_801496B8->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Marks animation slots 1..0x13 of the first variant's work block
/// reset-pending and reseeds each of them from the current animation id, then
/// records that id as the one now playing.
static void func_actor_143900_80132514(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_143900_801496B8->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_143900_801496B8->rig.anim, i, D_actor_143900_801496B8->st.animId);
        i++;
    } while (i < 0x14);
    D_actor_143900_801496B8->st.appliedAnimId = D_actor_143900_801496B8->st.animId;
}

/// Reseeds animation slots 1..0x13 of the first variant's work block from the
/// current animation id with the latched reset argument, and records that id as
/// the one now playing.
static void func_actor_143900_801325A4(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_143900_801496B8->rig.anim, i, D_actor_143900_801496B8->st.animId, 0,
                      D_actor_143900_801413B8);
        i++;
    } while (i < 0x14);
    D_actor_143900_801496B8->st.appliedAnimId = D_actor_143900_801496B8->st.animId;
}

/// Message 0x7D3 handler of the first variant: adopts `preset`'s animation id
/// when it is one of the first 0x14, latches the reset mode and the reset
/// argument the reseed uses, then hands the published task to the per-frame
/// update. Ids past the range are rejected with -1 and leave the work block
/// untouched.
s32 func_actor_143900_80132624(Task* task, s32 arg1, AnimationPlayRequest* preset)
{
    if (preset->animationId < 0x14) {
        D_actor_143900_801496B8->st.animId = preset->animationId;
        if (preset->blend != ANIMATION_BLEND_RESET) {
            D_actor_143900_801496B8->st.state = 1;
            D_actor_143900_801413B8           = preset->blendFrames;
        } else {
            D_actor_143900_801496B8->st.state = 2;
        }
        D_actor_143900_801496B8->st.field_6 = 0;
        func_actor_143900_80131FD4(D_actor_143900_801496BC);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler of the first variant: applies `arg2` to the model of the
/// task published in `D_actor_143900_801496BC` - bit 0 selects
/// `TmdObject.flags` 0 (shown) vs 0x80 (hidden), bit 1 ORs in 0x4.
s32 func_actor_143900_801326B4(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = D_actor_143900_801496BC->extra.tmd;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = TMD_OBJECT_HIDDEN;
    }
    if (arg2 & 2) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Message 0x7D4 handler of the first variant: seeds the task's `TmdObject`
/// coordinate frame from `placement`. Only the yaw is used, remembered in the
/// work block and applied with `Gfx_RotMatrixY`, then the three longs become
/// the coordinate's translation.
s32 func_actor_143900_801326FC(Task* task, s32 arg1, GpXformArg* placement)
{
    GfxCoord* coord;
    u16       yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_143900_801496B8->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Message 0x7DB handler of the first variant: when the payload's halfword at
/// 0x2 is zero, starts a 0x14-step turn, which the update performs while the
/// model plays animation 3.
s32 func_actor_143900_80132778(Task* task, s32 arg1, ActorCommand* msg)
{
    if (msg->command == 0) {
        D_actor_143900_801496B8->turnFrames = 0x14;
    }
    return 0;
}

/// Message 0x7DD handler of the first variant: turns the model to face
/// `target` -- away from it in mode 1 -- and stores the per-step distance of the
/// walk the update then performs: the planar distance over 60 steps in mode 0,
/// 15 in mode 1 and 25 otherwise.
s32 func_actor_143900_8013279C(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GfxCoord*        coord;
    Actor143900Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                         = task->extra.tmd->coords;
    work                          = (Actor143900Work*)task->work;
    D_actor_143900_801496C0_value = mode;
    dx                            = target->vx - coord->coord.t[0];
    dz                            = target->vz - coord->coord.t[2];
    angle                         = ratan2(dx, dz);
    work->st.yaw                  = angle;
    if (D_actor_143900_801496C0_value == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (D_actor_143900_801496C0_value) {
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

/// Spawn routine of the second variant (state 0 of `func_actor_143900_80132DEC`):
/// allocates the 0x4F8 work block and publishes it in `D_actor_143900_801496C4`
/// and the task's `work` slot, binds the model's coordinate to the view and
/// hands the object its light and colour matrices out of the block, publishes
/// the task in `D_actor_143900_801496C8`, relights the model from a point 0x320
/// above its translation and binds the animation stream. It then starts the two
/// helper tasks from the overlay's spawn table and runs the first update with
/// the reset mode 2 / id 1 it seeds.
static void func_actor_143900_801328D4(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor461800Work* work;
    GfxCoord*        coord;
    TmdObject*       obj;
    Task*            helper;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(0x4F8, false);
    D_actor_143900_801496C4 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_143900_80132ECC;
    coord->parent                = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->node.state.b.flags    = 1;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    obj->otOffset                = 0x10;
    obj->lightMtx                = &D_actor_143900_801496C4->light;
    obj->colorMtx                = &D_actor_143900_801496C4->color;
    obj->flags                   = 0;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    vec.vz                       = coord->workm.t[2];
    D_actor_143900_801496C8      = task;
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_143900_801496C4->rig.anim, D_actor_143900_80149688, obj,
                  &D_actor_143900_801496C4->rig.poses, D_actor_143900_801496C4->rig.slots);
    D_actor_143900_801496C4->st.animId = 1;
    D_actor_143900_801496C4->st.state  = 2;
    helper                             = Task_SpawnFromTable(D_actor_143900_80149664, 1, 1, 0);
    if (helper != NULL) {
        D_actor_143900_801496C4->helper1 = helper;
    }
    helper = Task_SpawnFromTable(D_actor_143900_80149664, 2, 0xC, 0);
    if (helper != NULL) {
        D_actor_143900_801496C4->helper2 = helper;
    }
    D_actor_143900_801496C4->st.travel  = 0;
    D_actor_143900_801496C4->turnFrames = 0;
    task->msgTable                      = D_actor_143900_80149634;
    func_actor_143900_80132A9C(task);
    task->state++;
}

/// Per-frame update of the second variant: modes 1 and 2 run their one-shot
/// reseed and switch to mode 3; mode 3 walks the model while `st.travel`
/// counts down (distance picked by `D_actor_143900_801496CC`), turns it while
/// `turnFrames` counts down in animation 3, then ticks the animation.
static void func_actor_143900_80132A9C(Task* task)
{
    GfxCoord*        coord = task->extra.tmd->coords;
    Actor461800Work* work  = (Actor461800Work*)task->work;

    if (D_actor_143900_801496C4->st.state == 1) {
        func_actor_143900_80133144();
        D_actor_143900_801496C4->st.state = 3;
    } else if (D_actor_143900_801496C4->st.state == 2) {
        func_actor_143900_801330B4();
        D_actor_143900_801496C4->st.state = 3;
    } else if (D_actor_143900_801496C4->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (D_actor_143900_801496CC) {
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
                    D_actor_143900_80149630 = 10;
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
        func_actor_143900_80133068();
    }
}

/// Two-state dispatcher of the second variant: publishes the task's work block
/// in `D_actor_143900_801496C4` on the way through, then calls the handler its
/// state selects from a table built on the stack.
void func_actor_143900_80132DEC(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_143900_801328D4,
        func_actor_143900_80132E48,
    };
    u8 scratch[0x40]; /* never referenced; only reserves the frame */

    D_actor_143900_801496C4 = (Actor461800Work*)task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Second state of the second variant's task: refreshes the model root's world
/// matrix, relights the model from a point 0x320 above its translation, then
/// runs the per-frame update and draws the ground shadow.
static void func_actor_143900_80132E48(GpEnemy* enemy, Task* task)
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
    func_actor_143900_80132A9C(task);
    func_actor_143900_80132F14(task);
}

/// `Task::exitCallback` of the second variant: hands the task's `GpEnemy`
/// (parked in `Task::spawnArg2`) back to `Gp_DestroyEnemy`, then kills the two
/// helper tasks the spawn routine started.
static void func_actor_143900_80132ECC(Task* task)
{
    Actor461800Work* work = (Actor461800Work*)task->work;

    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
    taskKill(work->helper1);
    taskKill(work->helper2);
}

/// Draws the second variant's ground shadow quad under the model root, unless
/// the model is hidden (`flags & 0x80`) or has no buffer yet. The root's world
/// translation is staged in a scratchpad `VECTOR3`, and the quad's brightness
/// follows the room's current ground shade.
static void func_actor_143900_80132F14(Task* task)
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
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Helper-task handler of the second variant: state 0 hangs the task's own
/// coordinate frame off part `spawnArg1` of the second variant's model and
/// steps to state 1; every later tick relights the helper's model from a point
/// 0x320 above that model's root translation.
void func_actor_143900_80132FB0(Task* task)
{
    TmdObject* extra = task->extra.tmd;
    GfxCoord*  coord = extra->coords;
    GfxCoord*  parts = D_actor_143900_801496C8->extra.tmd->coords;
    GfxCoord*  part  = parts + task->spawnArg1.value;
    VECTOR     vec;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            extra->flags        = 0;
            extra->otOffset     = 0xF;
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

/// Ticks animation slots 1..0x13 of the second variant's animation context.
static void func_actor_143900_80133068(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_143900_801496C4->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Marks animation slots 1..0x13 of the second variant's work block
/// reset-pending and reseeds each of them from the current animation id, then
/// records that id as the one now playing.
static void func_actor_143900_801330B4(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_143900_801496C4->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_143900_801496C4->rig.anim, i, D_actor_143900_801496C4->st.animId);
        i++;
    } while (i < 0x14);
    D_actor_143900_801496C4->st.appliedAnimId = D_actor_143900_801496C4->st.animId;
}

/// Reseeds animation slots 1..0x13 of the second variant's work block from the
/// current animation id with the latched reset argument, and records that id as
/// the one now playing.
static void func_actor_143900_80133144(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_143900_801496C4->rig.anim, i, D_actor_143900_801496C4->st.animId, 0,
                      D_actor_143900_80149630);
        i++;
    } while (i < 0x14);
    D_actor_143900_801496C4->st.appliedAnimId = D_actor_143900_801496C4->st.animId;
}

/// Message 0x7D3 handler of the second variant: adopts `preset`'s animation id
/// when it is one of the first 0xC, latches the reset mode and the reset
/// argument the reseed uses, then hands the published task to the per-frame
/// update. Ids past the range are rejected with -1 and leave the work block
/// untouched.
s32 func_actor_143900_801331C4(Task* task, s32 arg1, AnimationPlayRequest* preset)
{
    if (preset->animationId < 0xC) {
        D_actor_143900_801496C4->st.animId = preset->animationId;
        if (preset->blend != ANIMATION_BLEND_RESET) {
            D_actor_143900_801496C4->st.state = 1;
            D_actor_143900_80149630           = preset->blendFrames;
        } else {
            D_actor_143900_801496C4->st.state = 2;
        }
        D_actor_143900_801496C4->st.field_6 = 0;
        func_actor_143900_80132A9C(D_actor_143900_801496C8);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler of the second variant: applies `arg2` to the three
/// models it owns - its own task's and the two helper tasks'. Bit 0 selects
/// `TmdObject.flags` 0 (shown) vs 0x80 (hidden); bit 1 ORs in 0x4.
s32 func_actor_143900_80133254(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* own    = D_actor_143900_801496C8->extra.tmd;
    TmdObject* first  = D_actor_143900_801496C4->helper1->extra.tmd;
    TmdObject* second = D_actor_143900_801496C4->helper2->extra.tmd;

    if (arg2 & 1) {
        own->flags    = 0;
        first->flags  = 0;
        second->flags = 0;
    } else {
        own->flags    = TMD_OBJECT_HIDDEN;
        first->flags  = TMD_OBJECT_HIDDEN;
        second->flags = TMD_OBJECT_HIDDEN;
    }
    if (arg2 & 2) {
        own->flags    |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        first->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        second->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Message 0x7D4 handler of the second variant: seeds the task's `TmdObject`
/// coordinate frame from `placement`. Only the yaw is used, remembered in the
/// work block and applied with `Gfx_RotMatrixY`, then the three longs become
/// the coordinate's translation.
s32 func_actor_143900_801332E4(Task* task, s32 arg1, GpXformArg* placement)
{
    GfxCoord* coord;
    u16       yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_143900_801496C4->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Message 0x7DB handler of the second variant: the payload's halfword at 0x2
/// picks which of the two helper tasks' models is shown - 0 shows the second
/// (`helper2`) and hides the first, 1 the reverse; any other value leaves
/// both.
s32 func_actor_143900_80133360(Task* task, s32 arg1, ActorCommand* msg)
{
    TmdObject* first;
    TmdObject* second;

    first  = D_actor_143900_801496C4->helper1->extra.tmd;
    second = D_actor_143900_801496C4->helper2->extra.tmd;
    switch (msg->command) {
        case 0:
            second->flags = 0;
            first->flags  = TMD_OBJECT_HIDDEN;
            break;
        case 1:
            first->flags  = 0;
            second->flags = TMD_OBJECT_HIDDEN;
            break;
    }
    return 0;
}

/// Message 0x7DD handler of the second variant: turns the model to face
/// `target` -- away from it in mode 1 -- and stores the per-step distance of the
/// walk the update then performs: the planar distance over 60 steps in mode 0,
/// 15 in mode 1 and 25 otherwise.
s32 func_actor_143900_801333C4(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GfxCoord*        coord;
    Actor461800Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = task->extra.tmd->coords;
    work                    = (Actor461800Work*)task->work;
    D_actor_143900_801496CC = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->st.yaw            = angle;
    if (D_actor_143900_801496CC == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (D_actor_143900_801496CC) {
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
