#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
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
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
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

#include "rooms/dryfield_water_tower.h"
#include "../../shared/limb_shadows.h"
#include "../../shared/actor_messages.h"
#include "../../shared/actor_contacts.h"
#define DESERT_CHASER_BUILD DESERT_CHASER_WATER_TOWER
#include "../../shared/desert_chaser.h"

typedef struct Actor421600DamageScratch {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte pad_C[4];
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ s16  field_14;
    /* 0x16 */ byte pad_16[2];
    /* 0x18 */ s16  field_18;
    /* 0x1A */ s16  field_1A;
    /* 0x1C */ s16  field_1C;
    /* 0x1E */ byte pad_1E[2];
    /* 0x20 */ s32  field_20;
    /* 0x24 */ s32  field_24;
    /* 0x28 */ s32  field_28;
    /* 0x2C */ s16  field_2C;
    /* 0x2E */ s16  field_2E;
} Actor421600DamageScratch;
STATIC_ASSERT_SIZEOF(Actor421600DamageScratch, 0x30);

extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

/// The attack tick takes 0x14 bytes from the scratch stack for its direction,
/// wrapped angles, arena zone and player contact reply.
typedef struct Actor421600AttackScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s16     playerYaw;
    /* 0x0A */ s16     yaw;
    /* 0x0C */ s16     delta;
    /* 0x0E */ s16     zone;
    /* 0x10 */ s16     aim;
    /* 0x12 */ s16     reply;
} Actor421600AttackScratch;
STATIC_ASSERT_SIZEOF(Actor421600AttackScratch, 0x14);

/// Waypoint steering scratch with a zone-table index at the tail.
typedef struct Actor421600RouteScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ SVECTOR target;
    /* 0x10 */ MATRIX  matrix;
    /* 0x30 */ s16     delta;
    /* 0x32 */ s16     original;
    /* 0x34 */ s16     yaw;
    /* 0x36 */ s16     playerYaw;
    /* 0x38 */ s16     zone;
    /* 0x3A */ s16     pad;
} Actor421600RouteScratch;
STATIC_ASSERT_SIZEOF(Actor421600RouteScratch, 0x3C);

typedef struct Actor421600ParamRow {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
} Actor421600ParamRow;
STATIC_ASSERT_SIZEOF(Actor421600ParamRow, 0x8);

extern Actor421600ParamRow D_actor_421600_8013EF48[];
extern EnemyParams         D_actor_421600_8013EF38;
extern u8                  D_actor_421600_80151028[];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_421600_80151118[8];

/// The overlay's pose table: 8-byte records of three halfwords at 0x0/0x2/0x4
/// plus padding, i.e. `SVECTOR`s. Indexed by the low signed halfword of the
/// caller's id. `actor_403000` keeps a table of the same shape at 0x80158CE0
/// and reaches it with a body the shared-body index groups with this one; a
/// body that reads its own overlay's data cannot be promoted, so each carrier
/// keeps a plain-C copy -- `src/actors/actor_403000/actor_403000.c` for the
/// other.
extern SVECTOR D_actor_421600_80151158[];

/// Hit-position table `desertChaserHitEffect` picks one of twelve entries
/// from by relative hit yaw: the same 8-byte `SVECTOR` records as the pose
/// table above, at the 0x801510B8 end of the same trailing data run.

/// 4-byte table indexed by `(arg0 > 0) + ((arg1 < 1) << 1)`.
extern s8 D_actor_421600_801511D0[];

/// Two XZ waypoint pairs for each place-key mode, indexed by zone.
extern s32 D_actor_421600_801511D4[][8];

/// The records closing four of the overlay's model streams, which the
/// death-tick frames point `D_80114B34[5].data.model` at before each `Gp_SpawnEff`.
static TmdSource _gActor421600DesertChaserBurstLegRight;
static TmdSource _gActor421600DesertChaserBurstLegLeft;
static TmdSource _gActor421600DesertChaserBurstHead;
static TmdSource _gActor421600DesertChaserBurstTorso;

/// Global effect-model callback slot the spawn helpers read; a one-element
/// array so the store is absolute (see actor 401300's header for the same
/// declaration).

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

/// 4x4 zone table `func_actor_421600_8013A404` samples with the X and Z
/// buckets of the actor's position, cell `x | z * 4`; the sample is compared
/// against 0xB to pick between the 6 and 0x24 states.
extern s8 D_actor_421600_801511C0[16];

/// Animation tables selected by the attack tick for front and rear contact.

/// Idle yaw `func_actor_421600_80132A00` stamps onto the enemy's `field_40`
/// on every state message, the same slot actor 00100 keeps at 0x8013EF3C.

/// Progress counter the same handler compares against 4 / 5 / 2 / 0 to pick
/// the arena corner the actor is dropped into. Written by
/// `func_actor_421600_80134AD4` at spawn.
extern s16 D_actor_421600_80151268;

/// Signed transition durations, indexed by old animation * 25 + new animation.
extern s8 gDesertChaserClipStartFrames[];

/// Per-frame scratch: the view-space body position and its arena zone.
typedef struct Actor421600UpdateScratch {
    /* 0x00 */ VECTOR  unused;
    /* 0x10 */ SVECTOR pos;
    /* 0x18 */ s16     zone;
    /* 0x1A */ s16     pad;
} Actor421600UpdateScratch;
STATIC_ASSERT_SIZEOF(Actor421600UpdateScratch, 0x1C);

typedef struct Actor421600StateTable {
    TaskFunc fn[40];
} Actor421600StateTable;
STATIC_ASSERT_SIZEOF(Actor421600StateTable, 0xA0);

// Contact animations share endpoints with the following bank and hit positions.
// The alternate views include the sixth slot written by the contact handler.
// The rear endpoint overwrites hitOffsets[0].vx/vy; no restoration was found
// before later damage processing. Whether that sequence occurs needs a trace.
typedef union {
    struct {
        AnimationSet* front[5];
        AnimationSet* back[5];
        SVECTOR       hitOffsets[12];
    } data;
    AnimationSet* frontCommand[6];
    struct {
        AnimationSet* front[5];
        AnimationSet* command[6];
    } rear;
} Actor421600ContactStorage;
STATIC_ASSERT_SIZEOF(Actor421600ContactStorage, 136);
STATIC_ASSERT(OFFSET_OF(Actor421600ContactStorage, rear.command) == 20, actor421600_rear_contact_offset);
STATIC_ASSERT(OFFSET_OF(Actor421600ContactStorage, data.hitOffsets) == 40, actor421600_hit_offsets_offset);
extern Actor421600ContactStorage D_actor_421600_80151090;

typedef struct {
    AnimationSet* value;
} Actor421600AnimWord;

// These bounded symbol views preserve the original independent address loads.
extern AnimationSet*       gDesertChaserFrontAnim[6] __asm__("D_actor_421600_80151090");
extern AnimationSet*       gDesertChaserRearAnim[6] __asm__("D_actor_421600_80151090+20");
extern SVECTOR             gDesertChaserHitOffsets[12] __asm__("D_actor_421600_80151090+40");
extern Actor421600AnimWord Actor421600FrontContact __asm__("D_actor_421600_80151090+16");
extern Actor421600AnimWord Actor421600FallbackEnd __asm__("D_actor_421600_80151090+20");

static void func_actor_421600_8013E668(Task* task);
static void func_actor_421600_8013E858(Task* arg0);
static void func_actor_421600_8013E9D8(Task* arg0);
static void func_actor_421600_8013EAAC(Task* arg0);
static void func_actor_421600_8013EB7C(Task* arg0);

static AnimationSet _gActor421600Animation19E54;
static AnimationSet _gActor421600Animation1A5F0;
static AnimationSet _gActor421600Animation1ADFC;
static AnimationSet _gActor421600Animation1B610;
static AnimationSet _gActor421600Animation1B8C4;
static AnimationSet _gActor421600Animation1BBE4;
static TmdSource    _gActor421600DesertChaserBody;
s32                 func_actor_421600_80132A00(Task* task, s32 msgId, ActorCommand* request, s32 arg3);
s32                 func_actor_421600_8013E4EC(Task*, s32, s32, s32);
s32                 func_actor_421600_8013E654(Task*, s32, s32, s32);
s32                 func_actor_421600_8013E424(Task*, s32, s32, s32);

DamageAttack D_actor_421600_8013EF24[5] = {
    { 30, 0 },
    { 30, 0 },
    { 18, 0 },
    { 18, 0 },
    { 0xFFFF, 0 },
};

EnemyParams D_actor_421600_8013EF38 = { D_actor_421600_8013EF24, 200, 75, 50, 4, 100, 10, 100, 0 };

Actor421600ParamRow D_actor_421600_8013EF48[4] = {
    { 60, 36, 10, 150 },
    { 40, 26, 10, 120 },
    { 20, 18, 10, 120 },
    { 60, 60, 10, 150 },
};

static TmdBone _gActor421600DesertChaserBodySkeleton[18] = {
#include "assets/desert_chaser_body_skeleton.inc"
};

static u32 _gActor421600DesertChaserBodyPartVerts[18] = {
#include "assets/desert_chaser_body_partVerts.inc"
};

static SVECTOR _gActor421600DesertChaserBodyVerts[266] = {
#include "assets/desert_chaser_body_verts.inc"
};

static SVECTOR _gActor421600DesertChaserBodyNormals[324] = {
#include "assets/desert_chaser_body_normals.inc"
};

static u32 _gActor421600DesertChaserBodyStream[3435] = {
#include "assets/desert_chaser_body_stream.inc"
};

static TmdSource _gActor421600DesertChaserBody = {
    0,
    17616,
    5944,
    18,
    _gActor421600DesertChaserBodyPartVerts,
    _gActor421600DesertChaserBodyVerts,
    _gActor421600DesertChaserBodyNormals,
    _gActor421600DesertChaserBodySkeleton,
    _gActor421600DesertChaserBodyStream,
};

static TmdBone _gActor421600DesertChaserBurstLegRightSkeleton[1] = {
#include "assets/desert_chaser_burst_leg_right_skeleton.inc"
};

static u32 _gActor421600DesertChaserBurstLegRightPartVerts[1] = {
#include "assets/desert_chaser_burst_leg_right_partVerts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstLegRightVerts[21] = {
#include "assets/desert_chaser_burst_leg_right_verts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstLegRightNormals[1] = {
#include "assets/desert_chaser_burst_leg_right_normals.inc"
};

static u32 _gActor421600DesertChaserBurstLegRightStream[233] = {
#include "assets/desert_chaser_burst_leg_right_stream.inc"
};

static TmdSource _gActor421600DesertChaserBurstLegRight = {
    0,
    1536,
    0,
    1,
    _gActor421600DesertChaserBurstLegRightPartVerts,
    _gActor421600DesertChaserBurstLegRightVerts,
    _gActor421600DesertChaserBurstLegRightNormals,
    _gActor421600DesertChaserBurstLegRightSkeleton,
    _gActor421600DesertChaserBurstLegRightStream,
};

static TmdBone _gActor421600DesertChaserBurstLegLeftSkeleton[1] = {
#include "assets/desert_chaser_burst_leg_left_skeleton.inc"
};

static u32 _gActor421600DesertChaserBurstLegLeftPartVerts[1] = {
#include "assets/desert_chaser_burst_leg_left_partVerts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstLegLeftVerts[23] = {
#include "assets/desert_chaser_burst_leg_left_verts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstLegLeftNormals[1] = {
#include "assets/desert_chaser_burst_leg_left_normals.inc"
};

static u32 _gActor421600DesertChaserBurstLegLeftStream[242] = {
#include "assets/desert_chaser_burst_leg_left_stream.inc"
};

static TmdSource _gActor421600DesertChaserBurstLegLeft = {
    0,
    1612,
    0,
    1,
    _gActor421600DesertChaserBurstLegLeftPartVerts,
    _gActor421600DesertChaserBurstLegLeftVerts,
    _gActor421600DesertChaserBurstLegLeftNormals,
    _gActor421600DesertChaserBurstLegLeftSkeleton,
    _gActor421600DesertChaserBurstLegLeftStream,
};

static TmdBone _gActor421600DesertChaserBurstHeadSkeleton[1] = {
#include "assets/desert_chaser_burst_head_skeleton.inc"
};

static u32 _gActor421600DesertChaserBurstHeadPartVerts[1] = {
#include "assets/desert_chaser_burst_head_partVerts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstHeadVerts[59] = {
#include "assets/desert_chaser_burst_head_verts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstHeadNormals[81] = {
#include "assets/desert_chaser_burst_head_normals.inc"
};

static u32 _gActor421600DesertChaserBurstHeadStream[556] = {
#include "assets/desert_chaser_burst_head_stream.inc"
};

static TmdSource _gActor421600DesertChaserBurstHead = {
    0,
    3812,
    0,
    1,
    _gActor421600DesertChaserBurstHeadPartVerts,
    _gActor421600DesertChaserBurstHeadVerts,
    _gActor421600DesertChaserBurstHeadNormals,
    _gActor421600DesertChaserBurstHeadSkeleton,
    _gActor421600DesertChaserBurstHeadStream,
};

static TmdBone _gActor421600DesertChaserBurstTorsoSkeleton[1] = {
#include "assets/desert_chaser_burst_torso_skeleton.inc"
};

static u32 _gActor421600DesertChaserBurstTorsoPartVerts[1] = {
#include "assets/desert_chaser_burst_torso_partVerts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstTorsoVerts[19] = {
#include "assets/desert_chaser_burst_torso_verts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstTorsoNormals[31] = {
#include "assets/desert_chaser_burst_torso_normals.inc"
};

static u32 _gActor421600DesertChaserBurstTorsoStream[193] = {
#include "assets/desert_chaser_burst_torso_stream.inc"
};

static TmdSource _gActor421600DesertChaserBurstTorso = {
    0,
    1248,
    0,
    1,
    _gActor421600DesertChaserBurstTorsoPartVerts,
    _gActor421600DesertChaserBurstTorsoVerts,
    _gActor421600DesertChaserBurstTorsoNormals,
    _gActor421600DesertChaserBurstTorsoSkeleton,
    _gActor421600DesertChaserBurstTorsoStream,
};

static AnimationPackedPose _gActor421600Animation13D18Bank1[10] = {
#include "assets/actor_421600_animation_13D18_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation13D18Bank4[110] = {
#include "assets/actor_421600_animation_13D18_bank4.inc"
};

static AnimationRecord _gActor421600Animation13D18Records[175] = {
#include "assets/actor_421600_animation_13D18_records.inc"
};

static u16 _gActor421600Animation13D18Indices[18] = {
#include "assets/actor_421600_animation_13D18_indices.inc"
};

static AnimationSet _gActor421600Animation13D18 = {
    _gActor421600Animation13D18Records,
    _gActor421600Animation13D18Indices,
    { NULL, _gActor421600Animation13D18Bank1, NULL, NULL, _gActor421600Animation13D18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation13FD4Bank1[5] = {
#include "assets/actor_421600_animation_13FD4_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation13FD4Bank4[29] = {
#include "assets/actor_421600_animation_13FD4_bank4.inc"
};

static AnimationRecord _gActor421600Animation13FD4Records[112] = {
#include "assets/actor_421600_animation_13FD4_records.inc"
};

static u16 _gActor421600Animation13FD4Indices[18] = {
#include "assets/actor_421600_animation_13FD4_indices.inc"
};

static AnimationSet _gActor421600Animation13FD4 = {
    _gActor421600Animation13FD4Records,
    _gActor421600Animation13FD4Indices,
    { NULL, _gActor421600Animation13FD4Bank1, NULL, NULL, _gActor421600Animation13FD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation14608Bank1[13] = {
#include "assets/actor_421600_animation_14608_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation14608Bank4[129] = {
#include "assets/actor_421600_animation_14608_bank4.inc"
};

static AnimationRecord _gActor421600Animation14608Records[210] = {
#include "assets/actor_421600_animation_14608_records.inc"
};

static u16 _gActor421600Animation14608Indices[18] = {
#include "assets/actor_421600_animation_14608_indices.inc"
};

static AnimationSet _gActor421600Animation14608 = {
    _gActor421600Animation14608Records,
    _gActor421600Animation14608Indices,
    { NULL, _gActor421600Animation14608Bank1, NULL, NULL, _gActor421600Animation14608Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation14BE0Bank1[11] = {
#include "assets/actor_421600_animation_14BE0_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation14BE0Bank4[131] = {
#include "assets/actor_421600_animation_14BE0_bank4.inc"
};

static AnimationRecord _gActor421600Animation14BE0Records[191] = {
#include "assets/actor_421600_animation_14BE0_records.inc"
};

static u16 _gActor421600Animation14BE0Indices[18] = {
#include "assets/actor_421600_animation_14BE0_indices.inc"
};

static AnimationSet _gActor421600Animation14BE0 = {
    _gActor421600Animation14BE0Records,
    _gActor421600Animation14BE0Indices,
    { NULL, _gActor421600Animation14BE0Bank1, NULL, NULL, _gActor421600Animation14BE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation15014Bank1[9] = {
#include "assets/actor_421600_animation_15014_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation15014Bank4[83] = {
#include "assets/actor_421600_animation_15014_bank4.inc"
};

static AnimationRecord _gActor421600Animation15014Records[140] = {
#include "assets/actor_421600_animation_15014_records.inc"
};

static u16 _gActor421600Animation15014Indices[18] = {
#include "assets/actor_421600_animation_15014_indices.inc"
};

static AnimationSet _gActor421600Animation15014 = {
    _gActor421600Animation15014Records,
    _gActor421600Animation15014Indices,
    { NULL, _gActor421600Animation15014Bank1, NULL, NULL, _gActor421600Animation15014Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation156ECBank1[19] = {
#include "assets/actor_421600_animation_156EC_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation156ECBank4[138] = {
#include "assets/actor_421600_animation_156EC_bank4.inc"
};

static AnimationRecord _gActor421600Animation156ECRecords[224] = {
#include "assets/actor_421600_animation_156EC_records.inc"
};

static u16 _gActor421600Animation156ECIndices[18] = {
#include "assets/actor_421600_animation_156EC_indices.inc"
};

static AnimationSet _gActor421600Animation156EC = {
    _gActor421600Animation156ECRecords,
    _gActor421600Animation156ECIndices,
    { NULL, _gActor421600Animation156ECBank1, NULL, NULL, _gActor421600Animation156ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation15CECBank1[14] = {
#include "assets/actor_421600_animation_15CEC_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation15CECBank4[99] = {
#include "assets/actor_421600_animation_15CEC_bank4.inc"
};

static AnimationRecord _gActor421600Animation15CECRecords[224] = {
#include "assets/actor_421600_animation_15CEC_records.inc"
};

static u16 _gActor421600Animation15CECIndices[18] = {
#include "assets/actor_421600_animation_15CEC_indices.inc"
};

static AnimationSet _gActor421600Animation15CEC = {
    _gActor421600Animation15CECRecords,
    _gActor421600Animation15CECIndices,
    { NULL, _gActor421600Animation15CECBank1, NULL, NULL, _gActor421600Animation15CECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation15F48Bank1[4] = {
#include "assets/actor_421600_animation_15F48_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation15F48Bank4[34] = {
#include "assets/actor_421600_animation_15F48_bank4.inc"
};

static AnimationRecord _gActor421600Animation15F48Records[86] = {
#include "assets/actor_421600_animation_15F48_records.inc"
};

static u16 _gActor421600Animation15F48Indices[18] = {
#include "assets/actor_421600_animation_15F48_indices.inc"
};

static AnimationSet _gActor421600Animation15F48 = {
    _gActor421600Animation15F48Records,
    _gActor421600Animation15F48Indices,
    { NULL, _gActor421600Animation15F48Bank1, NULL, NULL, _gActor421600Animation15F48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1656CBank1[12] = {
#include "assets/actor_421600_animation_1656C_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1656CBank4[147] = {
#include "assets/actor_421600_animation_1656C_bank4.inc"
};

static AnimationRecord _gActor421600Animation1656CRecords[191] = {
#include "assets/actor_421600_animation_1656C_records.inc"
};

static u16 _gActor421600Animation1656CIndices[18] = {
#include "assets/actor_421600_animation_1656C_indices.inc"
};

static AnimationSet _gActor421600Animation1656C = {
    _gActor421600Animation1656CRecords,
    _gActor421600Animation1656CIndices,
    { NULL, _gActor421600Animation1656CBank1, NULL, NULL, _gActor421600Animation1656CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation169BCBank1[9] = {
#include "assets/actor_421600_animation_169BC_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation169BCBank4[73] = {
#include "assets/actor_421600_animation_169BC_bank4.inc"
};

static AnimationRecord _gActor421600Animation169BCRecords[157] = {
#include "assets/actor_421600_animation_169BC_records.inc"
};

static u16 _gActor421600Animation169BCIndices[18] = {
#include "assets/actor_421600_animation_169BC_indices.inc"
};

static AnimationSet _gActor421600Animation169BC = {
    _gActor421600Animation169BCRecords,
    _gActor421600Animation169BCIndices,
    { NULL, _gActor421600Animation169BCBank1, NULL, NULL, _gActor421600Animation169BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation16FA0Bank1[12] = {
#include "assets/actor_421600_animation_16FA0_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation16FA0Bank4[126] = {
#include "assets/actor_421600_animation_16FA0_bank4.inc"
};

static AnimationRecord _gActor421600Animation16FA0Records[196] = {
#include "assets/actor_421600_animation_16FA0_records.inc"
};

static u16 _gActor421600Animation16FA0Indices[18] = {
#include "assets/actor_421600_animation_16FA0_indices.inc"
};

static AnimationSet _gActor421600Animation16FA0 = {
    _gActor421600Animation16FA0Records,
    _gActor421600Animation16FA0Indices,
    { NULL, _gActor421600Animation16FA0Bank1, NULL, NULL, _gActor421600Animation16FA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1711CBank1[2] = {
#include "assets/actor_421600_animation_1711C_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1711CBank4[16] = {
#include "assets/actor_421600_animation_1711C_bank4.inc"
};

static AnimationRecord _gActor421600Animation1711CRecords[54] = {
#include "assets/actor_421600_animation_1711C_records.inc"
};

static u16 _gActor421600Animation1711CIndices[18] = {
#include "assets/actor_421600_animation_1711C_indices.inc"
};

static AnimationSet _gActor421600Animation1711C = {
    _gActor421600Animation1711CRecords,
    _gActor421600Animation1711CIndices,
    { NULL, _gActor421600Animation1711CBank1, NULL, NULL, _gActor421600Animation1711CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation177C0Bank1[13] = {
#include "assets/actor_421600_animation_177C0_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation177C0Bank4[158] = {
#include "assets/actor_421600_animation_177C0_bank4.inc"
};

static AnimationRecord _gActor421600Animation177C0Records[209] = {
#include "assets/actor_421600_animation_177C0_records.inc"
};

static u16 _gActor421600Animation177C0Indices[18] = {
#include "assets/actor_421600_animation_177C0_indices.inc"
};

static AnimationSet _gActor421600Animation177C0 = {
    _gActor421600Animation177C0Records,
    _gActor421600Animation177C0Indices,
    { NULL, _gActor421600Animation177C0Bank1, NULL, NULL, _gActor421600Animation177C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation17A84Bank1[4] = {
#include "assets/actor_421600_animation_17A84_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation17A84Bank4[45] = {
#include "assets/actor_421600_animation_17A84_bank4.inc"
};

static AnimationRecord _gActor421600Animation17A84Records[101] = {
#include "assets/actor_421600_animation_17A84_records.inc"
};

static u16 _gActor421600Animation17A84Indices[18] = {
#include "assets/actor_421600_animation_17A84_indices.inc"
};

static AnimationSet _gActor421600Animation17A84 = {
    _gActor421600Animation17A84Records,
    _gActor421600Animation17A84Indices,
    { NULL, _gActor421600Animation17A84Bank1, NULL, NULL, _gActor421600Animation17A84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation17F88Bank1[9] = {
#include "assets/actor_421600_animation_17F88_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation17F88Bank4[123] = {
#include "assets/actor_421600_animation_17F88_bank4.inc"
};

static AnimationRecord _gActor421600Animation17F88Records[152] = {
#include "assets/actor_421600_animation_17F88_records.inc"
};

static u16 _gActor421600Animation17F88Indices[18] = {
#include "assets/actor_421600_animation_17F88_indices.inc"
};

static AnimationSet _gActor421600Animation17F88 = {
    _gActor421600Animation17F88Records,
    _gActor421600Animation17F88Indices,
    { NULL, _gActor421600Animation17F88Bank1, NULL, NULL, _gActor421600Animation17F88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation18508Bank1[13] = {
#include "assets/actor_421600_animation_18508_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation18508Bank4[113] = {
#include "assets/actor_421600_animation_18508_bank4.inc"
};

static AnimationRecord _gActor421600Animation18508Records[181] = {
#include "assets/actor_421600_animation_18508_records.inc"
};

static u16 _gActor421600Animation18508Indices[18] = {
#include "assets/actor_421600_animation_18508_indices.inc"
};

static AnimationSet _gActor421600Animation18508 = {
    _gActor421600Animation18508Records,
    _gActor421600Animation18508Indices,
    { NULL, _gActor421600Animation18508Bank1, NULL, NULL, _gActor421600Animation18508Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation18AB8Bank1[14] = {
#include "assets/actor_421600_animation_18AB8_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation18AB8Bank4[116] = {
#include "assets/actor_421600_animation_18AB8_bank4.inc"
};

static AnimationRecord _gActor421600Animation18AB8Records[187] = {
#include "assets/actor_421600_animation_18AB8_records.inc"
};

static u16 _gActor421600Animation18AB8Indices[18] = {
#include "assets/actor_421600_animation_18AB8_indices.inc"
};

static AnimationSet _gActor421600Animation18AB8 = {
    _gActor421600Animation18AB8Records,
    _gActor421600Animation18AB8Indices,
    { NULL, _gActor421600Animation18AB8Bank1, NULL, NULL, _gActor421600Animation18AB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation19178Bank1[13] = {
#include "assets/actor_421600_animation_19178_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation19178Bank4[160] = {
#include "assets/actor_421600_animation_19178_bank4.inc"
};

static AnimationRecord _gActor421600Animation19178Records[214] = {
#include "assets/actor_421600_animation_19178_records.inc"
};

static u16 _gActor421600Animation19178Indices[18] = {
#include "assets/actor_421600_animation_19178_indices.inc"
};

static AnimationSet _gActor421600Animation19178 = {
    _gActor421600Animation19178Records,
    _gActor421600Animation19178Indices,
    { NULL, _gActor421600Animation19178Bank1, NULL, NULL, _gActor421600Animation19178Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1960CBank1[8] = {
#include "assets/actor_421600_animation_1960C_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1960CBank4[100] = {
#include "assets/actor_421600_animation_1960C_bank4.inc"
};

static AnimationRecord _gActor421600Animation1960CRecords[150] = {
#include "assets/actor_421600_animation_1960C_records.inc"
};

static u16 _gActor421600Animation1960CIndices[18] = {
#include "assets/actor_421600_animation_1960C_indices.inc"
};

static AnimationSet _gActor421600Animation1960C = {
    _gActor421600Animation1960CRecords,
    _gActor421600Animation1960CIndices,
    { NULL, _gActor421600Animation1960CBank1, NULL, NULL, _gActor421600Animation1960CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation19E54Bank1[21] = {
#include "assets/actor_421600_animation_19E54_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation19E54Bank4[193] = {
#include "assets/actor_421600_animation_19E54_bank4.inc"
};

static AnimationRecord _gActor421600Animation19E54Records[254] = {
#include "assets/actor_421600_animation_19E54_records.inc"
};

static u16 _gActor421600Animation19E54Indices[20] = {
#include "assets/actor_421600_animation_19E54_indices.inc"
};

static AnimationSet _gActor421600Animation19E54 = {
    _gActor421600Animation19E54Records,
    _gActor421600Animation19E54Indices,
    { NULL, _gActor421600Animation19E54Bank1, NULL, NULL, _gActor421600Animation19E54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1A5F0Bank1[20] = {
#include "assets/actor_421600_animation_1A5F0_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1A5F0Bank4[172] = {
#include "assets/actor_421600_animation_1A5F0_bank4.inc"
};

static AnimationRecord _gActor421600Animation1A5F0Records[235] = {
#include "assets/actor_421600_animation_1A5F0_records.inc"
};

static u16 _gActor421600Animation1A5F0Indices[20] = {
#include "assets/actor_421600_animation_1A5F0_indices.inc"
};

static AnimationSet _gActor421600Animation1A5F0 = {
    _gActor421600Animation1A5F0Records,
    _gActor421600Animation1A5F0Indices,
    { NULL, _gActor421600Animation1A5F0Bank1, NULL, NULL, _gActor421600Animation1A5F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1ADFCBank1[15] = {
#include "assets/actor_421600_animation_1ADFC_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1ADFCBank4[206] = {
#include "assets/actor_421600_animation_1ADFC_bank4.inc"
};

static AnimationRecord _gActor421600Animation1ADFCRecords[244] = {
#include "assets/actor_421600_animation_1ADFC_records.inc"
};

static u16 _gActor421600Animation1ADFCIndices[20] = {
#include "assets/actor_421600_animation_1ADFC_indices.inc"
};

static AnimationSet _gActor421600Animation1ADFC = {
    _gActor421600Animation1ADFCRecords,
    _gActor421600Animation1ADFCIndices,
    { NULL, _gActor421600Animation1ADFCBank1, NULL, NULL, _gActor421600Animation1ADFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1B610Bank1[14] = {
#include "assets/actor_421600_animation_1B610_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1B610Bank4[207] = {
#include "assets/actor_421600_animation_1B610_bank4.inc"
};

static AnimationRecord _gActor421600Animation1B610Records[248] = {
#include "assets/actor_421600_animation_1B610_records.inc"
};

static u16 _gActor421600Animation1B610Indices[20] = {
#include "assets/actor_421600_animation_1B610_indices.inc"
};

static AnimationSet _gActor421600Animation1B610 = {
    _gActor421600Animation1B610Records,
    _gActor421600Animation1B610Indices,
    { NULL, _gActor421600Animation1B610Bank1, NULL, NULL, _gActor421600Animation1B610Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1B8C4Bank1[5] = {
#include "assets/actor_421600_animation_1B8C4_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1B8C4Bank4[55] = {
#include "assets/actor_421600_animation_1B8C4_bank4.inc"
};

static AnimationRecord _gActor421600Animation1B8C4Records[83] = {
#include "assets/actor_421600_animation_1B8C4_records.inc"
};

static u16 _gActor421600Animation1B8C4Indices[20] = {
#include "assets/actor_421600_animation_1B8C4_indices.inc"
};

static AnimationSet _gActor421600Animation1B8C4 = {
    _gActor421600Animation1B8C4Records,
    _gActor421600Animation1B8C4Indices,
    { NULL, _gActor421600Animation1B8C4Bank1, NULL, NULL, _gActor421600Animation1B8C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1BBE4Bank1[6] = {
#include "assets/actor_421600_animation_1BBE4_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1BBE4Bank4[66] = {
#include "assets/actor_421600_animation_1BBE4_bank4.inc"
};

static AnimationRecord _gActor421600Animation1BBE4Records[96] = {
#include "assets/actor_421600_animation_1BBE4_records.inc"
};

static u16 _gActor421600Animation1BBE4Indices[20] = {
#include "assets/actor_421600_animation_1BBE4_indices.inc"
};

static AnimationSet _gActor421600Animation1BBE4 = {
    _gActor421600Animation1BBE4Records,
    _gActor421600Animation1BBE4Indices,
    { NULL, _gActor421600Animation1BBE4Bank1, NULL, NULL, _gActor421600Animation1BBE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1C09CBank1[9] = {
#include "assets/actor_421600_animation_1C09C_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1C09CBank4[68] = {
#include "assets/actor_421600_animation_1C09C_bank4.inc"
};

static AnimationRecord _gActor421600Animation1C09CRecords[188] = {
#include "assets/actor_421600_animation_1C09C_records.inc"
};

static u16 _gActor421600Animation1C09CIndices[18] = {
#include "assets/actor_421600_animation_1C09C_indices.inc"
};

static AnimationSet _gActor421600Animation1C09C = {
    _gActor421600Animation1C09CRecords,
    _gActor421600Animation1C09CIndices,
    { NULL, _gActor421600Animation1C09CBank1, NULL, NULL, _gActor421600Animation1C09CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1C6A0Bank1[11] = {
#include "assets/actor_421600_animation_1C6A0_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1C6A0Bank4[138] = {
#include "assets/actor_421600_animation_1C6A0_bank4.inc"
};

static AnimationRecord _gActor421600Animation1C6A0Records[195] = {
#include "assets/actor_421600_animation_1C6A0_records.inc"
};

static u16 _gActor421600Animation1C6A0Indices[18] = {
#include "assets/actor_421600_animation_1C6A0_indices.inc"
};

static AnimationSet _gActor421600Animation1C6A0 = {
    _gActor421600Animation1C6A0Records,
    _gActor421600Animation1C6A0Indices,
    { NULL, _gActor421600Animation1C6A0Bank1, NULL, NULL, _gActor421600Animation1C6A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1CE30Bank1[13] = {
#include "assets/actor_421600_animation_1CE30_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1CE30Bank4[185] = {
#include "assets/actor_421600_animation_1CE30_bank4.inc"
};

static AnimationRecord _gActor421600Animation1CE30Records[241] = {
#include "assets/actor_421600_animation_1CE30_records.inc"
};

static u16 _gActor421600Animation1CE30Indices[18] = {
#include "assets/actor_421600_animation_1CE30_indices.inc"
};

static AnimationSet _gActor421600Animation1CE30 = {
    _gActor421600Animation1CE30Records,
    _gActor421600Animation1CE30Indices,
    { NULL, _gActor421600Animation1CE30Bank1, NULL, NULL, _gActor421600Animation1CE30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1D030Bank1[2] = {
#include "assets/actor_421600_animation_1D030_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1D030Bank4[24] = {
#include "assets/actor_421600_animation_1D030_bank4.inc"
};

static AnimationRecord _gActor421600Animation1D030Records[79] = {
#include "assets/actor_421600_animation_1D030_records.inc"
};

static u16 _gActor421600Animation1D030Indices[18] = {
#include "assets/actor_421600_animation_1D030_indices.inc"
};

static AnimationSet _gActor421600Animation1D030 = {
    _gActor421600Animation1D030Records,
    _gActor421600Animation1D030Indices,
    { NULL, _gActor421600Animation1D030Bank1, NULL, NULL, _gActor421600Animation1D030Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1EF6CBank1[84] = {
#include "assets/actor_421600_animation_1EF6C_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1EF6CBank4[736] = {
#include "assets/actor_421600_animation_1EF6C_bank4.inc"
};

static AnimationRecord _gActor421600Animation1EF6CRecords[992] = {
#include "assets/actor_421600_animation_1EF6C_records.inc"
};

static u16 _gActor421600Animation1EF6CIndices[18] = {
#include "assets/actor_421600_animation_1EF6C_indices.inc"
};

static AnimationSet _gActor421600Animation1EF6C = {
    _gActor421600Animation1EF6CRecords,
    _gActor421600Animation1EF6CIndices,
    { NULL, _gActor421600Animation1EF6CBank1, NULL, NULL, _gActor421600Animation1EF6CBank4, NULL, NULL, NULL },
};

s8 gDesertChaserClipStartFrames[628] = {
    0,
    0,
    5,
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
    0,
    0,
    8,
    8,
    8,
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
    3,
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
    5,
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
    5,
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
    5,
    0,
    5,
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
    5,
    0,
    5,
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
    5,
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
    5,
    7,
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
    5,
    7,
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
    5,
    7,
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
};

u8 D_actor_421600_80151028[104] = {
    56,
    91,
    20,
    128,
    244,
    93,
    20,
    128,
    40,
    100,
    20,
    128,
    0,
    106,
    20,
    128,
    52,
    110,
    20,
    128,
    12,
    117,
    20,
    128,
    12,
    123,
    20,
    128,
    104,
    125,
    20,
    128,
    140,
    131,
    20,
    128,
    220,
    135,
    20,
    128,
    192,
    141,
    20,
    128,
    60,
    143,
    20,
    128,
    224,
    149,
    20,
    128,
    188,
    222,
    20,
    128,
    192,
    228,
    20,
    128,
    80,
    236,
    20,
    128,
    80,
    238,
    20,
    128,
    140,
    13,
    21,
    128,
    164,
    152,
    20,
    128,
    168,
    157,
    20,
    128,
    40,
    163,
    20,
    128,
    216,
    168,
    20,
    128,
    152,
    175,
    20,
    128,
    44,
    180,
    20,
    128,
    192,
    141,
    20,
    128,
    0,
    0,
    0,
    0,
};

Actor421600ContactStorage D_actor_421600_80151090 = { .data = { { NULL, &_gActor421600Animation19E54, &_gActor421600Animation1ADFC, &_gActor421600Animation1B8C4, NULL }, { NULL, &_gActor421600Animation1A5F0, &_gActor421600Animation1B610, &_gActor421600Animation1BBE4, NULL }, { { 60, -12, 30, 2 }, { -50, -130, 29, 2 }, { 20, -70, 25, 2 }, { -30, -65, 25, 2 }, { 60, -120, 30, 2 }, { 20, -20, -5, 2 }, { -15, -50, 0, 2 }, { 2, 10, -15, 2 }, { 14, 0, 0, 7 }, { 25, 0, 0, 2 }, { -14, 0, 0, 9 }, { -25, 0, 0, 2 } } } };

TaskMessageEntry D_actor_421600_80151118[8] = {
    { 2015, func_actor_421600_8013E424 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, func_actor_421600_8013E4EC },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawFirst },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_421600_80132A00 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, desertChaserMsgPlayAnim },
    { ROOM_MESSAGE_ACTOR_EVENT, func_actor_421600_8013E654 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_actor_421600_80151158[13] = {
    { 1000, 0, 4500, 0 },
    { 1000, 0, 4500, 0 },
    { -1000, 0, 4500, 0 },
    { -4500, 0, 4500, 0 },
    { -4500, 0, 1500, 0 },
    { -4500, 0, -1500, 0 },
    { -4500, 0, -5000, 0 },
    { -1000, 0, -5300, 0 },
    { 1000, 0, -5500, 0 },
    { 4500, 0, -5300, 0 },
    { 4500, 0, -1500, 0 },
    { 4500, 0, 1500, 0 },
    { 0, 0, 0, 0 },
};

s8 D_actor_421600_801511C0[16] = {
    3,
    2,
    1,
    0,
    4,
    13,
    12,
    11,
    5,
    15,
    14,
    10,
    6,
    7,
    8,
    9,
};

s8 D_actor_421600_801511D0[4] = {
    1,
    0,
    3,
    2,
};

s32 D_actor_421600_801511D4[4][8] = {
    { 0x1F948, 538, 0x1FAC1, 2373, 0x1038C, 0xF78A, 0x108BC, 0xF930 },
    { 0x102D1, 751, 0x101C1, 2219, 0x1022D, 2329, 0x106DC, 0xFB69 },
    { 0x1F948, 538, 0x1FAC1, 2373, 0x10247, 2114, 0x10842, 1445 },
    { 0x1F7BC, 1553, 0x1FBDC, 1606, 0x10459, 0xF66F, 0x106C9, 0xFB85 },
};

TaskDesc D_actor_421600_80151254 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, desertChaserTask, { .model = &_gActor421600DesertChaserBody } };

SVECTOR ActorContact_ScratchPosition;

s16 D_actor_421600_80151268;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static __inline__ Actor421600UpdateScratch* Actor421600_AllocUpdateScratch(Actor421600UpdateScratch** head);
static __inline__ s32                       Actor421600_HasPlayerContact(WorldCollisionContact* records);
static s32                                  func_actor_421600_80133334(GfxCoord* arg0);
static void                                 func_actor_421600_80133444(GfxCoord* coord);
static s32                                  desertChaserAvoidWalk(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos);
static __inline__ void                      Actor421600_BindMatrices(Task* actor);
static void                                 func_actor_421600_80134AD4(Enemy* enemy, Task* actor);
static __inline__ s32                       Actor421600_FindDamageHit(WorldCollisionContact* records,
                                                                      SVECTOR*               pos);
static void                                 func_actor_421600_801354D8(Task* arg0);
static void                                 func_actor_421600_80135F6C(Task* arg0);
static __inline__ s16                       Actor421600_Zone(GfxCoord* coord);
static void                                 func_actor_421600_80136138(Task* arg0);
static __inline__ void                      Actor421600_ShrinkCoord(GfxCoord* coord, s16 y);
static void                                 func_actor_421600_801366F4(Task* arg0);
static void                                 func_actor_421600_801369A0(Task* arg0);
static void                                 func_actor_421600_80138D24(Task* arg0);
static void                                 func_actor_421600_8013903C(Task* arg0);
static void                                 func_actor_421600_8013947C(Task* arg0);
static void                                 func_actor_421600_8013A404(Task* arg0);
static void                                 func_actor_421600_8013A554(Task* arg0);
static void                                 func_actor_421600_8013B00C(Task* arg0);
static void                                 func_actor_421600_8013B4C4(Task* arg0);
static void                                 func_actor_421600_8013B8E0(Task* arg0);
static __inline__ s32                       Actor421600_RouteZone(s32 x, s32 z);
static void                                 func_actor_421600_8013BA70(Task* arg0);
static void                                 func_actor_421600_8013C8E0(Task* arg0);
static void                                 func_actor_421600_8013D658(Enemy* enemy, Task* actor);
static void                                 func_actor_421600_8013E7F8(SVECTOR* arg0, s32 arg1);
static s8                                   func_actor_421600_8013E830(s32 arg0, s32 arg1);

static __inline__ Actor421600UpdateScratch* Actor421600_AllocUpdateScratch(Actor421600UpdateScratch** head)
{
    Actor421600UpdateScratch* p                     = SCRATCH_HEAD_AT(head, Actor421600UpdateScratch) - 1;
    SCRATCH_HEAD_AT(head, Actor421600UpdateScratch) = p;
    return p;
}
static __inline__ s32 Actor421600_HasPlayerContact(WorldCollisionContact* records)
{
    s16 i;
    for (i = 0; i < 12; i++) {
        if (records[i].key.value == 0)
            break;
        if ((records[i].key.value & 0xFFFF0000) == 0x10000)
            return 1;
    }
    return 0;
}

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

/// Message handler. Message 0x109 nudges the state machine by its sub-command
/// (1 copies `chaseHoldoffFrames` into `chaseHoldoff`, 3 moves state 1 on to 2). Any other
/// message has its opcode and the low byte of its sub-command latched into
/// `lastCommand`; for 0x1402 the enemy's hit points are restored and, by
/// sub-command, the placement mode in `placeKey` and the progress counter
/// `D_actor_421600_80151268`, the actor is dropped at a fixed spot with a new
/// state. Returns 1 when the message was handled.
s32 func_actor_421600_80132A00(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    DesertChaserWork* work;
    Enemy*            enemy;
    s32               angle;
    s16               mode;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;

    if (request->context.key == 0x109) {
        switch (request->command) {
            case 1:
                work->chaseHoldoff = work->chaseHoldoffFrames;
                break;
            case 2:
                if (work->state == 0x26) {
                    work->state = 0x26;
                }
                break;
            case 3:
                if (work->state == 1) {
                    work->state = 2;
                }
                break;
        }
        return 1;
    }

    work->lastCommand.fields.stage   = request->context.loc.stage;
    work->lastCommand.fields.area    = request->context.loc.area;
    work->lastCommand.fields.command = (u8)request->command;

    if (request->context.key != 0x1402) {
        return 0;
    }

    switch (request->command) {
        case 0:
            enemy->hp = D_actor_421600_8013EF38.hpMax;
            if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
                work->state = 2;
            }
            return 1;

        case 1:
            mode      = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
            enemy->hp = D_actor_421600_8013EF38.hpMax;
            switch (mode) {
                case 0:
                    if (D_actor_421600_80151268 < 4) {
                        goto negstate;
                    }
                    if (work->state != 0) {
                        goto tail;
                    }
                    arg0->extra.tmd->coords->coord.t[0] = 0x1057;
                    arg0->extra.tmd->coords->coord.t[2] = -0x11A3;
                    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x400, 1);
                    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(arg0->extra.tmd->coords);
                    work->state     = 0x20;
                    work->prevState = -1;
                    goto tail;
                case 1:
                    if (D_actor_421600_80151268 < 5) {
                        goto negstate;
                    }
                    if (work->state != 0) {
                        goto tail;
                    }
                    arg0->extra.tmd->coords->coord.t[0] = 0x1467;
                    arg0->extra.tmd->coords->coord.t[2] = 0x4B9;
                    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x7BC, 1);
                    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(arg0->extra.tmd->coords);
                    work->state     = 0x20;
                    work->prevState = -1;
                    goto tail;
            }
            goto tail;
        negstate:
            work->state     = 0;
            work->prevState = -1;
        tail:
            Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
            enemy->reactionFlags = 0;
            enemy->hp            = D_actor_421600_8013EF38.hpMax;
            return 1;

        case 2:
            switch (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
                case 0:
                    if (D_actor_421600_80151268 <= 0) {
                        goto blockDE0;
                    }
                    arg0->extra.tmd->coords->coord.t[0] = -0xD40;
                    arg0->extra.tmd->coords->coord.t[2] = 0x104F;
                    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x76C, 1);
                    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(arg0->extra.tmd->coords);
                    Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_421600_8013EF38.hpMax;
                    work->state          = 6;
                    goto blockDE0;
                case 1:
                    if (D_actor_421600_80151268 < 2) {
                        goto blockDE0;
                    }
                    arg0->extra.tmd->coords->coord.t[0] = 0x138C;
                    arg0->extra.tmd->coords->coord.t[2] = 0x4B2;
                    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x7BC, 1);
                    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(arg0->extra.tmd->coords);
                    Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_421600_8013EF38.hpMax;
                    work->state          = 6;
                    goto blockDE0;
                default:
                    goto blockDE0;
            }
        blockDE0:
            if (D_actor_421600_80151268 == 0) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
            }
            return 1;

        case 3:
            if (work->playerHeld == 1) {
                work->playerHeld = 0;
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
            }
            if (work->state != 0x14 && work->state != 0x11 && work->state != 0x15 &&
                work->state != 0x16 && work->state != 0 && work->state != 8) {
                work->state     = 5;
                work->prevState = -1;
            }
            return 1;

        case 9:
            work->state     = 0;
            work->prevState = -1;
            return 1;

        default:
            return 0;
    }
}

#include "../../shared/limb_shadows_segment.inc.c"

/// Moves an interior coordinate to the nearest padded X or Z edge.
/// Returns 1 when moved, or 0 when already outside the rectangle.
static s32 func_actor_421600_80133334(GfxCoord* arg0)
{
    s16 dx;
    s16 dz;
    s32 adx;
    s32 adz;
    s32 x;
    s32 z;
    s32 z2;

    x = arg0->coord.t[0];
    if ((x >= -0xC4D) && (x < 0xD16)) {
        z = arg0->coord.t[2];
        if (z < 0xC4E) {
            if (z >= -0xC4D) {
                if ((0xD16 - x) > (x + 0xC4E)) {
                    dx = -((u16)arg0->coord.t[0] + 0xCE4);
                } else {
                    dx = 0xDAC - (u16)arg0->coord.t[0];
                }
                z2 = arg0->coord.t[2];
                if ((0xC4E - z2) > (z2 + 0xC4E)) {
                    dz = -((u16)arg0->coord.t[2] + 0xCE4);
                } else {
                    dz = 0xCE4 - (u16)arg0->coord.t[2];
                }
                adx = ABS(dx);
                adz = ABS(dz);
                if (adz < adx) {
                    arg0->coord.t[2] += dz;
                } else {
                    arg0->coord.t[0] += dx;
                }
                arg0->composeStamp = GRAPHICS_COORD_DIRTY;
                return 1;
            }
        }
    }
    return 0;
}

static void func_actor_421600_80133444(GfxCoord* coord)
{
    SVECTOR              vec;
    SVECTOR*             direction;
    OverlayRangeScratch* rangeScratch;
    OverlayRangeScratch* savedScratchHead;
    s32                  outside;
    u8*                  scratchBase;
    u8*                  scratchRestoreBase;

    if ((u32)(coord->coord.t[0] - 0x1F5) < 0x3E7) {
        if (coord->coord.t[2] < 0x1F4) {
            if (coord->coord.t[2] < -0x1F4) {
                // Reserve squared-distance operands, then release them before the test.
                savedScratchHead                                                       = SCRATCH_STACK_CURSOR(OverlayRangeScratch);
                rangeScratch                                                           = savedScratchHead - 1;
                scratchBase                                                            = PLAYSTATION_SCRATCHPAD_BASE;
                *(OverlayRangeScratch**)(scratchBase + SCRATCH_STACK_HEAD_BYTE_OFFSET) = rangeScratch;
                vec.vx                                                                 = (u16)coord->coord.t[0] - 0x3E8;
                vec.vy                                                                 = 0;
                vec.vz                                                                 = (u16)coord->coord.t[2] + 1;
                rangeScratch->dx                                                       = vec.vx;
                direction                                                              = &vec;
                rangeScratch->dz                                                       = direction->vz;
                rangeScratch->radius                                                   = 0x2D0;
                rangeScratch->dx                                                       = rangeScratch->dx * rangeScratch->dx;
                rangeScratch->dz                                                       = rangeScratch->dz * rangeScratch->dz;
                rangeScratch->radius                                                   = rangeScratch->radius * rangeScratch->radius;
                scratchRestoreBase                                                     = PLAYSTATION_SCRATCHPAD_BASE + (SCRATCH_STACK_HEAD_BYTE_OFFSET - sizeof(void*));
                *(OverlayRangeScratch**)(scratchRestoreBase + sizeof(void*))           = savedScratchHead;
                outside                                                                = rangeScratch->dx + rangeScratch->dz >= rangeScratch->radius;
                if (outside != 0) {
                    return;
                }
                VectorNormalSS(direction, direction);
                gte_lddp(0x2BC);
                gte_ldsv(direction);
                gte_gpf12();
                gte_stsv(direction);
                coord->coord.t[0]   = vec.vx + 0x3E8;
                coord->coord.t[2]   = vec.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
            }
        }
    }
}

static s32 desertChaserAvoidWalk(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos)
{
    DesertChaserAvoidScratch* s;
    s16                       diff;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }

    s = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserAvoidScratch);

    s->blocked = 0;
    pos->vz    = 0;
    pos->vy    = 0;
    pos->vx    = 0;

    Gfx_MatrixCol1(&coord->workm, &s->dir);
    VectorNormalSS(&s->dir, &s->dir);

    if (ABS(s->dir.vz) < 0x818) {
        s->heading = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->heading = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->origin.vx = (u16)coord->workm.t[0];
    s->origin.vy = (u16)coord->workm.t[1];
    s->origin.vz = (u16)coord->workm.t[2];
    s->count     = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key.value == 0) {
            break;
        }
        s->kind        = recs[s->i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        s->nonBlocking = recs[s->i].key.value & 0x80;
        switch (s->kind) {
            case 0x10000:
                if (s->nonBlocking == 0) {
                    s->blocked = 1;
                }
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->bearing[s->count] = overlayBearingXZ((SVECTOR3*)&recs[s->i].point, &s->origin);
        } else {
            s->bearing[s->count] = overlayBearingXY((SVECTOR3*)&recs[s->i].point, &s->origin);
        }
        s->kept[s->count] = 1;
        s->count++;
        if (s->count >= ARRAY_SIZE(s->bearing)) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            s->diff = actorWrapAngle((u16)s->bearing[s->i] - (u16)s->bearing[s->j]);
            if (abs(s->diff) > 0x400) {
                s->kept[s->i] = 0;
                s->kept[s->j] = 0;
            }
        }
        if (s->kept[s->i] != 0) {
            diff = ((u16)s->bearing[s->i] - (u16)s->heading) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            gfxRotMatrixY(&s->rot, diff, 1);
            gfxReadMatrixZAxis(&s->rot, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(DesertChaserAvoidScratch);
    return s->blocked != 0;
}

void desertChaserBlendTick(Task* arg0)
{
    AnimationPose     pose;
    AnimationPose     blendPose;
    DesertChaserWork* work;
    s32               blend;
    s32               invBlend;
    s16               index;
    s16               next;

    index = 1;
    work  = arg0->work;
    do {
        switch (index) {
            case 1:
                blend = 0xC00;
                break;
            case 2:
                blend = 0x800;
                break;
            case 3:
            case 4:
            case 5:
                blend = 0x5DE;
                break;
            default:
                blend = 0xBD0;
                break;
        }
        invBlend = 0x1000 - blend;
        if (index < 0xB) {
            work->blend.slots[index].rate = work->blendRate;
            work->rig.slots[index].rate   = (work->animRate - 3);
            animationTickSlotPose(&work->rig.anim, index, &pose, 0);
            animationTickSlotPose(&work->blend.anim, index, &blendPose, 0);
            Gp_AnimWritePoseCopy(&work->rig.anim, index, &pose, &blendPose, blend, invBlend);
        } else {
            work->rig.slots[index].rate = (work->animRate - 3);
            animationTickSlot(&work->rig.anim, index);
        }
        next  = index + 1;
        index = next;
    } while (next < ARRAY_SIZE(work->rig.slots));
}

/// Per-frame effect dispatch keyed on `animId` and the low ten bits of
/// `field_5A`. Each recognised frame is handled once: `lastCueFrames[1]` remembers
/// the frame last handled, and meeting it again only clears `clear`. A handled
/// frame spawns its effects while the room effect mode is 2 and returns a
/// request word; otherwise the result is 0, after wiping `lastCueFrames` when no
/// case claimed the frame.
///
/// `steer` is a matching carrier (see `CSE_STEER`); it has no effect.
s32 desertChaserAnimCues(Task* arg0, DesertChaserWork* work)
{
    SVECTOR offset;
    u32     prev;
    s32     clear = 1;
    s32     steer;

    switch (work->animId) {
        case 0:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 9) {
                prev = work->lastCueFrames[1];
                if (prev != 9) {
                    work->lastCueFrames[1] = 9;
                    offset.vz              = 0;
                    offset.vx              = 0;
                    offset.vy              = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80002280, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80002120, &offset);
                    }
                    return 0x40010002;
                }
                work->lastCueFrames[1] = prev;
                clear                  = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 6) {
                prev = work->lastCueFrames[1];
                if (prev != 6) {
                    work->lastCueFrames[1] = 6;
                    offset.vz              = 0;
                    offset.vx              = 0;
                    offset.vy              = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80002220, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80002120, &offset);
                    }
                    return 0x40010001;
                }
                work->lastCueFrames[1] = prev;
                clear                  = 0;
            }
            break;
        case 10:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 10) {
                prev = work->lastCueFrames[1];
                if (prev != 10) {
                    work->lastCueFrames[1] = 10;
                    offset.vz              = 0;
                    offset.vx              = 0;
                    offset.vy              = 0x0;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[0], 0x80004A00, &offset);
                    }
                    return 0x40010005;
                }
                work->lastCueFrames[1] = prev;
                clear                  = 0;
            }
            break;
        case 3:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 12) {
                prev = work->lastCueFrames[1];
                if (prev != 12) {
                    work->lastCueFrames[1] = 12;
                    return 0x40010004;
                }
                work->lastCueFrames[1] = prev;
                clear                  = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 8) {
                prev = work->lastCueFrames[1];
                if (prev != 8) {
                    work->lastCueFrames[1] = 8;
                    return 0x40010003;
                }
                work->lastCueFrames[1] = prev;
                clear                  = 0;
            }
            break;
        case 6:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 6) {
                prev = work->lastCueFrames[1];
                if (prev != 6) {
                    work->lastCueFrames[1] = 6;
                    offset.vz              = 0;
                    offset.vx              = 0;
                    offset.vy              = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80003200, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80003200, &offset);
                    }
                    return 0x40010011;
                }
                work->lastCueFrames[1] = prev;
                clear                  = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 12) {
                prev = work->lastCueFrames[1];
                if (prev != 12) {
                    work->lastCueFrames[1] = 12;
                    offset.vz              = 0;
                    offset.vx              = 0;
                    offset.vy              = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80004480, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80004480, &offset);
                    }
                    return 0x40010011;
                }
                work->lastCueFrames[1] = prev;
                clear                  = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 13) {
                prev = work->lastCueFrames[1];
                if (prev != 13) {

                    work->lastCueFrames[1] = 13;
                    offset.vz              = 0;
                    offset.vx              = 0;
                    offset.vy              = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80002200, &offset);
                    }
                    CSE_STEER(steer);
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x2BC;
                    if (steer == 0 && gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80002240, &offset);
                    }
                    CSE_STEER(steer);
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x258;
                    if (steer == 0 && gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80003300, &offset);
                    }

                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80003340, &offset);
                    }
                    return 0x40010011;
                }
                work->lastCueFrames[1] = prev;
                clear                  = 0;
            }
            break;
        case 21:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 6) {
                prev = work->lastCueFrames[1];
                if (prev != 6) {
                    work->lastCueFrames[1] = 6;
                    offset.vz              = 0;
                    offset.vx              = 0;
                    offset.vy              = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80003200, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80003200, &offset);
                    }
                    return 0x40010001;
                }
                work->lastCueFrames[1] = prev;
                clear                  = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 9) {
                prev = work->lastCueFrames[1];
                if (prev != 9) {
                    work->lastCueFrames[1] = 9;
                    offset.vz              = 0;
                    offset.vx              = 0;
                    offset.vy              = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80003200, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80003200, &offset);
                    }
                    return 0x40010001;
                }
                work->lastCueFrames[1] = prev;
                clear                  = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 14) {
                prev = work->lastCueFrames[1];
                if (prev != 14) {
                    work->lastCueFrames[1] = 14;
                    offset.vz              = 0;
                    offset.vx              = 0;
                    offset.vy              = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80003200, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80003200, &offset);
                    }
                    return 0x40010002;
                }
                work->lastCueFrames[1] = prev;
                clear                  = 0;
            }
            break;
        case 20:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 6) {
                prev = work->lastCueFrames[1];
                if (prev != 6) {
                    work->lastCueFrames[1] = 6;
                    offset.vz              = 0;
                    offset.vx              = 0;
                    offset.vy              = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80003200, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80003200, &offset);
                    }
                    return 0x40010001;
                }
                work->lastCueFrames[1] = prev;
                clear                  = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 10) {
                prev = work->lastCueFrames[1];
                if (prev != 10) {
                    work->lastCueFrames[1] = 10;
                    offset.vz              = 0;
                    offset.vx              = 0;
                    offset.vy              = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80003200, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80003200, &offset);
                    }
                    return 0x40010001;
                }
                work->lastCueFrames[1] = prev;
                clear                  = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 14) {
                prev = work->lastCueFrames[1];
                if (prev != 14) {
                    work->lastCueFrames[1] = 14;
                    offset.vz              = 0;
                    offset.vx              = 0;
                    offset.vy              = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80003200, &offset);
                    }
                    offset.vz = 0;
                    offset.vx = 0;
                    offset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80003200, &offset);
                    }
                    return 0x40010002;
                }
                work->lastCueFrames[1] = prev;
                clear                  = 0;
            }
            break;
    }
    if (clear == 1) {
        memFillBytes(work->lastCueFrames, 0, sizeof(work->lastCueFrames));
    }
    return 0;
}

#include "../../shared/desert_chaser_anim_tick.inc.c"

static __inline__ void Actor421600_BindMatrices(Task* actor)
{
    DesertChaserWork* work;
    TmdObject*        obj;
    work          = actor->work;
    obj           = actor->extra.tmd;
    obj->lightMtx = &work->lightMtx;
    obj->colorMtx = &work->colorMtx;
}

static void func_actor_421600_80134AD4(Enemy* enemy, Task* actor)
{
    SVECTOR             dir;
    s32                 kind;
    SVECTOR*            v;
    VECTOR              pos;
    TmdObject*          obj;
    GfxCoord*           root;
    DesertChaserWork*   work;
    WorldCollisionBody* body;
    WorldCollisionBody* head;
    root        = actor->extra.tmd->coords;
    obj         = actor->extra.tmd;
    work        = memCalloc(sizeof(DesertChaserWork), 0);
    actor->work = work;
    if (work == 0) {
        enemyDestroy(enemy, actor);
        return;
    }
    (Gp_IncStateF0Ref)(0);
    actor->exitCallback = func_actor_421600_8013E668;
    Actor421600_BindMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords[0].coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)D_actor_421600_8013EF38.hpMax;
    enemy->param                  = &D_actor_421600_8013EF38;
    enemy->recs                   = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_421600_80151028, obj, work->rig.poses, work->rig.slots);
    animationInitContext(&work->blend.anim, (AnimationSet**)D_actor_421600_80151028, obj, work->blend.poses, work->blend.slots);
    work->animRequest   = DESERT_CHASER_ANIM_REQUEST_RESET;
    work->blendActive   = 0;
    work->animId        = 1;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->baseRate      = 0x10;
    work->animRate      = 0x10;
    desertChaserAnimTick(actor);
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.context.contacts = work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.coord            = root;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.pos.vx           = 0;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.pos.vy           = -0x11C;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.pos.vz           = 0;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.key              = 0x30001;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.radius           = 0x12C;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->spheres[DESERT_CHASER_SPHERE_ROOT].body);
    work->wallProbe.shape.ends[0].vx                     = 0;
    work->wallProbe.shape.ends[0].vy                     = -0x180;
    work->wallProbe.shape.ends[0].vz                     = 0;
    work->wallProbe.shape.ends[1].vx                     = 0;
    work->wallProbe.shape.ends[1].vy                     = -0x180;
    work->wallProbe.shape.ends[1].vz                     = 0x2BC;
    work->wallProbe.shape.end0Radius                     = 0x12C;
    work->wallProbe.shape.end1Radius                     = 0x12C;
    work->wallProbe.shape.contacts                       = work->wallProbe.contacts;
    work->wallProbe.body.context.capsule                 = &work->wallProbe.shape;
    work->wallProbe.body.coord                           = root;
    work->wallProbe.body.pos.vx                          = 0;
    work->wallProbe.body.pos.vy                          = 0;
    work->wallProbe.body.pos.vz                          = 0;
    work->wallProbe.body.key                             = 0x30001;
    work->wallProbe.body.radius                          = 0;
    work->wallProbe.body.flags                           = WORLD_COLLISION_BODY_CAPSULE;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    Gp_LinkObj(2, &work->wallProbe.body);
    work->wallProbe.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    Gp_InitRec18Table(work->wallProbe.contacts, ARRAY_SIZE(work->wallProbe.contacts), 0);
    Gp_InitRec18Table(work->spheres[DESERT_CHASER_SPHERE_ROOT].body.context.contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts), 0);
    body                   = &work->spheres[DESERT_CHASER_SPHERE_FRONT].body;
    body->coord            = &actor->extra.tmd->coords[2];
    body->context.contacts = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->key              = 0x30001;
    body->radius           = 0x19C;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(body->context.contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), 0);
    head                   = &work->spheres[DESERT_CHASER_SPHERE_REAR].body;
    head->coord            = &actor->extra.tmd->coords[10];
    head->context.contacts = work->spheres[DESERT_CHASER_SPHERE_REAR].contacts;
    head->pos.vx           = 0;
    head->pos.vy           = 0;
    head->pos.vz           = 0;
    head->key              = 0x30001;
    head->radius           = 0x100;
    head->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, head);
    head->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(head->context.contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts), 0);
    work->spheres[DESERT_CHASER_SPHERE_REAR].body.pos.vx = 0;
    work->spheres[DESERT_CHASER_SPHERE_REAR].body.pos.vy = 0;
    work->spheres[DESERT_CHASER_SPHERE_REAR].body.pos.vz = -0x100;
    work->patrolTarget                                   = 0;
    work->patrolPoints[0].x                              = actor->extra.tmd->coords->coord.t[0];
    work->patrolPoints[0].z                              = actor->extra.tmd->coords->coord.t[2];
    gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, &dir);
    dir.vy = 0;
    v      = &dir;
    VectorNormalSS(v, v);
    gte_lddp(5000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    work->patrolPoints[1].x               = actor->extra.tmd->coords->coord.t[0] + dir.vx;
    work->patrolPoints[1].z               = actor->extra.tmd->coords->coord.t[2] + dir.vz;
    work->playerAnim.blendFrames          = 3;
    work->playerAnim.source.sets          = 0;
    work->playerAnim.animationId          = 1;
    work->playerAnim.blend                = ANIMATION_BLEND_RESET;
    work->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
    actor->msgTable                       = D_actor_421600_80151118;
    root->parent                          = &gGfxViewCoord;
    root->composeStamp                    = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    kind = actor->spawnArg1.value >> 16;
    switch (kind & 0xF) {
        case 1:
            work->prevState = -1;
            work->state     = 0;
            break;

        case 2:
            work->prevState = -1;
            work->state     = 0x21;
            break;

        case 0:

        default:
            work->prevState = -1;
            work->state     = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }

    switch (actor->spawnArg1.value & 0xF) {
        case 2:
            work->windupFrames       = D_actor_421600_8013EF48[0].field_2;
            work->downFramesBase     = D_actor_421600_8013EF48[0].field_0;
            work->roamLookDelay      = D_actor_421600_8013EF48[0].field_4;
            work->chaseHoldoffFrames = D_actor_421600_8013EF48[0].field_6;
            break;

        case 1:
            work->windupFrames       = D_actor_421600_8013EF48[2].field_2;
            work->downFramesBase     = D_actor_421600_8013EF48[2].field_0;
            work->roamLookDelay      = D_actor_421600_8013EF48[2].field_4;
            work->chaseHoldoffFrames = D_actor_421600_8013EF48[2].field_6;
            break;

        case 0:

        default:
            work->windupFrames       = D_actor_421600_8013EF48[1].field_2;
            work->downFramesBase     = D_actor_421600_8013EF48[1].field_0;
            work->roamLookDelay      = D_actor_421600_8013EF48[1].field_4;
            work->chaseHoldoffFrames = D_actor_421600_8013EF48[1].field_6;
            break;
    }

    gSceneCombatState.battleRefs = 8;
    D_actor_421600_80151268      = 8;
    actor->state++;
}

#include "../../shared/desert_chaser_hit_effect.inc.c"

static __inline__ s32 Actor421600_FindDamageHit(WorldCollisionContact* records,
                                                SVECTOR*               pos)
{
    s16 i;
    for (i = 0; i < 12; i++) {
        if (!records[i].key.value)
            break;
        if ((records[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key.value;
        }
    }
    return 0;
}

static void func_actor_421600_801354D8(Task* arg0)
{
    s32                       callAngle;
    s32                       debugMode;
    PlayerStatus*             config = &gPlayerStatus;
    s16                       effect;
    s16                       delta;
    s16                       z;
    s32                       state5;
    s16                       damageState;
    s16                       deathState;
    s16                       hurtState;
    s16                       poisonState;
    s16                       state0;
    s16                       state1;
    s16                       state2;
    s16                       state3;
    s16                       state4;
    s16                       wrapped;
    s16                       hitState;
    s16                       nextDeathState;
    GfxCoord*                 objectCoord;
    s32                       tickDamage;
    s32                       dxSquared;
    s32                       dySquared;
    s32                       yaw;
    s32                       deathSound;
    s32                       hurtSound;
    s32                       hitSound;
    s32                       doubleDamage;
    s32                       dx;
    s32                       dy;
    s32                       dz;
    s32                       distance;
    SVECTOR*                  hitPos;
    s32                       soundBase;
    s32                       deathPan;
    s32                       hurtPan;
    s32                       hitPan;
    u16                       totalDamage;
    u32                       kind;
    DesertChaserWork*         work;
    Enemy*                    enemy;
    Actor421600DamageScratch* scratch;
    Actor421600DamageScratch* head;
    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    if (enemy->hp > 0) {
        head              = SCRATCH_STACK_CURSOR(Actor421600DamageScratch);
        scratch           = (SCRATCH_STACK_CURSOR(Actor421600DamageScratch) = head - 1);
        scratch->field_20 = Actor421600_FindDamageHit(
            work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, (SVECTOR*)&scratch->field_18);
        if (scratch->field_20 == 0) {
            hitPos            = (SVECTOR*)&scratch->field_18;
            scratch->field_20 = Actor421600_FindDamageHit(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts, hitPos);
        }
        if (scratch->field_20 != 0) {
            scratch->field_2E = -1;
            work->hitCooldown = Gp_GetIdParam2(scratch->field_20);
            kind              = Gp_GetIdParam0(scratch->field_20) & 0xFFFF;
            switch (kind) {
                case 0:
                case 6:
                case 7:
                case 8:
                case 9:
                    state0 = work->state;
                    if (state0 == 24 || state0 == 38 || state0 == 39 || state0 == 1) {
                        if (work->state == 0x20) {
                            work->state = 3;
                        } else {
                            work->state = 0x1C;
                        }
                    }
                    if (work->state == 0x21) {
                        work->state = 0x22;
                    }
                    if (work->blendActive == 0) {
                        work->recentDamage = 0U;
                    }
                    state1 = work->state;
                    if ((state1 != 0x22) && (state1 != 0x14) && (state1 != 0x11) &&
                        (state1 != 0x15) && (state1 != 0x16) && (state1 != 4) &&
                        (state1 != 0xB) && (state1 != 0x24) && (state1 != 7)) {
                        work->blendActive  = 1;
                        work->blendAnimId  = 9;
                        work->blendRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
                    }
                    break;
                case 4:
                case 5:
                    state2 = work->state;
                    if (state2 == 4 || state2 == 11 || state2 == 20 || state2 == 17) {
                        work->state     = 11;
                        work->prevState = -1;
                    } else if (state2 != 21 && state2 != 7) {
                        work->state = 20;
                    }
                    break;
                case 2:
                    state3 = work->state;
                    if (state3 == 33 || state3 == 4 || state3 == 11 || state3 == 17) {
                        work->state     = 11;
                        work->prevState = -1;
                    } else if (state3 != 21 && state3 != 7) {
                        work->state = 20;
                    }
                    Gp_SetObjFlag2(enemy, scratch->field_20, 0);
                    break;
                case 3:
                    state4 = work->state;
                    if ((state4 == 0x18) || (state4 == 0x26) || (state4 == 1) ||
                        (state4 == 0x20)) {
                        work->state = 0x1C;
                    }
                    if (work->state == 0x21) {
                        work->state = 0x22;
                    }
                    Gp_SetObjFlag4(enemy, scratch->field_20, 0);
                    break;
                case 1:
                    state5 = work->state;
                    if (state5 != 7) {
                        if (state5 == 4 || state5 == 11 || state5 == 20 || state5 == 17 ||
                            (state5 == 36 && work->stateTimer < 10)) {
                            hitState    = 11;
                            work->state = hitState;
                        } else if (state5 != 21 && state5 != 0 && state5 != 22 &&
                                   state5 != 7) {
                            hitState    = 20;
                            work->state = hitState;
                        }
                    }
                    break;
            }
            dx                                    = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            dxSquared                             = dx * dx;
            scratch->field_0                      = dx;
            dy                                    = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            dySquared                             = dy * dy;
            scratch->field_4                      = dy;
            dz                                    = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
            scratch->field_8                      = dz;
            distance                              = SquareRoot0(dxSquared + dySquared + (dz * dz));
            scratch->field_28                     = distance;
            scratch->field_24                     = Gp_ComputeDamage(scratch->field_20, distance, 0, 0);
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(arg0->extra.tmd->coords);
            scratch->field_10 = (u16)arg0->extra.tmd->coords->workm.t[0];
            scratch->field_12 = (u16)arg0->extra.tmd->coords->workm.t[1];
            scratch->field_14 = (u16)arg0->extra.tmd->coords->workm.t[2];
            scratch->field_10 =
                (u16)(scratch->field_18 - arg0->extra.tmd->coords->workm.t[0]);
            scratch->field_12 =
                (u16)(scratch->field_1A - arg0->extra.tmd->coords->workm.t[1]);
            z                 = scratch->field_1C - arg0->extra.tmd->coords->workm.t[2];
            scratch->field_14 = (u16)z;
            yaw               = ratan2((s16)scratch->field_10, z);
            objectCoord       = arg0->extra.tmd->coords;
            delta =
                yaw - ratan2(-objectCoord->workm.m[2][0], objectCoord->workm.m[2][2]);
            wrapped           = delta;
            scratch->field_2C = delta;
            if (delta < 0) {
                while (1) {
                    if (wrapped >= -0x800)
                        break;
                    wrapped += 0x1000;
                }
            } else {
                while (1) {
                    if (wrapped <= 0x800)
                        break;
                    wrapped -= 0x1000;
                }
            }
            callAngle         = wrapped;
            scratch->field_2C = callAngle;
            desertChaserHitEffect(arg0, callAngle, scratch->field_20);
            work->lookYaw       = 0;
            work->lookYawTarget = 0;
            if (Gp_RollEnemyChance(enemy, scratch->field_20, 0) != 0) {
                scratch->field_2E = 0;
                scratch->field_24 = (s32)(scratch->field_24 * 4);
            }
            damageState = work->state;
            if ((damageState == 4) || (damageState == 0xB) || (damageState == 0x11) ||
                (damageState == 0x24)) {
                doubleDamage      = scratch->field_24 * 2;
                scratch->field_24 = doubleDamage;
                if (doubleDamage != 0) {
                    scratch->field_2E = 3;
                }
            }
            func_800E2C78(enemy, scratch->field_20, scratch->field_24, 0);
            effect = scratch->field_2E;
            if (effect != -1) {
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords + 2, (s32)(effect), 0);
            }
            scratch->field_24 = (s32)(scratch->field_24 * 2);
            enemy->hp         = (s16)((u16)enemy->hp - (u16)scratch->field_24);
            func_800DA6E8(&enemy->node, scratch->field_24, 0);
            totalDamage        = work->recentDamage + (u16)scratch->field_24;
            work->recentDamage = totalDamage;
            if (enemy->hp <= 0) {
                D_actor_421600_80151268 -= 1;
                if ((Gp_GetIdParam0(scratch->field_20) & 0xFFFF) == 4) {
                    nextDeathState = 8;
                    goto setDeathState;
                }
                deathState = work->state;
                if (deathState == 33 || deathState == 17 || deathState == 11 ||
                    deathState == 4) {
                    work->state     = 11;
                    work->prevState = -1;
                } else if (deathState == 7) {
                    work->state = 21;
                    deathSound  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010008;
                    deathPan    = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    SndEvt_EnqueueType6(deathSound, deathPan,
                                        (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                } else {
                    hurtSound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010008;
                    hurtPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    SndEvt_EnqueueType6(hurtSound, hurtPan,
                                        (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    nextDeathState = 20;
                setDeathState:
                    work->state = nextDeathState;
                }
                work->broadcast.context.loc.stage = 9;
                work->broadcast.context.loc.area  = 1;
                work->broadcast.command           = 3;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &work->broadcast, ACTOR_COMMAND_MESSAGE_APPLY);
            } else {
                if ((s16)totalDamage >= 0x47) {
                    hurtState = work->state;
                    if ((hurtState != 0x21) && (hurtState != 0x14) &&
                        (hurtState != 0x11) && (hurtState != 7) &&
                        (work->lastCommand.fields.command != 1)) {
                        soundBase   = 0x40010008;
                        work->state = 0x14;
                    } else {
                        goto normalHitSound;
                    }
                } else {
                normalHitSound:
                    soundBase = 0x40010007;
                }
                hitSound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundBase;
                hitPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(hitSound, hitPan,
                                    (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            debugMode = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
            if (debugMode == 1) {
                enemy->hp          = 0x64;
                work->blendAnimId  = 9;
                work->blendActive  = (s16)debugMode;
                work->blendRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
            }
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            scratch->field_24 = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags = (u8)(enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR);
            }
            enemy->hp  = (s16)((u16)enemy->hp - (u16)scratch->field_24);
            tickDamage = scratch->field_24;
            if (tickDamage != 0) {
                func_800DA6E8(&enemy->node, tickDamage, 0);
                if (enemy->hp <= 0) {
                    D_actor_421600_80151268 -= 1;
                    poisonState              = work->state;
                    if ((poisonState != 4) && (poisonState != 0xB) &&
                        (poisonState != 0x11)) {
                        work->state = 0xC;
                    } else {
                        work->state = 0x15;
                    }
                } else {
                    if (work->state == 0x1C) {
                        work->state = 0x26;
                    }
                    work->blendActive  = 1;
                    work->blendAnimId  = 0x12;
                    work->blendRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
                }
            }
        }
        SCRATCH_STACK_RELEASE_BYTES(0x30);
    }
}

static void func_actor_421600_80135F6C(Task* arg0)
{
    SVECTOR           offset;
    DesertChaserWork* work;
    TmdObject*        obj;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->animRate                                       = 0x10;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        if (work->animId == 0xD) {
            work->animRequest = DESERT_CHASER_ANIM_REQUEST_BLEND;
        } else {
            work->animRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
        }
        work->waistYawTarget = 0;
        work->lookYawTarget  = 0;
        desertChaserAnimTick(arg0);
        return;
    }
    desertChaserAnimTick(arg0);
    if ((work->rig.slots[1].status.fields.flags & 2) && (work->animId == 0xD)) {
        work->animId      = 1;
        work->animRequest = DESERT_CHASER_ANIM_REQUEST_BLEND;
    }
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if (work->animId == 0xF) {
            work->animRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
            work->animId      = 0x10;
        }
        desertChaserAnimTick(arg0);
    }
    if (work->animId == 0xE) {
        if ((u32)((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) - 8) < 2U) {
            offset.vz = 0;
            offset.vx = 0;
            offset.vy = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                Gp_SpawnEff(EFFECT_DUST_PUFF, arg0->extra.tmd->coords + 7, 0x80002300, &offset);
            }
        }
        if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 8) {
            offset.vz = 0;
            offset.vx = 0;
            offset.vy = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                Gp_SpawnEff(EFFECT_DUST_PUFF, arg0->extra.tmd->coords + 7, 0x80003400, &offset);
            }
        }
    }
}

static __inline__ s16 Actor421600_Zone(GfxCoord* coord)
{
    s32 x, z, ix, iz;
    x = coord->coord.t[0];
    z = coord->coord.t[2];
    if (x >= 0xD49)
        ix = 3;
    else if (x > 0)
        ix = 2;
    else
        ix = x >= -0xC7F;
    iz = 0;
    if (z < 0xBB9) {
        iz = 1;
        if (z <= 0) {
            iz = 3;
            if (z >= -0xBB7)
                iz = 2;
        }
    }
    return D_actor_421600_801511C0[ix | (iz * 4)];
}

static void func_actor_421600_80136138(Task* arg0)
{
    DesertChaserWork* work;
    ActorTurnScratch *head, *turn;
    Enemy*            ctx;
    TmdObject*        obj;
    GfxCoord *        coord2, *coord3, *coord4;
    s16               playerZone, zone;
    s16               nextZone;
    s16               angle;
    s32               wrapped;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->animRate                                       = 0x10;
        work->animId                                         = 0;
        work->animRequest                                    = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        desertChaserAnimTick(arg0);
        return;
    }
    playerZone = Actor421600_Zone(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords);
    zone       = Actor421600_Zone(arg0->extra.tmd->coords);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    if (playerZone == zone) {
        work->state = 0x26;
        return;
    }
    switch ((s16)(playerZone - 1)) {
        case 0:
        case 1:
            if (zone >= 1 && zone <= 3) {
                work->state = 0x26;
                return;
            }
            break;
        case 2:
            if (zone >= 1 && zone <= 6) {
                work->state = 0x26;
                return;
            }
            break;
        case 3:
        case 4:
            if (zone >= 3 && zone <= 6) {
                work->state = 0x26;
                return;
            }
            break;
        case 5:
            if (zone >= 3 && zone <= 9) {
                work->state = 0x26;
                return;
            }
            break;
        case 6:
        case 7:
            if (zone >= 6 && zone <= 9) {
                work->state = 0x26;
                return;
            }
            break;
        case 8:
            if (zone >= 6 && zone <= 11) {
                work->state = 0x26;
                return;
            }
            break;
        case 9:
        case 10:
            if (zone >= 9 && zone <= 11) {
                work->state = 0x26;
                return;
            }
            break;
        case 11:
            if (zone >= 0xB) {
                work->state = 0x26;
                return;
            }
            if (zone >= 0xC) {
                work->state = 0x26;
                return;
            }
            break;
    }
    desertChaserAnimTick(arg0);
    head = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn = head - 1;
    if (zone > playerZone)
        nextZone = zone - 1;
    else
        nextZone = zone + 1;
    head[-1].delta.vx = D_actor_421600_80151158[nextZone].vx;
    turn->delta.vy    = D_actor_421600_80151158[nextZone].vy;
    turn->delta.vz    = D_actor_421600_80151158[nextZone].vz;
    turn->delta.vx    = turn->delta.vx - (u16)arg0->extra.tmd->coords->coord.t[0];
    turn->delta.vy    = 0;
    turn->delta.vz    = turn->delta.vz - (u16)arg0->extra.tmd->coords->coord.t[2];
    coord2            = arg0->extra.tmd->coords;
    angle             = ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    wrapped             = angle;
    turn->angle         = wrapped;
    work->lookYawTarget = wrapped;
    if (turn->angle >= 0x21)
        turn->angle = 0x20;
    if (turn->angle < -0x20)
        turn->angle = -0x20;
    work->waistYawTarget = turn->angle;
    coord3               = arg0->extra.tmd->coords;
    turn->angle          = turn->angle + ratan2(-coord3->coord.m[2][0], coord3->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    if (work->blendActive == 0) {
        coord4 = arg0->extra.tmd->coords;
        actorMoveForward(coord4, 0x14);
    }
    ActorContact_Steer(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &turn->delta);
    func_actor_421600_80133334(arg0->extra.tmd->coords);
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Rebuild `coord`'s Y rotation from its current yaw (`ratan2` of
/// `-m[2][0], m[2][2]`), scaled by `y` on Y and left at 1.0 on X and Z, through
/// an `ActorScaleRotScratch` block borrowed from the scratchpad. Marks the coordinate dirty.
static __inline__ void Actor421600_ShrinkCoord(GfxCoord* coord, s16 y)
{
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    head                                       = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    blk                                        = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = blk;

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vx = 0x1000;
    blk->scale.vy = y;
    blk->scale.vz = 0x1000;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0] = (u16)(head - 1)->rotation.m[0][0];
    coord->coord.m[0][1] = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2] = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0] = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1] = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2] = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0] = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1] = (u16)blk->rotation.m[2][1];
    m22                  = (u16)blk->rotation.m[2][2];
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
}

/// Shrink tick: on the live-actor edge it drops the model's dirty flag, clears
/// the 0x4000 bit on the 0xB6C node, marks the enemy's list node and resets
/// `stateTimer` / `roomNotified`. Then it counts frames in `stateTimer` and, from frame
/// 0xB on, scales the model's coordinate Y by `0x1000 - (frame - 0xA) * 0x6B`
/// until that factor runs out at 0, through `Actor421600_ShrinkCoord`. The
/// frame counter also drives the light state: 1 sets modes 0 and 1, 20 (and
/// the fall-through from 1) sets mode 2, 38 sets `field_C` 0x80 and the
/// `state` 0x16. Counting stops at 0x401.
static void func_actor_421600_801366F4(Task* arg0)
{
    DesertChaserWork* work;
    Enemy*            ctx;
    TmdObject*        obj;
    s32               t;
    u16               tick;

    work = arg0->work;
    obj  = arg0->extra.tmd;
    ctx  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj->flags                                           = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        ctx->node.state.parts.flags                          = WORLD_TARGET_NOT_LOCKABLE;
        work->stateTimer                                     = 0;
        work->roomNotified                                   = 0;
    }
    if (work->stateTimer < 0x401) {
        tick             = work->stateTimer + 1;
        work->stateTimer = tick;
        switch ((s16)tick) {
            case 1:
                Gp_SetLightMode(ctx, ENEMY_COLOR_DEFAULT);
                Gp_SetLightMode(ctx, ENEMY_COLOR_WEIGHTED);
                /* fallthrough */
            case 20:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                Gp_SetLightMode(ctx, ENEMY_COLOR_BLACK);
                break;
            case 22:
                break;
            case 38:
                arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->state            = 0x16;
                break;
        }
        if (work->stateTimer >= 0xB) {
            t = (work->stateTimer - 10) * 0x6B;
            if (t < 0x1000) {
                Actor421600_ShrinkCoord(arg0->extra.tmd->coords, 0x1000 - t);
            } else {
                Actor421600_ShrinkCoord(arg0->extra.tmd->coords, 0);
            }
        }
    }
}

static void func_actor_421600_801369A0(Task* arg0)
{
    DesertChaserWork* work;
    Enemy*            ctx;
    Enemy*            found;
    s32               hi;
    s32               id;
    s32               stageAreaId;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        if (work->playerHeld == 1) {
            work->prevState  = -1;
            work->stateTimer = 0;
            return;
        }
        work->stateTimer = 0;
        do {
        } while (0);
        if (gSceneCombatState.battleRefs >= 2U) {
            Gp_ReleaseStateF0Add(arg0, 1);
        }
        if (D_actor_421600_80151268 <= 0) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
            work->roomNotified = 1;
            work->state        = 0;
            return;
        }
    }
    if (work->stateTimer < 0x80) {
        work->stateTimer = work->stateTimer + 1;
    }
    if (work->roomEventCountdown > 0) {
        work->roomEventCountdown = work->roomEventCountdown - 1;
    }
    if (work->lastCommand.fields.command != 2) {
        work->state = 0;
        return;
    }
    found = NULL;
    switch (ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
        case 0:
            hi    = gGameSession->location.loc.stage << 8;
            id    = gGameSession->location.loc.area | 0x1000;
            found = Gp_FindWorkById(id | hi);
            break;
        case 1:
            stageAreaId = (gGameSession->location.loc.stage << 8) | gGameSession->location.loc.area;
            found       = Gp_FindWorkById(stageAreaId);
            break;
    }
    if (found != NULL) {
        if (found->hp > 0) {
            if (D_actor_421600_80151268 == 1) {
                work->state = 0;
            }
        }
        if ((D_actor_421600_80151268 >= 2) || ((found->hp <= 0) && (D_actor_421600_80151268 == 1))) {
            switch (ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
                case 0:
                    arg0->extra.tmd->coords->coord.t[0]   = -0xD40;
                    arg0->extra.tmd->coords->coord.t[2]   = 0x104F;
                    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(arg0->extra.tmd->coords);
                    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x76C, 1);
                    Gp_SetLightMode(ctx, ENEMY_COLOR_DEFAULT);
                    ctx->reactionFlags = 0;
                    ctx->hp            = D_actor_421600_8013EF38.hpMax;
                    work->state        = 6;
                    break;
                case 1:
                    arg0->extra.tmd->coords->coord.t[0]   = 0x138C;
                    arg0->extra.tmd->coords->coord.t[2]   = 0x4B2;
                    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(arg0->extra.tmd->coords);
                    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x7BC, 1);
                    Gp_SetLightMode(ctx, ENEMY_COLOR_DEFAULT);
                    ctx->reactionFlags = 0;
                    ctx->hp            = D_actor_421600_8013EF38.hpMax;
                    work->state        = 6;
                    break;
            }
        }
    }
}

#include "../../shared/desert_chaser_approach.inc.c"

#include "../../shared/desert_chaser_pursue.inc.c"

#include "../../shared/desert_chaser_spawn_aim.inc.c"

#include "../../shared/desert_chaser_strike.inc.c"

/// Aim tick: on the live-actor edge it re-arms the model the way
/// `desertChaserSpawnAim` does -- buffers reallocated, clip 0x10,
/// `animId` 6, the 0xB6C node's 0x4000 flag up -- with `stateCounter` and the
/// 0xCD8 offset it owns reseeded, then, while `stateTimer` is inside 9..0x18 and
/// `stateCounter` below 5, walks the 0xB8C table and counts a retry for every hit.
/// The 0xCE4 records decide which way the model is aimed: one carrying the
/// 0x100000 kind turns it by `-0x55`, none by `-0xC8`, through
/// `actorMoveForward`. Outside that frame window, and in both aim arms,
/// the 0xB8C walk is what runs.
static void func_actor_421600_80138D24(Task* arg0)
{
    DesertChaserWork* work;
    Enemy*            ctx;
    TmdObject*        obj;
    s16               found;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx                         = arg0->spawnArg2.pointer;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = 0x10;
        work->blendActive                                     = 0;
        work->animId                                          = 6;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        desertChaserAnimTick(arg0);
        work->stateTimer                 = 0;
        work->stateCounter               = 0;
        work->wallProbe.shape.ends[1].vz = -0x320;
    }
    work->stateTimer++;
    desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = 2;
    }
    if (((u32)((u16)work->stateTimer - 9) < 0x10) && (work->stateCounter < 5)) {
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) != 0) {
            work->stateCounter++;
        }
        found = desertChaserCapsuleTouchesGrid(arg0);
        if (found != 0) {
            actorMoveForward(arg0->extra.tmd->coords, -0x55);
        } else {
            actorMoveForward(arg0->extra.tmd->coords, -0xC8);
        }
    } else {
        ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Re-arms the model the way `desertChaserSpawnAim` does -- buffers
/// reallocated, clip 0x10, `animId` 2, the 0xB6C node's 0x4000 flag up --
/// then walks the 0xB8C `WorldCollisionContact` table through `ActorContact_PushContact`.
/// Takes two `SVECTOR`s off the scratch stack and fills the XZ offset of the
/// model coordinate from `gPlayerStatus.coordMtx` (the player's coordinate matrix),
/// forms the yaw difference against the model's own facing (row 2 of its
/// matrix), wraps it into `[-0x800, 0x800]` into `lookYawTarget` and re-aims the
/// coordinate with `gfxRotMatrixY`. Ends by writing the view index into
/// `state` on the two view transitions.
///
/// The coordinate is read twice into two locals: `coord` only feeds the offset
/// and dies before the first `ratan2`, while `coord2` is live across it, so GCC
/// 2.8.1 keeps them in a caller-saved and a callee-saved register respectively.
/// One local assigned twice is one pseudo with one live range and costs a sixth
/// saved register.
static void func_actor_421600_8013903C(Task* arg0)
{
    DesertChaserWork* work;
    Enemy*            ctx;
    TmdObject*        obj;
    SVECTOR*          head;
    SVECTOR*          vec;
    GfxCoord*         coord;
    GfxCoord*         coord2;
    s16               angle;
    s32               view;

    head                           = SCRATCH_STACK_CURSOR(SVECTOR);
    SCRATCH_STACK_CURSOR(SVECTOR) -= 2;
    vec                            = head - 2;
    work                           = arg0->work;
    ctx                            = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = 0x10;
        work->blendActive                                     = 0;
        work->animId                                          = 2;
        work->waistYawTarget                                  = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        desertChaserAnimTick(arg0);
        work->stateTimer = 0;
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = arg0->extra.tmd->coords;
    head[-2].vx                           = (u16)gPlayerStatus.coordMtx->t[0] - (u16)coord->coord.t[0];
    vec->vy                               = (u16)gPlayerStatus.coordMtx->t[1] - (u16)coord->coord.t[1];
    vec->vz                               = (u16)gPlayerStatus.coordMtx->t[2] - (u16)coord->coord.t[2];
    coord2                                = arg0->extra.tmd->coords;
    angle                                 = ratan2(head[-2].vx, vec->vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    work->lookYawTarget = angle;
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, (s16)ratan2(vec->vx, vec->vz), 1);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    desertChaserAnimTick(arg0);
    if (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
        view = Gp_GetViewIndex() & 0xFF;
        if (view == 3) {
            work->state = view;
        }
    }
    if ((((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 1) && ((Gp_GetViewIndex() & 0xFF) == 8)) {
        work->state = 3;
    }
    SCRATCH_STACK_CURSOR(SVECTOR) += 2;
}

#include "../../shared/desert_chaser_steer.inc.c"

/// Death / respawn tick: re-arms the model buffers and the 0x828 motion block,
/// fires the 0x40010009 spawn sound and the 0x40010007 tick sound (draining
/// `hp` by 0xF and flooring it at 1), then walks the two `WorldCollisionContact`
/// movement tables. While the last command received is 2 the actor is held
/// in the arena by clamping X -- and Z only when X was already inside -- and
/// otherwise `func_actor_421600_80133334` drags it back. Picks the state
/// `state` from `hp` and the buildup bit of `reactionFlags`.
static void func_actor_421600_8013947C(Task* arg0)
{
    DesertChaserWork* work;
    Enemy*            ctx;
    GfxCoord*         coord;
    TmdObject*        obj;
    s32               sound;
    s32               pan;
    s32               eventSound;
    s32               eventPan;
    s32               x;
    s32               z;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRate                                        = 0x10;
        work->animId                                          = 0xA;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->blendActive                                     = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        desertChaserAnimTick(arg0);
        sound = (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010009;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        ctx->hp -= 0xF;
        func_800DA6E8(&ctx->node, 0xF, 0);
        if (ctx->hp <= 0) {
            ctx->hp = 1;
        }
        eventSound = (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010007;
        eventPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(eventSound, eventPan,
                            (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts));
    if (work->lastCommand.fields.command == 2) {
        coord = arg0->extra.tmd->coords;
        x     = coord->coord.t[0];
        if (x > 0) {
            if (x >= 0xBEB) {
                coord->coord.t[0] = 0xB54;
            } else {
                goto block_10;
            }
        } else if (x < -0xB22) {
            coord->coord.t[0] = -0xA8C;
        } else {
        block_10:
            z = coord->coord.t[2];
            if (z > 0) {
                if (z >= 0xB23) {
                    coord->coord.t[2] = 0xA8C;
                }
            } else if (z < -0xB22) {
                coord->coord.t[2] = -0xA8C;
            }
        }
    } else {
        func_actor_421600_80133334(arg0->extra.tmd->coords);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if (ctx->hp > 0) {
            if (ctx->reactionFlags & ENEMY_REACTION_BUILDUP) {
                work->state = 4;
            } else {
                work->state = 0x11;
            }
        } else {
            work->state = 0x15;
        }
    }
}

#include "../../shared/desert_chaser_roam.inc.c"

static void func_actor_421600_8013A404(Task* arg0)
{
    DesertChaserWork* temp_s0;
    GfxCoord*         temp_v0_2;
    s32               temp_a0;
    s32               temp_a1;
    s32               var_a0;
    s32               var_v1;
    u32               temp_v0;
    u8                temp_v1;

    temp_s0 = arg0->work;
    if (temp_s0->stateEntered != 0) {
        temp_v0             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState     = temp_v0;
        temp_s0->stateTimer = temp_s0->downFramesBase + ((temp_v0 >> 0x10) & 0xF);
    }
    temp_s0->stateTimer -= 1;
    desertChaserAnimTick(arg0);
    if ((s16)temp_s0->stateTimer < 0) {
        temp_v1 = temp_s0->lastCommand.fields.command;
        if ((temp_v1 == 1) || (temp_v1 == 3)) {
            temp_s0->state = 5;
        } else if (temp_v1 == 2) {
            temp_v0_2 = arg0->extra.tmd->coords;
            temp_a0   = temp_v0_2->coord.t[0];
            temp_a1   = temp_v0_2->coord.t[2];
            if (temp_a0 >= 0xD49) {
                var_a0 = 3;
            } else if (temp_a0 > 0) {
                var_a0 = 2;
            } else {
                var_a0 = temp_a0 >= -0xC7F;
            }
            var_v1 = 0;
            if (temp_a1 < 0xBB9) {
                var_v1 = 1;
                if (temp_a1 <= 0) {
                    var_v1 = 3;
                    if (temp_a1 >= -0xBB7) {
                        var_v1 = 2;
                    }
                }
            }
            if (D_actor_421600_801511C0[var_a0 | (var_v1 * 4)] >= 0xB) {
                temp_s0->state = 0x24;
            } else {
                temp_s0->state = 6;
            }
        } else {
            temp_s0->state = 0x24;
        }
    }
}

static void func_actor_421600_8013A554(Task* arg0)
{
    SVECTOR                   effect;
    s16                       aimZ;
    s16                       fallbackZ;
    s32                       fallbackAngle;
    s16                       fallbackDelta;
    s32                       moveAngle;
    s16                       moveDelta;
    s32                       playerX;
    s32                       facingAngle;
    s16                       facingDelta;
    s32                       aimAngle;
    s16                       aimDelta;
    s16                       targetZ;
    s16                       yaw;
    s16                       nextState;
    GfxCoord*                 targetCoord;
    GfxCoord*                 aimCoord;
    GfxCoord*                 fallbackCoord;
    GfxCoord*                 fallbackFacing;
    GfxCoord*                 moveCoord;
    GfxCoord*                 facingCoord;
    GfxCoord*                 aimFacing;
    GfxCoord*                 stepCoord;
    GfxCoord*                 coord;
    s32                       sound;
    s32                       spawnEffect;
    s32                       effectFlags;
    s32                       part;
    s32                       fallbackYaw;
    s32                       distance;
    s32                       closeDistance;
    s32                       farDistance;
    s32                       pan;
    TmdObject*                obj;
    Enemy*                    ctx;
    Task*                     player;
    DesertChaserWork*         work;
    Enemy*                    enemy;
    Actor421600AttackScratch* head;
    Actor421600AttackScratch* scratch;

    work   = arg0->work;
    enemy  = arg0->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx                         = arg0->spawnArg2.pointer;
        ctx->node.state.parts.flags = 0;
        Gp_ArmStateF0(1);
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = 0x10;
        work->blendActive                                     = 0;
        work->animId                                          = 3;
        work->waistYawTarget                                  = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags   = (u16)(work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        desertChaserAnimTick(arg0);
        work->stateTimer    = 0;
        work->stateCounter  = 0;
        work->lungeDistance = 0;
        sound               = (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010006;
        pan                 = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        return;
    }
    head          = SCRATCH_STACK_CURSOR(Actor421600AttackScratch);
    scratch       = (SCRATCH_STACK_CURSOR(Actor421600AttackScratch) = head - 1);
    coord         = arg0->extra.tmd->coords;
    scratch->zone = Actor421600_Zone(coord);
    if ((ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) != 0) && (work->stateTimer >= 0xB)) {
        work->state = 5;
    }
    if ((ActorContact_Steer(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &scratch->vec) << 0x10) != 0 && work->animId == 3) {
        work->playerButtonHold.pressCount = 8;
        if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
            playerX            = -gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][0];
            scratch->playerYaw = ratan2(playerX, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][2]);
            targetCoord        = arg0->extra.tmd->coords;
            scratch->vec.vx    = (s16)(gPlayerStatus.coordMtx->t[0] - targetCoord->coord.t[0]);
            scratch->vec.vy    = (s16)(gPlayerStatus.coordMtx->t[1] - targetCoord->coord.t[1]);
            targetZ            = gPlayerStatus.coordMtx->t[2] - targetCoord->coord.t[2];
            scratch->vec.vz    = targetZ;
            yaw                = ratan2(scratch->vec.vx, targetZ) + 0x800;
            scratch->yaw       = yaw;
            scratch->yaw       = actorNormalizeYaw(yaw);
            facingCoord        = arg0->extra.tmd->coords;
            facingAngle        = ratan2(scratch->vec.vx, scratch->vec.vz);
            facingDelta        = facingAngle - ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
            scratch->delta     = actorNormalizeYaw(facingDelta);
            distance           = scratch->yaw - scratch->playerYaw;
            distance           = abs(distance);
            if (distance < 0x400) {
                work->playerAnim.source.sets = gDesertChaserFrontAnim;
            } else {
                work->playerAnim.source.sets = gDesertChaserRearAnim;
                scratch->yaw                 = (s16)((u16)scratch->yaw + 0x800);
            }
            work->playerPlacement.rot.vx = 0;
            work->playerPlacement.rot.vy = (u16)scratch->yaw;
            work->playerPlacement.rot.vz = 0;
            work->playerPlacement.pos.vx = (s32)player->extra.tmd->coords->coord.t[0];
            work->playerPlacement.pos.vy = (s32)player->extra.tmd->coords->coord.t[1];
            work->playerPlacement.pos.vz = (s32)player->extra.tmd->coords->coord.t[2];
            TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
            if (work->lungeDistance < 0x3E8) {
                if (enemy->hp > 0) {
                    closeDistance = scratch->yaw - scratch->playerYaw;
                    closeDistance = abs(closeDistance);
                    if (closeDistance < 0x400) {
                        scratch->reply = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 2), 0);
                    } else {
                        scratch->reply = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 3), 0);
                    }
                }
                if (scratch->reply != 1) {
                    work->playerAnim.animationId         = 3;
                    work->playerAnim.blend               = ANIMATION_BLEND_RESET;
                    work->playerAnim.blendFrames         = 0;
                    work->playerMove.displacement.vx     = 0;
                    work->playerMove.displacement.vy     = 0;
                    work->playerMove.displacement.vz     = 0;
                    work->playerMove.collisionRequests   = GAME_ACTOR_COLLISION_REQUEST_MASK;
                    work->playerMove.keepControl         = 1;
                    work->playerHeld                     = 1;
                    work->lastCommand.fields.catchFrames = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                }
                nextState = 0x25;
            } else {
                if (enemy->hp > 0) {
                    farDistance = scratch->yaw - scratch->playerYaw;
                    farDistance = abs(farDistance);
                    if (farDistance < 0x400) {
                        scratch->reply = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 0), 0);
                    } else {
                        scratch->reply = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 1), 0);
                    }
                }
                if (scratch->reply == 1) {
                    ((GameActor*)player->work)->state = 0xA;
                }
                work->playerAnim.animationId         = 1;
                work->playerAnim.blend               = ANIMATION_BLEND_RESET;
                work->playerAnim.blendFrames         = 0;
                work->playerMove.displacement.vx     = 0;
                work->playerMove.displacement.vy     = 0;
                work->playerMove.displacement.vz     = 0;
                work->playerMove.collisionRequests   = GAME_ACTOR_COLLISION_REQUEST_MASK;
                work->playerMove.keepControl         = 1;
                work->playerHeld                     = 1;
                work->lastCommand.fields.catchFrames = 0;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                nextState = 0x1E;
            }
            work->state = nextState;
        }
        aimCoord        = arg0->extra.tmd->coords;
        scratch->vec.vx = (s16)(gPlayerStatus.coordMtx->t[0] - aimCoord->coord.t[0]);
        scratch->vec.vy = (s16)(gPlayerStatus.coordMtx->t[1] - aimCoord->coord.t[1]);
        aimZ            = gPlayerStatus.coordMtx->t[2] - aimCoord->coord.t[2];
        scratch->vec.vz = aimZ;
        aimFacing       = arg0->extra.tmd->coords;
        aimAngle        = ratan2(scratch->vec.vx, aimZ);
        aimDelta        = aimAngle - ratan2(-aimFacing->coord.m[2][0], aimFacing->coord.m[2][2]);
        scratch->aim    = actorNormalizeYaw(aimDelta);
    } else {
        fallbackCoord   = arg0->extra.tmd->coords;
        scratch->vec.vx = (s16)(gPlayerStatus.coordMtx->t[0] - fallbackCoord->coord.t[0]);
        scratch->vec.vy = (s16)(gPlayerStatus.coordMtx->t[1] - fallbackCoord->coord.t[1]);
        fallbackZ       = gPlayerStatus.coordMtx->t[2] - fallbackCoord->coord.t[2];
        scratch->vec.vz = fallbackZ;
        fallbackFacing  = arg0->extra.tmd->coords;
        fallbackAngle   = ratan2(scratch->vec.vx, fallbackZ);
        fallbackDelta   = fallbackAngle - ratan2(-fallbackFacing->coord.m[2][0], fallbackFacing->coord.m[2][2]);
        fallbackYaw     = actorNormalizeYaw(fallbackDelta);
        scratch->aim    = (s16)fallbackYaw;
        fallbackYaw     = abs(fallbackYaw);
        if (fallbackYaw >= 0x601) {
            work->state = 0x1D;
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    moveCoord                             = arg0->extra.tmd->coords;
    moveAngle                             = ratan2(work->playerDelta.vx, work->playerDelta.vz);
    moveDelta                             = moveAngle - ratan2(-moveCoord->coord.m[2][0], moveCoord->coord.m[2][2]);
    scratch->delta                        = actorNormalizeYaw(moveDelta);
    desertChaserAnimTick(arg0);
    stepCoord = arg0->extra.tmd->coords;
    actorMoveForward(stepCoord, 200);
    work->lungeDistance = (s16)((u16)work->lungeDistance + 0xC8);
    if (work->animId == 3) {
        switch (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) {
            case 5:
                spawnEffect = 1;
                part        = 7;
                effectFlags = 0x4300;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 700;
                break;
            case 8:
                spawnEffect = 1;
                part        = 9;
                effectFlags = 0x3500;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 700;
                break;
            case 10:
                spawnEffect = 1;
                part        = 14;
                effectFlags = 0x5A00;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 600;
                break;
            case 13:
                spawnEffect = 1;
                part        = 17;
                effectFlags = 0x4800;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 600;
                break;
            default:
                effectFlags = 0;
                spawnEffect = 0;
                part        = 0;
                break;
        }
        if ((gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) && (spawnEffect == 1)) {
            Gp_SpawnEff(EFFECT_DUST_PUFF, arg0->extra.tmd->coords + part, effectFlags | 0x80000000, &effect);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor421600AttackScratch);
}

static void func_actor_421600_8013B00C(Task* arg0)
{
    DesertChaserWork* work;
    ActorTurnScratch* head;
    ActorTurnScratch* turn;
    Enemy*            ctx;
    TmdObject*        obj;
    GfxCoord*         coord;
    GfxCoord*         coord2;
    GfxCoord*         coord3;
    GfxCoord*         coord4;
    s32               zone;
    s32               x_entry;
    s32               z_entry;
    s32               var_a0_entry;
    s32               var_v1_entry;
    Task*             task;
    GfxCoord*         playerCoord;
    s32               x;
    s32               z;
    s16               angle;
    s32               wrapped;
    s32               var_a0;
    s32               var_v1;

    work = arg0->work;
    task = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx                         = arg0->spawnArg2.pointer;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->animRate                                       = 0x10;
        work->animId                                         = 3;
        work->animRequest                                    = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        desertChaserAnimTick(arg0);
        playerCoord = task->extra.tmd->coords;
        x_entry     = playerCoord->coord.t[0];
        z_entry     = playerCoord->coord.t[2];
        if (x_entry >= 0xD49) {
            var_a0_entry = 3;
        } else if (x_entry > 0) {
            var_a0_entry = 2;
        } else {
            var_a0_entry = x_entry >= -0xC7F;
        }
        var_v1_entry = 0;
        if (z_entry < 0xBB9) {
            var_v1_entry = 1;
            if (z_entry <= 0) {
                var_v1_entry = 3;
                if (z_entry >= -0xBB7) {
                    var_v1_entry = 2;
                }
            }
        }
        if (D_actor_421600_801511C0[var_a0_entry | (var_v1_entry * 4)] >= 7) {
            work->fleeZone = 1;
            return;
        }
        work->fleeZone = 0xB;
        return;
    }
    coord = arg0->extra.tmd->coords;
    x     = coord->coord.t[0];
    z     = coord->coord.t[2];
    if (x >= 0xD49) {
        var_a0 = 3;
    } else if (x > 0) {
        var_a0 = 2;
    } else {
        var_a0 = x >= -0xC7F;
    }
    var_v1 = 0;
    if (z < 0xBB9) {
        var_v1 = 1;
        if (z <= 0) {
            var_v1 = 3;
            if (z >= -0xBB7) {
                var_v1 = 2;
            }
        }
    }
    zone = D_actor_421600_801511C0[var_a0 | (var_v1 * 4)];
    if ((s16)zone == work->fleeZone) {
        work->state = 0;
        return;
    }
    head = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn = head - 1;
    if ((s16)zone > work->fleeZone) {
        head[-1].delta.vx = D_actor_421600_80151158[zone - 1].vx;
        turn->delta.vy    = D_actor_421600_80151158[zone - 1].vy;
        turn->delta.vz    = D_actor_421600_80151158[zone - 1].vz;
    } else {
        head[-1].delta.vx = D_actor_421600_80151158[zone + 1].vx;
        turn->delta.vy    = D_actor_421600_80151158[zone + 1].vy;
        turn->delta.vz    = D_actor_421600_80151158[zone + 1].vz;
    }
    turn->delta.vx = turn->delta.vx - (u16)arg0->extra.tmd->coords->coord.t[0];
    turn->delta.vy = 0;
    turn->delta.vz = turn->delta.vz - (u16)arg0->extra.tmd->coords->coord.t[2];
    desertChaserAnimTick(arg0);
    coord2 = arg0->extra.tmd->coords;
    angle  = ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    wrapped             = angle;
    turn->angle         = wrapped;
    work->lookYawTarget = wrapped;
    if (turn->angle >= 0x81) {
        turn->angle = 0x80;
    }
    if (turn->angle < -0x80) {
        turn->angle = -0x80;
    }
    work->waistYawTarget = turn->angle;
    coord3               = arg0->extra.tmd->coords;
    turn->angle          = turn->angle + ratan2(-coord3->coord.m[2][0], coord3->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    if (work->blendActive == 0) {
        coord4 = arg0->extra.tmd->coords;
        actorMoveForward(coord4, 0xC8);
    }
    ActorContact_Steer(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &turn->delta);
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Zone-aim tick: the live-actor edge re-arms the model the way
/// `func_actor_421600_80138D24` does -- buffers reallocated, clip 0x10, pose 3,
/// motion 1, the 0xB6C node's 0x4000 flag up.
///
/// Otherwise the X and Z of the actor's coordinate are bucketed into the 4x4
/// zone table `D_actor_421600_801511C0` exactly as `func_actor_421600_8013A404`
/// does, and zone 5 abandons the tick into state 7. Any other zone picks the
/// neighbouring entry of the 8-byte pose table `D_actor_421600_80151158` --
/// `zone - 1` above the table's midpoint `mode`, `zone + 1` at or below it --
/// and copies all three halfwords into a 0xC block taken off the scratch stack,
/// which becomes the XZ direction from the actor to that pose.
///
/// `mode` and the `(s8)` casts on `zone` are load-bearing, and so is the
/// `turn->delta.vy = 0` between the two coordinate subtractions. A plain `5`
/// literal lets expand fold `zone > 5` into `zone < 6`, which drops the two
/// register copies and the `slt` the ROM has; keeping the limit in a
/// declaration-initialised `s8` leaves it a register operand so the fold never
/// runs. The midpoint store then lands in the load-delay slot the subtractions
/// leave open.
static void func_actor_421600_8013B4C4(Task* arg0)
{
    DesertChaserWork* work;
    ActorTurnScratch* head;
    ActorTurnScratch* turn;
    Enemy*            ctx;
    TmdObject*        obj;
    GfxCoord*         coord;
    GfxCoord*         coord2;
    GfxCoord*         coord3;
    GfxCoord*         coord4;
    s32               zone;
    s8                mode = 5;
    s32               v;
    s32               x;
    s32               z;
    s16               angle;
    s32               wrapped;
    s32               var_a0;
    s32               var_v1;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx                         = arg0->spawnArg2.pointer;
        ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->animRate                                       = 0x10;
        work->animId                                         = 3;
        work->animRequest                                    = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        desertChaserAnimTick(arg0);
        return;
    }
    coord = arg0->extra.tmd->coords;
    x     = coord->coord.t[0];
    z     = coord->coord.t[2];
    if (x >= 0xD49) {
        var_a0 = 3;
    } else if (x > 0) {
        var_a0 = 2;
    } else {
        var_a0 = x >= -0xC7F;
    }
    var_v1 = 0;
    if (z < 0xBB9) {
        var_v1 = 1;
        if (z <= 0) {
            var_v1 = 3;
            if (z >= -0xBB7) {
                var_v1 = 2;
            }
        }
    }
    zone = D_actor_421600_801511C0[var_a0 | (var_v1 * 4)];
    if ((s8)zone == mode) {
        work->state = 7;
        return;
    }
    head = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn = head - 1;
    if ((s8)zone > mode) {
        head[-1].delta.vx = D_actor_421600_80151158[zone - 1].vx;
        turn->delta.vy    = D_actor_421600_80151158[zone - 1].vy;
        turn->delta.vz    = D_actor_421600_80151158[zone - 1].vz;
    } else {
        head[-1].delta.vx = D_actor_421600_80151158[zone + 1].vx;
        turn->delta.vy    = D_actor_421600_80151158[zone + 1].vy;
        turn->delta.vz    = D_actor_421600_80151158[zone + 1].vz;
    }
    turn->delta.vx = turn->delta.vx - (u16)arg0->extra.tmd->coords->coord.t[0];
    turn->delta.vy = 0;
    turn->delta.vz = turn->delta.vz - (u16)arg0->extra.tmd->coords->coord.t[2];
    desertChaserAnimTick(arg0);
    coord2 = arg0->extra.tmd->coords;
    angle  = ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    wrapped             = angle;
    turn->angle         = wrapped;
    work->lookYawTarget = wrapped;
    if (turn->angle >= 0x81) {
        turn->angle = 0x80;
    }
    if (turn->angle < -0x80) {
        turn->angle = -0x80;
    }
    work->waistYawTarget = turn->angle;
    coord3               = arg0->extra.tmd->coords;
    turn->angle          = turn->angle + ratan2(-coord3->coord.m[2][0], coord3->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    if (work->blendActive == 0) {
        coord4 = arg0->extra.tmd->coords;
        actorMoveForward(coord4, 0xC8);
    }
    ActorContact_Steer(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &turn->delta);
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_421600_8013B8E0(Task* arg0)
{
    DesertChaserWork* temp_s1;
    TmdObject*        temp_a0;

    temp_s1 = arg0->work;
    if (temp_s1->stateEntered != 0) {
        temp_a0                                                   = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        temp_a0->flags                                            = 0;
        Tmd_AllocBuffers(temp_a0);
        temp_s1->animRate                                       = 0x10;
        temp_s1->animId                                         = 0x11;
        temp_s1->animRequest                                    = DESERT_CHASER_ANIM_REQUEST_RESET;
        temp_s1->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        arg0->extra.tmd->coords->coord.t[0]                     = 0;
        arg0->extra.tmd->coords->coord.t[1]                     = 0;
        arg0->extra.tmd->coords->coord.t[2]                     = 0;
        arg0->extra.tmd->coords->composeStamp                   = GRAPHICS_COORD_DIRTY;
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0, 1);
        desertChaserAnimTick(arg0);
    }
    desertChaserAnimTick(arg0);
    if (temp_s1->rig.slots[1].status.fields.flags & 0x100) {
        arg0->extra.tmd->coords->coord.t[0]   = -0x334;
        arg0->extra.tmd->coords->coord.t[1]   = 0;
        arg0->extra.tmd->coords->coord.t[2]   = -0x4C4;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x400, 1);
        temp_s1->animRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
        temp_s1->animId      = 0;
        desertChaserAnimTick(arg0);
        desertChaserAnimTick(arg0);
        temp_s1->state = 0x27;
    }
}

static __inline__ s32 Actor421600_RouteZone(s32 x, s32 z)
{
    s32 ix = x > 0;
    s32 iz = z < 1;
    return D_actor_421600_801511D0[ix + (iz * 2)];
}

static void func_actor_421600_8013BA70(Task* arg0)
{
    s32                      radius = 0x5DC;
    DesertChaserWork*        work;
    Enemy*                   enemy;
    GfxCoord*                zoneCoord;
    GfxCoord*                clampCoord;
    s32                      x, zClamp;
    WorldCollisionContact*   record;
    GfxCoord*                coord;
    GfxCoord*                coord2;
    GfxCoord*                coord3;
    GfxCoord*                facing3;
    GfxCoord*                facing4;
    GfxCoord*                facing5;
    GfxCoord*                facing;
    GfxCoord*                facing2;
    GfxCoord*                turnCoord;
    Actor421600RouteScratch* scratch;
    SVECTOR*                 target;
    SVECTOR*                 target2;
    Actor421600RouteScratch* head;
    Actor421600RouteScratch* head2;
    TmdObject*               obj;
    s16                      targetDelta;
    s16                      delta;
    s16                      yaw;
    s16                      delta3;
    s16                      delta4;
    s16                      delta5;
    s32                      playerX;
    s16                      delta1;
    s16                      delta2;
    s16                      targetYaw;
    s16                      z;
    s32                      magnitude;
    s32                      targetMagnitude;
    s16                      adjustedDelta;
    s32                      originalMagnitude;
    s16                      wrapped;
    s16                      wrapped2;
    s16                      wrapped3;
    s16                      wrapped4;
    s16                      wrapped5;
    s16                      wrappedYaw;
    s32                      angle3;
    s32                      angle4;
    s32                      angle5;
    s32                      angle;
    s32                      angle2;
    s32                      finalYaw;
    s32                      turnDelta;
    s32                      finalDelta;
    s32                      yawDifference;
    u16                      unsignedDelta;
    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        head                          = SCRATCH_STACK_CURSOR(Actor421600RouteScratch);
        obj                           = arg0->extra.tmd;
        scratch                       = (SCRATCH_STACK_CURSOR(Actor421600RouteScratch) = head - 1);
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = 0x10;
        work->blendActive                                     = 0;
        work->animId                                          = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;

        desertChaserAnimTick(arg0);
        desertChaserAnimTick(arg0);
        work->stateTimer   = 0;
        work->stateCounter = 0;
        zoneCoord          = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        scratch->zone      = Actor421600_RouteZone(zoneCoord->coord.t[0], zoneCoord->coord.t[2]);
        coord              = arg0->extra.tmd->coords;
        head[-1].vec.vx    = (s16)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
        scratch->vec.vy    = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
        z                  = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
        scratch->vec.vz    = z;
        facing             = arg0->extra.tmd->coords;
        angle              = ratan2((s32)head[-1].vec.vx, (s32)z);
        delta1             = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
        wrapped            = delta1;
        if (delta1 < 0) {
        wrapNegative:
            if (wrapped < -0x800) {
                wrapped += 0x1000;
                goto wrapNegative;
            }
        } else {
        wrapPositive:
            if (wrapped >= 0x801) {
                wrapped -= 0x1000;
                goto wrapPositive;
            }
        }
        work->lookYawTarget = wrapped;
        work->patrolTarget  = 0;
        if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
            work->patrolPoints[0].x = D_actor_421600_801511D4[scratch->zone][0];
            work->patrolPoints[0].z = D_actor_421600_801511D4[scratch->zone][1];
            work->patrolPoints[1].x = D_actor_421600_801511D4[scratch->zone][2];
            work->patrolPoints[1].z = D_actor_421600_801511D4[scratch->zone][3];
        } else {
            work->patrolPoints[0].x = D_actor_421600_801511D4[scratch->zone][4];
            work->patrolPoints[0].z = D_actor_421600_801511D4[scratch->zone][5];
            work->patrolPoints[1].x = D_actor_421600_801511D4[scratch->zone][6];
            work->patrolPoints[1].z = D_actor_421600_801511D4[scratch->zone][7];
        }
        SCRATCH_STACK_RELEASE_BLOCK(Actor421600RouteScratch);
        work->wallProbe.shape.ends[1].vz = 0x26C;
        return;
    }
    work->stateCounter += 1;
    head2               = SCRATCH_STACK_CURSOR(Actor421600RouteScratch);
    scratch             = (SCRATCH_STACK_CURSOR(Actor421600RouteScratch) = head2 - 1);
    head2[-1].vec.vx    = (s16)(work->patrolPoints[work->patrolTarget].x - arg0->extra.tmd->coords->coord.t[0]);
    scratch->vec.vy     = 0;
    scratch->vec.vz     = work->patrolPoints[work->patrolTarget].z - arg0->extra.tmd->coords->coord.t[2];
    coord2              = arg0->extra.tmd->coords;
    head2[-1].target.vx = (s16)(gPlayerStatus.coordMtx->t[0] - coord2->coord.t[0]);
    target              = &head2[-1].target;
    target->vy          = gPlayerStatus.coordMtx->t[1] - coord2->coord.t[1];
    target->vz          = gPlayerStatus.coordMtx->t[2] - coord2->coord.t[2];
    if (!actorOutsideRadius(&scratch->vec, 0xA0) || work->stateTimer >= 0x15) {
        facing2  = arg0->extra.tmd->coords;
        angle2   = ratan2((s32)head2[-1].target.vx, (s32)target->vz);
        delta2   = angle2 - ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
        wrapped2 = delta2;
        if (delta2 < 0) {
        wrapNegative2:
            if (wrapped2 < -0x800) {
                wrapped2 += 0x1000;
                goto wrapNegative2;
            }
        } else {
        wrapPositive2:
            if (wrapped2 >= 0x801) {
                wrapped2 -= 0x1000;
                goto wrapPositive2;
            }
        }
        work->lookYawTarget = wrapped2;
        if (work->patrolTarget == 0) {
            gfxRotMatrixY(&scratch->matrix, (s16)ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) - 0x2EE, 1);
            work->patrolTarget = 1;
        } else {
            gfxRotMatrixY(&scratch->matrix, (s16)ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) + 0x2EE, 1);
            work->patrolTarget = 0;
        }
        zoneCoord     = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        scratch->zone = Actor421600_RouteZone(zoneCoord->coord.t[0], zoneCoord->coord.t[2]);
        if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
            work->patrolPoints[0].x = D_actor_421600_801511D4[scratch->zone][0];
            work->patrolPoints[0].z = D_actor_421600_801511D4[scratch->zone][1];
            work->patrolPoints[1].x = D_actor_421600_801511D4[scratch->zone][2];
            work->patrolPoints[1].z = D_actor_421600_801511D4[scratch->zone][3];
        } else {
            work->patrolPoints[0].x = D_actor_421600_801511D4[scratch->zone][4];
            work->patrolPoints[0].z = D_actor_421600_801511D4[scratch->zone][5];
            work->patrolPoints[1].x = D_actor_421600_801511D4[scratch->zone][6];
            work->patrolPoints[1].z = D_actor_421600_801511D4[scratch->zone][7];
        }
        work->stateTimer = 0;
    }
    desertChaserAnimTick(arg0);
    facing3  = arg0->extra.tmd->coords;
    angle3   = ratan2((s32)scratch->target.vx, (s32)scratch->target.vz);
    delta3   = angle3 - ratan2((s32)-facing3->coord.m[2][0], (s32)facing3->coord.m[2][2]);
    wrapped3 = delta3;
    if (delta3 < 0) {
    wrapNegative3:
        if (wrapped3 < -0x800) {
            wrapped3 += 0x1000;
            goto wrapNegative3;
        }
    } else {
    wrapPositive3:
        if (wrapped3 >= 0x801) {
            wrapped3 -= 0x1000;
            goto wrapPositive3;
        }
    }
    work->lookYawTarget = wrapped3;
    facing4             = arg0->extra.tmd->coords;
    angle4              = ratan2((s32)scratch->vec.vx, (s32)scratch->vec.vz);
    delta4              = angle4 - ratan2((s32)-facing4->coord.m[2][0], (s32)facing4->coord.m[2][2]);
    wrapped4            = delta4;
    if (delta4 < 0) {
    wrapNegative4:
        if (wrapped4 < -0x800) {
            wrapped4 += 0x1000;
            goto wrapNegative4;
        }
    } else {
    wrapPositive4:
        if (wrapped4 >= 0x801) {
            wrapped4 -= 0x1000;
            goto wrapPositive4;
        }
    }
    turnDelta         = wrapped4;
    scratch->original = (scratch->delta = (s16)turnDelta);
    delta             = scratch->delta;
    unsignedDelta     = (u16)scratch->delta;
    magnitude         = abs(scratch->delta);
    if (magnitude >= 0x601) {
        targetDelta     = work->lookYawTarget;
        targetMagnitude = abs(targetDelta);
        if ((targetMagnitude >= 0x101) && ((targetDelta * delta) < 0)) {
            adjustedDelta = unsignedDelta - 0x1000;
            if (delta < 0) {
                adjustedDelta = unsignedDelta + 0x1000;
            }
            scratch->delta = adjustedDelta;
        }
    }
    if (scratch->delta >= 0x21) {
        scratch->delta = 0x20;
    }
    if (scratch->delta < -0x20) {
        scratch->delta = -0x20;
    }
    work->waistYawTarget = scratch->delta * 0x10;
    turnCoord            = arg0->extra.tmd->coords;
    yaw                  = (u16)scratch->delta + ratan2((s32)-turnCoord->coord.m[2][0], (s32)turnCoord->coord.m[2][2]);
    scratch->delta       = yaw;
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, (s32)yaw, 1);
    record = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    if (work->blendActive == 0) {
        if (desertChaserCapsuleTouchesGrid(arg0)) {
            actorMoveForward(arg0->extra.tmd->coords, 20);
        } else {
            actorMoveForward(arg0->extra.tmd->coords, 20);
        }
        record = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    }
    ActorContact_Steer(arg0->extra.tmd->coords, record, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &scratch->vec);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) == 1) {
        originalMagnitude = abs(scratch->original);
        if (originalMagnitude < 0x20) {
            work->stateTimer += 1;
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord3                                = arg0->extra.tmd->coords;
    scratch->target.vx                    = (s16)(gPlayerStatus.coordMtx->t[0] - coord3->coord.t[0]);
    target2                               = &scratch->target;
    target2->vy                           = gPlayerStatus.coordMtx->t[1] - coord3->coord.t[1];
    target2->vz                           = gPlayerStatus.coordMtx->t[2] - coord3->coord.t[2];
    if (work->stateCounter > work->windupFrames) {
        if (work->chaseHoldoff <= 0) {

            if (actorOutsideRadius(&scratch->target, radius)) {
                if (!actorOutsideRadius(&scratch->target, 0x1F40) && work->stateCounter >= 0x1C3) {
                    facing5  = arg0->extra.tmd->coords;
                    angle5   = ratan2((s32)scratch->vec.vx, (s32)scratch->vec.vz);
                    delta5   = angle5 - ratan2((s32)-facing5->coord.m[2][0], (s32)facing5->coord.m[2][2]);
                    wrapped5 = delta5;
                    if (delta5 < 0) {
                    wrapNegative5:
                        if (wrapped5 < -0x800) {
                            wrapped5 += 0x1000;
                            goto wrapNegative5;
                        }
                    } else {
                    wrapPositive5:
                        if (wrapped5 >= 0x801) {
                            wrapped5 -= 0x1000;
                            goto wrapPositive5;
                        }
                    }
                    finalDelta     = wrapped5;
                    scratch->delta = (s16)finalDelta;
                    finalDelta     = abs(finalDelta);
                    if (finalDelta < 0x300) {
                        goto changeState;
                    }
                }
            } else {
            changeState:
                work->state = 0x1C;
            }
            playerX            = -(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0];
            scratch->playerYaw = ratan2((s32)playerX, (s32)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
            targetYaw          = ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) + 0x800;
            wrappedYaw         = targetYaw;
            scratch->yaw       = targetYaw;
            if (targetYaw < 0) {
            wrapYawNegative:
                if (wrappedYaw < -0x800) {
                    wrappedYaw += 0x1000;
                    goto wrapYawNegative;
                }
            } else {
            wrapYawPositive:
                if (wrappedYaw >= 0x801) {
                    wrappedYaw -= 0x1000;
                    goto wrapYawPositive;
                }
            }
            finalYaw      = wrappedYaw;
            scratch->yaw  = (s16)finalYaw;
            yawDifference = finalYaw - scratch->playerYaw;
            if (yawDifference < 0) {
                yawDifference = -yawDifference;
            }
            if (yawDifference >= 0x601 || Gp_NodeSlotMask(&enemy->node) == 0) {
                work->state = 0x1C;
            }
        } else {
            work->chaseHoldoff -= 1;
        }
    }
    clampCoord = arg0->extra.tmd->coords;
    x          = clampCoord->coord.t[0];
    if (x > 0) {
        if (x >= 0xBEB) {
            clampCoord->coord.t[0] = 0xB54;
        } else {
            goto block_10;
        }
    } else if (x < -0xB22) {
        clampCoord->coord.t[0] = -0xA8C;
    } else {
    block_10:
        zClamp = clampCoord->coord.t[2];
        if (zClamp > 0) {
            if (zClamp >= 0xB23) {
                clampCoord->coord.t[2] = 0xA8C;
            }
        } else if (zClamp < -0xB22) {
            clampCoord->coord.t[2] = -0xA8C;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor421600RouteScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Death tick: the live-actor edge arms the model (dirty 0x80, clip 0x19C, the
/// 0xB6C node's 0x4000 flag down, the enemy's list node marked, the 0x83E /
/// 0x840 / 0x844 triple and `stateTimer` cleared) and spawns the 0x60030 effect on
/// the second coordinate. Frames 2, 3, 5, 7 and 8 then free the model buffers
/// and spawn one effect each -- 0xA0005 on coordinate 9, 12, 1 and 3 -- whose
/// model is tinted from the enemy's area record (`field_24` / `field_25`) and
/// re-streamed. Frame 0xA writes the 0x16 state. The counter stops at 0x400.
static void func_actor_421600_8013C8E0(Task* arg0)
{
    DesertChaserWork* work;
    Enemy*            ctx;
    TmdObject*        obj;
    SVECTOR           vec;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    obj  = arg0->extra.tmd;
    if (work->stateEntered != 0) {
        obj->flags                                            = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        ctx->node.state.parts.flags                           = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYaw                                         = 0;
        work->lookYawTarget                                   = 0;
        work->waistYawTarget                                  = 0;
        work->stateTimer                                      = 0;
        vec.vx                                                = 0x64;
        vec.vz                                                = 0;
        vec.vy                                                = 0;
        Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
    }
    if (work->stateTimer == 2) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        Tmd_FreeBuffers(obj);
    }
    if (work->stateTimer == 3) {
        D_80114B34[5].data.model = &_gActor421600DesertChaserBurstLegRight;
        vec.vz                   = 0x64;
        vec.vy                   = 0;
        vec.vx                   = 0;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &vec), ctx);
    }
    if (work->stateTimer == 5) {
        D_80114B34[5].data.model = &_gActor421600DesertChaserBurstLegLeft;
        vec.vy                   = 0;
        vec.vx                   = 0;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 12, 0x200, &vec), ctx);
    }
    if (work->stateTimer == 7) {
        D_80114B34[5].data.model = &_gActor421600DesertChaserBurstTorso;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 1, 0x200, NULL), ctx);
    }
    if (work->stateTimer == 8) {
        D_80114B34[5].data.model = &_gActor421600DesertChaserBurstHead;
        actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 3, 0x200, NULL), ctx);
    }
    if (work->stateTimer == 0xA) {
        work->state = 0x16;
    }
    if (work->stateTimer < 0x400) {
        work->stateTimer++;
    }
}

#include "../../shared/desert_chaser_turn_step.inc.c"

#include "../../shared/desert_chaser_turn_step_probe.inc.c"

static const Actor421600StateTable D_actor_421600_80131EFC = { { func_actor_421600_8013E858,
                                                                 func_actor_421600_80135F6C,
                                                                 func_actor_421600_80136138,
                                                                 func_actor_421600_8013A554,
                                                                 desertChaserStunned,
                                                                 func_actor_421600_8013B00C,
                                                                 func_actor_421600_8013B4C4,
                                                                 func_actor_421600_8013B8E0,
                                                                 func_actor_421600_8013C8E0,
                                                                 desertChaserTurnStep,
                                                                 desertChaserTurnStepProbe,
                                                                 desertChaserStagger,
                                                                 desertChaserCollapse,
                                                                 NULL,
                                                                 NULL,
                                                                 NULL,
                                                                 NULL,
                                                                 func_actor_421600_8013A404,
                                                                 NULL,
                                                                 NULL,
                                                                 desertChaserFlinch,
                                                                 func_actor_421600_801366F4,
                                                                 func_actor_421600_801369A0,
                                                                 NULL,
                                                                 desertChaserApproach,
                                                                 NULL,
                                                                 NULL,
                                                                 NULL,
                                                                 desertChaserPursue,
                                                                 func_actor_421600_8013E9D8,
                                                                 desertChaserSpawnAim,
                                                                 func_actor_421600_80138D24,
                                                                 func_actor_421600_8013903C,
                                                                 desertChaserSteer,
                                                                 func_actor_421600_8013EAAC,
                                                                 func_actor_421600_8013947C,
                                                                 func_actor_421600_8013EB7C,
                                                                 desertChaserStrike,
                                                                 desertChaserRoam,
                                                                 func_actor_421600_8013BA70 } };
static void                        func_actor_421600_8013D658(Enemy* enemy, Task* actor)
{
    PlayerStatus*         config;
    VECTOR                pos;
    Actor421600StateTable states;

    s16                       view;
    s16                       height;
    s16                       state;
    s16                       activeState;
    s16                       finalState;
    GfxCoord*                 playerCoord;
    GfxCoord*                 actorCoord;
    GfxCoord*                 actorCoord2;
    GfxCoord*                 playerCoord2;
    s32                       action;
    s32                       x;
    s32                       z;
    s32                       nextAction;
    s32                       result;
    u8                        kind;
    s32                       contactKind;
    void**                    scratchHead;
    AnimationSet**            nextPlayerSets;
    DesertChaserWork*         actorWork;
    DesertChaserWork*         actorWork2;
    DesertChaserWork*         work;
    Task*                     player;
    AnimationSet**            playerSets;
    Task*                     slot;
    Task*                     slot2;
    Actor421600UpdateScratch* scratch;
    GfxCoord*                 clampCoord;
    AnimationPlayRequest*     message;
    AnimationPlayRequest*     nextMessage;

    work                                   = actor->work;
    player                                 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    config                                 = &gPlayerStatus;
    view                                   = Gp_GetViewIndex() & 0xFF;
    states                                 = D_actor_421600_80131EFC;
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(actor->extra.tmd->coords);
    pos.vx = actor->extra.tmd->coords->workm.t[0];
    pos.vy = actor->extra.tmd->coords->workm.t[1];
    pos.vz = actor->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->state != 0x15 && work->state != 0 && work->state != 0x16 && work->state != 7 && work->state != 8) {
                height = actor->extra.tmd->coords->coord.t[1];
                limbShadowDrawSegment(actor, 1, 3, 0x12C, (s32)height, 0xFF);
                limbShadowDrawSegment(actor, 3, 4, 0xC8, (s32)height, 0xFF);
                limbShadowDrawSegment(actor, 1, 0xB, 0xFA, (s32)height, 0xFF);
                if ((Gp_GetViewIndex() & 0xFF) == 0x13) {
                    actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                } else {
                    actor->extra.tmd->flags = 0;
                }
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->state != 0x15 && work->state != 0 && work->state != 0x16 && work->state != 7 && work->state != 8) {
                height = actor->extra.tmd->coords->coord.t[1];
                limbShadowDrawSegment(actor, 1, 3, 0x12C, (s32)height, 0xFF);
                limbShadowDrawSegment(actor, 3, 4, 0xC8, (s32)height, 0xFF);
                limbShadowDrawSegment(actor, 1, 0xB, 0xFA, (s32)height, 0xFF);
                if ((Gp_GetViewIndex() & 0xFF) == 0x13) {
                    actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                } else {
                    actor->extra.tmd->flags = 0;
                }
            }
            Gp_ClearRec18Occupied(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts);
            Gp_ClearRec18Occupied(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts);
            Gp_ClearRec18Occupied(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts);
            Gp_ClearRec18Occupied(work->wallProbe.contacts);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Gp_ClearRec18Occupied(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts);
            Gp_ClearRec18Occupied(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts);
            Gp_ClearRec18Occupied(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts);
            Gp_ClearRec18Occupied(work->wallProbe.contacts);
            return;
    }
    scratchHead = SCRATCH_HEAD_ADDR;
    scratch     = Actor421600_AllocUpdateScratch((Actor421600UpdateScratch**)scratchHead);
    if (work->hitCooldown > 0) {
        work->hitCooldown = (s16)((u16)work->hitCooldown - 1);
    } else if (config->hp > 0) {
        func_actor_421600_801354D8(actor);
    }
    kind = work->lastCommand.fields.command;
    if ((kind == 2) && ((state = work->state, (state == 0x26)) || (state == kind))) {
        work->state = 0x27;
    }
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = (s16)(u16)work->state;
    scratch->zone   = Actor421600_Zone(actor->extra.tmd->coords);
    if (work->playerHeld == 1) {
        activeState = work->state;
        if ((activeState != 0x15) && (activeState != 0) && (activeState != 8)) {
            actorWork = actor->work;
            slot      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            if ((slot != NULL) && (actorWork->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
                playerCoord = slot->extra.tmd->coords;
                actorCoord  = actor->extra.tmd->coords;
                if (abs(playerCoord->coord.t[1] - actorCoord->coord.t[1]) >= 0x12D) {
                    playerCoord->coord.t[1]               = actorCoord->coord.t[1];
                    slot->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                }
            }
        }
        actorWork2 = actor->work;
        slot2      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if ((slot2 != NULL) && (actorWork2->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
            playerCoord2 = slot2->extra.tmd->coords;
            actorCoord2  = actor->extra.tmd->coords;
            if (abs(playerCoord2->coord.t[1] - actorCoord2->coord.t[1]) >= 0x12D) {
                playerCoord2->coord.t[1]               = actorCoord2->coord.t[1];
                slot2->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
        }
        contactKind = work->lastCommand.fields.command;
        work->lastCommand.fields.catchFrames++;
        if (contactKind == 1) {
            if (D_dryfield_water_tower_801876AA == (D_dryfield_water_tower_801876A8 + 1)) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, NULL, 0);
                work->playerHeld = 0;
            }
            if ((work->lastCommand.fields.command == contactKind) && ((s32)D_dryfield_water_tower_801876AA < (D_dryfield_water_tower_801876A8 + 3))) {
                work->playerMove.displacement.vx = 0;
                work->playerMove.displacement.vy = 0;
                work->playerMove.displacement.vz = 0;
            }
        }
        action = work->playerAnim.animationId;
        switch (action) {
            case 4:
                break;
            case 1:
                if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &work->playerMove, 0) == 1) {
                    work->playerMove.displacement.vx = 0;
                    work->playerMove.displacement.vy = 0;
                    work->playerMove.displacement.vz = 0;
                }
                if ((work->lastCommand.fields.catchFrames >= 0xFU) && (work->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
                    work->playerMove.displacement.vx >>= 1;
                    work->playerMove.displacement.vy >>= 1;
                    work->playerMove.displacement.vz >>= 1;
                }
                break;
            case 2:
                playerSets = work->playerAnim.source.sets;
                if (playerSets == gDesertChaserRearAnim) {
                    if ((config->hp > 0) && (work->lastCommand.fields.catchFrames >= 0x17U)) {
                        message                      = &work->playerAnim;
                        playerSets[4]                = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[7];
                        work->playerAnim.animationId = 4;
                        work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
                        work->playerAnim.blendFrames = 3;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, message, 0);
                        work->lastCommand.fields.catchFrames = 0U;
                    }
                } else if ((config->hp > 0) && (work->lastCommand.fields.catchFrames >= 0x22U)) {
                    message                       = &work->playerAnim;
                    Actor421600FrontContact.value = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[7];

                    work->playerAnim.animationId = 4;
                    work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
                    work->playerAnim.blendFrames = 3;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, message, 0);
                    work->lastCommand.fields.catchFrames = 0U;
                }
                break;
            case 3:
                if ((work->lastCommand.fields.catchFrames < 6U) && (config->hp > 0)) {
                    result = TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &work->playerMove, 0);
                    if (result == 1) {
                        work->playerMove.displacement.vx = 0;
                        work->playerMove.displacement.vy = 0;
                        work->playerMove.displacement.vz = 0;
                        work->playerMove.keepControl     = result;
                    }
                }
                break;
            case 5:
                if ((config->hp > 0) && (work->lastCommand.fields.catchFrames >= 7U)) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
                    work->playerHeld = 0;
                }
                break;
        }
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, NULL, 0) == 0) {
            nextAction = work->playerAnim.animationId;
            switch (nextAction) {
                case 1:
                    if (((((GAME_LOCATION_WORD(gGameSession->location.loc)) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(4, 1, 0, 0)) || (work->playerMove.collisionRequests != (GAME_ACTOR_COLLISION_REQUEST_MASK << GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT))) && (config->hp > 0)) {
                        nextMessage                  = &work->playerAnim;
                        work->playerAnim.blend       = ANIMATION_BLEND_RESET;
                        work->playerAnim.blendFrames = 0;
                        work->playerAnim.animationId = 2;

                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, nextMessage, 0);
                        work->lastCommand.fields.catchFrames = 0U;
                    }
                    break;
                case 3:
                    if (config->hp > 0) {
                        work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
                        work->playerAnim.blendFrames = 6;
                        work->playerAnim.animationId = 5;
                        nextPlayerSets               = work->playerAnim.source.sets;
                        if (nextPlayerSets == gDesertChaserRearAnim) {
                            nextPlayerSets[5] = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[9];
                        } else {
                            Actor421600FallbackEnd.value = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[9];
                        }
                        nextMessage = &work->playerAnim;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, nextMessage, 0);
                        work->lastCommand.fields.catchFrames = 0U;
                    }
                    break;
                case 4:
                case 7:
                    if (config->hp > 0) {
                        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
                        work->playerHeld = 0;
                    }
                    break;
            }
        }
    }
    states.fn[work->state](actor);
    if (work->state == 1 && work->lastCommand.fields.command == 0) {
        if ((Gp_GetViewIndex() & 0xFF) == 5)
            work->state = 2;
    }
    if (Actor421600_HasPlayerContact(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts)) {
        switch (view) {
            case 9:
            case 0x12:
                clampCoord = player->extra.tmd->coords;
                x          = clampCoord->coord.t[0];
                if (x > 0) {
                    if (x >= 0xBEB) {
                        clampCoord->coord.t[0] = 0xB54;
                    } else {
                        goto clampZ;
                    }
                } else if (x < -0xB22) {
                    clampCoord->coord.t[0] = -0xA8C;
                } else {
                clampZ:
                    z = clampCoord->coord.t[2];
                    if (z > 0) {
                        if (z >= 0xB23) {
                            clampCoord->coord.t[2] = 0xA8C;
                        }
                    } else if (z < -0xB22) {
                        clampCoord->coord.t[2] = -0xA8C;
                    }
                }

                break;
            case 0xE:
                break;
            default:
                func_actor_421600_80133334(player->extra.tmd->coords);
                break;
        }
    }
    finalState = work->state;
    if ((finalState != 0x15) && (finalState != 0) && (finalState != 5) && (finalState != 0x16) && (finalState != 8)) {
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->spheres[DESERT_CHASER_SPHERE_REAR].body.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->spheres[DESERT_CHASER_SPHERE_REAR].body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    Gp_ClearRec18Occupied(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts);
    Gp_ClearRec18Occupied(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts);
    Gp_ClearRec18Occupied(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts);
    Gp_ClearRec18Occupied(work->wallProbe.contacts);
    scratch->pos.vx = 0U;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    actorTransformToView(actor->extra.tmd->coords + 2, &scratch->pos);
    enemy->bodyPos.vx = (s32)(s16)scratch->pos.vx;
    enemy->bodyPos.vy = (s32)scratch->pos.vy;
    enemy->bodyPos.vz = (s32)scratch->pos.vz;
    enemy->coord      = &gGfxViewCoord;
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
    func_actor_421600_80133444(actor->extra.tmd->coords);
}

s32 func_actor_421600_8013E424(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

/// The enemy task's state handlers - spawn, per-frame tick and teardown - run by
/// `desertChaserTask`.
static const DesertChaserTaskStates gDesertChaserTaskStates = {
    {
        func_actor_421600_80134AD4,
        func_actor_421600_8013D658,
        enemyDestroy,
    },
};

#include "../../shared/actor_messages_visibility.inc.c"

/// Handler for message 0x7D6: returns 1 while the enemy still has hit points
/// or its model is shown (flag 0x80 clear), 0 once it is dead and hidden.
s32 func_actor_421600_8013E4EC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (((Enemy*)task->spawnArg2.pointer)->hp > 0) {
        goto return_one;
    }

    if ((task->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) != 0) {
        return 0;
    }

return_one:
    return 1;
}

#include "../../shared/actor_messages_place_yaw_first.inc.c"

#include "../../shared/desert_chaser_play_anim.inc.c"

s32 func_actor_421600_8013E654(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    DesertChaserWork* work = task->work;

    work->roomEventCountdown = 0x1E;
    return 1;
}

/// `Task::exitCallback` teardown: kill the two helper tasks, unlink the three
/// display nodes, clear the enemy's `recs`, then `enemyDestroy`.
static void func_actor_421600_8013E668(Task* task)
{
    DesertChaserWork* work;
    Enemy*            enemy;

    work  = task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    if (work != NULL) {
        if (work->childTask0 != NULL) {
            taskKill(work->childTask0);
        }
        if (work->childTask1 != NULL) {
            taskKill(work->childTask1);
        }
        Gp_UnlinkObj(&work->spheres[DESERT_CHASER_SPHERE_FRONT].body);
        Gp_UnlinkObj(&work->spheres[DESERT_CHASER_SPHERE_REAR].body);
        Gp_UnlinkObj(&work->spheres[DESERT_CHASER_SPHERE_ROOT].body);
        enemy->recs = 0;
    }
    enemyDestroy(enemy, task);
}

#include "../../shared/desert_chaser_part_effect.inc.c"

/// Copy the `vx`/`vy`/`vz` of entry `arg1` of the pose table into `arg0`.
static void func_actor_421600_8013E7F8(SVECTOR* arg0, s32 arg1)
{
    arg0->vx = D_actor_421600_80151158[(s16)arg1].vx;
    arg0->vy = D_actor_421600_80151158[(s16)arg1].vy;
    arg0->vz = D_actor_421600_80151158[(s16)arg1].vz;
}

static s8 func_actor_421600_8013E830(s32 arg0, s32 arg1)
{
    s8* p;
    s32 a;
    s32 b;

    p = D_actor_421600_801511D0;
    a = arg0 > 0;
    b = arg1 < 1;
    return p[a + (b << 1)];
}

static void func_actor_421600_8013E858(Task* arg0)
{
    TmdObject*        obj;
    DesertChaserWork* work;
    Enemy*            enemy;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                  = arg0->extra.tmd;
        enemy                                                = arg0->spawnArg2.pointer;
        enemy->node.state.parts.flags                        = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                          |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->hp                                            = 0;
    }
}

#include "../../shared/desert_chaser_stunned.inc.c"

/// State handler: on the live-actor edge (`stateEntered` set) show the model, start
/// animation 4 and step it once; afterwards step the animation and, once its
/// flag 0x100 is up, go to state 5 when the last command received is
/// `DESERT_CHASER_COMMAND_WATER_TOWER_1`, else 2.
static void func_actor_421600_8013E9D8(Task* arg0)
{
    TmdObject*        obj;
    DesertChaserWork* work;
    s32               state;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = 0x10;
        work->blendActive                                     = 0;
        work->animId                                          = 4;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        desertChaserAnimTick(arg0);
        return;
    }
    desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        state = work->lastCommand.word & DESERT_CHASER_COMMAND_MASK;
        if (state == DESERT_CHASER_COMMAND_WATER_TOWER_1) {
            state = 5;
        } else {
            state = 2;
        }
        work->state = state;
    }
}

static void func_actor_421600_8013EAAC(Task* arg0)
{
    TmdObject*        obj;
    DesertChaserWork* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = 0x10;
        work->blendActive                                     = 0;
        work->animId                                          = 8;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        desertChaserAnimTick(arg0);
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = 0x1C;
    }
}

static void func_actor_421600_8013EB7C(Task* arg0)
{
    TmdObject*        obj;
    DesertChaserWork* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = 0x10;
        work->blendActive                                     = 0;
        work->animId                                          = 0xC;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        desertChaserAnimTick(arg0);
    }
    desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = 2;
    }
}

#include "../../shared/desert_chaser_flinch.inc.c"

#include "../../shared/desert_chaser_stagger.inc.c"

#include "../../shared/desert_chaser_collapse.inc.c"

#include "../../shared/desert_chaser_task.inc.c"
