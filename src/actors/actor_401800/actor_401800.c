#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
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
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/player_detection.h"
#include "../../shared/actor_messages.h"
// Which of the two Odd Stranger builds this package is (odd_stranger.h).
#define ODD_STRANGER_VARIANT 2
#include "../../shared/odd_stranger.h"

/// The 34 state handlers copied to the frame before the per-frame dispatch.
typedef struct Actor401800StateTable {
    TaskFunc fn[34];
} Actor401800StateTable;
STATIC_ASSERT_SIZEOF(Actor401800StateTable, 0x88);

static const Actor401800StateTable D_actor_401800_80131FDC;

/// Payload of the `0x3FF` message `func_actor_401800_80138F5C` sends: the same
/// 0x14-byte animation record other actors keep as `AnimationPlayRequest` data
/// (`D_actor_356100_80173244` and friends); `field_4` is the animation id.
extern AnimationPlayRequest D_actor_401800_80155A0C;

extern AnimationSet* D_actor_401800_801559F8[];
extern AnimationSet* D_actor_401800_801559F0[];

/// Twelve `SVECTOR` hit positions `func_actor_401800_801348A8` picks from by
/// damage magnitude: the low four when the hit is light, the high two when it
/// is heavy, and the last four on the `arg1 > 0` / `arg1 <= 0` split in
/// between. The fourth halfword (`pad`, unused by the effect itself) is the
/// model part index `func_800FDB18` anchors the spawned effect to. Same role
/// `Actor00100_D1B9F4` plays for `Actor00100_Fn03340`.
extern SVECTOR D_actor_401800_80155A20[12];

/// Animation bank both `func_800B3F84` contexts are initialised from. Same
/// role `Actor01900_D17174` plays for actor 01900.
extern AnimationSet* D_actor_401800_80155938[46];

/// Enemy description record the init body copies `hpMax` out of into
/// `Enemy.hp` and points `Enemy.param` at. Same role
/// `D_actor_401300_80141FA0` plays for actor 401300.
extern EnemyParams D_actor_401800_8013E6F0;

/// The three `ActorSpawnParamRow` variants the init body picks from by the
/// spawn argument's low nibble: `[0]` when it is 2, `[2]` when it is 1, `[1]`
/// otherwise. Same table shape as `Actor01900_D0AC64`.
extern ActorSpawnParamRow D_actor_401800_8013E700[];

/// Handler table the actor's task receives in `Task::msgTable`; same role
/// `Actor01900_D1728C` plays for actor 01900.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32  (*call0)(Task*);
        s32  (*call1)(Task*, s32, AnimationPlayRequest*);
        s32  (*call2)(Task*, s32, ActorTransform*);
        s32  (*call3)(Task*, s32, s32);
        s32  (*call4)(Task*, s32, u16*);
        void (*call5)(void);
    } handler;
} Actor401800MessageEntry;
STATIC_ASSERT_SIZEOF(Actor401800MessageEntry, 8);

extern Actor401800MessageEntry D_actor_401800_80155A80[8];

/// Payload `func_actor_401800_80138C28` fills and sends with message 0x3E9.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    ActorTransform value;
    u8             retained[8];
} Actor401800Storage5AD8;
STATIC_ASSERT_SIZEOF(Actor401800Storage5AD8, 32);

/// Frame counter the chase body of `func_actor_80136EAC` accumulates its step
/// `field_C04` into and the init body clears; the aim-and-rescale body reads it
/// back as the phase of the step it walks. Same role `Actor01900_D172FC` plays
/// for actor 01900.
extern u16 D_actor_401800_80155AC0;

/// The block `func_actor_401800_8013A034` posts into `D_actor_401800_80155938[16]`
/// when the actor's live flag is set, taking over the animation the actor had
/// been running. Same pair `Actor401300` keeps as `D_actor_401300_80158878` /
/// `D_actor_401300_80152BB8`.
extern AnimationSet D_actor_401800_80155124;

/// Gameplay slot `Gp_SpawnEff` effects read their model data from; set before
/// each spawn in `func_actor_401800_8013BB10`.

/// The records closing three of the overlay's model streams, which
/// `func_actor_401800_8013BB10` points `D_80114B34[5].data.model` at before spawning: the
/// 0x60030 debris burst, then the 0xA0005 fan the step counter trips at 3 and 5
/// and the two 0xA0005 bursts at 7 and 9.
extern TmdSource D_actor_401800_80143E9C;
extern TmdSource D_actor_401800_80144434;
extern TmdSource D_actor_401800_80144F24;

#include "../../shared/actor_contacts.h"

static s32  func_actor_401800_8013629C(Task* arg0, WorldCollisionContact* recs, s16 count);
static void func_actor_401800_801348A8(Task* arg0, s16 arg1, s32 arg2);

static void func_actor_401800_8013423C(Enemy* enemy, Task* actor);
static void func_actor_401800_8013E0A0(Task* task);
static void func_actor_401800_8013E138(Task* arg0);
static void func_actor_401800_8013E194(Task* arg0);
static void func_actor_401800_8013E23C(Task* arg0);
static void func_actor_401800_8013E2E8(Task* arg0);
static void func_actor_401800_8013E394(Task* arg0);
static void func_actor_401800_8013E44C(Task* arg0);
static void func_actor_401800_8013E4F0(Task* arg0);
static void func_actor_401800_8013E5A4(Task* arg0);
static void func_actor_401800_801381E4(Task* arg0);

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

/// Integer part of the last movement step `func_actor_401800_80132C68`
/// applied, nudged one unit outward where the step had a fractional part.
static SVECTOR ActorContact_ScratchPosition;

extern Actor401800Storage5AD8 D_actor_401800_80155AD8;

extern GpDelayArg D_actor_401800_80155AF8;

/// Clip-transition table the cross-fade reads: one byte per (previous clip,
/// requested clip) pair, rows of 0x2D, handed to `func_800B4114` as the
/// transition argument.
extern s8 gOddStrangerTransitions[45][45];

extern AnimationSet D_actor_401800_80151214;
extern AnimationSet D_actor_401800_80151C0C;
extern AnimationSet D_actor_401800_8015256C;

extern TmdSource D_actor_401800_80143918;
s32              func_actor_401800_8013DF80(Task*, s32, u16*);
void             func_actor_401800_8013DCB4(void);
void             func_actor_401800_8013E68C(Task*);

extern AnimationSet D_actor_401800_80146F38;
extern AnimationSet D_actor_401800_80147518;
extern AnimationSet D_actor_401800_80147AFC;
extern AnimationSet D_actor_401800_80148550;
extern AnimationSet D_actor_401800_80149B04;
extern AnimationSet D_actor_401800_80149E94;
extern AnimationSet D_actor_401800_8014A6CC;
extern AnimationSet D_actor_401800_8014AC1C;
extern AnimationSet D_actor_401800_8014BB34;
extern AnimationSet D_actor_401800_8014BED8;
extern AnimationSet D_actor_401800_8014C28C;
extern AnimationSet D_actor_401800_8014C610;
extern AnimationSet D_actor_401800_8014CD04;
extern AnimationSet D_actor_401800_8014D568;
extern AnimationSet D_actor_401800_8014DDE0;
extern AnimationSet D_actor_401800_8014E430;
extern AnimationSet D_actor_401800_8014EE04;
extern AnimationSet D_actor_401800_8014FC7C;
extern AnimationSet D_actor_401800_80150B8C;
extern AnimationSet D_actor_401800_80152F00;
extern AnimationSet D_actor_401800_801538D8;
extern AnimationSet D_actor_401800_801542C8;

DamageAttack D_actor_401800_8013E6E8[2] = {
    { 26, 7 },
    { 18, 7 },
};

EnemyParams D_actor_401800_8013E6F0 = { D_actor_401800_8013E6E8, 180, 34, 34, 3, 100, 10, 100, 0 };

ActorSpawnParamRow D_actor_401800_8013E700[3] = {
    { 40, 400, 7, 2000, { 0, 0, 0, 0 } },
    { 20, 400, 9, 2500, { 0, 0, 0, 0 } },
    { 0, 600, 5, 3000, { 0, 0, 0, 0 } },
};

TmdBone D_actor_401800_8013E724[19] = {
#include "assets/actor_401800_model_11AF8_skeleton.inc"
};

u32 D_actor_401800_8013E9D0[19] = {
#include "assets/actor_401800_model_11AF8_partVerts.inc"
};

SVECTOR D_actor_401800_8013EA1C[295] = {
#include "assets/actor_401800_model_11AF8_verts.inc"
};

SVECTOR D_actor_401800_8013F354[335] = {
#include "assets/actor_401800_model_11AF8_normals.inc"
};

u32 D_actor_401800_8013FDCC[3795] = {
#include "assets/actor_401800_model_11AF8_stream.inc"
};

TmdSource D_actor_401800_80143918 = {
    0,
    19204,
    7092,
    19,
    D_actor_401800_8013E9D0,
    D_actor_401800_8013EA1C,
    D_actor_401800_8013F354,
    D_actor_401800_8013E724,
    D_actor_401800_8013FDCC,
};

TmdBone D_actor_401800_8014393C[1] = {
#include "assets/actor_401800_model_1207C_skeleton.inc"
};

u32 D_actor_401800_80143960[1] = {
#include "assets/actor_401800_model_1207C_partVerts.inc"
};

SVECTOR D_actor_401800_80143964[24] = {
#include "assets/actor_401800_model_1207C_verts.inc"
};

SVECTOR D_actor_401800_80143A24[34] = {
#include "assets/actor_401800_model_1207C_normals.inc"
};

u32 D_actor_401800_80143B34[218] = {
#include "assets/actor_401800_model_1207C_stream.inc"
};

TmdSource D_actor_401800_80143E9C = {
    0,
    1452,
    0,
    1,
    D_actor_401800_80143960,
    D_actor_401800_80143964,
    D_actor_401800_80143A24,
    D_actor_401800_8014393C,
    D_actor_401800_80143B34,
};

TmdBone D_actor_401800_80143EC0[1] = {
#include "assets/actor_401800_model_12614_skeleton.inc"
};

u32 D_actor_401800_80143EE4[1] = {
#include "assets/actor_401800_model_12614_partVerts.inc"
};

SVECTOR D_actor_401800_80143EE8[22] = {
#include "assets/actor_401800_model_12614_verts.inc"
};

SVECTOR D_actor_401800_80143F98[33] = {
#include "assets/actor_401800_model_12614_normals.inc"
};

u32 D_actor_401800_801440A0[229] = {
#include "assets/actor_401800_model_12614_stream.inc"
};

TmdSource D_actor_401800_80144434 = {
    0,
    1488,
    0,
    1,
    D_actor_401800_80143EE4,
    D_actor_401800_80143EE8,
    D_actor_401800_80143F98,
    D_actor_401800_80143EC0,
    D_actor_401800_801440A0,
};

TmdBone D_actor_401800_80144458[1] = {
#include "assets/actor_401800_model_13104_skeleton.inc"
};

u32 D_actor_401800_8014447C[1] = {
#include "assets/actor_401800_model_13104_partVerts.inc"
};

SVECTOR D_actor_401800_80144480[47] = {
#include "assets/actor_401800_model_13104_verts.inc"
};

SVECTOR D_actor_401800_801445F8[68] = {
#include "assets/actor_401800_model_13104_normals.inc"
};

u32 D_actor_401800_80144818[451] = {
#include "assets/actor_401800_model_13104_stream.inc"
};

TmdSource D_actor_401800_80144F24 = {
    0,
    3064,
    0,
    1,
    D_actor_401800_8014447C,
    D_actor_401800_80144480,
    D_actor_401800_801445F8,
    D_actor_401800_80144458,
    D_actor_401800_80144818,
};

AnimationPackedPose D_actor_401800_80144F48[28] = {
#include "assets/actor_401800_animation_13C24_bank1.inc"
};

AnimationPackedRotation D_actor_401800_80145098[247] = {
#include "assets/actor_401800_animation_13C24_bank4.inc"
};

AnimationRecord D_actor_401800_80145474[362] = {
#include "assets/actor_401800_animation_13C24_records.inc"
};

u16 D_actor_401800_80145A1C[20] = {
#include "assets/actor_401800_animation_13C24_indices.inc"
};

AnimationSet D_actor_401800_80145A44 = {
    D_actor_401800_80145474,
    D_actor_401800_80145A1C,
    { NULL, D_actor_401800_80144F48, NULL, NULL, D_actor_401800_80145098, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_80145A6C[32] = {
#include "assets/actor_401800_animation_146A8_bank1.inc"
};

AnimationPackedRotation D_actor_401800_80145BEC[223] = {
#include "assets/actor_401800_animation_146A8_bank4.inc"
};

AnimationRecord D_actor_401800_80145F68[334] = {
#include "assets/actor_401800_animation_146A8_records.inc"
};

u16 D_actor_401800_801464A0[20] = {
#include "assets/actor_401800_animation_146A8_indices.inc"
};

AnimationSet D_actor_401800_801464C8 = {
    D_actor_401800_80145F68,
    D_actor_401800_801464A0,
    { NULL, D_actor_401800_80145A6C, NULL, NULL, D_actor_401800_80145BEC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_801464F0[24] = {
#include "assets/actor_401800_animation_15118_bank1.inc"
};

AnimationPackedRotation D_actor_401800_80146610[248] = {
#include "assets/actor_401800_animation_15118_bank4.inc"
};

AnimationRecord D_actor_401800_801469F0[328] = {
#include "assets/actor_401800_animation_15118_records.inc"
};

u16 D_actor_401800_80146F10[20] = {
#include "assets/actor_401800_animation_15118_indices.inc"
};

AnimationSet D_actor_401800_80146F38 = {
    D_actor_401800_801469F0,
    D_actor_401800_80146F10,
    { NULL, D_actor_401800_801464F0, NULL, NULL, D_actor_401800_80146610, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_80146F60[10] = {
#include "assets/actor_401800_animation_156F8_bank1.inc"
};

AnimationPackedRotation D_actor_401800_80146FD8[138] = {
#include "assets/actor_401800_animation_156F8_bank4.inc"
};

AnimationRecord D_actor_401800_80147200[188] = {
#include "assets/actor_401800_animation_156F8_records.inc"
};

u16 D_actor_401800_801474F0[20] = {
#include "assets/actor_401800_animation_156F8_indices.inc"
};

AnimationSet D_actor_401800_80147518 = {
    D_actor_401800_80147200,
    D_actor_401800_801474F0,
    { NULL, D_actor_401800_80146F60, NULL, NULL, D_actor_401800_80146FD8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_80147540[8] = {
#include "assets/actor_401800_animation_15CDC_bank1.inc"
};

AnimationPackedRotation D_actor_401800_801475A0[151] = {
#include "assets/actor_401800_animation_15CDC_bank4.inc"
};

AnimationRecord D_actor_401800_801477FC[182] = {
#include "assets/actor_401800_animation_15CDC_records.inc"
};

u16 D_actor_401800_80147AD4[20] = {
#include "assets/actor_401800_animation_15CDC_indices.inc"
};

AnimationSet D_actor_401800_80147AFC = {
    D_actor_401800_801477FC,
    D_actor_401800_80147AD4,
    { NULL, D_actor_401800_80147540, NULL, NULL, D_actor_401800_801475A0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_80147B24[20] = {
#include "assets/actor_401800_animation_16730_bank1.inc"
};

AnimationPackedRotation D_actor_401800_80147C14[228] = {
#include "assets/actor_401800_animation_16730_bank4.inc"
};

AnimationRecord D_actor_401800_80147FA4[353] = {
#include "assets/actor_401800_animation_16730_records.inc"
};

u16 D_actor_401800_80148528[20] = {
#include "assets/actor_401800_animation_16730_indices.inc"
};

AnimationSet D_actor_401800_80148550 = {
    D_actor_401800_80147FA4,
    D_actor_401800_80148528,
    { NULL, D_actor_401800_80147B24, NULL, NULL, D_actor_401800_80147C14, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_80148578[23] = {
#include "assets/actor_401800_animation_17474_bank1.inc"
};

AnimationPackedRotation D_actor_401800_8014868C[334] = {
#include "assets/actor_401800_animation_17474_bank4.inc"
};

AnimationRecord D_actor_401800_80148BC4[426] = {
#include "assets/actor_401800_animation_17474_records.inc"
};

u16 D_actor_401800_8014926C[20] = {
#include "assets/actor_401800_animation_17474_indices.inc"
};

AnimationSet D_actor_401800_80149294 = {
    D_actor_401800_80148BC4,
    D_actor_401800_8014926C,
    { NULL, D_actor_401800_80148578, NULL, NULL, D_actor_401800_8014868C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_801492BC[14] = {
#include "assets/actor_401800_animation_17CE4_bank1.inc"
};

AnimationPackedRotation D_actor_401800_80149364[216] = {
#include "assets/actor_401800_animation_17CE4_bank4.inc"
};

AnimationRecord D_actor_401800_801496C4[262] = {
#include "assets/actor_401800_animation_17CE4_records.inc"
};

u16 D_actor_401800_80149ADC[20] = {
#include "assets/actor_401800_animation_17CE4_indices.inc"
};

AnimationSet D_actor_401800_80149B04 = {
    D_actor_401800_801496C4,
    D_actor_401800_80149ADC,
    { NULL, D_actor_401800_801492BC, NULL, NULL, D_actor_401800_80149364, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_80149B2C[5] = {
#include "assets/actor_401800_animation_18074_bank1.inc"
};

AnimationPackedRotation D_actor_401800_80149B68[82] = {
#include "assets/actor_401800_animation_18074_bank4.inc"
};

AnimationRecord D_actor_401800_80149CB0[111] = {
#include "assets/actor_401800_animation_18074_records.inc"
};

u16 D_actor_401800_80149E6C[20] = {
#include "assets/actor_401800_animation_18074_indices.inc"
};

AnimationSet D_actor_401800_80149E94 = {
    D_actor_401800_80149CB0,
    D_actor_401800_80149E6C,
    { NULL, D_actor_401800_80149B2C, NULL, NULL, D_actor_401800_80149B68, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_80149EBC[16] = {
#include "assets/actor_401800_animation_188AC_bank1.inc"
};

AnimationPackedRotation D_actor_401800_80149F7C[199] = {
#include "assets/actor_401800_animation_188AC_bank4.inc"
};

AnimationRecord D_actor_401800_8014A298[259] = {
#include "assets/actor_401800_animation_188AC_records.inc"
};

u16 D_actor_401800_8014A6A4[20] = {
#include "assets/actor_401800_animation_188AC_indices.inc"
};

AnimationSet D_actor_401800_8014A6CC = {
    D_actor_401800_8014A298,
    D_actor_401800_8014A6A4,
    { NULL, D_actor_401800_80149EBC, NULL, NULL, D_actor_401800_80149F7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_8014A6F4[10] = {
#include "assets/actor_401800_animation_18DFC_bank1.inc"
};

AnimationPackedRotation D_actor_401800_8014A76C[113] = {
#include "assets/actor_401800_animation_18DFC_bank4.inc"
};

AnimationRecord D_actor_401800_8014A930[177] = {
#include "assets/actor_401800_animation_18DFC_records.inc"
};

u16 D_actor_401800_8014ABF4[20] = {
#include "assets/actor_401800_animation_18DFC_indices.inc"
};

AnimationSet D_actor_401800_8014AC1C = {
    D_actor_401800_8014A930,
    D_actor_401800_8014ABF4,
    { NULL, D_actor_401800_8014A6F4, NULL, NULL, D_actor_401800_8014A76C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_8014AC44[26] = {
#include "assets/actor_401800_animation_19D14_bank1.inc"
};

AnimationPackedRotation D_actor_401800_8014AD7C[330] = {
#include "assets/actor_401800_animation_19D14_bank4.inc"
};

AnimationRecord D_actor_401800_8014B2A4[538] = {
#include "assets/actor_401800_animation_19D14_records.inc"
};

u16 D_actor_401800_8014BB0C[20] = {
#include "assets/actor_401800_animation_19D14_indices.inc"
};

AnimationSet D_actor_401800_8014BB34 = {
    D_actor_401800_8014B2A4,
    D_actor_401800_8014BB0C,
    { NULL, D_actor_401800_8014AC44, NULL, NULL, D_actor_401800_8014AD7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_8014BB5C[10] = {
#include "assets/actor_401800_animation_1A0B8_bank1.inc"
};

AnimationPackedRotation D_actor_401800_8014BBD4[73] = {
#include "assets/actor_401800_animation_1A0B8_bank4.inc"
};

AnimationRecord D_actor_401800_8014BCF8[110] = {
#include "assets/actor_401800_animation_1A0B8_records.inc"
};

u16 D_actor_401800_8014BEB0[20] = {
#include "assets/actor_401800_animation_1A0B8_indices.inc"
};

AnimationSet D_actor_401800_8014BED8 = {
    D_actor_401800_8014BCF8,
    D_actor_401800_8014BEB0,
    { NULL, D_actor_401800_8014BB5C, NULL, NULL, D_actor_401800_8014BBD4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_8014BF00[6] = {
#include "assets/actor_401800_animation_1A46C_bank1.inc"
};

AnimationPackedRotation D_actor_401800_8014BF48[81] = {
#include "assets/actor_401800_animation_1A46C_bank4.inc"
};

AnimationRecord D_actor_401800_8014C08C[118] = {
#include "assets/actor_401800_animation_1A46C_records.inc"
};

u16 D_actor_401800_8014C264[20] = {
#include "assets/actor_401800_animation_1A46C_indices.inc"
};

AnimationSet D_actor_401800_8014C28C = {
    D_actor_401800_8014C08C,
    D_actor_401800_8014C264,
    { NULL, D_actor_401800_8014BF00, NULL, NULL, D_actor_401800_8014BF48, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_8014C2B4[13] = {
#include "assets/actor_401800_animation_1A7F0_bank1.inc"
};

AnimationPackedRotation D_actor_401800_8014C350[58] = {
#include "assets/actor_401800_animation_1A7F0_bank4.inc"
};

AnimationRecord D_actor_401800_8014C438[108] = {
#include "assets/actor_401800_animation_1A7F0_records.inc"
};

u16 D_actor_401800_8014C5E8[20] = {
#include "assets/actor_401800_animation_1A7F0_indices.inc"
};

AnimationSet D_actor_401800_8014C610 = {
    D_actor_401800_8014C438,
    D_actor_401800_8014C5E8,
    { NULL, D_actor_401800_8014C2B4, NULL, NULL, D_actor_401800_8014C350, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_8014C638[15] = {
#include "assets/actor_401800_animation_1AEE4_bank1.inc"
};

AnimationPackedRotation D_actor_401800_8014C6EC[159] = {
#include "assets/actor_401800_animation_1AEE4_bank4.inc"
};

AnimationRecord D_actor_401800_8014C968[221] = {
#include "assets/actor_401800_animation_1AEE4_records.inc"
};

u16 D_actor_401800_8014CCDC[20] = {
#include "assets/actor_401800_animation_1AEE4_indices.inc"
};

AnimationSet D_actor_401800_8014CD04 = {
    D_actor_401800_8014C968,
    D_actor_401800_8014CCDC,
    { NULL, D_actor_401800_8014C638, NULL, NULL, D_actor_401800_8014C6EC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_8014CD2C[20] = {
#include "assets/actor_401800_animation_1B748_bank1.inc"
};

AnimationPackedRotation D_actor_401800_8014CE1C[190] = {
#include "assets/actor_401800_animation_1B748_bank4.inc"
};

AnimationRecord D_actor_401800_8014D114[267] = {
#include "assets/actor_401800_animation_1B748_records.inc"
};

u16 D_actor_401800_8014D540[20] = {
#include "assets/actor_401800_animation_1B748_indices.inc"
};

AnimationSet D_actor_401800_8014D568 = {
    D_actor_401800_8014D114,
    D_actor_401800_8014D540,
    { NULL, D_actor_401800_8014CD2C, NULL, NULL, D_actor_401800_8014CE1C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_8014D590[20] = {
#include "assets/actor_401800_animation_1BFC0_bank1.inc"
};

AnimationPackedRotation D_actor_401800_8014D680[196] = {
#include "assets/actor_401800_animation_1BFC0_bank4.inc"
};

AnimationRecord D_actor_401800_8014D990[266] = {
#include "assets/actor_401800_animation_1BFC0_records.inc"
};

u16 D_actor_401800_8014DDB8[20] = {
#include "assets/actor_401800_animation_1BFC0_indices.inc"
};

AnimationSet D_actor_401800_8014DDE0 = {
    D_actor_401800_8014D990,
    D_actor_401800_8014DDB8,
    { NULL, D_actor_401800_8014D590, NULL, NULL, D_actor_401800_8014D680, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_8014DE08[11] = {
#include "assets/actor_401800_animation_1C610_bank1.inc"
};

AnimationPackedRotation D_actor_401800_8014DE8C[150] = {
#include "assets/actor_401800_animation_1C610_bank4.inc"
};

AnimationRecord D_actor_401800_8014E0E4[201] = {
#include "assets/actor_401800_animation_1C610_records.inc"
};

u16 D_actor_401800_8014E408[20] = {
#include "assets/actor_401800_animation_1C610_indices.inc"
};

AnimationSet D_actor_401800_8014E430 = {
    D_actor_401800_8014E0E4,
    D_actor_401800_8014E408,
    { NULL, D_actor_401800_8014DE08, NULL, NULL, D_actor_401800_8014DE8C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_8014E458[18] = {
#include "assets/actor_401800_animation_1CFE4_bank1.inc"
};

AnimationPackedRotation D_actor_401800_8014E530[232] = {
#include "assets/actor_401800_animation_1CFE4_bank4.inc"
};

AnimationRecord D_actor_401800_8014E8D0[323] = {
#include "assets/actor_401800_animation_1CFE4_records.inc"
};

u16 D_actor_401800_8014EDDC[20] = {
#include "assets/actor_401800_animation_1CFE4_indices.inc"
};

AnimationSet D_actor_401800_8014EE04 = {
    D_actor_401800_8014E8D0,
    D_actor_401800_8014EDDC,
    { NULL, D_actor_401800_8014E458, NULL, NULL, D_actor_401800_8014E530, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_8014EE2C[33] = {
#include "assets/actor_401800_animation_1DE5C_bank1.inc"
};

AnimationPackedRotation D_actor_401800_8014EFB8[326] = {
#include "assets/actor_401800_animation_1DE5C_bank4.inc"
};

AnimationRecord D_actor_401800_8014F4D0[481] = {
#include "assets/actor_401800_animation_1DE5C_records.inc"
};

u16 D_actor_401800_8014FC54[20] = {
#include "assets/actor_401800_animation_1DE5C_indices.inc"
};

AnimationSet D_actor_401800_8014FC7C = {
    D_actor_401800_8014F4D0,
    D_actor_401800_8014FC54,
    { NULL, D_actor_401800_8014EE2C, NULL, NULL, D_actor_401800_8014EFB8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_8014FCA4[26] = {
#include "assets/actor_401800_animation_1ED6C_bank1.inc"
};

AnimationPackedRotation D_actor_401800_8014FDDC[351] = {
#include "assets/actor_401800_animation_1ED6C_bank4.inc"
};

AnimationRecord D_actor_401800_80150358[515] = {
#include "assets/actor_401800_animation_1ED6C_records.inc"
};

u16 D_actor_401800_80150B64[20] = {
#include "assets/actor_401800_animation_1ED6C_indices.inc"
};

AnimationSet D_actor_401800_80150B8C = {
    D_actor_401800_80150358,
    D_actor_401800_80150B64,
    { NULL, D_actor_401800_8014FCA4, NULL, NULL, D_actor_401800_8014FDDC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_80150BB4[15] = {
#include "assets/actor_401800_animation_1F3F4_bank1.inc"
};

AnimationPackedRotation D_actor_401800_80150C68[154] = {
#include "assets/actor_401800_animation_1F3F4_bank4.inc"
};

AnimationRecord D_actor_401800_80150ED0[199] = {
#include "assets/actor_401800_animation_1F3F4_records.inc"
};

u16 D_actor_401800_801511EC[20] = {
#include "assets/actor_401800_animation_1F3F4_indices.inc"
};

AnimationSet D_actor_401800_80151214 = {
    D_actor_401800_80150ED0,
    D_actor_401800_801511EC,
    { NULL, D_actor_401800_80150BB4, NULL, NULL, D_actor_401800_80150C68, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_8015123C[19] = {
#include "assets/actor_401800_animation_1FDEC_bank1.inc"
};

AnimationPackedRotation D_actor_401800_80151320[250] = {
#include "assets/actor_401800_animation_1FDEC_bank4.inc"
};

AnimationRecord D_actor_401800_80151708[311] = {
#include "assets/actor_401800_animation_1FDEC_records.inc"
};

u16 D_actor_401800_80151BE4[20] = {
#include "assets/actor_401800_animation_1FDEC_indices.inc"
};

AnimationSet D_actor_401800_80151C0C = {
    D_actor_401800_80151708,
    D_actor_401800_80151BE4,
    { NULL, D_actor_401800_8015123C, NULL, NULL, D_actor_401800_80151320, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_80151C34[21] = {
#include "assets/actor_401800_animation_2074C_bank1.inc"
};

AnimationPackedRotation D_actor_401800_80151D30[233] = {
#include "assets/actor_401800_animation_2074C_bank4.inc"
};

AnimationRecord D_actor_401800_801520D4[284] = {
#include "assets/actor_401800_animation_2074C_records.inc"
};

u16 D_actor_401800_80152544[20] = {
#include "assets/actor_401800_animation_2074C_indices.inc"
};

AnimationSet D_actor_401800_8015256C = {
    D_actor_401800_801520D4,
    D_actor_401800_80152544,
    { NULL, D_actor_401800_80151C34, NULL, NULL, D_actor_401800_80151D30, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_80152594[28] = {
#include "assets/actor_401800_animation_210E0_bank1.inc"
};

AnimationPackedRotation D_actor_401800_801526E4[205] = {
#include "assets/actor_401800_animation_210E0_bank4.inc"
};

AnimationRecord D_actor_401800_80152A18[304] = {
#include "assets/actor_401800_animation_210E0_records.inc"
};

u16 D_actor_401800_80152ED8[20] = {
#include "assets/actor_401800_animation_210E0_indices.inc"
};

AnimationSet D_actor_401800_80152F00 = {
    D_actor_401800_80152A18,
    D_actor_401800_80152ED8,
    { NULL, D_actor_401800_80152594, NULL, NULL, D_actor_401800_801526E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_80152F28[30] = {
#include "assets/actor_401800_animation_21AB8_bank1.inc"
};

AnimationPackedRotation D_actor_401800_80153090[219] = {
#include "assets/actor_401800_animation_21AB8_bank4.inc"
};

AnimationRecord D_actor_401800_801533FC[301] = {
#include "assets/actor_401800_animation_21AB8_records.inc"
};

u16 D_actor_401800_801538B0[20] = {
#include "assets/actor_401800_animation_21AB8_indices.inc"
};

AnimationSet D_actor_401800_801538D8 = {
    D_actor_401800_801533FC,
    D_actor_401800_801538B0,
    { NULL, D_actor_401800_80152F28, NULL, NULL, D_actor_401800_80153090, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_80153900[19] = {
#include "assets/actor_401800_animation_224A8_bank1.inc"
};

AnimationPackedRotation D_actor_401800_801539E4[252] = {
#include "assets/actor_401800_animation_224A8_bank4.inc"
};

AnimationRecord D_actor_401800_80153DD4[307] = {
#include "assets/actor_401800_animation_224A8_records.inc"
};

u16 D_actor_401800_801542A0[20] = {
#include "assets/actor_401800_animation_224A8_indices.inc"
};

AnimationSet D_actor_401800_801542C8 = {
    D_actor_401800_80153DD4,
    D_actor_401800_801542A0,
    { NULL, D_actor_401800_80153900, NULL, NULL, D_actor_401800_801539E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_401800_801542F0[18] = {
#include "assets/actor_401800_animation_23304_bank1.inc"
};

AnimationPackedRotation D_actor_401800_801543C8[361] = {
#include "assets/actor_401800_animation_23304_bank4.inc"
};

AnimationRecord D_actor_401800_8015496C[484] = {
#include "assets/actor_401800_animation_23304_records.inc"
};

u16 D_actor_401800_801550FC[20] = {
#include "assets/actor_401800_animation_23304_indices.inc"
};

AnimationSet D_actor_401800_80155124 = {
    D_actor_401800_8015496C,
    D_actor_401800_801550FC,
    { NULL, D_actor_401800_801542F0, NULL, NULL, D_actor_401800_801543C8, NULL, NULL, NULL },
};

s8 gOddStrangerTransitions[45][45] = {
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 4, 4, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 3, 3, 0, 0, 0, 0, 15, 5, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 4, 3, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 5, 3, 3, 3, 3, 5, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 5, 5, 3, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 5, 12, 12, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 8, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AnimationSet* D_actor_401800_80155938[46] = {
    NULL,
    NULL,
    &D_actor_401800_80152F00,
    &D_actor_401800_801538D8,
    &D_actor_401800_80150B8C,
    &D_actor_401800_8014E430,
    &D_actor_401800_8014EE04,
    &D_actor_401800_8014FC7C,
    &D_actor_401800_801542C8,
    &D_actor_401800_8014A6CC,
    &D_actor_401800_8014BED8,
    &D_actor_401800_8014C28C,
    &D_actor_401800_80147AFC,
    &D_actor_401800_80147518,
    &D_actor_401800_8014BB34,
    &D_actor_401800_80148550,
    NULL,
    &D_actor_401800_8014AC1C,
    &D_actor_401800_80146F38,
    &D_actor_401800_8014CD04,
    &D_actor_401800_8014D568,
    &D_actor_401800_8014DDE0,
    &D_actor_401800_80149B04,
    &D_actor_401800_8014C28C,
    &D_actor_401800_80147AFC,
    &D_actor_401800_80149E94,
    &D_actor_401800_8014C610,
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
    NULL,
    NULL,
    NULL,
    NULL,
};

AnimationSet* D_actor_401800_801559F0[2] = { 0 };

AnimationSet* D_actor_401800_801559F8[5] = {
    NULL,
    &D_actor_401800_80151214,
    &D_actor_401800_80151C0C,
    &D_actor_401800_8015256C,
    NULL,
};

AnimationPlayRequest D_actor_401800_80155A0C = { { .sets = D_actor_401800_801559F0 }, 1, ANIMATION_BLEND_RESET, 3, ANIMATION_WORLD_COLLISION_DISABLE };

SVECTOR D_actor_401800_80155A20[12] = {
    { 60, -12, 30, 2 },
    { -50, -130, 29, 2 },
    { 20, -70, 25, 2 },
    { -30, -65, 25, 2 },
    { 60, -120, 30, 2 },
    { 20, -20, -5, 2 },
    { -15, -50, 0, 2 },
    { 2, 10, -15, 2 },
    { 14, 0, 0, 7 },
    { 25, 0, 0, 2 },
    { -14, 0, 0, 9 },
    { -25, 0, 0, 2 },
};

Actor401800MessageEntry D_actor_401800_80155A80[8] = {
    { 2015, { .call5 = func_actor_401800_8013DCB4 } },
    { 2003, { .call1 = oddStrangerPlayMessage } },
    { 2005, { .call3 = actorMsgSetVisibility } },
    { 2006, { .call0 = actorMsgIsPresent } },
    { 2004, { .call2 = actorMsgPlaceRecordYaw } },
    { 2014, { .call0 = actorMsgReleaseHold } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call4 = func_actor_401800_8013DF80 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

u16 D_actor_401800_80155AC0 = 0;

TaskDesc D_actor_401800_80155AC4 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_MODEL_BUFFER), 96 } }, func_actor_401800_8013E68C, { .model = &D_actor_401800_80143918 } };

static SVECTOR ActorContact_ScratchPosition;

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

Actor401800Storage5AD8 D_actor_401800_80155AD8;

GpDelayArg D_actor_401800_80155AF8;

static __inline__ void Actor401800_BindMatrices(Task* actor);
static void            func_actor_401800_80134C94(Task* arg0);
static void            func_actor_401800_80135DAC(Task* arg0);
static void            func_actor_401800_80135F58(Task* arg0);
static __inline__ s32  Actor401800_OutOfRange(SVECTOR* d, s16 r);
static __inline__ s32  Actor401800_ChaseOutOfRange(SVECTOR* d, s16 r);
static void            func_actor_401800_80136560(Task* arg0);
static void            func_actor_401800_80136EAC(Task* arg0);
static void            func_actor_401800_80137714(Task* arg0);
static void            func_actor_401800_80137DDC(Task* arg0);
static __inline__ void Actor401800_ViewWalk(GfxCoord* coord, SVECTOR* svp, SVECTOR* dir);
static __inline__ void Actor401800_SetGrabAnim(void);
static void            func_actor_401800_80138C28(Task* arg0);
static void            func_actor_401800_80138F5C(Task* arg0);
static void            func_actor_401800_80139118(Task* arg0);
static void            func_actor_401800_8013945C(Task* arg0);
static void            func_actor_401800_8013971C(Task* arg0);
static void            func_actor_401800_80139870(Task* arg0);
static void            func_actor_401800_801399C4(Task* arg0);
static void            func_actor_401800_80139B18(Task* arg0);
static void            func_actor_401800_80139D60(Task* arg0);
static void            func_actor_401800_8013A034(Task* arg0);
static void            func_actor_401800_8013A2E8(Task* arg0);
static void            func_actor_401800_8013AB64(Task* arg0);
static void            func_actor_401800_8013AF1C(Task* arg0);
static void            func_actor_401800_8013B444(Task* arg0);
static void            func_actor_401800_8013B784(Task* arg0);
static void            func_actor_401800_8013BB10(Task* arg0);
static void            func_actor_401800_8013BF48(Task* arg0);
static void            func_actor_401800_8013CD98(Task* arg0);
static void            func_actor_401800_8013D64C(Enemy* arg0, Task* arg1);

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/player_detection_reach.inc.c"

#include "../../shared/odd_stranger_tick_blended.inc.c"

#include "../../shared/player_detection_sight.inc.c"

#include "../../shared/odd_stranger_anim_event.inc.c"

#include "../../shared/odd_stranger_drive.inc.c"

/// Binds the work block's light and color matrices onto the model object.
/// Same body as `Actor01900_BindMatrices` / `Actor401300_BindMatrices`.
static __inline__ void Actor401800_BindMatrices(Task* actor)
{
    OddStrangerWork* work;
    TmdObject*       obj;

    work          = actor->work;
    obj           = actor->extra.tmd;
    obj->lightMtx = &work->field_B88;
    obj->colorMtx = &work->field_BA8;
}

/// Enemy init: allocates the work block, binds the model matrices, sets up both
/// animation contexts and the three hit/body `WorldCollisionBody` nodes, then picks the
/// starting state and tint row from the spawn flags and rescales the model.
/// Same body as `Actor01900_Fn02018` / `func_actor_401300_80134454`.
static void func_actor_401800_8013423C(Enemy* enemy, Task* actor)
{
    SVECTOR             dir;
    VECTOR              pos;
    SVECTOR*            v;
    TmdObject*          obj;
    GfxCoord*           root;
    OddStrangerWork*    work;
    WorldCollisionBody* body;
    WorldCollisionBody* head;
    s32                 kind;

    root        = actor->extra.tmd->coords;
    obj         = actor->extra.tmd;
    work        = memCalloc(0xC78, 0);
    actor->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, actor);
        return;
    }
    (Gp_IncStateF0Ref)(0);
    actor->exitCallback = func_actor_401800_8013E0A0;
    Actor401800_BindMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)D_actor_401800_8013E6F0.hpMax;
    enemy->param                  = &D_actor_401800_8013E6F0;
    enemy->recs                   = work->field_8F0;
    func_800B3F84(&((OddStrangerAnimWork*)work)->rig.anim, D_actor_401800_80155938, obj,
                  ((OddStrangerAnimWork*)work)->rig.poses, ((OddStrangerAnimWork*)work)->rig.slots);
    func_800B3F84(&((OddStrangerAnimWork*)work)->blend.anim, D_actor_401800_80155938, obj,
                  ((OddStrangerAnimWork*)work)->blend.poses, ((OddStrangerAnimWork*)work)->blend.slots);
    work->field_898 = 2;
    work->field_89E = 2;
    work->field_89A = 0;
    work->field_8B0 = 0;
    work->field_8AE = 0;
    work->field_8A4 = 0x10;
    work->field_8A2 = 0x10;
    if ((s16)((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) & 1) == 1) {
        work->field_8A4++;
    } else {
        work->field_8A4--;
    }
    oddStrangerDrive(actor);

    work->field_A10.context.contacts = work->field_A30;
    work->field_A10.coord            = root;
    work->field_A10.pos.vx           = 0;
    work->field_A10.pos.vy           = -0xAC;
    work->field_A10.pos.vz           = 0;
    work->field_A10.key              = 0x30012;
    work->field_A10.radius           = 0x12C;
    work->field_A10.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->field_A10);
    work->field_BE8        = 0;
    work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    Gp_InitRec18Table(work->field_A10.context.contacts, 0xC, 0);

    body                   = &work->field_8D0;
    body->coord            = &actor->extra.tmd->coords[2];
    body->context.contacts = work->field_8F0;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->key              = 0x30000;
    body->radius           = 0x12C;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(body->context.contacts, 0xC, 0);

    dir.vx                 = 0;
    dir.vy                 = 0;
    dir.vz                 = 0;
    head                   = &work->field_B50;
    head->coord            = &actor->extra.tmd->coords[6];
    head->context.contacts = &work->field_B70;
    v                      = &dir;
    head->pos.vx           = v->vx;
    head->pos.vy           = v->vy;
    head->pos.vz           = v->vz;
    head->radius           = 0x180;
    head->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, head);
    Gp_InitRec18Table(head->context.contacts, 1, 0);

    work->field_14     = 0;
    work->field_C[0].x = actor->extra.tmd->coords->coord.t[0];
    work->field_C[0].z = actor->extra.tmd->coords->coord.t[2];
    Gfx_MatrixCol2(&actor->extra.tmd->coords->coord, v);
    dir.vy = 0;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    work->field_C[1].x = actor->extra.tmd->coords->coord.t[0] + dir.vx;
    work->field_C[1].z = actor->extra.tmd->coords->coord.t[2] + dir.vz;

    actor->msgTable    = D_actor_401800_80155A80;
    root->parent       = &gGfxViewCoord;
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->field_8B8.coord      = &actor->extra.tmd->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
    kind                       = (actor->spawnArg1.value >> 16);
    switch (kind & 0xF) {
        case 2:
            work->field_2 = -1;
            work->field_0 = 0;
            break;
        case 4:
            work->field_2 = -1;
            work->field_0 = 0x16;
            break;
        default:
            work->field_2 = -1;
            work->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }
    switch (actor->spawnArg1.value & 0xF) {
        case 2:
            work->field_C10 = D_actor_401800_8013E700[0].field_0;
            work->field_C12 = D_actor_401800_8013E700[0].field_2;
            work->field_C14 = D_actor_401800_8013E700[0].field_4;
            work->field_C16 = D_actor_401800_8013E700[0].field_6;
            break;
        case 1:
            work->field_C10 = D_actor_401800_8013E700[2].field_0;
            work->field_C12 = D_actor_401800_8013E700[2].field_2;
            work->field_C14 = D_actor_401800_8013E700[2].field_4;
            work->field_C16 = D_actor_401800_8013E700[2].field_6;
            break;
        case 0:
        default:
            work->field_C10 = D_actor_401800_8013E700[1].field_0;
            work->field_C12 = D_actor_401800_8013E700[1].field_2;
            work->field_C14 = D_actor_401800_8013E700[1].field_4;
            work->field_C16 = D_actor_401800_8013E700[1].field_6;
            break;
    }

    actorRescaleYaw(actor->extra.tmd->coords, 0x1194);
    work->field_C7C = 0;
    actor->state++;
}

/// Picks one of twelve hit positions out of `D_actor_401800_80155A20` by damage
/// magnitude `arg1`, copies it to an 8-byte scratch vector, then arms the
/// `field_8B8` spawn record with the actor's part-1 coordinate as its anchor
/// and hands it to `func_800FDB18` to spawn effect `Gp_GetIdParam1(arg2)`.
/// Scale 0x300 and count 2 are the effect's; the record's coordinate comes from
/// `TmdObject.coords[sc->pad]`, so the effect follows the part the table entry
/// names. Same body as `Actor00100_Fn03340` (actors/lib/actor_400100_damage.c).
static void func_actor_401800_801348A8(Task* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         sc;
    s32              mag;
    OddStrangerWork* work;

    sc   = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = (OddStrangerWork*)arg0->work;
    if (mag < 0x200) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 3) {
            case 0:
                *sc = D_actor_401800_80155A20[0];
                break;
            case 1:
                *sc = D_actor_401800_80155A20[1];
                break;
            case 2:
                *sc = D_actor_401800_80155A20[2];
                break;
            case 3:
                *sc = D_actor_401800_80155A20[3];
                break;
            default:
                *sc = D_actor_401800_80155A20[4];
                break;
        }
    } else if (mag > 0x600) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 2) {
            case 0:
                *sc = D_actor_401800_80155A20[5];
                break;
            case 1:
                *sc = D_actor_401800_80155A20[6];
                break;
            default:
                *sc = D_actor_401800_80155A20[7];
                break;
        }
    } else if (arg1 > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *sc = D_actor_401800_80155A20[8];
        } else {
            *sc = D_actor_401800_80155A20[9];
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *sc = D_actor_401800_80155A20[10];
        } else {
            *sc = D_actor_401800_80155A20[11];
        }
    }
    work->field_8B8.coord      = &arg0->extra.tmd->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->extra.tmd->coords[sc->pad], sc, &work->field_8B8);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void func_actor_401800_80134C94(Task* arg0)
{
    PlayerStatus*    config = &gPlayerStatus;
    OddStrangerWork* work;
    Enemy*           enemy;
    ActorHitScratch* head;
    ActorHitScratch* s;
    GfxCoord*        coord;
    Task*            player;
    SVECTOR*         dir;
    s16              z;
    s32              yaw;
    s32              dx;
    s32              dy;
    s32              dz;
    s32              deathSound;
    s32              deathPan;
    s32              hitSound;
    s32              hitPan;
    s32              mag;
    s32              value;
    s16              state;
    s16              animState;
    s16              next;
    s16              effect;
    u32              damage;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    if (enemy->hp > 0) {
        head  = SCRATCH_STACK_CURSOR(ActorHitScratch);
        s     = (SCRATCH_STACK_CURSOR(ActorHitScratch) = head - 1);
        s->id = actorFindHit(&head[-1].hitPos, work->field_8F0);
        if (s->id == 0) {
            s->id = actorFindHit(&s->hitPos, work->field_A30);
        }
        if (s->id != 0) {
            if (s->id & 0x8000) {
                player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                s->hitPos.vx = player->extra.tmd->coords->workm.t[0];
                s->hitPos.vy = player->extra.tmd->coords->workm.t[1];
                s->hitPos.vz = player->extra.tmd->coords->workm.t[2];
            }
            if (work->field_C28 == 1) {
                Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 0, 0);
                work->field_C28 = 0;
                if ((u16)(work->field_0 - 0xB) < 4) {
                    work->field_0 = 0x13;
                }
            }
            work->field_C24                       = 0;
            work->field_C26                       = 0;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(arg0->extra.tmd->coords);
            s->dir.vx = arg0->extra.tmd->coords->workm.t[0];
            s->dir.vy = arg0->extra.tmd->coords->workm.t[1];
            s->dir.vz = arg0->extra.tmd->coords->workm.t[2];
            s->dir.vx = s->hitPos.vx - arg0->extra.tmd->coords->workm.t[0];
            s->dir.vy = s->hitPos.vy - arg0->extra.tmd->coords->workm.t[1];
            z         = s->hitPos.vz - arg0->extra.tmd->coords->workm.t[2];
            s->dir.vz = z;
            yaw       = ratan2(s->dir.vx, z);
            coord     = arg0->extra.tmd->coords;
            s->yaw    = yaw - ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
            s->yaw    = actorNormalizeYaw(s->yaw);
            func_actor_401800_801348A8(arg0, s->yaw, s->id);
            work->field_8B0 = 0;
            work->field_8AE = 0;
            s->effect       = -1;
            state           = work->field_0;
            if (state != 0x13 && state != 0x14 && state != 0x11 && state != 0x1F && state != 0x20 && state != 0xF && state != 0x10 &&
                state != 4) {
                s->m = arg0->extra.tmd->coords->coord;
                gfxRotMatrixY(&s->m, s->yaw, 0);
                dir = &s->dir;
                Gfx_MatrixCol2(&s->m, dir);
                VectorNormalSS(dir, dir);
                if ((s16)work->field_BEC > 0) {
                    gte_lddp(-0x19);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                } else {
                    gte_lddp(-0x64);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                }
                arg0->extra.tmd->coords->coord.t[0]  += s->dir.vx;
                arg0->extra.tmd->coords->coord.t[1]  += s->dir.vy;
                arg0->extra.tmd->coords->coord.t[2]  += s->dir.vz;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            dx        = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            s->dx     = dx;
            dy        = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            s->dy     = dy;
            dz        = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
            s->dz     = dz;
            s->dist   = SquareRoot0(dx * dx + dy * dy + dz * dz);
            s->damage = Gp_ComputeDamage(s->id, s->dist, 0, 0);
            if (Gp_RollEnemyChance(enemy, s->id, 0) != 0) {
                s->crit    = 1;
                s->effect  = 0;
                s->damage *= 4;
            } else {
                s->crit = 0;
            }
            mag = s->yaw;
            if (mag < 0) {
                mag = -mag;
            }
            if (mag > 0x500) {
                state = work->field_0;
                if (state != 0x13) {
                    if (state != 0x14 && state != 0x11 && state != 0x1F && state != 0x20 && state != 0xF && state != 0x10 && state != 4) {
                        damage    = s->damage * 2;
                        s->damage = damage;
                        if (damage != 0) {
                            s->effect = 4;
                        }
                    }
                }
            }
            func_800E2C78(enemy, s->id, s->damage, 0);
            enemy->hp -= s->damage;
            func_800DA6E8(&enemy->node, s->damage, 0);
            work->field_BEA += s->damage;
            effect           = s->effect;
            if (effect != -1) {
                Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[2], (s32)(effect), NULL);
            }
            if (work->field_0 == 0x17) {
                SndEvt_EnqueueType7(0x51030008, 1);
            }
            if ((u16)(work->field_0 - 0xC) < 3 && config->hp > 0 && work->field_C28 == 1) {
                Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 0, 0);
            }
            if (enemy->hp <= 0) {
                deathSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400A0008;
                deathPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(deathSound, deathPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            } else {
                hitSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400A0007;
                hitPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(hitSound, hitPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            work->field_BE8 = Gp_GetIdParam2(s->id);
            switch (Gp_GetIdParam0(s->id) & 0xFFFF) {
                case 4:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 0x11) {
                        if (state == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
                case 0:
                case 5:
                case 6:
                case 7:
                    if (work->field_0 == 0x18 || work->field_0 == 0x16 || work->field_0 == 0x17) {
                        work->field_0 = 6;
                    }
                    state = work->field_0;
                    if (state == 0x13 || state == 0x14 || state == 0xF || state == 0x10 || state == 4 || state == 0x11) {
                        if (work->field_89E == 0xB || work->field_89E == 0x17 || work->field_89E == 8 || work->field_89E == 0xA) {
                            work->field_89A = 1;
                            work->field_8A8 = 0xB;
                        } else {
                            work->field_89A = 1;
                            work->field_8A8 = 0x19;
                        }
                        work->field_8A6 = 2;
                    } else if ((s16)work->field_BEA >= 0x38 || s->crit == 1) {
                        if (state == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    } else {
                        work->field_8A8 = 0xD;
                        work->field_89A = 1;
                        work->field_8A6 = 2;
                    }
                    break;
                case 2:
                    Gp_SetObjFlag2(enemy, s->id, 0);
                    state = work->field_0;
                    if (state == 0x11 || state == 4) {
                        work->field_0 = 4;
                    } else if (state == 0x10 && work->field_6 < 0x21) {
                        work->field_0 = 0x20;
                    } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                        work->field_0 = 0x1F;
                    } else {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                    }
                    break;
                case 3:
                    if (work->field_0 == 0x18 || work->field_0 == 0x16 || work->field_0 == 0x17) {
                        work->field_0 = 6;
                    }
                    Gp_SetObjFlag4(enemy, s->id, 0);
                    break;
                case 1:
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    state                 = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 0x11 && state != 4) {
                        if (state == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
                case 8:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 0x11 && state != 4) {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag <= 0x500) {
                            if (state == 0x10 && work->field_6 < 0x21) {
                                work->field_0 = 0x20;
                            } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                                work->field_0 = 0x1F;
                            } else {
                                mag = s->yaw;
                                if (mag < 0) {
                                    mag = -mag;
                                }
                                work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                            }
                        }
                    }
                    break;
                case 9:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 4 && state != 0x11) {
                        if (state == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
            }
            next = 5;
        } else if ((s16)work->field_BEC <= 0) {
            work->field_BEA = 0;
            goto be4_done;
        } else {
            next = work->field_BEC - 1;
        }
        work->field_BEC = next;
    be4_done:
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            s->damage = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            if (s->damage != 0) {
                enemy->hp -= s->damage;
                func_800DA6E8(&enemy->node, s->damage, 0);
                state = work->field_0;
                value = (u16)work->field_0;
                if (state == 7 || state == 0x1E || state == 0xB || state == 0x1B) {
                    work->field_0 = 5;
                } else if ((u16)(value - 0x13) < 2 || state == 0xF || state == 0x10 || state == 4 || state == 0x11) {
                    if (work->field_89E == 0xB || work->field_89E == 0x17 || work->field_89E == 8 || work->field_89E == 0xA) {
                        work->field_89A = 1;
                        work->field_8A8 = 0xB;
                    } else {
                        work->field_89A = 1;
                        work->field_8A8 = 0x19;
                    }
                    work->field_8A6 = 2;
                } else {
                    work->field_89A = 1;
                    work->field_8A8 = 0xD;
                    work->field_8A6 = 2;
                }
            }
        }
        if (enemy->hp <= 0) {
            value = s->id;
            if (value != 0) {
                if ((Gp_GetIdParam0(value) & 0xFFFF) == 4 || (Gp_GetIdParam0(s->id) & 0xFFFF) == 6) {
                    animState = work->field_89E;
                    if (animState == 2 || animState == 3) {
                        work->field_0 = 0x21;
                    } else {
                        work->field_0 = 0x1D;
                    }
                } else {
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x11) {
                        if (state == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                }
            } else {
                if (work->field_C28 == 1) {
                    Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 0, 0);
                    work->field_C28 = 0;
                }
                state = work->field_0;
                if ((u16)(state - 0x13) >= 3 && state != 0x1D && state != 0x21 && state != 0 && state != 0x1F && state != 0x20 && state != 0x11) {
                    if (state == 0x10 && work->field_6 < 0x21) {
                        work->field_0 = 0x20;
                    } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                        work->field_0 = 0x1F;
                    } else {
                        work->field_0 = 0x14;
                    }
                }
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorHitScratch);
    }
}

/// Walk-state body, split on the live flag. Live: hand the model back to
/// `Tmd_AllocBuffers`, restart the 0x898 slot, ramp `field_8A2` to 0x10, remap
/// the state at 0x89E (11 -> 0x17, 12/25 -> 0x18, anything else -> 0x17) and
/// hold the two `field_5A` countdowns open until the step helper has run its
/// course, then drop `field_8A2` to 0x20. Dead: clear the model's coordinate
/// flag, halve `field_8A2` with the 1 / -1 wrap, and once `Gp_TickObjFlag2`
/// reports 1 clear the enemy's node bit 1 and move to state 0x11.
/// Same body as `func_actor_401300_80135DDC`.
static void func_actor_401800_80135DAC(Task* arg0)
{
    OddStrangerWork* work  = arg0->work;
    Enemy*           enemy = arg0->spawnArg2.pointer;
    TmdObject*       tmd;

    if (work->field_4 != 0) {
        tmd                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        tmd->flags                    = 0;
        Tmd_AllocBuffers(tmd);
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        if (work->field_89E == 11) {
            work->field_89E = 0x17;
        } else if (work->field_89E == 12 || work->field_89E == 25) {
            work->field_89E = 0x18;
        }
        if ((u16)(work->field_89E - 0x17) >= 2) {
            work->field_89E = 0x17;
        }
        do {
            oddStrangerDrive(arg0);
        } while (!(work->field_89E == 0x17 && (work->field_5A & 0x3FF) >= 6) &&
                 !(work->field_89E == 0x18 && (work->field_5A & 0x3FF) >= 9));
        work->field_8A2 = 0x20;
        return;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_8A2                       = work->field_8A2 / 2;
    if (work->field_8A2 == 1) {
        work->field_8A2 = -0x10;
    }
    if (work->field_8A2 == -1) {
        work->field_8A2 = 0x10;
    }
    oddStrangerDrive(arg0);
    if (Gp_TickObjFlag2(enemy) == 1) {
        enemy->reactionFlags &= ~ENEMY_REACTION_BUILDUP;
        work->field_0         = 0x11;
    }
}

/// Aim the actor at the player and rescale its root coordinate. On the live
/// flag it resets the model buffers and hands back the pose the actor was
/// running; otherwise it takes a 0x10 scratch for the player offset and the
/// clamped turn, folds the turn into the coordinate's Y rotation and rebuilds
/// the matrix from the new yaw at scale 0x1194.
/// Same body as `Actor01900_Fn080A8`, with the aim and rescale helpers inlined.
static void func_actor_401800_80135F58(Task* arg0)
{
    OddStrangerWork*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 9;
        work->field_89A        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        oddStrangerDrive(arg0);
        work->field_8D0.radius = 0x12C;
        Gp_ArmStateF0(1);
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
    aim->turn       = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->field_8AE = aim->turn;
    if (aim->turn > 0x10) {
        aim->turn = 0x10;
    }
    if (aim->turn < -0x10) {
        aim->turn = -0x10;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    oddStrangerDrive(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Push the root coordinate out of a `WorldCollisionContact` table: take a 0x34 scratch, seed
/// its position from the second coordinate, then walk the records until `count`
/// or a zero `key`. Each kind 0x10000 / 0x30000 record contributes half its
/// offset along X and Z, normalised to length 0x96 first when it is longer than
/// that; `hit` reports whether one was seen.
/// Same body as `Actor01900_Fn03FF8` / `func_actor_401300_80132910`, with the
/// coordinate update written out in both arms of the length test.
static s32 func_actor_401800_8013629C(Task* arg0, WorldCollisionContact* recs, s16 count)
{
    ActorPushScratch* head;
    ActorPushScratch* s;
    ActorPushScratch* blk;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    head                                    = SCRATCH_STACK_CURSOR(ActorPushScratch);
    blk                                     = head - 1;
    SCRATCH_STACK_CURSOR(ActorPushScratch)  = blk;
    s                                       = blk;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
    s->pos.vx = arg0->extra.tmd->coords[1].workm.t[0];
    s->pos.vy = arg0->extra.tmd->coords[1].workm.t[1];
    s->pos.vz = arg0->extra.tmd->coords[1].workm.t[2];
    s->hit    = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key.value == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key.value & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            worldCollisionCalcContactViewOffset(&s->pos, &recs[s->i], &s->offset);
            s->len = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->len = SquareRoot0(s->len);
            if (s->len >= 0x96) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0x96);
                gte_ldsv(&s->offset);
                gte_gpf12();
                gte_stsv(&s->offset);
                arg0->extra.tmd->coords->coord.t[0] += s->offset.vx / 2;
                arg0->extra.tmd->coords->coord.t[2] += s->offset.vz / 2;
            } else {
                arg0->extra.tmd->coords->coord.t[0] += s->offset.vx / 2;
                arg0->extra.tmd->coords->coord.t[2] += s->offset.vz / 2;
            }
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorPushScratch);
    return s->hit;
}

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor401800_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                  head;
    OverlayRangeScratch* blk;
    s32                  ret;

    head                                      = SCRATCH_STACK_CURSOR(u8);
    blk                                       = (OverlayRangeScratch*)(head - 0xC);
    ((OverlayRangeScratch*)(head - 0xC))->dx  = d->vx;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = blk;
    blk->dz                                   = d->vz;
    blk->r                                    = r;
    ((OverlayRangeScratch*)(head - 0xC))->dx *= ((OverlayRangeScratch*)(head - 0xC))->dx;
    blk->dz                                  *= blk->dz;
    blk->r                                   *= blk->r;
    SCRATCH_STACK_CURSOR(u8)                  = head;
    ret                                       = ((OverlayRangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

static __inline__ s32 Actor401800_ChaseOutOfRange(SVECTOR* d, s16 r)
{
    u8*                  head;
    OverlayRangeScratch* blk;
    s32                  ret;

    head                                      = SCRATCH_STACK_CURSOR(u8);
    ((OverlayRangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                       = (OverlayRangeScratch*)(head - 0xC);
    blk->dz                                   = d->vz;
    blk->r                                    = r;
    ((OverlayRangeScratch*)(head - 0xC))->dx *= ((OverlayRangeScratch*)(head - 0xC))->dx;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = blk;
    blk->dz                                  *= blk->dz;
    blk->r                                   *= blk->r;
    SCRATCH_STACK_CURSOR(u8)                  = head;
    ret                                       = ((OverlayRangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

static void func_actor_401800_80136560(Task* arg0)
{
    OddStrangerWork*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          turnCoord;
    GfxCoord*          facing;
    void**             scratch;
    u8*                head;
    u8*                block;
    ActorChaseScratch* s;
    s32                kind;

    kind = (arg0->spawnArg1.value >> 16);
    work = arg0->work;
    if ((kind & 0xF0) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x12C;
        work->field_898        = 1;
        work->field_89A        = 0;
        work->field_89E        = 3;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_8A2        = work->field_8A4;
        oddStrangerDrive(arg0);
        work->field_C24 = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        return;
    }
    work->field_6++;
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = head - 0x10;
    SCRATCH_HEAD_AT(scratch, void) = block;
    s                              = (ActorChaseScratch*)block;
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) != 1) {
        func_actor_401800_8013629C(arg0, work->field_8F0, 0xC);
    }
    coord                                         = arg0->extra.tmd->coords;
    ((ActorChaseScratch*)(head - 0x10))->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    s->delta.vy                                   = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    s->delta.vz                                   = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    arg0->extra.tmd->coords->composeStamp         = GRAPHICS_COORD_DIRTY;
    oddStrangerDrive(arg0);
    s->playerYaw                                  = ratan2(-gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][0],
                                                           gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][2]);
    coord                                         = arg0->extra.tmd->coords;
    ((ActorChaseScratch*)(head - 0x10))->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    s->delta.vy                                   = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    s->delta.vz                                   = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    s->yaw                                        = ratan2(s->delta.vx, s->delta.vz) + 0x800;
    s->yaw                                        = actorNormalizeYaw(s->yaw);
    turnCoord                                     = arg0->extra.tmd->coords;
    s->turn                                       = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-turnCoord->coord.m[2][0], turnCoord->coord.m[2][2]));
    work->field_8AE                               = s->turn;
    if (abs(s->yaw - s->playerYaw) < 0x44) {
        if (((s16)work->field_C14 + work->field_C26 / 2) < work->field_6) {
            if (abs(s->turn) < 0x80) {
                if (Actor401800_ChaseOutOfRange(&s->delta, 0x708) && detectSightBlocked(arg0) != 1) {
                    work->field_0 = 0xA;
                }
            }
        }
    }
    if (s->turn < 0x200) {
        if (!Actor401800_ChaseOutOfRange(&s->delta, 0x44C) && detectSightBlocked(arg0) != 1 && work->field_8CA == 0) {
            work->field_0 = 0xB;
        }
    }
    if (s->turn > 0x30) {
        s->turn = 0x30;
    }
    if (s->turn < -0x30) {
        s->turn = -0x30;
    }
    facing   = arg0->extra.tmd->coords;
    s->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, s->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89E == 3) {
        if (work->field_89A == 0) {
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, ((work->field_8A4 + 2) * 0x42) / 18) != 0) {
                actorMoveForwardNonzero(arg0->extra.tmd->coords, ((work->field_8A4 + 2) * 0x42) / 18);
            }
        } else if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, (((work->field_8A4 + 2) * 0x42) / 18) >> 2) != 0) {
            actorMoveForwardNonzero(arg0->extra.tmd->coords, (((work->field_8A4 + 2) * 0x42) / 18) >> 2);
        }
    } else if (work->flags_68.half & 1) {
        work->field_89E = 3;
        work->field_898 = 1;
    }
    if (work->field_8CA != 0) {
        work->field_8CA--;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Chase body that steers the actor along its own local Z while the step
/// countdown runs: takes a 0x10 scratch for the player offset and the yaws, and
/// on the live flag resets the model buffers, arms the walk state and seeds
/// `field_C06` to 8. Otherwise it re-aims the actor at the player once
/// `field_8` has run 7 frames, clamps the turn toward the player into
/// `s->angle`, rebuilds the root coordinate at scale 0x1194 and steps the actor
/// by `field_C04` (the step countdown, halved while `field_89A` is set, forced
/// to 2 while `field_8` is live) while `detectPlayerOutOfReach` reports the
/// path clear. `field_C06` then walks 8 -> -1 -> 0 against `field_8A2`, and at
/// 0 the fifth `field_6` frame picks `field_0` from the yaw offset to the
/// player. Same step ramp as `Actor01900_Fn04D14`, with the aim and the step
/// helper inlined.
static void func_actor_401800_80136EAC(Task* arg0)
{
    OddStrangerWork*   work;
    ActorChaseScratch* head;
    ActorChaseScratch* s;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    s32                turn;
    s32                diffPos;
    s32                diffNeg;
    s32                yaw;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x96;
        work->field_898        = 1;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        oddStrangerDrive(arg0);
        work->field_C06         = 8;
        work->field_6           = 0;
        work->field_8           = 0;
        D_actor_401800_80155AC0 = 0;
        work->field_C24++;
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    s                                       = head - 1;
    arg0->extra.tmd->coords->composeStamp   = GRAPHICS_COORD_DIRTY;
    oddStrangerDrive(arg0);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) != 0) {
        work->field_8++;
    } else {
        func_actor_401800_8013629C(arg0, work->field_8F0, 0xC);
    }
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
    if (work->field_8 >= 7) {
        s->playerYaw  = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                               (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
        s->yaw        = ratan2(s->delta.vx, s->delta.vz) + 0x800;
        s->yaw        = actorNormalizeYaw(s->yaw);
        work->field_0 = 0x1A;
    }
    coord   = arg0->extra.tmd->coords;
    s->turn = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    turn    = s->turn;
    if (turn >= 0) {
        diffPos = turn - 1000;
        if (((diffPos < 0) ? -diffPos : diffPos) < 0x60) {
            s->angle = s->turn - 1000;
        } else if (diffPos > 0) {
            s->angle = 0x60;
        } else {
            s->angle = -0x60;
        }
    } else {
        diffNeg = turn + 1000;
        if (((diffNeg < 0) ? -diffNeg : diffNeg) < 0x60) {
            s->angle = s->turn + 1000;
        } else if (diffNeg > 0) {
            s->angle = 0x60;
        } else {
            s->angle = -0x60;
        }
    }
    facing    = arg0->extra.tmd->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, s->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    coord                                 = arg0->extra.tmd->coords;
    work->field_8AE                       = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_C04                       = work->field_8A2 * 8;
    if (work->field_89A != 0) {
        work->field_C04 = work->field_C04 >> 1;
    }
    if (work->field_8 != 0) {
        work->field_C04 = 2;
    }
    if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, work->field_C04) != 0) {
        actorMoveForwardNonzero(arg0->extra.tmd->coords, work->field_C04);
    }
    D_actor_401800_80155AC0 += work->field_C04;
    if (work->field_C06 == 8 && work->field_8A2 >= 0x18) {
        work->field_C06 = -1;
    }
    if (work->field_C06 == -1 && work->field_8A2 == 0x12) {
        work->field_C06 = 0;
        work->field_6   = 0;
    }
    if (work->field_C06 == 0) {
        if (++work->field_6 == 5) {
            s->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                                  (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
            actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
            s->yaw = ratan2(s->delta.vx, s->delta.vz) + 0x800;
            yaw    = actorNormalizeYaw(s->yaw);
            s->yaw = yaw;
            yaw    = yaw - s->playerYaw;
            if (yaw < 0) {
                yaw = -yaw;
            }
            if (yaw >= 0x401 && detectSightBlocked(arg0) != 1 && work->field_8CA == 0) {
                work->field_0 = 0xB;
            } else {
                work->field_0 = 0x1A;
                work->field_2 = -1;
            }
        }
    }
    work->field_8A2 += work->field_C06;
    if (work->field_8CA != 0) {
        work->field_8CA--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Chase body: takes a 0x10 scratch for the player offset and the heading it
/// folds into the root coordinate. On the live flag it resets the model
/// buffers, arms the walk state and stores the facing yaw `field_C00` along
/// with the target `field_C02` — the facing plus twice the wrapped turn toward
/// the player. Otherwise it picks `field_0` from the `field_C24` contact range
/// and the `field_8CA` countdown, steers `field_C00` 0x89 a frame toward
/// `field_C02`, rebuilds the root coordinate at scale 0x1194 and steps the
/// actor 0x28 / 0x14 along its local Z while `detectPlayerOutOfReach`
/// reports the path clear. Same body as `func_actor_401300_801376E4` /
/// `Actor01900_Fn0551C`, with the step helper's clear-path test added.
static void func_actor_401800_80137714(Task* arg0)
{
    OddStrangerWork*   work;
    ActorChaseScratch* head;
    ActorChaseScratch* s;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;

    work = arg0->work;
    if (work->field_4 != 0) {
        head                                                      = SCRATCH_STACK_CURSOR(ActorChaseScratch);
        obj                                                       = arg0->extra.tmd;
        SCRATCH_STACK_CURSOR(ActorChaseScratch)                   = head - 1;
        s                                                         = head - 1;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_8AE        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        oddStrangerDrive(arg0);
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
        coord           = arg0->extra.tmd->coords;
        s->turn         = actorNormalizeYaw(ratan2(head[-1].delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        facing          = arg0->extra.tmd->coords;
        s->angle        = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->field_C00 = s->angle;
        work->field_C02 = s->angle + (u16)s->turn * 2;
        SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    s                                       = head - 1;
    oddStrangerDrive(arg0);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
    if (work->field_C00 == work->field_C02) {
        if (work->field_C24 < 2 || Actor401800_OutOfRange(&s->delta, 0x384)) {
            work->field_0 = 8;
        } else if (detectSightBlocked(arg0) != 1 && work->field_8CA == 0) {
            work->field_0 = 0xB;
        } else {
            work->field_0 = 8;
        }
    }
    if (work->field_C00 > work->field_C02) {
        work->field_C00 -= 0x89;
        if (work->field_C00 < work->field_C02) {
            work->field_C00 = work->field_C02;
        }
    }
    if (work->field_C00 < work->field_C02) {
        work->field_C00 += 0x89;
        if (work->field_C00 > work->field_C02) {
            work->field_C00 = work->field_C02;
        }
    }
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, work->field_C00, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89A == 0) {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, 0x28) != 0) {
            actorStepForward(arg0->extra.tmd->coords, 0x28);
        }
    } else {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, 0x14) != 0) {
            actorStepForward(arg0->extra.tmd->coords, 0x14);
        }
    }
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) != 1) {
        func_actor_401800_8013629C(arg0, work->field_8F0, 0xC);
    }
    if (work->field_8CA != 0) {
        work->field_8CA--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Live-actor swing. On the live flag it resets the model buffers, takes the
/// swing side `field_C08` from `gRandomLcgState`, offsets the player bearing in
/// `field_BF0` by +-0x171 on the first frame and stores the 0x15 / 0x14 state
/// the walk body runs. The `field_BF0` yaw is then rebuilt into a direction and
/// GPF-scaled by `field_C0A` into the offset added to the root coordinate while
/// `field_6` sits in 0xC..0x15 — halving the scale once the `field_A30` contact
/// test fires. The step counter moves the actor to state 7 at 0x1E; a kind
/// 0x10 actor reloads 0x1E instead. Same body as `Actor01900_Fn05B4C`, with the
/// position delta inlined.
static void func_actor_401800_80137DDC(Task* arg0)
{
    OddStrangerWork*   work;
    ActorChaseScratch* head;
    ActorChaseScratch* aim;
    TmdObject*         obj;
    GfxCoord*          coord;
    SVECTOR*           dir;
    MATRIX             mat;
    u16                angle;
    s32                kind;

    kind = (arg0->spawnArg1.value >> 16);
    work = arg0->work;
    if ((kind & 0xF0) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    aim                                     = head - 1;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x96;
        work->field_6          = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
        aim->turn = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->field_C08 == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->field_C08 = 1;
            } else {
                work->field_C08 = -1;
            }
        }
        if (work->field_C08 == 1) {
            work->field_89E = 0x15;
            if (work->field_C26 == 0) {
                angle     = aim->turn + 0x171;
                aim->turn = work->field_C12 + angle;
            } else {
                aim->turn += work->field_C12;
            }
            work->field_C08 = -1;
        } else {
            work->field_89E = 0x14;
            if (work->field_C26 == 0) {
                angle     = aim->turn - 0x171;
                aim->turn = angle - work->field_C12;
            } else {
                aim->turn -= work->field_C12;
            }
            work->field_C08 = 1;
        }
        work->field_898 = 1;
        work->field_8A2 = 0xC;
        work->field_89A = 0;
        oddStrangerDrive(arg0);
        gfxRotMatrixY(&mat, aim->turn, 1);
        dir = &work->field_BF0;
        Gfx_MatrixCol2(&mat, dir);
        VectorNormalSS(dir, dir);
        work->field_C0A = 0xDE;
        work->field_C26++;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    oddStrangerDrive(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89A == 0) {
        gte_lddp(work->field_C0A);
        gte_ldsv(&work->field_BF0);
        gte_gpf12();
        gte_stsv(aim);
    } else {
        gte_lddp(work->field_C0A >> 1);
        gte_ldsv(&work->field_BF0);
        gte_gpf12();
        gte_stsv(aim);
    }
    if ((u32)((u16)work->field_6 - 0xC) < 0xAU) {
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[2] += aim->delta.vz;
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) == 1) {
            work->field_C0A >>= 1;
        }
    }
    if (++work->field_6 >= 0x1E) {
        work->field_0 = 7;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static __inline__ void Actor401800_ViewWalk(GfxCoord* coord, SVECTOR* svp, SVECTOR* dir)
{
    VECTOR    vec;
    SVECTOR*  outp;
    u8*       head;
    VECTOR*   vecp;
    GfxCoord* p;
    GfxCoord* view;
    s32       flag;
    s32*      flagp;
    Task*     player;

    player                   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 8;
    outp                     = (SVECTOR*)(head - 8);
    view                     = &gGfxViewCoord;
    vecp                     = &vec;
    flagp                    = &flag;
    outp->vx                 = 0;
    outp->vy                 = 0;
    outp->vz                 = 0;
    p                        = &player->extra.tmd->coords[1];
    svp->vx                  = outp->vx;
    svp->vy                  = outp->vy;
    svp->vz                  = outp->vz;
    for (;;) {
        if (p->parent != NULL) {
            if (p != view) {
                gte_SetTransMatrix(&p->coord);
                gte_SetRotMatrix(&p->coord);
                gte_ldv0(svp);
                gte_rtv0tr();
                gte_stlvnl(vecp);
                gte_stflg(flagp);
                svp->vx = vec.vx;
                svp->vy = vec.vy;
                svp->vz = vec.vz;
                p       = p->parent;
                continue;
            }
            outp->vx = svp->vx;
            outp->vy = svp->vy;
            outp->vz = svp->vz;
        }
        break;
    }
    dir->vx = outp->vx - coord->coord.t[0];
    dir->vy = 0;
    dir->vz = outp->vz - coord->coord.t[2];
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static __inline__ void Actor401800_SetGrabAnim(void)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {
        D_actor_401800_80155A0C.source.sets = D_actor_401800_801559F8;
    } else {
        D_actor_401800_80155A0C.source.sets = D_actor_401800_801559F0;
    }
}

static void func_actor_401800_801381E4(Task* arg0)
{
    Enemy*           enemy;
    Task*            player;
    GameActor*       gactor;
    PlayerStatus*    config;
    OddStrangerWork* work;
    SVECTOR          dir;
    SVECTOR          sv;
    s32              ang;
    u16              step;

    enemy  = arg0->spawnArg2.pointer;
    work   = arg0->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    gactor = (GameActor*)player->work;
    config = &gPlayerStatus;
    if (work->field_4 != 0) {
        work->field_8CA               = 0xA;
        work->field_8D0.radius        = 0x12C;
        work->field_C26               = 0;
        work->field_C28               = 0;
        work->field_B50.flags         = (u16)(work->field_B50.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_A10.flags         = (u16)(work->field_A10.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->field_898               = 1;
        work->field_8A2               = 0x10;
        work->field_89E               = 4;
        oddStrangerDrive(arg0);
        ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC);
        work->field_BF8.vx = arg0->extra.tmd->coords->coord.t[0];
        work->field_BF8.vy = arg0->extra.tmd->coords->coord.t[1];
        work->field_BF8.vz = arg0->extra.tmd->coords->coord.t[2];
        work->field_6      = 0;
        return;
    }
    step          = (u16)work->field_6 + 1;
    work->field_6 = step;
    if ((s16)step == 1) {
        ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC);
        work->field_BF8.vx = arg0->extra.tmd->coords->coord.t[0];
        work->field_BF8.vy = arg0->extra.tmd->coords->coord.t[1];
        work->field_BF8.vz = arg0->extra.tmd->coords->coord.t[2];
        Actor401800_ViewWalk(arg0->extra.tmd->coords, &sv, &dir);
        ang = actorViewYaw(arg0->extra.tmd->coords, &dir);
        gfxRotMatrixY(&arg0->extra.tmd->coords[0].coord, ang, 0);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
        dir.vx                                = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        dir.vy                                = 0;
        dir.vz                                = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        work->field_8AE                       = 0;
        work->field_8B0                       = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->field_C26                       = 0;
        work->field_C28                       = 0;
        work->field_8CA                       = 0xA;
    }
    oddStrangerDrive(arg0);
    if ((work->field_5A & 0x3FF) == 0x10 && gactor->mode != GAME_ACTOR_MODE_SCRIPTED) {
        Actor401800_ViewWalk(arg0->extra.tmd->coords, &sv, &dir);
        ang = actorViewYaw(arg0->extra.tmd->coords, &dir);
        if (ang < 0) {
            ang = -ang;
        }
        if (ang < 0x20) {
            if (!Actor401800_OutOfRange(&dir, 0x5DC)) {
                Actor401800_SetGrabAnim();
                D_actor_401800_80155AF8.field_14 = 8;
                do {
                    if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F8, &D_actor_401800_80155AF8, 0) == 0) {
                        work->field_0                       = 0xC;
                        work->field_C28                     = 1;
                        D_actor_401800_80155A0C.animationId = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &D_actor_401800_80155A0C, 0);
                    }
                } while (0);
            }
        }
    }
    if (work->field_89E == 4 && (work->flags_68.half & 1)) {
        work->field_0 = 7;
    }
    if ((u32)(work->field_5A & 0x3FF) >= 0x11U) {
        dir.vx = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        dir.vy = 0;
        dir.vz = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        if (!Actor401800_OutOfRange(&dir, 0x578)) {
            VectorNormalSS(&dir, &dir);
            gte_lddp(0xA);
            gte_ldsv(&dir);
            gte_gpf12();
            gte_stsv(&dir);
            arg0->extra.tmd->coords->coord.t[0]  += dir.vx;
            arg0->extra.tmd->coords->coord.t[2]  += dir.vz;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) != 1) {
            func_actor_401800_8013629C(arg0, work->field_8F0, 0xC);
        }
    }
}

/// Live-actor body: arms the animation slots and the two `field_8D0` /
/// `field_A10` nodes, then aims the actor at the `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` task's
/// root position — the XZ offset normalized by `VectorNormalSS` and GPF-scaled
/// by 0x3E8, the heading taken through `ratan2` — sends it as message 0x3E9
/// and spawns the 0xC/8/0x8F pad-lerp. On work flag bit 0 while `field_89E` is
/// 5, restarts the actor's model (`field_0 = 0xD`, the 0x8B8 effect record for
/// the second coordinate). Same shape as `func_actor_401300_80138800`.
static void func_actor_401800_80138C28(Task* arg0)
{
    SVECTOR          dir;
    OddStrangerWork* work  = arg0->work;
    Enemy*           enemy = arg0->spawnArg2.pointer;
    Task*            player;
    SVECTOR*         pdir;

    if (work->field_4 != 0) {
        player                                  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        work->field_8D0.radius                  = 0x12C;
        work->field_B50.flags                  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags                  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags           = 0;
        work->field_898                         = 1;
        work->field_8A2                         = 0x10;
        work->field_89E                         = 5;
        player->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(player->extra.tmd->coords);
        D_actor_401800_80155AD8.value.pos.vx = player->extra.tmd->coords->coord.t[0];
        D_actor_401800_80155AD8.value.pos.vy = player->extra.tmd->coords->coord.t[1];
        D_actor_401800_80155AD8.value.pos.vz = player->extra.tmd->coords->coord.t[2];
        pdir                                 = &dir;
        dir.vx                               = (u16)arg0->extra.tmd->coords->coord.t[0] - (u16)player->extra.tmd->coords->coord.t[0];
        dir.vy                               = 0;
        dir.vz                               = (u16)arg0->extra.tmd->coords->coord.t[2] - (u16)player->extra.tmd->coords->coord.t[2];
        VectorNormalSS(pdir, pdir);
        gte_lddp(0x3E8);
        gte_ldsv(pdir);
        gte_gpf12();
        gte_stsv(pdir);
        arg0->extra.tmd->coords->coord.t[0]   = player->extra.tmd->coords->coord.t[0] + dir.vx;
        arg0->extra.tmd->coords->coord.t[2]   = player->extra.tmd->coords->coord.t[2] + dir.vz;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        D_actor_401800_80155AD8.value.rot.vx  = 0;
        D_actor_401800_80155AD8.value.rot.vy  = ratan2(dir.vx, dir.vz);
        D_actor_401800_80155AD8.value.rot.vz  = 0;
        TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &D_actor_401800_80155AD8.value, 0);
        Gp_SpawnPadLerp(0xC, 8, 0x8F);
    }
    oddStrangerDrive(arg0);
    Gfx_RotMatrixX(&arg0->extra.tmd->coords[2].coord, -0x80, 0);
    arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[2]);
    Gfx_RotMatrixX(&arg0->extra.tmd->coords[3].coord, -0x80, 0);
    arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[3]);
    if (work->field_89E == 5 && (work->flags_68.half & 1)) {
        work->field_0              = 0xD;
        work->field_8B8.coord      = &arg0->extra.tmd->coords[1];
        work->field_8B8.spawnArgLo = 0x200;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->extra.tmd->coords[5], 0, &work->field_8B8);
    }
}

/// On the live-actor flag, raises the three animation slots, sends the `0x3FF`
/// animation record and the `0x3F9` object pair to the `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)`
/// task, then spawns the 5/0xFF/8 pad-lerp. On work flag bit 0, restarts the
/// actor's model (`field_0 = 0xE`, the 0x8B8 effect record for the second
/// coordinate) and finally copies the `field_5A` clip id into `field_894` and
/// rebuilds the four coordinate parts the actor draws from.
static void func_actor_401800_80138F5C(Task* arg0)
{
    OddStrangerWork*      work;
    Enemy*                enemy;
    AnimationPlayRequest* msg;
    Task*                 playerTask;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_8A2  = 0x10;
        work->field_89E  = 6;
        work->field_898  = 2;
        msg              = &D_actor_401800_80155A0C;
        msg->animationId = 2;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, msg, 0);
        playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        Gp_DispatchMsg(playerTask, 0x3F9, Gp_PackObjPair(enemy, 0), 0);
        Gp_SpawnPadLerp(5, 0xFF, 8);
    }
    if (work->flags_68.half & 1) {
        work->field_0              = 0xE;
        work->field_8B8.coord      = &arg0->extra.tmd->coords[1];
        work->field_8B8.spawnArgLo = 0x200;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->extra.tmd->coords[5], 0, &work->field_8B8);
    }
    work->field_894 = work->field_5A & 0x3FF;
    oddStrangerDrive(arg0);
    Gfx_RotMatrixX(&arg0->extra.tmd->coords[2].coord, -0x80, 0);
    arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[3]);
    Gfx_RotMatrixX(&arg0->extra.tmd->coords[3].coord, -0x80, 0);
    arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[2]);
}

/// Per-frame body of the live actor while it walks: on work flag bit 0 it
/// raises the `0x10`/7/2 render slots, re-sends the `0x3FF` animation record
/// with clip 3 to the `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` task and seeds the walk step
/// `field_C0C` to -0x78; otherwise, while the `field_5A` clip is one of
/// `0x10..0x16`, it advances the actor along its own local Z by `field_C0C`
/// once `detectPlayerOutOfReach` says the path is still clear and halves
/// that step each time the `field_A30` contact fires. Both paths then tick the
/// animation, and — on work bit 0 — pick `field_0` from the enemy's state byte
/// (`6`, or `0xA` when the enemy is not the one `detectSightBlocked`
/// reports) and release the `0x3F1` message once.
static void func_actor_401800_80139118(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    PlayerStatus*    config;
    u8               kind;

    work   = arg0->work;
    enemy  = arg0->spawnArg2.pointer;
    config = &gPlayerStatus;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 7;
        work->field_898 = 2;
        oddStrangerDrive(arg0);
        D_actor_401800_80155A0C.animationId = 3;
        if (config->hp > 0) {
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &D_actor_401800_80155A0C, 0);
        }
        work->field_C0C        = -0x78;
        work->field_6          = 0;
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        return;
    }
    if ((Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3ED, 0, 0) == 0) && (config->hp > 0) && (work->field_C28 == 1)) {
        Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 0, 0);
        work->field_C28 = 0;
    }
    if ((u32)((work->field_5A & 0x3FF) - 0x10) < 7) {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, work->field_C0C) != 0) {
            actorMoveForwardNonzero(arg0->extra.tmd->coords, work->field_C0C);
        }
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) == 1) {
            work->field_C0C = work->field_C0C / 2;
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    oddStrangerDrive(arg0);
    if (work->flags_68.half & 1) {
        kind = enemy->node.state.parts.targeted;
        if (kind == 1) {
            if (detectSightBlocked(arg0) == kind) {
                work->field_0 = 6;
            } else {
                work->field_0 = 0xA;
            }
        } else {
            work->field_0 = 6;
        }
        if ((config->hp > 0) && (work->field_C28 == 1)) {
            Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 0, 0);
            work->field_C28 = 0;
        }
    }
}

/// Per-frame body of the live actor armed into state 1: raises the same
/// animation slots as `func_actor_401800_8013971C` but leaves `field_89E = 0xA`
/// (with `field_898 = 1` and `field_89A` cleared), then, while that slot is
/// still `0xA`, advances the actor along its own local Z by a fixed `-0x57`
/// once `detectPlayerOutOfReach` says the path is clear. The `0xA` branch
/// then flips the slots to `0xB`/2 and ticks the animation a second time before
/// the two contact records are rebuilt, after which work bit 0 picks `field_0`
/// from the enemy's HP sign and its buildup bit (`reactionFlags`).
static void func_actor_401800_8013945C(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8D0.radius        = 0x12C;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 1;
        work->field_89E               = 0xA;
        work->field_89A               = 0;
        work->field_8A2               = 0x10;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    if ((work->field_89E == 0xA) && ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, -0x57) != 0)) {
        actorStepForward(arg0->extra.tmd->coords, -0x57);
    }
    oddStrangerDrive(arg0);
    if ((work->flags_68.half & 1) && (work->field_89E == 0xA)) {
        work->field_89E = 0xB;
        work->field_898 = 2;
        oddStrangerDrive(arg0);
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_8F0, 0xC);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((work->flags_68.half & 1) && (work->field_89E == 0xB)) {
        work->field_8D0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Per-frame body of the live actor: arms the animation slots and the two
/// `field_8D0` / `field_A10` nodes, re-seeds the 0x8E8 and 0xA28 contact
/// records, then — while work bit 0x100 is set — picks `field_0` from the
/// enemy's HP sign and its buildup bit (`reactionFlags`). Same body as `Actor01900_Fn09BE8`.
static void func_actor_401800_8013971C(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8D0.radius        = 0x12C;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 2;
        work->field_89E               = 0xB;
        work->field_8A2               = 0x10;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    oddStrangerDrive(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_8F0, 0xC);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->flags_68.half & 0x100) {
        work->field_8D0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Second per-frame body of the live actor: as `func_actor_401800_8013971C`,
/// but it arms the animation slots with `field_89E = 0x19` and skips the
/// `field_5A` clip rebuild.
static void func_actor_401800_80139870(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8D0.radius        = 0x12C;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 2;
        work->field_89E               = 0x19;
        work->field_8A2               = 0x10;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    oddStrangerDrive(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_8F0, 0xC);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->flags_68.half & 0x100) {
        work->field_8D0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Third per-frame body of the live actor: `func_actor_401800_8013971C` with
/// the animation slots armed at 1 / 0xC, and its `field_0` selector driven by
/// work bit 0 instead of bit 8. Same body as `func_actor_401800_8013971C`
/// apart from those three constants.
static void func_actor_401800_801399C4(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8D0.radius        = 0x12C;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 1;
        work->field_89E               = 0xC;
        work->field_8A2               = 0x10;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    oddStrangerDrive(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_8F0, 0xC);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->flags_68.half & 1) {
        work->field_8D0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Grow-and-settle body of the live actor: while its flag is set it clears the
/// `field_C` overlay, drops the two `WorldCollisionBody` flag bits the previous body raised,
/// marks the enemy node live and restarts the step counter. The counter then
/// runs to 0x401, firing the light-mode and `0x600A5` effect cues as it crosses
/// steps 0x18, 0x1D, 0x29, 0x2F and 0x3F, and from step 0x1A on rebuilds the
/// root coordinate's Y rotation from its current yaw at scale 0x1194 with the
/// Y component shedding 0xB a step. Same body as `Actor01900_Fn06904` with the
/// `Gp_ReleaseStateF0Add` argument and the actor types changed.
static void func_actor_401800_80139B18(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       obj;
    s16              cur;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj->flags                    = 0;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_6                 = 0;
    }
    if (work->field_6 < 0x401) {
        switch ((s16)(work->field_6++ - 0x18)) {
            case 0:
                Gp_ReleaseStateF0Add(arg0, 0xA);
                break;
            case 5:
                Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                Gp_SpawnEff(0x600A5, arg0->extra.tmd->coords + 2, 3, NULL);
                break;
            case 23:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                break;
            case 17:
                Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case 39:
                arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
        cur = work->field_6;
        if (cur >= 0x1A) {
            actorRescaleYawY(arg0->extra.tmd->coords, 0x1194, 0x1194 - (cur - 0x14) * 0xB);
        }
    }
}

/// Countdown body: on the live-actor flag re-allocates the model's buffers,
/// copies the root coordinate over its `field_BC8` home and restarts the step
/// counter in state 0xE. The counter then runs to 0x961, rerolling the LCG each
/// frame past it and bailing for that frame on every 0xF-th draw; the surviving
/// frames re-test the squared XZ offset to the player against
/// `field_C16` and arm `gSceneCombatState` state 6 on a miss — bit 0x50000 there arms
/// it the same way. After the shared per-frame tick the body flips between
/// states 0xE and 0xF, one LCG draw per attempt, on the two `flags_68` mask
/// bits. Same shape as `func_actor_401800_8013A034`.
static void func_actor_401800_80139D60(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    SVECTOR          delta;
    SVECTOR*         d;
    u16              step;
    u32              lcg;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj        = arg0->extra.tmd;
        enemy      = arg0->spawnArg2.pointer;
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius        = 0x12C;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->field_6                 = 0;
        work->field_BC8               = work->field_BA8;
        work->field_89E               = 0xE;
        work->field_898               = 1;
        work->field_8A2               = work->field_8A4;
    }
    step = (u16)work->field_6;
    if (work->field_6 >= 0x961) {
        lcg             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = lcg;
        if (!((lcg >> 16) & 0xF)) {
            return;
        }
    } else {
        work->field_6 = (s16)(step + 1);
    }
    coord    = arg0->extra.tmd->coords;
    d        = &delta;
    delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy    = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz    = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!Actor401800_OutOfRange(d, work->field_C16)) {
        work->field_0 = 6;
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_NOISE_OR_OTHER_CAST) {
        work->field_0 = 6;
    }
    oddStrangerDrive(arg0);
    if (work->field_89E == 0xE) {
        if (work->flags_68.half & 2) {
            lcg             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = lcg;
            if ((lcg >> 16) & 1) {
                work->field_89E = 0xF;
                work->field_898 = 1;
                oddStrangerDrive(arg0);
            }
        }
    }
    if (work->field_89E == 0xF && (work->flags_68.half & 1)) {
        work->field_89E = 0xE;
        work->field_898 = 1;
        oddStrangerDrive(arg0);
    }
}

/// Walking body: on the live-actor flag re-allocates the model's buffers,
/// hands the actor the `D_actor_401800_80155124` animation block and zeroes the
/// step counter and the 0x8A2..0x8B0 pose slots, otherwise plays the actor's
/// 0x51030008 spawn sound once on the first frame. After the shared per-frame
/// tick, a `field_5A` state of 4 that differs from the last handled one
/// (`field_8B4`) sends the 0x200-scale effect for the second coordinate part.
/// Then, if the squared XZ offset to the player fits inside
/// `field_C16`, the actor plays 0x51030008 and arms `gSceneCombatState` in state 6 —
/// bit 0x50000 of `gSceneCombatState` arms it the same way. Same shape as
/// `Actor01900_Fn06B4C` and `func_actor_401300_801397F8`.
static void func_actor_401800_8013A034(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    SVECTOR          delta;
    SVECTOR*         d;
    s32              sound;
    s32              pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj                         = arg0->extra.tmd;
        D_actor_401800_80155938[16] = &D_actor_401800_80155124;
        work->field_89E             = 0x10;
        work->field_898             = 2;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius        = 0x12C;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_8B0               = 0;
        work->field_8A2               = 0x10;
        work->field_8AE               = 0;
        work->field_6                 = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x51030008;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->field_6 = 1;
    }
    oddStrangerDrive(arg0);
    if ((work->field_5A & 0x3FF) == 4 && work->field_8B4 != (work->field_5A & 0x3FF)) {
        work->field_8B8.coord      = arg0->extra.tmd->coords + 1;
        work->field_8B8.spawnArgLo = 0x200;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->extra.tmd->coords + 5, NULL, &work->field_8B8);
    }
    work->field_8B4 = work->field_5A & 0x3FF;
    coord           = arg0->extra.tmd->coords;
    d               = &delta;
    delta.vx        = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy           = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz           = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!Actor401800_OutOfRange(d, work->field_C16)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_NOISE_OR_OTHER_CAST) {
        work->field_0 = 6;
    }
}

/// Patrol state: walks toward the waypoint `field_14` selects, turning at most
/// 0x18 per step and swapping waypoints once the waypoint is inside 0xA0 or
/// `field_6` has run 0x15 frames. The turn is folded into the root coordinate,
/// which is rebuilt at scale 0x1194, and the actor steps 7 units along its own
/// local Z while `detectPlayerOutOfReach` reports the path clear. The
/// `field_A30` / `field_8F0` contact records then decide whether `field_6`
/// counts up or `func_actor_401800_8013629C` re-seeds them. In the tail the
/// `gPlayerStatus` offset arms state 6 within `field_C16`, or within 0xFA0 when
/// the aim toward the player is under 0x300. Same body as
/// `Actor01900_Fn06F40` / `func_actor_401300_80139AB0`, with the aim and step
/// helpers inlined. The waypoint delta is written twice; the retail build keeps
/// both sets of stores.
static void func_actor_401800_8013A2E8(Task* arg0)
{
    OddStrangerWork*  work;
    TmdObject*        obj;
    GfxCoord*         coord;
    GfxCoord*         facing;
    ActorTurnScratch* s;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        oddStrangerDrive(arg0);
        work->field_6 = 0;
        if ((arg0->spawnArg1.value >> 16) == 0x10) {
            work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        }
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    s           = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    s->delta.vx = work->field_C[work->field_14].x - arg0->extra.tmd->coords->coord.t[0];
    s->delta.vy = 0;
    s->delta.vz = work->field_C[work->field_14].z - arg0->extra.tmd->coords->coord.t[2];
    s->delta.vx = work->field_C[work->field_14].x - arg0->extra.tmd->coords->coord.t[0];
    s->delta.vy = 0;
    s->delta.vz = work->field_C[work->field_14].z - arg0->extra.tmd->coords->coord.t[2];
    if (!Actor401800_OutOfRange(&s->delta, 0xA0) || work->field_6 >= 0x15) {
        if (work->field_14 == 0) {
            work->field_14 = 1;
        } else {
            work->field_14 = 0;
        }
        work->field_6 = 0;
    }
    oddStrangerDrive(arg0);
    coord           = arg0->extra.tmd->coords;
    s->angle        = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = s->angle;
    if (s->angle > 0x18) {
        s->angle = 0x18;
    }
    if (s->angle < -0x18) {
        s->angle = -0x18;
    }
    facing    = arg0->extra.tmd->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, s->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    if (work->field_89A == 0 && (s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, 7) != 0) {
        actorStepForward(arg0->extra.tmd->coords, 7);
    }
    if ((arg0->spawnArg1.value >> 16) != 0x10) {
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) == 1 &&
            ABS(work->field_8AE) < 0x80) {
            work->field_6++;
        } else {
            func_actor_401800_8013629C(arg0, work->field_8F0, 0xC);
        }
    } else {
        if ((ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) == 1 ||
             ActorContact_PushContact(arg0->extra.tmd->coords, work->field_8F0, 0xC) == 1) &&
            ABS(work->field_8AE) < 0x80) {
            work->field_6++;
        } else {
            func_actor_401800_8013629C(arg0, work->field_8F0, 0xC);
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (detectSightBlocked(arg0) != 1) {
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
        if (!Actor401800_OutOfRange(&s->delta, work->field_C16)) {
            work->field_0 = 6;
        } else if (!Actor401800_OutOfRange(&s->delta, 0xFA0)) {
            coord    = arg0->extra.tmd->coords;
            s->angle = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
            if (ABS(s->angle) < 0x300) {
                work->field_0 = 6;
            }
        }
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_ATTACK_MASK) {
        work->field_0 = 6;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// Aim the actor at the player, fold the clamped turn into the root
/// coordinate's Y rotation, then step it along its own local Z while
/// `detectPlayerOutOfReach` says the path is clear — reloading
/// `field_0 = 9` once the `field_C04` step countdown runs out. Same body as
/// `func_actor_401300_8013A208`, with the aim and step helpers inlined.
static void func_actor_401800_8013AB64(Task* arg0)
{
    OddStrangerWork*  work;
    Enemy*            enemy;
    TmdObject*        obj;
    GfxCoord*         coord;
    ActorTurnScratch* turn;
    u16               next;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy           = arg0->spawnArg2.pointer;
        obj             = arg0->extra.tmd;
        work->field_89E = 0x12;
        work->field_898 = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius        = 0x12C;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_8B0               = 0;
        work->field_8A2               = 0x1E;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn            = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turn->angle     = actorPositionYaw(arg0, &turn->delta, &gPlayerStatus);
    work->field_8AE = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->extra.tmd->coords;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) != 1) {
        func_actor_401800_8013629C(arg0, work->field_8F0, 0xC);
    }
    if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, work->field_C04) != 0) {
        actorMoveForwardNonzero(arg0->extra.tmd->coords, work->field_C04);
    }
    if (work->field_C04 > 0) {
        next            = work->field_C04 - 0xA;
        work->field_C04 = next;
        if ((s16)next < 0) {
            work->field_C04 = 0;
        }
    }
    oddStrangerDrive(arg0);
    if ((work->flags_68.half & 1) || work->field_C04 == 0) {
        work->field_0 = 9;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// Aim the actor at the player and rebuild its root coordinate from the new
/// yaw at scale 0x1194, clamping the turn it adds to +-0x80 and halving it
/// when it is not below -0x80. Once the aim state reaches 0x11 it counts
/// frames in `field_6`, steps the actor along its own local Z while
/// `detectPlayerOutOfReach` says the path is clear, re-seeds the
/// `field_A30` contact record and past 0x13 frames turns the actor away from
/// the side the player is on by +-0x4B0. On the live flag it resets the model
/// buffers and arms the state 2 the aim test promotes to 0x11 within 0x80.
/// Same body as `Actor01900_Fn080A8` / `func_actor_401300_8013A5C0`, with the
/// aim, rescale and step helpers inlined.
static void func_actor_401800_8013AF1C(Task* arg0)
{
    OddStrangerWork*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x16;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        oddStrangerDrive(arg0);
        return;
    }
    oddStrangerDrive(arg0);
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn       = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->field_8AE = aim->turn;
    if (ABS(aim->turn) <= 0x80 && work->field_89E == 2) {
        work->field_8A2 = 0x16;
        work->field_89E = 0x11;
        work->field_898 = 1;
        work->field_6   = 0;
        oddStrangerDrive(arg0);
    }
    if (aim->turn > 0x80) {
        aim->turn = 0x80;
    }
    if (aim->turn < -0x80) {
        aim->turn = -0x80;
    } else {
        aim->turn = aim->turn >> 1;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89E == 0x11) {
        work->field_6++;
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, -0x10) != 0) {
            actorStepForward(arg0->extra.tmd->coords, -0x10);
        }
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) != 1) {
            func_actor_401800_8013629C(arg0, work->field_8F0, 0xC);
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->field_6 >= 0x13) {
            if (work->field_8AE <= 0) {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x4B0, 0);
            } else {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x4B0, 0);
            }
            work->field_0 = 7;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401800_8013B444(Task* arg0)
{
    OddStrangerWork*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 9;
        work->field_89A        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        oddStrangerDrive(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
    aim->turn       = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->field_8AE = aim->turn;
    if (aim->turn > 0) {
        aim->turn = 0;
    }
    if (aim->turn < 0) {
        aim->turn = 0;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    oddStrangerDrive(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Aim the actor at the player and rescale its root coordinate, turning the
/// stored yaw toward the target by at most 0x28 a frame instead of the hard
/// clamp `func_actor_401800_80135F58` uses. On the live flag it resets the
/// model buffers and re-arms the step countdown; otherwise it hands the
/// player offset and the new yaw to the actor's state body and rebuilds the
/// matrix at scale 0x1194.
static void func_actor_401800_8013B784(Task* arg0)
{
    OddStrangerWork*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x12C;
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_89E        = 0x13;
        work->field_89A        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        oddStrangerDrive(arg0);
        oddStrangerDrive(arg0);
        work->field_6   = 0;
        work->field_8B0 = 0;
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim       = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    if (work->field_8AE < aim->turn) {
        if (aim->turn - work->field_8AE >= 0x29) {
            work->field_8AE = (u16)work->field_8AE + 0x28;
        } else {
            work->field_8AE = aim->turn;
        }
    } else if (work->field_8AE - aim->turn >= 0x29) {
        work->field_8AE = (u16)work->field_8AE - 0x28;
    } else {
        work->field_8AE = aim->turn;
    }
    if (work->field_8AE == aim->turn && detectSightBlocked(arg0) != 1 && work->field_8CA == 0) {
        work->field_0 = 0xB;
    }
    coord     = arg0->extra.tmd->coords;
    aim->turn = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    work->field_898 = 2;
    oddStrangerDrive(arg0);
    if (work->field_8CA != 0) {
        work->field_8CA--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Step-driven effect spawner for the actor's live ramp: while the spawn flag
/// is set the actor crouches (0x8C8 node pitched to 0x12C, 0xA08 flags bit
/// 0x4000 cleared), plays the 0x60030 debris burst and hands the task to the
/// state-F0 list; the step counter then fires the 0xA0005 effects at 3, 5, 7
/// and 9, each tinted from the enemy's area record, and parks the actor at 0x3D.
static void func_actor_401800_8013BB10(Task* arg0)
{
    SVECTOR          vec;
    OddStrangerWork* work;
    Enemy*           enemy;
    u16              next;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->field_8D0.radius        = 0x12C;
        work->field_A10.flags         = (u16)(work->field_A10.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_8AE               = 0;
        work->field_6                 = 0U;
        vec.vx                        = 0x64;
        vec.vz                        = 0;
        vec.vy                        = 0;
        Gp_SpawnEff(0x60030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
        Gp_ReleaseStateF0Add(arg0, 0xA);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    if ((s16)next == 3) {
        D_80114B34[5].data.model = &D_actor_401800_80143E9C;
        vec.vz                   = 0x64;
        vec.vy                   = 0;
        vec.vx                   = 0;
        actorTintEffect(Gp_SpawnEff(0xA0005, arg0->extra.tmd->coords + 9, 0x200, &vec), enemy);
    }
    if (work->field_6 == 5) {
        D_80114B34[5].data.model = &D_actor_401800_80144434;
        vec.vy                   = 0;
        vec.vx                   = 0;
        actorTintEffect(Gp_SpawnEff(0xA0005, arg0->extra.tmd->coords + 12, 0x200, &vec), enemy);
    }
    if (work->field_6 == 7) {
        D_80114B34[5].data.model = &D_actor_401800_80143E9C;
        actorTintEffect(Gp_SpawnEff(0xA0005, arg0->extra.tmd->coords + 1, 0x200, NULL), enemy);
    }
    if (work->field_6 == 9) {
        D_80114B34[5].data.model = &D_actor_401800_80144F24;
        actorTintEffect(Gp_SpawnEff(0xA0005, arg0->extra.tmd->coords + 3, 0x200, NULL), enemy);
    }
    if (work->field_6 >= 0x3D) {
        work->field_0 = 0;
    }
}

/// Step-driven aim-and-rescale body: while the spawn flag is set the actor
/// pitches its 0x8C8 node to the 0x12C walk animation, sets the 0xA08 flags bit
/// 0x4000, plays the 0x60030 burst and arms the 2/0x1A state pair; the step
/// counter then steps the actor along its facing while
/// `detectPlayerOutOfReach` reports the path clear, re-seeds the
/// `field_A30` contact and fires the 0xA0005 effects at 3, 5 and 6. The 0x1A
/// state runs its light/state table and folds the countdown into the root
/// coordinate's Y scale; the nine body coordinates from +2 to +10 are then
/// rebuilt at unit scale.
static void func_actor_401800_8013BF48(Task* arg0)
{
    SVECTOR          vec;
    OddStrangerWork* work;
    Enemy*           enemy;
    u16              next;
    s16              cur;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_8D0.radius        = 0x12C;
        work->field_A10.flags         = (u16)(work->field_A10.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_8AE               = 0;
        work->field_6                 = 0U;
        vec.vx                        = 0x64;
        vec.vz                        = 0;
        vec.vy                        = 0;
        work->field_89E               = 2;
        work->field_898               = 1;
        work->field_8A2               = 0x10;
        Gp_SpawnEff(0x60030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
        work->field_6 = 0U;
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    switch (work->field_89E) {
        case 2:
            if ((s16)next >= 0x10 && (work->flags_68.half & 2)) {
                work->field_89E = 0x1A;
                work->field_898 = 2;
                work->field_8A2 = 0x10;
                work->field_89A = 0;
            }
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, 7) != 0) {
                actorStepForward(arg0->extra.tmd->coords, 7);
            }
            ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC);
            if (work->field_6 == 3) {
                D_80114B34[5].data.model = &D_actor_401800_80143E9C;
                vec.vz                   = 0x64;
                vec.vy                   = 0;
                vec.vx                   = 0;
                actorTintEffect(Gp_SpawnEff(0xA0005, arg0->extra.tmd->coords + 9, 0x200, &vec), enemy);
            }
            if (work->field_6 == 5) {
                D_80114B34[5].data.model = &D_actor_401800_80143E9C;
                actorTintEffect(Gp_SpawnEff(0xA0005, arg0->extra.tmd->coords + 1, 0x200, NULL), enemy);
            }
            if (work->field_6 == 6) {
                D_80114B34[5].data.model = &D_actor_401800_80144F24;
                actorTintEffect(Gp_SpawnEff(0xA0005, arg0->extra.tmd->coords + 3, 0x200, NULL), enemy);
            }
            break;
        case 0x1A:
            if (!(work->flags_68.half & 0x100)) {
                work->field_6 = 0;
            }
            switch ((s16)(work->field_6 - 0x19)) {
                case 0:
                    Gp_ReleaseStateF0Add(arg0, 0xA);
                    break;
                case 5:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                    Gp_SpawnEff(0x600A5, arg0->extra.tmd->coords + 2, 2, NULL);
                    break;
                case 23:
                    arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    break;
                case 17:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case 39:
                    arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->field_0          = 0;
                    break;
            }
            cur = work->field_6;
            if (cur >= 0x1A) {
                actorRescaleYawY(arg0->extra.tmd->coords, 0x1194, 0x1194 - (cur - 0x14) * 0xB);
            }
            break;
    }
    oddStrangerDrive(arg0);
    actorResetYaw(arg0->extra.tmd->coords + 2);
    actorResetYaw(arg0->extra.tmd->coords + 3);
    actorResetYaw(arg0->extra.tmd->coords + 4);
    actorResetYaw(arg0->extra.tmd->coords + 5);
    actorResetYaw(arg0->extra.tmd->coords + 6);
    actorResetYaw(arg0->extra.tmd->coords + 7);
    actorResetYaw(arg0->extra.tmd->coords + 8);
    actorResetYaw(arg0->extra.tmd->coords + 9);
    actorResetYaw(arg0->extra.tmd->coords + 10);
}

/// Walk body: takes a 0x10 scratch for the player offset, the facing yaws and
/// the wrapped turn toward the player. On the live flag it resets the model
/// buffers, arms the walk state (`0x12C` animation, step 0x30) and zeroes the
/// step counters; otherwise it counts both step slots, re-seeds the `field_A30`
/// / `field_8F0` contacts, folds the turn into the root coordinate's Y rotation
/// at scale 0x1194 and steps the actor 0x15 / 5 along its own local Z while
/// `detectPlayerOutOfReach` says the path is clear. On the `func_actor_
/// 401800_80133918` hit it clears the stride, picks a side from the LCG and
/// flips it every 0xF1 frames instead. Same body as `Actor01900_Fn042BC`.
static void func_actor_401800_8013CD98(Task* arg0)
{
    OddStrangerWork*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    ActorChaseScratch* s;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x30;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_C24        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Gp_ArmStateF0(1);
        work->field_6 = 0;
        work->field_8 = 0;
        if ((arg0->spawnArg1.value >> 16) == 0x10) {
            work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    }
    work->field_6++;
    work->field_8++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    s = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) != 1) {
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_8F0, 0xC) != 1) {
            func_actor_401800_8013629C(arg0, work->field_8F0, 0xC);
        }
    }
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    oddStrangerDrive(arg0);
    s->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                          (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
    s->yaw = ratan2(s->delta.vx, s->delta.vz) + 0x800;
    s->yaw = actorNormalizeYaw(s->yaw);
    if (detectSightBlocked(arg0) != 1) {
        work->field_6   = 0;
        coord           = arg0->extra.tmd->coords;
        s->turn         = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) -
                                            ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->turn;
        if (s->turn < 0x200) {
            if (!Actor401800_OutOfRange(&s->delta, 0x44C) && work->field_8CA == 0) {
                work->field_0 = 0xB;
            }
        }
        if (work->field_8 >= 0x5B) {
            work->field_0 = 0x1B;
        }
    } else {
        work->field_8 = 0;
        work->field_6++;
        coord           = arg0->extra.tmd->coords;
        s->turn         = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) -
                                            ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->turn;
        if (work->field_C08 == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->field_C08 = -1;
            } else {
                work->field_C08 = 1;
            }
        }
        if (work->field_C08 == 1) {
            s->turn += 0x400;
        } else {
            s->turn -= 0x400;
        }
        if (work->field_6 >= 0xF1) {
            work->field_6   = 0;
            work->field_C08 = -work->field_C08;
        }
    }
    if (s->turn > 0x20) {
        s->turn = 0x20;
    }
    if (s->turn < -0x20) {
        s->turn = -0x20;
    }
    facing   = arg0->extra.tmd->coords;
    s->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, s->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89E == 2) {
        if (work->field_89A == 0) {
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, 0x15) != 0) {
                actorStepForward(arg0->extra.tmd->coords, 0x15);
            }
        } else if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, 5) != 0) {
            actorStepForward(arg0->extra.tmd->coords, 5);
        }
    } else if (work->flags_68.half & 1) {
        work->field_89E = 2;
        work->field_898 = 1;
    }
    if (work->field_8CA != 0) {
        work->field_8CA--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static const Actor401800StateTable D_actor_401800_80131FDC = { {
    func_actor_401800_8013E138,
    func_actor_401800_8013E194,
    func_actor_401800_8013E23C,
    func_actor_401800_8013E2E8,
    func_actor_401800_80135DAC,
    func_actor_401800_8013E394,
    func_actor_401800_80135F58,
    func_actor_401800_80136560,
    func_actor_401800_80136EAC,
    func_actor_401800_80137714,
    func_actor_401800_80137DDC,
    func_actor_401800_801381E4,
    func_actor_401800_80138C28,
    func_actor_401800_80138F5C,
    func_actor_401800_80139118,
    func_actor_401800_8013E44C,
    func_actor_401800_8013E4F0,
    func_actor_401800_8013E5A4,
    NULL,
    func_actor_401800_8013945C,
    func_actor_401800_801399C4,
    func_actor_401800_80139B18,
    func_actor_401800_80139D60,
    func_actor_401800_8013A034,
    func_actor_401800_8013A2E8,
    func_actor_401800_8013AF1C,
    func_actor_401800_8013AB64,
    func_actor_401800_8013B444,
    func_actor_401800_8013B784,
    func_actor_401800_8013BB10,
    func_actor_401800_8013CD98,
    func_actor_401800_8013971C,
    func_actor_401800_80139870,
    func_actor_401800_8013BF48,
} };

static void func_actor_401800_8013D64C(Enemy* arg0, Task* arg1)
{
    VECTOR                pos;
    Actor401800StateTable states;
    OddStrangerWork*      work;
    ActorViewScratch*     scratch;
    ActorViewScratch*     head;
    s32                   state;
    s32                   stop;
    s32                   index;

    work   = arg1->work;
    states = D_actor_401800_80131FDC;

    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    pos.vx = arg1->extra.tmd->coords->workm.t[0];
    pos.vy = arg1->extra.tmd->coords->workm.t[1];
    pos.vz = arg1->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x21)) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
                state = work->field_0;
            }
            if ((state == 0x21) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x21)) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
                state = work->field_0;
            }
            if ((state == 0x21) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            Gp_ClearRec18Occupied(work->field_A30);
            Gp_ClearRec18Occupied(work->field_8F0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Gp_ClearRec18Occupied(work->field_A30);
            Gp_ClearRec18Occupied(work->field_8F0);
            return;
    }

    head                                   = SCRATCH_STACK_CURSOR(ActorViewScratch);
    SCRATCH_STACK_CURSOR(ActorViewScratch) = head - 1;
    scratch                                = head - 1;

    if (work->field_BE8 > 0) {
        work->field_BE8 = (s16)((u16)work->field_BE8 - 1);
    } else {
        func_actor_401800_80134C94(arg1);
    }
    if (work->field_2 != work->field_0) {
        if ((work->field_2 == 0xB) || (work->field_2 == 0xD)) {
            arg1->extra.tmd->coords->coord.t[0]   = work->field_BF8.vx;
            arg1->extra.tmd->coords->coord.t[1]   = work->field_BF8.vy;
            arg1->extra.tmd->coords->coord.t[2]   = work->field_BF8.vz;
            arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(arg1->extra.tmd->coords);
        }
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    index         = work->field_0;
    SCHED_BARRIER();
    stop = 0x15;
    states.fn[index](arg1);
    state = work->field_0;
    if ((state == 0x1C) || (state == stop) || (state == 0) || (state == 0x1D) || (state == 0x21)) {
        work->field_8D0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->field_8D0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->field_A10.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    Gp_ClearRec18Occupied(work->field_A30);
    Gp_ClearRec18Occupied(work->field_8F0);
    if ((gSceneCombatState.signals.bytes.enemyAlert == 1) && (work->field_0 == 0x18)) {
        work->field_0 = 6;
    }

    scratch->pos.vx = 0;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    actorTransformToView(arg1->extra.tmd->coords + 2, &scratch->pos);

    work->field_C2C[work->field_C7C].vx = scratch->pos.vx;
    work->field_C2C[work->field_C7C].vy = scratch->pos.vy;
    work->field_C2C[work->field_C7C].vz = scratch->pos.vz;

    SCRATCH_STACK_RELEASE_BYTES(0x18);
    work->field_C7C = (u16)work->field_C7C + 1;
    if (work->field_C7C == 7) {
        work->field_C7C = 0;
    }
    if ((u32)((u16)work->field_89E - 0x14) < 2U) {
        arg0->bodyPos.vx = work->field_C2C[work->field_C7C].vx;
        arg0->bodyPos.vy = work->field_C2C[work->field_C7C].vy;
        arg0->bodyPos.vz = work->field_C2C[work->field_C7C].vz;
    } else {
        arg0->bodyPos.vx = scratch->pos.vx;
        arg0->bodyPos.vy = scratch->pos.vy;
        arg0->bodyPos.vz = scratch->pos.vz;
    }
    arg0->coord = &gGfxViewCoord;
    TOUCH_REG(stop);
}

void func_actor_401800_8013DCB4(void)
{
}

/// The enemy task's three handlers, indexed by `Task::state`: the first
/// initialises the actor, the second runs it every frame, and the third tears
/// the enemy down.
static const GpEnemyTaskFuncTable3 D_actor_401800_80132064 = { {
    func_actor_401800_8013423C,
    func_actor_401800_8013D64C,
    Gp_DestroyEnemy,
} };

#include "../../shared/odd_stranger_play_message.inc.c"

#include "../../shared/actor_messages_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place_yaw.inc.c"

#include "../../shared/actor_messages_release_hold.inc.c"

/// Room request handler: copies the request's three leading bytes into the work
/// block, then dispatches on the request's room id and state pair.
///
/// Room `0x301` only accepts state `1` (mount); room `0x1002` accepts `0`
/// (reset) or `2`, which parks the actor's root coordinate at
/// (-0x595, 0, -0x5B1) and rebuilds its Y rotation from -0x400, clearing `composeStamp`
/// so the coordinate tree recomputes. Anything else returns 0, leaving the work
/// block's `field_0` state alone. Same body as `Actor01900_Fn0A5A4`, which
/// takes the same record and dispatches on the same two room ids.
s32 func_actor_401800_8013DF80(Task* arg0, s32 arg1, u16* arg2)
{
    OddStrangerWork* work;
    u16              room;
    u16              state;
    u16              state2;

    work = arg0->work;

    work->field_C18[0] = ((u8*)arg2)[0];
    work->field_C18[1] = ((u8*)arg2)[1];
    work->field_C18[2] = ((u8*)arg2)[2];

    room = arg2[0];
    if (room == 0x301) {
        state = arg2[1];
        switch (state) {
            case 1:
                work->field_0 = 0x17;
                return 1;
            default:
                return 0;
        }
    } else if (room == 0x1002) {
        state2 = arg2[1];
        switch (state2) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 2:
                work->field_0                       = 0x1C;
                arg0->extra.tmd->coords->coord.t[0] = -0x595;
                arg0->extra.tmd->coords->coord.t[1] = 0;
                arg0->extra.tmd->coords->coord.t[2] = -0x5B1;
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x400, 1);
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                return 1;
            default:
                return 0;
        }
    } else {
        return 0;
    }
}

static void func_actor_401800_8013E0A0(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = (OddStrangerWork*)task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    if (work != NULL) {
        if (work->field_C1C != NULL) {
            taskKill(work->field_C1C);
        }
        if (work->field_C20 != NULL) {
            taskKill(work->field_C20);
        }
        Gp_UnlinkObj(&work->field_B50);
        Gp_UnlinkObj(&work->field_8D0);
        Gp_UnlinkObj(&work->field_A10);
        enemy->recs = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

static void func_actor_401800_8013E138(Task* arg0)
{
    TmdObject*       obj;
    OddStrangerWork* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                                = (u16)(obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->field_B50.flags                                     = (u16)(work->field_B50.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_A10.flags                                     = (u16)(work->field_A10.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
    }
}

static void func_actor_401800_8013E194(Task* arg0)
{
    TmdObject*       obj;
    OddStrangerWork* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 2;
        work->field_89A       = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_A10.flags = (u16)(work->field_A10.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        oddStrangerDrive(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        oddStrangerDrive(arg0);
    }
}

static void func_actor_401800_8013E23C(Task* arg0)
{
    TmdObject*       obj;
    OddStrangerWork* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 3;
        work->field_89A       = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_A10.flags = (u16)(work->field_A10.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        oddStrangerDrive(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        oddStrangerDrive(arg0);
    }
}

static void func_actor_401800_8013E2E8(Task* arg0)
{
    TmdObject*       obj;
    OddStrangerWork* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 0xB;
        work->field_89A       = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_A10.flags = (u16)(work->field_A10.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        oddStrangerDrive(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        oddStrangerDrive(arg0);
    }
}

static void func_actor_401800_8013E394(Task* arg0)
{
    TmdObject*       obj;
    OddStrangerWork* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x12;
        work->field_89E       = 0xD;
        work->field_89A       = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_A10.flags = (u16)(work->field_A10.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    oddStrangerDrive(arg0);
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
}

static void func_actor_401800_8013E44C(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8D0.radius        = 0x12C;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 2;
        work->field_89E               = 8;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        work->field_8A2               = work->field_8A4;
    }
    oddStrangerDrive(arg0);
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
}

static void func_actor_401800_8013E4F0(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags        = 0;
        work->field_8D0.radius        = 0x12C;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_898               = 2;
        work->field_89E               = 0x16;
        work->field_8B0               = 0;
        work->field_8AE               = 0;
        work->field_6                 = 0;
        work->field_8A2               = work->field_8A4;
    }
    work->field_6 = (u16)(work->field_6 + 1);
    oddStrangerDrive(arg0);
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
}

/// Idle-step handler: with the live flag set a fresh `gRandomLcgState` draw is
/// spread over the step countdown as 0..7 extra steps, and once the countdown
/// underflows the animation state at `field_89E` picks the actor's next
/// `field_0` (0xF for states 11/23, 0x10 for 12/24/25); a target with no HP
/// left forces 0x15 over that. Same body as `func_actor_401300_80141DF4`,
/// whose counterpart masks the LCG draw with 0xF instead of 7.
static void func_actor_401800_8013E5A4(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_6   = work->field_C10 + ((gRandomLcgState >> 16) & 7);
    }
    if (--work->field_6 < 0) {
        switch (work->field_89E) {
            case 11:
            case 23:
                work->field_0 = 0xF;
                break;
            case 12:
            case 24:
            case 25:
                work->field_0 = 0x10;
                break;
        }
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x15;
    }
    oddStrangerDrive(arg0);
}

/// Runs the actor's handler for the task's current state, copying the
/// three-entry table onto the stack first.
void func_actor_401800_8013E68C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_401800_80132064;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
