#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

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
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/gfxgte.h"
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

#include "rooms/shelter_b3_dumping_hole.h"

#include "rooms/shelter_b3_garbage_incinerator.h"
#include "../../shared/sucklerceph.h"

/// Work block the carriers hang off their context's 0x1C slot (the task's
/// `Task::work`). The three `WorldCollisionBody`s are the
/// display nodes `ActorsShared80138570` unlinks on its own exit path.
///
/// `field_36C` is the mode this body dispatches on - the sibling state machine
/// `ActorsShared80137e18` writes is the `field_36A`/`field_36E` pair, and this
/// one counts its own reaction out in `field_36E`. `field_370`/`field_372`/
/// `field_374` are the same (helper id, id the slots last saw, frames spent on
/// it) triple `Actor107000Work` keeps at 0x370 and `Actor207200Work` at 0x48C,
/// and `field_394` is the same already-reacted flag `ActorShared8014d378Work`
/// reads at 0x394.
typedef struct ActorShared80136288Work {
    /* 0x000 */ byte               pad_0[0x1DC];
    /* 0x1DC */ WorldCollisionBody field_1DC;
    /* 0x1FC */ byte               pad_1FC[0x30];
    /* 0x22C */ WorldCollisionBody field_22C;
    /* 0x24C */ byte               pad_24C[0x60];
    /* 0x2AC */ WorldCollisionBody field_2AC;
    /* 0x2CC */ byte               pad_2CC[0xA0];
    /* 0x36C */ s16                field_36C; // mode this handler dispatches on
    /* 0x36E */ u16                field_36E; // frames the reaction has run; 0x3D moves to mode 2
    /* 0x370 */ s16                field_370; // helper id the six slots are rebound to
    /* 0x372 */ u16                field_372; // id the six helper slots last saw
    /* 0x374 */ u16                field_374; // frames spent on the current helper id
    /* 0x376 */ byte               pad_376[0x1E];
    /* 0x394 */ u16                field_394; // non-zero: this frame has spent its reaction
} ActorShared80136288Work;

/// Ground-burst work allocated by the specimen's setup handler and advanced
/// by ActorsShared80136c80. The collision node points at the capsule, whose
/// contact table occupies the final 0x18 bytes.
typedef struct ActorsShared80136938Work {
    /* 0x00 */ SVECTOR               vec;
    /* 0x08 */ WorldCollisionBody    obj;
    /* 0x28 */ WorldCollisionCapsule rec;
    /* 0x40 */ WorldCollisionContact rec2;
} ActorsShared80136938Work;
STATIC_ASSERT_SIZEOF(ActorsShared80136938Work, 0x58);

/// Movement work the tick reaches through `Task::work`. This is the head of the
/// block each carrier's own work type overlays (`Actor107000Work` from 0x214
/// on, `Actor207000Work` from its own), so the two are views of one block, not
/// one layout.
///
/// `field_0`/`field_2`/`field_4` are the per-frame velocity the tick folds onto
/// the model: negated into `recs[0]`'s position and added to the coordinate's
/// own translation. `recs[1]` is the collision record the hit test walks, and
/// `field_26` the flag word it trims to 0x3FFF once a hit lands.
typedef struct ActorsShared80136c80Work {
    /* 0x00 */ u16                   field_0;
    /* 0x02 */ u16                   field_2;
    /* 0x04 */ u16                   field_4;
    /* 0x06 */ byte                  pad_6[0x20];
    /* 0x26 */ u16                   field_26;
    /* 0x28 */ WorldCollisionContact recs[2];
} ActorsShared80136c80Work;
STATIC_ASSERT_SIZEOF(ActorsShared80136c80Work, 0x58);

/// The part of the carriers' work block this body touches. Each carrier's block
/// is its own type (`Actor107000Work`, ...); the shared unit only names the two
/// collision-record tables `func_800E0C10` and `Gp_ClearRec18Occupied` walk,
/// and the step's own position and speed scalars. It overlaps the carriers'
/// own layouts - `Actor107000Work::field_2CC`, an `s16`, sits where the second
/// table starts - so the two are views of one block, not one layout.
typedef struct ActorsShared8013777cWork {
    /* 0x000 */ byte                  pad_0[0x24C];
    /* 0x24C */ WorldCollisionContact field_24C[4]; // collision table; `func_800E0C10` steps it with count 4
    /* 0x2AC */ byte                  pad_2AC[0x20];
    /* 0x2CC */ WorldCollisionContact field_2CC;    // second record table, wiped once the step is done
    /* 0x2E4 */ s16                   field_2E4;    // armed to 0x400 when the step is taken
    /* 0x2E6 */ byte                  pad_2E6[0x56];
    /* 0x33C */ VECTOR3               field_33C;    // position the mode-2 arm snaps back to
    /* 0x348 */ byte                  pad_348[0x30];
    /* 0x378 */ u16                   field_378;    // forward speed; a step loses a quarter of it
    /* 0x37A */ byte                  pad_37A[0x12];
    /* 0x38C */ s16                   field_38C;    // latched step mode
    /* 0x38E */ byte                  pad_38E[0xA];
    /* 0x398 */ s16                   field_398;    // vertical speed, -0x50 while a step runs
    /* 0x39A */ u16                   field_39A;    // non-zero once the step has been taken
} ActorsShared8013777cWork;

/// The part of the carriers' work block this body touches. Each carrier's block
/// is its own type (`Actor107000Work`, ...); the shared unit only names the
/// animation triple - the id being played, the id the six helper slots last
/// saw and the frames spent on it.
typedef struct ActorsShared80137cf4Work {
    /* 0x000 */ byte pad_0[0x370];
    /* 0x370 */ s16  field_370; // animation id the work is playing
    /* 0x372 */ u16  field_372; // id the six helper slots last saw
    /* 0x374 */ u16  field_374; // frames spent on the current id
} ActorsShared80137cf4Work;

/// Work block the carriers hang off `Task::work`. `coord` is the extra
/// `GfxCoord` this body wires as `parent` of the model's second part;
/// `scale` is that node's X/Y/Z in 4096-per-unit fixed point, which
/// `ActorsShared80137ea8` then reads as `field_34E` for the Y component.
/// `field_36A` is 5 when the 0x600A5 spawn is already armed.
typedef struct ActorShared80137e18Work {
    /* 0x000 */ byte     pad_0[0x2EC];
    /* 0x2EC */ GfxCoord coord;
    /* 0x33C */ byte     pad_33C[0x10];
    /* 0x34C */ SVECTOR  scale;
    /* 0x354 */ byte     pad_354[0x16];
    /* 0x36A */ s16      field_36A;
    /* 0x36C */ byte     pad_36C[2];
    /* 0x36E */ s16      field_36E;
} ActorShared80137e18Work;
STATIC_ASSERT_SIZEOF(ActorShared80137e18Work, 0x370);

/// Work block the carriers hang off `Task::work`. `coord` is the extra
/// `GfxCoord` `ActorsShared80137e18` wires as `parent` of the model's
/// second part; `field_34E` is that node's Y scale in 4096-per-unit fixed
/// point, the middle of the X/Y/Z trio that spawn path arms to 0x1000.
typedef struct ActorShared80137ea8Work {
    /* 0x000 */ byte     pad_0[0x2EC];
    /* 0x2EC */ GfxCoord coord;
    /* 0x33C */ byte     pad_33C[0x12];
    /* 0x34E */ u16      field_34E; // Y scale; 4096 = one unit
} ActorShared80137ea8Work;
STATIC_ASSERT_SIZEOF(ActorShared80137ea8Work, 0x350);

/// View of the work field used by both specimen actor overlays.
typedef struct ActorsShared80137f1cWork {
    /* 0x000 */ byte pad_0[0x384];
    /* 0x384 */ s16  field_384; // scale delta; 4096 is one unit
} ActorsShared80137f1cWork;

/// Work block the enemy's spawn function parks in the task's `Task::work`
/// slot. Only the three `WorldCollisionBody` collision
/// bodies are reached from this shared body -- `ActorsShared80138570` is the
/// exit callback that unlinks all three -- so the type stops after the last
/// one; whatever each overlay keeps around them differs per actor.
typedef struct ActorShared80138570Work {
    /* 0x000 */ byte               pad_0[0x1DC];
    /* 0x1DC */ WorldCollisionBody field_1DC;
    /* 0x1FC */ byte               pad_1FC[0x30];
    /* 0x22C */ WorldCollisionBody field_22C;
    /* 0x24C */ byte               pad_24C[0x60];
    /* 0x2AC */ WorldCollisionBody field_2AC;
} ActorShared80138570Work;

typedef struct ActorShared80138640Work {
    /* 0x000 */ byte    pad_0[0x33C];
    /* 0x33C */ VECTOR3 field_33C; ///< previous frame's coord translation
    /* 0x348 */ byte    pad_348[0x30];
    /* 0x378 */ s16     field_378; ///< forward speed, 4096 = 1.0
    /* 0x37A */ byte    pad_37A[0x1E];
    /* 0x398 */ s16     field_398; ///< vertical speed, whole units
} ActorShared80138640Work;

/// Work block of the task served by this shared body. Only the `WorldCollisionBody` collision
/// body at 0x8 is reached from here -- `ActorsShared801511c8` is the exit
/// callback that unlinks it -- so the type stops there; whatever each overlay
/// keeps after it differs per actor.
typedef struct ActorShared801511c8Work {
    /* 0x00 */ byte               pad_0[0x8];
    /* 0x08 */ WorldCollisionBody obj;
} ActorShared801511c8Work;

// Typed callback views for the task message dispatcher.

/// Animation work reached through `Task::work`. `field_2B8`/`field_2BA`/
/// `field_2BC` are the same (id, id the three helper slots last saw, frames
/// spent on it) triple as `Actor207200Work`'s `field_28C`/`field_28E`/
/// `field_290`; a non-zero `field_2D2` suppresses the per-frame rebind.
///
/// The second triple, `field_370`/`field_372`/`field_374`, is the same thing
/// over the work's *six* helper slots, mirroring `Actor207200Work`'s
/// `field_48C`/`field_48E`/`field_490`.
///
/// `field_36A`/`field_36E` are the same pair `Actor103800Work` and
/// `ActorShared80137e18Work` carry: the reaction sub-state the damage branch
/// writes (3 here, 5 once the 0x600A5 spawn is armed) and a word cleared
/// alongside it.
///
/// `field_28C`/`field_2CA` are the same pair as `Actor207200Work`'s
/// `field_264`/`field_2A0`: the transform `sucklercephFlatten` folds onto the
/// model, and the angle it is scaled by.
typedef struct Actor107000Work {
    /* 0x000 */ byte                  pad_0[0x11A];
    /* 0x11A */ u16                   field_11A; // flag word of the render node at 0xFC, `SucklercephWork::senseBody.flags`
    /* 0x11C */ WorldCollisionContact field_11C; // that node's collision table, `SucklercephWork::senseContact`
    /* 0x134 */ byte                  pad_134[0x1E];
    /* 0x152 */ u16                   field_152;
    /* 0x154 */ WorldCollisionContact field_154[4];
    /* 0x1B4 */ byte                  pad_1B4[0x1E];
    /* 0x1D2 */ u16                   field_1D2;
    /* 0x1D4 */ WorldCollisionContact field_1D4;
    /* 0x1EC */ byte                  pad_1EC[0x1E];
    /* 0x20A */ u16                   field_20A;
    /* 0x20C */ byte                  pad_20C[8];
    /* 0x214 */ WorldCollisionContact field_214; // collision record `Actor07000_Fn046B8` re-rolls
    /* 0x22C */ byte                  pad_22C[0x48];
    /* 0x274 */ VECTOR3               field_274; // saved root translation
    /* 0x280 */ byte                  pad_280[4];
    /* 0x284 */ EffectSpawnArg        field_284; // hit-effect coordinate and parameters
    /* 0x28C */ MATRIX                field_28C; // transform folded onto the model part
    /* 0x2AC */ s32                   field_2AC; // advanced by 0xC8 a frame while the death flag runs
    /* 0x2B0 */ s16                   field_2B0; // heading the specimen is turned toward; stepped 0x20 a frame
    /* 0x2B2 */ s16                   field_2B2; // armed to 3 by the hit branch, with the pair below
    /* 0x2B4 */ s16                   field_2B4; // cleared on the death branch
    /* 0x2B6 */ s16                   field_2B6; // cleared next to `field_2B2`
    /* 0x2B8 */ s16                   field_2B8; // animation id the work is playing
    /* 0x2BA */ s16                   field_2BA; // id the three helper slots last saw
    /* 0x2BC */ u16                   field_2BC; // frames spent on the current id
    /* 0x2BE */ s16                   field_2BE; // cleared next to the pair above
    /* 0x2C0 */ byte                  pad_2C0[0x6];
    /* 0x2C6 */ s16                   field_2C6; // armed to 1 with the stage handoff, two bytes before the stage selector
    /* 0x2C8 */ s16                   field_2C8; // reaction stage the per-frame handler switches on
    /* 0x2CA */ s16                   field_2CA; // angle the transform is scaled by
    /* 0x2CC */ s16                   field_2CC; // countdown seeded by the damage branch
    /* 0x2CE */ s16                   field_2CE; // remaining hit cooldown
    /* 0x2D0 */ u16                   field_2D0; // frames until the next sound cue; re-rolled from `gRandomLcgState`
    /* 0x2D2 */ s16                   field_2D2; // non-zero: the rebind is suppressed
    /* 0x2D4 */ u16                   field_2D4; // frames the reaction has run; the death branch fires at 5
    /* 0x2D6 */ s16                   field_2D6; // selects the sound event's high half
    /* 0x2D8 */ s16                   field_2D8; // latched copy of `field_2B8`
    /* 0x2DA */ s16                   field_2DA;
    /* 0x2DC */ byte                  pad_2DC[2];
    /* 0x2DE */ s16                   field_2DE;
    /* 0x2E0 */ s16                   field_2E0;
    /* 0x2E2 */ s16                   field_2E2;
    /* 0x2E4 */ byte                  pad_2E4[0x86];
    /* 0x36A */ s16                   field_36A; // reaction sub-state, cleared once applied
    /* 0x36C */ s16                   field_36C; // cleared next to `field_36A`
    /* 0x36E */ u16                   field_36E; // cleared alongside `field_36A`
    /* 0x370 */ s16                   field_370; // animation id the work is playing
    /* 0x372 */ u16                   field_372; // id the six helper slots last saw
    /* 0x374 */ u16                   field_374; // frames spent on the current id
    /* 0x376 */ byte                  pad_376[0xC];
    /* 0x382 */ s16                   field_382; // reaction branch the hit handler selects
    /* 0x384 */ byte                  pad_384[0xC];
    /* 0x390 */ u16                   field_390; // frames until the next 0x60080 spawn
    /* 0x392 */ u16                   field_392; // spawns so far; the cue fires at 5
    /* 0x394 */ u16                   field_394; // non-zero: this frame has spent its reaction (see ActorShared80136288Work)
} Actor107000Work;

void ActorsShared801349d8(Task*);

static AnimationSet _gActor07000Actor107000Animation07D1C;
static AnimationSet _gActor07000Actor107000Animation07E64;
static AnimationSet _gActor07000Actor107000Animation08008;
static TmdSource    _gActor07000SucklercephBody;
s32                 Actor07000_Fn05AB8(Task* task, s32 msgId, ActorCommand* request, s32 arg3);
void                Actor07000_Fn05E6C(Task*);
void                Actor07000_Fn06338(Task*);
void                Actor07000_Fn067B4(Task*);

DamageAttack gSucklercephAttack = { 30, 7 };

EnemyParams gSucklercephParams = { &gSucklercephAttack, 70, 6, 12, 3, 100, 20, 100, 0 };

PadScriptCmd Actor07000_D06938[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
};

PadScriptVibrationSegment Actor07000_D06944[3] = {
    { 0, 0, 9, 0 },
    { 255, 255, 12, 1 },
    { 100, 50, 6, 1 },
};

static TmdBone _gActor07000SucklercephBodySkeleton[3] = {
#include "assets/sucklerceph_body_skeleton.inc"
};

static u32 _gActor07000SucklercephBodyPartVerts[3] = {
#include "assets/sucklerceph_body_partVerts.inc"
};

static SVECTOR _gActor07000SucklercephBodyVerts[68] = {
#include "assets/sucklerceph_body_verts.inc"
};

static SVECTOR _gActor07000SucklercephBodyNormals[68] = {
#include "assets/sucklerceph_body_normals.inc"
};

static u32 _gActor07000SucklercephBodyStream[752] = {
#include "assets/sucklerceph_body_stream.inc"
};

static TmdSource _gActor07000SucklercephBody = {
    0,
    4516,
    584,
    3,
    _gActor07000SucklercephBodyPartVerts,
    _gActor07000SucklercephBodyVerts,
    _gActor07000SucklercephBodyNormals,
    _gActor07000SucklercephBodySkeleton,
    _gActor07000SucklercephBodyStream,
};

static AnimationPackedPose _gActor07000Actor107000Animation07D1CBank1[34] = {
#include "assets/actor_107000_animation_07D1C_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation07D1CBank4[26] = {
#include "assets/actor_107000_animation_07D1C_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation07D1CRecords[74] = {
#include "assets/actor_107000_animation_07D1C_records.inc"
};

static u16 _gActor07000Actor107000Animation07D1CIndices[4] = {
#include "assets/actor_107000_animation_07D1C_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation07D1C = {
    _gActor07000Actor107000Animation07D1CRecords,
    _gActor07000Actor107000Animation07D1CIndices,
    { NULL, _gActor07000Actor107000Animation07D1CBank1, NULL, NULL, _gActor07000Actor107000Animation07D1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation07E64Bank1[11] = {
#include "assets/actor_107000_animation_07E64_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation07E64Bank4[9] = {
#include "assets/actor_107000_animation_07E64_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation07E64Records[28] = {
#include "assets/actor_107000_animation_07E64_records.inc"
};

static u16 _gActor07000Actor107000Animation07E64Indices[4] = {
#include "assets/actor_107000_animation_07E64_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation07E64 = {
    _gActor07000Actor107000Animation07E64Records,
    _gActor07000Actor107000Animation07E64Indices,
    { NULL, _gActor07000Actor107000Animation07E64Bank1, NULL, NULL, _gActor07000Actor107000Animation07E64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation08008Bank1[15] = {
#include "assets/actor_107000_animation_08008_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation08008Bank4[12] = {
#include "assets/actor_107000_animation_08008_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation08008Records[36] = {
#include "assets/actor_107000_animation_08008_records.inc"
};

static u16 _gActor07000Actor107000Animation08008Indices[4] = {
#include "assets/actor_107000_animation_08008_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation08008 = {
    _gActor07000Actor107000Animation08008Records,
    _gActor07000Actor107000Animation08008Indices,
    { NULL, _gActor07000Actor107000Animation08008Bank1, NULL, NULL, _gActor07000Actor107000Animation08008Bank4, NULL, NULL, NULL },
};

TaskMessageEntry gSucklercephDropMsgTable[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, sucklercephMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor07000_D08040 = { { { TASK_BODY_TMD, 96 } }, sucklercephTask, { .model = &_gActor07000SucklercephBody } };

TaskDesc Actor07000_D0804C = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, sucklercephDropTask, { .model = &_gActor07000SucklercephBody } };

AnimationSet* gSucklercephAnimSets[4] = {
    NULL,
    &_gActor07000Actor107000Animation07D1C,
    &_gActor07000Actor107000Animation07E64,
    &_gActor07000Actor107000Animation08008,
};

SVECTOR Actor07000_D08068 = { 0, -10, 0, 0 };

SVECTOR gSucklercephCollapseFxOffset = { 0, -300, 0, 0 };

DamageAttack Actor07000_D08078[2] = {
    { 20, 7 },
    { 12, 3 },
};

EnemyParams Actor07000_D08080 = { Actor07000_D08078, 120, 12, 36, 1, 250, 20, 100, 0 };

static TmdBone _gActor07000Actor107000Model08BB4Skeleton[7] = {
#include "assets/actor_107000_model_08BB4_skeleton.inc"
};

static u32 _gActor07000Actor107000Model08BB4PartVerts[7] = {
#include "assets/actor_107000_model_08BB4_partVerts.inc"
};

static SVECTOR _gActor07000Actor107000Model08BB4Verts[131] = {
#include "assets/actor_107000_model_08BB4_verts.inc"
};

static SVECTOR _gActor07000Actor107000Model08BB4Normals[190] = {
#include "assets/actor_107000_model_08BB4_normals.inc"
};

static u32 _gActor07000Actor107000Model08BB4Stream[1734] = {
#include "assets/actor_107000_model_08BB4_stream.inc"
};

static TmdSource _gActor07000Actor107000Model08BB4 = {
    0,
    9272,
    2448,
    7,
    _gActor07000Actor107000Model08BB4PartVerts,
    _gActor07000Actor107000Model08BB4Verts,
    _gActor07000Actor107000Model08BB4Normals,
    _gActor07000Actor107000Model08BB4Skeleton,
    _gActor07000Actor107000Model08BB4Stream,
};

static TmdBone _gActor07000SlouchBurstLegSkeleton[1] = {
#include "assets/slouch_burst_leg_skeleton.inc"
};

static u32 _gActor07000SlouchBurstLegPartVerts[1] = {
#include "assets/slouch_burst_leg_partVerts.inc"
};

static SVECTOR _gActor07000SlouchBurstLegVerts[17] = {
#include "assets/slouch_burst_leg_verts.inc"
};

static SVECTOR _gActor07000SlouchBurstLegNormals[27] = {
#include "assets/slouch_burst_leg_normals.inc"
};

static u32 _gActor07000SlouchBurstLegStream[179] = {
#include "assets/slouch_burst_leg_stream.inc"
};

static TmdSource _gActor07000SlouchBurstLeg = {
    0,
    1144,
    0,
    1,
    _gActor07000SlouchBurstLegPartVerts,
    _gActor07000SlouchBurstLegVerts,
    _gActor07000SlouchBurstLegNormals,
    _gActor07000SlouchBurstLegSkeleton,
    _gActor07000SlouchBurstLegStream,
};

static TmdBone _gActor07000SlouchBurstArmSkeleton[1] = {
#include "assets/slouch_burst_arm_skeleton.inc"
};

static u32 _gActor07000SlouchBurstArmPartVerts[1] = {
#include "assets/slouch_burst_arm_partVerts.inc"
};

static SVECTOR _gActor07000SlouchBurstArmVerts[27] = {
#include "assets/slouch_burst_arm_verts.inc"
};

static SVECTOR _gActor07000SlouchBurstArmNormals[29] = {
#include "assets/slouch_burst_arm_normals.inc"
};

static u32 _gActor07000SlouchBurstArmStream[274] = {
#include "assets/slouch_burst_arm_stream.inc"
};

static TmdSource _gActor07000SlouchBurstArm = {
    0,
    1804,
    0,
    1,
    _gActor07000SlouchBurstArmPartVerts,
    _gActor07000SlouchBurstArmVerts,
    _gActor07000SlouchBurstArmNormals,
    _gActor07000SlouchBurstArmSkeleton,
    _gActor07000SlouchBurstArmStream,
};

static TmdBone _gActor07000SlouchPoisonSkeleton[1] = {
#include "assets/slouch_poison_skeleton.inc"
};

static u32 _gActor07000SlouchPoisonPartVerts[1] = {
#include "assets/slouch_poison_partVerts.inc"
};

static SVECTOR _gActor07000SlouchPoisonVerts[26] = {
#include "assets/slouch_poison_verts.inc"
};

static SVECTOR _gActor07000SlouchPoisonNormals[28] = {
#include "assets/slouch_poison_normals.inc"
};

static u32 _gActor07000SlouchPoisonStream[232] = {
#include "assets/slouch_poison_stream.inc"
};

static TmdSource _gActor07000SlouchPoison = {
    0,
    1556,
    0,
    1,
    _gActor07000SlouchPoisonPartVerts,
    _gActor07000SlouchPoisonVerts,
    _gActor07000SlouchPoisonNormals,
    _gActor07000SlouchPoisonSkeleton,
    _gActor07000SlouchPoisonStream,
};

static AnimationPackedPose _gActor07000Actor107000Animation0B868Bank1[4] = {
#include "assets/actor_107000_animation_0B868_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0B868Bank4[12] = {
#include "assets/actor_107000_animation_0B868_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0B868Records[41] = {
#include "assets/actor_107000_animation_0B868_records.inc"
};

static u16 _gActor07000Actor107000Animation0B868Indices[8] = {
#include "assets/actor_107000_animation_0B868_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0B868 = {
    _gActor07000Actor107000Animation0B868Records,
    _gActor07000Actor107000Animation0B868Indices,
    { NULL, _gActor07000Actor107000Animation0B868Bank1, NULL, NULL, _gActor07000Actor107000Animation0B868Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0BCF4Bank1[18] = {
#include "assets/actor_107000_animation_0BCF4_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0BCF4Bank4[92] = {
#include "assets/actor_107000_animation_0BCF4_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0BCF4Records[131] = {
#include "assets/actor_107000_animation_0BCF4_records.inc"
};

static u16 _gActor07000Actor107000Animation0BCF4Indices[8] = {
#include "assets/actor_107000_animation_0BCF4_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0BCF4 = {
    _gActor07000Actor107000Animation0BCF4Records,
    _gActor07000Actor107000Animation0BCF4Indices,
    { NULL, _gActor07000Actor107000Animation0BCF4Bank1, NULL, NULL, _gActor07000Actor107000Animation0BCF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0BF18Bank1[6] = {
#include "assets/actor_107000_animation_0BF18_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0BF18Bank4[41] = {
#include "assets/actor_107000_animation_0BF18_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0BF18Records[64] = {
#include "assets/actor_107000_animation_0BF18_records.inc"
};

static u16 _gActor07000Actor107000Animation0BF18Indices[8] = {
#include "assets/actor_107000_animation_0BF18_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0BF18 = {
    _gActor07000Actor107000Animation0BF18Records,
    _gActor07000Actor107000Animation0BF18Indices,
    { NULL, _gActor07000Actor107000Animation0BF18Bank1, NULL, NULL, _gActor07000Actor107000Animation0BF18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0C268Bank1[13] = {
#include "assets/actor_107000_animation_0C268_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0C268Bank4[66] = {
#include "assets/actor_107000_animation_0C268_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0C268Records[93] = {
#include "assets/actor_107000_animation_0C268_records.inc"
};

static u16 _gActor07000Actor107000Animation0C268Indices[8] = {
#include "assets/actor_107000_animation_0C268_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0C268 = {
    _gActor07000Actor107000Animation0C268Records,
    _gActor07000Actor107000Animation0C268Indices,
    { NULL, _gActor07000Actor107000Animation0C268Bank1, NULL, NULL, _gActor07000Actor107000Animation0C268Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0C3D4Bank1[5] = {
#include "assets/actor_107000_animation_0C3D4_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0C3D4Bank4[21] = {
#include "assets/actor_107000_animation_0C3D4_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0C3D4Records[41] = {
#include "assets/actor_107000_animation_0C3D4_records.inc"
};

static u16 _gActor07000Actor107000Animation0C3D4Indices[8] = {
#include "assets/actor_107000_animation_0C3D4_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0C3D4 = {
    _gActor07000Actor107000Animation0C3D4Records,
    _gActor07000Actor107000Animation0C3D4Indices,
    { NULL, _gActor07000Actor107000Animation0C3D4Bank1, NULL, NULL, _gActor07000Actor107000Animation0C3D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0CA98Bank1[30] = {
#include "assets/actor_107000_animation_0CA98_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0CA98Bank4[140] = {
#include "assets/actor_107000_animation_0CA98_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0CA98Records[189] = {
#include "assets/actor_107000_animation_0CA98_records.inc"
};

static u16 _gActor07000Actor107000Animation0CA98Indices[8] = {
#include "assets/actor_107000_animation_0CA98_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0CA98 = {
    _gActor07000Actor107000Animation0CA98Records,
    _gActor07000Actor107000Animation0CA98Indices,
    { NULL, _gActor07000Actor107000Animation0CA98Bank1, NULL, NULL, _gActor07000Actor107000Animation0CA98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0D270Bank1[32] = {
#include "assets/actor_107000_animation_0D270_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0D270Bank4[171] = {
#include "assets/actor_107000_animation_0D270_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0D270Records[221] = {
#include "assets/actor_107000_animation_0D270_records.inc"
};

static u16 _gActor07000Actor107000Animation0D270Indices[8] = {
#include "assets/actor_107000_animation_0D270_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0D270 = {
    _gActor07000Actor107000Animation0D270Records,
    _gActor07000Actor107000Animation0D270Indices,
    { NULL, _gActor07000Actor107000Animation0D270Bank1, NULL, NULL, _gActor07000Actor107000Animation0D270Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0D344Bank1[2] = {
#include "assets/actor_107000_animation_0D344_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0D344Bank4[5] = {
#include "assets/actor_107000_animation_0D344_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0D344Records[28] = {
#include "assets/actor_107000_animation_0D344_records.inc"
};

static u16 _gActor07000Actor107000Animation0D344Indices[8] = {
#include "assets/actor_107000_animation_0D344_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0D344 = {
    _gActor07000Actor107000Animation0D344Records,
    _gActor07000Actor107000Animation0D344Indices,
    { NULL, _gActor07000Actor107000Animation0D344Bank1, NULL, NULL, _gActor07000Actor107000Animation0D344Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0D458Bank1[4] = {
#include "assets/actor_107000_animation_0D458_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0D458Bank4[15] = {
#include "assets/actor_107000_animation_0D458_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0D458Records[28] = {
#include "assets/actor_107000_animation_0D458_records.inc"
};

static u16 _gActor07000Actor107000Animation0D458Indices[8] = {
#include "assets/actor_107000_animation_0D458_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0D458 = {
    _gActor07000Actor107000Animation0D458Records,
    _gActor07000Actor107000Animation0D458Indices,
    { NULL, _gActor07000Actor107000Animation0D458Bank1, NULL, NULL, _gActor07000Actor107000Animation0D458Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0D5A8Bank1[5] = {
#include "assets/actor_107000_animation_0D5A8_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0D5A8Bank4[17] = {
#include "assets/actor_107000_animation_0D5A8_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0D5A8Records[38] = {
#include "assets/actor_107000_animation_0D5A8_records.inc"
};

static u16 _gActor07000Actor107000Animation0D5A8Indices[8] = {
#include "assets/actor_107000_animation_0D5A8_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0D5A8 = {
    _gActor07000Actor107000Animation0D5A8Records,
    _gActor07000Actor107000Animation0D5A8Indices,
    { NULL, _gActor07000Actor107000Animation0D5A8Bank1, NULL, NULL, _gActor07000Actor107000Animation0D5A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0D67CBank1[3] = {
#include "assets/actor_107000_animation_0D67C_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0D67CBank4[9] = {
#include "assets/actor_107000_animation_0D67C_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0D67CRecords[21] = {
#include "assets/actor_107000_animation_0D67C_records.inc"
};

static u16 _gActor07000Actor107000Animation0D67CIndices[8] = {
#include "assets/actor_107000_animation_0D67C_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0D67C = {
    _gActor07000Actor107000Animation0D67CRecords,
    _gActor07000Actor107000Animation0D67CIndices,
    { NULL, _gActor07000Actor107000Animation0D67CBank1, NULL, NULL, _gActor07000Actor107000Animation0D67CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0D754Bank1[3] = {
#include "assets/actor_107000_animation_0D754_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0D754Bank4[10] = {
#include "assets/actor_107000_animation_0D754_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0D754Records[21] = {
#include "assets/actor_107000_animation_0D754_records.inc"
};

static u16 _gActor07000Actor107000Animation0D754Indices[8] = {
#include "assets/actor_107000_animation_0D754_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0D754 = {
    _gActor07000Actor107000Animation0D754Records,
    _gActor07000Actor107000Animation0D754Indices,
    { NULL, _gActor07000Actor107000Animation0D754Bank1, NULL, NULL, _gActor07000Actor107000Animation0D754Bank4, NULL, NULL, NULL },
};

AnimationSet* Actor07000_D0D77C[13] = {
    NULL,
    &_gActor07000Actor107000Animation0B868,
    &_gActor07000Actor107000Animation0BCF4,
    &_gActor07000Actor107000Animation0BF18,
    &_gActor07000Actor107000Animation0C268,
    &_gActor07000Actor107000Animation0C3D4,
    &_gActor07000Actor107000Animation0CA98,
    &_gActor07000Actor107000Animation0D270,
    &_gActor07000Actor107000Animation0D344,
    &_gActor07000Actor107000Animation0D458,
    &_gActor07000Actor107000Animation0D5A8,
    &_gActor07000Actor107000Animation0D67C,
    &_gActor07000Actor107000Animation0D754,
};

SVECTOR Actor07000_D0D7B0 = { 0, 0, -120, 0 };

SVECTOR Actor07000_D0D7B8 = { 0, -300, 0, 0 };

TaskMessageEntry Actor07000_D0D7C0[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, Actor07000_Fn05AB8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor07000_D0D7D0[2] = {
    { { { TASK_BODY_TMD, 96 } }, Actor07000_Fn05E6C, { .model = &_gActor07000Actor107000Model08BB4 } },
    { { { TASK_BODY_COORD, 96 } }, Actor07000_Fn06338, { .value = 0 } },
};

TaskDesc Actor07000_D0D7E8 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, Actor07000_Fn067B4, { .model = &_gActor07000Actor107000Model08BB4 } };

/// The 0x39C-byte work block the actor's *other* spawn handler
/// (`Actor07000_Fn05068`) allocates, next to `SucklercephWork`:
/// the same `AnimationContext`, seven animation slots instead of three, then three
/// `WorldCollisionBody` collision bodies where that one has four.
///
/// Node 1's `context.capsule` is not a record table but the `WorldCollisionCapsule` at 0x1FC -
/// the shape `CompanionWork` keeps, where the record's own `contacts` points at the
/// `WorldCollisionContact` run beside it (here the single record at 0x214). Nodes 2 and 3
/// hold plain tables of four and one, the way `SucklercephWork`'s do.
///
/// The tail from 0x360 is the same run `Actor107000Work` names from 0x360:
/// `field_370`/`field_372` are its animation id and the id the six helper slots
/// last saw, which is why the reset loop walks slots 1..6 and not 1..2.
typedef struct Actor107000Spawn2Work {
    /* 0x000 */ AnimationContext      context;
    /* 0x014 */ AnimationSlot         slots[7];        // six helper slots + slot 0
    /* 0x12C */ byte                  field_12C[0x70]; // pose buffer, animationInitContext poseBuffer
    /* 0x19C */ MATRIX                field_19C;       // colour matrix, TmdObject::colorMtx
    /* 0x1BC */ MATRIX                field_1BC;       // light matrix, TmdObject::lightMtx
    /* 0x1DC */ WorldCollisionBody    obj1;
    /* 0x1FC */ WorldCollisionCapsule field_1FC;
    /* 0x214 */ WorldCollisionContact field_214[1]; // the table `field_1FC` names
    /* 0x22C */ WorldCollisionBody    obj2;
    /* 0x24C */ WorldCollisionContact field_24C[4];
    /* 0x2AC */ WorldCollisionBody    obj3;
    /* 0x2CC */ WorldCollisionContact field_2CC[1];
    /* 0x2E4 */ SVECTOR               rotation;  // reaction rotation applied to model coordinates 3 and 5
    /* 0x2EC */ byte                  pad_2EC[0x50];
    /* 0x33C */ VECTOR3               field_33C; // saved position restored by collision response 2
    /* 0x348 */ byte                  pad_348[0x14];
    EffectSpawnArg                    hitEffect; // Hit effect placement and spawn arguments
    /* 0x364 */ s16                   field_364; // spawn arg's high half
    /* 0x366 */ u16                   field_366; // spawn arg's low half
    /* 0x368 */ byte                  pad_368[0x2];
    /* 0x36A */ s16                   field_36A; // reaction sub-state, as Actor107000Work::field_36A
    /* 0x36C */ s16                   field_36C;
    /* 0x36E */ u16                   field_36E;
    /* 0x370 */ s16                   field_370; // animation id the work is playing
    /* 0x372 */ u16                   field_372; // id the six helper slots last saw
    /* 0x374 */ u16                   field_374;
    /* 0x376 */ byte                  pad_376[2];
    /* 0x378 */ s16                   field_378; // seeded to 0xC8 by the reveal arm
    /* 0x37A */ s16                   field_37A;
    /* 0x37C */ byte                  pad_37C[2];
    /* 0x37E */ s16                   field_37E;
    /* 0x380 */ s16                   field_380;
    /* 0x382 */ s16                   field_382;
    /* 0x384 */ s16                   field_384;
    /* 0x386 */ s16                   field_386;
    /* 0x388 */ s16                   field_388;
    /* 0x38A */ s16                   field_38A;
    /* 0x38C */ s16                   field_38C;
    /* 0x38E */ s16                   field_38E;
    /* 0x390 */ u16                   field_390; // frames until the next 0x60080 spawn
    /* 0x392 */ u16                   field_392; // spawns so far; the cue fires at 5
    /* 0x394 */ u16                   field_394; // non-zero: this frame has spent its reaction
    /* 0x396 */ u16                   field_396; // armed to 1 by the reveal arm, cleared by the hide
    /* 0x398 */ s16                   field_398; // seeded to 0x64 by the reveal arm
    /* 0x39A */ s16                   field_39A; // cleared by the reveal arm
} Actor107000Spawn2Work;
STATIC_ASSERT_SIZEOF(Actor107000Spawn2Work, 0x39C);

/// Node 3's attack row, packed by `Gp_PackPair` into `SucklercephWork::attackBody`, and the enemy
/// parameters whose `attacks` point at it; its `hpMax` seeds the enemy's
/// `field_40`.
extern DamageAttack gSucklercephAttack;

extern EnemyParams gSucklercephParams;

extern PadScriptCmd Actor07000_D06938[];

extern PadScriptVibrationSegment Actor07000_D06944[];

extern TaskMessageEntry gSucklercephDropMsgTable[2];

/// Animation-set table bound to the caged specimen's context by `animationInitContext`.
extern AnimationSet* gSucklercephAnimSets[4];

extern SVECTOR Actor07000_D08068;

/// Offset the collapse arms spawn the 0x60080 effect at.
extern SVECTOR gSucklercephCollapseFxOffset;

/// Pair table the second form's node 3 and the projectile's render node pack
/// into their keys.
extern DamageAttack Actor07000_D08078[2];

/// Enemy parameters of the second form; its `hpMax` seeds the enemy's HP.
extern EnemyParams Actor07000_D08080;

/// Models effect 0x80005 spawns, set in `D_800626EC[5].data.model`, one per
/// random variant the roll selects.
static TmdSource _gActor07000SlouchBurstLeg;

static TmdSource _gActor07000SlouchBurstArm;

static TmdSource _gActor07000SlouchPoison;

/// Animation-set table bound to the second form's context by `animationInitContext`.
extern AnimationSet* Actor07000_D0D77C[13];

extern SVECTOR Actor07000_D0D7B0;

/// Offset the second form's collapse arms spawn the 0x60080 effect at,
/// `{ 0, -0x12C, 0 }`.
extern SVECTOR Actor07000_D0D7B8;

/// Message dispatch table the second form's spawn parks in `Task::msgTable`.
extern TaskMessageEntry Actor07000_D0D7C0[2];

extern TaskDesc Actor07000_D0D7D0[];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void Actor07000_Fn02E0C(Enemy* arg0, Task* arg1);

static void Actor07000_Fn03164(Enemy* arg0, Task* arg1);

static void Actor07000_Fn03460(Task* arg0, TmdObject* arg1, s32 arg2);

static void Actor07000_Fn037EC(Task* arg0, TmdObject* arg1, s32 arg2);

static void Actor07000_Fn03E08(Task* arg0);

static void Actor07000_Fn04274(Task* arg0, s32 arg1);

static void Actor07000_Fn04468(Enemy* arg0, Task* arg1);

/// Picks the reaction branch the specimen takes on this hit and stores it in
/// `field_382`, then hands back the collision record the caller armed. A
/// countdown of 0xBB8 or more, or a record with no slot matching the 0x10000
/// kind, clears the branch and `field_36E` instead. Otherwise the branch is 3
/// when the first `gRandomLcgState` draw folds to under 11, 2 when the target is
/// 2500 units or further. Closer than that, a second draw is taken: it lands on
/// 1 when that draw folds to 11 or more, and the branch stays 2 when it does
/// not. Either way `field_374`/`field_372` are reset, and the record is released.
static void Actor07000_Fn046B8(Task* arg0, s32 arg1);

static s32 Actor07000_Fn047F4(GfxCoord* arg0, u32* arg1);

static void Actor07000_Fn049C0(Task* arg0);

static void Actor07000_Fn04B18(Task* arg0);

static void Actor07000_Fn04E60(Task* arg0);

static void Actor07000_Fn05068(Enemy* arg0, Task* arg1);

static void Actor07000_Fn05400(Enemy* arg0, Task* arg1);

static void Actor07000_Fn0595C(Task* arg0);

static void Actor07000_Fn05ED4(Task* arg0);

static void Actor07000_Fn05F84(Task* task);

static void Actor07000_Fn05FF8(Task* arg0);

static void Actor07000_Fn06088(Task* arg0);

static void Actor07000_Fn060FC(Task* arg0);

static void Actor07000_Fn062A8(Task* arg0);

static void Actor07000_Fn06390(Task* arg0);

static void Actor07000_Fn0662C(Task* arg0);

static void Actor07000_Fn066FC(Task* dst, Task* src);

static void Actor07000_Fn06750(Task* task);

static void Actor07000_Fn06820(Task* arg0);

static void Actor07000_Fn068B4(Task* arg0);

static void Actor07000_Fn068F0(Task* arg0);

static __inline__ void Actor107000_TickAnim(Task* task);
static inline s32      _actor07000ClampToZero(s32 value);
static __inline__ void update_animation(Task* task);
static __inline__ void update_color(Enemy* enemy, GfxCoord* coord);
static __inline__ void rotate_parts(Task* arg0);

/// Rebinds the animation id `field_2B8` to the specimen's two helper slots
/// unless `field_2D2` suppresses the rebind. When the id has changed since the
/// last frame `field_2BA` follows it, the frame count `field_2BC` restarts and
/// both slots are pointed at the new id; otherwise the count ticks and the
/// slots advance by one frame.
static __inline__ void Actor107000_TickAnim(Task* task)
{
    Actor107000Work* work = (Actor107000Work*)task->work;
    s32              i;
    if (work->field_2D2 == 0) {
        if (work->field_2B8 != work->field_2BA) {
            work->field_2BA = work->field_2B8;
            work->field_2BC = 0;
            for (i = 1; i < 3; i++) {
                animationSeekSlotWithBlend((AnimationContext*)work, i, work->field_2B8, 0, 0);
            }
        } else {
            work->field_2BC++;
            for (i = 1; i < 3; i++) {
                animationTickSlot((AnimationContext*)work, i);
            }
        }
    }
}

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// Message dispatch table the caged specimen's spawn parks in `Task::msgTable`.

#include "../../shared/sucklerceph_spawn_state.inc.c"

/// Task states of the caged specimen as `sucklercephTask` dispatches
/// them: spawn, per-frame update and teardown.
static const EnemyTaskFuncTable3 gSucklercephTaskStates = {
    { sucklercephSpawnState, sucklercephUpdateState, sucklercephDeathState },
};

/// Task states of the caged specimen as `sucklercephDropTask` dispatches them:
/// the same update and teardown after a spawn that parks the specimen hidden,
/// and a fourth state for its drop into place.
static const EnemyTaskFuncTable4 gSucklercephDropTaskStates = {
    { sucklercephDropSpawnState, sucklercephUpdateState, sucklercephDeathState, sucklercephDropState },
};

#include "../../shared/sucklerceph_reaction_dispatch.inc.c"

#include "../../shared/sucklerceph_inlines.inc.c"

#include "../../shared/sucklerceph_dormant_tick.inc.c"

#include "../../shared/sucklerceph_awake_tick.inc.c"

/// `value`, or 0 where it is not positive.
static inline s32 _actor07000ClampToZero(s32 value)
{
    if (value <= 0) {
        value = 0;
    }
    return value;
}

#include "../../shared/sucklerceph_contacts.inc.c"

#include "../../shared/sucklerceph_take_damage.inc.c"

#include "../../shared/sucklerceph_turn_to_player.inc.c"

#include "../../shared/sucklerceph_death_state.inc.c"

void sucklercephKill(Task* arg0, u8 arg1)
{
    SucklercephWork* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              soundId;

    obj             = arg0->extra.tmd;
    enemy           = arg0->spawnArg2.pointer;
    work            = arg0->work;
    coord           = obj->coords;
    enemy->hp       = 0;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if (((gRandomLcgState >> 0x10) & 2) || (arg1 & 0xFF)) {
        if (work->variant != 0) {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4046000B;
            SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        } else {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402E0003;
            SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        }
        work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->blastBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        Gp_SpawnEff(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords, 1, NULL);
        Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords, 0x300, &Actor07000_D08068);
        Gp_SpawnScript18(Actor07000_D06938, Actor07000_D06944);
        work->hasBurst = 1;
    } else {
        if (work->variant != 0) {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4046000C;
            SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        } else {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402E0004;
            SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        }
        work->state = SUCKLERCEPH_STATE_SLUMP_DEATH;
    }
}

#include "../../shared/sucklerceph_drop_spawn_state.inc.c"

/// Task states of the specimen's second form as `Actor07000_Fn05E6C`
/// dispatches them: spawn, per-frame update, death and destruction.
static const EnemyTaskFuncTable4 Actor07000_D0003C = {
    { Actor07000_Fn02E0C, Actor07000_Fn03164, Actor07000_Fn04468, enemyDestroy },
};

/// Task states of the specimen's second form as `Actor07000_Fn067B4`
/// dispatches them: the same update, death and destruction after a spawn that
/// parks the model hidden, and a fifth state for its drop into place.
static const EnemyTaskFuncTable5 Actor07000_D0004C = {
    { Actor07000_Fn05068, Actor07000_Fn03164, Actor07000_Fn04468, enemyDestroy, Actor07000_Fn05400 },
};

#include "../../shared/sucklerceph_drop_state.inc.c"

#include "../../shared/sucklerceph_drop_collide.inc.c"

#include "../../shared/sucklerceph_message.inc.c"

#include "../../shared/sucklerceph_task.inc.c"

#include "../../shared/sucklerceph_update_state.inc.c"

#include "../../shared/sucklerceph_reaction_flags.inc.c"

#include "../../shared/sucklerceph_step.inc.c"

#include "../../shared/sucklerceph_animate.inc.c"

#include "../../shared/sucklerceph_colour.inc.c"

#include "../../shared/sucklerceph_draw_shadow.inc.c"

#include "../../shared/sucklerceph_scale_part.inc.c"

#include "../../shared/sucklerceph_flatten.inc.c"

#include "../../shared/sucklerceph_exit.inc.c"

#include "../../shared/sucklerceph_drop_task.inc.c"

#include "../../shared/sucklerceph_fall_step.inc.c"

/// Spawn handler of the specimen's second form, entry 0 of
/// `Actor07000_D0003C`: allocates the 0x39C-byte `Actor107000Spawn2Work`,
/// rebinds the model's light and colour matrices into it, links the enemy
/// node and the three render nodes with their collision tables, seeds the six
/// helper animation slots, draws the two `gRandomLcgState` timers `field_390`/
/// `field_392`, installs `Actor07000_Fn06750` as the exit callback and moves
/// the task on to its per-frame state.
static void Actor07000_Fn02E0C(Enemy* arg0, Task* arg1)
{
    Actor107000Spawn2Work* work;
    WorldCollisionContact* table;
    TmdObject*             obj;
    GfxCoord*              coord;
    GfxCoord*              coord6;
    u32                    rng;
    u32                    rng2;
    s32                    i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x39C, false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    coord6              = &coord[6];
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->field_1BC;
    obj->colorMtx       = &work->field_19C;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->bodyPos.vy             = -0x64;
    arg0->coord                  = coord;
    arg0->node.state.parts.flags = 0;
    arg0->bodyPos.vx             = 0;
    arg0->bodyPos.vz             = 0;
    arg0->param                  = &Actor07000_D08080;
    arg0->hp                     = Actor07000_D08080.hpMax;
    arg0->recs                   = work->field_24C;
    work->hitEffect.coord        = &arg1->extra.tmd->coords[1];
    work->hitEffect.spawnArgLo   = 0x280;
    work->hitEffect.spawnArgHi   = 2;
    animationInitContext(&work->context, Actor07000_D0D77C, obj,
                         (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->field_12C, work->slots);
    for (i = 1; i < 7; i++) {
        animationResetSlot(&work->context, i, 1);
    }
    (Gp_IncStateF0Ref)(0);
    work->field_370            = 1;
    work->field_372            = 1;
    work->field_388            = 0;
    work->field_384            = 0;
    work->field_386            = 0;
    work->field_38E            = 0;
    work->field_38A            = 0;
    work->field_1FC.ends[0].vz = 0xBB8;
    work->field_1FC.end0Radius = 0xFA0;
    work->field_1FC.end1Radius = 0x7D0;
    table                      = work->field_214;
    work->field_1FC.contacts   = table;
    work->obj1.context.capsule = &work->field_1FC;
    work->obj1.coord           = coord;
    work->obj1.pos.vx          = 0;
    work->obj1.pos.vy          = 0;
    work->obj1.pos.vz          = 0;
    work->obj1.key             = 0;
    work->obj1.radius          = 0;
    work->obj1.flags           = WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(table, 1, 0);
    work->obj2.coord            = coord;
    work->obj2.context.contacts = work->field_24C;
    work->obj2.pos.vx           = 0;
    work->obj2.pos.vy           = -0x15E;
    work->obj2.pos.vz           = 0;
    work->obj2.key              = 0x3002A;
    work->obj2.radius           = 0x15E;
    work->obj2.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->obj1.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_LinkObj(2, &work->obj2);
    Gp_InitRec18Table(work->field_24C, 4, 0);
    work->obj2.flags           |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj3.coord            = coord6;
    work->obj3.context.contacts = work->field_2CC;
    work->obj3.pos.vx           = -0x154;
    work->obj3.pos.vy           = 0;
    work->obj3.pos.vz           = 0;
    work->obj3.key              = Gp_PackPair(Actor07000_D08078, 0);
    work->obj3.radius           = 0x1F4;
    work->obj3.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj3);
    Gp_InitRec18Table(work->field_2CC, 1, 0);
    work->field_394    = 0;
    work->obj3.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    rng                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState    = rng;
    work->field_390    = (u16)((rng >> 16) % 20U + 0x50);
    rng2               = rng * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState    = rng2;
    work->field_392    = (u16)((rng2 >> 16) % 50U + 0x32);
    arg1->exitCallback = Actor07000_Fn06750;
    arg1->state       += 1;
}

/// Per-frame mode handler of the specimen. The `gSceneCombatState.actorControl` switch is the same
/// one `sucklercephUpdateState` runs: mode 1 skips to the tail, mode 2 puts
/// the model in its hidden pose and returns, mode 0 clears both flags and falls
/// into the body. The body first dispatches the reaction sub-state `field_36A` -
/// 0 and 1 hand the frame to their own handler, 3 counts `field_36E` out to 0xB
/// before resyncing the animation and dropping back to 0 once the enemy's buildup
/// fires, 4 does the count alone, and 5 adds the 0x392 spawn counter whose fifth
/// hit re-cues the impact sound, switches the task to its death state and clears
/// the transform angle. 4 and 5 share the `field_390` timer that spawns a
/// 0x60080 effect every 0x10 frames. The tail then twists the model's second
/// coordinate part, runs the five per-frame helpers, and clears the display
/// flags of the model's first two parts before recomputing the second.
static void Actor07000_Fn03164(Enemy* arg0, Task* arg1)
{
    TmdObject*       obj;
    Actor107000Work* work;
    GfxCoord*        coord;
    s32              state;
    s32              one;
    s32              soundId;

    obj   = arg1->extra.tmd;
    state = gSceneCombatState.actorControl;
    work  = (Actor107000Work*)arg1->work;
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
case2:
    obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
default_body:
    switch (work->field_36A) {
        case 0:
            Actor07000_Fn03460(arg1, obj, one);
            break;
        case 1:
            Actor07000_Fn037EC(arg1, obj, one);
            break;
        case 3:
            work->field_36E += 1;
            if ((s16)work->field_36E >= 0xB) {
                work->field_372 = 2;
                work->field_370 = 5;
                work->field_374 = 0;
                work->field_36E = 0;
            }
            if (Gp_TickObjFlag2(arg1->spawnArg2.pointer) != 0) {
                work->field_36A = 0;
            }
            break;
        case 4:
            work->field_36E += 1;
            if ((s16)work->field_36E >= 0xB) {
                work->field_372 = 2;
                work->field_370 = 5;
                work->field_374 = 0;
                work->field_36E = 0;
            }
            goto block_21;
        case 5:
            work->field_36E += 1;
            if ((s16)work->field_36E >= 0xB) {
                work->field_372  = 2;
                work->field_370  = 5;
                work->field_374  = 0;
                work->field_36E  = 0;
                work->field_392 += 1;
                if ((u32)work->field_392 >= 5U) {
                    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 3), 0);
                    soundId = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460005;
                    SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    work->field_2CA &= 0x7FFF;
                    arg1->state      = 2;
                    work->field_36C  = 0;
                }
            }
        block_21:
            work->field_390 += 1;
            if ((u32)work->field_390 >= 0x10U) {
                Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, arg1->extra.tmd->coords, 0x400, &Actor07000_D0D7B8);
                work->field_390 = 0;
            }
            break;
    }
    coord->coord.t[1] += 0x80;
    Actor07000_Fn0662C(arg1);
    Actor07000_Fn03E08(arg1);
    Actor07000_Fn05ED4(arg1);
    Actor07000_Fn060FC(arg1);
    Actor07000_Fn06390(arg1);
    arg1->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg1->extra.tmd->coords[1]);
case1:
    Actor07000_Fn05F84(arg1);
}

static void Actor07000_Fn03460(Task* arg0, TmdObject* arg1, s32 arg2)
{
    Actor107000Spawn2Work* work;
    GfxCoord*              coord;
    s32                    soundId;
    s16                    state;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    state = work->field_370 - 1;
    switch (state) {
        case 0:
            work->field_36E++;
            if ((s16)work->field_36E > work->field_390) {
                work->field_36E = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_390 = (gRandomLcgState >> 16) % 20 + 80;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((u16)((gRandomLcgState >> 16) % 100) < 31U) {
                    work->field_370 = 2;
                } else {
                    work->field_370 = 9;
                }
            }
            work->field_380 = 1;
            work->field_378 = 0;
            break;
        case 1:
            work->field_380 = 1;
            work->field_378 = 0;
            if ((s16)work->field_374 == 10) {
                soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460003;
                SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((s16)work->field_374 == 105) {
                soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460006;
                SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((s16)work->field_374 >= 110) {
                work->field_370 = 1;
            }
            break;
        case 8:
            work->field_378 = 0;
            if ((s16)work->field_374 >= 18) {
                work->field_38E = 1;
            }
            if ((s16)work->field_374 >= work->field_392 + 18) {
                work->field_38E = 0;
                work->field_370 = 1;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_392 = (gRandomLcgState >> 16) % 50 + 50;
            }
            break;
        case 4:
            if ((s16)work->field_374 < 22) {
                return;
            }
            work->field_370 = 2;
            break;
        case 5:
            if ((s16)work->field_374 >= 57) {
                work->field_36A = 1;
                work->field_36E = 0;
                work->field_382 = 0;
            }
            break;
    }
    if (Gp_CountRec18Hi(work->field_214, 0x10000) != 0 && (u16)work->field_38E != 0) {
        work->field_36A = 1;
        work->field_36E = 0;
        work->field_382 = 0;
    }
    if (Gp_CountRec18Hi(work->field_24C, 0x10000) != 0 || work->field_388 != 0) {
        work->field_36A = 1;
        work->field_36E = 0;
        work->field_382 = 2;
    }
    Gp_ClearRec18Occupied(work->field_214);
}

static void Actor07000_Fn037EC(Task* arg0, TmdObject* arg1, s32 arg2)
{
    u32                    distance;
    Actor107000Spawn2Work* work;
    GfxCoord*              coord;
    Enemy*                 enemy;
    s32                    soundId;
    s16                    amount;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    switch (work->field_382) {
        case 0:
            work->field_370 = 9;
            if ((s16)work->field_374 >= 18) {
                Gp_SetStateF0Byte3(1);
                Gp_ArmStateF0(1);
            }
            Actor07000_Fn047F4(arg0->extra.tmd->coords, &distance);
            if (Gp_CountRec18Hi(work->field_214, 0x10000) == 0 || distance >= 5000U) {
                work->field_36E++;
                if ((s16)work->field_36E > work->field_390) {
                    work->field_36E = 0;
                    work->field_36A = 0;
                    work->field_370 = 2;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->field_390 = (gRandomLcgState >> 16) % 20 + 80;
                }
            } else {
                work->field_382 = 1;
            }
            Gp_ClearRec18Occupied(work->field_214);
            break;
        case 1:
            Gp_ArmStateF0(1);
            work->field_370 = 4;
            work->field_378 = 0;
            Actor07000_Fn047F4(arg0->extra.tmd->coords, &distance);
            if (distance < 900U) {
                work->field_386 = 0;
            } else if (distance > 2700U) {
                work->field_386 = 0x2000;
            } else {
                work->field_386 = ((distance - 900) << 9) / 100;
            }
            if ((s16)work->field_374 == 40) {
                soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460001;
                SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((u32)(work->field_374 - 32) < 10U) {
                amount = work->field_386;
                if (work->field_384 < amount) {
                    work->field_384 += amount >> 3;
                }
            } else if ((s16)work->field_374 >= 42) {
                if (work->field_384 >= 0x200) {
                    work->field_384 -= 0x250;
                } else {
                    work->field_384 = 0;
                }
            }
            if ((u32)(work->field_374 - 32) < 11U) {
                work->obj3.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            } else {
                work->obj3.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if ((s16)work->field_374 >= 64) {
                Actor07000_Fn046B8(arg0, 1);
            }
            break;
        case 2:
            Gp_ArmStateF0(1);
            work->field_370 = 3;
            if ((s16)work->field_374 == 48) {
                Actor07000_Fn062A8(arg0);
                if (enemy->hp <= 0) {
                    work->obj3.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    arg0->state       = 2;
                    work->field_36C   = 0;
                    return;
                }
            }
            if (work->field_388 != 0 && (s16)work->field_374 == 50) {
                Actor07000_Fn046B8(arg0, 0);
            }
            if ((s16)work->field_374 >= 83) {
                Actor07000_Fn046B8(arg0, 0);
            }
            if (work->field_384 >= 0x200) {
                work->field_384 -= 0x250;
            } else {
                work->field_384 = 0;
            }
            work->obj3.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 3:
            work->field_370 = 2;
            if ((s16)work->field_374 == 105) {
                soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460006;
                SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((s16)work->field_374 >= 110) {
                Actor07000_Fn046B8(arg0, 0);
            }
            break;
        case 4:
            work->field_370 = 7;
            Actor07000_Fn047F4(arg0->extra.tmd->coords, &distance);
            if (distance < 900U) {
                work->field_386 = 0;
            } else if (distance > 2700U) {
                work->field_386 = 0x2000;
            } else {
                work->field_386 = ((distance - 900) << 9) / 100;
            }
            if ((s16)work->field_374 == 30) {
                soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460001;
                SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((u32)(work->field_374 - 25) < 10U) {
                amount = work->field_386;
                if (work->field_384 < amount) {
                    work->field_384 += amount >> 3;
                }
            } else if ((s16)work->field_374 >= 35) {
                if (work->field_384 >= 0x200) {
                    work->field_384 -= 0x250;
                } else {
                    work->field_384 = 0;
                }
            } else {
                if (work->field_384 >= 0x200) {
                    work->field_384 -= 0x250;
                } else {
                    work->field_384 = 0;
                }
            }
            if ((u32)(work->field_374 - 25) < 11U) {
                work->obj3.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            } else {
                work->obj3.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if ((s16)work->field_374 == 2) {
                Actor07000_Fn062A8(arg0);
                if (enemy->hp <= 0) {
                    work->obj3.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    arg0->state       = 2;
                    work->field_36C   = 0;
                    return;
                }
            }
            if ((s16)work->field_374 >= 47) {
                Actor07000_Fn046B8(arg0, 0);
            }
            break;
    }
}

static void Actor07000_Fn03E08(Task* arg0)
{
    s32                    movement;
    s32                    dx;
    s32                    dy;
    s32                    dz;
    s32                    reaction;
    s32                    cooldown;
    u32                    random;
    u32                    kind;
    u32                    damage;
    s32                    i;
    Actor107000Spawn2Work* work;
    GfxCoord*              coord;
    Enemy*                 enemy;
    void*                  head;
    ActorDeltaFrame38*     scratch;

    work     = arg0->work;
    head     = SCRATCH_STACK_RESERVE_BYTES(0x38);
    coord    = arg0->extra.tmd->coords;
    enemy    = arg0->spawnArg2.pointer;
    scratch  = head;
    movement = func_800E0C10(work->field_24C, &scratch->delta, 4, NULL);
    switch (movement) {
        case 0:
            break;
        case 1:
            coord->coord.t[0]  = (s32)(coord->coord.t[0] + scratch->delta.fixed.vx.halves.integer);
            coord->coord.t[1]  = (s32)(coord->coord.t[1] + scratch->delta.fixed.vy.halves.integer);
            coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = (s32)work->field_33C.vx;
            coord->coord.t[1] = (s32)work->field_33C.vy;
            coord->coord.t[2] = work->field_33C.vz;
            break;
    }
    if (work->field_38A != 0) {
        if (--work->field_38A <= 0) {
            work->field_38A = 0;
        }
    }
    for (i = 0; i < 4; i++) {
        kind = work->field_24C[i].key.value & 0xFFFF0000;
        switch (kind) {
            case 0x20000:
                if (work->field_38A == 0) {
                    dx                       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                    scratch->delta.vector.vx = dx;
                    dy                       = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
                    scratch->delta.vector.vy = dy;
                    dz                       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                    scratch->delta.vector.vz = dz;
                    damage                   = Gp_ComputeDamage(work->field_24C[i].key.value, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
                    if (Gp_RollEnemyChance(arg0->spawnArg2.pointer, work->field_24C[i].key.value, 0) != 0) {
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords, 0, 0);
                        damage *= 4;
                    }
                    func_800E2C78(enemy, (s32)work->field_24C[i].key.value, (s32)damage, 0);
                    Actor07000_Fn04274(arg0, (s32)damage);
                    reaction = Gp_GetIdParam0((s32)work->field_24C[i].key.value) & 0xFFFF;
                    switch (reaction) {
                        case 1:
                        case 7:
                            if (work->field_36A < 2) {
                                work->field_374 = 0;
                                work->field_36A = 1;
                                work->field_36E = 0;
                                work->field_382 = 4;
                            }
                            break;
                        case 3:
                            Gp_SetObjFlag4(enemy, (s32)work->field_24C[i].key.value, 0);
                            break;
                        case 2:
                        case 8:
                        case 9:
                            Gp_SetObjFlag2(enemy, (s32)work->field_24C[i].key.value, 0);
                            break;
                        case 4:
                        case 6:
                            if (enemy->hp < 0) {
                                Actor07000_Fn049C0(arg0);
                                work->field_394 = 1;
                            }
                            break;
                    }
                    work->field_38E = 1;
                    func_800FDB18(Gp_GetIdParam1((s32)work->field_24C[i].key.value) & 0xFFFF, (arg0->extra.tmd->coords + 1), &Actor07000_D0D7B0, &work->hitEffect);
                    cooldown = Gp_GetIdParam2((s32)work->field_24C[i].key.value);
                    if ((cooldown << 0x10) > 0) {
                        work->field_38A = (s16)cooldown;
                    }
                    work->field_38C   = 1;
                    random            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState   = random;
                    work->rotation.vx = (s16)(((random >> 0xB) & 0x60) + 0x100);
                }
                break;
            case 0x10000:
                if (work->field_36A == 0) {
                    work->field_36A = 1;
                    work->field_36E = 0;
                    work->field_382 = 2;
                }
                break;
            case 0x30000:
                if (work->field_37E == 0) {
                    if ((Gp_CountRec18Hi(work->field_24C, 0x30000) != 0) && (work->field_37A == 0)) {
                        work->field_378 = 0;
                        work->field_37A = 1;
                        work->field_370 = 1;
                        work->field_37E = 1;
                    }
                } else if (work->field_37A == 0) {
                    work->field_37E = (s16)((u16)work->field_37E - 1);
                }
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_24C);
    Gp_ClearRec18Occupied(work->field_2CC);
    SCRATCH_STACK_RELEASE_BYTES(0x38);
}

/// Hit reaction of the specimen. `arg1` comes off the context's HP countdown
/// and is pushed through the lock-slot updater by the same amount. A spent
/// countdown switches the task to its death state (2), clears the transform
/// angle and drops the work out of the pose; a live one cues the impact sound
/// - bits 12+ of the context's `field_8` pick the sound bank - and then walks
/// the reaction sub-state `field_36A` through its wind-up.
///
/// The sub-state is only advanced while it sits below 2: `arg1` at or above
/// 0x33 lands on the long recoil (sub-state 1, animation 4) and 0x15 or above
/// on the short one (sub-state 0, animation 6). Below both, an idle sub-state
/// with no branch selected re-measures the coordinate with
/// `Actor07000_Fn047F4` and picks branch 2 once the target is 2500
/// units away, branch 1 otherwise.
static void Actor07000_Fn04274(Task* arg0, s32 arg1)
{
    u32              sp10;
    Actor107000Work* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              soundId;
    s16              state;

    enemy      = arg0->spawnArg2.pointer;
    obj        = arg0->extra.tmd;
    coord      = obj->coords;
    work       = (Actor107000Work*)arg0->work;
    enemy->hp -= arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->hp <= 0) {
        SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 3), 0);
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460005;
        SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        work->field_2CA &= 0x7FFF;
        arg0->state      = 2;
        work->field_36C  = 0;
        return;
    }
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 3), 0);
    soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460004;
    SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    state = work->field_36A;
    if (state < 2) {
        if ((u32)arg1 >= 0x33) {
            work->field_36A = 1;
            work->field_374 = 0;
            work->field_36E = 0;
            work->field_382 = 4;
            return;
        }
        if ((u32)arg1 >= 0x15) {
            work->field_374 = 0;
            work->field_36A = 0;
            work->field_36E = 0;
            work->field_370 = 6;
            return;
        }
        if (state == 0 || work->field_382 == 0) {
            Actor07000_Fn047F4(arg0->extra.tmd->coords, &sp10);
            work->field_36A = 1;
            work->field_36E = 0;
            if (sp10 >= 0x9C4) {
                work->field_382 = 2;
                return;
            }
            work->field_382 = 1;
        }
    }
}

/// Task states of a specimen projectile as `Actor07000_Fn06338` dispatches
/// them: launch, flight and the countdown after impact.
static const TaskFuncTable3 Actor07000_D000E0 = {
    { Actor07000_Fn04B18, Actor07000_Fn04E60, Actor07000_Fn068B4 },
};

static __inline__ void update_animation(Task* task)
{
    Actor107000Spawn2Work* work;
    s32                    i;
    work = (Actor107000Spawn2Work*)task->work;
    if (work->field_370 != (s16)work->field_372) {
        work->field_372 = work->field_370;
        work->field_374 = 0;
        for (i = 1; i < 7; i++)
            animationSeekSlotWithBlend(&work->context, i, work->field_370, 0, 8);
    } else {
        work->field_374++;
        for (i = 1; i < 7; i++)
            animationTickSlot(&work->context, i);
    }
}

/// Death handler of the specimen's second form, entry 2 of
/// `Actor07000_D0003C`. `gSceneCombatState.actorControl` mode 1 does nothing and mode 2 hides the
/// model. Otherwise `field_36C` steps the death: phase 0 (unless `field_394`
/// has spent the frame's reaction) cues the death sound, sets the model's flag
/// word to 2 and splices a scaling coordinate into the model through
/// `Actor07000_Fn05FF8`, then unlinks the enemy node and the three
/// render nodes, releases the global state and switches the animation to 0xC;
/// phase 1 flattens that coordinate through `Actor07000_Fn06088` for 0x3D
/// frames; phase 2 cues the closing sound,
/// hides the model, re-parents its second coordinate onto the root and moves
/// the task to state 3. The six helper animation slots are then rebound or
/// ticked with the lighting mode set to 1.
static void Actor07000_Fn04468(Enemy* arg0, Task* arg1)
{
    ActorShared80136288Work* work;
    TmdObject*               obj;
    GfxCoord*                coord;
    GfxCoord*                part;

    obj   = arg1->extra.tmd;
    work  = (ActorShared80136288Work*)arg1->work;
    coord = obj->coords;
    part  = &coord[1];
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            switch (work->field_36C) {
                case 0:
                    if (work->field_394 == 0) {
                        SndEvt_EnqueueType6(SOUND_COMMON(0x0D), 0, 0);
                        obj->flags = TMD_OBJECT_SEMI_TRANS;
                        Actor07000_Fn05FF8(arg1);
                    }
                    arg0->recs = 0;
                    worldTargetUnlinkNode(&arg0->node);
                    Gp_UnlinkObj(&work->field_1DC);
                    Gp_UnlinkObj(&work->field_22C);
                    Gp_UnlinkObj(&work->field_2AC);
                    Gp_ReleaseStateF0Add(arg1, 0x2A);
                    work->field_370 = 0xC;
                    work->field_36E = 0;
                    work->field_36C = 1;
                    break;
                case 1:
                    if (work->field_394 == 0) {
                        Actor07000_Fn06088(arg1);
                    } else {
                        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    work->field_36E++;
                    if ((s16)work->field_36E >= 0x3D) {
                        work->field_36C = 2;
                    }
                    break;
                case 2:
                    SndEvt_EnqueueType7(SOUND_COMMON(0x0D), 1);
                    obj->flags   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    part->parent = coord;
                    arg1->state  = 3;
                    break;
            }
            Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
            update_animation(arg1);
            break;
    }
}

/// Picks the reaction branch in `field_382` from the collision record at
/// `field_214` and the distance to the model in pointer slot 3. With no occupant
/// of kind 0x10000, or at 3000 or more, the branch is cleared. Otherwise a
/// `gRandomLcgState` draw under 11 of 100 selects branch 3; failing that, 2500 or
/// more selects 2, and closer in a second draw picks 2 on the same odds or 1.
/// A chosen branch restarts the animation bookkeeping. The record is released
/// either way.
static void Actor07000_Fn046B8(Task* arg0, s32 arg1)
{
    u32              dist;
    Actor107000Work* work;
    u32              rng;

    work = (Actor107000Work*)arg0->work;
    Actor07000_Fn047F4(arg0->extra.tmd->coords, &dist);
    if (Gp_CountRec18Hi(&work->field_214, 0x10000) == 0 || dist >= 3000) {
        work->field_382 = 0;
        work->field_36E = 0;
    } else {
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        if ((u16)((rng >> 16) % 100) < 11) {
            work->field_382 = 3;
        } else if (dist >= 2500) {
            work->field_382 = 2;
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % 100) < 11) {
                work->field_382 = 2;
            } else {
                work->field_382 = 1;
            }
        }
        work->field_374 = 0;
        work->field_372 = 0;
    }
    Gp_ClearRec18Occupied(&work->field_214);
}

/// Measures the model held in pointer slot 3 from coordinate `arg0`: returns
/// its bearing in `arg0`'s own frame, folded into -0x800..0x800, and stores
/// in `*arg1` the planar x/z distance between the two coordinates' local
/// translations. The work is staged in a block of the scratch stack.
static s32 Actor07000_Fn047F4(GfxCoord* arg0, u32* arg1)
{
    GfxCoord*            other;
    ActorBearingScratch* blk;
    s32                  angle;

    other         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    blk           = SCRATCH_STACK_RESERVE_BLOCK(ActorBearingScratch);
    angle         = actorBearingInFrame(blk, arg0, other);
    blk->delta.vx = other->coord.t[0] - arg0->coord.t[0];
    blk->delta.vz = other->coord.t[2] - arg0->coord.t[2];
    *arg1         = SquareRoot0(blk->delta.vx * blk->delta.vx + blk->delta.vz * blk->delta.vz);
    SCRATCH_STACK_RELEASE_BLOCK(ActorBearingScratch);
    return angle;
}

/// Ground-burst tick of the specimen. The `gRandomLcgState` draw is folded to a
/// variant and each of the three effect-setup records, with its own part of the
/// model, is spawned as effect 0x80005: variant 2 uses part 5, variant 3 part 4,
/// and variants 0 and 1 share part 1. The spawned effect is re-coloured from the
/// specimen's own palette before the tick ends by arming effect 0x60030 on parts
/// 1 and 4.
static void Actor07000_Fn049C0(Task* arg0)
{
    EffectWork* effect;
    s32         r;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    r               = (gRandomLcgState >> 16) & 3;
    switch (r) {
        case 0:
        case 1:
            D_800626EC[5].data.model = &_gActor07000SlouchPoison;
            effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 1, 0, NULL);
            if (effect != NULL) {
                Actor07000_Fn066FC(effect->task, arg0);
            }
            break;
        case 2:
            D_800626EC[5].data.model = &_gActor07000SlouchBurstArm;
            effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 5, 0, NULL);
            if (effect != NULL) {
                Actor07000_Fn066FC(effect->task, arg0);
            }
            break;
        case 3:
            D_800626EC[5].data.model = &_gActor07000SlouchBurstLeg;
            effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 4, 0, NULL);
            if (effect != NULL) {
                Actor07000_Fn066FC(effect->task, arg0);
            }
            break;
    }
    Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 1, 0x300, NULL);
    Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 4, 0x300, NULL);
}

/// Spawn handler of a specimen projectile, entry 0 of `Actor07000_D000E0`.
/// Allocates the 0x58-byte work, spawns effect 0x60081 on the parent's
/// coordinate and re-parents the task under it, and derives a launch velocity
/// from the spawn angle in `spawnArg1` and two `gRandomLcgState` draws, rotated
/// into the coordinate's frame and scaled on the GTE. The coordinate's rotation
/// is reset to identity and nudged by that velocity, and the render node is
/// linked with a capsule collision record keyed by `Actor07000_D08078`. The
/// task takes `Actor07000_Fn068F0` as its exit callback, cues the launch sound
/// and runs its first frame through `Actor07000_Fn04E60`.
static void Actor07000_Fn04B18(Task* arg0)
{
    ActorsShared80136938Work* work;
    GfxCoord*                 coord;
    EffectWork*               eff;
    WorldCollisionBody*       obj;
    WorldCollisionCapsule*    rec;
    SVECTOR*                  vec;
    s32                       angle;
    s32                       pan;

    coord = arg0->extra.tmd->coords;
    work  = memCalloc(0x58, false);
    if (work == NULL) {
        Task_CallExit(arg0);
        return;
    }
    obj                     = &work->obj;
    rec                     = &work->rec;
    vec                     = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    arg0->work              = work;
    eff                     = Gp_SpawnEff(EFFECT_PROJECTILE_GLOW_SPRITE, coord, 0, NULL);
    arg0->spawnArg2.pointer = eff->task;
    taskReparent(arg0, eff->task);
    angle           = arg0->spawnArg1.value;
    vec->vy         = -rcos(angle);
    vec->vx         = rsin(angle);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    vec->vz         = 0xE000 - ((gRandomLcgState >> 16) & 0x1FFF);
    gfxRotateSv(&coord->coord, vec);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gte_lddp(((gRandomLcgState >> 16) & 0x1F) + 0x1E);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(&work->vec);
    gfxSetRotIdentity(&coord->coord);
    coord->coord.t[0]   += work->vec.vx;
    gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    coord->coord.t[1]   += (gRandomLcgState >> 16) & 0x7F;
    coord->coord.t[2]   += work->vec.vz;
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    obj->coord           = coord;
    obj->context.capsule = rec;
    obj->pos.vx          = 0;
    obj->pos.vy          = 0;
    obj->pos.vz          = 0;
    obj->radius          = 0;
    obj->key             = Gp_PackPair(Actor07000_D08078, 1);
    obj->flags           = WORLD_COLLISION_BODY_CAPSULE;
    rec->contacts        = &work->rec2;
    rec->ends[1].vx      = 0;
    rec->ends[1].vy      = 0;
    rec->ends[1].vz      = 0;
    rec->ends[0].vx      = 0;
    rec->ends[0].vy      = 0;
    rec->ends[0].vz      = 0;
    rec->end0Radius      = 0x96;
    rec->end1Radius      = 0x96;
    Gp_InitRec18Table(&work->rec2, 1, 0);
    Gp_LinkObj(3, obj);
    obj->flags        |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    arg0->exitCallback = Actor07000_Fn068F0;
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    arg0->state += 1;
    pan          = (s8)worldCoordGetOriginAudioPan(coord);
    SndEvt_EnqueueType6(SOUND_SUCKLERCEPH_PROJECTILE_LAUNCH, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    Actor07000_Fn04E60(arg0);
}

/// Per-frame handler of a specimen projectile, entry 1 of
/// `Actor07000_D000E0`. `gSceneCombatState.actorControl` mode 1 returns at once and mode 2 hides
/// the model; mode 0 shows it again before the update. The update moves the
/// coordinate by the velocity in the work (mirrored into the first collision
/// record's position), lets the vertical speed grow by 0xA a frame, and tests
/// the second collision record: a hit on an object of the 0x10000 kind or on
/// any occupied slot cues the impact sound, tells the child task how it landed
/// through `spawnArg1` (3, or 2 for a slot hit below -0xC00 in the normal's Y),
/// clears the top bits of the flag word, arms a 0x1E-frame kill countdown and
/// moves on to `Actor07000_Fn068B4`. The collision table is cleared either way.
///
/// The 2/3 pair is written into each arm rather than through a temp: the shared
/// store m2c reads as one variable is `jump.c` cross-jumping the two arms, and
/// a named temp puts the value's live range in front of the comparison that
/// picks it, where it can no longer share `$v0` with the `slti` result.
static void Actor07000_Fn04E60(Task* arg0)
{
    ActorsShared80136c80Work* work;
    TmdObject*                part;
    Task*                     child;
    GfxCoord*                 coord;
    WorldCollisionContact*    rec;
    WorldCollisionContact*    hit;
    WorldCollisionContact*    recs;
    s32                       state;
    s32                       one;

    work  = (ActorsShared80136c80Work*)arg0->work;
    part  = arg0->extra.tmd;
    state = gSceneCombatState.actorControl;
    child = arg0->firstChild;
    rec   = &work->recs[0];
    coord = part->coords;
    hit   = &work->recs[1];
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
    part->flags = 0;
    goto default_body;
case1:
    return;
case2:
    part->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return;
default_body:
    rec->point.vx       = -work->field_0;
    rec->point.vy       = -work->field_2;
    rec->point.vz       = -work->field_4;
    coord->coord.t[0]   = coord->coord.t[0] + (s16)work->field_0;
    recs                = &work->recs[1];
    coord->coord.t[1]   = coord->coord.t[1] + (s16)work->field_2;
    coord->coord.t[2]   = coord->coord.t[2] + (s16)work->field_4;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_2       = work->field_2 + 0xA;
    if (Gp_CountRec18Hi(recs, 0x10000) != 0) {
        SndEvt_EnqueueType6(SOUND_SUCKLERCEPH_PROJECTILE_IMPACT, (s8)worldCoordGetOriginAudioPan(coord),
                            (s8)worldCoordGetOriginAudioDepth(coord));
        if (child != NULL) {
            child->spawnArg1.value = 3;
        }
        goto block_16;
    }
    if (Gp_FindRec18(recs, 0) != 0) {
        SndEvt_EnqueueType6(SOUND_SUCKLERCEPH_PROJECTILE_IMPACT, (s8)worldCoordGetOriginAudioPan(coord),
                            (s8)worldCoordGetOriginAudioDepth(coord));
        if (child != NULL) {
            if (hit->response.direction.vy >= -0xC00) {
                child->spawnArg1.value = 3;
            } else {
                child->spawnArg1.value = 2;
            }
        }
    block_16:
        work->field_26      = work->field_26 & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        arg0->killCountdown = 0x1E;
        arg0->state         = arg0->state + 1;
    }
    Gp_ClearRec18Occupied(&work->recs[1]);
}

/// The variant's spawn handler: allocate the `Actor107000Spawn2Work` block,
/// rebind the model's light and colour matrices into it, link its three `WorldCollisionBody`
/// render nodes and their collision tables, and hand the task over to the state
/// table in `Task::msgTable`.
///
/// Node 1 is the odd one: it points its context at the `WorldCollisionCapsule` at 0x1FC
/// rather than at a record table, and the record's own `contacts` names the one
/// `WorldCollisionContact` beside it - the pair `CompanionWork` keeps, and the three constants it
/// carries are that record's fields rather than an object's. Node 3's `field_8`
/// is the model's seventh coordinate (`&coord[6]`), which is the value the
/// sibling `sucklercephSpawnState` computes for its `part`.
///
/// The spawn arg seeds `field_364`/`field_366` the same way it does there, and
/// a high halfword of 1 kills the specimen instead. The tail draws two numbers
/// off `gRandomLcgState` for `field_390`/`field_392`, the spawn countdown and the
/// running total the spawn cue fires on at 5.
static void Actor07000_Fn05068(Enemy* arg0, Task* arg1)
{
    Actor107000Spawn2Work* work;
    TmdObject*             obj;
    GfxCoord*              coord;
    GfxCoord*              part;
    u32                    draw;
    s32                    one;
    s32                    i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    part  = &coord[6];
    one   = 1;
    if ((s16)(arg1->spawnArg1.value >> 16) == one) {
        enemyDestroy(arg0, arg1);
        return;
    }
    work = memCalloc(0x39CU, false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    work->field_366     = (u16)arg1->spawnArg1.value;
    work->field_364     = (s16)(arg1->spawnArg1.value >> 16);
    obj->flags         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->field_1BC;
    obj->colorMtx       = &work->field_19C;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                  = coord;
    arg0->node.state.parts.flags = one;
    arg0->bodyPos.vx             = 0;
    arg0->bodyPos.vy             = 0;
    arg0->bodyPos.vz             = 0;
    arg0->param                  = &Actor07000_D08080;
    arg0->hp                     = Actor07000_D08080.hpMax;
    arg0->recs                   = &work->field_24C[0];
    work->hitEffect.coord        = &arg1->extra.tmd->coords[1];
    work->hitEffect.spawnArgLo   = 0x100;
    work->hitEffect.spawnArgHi   = one;
    animationInitContext(&work->context, Actor07000_D0D77C, obj, (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->field_12C,
                         &work->slots[0]);
    i = 1;
    do {
        animationResetSlot(&work->context, i, 1);
        i += 1;
    } while (i < 7);
    (Gp_IncStateF0Ref)(0);
    work->field_370            = 1;
    work->field_372            = 1;
    work->field_388            = 0;
    work->field_384            = 0;
    work->field_386            = 0;
    work->field_38E            = 0;
    work->field_38A            = 0;
    work->field_1FC.ends[0].vz = 0xBB8;
    work->field_1FC.end0Radius = 0xFA0;
    work->field_1FC.end1Radius = 0x7D0;
    work->field_1FC.contacts   = work->field_214;
    work->obj1.context.capsule = &work->field_1FC;
    work->obj1.coord           = coord;
    work->obj1.pos.vx          = 0;
    work->obj1.pos.vy          = 0;
    work->obj1.pos.vz          = 0;
    work->obj1.key             = 0;
    work->obj1.radius          = 0;
    work->obj1.flags           = (u32)WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->field_214, 1, 0);
    work->obj2.coord            = coord;
    work->obj2.context.contacts = &work->field_24C[0];
    work->obj2.pos.vx           = 0;
    work->obj2.pos.vy           = -0x258;
    work->obj2.pos.vz           = 0;
    work->obj2.key              = 0x3002A;
    work->obj2.radius           = 0x258;
    work->obj2.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    work->obj1.flags            = (u16)(work->obj1.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    Gp_LinkObj(2, &work->obj2);
    Gp_InitRec18Table(&work->field_24C[0], 4, 0);
    work->obj3.coord            = part;
    work->obj3.context.contacts = &work->field_2CC[0];
    work->obj3.pos.vx           = -0x154;
    work->obj3.pos.vy           = 0;
    work->obj3.pos.vz           = 0;
    work->obj2.flags            = (u16)(work->obj2.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
    work->obj3.key              = Gp_PackPair(Actor07000_D08078, 0);
    work->obj3.radius           = 0x12C;
    work->obj3.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj3);
    Gp_InitRec18Table(&work->field_2CC[0], 1, 0);
    work->field_394  = 0;
    work->field_396  = 0;
    work->obj3.flags = (u16)(work->obj3.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    draw = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->field_390        = (u16)(((u32)draw >> 16) % 20U + 0x50);
    draw = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->field_392        = (u16)(((u32)draw >> 16) % 50U + 0x32);
    arg1->msgTable         = Actor07000_D0D7C0;
    arg1->state            = 4;
}

static __inline__ void update_color(Enemy* enemy, GfxCoord* coord)
{
    VECTOR* block;
    block                        = (VECTOR*)(SCRATCH_STACK_CURSOR(u8) - 0x10);
    SCRATCH_STACK_CURSOR(VECTOR) = block;
    block->vx                    = coord->workm.t[0];
    block->vy                    = coord->workm.t[1];
    block->vz                    = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}
static __inline__ void rotate_parts(Task* arg0)
{
    Actor107000Spawn2Work* work;
    GfxCoord*              coord;
    MATRIX*                scratch;
    u8*                    head;
    s16                    value;

    work                         = (Actor107000Spawn2Work*)arg0->work;
    head                         = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(MATRIX) = (MATRIX*)(head - 0x20);
    scratch                      = (MATRIX*)(head - 0x20);
    coord                        = arg0->extra.tmd->coords;
    RotMatrix(&work->rotation, scratch);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][2]);
    RotMatrix(&work->rotation, scratch);
    gte_SetRotMatrix(&coord[5].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[5].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[5].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[5].coord.m[0][2]);
    value = work->rotation.vx;
    if (value != 0) {
        if (value < 0x21) {
            work->rotation.vx = 0;
            work->field_38C   = 0;
        } else {
            work->rotation.vx = (u16)work->rotation.vx - 0x20;
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Per-frame handler of the specimen's second form while it drops into place,
/// entry 4 of `Actor07000_D0004C`. `gSceneCombatState.actorControl` mode 1 only re-colours the
/// actor and mode 2 hides the model; otherwise, once `field_396` has armed the
/// drop, the root is stepped along its facing and by the fall speed
/// `field_398`, the collision response is applied, the six helper animation
/// slots tick, the reaction twist is applied to coordinates 3 and 5, and the
/// root is recomputed, with the step length `field_378` decaying by 2 a frame.
/// Once the root is below the floor (Y above 0) the landing sound is cued, the
/// twist is armed at 0x400, the root is pinned at 0 and the task moves to
/// state 1; until then the fall speed grows by 10 a frame, or 20 once the
/// collision response has latched `field_39A`.
static void Actor07000_Fn05400(Enemy* arg0, Task* arg1)
{
    Actor107000Spawn2Work* work;
    GfxCoord*              coord;
    s32                    sound;
    s32                    pan;
    work = (Actor107000Spawn2Work*)arg1->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            update_color(arg1->spawnArg2.pointer, &arg1->extra.tmd->coords[1]);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            if (work->field_396 != 0) {
                Actor07000_Fn06820(arg1);
                Actor07000_Fn0595C(arg1);
                update_animation(arg1);
                rotate_parts(arg1);
                Gp_UpdateCoord(arg1->extra.tmd->coords);
                update_color(arg1->spawnArg2.pointer, &arg1->extra.tmd->coords[1]);
                work->field_378 -= 2;
                if (work->field_378 < 0)
                    work->field_378 = 0;
                coord = arg1->extra.tmd->coords;
                if (coord->coord.t[1] > 0) {
                    sound = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0009;
                    pan   = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                    work->field_38C                     = 1;
                    work->rotation.vx                   = 0x400;
                    work->field_398                     = 0;
                    work->field_378                     = 0;
                    arg1->extra.tmd->coords->coord.t[1] = 0;
                    arg1->state                         = 1;
                    return;
                }
                if ((u16)work->field_39A == 0)
                    work->field_398 += 10;
                else
                    work->field_398 += 20;
            }
            break;
    }
}

/// Collision response of the specimen's second form: node 2's collision table
/// is run through `func_800E0C10` with a 0x38-byte scratch. Response 1 adds
/// the returned X and Z offsets to the root; only the first one (while
/// `field_39A` is clear) also adds Y, latches the response in `field_38C` and
/// `field_39A`, arms `field_2E4` to 0x400, sets the fall speed `field_398` to
/// -0x50 and takes a quarter off the step length `field_378`. Response 2 puts
/// the root back at the translation saved in `field_33C`. Both collision
/// tables are released either way.
static void Actor07000_Fn0595C(Task* arg0)
{
    ActorDeltaFrame38*        scratch;
    ActorsShared8013777cWork* work;
    GfxCoord*                 coord;
    s32                       movement;

    work     = (ActorsShared8013777cWork*)arg0->work;
    scratch  = (ActorDeltaFrame38*)SCRATCH_STACK_RESERVE_BYTES(0x38);
    coord    = arg0->extra.tmd->coords;
    movement = func_800E0C10(&work->field_24C[0], &scratch->delta, 4, NULL);
    switch (movement) {
        case 0:
            break;
        case 1:
            if (work->field_39A == 0) {
                work->field_38C    = movement;
                work->field_2E4    = 0x400;
                coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
                work->field_398    = -0x50;
                work->field_378    = work->field_378 - (s16)work->field_378 / 4;
                work->field_39A    = movement;
            }
            coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->field_33C.vx;
            coord->coord.t[1] = work->field_33C.vy;
            coord->coord.t[2] = work->field_33C.vz;
            break;
    }
    Gp_ClearRec18Occupied(&work->field_24C[0]);
    Gp_ClearRec18Occupied(&work->field_2CC);
    SCRATCH_STACK_RELEASE_BYTES(0x38);
}

/// Message 0x7DB handler of the second form's table (`Actor07000_D0D7C0`,
/// parked in `Task::msgTable` by `Actor07000_Fn05068`). The payload's
/// halfword at 0x2 is a command word.
///
/// 4 and 5 are the collapse arms: both spawn the 0x60080 effect on the model's
/// root coordinate, clear the spawn countdown `field_390` and arm the reaction
/// sub-state `field_36A` to 4; 5 clears the spawn counter `field_392` as well.
///
/// Low byte 1 is the reveal. Unless the task already runs one of the two live
/// states, the model is placed at the spawn point bits 8..11 select from the
/// current map's table - `D_shelter_b3_dumping_hole_8018B74C` on map 0x27, where the appearance sound
/// is cued through `SndEvt_EnqueueType6` as well, `D_shelter_b3_garbage_incinerator_801874C4` on 0x28 - the
/// buffers are re-armed, the 0x80 and 4 bits are cleared from the model's flag
/// word, the enemy's `node.state.parts.flags` is zeroed, both render nodes are revealed, and
/// the model is turned to the spawn point's heading. Low byte 3 is the hide:
/// the two bits and the pose flag go the other way, both nodes are hidden, the
/// model's translation and rotation are zeroed, and the task moves to state 4.
s32 Actor07000_Fn05AB8(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    Actor107000Spawn2Work* work;
    Enemy*                 enemy;
    TmdObject*             obj;
    GfxCoord*              coord;
    SVECTOR                rot;
    u16                    word;
    s32                    mode;
    s32                    sound;
    s32                    pan;

    word  = request->command;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    work  = (Actor107000Spawn2Work*)arg0->work;
    mode  = word & 0xFFFF;
    coord = obj->coords;
    if (mode == 4) {
        Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, coord, 0x400, &Actor07000_D0D7B8);
        work->field_390 = 0;
        work->field_36A = 4;
        return 0;
    }
    if (mode == 5) {
        Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, coord, 0x400, &Actor07000_D0D7B8);
        work->field_390 = 0;
        work->field_392 = 0;
        work->field_36A = 4;
        return 0;
    }
    if ((word & 0xFF) == 1) {
        if ((u32)(arg0->state - 1) >= 2U) {
            if (gGameSession->location.loc.area == 0x27) {
                rot.vx            = 0;
                rot.vy            = D_shelter_b3_dumping_hole_8018B74C[request->command >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_shelter_b3_dumping_hole_8018B74C[request->command >> 8].x;
                coord->coord.t[1] = D_shelter_b3_dumping_hole_8018B74C[request->command >> 8].y;
                coord->coord.t[2] = D_shelter_b3_dumping_hole_8018B74C[request->command >> 8].z;
                sound             = (((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54270006);
                pan               = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            } else if (gGameSession->location.loc.area == 0x28) {
                rot.vx            = 0;
                rot.vy            = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].x;
                coord->coord.t[1] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].y;
                coord->coord.t[2] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].z;
            }
            Tmd_AllocBuffers(arg0->extra.tmd);
            arg0->extra.tmd->flags       &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->extra.tmd->flags       &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            enemy->node.state.parts.flags = 0;
            work->obj1.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->obj2.flags             |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            RotMatrix(&rot, &coord->coord);
            work->field_378                       = 0xC8;
            work->field_396                       = 1;
            work->field_398                       = 0x64;
            work->field_39A                       = 0;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(arg0->extra.tmd->coords);
        }
        return 0;
    }
    if ((word & 0xFF) == 3) {
        arg0->extra.tmd->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        arg0->extra.tmd->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->obj1.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj2.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        rot.vz                        = 0;
        rot.vy                        = 0;
        rot.vx                        = 0;
        RotMatrix(&rot, &coord->coord);
        coord->coord.t[2]                     = 0;
        coord->coord.t[1]                     = 0;
        coord->coord.t[0]                     = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(arg0->extra.tmd->coords);
        arg0->state     = 4;
        work->field_396 = 0;
    }
    return 0;
}

void Actor07000_Fn05E6C(Task* arg0)
{
    EnemyTaskFuncTable4 sp;

    sp = Actor07000_D0003C;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Rebinds the second form's animation id `field_370` to its six helper
/// slots. When the id has changed since the last frame `field_372` follows it,
/// the frame count `field_374` restarts and every slot is pointed at the new
/// id with a blend of 8; otherwise the count ticks and the slots advance by
/// one frame.
static void Actor07000_Fn05ED4(Task* arg0)
{
    ActorsShared80137cf4Work* work;
    s32                       i;

    work = arg0->work;
    if (work->field_370 != (s16)work->field_372) {
        work->field_372 = work->field_370;
        work->field_374 = 0;
        for (i = 1; i < 7; i++) {
            animationSeekSlotWithBlend((AnimationContext*)work, i, work->field_370, 0, 8);
        }
    } else {
        work->field_374++;
        for (i = 1; i < 7; i++) {
            animationTickSlot((AnimationContext*)work, i);
        }
    }
}

/// Colours the specimen's second form from the world position of the model's
/// second coordinate, staged in a `VECTOR` taken off the scratch stack; the
/// colour target is the task's enemy.
static void Actor07000_Fn05F84(Task* task)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;
    void*     obj;

    obj                            = task->spawnArg2.pointer;
    coord                          = &task->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    Gp_UpdateActorColor(obj, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Splices the work's own coordinate between the model's root and its second
/// part, with an identity rotation, marks both dirty and resets the scale
/// `SVECTOR` beside it to one. `field_36E` is cleared, and unless the reaction
/// sub-state `field_36A` is 5 the 0x600A5 effect is spawned on the root.
static void Actor07000_Fn05FF8(Task* arg0)
{
    GfxCoord*                parts;
    GfxCoord*                coord;
    GfxMatrix*               mat;
    ActorShared80137e18Work* work;

    work                      = arg0->work;
    parts                     = arg0->extra.tmd->coords;
    coord                     = &work->coord;
    coord->parent             = parts;
    parts[1].parent           = coord;
    mat                       = (GfxMatrix*)&coord->coord;
    mat->rotationWords.m00M01 = ONE;
    mat->rotationWords.m02M10 = 0;
    mat->rotationWords.m11M12 = ONE;
    mat->rotationWords.m20M21 = 0;
    mat->rotationWords.m22    = ONE;
    coord->composeStamp       = GRAPHICS_COORD_DIRTY;
    parts[1].composeStamp     = GRAPHICS_COORD_DIRTY;
    work->scale.vx            = 0x1000;
    work->scale.vy            = 0x1000;
    work->scale.vz            = 0x1000;
    work->field_36E           = 0;
    if (work->field_36A != 5) {
        Gp_SpawnEff(EFFECT_CORPSE_BURN, arg0->extra.tmd->coords, 2, NULL);
    }
}

/// Flattens the coordinate `Actor07000_Fn05FF8` spliced in: the Y scale
/// `field_34E` loses 2 and the matrix's second column is scaled by it, then
/// the coordinate is marked dirty.
static void Actor07000_Fn06088(Task* arg0)
{
    u16                      scale;
    GfxCoord*                coord;
    ActorShared80137ea8Work* work;

    work                     = arg0->work;
    coord                    = &work->coord;
    scale                    = work->field_34E - 2;
    work->field_34E          = scale;
    coord->coord.m[0][1]     = (s16)((s32)(coord->coord.m[0][1] * (s16)scale) >> 0xC);
    coord->coord.m[1][1]     = (s16)((s32)(coord->coord.m[1][1] * (s16)work->field_34E) >> 0xC);
    coord->coord.m[2][1]     = (s16)((s32)(coord->coord.m[2][1] * (s16)work->field_34E) >> 0xC);
    work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Stretches the model's sixth coordinate by the scale delta `field_384`
/// while it is non-zero: the matrix's first column is scaled by
/// `0x1000 + field_384`, the other two by a quarter of that delta, each column
/// through an `SVECTOR` on the GTE, and the coordinate is marked dirty.
static void Actor07000_Fn060FC(Task* arg0)
{
    SVECTOR                   vec;
    MATRIX*                   m;
    ActorsShared80137f1cWork* work;
    GfxCoord*                 coord;

    work  = (ActorsShared80137f1cWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_384 != 0) {
        m = &coord[5].coord;
        gte_ReadMatrixColumn(m, 0, &vec);
        gte_lddp(work->field_384 + 0x1000);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        gte_WriteMatrixColumn(&vec, m, 0);

        gte_ReadMatrixColumn(m, 1, &vec);
        gte_lddp((work->field_384 >> 2) + 0x1000);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        gte_WriteMatrixColumn(&vec, m, 1);

        gte_ReadMatrixColumn(m, 2, &vec);
        gte_lddp((work->field_384 >> 2) + 0x1000);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        gte_WriteMatrixColumn(&vec, m, 2);

        coord[5].composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

static void Actor07000_Fn062A8(Task* arg0)
{
    SVECTOR   offset;
    u32       dist;
    GfxCoord* coords;
    GfxCoord* child;
    Task*     task;
    s32       angle;

    coords    = arg0->extra.tmd->coords;
    child     = &coords[1];
    angle     = Actor07000_Fn047F4(coords, &dist);
    offset.vz = 0;
    offset.vy = 0;
    offset.vx = 0;
    task      = Task_SpawnFromTable(Actor07000_D0D7D0, 1, angle, 0);
    if (task != NULL) {
        Gp_CopyCoordOffset(task, child, &offset);
        taskReparent(arg0, task);
    }
}

/// Task handler of the specimen's contact effect: runs the entry of
/// `Actor07000_D000E0` for the task's state. The table is copied onto the
/// stack before the call.
void Actor07000_Fn06338(Task* task)
{
    TaskFuncTable3 sp;

    sp = Actor07000_D000E0;
    sp.funcs[task->state](task);
}

/// Applies the second form's reaction twist: the rotation `rotation` is
/// turned into a matrix on the scratch stack and multiplied into the rotations
/// of coordinates 3 and 5 on the GTE. The twist's X angle then decays by 0x20
/// a frame; once it would drop to 0x20 or below it is cleared together with
/// `field_38C`.
static void Actor07000_Fn06390(Task* arg0)
{
    Actor107000Spawn2Work* work;
    GfxCoord*              coord;
    MATRIX*                scratch;
    u8*                    head;
    s16                    value;

    work                         = (Actor107000Spawn2Work*)arg0->work;
    head                         = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(MATRIX) = (MATRIX*)(head - 0x20);
    scratch                      = (MATRIX*)(head - 0x20);
    coord                        = arg0->extra.tmd->coords;
    RotMatrix(&work->rotation, scratch);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][2]);
    RotMatrix(&work->rotation, scratch);
    gte_SetRotMatrix(&coord[5].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[5].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[5].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[5].coord.m[0][2]);
    value = work->rotation.vx;
    if (value != 0) {
        if (value < 0x21) {
            work->rotation.vx = 0;
            work->field_38C   = 0;
        } else {
            work->rotation.vx = (u16)work->rotation.vx - 0x20;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Per-frame reaction handler: folds the generic hit flags into the enemy's
/// `reactionFlags`, applies a pending hit, and drops the work to its death pose when
/// the hit lands.
static void Actor07000_Fn0662C(Task* arg0)
{
    Actor107000Work* work;
    Enemy*           enemy;
    s32              tick;
    u8               flags;

    enemy = arg0->spawnArg2.pointer;
    flags = enemy->reactionFlags;
    work  = (Actor107000Work*)arg0->work;
    if (flags != 0) {
        if (flags & ENEMY_REACTION_STAGGER) {
            enemy->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
            work->field_36A       = 3;
            work->field_36E       = 0;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            tick = Gp_TickObjFlag4(enemy);
            if (tick != 0) {
                Actor07000_Fn04274(arg0, tick);
                work->field_36A = 0;
                work->field_370 = 5;
            }
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
        }
    }
}

/// Gives `dst`'s model the texture page and CLUT of `src`'s, re-streaming it
/// twice when it has a buffer.
static void Actor07000_Fn066FC(Task* dst, Task* src)
{
    TmdObject* to;
    TmdObject* from;

    from                  = src->extra.tmd;
    to                    = dst->extra.tmd;
    to->texturePageOffset = from->texturePageOffset;
    to->clutRowOffset     = from->clutRowOffset;
    if (to->buffer != NULL) {
        tmdProcessStream(to);
        tmdProcessStream(to);
    }
}

/// Exit callback of the specimen's second form: flags the enemy's node, takes
/// it and the three render nodes back off their lists and runs the common
/// enemy task exit.
static void Actor07000_Fn06750(Task* task)
{
    ActorShared80138570Work* work;
    Enemy*                   enemy;

    enemy = task->spawnArg2.pointer;
    work  = (ActorShared80138570Work*)task->work;

    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->recs                   = 0;
    worldTargetUnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_1DC);
    Gp_UnlinkObj(&work->field_22C);
    Gp_UnlinkObj(&work->field_2AC);
    enemyTaskExit(task);
}

/// Task handler of the specimen's second form: runs the entry of
/// `Actor07000_D0004C` for the task's state with the enemy and the task. The
/// table is copied onto the stack before the call.
void Actor07000_Fn067B4(Task* task)
{
    EnemyTaskFuncTable5 sp;

    sp = Actor07000_D0004C;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Steps the second form's root one frame: saves the current translation in
/// `field_33C`, advances X and Z along the rotation's Z column scaled by the
/// step length `field_378`, and Y by the fall speed `field_398`.
static void Actor07000_Fn06820(Task* arg0)
{
    GfxCoord*                coord;
    ActorShared80138640Work* work;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;

    work->field_33C.vx = coord->coord.t[0];
    work->field_33C.vy = coord->coord.t[1];
    work->field_33C.vz = coord->coord.t[2];

    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_378) >> 12;
    coord->coord.t[1] += work->field_398;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_378) >> 12;
}

/// Counts the task's kill countdown down and runs its exit callback once it
/// runs out.
static void Actor07000_Fn068B4(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown - 1;
    arg0->killCountdown = temp_v0;
    if ((temp_v0 << 0x10) <= 0) {
        Task_CallExit(arg0);
    }
}

/// Exit callback of the contact effect: takes its render node back off the
/// object list and kills the task.
static void Actor07000_Fn068F0(Task* arg0)
{
    Gp_UnlinkObj(&((ActorShared801511c8Work*)arg0->work)->obj);
    taskKill(arg0);
}
