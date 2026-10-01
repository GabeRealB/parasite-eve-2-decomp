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
#include "../../shared/moth.h"

extern ActorSpriteUv gMothBurstUvs[];

extern s16 gRatSlowMoveChance[];
extern u16 gRatSlowMoveTimes[];
extern s16 gRatFastMoveChance[];
extern u16 gRatFastMoveTimes[];
extern s16 gRatAttackRepeatChance[];
extern s16 gRatAnimBlend[];

/// Per-`field_F` drift speed for the state-1 wander in `mothDrift`,
/// summed with a 5-bit `gRandomLcgState` draw.
extern s16 gMothSpeeds[];

/// The records `ratSpawn` binds the first body to: the pair it packs
/// into the fourth collision node's key, the context's parameter source (whose
/// `hpMax` seeds the health), and the second argument of `func_800B3F84`.
extern struct DamageAttack gRatAttack;
extern EnemyParams         gRatParams;
extern AnimationSet*       gRatAnimSets[11];

/// The same three for the second body, used by `mothSpawn`.
extern EnemyParams         gMothParams;
extern struct DamageAttack gMothAttack;
extern AnimationSet*       gMothAnimSets[2];

/// The state handlers `ratTask` dispatches on `Task::state`:
/// set-up, per-frame update, and the one entered once the health runs out.
static const GpEnemyTaskFuncTable3 gRatStateHandlers = {
    { ratSpawn, ratUpdate, ratDeath },
};

TmdBone Actor00700_D03670[7] = {
#include "assets/rat_body_skeleton.inc"
};

u32 Actor00700_D0376C[7] = {
#include "assets/rat_body_partVerts.inc"
};

SVECTOR Actor00700_D03788[78] = {
#include "assets/rat_body_verts.inc"
};

SVECTOR Actor00700_D039F8[113] = {
#include "assets/rat_body_normals.inc"
};

u32 Actor00700_D03D80[1101] = {
#include "assets/rat_body_stream.inc"
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
#include "assets/moth_body_skeleton.inc"
};

u32 Actor00700_D06F40[4] = {
#include "assets/moth_body_partVerts.inc"
};

SVECTOR Actor00700_D06F50[26] = {
#include "assets/moth_body_verts.inc"
};

SVECTOR Actor00700_D07020[20] = {
#include "assets/moth_body_normals.inc"
};

u32 Actor00700_D070C0[265] = {
#include "assets/moth_body_stream.inc"
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

DamageAttack gMothAttack = { 5, 1 };

EnemyParams gMothParams = { &gMothAttack, 1, 2, 18, 1, 100, 0, 100, 99 };

s16 gMothSpeeds[8] = {
    2,
    8,
    16,
    24,
    32,
    32,
    32,
    36,
};

TaskDesc Actor00700_D075A8 = { { { TASK_BODY_TMD, 96 } }, mothTask, { .model = &Actor00700_D074E4 } };

AnimationSet* gMothAnimSets[2] = {
    NULL,
    &Actor00700_D0755C,
};

ActorSpriteUv gMothBurstUvs[8] = {
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

#include "../../shared/rat_contacts.inc.c"

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

/// The state handlers `mothTask` dispatches on `Task::state`,
/// for the second body this package carries: set-up, per-frame update, and the
/// one a resolved hit switches it to.
static const GpEnemyTaskFuncTable3 gMothStateHandlers = {
    { mothSpawn, mothUpdate, mothDeath },
};

#include "../../shared/moth_spawn.inc.c"

#include "../../shared/moth_update.inc.c"

#include "../../shared/moth_contacts.inc.c"

#include "../../shared/moth_oscillate_parts.inc.c"

#include "../../shared/moth_steer.inc.c"

#include "../../shared/moth_drift.inc.c"

#include "../../shared/moth_death.inc.c"

#include "../../shared/moth_draw_burst.inc.c"

#include "../../shared/moth_task.inc.c"

#include "../../shared/moth_update_color.inc.c"

#include "../../shared/moth_squash.inc.c"
