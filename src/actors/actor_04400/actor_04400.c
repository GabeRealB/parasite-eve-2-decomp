#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actors_shared_80163354.h"

#include "actors/actors_shared_801673f8.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_dumping_hole.h"

#include "rooms/shelter_b3_garbage_incinerator.h"
#include "../../shared/mad_chaser.h"

/// Status flags at `Actor104400Work` + 0xEC, read through two widths.
///
/// Guards test bit 0 as a halfword and then bits 0x102 as a word
/// (`madChaserAnimEnded` is the out-of-line copy of the test).
typedef union Actor104400Flags {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
} Actor104400Flags;
STATIC_ASSERT_SIZEOF(Actor104400Flags, 0x4);

/// Per-actor state block for the `actor_104400` overlay's enemy.
///
/// `madChaserSpawn` and `madChaserSpawnHidden` both allocate it with
/// `memCalloc(0x454, 0)` and store it in the `Task::work` slot (0x1C), so
/// the size below is the allocation, not a guess: this actor reuses that
/// pointer field for its own work block and it is *not* a `TaskIdMap` here.
/// Reach it with `(Actor104400Work*)task->work`.
///
/// `field_420` / `field_422` are the state and sub-state indices the handler
/// table walks, `field_412` is the per-state frame counter, and
/// `field_414` .. `field_426` are the animation request the actor hands to
/// its player. The three `WorldCollisionBody` nodes are the collision objects
/// `madChaserDropBodies` hands back to `Gp_UnlinkObj`. `obj_2AC` and
/// `obj_2CC` share `rec_2EC`; `obj_3AC` has its own table at `rec_3CC`.
typedef struct Actor104400Work {
    /* 0x000 */ MATRIX           matrix_0; // model root coord, copied out on the kill path
    /* 0x020 */ MATRIX           colorMtx; // the model's `TmdObject::colorMtx`
    /* 0x040 */ MATRIX           lightMtx; // the model's `TmdObject::lightMtx`
    /* 0x060 */ VECTOR           field_60; // position madChaserApplyContacts snaps the root back to when blocked
    /* 0x070 */ SVECTOR          field_70; // origin of slot 4 entry 0's coords[3], carried into view space by Actor04400_Fn05B08
    /* 0x078 */ s16              field_78; // pitch, fed to RotMatrixX
    /* 0x07A */ s16              field_7A; // heading
    /* 0x07C */ s16              field_7C; // roll, fed to RotMatrixZ
    /* 0x07E */ byte             pad_7E[0x2];
    /* 0x080 */ u16              field_80; // spawn position: root coord.t[0]
    /* 0x082 */ u16              field_82; // root coord.t[1], after lifting it by 0x3C
    /* 0x084 */ u16              field_84; // root coord.t[2]
    /* 0x086 */ byte             pad_86[0x2];
    /* 0x088 */ s16              field_88; // x of the vector turned towards
    /* 0x08A */ s16              field_8A;
    /* 0x08C */ s16              field_8C; // z of the vector turned towards
    /* 0x08E */ byte             pad_8E[0x2];
    /* 0x090 */ u16              field_90; // root coord.t[0], snapshotted with field_92 / field_94
    /* 0x092 */ u16              field_92; // root coord.t[1]
    /* 0x094 */ u16              field_94; // root coord.t[2]
    /* 0x096 */ byte             pad_96[0x2];
    /* 0x098 */ SVECTOR          field_98; // translation of coords[6] relative to the view
    /* 0x0A0 */ AnimationContext anim;
    /// First of the nine `AnimationSlot`s (0xB4..0x21C); the second overlaps
    /// `flags_EC`, so only the first is spelled out.
    /* 0x0B4 */ AnimationSlot         slot_B4;
    /* 0x0DC */ byte                  pad_DC[0x10];
    /* 0x0EC */ Actor104400Flags      flags_EC;
    /* 0x0F0 */ byte                  pad_F0[0x12C];
    /* 0x21C */ byte                  field_21C[0x90]; // `func_800B3F84`'s arg3 buffer
    /* 0x2AC */ WorldCollisionBody    obj_2AC;
    /* 0x2CC */ WorldCollisionBody    obj_2CC;
    /* 0x2EC */ WorldCollisionContact rec_2EC[8];
    /* 0x3AC */ WorldCollisionBody    obj_3AC;
    /* 0x3CC */ WorldCollisionContact rec_3CC[2];
    /* 0x3FC */ EffectSpawnArg        eff_3FC;   // `coord` is the model's `coords[1]`
    /* 0x404 */ byte                  pad_404[0x8];
    /* 0x40C */ s16                   field_40C; // heading madChaserLeapAttack moves the root along
    /* 0x40E */ s16                   field_40E; // hit cooldown: `Gp_GetIdParam2` of the last hit, counted down each frame
    /* 0x410 */ s16                   field_410; // random 0..0x7FF drawn from `gRandomLcgState`
    /* 0x412 */ u16                   field_412; // per-state frame counter
    /* 0x414 */ s16                   field_414; // animation request kind
    /* 0x416 */ s16                   field_416; // animation id last applied to the slots
    /* 0x418 */ s16                   field_418; // animation id
    /* 0x41A */ u16                   field_41A; // frames since the animation was applied
    /* 0x41C */ s16                   field_41C; // animation speed / step scale
    /* 0x41E */ s16                   field_41E;
    /* 0x420 */ u16                   field_420; // state index
    /* 0x422 */ u16                   field_422; // sub-state index
    /* 0x424 */ s16                   field_424; // yaw added to model parts 3..5, a third each
    /* 0x426 */ s16                   field_426;
    /* 0x428 */ s16                   field_428;
    /* 0x42A */ s16                   field_42A;
    /* 0x42C */ s16                   field_42C; // frames spent turning toward field_444; 16 enters state 3
    /* 0x42E */ byte                  pad_42E[0x2];
    /* 0x430 */ s16                   field_430;
    /* 0x432 */ s16                   field_432; // 1 runs madChaserPinPart on the spawn position
    /* 0x434 */ s16                   field_434; // pitch, eased back to zero while falling
    /* 0x436 */ s16                   field_436; // step picked from `field_43A`'s distance band
    /* 0x438 */ s16                   field_438; // 1 on the death path
    /* 0x43A */ s16                   field_43A; // distance to the nearer player actor
    /* 0x43C */ byte                  pad_43C[0x2];
    /* 0x43E */ s16                   field_43E; // counted down each frame by madChaserApplyContacts
    /* 0x440 */ s16                   field_440; // picks animation 5 (zero) or 6 after animation 8
    /* 0x442 */ u16                   field_442;
    /* 0x444 */ u16                   field_444; // heading to the nearer player actor, relative to field_7A
    /* 0x446 */ s16                   field_446; // randomised hold compared against field_412
    /* 0x448 */ s16                   field_448;
    /* 0x44A */ s16                   field_44A;
    /* 0x44C */ u16                   field_44C; // message 0x2C00's halfword, when its low nibble is 1..5
    /* 0x44E */ u8                    field_44E; // set while the enemy carries status flag 4/8
    /* 0x44F */ u8                    field_44F;
    /* 0x450 */ byte                  pad_450[0x1];
    /* 0x451 */ u8                    field_451; // 1 skips madChaserDrawLimbShadow part-pair colour
    /* 0x452 */ byte                  pad_452[0x2];
} Actor104400Work;
STATIC_ASSERT_SIZEOF(Actor104400Work, 0x454);

extern u8            gMadChaserAnimStance[];  // per animation id (1-based): the value to put in `field_44F`
extern u8            gMadChaserSettleAnims[]; // per animation id (1-based): the animation to follow it
extern EnemyParams   gMadChaserEnemyParams;   // the main enemy's `Enemy::param` record
extern AnimationSet* gMadChaserAnimBank[21];  // animation bank handed to `func_800B3F84`
// Typed callback views for the task message dispatcher.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s16, VECTOR3*);
        void (*call1)(Task*, s32, ActorCommand* request);
    } handler;
} Actor04400RecoveredMsgEntry;
STATIC_ASSERT_SIZEOF(Actor04400RecoveredMsgEntry, 8);

extern Actor04400RecoveredMsgEntry gMadChaserMsgTable[3]; // stored into `Task::msgTable` by madChaserSpawn
static const TaskFuncTable3        Actor04400_D00070;     // dispatcher table Actor04400_Fn06ACC copies onto its stack
static const TaskFuncTable3        Actor04400_D0007C;     // dispatcher table Actor04400_Fn06870 copies onto its stack
static const TaskFuncTable5        Actor04400_D00088;     // dispatcher table Actor04400_Fn068F8 copies onto its stack
static const TaskFuncTable5        Actor04400_D0009C;     // dispatcher table Actor04400_Fn06964 copies onto its stack
static const TaskFuncTable3        Actor04400_D00150;     // dispatcher table Actor04400_Fn07CF0 copies onto its stack
static const TaskFuncTable3        Actor04400_D0015C;     // dispatcher table Actor04400_Fn07D78 copies onto its stack
static const TaskFuncTable4        Actor04400_D00174;     // dispatcher table Actor04400_Fn07F04 copies onto its stack
static const TaskFuncTable6        Actor04400_D001AC;     // dispatcher table Actor04400_Fn06B50 copies onto its stack

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`): the angle is a `long`,
/// so a negated angle is passed without re-truncation to 16 bits.

static void Actor04400_Fn00F7C(Task* arg0);
static void Actor04400_Fn02E8C(Task* arg0);
static void Actor04400_Fn03538(Task* arg0);
static void Actor04400_Fn03F8C(Task* arg0);
static void Actor04400_Fn05260(Task* arg0);
static void Actor04400_Fn05DE0(Task* arg0);
static void Actor04400_Fn05FC8(Task* arg0);
static void Actor04400_Fn062D4(Task* arg0);
static void Actor04400_Fn0674C(Task* arg0);
static void Actor04400_Fn06834(Task* arg0);
static void Actor04400_Fn06848(Task* arg0);
static void Actor04400_Fn0685C(Task* arg0);
static void Actor04400_Fn06870(Task* arg0);
static void Actor04400_Fn068F8(Task* arg0);
static void Actor04400_Fn06964(Task* arg0);
static void Actor04400_Fn069D0(Task* arg0);
static void Actor04400_Fn06A24(Task* arg0);
static void Actor04400_Fn06A78(Task* arg0);
static void Actor04400_Fn06ACC(Task* arg0);
static void Actor04400_Fn06B50(Task* arg0);
static void Actor04400_Fn06BC4(Task* arg0);
static void Actor04400_Fn0710C(Task* arg0);
static void Actor04400_Fn073C8(Task* arg0);
static void Actor04400_Fn07878(Task* arg0);
static void Actor04400_Fn07890(Task* arg0);
static void Actor04400_Fn07968(Task* arg0);
static void Actor04400_Fn07CF0(Task* arg0);
static void Actor04400_Fn07D78(Task* arg0);
static void Actor04400_Fn07E74(Task* arg0);
static void Actor04400_Fn07F04(Task* arg0);
static void Actor04400_Fn089C0(Task* arg0);
static void Actor04400_Fn08A9C(Task* arg0);
static void Actor04400_Fn08AA4(Task* arg0);
static void Actor04400_Fn08C08(Task* arg0);
static void Actor04400_Fn08DA4(Task* arg0);

/* `D_800678F0` selects the model stream the next `Gp_SpawnEff` copies into
 * its effect's `TmdObject`. Declared as a one-element array so GCC 2.8.1
 * cannot treat the store as a non-aliasing scalar and sink it past the
 * `TmdObject` loads. */
extern void*     D_800678F0[1];
extern TmdSource gMadChaserChunkModel0;
extern TmdSource gMadChaserChunkModel1;
extern TmdSource gMadChaserChunkModel2;

static const TaskFuncTable3 Actor04400_D00070;
static const TaskFuncTable3 Actor04400_D0007C;
static const TaskFuncTable5 Actor04400_D00088;
static const TaskFuncTable5 Actor04400_D0009C;
static const TaskFuncTable3 Actor04400_D00150;
static const TaskFuncTable3 Actor04400_D0015C;
static const TaskFuncTable4 Actor04400_D00174;
static const TaskFuncTable6 Actor04400_D001AC;

extern TmdSource Actor04400_D0D2F0;
void             Actor04400_Fn06658(Task*);

void Actor04400_Fn0648C(Task*, s32, ActorCommand* request);
void Actor04400_Fn064EC(Task*, s16, VECTOR3*);
void Actor04400_Fn06658(Task*);
void Actor04400_Fn066DC(Task*);

TmdBone Actor04400_D08E14[1] = {
#include "assets/actor_104400_model_098FC_skeleton.inc"
};

u32 Actor04400_D08E38[1] = {
#include "assets/actor_104400_model_098FC_partVerts.inc"
};

SVECTOR Actor04400_D08E3C[48] = {
#include "assets/actor_104400_model_098FC_verts.inc"
};

SVECTOR Actor04400_D08FBC[53] = {
#include "assets/actor_104400_model_098FC_normals.inc"
};

u32 Actor04400_D09164[486] = {
#include "assets/actor_104400_model_098FC_stream.inc"
};

TmdSource gMadChaserChunkModel0 = {
    0,
    3260,
    0,
    1,
    Actor04400_D08E38,
    Actor04400_D08E3C,
    Actor04400_D08FBC,
    Actor04400_D08E14,
    Actor04400_D09164,
};

TmdBone Actor04400_D09920[1] = {
#include "assets/actor_104400_model_09FA0_skeleton.inc"
};

u32 Actor04400_D09944[1] = {
#include "assets/actor_104400_model_09FA0_partVerts.inc"
};

SVECTOR Actor04400_D09948[28] = {
#include "assets/actor_104400_model_09FA0_verts.inc"
};

SVECTOR Actor04400_D09A28[37] = {
#include "assets/actor_104400_model_09FA0_normals.inc"
};

u32 Actor04400_D09B50[276] = {
#include "assets/actor_104400_model_09FA0_stream.inc"
};

TmdSource gMadChaserChunkModel1 = {
    0,
    1828,
    0,
    1,
    Actor04400_D09944,
    Actor04400_D09948,
    Actor04400_D09A28,
    Actor04400_D09920,
    Actor04400_D09B50,
};

TmdBone Actor04400_D09FC4[1] = {
#include "assets/actor_104400_model_0A510_skeleton.inc"
};

u32 Actor04400_D09FE8[1] = {
#include "assets/actor_104400_model_0A510_partVerts.inc"
};

SVECTOR Actor04400_D09FEC[25] = {
#include "assets/actor_104400_model_0A510_verts.inc"
};

SVECTOR Actor04400_D0A0B4[32] = {
#include "assets/actor_104400_model_0A510_normals.inc"
};

u32 Actor04400_D0A1B4[215] = {
#include "assets/actor_104400_model_0A510_stream.inc"
};

TmdSource gMadChaserChunkModel2 = {
    0,
    1448,
    0,
    1,
    Actor04400_D09FE8,
    Actor04400_D09FEC,
    Actor04400_D0A0B4,
    Actor04400_D09FC4,
    Actor04400_D0A1B4,
};

TmdBone Actor04400_D0A534[9] = {
#include "assets/actor_104400_model_0D2F0_skeleton.inc"
};

u32 Actor04400_D0A678[9] = {
#include "assets/actor_104400_model_0D2F0_partVerts.inc"
};

SVECTOR Actor04400_D0A69C[160] = {
#include "assets/actor_104400_model_0D2F0_verts.inc"
};

SVECTOR Actor04400_D0AB9C[206] = {
#include "assets/actor_104400_model_0D2F0_normals.inc"
};

u32 Actor04400_D0B20C[2105] = {
#include "assets/actor_104400_model_0D2F0_stream.inc"
};

TmdSource Actor04400_D0D2F0 = {
    0,
    11212,
    3088,
    9,
    Actor04400_D0A678,
    Actor04400_D0A69C,
    Actor04400_D0AB9C,
    Actor04400_D0A534,
    Actor04400_D0B20C,
};

DamageAttack Actor04400_D0D314[1] = {
    { 22, 0 },
};

EnemyParams gMadChaserEnemyParams = { Actor04400_D0D314, 110, 20, 40, 1, 100, 10, 100, 0 };

AnimationPackedPose Actor04400_D0D328[6] = {
#include "assets/actor_104400_animation_0D554_bank1.inc"
};

AnimationPackedRotation Actor04400_D0D370[39] = {
#include "assets/actor_104400_animation_0D554_bank4.inc"
};

AnimationRecord Actor04400_D0D40C[77] = {
#include "assets/actor_104400_animation_0D554_records.inc"
};

u16 Actor04400_D0D540[10] = {
#include "assets/actor_104400_animation_0D554_indices.inc"
};

AnimationSet Actor04400_D0D554 = {
    Actor04400_D0D40C,
    Actor04400_D0D540,
    { NULL, Actor04400_D0D328, NULL, NULL, Actor04400_D0D370, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0D57C[3] = {
#include "assets/actor_104400_animation_0D6F8_bank1.inc"
};

AnimationPackedRotation Actor04400_D0D5A0[21] = {
#include "assets/actor_104400_animation_0D6F8_bank4.inc"
};

AnimationRecord Actor04400_D0D5F4[60] = {
#include "assets/actor_104400_animation_0D6F8_records.inc"
};

u16 Actor04400_D0D6E4[10] = {
#include "assets/actor_104400_animation_0D6F8_indices.inc"
};

AnimationSet Actor04400_D0D6F8 = {
    Actor04400_D0D5F4,
    Actor04400_D0D6E4,
    { NULL, Actor04400_D0D57C, NULL, NULL, Actor04400_D0D5A0, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0D720[20] = {
#include "assets/actor_104400_animation_0DBCC_bank1.inc"
};

AnimationPackedRotation Actor04400_D0D810[99] = {
#include "assets/actor_104400_animation_0DBCC_bank4.inc"
};

AnimationRecord Actor04400_D0D99C[135] = {
#include "assets/actor_104400_animation_0DBCC_records.inc"
};

u16 Actor04400_D0DBB8[10] = {
#include "assets/actor_104400_animation_0DBCC_indices.inc"
};

AnimationSet Actor04400_D0DBCC = {
    Actor04400_D0D99C,
    Actor04400_D0DBB8,
    { NULL, Actor04400_D0D720, NULL, NULL, Actor04400_D0D810, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0DBF4[25] = {
#include "assets/actor_104400_animation_0E150_bank1.inc"
};

AnimationPackedRotation Actor04400_D0DD20[110] = {
#include "assets/actor_104400_animation_0E150_bank4.inc"
};

AnimationRecord Actor04400_D0DED8[153] = {
#include "assets/actor_104400_animation_0E150_records.inc"
};

u16 Actor04400_D0E13C[10] = {
#include "assets/actor_104400_animation_0E150_indices.inc"
};

AnimationSet Actor04400_D0E150 = {
    Actor04400_D0DED8,
    Actor04400_D0E13C,
    { NULL, Actor04400_D0DBF4, NULL, NULL, Actor04400_D0DD20, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0E178[2] = {
#include "assets/actor_104400_animation_0E250_bank1.inc"
};

AnimationPackedRotation Actor04400_D0E190[7] = {
#include "assets/actor_104400_animation_0E250_bank4.inc"
};

AnimationRecord Actor04400_D0E1AC[36] = {
#include "assets/actor_104400_animation_0E250_records.inc"
};

u16 Actor04400_D0E23C[10] = {
#include "assets/actor_104400_animation_0E250_indices.inc"
};

AnimationSet Actor04400_D0E250 = {
    Actor04400_D0E1AC,
    Actor04400_D0E23C,
    { NULL, Actor04400_D0E178, NULL, NULL, Actor04400_D0E190, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0E278[2] = {
#include "assets/actor_104400_animation_0E350_bank1.inc"
};

AnimationPackedRotation Actor04400_D0E290[7] = {
#include "assets/actor_104400_animation_0E350_bank4.inc"
};

AnimationRecord Actor04400_D0E2AC[36] = {
#include "assets/actor_104400_animation_0E350_records.inc"
};

u16 Actor04400_D0E33C[10] = {
#include "assets/actor_104400_animation_0E350_indices.inc"
};

AnimationSet Actor04400_D0E350 = {
    Actor04400_D0E2AC,
    Actor04400_D0E33C,
    { NULL, Actor04400_D0E278, NULL, NULL, Actor04400_D0E290, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0E378[24] = {
#include "assets/actor_104400_animation_0E780_bank1.inc"
};

AnimationPackedRotation Actor04400_D0E498[69] = {
#include "assets/actor_104400_animation_0E780_bank4.inc"
};

AnimationRecord Actor04400_D0E5AC[112] = {
#include "assets/actor_104400_animation_0E780_records.inc"
};

u16 Actor04400_D0E76C[10] = {
#include "assets/actor_104400_animation_0E780_indices.inc"
};

AnimationSet Actor04400_D0E780 = {
    Actor04400_D0E5AC,
    Actor04400_D0E76C,
    { NULL, Actor04400_D0E378, NULL, NULL, Actor04400_D0E498, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0E7A8[22] = {
#include "assets/actor_104400_animation_0EC24_bank1.inc"
};

AnimationPackedRotation Actor04400_D0E8B0[87] = {
#include "assets/actor_104400_animation_0EC24_bank4.inc"
};

AnimationRecord Actor04400_D0EA0C[129] = {
#include "assets/actor_104400_animation_0EC24_records.inc"
};

u16 Actor04400_D0EC10[10] = {
#include "assets/actor_104400_animation_0EC24_indices.inc"
};

AnimationSet Actor04400_D0EC24 = {
    Actor04400_D0EA0C,
    Actor04400_D0EC10,
    { NULL, Actor04400_D0E7A8, NULL, NULL, Actor04400_D0E8B0, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0EC4C[14] = {
#include "assets/actor_104400_animation_0F17C_bank1.inc"
};

AnimationPackedRotation Actor04400_D0ECF4[99] = {
#include "assets/actor_104400_animation_0F17C_bank4.inc"
};

AnimationRecord Actor04400_D0EE80[186] = {
#include "assets/actor_104400_animation_0F17C_records.inc"
};

u16 Actor04400_D0F168[10] = {
#include "assets/actor_104400_animation_0F17C_indices.inc"
};

AnimationSet Actor04400_D0F17C = {
    Actor04400_D0EE80,
    Actor04400_D0F168,
    { NULL, Actor04400_D0EC4C, NULL, NULL, Actor04400_D0ECF4, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0F1A4[8] = {
#include "assets/actor_104400_animation_0F430_bank1.inc"
};

AnimationPackedRotation Actor04400_D0F204[54] = {
#include "assets/actor_104400_animation_0F430_bank4.inc"
};

AnimationRecord Actor04400_D0F2DC[80] = {
#include "assets/actor_104400_animation_0F430_records.inc"
};

u16 Actor04400_D0F41C[10] = {
#include "assets/actor_104400_animation_0F430_indices.inc"
};

AnimationSet Actor04400_D0F430 = {
    Actor04400_D0F2DC,
    Actor04400_D0F41C,
    { NULL, Actor04400_D0F1A4, NULL, NULL, Actor04400_D0F204, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0F458[7] = {
#include "assets/actor_104400_animation_0F6C4_bank1.inc"
};

AnimationPackedRotation Actor04400_D0F4AC[50] = {
#include "assets/actor_104400_animation_0F6C4_bank4.inc"
};

AnimationRecord Actor04400_D0F574[79] = {
#include "assets/actor_104400_animation_0F6C4_records.inc"
};

u16 Actor04400_D0F6B0[10] = {
#include "assets/actor_104400_animation_0F6C4_indices.inc"
};

AnimationSet Actor04400_D0F6C4 = {
    Actor04400_D0F574,
    Actor04400_D0F6B0,
    { NULL, Actor04400_D0F458, NULL, NULL, Actor04400_D0F4AC, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0F6EC[7] = {
#include "assets/actor_104400_animation_0F974_bank1.inc"
};

AnimationPackedRotation Actor04400_D0F740[53] = {
#include "assets/actor_104400_animation_0F974_bank4.inc"
};

AnimationRecord Actor04400_D0F814[83] = {
#include "assets/actor_104400_animation_0F974_records.inc"
};

u16 Actor04400_D0F960[10] = {
#include "assets/actor_104400_animation_0F974_indices.inc"
};

AnimationSet Actor04400_D0F974 = {
    Actor04400_D0F814,
    Actor04400_D0F960,
    { NULL, Actor04400_D0F6EC, NULL, NULL, Actor04400_D0F740, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0F99C[6] = {
#include "assets/actor_104400_animation_0FB88_bank1.inc"
};

AnimationPackedRotation Actor04400_D0F9E4[42] = {
#include "assets/actor_104400_animation_0FB88_bank4.inc"
};

AnimationRecord Actor04400_D0FA8C[58] = {
#include "assets/actor_104400_animation_0FB88_records.inc"
};

u16 Actor04400_D0FB74[10] = {
#include "assets/actor_104400_animation_0FB88_indices.inc"
};

AnimationSet Actor04400_D0FB88 = {
    Actor04400_D0FA8C,
    Actor04400_D0FB74,
    { NULL, Actor04400_D0F99C, NULL, NULL, Actor04400_D0F9E4, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0FBB0[2] = {
#include "assets/actor_104400_animation_0FCA8_bank1.inc"
};

AnimationPackedRotation Actor04400_D0FBC8[11] = {
#include "assets/actor_104400_animation_0FCA8_bank4.inc"
};

AnimationRecord Actor04400_D0FBF4[40] = {
#include "assets/actor_104400_animation_0FCA8_records.inc"
};

u16 Actor04400_D0FC94[10] = {
#include "assets/actor_104400_animation_0FCA8_indices.inc"
};

AnimationSet Actor04400_D0FCA8 = {
    Actor04400_D0FBF4,
    Actor04400_D0FC94,
    { NULL, Actor04400_D0FBB0, NULL, NULL, Actor04400_D0FBC8, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0FCD0[4] = {
#include "assets/actor_104400_animation_0FDE4_bank1.inc"
};

AnimationPackedRotation Actor04400_D0FD00[18] = {
#include "assets/actor_104400_animation_0FDE4_bank4.inc"
};

AnimationRecord Actor04400_D0FD48[34] = {
#include "assets/actor_104400_animation_0FDE4_records.inc"
};

u16 Actor04400_D0FDD0[10] = {
#include "assets/actor_104400_animation_0FDE4_indices.inc"
};

AnimationSet Actor04400_D0FDE4 = {
    Actor04400_D0FD48,
    Actor04400_D0FDD0,
    { NULL, Actor04400_D0FCD0, NULL, NULL, Actor04400_D0FD00, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D0FE0C[10] = {
#include "assets/actor_104400_animation_0FFE0_bank1.inc"
};

AnimationPackedRotation Actor04400_D0FE84[31] = {
#include "assets/actor_104400_animation_0FFE0_bank4.inc"
};

AnimationRecord Actor04400_D0FF00[51] = {
#include "assets/actor_104400_animation_0FFE0_records.inc"
};

u16 Actor04400_D0FFCC[10] = {
#include "assets/actor_104400_animation_0FFE0_indices.inc"
};

AnimationSet Actor04400_D0FFE0 = {
    Actor04400_D0FF00,
    Actor04400_D0FFCC,
    { NULL, Actor04400_D0FE0C, NULL, NULL, Actor04400_D0FE84, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D10008[16] = {
#include "assets/actor_104400_animation_1041C_bank1.inc"
};

AnimationPackedRotation Actor04400_D100C8[83] = {
#include "assets/actor_104400_animation_1041C_bank4.inc"
};

AnimationRecord Actor04400_D10214[125] = {
#include "assets/actor_104400_animation_1041C_records.inc"
};

u16 Actor04400_D10408[10] = {
#include "assets/actor_104400_animation_1041C_indices.inc"
};

AnimationSet Actor04400_D1041C = {
    Actor04400_D10214,
    Actor04400_D10408,
    { NULL, Actor04400_D10008, NULL, NULL, Actor04400_D100C8, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D10444[5] = {
#include "assets/actor_104400_animation_105A8_bank1.inc"
};

AnimationPackedRotation Actor04400_D10480[26] = {
#include "assets/actor_104400_animation_105A8_bank4.inc"
};

AnimationRecord Actor04400_D104E8[43] = {
#include "assets/actor_104400_animation_105A8_records.inc"
};

u16 Actor04400_D10594[10] = {
#include "assets/actor_104400_animation_105A8_indices.inc"
};

AnimationSet Actor04400_D105A8 = {
    Actor04400_D104E8,
    Actor04400_D10594,
    { NULL, Actor04400_D10444, NULL, NULL, Actor04400_D10480, NULL, NULL, NULL },
};

AnimationPackedPose Actor04400_D105D0[5] = {
#include "assets/actor_104400_animation_10750_bank1.inc"
};

AnimationPackedRotation Actor04400_D1060C[30] = {
#include "assets/actor_104400_animation_10750_bank4.inc"
};

AnimationRecord Actor04400_D10684[46] = {
#include "assets/actor_104400_animation_10750_records.inc"
};

u16 Actor04400_D1073C[10] = {
#include "assets/actor_104400_animation_10750_indices.inc"
};

AnimationSet Actor04400_D10750 = {
    Actor04400_D10684,
    Actor04400_D1073C,
    { NULL, Actor04400_D105D0, NULL, NULL, Actor04400_D1060C, NULL, NULL, NULL },
};

AnimationSet* gMadChaserAnimBank[21] = {
    NULL,
    &Actor04400_D0D554,
    &Actor04400_D0D6F8,
    &Actor04400_D0DBCC,
    &Actor04400_D0E150,
    &Actor04400_D0E250,
    &Actor04400_D0E350,
    &Actor04400_D0E780,
    &Actor04400_D0EC24,
    &Actor04400_D0F17C,
    &Actor04400_D0F430,
    &Actor04400_D0F6C4,
    &Actor04400_D0F974,
    &Actor04400_D0FB88,
    &Actor04400_D0FCA8,
    &Actor04400_D0FDE4,
    &Actor04400_D0FFE0,
    &Actor04400_D1041C,
    &Actor04400_D105A8,
    &Actor04400_D10750,
    NULL,
};

Actor04400RecoveredMsgEntry gMadChaserMsgTable[3] = {
    { 2004, { .call0 = Actor04400_Fn064EC } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = Actor04400_Fn0648C } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc Actor04400_D107E4 = { { { TASK_BODY_TMD, 96 } }, Actor04400_Fn066DC, { .model = &Actor04400_D0D2F0 } };

TaskDesc Actor04400_D107F0 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_MODEL_BUFFER), 96 } }, Actor04400_Fn06658, { .model = &Actor04400_D0D2F0 } };

TaskDesc Actor04400_D107FC = { { { TASK_BODY_COORD, 96 } }, taskKill, { .value = 0 } };

TaskDesc Actor04400_D10808 = { { { TASK_BODY_TMD, 96 } }, Actor04400_Fn06658, { .model = &Actor04400_D0D2F0 } };

u8 gMadChaserAnimStance[20] = {
    0,
    1,
    0,
    1,
    0,
    1,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    1,
    0,
};

u8 gMadChaserSettleAnims[19] = { 5, 6, 5, 6, 5, 6, 6, 5, 5, 5, 6, 6, 5, 5, 5, 6, 5, 6, 5 };

#include "../../shared/mad_chaser_inlines.inc.c"

static __inline__ s16  Actor04400_TakeHit(Task* arg0);
static __inline__ s16  Actor04400_TakeHit3(Task* arg0);
static __inline__ void Actor04400_UpdateRotation(Task* arg0);
static __inline__ s16  Actor04400_PickStep(s16 step, s16 push);
static void            Actor04400_Fn03390(Task* arg0);

/// Message 0x2C00 (see `field_44C`) consumes the message and restarts the
/// state machine: low nibble 2 enters state 3 at state index 10 unless
/// `field_438` is set, low nibble 3 enters state 7. Returns 1 when it did, so
/// the caller skips this frame's state handler.
///
/// Each arm has to `return 1` on its own, with `return 0` after them: that
/// leaves a `hit = 0` block between the second arm and the join, so jump2
/// cannot cross-jump the first arm's `field_422` store into the second's
/// (dbr later steals the `hit = 0` into the branch delay slots and the block
/// disappears). A flag set to 0 up front and to 1 in each arm cross-jumps.
static __inline__ s16 Actor04400_TakeHit(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    Actor104400Work* w2;

    if ((work->field_44C & 0xF) == 2) {
        if (work->field_438 == 0) {
            work->field_44C = 0;
            madChaserEnterState(arg0, 3);
            w2            = (Actor104400Work*)arg0->work;
            w2->field_420 = 10;
            w2->field_422 = 0;
            return 1;
        }
    } else if ((work->field_44C & 0xF) == 3) {
        work->field_44C = 0;
        madChaserEnterState(arg0, 7);
        return 1;
    }
    return 0;
}

/// A narrower `Actor04400_TakeHit`: only low nibble 3 of
/// message 0x2C00 counts, consuming it into task state 7 with a fresh state
/// machine. Returns 1 when it did, so the caller skips this frame's handler.
static __inline__ s16 Actor04400_TakeHit3(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    s16              hit  = 0;
    Actor104400Work* w2;

    if ((work->field_44C & 0xF) == 3) {
        hit             = 1;
        work->field_44C = 0;
        arg0->state     = 7;
        w2              = (Actor104400Work*)arg0->work;
        w2->field_420   = 0;
        w2->field_422   = 0;
    }
    return hit;
}

/// Wraps the pitch / heading / roll at 0x78..0x7C to 12 bits and rebuilds the
/// model root's rotation from them (Z, then X, then the heading) in a matrix
/// taken off the scratch stack.
static __inline__ void Actor04400_UpdateRotation(Task* arg0)
{
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    MATRIX*          m     = (MATRIX*)(SCRATCH_STACK_CURSOR(u8) - 0x20);
    GfxCoord*        coord = arg0->extra.tmd->coords;
    MATRIX*          dst;

    work->field_78              &= 0xFFF;
    work->field_7A              &= 0xFFF;
    work->field_7C              &= 0xFFF;
    MATRIX_PAIR(m, 0, 0)         = 0x1000;
    MATRIX_PAIR(m, 0, 2)         = 0;
    MATRIX_PAIR(m, 1, 1)         = 0x1000;
    MATRIX_PAIR(m, 2, 0)         = 0;
    m->m[2][2]                   = 0x1000;
    SCRATCH_STACK_CURSOR(MATRIX) = m;
    RotMatrixZ(work->field_7C, m);
    RotMatrixX(work->field_78, m);
    RotMatrixY(work->field_7A, m);
    dst          = &coord->coord;
    dst->m[0][0] = m->m[0][0];
    dst->m[0][1] = m->m[0][1];
    dst->m[0][2] = m->m[0][2];
    dst->m[1][0] = m->m[1][0];
    dst->m[1][1] = m->m[1][1];
    dst->m[1][2] = m->m[1][2];
    dst->m[2][0] = m->m[2][0];
    dst->m[2][1] = m->m[2][1];
    SCRATCH_STACK_RELEASE_BYTES(0x20);
    dst->m[2][2] = m->m[2][2];
}

/// Picks the per-axis step: the collision `step` when there is one and the
/// push-out opposes it, otherwise whichever of the two is larger in the
/// direction of `step`.
static __inline__ s16 Actor04400_PickStep(s16 step, s16 push)
{
    if (step == 0) {
        return push;
    }
    if ((step > 0 && push < 0) || (step < 0 && push > 0)) {
        return step;
    }
    if (step > 0) {
        if (push < step) {
            return step;
        }
        return push;
    }
    if (push < step) {
        return push;
    }
    return step;
}

/// Task-state handlers of the first enemy form, dispatched by
/// `Actor04400_Fn066DC` on `Task::state`.
static const TaskFuncTable6 Actor04400_D00004 = { {
    madChaserSpawn,
    Actor04400_Fn03538,
    madChaserDangleFrame,
    Actor04400_Fn00F7C,
    Actor04400_Fn02E8C,
    Actor04400_Fn0674C,
} };

/// Task-state handlers of the second enemy form, dispatched by
/// `Actor04400_Fn06658` on `Task::state`.
static const TaskFuncTable10 Actor04400_D0001C = { {
    madChaserSpawnHidden,
    Actor04400_Fn03538,
    madChaserDangleFrame,
    Actor04400_Fn00F7C,
    Actor04400_Fn02E8C,
    Actor04400_Fn0674C,
    Actor04400_Fn03F8C,
    Actor04400_Fn062D4,
    Actor04400_Fn05DE0,
    Actor04400_Fn05FC8,
} };

/// State handlers `Actor04400_Fn00F7C` dispatches by `field_420`.
static const TaskFuncTable11 Actor04400_D00044 = { {
    Actor04400_Fn06834,
    Actor04400_Fn06848,
    Actor04400_Fn0685C,
    Actor04400_Fn06870,
    Actor04400_Fn068F8,
    Actor04400_Fn06964,
    Actor04400_Fn069D0,
    Actor04400_Fn06A24,
    Actor04400_Fn06A78,
    Actor04400_Fn06ACC,
    Actor04400_Fn06B50,
} };

/// Sub-state handlers `Actor04400_Fn06ACC` dispatches by `field_422`.
static const TaskFuncTable3 Actor04400_D00070 = { {
    madChaserKnockdownStart,
    madChaserKnockdownRise,
    madChaserKnockdownEnd,
} };

/// Sub-state handlers `Actor04400_Fn06870` dispatches by `field_422`.
static const TaskFuncTable3 Actor04400_D0007C = { {
    madChaserWalkStart,
    madChaserWalkApproach,
    madChaserWalkFinish,
} };

/// Sub-state handlers `Actor04400_Fn068F8` dispatches by `field_422`.
static const TaskFuncTable5 Actor04400_D00088 = { {
    madChaserStartLeap,
    madChaserLeapAttack,
    madChaserLeapTurnAway,
    madChaserLeapRebound,
    madChaserLeapLand,
} };

/// Sub-state handlers `Actor04400_Fn06964` dispatches by `field_422`.
static const TaskFuncTable5 Actor04400_D0009C = { {
    madChaserAlertCry,
    Actor04400_Fn0710C,
    madChaserAlertRelease,
    madChaserAlertCrouch,
    madChaserAlertSidestep,
} };

/// Sub-state handlers `madChaserDangleState` dispatches by `field_422`.
static const TaskFuncTable4 Actor04400_D000B0 = { {
    Actor04400_Fn073C8,
    madChaserDangleSway,
    madChaserDangleFall,
    madChaserDangleLand,
} };

#include "../../shared/mad_chaser_limb_shadow.inc.c"

#include "../../shared/mad_chaser_spawn_gibs.inc.c"

#include "../../shared/mad_chaser_twist.inc.c"

#include "../../shared/mad_chaser_spawn.inc.c"

#include "../../shared/mad_chaser_spawn_hidden.inc.c"

/// Per-frame callback for the main enemy. In mode 0 it aims at the nearest actor (`madChaserTrackPlayer`), lets
/// a pending hit (`Actor04400_TakeHit`) replace the state handler, eases
/// `field_424` toward zero, rebuilds the root rotation, and then picks the
/// next state: the `field_448` request once dead, state 4 when dead, 8 / 9 for
/// messages 4 / 5 while `field_438` is clear.
static void Actor04400_Fn00F7C(Task* arg0)
{
    Enemy*           enemy = arg0->spawnArg2.pointer;
    TmdObject*       obj   = arg0->extra.tmd;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable11  sp    = Actor04400_D00044;
    s32              cur;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            madChaserTrackPlayer(arg0);
            if (Actor04400_TakeHit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            madChaserTickAnim(arg0);
            cur             = (u16)work->field_424;
            work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
            madChaserTwistSpine(arg0);
            if (work->field_432 == 1) {
                madChaserPinPart(arg0, 6, (SVECTOR3*)&work->field_98);
            }
            Actor04400_UpdateRotation(arg0);
            madChaserApplyContacts(arg0, 0);
            if (work->field_44A != 0) {
                work->field_44A--;
            }
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->hp <= 0) {
                madChaserEnterState(arg0, work->field_448);
            }
            if (work->field_438 == 0 && enemy->hp <= 0) {
                madChaserEnterState(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                madChaserEnterState(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                madChaserEnterState(arg0, 9);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
            madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
            madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            return;
    }
}

#include "../../shared/mad_chaser_walk_start.inc.c"

#include "../../shared/mad_chaser_walk_approach.inc.c"

#include "../../shared/mad_chaser_leap_attack.inc.c"

#include "../../shared/mad_chaser_leap_turn_away.inc.c"

#include "../../shared/mad_chaser_leap_rebound.inc.c"

#include "../../shared/mad_chaser_dangle_frame.inc.c"

#include "../../shared/mad_chaser_dangle_fall.inc.c"

#include "../../shared/mad_chaser_dangle_land.inc.c"

/// Per-frame contact handling for the enemy. Walks the eight contact records: kind 1 (skipped when
/// `arg1` is set) and kind 3 push the model out, kind 2 applies a hit -
/// damage, status effects and the pending state request in `field_448` -
/// unless `field_40E` is still cooling down. Then ticks the status flags,
/// applies `func_800E0C10`'s collision step (snapping back to `field_60` when
/// it reports a conflict) and moves the root by the combined step and
/// push-out.
void madChaserApplyContacts(Task* arg0, s16 arg1)
{
    GpDeltaScratch   delta;
    SVECTOR          push;
    s16              maxX;
    s16              maxZ;
    s16              stepX;
    s16              stepZ;
    u8               blocked;
    Actor104400Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    s16              amount;
    s32              dmg;
    s32              tmp;
    s16              tick;
    s32              i;

    stepZ   = 0;
    maxX    = 0;
    maxZ    = 0;
    stepX   = 0;
    blocked = 0;
    work    = (Actor104400Work*)arg0->work;
    coord   = arg0->extra.tmd->coords;
    enemy   = arg0->spawnArg2.pointer;
    SCRATCH_STACK_RESERVE_BYTES(8);
    work->field_41E = 0;
    for (i = 0; i < 8; i++) {
        switch (work->rec_2EC[i].key.value & 0xFFFF0000) {
            case 0x10000:
                if (arg1 != 0) {
                    break;
                }
            case 0x30000:
                madChaserCalcPush(arg0, coord, &work->rec_2EC[i], &push);
                if (ABS(maxX) < ABS(push.vx)) {
                    maxX = push.vx;
                }
                if (ABS(maxZ) < ABS(push.vz)) {
                    maxZ = push.vz;
                }
                break;
            case 0x20000:
                if (work->field_40E == 0) {
                    work->field_41E = 1;
                    dmg             = Gp_ComputeDamage(work->rec_2EC[i].key.value, work->field_43A, 0, 0);
                    amount          = dmg;
                    work->field_40E = Gp_GetIdParam2(work->rec_2EC[i].key.value);
                    if (Gp_RollEnemyChance(enemy, work->rec_2EC[i].key.value, 0) != 0) {
                        amount = ((u32)dmg << 16) >> 14;
                        Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 0, NULL);
                    }
                    func_800E2C78(enemy, work->rec_2EC[i].key.value, amount, 0);
                    func_800DA6E8(&enemy->node, amount, 0);
                    enemy->hp -= amount;
                    if (enemy->hp < 0) {
                        enemy->hp = 0;
                    }
                    func_800FDB18(Gp_GetIdParam1(work->rec_2EC[i].key.value) & 0xFFFF,
                                  &arg0->extra.tmd->coords[1], NULL, &work->eff_3FC);
                    if (amount >= 0x28) {
                        work->field_448 = 2;
                    } else {
                        work->field_448 = 1;
                    }
                    switch (Gp_GetIdParam0(work->rec_2EC[i].key.value) & 0xFFFF) {
                        case 0:
                            break;
                        case 1:
                            Gp_SetObjFlag1(enemy);
                            break;
                        case 2:
                            Gp_SetObjFlag2(enemy, work->rec_2EC[i].key.value, 0);
                            break;
                        case 3:
                            Gp_SetObjFlag4(enemy, work->rec_2EC[i].key.value, 0);
                            break;
                        case 4:
                            work->field_448 = 4;
                            break;
                        case 5:
                            work->field_448 = 2;
                            break;
                        case 6:
                            work->field_448 = 4;
                            break;
                        case 7:
                            work->field_448 = 2;
                            break;
                        case 8:
                            work->field_448 = 3;
                            break;
                        case 9:
                            work->field_448 = 3;
                            break;
                    }
                } else if ((Gp_GetIdParam1(work->rec_2EC[i].key.value) & 0xFFFF) == 0xD) {
                    func_800FDB18(0xD, &arg0->extra.tmd->coords[1], NULL, &work->eff_3FC);
                }
                break;
        }
    }

    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
        work->field_448       = 5;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->field_448       = 3;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        work->field_44E = 1;
        tmp             = Gp_TickObjFlag4(enemy);
        tick            = tmp;
        if (tick != 0) {
            enemy->hp -= tmp;
            func_800DA6E8(&enemy->node, tick, 0);
            if (enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->field_41E = 1;
            work->field_448 = 2;
        }
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }

    switch (func_800E0C10(work->rec_2EC, &delta, 8, NULL)) {
        case 0:
            break;
        case 1:
            stepZ = delta.vz.halves.integer;
            stepX = delta.vx.word >> 16;
            if (delta.vx.word & 0xFFFF) {
                if (delta.vx.word > 0) {
                    stepX++;
                } else {
                    stepX--;
                }
            }
            if (delta.vz.word & 0xFFFF) {
                if (delta.vz.word > 0) {
                    stepZ++;
                } else {
                    stepZ--;
                }
            }
            break;
        case 2:
            coord->coord.t[0]   = work->field_60.vx;
            coord->coord.t[2]   = work->field_60.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            blocked             = 1;
            break;
    }

    Gp_ClearRec18Occupied(work->rec_2EC);
    if (work->field_43E != 0) {
        work->field_43E--;
    }
    if (work->field_40E > 0) {
        work->field_40E--;
    }
    if (blocked == 0) {
        work->field_80     += Actor04400_PickStep(stepX, maxX >> 3);
        work->field_84     += Actor04400_PickStep(stepZ, maxZ >> 3);
        coord->coord.t[0]  += Actor04400_PickStep(stepX, maxX >> 3);
        coord->coord.t[2]  += Actor04400_PickStep(stepZ, maxZ >> 3);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

#include "../../shared/mad_chaser_tick_anim.inc.c"

#include "../../shared/mad_chaser_bodies.inc.c"

/// State handlers `Actor04400_Fn02E8C` dispatches by `field_420`.
static const TaskFuncTable9 Actor04400_D000EC = { {
    madChaserDeathCry,
    madChaserDeathSettle,
    madChaserDeathWaitAnim,
    madChaserBeginDeath,
    madChaserDeathTurnTranslucent,
    madChaserShrinkWithDust,
    Actor04400_Fn07878,
    Actor04400_Fn07890,
    madChaserBurst,
} };

/// Per-frame callback of the main enemy. `gSceneCombatState.actorControl` 2 hides the model, 0 runs the current state handler
/// (then colours it), 1 only colours it. Unless `field_451` is set, it then
/// runs `madChaserDrawLimbShadow` for three part pairs.
static void Actor04400_Fn02E8C(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable9   sp    = Actor04400_D000EC;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

#include "../../shared/mad_chaser_shrink_dust.inc.c"

#include "../../shared/mad_chaser_track_player.inc.c"

/// State handler: with `field_44F` 1, a pending request 1 while `field_41E`
/// is set queues animation 0xB (kind 2, speed 0x20); otherwise a consumed
/// request wins, and a hit moves to state 3. With `field_44F` clear, a hit
/// calls `madChaserSetAlertHold` and moves to state 5. The request test
/// compares against the constant 1, which CSE folds into the `field_44F`
/// register; writing `== work->field_44F` reloads the byte instead.
static void Actor04400_Fn03390(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (work->field_44F == 1) {
        if (work->field_41E != 0 && work->field_448 == 1) {
            work->field_41C = 0x20;
            work->field_418 = 0xB;
            work->field_414 = 2;
            return;
        }
        if (madChaserTakeRequest(arg0) == 0 && madChaserIsHit(arg0)) {
            madChaserSetStateS16(arg0, 3);
        }
    } else if (madChaserIsHit(arg0)) {
        madChaserSetAlertHold(arg0, 1);
        madChaserSetStateS16(arg0, 5);
    }
}

/// State handlers `Actor04400_Fn03538` dispatches by `field_420`.
static const TaskFuncTable5 Actor04400_D00128 = { {
    Actor04400_Fn07CF0,
    Actor04400_Fn07D78,
    madChaserLurkRiseState,
    Actor04400_Fn07E74,
    Actor04400_Fn07F04,
} };

/// The five-state per-frame callback of the enemy's state machine, the
/// counterpart of `Actor04400_Fn05DE0`. Mode 0 counts `field_442` up, aims
/// (`madChaserTrackPlayer`), lets `Actor04400_TakeHit` replace the handler
/// `field_420` selects from `Actor04400_D00128`, rebuilds the model root
/// rotation through part 0's coordinate, and picks the next state: 4 once the
/// `field_40` hold is empty, 8 / 9 for messages 4 / 5, and 3 after a consumed
/// `field_448` request. Mode 1 recolours from part 1's world position; both
/// clear bit 0x80 of the model flags, which mode 2 sets.
static void Actor04400_Fn03538(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Enemy*           enemy = arg0->spawnArg2.pointer;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable5   sp    = Actor04400_D00128;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            madChaserTrackPlayer(arg0);
            if (Actor04400_TakeHit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            madChaserTickAnim(arg0);
            madChaserTwistSpine(arg0);
            Actor04400_UpdateRotation(arg0);
            madChaserApplyContacts(arg0, 0);
            if (work->field_438 == 0 && enemy->hp <= 0) {
                madChaserEnterState(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                madChaserEnterState(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                madChaserEnterState(arg0, 9);
            } else if (madChaserTakeRequest(arg0)) {
                work->field_438 = 0;
                madChaserEnterState(arg0, 3);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
            madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
            madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}

/// Sub-state handlers `Actor04400_Fn07CF0` dispatches by `field_422`.
static const TaskFuncTable3 Actor04400_D00150 = { {
    madChaserStartHold,
    madChaserLurkWait,
    madChaserLurkIdleEnd,
} };

/// Sub-state handlers `Actor04400_Fn07D78` dispatches by `field_422`.
static const TaskFuncTable3 Actor04400_D0015C = { {
    madChaserLurkCrouch,
    madChaserLurkRaise,
    madChaserLurkLookAround,
} };

/// Sub-state handlers `Actor04400_Fn07E74` dispatches by `field_422`.
static const TaskFuncTable3 Actor04400_D00168 = { {
    madChaserStartAlert,
    madChaserLurkBrace,
    madChaserLurkSidestepToCombat,
} };

/// Sub-state handlers `Actor04400_Fn07F04` dispatches by `field_422`.
static const TaskFuncTable4 Actor04400_D00174 = { {
    madChaserLurkShiftStart,
    madChaserLurkShiftBrace,
    madChaserLurkSidestepRight,
    madChaserLurkSidestepLeft,
} };

/// State handlers `Actor04400_Fn03F8C` dispatches by `field_420`.
static const TaskFuncTable10 Actor04400_D00184 = { {
    madChaserEmergeAtSpot,
    madChaserEmergeBackflip,
    madChaserEmergeHopForward,
    madChaserCreepUntilHit,
    madChaserEmergeArcBack,
    madChaserEmergeHopBack,
    madChaserEmergeBackOff,
    madChaserEmergeHighArc,
    madChaserEmergeFlipOver,
    Actor04400_Fn05260,
} };

/// Sub-state handlers `Actor04400_Fn06B50` dispatches by `field_422`.
static const TaskFuncTable6 Actor04400_D001AC = { {
    madChaserPullStart,
    madChaserPullReact,
    madChaserPulledStruggle,
    madChaserPulledIn,
    madChaserPulledLimp,
    madChaserPulledIn,
} };

/// State handlers `Actor04400_Fn05DE0` dispatches by `field_420`.
static const TaskFuncTable5 Actor04400_D001C4 = { {
    madChaserDeathCryUnlink,
    madChaserDeathSettleQuiet,
    Actor04400_Fn089C0,
    madChaserDropBodies,
    Actor04400_Fn08A9C,
} };

/// State handlers `Actor04400_Fn05FC8` dispatches by `field_420`.
static const TaskFuncTable7 Actor04400_D001D8 = { {
    Actor04400_Fn08AA4,
    madChaserDeathSettleQuiet,
    Actor04400_Fn089C0,
    madChaserBeginShrink,
    Actor04400_Fn08C08,
    madChaserShrink,
    Actor04400_Fn08DA4,
} };

#include "../../shared/mad_chaser_lurk_look.inc.c"

#include "../../shared/mad_chaser_lurk_sidestep_combat.inc.c"

#include "../../shared/mad_chaser_lurk_sidestep_right.inc.c"

#include "../../shared/mad_chaser_lurk_sidestep_left.inc.c"

/// Per-frame callback of the enemy this overlay drives, and the ten-state
/// counterpart of `Actor04400_Fn05DE0`: its handlers come from the
/// `Actor04400_D00184` table copied onto the stack, and in mode 0 a pending hit
/// (`Actor04400_TakeHit3`) replaces this frame's handler. `madChaserTickAnim`
/// advances the animation, the root rotation is rebuilt from 0x78..0x7C, and
/// `madChaserApplyContacts` applies the frame's motion before the root coordinate
/// is marked dirty. Mode 1 re-pushes the model's second coordinate for
/// `Gp_UpdateActorColor` and rebuilds the part-pair colour quads while
/// `field_451` is clear. `gSceneCombatState.actorControl` short-circuits both: 1 runs mode 1 only,
/// 2 hides the model instead.
static void Actor04400_Fn03F8C(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable10  sp    = Actor04400_D00184;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            if (Actor04400_TakeHit3(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            madChaserTickAnim(arg0);
            Actor04400_UpdateRotation(arg0);
            madChaserApplyContacts(arg0, 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

#include "../../shared/mad_chaser_emerge_at_spot.inc.c"

#include "../../shared/mad_chaser_emerge_backflip.inc.c"

#include "../../shared/mad_chaser_emerge_hop_forward.inc.c"

#include "../../shared/mad_chaser_creep.inc.c"

#include "../../shared/mad_chaser_emerge_arc_back.inc.c"

#include "../../shared/mad_chaser_emerge_hop_back.inc.c"

#include "../../shared/mad_chaser_emerge_back_off.inc.c"

#include "../../shared/mad_chaser_emerge_high_arc.inc.c"

#include "../../shared/mad_chaser_emerge_flip_over.inc.c"

/// A further copy, under this file's own name.
#define madChaserCreepUntilHit Actor04400_Fn05260
#include "../../shared/mad_chaser_creep.inc.c"
#undef madChaserCreepUntilHit

#include "../../shared/mad_chaser_pulled_struggle.inc.c"

#include "../../shared/mad_chaser_pulled_in.inc.c"

#include "../../shared/mad_chaser_pulled_limp.inc.c"

/// The per-frame callback the actor's AI states are dispatched from: state 0
/// counts `field_442` up, runs the handler `field_420` selects from
/// `Actor04400_D001C4` and spawns effect 3 on the model's second coordinate
/// part every 32 frames, then falls into state 1, which re-pushes that
/// coordinate's world position for `Gp_UpdateActorColor` and rebuilds the
/// part-pair colour quads while `field_451` is clear. `gSceneCombatState.actorControl` short-
/// circuits both: nonzero runs state 1 only, 2 hides the model instead.
static void Actor04400_Fn05DE0(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable5   sp    = Actor04400_D001C4;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &arg0->extra.tmd->coords[1], NULL, &work->eff_3FC);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}

/// `Actor04400_Fn05DE0`'s seven-state counterpart, and the only difference is
/// the exit: mode 1 ends without clearing bit 0x80 of the model flags, which
/// leaves `obj` live only as far as mode 2 and lets it stay in `$a0` instead of
/// a saved register.
static void Actor04400_Fn05FC8(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable7   sp    = Actor04400_D001D8;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &arg0->extra.tmd->coords[1], NULL, &work->eff_3FC);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

#include "../../shared/mad_chaser_sound_bank.inc.c"

/// Walks the death sequence's two-state handler table on the work block's
/// state index.
static void Actor04400_Fn062D4(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        madChaserVanish,
        madChaserVanishFree,
    };

    states[(s16)work->field_420](arg0);
}

/// Once bit 7 of `gSceneCombatState.madChaserAlertOwner` is set, puts the task in state 3 with
/// its state machine at state 5 and returns 1; otherwise returns 0.
s16 madChaserJoinAlert(Task* arg0)
{
    if ((s8)gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_ALERT_CLAIMED) {
        madChaserEnterState(arg0, 3);
        madChaserSetStateS16(arg0, 5);
        return 1;
    }
    return 0;
}

#include "../../shared/mad_chaser_alert_hold.inc.c"

/// While `field_41E` is 1, consumes the pending request in `field_448`:
/// requests 1..5 jump the state machine to states 6, 7, 8, 7 and 9 at
/// sub-state 0, anything else is just cleared. Returns 1 when `field_41E` is 1
/// and 0 otherwise. Each case reloads the work block through its own local;
/// one shared local lands in `$a0` instead of `$v1`.
s32 madChaserTakeHitRequest(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (work->field_41E == 1) {
        switch ((s16)(work->field_448 - 1)) {
            case 0: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 6;
                w->field_422       = 0;
                break;
            }
            case 1: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 7;
                w->field_422       = 0;
                break;
            }
            case 2: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 8;
                w->field_422       = 0;
                break;
            }
            case 3: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 7;
                w->field_422       = 0;
                break;
            }
            case 4: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 9;
                w->field_422       = 0;
                break;
            }
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}

/// Message handler: on message 0x2C00 whose low nibble is 1..5, store the
/// message halfword in `field_44C`. The five identical case bodies are
/// cross-jumped into one, but only separate bodies keep the jump table; a
/// single `case 1 ... 5` becomes a range test. `arg1` is the dispatch's
/// handler index and is unused here.
void Actor04400_Fn0648C(Task* arg0, s32 arg1, ActorCommand* request)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (request->context.key == 0x2C00) {
        switch (request->command & 0xF) {
            case 1:
                work->field_44C = request->command;
                break;
            case 2:
                work->field_44C = request->command;
                break;
            case 3:
                work->field_44C = request->command;
                break;
            case 4:
                work->field_44C = request->command;
                break;
            case 5:
                work->field_44C = request->command;
                break;
        }
    }
}

void Actor04400_Fn064EC(Task* task, s16 part, VECTOR3* pos)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = pos->vx;
    coord->coord.t[1]   = pos->vy;
    coord->coord.t[2]   = pos->vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/mad_chaser_pin_part.inc.c"

/// Scales `value` by the animation speed `field_41C`, in 1/16 units.
s32 madChaserScaleBySpeed(Task* arg0, s16 value)
{
    return (s32)((((Actor104400Work*)arg0->work)->field_41C * value) << 0xC) >> 0x10;
}

/// Whether slot 1 reports a reached boundary, control jump, or held boundary pose.
s16 madChaserAnimEnded(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if ((work->flags_EC.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_EC.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        return 1;
    }
    return 0;
}

/// Dispatches the second form's `Actor04400_D0001C` table by `Task::state`.
void Actor04400_Fn06658(Task* arg0)
{
    TaskFuncTable10 sp;

    sp = Actor04400_D0001C;
    sp.funcs[arg0->state](arg0);
}

/// Dispatches the first form's `Actor04400_D00004` table by `Task::state`.
void Actor04400_Fn066DC(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = Actor04400_D00004;
    sp.funcs[arg0->state](arg0);
}

/// Runs the intro's two-state handler table on the work block's state index,
/// the same shape as `Actor04400_Fn062D4`.
static void Actor04400_Fn0674C(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        Actor04400_Fn07968,
        madChaserDespawn,
    };

    states[(s16)work->field_420](arg0);
}

#include "../../shared/mad_chaser_turn_to_player.inc.c"

static void Actor04400_Fn06834(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

static void Actor04400_Fn06848(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

static void Actor04400_Fn0685C(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

/// Copies this overlay's `Actor04400_D0007C` dispatcher table onto the stack and
/// lets the pending-request handler `madChaserTakeHitRequest` consume the request
/// first: the table entry `field_422` selects runs only when nothing was
/// consumed.
static void Actor04400_Fn06870(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D0007C;
    if ((s16)madChaserTakeHitRequest(arg0) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

/// Copies this overlay's five-entry `Actor04400_D00088` dispatcher table onto the
/// stack and calls the entry `field_422` selects, the same shape as
/// `Actor04400_Fn06964` with the other table.
static void Actor04400_Fn068F8(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    TaskFuncTable5   sp;

    sp = Actor04400_D00088;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Copies this overlay's five-entry `Actor04400_D0009C` dispatcher table onto the
/// stack and calls the entry `field_422` selects, the same shape as
/// `Actor04400_Fn06870` without the pending-request handler in front of it.
static void Actor04400_Fn06964(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    TaskFuncTable5   sp;

    sp = Actor04400_D0009C;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Dispatches through a two-entry table built on the stack: entry 0 applies the
/// encounter's animation (`madChaserRecoilLight`, which then advances `field_422`
/// itself), entry 1 runs the handler that answers a pending request or a hit
/// (`Actor04400_Fn03390`), chosen by the sub-state index `field_422`.
static void Actor04400_Fn069D0(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        madChaserRecoilLight,
        Actor04400_Fn03390,
    };

    states[(s16)work->field_422](arg0);
}

/// Dispatches through a two-entry table built on the stack: entry 0 applies the
/// animation the encounter asked for (`madChaserRecoilHeavy`), entry 1 finishes
/// the encounter (`madChaserRecoilHeavyEnd`), chosen by the sub-state index
/// `field_422`.
static void Actor04400_Fn06A24(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        madChaserRecoilHeavy,
        madChaserRecoilHeavyEnd,
    };

    states[(s16)work->field_422](arg0);
}

/// Dispatches through a two-entry table built on the stack: entry 0 advances the
/// animation sub-state (`Actor04400_Fn06BC4`), entry 1 runs the pending-request
/// handler (`madChaserStatusHold`), chosen by the sub-state index `field_422`.
static void Actor04400_Fn06A78(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        Actor04400_Fn06BC4,
        madChaserStatusHold,
    };

    states[(s16)work->field_422](arg0);
}

static void Actor04400_Fn06ACC(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D00070;
    sp.funcs[(s16)work->field_422](arg0);
    if (work->field_44F == 1) {
        madChaserTakeKnockdownRequest(arg0);
    }
}

/// Copies this overlay's six-entry `Actor04400_D001AC` dispatcher table onto the
/// stack and calls the entry `field_422` selects.
static void Actor04400_Fn06B50(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    TaskFuncTable6   sp;

    sp = Actor04400_D001AC;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Requests animation 0xC (kind 1, speed 0x10, `field_426` 8) and advances
/// the sub-state.
static void Actor04400_Fn06BC4(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 0xC;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}

#include "../../shared/mad_chaser_status_hold.inc.c"

#include "../../shared/mad_chaser_knockdown_start.inc.c"

#include "../../shared/mad_chaser_knockdown_rise.inc.c"

#include "../../shared/mad_chaser_knockdown_end.inc.c"

#include "../../shared/mad_chaser_walk_finish.inc.c"

#include "../../shared/mad_chaser_start_leap.inc.c"

#include "../../shared/mad_chaser_leap_land.inc.c"

#include "../../shared/mad_chaser_alert_cry.inc.c"

static void Actor04400_Fn0710C(Task* arg0)
{
    u16              ticks;
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->work;
    ticks           = work->field_412;
    work->field_412 = ticks + 1;
    if ((s16)ticks >= 0x51) {
        work->field_422 = work->field_422 + 1;
    }
}

#include "../../shared/mad_chaser_alert_release.inc.c"

#include "../../shared/mad_chaser_alert_crouch.inc.c"

#include "../../shared/mad_chaser_alert_sidestep.inc.c"

/// Dispatches this overlay's `Actor04400_D000B0` dispatcher table by the
/// sub-state index `field_422`. Entry 2 is the fall-to-floor handler
/// `madChaserDangleFall` and entry 3 the landing it triggers
/// (`madChaserDangleLand`).
void madChaserDangleState(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable4   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D000B0;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Sets `field_432`, which makes the per-frame callbacks hold part 6 in
/// place, requests animation 7 (kind 2, speed 0x10) and advances the
/// sub-state.
static void Actor04400_Fn073C8(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    work             = (Actor104400Work*)arg0->work;
    work->field_432  = 1;
    work2            = (Actor104400Work*)arg0->work;
    work2->field_41C = 0x10;
    work2->field_418 = 7;
    work2->field_414 = 2;
    work->field_422  = work->field_422 + 1;
}

#include "../../shared/mad_chaser_dangle_sway.inc.c"

#include "../../shared/mad_chaser_death_cry.inc.c"

#include "../../shared/mad_chaser_death_settle.inc.c"

#include "../../shared/mad_chaser_death_wait_anim.inc.c"

#include "../../shared/mad_chaser_begin_death.inc.c"

#include "../../shared/mad_chaser_death_translucent.inc.c"

static void Actor04400_Fn07878(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

static void Actor04400_Fn07890(Task* arg0)
{
    Actor104400Work* work;
    u16              ticks;

    work            = (Actor104400Work*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 2) {
        work->field_420 = work->field_420 + 1;
    }
}

#include "../../shared/mad_chaser_burst.inc.c"

static void Actor04400_Fn07968(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->work;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

#include "../../shared/mad_chaser_despawn.inc.c"

#include "../../shared/mad_chaser_recoil_light.inc.c"

#include "../../shared/mad_chaser_recoil_heavy.inc.c"

#include "../../shared/mad_chaser_recoil_heavy_end.inc.c"

static void Actor04400_Fn07CF0(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D00150;
    if ((madChaserJoinAlert(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

static void Actor04400_Fn07D78(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D0015C;
    if ((madChaserJoinAlert(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

#include "../../shared/mad_chaser_lurk_rise_state.inc.c"

static void Actor04400_Fn07E74(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D00168;
    if ((madChaserJoinAlert(arg0) << 0x10) != 0) {
        work->field_438 = 0;
        return;
    }
    sp.funcs[(s16)work->field_422](arg0);
}

static void Actor04400_Fn07F04(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable4   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D00174;
    sp.funcs[(s16)work->field_422](arg0);
}

#include "../../shared/mad_chaser_start_hold.inc.c"

#include "../../shared/mad_chaser_lurk_wait.inc.c"

#include "../../shared/mad_chaser_lurk_idle_end.inc.c"

#include "../../shared/mad_chaser_lurk_crouch.inc.c"

#include "../../shared/mad_chaser_lurk_raise.inc.c"

/// Requests animation 0xF (kind 1, speed 0x10, `field_426` 4) and advances
/// the sub-state.
void madChaserLurkRiseStart(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}

#include "../../shared/mad_chaser_lurk_rise_end.inc.c"

#include "../../shared/mad_chaser_start_alert.inc.c"

#include "../../shared/mad_chaser_lurk_brace.inc.c"

#include "../../shared/mad_chaser_lurk_shift_start.inc.c"

#include "../../shared/mad_chaser_lurk_shift_brace.inc.c"

#include "../../shared/mad_chaser_pull_start.inc.c"

#include "../../shared/mad_chaser_pull_react.inc.c"

#include "../../shared/mad_chaser_vanish.inc.c"

#include "../../shared/mad_chaser_vanish_free.inc.c"

#include "../../shared/mad_chaser_death_cry_unlink.inc.c"

#include "../../shared/mad_chaser_death_settle_quiet.inc.c"

/// A further copy, under this file's own name.
#define madChaserDeathWaitAnim Actor04400_Fn089C0
#include "../../shared/mad_chaser_death_wait_anim.inc.c"
#undef madChaserDeathWaitAnim

#include "../../shared/mad_chaser_drop_bodies.inc.c"

static void Actor04400_Fn08A9C(Task* arg0)
{
}

/// A further copy, under this file's own name.
#define madChaserDeathCryUnlink Actor04400_Fn08AA4
#include "../../shared/mad_chaser_death_cry_unlink.inc.c"
#undef madChaserDeathCryUnlink

#include "../../shared/mad_chaser_begin_shrink.inc.c"

/// A further copy, under this file's own name.
#define madChaserDeathTurnTranslucent Actor04400_Fn08C08
#include "../../shared/mad_chaser_death_translucent.inc.c"
#undef madChaserDeathTurnTranslucent

#include "../../shared/mad_chaser_shrink.inc.c"

static void Actor04400_Fn08DA4(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

#include "../../shared/mad_chaser_take_knockdown_request.inc.c"
