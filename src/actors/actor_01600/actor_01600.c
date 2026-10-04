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
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
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
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/gfxgte.h"
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

typedef struct Actor01600Range {
    /* 0x0 */ s32 low;
    /* 0x4 */ s32 high;
} Actor01600Range;
STATIC_ASSERT_SIZEOF(Actor01600Range, 0x8);

/// The scavenger's own body in the world's collision lists: one sphere and the contact table it fills.
///
/// The sphere rides the model's root transform, its centre one radius above
/// the root, and carries a key of the category every enemy body uses. Its
/// table is also the enemy record's hit records. Each frame the table's
/// push-back moves the root, a contact of category 2 is a hit the scavenger
/// takes, a contact of category 3 is another enemy's body that pushes it away
/// in some animations, and the occupied entries are then cleared.
typedef struct {
    WorldCollisionBody    body;        // Sphere linked on list 2; floor-query, grid and pair tests are enabled after the link. Radius 0x190, widened to 0x258 over the first frames of an attack animation
    WorldCollisionContact contacts[8]; // Table `body` borrows, also installed as the enemy record's hit records; the last entry is marked LAST
} _Actor01600BodySphere;
STATIC_ASSERT_SIZEOF(_Actor01600BodySphere, 0xE0);

typedef struct Actor01600Work {
    /* 0x000 */ AnimationContext       anim;
    /* 0x014 */ AnimationSlot          slots[9];
    /* 0x17C */ byte                   pad_17C[0x90];
    /* 0x20C */ MATRIX                 field_20C;
    /* 0x22C */ MATRIX                 field_22C;
    /* 0x24C */ GfxCoord               field_24C;
    /* 0x29C */ byte                   field_29C[8];
    /* 0x2A4 */ GfxCoord*              field_2A4;
    /* 0x2A8 */ s8*                    field_2A8;
    /* 0x2AC */ s16                    field_2AC;
    /* 0x2AE */ s16                    field_2AE;
    /* 0x2B0 */ s16                    field_2B0;
    /* 0x2B2 */ s16                    field_2B2;
    /* 0x2B4 */ s32                    field_2B4;
    /* 0x2B8 */ s16                    field_2B8;
    /* 0x2BA */ u16                    field_2BA;
    /* 0x2BC */ byte                   pad_2BC[4];
    /* 0x2C0 */ s16                    field_2C0;
    /* 0x2C2 */ byte                   pad_2C2[0xA];
    /* 0x2CC */ s16                    field_2CC;
    /* 0x2CE */ s16                    field_2CE;
    /* 0x2D0 */ WorldCollisionContact* field_2D0;
    /* 0x2D4 */ WorldCollisionContact  field_2D4;
    /* 0x2EC */ _Actor01600BodySphere  collision;
    /* 0x3CC */ byte                   field_3CC[8];
    /* 0x3D4 */ GfxCoord*              field_3D4;
    /* 0x3D8 */ WorldCollisionContact* field_3D8;
    /* 0x3DC */ s16                    field_3DC;
    /* 0x3DE */ s16                    field_3DE;
    /* 0x3E0 */ s16                    field_3E0;
    /* 0x3E2 */ s16                    field_3E2;
    /* 0x3E4 */ s32                    field_3E4;
    /* 0x3E8 */ s16                    field_3E8;
    /* 0x3EA */ u16                    field_3EA;
    /* 0x3EC */ WorldCollisionContact  contact_3EC;
    /* 0x404 */ EffectSpawnArg         hitEffect;
    /* 0x40C */ byte                   field_40C[8];
    /* 0x414 */ GfxCoord*              field_414;
    /* 0x418 */ s8*                    field_418;
    /* 0x41C */ s16                    field_41C;
    /* 0x41E */ s16                    field_41E;
    /* 0x420 */ s16                    field_420;
    /* 0x422 */ s16                    field_422;
    /* 0x424 */ s32                    field_424;
    /* 0x428 */ s16                    field_428;
    /* 0x42A */ u16                    field_42A;
    /* 0x42C */ s16                    field_42C;
    /* 0x42E */ byte                   pad_42E[2];
    /* 0x430 */ s16                    field_430;
    /* 0x432 */ byte                   pad_432[0xA];
    /* 0x43C */ s16                    field_43C;
    /* 0x43E */ s16                    field_43E;
    /* 0x440 */ WorldCollisionContact* capsuleContacts;
    /* 0x444 */ WorldCollisionContact  capsuleContact;
    /* 0x45C */ Actor01600Range        ranges[8];
    /* 0x49C */ MATRIX                 field_49C;
    /* 0x4BC */ s32                    field_4BC;
    /* 0x4C0 */ s32                    field_4C0;
    /* 0x4C4 */ s32                    field_4C4;
    /* 0x4C8 */ byte                   pad_4C8[4];
    /* 0x4CC */ s16                    field_4CC;
    /* 0x4CE */ byte                   pad_4CE[6];
    /* 0x4D4 */ Task*                  field_4D4;
    /* 0x4D8 */ s16                    field_4D8;
    /* 0x4DA */ s16                    field_4DA;
    /* 0x4DC */ s16                    field_4DC;
    /* 0x4DE */ byte                   pad_4DE[2];
    /* 0x4E0 */ s32                    field_4E0;
    /* 0x4E4 */ s32                    field_4E4;
    /* 0x4E8 */ s16                    field_4E8;
    /* 0x4EA */ s16                    field_4EA;
    /* 0x4EC */ s16                    field_4EC;
    /* 0x4EE */ s16                    field_4EE;
    /* 0x4F0 */ s16                    field_4F0;
    /* 0x4F2 */ s16                    field_4F2;
    /* 0x4F4 */ u16                    field_4F4;
    /* 0x4F6 */ s16                    field_4F6;
    /* 0x4F8 */ s16                    field_4F8;
    /* 0x4FA */ s16                    field_4FA;
    /* 0x4FC */ s16                    field_4FC;
    /* 0x4FE */ s16                    field_4FE;
    /* 0x500 */ s16                    field_500;
    /* 0x502 */ s16                    field_502;
    /* 0x504 */ s16                    field_504;
    /* 0x506 */ s16                    field_506;
    /* 0x508 */ s16                    field_508;
    /* 0x50A */ s16                    field_50A;
    /* 0x50C */ s16                    field_50C;
    /* 0x50E */ s16                    field_50E;
    /* 0x510 */ s16                    field_510;
    /* 0x512 */ s16                    field_512;
    /* 0x514 */ s16                    field_514;
    /* 0x516 */ s16                    field_516;
    /* 0x518 */ s16                    field_518;
    /* 0x51A */ s16                    field_51A;
    /* 0x51C */ s16                    field_51C;
    /* 0x51E */ s16                    field_51E;
    /* 0x520 */ s16                    field_520;
    /* 0x522 */ s16                    field_522;
    /* 0x524 */ s16                    field_524;
    /* 0x526 */ s16                    field_526;
    /* 0x528 */ s16                    field_528;
    /* 0x52A */ s16                    field_52A;
    /* 0x52C */ s16                    field_52C;
    /* 0x52E */ s16                    field_52E;
    /* 0x530 */ s16                    field_530;
    /* 0x532 */ s16                    field_532;
    /* 0x534 */ s16                    field_534;
    /* 0x536 */ s16                    field_536;
    /* 0x538 */ s16                    field_538;
    /* 0x53A */ s16                    field_53A;
    /* 0x53C */ s16                    field_53C;
    /* 0x53E */ s16                    field_53E;
    /* 0x540 */ s16                    field_540;
    /* 0x542 */ u16                    field_542;
    /* 0x544 */ s16                    field_544;
    /// Second animation id the `variant == 2` and `variant == 4` paths of
    /// `Actor01600_Fn05F80` run their countdown against: it is stored into
    /// `field_506` and steps 7 -> 9.
    /* 0x546 */ s16 field_546;
    /* 0x548 */ u16 field_548;
    /// Copy of the spawn variant `Actor01600_Fn05F80` takes its `case 0x1A`
    /// path for.
    /* 0x54A */ s16  field_54A;
    /* 0x54C */ u16  field_54C;
    /* 0x54E */ s16  field_54E;
    /* 0x550 */ s16  field_550;
    /* 0x552 */ byte pad_552[2];
    /* 0x554 */ s16  field_554;
    /* 0x556 */ s16  field_556;
} Actor01600Work;
STATIC_ASSERT_SIZEOF(Actor01600Work, 0x558);
STATIC_ASSERT(OFFSET_OF(Actor01600Work, collision.contacts) == 0x30C, actor01600_contact_offset);
STATIC_ASSERT(OFFSET_OF(Actor01600Work, contact_3EC) == 0x3EC, actor01600_single_contact_offset);
STATIC_ASSERT(OFFSET_OF(Actor01600Work, hitEffect) == 0x404, actor01600_effect_offset);

/// Collision displacement and normalized push vectors in the scratch arena.
typedef struct Actor01600HitScratch {
    /* 0x00 */ byte                pad[0x20];
    /* 0x20 */ WorldCollisionDelta delta;
    /* 0x30 */ VECTOR              normal;
    /* 0x40 */ byte                tail[0xC];
} Actor01600HitScratch;
STATIC_ASSERT_SIZEOF(Actor01600HitScratch, 0x4C);

typedef struct Actor01600RotScratch {
    /* 0x00 */ VECTOR  position;
    /* 0x10 */ SVECTOR rotation;
} Actor01600RotScratch;
STATIC_ASSERT_SIZEOF(Actor01600RotScratch, 0x18);

/// 0x7C-byte scratch from the scratch stack that `Actor01600_Fn045A8` aims from:
/// `delta` takes the world-space offset from the actor to the player, `dir` the
/// same offset written as an `SVECTOR` and then replaced by that offset turned
/// into the actor's own frame, and `mat` the transpose of the actor's rotation
/// the turn multiplies by.
typedef struct Actor01600AimScratch {
    /* 0x00 */ byte    pad_0[0x20];
    /* 0x20 */ VECTOR  delta;
    /* 0x30 */ byte    pad_30[0xC];
    /* 0x3C */ SVECTOR dir;
    /* 0x44 */ byte    pad_44[0x18];
    /* 0x5C */ MATRIX  mat;
} Actor01600AimScratch;
STATIC_ASSERT_SIZEOF(Actor01600AimScratch, 0x7C);

/// 0x30-byte scratch from the scratch stack used by `Actor01600_Fn04C64`: `vec`
/// takes (0, 0, `distance`), `mat` the yaw rotation `RotMatrixY` builds from
/// the work block's `field_4EC`, and `out` the `vec` turned by it - the
/// displacement the actor keeps in `field_42C` / `field_430`.
typedef struct Actor01600YawScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ SVECTOR out;
    /* 0x10 */ MATRIX  mat;
} Actor01600YawScratch;
STATIC_ASSERT_SIZEOF(Actor01600YawScratch, 0x30);

/// Index of the `gPlayerActorTasks` actor nearer to `arg0`, or 0 when slot 0 is
/// empty (or slot 1 is at least as far). The distance is planar: the Y
/// difference is computed into the scratch vector but never enters the sum.
static s32 Actor01600_Fn052C4(Task* arg0);

extern GameActorButtonPressHold Actor01600_D12878;
extern ActorTransform           Actor01600_D12890;

extern s32 Actor01600_D12874;
extern s32 Actor01600_D12870;

/// Models effect 0x80005 spawns, set in `D_800626EC[5].data.model`.
static TmdSource _gActor01600ScavengerBurstHead;
static TmdSource _gActor01600ScavengerBurstLeg;
static TmdSource _gActor01600ScavengerBurstEar;
extern SVECTOR   Actor01600_D12868;

static void Actor01600_Fn070AC(Task* arg0, Task* arg1);

static void Actor01600_Fn01420(Task* arg0);
static void Actor01600_Fn04054(Enemy* arg0, Task* arg1);
void        Actor01600_Fn066E8(Task* arg0);

static s32 Actor01600_Fn045A8(Task* arg0, s32* distance);
static s32 Actor01600_Fn04C64(Task* arg0, s32 distance, s32 angle);

static u8 Actor01600_Fn04EB0(Task* arg0);

/// Scratchpad storage for the actor's ground-quad position and rotated offset.
typedef struct Actor01600GroundScratch {
    /* 0x00 */ VECTOR3 pos;
    /* 0x0C */ s32     pad_C;
    /* 0x10 */ SVECTOR offset;
} Actor01600GroundScratch;
STATIC_ASSERT_SIZEOF(Actor01600GroundScratch, 0x18);

/// 0x3C-byte scratch stack block `Actor01600_Fn06974` steps the attachment
/// coordinate in: the step vector the coordinate's facing is rotated into, the
/// `SVECTOR` `gfxReadMatrixZAxis` reads that facing into, the rotation
/// `RotMatrixY` builds for the yaw and the yaw itself.
typedef struct Actor01600StepScratch {
    /* 0x00 */ VECTOR    move;
    /* 0x10 */ SVECTOR   dir;
    /* 0x18 */ GfxMatrix mat;
    /* 0x38 */ s16       yaw;
    /* 0x3A */ byte      pad_3A[2];
} Actor01600StepScratch;
STATIC_ASSERT_SIZEOF(Actor01600StepScratch, 0x3C);

static void Actor01600_Fn03A60(Task* actor);

extern AnimationPlayRequest Actor01600_D127D8;

static s32  Actor01600_Fn047A0(Task* actor);
static s32  Actor01600_Fn04974(Task* actor, s32 angle, s32 distance, s32 flags);
static void Actor01600_Fn06974(Task* actor, s32 distance);
static s32  Actor01600_Fn06C1C(Task* actor);
static s32  Actor01600_Fn06C94(Task* actor, s32 angle, s32 distance);
static s32  Actor01600_Fn06D74(Task* actor, s32 angle, s32 distance);

extern EnemyParams   Actor01600_D09F0C;
extern AnimationSet* Actor01600_D127EC[31];
// Typed callback views for the task message dispatcher.

extern TaskMessageEntry Actor01600_D127A4[3];
static void             Actor01600_Fn05400(Task* actor);
static void             Actor01600_Fn06EA4(Task* actor);

extern DamageAttack Actor01600_D09F04[2];

extern SVECTOR Actor01600_D09F1C[];
extern SVECTOR Actor01600_D09F3C[];

/// Calls `Gp_ClearRec18Occupied` as a function with a result, although the
/// gameplay definition returns nothing: this actor was compiled against a
/// declaration with one. Called as `void`, the code after the call that ends
/// `Actor01600_Fn04C64` takes different registers and the overlay no longer
/// matches.
MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void Actor01600_Fn001F4(Enemy* ctx, Task* actor);
static void Actor01600_Fn00480(Task* arg0);
static void Actor01600_Fn00674(Enemy* arg0, Task* arg1);
static void Actor01600_Fn00A4C(Task* arg0);
static void Actor01600_Fn00BAC(Task* arg0);
static void Actor01600_Fn0131C(Task* arg0, s32 damage);
static void Actor01600_Fn017BC(Task* arg0);
static void Actor01600_Fn020F8(Task* arg0);
static void Actor01600_Fn03D48(Task* arg0);
static void Actor01600_Fn03EEC(Task* arg0);
static void Actor01600_Fn04AD8(Task* arg0);
static s32  Actor01600_Fn05558(Task* arg0);
static void Actor01600_Fn05F80(Task* arg0);
static void Actor01600_Fn0646C(Task* arg0);
static void Actor01600_Fn06744(Task* arg0);
static void Actor01600_Fn06810(Enemy* arg0, Task* arg1);
static void Actor01600_Fn06880(Task* arg0);
static void Actor01600_Fn06A84(Task* arg0);
static void Actor01600_Fn06F10(Task* arg0);
static u8   Actor01600_Fn06F78(void);
static void Actor01600_Fn06FDC(Task* arg0, s32 arg1);

static AnimationSet _gActor01600Actor101600Animation0A1A8;
static AnimationSet _gActor01600Actor101600Animation0A4C0;
static AnimationSet _gActor01600Actor101600Animation0A610;
static AnimationSet _gActor01600Actor101600Animation0AA20;
static AnimationSet _gActor01600Actor101600Animation0AC58;
static AnimationSet _gActor01600Actor101600Animation0B03C;
static AnimationSet _gActor01600Actor101600Animation0B3DC;
static AnimationSet _gActor01600Actor101600Animation0B7C4;
static AnimationSet _gActor01600Actor101600Animation0BE80;
static AnimationSet _gActor01600Actor101600Animation0C370;
static AnimationSet _gActor01600Actor101600Animation0C6D8;
static AnimationSet _gActor01600Actor101600Animation0C898;
static AnimationSet _gActor01600Actor101600Animation0CA18;
static AnimationSet _gActor01600Actor101600Animation0CD10;
static AnimationSet _gActor01600Actor101600Animation0CFB0;
static AnimationSet _gActor01600Actor101600Animation0D0EC;
static AnimationSet _gActor01600Actor101600Animation0D584;
static AnimationSet _gActor01600Actor101600Animation0D7B4;
static AnimationSet _gActor01600Actor101600Animation0DCA4;
static AnimationSet _gActor01600Actor101600Animation0E168;
static AnimationSet _gActor01600Actor101600Animation0E5C8;
static AnimationSet _gActor01600Actor101600Animation0EA60;
static AnimationSet _gActor01600Actor101600Animation0EE58;
static AnimationSet _gActor01600Actor101600Animation0EFD4;
static AnimationSet _gActor01600Actor101600Animation0F394;
static AnimationSet _gActor01600Actor101600Animation0FB10;
static AnimationSet _gActor01600Actor101600Animation0FED4;
static AnimationSet _gActor01600Actor101600Animation10114;
static AnimationSet _gActor01600Actor101600Animation106BC;
static AnimationSet _gActor01600Actor101600Animation10B6C;
static AnimationSet _gActor01600Actor101600Animation11820;
static AnimationSet _gActor01600Actor101600Animation12074;
static AnimationSet _gActor01600Actor101600Animation1277C;
s32                 Actor01600_Fn05B08(Task* task, s32 msgId, ActorCommand* request, s32 arg3);
s32                 Actor01600_Fn07100(Task*, s32, s32, s32);
void                Actor01600_Fn066E8(Task*);

extern AnimationSet* Actor01600_D127C8[4];

static TmdBone _gActor01600ScavengerBodySkeleton[9] = {
#include "assets/scavenger_body_skeleton.inc"
};

static u32 _gActor01600ScavengerBodyPartVerts[9] = {
#include "assets/scavenger_body_partVerts.inc"
};

static SVECTOR _gActor01600ScavengerBodyVerts[132] = {
#include "assets/scavenger_body_verts.inc"
};

static SVECTOR _gActor01600ScavengerBodyNormals[133] = {
#include "assets/scavenger_body_normals.inc"
};

static u32 _gActor01600ScavengerBodyStream[1324] = {
#include "assets/scavenger_body_stream.inc"
};

static TmdSource _gActor01600ScavengerBody = {
    0,
    7240,
    1956,
    9,
    _gActor01600ScavengerBodyPartVerts,
    _gActor01600ScavengerBodyVerts,
    _gActor01600ScavengerBodyNormals,
    _gActor01600ScavengerBodySkeleton,
    _gActor01600ScavengerBodyStream,
};

static TmdBone _gActor01600ScavengerBurstHeadSkeleton[1] = {
#include "assets/scavenger_burst_head_skeleton.inc"
};

static u32 _gActor01600ScavengerBurstHeadPartVerts[1] = {
#include "assets/scavenger_burst_head_partVerts.inc"
};

static SVECTOR _gActor01600ScavengerBurstHeadVerts[41] = {
#include "assets/scavenger_burst_head_verts.inc"
};

static SVECTOR _gActor01600ScavengerBurstHeadNormals[41] = {
#include "assets/scavenger_burst_head_normals.inc"
};

static u32 _gActor01600ScavengerBurstHeadStream[315] = {
#include "assets/scavenger_burst_head_stream.inc"
};

static TmdSource _gActor01600ScavengerBurstHead = {
    0,
    2200,
    0,
    1,
    _gActor01600ScavengerBurstHeadPartVerts,
    _gActor01600ScavengerBurstHeadVerts,
    _gActor01600ScavengerBurstHeadNormals,
    _gActor01600ScavengerBurstHeadSkeleton,
    _gActor01600ScavengerBurstHeadStream,
};

static TmdBone _gActor01600ScavengerBurstLegSkeleton[1] = {
#include "assets/scavenger_burst_leg_skeleton.inc"
};

static u32 _gActor01600ScavengerBurstLegPartVerts[1] = {
#include "assets/scavenger_burst_leg_partVerts.inc"
};

static SVECTOR _gActor01600ScavengerBurstLegVerts[27] = {
#include "assets/scavenger_burst_leg_verts.inc"
};

static SVECTOR _gActor01600ScavengerBurstLegNormals[27] = {
#include "assets/scavenger_burst_leg_normals.inc"
};

static u32 _gActor01600ScavengerBurstLegStream[241] = {
#include "assets/scavenger_burst_leg_stream.inc"
};

static TmdSource _gActor01600ScavengerBurstLeg = {
    0,
    1580,
    0,
    1,
    _gActor01600ScavengerBurstLegPartVerts,
    _gActor01600ScavengerBurstLegVerts,
    _gActor01600ScavengerBurstLegNormals,
    _gActor01600ScavengerBurstLegSkeleton,
    _gActor01600ScavengerBurstLegStream,
};

static TmdBone _gActor01600ScavengerBurstEarSkeleton[1] = {
#include "assets/scavenger_burst_ear_skeleton.inc"
};

static u32 _gActor01600ScavengerBurstEarPartVerts[1] = {
#include "assets/scavenger_burst_ear_partVerts.inc"
};

static SVECTOR _gActor01600ScavengerBurstEarVerts[8] = {
#include "assets/scavenger_burst_ear_verts.inc"
};

static SVECTOR _gActor01600ScavengerBurstEarNormals[8] = {
#include "assets/scavenger_burst_ear_normals.inc"
};

static u32 _gActor01600ScavengerBurstEarStream[70] = {
#include "assets/scavenger_burst_ear_stream.inc"
};

static TmdSource _gActor01600ScavengerBurstEar = {
    0,
    396,
    0,
    1,
    _gActor01600ScavengerBurstEarPartVerts,
    _gActor01600ScavengerBurstEarVerts,
    _gActor01600ScavengerBurstEarNormals,
    _gActor01600ScavengerBurstEarSkeleton,
    _gActor01600ScavengerBurstEarStream,
};

DamageAttack Actor01600_D09F04[2] = {
    { 12, 7 },
    { 14, 7 },
};

EnemyParams Actor01600_D09F0C = { Actor01600_D09F04, 85, 10, 62, 2, 100, 20, 100, 0 };

SVECTOR Actor01600_D09F1C[4] = {
    { 5632, 0, 3040, 0 },
    { 3168, 0, 1408, 0 },
    { 992, 0, 2880, 0 },
    { -2688, 0, 1408, 0 },
};

SVECTOR Actor01600_D09F3C[5] = {
    { 0x32E0, 0, 832, 0 },
    { 9632, 0, 2432, 0 },
    { 6592, 0, 864, 0 },
    { 3616, 0, 2523, 0 },
    { 2176, 0, 861, 0 },
};

static AnimationPackedPose _gActor01600Actor101600Animation0A1A8Bank1[6] = {
#include "assets/actor_101600_animation_0A1A8_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0A1A8Bank4[46] = {
#include "assets/actor_101600_animation_0A1A8_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0A1A8Records[76] = {
#include "assets/actor_101600_animation_0A1A8_records.inc"
};

static u16 _gActor01600Actor101600Animation0A1A8Indices[10] = {
#include "assets/actor_101600_animation_0A1A8_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0A1A8 = {
    _gActor01600Actor101600Animation0A1A8Records,
    _gActor01600Actor101600Animation0A1A8Indices,
    { NULL, _gActor01600Actor101600Animation0A1A8Bank1, NULL, NULL, _gActor01600Actor101600Animation0A1A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0A4C0Bank1[11] = {
#include "assets/actor_101600_animation_0A4C0_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0A4C0Bank4[61] = {
#include "assets/actor_101600_animation_0A4C0_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0A4C0Records[89] = {
#include "assets/actor_101600_animation_0A4C0_records.inc"
};

static u16 _gActor01600Actor101600Animation0A4C0Indices[10] = {
#include "assets/actor_101600_animation_0A4C0_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0A4C0 = {
    _gActor01600Actor101600Animation0A4C0Records,
    _gActor01600Actor101600Animation0A4C0Indices,
    { NULL, _gActor01600Actor101600Animation0A4C0Bank1, NULL, NULL, _gActor01600Actor101600Animation0A4C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0A610Bank1[4] = {
#include "assets/actor_101600_animation_0A610_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0A610Bank4[14] = {
#include "assets/actor_101600_animation_0A610_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0A610Records[43] = {
#include "assets/actor_101600_animation_0A610_records.inc"
};

static u16 _gActor01600Actor101600Animation0A610Indices[10] = {
#include "assets/actor_101600_animation_0A610_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0A610 = {
    _gActor01600Actor101600Animation0A610Records,
    _gActor01600Actor101600Animation0A610Indices,
    { NULL, _gActor01600Actor101600Animation0A610Bank1, NULL, NULL, _gActor01600Actor101600Animation0A610Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0AA20Bank1[23] = {
#include "assets/actor_101600_animation_0AA20_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0AA20Bank4[70] = {
#include "assets/actor_101600_animation_0AA20_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0AA20Records[106] = {
#include "assets/actor_101600_animation_0AA20_records.inc"
};

static u16 _gActor01600Actor101600Animation0AA20Indices[10] = {
#include "assets/actor_101600_animation_0AA20_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0AA20 = {
    _gActor01600Actor101600Animation0AA20Records,
    _gActor01600Actor101600Animation0AA20Indices,
    { NULL, _gActor01600Actor101600Animation0AA20Bank1, NULL, NULL, _gActor01600Actor101600Animation0AA20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0AC58Bank1[8] = {
#include "assets/actor_101600_animation_0AC58_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0AC58Bank4[41] = {
#include "assets/actor_101600_animation_0AC58_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0AC58Records[62] = {
#include "assets/actor_101600_animation_0AC58_records.inc"
};

static u16 _gActor01600Actor101600Animation0AC58Indices[10] = {
#include "assets/actor_101600_animation_0AC58_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0AC58 = {
    _gActor01600Actor101600Animation0AC58Records,
    _gActor01600Actor101600Animation0AC58Indices,
    { NULL, _gActor01600Actor101600Animation0AC58Bank1, NULL, NULL, _gActor01600Actor101600Animation0AC58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0B03CBank1[14] = {
#include "assets/actor_101600_animation_0B03C_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0B03CBank4[79] = {
#include "assets/actor_101600_animation_0B03C_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0B03CRecords[113] = {
#include "assets/actor_101600_animation_0B03C_records.inc"
};

static u16 _gActor01600Actor101600Animation0B03CIndices[10] = {
#include "assets/actor_101600_animation_0B03C_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0B03C = {
    _gActor01600Actor101600Animation0B03CRecords,
    _gActor01600Actor101600Animation0B03CIndices,
    { NULL, _gActor01600Actor101600Animation0B03CBank1, NULL, NULL, _gActor01600Actor101600Animation0B03CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0B3DCBank1[13] = {
#include "assets/actor_101600_animation_0B3DC_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0B3DCBank4[72] = {
#include "assets/actor_101600_animation_0B3DC_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0B3DCRecords[106] = {
#include "assets/actor_101600_animation_0B3DC_records.inc"
};

static u16 _gActor01600Actor101600Animation0B3DCIndices[10] = {
#include "assets/actor_101600_animation_0B3DC_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0B3DC = {
    _gActor01600Actor101600Animation0B3DCRecords,
    _gActor01600Actor101600Animation0B3DCIndices,
    { NULL, _gActor01600Actor101600Animation0B3DCBank1, NULL, NULL, _gActor01600Actor101600Animation0B3DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0B7C4Bank1[14] = {
#include "assets/actor_101600_animation_0B7C4_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0B7C4Bank4[79] = {
#include "assets/actor_101600_animation_0B7C4_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0B7C4Records[114] = {
#include "assets/actor_101600_animation_0B7C4_records.inc"
};

static u16 _gActor01600Actor101600Animation0B7C4Indices[10] = {
#include "assets/actor_101600_animation_0B7C4_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0B7C4 = {
    _gActor01600Actor101600Animation0B7C4Records,
    _gActor01600Actor101600Animation0B7C4Indices,
    { NULL, _gActor01600Actor101600Animation0B7C4Bank1, NULL, NULL, _gActor01600Actor101600Animation0B7C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0BE80Bank1[24] = {
#include "assets/actor_101600_animation_0BE80_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0BE80Bank4[139] = {
#include "assets/actor_101600_animation_0BE80_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0BE80Records[205] = {
#include "assets/actor_101600_animation_0BE80_records.inc"
};

static u16 _gActor01600Actor101600Animation0BE80Indices[10] = {
#include "assets/actor_101600_animation_0BE80_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0BE80 = {
    _gActor01600Actor101600Animation0BE80Records,
    _gActor01600Actor101600Animation0BE80Indices,
    { NULL, _gActor01600Actor101600Animation0BE80Bank1, NULL, NULL, _gActor01600Actor101600Animation0BE80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0C370Bank1[19] = {
#include "assets/actor_101600_animation_0C370_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0C370Bank4[100] = {
#include "assets/actor_101600_animation_0C370_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0C370Records[144] = {
#include "assets/actor_101600_animation_0C370_records.inc"
};

static u16 _gActor01600Actor101600Animation0C370Indices[10] = {
#include "assets/actor_101600_animation_0C370_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0C370 = {
    _gActor01600Actor101600Animation0C370Records,
    _gActor01600Actor101600Animation0C370Indices,
    { NULL, _gActor01600Actor101600Animation0C370Bank1, NULL, NULL, _gActor01600Actor101600Animation0C370Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0C6D8Bank1[13] = {
#include "assets/actor_101600_animation_0C6D8_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0C6D8Bank4[68] = {
#include "assets/actor_101600_animation_0C6D8_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0C6D8Records[96] = {
#include "assets/actor_101600_animation_0C6D8_records.inc"
};

static u16 _gActor01600Actor101600Animation0C6D8Indices[10] = {
#include "assets/actor_101600_animation_0C6D8_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0C6D8 = {
    _gActor01600Actor101600Animation0C6D8Records,
    _gActor01600Actor101600Animation0C6D8Indices,
    { NULL, _gActor01600Actor101600Animation0C6D8Bank1, NULL, NULL, _gActor01600Actor101600Animation0C6D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0C898Bank1[5] = {
#include "assets/actor_101600_animation_0C898_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0C898Bank4[25] = {
#include "assets/actor_101600_animation_0C898_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0C898Records[57] = {
#include "assets/actor_101600_animation_0C898_records.inc"
};

static u16 _gActor01600Actor101600Animation0C898Indices[10] = {
#include "assets/actor_101600_animation_0C898_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0C898 = {
    _gActor01600Actor101600Animation0C898Records,
    _gActor01600Actor101600Animation0C898Indices,
    { NULL, _gActor01600Actor101600Animation0C898Bank1, NULL, NULL, _gActor01600Actor101600Animation0C898Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0CA18Bank1[5] = {
#include "assets/actor_101600_animation_0CA18_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0CA18Bank4[25] = {
#include "assets/actor_101600_animation_0CA18_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0CA18Records[41] = {
#include "assets/actor_101600_animation_0CA18_records.inc"
};

static u16 _gActor01600Actor101600Animation0CA18Indices[10] = {
#include "assets/actor_101600_animation_0CA18_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0CA18 = {
    _gActor01600Actor101600Animation0CA18Records,
    _gActor01600Actor101600Animation0CA18Indices,
    { NULL, _gActor01600Actor101600Animation0CA18Bank1, NULL, NULL, _gActor01600Actor101600Animation0CA18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0CD10Bank1[11] = {
#include "assets/actor_101600_animation_0CD10_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0CD10Bank4[59] = {
#include "assets/actor_101600_animation_0CD10_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0CD10Records[83] = {
#include "assets/actor_101600_animation_0CD10_records.inc"
};

static u16 _gActor01600Actor101600Animation0CD10Indices[10] = {
#include "assets/actor_101600_animation_0CD10_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0CD10 = {
    _gActor01600Actor101600Animation0CD10Records,
    _gActor01600Actor101600Animation0CD10Indices,
    { NULL, _gActor01600Actor101600Animation0CD10Bank1, NULL, NULL, _gActor01600Actor101600Animation0CD10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0CFB0Bank1[5] = {
#include "assets/actor_101600_animation_0CFB0_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0CFB0Bank4[28] = {
#include "assets/actor_101600_animation_0CFB0_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0CFB0Records[110] = {
#include "assets/actor_101600_animation_0CFB0_records.inc"
};

static u16 _gActor01600Actor101600Animation0CFB0Indices[10] = {
#include "assets/actor_101600_animation_0CFB0_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0CFB0 = {
    _gActor01600Actor101600Animation0CFB0Records,
    _gActor01600Actor101600Animation0CFB0Indices,
    { NULL, _gActor01600Actor101600Animation0CFB0Bank1, NULL, NULL, _gActor01600Actor101600Animation0CFB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0D0ECBank1[4] = {
#include "assets/actor_101600_animation_0D0EC_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0D0ECBank4[18] = {
#include "assets/actor_101600_animation_0D0EC_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0D0ECRecords[34] = {
#include "assets/actor_101600_animation_0D0EC_records.inc"
};

static u16 _gActor01600Actor101600Animation0D0ECIndices[10] = {
#include "assets/actor_101600_animation_0D0EC_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0D0EC = {
    _gActor01600Actor101600Animation0D0ECRecords,
    _gActor01600Actor101600Animation0D0ECIndices,
    { NULL, _gActor01600Actor101600Animation0D0ECBank1, NULL, NULL, _gActor01600Actor101600Animation0D0ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0D584Bank1[18] = {
#include "assets/actor_101600_animation_0D584_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0D584Bank4[90] = {
#include "assets/actor_101600_animation_0D584_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0D584Records[135] = {
#include "assets/actor_101600_animation_0D584_records.inc"
};

static u16 _gActor01600Actor101600Animation0D584Indices[10] = {
#include "assets/actor_101600_animation_0D584_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0D584 = {
    _gActor01600Actor101600Animation0D584Records,
    _gActor01600Actor101600Animation0D584Indices,
    { NULL, _gActor01600Actor101600Animation0D584Bank1, NULL, NULL, _gActor01600Actor101600Animation0D584Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0D7B4Bank1[8] = {
#include "assets/actor_101600_animation_0D7B4_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0D7B4Bank4[40] = {
#include "assets/actor_101600_animation_0D7B4_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0D7B4Records[61] = {
#include "assets/actor_101600_animation_0D7B4_records.inc"
};

static u16 _gActor01600Actor101600Animation0D7B4Indices[10] = {
#include "assets/actor_101600_animation_0D7B4_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0D7B4 = {
    _gActor01600Actor101600Animation0D7B4Records,
    _gActor01600Actor101600Animation0D7B4Indices,
    { NULL, _gActor01600Actor101600Animation0D7B4Bank1, NULL, NULL, _gActor01600Actor101600Animation0D7B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0DCA4Bank1[18] = {
#include "assets/actor_101600_animation_0DCA4_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0DCA4Bank4[98] = {
#include "assets/actor_101600_animation_0DCA4_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0DCA4Records[149] = {
#include "assets/actor_101600_animation_0DCA4_records.inc"
};

static u16 _gActor01600Actor101600Animation0DCA4Indices[10] = {
#include "assets/actor_101600_animation_0DCA4_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0DCA4 = {
    _gActor01600Actor101600Animation0DCA4Records,
    _gActor01600Actor101600Animation0DCA4Indices,
    { NULL, _gActor01600Actor101600Animation0DCA4Bank1, NULL, NULL, _gActor01600Actor101600Animation0DCA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0E168Bank1[17] = {
#include "assets/actor_101600_animation_0E168_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0E168Bank4[96] = {
#include "assets/actor_101600_animation_0E168_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0E168Records[143] = {
#include "assets/actor_101600_animation_0E168_records.inc"
};

static u16 _gActor01600Actor101600Animation0E168Indices[10] = {
#include "assets/actor_101600_animation_0E168_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0E168 = {
    _gActor01600Actor101600Animation0E168Records,
    _gActor01600Actor101600Animation0E168Indices,
    { NULL, _gActor01600Actor101600Animation0E168Bank1, NULL, NULL, _gActor01600Actor101600Animation0E168Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0E5C8Bank1[17] = {
#include "assets/actor_101600_animation_0E5C8_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0E5C8Bank4[90] = {
#include "assets/actor_101600_animation_0E5C8_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0E5C8Records[124] = {
#include "assets/actor_101600_animation_0E5C8_records.inc"
};

static u16 _gActor01600Actor101600Animation0E5C8Indices[10] = {
#include "assets/actor_101600_animation_0E5C8_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0E5C8 = {
    _gActor01600Actor101600Animation0E5C8Records,
    _gActor01600Actor101600Animation0E5C8Indices,
    { NULL, _gActor01600Actor101600Animation0E5C8Bank1, NULL, NULL, _gActor01600Actor101600Animation0E5C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0EA60Bank1[18] = {
#include "assets/actor_101600_animation_0EA60_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0EA60Bank4[95] = {
#include "assets/actor_101600_animation_0EA60_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0EA60Records[130] = {
#include "assets/actor_101600_animation_0EA60_records.inc"
};

static u16 _gActor01600Actor101600Animation0EA60Indices[10] = {
#include "assets/actor_101600_animation_0EA60_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0EA60 = {
    _gActor01600Actor101600Animation0EA60Records,
    _gActor01600Actor101600Animation0EA60Indices,
    { NULL, _gActor01600Actor101600Animation0EA60Bank1, NULL, NULL, _gActor01600Actor101600Animation0EA60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0EE58Bank1[22] = {
#include "assets/actor_101600_animation_0EE58_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0EE58Bank4[66] = {
#include "assets/actor_101600_animation_0EE58_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0EE58Records[107] = {
#include "assets/actor_101600_animation_0EE58_records.inc"
};

static u16 _gActor01600Actor101600Animation0EE58Indices[10] = {
#include "assets/actor_101600_animation_0EE58_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0EE58 = {
    _gActor01600Actor101600Animation0EE58Records,
    _gActor01600Actor101600Animation0EE58Indices,
    { NULL, _gActor01600Actor101600Animation0EE58Bank1, NULL, NULL, _gActor01600Actor101600Animation0EE58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0EFD4Bank1[5] = {
#include "assets/actor_101600_animation_0EFD4_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0EFD4Bank4[24] = {
#include "assets/actor_101600_animation_0EFD4_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0EFD4Records[41] = {
#include "assets/actor_101600_animation_0EFD4_records.inc"
};

static u16 _gActor01600Actor101600Animation0EFD4Indices[10] = {
#include "assets/actor_101600_animation_0EFD4_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0EFD4 = {
    _gActor01600Actor101600Animation0EFD4Records,
    _gActor01600Actor101600Animation0EFD4Indices,
    { NULL, _gActor01600Actor101600Animation0EFD4Bank1, NULL, NULL, _gActor01600Actor101600Animation0EFD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0F394Bank1[13] = {
#include "assets/actor_101600_animation_0F394_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0F394Bank4[70] = {
#include "assets/actor_101600_animation_0F394_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0F394Records[116] = {
#include "assets/actor_101600_animation_0F394_records.inc"
};

static u16 _gActor01600Actor101600Animation0F394Indices[10] = {
#include "assets/actor_101600_animation_0F394_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0F394 = {
    _gActor01600Actor101600Animation0F394Records,
    _gActor01600Actor101600Animation0F394Indices,
    { NULL, _gActor01600Actor101600Animation0F394Bank1, NULL, NULL, _gActor01600Actor101600Animation0F394Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0FB10Bank1[42] = {
#include "assets/actor_101600_animation_0FB10_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0FB10Bank4[139] = {
#include "assets/actor_101600_animation_0FB10_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0FB10Records[199] = {
#include "assets/actor_101600_animation_0FB10_records.inc"
};

static u16 _gActor01600Actor101600Animation0FB10Indices[10] = {
#include "assets/actor_101600_animation_0FB10_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0FB10 = {
    _gActor01600Actor101600Animation0FB10Records,
    _gActor01600Actor101600Animation0FB10Indices,
    { NULL, _gActor01600Actor101600Animation0FB10Bank1, NULL, NULL, _gActor01600Actor101600Animation0FB10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0FED4Bank1[16] = {
#include "assets/actor_101600_animation_0FED4_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0FED4Bank4[75] = {
#include "assets/actor_101600_animation_0FED4_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0FED4Records[103] = {
#include "assets/actor_101600_animation_0FED4_records.inc"
};

static u16 _gActor01600Actor101600Animation0FED4Indices[10] = {
#include "assets/actor_101600_animation_0FED4_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0FED4 = {
    _gActor01600Actor101600Animation0FED4Records,
    _gActor01600Actor101600Animation0FED4Indices,
    { NULL, _gActor01600Actor101600Animation0FED4Bank1, NULL, NULL, _gActor01600Actor101600Animation0FED4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation10114Bank1[8] = {
#include "assets/actor_101600_animation_10114_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation10114Bank4[41] = {
#include "assets/actor_101600_animation_10114_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation10114Records[64] = {
#include "assets/actor_101600_animation_10114_records.inc"
};

static u16 _gActor01600Actor101600Animation10114Indices[10] = {
#include "assets/actor_101600_animation_10114_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation10114 = {
    _gActor01600Actor101600Animation10114Records,
    _gActor01600Actor101600Animation10114Indices,
    { NULL, _gActor01600Actor101600Animation10114Bank1, NULL, NULL, _gActor01600Actor101600Animation10114Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation106BCBank1[21] = {
#include "assets/actor_101600_animation_106BC_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation106BCBank4[120] = {
#include "assets/actor_101600_animation_106BC_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation106BCRecords[164] = {
#include "assets/actor_101600_animation_106BC_records.inc"
};

static u16 _gActor01600Actor101600Animation106BCIndices[10] = {
#include "assets/actor_101600_animation_106BC_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation106BC = {
    _gActor01600Actor101600Animation106BCRecords,
    _gActor01600Actor101600Animation106BCIndices,
    { NULL, _gActor01600Actor101600Animation106BCBank1, NULL, NULL, _gActor01600Actor101600Animation106BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation10B6CBank1[24] = {
#include "assets/actor_101600_animation_10B6C_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation10B6CBank4[87] = {
#include "assets/actor_101600_animation_10B6C_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation10B6CRecords[126] = {
#include "assets/actor_101600_animation_10B6C_records.inc"
};

static u16 _gActor01600Actor101600Animation10B6CIndices[10] = {
#include "assets/actor_101600_animation_10B6C_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation10B6C = {
    _gActor01600Actor101600Animation10B6CRecords,
    _gActor01600Actor101600Animation10B6CIndices,
    { NULL, _gActor01600Actor101600Animation10B6CBank1, NULL, NULL, _gActor01600Actor101600Animation10B6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation11820Bank1[24] = {
#include "assets/actor_101600_animation_11820_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation11820Bank4[299] = {
#include "assets/actor_101600_animation_11820_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation11820Records[422] = {
#include "assets/actor_101600_animation_11820_records.inc"
};

static u16 _gActor01600Actor101600Animation11820Indices[20] = {
#include "assets/actor_101600_animation_11820_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation11820 = {
    _gActor01600Actor101600Animation11820Records,
    _gActor01600Actor101600Animation11820Indices,
    { NULL, _gActor01600Actor101600Animation11820Bank1, NULL, NULL, _gActor01600Actor101600Animation11820Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation12074Bank1[14] = {
#include "assets/actor_101600_animation_12074_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation12074Bank4[198] = {
#include "assets/actor_101600_animation_12074_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation12074Records[273] = {
#include "assets/actor_101600_animation_12074_records.inc"
};

static u16 _gActor01600Actor101600Animation12074Indices[20] = {
#include "assets/actor_101600_animation_12074_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation12074 = {
    _gActor01600Actor101600Animation12074Records,
    _gActor01600Actor101600Animation12074Indices,
    { NULL, _gActor01600Actor101600Animation12074Bank1, NULL, NULL, _gActor01600Actor101600Animation12074Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation1277CBank1[12] = {
#include "assets/actor_101600_animation_1277C_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation1277CBank4[178] = {
#include "assets/actor_101600_animation_1277C_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation1277CRecords[216] = {
#include "assets/actor_101600_animation_1277C_records.inc"
};

static u16 _gActor01600Actor101600Animation1277CIndices[20] = {
#include "assets/actor_101600_animation_1277C_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation1277C = {
    _gActor01600Actor101600Animation1277CRecords,
    _gActor01600Actor101600Animation1277CIndices,
    { NULL, _gActor01600Actor101600Animation1277CBank1, NULL, NULL, _gActor01600Actor101600Animation1277CBank4, NULL, NULL, NULL },
};

TaskMessageEntry Actor01600_D127A4[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, Actor01600_Fn05B08 },
    { 2014, Actor01600_Fn07100 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor01600_D127BC = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, Actor01600_Fn066E8, { .model = &_gActor01600ScavengerBody } };

AnimationSet* Actor01600_D127C8[4] = {
    NULL,
    &_gActor01600Actor101600Animation11820,
    &_gActor01600Actor101600Animation12074,
    &_gActor01600Actor101600Animation1277C,
};

AnimationPlayRequest Actor01600_D127D8 = { { .sets = Actor01600_D127C8 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationSet* Actor01600_D127EC[31] = {
    NULL,
    &_gActor01600Actor101600Animation0A1A8,
    &_gActor01600Actor101600Animation0A4C0,
    &_gActor01600Actor101600Animation0A610,
    &_gActor01600Actor101600Animation0AA20,
    &_gActor01600Actor101600Animation0AC58,
    &_gActor01600Actor101600Animation0B03C,
    &_gActor01600Actor101600Animation0B3DC,
    &_gActor01600Actor101600Animation0B7C4,
    &_gActor01600Actor101600Animation0BE80,
    &_gActor01600Actor101600Animation0C370,
    &_gActor01600Actor101600Animation0C6D8,
    &_gActor01600Actor101600Animation0C898,
    &_gActor01600Actor101600Animation0CA18,
    &_gActor01600Actor101600Animation0CD10,
    &_gActor01600Actor101600Animation0CFB0,
    &_gActor01600Actor101600Animation0D0EC,
    &_gActor01600Actor101600Animation0D584,
    &_gActor01600Actor101600Animation0D7B4,
    &_gActor01600Actor101600Animation0DCA4,
    &_gActor01600Actor101600Animation0E168,
    &_gActor01600Actor101600Animation0E5C8,
    &_gActor01600Actor101600Animation0EA60,
    &_gActor01600Actor101600Animation0EE58,
    &_gActor01600Actor101600Animation0EFD4,
    &_gActor01600Actor101600Animation0F394,
    &_gActor01600Actor101600Animation0FB10,
    &_gActor01600Actor101600Animation0FED4,
    &_gActor01600Actor101600Animation10114,
    &_gActor01600Actor101600Animation106BC,
    &_gActor01600Actor101600Animation10B6C,
};

SVECTOR Actor01600_D12868 = { 0, -100, 0, 0 };

s32 Actor01600_D12870 = 0;

s32 Actor01600_D12874 = 0;

/// Hold sent as `GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES`. The static press
/// count is 1; the grab overwrites it with 5 and selects the animation the
/// fallback companion handler plays.
GameActorButtonPressHold Actor01600_D12878 = { { { .sets = Actor01600_D127C8 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, 1 };

ActorTransform Actor01600_D12890;

static __inline__ void update_actor_color(Enemy* ctx, GfxCoord* attach);

/// Takes a 0x10-byte `VECTOR` from the scratch stack, fills it with `attach`'s
/// world position and hands it to `Gp_UpdateActorColor`. Inlined so the
/// scratch-head address is rematerialised on every access.
static __inline__ void update_actor_color(Enemy* ctx, GfxCoord* attach)
{
    u8*     head;
    VECTOR* block;

    head  = SCRATCH_STACK_CURSOR(u8);
    block = (VECTOR*)(head - 0x10);

    SCRATCH_STACK_CURSOR(VECTOR) = block;

    block->vx = attach->workm.t[0];
    block->vy = attach->workm.t[1];
    block->vz = attach->workm.t[2];
    Gp_UpdateActorColor(ctx, block, 0, 0);

    SCRATCH_STACK_CURSOR(u8) = SCRATCH_STACK_CURSOR(u8) + 0x10;
}

static const EnemyTaskFuncTable3 Actor01600_D00004 = {
    { Actor01600_Fn001F4, Actor01600_Fn00674, Actor01600_Fn04054 },
};

static void Actor01600_Fn001F4(Enemy* ctx, Task* actor)
{
    SVECTOR         sp18;
    GfxCoord*       next_coord;
    s32             i;
    u32             random;
    GfxCoord*       coord;
    TmdObject*      obj;
    Actor01600Work* work;
    MATRIX*         matrix;

    obj        = actor->extra.tmd;
    coord      = obj->coords;
    work       = memCalloc(0x558U, false);
    next_coord = coord + 1;
    if (work == NULL) {
        enemyDestroy(ctx, actor);
        return;
    }
    actor->work                               = work;
    obj->flags                                = 0;
    coord->composeStamp                       = GRAPHICS_COORD_DIRTY;
    obj->lightMtx                             = &work->field_22C;
    obj->colorMtx                             = &work->field_20C;
    work->field_24C.parent                    = &gGfxViewCoord;
    matrix                                    = &work->field_24C.coord;
    MATRIX_PAIR(&work->field_24C.coord, 0, 0) = 0x1000;
    MATRIX_PAIR(matrix, 0, 2)                 = 0;
    MATRIX_PAIR(matrix, 1, 1)                 = 0x1000;
    MATRIX_PAIR(matrix, 2, 0)                 = 0;
    matrix->m[2][2]                           = 0x1000;
    work->field_24C.coord.t[0]                = (s32)coord->coord.t[0];
    work->field_24C.coord.t[1]                = (s32)coord->coord.t[1];
    work->field_24C.coord.t[2]                = (s32)coord->coord.t[2];
    work->field_24C.composeStamp              = GRAPHICS_COORD_DIRTY;
    work->field_4F2                           = 0;
    ctx->field_4                              = &coord->coord;
    ctx->field_48                             = 0;
    Gp_LinkNode(&ctx->node);
    ctx->bodyPos.vy             = -0x190;
    ctx->node.state.parts.flags = 0;
    ctx->coord                  = coord;
    ctx->bodyPos.vx             = 0;
    ctx->bodyPos.vz             = 0;
    ctx->param                  = &Actor01600_D09F0C;
    ctx->recs                   = work->collision.contacts;
    ctx->hp                     = (u16)Actor01600_D09F0C.hpMax;
    work->hitEffect.spawnArgLo  = 0x280;
    work->hitEffect.spawnArgHi  = 2;
    work->hitEffect.coord       = next_coord;
    animationInitContext(&work->anim, Actor01600_D127EC, obj, (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->pad_17C, work->slots);
    for (i = 1; i < 9; i++) {
        animationResetSlot(&work->anim, i, 1);
    }
    Gp_IncStateF0Ref(0);
    work->field_506 = 1;
    work->field_508 = 1;
    work->field_542 = 0x14;
    work->field_4EA = 0;
    work->field_52A = 0;
    work->field_52C = 0;
    work->field_4FA = 0;
    work->field_50C = 0;
    work->field_526 = 0;
    work->field_528 = 0;
    work->field_51E = 0;
    work->field_520 = 0;
    work->field_510 = 0;
    work->field_530 = 0;
    work->field_532 = 0;
    work->field_534 = 0;
    work->field_53A = 0;
    work->field_53C = 0;
    work->field_540 = 0;
    work->field_53E = 0;
    work->field_524 = 0;
    work->field_544 = 0;
    work->field_538 = 0x10;
    random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    work->field_536 = (s16)(((random >> 0x10) & 0x1F) + 1);
    gRandomLcgState = random;
    gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, &sp18);
    work->field_4FC = ratan2((s32)sp18.vx, (s32)sp18.vz);
    Actor01600_Fn05400(actor);
    actor->exitCallback = &Actor01600_Fn06EA4;
    actor->msgTable     = Actor01600_D127A4;
    actor->state        = (s32)(actor->state + 1);
}

static void Actor01600_Fn00480(Task* actor)
{
    Actor01600Work*        work;
    WorldCollisionContact* table1;
    GfxCoord*              coord;
    WorldCollisionContact* table2;
    WorldCollisionContact* table3;
    WorldCollisionContact* table4;

    work            = actor->work;
    coord           = actor->extra.tmd->coords;
    work->field_2C0 = 0xFA0;
    work->field_2CC = 0x384;
    table1          = &work->field_2D4;
    work->field_2CE = 0x64;
    work->field_2D0 = table1;
    work->field_2A8 = work->pad_2BC;
    work->field_2AC = 0;
    work->field_2AE = -0x190;
    work->field_2B0 = 0;
    work->field_2B4 = 0;
    work->field_2B8 = 0;
    work->field_2BA = 3;
    work->field_2A4 = coord;
    Gp_LinkObj(3, (struct WorldCollisionBody*)work->field_29C);
    Gp_InitRec18Table(table1, 1, 0);
    table2                                = work->collision.contacts;
    work->collision.body.coord            = coord;
    work->collision.body.context.contacts = table2;
    work->collision.body.key              = 0x30010;
    work->collision.body.radius           = 0x190;
    work->collision.body.pos.vx           = 0;
    work->collision.body.pos.vy           = -0x190;
    work->collision.body.pos.vz           = 0;
    work->collision.body.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->field_2BA                      |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_LinkObj(2, &work->collision.body);
    Gp_InitRec18Table(table2, ARRAY_SIZE(work->collision.contacts), 0);
    table3                      = &work->capsuleContact;
    work->field_430             = 0x1F4;
    work->field_42C             = 0x1F4;
    work->field_43C             = 1;
    work->field_43E             = 1;
    work->capsuleContacts       = table3;
    work->field_414             = coord;
    work->field_418             = (s8*)&work->field_42C;
    work->field_41C             = 0;
    work->field_41E             = -0x190;
    work->field_420             = 0;
    work->field_424             = 0;
    work->field_428             = 0;
    work->field_42A             = 3;
    work->collision.body.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_LinkObj(2, (struct WorldCollisionBody*)work->field_40C);
    Gp_InitRec18Table(table3, 1, 0);
    table4           = &work->contact_3EC;
    work->field_3D4  = coord;
    work->field_3D8  = table4;
    work->field_3DC  = 0;
    work->field_3DE  = -0x186;
    work->field_3E0  = 0;
    work->field_42A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    work->field_3E4  = Gp_PackPair(Actor01600_D09F04, 1);
    work->field_3E8  = 0x12C;
    work->field_3EA  = 1;
    Gp_LinkObj(3, (struct WorldCollisionBody*)work->field_3CC);
    Gp_InitRec18Table(table4, 1, 0);
    work->field_3EA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

static void Actor01600_Fn00674(Enemy* arg0, Task* arg1)
{
    Actor01600Work* work;
    GfxCoord*       coord;
    TmdObject*      obj;
    s32             id;
    s32             stageAreaKey;
    u16             count;

    work  = arg1->work;
    coord = arg1->extra.tmd->coords;
    if (!(Actor01600_Fn05558(arg1) & 0xFF)) {
        switch (gSceneCombatState.actorControl) {
            case SCENE_COMBAT_ACTORS_RUNNING:
                arg1->extra.tmd->flags       = 0;
                arg0->node.state.parts.flags = 0;
                break;
            case SCENE_COMBAT_ACTORS_PAUSED:
                Actor01600_Fn06810(arg0, arg1);
                goto update;
            case SCENE_COMBAT_ACTORS_HIDDEN:
                obj                          = arg1->extra.tmd;
                obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                return;
            default:
                break;
        }
        Actor01600_Fn04AD8(arg1);
        if (arg0->reactionFlags != 0) {
            Actor01600_Fn00A4C(arg1);
        }
        Actor01600_Fn00BAC(arg1);
        if (work->field_528 == 0) {
            if (work->field_500 != 0) {
                arg1->state = 2;
            }
        }
        stageAreaKey = GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK;
        if (stageAreaKey != GAME_LOCATION_KEY(3, 38, 0, 0) && stageAreaKey != GAME_LOCATION_KEY(4, 7, 0, 0) && stageAreaKey != GAME_LOCATION_KEY(4, 1, 0, 0)) {
            if (coord->coord.t[1] >= 0x65) {
                coord->coord.t[1] = -0xA;
            }
        }
        if (((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(3, 29, 0, 0)) && (coord->coord.t[1] >= -0x3E7)) {
            id = (((u16)((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100005;
            SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            id = (((u16)((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000A;
            SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            Actor01600_Fn06F10(arg1);
            Actor01600_Fn06FDC(arg1, 0);
        }
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) {
            if (coord->coord.t[1] > 0) {
                work->field_532 = 1;
            }
            if (coord->coord.t[1] >= 0x3E9) {
                id = (((u16)((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100005;
                SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                id = (((u16)((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000A;
                SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                Actor01600_Fn06F10(arg1);
                Actor01600_Fn06FDC(arg1, 0);
            }
        }
        Actor01600_Fn01420(arg1);
        Actor01600_Fn03D48(arg1);
        Actor01600_Fn06A84(arg1);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        count           = work->field_550 + 1;
        work->field_550 = count;
        if (((s16)count >= 5) || (gGameSession->viewDirty == 1)) {
            work->field_550 = 0;
            Actor01600_Fn06810(arg0, arg1);
        }
        if (work->field_532 == 0) {
        update:
            Actor01600_Fn03EEC(arg1);
        }
    }
}

static void Actor01600_Fn00A4C(Task* arg0)
{
    Enemy*          ctx;
    Actor01600Work* work;
    s16             state;
    s32             damage;
    u8              flags2;
    u8              flags1;

    ctx    = arg0->spawnArg2.pointer;
    flags1 = ctx->reactionFlags;
    work   = arg0->work;
    if ((flags1 & ENEMY_REACTION_STAGGER) && (work->field_528 == 0)) {
        ctx->reactionFlags = flags1 & ENEMY_REACTION_STAGGER_CLEAR;
        work->field_4FE    = 2;
        work->field_538    = 0x14;
        work->field_504    = 0;
        work->field_506    = 0xE;
    }
    flags2 = ctx->reactionFlags;
    if ((flags2 & ENEMY_REACTION_BUILDUP) && (work->field_4FE != 2) && (work->field_528 == 0)) {
        ctx->reactionFlags = flags2 & ENEMY_REACTION_BUILDUP_CLEAR;
        work->field_4FE    = 3;
        if (work->field_556 != 0) {
            work->field_506 = 0xE;
            work->field_4FE = 5;
        }
        work->field_504 = 0;
    }
    if ((ctx->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) && (work->field_528 == 0)) {
        Actor01600_Fn06F10(arg0);
        damage = Gp_TickObjFlag4(ctx);
        if (damage != 0) {
            state = work->field_4FE;
            if ((state != 2) && (state != 5)) {
                work->field_4FE = 0;
                work->field_506 = 0xA;
            }
            Actor01600_Fn0131C(arg0, damage);
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

static void Actor01600_Fn00BAC(Task* actor)
{
    s32                   distance;
    Task**                slots;
    void*                 world;
    Actor01600Work*       work;
    Enemy*                ctx;
    GfxCoord*             coord;
    s32                   contactIndex;
    Actor01600HitScratch* scratch;
    void*                 old;
    GfxCoord*             other;
    s32                   x, y, z;
    s32                   damage;
    s32                   amount;
    s32                   product;
    s32                   push;
    s32                   clamped;
    s32                   cx, cz;
    s16                   count;
    s32                   mode;
    /* Keep the comparison state local to each reaction branch (GCC 2.8.1). */
    s32 ignoredState;
    work    = actor->work;
    old     = SCRATCH_STACK_CURSOR(void);
    scratch = (SCRATCH_STACK_CURSOR(void) = old - 0x4C);
    ctx     = actor->spawnArg2.pointer;
    coord   = actor->extra.tmd->coords;
    mode    = func_800E0C10(work->collision.contacts, &((Actor01600HitScratch*)(old - sizeof(Actor01600HitScratch)))->delta, ARRAY_SIZE(work->collision.contacts), old - 4);
    world   = coord + 1;
    if (mode == 1)
        goto mode1;
    if (mode < 2)
        goto mode_end;
    if (mode == 2)
        goto mode2;
    goto mode_end;
    {
    mode1:
        coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
        coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
        coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
        goto mode_end;
    mode2:
        coord->coord.t[0] = work->field_4BC;
        coord->coord.t[1] = work->field_4C0;
        coord->coord.t[2] = work->field_4C4;
    }
mode_end:
    slots = gPlayerActorTasks;
    if (work->field_51C != 0) {
        if (--work->field_51C <= 0)
            work->field_51C = 0;
    }
    /* Take the hits and the enemy push-outs the body sphere collected this frame. */
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->collision.contacts); contactIndex++) {
        switch (work->collision.contacts[contactIndex].key.parts.kind) {
            case 2:
                if (work->field_51C == 0) {
                    other                    = slots[(u8)work->collision.contacts[contactIndex].key.parts.id >> 7]->extra.tmd->coords;
                    x                        = other->coord.t[0] - coord->coord.t[0];
                    scratch->delta.vector.vx = x;
                    y                        = other->coord.t[1] - coord->coord.t[1];
                    scratch->delta.vector.vy = y;
                    z                        = other->coord.t[2] - coord->coord.t[2];
                    scratch->delta.vector.vz = z;
                    damage                   = Gp_ComputeDamage(work->collision.contacts[contactIndex].key.value, SquareRoot0(x * x + y * y + z * z), 0, 0);
                    if (Gp_RollEnemyChance(actor->spawnArg2.pointer, work->collision.contacts[contactIndex].key.value, 0)) {
                        damage *= 4;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords + 1, 0, 0);
                    }
                    if (work->field_4FE == 1 && work->field_528 != 0 && work->field_51E < 0) {
                        damage *= 2;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords + 1, 3, 0);
                    }
                    func_800E2C78(ctx, work->collision.contacts[contactIndex].key.value, damage, 0);
                    Actor01600_Fn0131C(actor, damage);
                    count = Gp_GetIdParam2(work->collision.contacts[contactIndex].key.value);
                    if (count > 0)
                        work->field_51C = count;
                    switch (Gp_GetIdParam0(work->collision.contacts[contactIndex].key.value) & 0xFFFF) {
                        case 4:
                        case 6:
                            if ((s16)ctx->hp <= 0)
                                goto dead;
                            Gp_SetObjFlag1(actor->spawnArg2.pointer);
                            break;
                        case 2:
                        case 9:
                            if (work->field_4FE != 3) {
                                ignoredState = 5;
                                if (work->field_4FE != ignoredState) {
                                    Gp_SetObjFlag2(actor->spawnArg2.pointer, work->collision.contacts[contactIndex].key.value, 0);
                                    work->field_556 = 1;
                                }
                            }
                            break;
                        case 8:
                            if (work->field_4FE != 3) {
                                ignoredState = 5;
                                if (work->field_4FE != ignoredState) {
                                    Gp_SetObjFlag2(actor->spawnArg2.pointer, work->collision.contacts[contactIndex].key.value, 0);
                                    work->field_556 = 0;
                                }
                            }
                            break;
                        case 1:
                        case 5:
                            if (work->field_4FE != 3) {
                                ignoredState = 5;
                                if (work->field_4FE != ignoredState) {
                                    Gp_SetObjFlag1(actor->spawnArg2.pointer);
                                }
                            }
                            break;
                        case 0:
                            break;
                        case 3:
                            Gp_SetObjFlag4(actor->spawnArg2.pointer, work->collision.contacts[contactIndex].key.value, 0);
                            break;
                    }
                    if (damage >= 40 && work->field_556 == 0) {
                        Gp_SetObjFlag1(ctx);
                        if (work->field_4FE == 0 && work->field_528 != 0) {
                            work->field_528 = 0;
                            work->field_51E = 0;
                            work->field_520 = 0;
                        }
                    } else {
                        work->field_522 = 1;
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->field_4CC = ((gRandomLcgState >> 11) & 0x60) + 0x100;
                    }
                    if (work->field_4FE != 0 && work->field_528 != 0) {
                        work->field_3EA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        Gp_ClearRec18Occupied(&work->contact_3EC);
                        work->field_4FA  = 0;
                        work->field_51E += 20;
                        amount           = Actor01600_Fn045A8(actor, &distance);
                        if (amount < 0)
                            amount = -amount;
                        if (amount < 0x400) {
                            work->field_506 = 14;
                            work->field_50E = -40;
                        } else {
                            work->field_506 = 11;
                            work->field_50E = 40;
                        }
                        work->field_516 = 8;
                    }
                    func_800FDB18(Gp_GetIdParam1(work->collision.contacts[contactIndex].key.value) & 0xFFFF, world, 0, &work->hitEffect);
                }
                break;
            case 3:
                cx                       = coord->workm.t[0] - work->collision.contacts[contactIndex].point.vx;
                scratch->delta.vector.vy = 0;
                scratch->delta.vector.vx = cx;
                cz                       = coord->workm.t[2] - work->collision.contacts[contactIndex].point.vz;
                scratch->delta.vector.vz = cz;
                push                     = cx * cx + cz * cz;
                push                     = SquareRoot0(push);
                push                     = -push;
                push                    += work->collision.contacts[contactIndex].distance;
                clamped                  = push;
                if (push <= 0)
                    clamped = 0;
                push                     = clamped;
                scratch->delta.vector.vx = coord->workm.t[0] - work->collision.contacts[contactIndex].point.vx;
                scratch->delta.vector.vy = coord->workm.t[1] - work->collision.contacts[contactIndex].point.vy;
                scratch->delta.vector.vz = coord->workm.t[2] - work->collision.contacts[contactIndex].point.vz;
                VectorNormal(&scratch->delta.vector, &scratch->normal);
                ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->delta.vector);
                if (work->field_506 == 23 || work->field_506 == 5 || work->field_506 == 6) {
                    coord->coord.t[0] += (push * scratch->delta.vector.vx) >> 12;
                    product            = push * scratch->delta.vector.vy;
                    if (product < 0)
                        coord->coord.t[1] += product >> 12;
                    coord->coord.t[2] += (push * scratch->delta.vector.vz) >> 12;
                }
                break;
            case 0:
            case 1:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
                break;
        }
    }
    Gp_ClearRec18Occupied(work->collision.contacts);
    if (work->field_516 && Gp_FindRec18(&work->contact_3EC, 0)) {
        work->collision.body.pos.vy = -400;
        work->collision.body.radius = 400;
        work->field_52A             = 1;
        work->field_3EA            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(&work->contact_3EC);
        if (work->field_50A < 15) {
            work->field_516  = 8;
            work->field_4FA  = 0;
            work->field_51E += 20;
            amount           = Actor01600_Fn045A8(actor, &distance);
            if (amount < 0)
                amount = -amount;
            if (amount >= 0x400)
                goto far_angle;
            work->field_506 = 14;
            work->field_50E = -40;
            goto release;
        dead:
            work->field_540 = 0;
            Actor01600_Fn0646C(actor);
            work->field_540 = 2;
            work->field_528 = 0;
            return;
        far_angle:
            work->field_506 = 11;
            work->field_50E = 40;
        }
    }
release:
    SCRATCH_STACK_RELEASE_BYTES(0x4C);
    return;
}

static void Actor01600_Fn0131C(Task* arg0, s32 damage)
{
    s32             id;
    s32             pan;
    Enemy*          ctx;
    Actor01600Work* work;
    GfxCoord*       coord;

    ctx     = arg0->spawnArg2.pointer;
    work    = arg0->work;
    coord   = arg0->extra.tmd->coords;
    ctx->hp = (u16)(ctx->hp - damage);
    func_800DA6E8(&ctx->node, damage, 0);
    work->field_536 = 1;
    if ((s16)ctx->hp <= 0) {
        work->field_500  = 1;
        work->field_502  = 0;
        work->field_3EA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        id               = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000A;
        SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    } else {
        if (work->field_506 == 1) {
            work->field_506 = 0xA;
            work->field_50A = 0;
        }
        work->field_50E = 0;
        work->field_51A = 0;
        id              = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100009;
        pan             = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
}

/// Per-frame tick for the actor's cornered/pursuit cycle, dispatched on
/// `field_4FE`. States 0 and 1 hand the frame to `Actor01600_Fn017BC` /
/// `Actor01600_Fn020F8` and then run the shared post-step
/// `Actor01600_Fn06744`. State 2 advances `field_504`, nudges `field_50E` back
/// by 0x3C while the animation is still 0xE and under 0x11 frames in, arms the
/// 0x16 animation at frame 0x28 and, past frame 0x5B, resets to animation 0x19
/// with the 0x8000 bit set in `field_2BA`. State 3 plays animation 0x13 until
/// `Gp_TickObjFlag2` fires. State 4 only selects animation 0x11. State 5 rolls
/// the 0x4CC swerve offset once per approach (animation 0xE, past frame 0x2C,
/// `field_522` still clear and bit 1 of `field_50A` set) and, on animation
/// 0x16 past frame 0x32, ends the cycle the same way state 2 does.
///
/// Whatever the state, animations 1/9/0x10/0x13/0x15/0x16/0x1B..0x1E are
/// silent; the rest count `field_542` down and, on expiry, play one of three
/// growls (`0x4010_0006..8`) picked by a `gRandomLcgState` draw modulo 5 - two of
/// the five outcomes stay quiet - panned and attenuated for the actor's
/// coordinate, then rearm the counter at 0x14.
static void Actor01600_Fn01420(Task* arg0)
{
    Actor01600Work* work;
    GfxCoord*       coord;
    s32             id;
    s32             state;
    s32             one;
    u16             sel;
    s16             count;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;

    switch (work->field_4FE) {
        case 0:
            Actor01600_Fn017BC(arg0);
            goto tick;
        case 1:
            Actor01600_Fn020F8(arg0);
            goto tick;
        case 2:
            Actor01600_Fn06F10(arg0);
            work->field_504 = work->field_504 + 1;
            if (work->field_506 == 0xE && work->field_50A < 0x11) {
                work->field_50E = -0x3C;
                Actor01600_Fn06744(arg0);
            } else {
                work->field_50E = 0;
            }
            if (work->field_504 == 0x28) {
                work->field_538 = 0x10;
                work->field_506 = 0x16;
            }
            if (work->field_504 >= 0x5B) {
                work->field_506  = 0x19;
                work->field_4FE  = 0;
                work->field_504  = 0;
                work->field_2BA |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            goto clear;
        case 3:
            Actor01600_Fn06F10(arg0);
            work->field_506 = 0x13;
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                work->field_4FE  = 0;
                work->field_506  = 0x19;
                work->field_2BA |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            goto clear;
        case 4:
            work->field_506 = 0x11;
            break;
        case 5:
            Actor01600_Fn06F10(arg0);
            if (work->field_506 == 0xE) {
                if (work->field_50A >= 0x2C && work->field_522 == 0 &&
                    ((u16)work->field_50A & 2)) {
                    work->field_522 = 1;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->field_4CC = ((gRandomLcgState >> 11) & 0x60) + 0x20;
                }
            } else if (work->field_506 == 0x16) {
                if (work->field_50A >= 0x32) {
                    work->field_4FE  = 0;
                    work->field_506  = 0x19;
                    work->field_2BA |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
            }
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                work->field_506 = 0x16;
            }
            goto clear;
    }
    goto tail;

clear:
    work->field_50E = 0;
tick:
    Actor01600_Fn06744(arg0);

tail:
    state = work->field_506;
    one   = 1;
    if (state == one || state == 0x16 || state == 0x15 || state == 0x10 ||
        state == 0x13 || state == 0x1C || state == 0x1D || state == 0x1E ||
        state == 0x1B || state == 9) {
        return;
    }
    count           = work->field_542 - 1;
    work->field_542 = count;
    if (count != 0) {
        return;
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    sel             = (gRandomLcgState >> 16) % 5;
    switch (sel) {
        case 0:
            id = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100006;
            SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 1:
            id = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100007;
            SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 2:
            id = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100008;
            SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            break;
    }
    work->field_542 = 0x14;
}

static void Actor01600_Fn017BC(Task* actor)
{
    Enemy*                 ctx;
    Actor01600Work*        work;
    GfxCoord*              coord;
    TmdObject*             model;
    s16                    frameOffset;
    s16                    count;
    s16                    tick;
    s16                    state;
    s16                    height;
    s16                    repeatHeight;
    s16                    frame;
    s32                    contact;
    s32                    id;
    void*                  old;
    s32                    pan8;
    s32                    distance;
    WorldCollisionContact* rec;
    s32                    pan1;
    s32                    pan2;
    s32                    pan3;
    s32                    pan4;
    s32                    pan5;
    s32                    pan6;
    s32                    pan7;
    u16                    flags;
    u16                    attackFrame;

    work                       = actor->work;
    old                        = SCRATCH_STACK_CURSOR(void);
    rec                        = &work->field_2D4;
    SCRATCH_STACK_CURSOR(void) = old - 8;
    model                      = actor->extra.tmd;
    coord                      = model->coords;
    ctx                        = actor->spawnArg2.pointer;
    if (Gp_CountRec18Hi(rec, 0x10000) != 0) {
        work->field_51A = 1;
    }
    if (work->field_51A != 0) {
        work->field_4FE  = 1;
        work->field_516  = 0;
        work->field_510  = 0;
        work->field_4FA  = 0;
        work->field_528  = 0;
        work->field_51E  = 0;
        work->field_520  = 0;
        work->field_50A  = 0;
        work->field_4EA  = 0;
        work->field_2BA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->field_42A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(rec);
    if (work->field_53A == 1) {
        count           = (u16)work->field_536 - 1;
        work->field_536 = count;
        if (count == 0) {
            work->field_50A = 0;
            work->field_506 = 2;
            work->field_53A = 0;
        }
    }
    tick            = (u16)work->field_504 + 1;
    work->field_504 = tick;
    if (tick >= 0x1F) {
        work->field_504 = 0;
    }
    state = (u16)work->field_506 - 1;
    switch (state) {
        case 0:
            work->field_2C0 = 0x3E8;
            work->field_50E = 0;
            work->field_538 = 0x10;
            if (work->field_50A >= 0x3E) {
                work->field_50A = 0;
            }
            if (ctx->place->mode == 0) {
                if ((gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_NOISE) || (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_CAST_FOOTSTEP_OR_ALERT)) {
                    work->field_53A = 1;
                }
            }
            break;
        case 1:
            work->field_2C0 = 0xFA0;
            work->field_50E = 0;
            work->field_538 = 0x10;
            work->field_4FA = 0;
            if (work->field_50A >= 0x36) {
                work->field_506  = 0x19;
                work->field_50A  = 0;
                work->field_4EA  = 0;
                work->field_514  = 1;
                work->field_42A |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case 24:
            work->field_50E = 0;
            work->field_538 = 0x10;
            work->field_4FA = 0;
            contact         = Actor01600_Fn04EB0(actor) & 0xFF;
            if (contact != 0) {
                flags           = work->field_42A & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                work->field_42A = flags;
                if (contact != 0xFF) {
                    distance        = work->field_4E0;
                    work->field_548 = 0U;
                    distance        = abs(distance);
                    work->field_50A = 0;
                    if ((distance >= 0x201) || ((contact & 0xF) == 2)) {
                        work->field_510 = 5;
                        if ((contact & 0xF0) == 0x80) {
                            work->field_506 = 7;
                        } else {
                            work->field_506 = 8;
                        }
                    } else {
                        work->field_510 = 0;
                        work->field_506 = 4;
                    }
                } else {
                    work->field_42A = flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->field_4EA = 0;
                }
            }
            if (work->field_50A >= 0x57) {
                work->field_50A = 0;
                work->field_508 = 0;
                work->field_506 = 0x19;
            }
            break;
        case 2:
            work->field_4FA = 0;
            work->field_538 = 0x10;
            work->field_50E = 0;
            if (work->field_50A >= 0x3D) {
                work->field_50A = 0;
                work->field_506 = 3;
            }
            break;
        case 3:
            work->field_538 = 0x10;
            work->field_4FA = 4;
            if (work->field_528 != 0) {
                if (work->field_50A >= 0xC) {
                    work->field_51E = (u16)work->field_51E + 0xF;
                } else {
                    work->field_51E = (u16)work->field_51E + 0xA;
                }
                height          = (u16)work->field_520 + (u16)work->field_51E;
                work->field_520 = height;
                if (height >= 0) {
                    work->field_528 = 0;
                    work->field_51E = 0;
                    work->field_520 = 0;
                }
            }
            if (work->field_50A == 6) {
                work->field_528 = 1;
                work->field_51E = -0x50;
                work->field_520 = (u16)work->field_520 - 0x50;
            }
            attackFrame = (u16)work->field_50A;
            if ((u32)(attackFrame - 5) < 0x10U) {
                if ((s16)attackFrame >= 0xC) {
                    work->field_50E = 0x5A;
                } else {
                    work->field_50E = 0x3C;
                }
                Actor01600_Fn03A60(actor);
            } else {
                work->field_50E = 0;
            }
            if (work->field_50A == 0x14) {
                id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100002;
                pan1 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan1, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_50A >= 0x15) {
                id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100001;
                pan2 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan2, (s8)worldCoordGetOriginAudioDepth(coord));
                work->field_506 = 0x17;
                work->field_528 = 0;
                work->field_51E = 0;
                work->field_520 = 0;
                work->field_50C = 0;
                work->field_50A = 0;
                work->field_548 = (u16)(work->field_548 + 1);
                work->field_2BA = (work->field_2BA | WORLD_COLLISION_BODY_PAIR_ENABLED) & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            }
            break;
        case 22:
            work->field_538 = 0x10;
            if (work->field_528 != 0) {
                if (work->field_50A >= (work->field_50C + 0xC)) {
                    work->field_51E = (u16)work->field_51E + 0xF;
                } else {
                    work->field_51E = (u16)work->field_51E + 0xA;
                }
                repeatHeight    = (u16)work->field_520 + (u16)work->field_51E;
                work->field_520 = repeatHeight;
                if (repeatHeight >= 0) {
                    work->field_528 = 0;
                    work->field_51E = 0;
                    work->field_520 = 0;
                }
            }
            if (work->field_50A == (work->field_50C + 6)) {
                work->field_528 = 1;
                work->field_51E = -0x50;
                work->field_520 = (u16)work->field_520 - 0x50;
            }
            frameOffset = work->field_50C;
            frame       = work->field_50A;
            if ((frame >= (frameOffset + 6)) && ((frameOffset + 0x15) >= frame)) {
                if ((frameOffset + 0xA) >= frame) {
                    work->field_50E = 0x5A;
                } else {
                    work->field_50E = 0x3C;
                }
                Actor01600_Fn03A60(actor);
            } else {
                work->field_50E = 0;
            }
            if (work->field_50A == (work->field_50C + 0x14)) {
                id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100002;
                pan3 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan3, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_50A >= (work->field_50C + 0x17)) {
                id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100001;
                pan4 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan4, (s8)worldCoordGetOriginAudioDepth(coord));
                work->field_548 = (u16)(work->field_548 + 1);
                work->field_4FA = 0;
                work->field_528 = 0;
                work->field_51E = 0;
                work->field_520 = 0;
                work->field_514 = 0;
                work->field_50A = 0;
                work->field_50C = -3;
                if ((s16)work->field_548 >= 3) {
                    work->field_506 = 0x18;
                } else {
                    work->field_506 = 0x17;
                }
                work->field_2BA = (work->field_2BA | WORLD_COLLISION_BODY_PAIR_ENABLED) & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            }
            break;
        case 23:
            work->field_538 = 0x10;
            work->field_4FA = 4;
            work->field_50E = 0;
            if (work->field_50A >= 0xA) {
                work->field_506  = 0x19;
                work->field_514  = 1;
                work->field_51A  = 0;
                work->field_4EA  = 0;
                work->field_42A |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->field_2BA  = (work->field_2BA | WORLD_COLLISION_BODY_PAIR_ENABLED) & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            }
            break;
        case 6:
        case 7:
            work->field_538 = 0x10;
            work->field_4FA = 0;
            if (work->field_506 == 7) {
                if (work->field_50A == 0xF) {
                    id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                    pan5 = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan5, (s8)worldCoordGetOriginAudioDepth(coord));
                }
                if (work->field_50A == 0x11) {
                    id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                    pan8 = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan8, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            } else {
                if (work->field_50A == 0xF) {
                    id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                    pan6 = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan6, (s8)worldCoordGetOriginAudioDepth(coord));
                }
                if (work->field_50A == 0x12) {
                    id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                    pan7 = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan7, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            work->field_50E = 0;
            if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                Actor01600_Fn03A60(actor);
            }
            if (work->field_50A >= 0x1C) {
                work->field_508  = 0;
                work->field_50A  = 0;
                work->field_2BA |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                if (work->field_510 == 7) {
                    work->field_514 = 0;
                    work->field_50A = 0;
                    work->field_506 = 4;
                }
            }
            break;
        case 9:
            work->field_4FA = 0;
            work->field_538 = 0x10;
            work->field_50E = 0;
            if (work->field_50A >= 0x28) {
                work->field_51A = 1;
                work->field_50A = 0;
                work->field_506 = 4;
            }
            break;
        default:
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void Actor01600_Fn020F8(Task* actor)
{
    PlayerStatus*   config = &gPlayerStatus;
    s32             neg_velocity;
    s32             reset_y;
    s32             reset_y2;
    SVECTOR         offset;
    s32             distance;
    Enemy*          ctx;
    Actor01600Work* work;
    GfxCoord*       effectCoord;
    GfxCoord*       coord;
    s16             targetKind;
    s16             temp_v0_3;
    s16             temp_v0_4;
    s16             temp_v0_7;
    s16             temp_v1;
    s16             temp_v1_5;
    s16             temp_v1_6;
    s16             temp_v1_7;
    s32             id;
    s32             flags;
    s32             angle;
    s32             var_v1;
    s32             pan10;
    s32             pan11;
    s32             pan12;
    s32             pan13;
    s32             pan14;
    s32             pan_case3;
    s32             pan15;
    s32             pan16;
    s32             pan17;
    s32             pan18;
    s32             pan19;
    s32             pan20;
    s32             pan21;
    s32             pan23;
    s32             pan24;
    s32             pan_msg_zero;
    s32             pan25;
    s32             pan26;
    s32             pan27;
    s32             pan28;
    s32             pan29;
    s32             pan2;
    s32             pan30;
    s32             pan31;
    s32             pan32;
    s32             pan33;
    s32             pan34;
    s32             pan35;
    s32             pan36;
    s32             pan3;
    s32             pan4;
    s32             pan5;
    s32             pan6;
    s32             pan7;
    s32             pan8;
    s32             pan9;
    u16             temp_v0_6;
    u16             temp_v1_3;
    u16             temp_v1_8;
    GfxCoord*       attachedCoord;
    GfxCoord*       attachedCoord2;
    GfxCoord*       attachedOffset;
    GfxCoord*       attachedOffset2;

    ctx         = actor->spawnArg2.pointer;
    work        = actor->work;
    coord       = actor->extra.tmd->coords;
    flags       = Actor01600_Fn052C4(actor) & 0xFF;
    effectCoord = &actor->extra.tmd->coords[2];
    memset(&offset, 0, 8);
    offset.vy = 0x32;
    temp_v1   = work->field_516;
    switch (temp_v1) {
        case 0:
            work->field_4FA = 4;
            work->field_506 = 5;
            work->field_50E = 0;
            work->field_538 = 0x10;
            if ((u32)((u16)work->field_50A - 9) < 9U) {
                work->field_50E = 0x3C;
                Actor01600_Fn03A60(actor);
                Actor01600_Fn06974(actor, 0x46);
                if (work->field_4F2 == 0) {
                    work->field_4F2 = 1;
                }
            }
            if (work->field_50A >= 0x12) {
                id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                pan2 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan2, (s8)worldCoordGetOriginAudioDepth(coord));
                if (!(Actor01600_Fn06C1C(actor) & 0xFF)) {
                    work->field_516 = 1;
                    work->field_50A = 0;
                    work->field_50E = 0;
                    work->field_506 = 0x17;
                    goto block_8;
                }
            } else {
            block_8:
                angle = Actor01600_Fn045A8(actor, &distance);
                if (!(Actor01600_Fn06C94(actor, angle, distance) & 0xFF)) {
                    if (Actor01600_Fn04974(actor, angle, distance, flags) & 0xFF) {
                        if (work->field_50A >= 0xA) {
                            id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                            pan3 = (s8)worldCoordGetOriginAudioPan(coord);
                            SndEvt_EnqueueType6(id, (s32)pan3, (s8)worldCoordGetOriginAudioDepth(coord));
                            id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                            pan4 = (s8)worldCoordGetOriginAudioPan(coord);
                            SndEvt_EnqueueType6(id, (s32)pan4, (s8)worldCoordGetOriginAudioDepth(coord));
                            work->field_50A = 0;
                            return;
                        }
                    } else if ((Actor01600_Fn06D74(actor, angle, distance) & 0xFF) && (work->field_50A >= 0xA)) {
                        id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                        pan5 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan5, (s8)worldCoordGetOriginAudioDepth(coord));
                        id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                        pan6 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan6, (s8)worldCoordGetOriginAudioDepth(coord));
                        work->field_50A = 0;
                        return;
                    }
                } else {
                    default:
                        return;
                }
            }
            break;
        case 1:
            work->field_4FA = 4;
            work->field_538 = 0x10;
            work->field_50E = 0x1E;
            work->field_506 = 6;
            if ((u32)((u16)work->field_50A - 8) < 8U) {
                work->field_50E = 0x3C;
                Actor01600_Fn03A60(actor);
                Actor01600_Fn06974(actor, -0x5A);
                if (work->field_4F2 == 0) {
                    work->field_4F2 = 1;
                }
            }
            if (work->field_50A == 0x12) {
                id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                pan7 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan7, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((u32)((u16)work->field_50A - 0x1A) < 9U) {
                work->field_50E = 0x3C;
                Actor01600_Fn03A60(actor);
                Actor01600_Fn06974(actor, 0x46);
                if (work->field_4F2 == 0) {
                    work->field_4F2 = 1;
                }
            }
            if (work->field_50A >= 0x22) {
                id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                pan8 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan8, (s8)worldCoordGetOriginAudioDepth(coord));
                work->field_50A = 0;
                if (Actor01600_Fn06C1C(actor) & 0xFF) {
                    return;
                }
            }
            angle = Actor01600_Fn045A8(actor, &distance);
            if (!(Actor01600_Fn06C94(actor, angle, distance) & 0xFF)) {
                if (Actor01600_Fn04974(actor, angle, distance, flags) & 0xFF) {
                    if (work->field_50A >= 8) {
                        id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                        pan9 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan9, (s8)worldCoordGetOriginAudioDepth(coord));
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                        pan10 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan10, (s8)worldCoordGetOriginAudioDepth(coord));
                        work->field_50A = 0;
                        return;
                    }
                } else if ((Actor01600_Fn06D74(actor, angle, distance) & 0xFF) && (work->field_50A >= 8)) {
                    id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                    pan11 = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan11, (s8)worldCoordGetOriginAudioDepth(coord));
                    id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                    pan12 = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan12, (s8)worldCoordGetOriginAudioDepth(coord));
                    work->field_50A = 0;
                    return;
                }
            }
            break;
        case 2:
            work->field_506 = 7;
            work->field_538 = 0x10;
            work->field_4FA = 0;
            work->field_50E = 0;
            if (work->field_50A == 0xF) {
                id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                pan13 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan13, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_50A == 0x11) {
                var_v1 = 0x40100004;
                id     = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | var_v1;
                pan14  = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan14, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                Actor01600_Fn03A60(actor);
            }
            if (work->field_50A >= 0x1C) {
                work->field_508 = 0;
                work->field_50A = 0;
                if (work->field_510 == 7) {
                    work->field_50A = 0;
                    work->field_516 = 0;
                    return;
                }
            }
            break;
        case 3:
            work->field_506 = 8;
            work->field_538 = 0x10;
            work->field_4FA = 0;
            work->field_50E = 0;
            if (work->field_50A == 0xF) {
                id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                pan15 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan15, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_50A == 0x12) {
                var_v1    = 0x40100003;
                id        = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | var_v1;
                pan_case3 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan_case3, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                Actor01600_Fn03A60(actor);
            }
            if (work->field_50A >= 0x1C) {
                work->field_508 = 0;
                work->field_50A = 0;
                if (work->field_510 == 7) {
                    work->field_50A = 0;
                    work->field_516 = 0;
                    return;
                }
            }
            break;
        case 4:
            work->field_538 = 0x10;
            work->field_506 = 9;
            work->field_50E = 0;
            work->field_4FA = 0;
            if (work->field_50A == 1) {
                id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000E;
                pan16 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan16, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_50A >= 0x25) {
                work->field_516 = 0;
                work->field_50A = 0;
                work->field_526 = 0;
                return;
            }
            break;
        case 5:
            work->field_538 = 0x10;
            work->field_50E = 0;
            work->field_4FA = 0;
            work->field_506 = 0x1A;
            if (work->field_528 != 0) {
                work->field_538 = 0x14;
                if (work->field_51E < 0) {
                    work->field_51E = (u16)work->field_51E + 0xF;
                } else {
                    work->field_51E = (u16)work->field_51E + 0x14;
                }
                temp_v0_3       = (u16)work->field_520 + (u16)work->field_51E;
                work->field_520 = temp_v0_3;
                if (temp_v0_3 >= 0) {
                    work->field_528 = 0;
                    work->field_51E = 0;
                    work->field_520 = 0;
                }
            }
            if (work->field_50A == 8) {
                work->field_528 = 1;
                work->field_530 = 0;
                work->field_51E = -0x96;
                work->field_520 = (u16)work->field_520 - 0x96;
            }
            if (work->field_50A == 0x1E) {
                id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100001;
                pan17 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan17, (s8)worldCoordGetOriginAudioDepth(coord));
                id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100002;
                pan18 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan18, (s8)worldCoordGetOriginAudioDepth(coord));
                work->field_528 = 0;
                work->field_51E = 0;
                work->field_520 = 0;
            }
            temp_v1_3 = (u16)work->field_50A;
            if ((u32)(temp_v1_3 - 8) < 0x17U) {
                if (work->field_52A == 0) {
                    if ((s16)temp_v1_3 < 0xC) {
                        work->collision.body.pos.vy = -0x258;
                        work->collision.body.radius = 0x258;
                        work->field_50E             = 0x12C;
                    } else if ((s16)temp_v1_3 < 0x18) {
                        work->collision.body.pos.vy = -0x190;
                        work->collision.body.radius = 0x190;
                        work->field_50E             = 0x32;
                    } else {
                        work->field_50E = 0x19;
                    }
                }
                Actor01600_Fn03A60(actor);
            }
            if ((work->field_50A == 9) && (work->field_53C == 0)) {
                work->field_3EA |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->field_50A == 0x17) {
                work->collision.body.flags = (u16)(work->collision.body.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
                work->field_3EA           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->field_50A >= 0x34) {
                if (work->field_53C != 0) {
                    work->field_516 = 4;
                } else {
                    work->field_516 = 0;
                }
                work->field_53C = 0;
                work->field_50A = 0;
                work->field_532 = 0;
                return;
            }
            break;
        case 6:
            work->field_538 = 0x10;
            work->field_4FA = 0;
            if (work->field_528 != 0) {
                work->field_538 = 0x14;
                if (work->field_51E < 0) {
                    work->field_51E = (u16)work->field_51E + 0xF;
                } else {
                    work->field_51E = (u16)work->field_51E + 0x14;
                }
                temp_v0_4       = (u16)work->field_520 + (u16)work->field_51E;
                work->field_520 = temp_v0_4;
                if (temp_v0_4 >= 0) {
                    work->field_528 = 0;
                    work->field_51E = 0;
                    work->field_520 = 0;
                }
            }
            temp_v1_5 = work->field_506;
            switch (temp_v1_5) {
                case 26:
                    temp_v1_6 = work->field_50A;
                    if (temp_v1_6 == 8) {
                        work->field_530 = 0;
                        work->field_528 = 1;
                        if (work->field_544 == 0) {
                            work->field_51E = -0x96;
                        } else {
                            work->field_51E = -0xC8;
                        }
                        work->field_520 = (u16)work->field_520 + (u16)work->field_51E;
                    }
                    work->field_50E = 0;
                    if (work->field_50A >= 8) {
                        if (work->field_544 == 0) {
                            if (work->field_50A < 0xC) {
                                work->collision.body.pos.vy = -0x258;
                                work->collision.body.radius = 0x258;
                                work->field_50E             = 0x12C;
                            } else {
                                if (work->field_50A < 0x18) {
                                    work->collision.body.pos.vy = -0x190;
                                    work->collision.body.radius = 0x190;
                                    work->field_50E             = 0x32;
                                } else {
                                    work->field_50E = 0x19;
                                }
                            }
                        } else {
                            if (work->field_50A < 0xC) {
                                work->field_50E = 0x1F4;
                            } else if (work->field_50A < 0x18) {
                                work->field_50E = 0x46;
                            } else {
                                work->field_50E = 0x32;
                            }
                        }
                    }
                    if ((work->field_50A == 9) && (work->field_53C == 0)) {
                        work->field_3EA |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    }
                    if (work->field_50A == 0x17) {
                        work->collision.body.flags = (u16)(work->collision.body.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
                        work->field_3EA           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    }
                    if (work->field_50A >= 0x19) {
                        work->field_506 = 0x1B;
                        work->field_50A = 0;
                        work->field_544 = 0;
                        work->field_53C = 0;
                        return;
                    }
                    break;
                case 27:
                    work->field_50E = 0x3C;
                    if (work->field_50A == 0xB) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100005;
                        pan19 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan19, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->field_50A >= 0xD) {
                        work->field_538 = 0x10;
                        work->field_50E = 0;
                    }
                    if (work->field_50A >= 0x28) {
                        work->field_506 = 0x15;
                        work->field_50A = 0;
                    }
                    break;
                case 21:
                    if (work->field_50A == 0x1F) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100001;
                        pan20 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan20, (s8)worldCoordGetOriginAudioDepth(coord));
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100002;
                        pan21 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan21, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->field_50A >= 0x2F) {
                        work->field_516 = 0;
                        work->field_50A = 0;
                        work->field_532 = 0;
                        return;
                    }
                    break;
            }
            break;
        case 7:
            work->field_526 = 1;
            work->field_50E = 0;
            work->field_4FA = 0;
            work->field_538 = 0x10;
            work->field_528 = 0;
            work->field_51E = 0;
            temp_v1_7       = (u16)work->field_506 - 9;
            work->field_520 = 0;
            switch (temp_v1_7) {
                case 19:
                    Actor01600_Fn03A60(actor);
                    Actor01600_Fn06974(actor, 0xA);
                    if (work->field_50A >= 0xD) {
                        work->field_50E = 0x5A;
                    }
                    if (work->field_50A >= 0x12) {
                        if (Actor01600_Fn047A0(actor) & 0xFF) {
                            targetKind = work->field_53E;
                            if (targetKind == 1) {
                                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp > 0) {
                                    if (taskMessageDispatch(work->field_4D4, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(ctx, 0), 0) == targetKind) {
                                        work->field_506 = 9;
                                        work->field_50E = 0;
                                        work->field_50A = 0;
                                        work->field_4E4 = (s32)coord->coord.t[1];
                                        return;
                                    }
                                    goto block_136;
                                }
                                work->field_506 = 9;
                                work->field_50E = 0;
                                work->field_50A = 0;
                                work->field_4E4 = (s32)coord->coord.t[1];
                                return;
                            }
                            if (config->hp <= 0) {
                                work->field_506 = 9;
                                work->field_50E = 0;
                                work->field_50A = 0;
                                work->field_4E4 = (s32)coord->coord.t[1];
                                return;
                            }
                        block_136:
                            work->field_534 = 1;
                            work->field_4FA = 0;
                            work->field_506 = 0x1D;
                            id              = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000D;
                            pan23           = (s8)worldCoordGetOriginAudioPan(coord);
                            SndEvt_EnqueueType6(id, (s32)pan23, (s8)worldCoordGetOriginAudioDepth(coord));
                            if (work->field_53E != 0) {
                                Actor01600_D127D8.animationId = 1;
                                TASK_MESSAGE_DISPATCH_POINTER(work->field_4D4, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &Actor01600_D127D8, 0);
                                id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4065000A;
                                pan24 = (s8)worldCoordGetOriginAudioPan(coord);
                                SndEvt_EnqueueType6(id, (s32)pan24, (s8)worldCoordGetOriginAudioDepth(coord));
                            } else {
                                Actor01600_D127D8.animationId = 2;
                                TASK_MESSAGE_DISPATCH_POINTER(work->field_4D4, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &Actor01600_D127D8, 0);
                                id           = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                                pan_msg_zero = (s8)worldCoordGetOriginAudioPan(coord);
                                SndEvt_EnqueueType6(id, (s32)pan_msg_zero, (s8)worldCoordGetOriginAudioDepth(coord));
                            }
                            work->field_554 = 2;
                            goto block_141;
                        }
                        work->field_4FA = 4;
                        work->field_526 = 0;
                        work->field_506 = 0x1B;
                    block_141:
                        work->field_50E = 0;
                        work->field_50A = 0;
                        work->field_54C = 0U;
                        return;
                    }
                    break;
                case 20:
                    attachedCoord     = work->field_4D4->extra.tmd->coords;
                    attachedOffset    = &attachedCoord[17];
                    coord->coord.t[0] = attachedCoord->coord.t[0] + attachedOffset->coord.t[0];
                    coord->coord.t[2] = attachedCoord->coord.t[2] + attachedOffset->coord.t[2];
                    temp_v0_6         = work->field_54C + 1;
                    work->field_54C   = temp_v0_6;
                    if ((s16)temp_v0_6 == 0x14) {
                        work->field_54C = 0U;
                        work->field_554 = (u16)work->field_554 + 1;
                        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, effectCoord, &offset, &work->hitEffect);
                        if (work->field_53E == 0) {
                            Gp_SpawnPadLerp(0xA, 0x80U, 0x80U);
                        }
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000D;
                        pan25 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan25, (s8)worldCoordGetOriginAudioDepth(coord));
                        if (work->field_53E == 0) {
                            taskMessageDispatch(work->field_4D4, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(ctx, 0), 0);
                        }
                        if (config->hp <= 0) {
                            if (work->field_53E == 0) {
                                work->field_506               = 9;
                                work->field_50E               = 0;
                                work->field_50A               = 0;
                                reset_y                       = coord->coord.t[1];
                                work->field_534               = 0;
                                work->field_4E4               = reset_y;
                                Actor01600_D127D8.animationId = 0;
                                Actor01600_D127D8.blend       = ANIMATION_BLEND_RESET;
                                Actor01600_D127D8.blendFrames = 0;
                                taskMessageDispatch(work->field_4D4, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                                return;
                            }
                            goto block_156;
                        }
                        goto block_150;
                    }
                block_150:
                    if (work->field_53E == 0) {
                        if (work->field_50A >= 0x31) {
                            if (work->field_554 >= 5) {
                                Actor01600_D127D8.animationId = 3;
                                Actor01600_D127D8.blend       = ANIMATION_BLEND_INTERPOLATE;
                                Actor01600_D127D8.blendFrames = 1;
                                TASK_MESSAGE_DISPATCH_POINTER(work->field_4D4, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &Actor01600_D127D8, 0);
                                work->field_508 = 0;
                                work->field_50A = 0;
                                work->field_506 = 0x1E;
                            }
                            if (work->field_50A >= 0x31) {
                                work->field_50A = 0;
                                return;
                            }
                        }
                    } else {
                    block_156:
                        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
                            work->field_506               = 9;
                            work->field_50E               = 0;
                            work->field_50A               = 0;
                            reset_y2                      = coord->coord.t[1];
                            work->field_534               = 0;
                            work->field_4E4               = reset_y2;
                            Actor01600_D127D8.animationId = 0;
                            Actor01600_D127D8.blend       = ANIMATION_BLEND_RESET;
                            Actor01600_D127D8.blendFrames = 0;
                            taskMessageDispatch(work->field_4D4, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                            return;
                        }
                        if (work->field_50A >= 0x31) {
                            work->field_50A = 0;
                            work->field_506 = 0x1E;
                            return;
                        }
                    }
                    break;
                case 21:
                    if (work->field_50A >= 0x10) {
                        work->field_50E = 0;
                        temp_v1_8       = (u16)work->field_50A;
                        if ((u32)(temp_v1_8 - 0x10) < 2U) {
                            work->field_50E = -0x12C;
                        } else if ((s16)temp_v1_8 < 0x1D) {
                            work->field_50E = -0x64;
                        } else if ((s16)temp_v1_8 < 0x24) {
                            work->field_50E = -0x28;
                        }
                    } else {
                        attachedCoord2    = work->field_4D4->extra.tmd->coords;
                        attachedOffset2   = &attachedCoord2[17];
                        coord->coord.t[0] = attachedCoord2->coord.t[0] + attachedOffset2->coord.t[0];
                        coord->coord.t[2] = attachedCoord2->coord.t[2] + attachedOffset2->coord.t[2];
                    }
                    if (work->field_50A >= 0x2D) {
                        work->field_534               = 0;
                        Actor01600_D127D8.animationId = 0;
                        Actor01600_D127D8.blend       = ANIMATION_BLEND_RESET;
                        Actor01600_D127D8.blendFrames = 0;
                        taskMessageDispatch(work->field_4D4, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    }
                    if (work->field_50A >= 0x38) {
                        work->field_506   = 9;
                        work->field_50E   = 0;
                        Actor01600_D12870 = 0;
                    }
                    if (work->field_50A == 0x13) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000B;
                        pan26 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan26, (s8)worldCoordGetOriginAudioDepth(coord));
                        if (work->field_53E == 0) {
                            Gp_SpawnPadLerp(0xA, 0xD0U, 0xD0U);
                        }
                    }
                    if (work->field_50A == 0x1F) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000C;
                        pan27 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan27, (s8)worldCoordGetOriginAudioDepth(coord));
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100001;
                        pan28 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan28, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->field_50A == 0x21) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100002;
                        pan29 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan29, (s8)worldCoordGetOriginAudioDepth(coord));
                        return;
                    }
                    break;
                case 0:
                    if (work->field_50A == 1) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000E;
                        pan30 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan30, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->field_50A >= 0x25) {
                        work->field_516 = 0;
                        work->field_50A = 0;
                        work->field_526 = 0;
                        return;
                    }
                    break;
                case 18:
                    work->field_50E = 0x28;
                    work->field_526 = 0;
                    work->field_4FA = 4;
                    if (work->field_50A >= 0xD) {
                        work->field_50E = 0;
                    }
                    if (work->field_50A == 0xD) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100005;
                        pan31 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan31, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->field_50A >= 0x30) {
                        work->field_506 = 0x15;
                        work->field_50E = 0;
                        work->field_50A = 0;
                    }
                    break;
                case 12:
                    work->field_526 = 0;
                    if (work->field_50A == 0x1F) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100001;
                        pan32 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan32, (s8)worldCoordGetOriginAudioDepth(coord));
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100002;
                        pan33 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan33, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->field_50A >= 0x2F) {
                        work->field_50E = 0;
                        work->field_516 = 0;
                        work->field_50A = 0;
                        work->field_526 = 0;
                        return;
                    }
                    break;
            }
            break;
        case 8:
            work->field_538 = 0x14;
            work->field_4FA = 0;
            switch (work->field_506) {
                case 11:
                case 14:
                    temp_v0_7 = work->field_50A;
                    if (temp_v0_7 <= 0) {
                        work->collision.body.pos.vy = -0x258;
                        work->collision.body.radius = 0x258;
                        work->field_50E             = 0x1F4;
                    } else if (temp_v0_7 < 0x13) {
                        work->collision.body.pos.vy = -0x190;
                        work->collision.body.radius = 0x190;
                        work->field_50E             = 0x4B;
                    } else {
                        work->field_50E = 0;
                    }
                    if (work->field_506 == 0xE) {
                        neg_velocity    = -work->field_50E;
                        work->field_50E = neg_velocity;
                    }
                    if (work->field_528 != 0) {
                        work->field_51E += 0xF;
                        work->field_520 += work->field_51E;
                        if (work->field_520 >= 0) {
                            id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100005;
                            pan36 = (s8)worldCoordGetOriginAudioPan(coord);
                            SndEvt_EnqueueType6(id, (s32)pan36, (s8)worldCoordGetOriginAudioDepth(coord));
                            work->field_528 = 0;
                            work->field_520 = 0;
                        }
                    }
                    if (work->field_50A >= 0x28) {
                        if (work->field_506 == 0xE) {
                            work->field_506 = 0x16;
                        } else {
                            work->field_506 = 0x15;
                        }
                        work->field_50A = 0;
                        work->field_50E = 0;
                        work->field_528 = 0;
                        work->field_51E = 0;
                        work->field_520 = 0;
                        work->field_532 = 0;
                        return;
                    }
                    break;
                case 21:
                case 22:
                    work->field_538 = 0x10;
                    work->field_50E = 0;
                    if (work->field_50A == 0x19) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100002;
                        pan34 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan34, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->field_50A == 0x1B) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100001;
                        pan35 = (s8)worldCoordGetOriginAudioPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan35, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->field_50A >= 0x2F) {
                        work->field_516 = 0;
                        work->field_50A = 0;
                    }
                    break;
                default:
                    break;
            }
            break;
    }
}

static void Actor01600_Fn03A60(Task* arg0)
{
    Task**                slot;
    Task**                slots;
    s16                   mode;
    s16                   diff;
    s32                   current;
    s16                   angle;
    s32                   absoluteDiff;
    s32                   randomTurn;
    s16                   turn;
    s32                   randomStep;
    s16                   wrap;
    s32                   amount;
    s32                   remaining;
    s32                   remaining2;
    u16                   wanted;
    u32                   randomState;
    u32                   randomState2;
    u32                   random;
    GfxCoord*             playerCoord;
    Actor01600Work*       work;
    GfxCoord*             coord;
    Actor01600RotScratch* allocated;
    Actor01600RotScratch* scratch;

    work                                       = arg0->work;
    coord                                      = arg0->extra.tmd->coords;
    slots                                      = gPlayerActorTasks;
    slot                                       = &slots[Actor01600_Fn052C4(arg0) & 0xFF];
    allocated                                  = SCRATCH_STACK_CURSOR(Actor01600RotScratch);
    allocated                                 -= 1;
    SCRATCH_STACK_CURSOR(Actor01600RotScratch) = allocated;
    mode                                       = work->field_510;
    playerCoord                                = (*slot)->extra.tmd->coords;
    scratch                                    = allocated;
    switch (mode) {
        case 0:
            scratch->position.vx = (s32)(playerCoord->coord.t[0] - coord->coord.t[0]);
            scratch->position.vy = 0;
            scratch->position.vz = (s32)(playerCoord->coord.t[2] - coord->coord.t[2]);
            wanted               = ratan2((s32)(s16)scratch->position.vx, (s32)(s16)scratch->position.vz) & 0xFFF;
            diff                 = wanted - (work->field_4FC & 0xFFF);
            absoluteDiff         = diff >= 0 ? diff : -diff;
            turn                 = diff;
            if (absoluteDiff < 0x21) {
                work->field_4FC = wanted;
            } else {
                if (absoluteDiff >= 0x801) {
                    wrap = diff - 0x1000;
                    if (diff <= 0) {
                        wrap = 0x1000 - diff;
                    }
                    turn = wrap;
                }
                current = (s16)work->field_4FC;
                if (turn > 0) {
                    work->field_4FC = (u16)(current + 0x20);
                } else {
                    work->field_4FC = (u16)(current - 0x20);
                }
            }
            break;
        case 1:
            randomState     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            random          = randomState >> 0x10;
            randomTurn      = (random & 0x3FF) + 0x400;
            gRandomLcgState = randomState;
            if (random & 0x400) {
                randomTurn = -randomTurn;
            }
            work->field_512 = randomTurn;
            work->field_510 = 2;
            break;
        case 2:
            randomState2    = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            randomStep      = (randomState2 >> 0x10) & 0x17;
            gRandomLcgState = randomState2;
            if (work->field_512 <= 0) {
                randomStep = -randomStep;
            }
            turn            = randomStep;
            work->field_4FC = (u16)(work->field_4FC + turn);
            remaining       = work->field_512 - turn;
            if ((remaining >= 0 ? remaining : -remaining) < 0x20) {
                work->field_510 = 0;
                work->field_512 = 0;
            } else {
                work->field_512 -= turn;
            }
            break;
        case 7:
            break;
        case 5:
            work->field_510 = 6;
            work->field_512 = (s16)work->field_4E0;
            break;
        case 6:
            amount = work->field_4F0;
            if (work->field_512 <= 0) {
                amount = -amount;
            }
            turn            = amount;
            angle           = work->field_4FC + amount;
            work->field_4FC = (u16)angle;
            if (angle >= 0x801) {
                work->field_4FC = angle - 0x1000;
            } else if (angle < -0x800) {
                work->field_4FC = angle + 0x1000;
            }
            remaining2 = work->field_512 - turn;
            if ((remaining2 >= 0 ? remaining2 : -remaining2) < work->field_4F0) {
                work->field_510 = 7;
                work->field_512 = 0;
            } else {
                work->field_512 -= turn;
            }
            break;
    }
    scratch->rotation.vx = 0;
    scratch->rotation.vy = (u16)work->field_4FC;
    scratch->rotation.vz = 0;
    RotMatrix(&scratch->rotation, &coord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(Actor01600RotScratch);
}

static void Actor01600_Fn03D48(Task* arg0)
{
    Actor01600Work* work;
    GfxCoord*       coord;
    s16             anim;
    s32             i;

    work = arg0->work;

    if (work->field_506 != 0xFF) {
        if (work->field_506 != work->field_508) {
            work->field_508 = (s16)(u16)work->field_506;
            work->field_50A = 0;
            for (i = 1; i < 9; i++) {
                animationSeekSlotWithBlend(&work->anim, i, work->field_506, 0, (s32)work->field_4FA);
            }
        } else {
            work->field_50A = (u16)work->field_50A + 1;
            for (i = 1; i < 9; i++) {
                work->slots[i].rate = work->field_538;
                animationTickSlot(&work->anim, i);
            }
        }
        anim = work->field_506;
        if (anim == 28 || anim == 30 || anim == 9 || anim == 21 || anim == 22 || anim == 5 || anim == 6 || anim == 27) {
            arg0->extra.tmd->coords[1].coord.t[0] = 0;
            arg0->extra.tmd->coords[1].coord.t[2] = 0;
            arg0->extra.tmd->coords[1].coord.t[0] = 0;
            arg0->extra.tmd->coords[1].coord.t[2] = 0;
            return;
        } else if (anim == 29) {
            arg0->extra.tmd->coords[1].coord.t[0] = 0;
            coord                                 = arg0->extra.tmd->coords;
            coord[1].coord.t[2]                   = (s32)(coord[1].coord.t[2] - 0x2BC);
        }
    }
}

static void Actor01600_Fn03EEC(Task* arg0)
{
    VECTOR3                  pos;
    Actor01600GroundScratch* scratch;
    Actor01600Work*          work;
    GfxCoord*                coord;
    s32                      height;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_526 == 0) {
        scratch = SCRATCH_STACK_RESERVE_BLOCK(Actor01600GroundScratch);
        if (work->field_528 != 0) {
            gte_SetRotMatrix(&coord->workm);
            scratch->offset.vx = 0;
            height             = work->field_520 - 0x80;
            scratch->offset.vy = -height;
            scratch->offset.vz = 0;
            gte_ldv0(&scratch->offset);
            gte_rtv0();
            gte_stlvnl(&scratch->pos);
            scratch->pos.vx += coord->workm.t[0];
            scratch->pos.vy += coord->workm.t[1];
            scratch->pos.vz += coord->workm.t[2];
        } else {
            scratch->pos.vx = coord->workm.t[0];
            scratch->pos.vy = coord->workm.t[1];
            scratch->pos.vz = coord->workm.t[2];
        }
        Gp_DrawEffGroundQuad(&scratch->pos, 0x1C0, 0);
        SCRATCH_STACK_RELEASE_BLOCK(Actor01600GroundScratch);
        return;
    }
    if (func_800EA1A8(MATRIX_TRANS(&coord[1].workm), &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x1C0, gRoomEffectState->groundShadowShade);
    }
}

static void Actor01600_Fn04054(Enemy* arg0, Task* arg1)
{
    Actor01600Work*   work;
    TmdObject*        obj;
    GfxCoord*         coords;
    GfxCoord*         attach;
    GfxCoord*         body;
    Actor01600Work*   w;
    SceneCombatState* state;
    AttachmentState*  attachment;
    SVECTOR           aim;
    s32               dist;
    s32               anim;
    s16               phase;
    s16               timer;
    s16               count;
    s32               mode;

    obj    = arg1->extra.tmd;
    work   = arg1->work;
    coords = obj->coords;
    mode   = gSceneCombatState.actorControl;
    if (mode == 1) {
        return;
    }
    if (mode > 1) {
        if (mode == 2) {
            obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        }
    }
    switch (work->field_502) {
        case 0:
            Actor01600_Fn06F10(arg1);
            if (work->field_540 != 2) {
                anim            = work->field_4FE;
                work->field_4FA = 0;
                work->field_538 = 0x14;
                if (anim != 2 && anim != 5) {
                    dist = Actor01600_Fn045A8(arg1, (s32*)&aim);
                    if (dist < 0) {
                        dist = -dist;
                    }
                    work->field_506 = (dist < 0x400) ? 0xE : 0xB;
                }
                work->field_504 = 0;
                work->field_518 = 0x1000;
                work->field_49C = coords[0].coord;
                Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
            }
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            arg0->recs                   = 0;
            worldTargetUnlinkNode(&arg0->node);
            Gp_UnlinkObj((WorldCollisionBody*)work->field_40C);
            Gp_UnlinkObj((WorldCollisionBody*)work->field_29C);
            Gp_UnlinkObj(&work->collision.body);
            Gp_UnlinkObj((WorldCollisionBody*)work->field_3CC);
            state = &gSceneCombatState;
            if (state->actor01600Wave >= 3) {
                if (Actor01600_Fn06F78() == 1) {
                    state->actor01600Wave = state->actor01600Wave + 1;
                }
            } else {
                Gp_ReleaseStateF0Add(arg1, 0x10);
            }
            work->field_502 = 1;
            break;
        case 1:
            Actor01600_Fn06880(arg1);
            phase           = work->field_504 + 1;
            work->field_504 = phase;
            if (work->field_540 != 2) {
                if (phase == 0xA) {
                    obj->flags = TMD_OBJECT_SEMI_TRANS;
                }
                if (work->field_504 == 0xF) {
                    Gp_SpawnEff(EFFECT_CORPSE_BURN, coords, 1, NULL);
                }
                if (work->field_504 < 0x10) {
                    body = arg1->extra.tmd->coords;
                    w    = arg1->work;

                    w->field_4BC = body->coord.t[0];
                    w->field_4C0 = body->coord.t[1];
                    w->field_4C4 = body->coord.t[2];

                    body->coord.t[0] += (body->coord.m[0][2] * w->field_50E) >> 12;
                    body->coord.t[2] += (body->coord.m[2][2] * w->field_50E) >> 12;
                    if (w->field_530 == 0) {
                        if (w->field_528 != 0) {
                            body->coord.t[1] += w->field_51E;
                        } else {
                            body->coord.t[1] += 0x80;
                        }
                    }
                }
            }
            if (work->field_504 >= 0x3C) {
                work->field_502 = 2;
            }
            break;
        case 2:
            if (gSceneCombatState.actor01600Wave >= 3) {
                if (Actor01600_D12874 == 1) {
                    attachment = &Gp_StateC08;
                    if (attachment->mode == ATTACHMENT_MODE_WHEEL) {
                        break;
                    }
                    if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                        break;
                    }
                    attachment->flags |= ATTACHMENT_FLAG_EVENT_LOCK;
                    Gp_PulseState1C();
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), 0x13F4, arg1, 0);
                    work->field_502         = 0xFF;
                    arg1->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    arg1->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    break;
                }
                Gp_ReleaseStateF0Add(arg1, 0x10);
            }
            work->field_54E         = 0x3C;
            arg1->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Actor01600_D12874--;
            arg1->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->field_502         = 3;
            break;
        case 3:
            timer           = work->field_54E - 1;
            work->field_54E = timer;
            if (timer == 0) {
                Actor01600_Fn06EA4(arg1);
            }
            return;
        default:
            return;
    }

    Actor01600_Fn03D48(arg1);
    coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coords);
    count           = work->field_550 + 1;
    work->field_550 = count;
    if (count < 5 && gGameSession->viewDirty != 1) {
        return;
    }
    work->field_550 = 0;

    attach = &arg1->extra.tmd->coords[1];
    update_actor_color(arg0, attach);
}

/// Measures the player against the actor: returns the yaw the actor would have
/// to turn through to face the player, wrapped into -0x800..0x800, and writes
/// the horizontal distance to `distance`.
static s32 Actor01600_Fn045A8(Task* arg0, s32* distance)
{
    GfxCoord*             coord;
    GfxCoord*             other;
    Actor01600AimScratch* scratch;
    SVECTOR*              vec;
    MATRIX*               matrix;
    s32                   angle;

    other   = gPlayerActorTasks[Actor01600_Fn052C4(arg0) & 0xFF]->extra.tmd->coords;
    coord   = arg0->extra.tmd->coords;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(Actor01600AimScratch);
    vec     = &scratch->dir;
    matrix  = &scratch->mat;
    vec->vx = other->workm.t[0] - coord->workm.t[0];
    vec->vy = other->workm.t[1] - coord->workm.t[1];
    vec->vz = other->workm.t[2] - coord->workm.t[2];
    TransposeMatrix(&coord->workm, matrix);
    gfxRotateSv(matrix, vec);
    angle = ratan2(vec->vx, vec->vz);
    if (angle > 0x800) {
        angle -= 0x1000;
    } else if (angle < -0x800) {
        angle += 0x1000;
    }
    scratch->delta.vx = other->coord.t[0] - coord->coord.t[0];
    scratch->delta.vy = other->coord.t[1] - coord->coord.t[1];
    scratch->delta.vz = other->coord.t[2] - coord->coord.t[2];
    *distance         = SquareRoot0(scratch->delta.vx * scratch->delta.vx + scratch->delta.vz * scratch->delta.vz);
    SCRATCH_STACK_RELEASE_BLOCK(Actor01600AimScratch);
    return angle;
}

static s32 Actor01600_Fn047A0(Task* arg0)
{
    SVECTOR3        delta;
    s32             distance;
    Actor01600Work* work;
    GfxCoord*       coord;
    Task*           task;
    s16             angle;
    s16             heading;
    s32             difference;
    GfxCoord*       other;

    work  = arg0->work;
    task  = work->field_4D4;
    other = task->extra.tmd->coords;
    coord = arg0->extra.tmd->coords;
    if (((GameActor*)gPlayerActorTasks[work->field_53E]->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
        if (Actor01600_D12870 != 1) {
            difference = Actor01600_Fn045A8(arg0, &distance);
            if (difference < 0) {
                difference = -difference;
            }
            if (difference < 0x401) {
                if (distance < 0x3E8) {
                    Actor01600_D12878.pressCount = 5;
                    if (work->field_53E != 0) {
                        Actor01600_D12878.animation.animationId = 1;
                    } else {
                        Actor01600_D12878.animation.animationId = 2;
                    }
                    if (TASK_MESSAGE_DISPATCH_POINTER(task, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &Actor01600_D12878, 0) == 0) {
                        other->composeStamp = GRAPHICS_COORD_DIRTY;
                        delta.vx            = coord->coord.t[0] - other->coord.t[0];
                        delta.vy            = 0;
                        delta.vz            = coord->coord.t[2] - other->coord.t[2];
                        angle               = ratan2(delta.vx, delta.vz);
                        heading             = angle;
                        if (angle >= 0x801) {
                            heading = angle - 0x1000;
                        } else if (angle < -0x800) {
                            heading = angle + 0x1000;
                        }
                        Actor01600_D12890.rot.vx = 0;
                        Actor01600_D12890.rot.vy = heading;
                        Actor01600_D12890.rot.vz = 0;
                        Actor01600_D12890.pos.vx = (s32)other->coord.t[0];
                        Actor01600_D12890.pos.vy = (s32)other->coord.t[1];
                        Actor01600_D12890.pos.vz = (s32)other->coord.t[2];
                        TASK_MESSAGE_DISPATCH_POINTER(task, 0x3E9, &Actor01600_D12890, 0);
                        Actor01600_D12870 = 1;
                        return 1;
                    }
                    return 0;
                }
                return 0;
            }
            return 0;
        }
        return 0;
    }
    return 0;
}

static s32 Actor01600_Fn04974(Task* actor, s32 angle, s32 distance, s32 flags)
{
    Actor01600Work* work;
    GfxCoord*       coord;
    GfxCoord*       other;
    s32             difference;
    s32             angleAbs;
    s32             done;
    s32             otherY;
    s32             tmp;

    work  = actor->work;
    coord = actor->extra.tmd->coords;
    other = (*gPlayerActorTasks)->extra.tmd->coords;
    if (((GameActor*)gPlayerActorTasks[flags]->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
        if (Actor01600_D12870 == 0) {
            otherY     = other->coord.t[1];
            tmp        = coord->coord.t[1];
            difference = otherY - tmp;
            if (difference < 0) {
                difference = -difference;
            }
            if (difference < 0x191) {
                if (distance < 0x3E9) {
                    tmp      = angle >= 0;
                    angleAbs = tmp ? angle : -angle;
                    if (angleAbs < 0x101) {
                        work->field_42A |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                        Actor01600_Fn04EB0(actor);
                        if (work->field_4EA >= 2) {
                            work->field_506  = 0x19;
                            work->field_51A  = 0;
                            work->field_4FE  = 0;
                            work->field_4EA  = 0;
                            work->field_514  = 1;
                            work->field_2BA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                            done             = 1;
                        } else {
                            done = 0;
                        }
                        if ((u8)done) {
                            return 1;
                        }
                        work->field_516 = 7;
                        work->field_506 = 0x1C;
                        work->field_52C = 0;
                        work->field_53E = flags;
                        work->field_4D4 = gPlayerActorTasks[flags];
                        return 1;
                    }
                    return 0;
                }
                return 0;
            }
            return 0;
        }
        return 0;
    }
    return 0;
}

static void Actor01600_Fn04AD8(Task* arg0)
{
    Enemy*          ctx;
    Actor01600Work* work;
    GfxCoord*       body;
    s16             state;
    u16             count;
    u16             count2;

    work  = arg0->work;
    ctx   = arg0->spawnArg2.pointer;
    state = work->field_4F2;
    body  = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_24C.coord.t[0] = body->coord.t[0];
            work->field_24C.coord.t[2] = body->coord.t[2];
            ctx->coord                 = body;
            break;
        case 1:
            work->field_24C.coord.t[0] = body->coord.t[0];
            work->field_24C.coord.t[2] = body->coord.t[2];
            ctx->coord                 = &work->field_24C;
            goto advance;
        case 2:
            count           = work->field_4F4 + 1;
            work->field_4F4 = count;
            if ((s16)count < 8) {
                break;
            }
        advance:
            work->field_4F4 = 0U;
            work->field_4F2 = (u16)work->field_4F2 + 1;
            break;
        case 3:
            work->field_4F6 = (s16)((body->coord.t[0] - work->field_24C.coord.t[0]) / 5);
            work->field_4F8 = (s16)((body->coord.t[2] - work->field_24C.coord.t[2]) / 5);
            work->field_4F2 = (u16)work->field_4F2 + 1;
            break;
        case 4:
            work->field_24C.coord.t[0] += work->field_4F6;
            work->field_24C.coord.t[2] += work->field_4F8;
            count2                      = work->field_4F4 + 1;
            work->field_4F4             = count2;
            if ((s16)count2 >= 5) {
                work->field_4F4 = 0U;
                work->field_4F2 = 0;
            }
            break;
    }
    work->field_24C.coord.t[1]   = body->coord.t[1];
    work->field_24C.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&work->field_24C);
}

/// Allocates the yaw scratch, updates `field_4EC` either by `angle` (clamped
/// into (-0x800, 0x801]) or, when `angle` is 0, by its own 0x71 / 0xA step -
/// returning 1 once the degree counter `field_4EE` passes a full turn - then
/// turns (0, 0, `distance`) by the resulting yaw into `field_42C` / `field_430`
/// and advances the swept-angle range `ranges[field_4E8]`.
static s32 Actor01600_Fn04C64(Task* arg0, s32 distance, s32 angle)
{
    Actor01600YawScratch* allocated;
    Actor01600YawScratch* scratch;
    Actor01600Work*       work;
    MATRIX*               m;
    SVECTOR*              out;
    s16                   temp_v0_3;
    s16                   temp_v0_4;
    s16                   temp_v0_5;
    s16                   temp_v1_2;
    s16                   var_v0;
    s16                   var_v0_2;
    s32                   scaled;
    s32                   temp_a0;
    s32                   var_s4;

    var_s4                                     = 0;
    allocated                                  = SCRATCH_STACK_CURSOR(Actor01600YawScratch) - 1;
    work                                       = arg0->work;
    SCRATCH_STACK_CURSOR(Actor01600YawScratch) = allocated;
    scratch                                    = allocated;
    scratch->vec.vx                            = 0;
    scratch->vec.vy                            = 0;
    scratch->vec.vz                            = (s16)distance;
    if (angle == 0) {
        temp_v1_2       = (u16)work->field_4EE + 0xA;
        scaled          = temp_v1_2 << 0x10;
        var_s4          = scaled > 0x01670000;
        work->field_4EC = (u16)work->field_4EC + 0x71;
        work->field_4EE = temp_v1_2;
    } else {
        work->field_4EC = (s16)angle;
        if ((s16)angle >= 0x801) {
            var_v0 = angle - 0x1000;
            goto block_5;
        }
        if ((s16)angle >= -0x800) {
            goto block_6;
        }
        var_v0 = angle + 0x1000;
    block_5:
        work->field_4EC = var_v0;
    block_6:;
    }
    m                    = &scratch->mat;
    MATRIX_PAIR(m, 0, 0) = 0x1000;
    MATRIX_PAIR(m, 0, 2) = 0;
    MATRIX_PAIR(m, 1, 1) = 0x1000;
    MATRIX_PAIR(m, 2, 0) = 0;
    m->m[2][2]           = 0x1000;
    RotMatrixY(work->field_4EC, m);
    out = &scratch->out;
    gte_SetRotMatrix(m);
    gte_ldv0(&scratch->vec);
    gte_rtv0();
    gte_stsv(out);
    work->field_42C = (s16)scratch->out.vx;
    work->field_430 = (s16)scratch->out.vz;
    if ((u16)(work->capsuleContact.key.value >> 16) != 0x10) {
        if (angle == 0) {
            temp_v0_3 = work->field_4E8;
            if (work->ranges[temp_v0_3].low == 0xFFFF) {
                work->ranges[temp_v0_3].low = (s32)work->field_4EC;
            } else {
                work->ranges[temp_v0_3].high = (s32)work->field_4EC;
                if (var_s4 == 1) {
                    var_v0_2 = (u16)work->field_4E8 + 1;
                    goto block_18;
                }
            }
        } else {
            var_s4 = 1;
        }
    } else if (angle == 0) {
        temp_v0_4 = work->field_4E8;
        temp_a0   = work->ranges[temp_v0_4].low;
        if (temp_a0 != 0xFFFF) {
            if (work->ranges[temp_v0_4].high == 0xFFFF) {
                work->ranges[temp_v0_4].high = temp_a0;
            }
            temp_v0_5       = (u16)work->field_4E8 + 1;
            work->field_4E8 = temp_v0_5;
            if (temp_v0_5 >= 7) {
                var_v0_2 = 7;
            block_18:
                work->field_4E8 = var_v0_2;
            }
        }
    }
    Gp_ClearRec18Occupied(&work->capsuleContact);
    SCRATCH_STACK_RELEASE_BLOCK(Actor01600YawScratch);
    return var_s4;
}

static u8 Actor01600_Fn04EB0(Task* arg0)
{
    SVECTOR         dir;
    s32             distance;
    Actor01600Work* work;
    s32             flags;
    s32             angle;
    s32             mag;
    s32             other;
    s32             span;
    s32             midpoint;
    s32             best;
    s32             direction;
    s32             i;
    s32             count;
    s32             callAngle;
    s32             circle;
    s32             turn;

    work  = arg0->work;
    flags = 0;
    switch (work->field_4EA) {
        case 0:
            callAngle = Actor01600_Fn045A8(arg0, &distance);
            TOUCH_REG(callAngle);
            Actor01600_Fn04C64(arg0, distance, callAngle);
            work->field_4E0 = callAngle;
            work->field_4EA++;
            break;
        case 1:
            work->field_4E0 = Actor01600_Fn045A8(arg0, &distance);
            if (Actor01600_Fn04C64(arg0, distance, work->field_4E0) & 0xFF) {
                gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, &dir);
                ratan2(dir.vx, dir.vz);
                mag   = __builtin_abs(work->field_4E0);
                other = 0x1000 - mag;
                if (other < mag) {
                    flags = 0;
                    angle = other;
                } else {
                    flags = 0x80;
                    angle = mag;
                }
                work->field_4F0 = angle / 16;
                if (work->field_4F0 < 0x20)
                    work->field_4F0 = 0x20;
                return flags | 1;
            }
            work->field_4EA++;
            break;
        case 2:
            work->field_4EC = -0x71;
            work->field_4E8 = 0;
            work->field_4EE = -10;
            for (i = 0; i < 8; i++) {
                work->ranges[i].low  = 0xFFFF;
                work->ranges[i].high = 0xFFFF;
            }
            Actor01600_Fn04C64(arg0, 2000, 0);
            work->field_4EA++;
            break;
        case 3:
            if (Actor01600_Fn04C64(arg0, 2000, 0) & 0xFF) {
                if (!work->field_4E8)
                    return 0xFF;
                work->field_4EA++;
            } else
                return 0;
            break;
        case 4:
            angle = Actor01600_Fn045A8(arg0, &distance);
            if (angle < 0)
                angle += 0x1000;
            mag             = work->field_4E8;
            work->field_4E0 = 0;
            if (mag == 1) {
                s32 high = work->ranges[0].high;
                s32 low  = work->ranges[0].low;

                span = high - low;
                if (span >= 0x400) {
                    span     = __builtin_abs(angle - high);
                    midpoint = __builtin_abs(angle - low);
                    if (span < midpoint)
                        work->field_4E0 = high;
                    else
                        work->field_4E0 = low;
                } else
                    work->field_4E0 = low + span / 2;
            } else {
                best = 0xFFFF;
                i    = 0;
                if (mag > 0) {
                    circle = 0x1000;
                    count  = mag;
                    do {
                        s32 high = work->ranges[i].high;
                        s32 low  = work->ranges[i].low;

                        span     = high - low;
                        midpoint = low + span / 2;
                        if (midpoint < 0x780 || midpoint > 0x880) {
                            turn      = __builtin_abs(angle - midpoint);
                            other     = circle - turn;
                            direction = 0;
                            if (other < turn)
                                turn = other;
                            else
                                direction = 0x80;
                            if (turn < best) {
                                flags = direction;
                                best  = turn;
                                if (span >= 0x400) {
                                    s32 high = work->ranges[i].high;
                                    s32 low  = work->ranges[i].low;

                                    span     = __builtin_abs(angle - high);
                                    midpoint = __builtin_abs(angle - low);
                                    if (span < midpoint)
                                        midpoint = high;
                                    else
                                        midpoint = low;
                                }
                                work->field_4E0 = midpoint;
                            }
                        }
                        i++;
                    } while (i < count);
                }
            }
            if (work->field_4E0 > 0x800)
                work->field_4E0 -= 0x1000;
            else if (work->field_4E0 < -0x800)
                work->field_4E0 += 0x1000;
            gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, &dir);
            ratan2(dir.vx, dir.vz);
            angle = __builtin_abs(work->field_4E0);
            if (angle > 0x800)
                angle = 0x1000 - angle;
            work->field_4F0 = angle / 16;
            if (work->field_4F0 < 0x20)
                work->field_4F0 = 0x20;
            return flags | 2;
    }
    return 0;
}

static s32 Actor01600_Fn052C4(Task* arg0)
{
    GfxCoord* coord;
    GfxCoord* other;
    SVECTOR   d;
    s32       dist;

    coord = arg0->extra.tmd->coords;
    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] != NULL) {
        other = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        d.vx  = (u16)other->coord.t[0] - (u16)coord->coord.t[0];
        d.vy  = (u16)other->coord.t[1] - (u16)coord->coord.t[1];
        d.vz  = (u16)other->coord.t[2] - (u16)coord->coord.t[2];
        dist  = SquareRoot0((d.vx * d.vx) + (d.vz * d.vz));
        if (gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] != NULL) {
            other = gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION]->extra.tmd->coords;
            d.vx  = (u16)other->coord.t[0] - (u16)coord->coord.t[0];
            d.vy  = (u16)other->coord.t[1] - (u16)coord->coord.t[1];
            d.vz  = (u16)other->coord.t[2] - (u16)coord->coord.t[2];
            return SquareRoot0((d.vx * d.vx) + (d.vz * d.vz)) < dist;
        }
    }
    return 0;
}

static void Actor01600_Fn05400(Task* arg0)
{
    TmdObject*      obj;
    TmdObject*      obj2;
    TmdObject*      obj3;
    TmdObject*      obj4;
    s32             kind;
    Enemy*          ctx;
    Actor01600Work* work;

    ctx  = arg0->spawnArg2.pointer;
    kind = ctx->place->mode;
    work = arg0->work;
    switch (kind) {
        case 0:
            Actor01600_Fn00480(arg0);
            Tmd_AllocBuffers(arg0->extra.tmd);
            obj             = arg0->extra.tmd;
            obj->flags     &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            obj2            = arg0->extra.tmd;
            obj2->flags    &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->field_52E = 1;
            return;
        case 3:
            work->field_53C             = 1;
            ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            work->field_52E             = 0;
            work->field_530             = 1;
            work->field_4D8             = 0;
            work->field_4DA             = (u16)ctx->place->yaw;
            work->field_4DC             = 0;
            Actor01600_D12874          += 1;
            return;
        case 1:
            work->field_536 = kind;

        default:
            obj3                        = arg0->extra.tmd;
            obj3->flags                |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj4                        = arg0->extra.tmd;
            obj4->flags                |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->field_53C             = 1;
            ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            work->field_532             = 1;
            work->field_52E             = 0;
            work->field_530             = 1;
            Actor01600_D12874          += 1;
            return;
    }
}
static s32 Actor01600_Fn05558(Task* arg0)
{
    SVECTOR         rot;
    Enemy*          ctx;
    AreaPlacement*  params;
    Actor01600Work* work;
    GfxCoord*       rootCoord;
    GfxCoord*       coord;
    TmdObject*      obj;
    TmdObject*      obj2;
    TmdObject*      obj3;
    TmdObject*      obj4;
    SVECTOR*        pos;
    SVECTOR*        tableA;
    SVECTOR*        tableB;
    SVECTOR*        pos2;
    s16             countdown;
    s16             countdown2;
    s16             countdown3;
    s32             event;
    s32             pan;
    s32             scriptArg;
    u16             kind;
    u8              mode;

    coord = arg0->extra.tmd->coords;
    ctx   = arg0->spawnArg2.pointer;
    work  = arg0->work;
    if ((u32)(gGameSession->location.loc.stage - 2) < 2U) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_KEY(0, 255, 255, 0)) == GAME_LOCATION_KEY(0, 34, 1, 0)) {
            if ((u16)work->field_4FE < 2U) {
                work->field_4FE = 4;
            }
        }
        if (((u32)(gGameSession->location.loc.stage - 2) < 2U) && (gGameSession->location.loc.area == 0x26) &&
            ((mode = gGameSession->location.loc.room, (mode == 1)) || (mode == 3)) && ((u16)work->field_4FE < 2U)) {
            work->field_4FE = 4;
        }
    }
    if (((u8)ctx->place->variant & 0x80) && ((u16)work->field_4FE < 2U)) {
        work->field_4FE = 4;
    }
    if (work->field_52E != 0) {
        return 0;
    }

    params = ctx->place;
    kind   = params->mode;
    switch (kind) {
        case 1:
        case 2:
            Actor01600_Fn03A60(arg0);
            if ((u32)((u8)gSceneCombatState.actor01600Wave - 1) >= 2U) {
                goto running;
            }
            scriptArg = (s8)(u8)gSceneCombatState.actor01600Wave;
            if (scriptArg == 1) {
                event = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000E;
                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(event, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                countdown       = (u16)work->field_536 - 1;
                work->field_536 = countdown;
                if ((countdown << 0x10) != 0) {
                    goto running;
                }
                if ((u8)ctx->place->variant == 0) {
                    work->field_516 = 6;
                    work->field_506 = 0x1A;
                } else {
                    work->field_53C = (s16)scriptArg;
                    work->field_516 = 5;
                }
                work->field_532 = 1;
            } else if (scriptArg == 2) {
                if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(3, 15, 0, 0)) {
                    tableA            = Actor01600_D09F1C;
                    pos               = &Actor01600_D09F1C[(u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT];
                    coord->coord.t[0] = pos->vx;
                    coord->coord.t[1] = pos->vy;
                    coord->coord.t[2] = pos->vz;
                }
                if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 4, 0, 0)) {
                    tableB            = Actor01600_D09F3C;
                    pos2              = &Actor01600_D09F3C[(u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT];
                    coord->coord.t[0] = pos2->vx;
                    coord->coord.t[1] = pos2->vy;
                    coord->coord.t[2] = pos2->vz;
                }
                Gp_ArmStateF0(1);
            }
            ctx->node.state.parts.flags = 0;
            work->field_52E             = 1;
            Actor01600_Fn00480(arg0);
            if (gSceneCombatState.actor01600Wave == 1) {
                work->collision.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            }
            Tmd_AllocBuffers(arg0->extra.tmd);
            obj             = arg0->extra.tmd;
            obj->flags     &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            obj2            = arg0->extra.tmd;
            obj2->flags    &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->field_4FE = 1;
            work->field_4FA = 0;
            work->field_508 = 0;
            work->field_50A = 0;
            work->field_52A = 0;
            return 0;
        case 3:
            if (gSceneCombatState.actor01600Wave == kind) {
                if ((u8)params->variant == 1) {
                    Actor01600_Fn06FDC(arg0, 1);
                running:
                    return 1;
                }
                countdown2      = (u16)work->field_536 - 1;
                work->field_536 = countdown2;
                if ((countdown2 << 0x10) == 0) {
                    ctx->node.state.parts.flags = 0;
                    Actor01600_Fn00480(arg0);
                    work->field_516 = 6;
                    work->field_52E = 1;
                    work->field_4FE = 1;
                    work->field_4FA = 0;
                    work->field_508 = 0;
                    work->field_50A = 0;
                    work->field_52A = 0;
                    work->field_506 = 0x1A;
                    return 0;
                }
                goto running;
            }
            if (arg0->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
                goto running;
            }
            Actor01600_Fn03D48(arg0);
            Actor01600_Fn05F80(arg0);
            if (work->field_54A != 0) {
                update_actor_color(ctx, arg0->extra.tmd->coords + 1);
                work->field_54A = 0;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            goto running;
        default:
            if (gSceneCombatState.actor01600Wave < (s32)ctx->place->mode) {
                goto running;
            }
            countdown3      = (u16)work->field_536 - 1;
            work->field_536 = countdown3;
            if ((countdown3 << 0x10) != 0) {
                goto running;
            }
            ctx->node.state.parts.flags = 0;
            work->field_52E             = 1;
            Actor01600_Fn00480(arg0);
            Tmd_AllocBuffers(arg0->extra.tmd);
            obj3            = arg0->extra.tmd;
            obj3->flags    &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            obj4            = arg0->extra.tmd;
            obj4->flags    &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->field_516 = 6;
            work->field_4FE = 1;
            work->field_4FA = 0;
            work->field_508 = 0;
            work->field_50A = 0;
            work->field_52A = 0;
            work->field_544 = 1;
            work->field_506 = 0x1A;
            rootCoord       = arg0->extra.tmd->coords;
            memset(&rot, 0, 8);
            rot.vy = -0x400;
            RotMatrix(&rot, &rootCoord->coord);
            work->field_532 = 1;
            return 0;
    }
}

s32 Actor01600_Fn05B08(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    SVECTOR         rot;
    Enemy*          ctx;
    Actor01600Work* work;
    GfxCoord*       coord;
    TmdObject*      obj;
    u32             variant;

    ctx     = arg0->spawnArg2.pointer;
    coord   = arg0->extra.tmd->coords;
    work    = arg0->work;
    variant = ctx->place->variant;

    switch (request->command) {
        case 1:
            work->field_54A = 1;
            if (variant == 2) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_50A = 0;
                work->field_508 = 0;
                work->field_506 = 3;
                work->field_546 = 9;
                work->field_524 = (gRandomLcgState >> 16) % 20;
            }
            if (variant == 4) {
                work->field_506 = 0x11;
                work->field_508 = 0;
            }
            if (variant == 3) {
                work->field_506   = 0x11;
                work->field_508   = 0;
                coord->coord.t[0] = 0x4021;
                coord->coord.t[1] = -0x190;
                coord->coord.t[2] = -0x6C0;
                rot.vx            = 0;
                rot.vy            = 0x600;
                rot.vz            = 0;
                RotMatrix(&rot, &coord->coord);
                goto setFlags;
            }
            break;
        case 2:
            work->field_54A = 1;
            if (variant == 1) {
                work->field_506 = 0x1A;
                work->field_508 = 0;
            }
            if (variant == 2) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_50A = 0;
                work->field_508 = 0;
                work->field_506 = 3;
                work->field_546 = 9;
                work->field_524 = (gRandomLcgState >> 16) % 20;
            }
            if (variant == 3) {
                obj = arg0->extra.tmd;
                do {
                    obj->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
                    obj         = arg0->extra.tmd;
                } while (0);
                obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case 3:
            work->field_54A = 1;
            if ((u32)(variant - 1) < 4) {
                obj         = arg0->extra.tmd;
                obj->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
                obj         = arg0->extra.tmd;
                obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            if (variant == 1) {
                work->field_506 = 9;
                work->field_508 = 0;
            }
            if (variant == 2) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_50A = 0;
                work->field_508 = 0;
                work->field_506 = 3;
                work->field_546 = 9;
                work->field_524 = (gRandomLcgState >> 16) % 20;
            }
            break;
        case 4:
            work->field_54A = 1;
            if (variant == 1) {
                work->field_506 = 0xFF;
                work->field_508 = 0;
                work->field_50A = 0;
            }
            if (variant == 2) {
                work->field_506 = 3;
                work->field_524 = 0xA;
                work->field_50A = 0;
                work->field_508 = 0;
                work->field_546 = 7;
            }
            if (variant == 4) {
                work->field_50A = 0;
                work->field_524 = 7;
                work->field_546 = 7;
            }
            break;
        case 5:
            work->field_54A = 1;
            break;
        case 6:
            work->field_54A = 1;
            if (variant == 3) {
                Actor01600_Fn06FDC(arg0, 0);
            }
            if (variant == 2 || variant == 4) {
                obj         = arg0->extra.tmd;
                obj->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
                obj         = arg0->extra.tmd;
                obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            gSceneCombatState.actor01600Wave = 3;
            break;
        case 7:
            work->field_54A = 1;
            if ((u32)(variant - 1) < 4) {
            setFlags:
                obj         = arg0->extra.tmd;
                obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                obj         = arg0->extra.tmd;
                obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            }
            break;
        case 8:
            Gp_ReleaseStateF0Add(arg0, 0x10);
            enemyDestroy(ctx, arg0);
            Actor01600_D12874 -= 1;
            break;
        default:
            return -1;
    }
    return 0;
}

static void Actor01600_Fn05F80(Task* arg0)
{
    SVECTOR         rot;
    GfxCoord*       coord;
    Actor01600Work* work;
    GfxCoord*       part1;
    u32             variant;
    s32             anim;
    u16             timer;
    s32             value;

    coord   = arg0->extra.tmd->coords;
    work    = arg0->work;
    variant = ((Enemy*)arg0->spawnArg2.pointer)->place->variant;
    part1   = &arg0->extra.tmd->coords[1];

    if (variant == 1) {
        switch (work->field_506) {
            case 0x1A:
                work->field_54A = variant;
                if ((u32)((u16)work->field_50A - 6) < 0xEU) {
                    coord->coord.t[1] -= 0xC8;
                }
                if ((u32)((u16)work->field_50A - 0x14) < 0xFU) {
                    value             = coord->coord.t[1] + 0x96;
                    coord->coord.t[1] = value;
                    if (value >= -0x497) {
                        coord->coord.t[1] = -0x498;
                    }
                }
                if ((u32)((u16)work->field_50A - 0xB) < 0x14U) {
                    value             = coord->coord.t[0] + ((coord->coord.m[0][2] * 0x4B) >> 0xB);
                    coord->coord.t[0] = value;
                    if (value < 0x3B23) {
                        coord->coord.t[0] = 0x3B23;
                    }
                    value             = coord->coord.t[2] + ((coord->coord.m[2][2] * 0x4B) >> 0xB);
                    coord->coord.t[2] = value;
                    if (value >= -0xD11) {
                        coord->coord.t[2] = -0xD12;
                    }
                }
                if ((s16)work->field_50A >= 0x31) {
                    work->field_506 = 8;
                    work->field_508 = 0;
                    work->field_50A = 0;
                }
                break;
            case 8:
                if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                    work->field_4D8  = 0;
                    work->field_4DC  = 0;
                    work->field_4DA += 0x32;
                    RotMatrix((SVECTOR*)&work->field_4D8, &coord->coord);
                }
                if ((s16)work->field_50A >= 0x1C) {
                    work->field_506 = 9;
                    work->field_508 = 0;
                    work->field_50A = 0;
                }
                break;
            case 0xFF:
                anim = work->field_50A;
                if (anim == 2) {
                    work->hitEffect.coord      = part1;
                    work->hitEffect.spawnArgLo = 0x100;
                    work->hitEffect.spawnArgHi = anim;
                    func_800FDB18(1, part1, 0, &work->hitEffect);
                    Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[1], 0, NULL);
                }
                if ((u32)((u16)work->field_50A - 0xF) < 8U) {
                    coord->coord.t[1] += 0x80;
                }
                if ((s16)work->field_50A >= 3) {
                    coord->coord.t[0] += 0x10E;
                    coord->coord.t[2] -= 0xC8;
                    coord              = &arg0->extra.tmd->coords[6];
                    rot.vx             = -0x400;
                    rot.vy             = 0;
                    rot.vz             = 0;
                    RotMatrix(&rot, &coord->coord);
                    coord  = &arg0->extra.tmd->coords[8];
                    rot.vx = -0x400;
                    rot.vy = 0;
                    rot.vz = 0;
                    RotMatrix(&rot, &coord->coord);
                    if ((s16)work->field_50A >= 3) {
                        work->field_4D8  = 0x384;
                        work->field_4DA += 0x96;
                        timer            = work->field_4DC + 0x64;
                        work->field_4DC  = timer;
                        if ((s16)timer >= 0x384) {
                            work->field_4DC = 0x384;
                        }
                        coord = &arg0->extra.tmd->coords[1];
                        RotMatrix((SVECTOR*)&work->field_4D8, &coord->coord);
                    }
                }
                if ((s16)work->field_50A >= 0x12) {
                    Actor01600_Fn06FDC(arg0, 0);
                }
                work->field_50A += 1;
                break;
        }
    }
    if (variant == 2) {
        timer           = work->field_524 - 1;
        work->field_524 = timer;
        if ((s16)timer < 0) {
            anim            = work->field_546;
            work->field_524 = 1;
            switch (anim) {
                case 7:
                    work->field_506 = anim;
                    if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                        work->field_4D8  = 0;
                        work->field_4DC  = 0;
                        work->field_4DA += 0x32;
                        RotMatrix((SVECTOR*)&work->field_4D8, &coord->coord);
                    }
                    if ((s16)work->field_50A >= 0x1C) {
                        work->field_546 = 9;
                        work->field_506 = 9;
                        work->field_508 = 0;
                        work->field_50A = 0;
                    }
                    break;
                case 9:
                    work->field_506 = anim;
                    if ((s16)work->field_50A >= 0x32) {
                        work->field_50A = 0;
                        work->field_508 = 0;
                        work->field_506 = 3;
                        work->field_546 = 0;
                    }
                    break;
            }
        }
    }
    if (variant == 4) {
        timer           = work->field_524 - 1;
        work->field_524 = timer;
        if ((s16)timer < 0) {
            anim            = work->field_546;
            work->field_524 = 1;
            if (anim == 7) {
                work->field_506 = anim;
                if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                    work->field_4D8  = 0;
                    work->field_4DC  = 0;
                    work->field_4DA -= 0xFA;
                    RotMatrix((SVECTOR*)&work->field_4D8, &coord->coord);
                }
                if ((s16)work->field_50A >= 0x1C) {
                    work->field_546 = 9;
                    work->field_506 = 9;
                    work->field_508 = 0;
                    work->field_50A = 0;
                }
            }
        }
    }
}

static void Actor01600_Fn0646C(Task* arg0)
{
    EffectWork* effect;
    TmdObject*  obj;
    TmdObject*  obj2;
    s32         randomState;
    s32         choice;

    if (((Actor01600Work*)arg0->work)->field_540 != 0) {
        D_800626EC[5].data.model = &_gActor01600ScavengerBurstHead;
        effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 1, 0, NULL);
        if (effect != NULL) {
            Actor01600_Fn070AC(effect->task, arg0);
        }
        D_800626EC[5].data.model = &_gActor01600ScavengerBurstEar;
        effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 2, 0, NULL);
        if (effect != NULL) {
            Actor01600_Fn070AC(effect->task, arg0);
        }
        D_800626EC[5].data.model = &_gActor01600ScavengerBurstEar;
        effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 3, 0, NULL);
        if (effect != NULL) {
            Actor01600_Fn070AC(effect->task, arg0);
        }
        Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 1, 0x300, &Actor01600_D12868);
        return;
    }
    randomState     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState = randomState;
    choice          = ((u32)randomState >> 0x10) & 3;
    switch (choice) {
        case 0:
        case 1:
            D_800626EC[5].data.model = &_gActor01600ScavengerBurstHead;
            effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 1, 0, NULL);
            if (effect != NULL) {
                Actor01600_Fn070AC(effect->task, arg0);
            }
            break;
        case 2:
            D_800626EC[5].data.model = &_gActor01600ScavengerBurstEar;
            effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 2, 0, NULL);
            if (effect != NULL) {
                Actor01600_Fn070AC(effect->task, arg0);
            }
            break;
        case 3:
            D_800626EC[5].data.model = &_gActor01600ScavengerBurstLeg;
            effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 6, 0, NULL);
            if (effect != NULL) {
                Actor01600_Fn070AC(effect->task, arg0);
            }
            break;
    }
    Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 1, 0x50, &Actor01600_D12868);
    obj          = arg0->extra.tmd;
    obj->flags  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    obj2         = arg0->extra.tmd;
    obj2->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
}

void Actor01600_Fn066E8(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor01600_D00004;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

/// Snapshots the attachment coordinate's translation into the work block's
/// `field_4BC`/`field_4C0`/`field_4C4`, then steps that coordinate along the
/// model's own facing: `field_50E` units of the coordinate's column 2 added to
/// its X/Z translation. Vertical follow-up only runs while `field_530` is
/// clear: with `field_528` set the frame counter `field_51E` is added to
/// `t[1]`, otherwise the coordinate is snapped down by 0x80.
static void Actor01600_Fn06744(Task* arg0)
{
    Actor01600Work* work;
    GfxCoord*       coord;

    coord              = arg0->extra.tmd->coords;
    work               = arg0->work;
    work->field_4BC    = coord->coord.t[0];
    work->field_4C0    = coord->coord.t[1];
    work->field_4C4    = coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_50E) >> 0xC;
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_50E) >> 0xC;
    if (work->field_530 == 0) {
        if (work->field_528 != 0) {
            coord->coord.t[1] += work->field_51E;
            return;
        }
        coord->coord.t[1] += 0x80;
    }
}

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off the scratch stack, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters.
static void Actor01600_Fn06810(Enemy* arg0, Task* arg1)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;

    coord                          = &arg1->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    Gp_UpdateActorColor(arg0, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Squashes the actor's attachment coordinate: the work block's `field_49C`
/// rotation is copied into the coordinate, an identity is splatted into a
/// 0x30-byte scratch block and scaled per axis by 1.0 / the decaying
/// `field_518` / 1.0, and the product replaces the coordinate's rotation.
/// `composeStamp` is cleared so its own work matrix is rebuilt from `coord` next frame.
static void Actor01600_Fn06880(Task* arg0)
{
    GfxCoord*          coord;
    u8*                head;
    ActorScaleScratch* scratch;
    Actor01600Work*    work;

    head                       = SCRATCH_STACK_CURSOR(u8);
    work                       = arg0->work;
    scratch                    = (ActorScaleScratch*)(head - 0x30);
    SCRATCH_STACK_CURSOR(void) = scratch;
    coord                      = arg0->extra.tmd->coords;
    if (work->field_518 >= 0x201) {
        work->field_518 = (u16)work->field_518 - 0x50;
    }
    scratch->scale.vx                 = 0x1000;
    scratch->scale.vy                 = (s32)work->field_518;
    scratch->scale.vz                 = 0x1000;
    coord->coord                      = work->field_49C;
    scratch->mat.rotationWords.m00M01 = ONE;
    scratch->mat.rotationWords.m02M10 = 0;
    scratch->mat.rotationWords.m11M12 = ONE;
    scratch->mat.rotationWords.m20M21 = 0;
    scratch->mat.rotationWords.m22    = ONE;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(0x30);
}

/// Steps the attachment coordinate `distance` units along the model's facing:
/// `gfxReadMatrixZAxis` reads that coordinate's column into `dir`, `ratan2` turns it
/// into a yaw, `RotMatrixY` builds the rotation for the yaw and
/// `ApplyMatrixLV` rotates the step vector `(distance, 0, 0)` by it before the
/// result is added to `coord.t`.
static void Actor01600_Fn06974(Task* actor, s32 distance)
{
    GfxMatrix*             mat;
    Actor01600StepScratch* work;
    GfxCoord*              coord;
    Actor01600StepScratch* head;
    void**                 scratch;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, Actor01600StepScratch);
    coord                          = actor->extra.tmd->coords;
    work                           = head - 1;
    work->move.vx                  = (s16)distance;
    work->move.vy                  = 0;
    work->move.vz                  = 0;
    SCRATCH_HEAD_AT(scratch, void) = work;
    gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, &(head - 1)->dir);
    mat                       = &(head - 1)->mat;
    work->yaw                 = ratan2(work->dir.vx, work->dir.vz);
    mat->rotationWords.m00M01 = ONE;
    mat->rotationWords.m02M10 = 0;
    mat->rotationWords.m11M12 = ONE;
    mat->rotationWords.m20M21 = 0;
    mat->rotationWords.m22    = ONE;
    RotMatrixY(work->yaw, &mat->mat);
    ApplyMatrixLV(&mat->mat, &work->move, &work->move);
    coord->coord.t[0] += work->move.vx;
    coord->coord.t[1] += work->move.vy;
    coord->coord.t[2] += work->move.vz;
    SCRATCH_POP_AT(scratch, Actor01600StepScratch);
}

static void Actor01600_Fn06A84(Task* arg0)
{
    Actor01600Work* work;
    GfxCoord*       coord;
    MATRIX*         scratch;
    u8*             head;
    s16             value;

    head                         = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(MATRIX) = (MATRIX*)(head - 0x20);
    scratch                      = (MATRIX*)(head - 0x20);
    work                         = arg0->work;
    coord                        = arg0->extra.tmd->coords;
    RotMatrix((SVECTOR*)&work->field_4CC, scratch);
    gte_SetRotMatrix(&coord[1].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[1].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[1].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[1].coord.m[0][2]);
    value = work->field_4CC;
    if (value != 0) {
        if (value < 0x21) {
            work->field_4CC = 0;
            work->field_522 = 0;
        } else {
            work->field_4CC = (u16)work->field_4CC - 0x20;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

static s32 Actor01600_Fn06C1C(Task* arg0)
{
    Actor01600Work* work;

    work            = arg0->work;
    work->field_42A = (u16)(work->field_42A | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    Actor01600_Fn04EB0(arg0);
    if (work->field_4EA >= 2) {
        work->field_506 = 0x19;
        work->field_51A = 0;
        work->field_4FE = 0;
        work->field_4EA = 0;
        work->field_514 = 1;
        work->field_2BA = (u16)(work->field_2BA & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
        return 1;
    }
    return 0;
}

/// Column 2 of the attachment matrix goes to `dir`; the trailing `SVECTOR`
/// is never read but owns the second half of the stack local block.
static s32 Actor01600_Fn06C94(Task* arg0, s32 arg1, s32 unusedDistance)
{
    SVECTOR         dir;
    SVECTOR         unused;
    Actor01600Work* work;
    s32             ang;
    s32             half;
    s32             res;

    work = arg0->work;
    if (ABS(arg1) < 0x301) {
        return 0;
    }
    work->field_4E0 = arg1;
    gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, &dir);
    ratan2(dir.vx, dir.vz);
    ang  = ABS(work->field_4E0);
    half = 0x1000 - ang;
    if (half < ang) {
        work->field_516 = 3;
        res             = half;
    } else {
        work->field_516 = 2;
        res             = ang;
    }
    work->field_4F0 = res / 16;
    if (work->field_4F0 < 0x20) {
        work->field_4F0 = 0x20;
    }
    work->field_50A = 0;
    work->field_510 = 5;
    return 1;
}

static s32 Actor01600_Fn06D74(Task* arg0, s32 arg1, s32 arg2)
{
    Actor01600Work* work;
    s32             scaledState;
    s32             handled;
    s32             angle;
    u32             state;

    work = arg0->work;
    if (arg2 < 0x7D1) {
        angle = (arg1 >= 0 ? arg1 : -arg1);
        if (angle < 0x201) {
            work->field_42A |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            Actor01600_Fn04EB0(arg0);
            handled = 0;
            if (work->field_4EA >= 2) {
                work->field_506  = 0x19;
                work->field_51A  = 0;
                work->field_4FE  = 0;
                work->field_4EA  = 0;
                work->field_514  = 1;
                work->field_2BA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                handled          = 1;
            }
            if ((u8)handled) {
                return 1;
            } else {
                scaledState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER;
                state           = scaledState + RANDOM_LCG_INCREMENT;
                work->field_4FA = 0;
                work->field_508 = 0;
                work->field_52A = 0;
                gRandomLcgState = state;
                if ((u32)(((state >> 16) % 100) & 0xFFFF) < 0x14U) {
                    work->field_516 = 6;
                    work->field_506 = 0x1A;
                } else {
                    work->field_516 = 5;
                }
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

/// Tears the actor down: flags its context node dead (`field_4` = 1), clears
/// `field_54`, unlinks the node from its list and releases the four display
/// objects held in the work block, then hands the task to `enemyTaskExit`.
static void Actor01600_Fn06EA4(Task* arg0)
{
    Enemy*          ctx;
    Actor01600Work* work;

    ctx  = arg0->spawnArg2.pointer;
    work = arg0->work;

    ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    ctx->recs                   = 0;
    worldTargetUnlinkNode(&ctx->node);
    Gp_UnlinkObj((WorldCollisionBody*)work->field_40C);
    Gp_UnlinkObj((WorldCollisionBody*)work->field_29C);
    Gp_UnlinkObj(&work->collision.body);
    Gp_UnlinkObj((WorldCollisionBody*)work->field_3CC);
    enemyTaskExit(arg0);
}

static void Actor01600_Fn06F10(Task* arg0)
{
    Actor01600Work* work;

    work = arg0->work;
    if (work->field_534 != 0) {
        taskMessageDispatch(work->field_4D4, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        Actor01600_D127D8.animationId = 0;
        work->field_534               = 0;
        Actor01600_D12870             = 0;
        work->field_50E               = 0;
        work->field_516               = 0;
        work->field_50A               = 0;
        work->field_526               = 0;
    }
}

/// Walks the sibling ring of task slot 4's children and reports whether any of
/// them has already been flagged `0x80` in its `field_2C` object. Returns 0xFF
/// when the slot has no children at all, 1 on the first flagged sibling and 0
/// when the whole ring is clean.
static u8 Actor01600_Fn06F78(void)
{
    Task* head;
    Task* iter;

    head = gameGetTaskSlot(GAME_TASK_SLOT_SCENE)->firstChild;
    if (head == NULL) {
        return 0xFF;
    }
    iter = head;
    do {
        if (iter->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
            return 1;
        }
        iter = iter->nextSibling;
    } while (iter != head);
    return 0;
}

static void Actor01600_Fn06FDC(Task* arg0, s32 arg1)
{
    Actor01600Work* work;
    Enemy*          ctx;
    TmdObject*      obj;
    TmdObject*      obj2;

    obj         = arg0->extra.tmd;
    ctx         = arg0->spawnArg2.pointer;
    work        = arg0->work;
    obj->flags  = (u16)(obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
    obj2        = arg0->extra.tmd;
    obj2->flags = (u16)(obj2->flags | TMD_OBJECT_SKIP_AUTO_BUFFER);
    ctx->recs   = 0;
    worldTargetUnlinkNode(&ctx->node);
    if (!(arg1 & 0xFF)) {
        Gp_UnlinkObj((WorldCollisionBody*)work->field_40C);
        Gp_UnlinkObj((WorldCollisionBody*)work->field_29C);
        Gp_UnlinkObj(&work->collision.body);
        Gp_UnlinkObj((WorldCollisionBody*)work->field_3CC);
    }
    Gp_SetLightMode(ctx, ENEMY_COLOR_WEIGHTED);
    Gp_ReleaseStateF0Add(arg0, 0x10);
    enemyDestroy(ctx, arg0);
    Actor01600_D12874 -= 1;
}

/// Copies the source actor's texture page and CLUT row (`field_24` /
/// `field_25`) onto this actor's model object, then re-runs the model stream
/// twice so the new page/CLUT is baked into both of the object's primitive
/// buffers. Objects without an aux buffer (`field_18` NULL) have nothing to
/// rebuild and are left alone.
static void Actor01600_Fn070AC(Task* arg0, Task* arg1)
{
    TmdObject* src;
    TmdObject* dst;

    src                    = arg1->extra.tmd;
    dst                    = arg0->extra.tmd;
    dst->texturePageOffset = src->texturePageOffset;
    dst->clutRowOffset     = src->clutRowOffset;
    if (dst->buffer != NULL) {
        tmdProcessStream(dst);
        tmdProcessStream(dst);
    }
}

s32 Actor01600_Fn07100(Task* arg0, s32 msgId, s32 arg2, s32 arg3)
{
    ((Actor01600Work*)arg0->work)->field_554 = 6;
    return 0;
}
