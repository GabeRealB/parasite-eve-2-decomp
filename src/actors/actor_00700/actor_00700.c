#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
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
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/rat.h"

/// The 0x2F4-byte allocation used by Actor00700_Fn01FE0.
typedef struct Actor00700SpawnWork {
    /* 0x000 */ AnimationContext      anim;
    /* 0x014 */ AnimationSlot         slots[4];
    /* 0x0B4 */ u8                    field_B4[0x40];
    /* 0x0F4 */ MATRIX                field_F4;
    /* 0x114 */ MATRIX                field_114;
    /* 0x134 */ u8                    field_134[8];
    /* 0x13C */ void*                 field_13C;
    /* 0x140 */ void*                 field_140;
    /* 0x144 */ u16                   field_144;
    /* 0x146 */ u16                   field_146;
    /* 0x148 */ u16                   field_148;
    /* 0x14A */ u8                    pad_14A[0x2];
    /* 0x14C */ s32                   field_14C;
    /* 0x150 */ u16                   field_150;
    /* 0x152 */ u16                   field_152;
    /* 0x154 */ WorldCollisionContact field_154;
    /* 0x16C */ u8                    field_16C[8];
    /* 0x174 */ void*                 field_174;
    /* 0x178 */ void*                 field_178;
    /* 0x17C */ u16                   field_17C;
    /* 0x17E */ u16                   field_17E;
    /* 0x180 */ u16                   field_180;
    /* 0x182 */ u8                    pad_182[0x2];
    /* 0x184 */ s32                   field_184;
    /* 0x188 */ u16                   field_188;
    /* 0x18A */ u16                   field_18A;
    /* 0x18C */ WorldCollisionContact field_18C[4];
    /* 0x1EC */ u8                    field_1EC[8];
    /* 0x1F4 */ void*                 field_1F4;
    /* 0x1F8 */ void*                 field_1F8;
    /* 0x1FC */ u16                   field_1FC;
    /* 0x1FE */ u16                   field_1FE;
    /* 0x200 */ u16                   field_200;
    /* 0x202 */ u8                    pad_202[0x2];
    /* 0x204 */ s32                   field_204;
    /* 0x208 */ u16                   field_208;
    /* 0x20A */ u16                   field_20A;
    /* 0x20C */ WorldCollisionContact field_20C;
    /* 0x224 */ void*                 field_224;
    /* 0x228 */ u16                   field_228;
    /* 0x22A */ u16                   field_22A;
    /* 0x22C */ u8                    pad_22C[0x80];
    /* 0x2AC */ s32                   field_2AC;
    /* 0x2B0 */ s32                   field_2B0;
    /* 0x2B4 */ s32                   field_2B4;
    /* 0x2B8 */ u8                    pad_2B8[0x1E];
    /* 0x2D6 */ u16                   field_2D6;
    /* 0x2D8 */ u8                    pad_2D8[0x4];
    /* 0x2DC */ u16                   field_2DC;
    /* 0x2DE */ u8                    pad_2DE[0x16];
} Actor00700SpawnWork;
STATIC_ASSERT_SIZEOF(Actor00700SpawnWork, 0x2F4);

extern ActorSpriteUv Actor00700_D075BC[];

extern s16 gRatSlowMoveChance[];
extern u16 gRatSlowMoveTimes[];
extern s16 gRatFastMoveChance[];
extern u16 gRatFastMoveTimes[];
extern s16 gRatAttackRepeatChance[];
extern s16 gRatAnimBlend[];

/// Per-`field_F` drift speed for the state-1 wander in `Actor00700_Fn02A28`,
/// summed with a 5-bit `gRandomLcgState` draw.
extern s16 Actor00700_D07598[];

/// The records `ratSpawn` binds the first body to: the pair it packs
/// into the fourth collision node's key, the context's parameter source (whose
/// `hpMax` seeds the health), and the second argument of `func_800B3F84`.
extern struct DamageAttack gRatAttack;
extern EnemyParams         gRatParams;
extern AnimationSet*       gRatAnimSets[11];

/// The same three for the second body, used by `Actor00700_Fn01FE0`.
extern EnemyParams         Actor00700_D07588;
extern struct DamageAttack Actor00700_D07584;
extern AnimationSet*       Actor00700_D075B4[2];

static void Actor00700_Fn01FE0(Enemy* ctx, Task* actor);
static void Actor00700_Fn02290(Enemy* arg0, Task* arg1);
static void Actor00700_Fn02414(Task* arg0);
static void Actor00700_Fn0268C(Task* arg0);
static void Actor00700_Fn02820(Task* arg0);
static void Actor00700_Fn02A28(Task* arg0);
static void Actor00700_Fn02D28(Enemy* arg0, Task* arg1);
static void Actor00700_Fn0305C(Task* arg0);
void        mothUpdateColor(Task* arg0);
static void Actor00700_Fn03570(Task* arg0);

/// The state handlers `ratTask` dispatches on `Task::state`:
/// set-up, per-frame update, and the one entered once the health runs out.
static const GpEnemyTaskFuncTable3 gRatStateHandlers = {
    { ratSpawn, ratUpdate, ratDeath },
};

static void Actor00700_Fn034BC(Task*);

TmdBone Actor00700_D03670[7] = {
#include "assets/actor_100700_model_04EB4_skeleton.inc"
};

u32 Actor00700_D0376C[7] = {
#include "assets/actor_100700_model_04EB4_partVerts.inc"
};

SVECTOR Actor00700_D03788[78] = {
#include "assets/actor_100700_model_04EB4_verts.inc"
};

SVECTOR Actor00700_D039F8[113] = {
#include "assets/actor_100700_model_04EB4_normals.inc"
};

u32 Actor00700_D03D80[1101] = {
#include "assets/actor_100700_model_04EB4_stream.inc"
};

TmdSource Actor00700_D04EB4 = {
    0,
    5164,
    2392,
    7,
    Actor00700_D0376C,
    Actor00700_D03788,
    Actor00700_D039F8,
    Actor00700_D03670,
    Actor00700_D03D80,
};

AnimationPackedPose Actor00700_D04ED8[20] = {
#include "assets/actor_100700_animation_053C0_bank1.inc"
};

AnimationPackedRotation Actor00700_D04FC8[105] = {
#include "assets/actor_100700_animation_053C0_bank4.inc"
};

AnimationRecord Actor00700_D0516C[145] = {
#include "assets/actor_100700_animation_053C0_records.inc"
};

u16 Actor00700_D053B0[8] = {
#include "assets/actor_100700_animation_053C0_indices.inc"
};

AnimationSet Actor00700_D053C0 = {
    Actor00700_D0516C,
    Actor00700_D053B0,
    { NULL, Actor00700_D04ED8, NULL, NULL, Actor00700_D04FC8, NULL, NULL, NULL },
};

AnimationPackedPose Actor00700_D053E8[15] = {
#include "assets/actor_100700_animation_0567C_bank1.inc"
};

AnimationPackedRotation Actor00700_D0549C[39] = {
#include "assets/actor_100700_animation_0567C_bank4.inc"
};

AnimationRecord Actor00700_D05538[77] = {
#include "assets/actor_100700_animation_0567C_records.inc"
};

u16 Actor00700_D0566C[8] = {
#include "assets/actor_100700_animation_0567C_indices.inc"
};

AnimationSet Actor00700_D0567C = {
    Actor00700_D05538,
    Actor00700_D0566C,
    { NULL, Actor00700_D053E8, NULL, NULL, Actor00700_D0549C, NULL, NULL, NULL },
};

AnimationPackedPose Actor00700_D056A4[24] = {
#include "assets/actor_100700_animation_05B0C_bank1.inc"
};

AnimationPackedRotation Actor00700_D057C4[85] = {
#include "assets/actor_100700_animation_05B0C_bank4.inc"
};

AnimationRecord Actor00700_D05918[121] = {
#include "assets/actor_100700_animation_05B0C_records.inc"
};

u16 Actor00700_D05AFC[8] = {
#include "assets/actor_100700_animation_05B0C_indices.inc"
};

AnimationSet Actor00700_D05B0C = {
    Actor00700_D05918,
    Actor00700_D05AFC,
    { NULL, Actor00700_D056A4, NULL, NULL, Actor00700_D057C4, NULL, NULL, NULL },
};

AnimationPackedPose Actor00700_D05B34[14] = {
#include "assets/actor_100700_animation_05E4C_bank1.inc"
};

AnimationPackedRotation Actor00700_D05BDC[65] = {
#include "assets/actor_100700_animation_05E4C_bank4.inc"
};

AnimationRecord Actor00700_D05CE0[87] = {
#include "assets/actor_100700_animation_05E4C_records.inc"
};

u16 Actor00700_D05E3C[8] = {
#include "assets/actor_100700_animation_05E4C_indices.inc"
};

AnimationSet Actor00700_D05E4C = {
    Actor00700_D05CE0,
    Actor00700_D05E3C,
    { NULL, Actor00700_D05B34, NULL, NULL, Actor00700_D05BDC, NULL, NULL, NULL },
};

AnimationPackedPose Actor00700_D05E74[9] = {
#include "assets/actor_100700_animation_0608C_bank1.inc"
};

AnimationPackedRotation Actor00700_D05EE0[40] = {
#include "assets/actor_100700_animation_0608C_bank4.inc"
};

AnimationRecord Actor00700_D05F80[63] = {
#include "assets/actor_100700_animation_0608C_records.inc"
};

u16 Actor00700_D0607C[8] = {
#include "assets/actor_100700_animation_0608C_indices.inc"
};

AnimationSet Actor00700_D0608C = {
    Actor00700_D05F80,
    Actor00700_D0607C,
    { NULL, Actor00700_D05E74, NULL, NULL, Actor00700_D05EE0, NULL, NULL, NULL },
};

AnimationPackedPose Actor00700_D060B4[10] = {
#include "assets/actor_100700_animation_06308_bank1.inc"
};

AnimationPackedRotation Actor00700_D0612C[46] = {
#include "assets/actor_100700_animation_06308_bank4.inc"
};

AnimationRecord Actor00700_D061E4[69] = {
#include "assets/actor_100700_animation_06308_records.inc"
};

u16 Actor00700_D062F8[8] = {
#include "assets/actor_100700_animation_06308_indices.inc"
};

AnimationSet Actor00700_D06308 = {
    Actor00700_D061E4,
    Actor00700_D062F8,
    { NULL, Actor00700_D060B4, NULL, NULL, Actor00700_D0612C, NULL, NULL, NULL },
};

AnimationPackedPose Actor00700_D06330[20] = {
#include "assets/actor_100700_animation_06648_bank1.inc"
};

AnimationPackedRotation Actor00700_D06420[50] = {
#include "assets/actor_100700_animation_06648_bank4.inc"
};

AnimationRecord Actor00700_D064E8[84] = {
#include "assets/actor_100700_animation_06648_records.inc"
};

u16 Actor00700_D06638[8] = {
#include "assets/actor_100700_animation_06648_indices.inc"
};

AnimationSet Actor00700_D06648 = {
    Actor00700_D064E8,
    Actor00700_D06638,
    { NULL, Actor00700_D06330, NULL, NULL, Actor00700_D06420, NULL, NULL, NULL },
};

AnimationPackedPose Actor00700_D06670[5] = {
#include "assets/actor_100700_animation_067DC_bank1.inc"
};

AnimationPackedRotation Actor00700_D066AC[20] = {
#include "assets/actor_100700_animation_067DC_bank4.inc"
};

AnimationRecord Actor00700_D066FC[52] = {
#include "assets/actor_100700_animation_067DC_records.inc"
};

u16 Actor00700_D067CC[8] = {
#include "assets/actor_100700_animation_067DC_indices.inc"
};

AnimationSet Actor00700_D067DC = {
    Actor00700_D066FC,
    Actor00700_D067CC,
    { NULL, Actor00700_D06670, NULL, NULL, Actor00700_D066AC, NULL, NULL, NULL },
};

AnimationPackedPose Actor00700_D06804[11] = {
#include "assets/actor_100700_animation_06A70_bank1.inc"
};

AnimationPackedRotation Actor00700_D06888[49] = {
#include "assets/actor_100700_animation_06A70_bank4.inc"
};

AnimationRecord Actor00700_D0694C[69] = {
#include "assets/actor_100700_animation_06A70_records.inc"
};

u16 Actor00700_D06A60[8] = {
#include "assets/actor_100700_animation_06A70_indices.inc"
};

AnimationSet Actor00700_D06A70 = {
    Actor00700_D0694C,
    Actor00700_D06A60,
    { NULL, Actor00700_D06804, NULL, NULL, Actor00700_D06888, NULL, NULL, NULL },
};

AnimationPackedPose Actor00700_D06A98[18] = {
#include "assets/actor_100700_animation_06DB4_bank1.inc"
};

AnimationPackedRotation Actor00700_D06B70[54] = {
#include "assets/actor_100700_animation_06DB4_bank4.inc"
};

AnimationRecord Actor00700_D06C48[87] = {
#include "assets/actor_100700_animation_06DB4_records.inc"
};

u16 Actor00700_D06DA4[8] = {
#include "assets/actor_100700_animation_06DB4_indices.inc"
};

AnimationSet Actor00700_D06DB4 = {
    Actor00700_D06C48,
    Actor00700_D06DA4,
    { NULL, Actor00700_D06A98, NULL, NULL, Actor00700_D06B70, NULL, NULL, NULL },
};

struct DamageAttack gRatAttack = { 6, 3 };

EnemyParams gRatParams = { &gRatAttack, 18, 4, 22, 1, 100, 20, 100, 0 };

s16 gRatSlowMoveChance[8] = {
    2,
    4,
    4,
    8,
    8,
    6,
    10,
    3,
};

u16 gRatSlowMoveTimes[16] = {
    15,
    20,
    25,
    30,
    32,
    34,
    36,
    38,
    40,
    42,
    44,
    46,
    48,
    53,
    58,
    63,
};

s16 gRatFastMoveChance[8] = {
    1,
    2,
    5,
    10,
    12,
    12,
    12,
    14,
};

u16 gRatFastMoveTimes[16] = {
    10,
    11,
    12,
    13,
    14,
    16,
    18,
    20,
    22,
    24,
    26,
    28,
    29,
    30,
    31,
    32,
};

s16 gRatAttackRepeatChance[8] = {
    0,
    0,
    4,
    8,
    12,
    12,
    13,
    16,
};

TaskDesc Actor00700_D06E60 = { { { TASK_BODY_TMD, 96 } }, ratTask, { .model = &Actor00700_D04EB4 } };

AnimationSet* gRatAnimSets[11] = {
    NULL,
    &Actor00700_D053C0,
    &Actor00700_D0567C,
    &Actor00700_D05B0C,
    &Actor00700_D05E4C,
    &Actor00700_D0608C,
    &Actor00700_D06308,
    &Actor00700_D06648,
    &Actor00700_D067DC,
    &Actor00700_D06A70,
    &Actor00700_D06DB4,
};

s16 gRatAnimBlend[12] = {
    0,
    3,
    3,
    0,
    2,
    0,
    3,
    0,
    0,
    0,
    0,
    0,
};

TmdBone Actor00700_D06EB0[4] = {
#include "assets/actor_100700_model_074E4_skeleton.inc"
};

u32 Actor00700_D06F40[4] = {
#include "assets/actor_100700_model_074E4_partVerts.inc"
};

SVECTOR Actor00700_D06F50[26] = {
#include "assets/actor_100700_model_074E4_verts.inc"
};

SVECTOR Actor00700_D07020[20] = {
#include "assets/actor_100700_model_074E4_normals.inc"
};

u32 Actor00700_D070C0[265] = {
#include "assets/actor_100700_model_074E4_stream.inc"
};

TmdSource Actor00700_D074E4 = {
    0,
    1488,
    208,
    4,
    Actor00700_D06F40,
    Actor00700_D06F50,
    Actor00700_D07020,
    Actor00700_D06EB0,
    Actor00700_D070C0,
};

AnimationPackedPose Actor00700_D07508[2] = {
#include "assets/actor_100700_animation_0755C_bank1.inc"
};

AnimationPackedRotation Actor00700_D07520[1] = {
#include "assets/actor_100700_animation_0755C_bank4.inc"
};

AnimationRecord Actor00700_D07524[12] = {
#include "assets/actor_100700_animation_0755C_records.inc"
};

u16 Actor00700_D07554[4] = {
#include "assets/actor_100700_animation_0755C_indices.inc"
};

AnimationSet Actor00700_D0755C = {
    Actor00700_D07524,
    Actor00700_D07554,
    { NULL, Actor00700_D07508, NULL, NULL, Actor00700_D07520, NULL, NULL, NULL },
};

DamageAttack Actor00700_D07584 = { 5, 1 };

EnemyParams Actor00700_D07588 = { &Actor00700_D07584, 1, 2, 18, 1, 100, 0, 100, 99 };

s16 Actor00700_D07598[8] = {
    2,
    8,
    16,
    24,
    32,
    32,
    32,
    36,
};

TaskDesc Actor00700_D075A8 = { { { TASK_BODY_TMD, 96 } }, Actor00700_Fn034BC, { .model = &Actor00700_D074E4 } };

AnimationSet* Actor00700_D075B4[2] = {
    NULL,
    &Actor00700_D0755C,
};

ActorSpriteUv Actor00700_D075BC[8] = {
    { 96, 0, 96, 0 },
    { 0, 0, 160, 0 },
    { 0, 0, 192, 0 },
    { 32, 0, 192, 0 },
    { 0, 0, 224, 0 },
    { 32, 0, 224, 0 },
    { 64, 0, 224, 0 },
    { 96, 0, 224, 0 },
};

#include "../../shared/rat_spawn.inc.c"

void ratContacts(Task* actor)
{
    RatWork*               work;
    ActorWallPushFrame*    frame;
    Enemy*                 ctx;
    GfxCoord*              coord;
    GfxCoord*              sourceCoord;
    WorldCollisionContact* effectRec;
    WorldCollisionContact* contactRec;
    s32                    push;
    s32                    result;
    s32                    i;
    s32                    depth;
    s32                    x;
    s32                    y;
    s32                    z;
    s32                    boundedDepth;
    s32                    cooldownParam;
    u32                    lastId;
    u32                    id;
    u32                    slot;
    u32                    hitId;
    u32                    damage;

    push   = 0;
    lastId = 0;
    work   = actor->work;
    SCRATCH_STACK_RESERVE_BLOCK(ActorWallPushFrame);
    frame  = SCRATCH_STACK_CURSOR(ActorWallPushFrame);
    coord  = actor->extra.tmd->coords;
    ctx    = actor->spawnArg2.pointer;
    result = func_800E0C10((WorldCollisionContact*)&work->field_27C[0x20], &frame->delta, 4, NULL);
    switch (result) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += frame->delta.vx.word >> 16;
            coord->coord.t[1] += frame->delta.vy.word >> 16;
            coord->coord.t[2] += frame->delta.vz.word >> 16;
            break;
        case 2:
            coord->coord.t[0] = work->field_360;
            coord->coord.t[1] = work->field_364;
            coord->coord.t[2] = work->field_368;
            break;
    }
    Gp_ClearRec18Occupied(&work->field_27C[0x20]);
    if (work->field_378 != 0) {
        if (--work->field_378 <= 0) {
            work->field_378 = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        id = work->field_22C.contacts.recs[i].key.value;
        switch (id >> 0x10) {
            case 0:
                break;
            case 2:
                if (work->field_378 == 0) {
                    slot                 = id >> 7;
                    sourceCoord          = gPlayerActorTasks[slot & 1]->extra.tmd->coords;
                    frame->delta.vx.word = sourceCoord->coord.t[0] - coord->coord.t[0];
                    frame->delta.vy.word = sourceCoord->coord.t[1] - coord->coord.t[1];
                    frame->delta.vz.word = sourceCoord->coord.t[2] - coord->coord.t[2];
                    damage               = Gp_ComputeDamage(work->field_22C.contacts.recs[i].key.value, SquareRoot0((frame->delta.vx.word * frame->delta.vx.word) + (frame->delta.vy.word * frame->delta.vy.word) + (frame->delta.vz.word * frame->delta.vz.word)), 0, 0);
                    if (Gp_RollEnemyChance(actor->spawnArg2.pointer, work->field_22C.contacts.recs[i].key.value, 0) != 0) {
                        damage *= 4;
                        Gp_SpawnEff(0x6009C, actor->extra.tmd->coords, 0, NULL);
                    }
                    func_800DA6E8(&((Enemy*)actor->spawnArg2.pointer)->node, damage, 0);
                    func_800E2C78(actor->spawnArg2.pointer, work->field_22C.contacts.recs[i].key.value, damage, 0);
                    ctx->hp -= damage;
                    if (ctx->hp <= 0) {
                        work->field_37A = 5;
                        work->field_37C = 0;
                        actor->state    = 2;
                    } else if (work->field_398 == 0) {
                        work->field_37A = 4;
                        work->field_37C = 0;
                    }
                    work->field_31A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    switch (Gp_GetIdParam0(work->field_22C.contacts.recs[i].key.value) & 0xFFFF) {
                        case 0:
                        case 4:
                        case 5:
                        case 6:
                        case 7:
                        case 8:
                            break;
                        case 2:
                            Gp_SetObjFlag2(actor->spawnArg2.pointer, work->field_22C.contacts.recs[i].key.value, 0);
                            break;
                        case 3:
                            Gp_SetObjFlag4(actor->spawnArg2.pointer, work->field_22C.contacts.recs[i].key.value, 0);
                            break;
                        case 1:
                        case 9:
                            Gp_SetObjFlag1(actor->spawnArg2.pointer);
                            break;
                    }
                    hitId = work->field_22C.contacts.recs[i].key.value;
                    if (lastId != hitId) {
                        lastId = hitId;
                        func_800FDB18(Gp_GetIdParam1(hitId) & 0xFFFF, coord, NULL, &work->field_334);
                    }
                    cooldownParam = Gp_GetIdParam2(work->field_22C.contacts.recs[i].key.value);
                    if (cooldownParam > 0) {
                        work->field_378 = cooldownParam;
                    }
                }
                break;
            case 1:
                x                    = coord->workm.t[0] - work->field_22C.contacts.recs[i].point.vx;
                frame->delta.vx.word = x;
                y                    = coord->workm.t[1] - work->field_22C.contacts.recs[i].point.vy;
                frame->delta.vy.word = y;
                z                    = coord->workm.t[2] - work->field_22C.contacts.recs[i].point.vz;
                frame->delta.vz.word = z;
                depth                = work->field_22C.contacts.recs[i].distance - SquareRoot0((x * x) + (y * y) + (z * z));
                boundedDepth         = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                if (push < depth) {
                    push = depth;
                    VectorNormal((VECTOR*)&frame->delta, &frame->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &frame->normal, &frame->dir);
                }
                break;
            case 3:
                x                    = coord->workm.t[0] - work->field_22C.contacts.recs[i].point.vx;
                frame->delta.vx.word = x;
                y                    = coord->workm.t[1] - work->field_22C.contacts.recs[i].point.vy;
                frame->delta.vy.word = y;
                z                    = coord->workm.t[2] - work->field_22C.contacts.recs[i].point.vz;
                frame->delta.vz.word = z;
                depth                = work->field_22C.contacts.recs[i].distance - SquareRoot0((x * x) + (y * y) + (z * z));
                boundedDepth         = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                if (push < depth) {
                    push = depth;
                    VectorNormal((VECTOR*)&frame->delta, &frame->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &frame->normal, &frame->dir);
                }
                break;
        }
    }
    if (push > 0) {
        coord->coord.t[0] += (push * frame->dir.vx) >> 0xC;
        coord->coord.t[2] += (push * frame->dir.vz) >> 0xC;
    }
    Gp_ClearRec18Occupied(work->field_22C.contacts.recs);
    effectRec = work->attackContacts;
    if (Gp_FindRec18(effectRec, 0) != 0) {
        work->field_31A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(effectRec);
    }
    contactRec = work->sensorContacts;
    if (Gp_CountRec18Hi(contactRec, 0x10000) != 0) {
        sourceCoord      = gPlayerActorTasks[(u8)work->sensorContacts[0].key.parts.id >> 7]->extra.tmd->coords;
        work->field_394  = 1;
        work->field_1FA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_33C  = sourceCoord;
    }
    Gp_ClearRec18Occupied(contactRec);
    SCRATCH_STACK_RELEASE_BLOCK(ActorWallPushFrame);
}

#include "../../shared/rat_idle.inc.c"

#include "../../shared/rat_attack.inc.c"

#include "../../shared/rat_stagger.inc.c"

#include "../../shared/rat_buildup.inc.c"

#include "../../shared/rat_turn.inc.c"

#include "../../shared/rat_death.inc.c"

#include "../../shared/rat_task.inc.c"

#include "../../shared/rat_update.inc.c"

#include "../../shared/rat_reactions.inc.c"

void ratBehavior(Task* arg0)
{
    switch (((RatWork*)arg0->work)->field_37A) {
        case 0:
            ratIdle(arg0);
            break;
        case 1:
            ratAttack(arg0);
            break;
        case 2:
            ratStagger(arg0);
            break;
        case 3:
            ratBuildup(arg0);
            break;
        case 4:
            ratHurt(arg0);
            break;
        case 5:
            break;
    }
}

#include "../../shared/rat_idle_sound.inc.c"

#include "../../shared/rat_hurt.inc.c"

#include "../../shared/rat_step.inc.c"

#include "../../shared/rat_animate.inc.c"

#include "../../shared/rat_update_color.inc.c"

#include "../../shared/rat_shadow.inc.c"

#include "../../shared/rat_squash.inc.c"

/// The state handlers `Actor00700_Fn034BC` dispatches on `Task::state`,
/// for the second body this package carries: set-up, per-frame update, and the
/// one a resolved hit switches it to.
static const GpEnemyTaskFuncTable3 Actor00700_D00054 = {
    { Actor00700_Fn01FE0, Actor00700_Fn02290, Actor00700_Fn02D28 },
};

static void Actor00700_Fn01FE0(Enemy* ctx, Task* actor)
{
    GfxCoord*            coord;
    TmdObject*           obj;
    s32                  i;
    void*                rec1;
    void*                rec2;
    void*                rec3;
    Actor00700SpawnWork* work;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x2F4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work             = work;
    obj->flags              = 0;
    coord->composeStamp     = GRAPHICS_COORD_DIRTY;
    obj->texturePageOffset += 1;
    obj->clutRowOffset     += 1;
    tmdProcessStream(obj);
    tmdProcessStream(obj);
    obj->lightMtx = &work->field_114;
    obj->colorMtx = &work->field_F4;
    ctx->field_4  = &coord->coord;
    ctx->field_48 = 0;
    Gp_LinkNode(&ctx->node);
    ctx->coord                  = coord;
    ctx->node.state.parts.flags = 0;
    ctx->bodyPos.vx             = 0;
    ctx->bodyPos.vy             = 0;
    ctx->bodyPos.vz             = 0;
    ctx->param                  = &Actor00700_D07588;
    ctx->recs                   = &work->field_154;
    ctx->hp                     = (u16)Actor00700_D07588.hpMax;
    work->field_228             = 0x100;
    work->field_22A             = 1;
    work->field_224             = coord;
    func_800B3F84(&work->anim, Actor00700_D075B4, obj, &work->field_B4, work->slots);
    for (i = 1; i < 4; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    Gp_IncStateF0Ref(0);
    work->field_2D6 = 1;
    work->field_2AC = (s32)coord->coord.t[0];
    work->field_2B0 = (s32)coord->coord.t[1];
    work->field_2B4 = (s32)coord->coord.t[2];
    work->field_2DC = (u16)((Enemy*)actor->spawnArg2.pointer)->place->yaw;
    rec1            = &work->field_154;
    work->field_13C = coord;
    work->field_140 = rec1;
    work->field_144 = 0;
    work->field_146 = 0;
    work->field_148 = 0;
    work->field_14C = 0x30008;
    work->field_150 = 0xFA;
    work->field_152 = 1U;
    Gp_LinkObj(2, &work->field_134);
    Gp_InitRec18Table(rec1, 1, 0);
    rec2            = &work->field_18C;
    work->field_174 = coord;
    work->field_178 = rec2;
    work->field_17C = 0;
    work->field_17E = 0;
    work->field_180 = 0;
    work->field_184 = 0x30008;
    work->field_188 = 0xFA;
    work->field_18A = 1U;
    work->field_152 = (u16)(work->field_152 | WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_LinkObj(2, &work->field_16C);
    Gp_InitRec18Table(rec2, 4, 0);
    rec3            = &work->field_20C;
    work->field_1F4 = coord;
    work->field_1F8 = rec3;
    work->field_1FC = 0;
    work->field_1FE = 0;
    work->field_200 = 0;
    work->field_18A = (u16)(work->field_18A | 0x4000);
    work->field_204 = Gp_PackPair(&Actor00700_D07584, 0);
    work->field_208 = 0x190;
    work->field_20A = 1U;
    Gp_LinkObj(3, &work->field_1EC);
    Gp_InitRec18Table(rec3, 1, 0);
    work->field_20A = (u16)(work->field_20A & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    actor->state    = 1;
}

static void Actor00700_Fn02290(Enemy* arg0, Task* arg1)
{
    TmdObject* obj;
    RatWork*   work;
    GfxCoord*  coord;
    s32        state;
    s32        one;

    work  = arg1->work;
    obj   = arg1->extra.tmd;
    state = gSceneCombatState.actorControl;
    coord = obj->coords;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->flags                   = 0;
    arg0->node.state.parts.flags = 0;
    goto default_body;
case1:
    mothUpdateColor(arg1);
    return;
case2:
    obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
default_body:
    Actor00700_Fn02414(arg1);
    Actor00700_Fn0268C(arg1);
    if (work->field_2E6 == 0 && gSceneCombatState.actor00700DeathAlert != 0) {
        work->field_2E6 = 1;
        Gp_ArmStateF0(1);
    }
    Actor00700_Fn02820(arg1);
    Actor00700_Fn02A28(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    mothUpdateColor(arg1);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16 & 0x7F) == 0) {
        s32 temp;
        s32 id;

        id   = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070008;
        temp = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
        SndEvt_EnqueueType6(id, temp, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
    }
}

static void Actor00700_Fn02414(Task* arg0)
{
    RatWork*        work;
    GfxCoord*       coord;
    s32             movement;
    s32             dx;
    s32             dy;
    s32             dz;
    s32             amount;
    s32             damage;
    s32             z;
    u16             state;
    GfxCoord*       target;
    GpDeltaScratch* head;
    GpDeltaScratch* delta;

    work     = arg0->work;
    head     = SCRATCH_STACK_CURSOR(void);
    delta    = (SCRATCH_STACK_CURSOR(void) = head - 1);
    coord    = arg0->extra.tmd->coords;
    movement = func_800E0C10(&work->field_18C, delta, 4, 0);
    switch (movement) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += head[-1].vx.halves.integer;
            coord->coord.t[1] += delta->vy.halves.integer;
            z                  = coord->coord.t[2] + delta->vz.halves.integer;
            coord->coord.t[2]  = z;
            break;
        case 2:
            coord->coord.t[0] = work->field_2BC;
            coord->coord.t[1] = work->field_2C0;
            coord->coord.t[2] = work->field_2C4;
            break;
    }
    Gp_ClearRec18Occupied(&work->field_18C);
    state = (u16)work->field_154.key.parts.kind;
    switch ((u32)state) {
        case 0:
            break;
        case 1:
            arg0->state                           = 2;
            ((Enemy*)arg0->spawnArg2.pointer)->hp = 0;
            Gp_ArmStateF0(1);
            break;
        case 2:
            arg0->state    = (s32)state;
            target         = gPlayerActorTasks[(u8)work->field_154.key.parts.id >> 7]->extra.tmd->coords;
            dx             = target->coord.t[0] - coord->coord.t[0];
            delta->vx.word = dx;
            dy             = target->coord.t[1] - coord->coord.t[1];
            delta->vy.word = dy;
            dz             = target->coord.t[2] - coord->coord.t[2];
            delta->vz.word = dz;
            damage         = Gp_ComputeDamage((s32)work->field_154.key.value, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
            amount         = damage;
            if (damage == 0) {
                damage = 1;
                amount = 1;
            }
            func_800DA6E8(&((Enemy*)arg0->spawnArg2.pointer)->node, amount, 0);
            func_800E2C78(arg0->spawnArg2.pointer, (s32)work->field_154.key.value, damage, 0);
            ((Enemy*)arg0->spawnArg2.pointer)->hp = 0;
            func_800FDB18(Gp_GetIdParam1((s32)work->field_154.key.value) & 0xFFFF, arg0->extra.tmd->coords, 0, &work->field_224);
            break;
    }
    Gp_ClearRec18Occupied(&work->field_154);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void Actor00700_Fn0268C(Task* arg0)
{
    RatWork*  work;
    GfxCoord* coord;
    GfxCoord* coord2;
    SVECTOR*  sc;
    s32       direction;
    s32       direction2;
    s32       product;
    sc   = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    work = arg0->work;
    if (++work->field_2E0 >= 16) {
        work->field_2E0 = 0;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_2D4 = !((gRandomLcgState >> 16) & 1);
    }
    switch (work->field_2D4) {
        case 0:
            work->field_2D8 += 0x100;
            if (work->field_2D8 >= 0x200) {
                work->field_2D8 = -0x100;
                direction       = work->field_2D6;
                work->field_2D6 = -direction;
            }
            break;
        case 1:
            work->field_2D8 = 0x100;
            direction2      = work->field_2D6;
            work->field_2D6 = -direction2;
            break;
    }
    sc->vx = 0;
    sc->vy = 0;
    sc->vz = work->field_2D8 * work->field_2D6;
    coord  = arg0->extra.tmd->coords;
    RotMatrix(sc, &coord[2].coord);
    coord[2].composeStamp = GRAPHICS_COORD_DIRTY;
    sc->vx                = 0;
    sc->vy                = 0;
    product               = work->field_2D8 * work->field_2D6;
    sc->vz                = -product;
    coord2                = arg0->extra.tmd->coords;
    RotMatrix(sc, &coord2[3].coord);
    coord2[3].composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void Actor00700_Fn02820(Task* arg0)
{
    RatWork*          work;
    GfxCoord*         coord;
    ActorFaceScratch* sc;
    s32               random;
    s32               amount;
    s32               cur;
    s32               cur2;
    s32               cur3;
    s32               random2;
    s32               amount2;
    u16               want;
    s16               diff;
    s32               adiff;
    s16               turn;
    s16               wrap;

    sc    = (ActorFaceScratch*)SCRATCH_STACK_RESERVE_BYTES(0x18);
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_2E6) {
        case 0:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            random          = gRandomLcgState >> 16;
            amount          = random & 0x1F;
            cur             = work->field_2DC;
            work->field_2DC = !(random & 0x20) ? cur - amount : cur + amount;
            break;
        case 1:
            sc->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            sc->delta.vy = 0;
            sc->delta.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            want         = ratan2((s16)sc->delta.vx, (s16)sc->delta.vz) & 0xFFF;
            diff         = want - (work->field_2DC & 0xFFF);
            adiff        = diff >= 0 ? diff : -diff;
            turn         = diff;
            if (adiff < 0x11) {
                work->field_2DC = want;
            } else {
                if (adiff >= 0x801) {
                    wrap = diff - 0x1000;
                    if (diff <= 0)
                        wrap = 0x1000 - diff;
                    turn = wrap;
                }
                cur2 = work->field_2DC;
                if (turn > 0) {
                    work->field_2DC = cur2 + 0x10;
                } else {
                    work->field_2DC = cur2 - 0x10;
                }
            }
            break;
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    random2         = gRandomLcgState >> 16;
    amount2         = random2 & 0x3F;
    cur3            = work->field_2DA;
    work->field_2DA = !(random2 & 0x40) ? cur3 - amount2 : cur3 + amount2;
    if (work->field_2DA > 0x100) {
        work->field_2DA = 0x100;
    } else if (work->field_2DA < -0x100) {
        work->field_2DA = -0x100;
    }
    sc->rot.vx = work->field_2DA;
    sc->rot.vy = work->field_2DC;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

static void Actor00700_Fn02A28(Task* arg0)
{
    RatWork*  work;
    GfxCoord* coord;
    u32       random;
    u32       random2;
    u32       random3;
    s32       amount;
    s32       amountB;
    s16       delta;
    s16       speed;
    s32       y;
    s32       newY;
    s16       base;

    work            = arg0->work;
    coord           = arg0->extra.tmd->coords;
    work->field_2BC = coord->coord.t[0];
    work->field_2C0 = coord->coord.t[1];
    work->field_2C4 = coord->coord.t[2];
    switch (work->field_2E6) {
        case 0:
            random = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
            amount = random & 0x1F;
            if (!(random & 0x20)) {
                amount = -amount;
            }
            delta = amount;
            if ((s16)(coord->coord.t[0] + (s16)delta) < work->field_2AC + 200 &&
                work->field_2AC - 200 < (s16)(coord->coord.t[0] + (s16)delta)) {
                coord->coord.t[0] += (s16)delta;
            } else {
                coord->coord.t[0] -= (s16)delta;
            }
            amountB = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x1F;
            if (work->field_2D4 != 0) {
                amountB = -amountB;
            }
            delta = amountB;
            if ((s16)(coord->coord.t[1] + (s16)delta) < work->field_2B0 + 500 &&
                work->field_2B0 - 500 < (s16)(coord->coord.t[1] + (s16)delta)) {
                coord->coord.t[1] += (s16)delta;
            } else {
                coord->coord.t[1] -= (s16)delta;
            }
            random3 = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
            amount  = random3 & 0x1F;
            if (!(random3 & 0x20)) {
                amount = -amount;
            }
            delta = amount;
            if ((s16)random3 < work->field_2B4 + 200 && work->field_2B4 - 200 < (s16)random3) {
                coord->coord.t[2] += (s16)delta;
            } else {
                coord->coord.t[2] -= (s16)delta;
            }
            break;
        case 1:
            speed = Actor00700_D07598[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] +
                    (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x1F);
            coord->coord.t[0] += (coord->coord.m[0][2] * speed) >> 12;
            coord->coord.t[2] += (coord->coord.m[2][2] * speed) >> 12;
            base               = gPlayerStatus.coordMtx->t[1] - 0x4B0;
            random2            = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
            y                  = coord->coord.t[1];
            if (y >= base + 400) {
                coord->coord.t[1] = y - (random2 & 0xF);
            } else {
                if (base - 400 >= y) {
                    newY = y + (random2 & 0xF);
                } else {
                    amountB = random2 & 0x1F;
                    if (work->field_2D4 != 0) {
                        newY = y - amountB;
                    } else {
                        newY = y + amountB;
                    }
                }
                coord->coord.t[1] = newY;
            }
            break;
    }
}

static void Actor00700_Fn02D28(Enemy* arg0, Task* arg1)
{
    RatWork*  work;
    GfxCoord* coord;
    SVECTOR*  head;
    SVECTOR*  rot;
    s32       angle;
    u32       rnd;
    u32       seed;
    s32       id;
    s32       pan;

    coord = arg1->extra.tmd->coords;
    work  = arg1->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            head                          = SCRATCH_STACK_CURSOR(SVECTOR);
            rot                           = head - 1;
            SCRATCH_STACK_CURSOR(SVECTOR) = rot;
            switch (work->field_2DE) {
                case 0:
                    gSceneCombatState.actor00700DeathAlert = 1;
                    seed                                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    rnd                                    = seed >> 16;
                    angle                                  = rnd & 0xFF;
                    arg1->extra.tmd->flags                 = TMD_OBJECT_SEMI_TRANS;
                    gRandomLcgState                        = seed;
                    work->field_2E2                        = 0x1000;
                    work->field_22C.matrix                 = coord->coord;
                    if (!(rnd & 0x100)) {
                        angle = -angle;
                    }
                    work->field_2E4                         = angle;
                    arg0->recs                              = 0;
                    ((Actor00700SpawnWork*)work)->field_152 = ((Actor00700SpawnWork*)work)->field_152 & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    ((Actor00700SpawnWork*)work)->field_18A = ((Actor00700SpawnWork*)work)->field_18A & 0xBFFF;
                    ((Actor00700SpawnWork*)work)->field_20A = ((Actor00700SpawnWork*)work)->field_20A | WORLD_COLLISION_BODY_PAIR_ENABLED;
                    id                                      = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070006;
                    pan                                     = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                    Gp_UnlinkNode(&arg0->node);
                    Gp_ReleaseStateF0Add(arg1, 8);
                    work->field_2E0 = 1;
                    work->field_2DE = 1;
                    break;
                case 1:
                    Actor00700_Fn03570(arg1);
                    work->field_2DA = (work->field_2DA + work->field_2E4) & 0xFFF;
                    work->field_2DC = (work->field_2DC + work->field_2E4) & 0xFFF;
                    rot->vx         = work->field_2DA;
                    rot->vy         = work->field_2DC;
                    rot->vz         = 0;
                    RotMatrix(rot, &coord->coord);
                    work->field_22C.matrix.t[1] += 0x18;
                    if ((s16)(work->field_2E0 / 3) < 8) {
                        Actor00700_Fn0305C(arg1);
                    } else {
                        arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    work->field_2E0++;
                    if (work->field_2E0 >= 0x1E) {
                        Gp_UnlinkObj(&((Actor00700SpawnWork*)work)->field_134);
                        Gp_UnlinkObj(&((Actor00700SpawnWork*)work)->field_16C);
                        Gp_UnlinkObj(&((Actor00700SpawnWork*)work)->field_1EC);
                        work->field_2DE = 2;
                    }
                    break;
                case 2:
                    work->field_2E0--;
                    if (work->field_2E0 <= 0) {
                        Gp_DestroyEnemy(arg0, arg1);
                    }
                    break;
            }
            SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
            break;
    }
}

static void Actor00700_Fn0305C(Task* arg0)
{
    ActorQuadScratch* sc;
    RatWork*          work;
    TmdObject*        obj;
    GfxCoord*         coord;
    s32               size, x, y;
    s16               i;
    SVECTOR*          v;
    POLY_FT4*         prim;
    ActorSpriteUv*    uv;
    obj         = arg0->extra.tmd;
    sc          = (ActorQuadScratch*)SCRATCH_STACK_RESERVE_BYTES(0x28);
    coord       = obj->coords;
    work        = arg0->work;
    sc->v[0].vx = coord->workm.t[0];
    sc->v[0].vy = coord->workm.t[1];
    sc->v[0].vz = coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->v[0]);
    gte_rtps();
    gte_stsxy(&sc->sxy);
    gte_stszotz(&sc->otz);
    if (sc->otz < 20) {
        SCRATCH_STACK_RELEASE_BYTES(0x28);
        return;
    }
    if (work->field_2E0 == 1) {
        sc->v[0].vx = 0;
        sc->v[0].vy = 0;
        sc->v[0].vz = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xFFF;
        RotMatrix(&sc->v[0], &work->field_22C.quad.rotation);
    }
    size        = 0x7800 / sc->otz;
    x           = sc->sxy & 0xFFFF;
    y           = sc->sxy >> 16;
    sc->v[0].vx = -size;
    sc->v[0].vy = -size;
    sc->v[0].vz = 0;
    sc->v[1].vx = size;
    sc->v[1].vy = -size;
    sc->v[1].vz = 0;
    sc->v[2].vx = -size;
    sc->v[2].vy = size;
    sc->v[2].vz = 0;
    sc->v[3].vx = size;
    sc->v[3].vy = size;
    sc->v[3].vz = 0;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&work->field_22C.quad.rotation);
        v = &sc->v[i];
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += x;
        v->vy += y;
    }
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2E);
    setRGB0(prim, 0x80, 0x80, 0x80);
    setShadeTex(prim, 1);
    prim->tpage = (((obj->texturePageOffset * 64 + 0x180) & 0x3FF) >> 6) | 0xD0;
    prim->clut  = (obj->clutRowOffset << 6) + 0x3D40;
    uv          = &Actor00700_D075BC[(s16)(work->field_2E0 / 3)];
    prim->u0    = uv->u;
    prim->v0    = uv->v;
    prim->u1    = uv->u + 31;
    prim->v1    = uv->v;
    prim->u2    = uv->u;
    prim->v2    = uv->v + 31;
    prim->u3    = uv->u + 31;
    prim->v3    = uv->v + 31;
    prim->x0    = sc->v[0].vx;
    prim->y0    = sc->v[0].vy;
    prim->x1    = sc->v[1].vx;
    prim->y1    = sc->v[1].vy;
    prim->x2    = sc->v[2].vx;
    prim->y2    = sc->v[2].vy;
    prim->x3    = sc->v[3].vx;
    prim->y3    = sc->v[3].vy;
    addPrim((&gGpuCurrentOt[(((((u32)sc->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), prim);
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}

static void Actor00700_Fn034BC(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor00700_D00054;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

/// A further copy, under this file's own name.
#define ratUpdateColor mothUpdateColor
#include "../../shared/rat_update_color.inc.c"
#undef ratUpdateColor

static void Actor00700_Fn03570(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    RatWork*           work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->field_2E2 >= 0x201) {
        work->field_2E2 = (u16)work->field_2E2 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_2E2;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_22C.matrix;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
