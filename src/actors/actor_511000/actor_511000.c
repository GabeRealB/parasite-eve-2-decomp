#include "actors/actor_511000.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/item_pickup.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

extern GpImgRec D_actor_511000_80146F94[2];

extern GpImgRec D_actor_511000_80146C74[2];

extern GpImgRec D_actor_511000_801472B4[2];

/// Work block of the enemy task, reached by its model-attach children through
/// the parent task's `Task::work`. The spawn handler
/// `func_actor_511000_80133958` allocates it (`memCalloc(0x488, 0)`), hands
/// `anim` / `slots` / `field_30C` to `func_800B3F84`, and points its own model
/// at the two matrices; the three children it spawns do the same.
typedef struct Actor511000ParentWork {
    /* 0x000 */ AnimationContext anim;
    /* 0x014 */ AnimationSlot    slots[1];
    /* 0x03C */ byte             pad_3C[0x2D0];
    /* 0x30C */ byte             field_30C[0x130];
    /* 0x43C */ MATRIX           field_43C; ///< colour matrix, handed to TmdObject::colorMtx
    /* 0x45C */ MATRIX           field_45C; ///< light matrix, handed to TmdObject::lightMtx
    /* 0x47C */ s32              field_47C; ///< cleared by the spawn handler
    /* 0x480 */ s16              field_480; ///< frame counter; fades both matrices every third tick in state 3+
    /* 0x482 */ byte             pad_482[6];
} Actor511000ParentWork;
STATIC_ASSERT_SIZEOF(Actor511000ParentWork, 0x488);

/// Work block the task running `D_actor_511000_80131E48` parks in
/// `Task::work`; its spawn state allocates it with `memCalloc(0x70, 0)`.
/// `light` / `color` are the matrices the model's `lightMtx` / `colorMtx`
/// point at. `field_8` is the `Tmd_FreeBuffers` countdown (-1 disables it);
/// `field_C` is the 16-colour CLUT published through
/// `D_actor_511000_80147EA4[0].data`, written byte by byte as little-endian 15-bit
/// colours by the palette fade `func_actor_511000_80132E6C`, which steps
/// `field_2C` and holds on `field_2E`. `field_2F` latches once the message-1
/// children have been spawned.
typedef struct Actor511000Work {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ s32  field_8;
    /* 0x0C */ union {
        u8     bytes[0x20];
        u_long words[8];
    } field_C;
    /* 0x2C */ s16    field_2C;
    /* 0x2E */ s8     field_2E;
    /* 0x2F */ s8     field_2F;
    /* 0x30 */ MATRIX light;
    /* 0x50 */ MATRIX color;
} Actor511000Work;
STATIC_ASSERT_SIZEOF(Actor511000Work, 0x70);

/// Work block `func_actor_511000_80132480` allocates (`memCalloc(0x4D4, 0)`)
/// and parks in that task's `Task::work`. Its front is the animation state the
/// animation message handler drives: the context, 20 slots and the pose
/// buffer handed to `func_800B3F84`. Its light/color pair is republished onto
/// model part 1, not the root coordinate.
typedef struct Actor511000Work2 {
    /* 0x000 */ ActorAnimRig20 rig;
    /* 0x474 */ s32            field_474; ///< nonzero while the tick state steps animation slots 1..19
    /* 0x478 */ s32            field_478; ///< animation id the slots were last restarted on; -1 out of the spawn handler
    /* 0x47C */ s32            field_47C; ///< animation source last loaded; also the id `func_actor_511000_80133DEC` resets slots to
    /* 0x480 */ union {
        s32 word;                         ///< seeded to -1 whole by the spawn handler
        s16 half;                         ///< the halfword `func_actor_511000_80133DEC` clears after the slot reseed
    } field_480;
    /* 0x484 */ MATRIX light;
    /* 0x4A4 */ MATRIX color;
    /* 0x4C4 */ Task*  field_4C4; ///< task spawned from the table's index 1
    /* 0x4C8 */ Task*  field_4C8; ///< task spawned from the table's index 2
    /* 0x4CC */ s16    field_4CC; ///< set to 1 alongside `field_4D0` by the message-0x7E0 handler's mode 3
    /* 0x4CE */ u16    field_4CE; ///< upload countdown the texture-upload state runs down, reloaded from `field_4CC` on underflow
    /* 0x4D0 */ s16    field_4D0; ///< texture-upload step in progress, 0 when idle
    /* 0x4D2 */ s16    field_4D2; ///< frame counter of the tick state's mode-1 effect; cleared by the spawn handler
} Actor511000Work2;
STATIC_ASSERT_SIZEOF(Actor511000Work2, 0x4D4);

static void func_actor_511000_80131E78(Task* arg0);
static void func_actor_511000_80132048(Task* arg0);
static void func_actor_511000_801321A8(Task* task);
static void func_actor_511000_80132224(Task* task);
static void func_actor_511000_80132284(Task* task);
static void func_actor_511000_80132390(Task* task);
static void func_actor_511000_80132480(Task* task);
static void func_actor_511000_801325A4(Task* task);
static void func_actor_511000_801329C4(Task* task);
static void func_actor_511000_80132B14(Task* task, CVECTOR* col, s8* rgb);
static void func_actor_511000_80133034(Task* task);
static void func_actor_511000_801330F0(Task* task);
static void func_actor_511000_80133220(Task* task);
static void func_actor_511000_80133240(Task* task);
static void func_actor_511000_801332E4(Task* task);
static void func_actor_511000_801333A4(Task* task);
static void func_actor_511000_801333C4(Task* task);
static void func_actor_511000_80133498(Task* task);
static void func_actor_511000_801336E0(Task* task, SVECTOR* rots, SVECTOR* trans, s32 index);
static void func_actor_511000_80133760(Task* task);
static void func_actor_511000_801337F0(Task* task);
static void func_actor_511000_80133958(GpEnemy* enemy, Task* task);
static void func_actor_511000_80133B80(GpEnemy* enemy, Task* task);
static void func_actor_511000_80133F48(GpEnemy* enemy, Task* task);
static void func_actor_511000_80133F88(GpEnemy* enemy, Task* task);
static void func_actor_511000_8013401C(GpEnemy* enemy, Task* task);
static void func_actor_511000_8013405C(GpEnemy* enemy, Task* task);
static void func_actor_511000_801340F0(GpEnemy* enemy, Task* task);
static void func_actor_511000_80134130(GpEnemy* enemy, Task* task);

/// State table of a child chained under a part of its spawner's model: the
/// attach state, an empty tick and the kill.
static const TaskFuncTable3 D_actor_511000_80131E24 = {
    func_actor_511000_801321A8,
    func_actor_511000_80132224,
    taskKill,
};

/// State table of a child chained under a part of its spawner's model that
/// also follows the spawner's visibility: the attach state, the flag-mirroring
/// tick and the kill.
static const TaskFuncTable3 D_actor_511000_80131E30 = {
    func_actor_511000_80132284,
    func_actor_511000_80132390,
    taskKill,
};

/// State table of the task that owns the `Actor511000Work2` block: its spawn
/// state, the per-frame tick and the enemy task exit.
static const TaskFuncTable3 D_actor_511000_80131E3C = {
    func_actor_511000_80132480,
    func_actor_511000_80131E78,
    Gp_EnemyTaskExit,
};

/// State table of the task that owns the `Actor511000Work` block: its spawn
/// state, the per-frame tick and the kill.
static const TaskFuncTable3 D_actor_511000_80131E48 = {
    func_actor_511000_80133034,
    func_actor_511000_801330F0,
    func_actor_511000_80133220,
};

/// State table of a child placed at a translation preset under its spawner:
/// the attach state, the spinning tick and the kill.
static const TaskFuncTable3 D_actor_511000_80131E54 = {
    func_actor_511000_80133240,
    func_actor_511000_801332E4,
    func_actor_511000_801333A4,
};

/// State table of a child posed from the kill-countdown rotations: the attach
/// state, the tick that follows the countdown and the kill.
static const TaskFuncTable3 D_actor_511000_80131E60 = {
    func_actor_511000_801333C4,
    func_actor_511000_801329C4,
    func_actor_511000_80133498,
};

/// The enemy's three state handlers - spawn, per-frame tick and teardown.
static const GpEnemyTaskFuncTable3 D_actor_511000_80131E6C = {
    func_actor_511000_80133958,
    func_actor_511000_80133B80,
    Gp_DestroyEnemy,
};

/// Camera path `func_actor_511000_801330F0` walks once the session reaches
/// mode 0x18, one 0x24-byte `GpViewRec` per step of the kill countdown: the
/// rotation and projection plane repeat down the table while the translation
/// descends, so the spawn of a view task per index pans the camera as the
/// actor goes down. Handed straight to `Gp_TrySpawnViewTask`, exactly as
/// `Gp_SpawnViewTasks` hands its own stage record.
extern GpViewRec D_actor_511000_80147EE4[];

/// The three texture records the tick state's upload steps and the
/// message-0x7E0 handler post. Each is a lone `GpImgRec` whose 0x18x0x10
/// source rect repeats the size the users' scratch `RECT` carries and whose
/// `data` points at its pixel blob.

/// Animation sources the animation message handler selects by index.
extern AnimationSet*  D_actor_511000_801472D4[4];
extern AnimationSet** D_actor_511000_801472E4[1];

/// Spawn table `func_actor_511000_80132480` starts its two child tasks from,
/// and the message table it parks in `Task::msgTable`.
extern TaskDesc D_actor_511000_801472E8[];
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*);
        s32 (*call1)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call2)(Task*, s32, ActorCommand* request);
        s32 (*call3)(Task*, s32, ActorTransform*);
        s32 (*call4)(Task*, s32, s32);
    } handler;
} Actor511000MessageEntry;
STATIC_ASSERT_SIZEOF(Actor511000MessageEntry, 8);

extern Actor511000MessageEntry D_actor_511000_8014730C[6];

/// Offset `Gp_SpawnEff` places the tick state's effect at.
extern SVECTOR D_actor_511000_8014733C;

extern SVECTOR D_actor_511000_80147344[];
extern SVECTOR D_actor_511000_80147704[];
extern SVECTOR D_actor_511000_80147AC4[];
// Color/byte updates and the GPU upload share the same backing storage.
typedef union {
    u8     bytes[32];
    u_long words[8];
} Actor511000Palette;
STATIC_ASSERT_SIZEOF(Actor511000Palette, 32);

extern Actor511000Palette D_actor_511000_80147E84;
extern u8                 D_actor_511000_80147EC4[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*);
        s32 (*call1)(Task*, s32, ActorTransform*, s32);
        s32 (*call2)(Task*, s32, s32);
    } handler;
} Actor511000MsgEntry;
STATIC_ASSERT_SIZEOF(Actor511000MsgEntry, 8);

extern Actor511000MsgEntry D_actor_511000_80148FC4[];

/// Translation presets `func_actor_511000_80133760` copies onto the root
/// coordinate; `Task::spawnArg1` selects the entry.
extern SVECTOR D_actor_511000_80148FE4[];

extern CVECTOR D_actor_511000_80149004[];
extern DVECTOR D_actor_511000_80149014[];

/// Spawn table and per-child args for the children spawned on message 1.
extern TaskDesc D_actor_511000_80139924[];
extern s32      D_actor_511000_80149054[];

/// Spawn table for the three children `func_actor_511000_80133958` creates.
extern TaskDesc D_actor_511000_80155070[];
/// Message table and animation data `func_actor_511000_80133958` installs.
extern Actor511000MessageEntry D_actor_511000_801550A0[4];
extern AnimationSet*           D_actor_511000_801550C0[4];

extern TmdSource D_actor_511000_80142554;
extern TmdSource D_actor_511000_80142AAC;
extern TmdSource D_actor_511000_80142C90;
s32              func_actor_511000_80132604(Task*, s32, AnimationPlayRequest*, s32);
s32              func_actor_511000_80132724(Task*, s32, ActorTransform* args);
s32              func_actor_511000_801327A0(Task*, s32, s32);
s32              func_actor_511000_8013287C(Task*, s32, ActorCommand* msg);
s32              func_actor_511000_80132904(Task*, s32, s32);
void             func_actor_511000_80132150(Task*);
void             func_actor_511000_8013222C(Task*);
void             func_actor_511000_80132428(Task*);

extern AnimationSet D_actor_511000_8014303C;
extern AnimationSet D_actor_511000_80144EF4;
extern AnimationSet D_actor_511000_8014694C;

extern Actor511000Palette D_actor_511000_80147E84;

s32  func_actor_511000_801334B8(Task*);
s32  func_actor_511000_801334C4(Task*, s32, ActorTransform* args, s32);
s32  func_actor_511000_80133554(Task*, s32, s32);
s32  func_actor_511000_80133DEC(Task*, s32, AnimationPlayRequest*);
s32  func_actor_511000_80133E48(Task*, s32, ActorTransform* args);
s32  func_actor_511000_80133EAC(Task*, s32, s32);
void func_actor_511000_80133D90(Task*);
void func_actor_511000_80133EF4(Task*);
void func_actor_511000_80133FC8(Task*);
void func_actor_511000_8013409C(Task*);

extern TmdSource D_actor_511000_8013BB10;
extern TmdSource D_actor_511000_8013BEF8;
extern TmdSource D_actor_511000_8013C058;
extern TmdSource D_actor_511000_8013C65C;
void             func_actor_511000_80133850(Task*);
void             func_actor_511000_801338A8(Task*);
void             func_actor_511000_80133900(Task*);

AnimationPackedPose D_actor_511000_80134170[84] = {
#include "assets/actor_511000_animation_04CC8_bank1.inc"
};

AnimationPackedRotation D_actor_511000_80134560[1028] = {
#include "assets/actor_511000_animation_04CC8_bank4.inc"
};

AnimationRecord D_actor_511000_80135570[1364] = {
#include "assets/actor_511000_animation_04CC8_records.inc"
};

u16 D_actor_511000_80136AC0[20] = {
#include "assets/actor_511000_animation_04CC8_indices.inc"
};

AnimationSet D_actor_511000_80136AE8 = {
    D_actor_511000_80135570,
    D_actor_511000_80136AC0,
    { NULL, D_actor_511000_80134170, NULL, NULL, D_actor_511000_80134560, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_511000_80136B10[75] = {
#include "assets/actor_511000_animation_07ADC_bank1.inc"
};

AnimationPackedRotation D_actor_511000_80136E94[1201] = {
#include "assets/actor_511000_animation_07ADC_bank4.inc"
};

AnimationRecord D_actor_511000_80138158[1503] = {
#include "assets/actor_511000_animation_07ADC_records.inc"
};

u16 D_actor_511000_801398D4[20] = {
#include "assets/actor_511000_animation_07ADC_indices.inc"
};

AnimationSet D_actor_511000_801398FC = {
    D_actor_511000_80138158,
    D_actor_511000_801398D4,
    { NULL, D_actor_511000_80136B10, NULL, NULL, D_actor_511000_80136E94, NULL, NULL, NULL },
};

TaskDesc D_actor_511000_80139924[4] = {
    { (TASK_BODY_TMD | 0x100), 192, func_actor_511000_80133850, { .model = &D_actor_511000_8013BB10 } },
    { (TASK_BODY_TMD | 0x100), 192, func_actor_511000_801338A8, { .model = &D_actor_511000_8013BEF8 } },
    { (TASK_BODY_TMD | 0x100), 192, func_actor_511000_801338A8, { .model = &D_actor_511000_8013C058 } },
    { (TASK_BODY_TMD | 0x100), 192, func_actor_511000_80133900, { .model = &D_actor_511000_8013C65C } },
};

TmdBone D_actor_511000_80139954[1] = {
#include "assets/actor_511000_model_09CF0_skeleton.inc"
};

u32 D_actor_511000_80139978[1] = {
#include "assets/actor_511000_model_09CF0_partVerts.inc"
};

SVECTOR D_actor_511000_8013997C[262] = {
#include "assets/actor_511000_model_09CF0_verts.inc"
};

SVECTOR D_actor_511000_8013A1AC[163] = {
#include "assets/actor_511000_model_09CF0_normals.inc"
};

u32 D_actor_511000_8013A6C4[1299] = {
#include "assets/actor_511000_model_09CF0_stream.inc"
};

TmdSource D_actor_511000_8013BB10 = {
    0,
    9368,
    0,
    1,
    D_actor_511000_80139978,
    D_actor_511000_8013997C,
    D_actor_511000_8013A1AC,
    D_actor_511000_80139954,
    D_actor_511000_8013A6C4,
};

TmdBone D_actor_511000_8013BB34[1] = {
#include "assets/actor_511000_model_0A0D8_skeleton.inc"
};

u32 D_actor_511000_8013BB58[1] = {
#include "assets/actor_511000_model_0A0D8_partVerts.inc"
};

SVECTOR D_actor_511000_8013BB5C[59] = {
#include "assets/actor_511000_model_0A0D8_verts.inc"
};

SVECTOR D_actor_511000_8013BD34[1] = {
#include "assets/actor_511000_model_0A0D8_normals.inc"
};

u32 D_actor_511000_8013BD3C[111] = {
#include "assets/actor_511000_model_0A0D8_stream.inc"
};

TmdSource D_actor_511000_8013BEF8 = {
    0,
    840,
    0,
    1,
    D_actor_511000_8013BB58,
    D_actor_511000_8013BB5C,
    D_actor_511000_8013BD34,
    D_actor_511000_8013BB34,
    D_actor_511000_8013BD3C,
};

TmdBone D_actor_511000_8013BF1C[1] = {
#include "assets/actor_511000_model_0A238_skeleton.inc"
};

u32 D_actor_511000_8013BF40[1] = {
#include "assets/actor_511000_model_0A238_partVerts.inc"
};

SVECTOR D_actor_511000_8013BF44[12] = {
#include "assets/actor_511000_model_0A238_verts.inc"
};

SVECTOR D_actor_511000_8013BFA4[2] = {
#include "assets/actor_511000_model_0A238_normals.inc"
};

u32 D_actor_511000_8013BFB4[41] = {
#include "assets/actor_511000_model_0A238_stream.inc"
};

TmdSource D_actor_511000_8013C058 = {
    0,
    280,
    0,
    1,
    D_actor_511000_8013BF40,
    D_actor_511000_8013BF44,
    D_actor_511000_8013BFA4,
    D_actor_511000_8013BF1C,
    D_actor_511000_8013BFB4,
};

TmdBone D_actor_511000_8013C07C[1] = {
#include "assets/actor_511000_model_0A83C_skeleton.inc"
};

u32 D_actor_511000_8013C0A0[1] = {
#include "assets/actor_511000_model_0A83C_partVerts.inc"
};

SVECTOR D_actor_511000_8013C0A4[36] = {
#include "assets/actor_511000_model_0A83C_verts.inc"
};

SVECTOR D_actor_511000_8013C1C4[15] = {
#include "assets/actor_511000_model_0A83C_normals.inc"
};

u32 D_actor_511000_8013C23C[264] = {
#include "assets/actor_511000_model_0A83C_stream.inc"
};

TmdSource D_actor_511000_8013C65C = {
    0,
    1576,
    0,
    1,
    D_actor_511000_8013C0A0,
    D_actor_511000_8013C0A4,
    D_actor_511000_8013C1C4,
    D_actor_511000_8013C07C,
    D_actor_511000_8013C23C,
};

TmdBone D_actor_511000_8013C680[20] = {
#include "assets/actor_511000_model_10734_skeleton.inc"
};

u32 D_actor_511000_8013C950[20] = {
#include "assets/actor_511000_model_10734_partVerts.inc"
};

SVECTOR D_actor_511000_8013C9A0[386] = {
#include "assets/actor_511000_model_10734_verts.inc"
};

SVECTOR D_actor_511000_8013D5B0[385] = {
#include "assets/actor_511000_model_10734_normals.inc"
};

u32 D_actor_511000_8013E1B8[4327] = {
#include "assets/actor_511000_model_10734_stream.inc"
};

TmdSource D_actor_511000_80142554 = {
    0,
    23980,
    6012,
    20,
    D_actor_511000_8013C950,
    D_actor_511000_8013C9A0,
    D_actor_511000_8013D5B0,
    D_actor_511000_8013C680,
    D_actor_511000_8013E1B8,
};

TmdBone D_actor_511000_80142578[1] = {
#include "assets/actor_511000_model_10C8C_skeleton.inc"
};

u32 D_actor_511000_8014259C[1] = {
#include "assets/actor_511000_model_10C8C_partVerts.inc"
};

SVECTOR D_actor_511000_801425A0[28] = {
#include "assets/actor_511000_model_10C8C_verts.inc"
};

SVECTOR D_actor_511000_80142680[28] = {
#include "assets/actor_511000_model_10C8C_normals.inc"
};

u32 D_actor_511000_80142760[211] = {
#include "assets/actor_511000_model_10C8C_stream.inc"
};

TmdSource D_actor_511000_80142AAC = {
    0,
    1464,
    0,
    1,
    D_actor_511000_8014259C,
    D_actor_511000_801425A0,
    D_actor_511000_80142680,
    D_actor_511000_80142578,
    D_actor_511000_80142760,
};

TmdBone D_actor_511000_80142AD0[1] = {
#include "assets/actor_511000_model_10E70_skeleton.inc"
};

u32 D_actor_511000_80142AF4[1] = {
#include "assets/actor_511000_model_10E70_partVerts.inc"
};

SVECTOR D_actor_511000_80142AF8[14] = {
#include "assets/actor_511000_model_10E70_verts.inc"
};

u32 D_actor_511000_80142B68[74] = {
#include "assets/actor_511000_model_10E70_stream.inc"
};

TmdSource D_actor_511000_80142C90 = {
    0,
    504,
    0,
    1,
    D_actor_511000_80142AF4,
    D_actor_511000_80142AF8,
    &D_actor_511000_80142AF8[14],
    D_actor_511000_80142AD0,
    D_actor_511000_80142B68,
};

AnimationPackedPose D_actor_511000_80142CB4[5] = {
#include "assets/actor_511000_animation_1121C_bank1.inc"
};

AnimationPackedRotation D_actor_511000_80142CF0[80] = {
#include "assets/actor_511000_animation_1121C_bank4.inc"
};

AnimationRecord D_actor_511000_80142E30[121] = {
#include "assets/actor_511000_animation_1121C_records.inc"
};

u16 D_actor_511000_80143014[20] = {
#include "assets/actor_511000_animation_1121C_indices.inc"
};

AnimationSet D_actor_511000_8014303C = {
    D_actor_511000_80142E30,
    D_actor_511000_80143014,
    { NULL, D_actor_511000_80142CB4, NULL, NULL, D_actor_511000_80142CF0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_511000_80143064[118] = {
#include "assets/actor_511000_animation_130D4_bank1.inc"
};

AnimationPackedRotation D_actor_511000_801435EC[422] = {
#include "assets/actor_511000_animation_130D4_bank4.inc"
};

AnimationRecord D_actor_511000_80143C84[1170] = {
#include "assets/actor_511000_animation_130D4_records.inc"
};

u16 D_actor_511000_80144ECC[20] = {
#include "assets/actor_511000_animation_130D4_indices.inc"
};

AnimationSet D_actor_511000_80144EF4 = {
    D_actor_511000_80143C84,
    D_actor_511000_80144ECC,
    { NULL, D_actor_511000_80143064, NULL, NULL, D_actor_511000_801435EC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_511000_80144F1C[19] = {
#include "assets/actor_511000_animation_14B2C_bank1.inc"
};

AnimationPackedRotation D_actor_511000_80145000[703] = {
#include "assets/actor_511000_animation_14B2C_bank4.inc"
};

AnimationRecord D_actor_511000_80145AFC[906] = {
#include "assets/actor_511000_animation_14B2C_records.inc"
};

u16 D_actor_511000_80146924[20] = {
#include "assets/actor_511000_animation_14B2C_indices.inc"
};

AnimationSet D_actor_511000_8014694C = {
    D_actor_511000_80145AFC,
    D_actor_511000_80146924,
    { NULL, D_actor_511000_80144F1C, NULL, NULL, D_actor_511000_80145000, NULL, NULL, NULL },
};

u_long D_actor_511000_80146974[192] = {
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x64838DA2,
    0x48555D4F,
    0x49494949,
    0x54554849,
    0x65657C5E,
    0x60737A7A,
    0x48485555,
    0x60545555,
    0x64795672,
    0xA2A28E76,
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x65838DA2,
    0x4948546F,
    0x45454545,
    0x54484949,
    0x7766705E,
    0x545F6A77,
    0x48484848,
    0x54554848,
    0x6479725F,
    0xA2A28262,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x65839AA1,
    0x4948545C,
    0x45454545,
    0x54484945,
    0x7777705E,
    0x555D7C8A,
    0x48484848,
    0x55484848,
    0x65687D53,
    0xA2A28276,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x64828DA1,
    0x49485451,
    0x45454549,
    0x60554949,
    0x6877686F,
    0x545F7B77,
    0x48484848,
    0x54554848,
    0x777A705F,
    0xA2A29A83,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x789A99A1,
    0x605F706A,
    0x55485554,
    0x72605455,
    0x6A86666A,
    0x5E577787,
    0x54545454,
    0x6F605454,
    0x8F8F786A,
    0xA2A2998E,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x96A6A3A1,
    0x9B639098,
    0x7C7C9489,
    0x776A7C7C,
    0x85829388,
    0x66878E8F,
    0x7A7A6A67,
    0xA38E898A,
    0x98A5A0A6,
    0xA2A2A6A9,
    0xFFFFFFFF,
    0xB5FFFFFF,
    0xA096A6A3,
    0x989F9F98,
    0xAB919090,
    0x99A36363,
    0x77878F99,
    0x99999A87,
    0x9191A6A1,
    0xAD9F9890,
    0x9FADADAD,
    0xA2A7969E,
    0xFFFFFFFF,
    0x61FFFFFF,
    0xA2A3A3A2,
    0x96A7A3A2,
    0x61619F98,
    0x9F616161,
    0x77788FA1,
    0x9E998778,
    0x6161AEAE,
    0xA99EA59F,
    0xA1A1A7A6,
    0xA2A1A1A1,
    0xFFFFFFFF,
    0x9FFFFFFF,
    0xA6A2A28D,
    0xADA5ADA5,
    0xB1B1B1AE,
    0xAEAEADAE,
    0x65929AA9,
    0xADA68766,
    0xADA5A5AD,
    0xADADADAD,
    0x8B969EA5,
    0xA2A2A2A2,
    0xFFFFFFFF,
    0xA5FFFFFF,
    0xA5958C8D,
    0xFFB9C29C,
    0xAE61B5FF,
    0xA9A9A9A5,
    0x7C788F95,
    0xA7958785,
    0x9FA5A68B,
    0xFFFFB561,
    0x9890C5BF,
    0x8C8C9795,
    0xFFFFFFFF,
    0xABFFFFFF,
    0x9B8C828D,
    0xFFB21716,
    0xCECCFFFF,
    0x9595A890,
    0x7B65864E,
    0x998D8665,
    0xCEACA997,
    0xB7FFFFC4,
    0x9BCCD5D0,
    0x87868281,
    0xFFFFFFFF,
    0x9BB6FFFF,
    0x828F828D,
    0x9894028A,
    0x1CCB0B1,
    0x81958B9B,
    0x72677882,
    0x998E8568,
    0xCC9B95A2,
    0x98AE7EC6,
    0x87891601,
    0x8A6A6686,
    0xFFFFFFFF,
    0x8F98FFFF,
    0x787A788E,
    0x8F868282,
    0x9388938E,
    0x9A999A8E,
    0x6F706488,
    0x8E86646C,
    0x9A9A8E8E,
    0x87879A9B,
    0x66788687,
    0x7A735E73,
    0xFFFFFFFF,
    0x8691FFFF,
    0x4F746A86,
    0x72576870,
    0x6A705E74,
    0x87868A8A,
    0x7D736777,
    0x87787072,
    0x8A898987,
    0x6C6C707A,
    0x605E6C57,
    0x6A605454,
    0xFFFFFFFF,
    0x7763FFFF,
    0x535F6A77,
    0x55555460,
    0x54554848,
    0x776A7353,
    0x5D7D577A,
    0x788A705F,
    0x72707C7A,
    0x54545460,
    0x55555454,
    0x7A735455,
    0xFFFFFFFF,
    0x6691FFFF,
    0x60737A77,
    0x45454854,
    0x55484945,
    0x7C735354,
    0x545F577A,
    0x7A7A7354,
    0x5453737C,
    0x48494848,
    0x55554848,
    0x777A7254,
};

GpImgRec D_actor_511000_80146C74[2] = {
    { 0, 0, { 0, 0, 24, 16 }, D_actor_511000_80146974 },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_511000_80146C94[192] = {
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x64838DA2,
    0x48555D4F,
    0x49494949,
    0x54554849,
    0x65657C5E,
    0x60737A7A,
    0x48485555,
    0x60545555,
    0x64795672,
    0xA2A28E76,
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x65838DA2,
    0x4948546F,
    0x45454545,
    0x54484949,
    0x7766705E,
    0x545F6A77,
    0x48484848,
    0x54554848,
    0x6479725F,
    0xA2A28262,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x65839AA1,
    0x4948545C,
    0x45454545,
    0x54484945,
    0x7777705E,
    0x555D7C8A,
    0x48484848,
    0x55484848,
    0x65687D53,
    0xA2A28276,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x64828DA1,
    0x49485451,
    0x45454549,
    0x60554949,
    0x6877686F,
    0x545F7B77,
    0x48484848,
    0x54554848,
    0x777A705F,
    0xA2A29A83,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x789A99A1,
    0x605F706A,
    0x55485554,
    0x72605455,
    0x6A86666A,
    0x5E577787,
    0x54545454,
    0x6F605454,
    0x8F8F786A,
    0xA2A2998E,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x96A6A3A1,
    0x9B639098,
    0x7C7C9489,
    0x776A7C7C,
    0x85829388,
    0x66878E8F,
    0x7A7A6A67,
    0xA38E898A,
    0x98A5A0A6,
    0xA2A2A6A9,
    0xFFFFFFFF,
    0xB5FFFFFF,
    0xA096A6A3,
    0x989F9F98,
    0xAB919090,
    0x99A36363,
    0x77878F99,
    0x99999A87,
    0x9191A6A1,
    0xAD9F9890,
    0x9FADADAD,
    0xA2A7969E,
    0xFFFFFFFF,
    0x61FFFFFF,
    0xA2A3A3A2,
    0x96A7A3A2,
    0x61619F98,
    0x9F616161,
    0x77788FA1,
    0x9E998778,
    0x6161AEAE,
    0xA99EA59F,
    0xA1A1A7A6,
    0xA2A1A1A1,
    0xFFFFFFFF,
    0x9FFFFFFF,
    0xA2A2A28D,
    0x8C8C8C8C,
    0xA9A9A68C,
    0xAEAEA9A9,
    0x65929AA9,
    0xADA68766,
    0xA9A9A5AD,
    0x9595A9A9,
    0x95959595,
    0xA2A2A2A2,
    0xFFFFFFFF,
    0xA5FFFFFF,
    0x95958C8D,
    0xB0C2A995,
    0xC2C2B0B0,
    0xA9A9A9A9,
    0x7C788F95,
    0xA7958785,
    0xA9A9A68B,
    0xC2B0B0C2,
    0x959595C2,
    0x8C8C9795,
    0xFFFFFFFF,
    0xABFFFFFF,
    0xC28C828D,
    0xB0B0B0B0,
    0xB0B0B0B0,
    0x9595A8B0,
    0x7B65864E,
    0x998D8665,
    0xB0B0A997,
    0xB0B0B0B0,
    0x9BC2B0B0,
    0x87868281,
    0xFFFFFFFF,
    0x9BB6FFFF,
    0x828F828D,
    0x9894028A,
    0x1CCB0B1,
    0x81958B9B,
    0x72677882,
    0x998E8568,
    0xCC9B95A2,
    0x98AE7EC6,
    0x87891601,
    0x8A6A6686,
    0xFFFFFFFF,
    0x8F98FFFF,
    0x787A788E,
    0x8F868282,
    0x9388938E,
    0x9A999A8E,
    0x6F706488,
    0x8E86646C,
    0x9A9A8E8E,
    0x879A9A9A,
    0x66788687,
    0x7A735E73,
    0xFFFFFFFF,
    0x8691FFFF,
    0x4F746A86,
    0x72576870,
    0x6A705E74,
    0x87868A8A,
    0x7D736777,
    0x87787072,
    0x8A898987,
    0x6C6C707A,
    0x605E6C57,
    0x6A605454,
    0xFFFFFFFF,
    0x7763FFFF,
    0x535F6A77,
    0x55555460,
    0x54554848,
    0x776A7353,
    0x5D7D577A,
    0x788A705F,
    0x72707C7A,
    0x54545460,
    0x55555454,
    0x7A735455,
    0xFFFFFFFF,
    0x6691FFFF,
    0x60737A77,
    0x45454854,
    0x55484945,
    0x7C735354,
    0x545F577A,
    0x7A7A7354,
    0x5453737C,
    0x48494848,
    0x55554848,
    0x777A7254,
};

GpImgRec D_actor_511000_80146F94[2] = {
    { 0, 0, { 0, 0, 24, 16 }, D_actor_511000_80146C94 },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_511000_80146FB4[192] = {
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x64838DA2,
    0x48555D4F,
    0x49494949,
    0x54554849,
    0x65657C5E,
    0x60737A7A,
    0x48485555,
    0x60545555,
    0x64795672,
    0xA2A28E76,
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x65838DA2,
    0x4948546F,
    0x45454545,
    0x54484949,
    0x7766705E,
    0x545F6A77,
    0x48484848,
    0x54554848,
    0x6479725F,
    0xA2A28262,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x65839AA1,
    0x4948545C,
    0x45454545,
    0x54484945,
    0x7777705E,
    0x555D7C8A,
    0x48484848,
    0x55484848,
    0x65687D53,
    0xA2A28276,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x64828DA1,
    0x49485451,
    0x45454549,
    0x60554949,
    0x6877686F,
    0x545F7B77,
    0x48484848,
    0x54554848,
    0x777A705F,
    0xA2A29A83,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x789A99A1,
    0x605F706A,
    0x55485554,
    0x72605455,
    0x6A86666A,
    0x5E577787,
    0x54545454,
    0x6F605454,
    0x8F8F786A,
    0xA2A2998E,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x96A6A3A1,
    0x9B639098,
    0x7C7C9489,
    0x776A7C7C,
    0x85829388,
    0x66878E8F,
    0x7A7A6A67,
    0xA38E898A,
    0x98A5A0A6,
    0xA2A2A6A9,
    0xFFFFFFFF,
    0xB5FFFFFF,
    0xA096A6A3,
    0x989F9F98,
    0xAB919090,
    0x99A36363,
    0x77878F99,
    0x99999A87,
    0x9191A6A1,
    0xAD9F9890,
    0x9FADADAD,
    0xA2A7969E,
    0xFFFFFFFF,
    0x61FFFFFF,
    0xA2A3A3A2,
    0x96A7A3A2,
    0x61619F98,
    0x9F616161,
    0x77788FA1,
    0x9E998778,
    0x6161AEAE,
    0xA99EA59F,
    0xA1A1A7A6,
    0xA2A1A1A1,
    0xFFFFFFFF,
    0x9FFFFFFF,
    0xA2A2A28D,
    0xA2A2A2A2,
    0xB19898A2,
    0xAEAEADAE,
    0x65929AA9,
    0xADA68766,
    0xADA5A5AD,
    0x8B8B9898,
    0xA2A2A2A2,
    0xA2A2A2A2,
    0xFFFFFFFF,
    0xA5FFFFFF,
    0xA2958C8D,
    0x788FA2A2,
    0xA2A28F8F,
    0xA9A9A9A6,
    0x7C788F95,
    0xA7958785,
    0xA2A2A7A7,
    0x8F788F8F,
    0x959595A2,
    0x8C8C9795,
    0xFFFFFFFF,
    0xABFFFFFF,
    0x958C828D,
    0x61ADADA9,
    0xAD616161,
    0xA9A99898,
    0x7B65864E,
    0x998D8665,
    0x61AD9897,
    0x61616161,
    0x95A9ADAD,
    0x87868281,
    0xFFFFFFFF,
    0x9BB6FFFF,
    0x828F828D,
    0x9898ADAD,
    0x98A9A9A9,
    0x818198AD,
    0x72677882,
    0x998E8568,
    0x98AD9898,
    0x98A9A9A9,
    0x87ADADAD,
    0x8A6A6686,
    0xFFFFFFFF,
    0x8F98FFFF,
    0x787A788E,
    0x8F868282,
    0x9388938E,
    0x9A999A8E,
    0x6F706488,
    0x8E86646C,
    0x9A9A8E8E,
    0x87879A9B,
    0x66788687,
    0x7A735E73,
    0xFFFFFFFF,
    0x8691FFFF,
    0x4F746A86,
    0x72576870,
    0x6A705E74,
    0x87868A8A,
    0x7D736777,
    0x87787072,
    0x8A898987,
    0x6C6C707A,
    0x605E6C57,
    0x6A605454,
    0xFFFFFFFF,
    0x7763FFFF,
    0x535F6A77,
    0x55555460,
    0x54554848,
    0x776A7353,
    0x5D7D577A,
    0x788A705F,
    0x72707C7A,
    0x54545460,
    0x55555454,
    0x7A735455,
    0xFFFFFFFF,
    0x6691FFFF,
    0x60737A77,
    0x45454854,
    0x55484945,
    0x7C735354,
    0x545F577A,
    0x7A7A7354,
    0x5453737C,
    0x48494848,
    0x55554848,
    0x777A7254,
};

GpImgRec D_actor_511000_801472B4[2] = {
    { 0, 0, { 0, 0, 24, 16 }, D_actor_511000_80146FB4 },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

AnimationSet* D_actor_511000_801472D4[4] = {
    NULL,
    &D_actor_511000_8014303C,
    &D_actor_511000_80144EF4,
    &D_actor_511000_8014694C,
};

AnimationSet** D_actor_511000_801472E4[1] = {
    D_actor_511000_801472D4,
};

TaskDesc D_actor_511000_801472E8[3] = {
    { (TASK_BODY_TMD | 0x100), 192, func_actor_511000_80132428, { .model = &D_actor_511000_80142554 } },
    { TASK_BODY_TMD, 192, func_actor_511000_80132150, { .model = &D_actor_511000_80142AAC } },
    { TASK_BODY_TMD, 192, func_actor_511000_8013222C, { .model = &D_actor_511000_80142C90 } },
};

Actor511000MessageEntry D_actor_511000_8014730C[6] = {
    { 2003, { .call1 = func_actor_511000_80132604 } },
    { 2004, { .call3 = func_actor_511000_80132724 } },
    { 2005, { .call4 = func_actor_511000_801327A0 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call2 = func_actor_511000_8013287C } },
    { 2016, { .call4 = func_actor_511000_80132904 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

SVECTOR D_actor_511000_8014733C = { -30, 440, 140, 0 };

SVECTOR D_actor_511000_80147344[120] = {
    { 0, 0, 0, 0 },
    { 0, -9, 0, 0 },
    { 0, -19, 0, 0 },
    { 0, -30, 0, 0 },
    { 0, -41, 0, 0 },
    { 0, -52, 0, 0 },
    { 0, -64, 0, 0 },
    { 0, -77, 0, 0 },
    { 0, -89, 0, 0 },
    { 0, -102, 0, 0 },
    { 0, -116, 0, 0 },
    { 0, -130, 0, 0 },
    { 0, -144, 0, 0 },
    { 0, -158, 0, 0 },
    { 0, -172, 0, 0 },
    { 0, -187, 0, 0 },
    { 0, -202, 0, 0 },
    { 0, -217, 0, 0 },
    { 0, -232, 0, 0 },
    { 0, -248, 0, 0 },
    { 0, -263, 0, 0 },
    { 0, -279, 0, 0 },
    { 0, -294, 0, 0 },
    { 0, -310, 0, 0 },
    { 0, -325, 0, 0 },
    { 0, -341, 0, 0 },
    { 0, -356, 0, 0 },
    { 0, -372, 0, 0 },
    { 0, -387, 0, 0 },
    { 0, -402, 0, 0 },
    { 0, -417, 0, 0 },
    { 0, -432, 0, 0 },
    { 0, -447, 0, 0 },
    { 0, -461, 0, 0 },
    { 0, -475, 0, 0 },
    { 0, -489, 0, 0 },
    { 0, -503, 0, 0 },
    { 0, -516, 0, 0 },
    { 0, -529, 0, 0 },
    { 0, -542, 0, 0 },
    { 0, -554, 0, 0 },
    { 0, -566, 0, 0 },
    { 0, -577, 0, 0 },
    { 0, -588, 0, 0 },
    { 0, -599, 0, 0 },
    { 0, -609, 0, 0 },
    { 0, -618, 0, 0 },
    { 0, -627, 0, 0 },
    { 0, -635, 0, 0 },
    { 0, -643, 0, 0 },
    { 0, -650, 0, 0 },
    { 0, -657, 0, 0 },
    { 0, -662, 0, 0 },
    { 0, -668, 0, 0 },
    { 0, -672, 0, 0 },
    { 0, -676, 0, 0 },
    { 0, -678, 0, 0 },
    { 0, -680, 0, 0 },
    { 0, -682, 0, 0 },
    { 0, -682, 0, 0 },
    { 0, -682, 0, 0 },
    { 0, -682, 0, 0 },
    { 0, -682, 0, 0 },
    { 0, -681, 0, 0 },
    { 0, -681, 0, 0 },
    { 0, -681, 0, 0 },
    { 0, -680, 0, 0 },
    { 0, -679, 0, 0 },
    { 0, -679, 0, 0 },
    { 0, -678, 0, 0 },
    { 0, -677, 0, 0 },
    { 0, -676, 0, 0 },
    { 0, -675, 0, 0 },
    { 0, -674, 0, 0 },
    { 0, -673, 0, 0 },
    { 0, -672, 0, 0 },
    { 0, -671, 0, 0 },
    { 0, -670, 0, 0 },
    { 0, -669, 0, 0 },
    { 0, -667, 0, 0 },
    { 0, -666, 0, 0 },
    { 0, -665, 0, 0 },
    { 0, -663, 0, 0 },
    { 0, -662, 0, 0 },
    { 0, -661, 0, 0 },
    { 0, -659, 0, 0 },
    { 0, -658, 0, 0 },
    { 0, -657, 0, 0 },
    { 0, -655, 0, 0 },
    { 0, -654, 0, 0 },
    { 0, -652, 0, 0 },
    { 0, -651, 0, 0 },
    { 0, -649, 0, 0 },
    { 0, -648, 0, 0 },
    { 0, -647, 0, 0 },
    { 0, -645, 0, 0 },
    { 0, -644, 0, 0 },
    { 0, -643, 0, 0 },
    { 0, -641, 0, 0 },
    { 0, -640, 0, 0 },
    { 0, -639, 0, 0 },
    { 0, -638, 0, 0 },
    { 0, -636, 0, 0 },
    { 0, -635, 0, 0 },
    { 0, -634, 0, 0 },
    { 0, -633, 0, 0 },
    { 0, -632, 0, 0 },
    { 0, -631, 0, 0 },
    { 0, -630, 0, 0 },
    { 0, -629, 0, 0 },
    { 0, -629, 0, 0 },
    { 0, -628, 0, 0 },
    { 0, -627, 0, 0 },
    { 0, -627, 0, 0 },
    { 0, -626, 0, 0 },
    { 0, -626, 0, 0 },
    { 0, -626, 0, 0 },
    { 0, -625, 0, 0 },
    { 0, -625, 0, 0 },
    { 0, -625, 0, 0 },
};

SVECTOR D_actor_511000_80147704[120] = {
    { 0, -0x6284, 0, 0 },
    { 0, -0x6284, 0, 0 },
    { 0, -0x6284, 0, 0 },
    { 0, -0x6286, 0, 0 },
    { 0, -0x6287, 0, 0 },
    { 0, -0x628A, 0, 0 },
    { 0, -0x628D, 0, 0 },
    { 0, -0x6290, 0, 0 },
    { 0, -0x6294, 0, 0 },
    { 0, -0x6299, 0, 0 },
    { 0, -0x629E, 0, 0 },
    { 0, -0x62A4, 0, 0 },
    { 0, -0x62AA, 0, 0 },
    { 0, -0x62B1, 0, 0 },
    { 0, -0x62B9, 0, 0 },
    { 0, -0x62C2, 0, 0 },
    { 0, -0x62CB, 0, 0 },
    { 0, -0x62D5, 0, 0 },
    { 0, -0x62E0, 0, 0 },
    { 0, -0x62EC, 0, 0 },
    { 0, -0x62F8, 0, 0 },
    { 0, -0x6306, 0, 0 },
    { 0, -0x6314, 0, 0 },
    { 0, -0x6323, 0, 0 },
    { 0, -0x6332, 0, 0 },
    { 0, -0x6343, 0, 0 },
    { 0, -0x6355, 0, 0 },
    { 0, -0x6367, 0, 0 },
    { 0, -0x637B, 0, 0 },
    { 0, -0x638F, 0, 0 },
    { 0, -0x63A4, 0, 0 },
    { 0, -0x63BB, 0, 0 },
    { 0, -0x63D2, 0, 0 },
    { 0, -0x63EA, 0, 0 },
    { 0, -0x6404, 0, 0 },
    { 0, -0x641E, 0, 0 },
    { 0, -0x643A, 0, 0 },
    { 0, -0x6456, 0, 0 },
    { 0, -0x6474, 0, 0 },
    { 0, -0x6493, 0, 0 },
    { 0, -0x64A8, 0, 0 },
    { 0, -0x6487, 0, 0 },
    { 0, -0x6464, 0, 0 },
    { 0, -0x6441, 0, 0 },
    { 0, -0x641C, 0, 0 },
    { 0, -0x63F6, 0, 0 },
    { 0, -0x63CF, 0, 0 },
    { 0, -0x63A6, 0, 0 },
    { 0, -0x637D, 0, 0 },
    { 0, -0x6352, 0, 0 },
    { 0, -0x6325, 0, 0 },
    { 0, -0x62F7, 0, 0 },
    { 0, -0x62C8, 0, 0 },
    { 0, -0x6298, 0, 0 },
    { 0, -0x6266, 0, 0 },
    { 0, -0x6233, 0, 0 },
    { 0, -0x61FE, 0, 0 },
    { 0, -0x61C8, 0, 0 },
    { 0, -0x6191, 0, 0 },
    { 0, -0x6158, 0, 0 },
    { 0, -0x611D, 0, 0 },
    { 0, -0x60E2, 0, 0 },
    { 0, -0x60A6, 0, 0 },
    { 0, -0x606A, 0, 0 },
    { 0, -0x602C, 0, 0 },
    { 0, -0x5FED, 0, 0 },
    { 0, -0x5FAD, 0, 0 },
    { 0, -0x5F6D, 0, 0 },
    { 0, -0x5F2B, 0, 0 },
    { 0, -0x5EE8, 0, 0 },
    { 0, -0x5EA5, 0, 0 },
    { 0, -0x5E60, 0, 0 },
    { 0, -0x5E1A, 0, 0 },
    { 0, -0x5DD3, 0, 0 },
    { 0, -0x5D8B, 0, 0 },
    { 0, -0x5D42, 0, 0 },
    { 0, -0x5CF7, 0, 0 },
    { 0, -0x5CAC, 0, 0 },
    { 0, -0x5C5F, 0, 0 },
    { 0, -0x5C11, 0, 0 },
    { 0, -0x5BC2, 0, 0 },
    { 0, -0x5B71, 0, 0 },
    { 0, -0x5B1F, 0, 0 },
    { 0, -0x5ACC, 0, 0 },
    { 0, -0x5A78, 0, 0 },
    { 0, -0x5A22, 0, 0 },
    { 0, -0x59CB, 0, 0 },
    { 0, -0x5973, 0, 0 },
    { 0, -0x5919, 0, 0 },
    { 0, -0x58BD, 0, 0 },
    { 0, -0x5861, 0, 0 },
    { 0, -0x5803, 0, 0 },
    { 0, -0x57A3, 0, 0 },
    { 0, -0x5742, 0, 0 },
    { 0, -0x56DF, 0, 0 },
    { 0, -0x567B, 0, 0 },
    { 0, -0x5615, 0, 0 },
    { 0, -0x55AD, 0, 0 },
    { 0, -0x5545, 0, 0 },
    { 0, -0x54DA, 0, 0 },
    { 0, -0x546E, 0, 0 },
    { 0, -0x5400, 0, 0 },
    { 0, -0x5390, 0, 0 },
    { 0, -0x531F, 0, 0 },
    { 0, -0x52AC, 0, 0 },
    { 0, -0x5237, 0, 0 },
    { 0, -0x51C1, 0, 0 },
    { 0, -0x5148, 0, 0 },
    { 0, -0x50CE, 0, 0 },
    { 0, -0x5053, 0, 0 },
    { 0, -0x4FD5, 0, 0 },
    { 0, -0x4F55, 0, 0 },
    { 0, -0x4ED4, 0, 0 },
    { 0, -0x4E50, 0, 0 },
    { 0, -0x4DCB, 0, 0 },
    { 0, -0x4D44, 0, 0 },
    { 0, -0x4CBB, 0, 0 },
    { 0, -0x4C30, 0, 0 },
    { 0, -0x4BA3, 0, 0 },
    { 0, -0x4B14, 0, 0 },
};

SVECTOR D_actor_511000_80147AC4[120] = {
    { -1027, 0, -245, 0 },
    { -1028, 0, -244, 0 },
    { -1029, 0, -243, 0 },
    { -1031, 0, -241, 0 },
    { -1033, 0, -238, 0 },
    { -1036, 0, -235, 0 },
    { -1040, 0, -231, 0 },
    { -1044, 0, -227, 0 },
    { -1048, 0, -222, 0 },
    { -1053, 0, -216, 0 },
    { -1059, 0, -210, 0 },
    { -1065, 0, -203, 0 },
    { -1072, 0, -196, 0 },
    { -1079, 0, -188, 0 },
    { -1086, 0, -180, 0 },
    { -1094, 0, -171, 0 },
    { -1103, 0, -162, 0 },
    { -1112, 0, -152, 0 },
    { -1121, 0, -142, 0 },
    { -1131, 0, -131, 0 },
    { -1141, 0, -120, 0 },
    { -1151, 0, -109, 0 },
    { -1162, 0, -97, 0 },
    { -1173, 0, -84, 0 },
    { -1185, 0, -72, 0 },
    { -1196, 0, -58, 0 },
    { -1209, 0, -45, 0 },
    { -1221, 0, -31, 0 },
    { -1234, 0, -17, 0 },
    { -1247, 0, -2, 0 },
    { -1260, 0, 12, 0 },
    { -1273, 0, 27, 0 },
    { -1287, 0, 43, 0 },
    { -1301, 0, 59, 0 },
    { -1315, 0, 75, 0 },
    { -1330, 0, 92, 0 },
    { -1344, 0, 109, 0 },
    { -1359, 0, 126, 0 },
    { -1374, 0, 143, 0 },
    { -1389, 0, 160, 0 },
    { -1404, 0, 178, 0 },
    { -1420, 0, 196, 0 },
    { -1435, 0, 214, 0 },
    { -1451, 0, 233, 0 },
    { -1466, 0, 251, 0 },
    { -1481, 0, 269, 0 },
    { -1495, 0, 286, 0 },
    { -1506, 0, 302, 0 },
    { -1517, 0, 317, 0 },
    { -1525, 0, 330, 0 },
    { -1533, 0, 343, 0 },
    { -1538, 0, 355, 0 },
    { -1543, 0, 365, 0 },
    { -1546, 0, 374, 0 },
    { -1547, 0, 383, 0 },
    { -1547, 0, 390, 0 },
    { -1546, 0, 396, 0 },
    { -1543, 0, 401, 0 },
    { -1539, 0, 405, 0 },
    { -1534, 0, 408, 0 },
    { -1527, 0, 411, 0 },
    { -1519, 0, 412, 0 },
    { -1510, 0, 411, 0 },
    { -1499, 0, 410, 0 },
    { -1488, 0, 408, 0 },
    { -1474, 0, 405, 0 },
    { -1460, 0, 401, 0 },
    { -1445, 0, 396, 0 },
    { -1428, 0, 390, 0 },
    { -1410, 0, 383, 0 },
    { -1391, 0, 375, 0 },
    { -1371, 0, 366, 0 },
    { -1350, 0, 356, 0 },
    { -1328, 0, 345, 0 },
    { -1305, 0, 333, 0 },
    { -1280, 0, 320, 0 },
    { -1255, 0, 307, 0 },
    { -1228, 0, 292, 0 },
    { -1201, 0, 276, 0 },
    { -1172, 0, 259, 0 },
    { -1143, 0, 242, 0 },
    { -1112, 0, 223, 0 },
    { -1081, 0, 204, 0 },
    { -1049, 0, 184, 0 },
    { -1015, 0, 162, 0 },
    { -981, 0, 140, 0 },
    { -946, 0, 117, 0 },
    { -911, 0, 93, 0 },
    { -874, 0, 68, 0 },
    { -837, 0, 43, 0 },
    { -801, 0, 17, 0 },
    { -769, 0, -5, 0 },
    { -742, 0, -26, 0 },
    { -718, 0, -45, 0 },
    { -698, 0, -62, 0 },
    { -681, 0, -78, 0 },
    { -667, 0, -92, 0 },
    { -656, 0, -105, 0 },
    { -648, 0, -116, 0 },
    { -642, 0, -125, 0 },
    { -639, 0, -134, 0 },
    { -637, 0, -141, 0 },
    { -638, 0, -147, 0 },
    { -640, 0, -152, 0 },
    { -644, 0, -156, 0 },
    { -649, 0, -159, 0 },
    { -655, 0, -161, 0 },
    { -662, 0, -163, 0 },
    { -669, 0, -164, 0 },
    { -677, 0, -165, 0 },
    { -685, 0, -165, 0 },
    { -694, 0, -164, 0 },
    { -702, 0, -164, 0 },
    { -709, 0, -163, 0 },
    { -716, 0, -162, 0 },
    { -722, 0, -161, 0 },
    { -728, 0, -160, 0 },
    { -732, 0, -159, 0 },
    { -734, 0, -159, 0 },
    { -735, 0, -159, 0 },
};

Actor511000Palette D_actor_511000_80147E84 = { .bytes = {
                                                   255,
                                                   255,
                                                   2,
                                                   128,
                                                   3,
                                                   128,
                                                   6,
                                                   128,
                                                   9,
                                                   128,
                                                   13,
                                                   128,
                                                   16,
                                                   128,
                                                   127,
                                                   148,
                                                   127,
                                                   177,
                                                   18,
                                                   132,
                                                   63,
                                                   169,
                                                   223,
                                                   156,
                                                   23,
                                                   132,
                                                   27,
                                                   132,
                                                   29,
                                                   132,
                                                   0,
                                                   128,
                                               } };

GpImgRec D_actor_511000_80147EA4[2] = {
    { 0, 0, { 0, 264, 16, 1 }, D_actor_511000_80147E84.words },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u8 D_actor_511000_80147EC4[32] = {
    255,
    255,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    33,
    132,
    0,
    128,
};

GpViewRec D_actor_511000_80147EE4[120] = {
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5598, 5926, 124 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5599, 5926, 122 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5599, 5926, 118 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5601, 5927, 110 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5602, 5927, 100 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5605, 5928, 86 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5607, 5930, 70 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5610, 5931, 52 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5614, 5933, 30 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5617, 5934, 6 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5622, 5936, -19 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5626, 5938, -47 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5630, 5940, -78 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5635, 5943, -111 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5640, 5945, -146 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5645, 5947, -184 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5651, 5950, -223 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5656, 5953, -264 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5661, 5955, -306 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5667, 5958, -351 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5672, 5961, -397 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5677, 5963, -444 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5682, 5966, -493 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5688, 5969, -543 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5693, 5972, -595 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5697, 5975, -648 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5702, 5977, -702 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5707, 5980, -756 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5711, 5983, -812 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5715, 5985, -869 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5719, 5988, -926 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5722, 5990, -985 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5725, 5993, -1043 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5728, 5995, -1103 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5730, 5997, -1162 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5732, 5999, -1222 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5734, 6001, -1282 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5735, 6003, -1343 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5736, 6005, -1403 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5736, 6006, -1464 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5736, 6008, -1524 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5735, 6009, -1585 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5734, 6010, -1644 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5732, 6011, -1704 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5730, 6012, -1763 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5727, 6012, -1822 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5724, 6013, -1880 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5719, 6013, -1937 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5714, 6012, -1993 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5708, 6012, -2048 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5701, 6011, -2103 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5694, 6010, -2156 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5685, 6009, -2207 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5675, 6007, -2258 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5663, 6005, -2306 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5650, 6002, -2353 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5634, 5998, -2397 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5616, 5993, -2439 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5593, 5987, -2476 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5564, 5979, -2505 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5527, 5968, -2521 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5486, 5956, -2521 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5446, 5944, -2510 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5407, 5931, -2492 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5370, 5919, -2469 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5334, 5908, -2443 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5299, 5897, -2414 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5265, 5885, -2383 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5232, 5875, -2350 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5199, 5864, -2316 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5167, 5853, -2281 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5136, 5843, -2245 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5105, 5833, -2207 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5074, 5822, -2169 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5045, 5812, -2131 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 5015, 5802, -2091 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4986, 5793, -2051 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4958, 5783, -2011 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4930, 5774, -1971 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4902, 5764, -1930 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4875, 5755, -1889 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4849, 5746, -1847 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4823, 5737, -1806 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4797, 5728, -1765 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4772, 5720, -1724 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4748, 5711, -1683 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4724, 5703, -1642 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4701, 5695, -1601 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4678, 5687, -1561 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4656, 5679, -1521 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4634, 5672, -1481 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4614, 5665, -1442 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4593, 5657, -1404 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4573, 5651, -1366 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4554, 5644, -1329 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4536, 5637, -1292 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4518, 5631, -1257 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4501, 5625, -1222 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4485, 5619, -1188 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4469, 5614, -1155 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4454, 5608, -1123 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4440, 5603, -1092 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4426, 5598, -1063 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4413, 5594, -1034 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4401, 5589, -1007 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4389, 5585, -981 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4379, 5581, -957 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4368, 5578, -934 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4359, 5574, -913 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4351, 5571, -893 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4343, 5568, -874 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4336, 5566, -858 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4330, 5564, -843 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4324, 5562, -830 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4320, 5560, -819 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4316, 5559, -810 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4313, 5558, -802 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4311, 5557, -797 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4309, 5556, -794 } }, 447 },
    { { { { -328, 0, -4082 }, { 3905, 1193, -313 }, { 1189, -3918, -95 } }, { 4309, 5556, -793 } }, 447 },
};

Actor511000MsgEntry D_actor_511000_80148FC4[4] = {
    { 2003, { .call0 = func_actor_511000_801334B8 } },
    { 2004, { .call1 = func_actor_511000_801334C4 } },
    { 2005, { .call2 = func_actor_511000_80133554 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

SVECTOR D_actor_511000_80148FE4[4] = {
    { 0, -0x3A98, 0, 0 },
    { 0, -1350, 1360, 0 },
    { 340, -230, -5790, 0 },
    { 0, 1520, 50, 0 },
};

CVECTOR D_actor_511000_80149004[4] = {
    { 255, 255, 255, 0 },
    { 200, 200, 200, 0 },
    { 150, 150, 150, 0 },
    { 60, 60, 60, 0 },
};

DVECTOR D_actor_511000_80149014[16] = {
    { 4096, 0 },
    { 3784, 1567 },
    { 2896, 2896 },
    { 1567, 3784 },
    { 0, 4096 },
    { -1567, 3784 },
    { -2896, 2896 },
    { -3784, 1567 },
    { -4096, 0 },
    { -3784, -1567 },
    { -2896, -2896 },
    { -1567, -3784 },
    { 0, -4096 },
    { 1567, -3784 },
    { 2896, -2896 },
    { 3784, -1567 },
};

s32 D_actor_511000_80149054[3] = {
    1,
    2,
    3,
};

TmdBone D_actor_511000_80149060[19] = {
#include "assets/actor_511000_model_1C8E8_skeleton.inc"
};

u32 D_actor_511000_8014930C[19] = {
#include "assets/actor_511000_model_1C8E8_partVerts.inc"
};

SVECTOR D_actor_511000_80149358[358] = {
#include "assets/actor_511000_model_1C8E8_verts.inc"
};

SVECTOR D_actor_511000_80149E88[356] = {
#include "assets/actor_511000_model_1C8E8_normals.inc"
};

u32 D_actor_511000_8014A9A8[3928] = {
#include "assets/actor_511000_model_1C8E8_stream.inc"
};

TmdSource D_actor_511000_8014E708 = {
    0,
    21588,
    5944,
    19,
    D_actor_511000_8014930C,
    D_actor_511000_80149358,
    D_actor_511000_80149E88,
    D_actor_511000_80149060,
    D_actor_511000_8014A9A8,
};

TmdBone D_actor_511000_8014E72C[1] = {
#include "assets/actor_511000_model_1CB8C_skeleton.inc"
};

u32 D_actor_511000_8014E750[1] = {
#include "assets/actor_511000_model_1CB8C_partVerts.inc"
};

SVECTOR D_actor_511000_8014E754[14] = {
#include "assets/actor_511000_model_1CB8C_verts.inc"
};

SVECTOR D_actor_511000_8014E7C4[12] = {
#include "assets/actor_511000_model_1CB8C_normals.inc"
};

u32 D_actor_511000_8014E824[98] = {
#include "assets/actor_511000_model_1CB8C_stream.inc"
};

TmdSource D_actor_511000_8014E9AC = {
    0,
    652,
    0,
    1,
    D_actor_511000_8014E750,
    D_actor_511000_8014E754,
    D_actor_511000_8014E7C4,
    D_actor_511000_8014E72C,
    D_actor_511000_8014E824,
};

TmdBone D_actor_511000_8014E9D0[1] = {
#include "assets/actor_511000_model_1CEA8_skeleton.inc"
};

u32 D_actor_511000_8014E9F4[1] = {
#include "assets/actor_511000_model_1CEA8_partVerts.inc"
};

SVECTOR D_actor_511000_8014E9F8[14] = {
#include "assets/actor_511000_model_1CEA8_verts.inc"
};

SVECTOR D_actor_511000_8014EA68[19] = {
#include "assets/actor_511000_model_1CEA8_normals.inc"
};

u32 D_actor_511000_8014EB00[114] = {
#include "assets/actor_511000_model_1CEA8_stream.inc"
};

TmdSource D_actor_511000_8014ECC8 = {
    0,
    724,
    0,
    1,
    D_actor_511000_8014E9F4,
    D_actor_511000_8014E9F8,
    D_actor_511000_8014EA68,
    D_actor_511000_8014E9D0,
    D_actor_511000_8014EB00,
};

TmdBone D_actor_511000_8014ECEC[1] = {
#include "assets/actor_511000_model_1D204_skeleton.inc"
};

u32 D_actor_511000_8014ED10[1] = {
#include "assets/actor_511000_model_1D204_partVerts.inc"
};

SVECTOR D_actor_511000_8014ED14[18] = {
#include "assets/actor_511000_model_1D204_verts.inc"
};

SVECTOR D_actor_511000_8014EDA4[17] = {
#include "assets/actor_511000_model_1D204_normals.inc"
};

u32 D_actor_511000_8014EE2C[126] = {
#include "assets/actor_511000_model_1D204_stream.inc"
};

TmdSource D_actor_511000_8014F024 = {
    0,
    860,
    0,
    1,
    D_actor_511000_8014ED10,
    D_actor_511000_8014ED14,
    D_actor_511000_8014EDA4,
    D_actor_511000_8014ECEC,
    D_actor_511000_8014EE2C,
};

AnimationPackedPose D_actor_511000_8014F048[95] = {
#include "assets/actor_511000_animation_208EC_bank1.inc"
};

AnimationPackedRotation D_actor_511000_8014F4BC[1397] = {
#include "assets/actor_511000_animation_208EC_bank4.inc"
};

AnimationRecord D_actor_511000_80150A90[1813] = {
#include "assets/actor_511000_animation_208EC_records.inc"
};

u16 D_actor_511000_801526E4[20] = {
#include "assets/actor_511000_animation_208EC_indices.inc"
};

AnimationSet D_actor_511000_8015270C = {
    D_actor_511000_80150A90,
    D_actor_511000_801526E4,
    { NULL, D_actor_511000_8014F048, NULL, NULL, D_actor_511000_8014F4BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_511000_80152734[38] = {
#include "assets/actor_511000_animation_21EBC_bank1.inc"
};

AnimationPackedRotation D_actor_511000_801528FC[559] = {
#include "assets/actor_511000_animation_21EBC_bank4.inc"
};

AnimationRecord D_actor_511000_801531B8[703] = {
#include "assets/actor_511000_animation_21EBC_records.inc"
};

u16 D_actor_511000_80153CB4[20] = {
#include "assets/actor_511000_animation_21EBC_indices.inc"
};

AnimationSet D_actor_511000_80153CDC = {
    D_actor_511000_801531B8,
    D_actor_511000_80153CB4,
    { NULL, D_actor_511000_80152734, NULL, NULL, D_actor_511000_801528FC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_511000_80153D04[109] = {
#include "assets/actor_511000_animation_23228_bank1.inc"
};

AnimationPackedRotation D_actor_511000_80154220[368] = {
#include "assets/actor_511000_animation_23228_bank4.inc"
};

AnimationRecord D_actor_511000_801547E0[528] = {
#include "assets/actor_511000_animation_23228_records.inc"
};

u16 D_actor_511000_80155020[20] = {
#include "assets/actor_511000_animation_23228_indices.inc"
};

AnimationSet D_actor_511000_80155048 = {
    D_actor_511000_801547E0,
    D_actor_511000_80155020,
    { NULL, D_actor_511000_80153D04, NULL, NULL, D_actor_511000_80154220, NULL, NULL, NULL },
};

TaskDesc D_actor_511000_80155070[4] = {
    { TASK_BODY_TMD, 96, func_actor_511000_80133D90, { .model = &D_actor_511000_8014E708 } },
    { TASK_BODY_TMD, 96, func_actor_511000_80133EF4, { .model = &D_actor_511000_8014E9AC } },
    { TASK_BODY_TMD, 96, func_actor_511000_80133FC8, { .model = &D_actor_511000_8014F024 } },
    { TASK_BODY_TMD, 96, func_actor_511000_8013409C, { .model = &D_actor_511000_8014ECC8 } },
};

Actor511000MessageEntry D_actor_511000_801550A0[4] = {
    { 2003, { .call0 = func_actor_511000_80133DEC } },
    { 2004, { .call3 = func_actor_511000_80133E48 } },
    { 2005, { .call4 = func_actor_511000_80133EAC } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

AnimationSet* D_actor_511000_801550C0[4] = {
    NULL,
    &D_actor_511000_8015270C,
    &D_actor_511000_80153CDC,
    &D_actor_511000_80155048,
};

static void func_actor_511000_80132E6C(Actor511000Work* work);

/// Tick state: while `field_474` is set, steps animation slots 1..19; in
/// mode 1 counts `field_4D2` up and, on frame 0x10, plays the sound and spawns
/// the effect at the first child's model. Then draws the ground shadow under
/// model part 1, refreshes that part's coordinate and colour when the session
/// asks, runs the texture-upload state, and ticks the `field_480` countdown
/// that frees the model's buffers when it reaches zero.
static void func_actor_511000_80131E78(Task* arg0)
{
    Actor511000Work2* work;
    TmdObject*        extra;
    GfxCoord*         coord;
    GfxCoord*         obj;
    VECTOR            pos;
    s32               i;
    s32               pan;

    extra = arg0->extra.tmd;
    work  = (Actor511000Work2*)((GameActor*)arg0->work);
    coord = &extra->coords[1];
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
        if (work->field_478 == 1) {
            if (++work->field_4D2 == 0x10) {
                obj = work->field_4C4->extra.tmd->coords;
                pan = (s8)Gp_GetObjPan(obj);
                SndEvt_EnqueueType6(0x313A0003, pan, (s8)gpGetObjDepth(obj));
                Gp_SpawnEff(0x6006A, obj, 0, &D_actor_511000_8014733C);
            }
        }
    }
    if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8((VECTOR3*)(arg0->extra.tmd)->coords[1].workm.t, (VECTOR3*)&pos) != 0) {
            Gp_DrawEffGroundQuad((VECTOR3*)&pos, 0x300, Gp_State1C->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
    }
    func_actor_511000_80132048(arg0);
    if (work->field_480.word >= 0) {
        if (work->field_480.word == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_480.word--;
    }
}

/// Texture-upload state: runs the countdown at `field_4CE` down one a frame
/// while `field_4D0` names the upload in progress, and on the frame it
/// underflows posts that step's image over the 0x18x0x10 rect at y 0x28 --
/// reloading the countdown from `field_4CC` and advancing `field_4D0` for
/// steps 1 and 2, or clearing it and starting over for step 3. Steps 1 and 2
/// share their whole tail, which is what makes the compiler emit one copy of
/// it that step 1 jumps into; step 3 only differs in clearing the step
/// instead of advancing it.
static void func_actor_511000_80132048(Task* arg0)
{
    Actor511000Work2* work;
    RECT              rect;

    work   = (Actor511000Work2*)((GameActor*)arg0->work);
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (work->field_4D0) {
        case 1:
            work->field_4CE = work->field_4CE - 1;
            if ((s16)work->field_4CE < 0) {
                Gp_LoadActorImage(arg0, &D_actor_511000_801472B4[0], &rect);
                work->field_4CE = work->field_4CC;
                work->field_4D0 = work->field_4D0 + 1;
            }
            break;
        case 2:
            work->field_4CE = work->field_4CE - 1;
            if ((s16)work->field_4CE < 0) {
                Gp_LoadActorImage(arg0, &D_actor_511000_80146F94[0], &rect);
                work->field_4CE = work->field_4CC;
                work->field_4D0 = work->field_4D0 + 1;
            }
            break;
        case 3:
            work->field_4CE = work->field_4CE - 1;
            if ((s16)work->field_4CE < 0) {
                Gp_LoadActorImage(arg0, &D_actor_511000_80146C74[0], &rect);
                work->field_4D0 = 0;
            }
            break;
    }
}

void func_actor_511000_80132150(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E24;
    sp.funcs[task->state](task);
}

/// Spawn state of the child in the first state table: chains this task's root
/// coordinate under the parent's part named by `spawnArg1`, takes the parent
/// model's light and colour matrices, reparents the task under the spawner
/// named by `spawnArg2` and advances to the next state.
static void func_actor_511000_801321A8(Task* task)
{
    Task*      parent;
    s32        part;
    TmdObject* extra;
    TmdObject* parentExtra;
    GfxCoord*  coord;
    GfxCoord*  dest;

    parent              = (Task*)task->spawnArg2.pointer;
    part                = task->spawnArg1.value;
    extra               = task->extra.tmd;
    parentExtra         = parent->extra.tmd;
    coord               = extra->coords;
    dest                = &parentExtra->coords[part];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = dest;
    extra->lightMtx     = parentExtra->lightMtx;
    extra->colorMtx     = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// Tick state of the first state table's child: nothing to do, the chained
/// coordinate follows the spawner by itself.
static void func_actor_511000_80132224(Task* task)
{
}

void func_actor_511000_8013222C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E30;
    sp.funcs[task->state](task);
}

static void func_actor_511000_80132284(Task* task)
{
    Task*      parent;
    TmdObject* obj;
    TmdObject* parentObj;
    GfxCoord*  coords;
    GfxCoord*  root;

    parent      = task->spawnArg2.pointer;
    obj         = task->extra.tmd;
    parentObj   = parent->extra.tmd;
    coords      = parentObj->coords;
    obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    root        = obj->coords;
    if (!(parentObj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (!(parentObj->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        obj->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        Tmd_AllocBuffers(obj);
    } else {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    obj->otOffset      = -2;
    coords            += task->spawnArg1.value;
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    root->parent       = coords;
    obj->lightMtx      = parentObj->lightMtx;
    obj->colorMtx      = parentObj->colorMtx;
    Task_Reparent(parent, task);
    task->state++;
}

/// Tick state of the child in the second state table: copies the spawner's
/// model flag bits 0x80 (hidden) and 0x4 (draw buffers allocated) onto this
/// task's model, rebuilding the buffers through `Tmd_AllocBuffers` when the
/// spawner's are gone.
static void func_actor_511000_80132390(Task* task)
{
    TmdObject* parentObject;
    TmdObject* object;

    parentObject = ((Task*)task->spawnArg2.pointer)->extra.tmd;
    object       = task->extra.tmd;

    if (!(parentObject->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        object->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        object->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (!(parentObject->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        object->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        Tmd_AllocBuffers(object);
        return;
    }
    object->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
}

void func_actor_511000_80132428(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E3C;
    sp.funcs[task->state](task);
}

/// Spawn handler: allocates the work block, seeds its head, excludes the model
/// from active drawing, starts the actor's two child tasks and
/// hands the model's matrices to the light/color rebuilder, then advances to the
/// tick handler. The retained shadow branch cannot run with this bit set.
static void func_actor_511000_80132480(Task* task)
{
    Actor511000Work2* work;
    TmdObject*        extra;
    VECTOR3           pos;
    u16               flags;

    extra = task->extra.tmd;
    work  = (Actor511000Work2*)memCalloc(0x4D4, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work           = work;
    work->field_478      = -1;
    work->field_47C      = -1;
    work->field_4D2      = 0;
    work->field_480.word = -1;
    flags                = extra->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    extra->flags         = flags;
    if (!(flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->groundShadowShade);
        }
    }
    work->field_4C4 = Task_SpawnFromTable(D_actor_511000_801472E8, 1, 8, task);
    work->field_4C8 = Task_SpawnFromTable(D_actor_511000_801472E8, 2, 0xC, task);
    func_actor_511000_801325A4(task);
    task->msgTable     = D_actor_511000_8014730C;
    task->exitCallback = Gp_EnemyTaskExit;
    task->state++;
}

/// Republishes the work block's light/color matrices onto the TMD object and
/// rebuilds model part 1's world matrix from it, then hands that part's
/// translation to the ground-shadow helper.
static void func_actor_511000_801325A4(Task* task)
{
    Actor511000Work2* work;
    GfxCoord*         coords;
    TmdObject*        extra;

    work                   = (Actor511000Work2*)task->work;
    extra                  = task->extra.tmd;
    coords                 = extra->coords;
    extra->lightMtx        = &work->light;
    extra->colorMtx        = &work->color;
    coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

/// Animation message handler: when the source index in the payload changes,
/// reseeds the animation context from that entry of the source table; when the
/// animation id changes, restarts slots 1..19 on it, blended when the
/// payload's third word is set, steps them once and turns on the tick state's
/// per-frame stepping.
s32 func_actor_511000_80132604(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    Actor511000Work2* work;
    s32               i;
    TmdObject*        ext;

    work = (Actor511000Work2*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->field_47C) {
        work->field_47C = msg->source.index;
        work->field_478 = -1;
        func_800B3F84(&work->rig.anim, D_actor_511000_801472E4[work->field_47C], ext, work->rig.poses,
                      work->rig.slots);
    }
    if (msg->animationId != work->field_478) {
        work->field_478 = msg->animationId;
        if (msg->blend != ANIMATION_BLEND_RESET) {
            for (i = 1; i < 0x14; i++) {
                func_800B4114(&work->rig.anim, i, work->field_478, 0, 6);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimResetSlot(&work->rig.anim, i, work->field_478);
            }
        }
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
        work->field_474 = 1;
    }
    return 0;
}

/// Placement message handler: writes the payload's translation into the root
/// coordinate, keeps its Euler angles in the coordinate's `rot` slot and
/// rebuilds the rotation from them, then clears `composeStamp` so the world matrix is
/// recomputed.
s32 func_actor_511000_80132724(Task* task, s32 arg1, ActorTransform* args)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Message-0x7D5 handler: the four-way visibility/mode switch on the message's
/// mode word, run against the `TmdObject` parked in `Task::extra`. Mode 0 shows
/// the model (`field_C` bit 0x80) and clears the 4 flag, 1 hides it, frees the
/// aux buffers and clears the flag, 2 does both plus latching the mode into the
/// work block's `field_480`, and 3 hides it while setting the flag. Anything
/// else returns 1 and leaves the object alone; the handled modes return 0.
/// The handler reads `work` before the switch even though mode 2 is its only
/// use, so retail's `lw $v1,0x1C($a0)` sits in the entry block. The same body
/// shape as `func_actor_141000_80133E8C` / `func_actor_503500_80132584`.
s32 func_actor_511000_801327A0(Task* arg0, s32 arg1, s32 mode)
{
    TmdObject*        obj;
    Actor511000Work2* work;
    s32               ret;

    obj  = arg0->extra.tmd;
    work = (Actor511000Work2*)((GameActor*)arg0->work);
    ret  = 0;

    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags          |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->field_480.word = mode;
            obj->flags          |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message-0x7DB handler: un-hides the model its first child task carries in
/// `Task::extra` (`field_C` bit 0x80) for mode 1 and hides it for mode 0, then
/// hides the second child as well on the mode-1 path -- the same two tasks
/// `func_actor_511000_80132480` parked at `field_4C4` / `field_4C8`. Any other
/// mode leaves both alone.
/// The `default:` arm jumps straight to the shared `return 0` instead of
/// falling through the hide block: retail's single epilogue is only reached
/// that way, the hide block and the shared return merging into one block whose
/// first label sits on the value store.
s32 func_actor_511000_8013287C(Task* arg0, s32 arg1, ActorCommand* msg)
{
    Actor511000Work2* work;
    Task*             child;
    u16               mode;

    mode = msg->command;
    work = (Actor511000Work2*)((GameActor*)arg0->work);

    switch (mode) {
        case 0:
            child = work->field_4C4;
            break;
        case 1:
            child = work->field_4C4;
            if (child != NULL) {
                child->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            child = work->field_4C8;
            break;
        default:
            goto out;
    }

    if (child != NULL) {
        child->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
out:
    return 0;
}

/// Message-0x7E0 handler: uploads one of the actor's three texture records
/// over the 0x18x0x10 rect at y 0x28 -- `D_actor_511000_801472B4[0]` for mode 1,
/// `D_actor_511000_80146C74[0]` for modes 0 and 2, and `D_actor_511000_80146F94[0]`
/// for mode 3, which sets the work block's `field_4D0` / `field_4CC` to 1
/// first. Any other mode leaves the image NULL and returns 0.
/// The mode-1 case is written first because the compiler lays the case bodies
/// out in source order and that is the order the retail image has them in.
s32 func_actor_511000_80132904(Task* arg0, s32 arg1, s32 mode)
{
    RECT      rect;
    GpImgRec* img;
    s32       ret;

    ret    = 0;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (mode) {
        case 1:
            img = &D_actor_511000_801472B4[0];
            break;
        case 0:
        case 2:
            img = &D_actor_511000_80146C74[0];
            break;
        case 3:
            ((Actor511000Work2*)((GameActor*)arg0->work))->field_4D0 = 1;
            ((Actor511000Work2*)((GameActor*)arg0->work))->field_4CC = 1;
            img                                                      = &D_actor_511000_80146F94[0];
            break;
        default:
            img = NULL;
            break;
    }

    if (img != NULL) {
        ret = Gp_LoadActorImage(arg0, img, &rect);
    }
    return ret;
}

/// Per-frame tick for a child of the spawner: mirrors the parent model's
/// visibility bit (`field_C` 0x80) onto its own model, and once the session
/// reaches mode 0x18 poses its root coordinate from the parent's
/// `killCountdown` entry in `D_actor_511000_80147AC4`. Within three steps of
/// countdown 0x59 it also picks that distance's colour from
/// `D_actor_511000_80149004`, darkened by 0x1E per channel, and hands both to
/// `func_actor_511000_80132B14`.
/// The table is loaded into its own local before indexing: `&table[d]` on the
/// symbol directly shifts `d` ahead of the `lui`/`addiu` pair.
static void func_actor_511000_801329C4(Task* task)
{
    Task*      parent;
    TmdObject* extra;
    GfxCoord*  coord;
    CVECTOR*   col;
    CVECTOR*   tbl;
    s32        d;
    s8         rgb[3];

    parent = (Task*)task->spawnArg2.pointer;
    extra  = task->extra.tmd;
    coord  = extra->coords;

    if (!(parent->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        extra->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    if (gGameSession->location.loc.view == 0x18) {
        coord->param.rot.vx = D_actor_511000_80147AC4[parent->killCountdown].vx;
        coord->param.rot.vy = D_actor_511000_80147AC4[parent->killCountdown].vy;
        coord->param.rot.vz = D_actor_511000_80147AC4[parent->killCountdown].vz;
        RotMatrix(&coord->param.rot, &coord->coord);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;

        d = parent->killCountdown - 0x59;
        if (d < 0) {
            d = 0x59 - parent->killCountdown;
        }
        if (d < 4) {
            tbl    = D_actor_511000_80149004;
            col    = &tbl[d];
            rgb[0] = col->r - 0x1E;
            rgb[1] = col->g - 0x1E;
            rgb[2] = col->b - 0x1E;
            func_actor_511000_80132B14(task, col, rgb);
        }
    }
}

/// Draws a semi-transparent gradient disc at the model's root: projects the
/// parent-composed origin, scales the 16 unit offsets in
/// `D_actor_511000_80149014` by 0x12C/0x1000 around it, and fans 16 `POLY_G3`
/// from the centre (`col`) to the rim (`rgb`) into one OT slot, followed by an
/// additive draw-mode `DR_TPAGE`. Both `pts` and the offset table walk by
/// pointer and `scale` is a variable, which is what keeps the `mult` and the
/// retail induction-variable order.
static void func_actor_511000_80132B14(Task* task, CVECTOR* col, s8* rgb)
{
    SVECTOR   pos;
    DVECTOR   pts[16];
    MATRIX    mtx;
    long      sxy;
    long      p;
    long      flag;
    s32       otz;
    POLY_G3*  prim;
    DR_TPAGE* dr;
    u16       x;
    u16       y;
    s32       scale;
    u32*      ot;
    s32       i;
    DVECTOR*  pt;
    DVECTOR*  src;

    Gp_ComposeParentWorld(task->extra.tmd->coords, &mtx, &pos);
    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);
    otz   = RotTransPers(&pos, &sxy, &p, &flag);
    x     = sxy;
    y     = sxy >> 16;
    src   = D_actor_511000_80149014;
    pt    = pts;
    scale = 0x12C;
    for (i = 0; i < 16; i++) {
        pt->vx = x + scale * src->vx / 0x1000;
        pt->vy = y + scale * src->vy / 0x1000;
        pt++;
        src++;
    }
    ot = (u32*)((u8*)gGpuCurrentOt + (((u32)(otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) - 30;
    pt = pts;
    for (i = 0; i < 15; i++, pt++) {
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG3(prim);
        setSemiTrans(prim, 1);
        prim->x0 = x;
        prim->y0 = y;
        prim->x1 = pt->vx;
        prim->y1 = pt->vy;
        prim->x2 = pt[1].vx;
        prim->y2 = pt[1].vy;
        prim->r0 = col->r;
        prim->g0 = col->g;
        prim->b0 = col->b;
        prim->r1 = rgb[0];
        prim->g1 = rgb[1];
        prim->b1 = rgb[2];
        prim->r2 = rgb[0];
        prim->g2 = rgb[1];
        prim->b2 = rgb[2];
        addPrim(ot, prim);
    }
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG3(prim);
    setSemiTrans(prim, 1);
    prim->x0 = x;
    prim->y0 = y;
    prim->x1 = pt->vx;
    prim->y1 = pt->vy;
    prim->x2 = pts[0].vx;
    prim->y2 = pts[0].vy;
    prim->r0 = col->r;
    prim->g0 = col->g;
    prim->b0 = col->b;
    prim->r1 = rgb[0];
    prim->g1 = rgb[1];
    prim->b1 = rgb[2];
    prim->r2 = rgb[0];
    prim->g2 = rgb[1];
    prim->b2 = rgb[2];
    addPrim(ot, prim);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setDrawTPage(dr, 1, 0, 0x2A);
    addPrim(ot, dr);
}

/// Palette fade: steps `field_2C` up by 0x555 per frame while the
/// `field_2E` hold counter is live (counting it down once the blend saturates
/// at 0x1000), otherwise snaps it back to 0 and re-arms the hold at 0x1E. Each
/// of the 16 little-endian 15-bit colours is then blended between
/// `D_actor_511000_80147E84.bytes` and `D_actor_511000_80147EC4` by that weight into
/// the `field_C` CLUT, which `D_actor_511000_80147EA4[0]` uploads.
/// The destination is formed as `work + i` before the field offset so the
/// `addu` keeps the index first and CSE cannot fold the 0xC into a store.
static void func_actor_511000_80132E6C(Actor511000Work* work)
{
    CVECTOR col[3];
    s32     i;
    s32     c;
    s32     inv;
    s32     fade;
    u8*     src0;
    u8*     src1;
    u8*     dst;

    if (work->field_2E != 0) {
        work->field_2C += 0x555;
        i               = 0;
        if (work->field_2C >= 0x1000) {
            work->field_2C = 0x1000;
            if (--work->field_2E < 0) {
                work->field_2E = 0;
            }
        }
    } else {
        work->field_2C -= 0x1000;
        i               = 0;
        if (work->field_2C <= 0) {
            work->field_2C = 0;
            work->field_2E = 0x1E;
        }
    }
    inv  = 0x1000 - work->field_2C;
    fade = work->field_2C;
    do {
        dst      = (u8*)(i + (s32)work);
        dst      = ((Actor511000Work*)dst)->field_C.bytes;
        src0     = &D_actor_511000_80147E84.bytes[i];
        src1     = &D_actor_511000_80147EC4[i];
        c        = src0[0] | (src0[1] << 8);
        col[0].r = ((u16)c >> 10) & 0x1F;
        col[0].g = ((u16)c >> 5) & 0x1F;
        col[0].b = c & 0x1F;
        c        = src1[0] | (src1[1] << 8);
        col[1].r = ((u16)c >> 10) & 0x1F;
        col[1].g = ((u16)c >> 5) & 0x1F;
        col[1].b = c & 0x1F;
        LoadAverageCol(&col[0], &col[1], inv, fade, &col[2]);
        c      = col[2].b + ((col[2].r << 10) + (col[2].g << 5));
        dst[1] = (u32)c >> 8;
        i     += 2;
        dst[0] = c;
    } while (i < 0x20);
    Gp_LoadImages(&D_actor_511000_80147EA4[0]);
}

/// Spawn/setup state: allocates the 0x70 work block, parks it in `work`,
/// arms the buffer-free countdown at -1, un-hides the model (`field_C` bit
/// 0x80), places it at rot/trans index 0, binds light/color, installs the
/// message table, and publishes `work->field_C` through
/// `D_actor_511000_80147EA4[0].data` before advancing to the per-frame state.
static void func_actor_511000_80133034(Task* task)
{
    Actor511000Work* work;
    TmdObject*       extra;

    extra = task->extra.tmd;
    work  = (Actor511000Work*)memCalloc(0x70, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work    = work;
    work->field_8 = -1;
    extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    func_actor_511000_801336E0(task, D_actor_511000_80147344, D_actor_511000_80147704, 0);
    func_actor_511000_801337F0(task);
    do {
        task->msgTable                  = D_actor_511000_80148FC4;
        D_actor_511000_80147EA4[0].data = work->field_C.words;
    } while (0);
    task->state += 1;
}

/// Per-frame state: while the model is hidden (`field_C` bit 0x80 clear) it
/// refreshes the root coordinate, rebuilds the colour matrix from that
/// coordinate's own translation, and runs the work block's follow-up. Once the
/// session reaches mode 0x18 it walks `killCountdown` up to 0x77, spawning a
/// view task for the camera record at each index and re-posing the model from
/// the matching rotations, and finally runs the `Tmd_FreeBuffers` countdown the
/// spawn state armed at -1, freeing the buffers and latching the field back to
/// -1 on the frame the countdown reaches zero.
static void func_actor_511000_801330F0(Task* task)
{
    Actor511000Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              countdown;
    s16              frame;

    obj   = task->extra.tmd;
    work  = (Actor511000Work*)task->work;
    coord = obj->coords;

    if (!(obj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        Gp_UpdateCoord(coord);
        func_800D7A9C(obj, (VECTOR*)coord->workm.t, 0, 3);
        func_actor_511000_80132E6C((Actor511000Work*)task->work);
    }
    if (gGameSession->location.loc.view == 0x18) {
        frame               = task->killCountdown + 1;
        task->killCountdown = frame;
        if (frame >= 0x78) {
            task->killCountdown = 0x77;
        }
        Gp_TrySpawnViewTask(&D_actor_511000_80147EE4[task->killCountdown]);
        func_actor_511000_801336E0(task, D_actor_511000_80147344, D_actor_511000_80147704, task->killCountdown);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    countdown = work->field_8;
    if (countdown >= 0) {
        if (countdown == 0) {
            Tmd_FreeBuffers(obj);
            countdown = work->field_8;
        }
        work->field_8 = countdown - 1;
    }
}

static void func_actor_511000_80133220(Task* task)
{
    taskKill(task);
}

/// Inherits the parent model's light/color and visibility bit, chains this
/// actor's root coordinate under the parent's, places it at the spawnArg1
/// translation, and reparents the task.
static void func_actor_511000_80133240(Task* task)
{
    Task*      parent;
    TmdObject* extra;
    TmdObject* parentExtra;
    GfxCoord*  coord;
    GfxCoord*  dest;

    parent          = (Task*)task->spawnArg2.pointer;
    parentExtra     = parent->extra.tmd;
    extra           = task->extra.tmd;
    dest            = parentExtra->coords;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    extra->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord           = extra->coords;
    if (!(parentExtra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        extra->flags = 0;
    }
    func_actor_511000_80133760(task);
    coord->parent = dest;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// Tracks the parent model's visibility bit every frame: while the parent model
/// is hidden (`field_C` bit 0x80 clear) this clears its own bit and, for
/// spawnArg1 1 or 2, spins the root coordinate's yaw (0x46) by 0x294 or its
/// pitch (0x44) by 0x3E8, wrapping each to 0x1000. The rotation matrix is then
/// rebuilt from the angles and the coordinate's `composeStamp` cleared. With the parent
/// visible the rotation is left alone and the visibility bit is set instead.
static void func_actor_511000_801332E4(Task* task)
{
    TmdObject* extra;
    TmdObject* parentExtra;
    GfxCoord*  coord;

    extra       = task->extra.tmd;
    coord       = extra->coords;
    parentExtra = ((Task*)task->spawnArg2.pointer)->extra.tmd;

    if (!(parentExtra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        extra->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;

        switch (task->spawnArg1.value) {
            case 1:
                coord->param.rot.vy = ((u16)coord->param.rot.vy + 0x294) & 0xFFF;
                break;
            case 2:
                coord->param.rot.vx = ((u16)coord->param.rot.vx + 0x3E8) & 0xFFF;
                break;
        }

        RotMatrix(&coord->param.rot, &coord->coord);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        return;
    }
    extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
}

static void func_actor_511000_801333A4(Task* task)
{
    taskKill(task);
}

/// Inherits the parent model's light/color and visibility bit, chains this
/// actor's root coordinate under the parent's, places it at the spawnArg1
/// translation, copies `D_actor_511000_80147AC4` onto the Euler angles,
/// rebuilds the rotation matrix, and reparents the task.
static void func_actor_511000_801333C4(Task* task)
{
    Task*      parent;
    TmdObject* extra;
    TmdObject* parentExtra;
    GfxCoord*  coord;
    GfxCoord*  dest;

    parent          = (Task*)task->spawnArg2.pointer;
    parentExtra     = parent->extra.tmd;
    extra           = task->extra.tmd;
    dest            = parentExtra->coords;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    extra->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord           = extra->coords;
    if (!(parentExtra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        extra->flags = 0;
    }
    func_actor_511000_80133760(task);
    (coord)->parent = dest;
    Task_Reparent(parent, task);
    coord->param.rot.vx = D_actor_511000_80147AC4[0].vx;
    coord->param.rot.vy = D_actor_511000_80147AC4[0].vy;
    coord->param.rot.vz = D_actor_511000_80147AC4[0].vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    task->state        += 1;
}

static void func_actor_511000_80133498(Task* task)
{
    taskKill(task);
}

s32 func_actor_511000_801334B8(Task* arg0)
{
    arg0->killCountdown = 0;
    return 0;
}

/// Placement message handler that also shows the model: writes the payload's
/// translation and Euler angles into the root coordinate, rebuilds its
/// rotation, clears `composeStamp` so the world matrix is recomputed and clears the
/// model's hidden bit 0x80.
s32 func_actor_511000_801334C4(Task* task, s32 arg1, ActorTransform* args, s32 arg3)
{
    GfxCoord*  coord;
    TmdObject* extra;

    extra               = task->extra.tmd;
    coord               = extra->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags       &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return 0;
}

s32 func_actor_511000_80133554(Task* task, s32 arg1, s32 msg)
{
    TmdObject*       obj;
    Actor511000Work* work;
    Task*            child;
    s32              ret;
    s32              i;

    obj  = task->extra.tmd;
    work = (Actor511000Work*)task->work;
    ret  = 0;
    switch (msg) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->field_8 = msg;
            obj->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    if (msg == 1 && work->field_2F == 0) {
        for (i = 1; i < 4; i++) {
            child = Task_SpawnFromTable(D_actor_511000_80139924, D_actor_511000_80149054[i - 1], i, task);
            if (child != NULL) {
                child->extra.tmd->flags &= ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            }
        }
        work->field_2F = 1;
    }
    return ret;
}

/// Places the task's model at the indexed rotation and translation: copies
/// `rots[index]` onto the root coordinate's Euler angles, `trans[index]` into
/// its local translation, rebuilds the rotation matrix and marks the
/// coordinate dirty.
static void func_actor_511000_801336E0(Task* task, SVECTOR* rots, SVECTOR* trans, s32 index)
{
    GfxCoord* coord;
    SVECTOR*  rot;
    SVECTOR*  pos;
    s32       off;

    off                 = (index << 16) >> 13;
    rot                 = (SVECTOR*)(off + (s32)rots);
    coord               = task->extra.tmd->coords;
    coord->param.rot.vx = rot->vx;
    coord->param.rot.vy = rot->vy;
    pos                 = (SVECTOR*)(off + (s32)trans);
    coord->param.rot.vz = rot->vz;
    coord->coord.t[0]   = pos->vx;
    coord->coord.t[1]   = pos->vy;
    coord->coord.t[2]   = pos->vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Places the task's model at the indexed translation: copies
/// `D_actor_511000_80148FE4[spawnArg1]` into the root coordinate's local
/// translation, zeros the Euler angles, rebuilds the rotation matrix and
/// marks the coordinate dirty.
static void func_actor_511000_80133760(Task* task)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = D_actor_511000_80148FE4[task->spawnArg1.value].vx;
    coord->coord.t[1]   = D_actor_511000_80148FE4[task->spawnArg1.value].vy;
    coord->coord.t[2]   = D_actor_511000_80148FE4[task->spawnArg1.value].vz;
    coord->param.rot.vx = 0;
    coord->param.rot.vy = 0;
    coord->param.rot.vz = 0;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Binds the task's TMD object to the work-block light/color matrices, clears
/// the root coordinate flag, and rebuilds lighting from the world translation.
static void func_actor_511000_801337F0(Task* task)
{
    GfxCoord*        coord;
    Actor511000Work* work;
    TmdObject*       extra;

    work                = (Actor511000Work*)task->work;
    extra               = task->extra.tmd;
    coord               = extra->coords;
    extra->lightMtx     = &work->light;
    extra->colorMtx     = &work->color;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
}

void func_actor_511000_80133850(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E48;
    sp.funcs[task->state](task);
}

void func_actor_511000_801338A8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E54;
    sp.funcs[task->state](task);
}

void func_actor_511000_80133900(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E60;
    sp.funcs[task->state](task);
}

/// Spawn handler: allocates the 0x488-byte work block and parks it in
/// `Task::work`, binds the task's model to the block's matrices and
/// animation state, installs the message table, then spawns table entries 1
/// and 2 - tinting each child's model from the current area's record - and
/// entry 3, and advances to state 1. An allocation failure destroys the enemy.
static void func_actor_511000_80133958(GpEnemy* enemy, Task* task)
{
    enum { MODEL_HIDDEN = 0x80,
           STATE_UPDATE = 1 };
    GameLocationKey        key;
    GameLocationKey*       sessionKey;
    u8                     view;
    u8                     stage;
    GpAreaVariant*         layout;
    TmdObject*             model;
    GfxCoord*              coord;
    Actor511000ParentWork* work;
    TaskDesc*              table;
    u32                    placementWord;
    GpEnemy*               spawned;
    GameSession*           session;

    model = task->extra.tmd;
    coord = model->coords;
    work  = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work      = work;
    model->flags    = MODEL_HIDDEN;
    model->lightMtx = &work->field_45C;
    model->colorMtx = &work->field_43C;
    func_800B3F84(&work->anim, D_actor_511000_801550C0, model, work->field_30C, work->slots);
    work->field_47C     = 0;
    task->msgTable      = D_actor_511000_801550A0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    table   = D_actor_511000_80155070;
    spawned = Gp_SpawnEnemyFromTable(table, 1, 0, enemy);
    session = gGameSession;
    // Child models inherit the texture relocation of the parent's placement.
    // Reusing the spawn result preserves its register preference at the task load.
    spawned       = (GpEnemy*)spawned->task;
    sessionKey    = &session->location.loc;
    placementWord = enemy->placeKey;
    stage         = sessionKey->stage;
    model         = ((Task*)spawned)->extra.tmd;
    key.stage     = stage;
    key.area      = sessionKey->area;
    key.room      = sessionKey->room;
    view          = session->location.loc.view;
    placementWord = placementWord >> 12;
    key.view      = view;
    areaSyncLocationVariant(&key);
    layout                   = Gp_GetNestedAreaRec(&key);
    placementWord          <<= 4;
    placementWord           += (u32)layout->field_0;
    model->texturePageOffset = ((AreaPlacement*)placementWord)->texturePageOffset;
    model->clutRowOffset     = ((AreaPlacement*)placementWord)->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    spawned       = Gp_SpawnEnemyFromTable(table, 2, 0, enemy);
    session       = gGameSession;
    spawned       = (GpEnemy*)spawned->task;
    sessionKey    = &session->location.loc;
    placementWord = enemy->placeKey;
    stage         = sessionKey->stage;
    model         = ((Task*)spawned)->extra.tmd;
    key.stage     = stage;
    key.area      = sessionKey->area;
    key.room      = sessionKey->room;
    view          = session->location.loc.view;
    placementWord = placementWord >> 12;
    key.view      = view;
    areaSyncLocationVariant(&key);
    layout                   = Gp_GetNestedAreaRec(&key);
    placementWord          <<= 4;
    placementWord           += (u32)layout->field_0;
    model->texturePageOffset = ((AreaPlacement*)placementWord)->texturePageOffset;
    model->clutRowOffset     = ((AreaPlacement*)placementWord)->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    Gp_SpawnEnemyFromTable(table, 3, 0, enemy);
    task->state = STATE_UPDATE;
}

static void func_actor_511000_80133B80(GpEnemy* enemy, Task* task)
{
    TmdObject*             extra;
    VECTOR*                pos;
    VECTOR*                out;
    Actor511000ParentWork* work;
    GfxCoord*              coords;
    GfxCoord*              coord;
    s32                    i;
    s32                    j;
    s32                    flag;

    extra = task->extra.tmd;
    SCRATCH_PUSH_BYTES(0x20);
    work   = (Actor511000ParentWork*)task->work;
    coords = extra->coords;
    coord  = &coords[1];
    flag   = work->field_47C;
    pos    = SCRATCH_STACK_CURSOR(VECTOR);
    if (flag != 0) {
        for (i = 1; i < 19; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (work->field_47C < 3) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        pos->vx = coord->workm.t[0];
        pos->vy = coord->workm.t[1];
        pos->vz = coord->workm.t[2];
        Gp_UpdateActorColor(enemy, pos, 0, 0);
        pos->vx = coord->workm.t[0];
        pos->vy = coord->workm.t[1];
        pos->vz = coord->workm.t[2];
        out     = pos + 1;
        if (func_800EA1A8((VECTOR3*)pos, (VECTOR3*)out) != 0) {
            Gp_DrawEffGroundQuad((VECTOR3*)out, 0x400, Gp_State1C->groundShadowShade);
        }
    } else {
        if ((s16)(work->field_480 % 3) == 0) {
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) {
                    work->field_43C.m[i][j] = (work->field_43C.m[i][j] * 15) >> 4;
                    work->field_45C.m[i][j] = (work->field_45C.m[i][j] * 15) >> 4;
                }
            }
        }
        if (work->field_480 > 96) {
            task->state = 2;
        }
    }
    work->field_480++;
    coords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Runs the enemy's current state handler, copying the table onto the stack
/// before the call.
void func_actor_511000_80133D90(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_511000_80131E6C;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Copies the animation id from `preset` into the work block parked in
/// `task->work`, reseeds slots 1..0x12 through `Gp_AnimResetSlot`, and
/// clears `field_480`'s halfword.
s32 func_actor_511000_80133DEC(Task* task, s32 arg1, AnimationPlayRequest* preset)
{
    Actor511000Work2* work;
    s32               i;

    work            = (Actor511000Work2*)task->work;
    work->field_47C = preset->animationId;
    i               = 1;
    do {
        Gp_AnimResetSlot(&work->rig.anim, i, work->field_47C);
        i++;
    } while (i < 0x13);
    work->field_480.half = 0;
    return 0;
}

/// Placement message handler: builds the root coordinate's matrix from the
/// payload's Euler angles, drops its translation in and clears `composeStamp` so the
/// world matrix is recomputed.
s32 func_actor_511000_80133E48(Task* task, s32 arg1, ActorTransform* args)
{
    TmdObject* ext   = task->extra.tmd;
    GfxCoord*  coord = ext->coords;

    RotMatrix(&args->rot, &coord->coord);
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Visibility message handler: bit 0 of `arg2` shows the model (flags 0)
/// instead of hiding it (0x80); bit 1 also sets flag 0x4.
s32 func_actor_511000_80133EAC(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = task->extra.tmd;
    if (!(arg2 & 1)) {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        obj->flags = 0;
    }
    if (arg2 & 2) {
        obj         = task->extra.tmd;
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

void func_actor_511000_80133EF4(Task* task)
{
    GpEnemyTaskFunc fns[2] = { func_actor_511000_80133F48, func_actor_511000_80133F88 };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Spawn state of the model child attached to the spawner's part 8: chains
/// the root coordinate under that part, takes the spawner work block's light
/// and colour matrices, shows the model and advances to the tick state.
static void func_actor_511000_80133F48(GpEnemy* enemy, Task* task)
{
    Task*                  parent;
    TmdObject*             obj;
    Actor511000ParentWork* work;
    GfxCoord*              coord;
    GfxCoord*              parentCoords;

    parent       = task->parent;
    obj          = task->extra.tmd;
    parentCoords = parent->extra.tmd->coords;
    coord        = obj->coords;
    work         = (Actor511000ParentWork*)parent->work;

    coord->parent = &parentCoords[8];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

static void func_actor_511000_80133F88(GpEnemy* arg0, Task* arg1)
{
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
}

void func_actor_511000_80133FC8(Task* task)
{
    GpEnemyTaskFunc fns[2] = { func_actor_511000_8013401C, func_actor_511000_8013405C };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_511000_8013401C(GpEnemy* enemy, Task* task)
{
    Task*                  parent;
    TmdObject*             obj;
    Actor511000ParentWork* work;
    GfxCoord*              coord;
    GfxCoord*              parentCoords;

    parent       = task->parent;
    obj          = task->extra.tmd;
    parentCoords = parent->extra.tmd->coords;
    coord        = obj->coords;
    work         = (Actor511000ParentWork*)parent->work;

    coord->parent = &parentCoords[3];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

static void func_actor_511000_8013405C(GpEnemy* arg0, Task* arg1)
{
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
}

void func_actor_511000_8013409C(Task* task)
{
    GpEnemyTaskFunc fns[2] = { func_actor_511000_801340F0, func_actor_511000_80134130 };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_511000_801340F0(GpEnemy* enemy, Task* task)
{
    Task*                  parent;
    TmdObject*             obj;
    Actor511000ParentWork* work;
    GfxCoord*              coord;
    GfxCoord*              parentCoords;

    parent       = task->parent;
    obj          = task->extra.tmd;
    parentCoords = parent->extra.tmd->coords;
    coord        = obj->coords;
    work         = (Actor511000ParentWork*)parent->work;

    coord->parent = &parentCoords[12];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

static void func_actor_511000_80134130(GpEnemy* arg0, Task* arg1)
{
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
}
