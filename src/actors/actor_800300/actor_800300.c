#include "actors/actor_800300.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachments.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

extern s32              D_map_neo_ark_8017A99C;
extern TaskMessageEntry D_actor_800300_80168880[26];
extern GpuImageUpload** D_actor_800300_80168950[];
extern GpuImageUpload** D_actor_800300_80168960[];

static void func_actor_800300_801623F8(Task* arg0);
static void func_actor_800300_801625A8(Task* task);
static void func_actor_800300_80162658(Task* arg0);
static void func_actor_800300_801628D0(Task* arg0);
static void func_actor_800300_80162A98(Task* arg0);
static void func_actor_800300_80162C2C(Task* arg0);
static void func_actor_800300_80162C98(Task* arg0);
static void func_actor_800300_80162D74(Task* arg0);
static void func_actor_800300_80162EEC(Task* arg0);
static void func_actor_800300_80162F24(Task* arg0);
static void func_actor_800300_80162F98(Task* arg0);
static void func_actor_800300_80163048(Task* arg0);
static void func_actor_800300_80163074(Task* arg0);

extern GpuImageUpload* D_actor_800300_80169A20[2];
extern GpuImageUpload* D_actor_800300_80169A28[4];
extern GpuImageUpload* D_actor_800300_80169A38[4];
extern GpuImageUpload* D_actor_800300_80169A48[6];
extern GpuImageUpload* D_actor_800300_80169A60[2];
extern GpuImageUpload* D_actor_800300_80169A68[2];

static AnimationSet _gActor800300Animation07E3C;
static AnimationSet _gActor800300Animation08218;
static AnimationSet _gActor800300Animation08618;
static AnimationSet _gActor800300Animation09194;
static AnimationSet _gActor800300Animation09478;
static AnimationSet _gActor800300Animation0981C;
static AnimationSet _gActor800300Animation09D10;
static AnimationSet _gActor800300Animation0A4D4;
static AnimationSet _gActor800300Animation0A9F8;
static AnimationSet _gActor800300Animation0AD50;

static TmdBone _gActor800300Model02CF4Skeleton[19] = {
#include "assets/actor_800300_model_02CF4_skeleton.inc"
};

static u32 _gActor800300Model02CF4PartVerts[19] = {
#include "assets/actor_800300_model_02CF4_partVerts.inc"
};

static SVECTOR _gActor800300Model02CF4Verts[365] = {
#include "assets/actor_800300_model_02CF4_verts.inc"
};

static SVECTOR _gActor800300Model02CF4Normals[386] = {
#include "assets/actor_800300_model_02CF4_normals.inc"
};

static u32 _gActor800300Model02CF4Stream[3923] = {
#include "assets/actor_800300_model_02CF4_stream.inc"
};

TmdSource gActor800300Model02CF4 = {
    0,
    21760,
    5992,
    19,
    _gActor800300Model02CF4PartVerts,
    _gActor800300Model02CF4Verts,
    _gActor800300Model02CF4Normals,
    _gActor800300Model02CF4Skeleton,
    _gActor800300Model02CF4Stream,
};

TaskMessageEntry D_actor_800300_80168880[26] = {
    { ANIMATION_MESSAGE_PLAY, companionPlayScriptedAnimation },
    { 1002, companionPlayScriptedAnimation },
    { 1003, companionPlayScriptedAnimation },
    { 1004, companionPlayScriptedAnimation },
    { GAME_ACTOR_MESSAGE_PLACE, playerActorPlace },
    { ANIMATION_MESSAGE_IS_PLAYING, playerActorIsAnimationPlaying },
    { GAME_ACTOR_MESSAGE_TURN_TO_YAW, companionTurnToYaw },
    { GAME_ACTOR_MESSAGE_CLIMB_STAIRS, companionPlayScriptedAnimation },
    { GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, playerActorIsScriptedMotionPending },
    { GAME_ACTOR_MESSAGE_END_SCRIPTED, companionEndScriptedMotion },
    { GAME_ACTOR_MESSAGE_MOVE_TO, companionMoveTo },
    { GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, playerActorSetModelDraw },
    { ANIMATION_MESSAGE_INSTALL_AND_PLAY, companionInstallScriptedAnimation },
    { GAME_ACTOR_MESSAGE_ATTACH_TO_COORD, playerActorAttachToCoord },
    { GAME_ACTOR_MESSAGE_WALK_STEPS, playerActorWalkSteps },
    { ANIMATION_MESSAGE_COPY_BANK_EXTENSION, animationCopyCompanionBankExtension },
    { GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, companionPlayScriptedAnimation },
    { GAME_ACTOR_MESSAGE_APPLY_DAMAGE, companionApplyDamage },
    { 1018, companionPlayScriptedAnimation },
    { 1019, companionRunTo },
    { 1020, companionPlayScriptedAnimation },
    { ANIMATION_MESSAGE_SET_RATE, playerActorSetAnimationRate },
    { GAME_ACTOR_MESSAGE_MOVE_BY, companionMoveBy },
    { ANIMATION_MESSAGE_REPLACE_AND_PLAY, companionEndScriptedMotion },
    { 1024, companionEndScriptedMotion },
    { GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE, playerActorSetTextureSequence },
};

GpuImageUpload** D_actor_800300_80168950[4] = {
    D_actor_800300_80169A20,
    D_actor_800300_80169A28,
    D_actor_800300_80169A38,
    D_actor_800300_80169A48,
};

GpuImageUpload** D_actor_800300_80168960[2] = {
    D_actor_800300_80169A68,
    D_actor_800300_80169A60,
};

u_long D_actor_800300_80168968[250] = {
#include "assets/actor_121300_image_09DC8.inc"
};

GpuImageUpload D_actor_800300_80168D50[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 20 }, D_actor_800300_80168968 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800300_80168D70[250] = {
#include "assets/actor_121300_image_0A1B0.inc"
};

GpuImageUpload D_actor_800300_80169158[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 20 }, D_actor_800300_80168D70 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800300_80169178[250] = {
#include "assets/actor_121300_image_0A598.inc"
};

GpuImageUpload D_actor_800300_80169560[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 25, 20 }, D_actor_800300_80169178 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800300_80169580[140] = {
#include "assets/actor_121300_image_0A980.inc"
};

GpuImageUpload D_actor_800300_801697B0[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 14, 20 }, D_actor_800300_80169580 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800300_801697D0[140] = {
#include "assets/actor_121300_image_0ABB0.inc"
};

GpuImageUpload D_actor_800300_80169A00[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 14, 20 }, D_actor_800300_801697D0 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

GpuImageUpload* D_actor_800300_80169A20[2] = {
    D_actor_800300_80168D50,
    NULL,
};

GpuImageUpload* D_actor_800300_80169A28[4] = {
    D_actor_800300_80168D50,
    D_actor_800300_80169158,
    D_actor_800300_80169560,
    NULL,
};

GpuImageUpload* D_actor_800300_80169A38[4] = {
    D_actor_800300_80169560,
    D_actor_800300_80169158,
    D_actor_800300_80168D50,
    NULL,
};

GpuImageUpload* D_actor_800300_80169A48[6] = {
    D_actor_800300_80168D50,
    D_actor_800300_80169158,
    D_actor_800300_80169560,
    D_actor_800300_80169158,
    D_actor_800300_80168D50,
    NULL,
};

GpuImageUpload* D_actor_800300_80169A60[2] = {
    D_actor_800300_801697B0,
    NULL,
};

GpuImageUpload* D_actor_800300_80169A68[2] = {
    D_actor_800300_80169A00,
    NULL,
};

static AnimationPackedPose _gActor800300Animation07E3CBank1[2] = {
#include "assets/actor_800300_animation_07E3C_bank1.inc"
};

static AnimationPackedRotation _gActor800300Animation07E3CBank4[23] = {
#include "assets/actor_800300_animation_07E3C_bank4.inc"
};

static AnimationRecord _gActor800300Animation07E3CRecords[84] = {
#include "assets/actor_800300_animation_07E3C_records.inc"
};

static u16 _gActor800300Animation07E3CIndices[20] = {
#include "assets/actor_800300_animation_07E3C_indices.inc"
};

static AnimationSet _gActor800300Animation07E3C = {
    _gActor800300Animation07E3CRecords,
    _gActor800300Animation07E3CIndices,
    { NULL, _gActor800300Animation07E3CBank1, NULL, NULL, _gActor800300Animation07E3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800300Animation08218Bank1[6] = {
#include "assets/actor_800300_animation_08218_bank1.inc"
};

static AnimationPackedRotation _gActor800300Animation08218Bank4[73] = {
#include "assets/actor_800300_animation_08218_bank4.inc"
};

static AnimationRecord _gActor800300Animation08218Records[136] = {
#include "assets/actor_800300_animation_08218_records.inc"
};

static u16 _gActor800300Animation08218Indices[20] = {
#include "assets/actor_800300_animation_08218_indices.inc"
};

static AnimationSet _gActor800300Animation08218 = {
    _gActor800300Animation08218Records,
    _gActor800300Animation08218Indices,
    { NULL, _gActor800300Animation08218Bank1, NULL, NULL, _gActor800300Animation08218Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800300Animation08618Bank1[5] = {
#include "assets/actor_800300_animation_08618_bank1.inc"
};

static AnimationPackedRotation _gActor800300Animation08618Bank4[76] = {
#include "assets/actor_800300_animation_08618_bank4.inc"
};

static AnimationRecord _gActor800300Animation08618Records[145] = {
#include "assets/actor_800300_animation_08618_records.inc"
};

static u16 _gActor800300Animation08618Indices[20] = {
#include "assets/actor_800300_animation_08618_indices.inc"
};

static AnimationSet _gActor800300Animation08618 = {
    _gActor800300Animation08618Records,
    _gActor800300Animation08618Indices,
    { NULL, _gActor800300Animation08618Bank1, NULL, NULL, _gActor800300Animation08618Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800300Animation09194Bank1[14] = {
#include "assets/actor_800300_animation_09194_bank1.inc"
};

static AnimationPackedRotation _gActor800300Animation09194Bank4[301] = {
#include "assets/actor_800300_animation_09194_bank4.inc"
};

static AnimationRecord _gActor800300Animation09194Records[372] = {
#include "assets/actor_800300_animation_09194_records.inc"
};

static u16 _gActor800300Animation09194Indices[20] = {
#include "assets/actor_800300_animation_09194_indices.inc"
};

static AnimationSet _gActor800300Animation09194 = {
    _gActor800300Animation09194Records,
    _gActor800300Animation09194Indices,
    { NULL, _gActor800300Animation09194Bank1, NULL, NULL, _gActor800300Animation09194Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800300Animation09478Bank1[2] = {
#include "assets/actor_800300_animation_09478_bank1.inc"
};

static AnimationPackedRotation _gActor800300Animation09478Bank4[58] = {
#include "assets/actor_800300_animation_09478_bank4.inc"
};

static AnimationRecord _gActor800300Animation09478Records[101] = {
#include "assets/actor_800300_animation_09478_records.inc"
};

static u16 _gActor800300Animation09478Indices[20] = {
#include "assets/actor_800300_animation_09478_indices.inc"
};

static AnimationSet _gActor800300Animation09478 = {
    _gActor800300Animation09478Records,
    _gActor800300Animation09478Indices,
    { NULL, _gActor800300Animation09478Bank1, NULL, NULL, _gActor800300Animation09478Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800300Animation0981CBank1[2] = {
#include "assets/actor_800300_animation_0981C_bank1.inc"
};

static AnimationPackedRotation _gActor800300Animation0981CBank4[83] = {
#include "assets/actor_800300_animation_0981C_bank4.inc"
};

static AnimationRecord _gActor800300Animation0981CRecords[124] = {
#include "assets/actor_800300_animation_0981C_records.inc"
};

static u16 _gActor800300Animation0981CIndices[20] = {
#include "assets/actor_800300_animation_0981C_indices.inc"
};

static AnimationSet _gActor800300Animation0981C = {
    _gActor800300Animation0981CRecords,
    _gActor800300Animation0981CIndices,
    { NULL, _gActor800300Animation0981CBank1, NULL, NULL, _gActor800300Animation0981CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800300Animation09D10Bank1[4] = {
#include "assets/actor_800300_animation_09D10_bank1.inc"
};

static AnimationPackedRotation _gActor800300Animation09D10Bank4[76] = {
#include "assets/actor_800300_animation_09D10_bank4.inc"
};

static AnimationRecord _gActor800300Animation09D10Records[209] = {
#include "assets/actor_800300_animation_09D10_records.inc"
};

static u16 _gActor800300Animation09D10Indices[20] = {
#include "assets/actor_800300_animation_09D10_indices.inc"
};

static AnimationSet _gActor800300Animation09D10 = {
    _gActor800300Animation09D10Records,
    _gActor800300Animation09D10Indices,
    { NULL, _gActor800300Animation09D10Bank1, NULL, NULL, _gActor800300Animation09D10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800300Animation0A4D4Bank1[18] = {
#include "assets/actor_800300_animation_0A4D4_bank1.inc"
};

static AnimationPackedRotation _gActor800300Animation0A4D4Bank4[184] = {
#include "assets/actor_800300_animation_0A4D4_bank4.inc"
};

static AnimationRecord _gActor800300Animation0A4D4Records[239] = {
#include "assets/actor_800300_animation_0A4D4_records.inc"
};

static u16 _gActor800300Animation0A4D4Indices[20] = {
#include "assets/actor_800300_animation_0A4D4_indices.inc"
};

static AnimationSet _gActor800300Animation0A4D4 = {
    _gActor800300Animation0A4D4Records,
    _gActor800300Animation0A4D4Indices,
    { NULL, _gActor800300Animation0A4D4Bank1, NULL, NULL, _gActor800300Animation0A4D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800300Animation0A9F8Bank1[5] = {
#include "assets/actor_800300_animation_0A9F8_bank1.inc"
};

static AnimationPackedRotation _gActor800300Animation0A9F8Bank4[101] = {
#include "assets/actor_800300_animation_0A9F8_bank4.inc"
};

static AnimationRecord _gActor800300Animation0A9F8Records[193] = {
#include "assets/actor_800300_animation_0A9F8_records.inc"
};

static u16 _gActor800300Animation0A9F8Indices[20] = {
#include "assets/actor_800300_animation_0A9F8_indices.inc"
};

static AnimationSet _gActor800300Animation0A9F8 = {
    _gActor800300Animation0A9F8Records,
    _gActor800300Animation0A9F8Indices,
    { NULL, _gActor800300Animation0A9F8Bank1, NULL, NULL, _gActor800300Animation0A9F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor800300Animation0AD50Bank1[4] = {
#include "assets/actor_800300_animation_0AD50_bank1.inc"
};

static AnimationPackedRotation _gActor800300Animation0AD50Bank4[41] = {
#include "assets/actor_800300_animation_0AD50_bank4.inc"
};

static AnimationRecord _gActor800300Animation0AD50Records[141] = {
#include "assets/actor_800300_animation_0AD50_records.inc"
};

static u16 _gActor800300Animation0AD50Indices[20] = {
#include "assets/actor_800300_animation_0AD50_indices.inc"
};

static AnimationSet _gActor800300Animation0AD50 = {
    _gActor800300Animation0AD50Records,
    _gActor800300Animation0AD50Indices,
    { NULL, _gActor800300Animation0AD50Bank1, NULL, NULL, _gActor800300Animation0AD50Bank4, NULL, NULL, NULL },
};

AnimationBank D_actor_800300_8016CB98 = { { {
    NULL,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation08218,
    &_gActor800300Animation08618,
    &_gActor800300Animation09194,
    &_gActor800300Animation09478,
    &_gActor800300Animation0981C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation0A9F8,
    &_gActor800300Animation0AD50,
    &_gActor800300Animation0A4D4,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation09D10,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    &_gActor800300Animation07E3C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
} } };

static void func_actor_800300_80161E80(Task* arg0);
static void func_actor_800300_80162064(Task* arg0);
static void func_actor_800300_8016259C(Task* arg0);

static void func_actor_800300_80161E80(Task* arg0)
{
    enum { COMPANION_DISTRESS_INITIAL_INTERVAL = 150 };

    GameActor*             actor;
    TmdObject*             extra;
    GfxCoord*              coord;
    GfxCoord*              next;
    GfxCoord**             addr;
    CompanionWork*         companion;
    WorldCollisionBody*    obj;
    WorldCollisionContact* recs;
    McSaveData*            save;
    s32                    packed;
    s8                     intervalByte;

    actor     = arg0->work;
    extra     = arg0->extra.tmd;
    companion = actor->companionWork;
    addr      = &extra->coords;
    coord     = *addr;
    arg0->state++;
    arg0->msgTable                                 = D_actor_800300_80168880;
    arg0->exitCallback                             = &func_actor_800300_801625A8;
    actor->animationSlotCount                      = GAME_ACTOR_NORMAL_ANIMATION_SLOTS;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] = arg0;
    coord->parent                                  = &gGfxViewCoord;
    coord->composeStamp                            = GRAPHICS_COORD_DIRTY;
    extra->flags                                   = 0;
    RotMatrix(&actor->rotation, &coord->coord);
    companionInitNativeAnimation(arg0);
    actor->animationRate = ANIMATION_RATE_ONE;
    playerActorResetChildSlots(arg0, actor->actionArgument);
    recs                                       = actor->collisionContacts;
    obj                                        = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    actor->previousPosition.vx                 = coord->coord.t[0];
    actor->previousPosition.vy                 = coord->coord.t[1];
    actor->previousPosition.vz                 = coord->coord.t[2];
    obj->context.motion                        = &actor->collisionMotionContexts[0];
    obj->coord                                 = coord;
    actor->collisionMotionContexts[0].contacts = recs;
    save                                       = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    obj->pos.vx                                = 0;
    obj->pos.vy                                = -0x12C;
    obj->pos.vz                                = 0;
    {
        s32 temp;

        temp        = save->state.characterId;
        obj->radius = 0x12C;
        obj->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        packed      = 0x10000;
        obj->key    = temp | packed;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_BODIES, obj);
    }
    worldCollisionInitContacts(actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), 0);
    obj->flags                                |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    obj                                        = &actor->collisionBodies[GAME_ACTOR_BODY_PART4];
    next                                       = arg0->extra.tmd->coords;
    obj->context.motion                        = &actor->collisionMotionContexts[1];
    obj->coord                                 = next + 4;
    actor->collisionMotionContexts[1].contacts = recs;
    obj->pos.vx                                = 0;
    obj->pos.vy                                = 0;
    obj->pos.vz                                = 0;
    {
        s32 temp;

        temp        = save->state.characterId;
        obj->radius = 0xC8;
        obj->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        obj->key    = temp | packed;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_BODIES, obj);
    }
    obj->flags                |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    actor->collisionEnableMask = GAME_ACTOR_COLLISION_REQUEST_MASK;
    companionSetDecisionDelay(arg0, 0x3C, 0x7F);
    intervalByte                                = (s8)COMPANION_DISTRESS_INITIAL_INTERVAL;
    companion->activity.distress.flinchInterval = intervalByte;
}

static void func_actor_800300_80162064(Task* arg0)
{
    void**                scratch;
    CompanionMoveScratch* frameEnd;
    CompanionMoveScratch* frame;
    GameActor*            actor;
    TmdObject*            obj;
    TmdObject*            extra;
    GfxCoord*             coord;
    WorldCollisionBody*   objs[2];
    s32                   dy;
    s32                   i;
    s8                    bits;

    scratch                                        = SCRATCH_HEAD_ADDR;
    frameEnd                                       = SCRATCH_HEAD_AT(scratch, CompanionMoveScratch);
    obj                                            = arg0->extra.tmd;
    SCRATCH_HEAD_AT(scratch, CompanionMoveScratch) = frameEnd - 1;
    extra                                          = obj;
    frame                                          = frameEnd - 1;
    actor                                          = arg0->work;
    coord                                          = extra->coords;
    if (actor->mode != GAME_ACTOR_MODE_SCRIPTED &&
        (dy = coord->coord.t[1], dy = dy - actor->previousPosition.vy, dy = ABS(dy), dy >= 0x200)) {
        coord->coord.t[0] = actor->previousPosition.vx;
        coord->coord.t[1] = actor->previousPosition.vy;
        coord->coord.t[2] = actor->previousPosition.vz;
    } else {
        if (actor->collisionEnableMask & 1) {
            actor->gridResponse = worldCollisionApplyResponsePushback(coord, actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), &actor->surfaceClass);
            if ((s8)actor->gridResponse == 2) {
                coord->coord.t[0] = actor->previousPosition.vx;
                coord->coord.t[1] = actor->previousPosition.vy;
                coord->coord.t[2] = actor->previousPosition.vz;
            }
        } else {
            actor->gridResponse = 0;
        }
        actor->previousPosition.vx = coord->coord.t[0];
        actor->previousPosition.vy = coord->coord.t[1];
        actor->previousPosition.vz = coord->coord.t[2];
    }
    objs[0] = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    objs[1] = &actor->collisionBodies[GAME_ACTOR_BODY_PART4];
    for (i = 0; i < 2; i++) {
        bits = actor->pendingCollisionUpdates;
        if ((bits >> i) & 1) {
            actor->collisionEnableMask |= 1 << i;
            objs[i]->flags             |= WORLD_COLLISION_BODY_GRID_ENABLED;
        } else if (bits & (8 << i)) {
            actor->collisionEnableMask &= ~(1 << i);
            objs[i]->flags             &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    }
    actor->pendingCollisionUpdates = 0;
    if (D_80115768 == 0) {
        func_actor_800300_80162C2C(arg0);
    }
    func_actor_800300_801623F8(arg0);
    worldCollisionClearContacts(actor->collisionContacts);
    if (actor->collisionEnableMask & 1) {
        coord->coord.t[1] = actor->previousPosition.vy + 0x10;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    if ((s8)actor->usesPushbackDirection != 0) {
        frame->motionDirection.vx = actor->pushbackDirection.vx;
        frame->motionDirection.vy = actor->pushbackDirection.vy;
        frame->motionDirection.vz = actor->pushbackDirection.vz;
    } else {
        frame->motionDirection.vx = (u16)coord->workm.m[0][2] * (s8) * (volatile u8*)&actor->movementSign;
        frame->motionDirection.vy = (u16)coord->workm.m[1][2] * (s8) * (volatile u8*)&actor->movementSign;
        frame->motionDirection.vz = (u16)coord->workm.m[2][2] * (s8) * (volatile u8*)&actor->movementSign;
    }
    actor->collisionMotionContexts[0].motionDirection.vx = frame->motionDirection.vx;
    actor->collisionMotionContexts[0].motionDirection.vy = frame->motionDirection.vy;
    actor->collisionMotionContexts[0].motionDirection.vz = frame->motionDirection.vz;
    actor->collisionMotionContexts[1].motionDirection.vx = frame->motionDirection.vx;
    actor->collisionMotionContexts[1].motionDirection.vy = frame->motionDirection.vy;
    actor->collisionMotionContexts[1].motionDirection.vz = frame->motionDirection.vz;
    actor->collisionMotionContexts[2].motionDirection.vx = frame->motionDirection.vx;
    actor->collisionMotionContexts[2].motionDirection.vy = frame->motionDirection.vy;
    actor->collisionMotionContexts[2].motionDirection.vz = frame->motionDirection.vz;
    if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&coord->workm), &frame->shadowCentre) != 0) {
            effectDrawGroundShadow(&frame->shadowCentre, 0x200, gRoomEffectState->groundShadowShade);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(CompanionMoveScratch);
}

static void func_actor_800300_801623F8(Task* arg0)
{
    void**            scratch;
    u8*               head;
    u8*               temp;
    RECT*             rect;
    GameActor*        actor;
    GpuImageUpload*** frameLists;
    s32               idx;
    u32               row;
    GpuImageUpload*   uploadList;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    actor                          = arg0->work;
    temp                           = head - 8;
    SCRATCH_HEAD_AT(scratch, void) = temp;
    rect                           = (RECT*)temp;

    if ((s8)actor->textureSequenceA != 0) {
        actor->textureDelayA--;
        if ((s8)actor->textureDelayA <= 0) {
            frameLists = D_actor_800300_80168950;
            idx        = (s8)actor->textureSequenceA - 1;
            uploadList = frameLists[idx][(s8)actor->textureFrameA];
            if (uploadList != NULL) {
                ((RECT*)head)[-1].x = 0;
                rect->y             = 0x40;
                rect->w             = 0x19;
                rect->h             = 0x14;
                actorRenderUploadTexture(arg0, uploadList, rect);
                actor->textureDelayA = 4;
                actor->textureFrameA++;
            } else {
                actor->textureSequenceA = 0;
            }
        }
    }

    if ((s8)actor->textureSequenceB != 0) {
        actor->textureDelayB--;
        if ((s8)actor->textureDelayB <= 0) {
            frameLists = D_actor_800300_80168960;
            idx        = (row = (s8)actor->textureSequenceB - 1);
            uploadList = frameLists[row][(s8)actor->textureFrameB];
            if (uploadList != NULL) {
                rect->x = 0xC;
                rect->y = 0x60;
                rect->w = 0xE;
                rect->h = 0x14;
                actorRenderUploadTexture(arg0, uploadList, rect);
                actor->textureDelayB = 8;
                actor->textureFrameB++;
            } else {
                actor->textureSequenceB = 0;
            }
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void func_actor_800300_8016259C(Task* arg0)
{
    arg0->state = 3;
}

/// Teardown of the actor's main task, run both as its exit callback and as
/// the last entry of its state table: clears the second `gPlayerActorTasks` slot,
/// unlinks the two collision objects the set-up state linked, and kills the
/// task.
static void func_actor_800300_801625A8(Task* task)
{
    GameActor* actor;

    actor                                          = (GameActor*)task->work;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] = NULL;
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_ROOT]);
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_PART4]);
    taskKill(task);
}

/// State handlers of the actor's main task, indexed by its state: set-up, the
/// per-frame update, a step that only advances to the last state, and the
/// teardown.
static const TaskFuncTable4 D_actor_800300_80161E24 = { {
    func_actor_800300_80161E80,
    func_actor_800300_80162064,
    func_actor_800300_8016259C,
    func_actor_800300_801625A8,
} };

/// Per-frame entry point of the actor's main task: runs the handler its state
/// selects. The table is a local, so it is copied from `.rodata` onto the
/// stack on every call.
void func_actor_800300_801625F4(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_800300_80161E24;
    states.funcs[task->state](task);
}

/// Handlers `func_actor_800300_80162C2C` runs, indexed by `mode`.
static const TaskFuncTable3 D_actor_800300_80161E34 = { {
    func_actor_800300_80162658,
    func_actor_800300_80162F24,
    func_actor_800300_80162F98,
} };

/// Behaviours `func_actor_800300_80162658` runs, indexed by `state`.
static const TaskFuncTable9 D_actor_800300_80161E40 = { {
    func_actor_800300_80162C98,
    func_actor_800300_801628D0,
    func_actor_800300_80162A98,
    func_actor_800300_80162C98,
    func_actor_800300_80162C98,
    func_actor_800300_80162EEC,
    func_actor_800300_80162C98,
    func_actor_800300_80162D74,
    func_actor_800300_80162C98,
} };

static void func_actor_800300_80162658(Task* arg0)
{
    enum {
        COMPANION_DISTRESS_INTERVAL_DECREMENT  = 7,
        COMPANION_DISTRESS_INTERVAL_THRESHOLD  = 90,
        COMPANION_DISTRESS_RESTART_INTERVAL    = 60,
        COMPANION_DISTRESS_SEVERE_FLINCH_COUNT = 5
    };

    TaskFuncTable9 sp;
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      obj;
    s8             nextInterval;
    s32            pan;
    s32            depth;
    s32            anim;
    s32            sound;

    sp        = D_actor_800300_80161E40;
    actor     = arg0->work;
    companion = actor->companionWork;
    obj       = arg0->extra.tmd->coords;
    if (companion->decisionTimer > 0) {
        companion->decisionTimer = (u16)companion->decisionTimer - 1;
    }
    if (D_map_neo_ark_8017A99C >= 0x30C) {
        if (actor->state != 5) {
            actor->idleTicks++;
            if ((s16)actor->idleTicks >= (s8)companion->activity.distress.flinchInterval) {
                actor->idleTicks = 0;
                companion->activity.distress.flinchCount++;
                // Keep the signed byte intermediate used by the interval threshold.
                nextInterval                                = (u8)companion->activity.distress.flinchInterval - COMPANION_DISTRESS_INTERVAL_DECREMENT;
                companion->activity.distress.flinchInterval = nextInterval;
                if (nextInterval < COMPANION_DISTRESS_INTERVAL_THRESHOLD) {
                    companion->activity.distress.flinchInterval = COMPANION_DISTRESS_RESTART_INTERVAL;
                }
                actor->animationState   = 7;
                actor->state            = 5;
                actor->movementMode     = 0;
                actor->turnRateIndex    = 0;
                actor->statePhase       = 0;
                actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
                worldCoordPlaySound(obj, (rand() & 1) + 0x55170005, 0);
                if (companionApplyDamage(arg0, 0, 0x40010, 0) != 0) {
                    return;
                }
                anim = 0x10;
                if ((s8)companion->activity.distress.flinchCount >= COMPANION_DISTRESS_SEVERE_FLINCH_COUNT) {
                    anim = 0x11;
                }
                playerActorPlayChildSlotsWithBlend(arg0, anim, 0, 3);
            }
        }
    }
    sp.funcs[actor->state](arg0);
    if ((s8)actor->recoveryTicks == 0) {
        playerActorResolveBodyContacts(arg0, &actor->collisionContacts[0]);
        if ((u16)actor->hitRegion != 0) {
            companionEnterDamageReaction(arg0);
            pan   = (s8)worldCoordGetOriginAudioPan(obj);
            depth = (s8)worldCoordGetOriginAudioDepth(obj);
            sound = 7;
            if ((u16)actor->hitRegion == 1) {
                sound = 6;
            }
            sndEvtRequestScriptStart(sound, pan, depth);
        }
    }
    playerActorTickAnimationState(arg0);
    playerActorTickChildSlots(arg0);
    playerActorUpdateFacing(arg0);
    playerActorStepMovement(arg0);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
        playerActorEnterStoppedPose(arg0, 0);
    }
}

static void func_actor_800300_801628D0(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  target;
    VECTOR3*   vec;
    s32        dist;
    s32        angle;
    s32        arg;

    coord  = arg0->extra.tmd->coords;
    target = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor  = arg0->work;
    switch (actor->statePhase) {
        case 0:
            actor->stateTimer = 0;
            if (companionGetPlayerPlanarDistance(coord) >= 0xE00) {
                arg                 = 4;
                actor->statePhase   = 2;
                actor->movementMode = 3;
            } else {
            resume:
                if (actor->statePhase != 3) {
                    actor->statePhase = 1;
                }
                actor->movementMode = 7;
                arg                 = 2;
            }
            playerActorPlayChildSlotsWithBlend(arg0, arg, 0, 5);
            /* fallthrough */
        case 1:
        case 2:
        case 3:
            actor->movementSign = 1;
            dist                = companionGetPlayerPlanarDistance(coord);
            if (dist < 0x301) {
                companionEnterIdle(arg0, 0);
                break;
            }
            if (actor->statePhase == 3) {
                break;
            }
            actor->stateTimer++;
            if (actor->stateTimer == 0xB4) {
                actor->statePhase = 3;
                goto resume;
            }
            if (actor->actionValue > 0) {
                actor->actionValue = (u16)actor->actionValue - 1;
            } else {
                angle = rand() & 0x3FF;
                if (((0x800 - angle) >= dist && actor->statePhase == 2) || (dist >= angle + 0xC00 && actor->statePhase == 1)) {
                    actor->statePhase  = 0;
                    actor->actionValue = 0x3C;
                }
            }
            break;
    }
    vec = MATRIX_TRANS(&target->coord);
    playerActorTurnBodyTowardPoint(arg0, vec);
    playerActorTurnAimTowardPoint(arg0, vec);
    playerActorPlayFootstepCue(arg0);
}

static void func_actor_800300_80162A98(Task* arg0)
{
    u8*              head;
    VECTOR3*         vec;
    GameActor*       actor;
    WorldTargetNode* node;
    TmdObject*       extra;
    GfxCoord*        src;
    s32              turnDelta;
    s32              arg;
    s32              flag;

    actor                    = arg0->work;
    extra                    = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd;
    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x10;
    vec                      = (VECTOR3*)(head - 0x10);
    node                     = actor->targetNode;
    src                      = extra->coords;
    if (node != NULL) {
        if (!(node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            worldTargetGetBodyPosition(node, vec);
        } else {
            actor->statePhase = 2;
        }
    } else {
        ((VECTOR3*)(head - 0x10))->vx = src->coord.t[0];
        vec->vy                       = src->coord.t[1];
        vec->vz                       = src->coord.t[2];
    }
    switch (actor->statePhase) {
        case 0:
            flag              = 1;
            actor->statePhase = flag;
            if (playerActorGetTurnToPoint(arg0, vec) < 0) {
                actor->actionValue = -1;
                arg                = 5;
            } else {
                actor->actionValue = 1;
                arg                = 6;
            }
            playerActorPlayChildSlotsWithBlend(arg0, arg, 0, 5);
            /* fallthrough */
        case 1:
            actor->turnSign = (u8)actor->actionValue;
            turnDelta       = playerActorGetTurnToPoint(arg0, vec);
            if (turnDelta < 0) {
                turnDelta = -turnDelta;
            }
            if ((turnDelta < 0x81) || (actor->statePhase == 2)) {
                companionEnterIdle(arg0, 0);
            }
            break;
    }
    playerActorTurnAimTowardPoint(arg0, MATRIX_TRANS(&src->coord));
    playerActorPlayFootstepCue(arg0);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_800300_80162C2C(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable3 sp;

    sp                  = D_actor_800300_80161E34;
    actor               = arg0->work;
    actor->movementSign = 0;
    actor->turnSign     = 0;
    sp.funcs[actor->mode](arg0);
    actor->usesPushbackDirection = 0;
}

static void func_actor_800300_80162C98(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  target;
    s32        turnDelta;

    actor  = arg0->work;
    coord  = arg0->extra.tmd->coords;
    target = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    if (((GameActor*)arg0->work)->companionWork->decisionTimer <= 0) {
        companionSetDecisionDelay(arg0, 0x14, 0x3F);
        if ((u32)(companionGetPlayerPlanarDistance(coord) - 0x581) < 0x87F) {
            func_actor_800300_80163048(arg0);
        }
        turnDelta = playerActorGetTurnToPoint(arg0, MATRIX_TRANS(&target->coord));
        if (turnDelta < 0) {
            turnDelta = -turnDelta;
        }
        if (turnDelta >= 0x200) {
            actor->targetNode = NULL;
            func_actor_800300_80163074(arg0);
        }
    }
    playerActorTurnAimTowardPoint(arg0, MATRIX_TRANS(&target->coord));
    playerActorPlayFootstepCue(arg0);
}

static void func_actor_800300_80162D74(Task* arg0)
{
    GameActor*       actor;
    GfxCoord*        coord;
    GfxCoord*        target;
    WorldTargetNode* lock;
    u8*              head;
    VECTOR3*         vec;
    u16              state;

    coord                    = arg0->extra.tmd->coords;
    target                   = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x10;
    vec                      = (VECTOR3*)(head - 0x10);
    actor                    = arg0->work;
    lock                     = actor->targetNode;
    if (lock != NULL) {
        if (!(lock->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            worldTargetGetBodyPosition(lock, vec);
        } else {
            actor->statePhase = 2;
        }
    } else {
        ((VECTOR3*)(head - 0x10))->vx = target->coord.t[0];
        vec->vy                       = target->coord.t[1];
        vec->vz                       = target->coord.t[2];
    }
    state = actor->statePhase;
    switch (state) {
        case 0:
            actor->statePhase   = 1;
            actor->stateTimer   = 0;
            actor->movementMode = 3;
            playerActorPlayChildSlotsWithBlend(arg0, 0xC, 0, 5);
            /* fallthrough */
        case 1:
            actor->movementSign = 1;
            if (companionGetPlayerPlanarDistance(coord) < 0x601) {
                companionEnterIdle(arg0, 0);
            }
            break;
    }
    playerActorTurnBodyTowardPoint(arg0, vec);
    playerActorTurnAimTowardPoint(arg0, vec);
    playerActorPlayFootstepCue(arg0);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_800300_80162EEC(Task* arg0)
{
    if (((GameActor*)arg0->work)->statePhase == 1) {
        companionEnterIdle(arg0, 0);
    }
}

static void func_actor_800300_80162F24(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    s32        flag;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (actor->statePhase) {
        case 0:
            flag               = 1;
            actor->statePhase  = flag;
            coord->coord.t[1] += 0xC0;
        case 1:
            playerActorTickChildSlots(arg0);
            playerActorUpdateFacing(arg0);
            playerActorStepMovement(arg0);
            break;
    }
}

/// Handlers `func_actor_800300_80162F98` runs, indexed by `state`: the
/// gameplay module's own mode-2 player states.
static const TaskFuncTable7 D_actor_800300_80161E64 = { {
    Gp_PlayerMode2State0,
    Gp_PlayerMode2State1,
    playerActorMode2State2,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State4,
    Gp_PlayerMode2State1,
    playerActorMode2State6,
} };

static void func_actor_800300_80162F98(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable7 sp;

    sp    = D_actor_800300_80161E64;
    actor = arg0->work;
    sp.funcs[(u16)actor->state](arg0);
    playerActorUpdateFacing(arg0);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
        playerActorEnterStoppedPose(arg0, 0);
    }
}

/// Switches the actor's update into its approach behaviour (entry 1 of the
/// behaviour table), restarting the behaviour's step and counters and setting
/// the approach timer to 60 frames.
static void func_actor_800300_80163048(Task* arg0)
{
    GameActor* actor;

    actor                 = arg0->work;
    actor->state          = 1;
    actor->turnRateIndex  = 1;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->animationState = 0;
    actor->statePhase     = 0;
    actor->idleTicks      = 0;
    actor->actionValue    = 0x3C;
}

/// Switches the actor's update into its turn-to-face behaviour (entry 2 of
/// the behaviour table), restarting the behaviour's step and counters.
static void func_actor_800300_80163074(Task* arg0)
{
    GameActor* actor;

    actor                 = arg0->work;
    actor->state          = 2;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode   = 0;
    actor->turnRateIndex  = 1;
    actor->animationState = 0;
    actor->statePhase     = 0;
    actor->idleTicks      = 0;
}
