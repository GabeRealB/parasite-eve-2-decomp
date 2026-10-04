#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
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

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// Per-actor work block, reached as `(Actor103700Work*)task->work`.
///
/// `field_24E` is the behaviour mode `Actor03700_Fn008D0` dispatches on and
/// `field_250` the phase within it. `field_248` is the animation requested and
/// `field_24A` the one playing, with `field_24C` counting its frames. The
/// movement helpers steer the root coordinate towards the target position
/// `field_23C` at turn rate `field_254` and forward speed `field_252`,
/// remembering the previous position in `field_22C`. `field_25E` and
/// `field_25C` are the bob and sway phases, `field_260` the ambient cue timer,
/// and `field_262` is set while the actor holds the player.
typedef struct Actor103700Work {
    /* 0x000 */ ActorAnimRig6         rig; // playback of the model's parts; slots 1 to 5 are driven
    /* 0x164 */ MATRIX                colorMtx;
    /* 0x184 */ MATRIX                lightMtx;
    /* 0x1A4 */ WorldCollisionBody    obj;
    /* 0x1C4 */ WorldCollisionContact records[4];
    /* 0x224 */ EffectSpawnArg        field_224; // hit-spark record for `func_800FDB18`
    /* 0x22C */ SVECTOR               field_22C;
    /* 0x234 */ SVECTOR               field_234;
    /* 0x23C */ SVECTOR               field_23C;
    /* 0x244 */ s16                   field_244;
    /* 0x246 */ s16                   field_246;
    /* 0x248 */ s16                   field_248;
    /* 0x24A */ s16                   field_24A;
    /* 0x24C */ s16                   field_24C;
    /* 0x24E */ s16                   field_24E;
    /* 0x250 */ s16                   field_250;
    /* 0x252 */ s16                   field_252;
    /* 0x254 */ s16                   field_254;
    /* 0x256 */ u16                   field_256;
    /* 0x258 */ u16                   field_258;
    /* 0x25A */ s16                   field_25A;
    /* 0x25C */ u16                   field_25C;
    /* 0x25E */ u16                   field_25E;
    /* 0x260 */ u16                   field_260;
    /* 0x262 */ s16                   field_262;
    /* 0x264 */ s16                   field_264;
    /* 0x266 */ s16                   field_266;
    /* 0x268 */ s16                   field_268;
    /* 0x26A */ s16                   field_26A;
    /* 0x26C */ u16                   field_26C;
} Actor103700Work;

/// 0x2C-byte scratch from the scratch stack used by `Actor03700_Fn03130`:
/// the 0x3F8 query buffer followed by the `AnimationPlayRequest` it sends as message 0x3FF.
typedef struct Actor103700HoldScratch {
    /* 0x00 */ GameActorButtonPressHold query;
    /* 0x18 */ AnimationPlayRequest     anim;
} Actor103700HoldScratch;
STATIC_ASSERT_SIZEOF(Actor103700HoldScratch, 0x2C);

extern u16 Actor03700_D07F7C[];

/// Halfword tables `Actor03700_Fn018C8` indexes by a 4-bit LCG draw:
/// the countdown seeded into `field_258` and `field_256`.
extern u16 Actor03700_D07F3C[];
extern u16 Actor03700_D07F5C[];

/// Pair `Actor03700_Fn01550` packs with `Gp_PackPair` for message 0x3F9.
extern DamageAttack Actor03700_D07F08;

/// Halfword table `Actor03700_Fn01550` indexes by a 4-bit LCG draw.
extern s16 Actor03700_D07F1C[];

/// Halfword bob table, one row of 15 per `arg1`: the row runs
/// 0, 10, 19, 24, 25, 22, 15, 5, -5, -15, -22, -25 before returning to 0.
/// Every use reads it as a signed halfword through `lh` and adds it to a
/// coordinate's Y translation, so it is the amplitude of an idle bob.
extern s16 Actor03700_D07F98[];

/// Halfword wave table `Actor03700_Fn03320` indexes by `field_25C`.
extern s16 Actor03700_D07FD4[];

/// 8-byte rise step: while `field_24C` is below `threshold` the Y and
/// forward displacements are spread over `steps` frames.
typedef struct Actor103700Rise {
    /* 0x0 */ s16 threshold;
    /* 0x2 */ s16 steps;
    /* 0x4 */ s16 dy;
    /* 0x6 */ s16 dist;
} Actor103700Rise;

extern Actor103700Rise Actor03700_D07FF4[];
extern Actor103700Rise Actor03700_D0802C[];

/// The enemy's parameters. The spawn stores them in `Enemy::param` and
/// seeds hit points from `hpMax`, which retail addresses as its own label.
extern EnemyParams Actor03700_D07F0C;

/// Animation-set table bound by `animationInitContext`, and the task's `field_24` table.
extern AnimationSet*    Actor03700_D080E4[6];
extern TaskMessageEntry Actor03700_D08108[2];

/// Animation-set table handed to the player as the 0x3FF payload's `source.sets`.
extern AnimationSet* Actor03700_D080FC[];

/// Halfword table indexed by the low 7 bits of a hit id; 3 cancels the damage.
extern s16 Actor03700_D08074[];

/// 0x18-byte scratch the approach helpers carve off the scratchpad stack: the
/// offset to the target and its `VectorNormalS`. `Actor03700_Fn029C0` never
/// gives it back; `Actor03700_Fn027DC` does.
typedef struct Actor103700SteerScratch {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ SVECTOR normal;
} Actor103700SteerScratch;
STATIC_ASSERT_SIZEOF(Actor103700SteerScratch, 0x18);

/* `D_80067704` selects the model stream the next `Gp_SpawnEff` builds its
 * `TmdObject` from. */
extern void* D_80067704[1];

/* The two model streams the death effect picks between, in this overlay's data. */
static TmdSource _gActor03700BatBurstWingRight;
static TmdSource _gActor03700BatBurstWingLeft;

static void Actor03700_Fn000A4(Enemy* arg0, Task* task);
static void Actor03700_Fn0042C(Task* task, TmdObject* arg1, s32 arg2);
static s32  Actor03700_Fn008D0(Task* task);
static void Actor03700_Fn00ABC(Task* task);
static void Actor03700_Fn00D5C(Task* task);
static void Actor03700_Fn00F88(Task* task);
static void Actor03700_Fn011B4(Task* task);
static void Actor03700_Fn01550(Task* task);
static void Actor03700_Fn018C8(Task* task);
static void Actor03700_Fn01C94(Task* task);
static s32  Actor03700_Fn01DFC(Task* task);
static void Actor03700_Fn01F48(Task* task);
static void Actor03700_Fn020D4(Enemy* enemy, Task* task);
static void Actor03700_Fn025C8(Task* task);
static void Actor03700_Fn027DC(Task* task);
static void Actor03700_Fn029C0(Task* task);
static void Actor03700_Fn03004(Enemy* enemy, Task* task);
static s32  Actor03700_Fn03130(Task* task);
static void Actor03700_Fn0321C(Task* task);
static void Actor03700_Fn032BC(Task* task, s32 arg1, s32 arg2);
static void Actor03700_Fn03320(Task* task, s32 arg1);
static void Actor03700_Fn033F0(Task* task);
static void Actor03700_Fn034A0(Task* task);
static void Actor03700_Fn0355C(Task* task);

static void Actor03700_Fn02FA8(Task*);

s32 Actor03700_Fn034F8(Task*, s32, s32, s32);

static TmdBone _gActor03700BatBodySkeleton[6] = {
#include "assets/bat_body_skeleton.inc"
};

static u32 _gActor03700BatBodyPartVerts[6] = {
#include "assets/bat_body_partVerts.inc"
};

static SVECTOR _gActor03700BatBodyVerts[74] = {
#include "assets/bat_body_verts.inc"
};

static SVECTOR _gActor03700BatBodyNormals[61] = {
#include "assets/bat_body_normals.inc"
};

static u32 _gActor03700BatBodyStream[424] = {
#include "assets/bat_body_stream.inc"
};

static TmdSource _gActor03700BatBody = {
    0,
    2184,
    480,
    6,
    _gActor03700BatBodyPartVerts,
    _gActor03700BatBodyVerts,
    _gActor03700BatBodyNormals,
    _gActor03700BatBodySkeleton,
    _gActor03700BatBodyStream,
};

static TmdBone _gActor03700BatBurstWingRightSkeleton[1] = {
#include "assets/bat_burst_wing_right_skeleton.inc"
};

static u32 _gActor03700BatBurstWingRightPartVerts[1] = {
#include "assets/bat_burst_wing_right_partVerts.inc"
};

static SVECTOR _gActor03700BatBurstWingRightVerts[16] = {
#include "assets/bat_burst_wing_right_verts.inc"
};

static SVECTOR _gActor03700BatBurstWingRightNormals[11] = {
#include "assets/bat_burst_wing_right_normals.inc"
};

static u32 _gActor03700BatBurstWingRightStream[54] = {
#include "assets/bat_burst_wing_right_stream.inc"
};

static TmdSource _gActor03700BatBurstWingRight = {
    0,
    320,
    0,
    1,
    _gActor03700BatBurstWingRightPartVerts,
    _gActor03700BatBurstWingRightVerts,
    _gActor03700BatBurstWingRightNormals,
    _gActor03700BatBurstWingRightSkeleton,
    _gActor03700BatBurstWingRightStream,
};

static TmdBone _gActor03700BatBurstWingLeftSkeleton[1] = {
#include "assets/bat_burst_wing_left_skeleton.inc"
};

static u32 _gActor03700BatBurstWingLeftPartVerts[1] = {
#include "assets/bat_burst_wing_left_partVerts.inc"
};

static SVECTOR _gActor03700BatBurstWingLeftVerts[16] = {
#include "assets/bat_burst_wing_left_verts.inc"
};

static SVECTOR _gActor03700BatBurstWingLeftNormals[12] = {
#include "assets/bat_burst_wing_left_normals.inc"
};

static u32 _gActor03700BatBurstWingLeftStream[54] = {
#include "assets/bat_burst_wing_left_stream.inc"
};

static TmdSource _gActor03700BatBurstWingLeft = {
    0,
    320,
    0,
    1,
    _gActor03700BatBurstWingLeftPartVerts,
    _gActor03700BatBurstWingLeftVerts,
    _gActor03700BatBurstWingLeftNormals,
    _gActor03700BatBurstWingLeftSkeleton,
    _gActor03700BatBurstWingLeftStream,
};

static AnimationPackedPose _gActor03700Actor103700Animation047C4Bank1[8] = {
#include "assets/actor_103700_animation_047C4_bank1.inc"
};

static AnimationPackedRotation _gActor03700Actor103700Animation047C4Bank4[28] = {
#include "assets/actor_103700_animation_047C4_bank4.inc"
};

static AnimationRecord _gActor03700Actor103700Animation047C4Records[49] = {
#include "assets/actor_103700_animation_047C4_records.inc"
};

static u16 _gActor03700Actor103700Animation047C4Indices[6] = {
#include "assets/actor_103700_animation_047C4_indices.inc"
};

static AnimationSet _gActor03700Actor103700Animation047C4 = {
    _gActor03700Actor103700Animation047C4Records,
    _gActor03700Actor103700Animation047C4Indices,
    { NULL, _gActor03700Actor103700Animation047C4Bank1, NULL, NULL, _gActor03700Actor103700Animation047C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03700Actor103700Animation04AC0Bank1[19] = {
#include "assets/actor_103700_animation_04AC0_bank1.inc"
};

static AnimationPackedRotation _gActor03700Actor103700Animation04AC0Bank4[18] = {
#include "assets/actor_103700_animation_04AC0_bank4.inc"
};

static AnimationRecord _gActor03700Actor103700Animation04AC0Records[103] = {
#include "assets/actor_103700_animation_04AC0_records.inc"
};

static u16 _gActor03700Actor103700Animation04AC0Indices[6] = {
#include "assets/actor_103700_animation_04AC0_indices.inc"
};

static AnimationSet _gActor03700Actor103700Animation04AC0 = {
    _gActor03700Actor103700Animation04AC0Records,
    _gActor03700Actor103700Animation04AC0Indices,
    { NULL, _gActor03700Actor103700Animation04AC0Bank1, NULL, NULL, _gActor03700Actor103700Animation04AC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03700Actor103700Animation04C58Bank1[7] = {
#include "assets/actor_103700_animation_04C58_bank1.inc"
};

static AnimationPackedRotation _gActor03700Actor103700Animation04C58Bank4[24] = {
#include "assets/actor_103700_animation_04C58_bank4.inc"
};

static AnimationRecord _gActor03700Actor103700Animation04C58Records[44] = {
#include "assets/actor_103700_animation_04C58_records.inc"
};

static u16 _gActor03700Actor103700Animation04C58Indices[6] = {
#include "assets/actor_103700_animation_04C58_indices.inc"
};

static AnimationSet _gActor03700Actor103700Animation04C58 = {
    _gActor03700Actor103700Animation04C58Records,
    _gActor03700Actor103700Animation04C58Indices,
    { NULL, _gActor03700Actor103700Animation04C58Bank1, NULL, NULL, _gActor03700Actor103700Animation04C58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03700Actor103700Animation05448Bank1[49] = {
#include "assets/actor_103700_animation_05448_bank1.inc"
};

static AnimationPackedRotation _gActor03700Actor103700Animation05448Bank4[126] = {
#include "assets/actor_103700_animation_05448_bank4.inc"
};

static AnimationRecord _gActor03700Actor103700Animation05448Records[222] = {
#include "assets/actor_103700_animation_05448_records.inc"
};

static u16 _gActor03700Actor103700Animation05448Indices[6] = {
#include "assets/actor_103700_animation_05448_indices.inc"
};

static AnimationSet _gActor03700Actor103700Animation05448 = {
    _gActor03700Actor103700Animation05448Records,
    _gActor03700Actor103700Animation05448Indices,
    { NULL, _gActor03700Actor103700Animation05448Bank1, NULL, NULL, _gActor03700Actor103700Animation05448Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03700Actor103700Animation05CE8Bank1[61] = {
#include "assets/actor_103700_animation_05CE8_bank1.inc"
};

static AnimationPackedRotation _gActor03700Actor103700Animation05CE8Bank4[132] = {
#include "assets/actor_103700_animation_05CE8_bank4.inc"
};

static AnimationRecord _gActor03700Actor103700Animation05CE8Records[224] = {
#include "assets/actor_103700_animation_05CE8_records.inc"
};

static u16 _gActor03700Actor103700Animation05CE8Indices[6] = {
#include "assets/actor_103700_animation_05CE8_indices.inc"
};

static AnimationSet _gActor03700Actor103700Animation05CE8 = {
    _gActor03700Actor103700Animation05CE8Records,
    _gActor03700Actor103700Animation05CE8Indices,
    { NULL, _gActor03700Actor103700Animation05CE8Bank1, NULL, NULL, _gActor03700Actor103700Animation05CE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03700Actor103700Animation06B4CBank1[31] = {
#include "assets/actor_103700_animation_06B4C_bank1.inc"
};

static AnimationPackedRotation _gActor03700Actor103700Animation06B4CBank4[344] = {
#include "assets/actor_103700_animation_06B4C_bank4.inc"
};

static AnimationRecord _gActor03700Actor103700Animation06B4CRecords[464] = {
#include "assets/actor_103700_animation_06B4C_records.inc"
};

static u16 _gActor03700Actor103700Animation06B4CIndices[20] = {
#include "assets/actor_103700_animation_06B4C_indices.inc"
};

static AnimationSet _gActor03700Actor103700Animation06B4C = {
    _gActor03700Actor103700Animation06B4CRecords,
    _gActor03700Actor103700Animation06B4CIndices,
    { NULL, _gActor03700Actor103700Animation06B4CBank1, NULL, NULL, _gActor03700Actor103700Animation06B4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03700Actor103700Animation07EE0Bank1[36] = {
#include "assets/actor_103700_animation_07EE0_bank1.inc"
};

static AnimationPackedRotation _gActor03700Actor103700Animation07EE0Bank4[522] = {
#include "assets/actor_103700_animation_07EE0_bank4.inc"
};

static AnimationRecord _gActor03700Actor103700Animation07EE0Records[603] = {
#include "assets/actor_103700_animation_07EE0_records.inc"
};

static u16 _gActor03700Actor103700Animation07EE0Indices[20] = {
#include "assets/actor_103700_animation_07EE0_indices.inc"
};

static AnimationSet _gActor03700Actor103700Animation07EE0 = {
    _gActor03700Actor103700Animation07EE0Records,
    _gActor03700Actor103700Animation07EE0Indices,
    { NULL, _gActor03700Actor103700Animation07EE0Bank1, NULL, NULL, _gActor03700Actor103700Animation07EE0Bank4, NULL, NULL, NULL },
};

DamageAttack Actor03700_D07F08 = { 3, 7 };

EnemyParams Actor03700_D07F0C = { &Actor03700_D07F08, 1, 5, 18, 1, 100, 0, 100, 99 };

s16 Actor03700_D07F1C[16] = {
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
    50,
    52,
    54,
    56,
    58,
    60,
};

u16 Actor03700_D07F3C[16] = {
    10,
    12,
    14,
    16,
    18,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
};

u16 Actor03700_D07F5C[16] = {
    245,
    250,
    255,
    260,
    265,
    270,
    275,
    280,
    285,
    290,
    295,
    300,
    295,
    300,
    305,
    310,
};

u16 Actor03700_D07F7C[8] = {
    16,
    20,
    24,
    28,
    36,
    44,
    48,
    48,
};

TaskDesc Actor03700_D07F8C = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, Actor03700_Fn02FA8, { .model = &_gActor03700BatBody } };

s16 Actor03700_D07F98[30] = {
    0,
    10,
    19,
    24,
    25,
    22,
    15,
    5,
    -5,
    -15,
    -22,
    -25,
    -24,
    -19,
    -10,
    0,
    20,
    38,
    48,
    50,
    44,
    30,
    10,
    -10,
    -30,
    -44,
    -50,
    -48,
    -38,
    -20,
};

s16 Actor03700_D07FD4[16] = {
    4096,
    3742,
    2741,
    1267,
    -426,
    -2046,
    -3312,
    -4006,
    -4007,
    -3316,
    -2052,
    -433,
    1261,
    2737,
    3739,
    0,
};

Actor103700Rise Actor03700_D07FF4[7] = {
    { 20, 21, 0, 0 },
    { 23, 3, 45, 5 },
    { 26, 3, 170, 47 },
    { 30, 4, 105, 189 },
    { 35, 5, -60, 70 },
    { 40, 5, -10, 50 },
    { 50, 10, 0, 0 },
};

Actor103700Rise Actor03700_D0802C[9] = {
    { 10, 11, 0, 0 },
    { 17, 7, 0, -20 },
    { 22, 5, 4, -276 },
    { 27, 5, 88, -97 },
    { 32, 5, -48, -47 },
    { 37, 5, 16, -26 },
    { 40, 3, -42, -7 },
    { 45, 5, -19, -5 },
    { 50, 5, 0, 0 },
};

s16 Actor03700_D08074[56] = {
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    3,
    0,
    0,
    0,
    0,
    0,
    0,
};

AnimationSet* Actor03700_D080E4[6] = {
    NULL,
    &_gActor03700Actor103700Animation047C4,
    &_gActor03700Actor103700Animation04AC0,
    &_gActor03700Actor103700Animation04C58,
    &_gActor03700Actor103700Animation05448,
    &_gActor03700Actor103700Animation05CE8,
};

AnimationSet* Actor03700_D080FC[3] = {
    NULL,
    &_gActor03700Actor103700Animation06B4C,
    &_gActor03700Actor103700Animation07EE0,
};

TaskMessageEntry Actor03700_D08108[2] = {
    { 2014, Actor03700_Fn034F8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static inline void Actor03700_BobInline(Task* task, s32 arg1, s32 arg2);
static inline void Actor03700_SwayInline(Task* task, s32 arg1);
static inline void _actor03700UpdateColor(Task* task);
static inline void _actor03700SpawnRemains(Task* task);

/// The bob step of `Actor03700_Fn032BC`, which `Actor03700_Fn029C0` carries
/// expanded in place rather than as a call.
static inline void Actor03700_BobInline(Task* task, s32 arg1, s32 arg2)
{
    Actor103700Work* work;
    GfxCoord*        coord;
    u16              frame;

    work  = (Actor103700Work*)task->work;
    coord = task->extra.tmd->coords;

    frame           = work->field_25E + 1;
    work->field_25E = frame;
    if (arg2 < (s16)frame) {
        work->field_25E = 0;
    }
    coord->coord.t[1] += Actor03700_D07F98[(arg1 * 15) + (s16)work->field_25E];
}

/// The sway step of `Actor03700_Fn03320`, expanded in place the same way.
static inline void Actor03700_SwayInline(Task* task, s32 arg1)
{
    Actor103700Work* work;
    GfxCoord*        coord;
    u16              frame;
    s32              amp;

    work  = (Actor103700Work*)task->work;
    coord = task->extra.tmd->coords;

    frame           = work->field_25C + 1;
    work->field_25C = frame;
    if ((s16)frame >= 15) {
        work->field_25C = 0;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_25A = arg1 + ((gRandomLcgState >> 16) & 0x3F);
    }
    amp                = (work->field_25A * Actor03700_D07FD4[(s16)work->field_25C] * 16) >> 16;
    coord->coord.t[0] += (amp * coord->coord.m[0][0]) >> 12;
    coord->coord.t[2] += (amp * coord->coord.m[2][0]) >> 12;
}

/// The enemy's state handlers, run by `Actor03700_Fn02FA8` for the task's
/// state: spawn, per-frame tick and death.
static const EnemyTaskFuncTable3 Actor03700_D00004 = {
    { Actor03700_Fn000A4, Actor03700_Fn03004, Actor03700_Fn020D4 },
};

/// Spawn handler. Allocates the 0x270-byte work block onto the task, points the
/// model at its light/colour matrices and links the enemy node. The model
/// variant (`AreaPlacement::mode`) picks the mode: tens digit 0 allocates
/// the model buffers and takes the units digit (0..2) as the pose, nudging the
/// root coordinate for poses 1 and 2; 1..3 set `TMD_OBJECT_SKIP_AUTO_BUFFER` and mode 7 or 10.
/// The animation slots then get a shared random phase, and the collision object
/// is linked with its four records before the task moves to state 1.
static void Actor03700_Fn000A4(Enemy* arg0, Task* task)
{
    TmdObject*       obj;
    GfxCoord*        coord;
    Actor103700Work* work;
    s32              kind;
    s32              i;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x270, 0);
    if (work == NULL) {
        enemyDestroy(arg0, task);
        return;
    }
    task->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->param                  = &Actor03700_D07F0C;
    arg0->coord                  = coord;
    arg0->node.state.parts.flags = 0;
    arg0->bodyPos.vx             = 0;
    arg0->bodyPos.vy             = 0;
    arg0->bodyPos.vz             = 0;
    arg0->recs                   = work->records;
    work->field_224.coord        = &task->extra.tmd->coords[1];
    work->field_224.spawnArgLo   = 0x100;
    work->field_224.spawnArgHi   = 1;
    work->field_246              = arg0->place->yaw;
    kind                         = arg0->place->mode;
    switch (kind / 10) {
        case 0:
            Tmd_AllocBuffers(obj);
            if (kind < 3) {
                work->field_24E = kind;
            } else {
                work->field_24E = 0;
            }
            work->field_248 = kind < 3 ? kind + 1 : 1;
            switch (work->field_24E) {
                case 1:
                    work->field_248    = 2;
                    coord->coord.t[1] += 0x50;
                    break;
                case 2:
                    work->field_248    = 3;
                    coord->coord.t[2] -= 0x55;
                    break;
                case 0:
                    work->field_248 = 1;
                    break;
            }
            break;
        case 1:
        case 2:
            obj->flags     |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->field_24E = 7;
            work->field_248 = 1;
            if (kind == 10) {
                work->field_266 = 1;
            }
            break;
        case 3:
            obj->flags     |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->field_24E = 10;
            work->field_248 = 1;
            break;
    }
    arg0->hp        = Actor03700_D07F0C.hpMax;
    work->field_24A = work->field_248;
    task->msgTable  = Actor03700_D08108;
    animationInitContext(&work->rig.anim, Actor03700_D080E4, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationResetSlot(&work->rig.anim, i, work->field_248);
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    kind            = (gRandomLcgState >> 16) & 3;
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        work->rig.slots[i].rate += kind;
    }
    (Gp_IncStateF0Ref)(0);
    work->field_234.vx         = coord->coord.t[0];
    work->field_234.vy         = coord->coord.t[1];
    work->field_234.vz         = coord->coord.t[2];
    work->obj.radius           = 0xC8;
    work->obj.coord            = coord;
    work->obj.context.contacts = work->records;
    work->obj.pos.vx           = 0;
    work->obj.pos.vy           = 0;
    work->obj.pos.vz           = 0;
    work->obj.key              = 0x30025;
    work->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(work->records, 4, 0);
    work->obj.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->state      = 1;
}

/// Collision step. Applies the `func_800E0C10` push-back to the root coordinate
/// (a delta for 1, an absolute reset to `field_22C` for 2), then walks the four
/// records: kind 1 records push the actor out along the deepest overlap, kind 2
/// records are hits from a player slot, which take damage, spawn the hit spark
/// and move the actor into its flinch / knockdown modes.
static void Actor03700_Fn0042C(Task* task, TmdObject* arg1, s32 arg2)
{
    ActorPushFrame*  scratch;
    GfxCoord*        coord;
    GfxCoord*        src;
    Actor103700Work* work;
    s32              push;
    s32              reach;
    s32              res;
    s32              i;
    s32              z;
    s32              val;
    s32              ex;
    s32              ey;
    s32              ez;
    s32              broke;
    u32              id;
    u32              damage;

    push    = 0;
    broke   = 0;
    work    = (Actor103700Work*)task->work;
    scratch = (ActorPushFrame*)SCRATCH_STACK_RESERVE_BYTES(0x58);
    coord   = task->extra.tmd->coords;
    res     = func_800E0C10(work->records, &scratch->delta, 4, NULL);
    if (res == 1)
        goto move_delta;
    if (res < 2)
        goto move_done;
    if (res == 2)
        goto move_absolute;
    goto move_done;
move_delta:
    coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
    coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
    z                  = coord->coord.t[2] + scratch->delta.fixed.vz.halves.integer;
    goto move_z;
move_absolute:
    coord->coord.t[0] = work->field_22C.vx;
    coord->coord.t[1] = work->field_22C.vy;
    z                 = work->field_22C.vz;
move_z:
    coord->coord.t[2] = z;
move_done:
    i               = 0;
    work->field_264 = 0;
    do {
        id = work->records[i].key.value;
        switch (id >> 16) {
            case 0:
                break;
            case 1:
                work->field_264          = id >> 16;
                scratch->delta.vector.vx = coord->workm.t[0] - work->records[i].point.vx;
                scratch->delta.vector.vy = coord->workm.t[1] - work->records[i].point.vy;
                scratch->delta.vector.vz = coord->workm.t[2] - work->records[i].point.vz;
                reach                    = work->records[i].distance - SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz);
                val                      = reach;
                if (reach <= 0) {
                    val = 0;
                }
                reach = val;
                if (push < reach) {
                    push = reach;
                    VectorNormal(&scratch->delta.vector, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->dir);
                }
                break;
            case 2:
                src                      = gPlayerActorTasks[(id >> 7) & 1]->extra.tmd->coords;
                ex                       = src->coord.t[0] - coord->coord.t[0];
                scratch->delta.vector.vx = ex;
                ey                       = src->coord.t[1] - coord->coord.t[1];
                scratch->delta.vector.vy = ey;
                ez                       = src->coord.t[2] - coord->coord.t[2];
                scratch->delta.vector.vz = ez;
                damage                   = Gp_ComputeDamage(work->records[i].key.value, SquareRoot0(ex * ex + ey * ey + ez * ez), 0, 0);
                id                       = work->records[i].key.value;
                if (id & 0x8000) {
                    if (Actor03700_D08074[id & 0x7F] == 3) {
                        broke  = 1;
                        damage = 0;
                    } else {
                        broke = Gp_GetIdParam1(id) & 0xFFFF;
                        if ((u32)(broke - 0xC) < 2) {
                            func_800FDB18(broke, coord, NULL, &work->field_224);
                        }
                        work->field_268 = Actor03700_D08074[work->records[i].key.value & 0x7F];
                        broke           = 0;
                    }
                } else {
                    work->field_268 = (Gp_GetIdParam1(id) & 0xFFFF) == 7;
                }
                func_800DA6E8(&((Enemy*)task->spawnArg2.pointer)->node, damage, 0);
                func_800E2C78(task->spawnArg2.pointer, work->records[i].key.value, damage, 0);
                if ((s32)damage > 0) {
                    ((Enemy*)task->spawnArg2.pointer)->hp = 0;
                    work->field_24E                       = 6;
                    work->field_250                       = 0;
                    task->state                           = 2;
                } else if (((Gp_GetIdParam0(work->records[i].key.value) & 0xFFFF) == 8 || broke == 1) &&
                           (u16)(work->field_24E - 1) >= 2) {
                    if (work->field_262 == 0) {
                        work->field_24E = 5;
                        work->field_250 = 3;
                    } else {
                        work->field_24E = 11;
                        work->field_250 = 0;
                    }
                }
                break;
        }
    } while (++i < 4);
    if (push > 0) {
        coord->coord.t[0] += (push * scratch->dir.vx) >> 12;
        coord->coord.t[2] += (push * scratch->dir.vz) >> 12;
    }
    Gp_ClearRec18Occupied(work->records);
    SCRATCH_STACK_RELEASE_BYTES(0x58);
}

/// The tick's mode dispatcher: runs the handler for the work block's mode
/// `field_24E`. Modes 0, 3, 4, 5, 8 and 9 (and 10 while `field_250` is set)
/// also count `field_260` up and replay the ambient cue from the placement's
/// sound bank every 16 frames. Mode 6 clears `field_250`, mode 7 releases the
/// contact records and runs the descent in `Actor03700_Fn025C8`; both report 1,
/// which tells the caller to skip this frame's movement.
static s32 Actor03700_Fn008D0(Task* task)
{
    s16              state;
    GfxCoord*        object;
    s32              ret;
    u16              timer;
    Actor103700Work* soundWork;
    Actor103700Work* work;

    work  = (Actor103700Work*)task->work;
    state = work->field_24E;
    ret   = 0;
    /* Each sound block needs separate locals to preserve the call scheduling. */
    switch (state) {
        case 0:
            Actor03700_Fn00ABC(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = task->extra.tmd->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)worldCoordGetOriginAudioPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
            }
            return ret;
        case 1:
            Actor03700_Fn00D5C(task);
            return ret;
        case 2:
            Actor03700_Fn00F88(task);
            return ret;
        case 3:
            Actor03700_Fn011B4(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = task->extra.tmd->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)worldCoordGetOriginAudioPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
            }
            return ret;
        case 4:
            Actor03700_Fn01550(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = task->extra.tmd->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)worldCoordGetOriginAudioPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
            }
            return ret;
        case 5:
            Actor03700_Fn018C8(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = task->extra.tmd->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)worldCoordGetOriginAudioPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
            }
            return ret;
        case 6:
            work->field_250 = 0;
            ret             = 1;
            break;
        case 7:
            Gp_ClearRec18Occupied(work->records);
            if (work->field_266 != 0) {
                Actor03700_Fn0355C(task);
            }
            Actor03700_Fn025C8(task);
            ret = 1;
            break;
        case 8:
            if (work->field_266 != 0) {
                Actor03700_Fn0355C(task);
            }
            Actor03700_Fn027DC(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = task->extra.tmd->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)worldCoordGetOriginAudioPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
            }
            return ret;
        case 9:
            Actor03700_Fn027DC(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = task->extra.tmd->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)worldCoordGetOriginAudioPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
            }
            return ret;
        case 10:
            Actor03700_Fn029C0(task);
            if (work->field_250 != 0) {
                soundWork            = (Actor103700Work*)task->work;
                object               = task->extra.tmd->coords;
                timer                = soundWork->field_260 + 1;
                soundWork->field_260 = timer;
                if ((s16)timer < 0x10) {
                    return ret;
                }
                soundWork->field_260 = 0;
                {
                    u32 soundId;
                    s32 pan;
                    soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
                    soundId >>= 0xC;
                    soundId <<= 8;
                    soundId  |= 0x40250005;
                    pan       = (s8)worldCoordGetOriginAudioPan(object);
                    SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
                }
            }
            return ret;
        case 11:
            Actor03700_Fn01C94(task);
            break;
    }
    return ret;
}

static void Actor03700_Fn00ABC(Task* task)
{
    Actor103700Work* work;
    GfxCoord*        coord;
    SVECTOR*         head;
    SVECTOR*         vec;
    s16              angle;
    s32              dist;

    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    vec                           = head - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = vec;
    work                          = (Actor103700Work*)task->work;
    coord                         = task->extra.tmd->coords;

    switch (work->field_250) {
        case 0:
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_252    = ((gRandomLcgState >> 16) & 0xF) + 20;
            work->field_254    = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            angle              = (gRandomLcgState >> 16) & 0xFFF;
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            dist               = (gRandomLcgState >> 16) & 0x1FF;
            work->field_23C.vx = work->field_234.vx + ((dist * rsin(angle)) >> 12);
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_23C.vy = work->field_234.vy + ((gRandomLcgState >> 16) & 0x1FF);
            work->field_23C.vz = work->field_234.vz + ((dist * rcos(angle)) >> 12);
            vec->vx            = work->field_23C.vx - coord->coord.t[0];
            vec->vy            = 0;
            vec->vz            = work->field_23C.vz - coord->coord.t[2];
            work->field_244    = ratan2(vec->vx, vec->vz) & 0xFFF;
            work->field_250    = 1;
            break;
        case 1:
            vec->vx = work->field_23C.vx - coord->coord.t[0];
            vec->vz = work->field_23C.vz - coord->coord.t[2];
            if ((s16)SquareRoot0(vec->vx * vec->vx + vec->vz * vec->vz) < 120) {
                work->field_250 = 0;
            }
            break;
    }
    Actor03700_Fn032BC(task, 0, 14);
    Actor03700_Fn03320(task, 20);
    if (Actor03700_Fn01DFC(task) != 0) {
        work->field_24E                    = 3;
        work->field_250                    = 0;
        gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

static void Actor03700_Fn00D5C(Task* task)
{
    Actor103700Work* work;
    GfxCoord*        coord;
    s32              dist;
    s32              i;

    work  = (Actor103700Work*)task->work;
    coord = task->extra.tmd->coords;

    switch (work->field_250) {
        case 0:
            if (Actor03700_Fn01DFC(task) != 0) {
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
                gRandomLcgState                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_250                    = 1;
                work->field_256                    = (gRandomLcgState >> 16) & 0x3F;
            }
            break;
        case 1:
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
                work->field_256 = 0;
                work->field_248 = 4;
            }
            break;
        case 2:
            for (i = 0; i < 7; i++) {
                if (Actor03700_D07FF4[i].threshold >= work->field_24C) {
                    coord->coord.t[1] += Actor03700_D07FF4[i].dy / Actor03700_D07FF4[i].steps;
                    dist               = Actor03700_D07FF4[i].dist / Actor03700_D07FF4[i].steps;
                    coord->coord.t[0] += (rsin(work->field_246) * dist) >> 12;
                    coord->coord.t[2] += (rcos(work->field_246) * dist) >> 12;
                    break;
                }
            }
            if (work->field_24C >= 50) {
                work->field_248                    = 1;
                work->field_24E                    = 3;
                work->field_250                    = 0;
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
            }
            break;
    }
}

static void Actor03700_Fn00F88(Task* task)
{
    Actor103700Work* work;
    GfxCoord*        coord;
    s32              dist;
    s32              i;

    work  = (Actor103700Work*)task->work;
    coord = task->extra.tmd->coords;

    switch (work->field_250) {
        case 0:
            if (Actor03700_Fn01DFC(task) != 0) {
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
                gRandomLcgState                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_250                    = 1;
                work->field_256                    = (gRandomLcgState >> 16) & 0x3F;
            }
            break;
        case 1:
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
                work->field_256 = 0;
                work->field_248 = 5;
            }
            break;
        case 2:
            for (i = 0; i < 9; i++) {
                if (Actor03700_D0802C[i].threshold >= work->field_24C) {
                    coord->coord.t[1] += Actor03700_D0802C[i].dy / Actor03700_D0802C[i].steps;
                    dist               = Actor03700_D0802C[i].dist / Actor03700_D0802C[i].steps;
                    coord->coord.t[0] += (rsin(work->field_246) * dist) >> 12;
                    coord->coord.t[2] += (rcos(work->field_246) * dist) >> 12;
                    break;
                }
            }
            if (work->field_24C >= 50) {
                work->field_248                    = 1;
                work->field_24E                    = 3;
                work->field_250                    = 0;
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
            }
            break;
    }
}

static void Actor03700_Fn011B4(Task* task)
{
    Actor103700Work* work;
    GfxCoord*        coord;
    void*            head;
    SVECTOR*         vec;
    s32              i;
    s32              sound;
    s8               slot;

    coord                      = task->extra.tmd->coords;
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = (u8*)head - sizeof(SVECTOR);
    work                       = (Actor103700Work*)task->work;
    vec                        = SCRATCH_STACK_CURSOR(void);

    switch (work->field_250) {
        case 0:
            work->field_23C.vx = gPlayerStatus.coordMtx->t[0];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_23C.vy = gPlayerStatus.coordMtx->t[1] - (((gRandomLcgState >> 16) & 0x3FF) + 800);
            work->field_23C.vz = gPlayerStatus.coordMtx->t[2];
            if (work->field_26A == 0) {
                work->field_252 = 5;
            } else {
                work->field_252 = 0;
                work->field_26A = 0;
            }
            work->field_254 = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            Actor03700_Fn03320(task, 20);
            if (work->field_246 == work->field_244) {
                work->field_250 = 1;
                work->field_252 = Actor03700_D07F1C[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                coord           = task->extra.tmd->coords;
                vec->vx         = work->field_23C.vx - coord->coord.t[0];
                vec->vy         = work->field_23C.vy - coord->coord.t[1];
                vec->vz         = work->field_23C.vz - coord->coord.t[2];
                work->field_256 = SquareRoot0(vec->vx * vec->vx + vec->vy * vec->vy + vec->vz * vec->vz) / work->field_252;
                slot            = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3) + 17;
                for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
                    work->rig.slots[i].rate = slot;
                }
                coord = task->extra.tmd->coords;
                sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40250002;
                SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 1:
            Actor03700_Fn03320(task, 40);
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 0;
                work->field_256 = 0;
                work->field_26A = 1;
            }
            if (work->field_264 != 0) {
                if (Actor03700_Fn03130(task) == 0) {
                    func_800FDB18(1, coord, NULL, &work->field_224);
                    work->field_24E = 5;
                } else {
                    work->field_24E = 4;
                }
                work->field_250 = 0;
            }
            if (gSceneCombatState.actor03700Flags & SCENE_COMBAT_ACTOR03700_PLAYER_RELEASE) {
                work->field_24E = 5;
                work->field_248 = 1;
                work->field_250 = 0;
            }
            break;
    }
    Actor03700_Fn032BC(task, 0, 14);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
}

static void Actor03700_Fn01550(Task* task)
{
    Actor103700Work*      work;
    GfxCoord*             obj;
    Task*                 player;
    void*                 head;
    AnimationPlayRequest* arg;
    s32                   sound;

    work                       = (Actor103700Work*)task->work;
    obj                        = task->extra.tmd->coords;
    player                     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = (u8*)head - 0x1C;
    arg                        = SCRATCH_STACK_CURSOR(AnimationPlayRequest);

    switch (work->field_250) {
        case 0:
            taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackPair(&Actor03700_D07F08, 0), 0);
            func_800FDB18(1, obj, NULL, &work->field_224);
            Gp_SpawnPadLerp(5, 0xC0, 8);
            sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40250004;
            SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
            if ((s16)++work->field_26C >= 6) {
                work->field_26C                    = 0;
                work->field_250                    = 3;
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_PLAYER_RELEASE;
            } else {
                work->field_250 = 1;
                work->field_256 = 20;
            }
            break;
        case 1:
            work->field_252 = -20;
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
            }
            break;
        case 2:
            work->field_23C.vx = gPlayerStatus.coordMtx->t[0];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_23C.vy = gPlayerStatus.coordMtx->t[1] - (((gRandomLcgState >> 16) & 0x3FF) + 800);
            work->field_23C.vz = gPlayerStatus.coordMtx->t[2];
            work->field_252    = Actor03700_D07F1C[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            work->field_254    = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            if (work->field_264 != 0) {
                work->field_250 = 0;
            }
            break;
        case 3:
            arg->source.sets          = Actor03700_D080FC;
            arg->animationId          = 2;
            arg->blend                = ANIMATION_BLEND_RESET;
            arg->blendFrames          = 0;
            arg->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, arg, 0);
            sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
            SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
            work->field_250 = 4;
            break;
        case 4:
            if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                work->field_262                    = 0;
                work->field_24E                    = 5;
                work->field_250                    = 0;
                gSceneCombatState.actor03700Flags &= SCENE_COMBAT_ACTOR03700_ALERT;
            }
            break;
    }
    Actor03700_Fn032BC(task, 1, 14);
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

static void Actor03700_Fn018C8(Task* task)
{
    Actor103700Work* work;
    GfxCoord*        obj;
    s32              period;
    s32              i;
    s32              slot;
    s32              sound;

    work   = (Actor103700Work*)task->work;
    obj    = task->extra.tmd->coords;
    period = 14;

    switch (work->field_250) {
        case 0:
            work->field_250 = 1;
            work->field_256 = 30;
            work->field_258 = Actor03700_D07F3C[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            work->field_26C = 0;
            slot            = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3) + 10;
            for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
                work->rig.slots[i].rate = slot;
            }
            break;
        case 1:
            work->field_23C.vx = gPlayerStatus.coordMtx->t[0];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_23C.vy = gPlayerStatus.coordMtx->t[1] - (((gRandomLcgState >> 16) & 0x3FF) + 800);
            work->field_23C.vz = gPlayerStatus.coordMtx->t[2];
            work->field_254    = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            if ((s16)--work->field_258 > 0) {
                work->field_252 = -50;
            } else {
                work->field_252 = 0;
                Actor03700_Fn03320(task, 20);
            }
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
                work->field_256 = 0;
                work->field_252 = 0;
            }
            break;
        case 2:
            work->field_23C.vx = gPlayerStatus.coordMtx->t[0];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_23C.vy = gPlayerStatus.coordMtx->t[1] - (((gRandomLcgState >> 16) & 0x1FF) + 800);
            work->field_23C.vz = gPlayerStatus.coordMtx->t[2];
            work->field_254    = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            work->field_252    = 5;
            Actor03700_Fn03320(task, 20);
            if ((s16)++work->field_256 >= 91) {
                work->field_256 = 0;
                work->field_24E = 3;
                work->field_250 = 0;
            }
            break;
        case 3:
            work->field_256 = Actor03700_D07F5C[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            work->field_258 = 15;
            work->field_250 = 4;
            work->field_254 = 0;
            sound           = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40250003;
            SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
            break;
        case 4:
            if ((s16)--work->field_258 > 0) {
                work->field_252 = -125;
            } else {
                work->field_252 = 0;
            }
            if ((s16)--work->field_256 <= 0) {
                work->field_256 = 0;
                work->field_24E = 3;
                work->field_250 = 0;
            }
            Actor03700_Fn03320(task, 80);
            period = 21;
            break;
    }
    Actor03700_Fn032BC(task, 0, period);
}

static void Actor03700_Fn01C94(Task* task)
{
    Actor103700Work*      work;
    GfxCoord*             obj;
    Task*                 player;
    void*                 head;
    AnimationPlayRequest* arg;
    s32                   sound;
    s32                   pan;

    work                       = (Actor103700Work*)task->work;
    obj                        = task->extra.tmd->coords;
    player                     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = (u8*)head - sizeof(AnimationPlayRequest);
    arg                        = SCRATCH_STACK_CURSOR(AnimationPlayRequest);

    switch (work->field_250) {
        case 0:
            arg->source.sets          = Actor03700_D080FC;
            arg->animationId          = 2;
            arg->blend                = ANIMATION_BLEND_RESET;
            arg->blendFrames          = 0;
            arg->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, arg, 0);
            sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
            pan   = (s8)worldCoordGetOriginAudioPan(obj);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(obj));
            work->field_250 = 1;
            break;
        case 1:
            if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                work->field_262 = 0;
                work->field_24E = 5;
                work->field_250 = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(AnimationPlayRequest));
}

/// Tests whether the actor has noticed the player: true when the player is
/// under 0x708 units away on the XZ plane (the offset is staged on the
/// scratchpad stack), mid-action (`gSceneCombatState.signals.bytes.actionFlags` low nibble) or holding
/// the aim button (`field_19` bit 0). On noticing, it arms `gSceneCombatState` and
/// plays the alert cue from the placement's sound bank. Returns 1 when noticed.
static s32 Actor03700_Fn01DFC(Task* task)
{
    void**    scratch;
    u8*       head;
    SVECTOR*  vec;
    GfxCoord* coord;
    s16       dx;
    s16       dz;
    s32       ret;
    u32       soundId;
    s32       pan;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    vec                            = (SVECTOR*)(head - 8);
    coord                          = task->extra.tmd->coords;
    vec->vx                        = (u16)gPlayerStatus.coordMtx->t[0] - (u16)coord->coord.t[0];
    dz                             = (u16)gPlayerStatus.coordMtx->t[2] - (u16)coord->coord.t[2];
    SCRATCH_HEAD_AT(scratch, void) = vec;
    vec->vz                        = dz;
    dx                             = ((SVECTOR*)(head - 8))->vx;
    ret                            = 0;
    if ((SquareRoot0((dx * dx) + (dz * dz)) < 0x708) || (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_ACTIVE | SCENE_COMBAT_ACTION_PE_CAST_MASK)) || (gSceneCombatState.actor03700Flags & SCENE_COMBAT_ACTOR03700_ALERT)) {
        ret = 1;
        Gp_ArmStateF0(ret);
        soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x40250000 | ret;
        pan       = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
    return ret;
}

/// Turns the root coordinate towards the target position `field_23C`:
/// `field_244` becomes the heading to the target, and `field_246` steps from
/// the matrix's current heading towards it by at most `field_254`, taking the
/// short way round. The result is written back as a Y rotation.
static void Actor03700_Fn01F48(Task* task)
{
    Actor103700Work* work;
    GfxCoord*        coord;
    SVECTOR*         rot;
    u16              want;
    s16              ang;
    s16              diff;
    s32              adiff;
    s32              step;
    s32              cur;
    s32              next;
    s32              wrapStep;

    coord           = task->extra.tmd->coords;
    work            = (Actor103700Work*)task->work;
    rot             = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    rot->vx         = work->field_23C.vx - coord->coord.t[0];
    rot->vy         = 0;
    rot->vz         = work->field_23C.vz - coord->coord.t[2];
    work->field_244 = ratan2(rot->vx, rot->vz) & 0xFFF;
    ang             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
    want            = work->field_244;
    ang            &= 0xFFF;
    diff            = want - ang;
    adiff           = diff >= 0 ? diff : -diff;

    work->field_246 = ang;
    if (adiff < 0x800) {
        step = work->field_254;
        if (step >= adiff) {
            work->field_246 = want;
        } else {
            next = ang;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            goto store;
        }
    } else {
        step = work->field_254;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_246 = work->field_244;
        goto done;
    turn:
        wrapStep = work->field_254;
        cur      = work->field_246;
        if (diff > 0) {
            next = cur - wrapStep;
        } else {
            next = cur + wrapStep;
        }
    store:
        work->field_246 = next;
    }
done:
    rot->vx = 0;
    rot->vy = work->field_246;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Relights the actor for the world position of its root coordinate.
static inline void _actor03700UpdateColor(Task* task)
{
    GfxCoord* coord;
    VECTOR    color;

    coord    = task->extra.tmd->coords;
    color.vx = coord->workm.t[0];
    color.vy = coord->workm.t[1];
    color.vz = coord->workm.t[2];
    Gp_UpdateActorColor(task->spawnArg2.pointer, &color, 0, 0);
}

/// Spawns effect 0x40007 at the model's fifth coordinate with one of two model
/// streams picked at random, and gives the spawned model the texture page and
/// CLUT of the actor's placement in the current area.
static inline void _actor03700SpawnRemains(Task* task)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    u8               view;
    AreaVariant*     layout;
    AreaPlacement*   entry;
    EffectWork*      eff;
    TmdObject*       model;
    s32              idx;
    u32              raw;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 1) {
        D_80067704[0] = &_gActor03700BatBurstWingRight;
    } else {
        D_80067704[0] = &_gActor03700BatBurstWingLeft;
    }
    eff = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &task->extra.tmd->coords[4], 0x80, NULL);
    if (eff == NULL) {
        return;
    }
    sessionKey = &gGameSession->location.loc;
    raw        = ((Enemy*)task->spawnArg2.pointer)->placeKey;
    model      = eff->task->extra.tmd;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    view       = sessionKey->view;
    idx        = raw >> 12;
    key.view   = view;
    areaSyncLocationVariant(&key);
    layout                   = Gp_GetNestedAreaRec(&key);
    entry                    = gpAreaPlaceAt(layout->placements, idx);
    model->texturePageOffset = entry->texturePageOffset;
    model->clutRowOffset     = entry->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

/// Death handler. Mode 1 of `gSceneCombatState.actorControl` only refreshes the actor colour and
/// mode 2 hides the model; otherwise it steps `field_250`: unlink the enemy and
/// play the death cue (releasing the player's hold if `field_262` is set), wait
/// out a short delay, spawn the `field_268` death effect, let the player go,
/// count 60 frames and destroy the enemy.
static void Actor03700_Fn020D4(Enemy* enemy, Task* task)
{
    TmdObject*           model;
    GfxCoord*            obj;
    Actor103700Work*     work;
    Task*                player;
    AnimationPlayRequest arg;
    s32                  sound;
    s32                  sound2;

    work   = (Actor103700Work*)task->work;
    obj    = task->extra.tmd->coords;
    model  = task->extra.tmd;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor03700UpdateColor(task);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            if (work->field_266 != 0) {
                Actor03700_Fn0355C(task);
            }
            switch (work->field_250) {
                case 0:
                    enemy->recs = 0;
                    Gp_UnlinkObj(&work->obj);
                    worldTargetUnlinkNode(&enemy->node);
                    Gp_ReleaseStateF0Add(task, 0x25);
                    model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    sound        = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40250003;
                    SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
                    if (work->field_262 != 0) {
                        arg.source.sets          = Actor03700_D080FC;
                        arg.animationId          = 2;
                        arg.blend                = ANIMATION_BLEND_RESET;
                        arg.blendFrames          = 0;
                        arg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &arg, 0);
                        sound2 = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                        SndEvt_EnqueueType6(sound2, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
                    }
                    work->field_250 = 1;
                    work->field_258 = (((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) % 6 + 2;
                    break;
                case 1:
                    if ((s16)--work->field_258 > 0) {
                        break;
                    }
                    switch (work->field_268) {
                        case 0:
                            Tmd_FreeBuffers(model);
                            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                            _actor03700SpawnRemains(task);
                            break;
                        case 1:
                            Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, obj, 0x10280, NULL);
                            break;
                        case 2:
                            Gp_SpawnEff(EFFECT_HIT_PUFF, obj, 0x10013380, NULL);
                            Gp_SpawnEff(EFFECT_HIT_PUFF, obj, 0x10111300, NULL);
                            break;
                    }
                    work->field_250 = 2;
                    break;
                case 2:
                    if (work->field_262 != 0) {
                        if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                            taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                            work->field_250 = 3;
                            work->field_256 = 60;
                            work->field_262 = 0;
                        }
                    } else {
                        work->field_250 = 3;
                        work->field_256 = 60;
                    }
                    break;
                case 3:
                    if ((s16)--work->field_256 < 0) {
                        work->field_250 = 4;
                    }
                    break;
                case 4:
                    if (work->field_266 == 0 || work->field_266 == 2) {
                        enemyDestroy(enemy, task);
                    }
                    break;
            }
            break;
    }
}

/// Mode 7, the drop-in: keeps the actor hidden, unlockable and out of the
/// contact passes until `gSceneCombatState.actor03700Wave` reaches the placement's `mode - 9`, then
/// counts `field_256` down from 5 and picks a target position `field_23C` above
/// the root coordinate from `gRandomLcgState` - a lower one and mode 8 for
/// placements below 10, a higher one and mode 9 above - before re-enabling the
/// contacts, rearming a random countdown and allocating the model buffers.
static void Actor03700_Fn025C8(Task* task)
{
    TmdObject*       obj;
    TmdObject*       ext;
    Actor103700Work* work;
    Enemy*           spawn;
    GfxCoord*        coord;
    s32              diff;

    ext                           = task->extra.tmd;
    work                          = (Actor103700Work*)task->work;
    coord                         = ext->coords;
    spawn                         = (Enemy*)task->spawnArg2.pointer;
    obj                           = ext;
    work->obj.flags              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    obj->flags                   |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    spawn->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;

    switch (work->field_250) {
        case 0:
            diff = spawn->place->mode - 9;
            if (gSceneCombatState.actor03700Wave >= diff) {
                work->field_250 = 1;
                work->field_256 = 5;
            }
            break;
        case 1:
            diff = spawn->place->mode - 9;
            if ((s16)--work->field_256 <= 0) {
                work->obj.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (diff < 10) {
                    work->field_24E    = 8;
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->field_23C.vx = (u16)coord->coord.t[0] + ((gRandomLcgState >> 16) & 0xFF);
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->field_23C.vy = (u16)coord->coord.t[1] - (((gRandomLcgState >> 16) & 0x1FF) + 0x352);
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->field_23C.vz = (u16)coord->coord.t[2] + ((gRandomLcgState >> 16) & 0xFF);
                } else {
                    work->field_24E    = 9;
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->field_23C.vx = (u16)coord->coord.t[0] + ((gRandomLcgState >> 16) & 0xFF);
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->field_23C.vy = (u16)coord->coord.t[1] - (((gRandomLcgState >> 16) & 0x1FF) + 0x73A);
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->field_23C.vz = (u16)coord->coord.t[2] + ((gRandomLcgState >> 16) & 0xFF);
                }
                work->field_250 = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_256 = (gRandomLcgState >> 16) & 0x1F;
                Tmd_AllocBuffers(obj);
                obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            }
            break;
    }
}

/// Modes 8 and 9, the approach after the drop-in: counts `field_256` down,
/// then moves the root coordinate towards the target position `field_23C` by
/// 75/2048 of the unit direction per frame until it is within 150 on Y, then
/// counts 30 frames and hands over to mode 3, arming `gSceneCombatState`.
static void Actor03700_Fn027DC(Task* task)
{
    Actor103700SteerScratch* s;
    Actor103700Work*         work;
    GfxCoord*                coord;
    s32                      d;

    s     = (Actor103700SteerScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor103700SteerScratch));
    work  = (Actor103700Work*)task->work;
    coord = task->extra.tmd->coords;
    switch (work->field_250) {
        case 0:
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 1;
            }
            break;
        case 1:
            s->delta.vx = work->field_23C.vx - coord->coord.t[0];
            s->delta.vy = work->field_23C.vy - coord->coord.t[1];
            s->delta.vz = work->field_23C.vz - coord->coord.t[2];
            VectorNormalS(&s->delta, &s->normal);
            coord->coord.t[0] += (s->normal.vx * 75) >> 11;
            coord->coord.t[1] += (s->normal.vy * 75) >> 11;
            coord->coord.t[2] += (s->normal.vz * 75) >> 11;
            d                  = (s32)work->field_23C.vy - coord->coord.t[1];
            if ((d < 0 ? -d : d) < 150) {
                work->field_250 = 2;
                work->field_256 = 30;
            }
            break;
        case 2:
            if ((s16)--work->field_256 <= 0) {
                work->field_24E = 3;
                work->field_250 = 0;
                work->field_256 = 0;
                Gp_ArmStateF0(1);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor103700SteerScratch));
}

static void Actor03700_Fn029C0(Task* task)
{
    Actor103700Work*         work;
    TmdObject*               obj;
    GfxCoord*                coord;
    Actor103700SteerScratch* scratch;
    s32                      mode;
    Enemy*                   ctx;

    scratch                                       = SCRATCH_STACK_CURSOR(Actor103700SteerScratch) - 1;
    SCRATCH_STACK_CURSOR(Actor103700SteerScratch) = scratch;
    obj                                           = task->extra.tmd;
    coord                                         = obj->coords;
    work                                          = (Actor103700Work*)task->work;
    mode                                          = work->field_250;
    ctx                                           = (Enemy*)task->spawnArg2.pointer;

    switch (mode) {
        case 0:
            work->obj.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            obj->flags                 |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            if (gSceneCombatState.actor03700Wave == 0) {
                work->field_250    = 1;
                work->obj.flags   |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_23C.vx = coord->coord.t[0] - (((gRandomLcgState >> 16) & 0x1FF) + 500);
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_23C.vy = coord->coord.t[1] + (((gRandomLcgState >> 16) & 0x1FF) + 3500);
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_23C.vz = coord->coord.t[2] + (((gRandomLcgState >> 16) & 0x1FF) + 2000);
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_256    = (gRandomLcgState >> 16) & 0x1F;
                Tmd_AllocBuffers(obj);
                obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            }
            break;
        case 1:
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
            }
            break;
        case 2:
            scratch->delta.vx = work->field_23C.vx - coord->coord.t[0];
            scratch->delta.vy = work->field_23C.vy - coord->coord.t[1];
            scratch->delta.vz = work->field_23C.vz - coord->coord.t[2];
            VectorNormalS(&scratch->delta, &scratch->normal);
            coord->coord.t[0] += (scratch->normal.vx * 5) >> 9;
            coord->coord.t[1] += (scratch->normal.vy * 5) >> 9;
            coord->coord.t[2] += (scratch->normal.vz * 5) >> 9;
            work->field_254    = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            Actor03700_Fn01F48(task);

            Actor03700_BobInline(task, 0, 21);
            Actor03700_SwayInline(task, 80);

            if (abs(work->field_23C.vy - coord->coord.t[1]) < 40) {
                work->field_250 = 3;
                work->field_256 = 30;
                Gp_ArmStateF0(1);
            }
            break;
        case 3:
            work->field_23C.vx = gPlayerStatus.coordMtx->t[0];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_23C.vy = gPlayerStatus.coordMtx->t[1] - (((gRandomLcgState >> 16) & 0x3FF) + 800);
            work->field_23C.vz = gPlayerStatus.coordMtx->t[2];
            work->field_254    = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            Actor03700_Fn01F48(task);

            Actor03700_BobInline(task, 0, 21);
            Actor03700_SwayInline(task, 80);

            if ((s16)work->field_256 == 30) {
                gSceneCombatState.actor03700Wave = 2;
            }
            if ((s16)--work->field_256 <= 0) {
                work->field_24E = mode;
                work->field_250 = 0;
                work->field_256 = 0;
            }
            break;
    }
}

/// The actor's per-frame task callback: runs the handler for the task's state
/// from `Actor03700_D00004` (spawn, tick, death), passing the enemy record the
/// task was spawned with. The table is copied onto the stack before the call.
static void Actor03700_Fn02FA8(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor03700_D00004;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

static void Actor03700_Fn03004(Enemy* enemy, Task* task)
{
    GfxCoord*        coord;
    TmdObject*       obj;
    Actor103700Work* work;
    s32              state;
    s32              one;

    obj   = task->extra.tmd;
    state = gSceneCombatState.actorControl;
    work  = (Actor103700Work*)task->work;
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
    obj->flags                    = 0;
    enemy->node.state.parts.flags = 0;
    goto default_body;
case2:
    obj->flags                   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    enemy->node.state.parts.flags = one;
    return;
default_body:
    if (work->field_24E < 7) {
        Actor03700_Fn0042C(task, obj, one);
    }
    if (Actor03700_Fn008D0(task) != 0) {
        return;
    }
    if (work->field_254 != 0) {
        Actor03700_Fn01F48(task);
    }
    if (work->field_252 != 0) {
        Actor03700_Fn0321C(task);
    }
    if (work->field_266 != 0) {
        Actor03700_Fn0355C(task);
    }
    Actor03700_Fn033F0(task);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
case1:
    Actor03700_Fn034A0(task);
}

/// Asks the player for the melee hold (message 0x3F8, range 8) and, once it is
/// accepted, starts the grab on the actor's animation slot (message 0x3FF) and
/// flags `Actor103700Work::field_262`. The task's own unit is held for as long
/// as `GameActor::mode` stays out of mode 2; the two message buffers come
/// from one 0x2C-byte scratch stack push.
static s32 Actor03700_Fn03130(Task* task)
{
    Actor103700Work*        work;
    Task*                   player;
    void*                   head;
    Actor103700HoldScratch* scratch;
    s32                     ret;

    work                       = (Actor103700Work*)task->work;
    player                     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = (u8*)head - sizeof(Actor103700HoldScratch);
    scratch                    = SCRATCH_STACK_CURSOR(Actor103700HoldScratch);

    ret = 0;
    if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
        scratch->query.pressCount = 8;
        if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, scratch, 0) == 0) {
            scratch->anim.source.sets          = Actor03700_D080FC;
            scratch->anim.animationId          = 1;
            scratch->anim.blend                = ANIMATION_BLEND_RESET;
            scratch->anim.blendFrames          = 0;
            scratch->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->anim, 0);
            work->field_262 = 1;
            ret             = 1;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor103700HoldScratch));
    return ret;
}

/// Moves the actor forward: remembers the root coordinate's position in
/// `field_22C` (where a collision reset returns it), steps it along the
/// matrix's third column scaled by `field_252`, and moves its height 30 units
/// toward the target's `field_23C.vy`.
static void Actor03700_Fn0321C(Task* task)
{
    GfxCoord*        coord;
    Actor103700Work* work;
    s32              y;

    coord = task->extra.tmd->coords;
    work  = (Actor103700Work*)task->work;

    work->field_22C.vx = coord->coord.t[0];
    work->field_22C.vy = coord->coord.t[1];
    work->field_22C.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_252) >> 12;
    y                  = coord->coord.t[1];
    coord->coord.t[1]  = (work->field_23C.vy - y > 0) ? y + 30 : y - 30;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_252) >> 12;
}

static void Actor03700_Fn032BC(Task* task, s32 arg1, s32 arg2)
{
    Actor103700Work* work;
    GfxCoord*        coord;
    u16              frame;

    work  = (Actor103700Work*)task->work;
    coord = task->extra.tmd->coords;

    frame           = work->field_25E + 1;
    work->field_25E = frame;
    if (arg2 < (s16)frame) {
        work->field_25E = 0;
    }
    coord->coord.t[1] += Actor03700_D07F98[(arg1 * 15) + (s16)work->field_25E];
}

static void Actor03700_Fn03320(Task* task, s32 arg1)
{
    Actor103700Work* work;
    GfxCoord*        coord;
    u16              frame;
    s32              amp;

    work  = (Actor103700Work*)task->work;
    coord = task->extra.tmd->coords;

    frame           = work->field_25C + 1;
    work->field_25C = frame;
    if ((s16)frame >= 15) {
        work->field_25C = 0;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_25A = arg1 + ((gRandomLcgState >> 16) & 0x3F);
    }
    amp                = (work->field_25A * Actor03700_D07FD4[(s16)work->field_25C] * 16) >> 16;
    coord->coord.t[0] += (amp * coord->coord.m[0][0]) >> 12;
    coord->coord.t[2] += (amp * coord->coord.m[2][0]) >> 12;
}

/// Drives the five animation slots from the requested animation `field_248`.
/// When the request differs from the one playing (`field_24A`), it is
/// latched, the frame counter `field_24C` restarts and every slot is pointed
/// at it with a blend of 4; otherwise the counter ticks and each slot advances.
static void Actor03700_Fn033F0(Task* task)
{
    Actor103700Work* work;
    s32              i;

    work = (Actor103700Work*)task->work;
    if (work->field_248 != work->field_24A) {
        work->field_24A = work->field_248;
        work->field_24C = 0;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->field_248, 0, 4);
        }
    } else {
        work->field_24C++;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// Refreshes the actor's colour from the world position of its root
/// coordinate, with no blend parameters.
static void Actor03700_Fn034A0(Task* task)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = task->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(task->spawnArg2.pointer, &vec, 0, 0);
}

/// Handler for message 0x7DE. Ignored unless the actor is in one of its
/// active modes (below 7) and the task is in its tick state. While the actor
/// holds the player (`field_262`) it moves the hold to its release phase
/// (`field_250` = 3); otherwise it drops to mode 5 with the base animation
/// requested. Always answers 0.
s32 Actor03700_Fn034F8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    Actor103700Work* work;
    s32              state;

    work = (Actor103700Work*)task->work;
    if (work->field_24E >= 7) {
        return 0;
    }
    state = task->state;
    if (state != 1) {
        return 0;
    }
    if (work->field_262 != 0) {
        work->field_250 = 3;
    } else {
        work->field_24E = 5;
        work->field_248 = state;
        work->field_250 = 0;
    }
    return 0;
}

static void Actor03700_Fn0355C(Task* task)
{
    Actor103700Work* work = (Actor103700Work*)task->work;
    s32              state;

    switch (gSceneCombatState.actor03700Wave) {
        case 0:
            break;
        case 1:
            gSceneCombatState.actor03700Wave = 2;
            return;
        case 2:
            if (gSceneCombatState.battleRefs < 0x11) {
                gSceneCombatState.actor03700Wave = 3;
                return;
            }
            break;
        case 3:
            if (gSceneCombatState.battleRefs < 0xE) {
                gSceneCombatState.actor03700Wave = 4;
                return;
            }
            break;
        case 4:
            if (gSceneCombatState.battleRefs < 0xA) {
                gSceneCombatState.actor03700Wave = 5;
                return;
            }
            break;
        case 5:
            state = task->state;
            if (state == 2 && gSceneCombatState.battleRefs == 0) {
                work->field_266 = state;
            }
            break;
    }
}
