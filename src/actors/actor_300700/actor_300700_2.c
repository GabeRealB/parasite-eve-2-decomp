#include "actor_300700_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
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
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/rat.h"
#include "../../shared/moth.h"

extern s16 gRatSlowMoveChance[8];

extern u16 gRatSlowMoveTimes[16];

extern s16 gRatFastMoveChance[8];

extern u16 gRatFastMoveTimes[16];

extern s16 gRatAttackRepeatChance[8];

/// Per-state animation id handed to `func_800B4114`, indexed by `field_37E`.
extern s16 gRatAnimBlend[];

/// The second variant's state handlers, in the same order as the first's:
/// spawn, per-frame update and state 2. `ratTask`
/// dispatches them.
static const GpEnemyTaskFuncTable3 gRatStateHandlers = {
    {
        ratSpawn,
        ratUpdate,
        ratDeath,
    },
};

extern AnimationSet D_actor_300700_8016790C;
extern AnimationSet D_actor_300700_80167BC8;
extern AnimationSet D_actor_300700_80168058;
extern AnimationSet D_actor_300700_80168398;
extern AnimationSet D_actor_300700_801685D8;
extern AnimationSet D_actor_300700_80168854;
extern AnimationSet D_actor_300700_80168B94;
extern AnimationSet D_actor_300700_80168D28;
extern AnimationSet D_actor_300700_80168FBC;
extern AnimationSet D_actor_300700_80169300;

AnimationPackedPose D_actor_300700_80167424[20] = {
#include "assets/actor_300700_animation_05AEC_bank1.inc"
};

AnimationPackedRotation D_actor_300700_80167514[105] = {
#include "assets/actor_300700_animation_05AEC_bank4.inc"
};

AnimationRecord D_actor_300700_801676B8[145] = {
#include "assets/actor_300700_animation_05AEC_records.inc"
};

u16 D_actor_300700_801678FC[8] = {
#include "assets/actor_300700_animation_05AEC_indices.inc"
};

AnimationSet D_actor_300700_8016790C = {
    D_actor_300700_801676B8,
    D_actor_300700_801678FC,
    { NULL, D_actor_300700_80167424, NULL, NULL, D_actor_300700_80167514, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_80167934[15] = {
#include "assets/actor_300700_animation_05DA8_bank1.inc"
};

AnimationPackedRotation D_actor_300700_801679E8[39] = {
#include "assets/actor_300700_animation_05DA8_bank4.inc"
};

AnimationRecord D_actor_300700_80167A84[77] = {
#include "assets/actor_300700_animation_05DA8_records.inc"
};

u16 D_actor_300700_80167BB8[8] = {
#include "assets/actor_300700_animation_05DA8_indices.inc"
};

AnimationSet D_actor_300700_80167BC8 = {
    D_actor_300700_80167A84,
    D_actor_300700_80167BB8,
    { NULL, D_actor_300700_80167934, NULL, NULL, D_actor_300700_801679E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_80167BF0[24] = {
#include "assets/actor_300700_animation_06238_bank1.inc"
};

AnimationPackedRotation D_actor_300700_80167D10[85] = {
#include "assets/actor_300700_animation_06238_bank4.inc"
};

AnimationRecord D_actor_300700_80167E64[121] = {
#include "assets/actor_300700_animation_06238_records.inc"
};

u16 D_actor_300700_80168048[8] = {
#include "assets/actor_300700_animation_06238_indices.inc"
};

AnimationSet D_actor_300700_80168058 = {
    D_actor_300700_80167E64,
    D_actor_300700_80168048,
    { NULL, D_actor_300700_80167BF0, NULL, NULL, D_actor_300700_80167D10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_80168080[14] = {
#include "assets/actor_300700_animation_06578_bank1.inc"
};

AnimationPackedRotation D_actor_300700_80168128[65] = {
#include "assets/actor_300700_animation_06578_bank4.inc"
};

AnimationRecord D_actor_300700_8016822C[87] = {
#include "assets/actor_300700_animation_06578_records.inc"
};

u16 D_actor_300700_80168388[8] = {
#include "assets/actor_300700_animation_06578_indices.inc"
};

AnimationSet D_actor_300700_80168398 = {
    D_actor_300700_8016822C,
    D_actor_300700_80168388,
    { NULL, D_actor_300700_80168080, NULL, NULL, D_actor_300700_80168128, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_801683C0[9] = {
#include "assets/actor_300700_animation_067B8_bank1.inc"
};

AnimationPackedRotation D_actor_300700_8016842C[40] = {
#include "assets/actor_300700_animation_067B8_bank4.inc"
};

AnimationRecord D_actor_300700_801684CC[63] = {
#include "assets/actor_300700_animation_067B8_records.inc"
};

u16 D_actor_300700_801685C8[8] = {
#include "assets/actor_300700_animation_067B8_indices.inc"
};

AnimationSet D_actor_300700_801685D8 = {
    D_actor_300700_801684CC,
    D_actor_300700_801685C8,
    { NULL, D_actor_300700_801683C0, NULL, NULL, D_actor_300700_8016842C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_80168600[10] = {
#include "assets/actor_300700_animation_06A34_bank1.inc"
};

AnimationPackedRotation D_actor_300700_80168678[46] = {
#include "assets/actor_300700_animation_06A34_bank4.inc"
};

AnimationRecord D_actor_300700_80168730[69] = {
#include "assets/actor_300700_animation_06A34_records.inc"
};

u16 D_actor_300700_80168844[8] = {
#include "assets/actor_300700_animation_06A34_indices.inc"
};

AnimationSet D_actor_300700_80168854 = {
    D_actor_300700_80168730,
    D_actor_300700_80168844,
    { NULL, D_actor_300700_80168600, NULL, NULL, D_actor_300700_80168678, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_8016887C[20] = {
#include "assets/actor_300700_animation_06D74_bank1.inc"
};

AnimationPackedRotation D_actor_300700_8016896C[50] = {
#include "assets/actor_300700_animation_06D74_bank4.inc"
};

AnimationRecord D_actor_300700_80168A34[84] = {
#include "assets/actor_300700_animation_06D74_records.inc"
};

u16 D_actor_300700_80168B84[8] = {
#include "assets/actor_300700_animation_06D74_indices.inc"
};

AnimationSet D_actor_300700_80168B94 = {
    D_actor_300700_80168A34,
    D_actor_300700_80168B84,
    { NULL, D_actor_300700_8016887C, NULL, NULL, D_actor_300700_8016896C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_80168BBC[5] = {
#include "assets/actor_300700_animation_06F08_bank1.inc"
};

AnimationPackedRotation D_actor_300700_80168BF8[20] = {
#include "assets/actor_300700_animation_06F08_bank4.inc"
};

AnimationRecord D_actor_300700_80168C48[52] = {
#include "assets/actor_300700_animation_06F08_records.inc"
};

u16 D_actor_300700_80168D18[8] = {
#include "assets/actor_300700_animation_06F08_indices.inc"
};

AnimationSet D_actor_300700_80168D28 = {
    D_actor_300700_80168C48,
    D_actor_300700_80168D18,
    { NULL, D_actor_300700_80168BBC, NULL, NULL, D_actor_300700_80168BF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_80168D50[11] = {
#include "assets/actor_300700_animation_0719C_bank1.inc"
};

AnimationPackedRotation D_actor_300700_80168DD4[49] = {
#include "assets/actor_300700_animation_0719C_bank4.inc"
};

AnimationRecord D_actor_300700_80168E98[69] = {
#include "assets/actor_300700_animation_0719C_records.inc"
};

u16 D_actor_300700_80168FAC[8] = {
#include "assets/actor_300700_animation_0719C_indices.inc"
};

AnimationSet D_actor_300700_80168FBC = {
    D_actor_300700_80168E98,
    D_actor_300700_80168FAC,
    { NULL, D_actor_300700_80168D50, NULL, NULL, D_actor_300700_80168DD4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_300700_80168FE4[18] = {
#include "assets/actor_300700_animation_074E0_bank1.inc"
};

AnimationPackedRotation D_actor_300700_801690BC[54] = {
#include "assets/actor_300700_animation_074E0_bank4.inc"
};

AnimationRecord D_actor_300700_80169194[87] = {
#include "assets/actor_300700_animation_074E0_records.inc"
};

u16 D_actor_300700_801692F0[8] = {
#include "assets/actor_300700_animation_074E0_indices.inc"
};

AnimationSet D_actor_300700_80169300 = {
    D_actor_300700_80169194,
    D_actor_300700_801692F0,
    { NULL, D_actor_300700_80168FE4, NULL, NULL, D_actor_300700_801690BC, NULL, NULL, NULL },
};

DamageAttack gRatAttack = { 6, 3 };

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

TaskDesc D_actor_300700_801693AC = { { { TASK_BODY_TMD, 96 } }, ratTask, { .model = &D_actor_300700_80167400 } };

AnimationSet* gRatAnimSets[11] = {
    NULL,
    &D_actor_300700_8016790C,
    &D_actor_300700_80167BC8,
    &D_actor_300700_80168058,
    &D_actor_300700_80168398,
    &D_actor_300700_801685D8,
    &D_actor_300700_80168854,
    &D_actor_300700_80168B94,
    &D_actor_300700_80168D28,
    &D_actor_300700_80168FBC,
    &D_actor_300700_80169300,
};

s16 gRatAnimBlend[10] = {
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
};

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
