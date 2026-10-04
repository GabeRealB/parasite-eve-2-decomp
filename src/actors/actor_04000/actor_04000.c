#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
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
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/coord_math.h"
#include "../../shared/actor_messages.h"
#include "../../shared/anim_driver.h"
#include "../../shared/actor_contacts.h"

/// Event packet handed to the message handlers: the same four bytes read as
/// two `u16` words, a command word (0x1003, 0x1203, 0x302) and a sub-command.
typedef union Actor104000Event {
    /* 0x0 */ u8  bytes[4];
    /* 0x0 */ u16 words[2];
} Actor104000Event;

// Typed callback views for the task message dispatcher.

/// The actor's per-instance work block (`Task::work`),
/// allocated and filled by `Actor04000_Fn010B8`. It embeds four
/// collision objects linked with `Gp_LinkObj`, each followed by the `WorldCollisionContact`
/// table its `field_C` points at; the high bit of their flag words gates one
/// behaviour and bit 0x4000 another.
typedef struct Actor104000Work {
    /* 0x000 */ s16                   field_0;
    /* 0x002 */ s16                   field_2;
    /* 0x004 */ s16                   field_4;
    /* 0x006 */ u16                   field_6;
    /* 0x008 */ s16                   field_8;
    /* 0x00A */ s16                   field_A;
    /* 0x00C */ AnimationContext      anim;
    /* 0x020 */ AnimationSlot         slots[1]; // slots 1..5 continue past here, overlapping the fields below
    /* 0x048 */ byte                  pad_48[2];
    /* 0x04A */ u16                   field_4A; // low ten bits: animation id (`slots[1].field_2`)
    /* 0x04C */ byte                  pad_4C[0xC];
    /* 0x058 */ u16                   field_58;
    /* 0x05A */ byte                  pad_5A[0xB6];
    /* 0x110 */ byte                  poses[0x60]; // `animationInitContext` poseBuffer
    /* 0x170 */ s16                   field_170;
    /* 0x172 */ s16                   field_172;
    /* 0x174 */ s16                   field_174;
    /* 0x176 */ s16                   field_176;
    /* 0x178 */ s16                   field_178;
    /* 0x17A */ s16                   field_17A;
    /* 0x17C */ s16                   field_17C;
    /* 0x17E */ s16                   field_17E;
    /* 0x180 */ s32                   field_180;
    /* 0x184 */ s32                   field_184;
    /* 0x188 */ s32                   field_188;
    /* 0x18C */ s32                   field_18C;
    /* 0x190 */ s32                   field_190;
    /* 0x194 */ u16                   field_194;
    /* 0x196 */ byte                  pad_196[2];
    /* 0x198 */ u16                   field_198;
    /* 0x19A */ u16                   field_19A;
    /* 0x19C */ s16                   field_19C;
    /* 0x19E */ byte                  pad_19E[2];
    /* 0x1A0 */ s16                   field_1A0;
    /* 0x1A2 */ s16                   field_1A2;
    /* 0x1A4 */ byte                  pad_1A4[0xC];
    /* 0x1B0 */ WorldCollisionContact rec1B0[8];
    /* 0x270 */ WorldCollisionBody    obj270;
    /* 0x290 */ WorldCollisionContact hits[8]; // this frame's collision records, ended by a zero id
    /* 0x350 */ WorldCollisionBody    obj350;
    /* 0x370 */ WorldCollisionContact rec370;
    /* 0x388 */ WorldCollisionBody    obj388;
    /* 0x3A8 */ WorldCollisionContact rec3A8;
    /* 0x3C0 */ WorldCollisionBody    obj3C0;
    /* 0x3E0 */ EffectSpawnArg        eff;           // `func_800FDB18` argument record
    /* 0x3E8 */ SVECTOR               effOfs;        // offset handed to `func_800FDB18`; `pad` picks the coordinate
    /* 0x3F0 */ SVECTOR               origin;        // model position at spawn
    /* 0x3F8 */ SVECTOR               dir;           // facing direction captured on restart
    /* 0x400 */ SVECTOR               patrol[2];     // spawn position plus (0) / minus (1) 1000 units along the facing (XZ)
    /* 0x410 */ s16                   patrolIdx;     // `patrol` point currently walked toward
    /* 0x412 */ byte                  pad_412[2];
    /* 0x414 */ MATRIX                lightMtx;      // installed at `TmdObject::lightMtx`
    /* 0x434 */ MATRIX                colorMtx;      // installed at `TmdObject::colorMtx`
    /* 0x454 */ MATRIX                savedColorMtx; // `colorMtx` before the death fade scales it
    /* 0x474 */ u16                   field_474;     // animation id that last raised the reaction
    /* 0x476 */ byte                  pad_476[3];
    /* 0x479 */ u8                    field_479;
    /* 0x47A */ u8                    field_47A;
    /* 0x47B */ byte                  pad_47B[1];
    /* 0x47C */ byte                  field_47C[0x14];
    /* 0x490 */ s32                   field_490;
    /* 0x494 */ s16                   field_494;
    /* 0x496 */ s16                   field_496;
} Actor104000Work;
STATIC_ASSERT_SIZEOF(Actor104000Work, 0x498);

/// 0x14-byte scratch taken from `0x1F8003FC` by the lunge state: the offset to
/// the player (later the snap direction), the final yaw and the relative yaw.
typedef struct Actor104000AimScratch {
    /* 0x00 */ SVECTOR d;
    /* 0x08 */ byte    pad_8[8];
    /* 0x10 */ s16     yaw;
    /* 0x12 */ s16     angle;
} Actor104000AimScratch;
STATIC_ASSERT_SIZEOF(Actor104000AimScratch, 0x14);

/// The nineteen handlers the tick copies onto its stack before dispatching.
typedef struct Actor104000StateTable {
    /* 0x00 */ EnemyTaskFunc fn[19];
} Actor104000StateTable;
STATIC_ASSERT_SIZEOF(Actor104000StateTable, 0x4C);

/// Whole-unit part of the last step `ActorContact_PushContact` applied.
extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

extern Task* Actor04000_D0C710[2];

extern Task* Actor04000_D0C718[8];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static void Actor04000_Fn06A5C(Enemy* enemy, Task* task);
static void Actor04000_Fn06878(Enemy* arg0, Task* arg1);
static void Actor04000_Fn06994(Enemy* arg0, Task* arg1);
static void Actor04000_Fn06AC4(Enemy* arg0, Task* arg1);
static void Actor04000_Fn06BC8(Enemy* arg0, Task* arg1);
static void Actor04000_Fn06C80(Enemy* arg0, Task* arg1);
static void Actor04000_Fn06D38(Enemy* arg0, Task* arg1);

static TmdSource _gActor04000BloodSucklerBody;
void             Actor04000_Fn06380(Task*);
void             Actor04000_Fn06E4C(Task*);
void             Actor04000_Fn06EA8(Task*);
void             Actor04000_Fn06F54(Task*);
void             Actor04000_Fn0703C(Task*);

s32 Actor04000_Fn0093C(Task*, s32, Actor104000Event*, s32);
s32 Actor04000_Fn06590(Task*, s32, s32, s32);
s32 Actor04000_Fn06704(Task*, s32, void*, s32);
s32 Actor04000_Fn06728(Task*, s32, AnimationPlayRequest*, s32);

DamageAttack Actor04000_D07078[3] = {
    { 1, 7 },
    { 18, 7 },
    { 30, 0 },
};

EnemyParams Actor04000_D07084 = { Actor04000_D07078, 1, 8, 28, 4, 100, 100, 100, 0 };

PadScriptCmd Actor04000_D07094[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment Actor04000_D070A0[3] = {
    { 0, 0, 9, 0 },
    { 255, 255, 12, 1 },
    { 100, 50, 6, 1 },
};

static TmdBone _gActor04000BloodSucklerBodySkeleton[6] = {
#include "assets/blood_suckler_body_skeleton.inc"
};

static u32 _gActor04000BloodSucklerBodyPartVerts[6] = {
#include "assets/blood_suckler_body_partVerts.inc"
};

static SVECTOR _gActor04000BloodSucklerBodyVerts[94] = {
#include "assets/blood_suckler_body_verts.inc"
};

static SVECTOR _gActor04000BloodSucklerBodyNormals[94] = {
#include "assets/blood_suckler_body_normals.inc"
};

static u32 _gActor04000BloodSucklerBodyStream[999] = {
#include "assets/blood_suckler_body_stream.inc"
};

static TmdSource _gActor04000BloodSucklerBody = {
    0,
    5624,
    1228,
    6,
    _gActor04000BloodSucklerBodyPartVerts,
    _gActor04000BloodSucklerBodyVerts,
    _gActor04000BloodSucklerBodyNormals,
    _gActor04000BloodSucklerBodySkeleton,
    _gActor04000BloodSucklerBodyStream,
};

static AnimationPackedPose _gActor04000Actor104000Animation08878Bank1[4] = {
#include "assets/actor_104000_animation_08878_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation08878Bank4[21] = {
#include "assets/actor_104000_animation_08878_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation08878Records[43] = {
#include "assets/actor_104000_animation_08878_records.inc"
};

static u16 _gActor04000Actor104000Animation08878Indices[6] = {
#include "assets/actor_104000_animation_08878_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation08878 = {
    _gActor04000Actor104000Animation08878Records,
    _gActor04000Actor104000Animation08878Indices,
    { NULL, _gActor04000Actor104000Animation08878Bank1, NULL, NULL, _gActor04000Actor104000Animation08878Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation08B3CBank1[19] = {
#include "assets/actor_104000_animation_08B3C_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation08B3CBank4[36] = {
#include "assets/actor_104000_animation_08B3C_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation08B3CRecords[71] = {
#include "assets/actor_104000_animation_08B3C_records.inc"
};

static u16 _gActor04000Actor104000Animation08B3CIndices[6] = {
#include "assets/actor_104000_animation_08B3C_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation08B3C = {
    _gActor04000Actor104000Animation08B3CRecords,
    _gActor04000Actor104000Animation08B3CIndices,
    { NULL, _gActor04000Actor104000Animation08B3CBank1, NULL, NULL, _gActor04000Actor104000Animation08B3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation08DE0Bank1[20] = {
#include "assets/actor_104000_animation_08DE0_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation08DE0Bank4[31] = {
#include "assets/actor_104000_animation_08DE0_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation08DE0Records[65] = {
#include "assets/actor_104000_animation_08DE0_records.inc"
};

static u16 _gActor04000Actor104000Animation08DE0Indices[6] = {
#include "assets/actor_104000_animation_08DE0_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation08DE0 = {
    _gActor04000Actor104000Animation08DE0Records,
    _gActor04000Actor104000Animation08DE0Indices,
    { NULL, _gActor04000Actor104000Animation08DE0Bank1, NULL, NULL, _gActor04000Actor104000Animation08DE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation08FC0Bank1[6] = {
#include "assets/actor_104000_animation_08FC0_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation08FC0Bank4[38] = {
#include "assets/actor_104000_animation_08FC0_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation08FC0Records[51] = {
#include "assets/actor_104000_animation_08FC0_records.inc"
};

static u16 _gActor04000Actor104000Animation08FC0Indices[6] = {
#include "assets/actor_104000_animation_08FC0_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation08FC0 = {
    _gActor04000Actor104000Animation08FC0Records,
    _gActor04000Actor104000Animation08FC0Indices,
    { NULL, _gActor04000Actor104000Animation08FC0Bank1, NULL, NULL, _gActor04000Actor104000Animation08FC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation090F8Bank1[4] = {
#include "assets/actor_104000_animation_090F8_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation090F8Bank4[13] = {
#include "assets/actor_104000_animation_090F8_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation090F8Records[40] = {
#include "assets/actor_104000_animation_090F8_records.inc"
};

static u16 _gActor04000Actor104000Animation090F8Indices[6] = {
#include "assets/actor_104000_animation_090F8_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation090F8 = {
    _gActor04000Actor104000Animation090F8Records,
    _gActor04000Actor104000Animation090F8Indices,
    { NULL, _gActor04000Actor104000Animation090F8Bank1, NULL, NULL, _gActor04000Actor104000Animation090F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation09298Bank1[7] = {
#include "assets/actor_104000_animation_09298_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation09298Bank4[28] = {
#include "assets/actor_104000_animation_09298_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation09298Records[42] = {
#include "assets/actor_104000_animation_09298_records.inc"
};

static u16 _gActor04000Actor104000Animation09298Indices[6] = {
#include "assets/actor_104000_animation_09298_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation09298 = {
    _gActor04000Actor104000Animation09298Records,
    _gActor04000Actor104000Animation09298Indices,
    { NULL, _gActor04000Actor104000Animation09298Bank1, NULL, NULL, _gActor04000Actor104000Animation09298Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation094ACBank1[6] = {
#include "assets/actor_104000_animation_094AC_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation094ACBank4[42] = {
#include "assets/actor_104000_animation_094AC_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation094ACRecords[60] = {
#include "assets/actor_104000_animation_094AC_records.inc"
};

static u16 _gActor04000Actor104000Animation094ACIndices[6] = {
#include "assets/actor_104000_animation_094AC_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation094AC = {
    _gActor04000Actor104000Animation094ACRecords,
    _gActor04000Actor104000Animation094ACIndices,
    { NULL, _gActor04000Actor104000Animation094ACBank1, NULL, NULL, _gActor04000Actor104000Animation094ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation098BCBank1[17] = {
#include "assets/actor_104000_animation_098BC_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation098BCBank4[85] = {
#include "assets/actor_104000_animation_098BC_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation098BCRecords[111] = {
#include "assets/actor_104000_animation_098BC_records.inc"
};

static u16 _gActor04000Actor104000Animation098BCIndices[6] = {
#include "assets/actor_104000_animation_098BC_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation098BC = {
    _gActor04000Actor104000Animation098BCRecords,
    _gActor04000Actor104000Animation098BCIndices,
    { NULL, _gActor04000Actor104000Animation098BCBank1, NULL, NULL, _gActor04000Actor104000Animation098BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation09A94Bank1[5] = {
#include "assets/actor_104000_animation_09A94_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation09A94Bank4[19] = {
#include "assets/actor_104000_animation_09A94_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation09A94Records[71] = {
#include "assets/actor_104000_animation_09A94_records.inc"
};

static u16 _gActor04000Actor104000Animation09A94Indices[6] = {
#include "assets/actor_104000_animation_09A94_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation09A94 = {
    _gActor04000Actor104000Animation09A94Records,
    _gActor04000Actor104000Animation09A94Indices,
    { NULL, _gActor04000Actor104000Animation09A94Bank1, NULL, NULL, _gActor04000Actor104000Animation09A94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0A260Bank1[14] = {
#include "assets/actor_104000_animation_0A260_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0A260Bank4[158] = {
#include "assets/actor_104000_animation_0A260_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0A260Records[279] = {
#include "assets/actor_104000_animation_0A260_records.inc"
};

static u16 _gActor04000Actor104000Animation0A260Indices[20] = {
#include "assets/actor_104000_animation_0A260_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0A260 = {
    _gActor04000Actor104000Animation0A260Records,
    _gActor04000Actor104000Animation0A260Indices,
    { NULL, _gActor04000Actor104000Animation0A260Bank1, NULL, NULL, _gActor04000Actor104000Animation0A260Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0A71CBank1[10] = {
#include "assets/actor_104000_animation_0A71C_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0A71CBank4[105] = {
#include "assets/actor_104000_animation_0A71C_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0A71CRecords[148] = {
#include "assets/actor_104000_animation_0A71C_records.inc"
};

static u16 _gActor04000Actor104000Animation0A71CIndices[20] = {
#include "assets/actor_104000_animation_0A71C_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0A71C = {
    _gActor04000Actor104000Animation0A71CRecords,
    _gActor04000Actor104000Animation0A71CIndices,
    { NULL, _gActor04000Actor104000Animation0A71CBank1, NULL, NULL, _gActor04000Actor104000Animation0A71CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0AECCBank1[14] = {
#include "assets/actor_104000_animation_0AECC_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0AECCBank4[147] = {
#include "assets/actor_104000_animation_0AECC_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0AECCRecords[283] = {
#include "assets/actor_104000_animation_0AECC_records.inc"
};

static u16 _gActor04000Actor104000Animation0AECCIndices[20] = {
#include "assets/actor_104000_animation_0AECC_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0AECC = {
    _gActor04000Actor104000Animation0AECCRecords,
    _gActor04000Actor104000Animation0AECCIndices,
    { NULL, _gActor04000Actor104000Animation0AECCBank1, NULL, NULL, _gActor04000Actor104000Animation0AECCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0B374Bank1[10] = {
#include "assets/actor_104000_animation_0B374_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0B374Bank4[103] = {
#include "assets/actor_104000_animation_0B374_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0B374Records[145] = {
#include "assets/actor_104000_animation_0B374_records.inc"
};

static u16 _gActor04000Actor104000Animation0B374Indices[20] = {
#include "assets/actor_104000_animation_0B374_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0B374 = {
    _gActor04000Actor104000Animation0B374Records,
    _gActor04000Actor104000Animation0B374Indices,
    { NULL, _gActor04000Actor104000Animation0B374Bank1, NULL, NULL, _gActor04000Actor104000Animation0B374Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0B6F0Bank1[15] = {
#include "assets/actor_104000_animation_0B6F0_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0B6F0Bank4[70] = {
#include "assets/actor_104000_animation_0B6F0_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0B6F0Records[95] = {
#include "assets/actor_104000_animation_0B6F0_records.inc"
};

static u16 _gActor04000Actor104000Animation0B6F0Indices[6] = {
#include "assets/actor_104000_animation_0B6F0_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0B6F0 = {
    _gActor04000Actor104000Animation0B6F0Records,
    _gActor04000Actor104000Animation0B6F0Indices,
    { NULL, _gActor04000Actor104000Animation0B6F0Bank1, NULL, NULL, _gActor04000Actor104000Animation0B6F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0BAA0Bank1[14] = {
#include "assets/actor_104000_animation_0BAA0_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0BAA0Bank4[64] = {
#include "assets/actor_104000_animation_0BAA0_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0BAA0Records[117] = {
#include "assets/actor_104000_animation_0BAA0_records.inc"
};

static u16 _gActor04000Actor104000Animation0BAA0Indices[6] = {
#include "assets/actor_104000_animation_0BAA0_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0BAA0 = {
    _gActor04000Actor104000Animation0BAA0Records,
    _gActor04000Actor104000Animation0BAA0Indices,
    { NULL, _gActor04000Actor104000Animation0BAA0Bank1, NULL, NULL, _gActor04000Actor104000Animation0BAA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0BCDCBank1[14] = {
#include "assets/actor_104000_animation_0BCDC_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0BCDCBank4[28] = {
#include "assets/actor_104000_animation_0BCDC_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0BCDCRecords[60] = {
#include "assets/actor_104000_animation_0BCDC_records.inc"
};

static u16 _gActor04000Actor104000Animation0BCDCIndices[6] = {
#include "assets/actor_104000_animation_0BCDC_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0BCDC = {
    _gActor04000Actor104000Animation0BCDCRecords,
    _gActor04000Actor104000Animation0BCDCIndices,
    { NULL, _gActor04000Actor104000Animation0BCDCBank1, NULL, NULL, _gActor04000Actor104000Animation0BCDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0BF20Bank1[10] = {
#include "assets/actor_104000_animation_0BF20_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0BF20Bank4[40] = {
#include "assets/actor_104000_animation_0BF20_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0BF20Records[62] = {
#include "assets/actor_104000_animation_0BF20_records.inc"
};

static u16 _gActor04000Actor104000Animation0BF20Indices[6] = {
#include "assets/actor_104000_animation_0BF20_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0BF20 = {
    _gActor04000Actor104000Animation0BF20Records,
    _gActor04000Actor104000Animation0BF20Indices,
    { NULL, _gActor04000Actor104000Animation0BF20Bank1, NULL, NULL, _gActor04000Actor104000Animation0BF20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0C070Bank1[6] = {
#include "assets/actor_104000_animation_0C070_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0C070Bank4[20] = {
#include "assets/actor_104000_animation_0C070_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0C070Records[33] = {
#include "assets/actor_104000_animation_0C070_records.inc"
};

static u16 _gActor04000Actor104000Animation0C070Indices[6] = {
#include "assets/actor_104000_animation_0C070_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0C070 = {
    _gActor04000Actor104000Animation0C070Records,
    _gActor04000Actor104000Animation0C070Indices,
    { NULL, _gActor04000Actor104000Animation0C070Bank1, NULL, NULL, _gActor04000Actor104000Animation0C070Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0C1C0Bank1[6] = {
#include "assets/actor_104000_animation_0C1C0_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0C1C0Bank4[20] = {
#include "assets/actor_104000_animation_0C1C0_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0C1C0Records[33] = {
#include "assets/actor_104000_animation_0C1C0_records.inc"
};

static u16 _gActor04000Actor104000Animation0C1C0Indices[6] = {
#include "assets/actor_104000_animation_0C1C0_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0C1C0 = {
    _gActor04000Actor104000Animation0C1C0Records,
    _gActor04000Actor104000Animation0C1C0Indices,
    { NULL, _gActor04000Actor104000Animation0C1C0Bank1, NULL, NULL, _gActor04000Actor104000Animation0C1C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0C2ECBank1[5] = {
#include "assets/actor_104000_animation_0C2EC_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0C2ECBank4[14] = {
#include "assets/actor_104000_animation_0C2EC_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0C2ECRecords[33] = {
#include "assets/actor_104000_animation_0C2EC_records.inc"
};

static u16 _gActor04000Actor104000Animation0C2ECIndices[6] = {
#include "assets/actor_104000_animation_0C2EC_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0C2EC = {
    _gActor04000Actor104000Animation0C2ECRecords,
    _gActor04000Actor104000Animation0C2ECIndices,
    { NULL, _gActor04000Actor104000Animation0C2ECBank1, NULL, NULL, _gActor04000Actor104000Animation0C2ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0C49CBank1[8] = {
#include "assets/actor_104000_animation_0C49C_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0C49CBank4[28] = {
#include "assets/actor_104000_animation_0C49C_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0C49CRecords[43] = {
#include "assets/actor_104000_animation_0C49C_records.inc"
};

static u16 _gActor04000Actor104000Animation0C49CIndices[6] = {
#include "assets/actor_104000_animation_0C49C_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0C49C = {
    _gActor04000Actor104000Animation0C49CRecords,
    _gActor04000Actor104000Animation0C49CIndices,
    { NULL, _gActor04000Actor104000Animation0C49CBank1, NULL, NULL, _gActor04000Actor104000Animation0C49CBank4, NULL, NULL, NULL },
};

AnimationSet* Actor04000_D0C4C4[19] = {
    NULL,
    &_gActor04000Actor104000Animation08878,
    &_gActor04000Actor104000Animation08B3C,
    &_gActor04000Actor104000Animation08DE0,
    &_gActor04000Actor104000Animation08FC0,
    &_gActor04000Actor104000Animation090F8,
    &_gActor04000Actor104000Animation09298,
    &_gActor04000Actor104000Animation094AC,
    &_gActor04000Actor104000Animation098BC,
    &_gActor04000Actor104000Animation0B6F0,
    &_gActor04000Actor104000Animation09A94,
    &_gActor04000Actor104000Animation0BF20,
    &_gActor04000Actor104000Animation0C070,
    &_gActor04000Actor104000Animation0BAA0,
    &_gActor04000Actor104000Animation0BCDC,
    &_gActor04000Actor104000Animation0C1C0,
    &_gActor04000Actor104000Animation0C2EC,
    &_gActor04000Actor104000Animation0C49C,
    NULL,
};

AnimationSet* Actor04000_D0C510[4] = {
    NULL,
    &_gActor04000Actor104000Animation0A260,
    &_gActor04000Actor104000Animation0A71C,
    NULL,
};

AnimationSet* Actor04000_D0C520[4] = {
    NULL,
    &_gActor04000Actor104000Animation0AECC,
    &_gActor04000Actor104000Animation0B374,
    NULL,
};

AnimationPlayRequest Actor04000_D0C530 = { { .sets = Actor04000_D0C520 }, 1, ANIMATION_BLEND_RESET, 3, ANIMATION_WORLD_COLLISION_DISABLE };

u8 Actor04000_D0C544[364] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    6,
    0,
    0,
    0,
    0,
    0,
    9,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    6,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    6,
    6,
    6,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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

TaskMessageEntry Actor04000_D0C6B0[6] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, Actor04000_Fn06590 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, Actor04000_Fn06728 },
    { ACTOR_COMMAND_MESSAGE_APPLY, Actor04000_Fn0093C },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { 2014, Actor04000_Fn06704 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor04000_D0C6E0 = { { { TASK_BODY_TMD, 96 } }, Actor04000_Fn06E4C, { .model = &_gActor04000BloodSucklerBody } };

TaskFunc Actor04000_D0C6EC[4] = {
    Actor04000_Fn06EA8,
    Actor04000_Fn06F54,
    Actor04000_Fn06380,
    taskKill,
};

TaskDesc Actor04000_D0C6FC = { { { TASK_BODY_NONE, 96 } }, Actor04000_Fn0703C, { .value = 0 } };

SVECTOR ActorContact_ScratchPosition;

Task* Actor04000_D0C710[2];

Task* Actor04000_D0C718[8];

extern EnemyParams Actor04000_D07084;

extern AnimationSet* Actor04000_D0C4C4[19];

extern TaskMessageEntry Actor04000_D0C6B0[6];

extern AnimationPlayRequest Actor04000_D0C530;

extern PadScriptCmd Actor04000_D07094[3];

extern PadScriptVibrationSegment Actor04000_D070A0[3];

extern AnimationSet* Actor04000_D0C510[4];

extern AnimationSet* Actor04000_D0C520[4];

/// The controller task's state handlers, indexed by its `state`.
extern TaskFunc Actor04000_D0C6EC[];

static s32             Actor04000_Fn00FDC(Actor104000Work* arg0);
static void            Actor04000_Fn010B8(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn0168C(Enemy* arg0, Task* arg1);
static __inline__ void Actor204000_FaceScale(GfxCoord* coord, s16 s);
static void            Actor04000_Fn01E1C(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn026FC(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn028F0(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn02F48(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn03798(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn03D30(Task* arg0, s16 arg1, u32 arg2);
static void            Actor04000_Fn03FB4(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn0432C(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn049C0(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn04FA4(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn0522C(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn055C8(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn05AE8(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn05F0C(Enemy* arg0, Task* arg1);

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

/// Message handler: 0x1003/1 registers the actor in its lead slot and places
/// it at that slot's start point; 0x1203 and 0x302 move it to the scripted
/// positions for its slot and pick the next state.
s32 Actor04000_Fn0093C(Task* arg0, s32 arg1, Actor104000Event* event, s32 arg3)
{
    Actor104000Work* work;
    Enemy*           ctx;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (event->words[0] == 0x1003 && event->words[1] == 1) {
        work->field_0                                               = 0xE;
        Actor04000_D0C718[ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT] = arg0;
        ctx->node.state.parts.flags                                 = WORLD_TARGET_NOT_LOCKABLE;
        switch (ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
            case 0:
                arg0->extra.tmd->coords->coord.t[0]   = 0x116;
                arg0->extra.tmd->coords->coord.t[1]   = -0xBB8;
                arg0->extra.tmd->coords->coord.t[2]   = 0x6A4;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 1:
                arg0->extra.tmd->coords->coord.t[0]   = 0x2BC;
                arg0->extra.tmd->coords->coord.t[1]   = -0xBB8;
                arg0->extra.tmd->coords->coord.t[2]   = 0x56A;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 2:
                arg0->extra.tmd->coords->coord.t[0]   = -0x1E;
                arg0->extra.tmd->coords->coord.t[1]   = -0xBB8;
                arg0->extra.tmd->coords->coord.t[2]   = 0x500;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 3:
                arg0->extra.tmd->coords->coord.t[0]   = 0xB2;
                arg0->extra.tmd->coords->coord.t[1]   = -0xBB8;
                arg0->extra.tmd->coords->coord.t[2]   = 0x22E;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 4:
                arg0->extra.tmd->coords->coord.t[0]   = 0x21E;
                arg0->extra.tmd->coords->coord.t[1]   = -0xBB8;
                arg0->extra.tmd->coords->coord.t[2]   = -0xF2;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 5:
                arg0->extra.tmd->coords->coord.t[0]   = -0x46;
                arg0->extra.tmd->coords->coord.t[1]   = -0xBB8;
                arg0->extra.tmd->coords->coord.t[2]   = -0x20B;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
        }
    }
    if (event->words[0] == 0x1203) {
        switch (event->words[1]) {
            case 0:
                work->field_0 = 0;
                break;
            case 1:
                switch (ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
                    case 0:
                        work->field_0                       = 0x10;
                        arg0->extra.tmd->coords->coord.t[0] = -0x3AC;
                        arg0->extra.tmd->coords->coord.t[1] = -0xF0;
                        arg0->extra.tmd->coords->coord.t[2] = 0x166C;
                        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x3E8, 1);
                        Gp_ArmStateF0(1);
                        break;
                    case 1:
                        work->field_0                       = 0x11;
                        arg0->extra.tmd->coords->coord.t[0] = 0x2A8;
                        arg0->extra.tmd->coords->coord.t[1] = -0x7D0;
                        arg0->extra.tmd->coords->coord.t[2] = 0x189C;
                        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x800, 1);
                        break;
                }
                break;
        }
    }
    if (event->words[0] == 0x302) {
        switch (event->words[1]) {
            case 0:
                work->field_0 = 9;
                break;
            case 9:
                work->field_0 = 0;
                break;
            case 1:
                switch (ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
                    case 0:
                        arg0->extra.tmd->coords->coord.t[0]   = 0xF1E;
                        arg0->extra.tmd->coords->coord.t[1]   = -0x384;
                        arg0->extra.tmd->coords->coord.t[2]   = 0xFE6;
                        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x400, 1);
                        work->field_0 = 0x11;
                        break;
                    case 1:
                        arg0->extra.tmd->coords->coord.t[0] = 0xA1E;
                        arg0->extra.tmd->coords->coord.t[1] = -0x384;
                        arg0->extra.tmd->coords->coord.t[2] = 0x1590;
                        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x7D0, 1);
                        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                        work->field_0                         = 0x12;
                        break;
                    case 2:
                        arg0->extra.tmd->coords->coord.t[0] = 0x1A4;
                        arg0->extra.tmd->coords->coord.t[1] = -0x4C4;
                        arg0->extra.tmd->coords->coord.t[2] = 0x1194;
                        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x3E8, 1);
                        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                        work->field_0                         = 0x12;
                        break;
                    case 3:
                    case 4:
                    default:
                        work->field_0 = 0x11;
                        break;
                }
                break;
        }
    }
    return 0;
}

#include "../../shared/anim_driver_tick.inc.c"

/// Reaction check keyed on the requested animation in `field_174`: for 2 and
/// 3, answers 0x40280001 the first time the playing animation id in `field_4A`
/// reaches one of that animation's trigger ids (latched in `field_474`, which
/// clears on any other id); for 5, answers 0x400C0005 while bit 2 of
/// `field_58` is set. Answers 0 otherwise.
static s32 Actor04000_Fn00FDC(Actor104000Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->field_174) {
        case 2:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->field_474 == v) {
                goto same;
            }
            arg0->field_474 = id;
            return 0x40280001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_474 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_474 = id;
            break;
        case 5:
            if (arg0->field_58 & 2) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

// animation bank handed to `animationInitContext`

/// Spawn state: allocates the work block, links the four collision objects and
/// the enemy node, seeds the size and HP from the enemy's level nibble, records
/// the spawn position and the points 1000 units ahead and behind it, then
/// starts in state 2 when the high half of `Task::spawnArg1` is 1 and state 7 otherwise.
static void Actor04000_Fn010B8(Enemy* arg0, Task* arg1)
{
    TmdObject*             obj;
    GfxCoord*              coord;
    Actor104000Work*       work;
    WorldCollisionContact* hits;
    SVECTOR                sv;
    VECTOR                 pos;
    SVECTOR*               p;
    SVECTOR*               q;
    WorldCollisionBody*    o1;
    WorldCollisionBody*    o2;
    WorldCollisionBody*    o3;
    WorldCollisionBody*    o4;

    obj        = arg1->extra.tmd;
    coord      = obj->coords;
    work       = memCalloc(sizeof(Actor104000Work), 0);
    arg1->work = work;
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    coord->parent   = &gGfxViewCoord;
    arg1->msgTable  = Actor04000_D0C6B0;
    work->field_180 = 0;
    work->field_184 = 1;
    work->field_18C = 3;
    work->field_188 = 0;
    work->field_190 = 1;
    obj->flags      = 0;
    animationInitContext(&work->anim, Actor04000_D0C4C4, obj, (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->poses, work->slots);

    o1                   = &work->obj270;
    o1->coord            = arg1->extra.tmd->coords + 1;
    o1->context.contacts = work->rec1B0;
    o1->pos.vy           = -0x110;
    o1->pos.vx           = 0;
    o1->pos.vz           = 0;
    o1->key              = 0x3000C;
    o1->radius           = 0x190;
    o1->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, o1);
    o1->flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    Gp_InitRec18Table(o1->context.contacts, 8, 0);

    o2                   = &work->obj350;
    sv.vx                = 0;
    sv.vy                = -0x168;
    sv.vz                = 0;
    p                    = &sv;
    hits                 = work->hits;
    o2->coord            = arg1->extra.tmd->coords + 2;
    o2->context.contacts = hits;
    o2->pos.vx           = p->vx;
    o2->pos.vy           = p->vy;
    o2->pos.vz           = p->vz;
    o2->key              = 0x3000C;
    o2->radius           = 0x168;
    o2->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, o2);
    o2->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(o2->context.contacts, 8, 0);

    sv.vx                = 0;
    sv.vy                = 0;
    sv.vz                = 0;
    o3                   = &work->obj388;
    o3->coord            = &gGfxViewCoord;
    o3->context.contacts = &work->rec370;
    o3->pos.vx           = p->vx;
    o3->pos.vy           = p->vy;
    o3->pos.vz           = p->vz;
    o3->radius           = 0x500;
    o3->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, o3);
    Gp_InitRec18Table(o3->context.contacts, 1, 0);

    o4                   = &work->obj3C0;
    o4->coord            = &gGfxViewCoord;
    o4->context.contacts = &work->rec3A8;
    o4->pos.vx           = p->vx;
    o4->pos.vy           = p->vy;
    o4->pos.vz           = p->vz;
    o4->radius           = 0x80;
    o4->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(8, o4);
    Gp_InitRec18Table(o4->context.contacts, 1, 0);

    arg0->field_4    = &coord->coord;
    arg0->field_48   = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->coord      = arg1->extra.tmd->coords + 2;
    Gp_LinkNode(&arg0->node);
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    arg0->reactionFlags          = 0;
    arg0->hp = arg0->hpMax = Actor04000_D07084.hpMax;
    arg0->param            = &Actor04000_D07084;
    arg0->recs             = hits;
    work->field_170        = 2;
    work->field_174        = 1;
    work->field_176        = 0x10;
    work->field_178        = 0;
    animDriverTick(arg1);
    work->field_17E     = 0;
    work->field_A       = 0;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    work->field_1A0 = 5;
    work->field_1A2 = 0x14;
    if ((u16)(arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) % 2 == 1) {
        work->field_176 += arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_1A2 += arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_1A0 += arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    } else {
        work->field_176 -= (u16)(arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_1A2 -= (arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_1A0 -= (arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
    }
    work->origin.vx = arg1->extra.tmd->coords->coord.t[0];
    work->origin.vy = arg1->extra.tmd->coords->coord.t[1];
    work->origin.vz = arg1->extra.tmd->coords->coord.t[2];
    gfxReadMatrixZAxis(&arg1->extra.tmd->coords->coord, &sv);
    sv.vy = 0;
    q     = &sv;
    VectorNormalSS(q, q);
    gte_lddp(1000);
    gte_ldsv(q);
    gte_gpf12();
    gte_stsv(q);
    work->patrol[0].vx = arg1->extra.tmd->coords->coord.t[0] + sv.vx;
    work->patrol[0].vy = arg1->extra.tmd->coords->coord.t[1];
    work->patrol[0].vz = arg1->extra.tmd->coords->coord.t[2] + sv.vz;
    work->patrol[1].vx = arg1->extra.tmd->coords->coord.t[0] - sv.vx;
    work->patrol[1].vy = arg1->extra.tmd->coords->coord.t[1];
    work->patrol[1].vz = arg1->extra.tmd->coords->coord.t[2] - sv.vz;
    /* the gameplay prototype takes no argument, but this call site passes 0 */
    (Gp_IncStateF0Ref)(0);
    if ((arg1->spawnArg1.value >> 16) == 0) {
        work->field_0 = 7;
    } else if ((arg1->spawnArg1.value >> 16) == 1) {
        work->field_0 = 2;
    } else {
        work->field_0 = 7;
    }
    work->field_2   = -1;
    work->field_479 = 0;
    work->field_47A = 0;
    arg1->state++;
}

/// Lunge state: steps forward on frames 8 and 9, then from frame 9 on grabs
/// the player when within 600 units and a quarter turn of the facing, dispatches
/// the side-dependent grab message and snaps the model beside and facing them.
static void Actor04000_Fn0168C(Enemy* arg0, Task* arg1)
{
    Actor104000Work*       work;
    Task*                  player;
    GameActor*             actor;
    TmdObject*             obj;
    Actor104000AimScratch* head;
    Actor104000AimScratch* sc;
    GfxCoord*              coord;
    GfxCoord*              pos;
    s16                    angle;
    s32                    mag;

    work   = arg1->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor  = player->work;
    if (work->field_4 != 0) {
        obj                                                       = arg1->extra.tmd;
        ((Enemy*)arg1->spawnArg2.pointer)->node.state.parts.flags = 0;
        Gp_ArmStateF0(1);
        obj->flags          = 0;
        work->field_170     = 1;
        work->field_176     = 0x10;
        work->field_178     = 0;
        work->field_174     = 0xD;
        work->obj270.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        animDriverTick(arg1);
        work->field_6 = 0;
        return;
    }
    animDriverTick(arg1);
    work->field_6++;
    if ((s16)work->field_6 < 8) {
        return;
    }
    if ((s16)work->field_6 == 8) {
        actorStepForward(arg1->extra.tmd->coords, 0x32);
        return;
    }
    if ((s16)work->field_6 == 9) {
        actorStepForward(arg1->extra.tmd->coords, 0x32);
    }
    work->field_0 = 0xC;
    head          = SCRATCH_STACK_CURSOR(Actor104000AimScratch);
    sc            = (Actor104000AimScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor104000AimScratch));
    pos           = arg1->extra.tmd->coords;
    head[-1].d.vx = gPlayerStatus.coordMtx->t[0] - pos->coord.t[0];
    sc->d.vy      = gPlayerStatus.coordMtx->t[1] - pos->coord.t[1];
    sc->d.vz      = gPlayerStatus.coordMtx->t[2] - pos->coord.t[2];
    coord         = arg1->extra.tmd->coords;
    angle         = ratan2(head[-1].d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle     = actorWrapAngle(angle);
    if (!overlayOutOfRange(&sc->d, 600)) {
        mag = (sc->angle >= 0) ? sc->angle : -sc->angle;
        if (mag < 0x200) {
            if (actor->mode != GAME_ACTOR_MODE_SCRIPTED) {
                work->field_490 = 0xC;
                if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, work->field_47C, 0) == 0) {
                    coord     = player->extra.tmd->coords;
                    angle     = ratan2(sc->d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
                    sc->angle = actorWrapAngle(angle);
                    if (sc->angle < 0) {
                        Actor04000_D0C530.source.sets = Actor04000_D0C510;
                    } else {
                        Actor04000_D0C530.source.sets = Actor04000_D0C520;
                    }
                    Actor04000_D0C530.animationId = 1;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &Actor04000_D0C530, 0);
                    work->field_0   = 0xB;
                    work->field_496 = 1;
                    Gfx_MatrixCol0(&player->extra.tmd->coords->coord, &sc->d);
                    sc->d.vy = 0;
                    VectorNormalSS(&sc->d, &sc->d);
                    if (sc->angle < 0) {
                        gte_lddp(-0x3C);
                        gte_ldsv(&sc->d);
                        gte_gpf12();
                        gte_stsv(&sc->d);
                    } else {
                        gte_lddp(0x3C);
                        gte_ldsv(&sc->d);
                        gte_gpf12();
                        gte_stsv(&sc->d);
                    }
                    arg1->extra.tmd->coords->coord.t[0] = player->extra.tmd->coords->coord.t[0] + sc->d.vx;
                    arg1->extra.tmd->coords->coord.t[1] = player->extra.tmd->coords->coord.t[1];
                    arg1->extra.tmd->coords->coord.t[2] = player->extra.tmd->coords->coord.t[2] + sc->d.vz;
                    sc->d.vy                            = 0;
                    VectorNormalSS(&sc->d, &sc->d);
                    gte_lddp(-0x258);
                    gte_ldsv(&sc->d);
                    gte_gpf12();
                    gte_stsv(&sc->d);
                    arg1->extra.tmd->coords->coord.t[0] += sc->d.vx;
                    arg1->extra.tmd->coords->coord.t[2] += sc->d.vz;
                    sc->d.vx                             = -sc->d.vx;
                    sc->d.vy                             = -sc->d.vy;
                    sc->d.vz                             = -sc->d.vz;
                    sc->yaw                              = ratan2(sc->d.vx, sc->d.vz);
                    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, sc->yaw, 1);
                    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor104000AimScratch));
}

/// Turns `coord` to face along its own Z axis in the XZ plane and scales the
/// rotation uniformly by `s`, working on a scratch block.
static __inline__ void Actor204000_FaceScale(GfxCoord* coord, s16 s)
{
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* sc;

    head                                       = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    sc                                         = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = sc;
    sc->yaw                                    = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&sc->rotation, sc->yaw, 1);
    sc->scale.vx = sc->scale.vy = sc->scale.vz = s;
    ScaleMatrix(&sc->rotation, &head[-1].scale);
    coord->coord.m[0][0] = head[-1].rotation.m[0][0];
    coord->coord.m[0][1] = sc->rotation.m[0][1];
    coord->coord.m[0][2] = sc->rotation.m[0][2];
    coord->coord.m[1][0] = sc->rotation.m[1][0];
    coord->coord.m[1][1] = sc->rotation.m[1][1];
    coord->coord.m[1][2] = sc->rotation.m[1][2];
    coord->coord.m[2][0] = sc->rotation.m[2][0];
    coord->coord.m[2][1] = sc->rotation.m[2][1];
    coord->coord.m[2][2] = sc->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
}

/// Frames 0x5B onward of the collapse: drifts the model along its facing for the
/// first 0x13 frames, steps the effects keyed on `field_6`, then fades the colour
/// matrix out and grows the model over frames 0x5C-0x64.
static void Actor04000_Fn01E1C(Enemy* arg0, Task* arg1)
{
    SVECTOR          dir;
    SVECTOR*         d;
    VECTOR           scale;
    Actor104000Work* work;
    TmdObject*       obj;
    s16              s;
    s32              pan;
    s32              id;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    if (work->field_4 != 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   = 0;
        work->obj350.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj388.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj388.key             = Gp_PackObjPair(arg0, 1);
        work->obj3C0.key             = 0x22222;
        work->field_6                = 0;
        work->obj270.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->savedColorMtx          = work->colorMtx;
        work->field_174              = 0xE;
        work->field_170              = 1;
        work->field_178              = 0;
        animDriverTick(arg1);
        work->obj3C0.pos.vx           = arg1->extra.tmd->coords->coord.t[0];
        work->obj3C0.pos.vy           = arg1->extra.tmd->coords->coord.t[1] - 0x1F4;
        work->obj3C0.pos.vz           = arg1->extra.tmd->coords->coord.t[2];
        work->obj388.pos.vx           = arg1->extra.tmd->coords->coord.t[0];
        work->obj388.pos.vy           = arg1->extra.tmd->coords->coord.t[1];
        work->obj388.pos.vz           = arg1->extra.tmd->coords->coord.t[2];
        Actor04000_D0C530.animationId = 2;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &Actor04000_D0C530, 0);
        work->field_6 = 0;
    }
    if ((s16)work->field_6 < 0x13) {
        Gfx_MatrixCol0(&arg1->extra.tmd->coords->coord, &dir);
        d      = &dir;
        dir.vy = 0;
        VectorNormalSS(d, d);
        gte_lddp(0x15);
        gte_ldsv(d);
        gte_gpf12();
        gte_stsv(d);
        arg1->extra.tmd->coords->coord.t[0]  += dir.vx;
        arg1->extra.tmd->coords->coord.t[2]  += dir.vz;
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    animDriverTick(arg1);
    switch ((s16)(work->field_6 - 0x5B)) {
        case 0:
            if (work->field_496 == 1) {
                if (((GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work)->mode == GAME_ACTOR_MODE_SCRIPTED) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                }
                work->field_496 = 0;
            }
            arg1->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case 1:
            Gp_SpawnScript18Ex(Actor04000_D07094, Actor04000_D070A0,
                               (s16)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            work->obj388.radius = 0x3E8;
            work->obj3C0.radius = 0xFA;
            work->obj388.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->obj3C0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg1->extra.tmd->coords[2], 1, NULL);
            break;
        case 2:
            work->obj3C0.radius = 0x1F4;
            break;
        case 3:
            work->obj3C0.radius = 0x3E8;
            work->obj388.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 5:
            Gp_ReleaseStateF0Add(arg1, 0xC);
            work->obj3C0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 7:
            if ((s8)work->field_479 == 0) {
                Gp_SpawnEff(EFFECT_RED_GROUND_GLOW, arg1->extra.tmd->coords, 0, NULL);
            }
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            id         = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40280004;
            pan        = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            break;
        case 28:
            work->field_0 = 0;
            break;
        default:
            work->colorMtx = work->savedColorMtx;
            break;
    }
    work->colorMtx = work->savedColorMtx;
    if ((u16)(work->field_6 - 0x17) < 0x44) {
        work->colorMtx.t[0] += ((s16)work->field_6 - 0x16) * 0x60;
    }
    if ((u16)(work->field_6 - 0x5C) < 9) {
        s = 0xBB8 - ((s16)work->field_6 - 0x5C) * 600;
        if (s < 0x4B0) {
            scale.vx = scale.vy = scale.vz = 0;
            Actor204000_FaceScale(arg1->extra.tmd->coords, 0x1000);
            ScaleMatrix(&work->colorMtx, &scale);
            work->colorMtx.t[0] = work->colorMtx.t[1] = work->colorMtx.t[2] = 0;
            Actor204000_FaceScale(arg1->extra.tmd->coords, 0x1000);
        } else {
            scale.vx = scale.vy = scale.vz = s;
            work->colorMtx                 = work->savedColorMtx;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(s);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            s = ((s16)work->field_6 - 0x5A) * 0x400 + 0x1000;
            if (s > 0x2000) {
                s = 0x2000;
            }
            Actor204000_FaceScale(arg1->extra.tmd->coords, s);
        }
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}

/// Restarts the actor when `field_4` is set; otherwise steps it, occasionally
/// switches to state 3 on a random roll, and arms the player state when the
/// player comes within 2000 units.
static void Actor04000_Fn026FC(Enemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    GfxCoord*        coord;
    SVECTOR          delta;
    SVECTOR*         d;
    TmdObject*       obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->field_174              = 5;
        work->field_170              = 1;
        work->field_178              = 0;
        work->obj350.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj388.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj270.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        animDriverTick(arg1);
        return;
    }
    animDriverTick(arg1);
    if ((work->field_58 & 2) && work->field_17C >= 0x19) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 0x10) & 7)) {
            work->field_0 = 3;
        }
    }
    coord    = arg1->extra.tmd->coords;
    d        = &delta;
    delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy    = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz    = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!overlayOutOfRange(d, 2000)) {
        Gp_ArmStateF0(1);
        work->field_0 = 3;
    }
}

/// Chasing state: restarts the actor when `field_4` is set; otherwise turns
/// toward the player by at most 0x10 a frame and steps forward, counting
/// frames spent more than 1000 units away (state 8 after 240), and switches to
/// state 10 within 600 units and an eighth turn of the facing.
static void Actor04000_Fn028F0(Enemy* arg0, Task* arg1)
{
    Actor104000Work*  work;
    ActorTurnScratch* head;
    ActorTurnScratch* sc;
    GfxCoord*         coord;
    GfxCoord*         target;
    GfxCoord*         pos;
    TmdObject*        obj;
    s16               angle;
    s32               mag;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->field_174              = 3;
        work->field_170              = 1;
        work->field_178              = 0x10;
        work->obj350.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj388.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj270.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Gp_ArmStateF0(1);
        animDriverTick(arg1);
        work->field_494 = 0;
        return;
    }
    head = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    sc   = (ActorTurnScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(ActorTurnScratch));
    animDriverTick(arg1);
    pos               = arg1->extra.tmd->coords;
    head[-1].delta.vx = gPlayerStatus.coordMtx->t[0] - pos->coord.t[0];
    sc->delta.vy      = gPlayerStatus.coordMtx->t[1] - pos->coord.t[1];
    sc->delta.vz      = gPlayerStatus.coordMtx->t[2] - pos->coord.t[2];
    coord             = arg1->extra.tmd->coords;
    angle             = ratan2(head[-1].delta.vx, sc->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle         = actorWrapAngle(angle);
    if (sc->angle > 0x10) {
        sc->angle = 0x10;
    }
    if (sc->angle < -0x10) {
        sc->angle = -0x10;
    }
    sc->angle += ratan2(-arg1->extra.tmd->coords->coord.m[2][0], arg1->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, sc->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 0x14);
    ActorContact_PushContact(arg1->extra.tmd->coords, work->rec1B0, 8);
    if (overlayOutOfRange(&sc->delta, 1000)) {
        work->field_494++;
    } else {
        work->field_494 = 0;
    }
    ActorContact_Steer(arg1->extra.tmd->coords, work->hits, 8, &sc->delta);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    sc->delta.vx                          = work->origin.vx - arg1->extra.tmd->coords->coord.t[0];
    sc->delta.vy                          = 0;
    sc->delta.vz                          = work->origin.vz - arg1->extra.tmd->coords->coord.t[2];
    overlayOutOfRange(&sc->delta, 3000);
    if (work->field_494 > 0xF0) {
        work->field_0 = 8;
    }
    target       = arg1->extra.tmd->coords;
    sc->delta.vx = gPlayerStatus.coordMtx->t[0] - target->coord.t[0];
    sc->delta.vy = gPlayerStatus.coordMtx->t[1] - target->coord.t[1];
    sc->delta.vz = gPlayerStatus.coordMtx->t[2] - target->coord.t[2];
    coord        = arg1->extra.tmd->coords;
    angle        = ratan2(sc->delta.vx, sc->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle    = actorWrapAngle(angle);
    if (!overlayOutOfRange(&sc->delta, 600)) {
        mag = (sc->angle >= 0) ? sc->angle : -sc->angle;
        if (mag < 0x200) {
            work->field_0 = 0xA;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorTurnScratch));
}

/// Frames 0x28 onward of the collapse: steps the effects keyed on `field_6`,
/// then fades the colour matrix out and grows the model over frames 0x2A-0x32.
static void Actor04000_Fn02F48(Enemy* arg0, Task* arg1)
{
    VECTOR           scale;
    Actor104000Work* work;
    TmdObject*       obj;
    s16              s;
    s32              pan;
    s32              id;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    if (work->field_4 != 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   = 0;
        work->obj350.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj388.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj388.key             = Gp_PackObjPair(arg0, 1);
        work->obj3C0.key             = 0x22222;
        work->field_6                = 0;
        work->obj270.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->savedColorMtx          = work->colorMtx;
        work->field_178              = 0;
        animDriverTick(arg1);
        work->obj3C0.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->obj3C0.pos.vy = arg1->extra.tmd->coords->coord.t[1] - 0x1F4;
        work->obj3C0.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        work->obj388.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->obj388.pos.vy = arg1->extra.tmd->coords->coord.t[1];
        work->obj388.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        if (work->field_194 == 0x1003) {
            Actor04000_D0C718[arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT] = NULL;
        }
        return;
    }
    animDriverTick(arg1);
    switch ((s16)(work->field_6 - 0x28)) {
        case 0:
            work->obj350.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 1:
            if (work->field_496 == 1) {
                if (((GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work)->mode == GAME_ACTOR_MODE_SCRIPTED) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                }
                work->field_496 = 0;
            }
            arg1->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case 2:
            Gp_SpawnScript18Ex(Actor04000_D07094, Actor04000_D070A0,
                               (s16)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            work->obj388.radius = 0x3E8;
            work->obj3C0.radius = 0xFA;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), 0x7DA, 0, 0x7DE);
            work->obj388.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->obj3C0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg1->extra.tmd->coords[2], 1, NULL);
            break;
        case 3:
            work->obj3C0.radius = 0x1F4;
            break;
        case 4:
            work->obj3C0.radius = 0x3E8;
            break;
        case 6:
            Gp_ReleaseStateF0Add(arg1, 0xC);
            work->obj3C0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 8:
            if ((s8)work->field_479 == 0) {
                Gp_SpawnEff(EFFECT_RED_GROUND_GLOW, arg1->extra.tmd->coords, 0, NULL);
            }
            id  = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40280004;
            pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            break;
        case 10:
            work->obj388.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case 12:
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 29:
            work->field_0 = 0;
            arg0->hp      = 0;
            break;
        default:
            work->colorMtx = work->savedColorMtx;
            break;
    }
    work->colorMtx = work->savedColorMtx;
    if ((u16)(work->field_6 - 0x17) < 0x12) {
        work->colorMtx.t[0] += ((s16)work->field_6 - 0x16) * 0x60;
    }
    if ((u16)(work->field_6 - 0x2A) < 9) {
        s = 0xBB8 - ((s16)work->field_6 - 0x2A) * 600;
        if (s < 0x4B0) {
            scale.vx = scale.vy = scale.vz = 0;
            Actor204000_FaceScale(arg1->extra.tmd->coords, 0x1000);
            ScaleMatrix(&work->colorMtx, &scale);
            work->colorMtx.t[0] = work->colorMtx.t[1] = work->colorMtx.t[2] = 0;
            Actor204000_FaceScale(arg1->extra.tmd->coords, 0x1000);
        } else {
            scale.vx = scale.vy = scale.vz = s;
            work->colorMtx                 = work->savedColorMtx;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(s);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            s = ((s16)work->field_6 - 0x28) * 0x400 + 0x1000;
            if (s > 0x2000) {
                s = 0x2000;
            }
            Actor204000_FaceScale(arg1->extra.tmd->coords, s);
        }
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}

/// Death state: saves the colour matrix, then fades it and grows the model
/// over frames 13-21 while stepping through the collapse effects.
static void Actor04000_Fn03798(Enemy* arg0, Task* arg1)
{
    VECTOR           scale;
    Actor104000Work* work;
    TmdObject*       obj;
    s16              s;
    s32              pan;
    s32              id;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    if (work->field_4 != 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   = 0;
        work->obj350.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj388.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj388.key             = Gp_PackObjPair(arg0, 1);
        work->obj3C0.key             = 0x22222;
        work->field_6                = 0;
        work->obj270.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        work->savedColorMtx          = work->colorMtx;
        work->field_174              = 0xA;
        work->field_170              = 1;
        work->field_178              = 0;
        work->field_176              = 0x2C;
        animDriverTick(arg1);
        work->obj3C0.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->obj3C0.pos.vy = arg1->extra.tmd->coords->coord.t[1] - 0x1F4;
        work->obj3C0.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        work->obj388.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->obj388.pos.vy = arg1->extra.tmd->coords->coord.t[1];
        work->obj388.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        Gp_ArmStateF0(1);
        if (work->field_194 == 0x1003) {
            Actor04000_D0C718[arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT] = NULL;
        }
    }
    animDriverTick(arg1);
    switch ((s16)(work->field_6 - 0xD)) {
        case 0:
            id  = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40280004;
            pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            arg1->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case 1:
            work->obj388.radius = 0x3E8;
            work->obj388.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg1->extra.tmd->coords[2], 1, NULL);
            Gp_SpawnScript18(Actor04000_D07094, Actor04000_D070A0);
            break;
        case 2:
            Gp_ReleaseStateF0Add(arg1, 0xC);
            work->obj3C0.radius = 0xFA;
            work->obj388.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj3C0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            break;
        case 3:
            work->obj3C0.radius = 0x1F4;
            break;
        case 4:
            work->obj3C0.radius = 0x3E8;
            break;
        case 6:
            work->obj3C0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            if ((s8)work->field_479 == 0) {
                Gp_SpawnEff(EFFECT_RED_GROUND_GLOW, arg1->extra.tmd->coords, 0, NULL);
            }
            break;
        case 8:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case 10:
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 25:
            work->field_0 = 0;
            break;
    }
    if ((u16)(work->field_6 - 0xD) < 9) {
        s = 0xBB8 - ((s16)work->field_6 - 0xB) * 0x320;
        if (s < 0) {
            s = 0;
        }
        scale.vx = scale.vy = scale.vz = s;
        work->colorMtx                 = work->savedColorMtx;
        ScaleMatrix(&work->colorMtx, &scale);
        gte_lddp(s);
        gte_ldlvl(work->colorMtx.t);
        gte_gpf12();
        gte_stlvl(work->colorMtx.t);
        s = (s16)work->field_6 * 0xB4 + 0x1000;
        if (s > 0x2000) {
            s = 0x2000;
        }
        Actor204000_FaceScale(arg1->extra.tmd->coords, s);
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}

/// Picks a random offset and coordinate index for an effect from the hit
/// angle `arg1` (front, back, right or left), copies it into `work->eff` and
/// spawns the effect for hit id `arg2`.
static void Actor04000_Fn03D30(Task* arg0, s16 arg1, u32 arg2)
{
    SVECTOR*         sc;
    Actor104000Work* work;
    s32              mag;
    GfxCoord*        coord;

    sc   = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(sizeof(SVECTOR));
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->work;
    if (mag < 0x200) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            sc->pad = 2;
            sc->vx  = 80;
            sc->vy  = -180;
            sc->vz  = 330;
        } else {
            sc->pad = 2;
            sc->vx  = -60;
            sc->vy  = -150;
            sc->vz  = 300;
        }
    } else if (mag > 0x600) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            sc->pad = 1;
            sc->vx  = 0;
            sc->vy  = 0;
            sc->vz  = -180;
        } else {
            sc->pad = 2;
            sc->vx  = 2;
            sc->vy  = -50;
            sc->vz  = -50;
        }
    } else if (arg1 > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            sc->pad = 5;
            sc->vx  = 100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 5;
            sc->vx  = 120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            sc->pad = 4;
            sc->vx  = -100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 4;
            sc->vx  = -120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    }
    work->effOfs         = *sc;
    coord                = &arg0->extra.tmd->coords[sc->pad];
    work->eff.spawnArgLo = 0x100;
    work->eff.spawnArgHi = 1;
    work->eff.coord      = coord;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->extra.tmd->coords[sc->pad], &work->effOfs, &work->eff);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
}

/// Applies the first type-2 hit in `work->hits`: computes its damage, turns the
/// model toward the hit, plays the impact sound and subtracts the damage from
/// `arg0->field_40`, switching to state 6 once it runs out.
static void Actor04000_Fn03FB4(Enemy* arg0, Task* arg1)
{
    ActorHitTakenScratch*  sc;
    Actor104000Work*       work;
    WorldCollisionContact* recs;
    SVECTOR*               pos;
    s32                    mask;
    s32                    kind;
    s32                    id;
    s16                    angle;
    s32                    snd;
    s32                    pan;
    s16                    i;

    work = arg1->work;
    sc   = (ActorHitTakenScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(ActorHitTakenScratch));
    pos  = &sc->pos;
    recs = work->hits;
    i    = 0;
    mask = 0xFFFF0000;
    kind = 0x20000;
scan:
    if (recs[i].key.value == 0) {
        goto missed;
    }
    if ((recs[i].key.value & mask) == kind) {
        pos->vx = recs[i].point.vx;
        pos->vy = recs[i].point.vy;
        pos->vz = recs[i].point.vz;
        id      = recs[i].key.value;
        goto found;
    }
    i++;
    if (i < 8) {
        goto scan;
    }
missed:
    id = 0;
found:
    sc->id = id;

    if (id != 0) {
        sc->dmg                               = Gp_ComputeDamage(sc->id, 0, 0, 0x1000);
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(arg1->extra.tmd->coords);
        sc->d.vx = arg1->extra.tmd->coords->workm.t[0];
        sc->d.vy = arg1->extra.tmd->coords->workm.t[1];
        sc->d.vz = arg1->extra.tmd->coords->workm.t[2];
        sc->d.vx = sc->pos.vx - arg1->extra.tmd->coords->workm.t[0];
        sc->d.vy = sc->pos.vy - arg1->extra.tmd->coords->workm.t[1];
        sc->d.vz = sc->pos.vz - arg1->extra.tmd->coords->workm.t[2];
        angle    = ratan2(sc->d.vx, sc->d.vz) -
                ratan2(-arg1->extra.tmd->coords->workm.m[2][0], arg1->extra.tmd->coords->workm.m[2][2]);
        sc->angle = angle;
        sc->angle = actorWrapAngle(angle);
        Actor04000_Fn03D30(arg1, sc->angle, sc->id);
        snd = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40280003;
        pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
        SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
        func_800E2C78(arg0, sc->id, sc->dmg, 0);
        func_800DA6E8(&arg0->node, sc->dmg, 0);
        arg0->hp -= sc->dmg;
        if (arg0->hp <= 0) {
            work->field_0 = 6;
        }
        if (work->field_496 == 1) {
            if (((GameActor*)gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work)->mode == GAME_ACTOR_MODE_SCRIPTED) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            work->field_496 = 0;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorHitTakenScratch));
}

/// Patrol state: restarts the actor when `field_4` is set; otherwise turns the
/// model toward the current patrol point by at most 0x20 a frame and steps it
/// forward, swapping patrol points within 400 units or after 97 blocked frames,
/// switching to state 4 when the player is within 2000 units and either
/// inside a quarter turn of the facing or within 1000 units, and occasionally to
/// state 1 once `field_17C` passes 20.
static void Actor04000_Fn0432C(Enemy* arg0, Task* arg1)
{
    Actor104000Work*  work;
    ActorTurnScratch* head;
    ActorTurnScratch* sc;
    GfxCoord*         coord;
    GfxCoord*         target;
    TmdObject*        obj;
    s16               angle;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->field_174              = 2;
        work->field_170              = 1;
        work->field_178              = 0;
        work->patrolIdx              = 0;
        work->obj350.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj388.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj270.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        animDriverTick(arg1);
        work->field_6 = 0;
        return;
    }
    head              = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    sc                = (ActorTurnScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(ActorTurnScratch));
    head[-1].delta.vx = work->patrol[work->patrolIdx].vx - arg1->extra.tmd->coords->coord.t[0];
    sc->delta.vy      = 0;
    sc->delta.vz      = work->patrol[work->patrolIdx].vz - arg1->extra.tmd->coords->coord.t[2];
    coord             = arg1->extra.tmd->coords;
    angle             = ratan2(head[-1].delta.vx, sc->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle         = actorWrapAngle(angle);
    if (sc->angle > 0x20) {
        sc->angle = 0x20;
    }
    if (sc->angle < -0x20) {
        sc->angle = -0x20;
    }
    sc->angle += ratan2(-arg1->extra.tmd->coords->coord.m[2][0], arg1->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, sc->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 5);
    if (ActorContact_PushContact(arg1->extra.tmd->coords, work->rec1B0, 8)) {
        work->field_6++;
    }
    if (!overlayOutOfRange(&sc->delta, 400) || (s16)work->field_6 > 0x60) {
        if (work->patrolIdx == 0) {
            work->patrolIdx = 1;
        } else {
            work->patrolIdx = 0;
        }
        work->field_6 = 0;
    }
    ActorContact_Steer(arg1->extra.tmd->coords, work->hits, 8, &sc->delta);
    target       = arg1->extra.tmd->coords;
    sc->delta.vx = gPlayerStatus.coordMtx->t[0] - target->coord.t[0];
    sc->delta.vy = gPlayerStatus.coordMtx->t[1] - target->coord.t[1];
    sc->delta.vz = gPlayerStatus.coordMtx->t[2] - target->coord.t[2];
    if (!overlayOutOfRange(&sc->delta, 2000)) {
        coord = arg1->extra.tmd->coords;
        angle = ratan2(sc->delta.vx, sc->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (actorWrapAngle(angle) < 0x400 || !overlayOutOfRange(&sc->delta, 1000)) {
            work->field_0 = 4;
        }
    }
    animDriverTick(arg1);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((work->field_58 & 2) && work->field_17C > 0x14) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 0x10) & 7)) {
            work->field_0 = 1;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorTurnScratch));
}

/// Walking state: restarts the actor when `field_4` is set; otherwise turns the
/// model toward its spawn point by at most 0x10 a frame and steps it forward,
/// switching to state 1 within 80 units of the spawn point and to state 4 when
/// the player is within 2000 units and either inside a quarter turn of
/// the facing or within 1000 units.
static void Actor04000_Fn049C0(Enemy* arg0, Task* arg1)
{
    Actor104000Work*  work;
    ActorTurnScratch* head;
    ActorTurnScratch* sc;
    GfxCoord*         coord;
    GfxCoord*         target;
    TmdObject*        obj;
    s16               angle;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->field_174              = 2;
        work->field_170              = 1;
        work->field_178              = 0;
        work->obj350.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj388.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj270.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        animDriverTick(arg1);
        work->field_494 = 0;
        return;
    }
    head = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    sc   = (ActorTurnScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(ActorTurnScratch));
    animDriverTick(arg1);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    head[-1].delta.vx                     = work->origin.vx - arg1->extra.tmd->coords->coord.t[0];
    sc->delta.vy                          = 0;
    sc->delta.vz                          = work->origin.vz - arg1->extra.tmd->coords->coord.t[2];
    coord                                 = arg1->extra.tmd->coords;
    angle                                 = ratan2(head[-1].delta.vx, sc->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle                             = actorWrapAngle(angle);
    if (sc->angle > 0x10) {
        sc->angle = 0x10;
    }
    if (sc->angle < -0x10) {
        sc->angle = -0x10;
    }
    sc->angle += ratan2(-arg1->extra.tmd->coords->coord.m[2][0], arg1->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, sc->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 8);
    ActorContact_PushContact(arg1->extra.tmd->coords, work->rec1B0, 8);
    if (!overlayOutOfRange(&sc->delta, 80)) {
        work->field_0 = 1;
    }
    ActorContact_Steer(arg1->extra.tmd->coords, work->hits, 8, &sc->delta);
    target       = arg1->extra.tmd->coords;
    sc->delta.vx = gPlayerStatus.coordMtx->t[0] - target->coord.t[0];
    sc->delta.vy = gPlayerStatus.coordMtx->t[1] - target->coord.t[1];
    sc->delta.vz = gPlayerStatus.coordMtx->t[2] - target->coord.t[2];
    if (!overlayOutOfRange(&sc->delta, 2000)) {
        coord = arg1->extra.tmd->coords;
        angle = ratan2(sc->delta.vx, sc->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (actorWrapAngle(angle) < 0x400 || !overlayOutOfRange(&sc->delta, 1000)) {
            work->field_0 = 4;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorTurnScratch));
}

/// Restarts the actor when `field_4` is set; otherwise waits 50 frames, then
/// drops the model with growing speed, unwinding its Z roll by at most 0x92 a
/// frame, and on landing plays the impact sound and switches to state 3.
static void Actor04000_Fn04FA4(Enemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    s32              id;
    s32              pan;
    s32              rot;
    s16              step;

    work = arg1->work;
    if (work->field_4 != 0) {
        arg1->extra.tmd->flags = 0;
        work->field_174        = 5;
        work->field_170        = 1;
        work->field_178        = 0;
        work->obj350.flags    |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj388.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj270.flags    |= WORLD_COLLISION_BODY_GRID_ENABLED;
        animDriverTick(arg1);
        ratan2(-arg1->extra.tmd->coords->coord.m[2][0], arg1->extra.tmd->coords->coord.m[2][2]);
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->field_19A                       = 10;
        work->field_198                       = 0;
        work->field_6                         = 0;
        work->field_19C                       = 0x800;
        work->field_479                       = 1;
        return;
    }
    if ((s16)work->field_6 < 0x32) {
        work->field_6++;
        return;
    }
    work->field_19A                     += 4;
    work->field_198                     += work->field_19A;
    arg1->extra.tmd->coords->coord.t[1] += (s16)work->field_198;
    if (arg1->extra.tmd->coords->coord.t[1] >= -0x12B) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(3, 16, 0, 0)) {
            id  = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x53100006;
            pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
        }
        arg1->extra.tmd->coords->coord.t[1] = 0;
        work->field_0                       = 3;
        work->field_479                     = 0;
        gfxRotMatrixZ(&arg1->extra.tmd->coords->coord, -work->field_19C, GRAPHICS_ROTATION_COMPOSE);
    } else {
        step = work->field_19C;
        rot  = step;
        if (rot != 0) {
            step = -step;
            if (abs(rot) > 0x92) {
                step = (rot < 0) ? 0x92 : -0x92;
            }
            gfxRotMatrixZ(&arg1->extra.tmd->coords->coord, step, GRAPHICS_ROTATION_COMPOSE);
            work->field_19C += step;
        }
    }
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    animDriverTick(arg1);
}

/// Restarts the actor when `field_4` is set; otherwise cycles `field_6` through
/// a 32-frame loop that resets the model position, steps it back and forth
/// and changes `field_176`, then spins it and raises `field_14` in view 5.
static void Actor04000_Fn0522C(Enemy* arg0, Task* arg1)
{
    Actor104000Work* work;

    work = arg1->work;
    if (work->field_4 != 0) {
        arg1->extra.tmd->flags       = 0;
        work->obj350.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj388.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj270.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        arg0->node.state.parts.flags = 0;
        work->field_174              = 3;
        work->field_170              = 2;
        work->field_178              = 0;
        animDriverTick(arg1);
        gfxRotMatrixX(&arg1->extra.tmd->coords->coord, 0x400, GRAPHICS_ROTATION_COMPOSE);
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->field_479                       = 1;
        work->obj350.coord                    = &gGfxViewCoord;
        work->obj350.pos.vx                   = -0x3AC;
        work->obj350.pos.vy                   = -0xF0;
        work->field_6                         = 0;
        work->obj350.pos.vz                   = 0x166C;
        return;
    }
    switch ((s16)++work->field_6 % 32) {
        case 0:
            work->field_176                     = 0x40;
            arg1->extra.tmd->coords->coord.t[0] = -0x3AC;
            arg1->extra.tmd->coords->coord.t[1] = -0xF0;
            arg1->extra.tmd->coords->coord.t[2] = 0x166C;
            break;
        case 1:
        case 2:
        case 4:
        case 5:
        case 7:
        case 8:
            actorStepForward(arg1->extra.tmd->coords, -0x78);
            break;
        case 11:
        case 12:
        case 14:
            actorStepForward(arg1->extra.tmd->coords, 0xC8);
            break;
        case 17:
            work->field_176 = 0x10;
            break;
        case 25:
            work->field_176 = 8;
            break;
    }
    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, 0x44C, 1);
    gfxRotMatrixX(&arg1->extra.tmd->coords->coord, 0x190, GRAPHICS_ROTATION_COMPOSE);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    animDriverTick(arg1);
    if ((u8)Gp_GetViewIndex() == 5) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        Gp_ClearNodeSlots(&(arg0)->node);
        return;
    }
    arg0->node.state.parts.flags = 0;
}

/// Restarts the actor when `field_4` is set; otherwise runs state 0xC (drop the
/// model to the ground, then hop forward and tip it over), state 0x10 (wait for
/// the flag or 80 frames) and state 0x11 (slide along `dir` while rolling).
static void Actor04000_Fn055C8(Enemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    GfxCoord*        coord;
    SVECTOR          sv;
    s32              y;
    u16              h;
    s16              t;

    work = arg1->work;
    if (work->field_4 != 0) {
        arg1->extra.tmd->flags       = 0;
        work->obj350.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj388.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj270.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        arg0->node.state.parts.flags = 0;
        work->field_174              = 0xC;
        work->field_170              = 2;
        work->field_178              = 0;
        work->field_176              = 1;
        animDriverTick(arg1);
        animDriverTick(arg1);
        work->field_176                       = 0;
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->field_6                         = 0;
        work->field_479                       = 1;
        work->field_19A                       = 0xA;
        work->field_198                       = 0;
        work->field_6                         = 0;
        work->field_8                         = 0;
        Gfx_MatrixCol0(&arg1->extra.tmd->coords->coord, &work->dir);
        VectorNormalSS(&work->dir, &work->dir);
    }
    work->field_6++;
    switch (work->field_174) {
        case 12:
            if (work->field_176 == 0) {
                work->field_19A += 2;
                work->field_198 += work->field_19A;
                h                = work->field_198;
                coord            = arg1->extra.tmd->coords;
                y                = coord->coord.t[1];
                if (y >= 0 || (y < 0 ? -y : y) < (s16)h) {
                    coord->coord.t[1] = 0;
                    work->field_176   = 0x10;
                    work->field_6     = 0;
                } else {
                    coord->coord.t[1] = y + (s16)h;
                }
                arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                return;
            }
            animDriverTick(arg1);
            if ((s16)work->field_6 < 0xA) {
                actorStepForward(arg1->extra.tmd->coords, 0x23);
            }
            if ((s16)work->field_6 < 4) {
                arg1->extra.tmd->coords->coord.t[1] -= 0x67;
            }
            if ((u32)(work->field_6 - 4) < 8) {
                arg1->extra.tmd->coords->coord.t[1]  -= 0xB;
                arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                gfxRotMatrixX(&arg1->extra.tmd->coords->coord, -0x100, GRAPHICS_ROTATION_COMPOSE);
            }
            if ((s16)work->field_6 == 0xC) {
                work->field_170 = 2;
                work->field_174 = 0x10;
                work->field_6   = 0;
                work->field_176 = 0x10;
            }
            break;
        case 16:
            animDriverTick(arg1);
            if (!((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) & 1)) {
                t = work->field_6;
                if (t < 0x14) {
                    break;
                }
                if (t < 0x50) {
                    break;
                }
            }
            work->field_174 = 0x11;
            work->field_170 = 2;
            work->field_6   = 0;
            break;
        case 17:
            animDriverTick(arg1);
            if ((u32)(work->field_6 - 0xD) < 0x10) {
                arg1->extra.tmd->coords->coord.t[1] += 0xD;
                sv                                   = work->dir;
                gte_lddp(0x14);
                gte_ldsv(&sv);
                gte_gpf12();
                gte_stsv(&sv);
                arg1->extra.tmd->coords->coord.t[0] += sv.vx;
                arg1->extra.tmd->coords->coord.t[2] += sv.vz;
                gfxRotMatrixZ(&arg1->extra.tmd->coords->coord, -0x88, GRAPHICS_ROTATION_COMPOSE);
                arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            if (work->field_58 & 1) {
                work->field_0   = 4;
                work->field_479 = 0;
            }
            break;
    }
}

/// Resets the actor when `field_4` is set; otherwise advances the `field_6`
/// timer, stepping the model forward in three speed bands and switching to
/// state 0x11 once it passes 48.
static void Actor04000_Fn05AE8(Enemy* arg0, Task* arg1)
{
    Actor104000Work* work;

    work = arg1->work;
    if (work->field_4 != 0) {
        arg1->extra.tmd->flags       = 0;
        work->obj350.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj388.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj270.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        arg0->node.state.parts.flags = 0;
        work->field_174              = 0xB;
        work->field_170              = 2;
        work->field_178              = 0;
        work->field_176              = 1;
        animDriverTick(arg1);
        work->field_176                       = 0x10;
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->field_6                         = 0;
        work->field_479                       = 1;
        work->field_19A                       = 0xA;
        work->field_198                       = 0;
        work->field_6                         = 0;
        work->field_8                         = 0;
        return;
    }
    work->field_6++;
    animDriverTick(arg1);
    if (work->field_6 >= 0x13 && work->field_6 < 0x23) {
        actorStepForward(arg1->extra.tmd->coords, 4);
    }
    if (work->field_6 >= 0x23 && work->field_6 < 0x28) {
        actorStepForward(arg1->extra.tmd->coords, 0xC);
    }
    if (work->field_6 >= 0x28 && work->field_6 < 0x31) {
        actorStepForward(arg1->extra.tmd->coords, 0x18);
        arg1->extra.tmd->coords->coord.t[1] += 0x28;
    }
    if ((s16)work->field_6 > 0x30) {
        work->field_0 = 0x11;
    }
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

static const Actor104000StateTable Actor04000_D001F4 = {
    {
        Actor04000_Fn06A5C,
        Actor04000_Fn06BC8,
        Actor04000_Fn026FC,
        Actor04000_Fn06C80,
        Actor04000_Fn028F0,
        Actor04000_Fn02F48,
        Actor04000_Fn03798,
        Actor04000_Fn0432C,
        Actor04000_Fn049C0,
        Actor04000_Fn06AC4,
        Actor04000_Fn0168C,
        Actor04000_Fn06878,
        Actor04000_Fn06994,
        Actor04000_Fn01E1C,
        Actor04000_Fn06D38,
        Actor04000_Fn04FA4,
        Actor04000_Fn0522C,
        Actor04000_Fn055C8,
        Actor04000_Fn05AE8,
    }
};

/// Per-frame tick: tints the model from its position, draws the ground shadow
/// for the current light mode, runs the state handler (flagging a state change
/// in `field_4`), applies pending hits and plays the queued sound.
static void Actor04000_Fn05F0C(Enemy* arg0, Task* arg1)
{
    VECTOR                pos;
    SVECTOR               unused; // never written; retail's frame keeps 8 bytes here
    Actor104000StateTable table;
    GfxCoord              coord;
    Actor104000Work*      work;
    GfxRotationWords*     mw;
    s32                   snd;
    s32                   pan;
    s32                   id;

    work                                  = arg1->work;
    table                                 = Actor04000_D001F4;
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    pos.vx = arg1->extra.tmd->coords->workm.t[0];
    pos.vy = arg1->extra.tmd->coords->workm.t[1];
    pos.vz = arg1->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 5 && work->field_0 != 0xD &&
                work->field_0 != 0xF && work->field_0 != 0x10 && work->field_0 != 0x11) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x100, gRoomEffectState->groundShadowShade);
            }
            if (work->field_0 == 0xF) {
                mw                                        = (GfxRotationWords*)&coord.coord;
                mw->m00M01                                = ONE;
                ((GfxRotationWords*)&coord.coord)->m02M10 = 0;
                mw->m11M12                                = ONE;
                ((GfxRotationWords*)&coord.coord)->m20M21 = 0;
                mw->m22                                   = ONE;
                coord.coord.t[0]                          = arg1->extra.tmd->coords->coord.t[0];
                coord.coord.t[1]                          = 0;
                coord.coord.t[2]                          = arg1->extra.tmd->coords->coord.t[2];
                coord.parent                              = &gGfxViewCoord;
                coord.composeStamp                        = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&coord.workm), 0x60, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 0xD && work->field_0 != 5 &&
                work->field_0 != 0xF && work->field_0 != 0x10 && work->field_0 != 0x11) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            Gp_ClearRec18Occupied(work->rec1B0);
            Gp_ClearRec18Occupied(work->hits);
            Gp_ClearRec18Occupied(&work->rec370);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Gp_ClearRec18Occupied(work->rec1B0);
            Gp_ClearRec18Occupied(work->hits);
            Gp_ClearRec18Occupied(&work->rec370);
            return;
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    table.fn[work->field_0](arg0, arg1);
    if (work->field_496 == 1) {
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0 || arg0->hp < 0) {
            if (((GameActor*)gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work)->mode == GAME_ACTOR_MODE_SCRIPTED) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            work->field_496 = 0;
        }
    }
    if (arg0->hp > 0) {
        Actor04000_Fn03FB4(arg0, arg1);
        if (arg0->hp <= 0) {
            work->field_0 = 6;
        }
    }
    Gp_ClearRec18Occupied(work->rec1B0);
    Gp_ClearRec18Occupied(work->hits);
    Gp_ClearRec18Occupied(&work->rec370);
    id = Actor04000_Fn00FDC(work);
    if (id != 0) {
        snd = id | ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
        SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
    }
    if (gGameSession->viewReady != 0) {
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Refills the two lead slots: a slot whose actor's `field_40` has run out is
/// cleared, and an empty one takes the pooled actor in state 0xE farthest from
/// the player, moving it to state 0xF. With no candidate left the controller
/// task moves on to its next state.
void Actor04000_Fn06380(Task* arg0)
{
    s32           dist[8];
    SVECTOR       d;
    GfxCoord*     coord;
    MATRIX*       m;
    PlayerStatus* cfg;
    s32           best;
    s16           i;
    s16           j;
    s16           bi;

    bi = 0;
    for (i = 0; i < 2; i++) {
        if (Actor04000_D0C710[i] != NULL) {
            if (((Enemy*)Actor04000_D0C710[i]->spawnArg2.pointer)->hp <= 0) {
                Actor04000_D0C710[i] = NULL;
            }
            if (Actor04000_D0C710[i] != NULL) {
                continue;
            }
        }
        j   = 0;
        cfg = &gPlayerStatus;
        for (; j < 6; j++) {
            if (Actor04000_D0C718[j] != NULL && ((Actor104000Work*)Actor04000_D0C718[j]->work)->field_0 == 0xE) {
                coord    = Actor04000_D0C718[j]->extra.tmd->coords;
                m        = cfg->coordMtx;
                d.vx     = m->t[0] - coord->coord.t[0];
                d.vy     = m->t[1] - coord->coord.t[1];
                d.vz     = m->t[2] - coord->coord.t[2];
                dist[j]  = d.vx * d.vx;
                dist[j] += d.vz * d.vz;
            } else {
                dist[j] = -1;
            }
        }
        best = -1;
        for (j = 0; j < 6; j++) {
            if (best < dist[j]) {
                best = dist[j];
                bi   = j;
            }
        }
        if (best == -1) {
            arg0->state++;
            return;
        }
        ((Actor104000Work*)Actor04000_D0C718[bi]->work)->field_0 = 0xF;
        Actor04000_D0C710[i]                                     = Actor04000_D0C718[bi];
        Actor04000_D0C718[bi]                                    = NULL;
    }
}

/// Handler for message 0x7D5: sets the display object's visibility bits and
/// the work block's state from a mode. 0 shows the object (0x80) and sets state
/// 7, 1 hides it and sets state 7, 2 raises the lost-model flag (4) and clears
/// the state, 3 hides it, clears the state and then raises the flag, and any
/// other mode leaves both alone. Always answers 0.
s32 Actor04000_Fn06590(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    Actor104000Work* work;
    TmdObject*       obj;

    obj  = task->extra.tmd;
    work = (Actor104000Work*)task->work;

    switch (arg2) {
        case 0:
            obj->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->field_0 = 7;
            break;
        case 1:
            obj->flags    = 0;
            work->field_0 = 7;
            break;
        case 2:
            obj->flags    = (u16)(obj->flags | TMD_OBJECT_SKIP_AUTO_BUFFER);
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags    = (u16)(obj->flags | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
        default:
            return 0;
    }
    return 0;
}

#include "../../shared/actor_messages_place.inc.c"

/// Message handler for 0x7DE: advances the work block's state from 0xB to 0xD
/// and leaves any other state alone.
s32 Actor04000_Fn06704(Task* arg0, s32 arg1, void* arg2, s32 arg3)
{
    Actor104000Work* work;

    work = arg0->work;
    if (work->field_0 == 0xB) {
        work->field_0 = 0xD;
    }
    return 1;
}

/// Handler for message 0x7D3: latches the animation id the sender asks for and
/// picks motion state 2 when its flag is clear, 1 otherwise. Always answers 1.
s32 Actor04000_Fn06728(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    Actor104000Work* work = (Actor104000Work*)task->work;

    work->field_174 = msg->animationId;
    if (msg->blend == ANIMATION_BLEND_RESET) {
        work->field_170 = 2;
    } else {
        work->field_170 = 1;
    }
    return 1;
}

#include "../../shared/coord_math_yaw_scale.inc.c"

static void Actor04000_Fn06878(Enemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->field_176              = 0x10;
        work->field_178              = 0;
        work->obj270.flags           = (u16)(work->obj270.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        animDriverTick(arg1);
        work->field_6 = 0;
        return;
    }
    animDriverTick(arg1);
    if (!(work->field_6 & 7)) {
        Gp_SpawnPadLerp(3, 0xFF, 8);
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(arg0, 0), 0);
    }
    work->field_6++;
    if ((s16)work->field_6 > 0x28) {
        work->field_490 = 0x270F;
        if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, work->field_47C, 0) == 0) {
            work->field_0 = 5;
        } else {
            work->field_0 = 0xD;
        }
    }
}

static void Actor04000_Fn06994(Enemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                                                       = arg1->extra.tmd;
        ((Enemy*)arg1->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        work->field_170                                           = 2;
        work->field_176                                           = 0x10;
        work->field_178                                           = 0;
        work->field_174                                           = 0xF;
        work->obj270.flags                                        = (u16)(work->obj270.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        animDriverTick(arg1);
        work->field_6   = 0;
        work->field_47A = (u8)(work->field_47A + 1);
    }
    animDriverTick(arg1);
    if (work->field_58 & 1) {
        work->field_0 = 4;
    }
    if ((s8)work->field_47A >= 4) {
        work->field_0 = 5;
    }
}

/// State 0 of the actor's per-frame dispatch: on the frame the state is
/// entered (`field_4` latch) it marks the enemy not lockable, raises the
/// display object's 0x80 bit, and clears the gate bits of the work block's
/// four collision objects (the high bit on three, 0x4000 on `obj270`).
static void Actor04000_Fn06A5C(Enemy* enemy, Task* task)
{
    Actor104000Work* work;
    TmdObject*       obj;

    work = (Actor104000Work*)task->work;
    if (work->field_4 != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = (u16)(obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->obj350.flags            = (u16)(work->obj350.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->obj388.flags            = (u16)(work->obj388.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->obj3C0.flags            = (u16)(work->obj3C0.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->obj270.flags            = (u16)(work->obj270.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
    }
}

static void Actor04000_Fn06AC4(Enemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    if (work->field_4 != 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->field_174              = 1;
        work->field_170              = 2;
        work->obj350.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj388.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj270.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        return;
    }
    animDriverTick(arg1);
    switch (arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
        case 6:
        case 7:
            arg0->node.state.parts.flags = 0;
            obj->flags                   = 0;
            break;
        case 3:
        case 4:
        case 5:
        default:
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        work->field_0 = 7;
    }
}

static void Actor04000_Fn06BC8(Enemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->field_174              = 4;
        work->field_170              = 1;
        work->field_178              = 0;
        work->obj350.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj388.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj270.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        animDriverTick(arg1);
        return;
    }
    animDriverTick(arg1);
    if (work->field_58 & 1) {
        work->field_0 = 2;
    }
}

static void Actor04000_Fn06C80(Enemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->field_174              = 6;
        work->field_170              = 1;
        work->field_178              = 0;
        work->obj350.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj388.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj270.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        animDriverTick(arg1);
        return;
    }
    animDriverTick(arg1);
    if (work->field_58 & 1) {
        work->field_0 = 7;
    }
}

static void Actor04000_Fn06D38(Enemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    s16              angle;

    work = arg1->work;
    if (work->field_4 != 0) {
        arg1->extra.tmd->flags       = 0;
        arg0->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
        work->field_174              = 1;
        work->field_170              = 2;
        work->field_178              = 0;
        work->obj350.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj388.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3C0.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj270.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        animDriverTick(arg1);
        angle = ratan2(-arg1->extra.tmd->coords->coord.m[2][0], arg1->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixZ(&arg1->extra.tmd->coords->coord, 0x800, GRAPHICS_ROTATION_REPLACE);
        gfxRotMatrixY(&arg1->extra.tmd->coords->coord, angle, 0);
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        return;
    }
    animDriverTick(arg1);
}

/// The enemy's spawn, per-frame and teardown handlers, indexed by the task's
/// state.
static const EnemyTaskFuncTable3 Actor04000_D00240 = {
    Actor04000_Fn010B8,
    Actor04000_Fn05F0C,
    enemyDestroy,
};

/// The enemy task's callback: runs the handler for the task's current state,
/// copying the table onto the stack before the call.
void Actor04000_Fn06E4C(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor04000_D00240;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

void Actor04000_Fn06EA8(Task* arg0)
{
    ActorCommand msg;
    s16          i;

    for (i = 0; i < 6; i++) {
        Actor04000_D0C718[i] = NULL;
    }
    Actor04000_D0C710[1]  = NULL;
    msg.context.loc.stage = 3;
    msg.context.loc.area  = 0x10;
    Actor04000_D0C710[0]  = NULL;
    msg.command           = 1;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
    arg0->state++;
}

void Actor04000_Fn06F54(Task* arg0)
{
    s16 i;

    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        for (i = 0; i < 6; i++) {
            if (Actor04000_D0C718[i] != NULL) {
                ((Enemy*)Actor04000_D0C718[i]->spawnArg2.pointer)->node.state.parts.flags = 0;
            }
        }
        arg0->state++;
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == 5) {
        for (i = 0; i < 6; i++) {
            if (Actor04000_D0C718[i] != NULL) {
                Gp_ArmStateF0(1);
                return;
            }
        }
    }
}

void Actor04000_Fn0703C(Task* arg0)
{
    Actor04000_D0C6EC[arg0->state](arg0);
}
