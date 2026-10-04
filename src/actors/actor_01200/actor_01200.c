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
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
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
#include "../../shared/coord_math.h"
#include "../../shared/actor_messages.h"
#include "../../shared/anim_driver.h"
/// This file's `ActorContact_Steer` returns `s16`.
///
/// Defined before `actor_contacts.h`, which otherwise declares the return as
/// `s32`. Callers here compare the return with 1, and that comparison
/// sign-extends a 16-bit result.
#define ACTOR_CONTACT_STEER_RESULT s16
#include "../../shared/actor_contacts.h"

/// Work block this overlay parks in `Task::work`. `field_0` is the
/// substate the message handler below switches on; the three bytes at 0x194
/// are the message echo the dispatcher copies in for every 0xB02 message.
typedef struct Actor01200Work {
    /* 0x000 */ s16                   field_0;
    /* 0x002 */ s16                   field_2;
    /* 0x004 */ s16                   field_4;
    /* 0x006 */ s16                   field_6; // frame counter within the substate
    /* 0x008 */ s16                   field_8;
    /* 0x00A */ byte                  pad_A[2];
    /* 0x00C */ AnimationContext      anim;
    /* 0x020 */ AnimationSlot         slots[1]; // `animationInitContext` slots; later slots overlap the fields below
    /* 0x048 */ byte                  pad_48[0x2];
    /* 0x04A */ u16                   field_4A; // low ten bits: slot 1's animation id
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
    /* 0x180 */ byte                  pad_180[0x14];
    /* 0x194 */ u8                    field_194;
    /* 0x195 */ u8                    field_195;
    /* 0x196 */ u8                    field_196;
    /* 0x197 */ byte                  pad_197[1];
    /* 0x198 */ u16                   field_198;
    /* 0x19A */ u16                   field_19A;
    /* 0x19C */ byte                  pad_19C[0xC];
    /* 0x1A8 */ EffectSpawnArg        eff1A8; // `func_800FDB18`'s argument record
    /* 0x1B0 */ SVECTOR               effOfs; // offset handed to `func_800FDB18`; `pad` picks the coordinate
    /* 0x1B8 */ WorldCollisionContact rootContacts[5];
    /* 0x230 */ WorldCollisionBody    obj230;
    /* 0x250 */ WorldCollisionContact jointContacts[5];
    /* 0x2C8 */ WorldCollisionBody    obj2C8;
    /* 0x2E8 */ WorldCollisionContact rec2E8;
    /* 0x300 */ WorldCollisionBody    obj300;
    /* 0x320 */ WorldCollisionContact sensorContacts[1]; // Single result for the linked sensor body
    /* 0x338 */ WorldCollisionBody    obj338;
    /* 0x358 */ SVECTOR               origin;            // model position at spawn
    /* 0x360 */ SVECTOR               patrol[2];         // spawn position plus (0) / minus (1) 1000 units along the facing (XZ)
    /* 0x370 */ s16                   patrolIdx;
    /* 0x372 */ byte                  pad_372[2];
    /* 0x374 */ MATRIX                lightMtx;      // installed at `TmdObject.lightMtx`
    /* 0x394 */ MATRIX                colorMtx;
    /* 0x3B4 */ MATRIX                savedColorMtx; // colorMtx as it was on entering the death state
    /* 0x3D4 */ u16                   field_3D4;     // last animation id the sound check reported
    /* 0x3D6 */ byte                  pad_3D6[0x2];
    /* 0x3D8 */ s8                    field_3D8;     // nonzero rebuilds the color matrix each tick
    /* 0x3D9 */ byte                  pad_3D9[3];
    /* 0x3DC */ s16                   field_3DC;
    /* 0x3DE */ byte                  pad_3DE[2];
} Actor01200Work;
STATIC_ASSERT_SIZEOF(Actor01200Work, 0x3E0);

/// The ten substate handlers the tick copies onto its stack before dispatching.
typedef struct Actor01200StateTable {
    /* 0x00 */ EnemyTaskFunc fn[10];
} Actor01200StateTable;

extern EnemyParams               Actor01200_D04034;
extern PadScriptCmd              Actor01200_D04044[3];
extern PadScriptVibrationSegment Actor01200_D04050[3];
extern AnimationSet*             Actor01200_D06F98[19]; // animation bank handed to `animationInitContext`
// Typed callback views for the task message dispatcher.

extern TaskMessageEntry Actor01200_D07058[4];

/// Integer part of the last movement step `ActorContact_PushContact` applied.
extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

static void Actor01200_Fn03D58(Enemy* arg0, Task* arg1);
static void Actor01200_Fn03DC0(Enemy* arg0, Task* arg1);
static void Actor01200_Fn03E78(Enemy* arg0, Task* arg1);
static void Actor01200_Fn03F30(Enemy* arg0, Task* arg1);

static TmdSource _gActor01200BoneSucklerBody;
void             Actor01200_Fn03FD4(Task*);

s32 Actor01200_Fn03A00(Task*, s32, s32, s32);
s32 Actor01200_Fn03ABC(Task* task, s32 msgId, ActorCommand* request, s32 arg3);

DamageAttack Actor01200_D04030[1] = {
    { 24, 7 },
};

EnemyParams Actor01200_D04034 = { Actor01200_D04030, 1, 6, 20, 3, 100, 0, 100, 0 };

PadScriptCmd Actor01200_D04044[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment Actor01200_D04050[3] = {
    { 0, 0, 9, 0 },
    { 255, 255, 12, 1 },
    { 100, 50, 6, 1 },
};

static TmdBone _gActor01200BoneSucklerBodySkeleton[6] = {
#include "assets/bone_suckler_body_skeleton.inc"
};

static u32 _gActor01200BoneSucklerBodyPartVerts[6] = {
#include "assets/bone_suckler_body_partVerts.inc"
};

static SVECTOR _gActor01200BoneSucklerBodyVerts[102] = {
#include "assets/bone_suckler_body_verts.inc"
};

static SVECTOR _gActor01200BoneSucklerBodyNormals[139] = {
#include "assets/bone_suckler_body_normals.inc"
};

static u32 _gActor01200BoneSucklerBodyStream[1048] = {
#include "assets/bone_suckler_body_stream.inc"
};

static TmdSource _gActor01200BoneSucklerBody = {
    0,
    5984,
    1264,
    6,
    _gActor01200BoneSucklerBodyPartVerts,
    _gActor01200BoneSucklerBodyVerts,
    _gActor01200BoneSucklerBodyNormals,
    _gActor01200BoneSucklerBodySkeleton,
    _gActor01200BoneSucklerBodyStream,
};

static AnimationPackedPose _gActor01200Actor101200Animation05A94Bank1[4] = {
#include "assets/actor_101200_animation_05A94_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation05A94Bank4[21] = {
#include "assets/actor_101200_animation_05A94_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation05A94Records[43] = {
#include "assets/actor_101200_animation_05A94_records.inc"
};

static u16 _gActor01200Actor101200Animation05A94Indices[6] = {
#include "assets/actor_101200_animation_05A94_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation05A94 = {
    _gActor01200Actor101200Animation05A94Records,
    _gActor01200Actor101200Animation05A94Indices,
    { NULL, _gActor01200Actor101200Animation05A94Bank1, NULL, NULL, _gActor01200Actor101200Animation05A94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation05D58Bank1[19] = {
#include "assets/actor_101200_animation_05D58_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation05D58Bank4[36] = {
#include "assets/actor_101200_animation_05D58_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation05D58Records[71] = {
#include "assets/actor_101200_animation_05D58_records.inc"
};

static u16 _gActor01200Actor101200Animation05D58Indices[6] = {
#include "assets/actor_101200_animation_05D58_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation05D58 = {
    _gActor01200Actor101200Animation05D58Records,
    _gActor01200Actor101200Animation05D58Indices,
    { NULL, _gActor01200Actor101200Animation05D58Bank1, NULL, NULL, _gActor01200Actor101200Animation05D58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation05FFCBank1[20] = {
#include "assets/actor_101200_animation_05FFC_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation05FFCBank4[31] = {
#include "assets/actor_101200_animation_05FFC_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation05FFCRecords[65] = {
#include "assets/actor_101200_animation_05FFC_records.inc"
};

static u16 _gActor01200Actor101200Animation05FFCIndices[6] = {
#include "assets/actor_101200_animation_05FFC_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation05FFC = {
    _gActor01200Actor101200Animation05FFCRecords,
    _gActor01200Actor101200Animation05FFCIndices,
    { NULL, _gActor01200Actor101200Animation05FFCBank1, NULL, NULL, _gActor01200Actor101200Animation05FFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation061DCBank1[6] = {
#include "assets/actor_101200_animation_061DC_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation061DCBank4[38] = {
#include "assets/actor_101200_animation_061DC_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation061DCRecords[51] = {
#include "assets/actor_101200_animation_061DC_records.inc"
};

static u16 _gActor01200Actor101200Animation061DCIndices[6] = {
#include "assets/actor_101200_animation_061DC_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation061DC = {
    _gActor01200Actor101200Animation061DCRecords,
    _gActor01200Actor101200Animation061DCIndices,
    { NULL, _gActor01200Actor101200Animation061DCBank1, NULL, NULL, _gActor01200Actor101200Animation061DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation06314Bank1[4] = {
#include "assets/actor_101200_animation_06314_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation06314Bank4[13] = {
#include "assets/actor_101200_animation_06314_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation06314Records[40] = {
#include "assets/actor_101200_animation_06314_records.inc"
};

static u16 _gActor01200Actor101200Animation06314Indices[6] = {
#include "assets/actor_101200_animation_06314_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation06314 = {
    _gActor01200Actor101200Animation06314Records,
    _gActor01200Actor101200Animation06314Indices,
    { NULL, _gActor01200Actor101200Animation06314Bank1, NULL, NULL, _gActor01200Actor101200Animation06314Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation064B4Bank1[7] = {
#include "assets/actor_101200_animation_064B4_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation064B4Bank4[28] = {
#include "assets/actor_101200_animation_064B4_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation064B4Records[42] = {
#include "assets/actor_101200_animation_064B4_records.inc"
};

static u16 _gActor01200Actor101200Animation064B4Indices[6] = {
#include "assets/actor_101200_animation_064B4_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation064B4 = {
    _gActor01200Actor101200Animation064B4Records,
    _gActor01200Actor101200Animation064B4Indices,
    { NULL, _gActor01200Actor101200Animation064B4Bank1, NULL, NULL, _gActor01200Actor101200Animation064B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation066C8Bank1[6] = {
#include "assets/actor_101200_animation_066C8_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation066C8Bank4[42] = {
#include "assets/actor_101200_animation_066C8_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation066C8Records[60] = {
#include "assets/actor_101200_animation_066C8_records.inc"
};

static u16 _gActor01200Actor101200Animation066C8Indices[6] = {
#include "assets/actor_101200_animation_066C8_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation066C8 = {
    _gActor01200Actor101200Animation066C8Records,
    _gActor01200Actor101200Animation066C8Indices,
    { NULL, _gActor01200Actor101200Animation066C8Bank1, NULL, NULL, _gActor01200Actor101200Animation066C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation06AD8Bank1[17] = {
#include "assets/actor_101200_animation_06AD8_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation06AD8Bank4[85] = {
#include "assets/actor_101200_animation_06AD8_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation06AD8Records[111] = {
#include "assets/actor_101200_animation_06AD8_records.inc"
};

static u16 _gActor01200Actor101200Animation06AD8Indices[6] = {
#include "assets/actor_101200_animation_06AD8_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation06AD8 = {
    _gActor01200Actor101200Animation06AD8Records,
    _gActor01200Actor101200Animation06AD8Indices,
    { NULL, _gActor01200Actor101200Animation06AD8Bank1, NULL, NULL, _gActor01200Actor101200Animation06AD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation06D98Bank1[12] = {
#include "assets/actor_101200_animation_06D98_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation06D98Bank4[52] = {
#include "assets/actor_101200_animation_06D98_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation06D98Records[75] = {
#include "assets/actor_101200_animation_06D98_records.inc"
};

static u16 _gActor01200Actor101200Animation06D98Indices[6] = {
#include "assets/actor_101200_animation_06D98_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation06D98 = {
    _gActor01200Actor101200Animation06D98Records,
    _gActor01200Actor101200Animation06D98Indices,
    { NULL, _gActor01200Actor101200Animation06D98Bank1, NULL, NULL, _gActor01200Actor101200Animation06D98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation06F70Bank1[5] = {
#include "assets/actor_101200_animation_06F70_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation06F70Bank4[19] = {
#include "assets/actor_101200_animation_06F70_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation06F70Records[71] = {
#include "assets/actor_101200_animation_06F70_records.inc"
};

static u16 _gActor01200Actor101200Animation06F70Indices[6] = {
#include "assets/actor_101200_animation_06F70_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation06F70 = {
    _gActor01200Actor101200Animation06F70Records,
    _gActor01200Actor101200Animation06F70Indices,
    { NULL, _gActor01200Actor101200Animation06F70Bank1, NULL, NULL, _gActor01200Actor101200Animation06F70Bank4, NULL, NULL, NULL },
};

AnimationSet* Actor01200_D06F98[19] = {
    NULL,
    &_gActor01200Actor101200Animation05A94,
    &_gActor01200Actor101200Animation05D58,
    &_gActor01200Actor101200Animation05FFC,
    &_gActor01200Actor101200Animation061DC,
    &_gActor01200Actor101200Animation06314,
    &_gActor01200Actor101200Animation064B4,
    &_gActor01200Actor101200Animation066C8,
    &_gActor01200Actor101200Animation06AD8,
    &_gActor01200Actor101200Animation06D98,
    &_gActor01200Actor101200Animation06F70,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

u8 Actor01200_D06FE4[116] = {
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
};

TaskMessageEntry Actor01200_D07058[4] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, Actor01200_Fn03A00 },
    { ACTOR_COMMAND_MESSAGE_APPLY, Actor01200_Fn03ABC },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor01200_D07078 = { { { TASK_BODY_TMD, 96 } }, Actor01200_Fn03FD4, { .model = &_gActor01200BoneSucklerBody } };

SVECTOR ActorContact_ScratchPosition = { 0 };

static s32             Actor01200_Fn00990(Actor01200Work* arg0);
static void            Actor01200_Fn00A6C(Enemy* arg0, Task* arg1);
static void            Actor01200_Fn01040(Enemy* arg0, Task* arg1);
static void            Actor01200_Fn01234(Enemy* arg0, Task* arg1);
static __inline__ void Actor01200_FaceScale(GfxCoord* coord, s16 s);
static void            Actor01200_Fn017DC(Enemy* arg0, Task* arg1);
static void            Actor01200_Fn01FDC(Enemy* arg0, Task* arg1);
static void            Actor01200_Fn026A0(Task* arg0, s16 arg1, u32 arg2);
static void            Actor01200_Fn02918(Enemy* arg0, Task* arg1);
static void            Actor01200_Fn02BE8(Enemy* arg0, Task* arg1);
static void            Actor01200_Fn03294(Enemy* arg0, Task* arg1);
static void            Actor01200_Fn036B0(Enemy* arg0, Task* arg1);

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/anim_driver_tick.inc.c"

/// Sound check for the tick: for animations 2 and 3 (`field_174`) it returns
/// sound 0x400C0001 the first time the low ten bits of `field_4A` reach one of
/// that animation's two trigger values, latching the value in `field_3D4` so
/// it reports once; for animation 4 it returns 0x400C0005 while bit 0 of
/// `field_58` is set. Returns 0 otherwise.
static s32 Actor01200_Fn00990(Actor01200Work* arg0)
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
            if (arg0->field_3D4 == v) {
                goto same;
            }
            arg0->field_3D4 = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_3D4 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_3D4 = id;
            break;
        case 4:
            if (arg0->field_58 & 1) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

static void Actor01200_Fn00A6C(Enemy* arg0, Task* arg1)
{
    TmdObject*             obj;
    GfxCoord*              coord;
    GfxCoord*              part;
    Actor01200Work*        work;
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
    work       = memCalloc(sizeof(Actor01200Work), 0);
    arg1->work = work;
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->msgTable = Actor01200_D07058;
    coord->parent  = &gGfxViewCoord;
    obj->flags     = 0;
    animationInitContext(&work->anim, Actor01200_D06F98, obj, (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->poses, work->slots);

    o1                   = &work->obj230;
    o1->context.contacts = work->rootContacts;
    o1->pos.vy           = -0x34;
    o1->coord            = coord;
    o1->pos.vx           = 0;
    o1->pos.vz           = 0;
    o1->key              = 0x3000C;
    o1->radius           = 0xB4;
    o1->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, o1);
    o1->flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    Gp_InitRec18Table(o1->context.contacts, 5, 0);

    o2                   = &work->obj2C8;
    sv.vx                = 0;
    sv.vy                = -0x168;
    sv.vz                = 0;
    p                    = &sv;
    hits                 = work->jointContacts;
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
    Gp_InitRec18Table(o2->context.contacts, 5, 0);

    o3                   = &work->obj300;
    sv.vx                = 0;
    sv.vy                = 0;
    sv.vz                = 0;
    o3->coord            = &gGfxViewCoord;
    o3->context.contacts = &work->rec2E8;
    o3->pos.vx           = p->vx;
    o3->pos.vy           = p->vy;
    o3->pos.vz           = p->vz;
    o3->radius           = 0x500;
    o3->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, o3);
    Gp_InitRec18Table(o3->context.contacts, 1, 0);

    o4                   = &work->obj338;
    o4->coord            = &gGfxViewCoord;
    o4->context.contacts = work->sensorContacts;
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
    arg0->hp = arg0->hpMax = 1;
    arg0->reactionFlags    = 0;
    arg0->hp = arg0->hpMax = Actor01200_D04034.hpMax;
    arg0->param            = &Actor01200_D04034;
    arg0->recs             = hits;
    work->field_170        = 2;
    work->field_174        = 1;
    work->field_176        = 0x10;
    work->field_178        = 0;
    animDriverTick(arg1);
    work->field_17E     = 0;
    work->field_8       = 0;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    work->field_198 = 5;
    work->field_19A = 0x14;
    if ((u16)(arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) % 2 == 1) {
        work->field_176 += arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_19A += arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_198 += arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    } else {
        work->field_176 -= (u16)(arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_19A -= (arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_198 -= (arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
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
    (Gp_IncStateF0Ref)(0);
    if ((arg1->spawnArg1.value >> 16) == 0) {
        work->field_0 = 7;
    } else if ((arg1->spawnArg1.value >> 16) == 1) {
        work->field_0 = 2;
    } else {
        work->field_0 = 7;
    }
    work->field_2           = -1;
    part                    = arg1->extra.tmd->coords;
    work->eff1A8.spawnArgLo = 0x80;
    work->eff1A8.spawnArgHi = 2;
    work->eff1A8.coord      = part + 1;
    arg1->state++;
}

static void Actor01200_Fn01040(Enemy* arg0, Task* arg1)
{
    Actor01200Work* work;
    GfxCoord*       coord;
    SVECTOR         delta;
    SVECTOR*        d;
    TmdObject*      obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->field_174              = 5;
        work->field_170              = 1;
        work->field_178              = 0;
        work->obj2C8.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj300.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj338.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj230.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
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

static void Actor01200_Fn01234(Enemy* arg0, Task* arg1)
{
    Actor01200Work*   work;
    GfxCoord*         coord;
    GfxCoord*         facing;
    GfxCoord*         part;
    TmdObject*        obj;
    ActorTurnScratch* head;
    ActorTurnScratch* s;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->field_174              = 3;
        work->field_170              = 1;
        work->field_178              = 0x10;
        work->obj2C8.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj300.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj338.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj230.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        animDriverTick(arg1);
        work->field_3DC = 0;
        Gp_ArmStateF0(1);
        return;
    }
    head                                   = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_CURSOR(ActorTurnScratch) = head - 1;
    s                                      = head - 1;
    animDriverTick(arg1);
    coord             = arg1->extra.tmd->coords;
    head[-1].delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    s->delta.vy       = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    s->delta.vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    facing            = arg1->extra.tmd->coords;
    s->angle          = actorNormalizeYaw(ratan2(head[-1].delta.vx, s->delta.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]));
    if (s->angle > 0x10) {
        s->angle = 0x10;
    }
    if (s->angle < -0x10) {
        s->angle = -0x10;
    }
    part      = arg1->extra.tmd->coords;
    s->angle += ratan2(-part->coord.m[2][0], part->coord.m[2][2]);
    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, s->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 0x14);
    ActorContact_PushContact(arg1->extra.tmd->coords, work->rootContacts, 5);
    if (overlayOutOfRange(&s->delta, 1000)) {
        work->field_3DC++;
    } else {
        work->field_3DC = 0;
    }
    if (!overlayOutOfRange(&s->delta, 1000)) {
        work->field_8++;
    } else {
        work->field_8 = 0;
    }
    if (work->field_8 >= 0x15) {
        work->field_0 = 5;
    }
    if (ActorContact_Steer(arg1->extra.tmd->coords, work->jointContacts, 5, &s->delta) == 1) {
        work->field_0 = 6;
    }
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    s->delta.vx                           = work->origin.vx - arg1->extra.tmd->coords->coord.t[0];
    s->delta.vy                           = 0;
    s->delta.vz                           = work->origin.vz - arg1->extra.tmd->coords->coord.t[2];
    overlayOutOfRange(&s->delta, 3000);
    if (work->field_3DC >= 0xF1) {
        work->field_0 = 8;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

static __inline__ void Actor01200_FaceScale(GfxCoord* coord, s16 s)
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

static void Actor01200_Fn017DC(Enemy* arg0, Task* arg1)
{
    SVECTOR         ofs;
    VECTOR          scale;
    Actor01200Work* work;
    TmdObject*      obj;
    s16             s;
    s32             pan;
    s32             id;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    memset(&ofs, 0, 8);
    if (work->field_4 != 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   = 0;
        work->obj2C8.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj300.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj338.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj300.key             = Gp_PackObjPair(arg0, 0);
        work->obj338.key             = 0x22121;
        work->field_6                = 0;
        work->obj230.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->savedColorMtx          = work->colorMtx;
        work->field_174              = 0xA;
        work->field_170              = 1;
        work->field_178              = 8;
        animDriverTick(arg1);
        work->obj338.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->obj338.pos.vy = arg1->extra.tmd->coords->coord.t[1] - 0x190;
        work->obj338.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        work->obj300.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->obj300.pos.vy = arg1->extra.tmd->coords->coord.t[1];
        work->obj300.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        return;
    }
    animDriverTick(arg1);
    switch ((s16)(work->field_6 - 0x29)) {
        case 0:
            arg1->extra.tmd->flags |= TMD_OBJECT_SEMI_TRANS;
            ofs.vx                  = 0x1E;
            ofs.vz                  = 0x1E;
            ofs.vy                  = -0xA;
            Gp_SpawnEff(EFFECT_030, arg1->extra.tmd->coords, 0x10100, &ofs);
            ofs.vy = -0x14;
            ofs.vz = -0x50;
            Gp_SpawnEff(EFFECT_030, arg1->extra.tmd->coords, 0x10100, &ofs);
            break;
        case 1:
            work->eff1A8.coord      = &arg1->extra.tmd->coords[4];
            work->eff1A8.spawnArgLo = 0x120;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->extra.tmd->coords[4], NULL, &work->eff1A8);
            Gp_SpawnScript18Ex(Actor01200_D04044, Actor01200_D04050, (s16)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            work->obj300.radius = 0x320;
            work->obj338.radius = 0xC8;
            work->obj300.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->obj338.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg1->extra.tmd->coords[2], 1, NULL);
            break;
        case 2:
            work->obj338.radius = 0x190;
            work->obj300.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 3:
            work->eff1A8.coord      = &arg1->extra.tmd->coords[1];
            work->eff1A8.spawnArgLo = 0x80;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->extra.tmd->coords[1], NULL, &work->eff1A8);
            work->obj338.radius = 0x320;
            break;
        case 5:
            work->obj338.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 7:
            work->eff1A8.coord      = &arg1->extra.tmd->coords[1];
            work->eff1A8.spawnArgLo = 0x200;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->extra.tmd->coords[1], NULL, &work->eff1A8);
            Gp_SpawnEff(EFFECT_RED_GROUND_GLOW, arg1->extra.tmd->coords, 0, &ofs);
            id  = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400C0004;
            pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            break;
        case 9:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case 28:
            Gp_ReleaseStateF0Add(arg1, 0xC);
            work->field_0 = 0;
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
        s = 0xBB8 - ((s16)work->field_6 - 0x2A) * 0x258;
        if (s < 0x4B0) {
            scale.vx = scale.vy = scale.vz = 0;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(0);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            Actor01200_FaceScale(arg1->extra.tmd->coords, 0x1000);
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
            Actor01200_FaceScale(arg1->extra.tmd->coords, s);
        }
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}

static void Actor01200_Fn01FDC(Enemy* arg0, Task* arg1)
{
    SVECTOR         ofs;
    VECTOR          scale;
    Actor01200Work* work;
    TmdObject*      obj;
    s16             s;
    s32             pan;
    s32             id;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    if (work->field_4 != 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   = 0;
        work->obj2C8.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj300.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj338.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj300.key             = Gp_PackObjPair(arg0, 0);
        work->obj338.key             = 0x22121;
        work->field_6                = 0;
        work->obj230.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        work->savedColorMtx          = work->colorMtx;
        work->field_174              = 0xA;
        work->field_170              = 1;
        work->field_178              = 0;
        work->field_176              = 0x2C;
        animDriverTick(arg1);
        work->obj338.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->obj338.pos.vy = arg1->extra.tmd->coords->coord.t[1] - 0x190;
        work->obj338.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        work->obj300.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->obj300.pos.vy = arg1->extra.tmd->coords->coord.t[1];
        work->obj300.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        return;
    }
    animDriverTick(arg1);
    switch ((s16)(work->field_6 - 0xD)) {
        case 0:
            ofs.vx = 0x1E;
            ofs.vz = 0x1E;
            ofs.vy = -0x3C;
            Gp_SpawnEff(EFFECT_030, arg1->extra.tmd->coords, 0x10080, &ofs);
            ofs.vy = -0xA;
            ofs.vz = -0x50;
            Gp_SpawnEff(EFFECT_030, arg1->extra.tmd->coords, 0x10030, &ofs);
            id  = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400C0004;
            pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            arg1->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case 1:
            work->obj300.radius = 0x320;
            work->obj300.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg1->extra.tmd->coords[2], 1, NULL);
            Gp_SpawnScript18(Actor01200_D04044, Actor01200_D04050);
            work->eff1A8.coord      = &arg1->extra.tmd->coords[4];
            work->eff1A8.spawnArgLo = 0x120;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->extra.tmd->coords[4], NULL, &work->eff1A8);
            break;
        case 2:
            work->obj338.radius = 0xC8;
            work->obj300.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj338.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            break;
        case 3:
            work->obj338.radius     = 0x190;
            work->eff1A8.coord      = &arg1->extra.tmd->coords[2];
            work->eff1A8.spawnArgLo = 0x100;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->extra.tmd->coords[2], NULL, &work->eff1A8);
            break;
        case 4:
            work->obj338.radius = 0x320;
            break;
        case 6:
            work->obj338.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            ofs.vx              = arg1->extra.tmd->coords->coord.t[0];
            ofs.vy              = arg1->extra.tmd->coords->coord.t[1];
            ofs.vz              = arg1->extra.tmd->coords->coord.t[2];
            Gp_SpawnEff(EFFECT_RED_GROUND_GLOW, &gGfxViewCoord, 0, &ofs);
            work->eff1A8.coord      = &arg1->extra.tmd->coords[1];
            work->eff1A8.spawnArgLo = 0x200;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->extra.tmd->coords[1], NULL, &work->eff1A8);
            break;
        case 8:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case 25:
            Gp_ReleaseStateF0Add(arg1, 0xC);
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
        Actor01200_FaceScale(arg1->extra.tmd->coords, s);
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}

/// Hit spark: picks one of two offsets at random for the quadrant the hit
/// angle `arg1` falls in (front, back, right or left), each with the model
/// coordinate it is relative to, keeps it in `work->effOfs` and spawns hit id
/// `arg2`'s effect there.
static void Actor01200_Fn026A0(Task* arg0, s16 arg1, u32 arg2)
{
    SVECTOR*        sc;
    Actor01200Work* work;
    s32             mag;
    GfxCoord*       coord;

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
    coord                   = &arg0->extra.tmd->coords[1];
    work->eff1A8.spawnArgLo = 0x80;
    work->eff1A8.spawnArgHi = 2;
    work->eff1A8.coord      = coord;
    work->effOfs            = *sc;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->extra.tmd->coords[sc->pad], &work->effOfs, &work->eff1A8);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
}

/// Hit check: finds the first type-2 record among the five in `jointContacts`, and
/// on a hit applies its damage, turns the model toward it and, once the hit
/// points run out, moves to substate 6.
static void Actor01200_Fn02918(Enemy* arg0, Task* arg1)
{
    ActorHitTakenScratch*  sc;
    Actor01200Work*        work;
    WorldCollisionContact* recs;
    SVECTOR*               pos;
    s32                    mask;
    s32                    kind;
    s32                    id;
    s16                    angle;
    s16                    i;

    work = arg1->work;
    sc   = (ActorHitTakenScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(ActorHitTakenScratch));
    pos  = &sc->pos;
    recs = work->jointContacts;
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
    if (i < 5) {
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
        Actor01200_Fn026A0(arg1, sc->angle, sc->id);
        func_800DA6E8(&arg0->node, sc->dmg, 0);
        arg0->hp -= sc->dmg;
        if (arg0->hp <= 0) {
            arg0->spawnState = 0;
            work->field_0    = 6;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorHitTakenScratch));
}

/// Patrol between the two `patrol` points: turn at most 0x20 toward the current
/// one, step 5 units, and swap points within 400 units or after 0x60 blocked
/// frames; state 6 when `ActorContact_Steer` reports 1, state 4 when the
/// player is within 2000 units and inside a quarter turn or 1000 units.
static void Actor01200_Fn02BE8(Enemy* arg0, Task* arg1)
{
    Actor01200Work*   work;
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
        work->obj2C8.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj300.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj338.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj230.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        animDriverTick(arg1);
        work->field_6 = 0;
        return;
    }
    head                                   = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_CURSOR(ActorTurnScratch) = head - 1;
    sc                                     = head - 1;
    head[-1].delta.vx                      = work->patrol[work->patrolIdx].vx - arg1->extra.tmd->coords->coord.t[0];
    sc->delta.vy                           = 0;
    sc->delta.vz                           = work->patrol[work->patrolIdx].vz - arg1->extra.tmd->coords->coord.t[2];
    coord                                  = arg1->extra.tmd->coords;
    angle                                  = ratan2(head[-1].delta.vx, sc->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle                              = actorNormalizeYaw(angle);
    if (sc->angle > 0x20) {
        sc->angle = 0x20;
    }
    if (sc->angle < -0x20) {
        sc->angle = -0x20;
    }
    sc->angle += ratan2(-arg1->extra.tmd->coords->coord.m[2][0], arg1->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, sc->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 5);
    if (ActorContact_PushContact(arg1->extra.tmd->coords, work->rootContacts, 5)) {
        work->field_6++;
    }
    if (!overlayOutOfRange(&sc->delta, 400) || work->field_6 > 0x60) {
        if (work->patrolIdx == 0) {
            work->patrolIdx = 1;
        } else {
            work->patrolIdx = 0;
        }
        work->field_6 = 0;
    }
    if (ActorContact_Steer(arg1->extra.tmd->coords, work->jointContacts, 5, &sc->delta) == 1) {
        work->field_0 = 6;
    }
    target       = arg1->extra.tmd->coords;
    sc->delta.vx = gPlayerStatus.coordMtx->t[0] - target->coord.t[0];
    sc->delta.vy = gPlayerStatus.coordMtx->t[1] - target->coord.t[1];
    sc->delta.vz = gPlayerStatus.coordMtx->t[2] - target->coord.t[2];
    if (!overlayOutOfRange(&sc->delta, 2000)) {
        coord = arg1->extra.tmd->coords;
        angle = ratan2(sc->delta.vx, sc->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (actorNormalizeYaw(angle) < 0x400 || !overlayOutOfRange(&sc->delta, 1000)) {
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
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// Walk back toward the spawn point: turn at most 0x10 toward it, step 8 units,
/// and hand over to state 7 once within 0x50 or after 0xDD frames (state 6 when
/// `ActorContact_Steer` reports 1).
static void Actor01200_Fn03294(Enemy* arg0, Task* arg1)
{
    Actor01200Work*   work;
    GfxCoord*         coord;
    GfxCoord*         facing;
    TmdObject*        obj;
    ActorTurnScratch* head;
    ActorTurnScratch* s;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->field_174              = 2;
        work->field_170              = 1;
        work->field_178              = 0;
        work->obj2C8.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj300.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj338.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj230.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        animDriverTick(arg1);
        work->field_3DC = 0;
        work->field_6   = 0;
        return;
    }
    head                                   = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_CURSOR(ActorTurnScratch) = head - 1;
    s                                      = head - 1;
    animDriverTick(arg1);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    head[-1].delta.vx                     = work->origin.vx - arg1->extra.tmd->coords->coord.t[0];
    s->delta.vy                           = 0;
    s->delta.vz                           = work->origin.vz - arg1->extra.tmd->coords->coord.t[2];
    coord                                 = arg1->extra.tmd->coords;
    s->angle                              = actorNormalizeYaw(ratan2(head[-1].delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    if (s->angle > 0x10) {
        s->angle = 0x10;
    }
    if (s->angle < -0x10) {
        s->angle = -0x10;
    }
    facing    = arg1->extra.tmd->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, s->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 8);
    ActorContact_PushContact(arg1->extra.tmd->coords, work->rootContacts, 5);
    work->field_6++;
    if (!overlayOutOfRange(&s->delta, 0x50) || work->field_6 >= 0xDD) {
        work->field_0 = 7;
    }
    if (ActorContact_Steer(arg1->extra.tmd->coords, work->jointContacts, 5, &s->delta) == 1) {
        work->field_0 = 6;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

static const Actor01200StateTable Actor01200_D000E4 = {
    {
        Actor01200_Fn03D58,
        Actor01200_Fn03DC0,
        Actor01200_Fn01040,
        Actor01200_Fn03E78,
        Actor01200_Fn01234,
        Actor01200_Fn017DC,
        Actor01200_Fn01FDC,
        Actor01200_Fn02BE8,
        Actor01200_Fn03294,
        Actor01200_Fn03F30,
    }
};

/// Per-frame tick: refreshes the coordinate and color, handles the render
/// mode in `gSceneCombatState.actorControl`, dispatches the substate handler and plays its sound.
static void Actor01200_Fn036B0(Enemy* arg0, Task* arg1)
{
    VECTOR               pos;
    Actor01200StateTable table;
    Actor01200Work*      work;
    s32                  snd;
    s32                  pan;
    s32                  id;

    work                                  = arg1->work;
    table                                 = Actor01200_D000E4;
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    pos.vx = arg1->extra.tmd->coords->workm.t[0];
    pos.vy = arg1->extra.tmd->coords->workm.t[1];
    pos.vz = arg1->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 5) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 5) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            Gp_ClearRec18Occupied(work->rootContacts);
            Gp_ClearRec18Occupied(work->jointContacts);
            Gp_ClearRec18Occupied(&work->rec2E8);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Gp_ClearRec18Occupied(work->rootContacts);
            Gp_ClearRec18Occupied(work->jointContacts);
            Gp_ClearRec18Occupied(&work->rec2E8);
            return;
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    table.fn[work->field_0](arg0, arg1);
    if (arg0->hp > 0) {
        Actor01200_Fn02918(arg0, arg1);
        if (arg0->hp <= 0) {
            work->field_0 = 6;
        }
    }
    Gp_ClearRec18Occupied(work->rootContacts);
    Gp_ClearRec18Occupied(work->jointContacts);
    Gp_ClearRec18Occupied(&work->rec2E8);
    id = Actor01200_Fn00990(work);
    if (id != 0) {
        snd = id | ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
        SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
    }
    if (work->field_3D8 != 0) {
        func_800D7A9C(arg1->extra.tmd, (VECTOR*)arg1->extra.tmd->coords->workm.t, 0, 3);
    }
    if (gGameSession->viewReady != 0) {
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// The actor's three task states: spawn, per-frame tick and teardown.
static const EnemyTaskFuncTable3 Actor01200_D0010C = {
    {
        Actor01200_Fn00A6C,
        Actor01200_Fn036B0,
        enemyDestroy,
    }
};

/// Display mode handler for the model (`Task::extra`), selected by `arg2`:
/// 0 hides it and 1 shows it, both reinstating its buffers and moving to
/// state 7; 2 sets `TMD_OBJECT_SKIP_AUTO_BUFFER` and 3 replaces its flags with
/// `TMD_OBJECT_SKIP_AUTO_BUFFER`, both moving
/// to state 0. `arg1` is unused.
s32 Actor01200_Fn03A00(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*      obj;
    Actor01200Work* work;

    obj  = task->extra.tmd;
    work = task->work;
    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            work->field_0 = 7;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 7;
            break;
        case 2:
            obj->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

s32 Actor01200_Fn03ABC(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    Actor01200Work* work;
    Enemy*          ctx;

    work            = arg0->work;
    ctx             = arg0->spawnArg2.pointer;
    work->field_194 = request->context.loc.stage;
    work->field_195 = request->context.loc.area;
    work->field_196 = (u8)request->command;
    if (request->context.key == 0xB02) {
        switch (request->command) {
            case 0:
                work->field_0 = 0;
                break;
            case 1:
            case 2:
                break;
            case 3:
                if (ctx->hp > 0) {
                    work->field_0                         = 4;
                    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                }
                break;
            case 4:
                if (ctx->hp > 0) {
                    work->field_0 = 9;
                }
                break;
        }
    }
    return 0;
}

#include "../../shared/actor_messages_place.inc.c"

#include "../../shared/coord_math_yaw_scale.inc.c"

/// State 0, the idle state: on entry (`field_4` set) it marks the enemy not lockable,
/// hides the model, and turns off the collision bodies the other states
/// enable - the pair pass of `obj2C8`, `obj300` and `obj338`, and the grid
/// pass of `obj230`. Nothing happens afterwards.
static void Actor01200_Fn03D58(Enemy* arg0, Task* arg1)
{
    Actor01200Work* work;
    TmdObject*      obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   = (u16)(obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->obj2C8.flags           = (u16)(work->obj2C8.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->obj300.flags           = (u16)(work->obj300.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->obj338.flags           = (u16)(work->obj338.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->obj230.flags           = (u16)(work->obj230.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
    }
}

/// State 1: on entry (`field_4` set) clear the actor and model flags, set
/// `field_174` to 4, and set or clear the high bits of the four sub-object
/// flags; afterwards run `animDriverTick` and move to state 2 once bit 0
/// of `field_58` is set.
static void Actor01200_Fn03DC0(Enemy* arg0, Task* arg1)
{
    Actor01200Work* work;
    TmdObject*      obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->field_174              = 4;
        work->field_170              = 1;
        work->field_178              = 0;
        work->obj2C8.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj300.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj338.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj230.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        animDriverTick(arg1);
        return;
    }
    animDriverTick(arg1);
    if (work->field_58 & 1) {
        work->field_0 = 2;
    }
}

/// State 3: on entry (`field_4` set) clear the actor and model flags, set
/// `field_174` to 6, and set or clear the high bits of the four sub-object
/// flags; afterwards run `animDriverTick` and move to state 7 once bit 0
/// of `field_58` is set.
static void Actor01200_Fn03E78(Enemy* arg0, Task* arg1)
{
    Actor01200Work* work;
    TmdObject*      obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->field_174              = 6;
        work->field_170              = 1;
        work->field_178              = 0;
        work->obj2C8.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj300.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj338.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj230.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
        animDriverTick(arg1);
        return;
    }
    animDriverTick(arg1);
    if (work->field_58 & 1) {
        work->field_0 = 7;
    }
}

/// On entry (`field_4` set) clear the actor and model flags, set `field_174`
/// to 2, and set or clear the high bits of the four sub-object flags; then run
/// `animDriverTick` and clear the model's coordinate flag every frame.
static void Actor01200_Fn03F30(Enemy* arg0, Task* arg1)
{
    Actor01200Work* work;
    TmdObject*      obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->field_174              = 2;
        work->field_170              = 1;
        work->field_178              = 0;
        work->obj2C8.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj300.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj338.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj230.flags          |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    animDriverTick(arg1);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Task entry point: runs the handler for the task's current state from a
/// stack copy of `Actor01200_D0010C`.
void Actor01200_Fn03FD4(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor01200_D0010C;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
