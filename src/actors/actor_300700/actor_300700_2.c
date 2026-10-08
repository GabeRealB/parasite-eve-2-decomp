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

static void _ratAnimate(Task* actor);
static void _ratAttack(Task* actor);
static void _ratBehavior(Task* actor);
static void _ratBuildup(Task* actor);
static void _ratHurt(Task* actor);
static void _ratIdle(Task* actor);
static void _ratIdleSound(Task* actor);
static void _ratReactions(Task* actor);
static void _ratShadow(Task* actor);
static void _ratSquash(Task* actor);
static void _ratStagger(Task* actor);
static void _ratStep(Task* actor);
static void _ratTask(Task* actor);
static void _ratTurn(Task* actor);
static void _ratUpdateColor(Task* actor);

extern s16 gRatSlowMoveChance[8];

extern u16 gRatSlowMoveTimes[16];

extern s16 gRatFastMoveChance[8];

extern u16 gRatFastMoveTimes[16];

extern s16 gRatAttackRepeatChance[8];

/// Per-state animation id handed to `animationSeekSlotWithBlend`, indexed by `field_37E`.
extern s16 gRatAnimBlend[];

/// The second variant's state handlers, in the same order as the first's:
/// spawn, per-frame update and state 2. `_ratTask`
/// dispatches them.
static const EnemyTaskFuncTable3 gRatStateHandlers = {
    {
        ratSpawn,
        ratUpdate,
        ratDeath,
    },
};

static AnimationSet _gActor300700Animation05AEC;
static AnimationSet _gActor300700Animation05DA8;
static AnimationSet _gActor300700Animation06238;
static AnimationSet _gActor300700Animation06578;
static AnimationSet _gActor300700Animation067B8;
static AnimationSet _gActor300700Animation06A34;
static AnimationSet _gActor300700Animation06D74;
static AnimationSet _gActor300700Animation06F08;
static AnimationSet _gActor300700Animation0719C;
static AnimationSet _gActor300700Animation074E0;

static AnimationPackedPose _gActor300700Animation05AECBank1[20] = {
#include "assets/actor_300700_animation_05AEC_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation05AECBank4[105] = {
#include "assets/actor_300700_animation_05AEC_bank4.inc"
};

static AnimationRecord _gActor300700Animation05AECRecords[145] = {
#include "assets/actor_300700_animation_05AEC_records.inc"
};

static u16 _gActor300700Animation05AECIndices[8] = {
#include "assets/actor_300700_animation_05AEC_indices.inc"
};

static AnimationSet _gActor300700Animation05AEC = {
    _gActor300700Animation05AECRecords,
    _gActor300700Animation05AECIndices,
    { NULL, _gActor300700Animation05AECBank1, NULL, NULL, _gActor300700Animation05AECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation05DA8Bank1[15] = {
#include "assets/actor_300700_animation_05DA8_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation05DA8Bank4[39] = {
#include "assets/actor_300700_animation_05DA8_bank4.inc"
};

static AnimationRecord _gActor300700Animation05DA8Records[77] = {
#include "assets/actor_300700_animation_05DA8_records.inc"
};

static u16 _gActor300700Animation05DA8Indices[8] = {
#include "assets/actor_300700_animation_05DA8_indices.inc"
};

static AnimationSet _gActor300700Animation05DA8 = {
    _gActor300700Animation05DA8Records,
    _gActor300700Animation05DA8Indices,
    { NULL, _gActor300700Animation05DA8Bank1, NULL, NULL, _gActor300700Animation05DA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation06238Bank1[24] = {
#include "assets/actor_300700_animation_06238_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation06238Bank4[85] = {
#include "assets/actor_300700_animation_06238_bank4.inc"
};

static AnimationRecord _gActor300700Animation06238Records[121] = {
#include "assets/actor_300700_animation_06238_records.inc"
};

static u16 _gActor300700Animation06238Indices[8] = {
#include "assets/actor_300700_animation_06238_indices.inc"
};

static AnimationSet _gActor300700Animation06238 = {
    _gActor300700Animation06238Records,
    _gActor300700Animation06238Indices,
    { NULL, _gActor300700Animation06238Bank1, NULL, NULL, _gActor300700Animation06238Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation06578Bank1[14] = {
#include "assets/actor_300700_animation_06578_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation06578Bank4[65] = {
#include "assets/actor_300700_animation_06578_bank4.inc"
};

static AnimationRecord _gActor300700Animation06578Records[87] = {
#include "assets/actor_300700_animation_06578_records.inc"
};

static u16 _gActor300700Animation06578Indices[8] = {
#include "assets/actor_300700_animation_06578_indices.inc"
};

static AnimationSet _gActor300700Animation06578 = {
    _gActor300700Animation06578Records,
    _gActor300700Animation06578Indices,
    { NULL, _gActor300700Animation06578Bank1, NULL, NULL, _gActor300700Animation06578Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation067B8Bank1[9] = {
#include "assets/actor_300700_animation_067B8_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation067B8Bank4[40] = {
#include "assets/actor_300700_animation_067B8_bank4.inc"
};

static AnimationRecord _gActor300700Animation067B8Records[63] = {
#include "assets/actor_300700_animation_067B8_records.inc"
};

static u16 _gActor300700Animation067B8Indices[8] = {
#include "assets/actor_300700_animation_067B8_indices.inc"
};

static AnimationSet _gActor300700Animation067B8 = {
    _gActor300700Animation067B8Records,
    _gActor300700Animation067B8Indices,
    { NULL, _gActor300700Animation067B8Bank1, NULL, NULL, _gActor300700Animation067B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation06A34Bank1[10] = {
#include "assets/actor_300700_animation_06A34_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation06A34Bank4[46] = {
#include "assets/actor_300700_animation_06A34_bank4.inc"
};

static AnimationRecord _gActor300700Animation06A34Records[69] = {
#include "assets/actor_300700_animation_06A34_records.inc"
};

static u16 _gActor300700Animation06A34Indices[8] = {
#include "assets/actor_300700_animation_06A34_indices.inc"
};

static AnimationSet _gActor300700Animation06A34 = {
    _gActor300700Animation06A34Records,
    _gActor300700Animation06A34Indices,
    { NULL, _gActor300700Animation06A34Bank1, NULL, NULL, _gActor300700Animation06A34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation06D74Bank1[20] = {
#include "assets/actor_300700_animation_06D74_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation06D74Bank4[50] = {
#include "assets/actor_300700_animation_06D74_bank4.inc"
};

static AnimationRecord _gActor300700Animation06D74Records[84] = {
#include "assets/actor_300700_animation_06D74_records.inc"
};

static u16 _gActor300700Animation06D74Indices[8] = {
#include "assets/actor_300700_animation_06D74_indices.inc"
};

static AnimationSet _gActor300700Animation06D74 = {
    _gActor300700Animation06D74Records,
    _gActor300700Animation06D74Indices,
    { NULL, _gActor300700Animation06D74Bank1, NULL, NULL, _gActor300700Animation06D74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation06F08Bank1[5] = {
#include "assets/actor_300700_animation_06F08_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation06F08Bank4[20] = {
#include "assets/actor_300700_animation_06F08_bank4.inc"
};

static AnimationRecord _gActor300700Animation06F08Records[52] = {
#include "assets/actor_300700_animation_06F08_records.inc"
};

static u16 _gActor300700Animation06F08Indices[8] = {
#include "assets/actor_300700_animation_06F08_indices.inc"
};

static AnimationSet _gActor300700Animation06F08 = {
    _gActor300700Animation06F08Records,
    _gActor300700Animation06F08Indices,
    { NULL, _gActor300700Animation06F08Bank1, NULL, NULL, _gActor300700Animation06F08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation0719CBank1[11] = {
#include "assets/actor_300700_animation_0719C_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation0719CBank4[49] = {
#include "assets/actor_300700_animation_0719C_bank4.inc"
};

static AnimationRecord _gActor300700Animation0719CRecords[69] = {
#include "assets/actor_300700_animation_0719C_records.inc"
};

static u16 _gActor300700Animation0719CIndices[8] = {
#include "assets/actor_300700_animation_0719C_indices.inc"
};

static AnimationSet _gActor300700Animation0719C = {
    _gActor300700Animation0719CRecords,
    _gActor300700Animation0719CIndices,
    { NULL, _gActor300700Animation0719CBank1, NULL, NULL, _gActor300700Animation0719CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation074E0Bank1[18] = {
#include "assets/actor_300700_animation_074E0_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation074E0Bank4[54] = {
#include "assets/actor_300700_animation_074E0_bank4.inc"
};

static AnimationRecord _gActor300700Animation074E0Records[87] = {
#include "assets/actor_300700_animation_074E0_records.inc"
};

static u16 _gActor300700Animation074E0Indices[8] = {
#include "assets/actor_300700_animation_074E0_indices.inc"
};

static AnimationSet _gActor300700Animation074E0 = {
    _gActor300700Animation074E0Records,
    _gActor300700Animation074E0Indices,
    { NULL, _gActor300700Animation074E0Bank1, NULL, NULL, _gActor300700Animation074E0Bank4, NULL, NULL, NULL },
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

TaskDesc D_actor_300700_801693AC = { { { TASK_BODY_TMD, 96 } }, _ratTask, { .model = &gActor300700RatBody } };

AnimationSet* gRatAnimSets[11] = {
    NULL,
    &_gActor300700Animation05AEC,
    &_gActor300700Animation05DA8,
    &_gActor300700Animation06238,
    &_gActor300700Animation06578,
    &_gActor300700Animation067B8,
    &_gActor300700Animation06A34,
    &_gActor300700Animation06D74,
    &_gActor300700Animation06F08,
    &_gActor300700Animation0719C,
    &_gActor300700Animation074E0,
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

#include "../../shared/rat_behavior.inc.c"

#include "../../shared/rat_idle_sound.inc.c"

#include "../../shared/rat_hurt.inc.c"

#include "../../shared/rat_step.inc.c"

#include "../../shared/rat_animate.inc.c"

#include "../../shared/rat_update_color.inc.c"

#include "../../shared/rat_shadow.inc.c"

#include "../../shared/rat_squash.inc.c"
