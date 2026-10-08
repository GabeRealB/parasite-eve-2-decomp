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

static void _actor800300Init(Task* task);
static void _actor800300UpdateMove(Task* task);
static void _actor800300DeferRemoval(Task* task);
static void _actor800300TickTextureSequences(Task* task);
static void _actor800300KillTask(Task* task);
static void _actor800300TickNormalMode(Task* task);
static void _actor800300FollowPlayerState(Task* task);
static void _actor800300TurnToTargetState(Task* task);
static void _actor800300TickMode(Task* task);
static void _actor800300IdleDecisionState(Task* task);
static void func_actor_800300_80162D74(Task* arg0);
static void _actor800300FlinchState(Task* task);
static void _actor800300TickDamageMode(Task* task);
static void _actor800300TickScriptedMode(Task* task);
static void _actor800300EnterFollowPlayer(Task* task);
static void _actor800300EnterTurnToTarget(Task* task);

// Native behavior selectors shared by this companion's state transitions.
enum {
    ACTOR_800300_STATE_FOLLOW_PLAYER           = 1,
    ACTOR_800300_STATE_TURN_TO_TARGET          = 2,
    ACTOR_800300_STATE_FLINCH                  = 5,
    ACTOR_800300_MOVEMENT_STOPPED              = 0,
    ACTOR_800300_TURN_DISABLED                 = 0,
    ACTOR_800300_TURN_RATE_64                  = 1,
    ACTOR_800300_ANIMATION_CONTROLLER_NONE     = 0,
    ACTOR_800300_ANIMATION_CONTROLLER_CLIP_END = 7,
    ACTOR_800300_MOVEMENT_BLEND_FRAMES         = 5,
    ACTOR_800300_FOLLOW_RESELECT_DELAY_TICKS   = 60
};

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
    { GAME_ACTOR_MESSAGE_RUN_TO, companionRunTo },
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

/// Stages a Q12 movement or pushback heading for all three motion contexts.
///
/// Requires a current composed root basis and caller-owned scratch. Uses the
/// Q12 pushback vector while its override is set, otherwise the root's composed
/// +Z axis times movementSign (-1 backward, 0 stopped, 1 forward). Narrows XYZ
/// to signed halfwords in scratch before copying them to contexts 0..2; keeps
/// all vector pads and retains neither pointer.
static inline void _actor800300PublishMotionDirection(GameActor* actor, const GfxCoord* rootCoord, CompanionMoveScratch* scratch)
{
    if (actor->usesPushbackDirection != 0) {
        scratch->motionDirection.vx = actor->pushbackDirection.vx;
        scratch->motionDirection.vy = actor->pushbackDirection.vy;
        scratch->motionDirection.vz = actor->pushbackDirection.vz;
    } else {
        scratch->motionDirection.vx = rootCoord->workm.m[0][2] * actor->movementSign;
        scratch->motionDirection.vy = rootCoord->workm.m[1][2] * actor->movementSign;
        scratch->motionDirection.vz = rootCoord->workm.m[2][2] * actor->movementSign;
    }
    actor->collisionMotionContexts[0].motionDirection.vx = scratch->motionDirection.vx;
    actor->collisionMotionContexts[0].motionDirection.vy = scratch->motionDirection.vy;
    actor->collisionMotionContexts[0].motionDirection.vz = scratch->motionDirection.vz;
    actor->collisionMotionContexts[1].motionDirection.vx = scratch->motionDirection.vx;
    actor->collisionMotionContexts[1].motionDirection.vy = scratch->motionDirection.vy;
    actor->collisionMotionContexts[1].motionDirection.vz = scratch->motionDirection.vz;
    actor->collisionMotionContexts[2].motionDirection.vx = scratch->motionDirection.vx;
    actor->collisionMotionContexts[2].motionDirection.vy = scratch->motionDirection.vy;
    actor->collisionMotionContexts[2].motionDirection.vz = scratch->motionDirection.vz;
}

/// Restores all three root translations from the last accepted collision position.
///
/// Requires live actor storage and a writable root in the cache's parent frame.
/// Copies full-width game coordinates; keeps rotation and composition stamp.
/// The caller invalidates composition before publishing the next motion heading.
static inline void _actor800300RestorePreviousPosition(GfxCoord* rootCoord, const GameActor* actor)
{
    rootCoord->coord.t[0] = actor->previousPosition.vx;
    rootCoord->coord.t[1] = actor->previousPosition.vy;
    rootCoord->coord.t[2] = actor->previousPosition.vz;
}

/// Starts native playback and links the noncombatant companion's two motion spheres.
///
/// Requires zeroed GameActor/CompanionWork storage, a loaded native animation
/// bank and a model with root and part-4 coordinates. Publishes the companion
/// task until teardown; the linked bodies borrow its work and model coordinates.
static void _actor800300Init(Task* task)
{
    enum {
        COMPANION_DISTRESS_INITIAL_INTERVAL       = 150,
        ACTOR_800300_ROOT_SPHERE_RADIUS           = 300,
        ACTOR_800300_PART4_SPHERE_RADIUS          = 200,
        ACTOR_800300_PART4_COORD_INDEX            = 4,
        ACTOR_800300_INITIAL_DECISION_BASE_TICKS  = 60,
        ACTOR_800300_INITIAL_DECISION_RANDOM_MASK = 0x7F
    };

    GameActor*             actor;
    TmdObject*             model;
    GfxCoord*              rootCoord;
    GfxCoord*              partCoords;
    GfxCoord**             coordsSlot;
    CompanionWork*         companion;
    WorldCollisionBody*    body;
    WorldCollisionContact* contacts;
    McSaveData*            save;
    s32                    bodyKeyKind;
    s8                     intervalByte;

    actor      = task->work;
    model      = task->extra.tmd;
    companion  = actor->companionWork;
    coordsSlot = &model->coords;
    rootCoord  = *coordsSlot;
    task->state++;
    task->msgTable                                 = D_actor_800300_80168880;
    task->exitCallback                             = &_actor800300KillTask;
    actor->animationSlotCount                      = GAME_ACTOR_NORMAL_ANIMATION_SLOTS;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] = task;
    rootCoord->parent                              = &gGfxViewCoord;
    rootCoord->composeStamp                        = GRAPHICS_COORD_DIRTY;
    model->flags                                   = 0;
    RotMatrix(&actor->rotation, &rootCoord->coord);
    companionInitNativeAnimation(task);
    actor->animationRate = ANIMATION_RATE_ONE;
    playerActorResetChildSlots(task, actor->actionArgument);
    // Both spheres share contacts; only the root requests the floor.
    contacts                                   = actor->collisionContacts;
    body                                       = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    actor->previousPosition.vx                 = rootCoord->coord.t[0];
    actor->previousPosition.vy                 = rootCoord->coord.t[1];
    actor->previousPosition.vz                 = rootCoord->coord.t[2];
    body->context.motion                       = &actor->collisionMotionContexts[0];
    body->coord                                = rootCoord;
    actor->collisionMotionContexts[0].contacts = contacts;
    save                                       = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    body->pos.vx                               = 0;
    body->pos.vy                               = -ACTOR_800300_ROOT_SPHERE_RADIUS;
    body->pos.vz                               = 0;
    {
        s32 characterId;

        characterId  = save->state.characterId;
        body->radius = ACTOR_800300_ROOT_SPHERE_RADIUS;
        body->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        bodyKeyKind  = WORLD_COLLISION_CONTACT_PLAYER_BODY;
        body->key    = characterId | bodyKeyKind;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_BODIES, body);
    }
    worldCollisionInitContacts(actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), 0);
    body->flags                               |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    body                                       = &actor->collisionBodies[GAME_ACTOR_BODY_PART4];
    partCoords                                 = task->extra.tmd->coords;
    body->context.motion                       = &actor->collisionMotionContexts[1];
    body->coord                                = partCoords + ACTOR_800300_PART4_COORD_INDEX;
    actor->collisionMotionContexts[1].contacts = contacts;
    body->pos.vx                               = 0;
    body->pos.vy                               = 0;
    body->pos.vz                               = 0;
    {
        s32 characterId;

        characterId  = save->state.characterId;
        body->radius = ACTOR_800300_PART4_SPHERE_RADIUS;
        body->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        body->key    = characterId | bodyKeyKind;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_BODIES, body);
    }
    body->flags               |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    actor->collisionEnableMask = GAME_ACTOR_COLLISION_REQUEST_MASK;
    companionSetDecisionDelay(task, ACTOR_800300_INITIAL_DECISION_BASE_TICKS, ACTOR_800300_INITIAL_DECISION_RANDOM_MASK);
    intervalByte                                = (s8)COMPANION_DISTRESS_INITIAL_INTERVAL;
    companion->activity.distress.flinchInterval = intervalByte;
}

/// Advances collision response, behavior and textures, then publishes motion and draws the shadow.
///
/// Requires initialized companion work, linked bodies and live room/view
/// resources. Borrows one CompanionMoveScratch block until return. Headings
/// use Q12 unit vectors; positions and shadow dimensions use game coordinates.
static void _actor800300UpdateMove(Task* task)
{
    enum {
        ACTOR_800300_MAX_VERTICAL_STEP = 512,
        ACTOR_800300_ROOT_FLOOR_LIFT   = 16,
        ACTOR_800300_SHADOW_HALF_SIZE  = 512,
    };

    void**                cursorSlot;
    CompanionMoveScratch* blockEnd;
    CompanionMoveScratch* block;
    GameActor*            actor;
    TmdObject*            model;
    TmdObject*            coordsModel;
    GfxCoord*             rootCoord;
    WorldCollisionBody*   bodies[2];
    s32                   verticalDelta;
    s32                   bodyIndex;
    s8                    updateRequests;

    cursorSlot                                        = SCRATCH_HEAD_ADDR;
    blockEnd                                          = SCRATCH_HEAD_AT(cursorSlot, CompanionMoveScratch);
    model                                             = task->extra.tmd;
    SCRATCH_HEAD_AT(cursorSlot, CompanionMoveScratch) = blockEnd - 1;
    // Retain the two model locals so the frame prologue keeps its original scheduling.
    coordsModel = model;
    block       = blockEnd - 1;
    actor       = task->work;
    rootCoord   = coordsModel->coords;
    // Reject abrupt height changes before consuming the preceding grid pass.
    if (actor->mode != GAME_ACTOR_MODE_SCRIPTED &&
        (verticalDelta = rootCoord->coord.t[1], verticalDelta = verticalDelta - actor->previousPosition.vy, verticalDelta = ABS(verticalDelta), verticalDelta >= ACTOR_800300_MAX_VERTICAL_STEP)) {
        _actor800300RestorePreviousPosition(rootCoord, actor);
    } else {
        if (actor->collisionEnableMask & (1 << GAME_ACTOR_BODY_ROOT)) {
            actor->gridResponse = worldCollisionApplyResponsePushback(rootCoord, actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), &actor->surfaceClass);
            if ((s8)actor->gridResponse == WORLD_COLLISION_PUSHBACK_OPPOSED) {
                _actor800300RestorePreviousPosition(rootCoord, actor);
            }
        } else {
            actor->gridResponse = WORLD_COLLISION_PUSHBACK_NO_GRID_HIT;
        }
        actor->previousPosition.vx = rootCoord->coord.t[0];
        actor->previousPosition.vy = rootCoord->coord.t[1];
        actor->previousPosition.vz = rootCoord->coord.t[2];
    }
    // Apply deferred enable/disable requests to this model's two linked bodies.
    bodies[0] = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    bodies[1] = &actor->collisionBodies[GAME_ACTOR_BODY_PART4];
    for (bodyIndex = 0; bodyIndex < (s32)ARRAY_SIZE(bodies); bodyIndex++) {
        updateRequests = actor->pendingCollisionUpdates;
        if ((updateRequests >> bodyIndex) & 1) {
            actor->collisionEnableMask |= 1 << bodyIndex;
            bodies[bodyIndex]->flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        } else if (updateRequests & ((1 << GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT) << bodyIndex)) {
            actor->collisionEnableMask &= ~(1 << bodyIndex);
            bodies[bodyIndex]->flags   &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    }
    actor->pendingCollisionUpdates = 0;
    if (D_80115768 == 0) {
        _actor800300TickMode(task);
    }
    _actor800300TickTextureSequences(task);
    worldCollisionClearContacts(actor->collisionContacts);
    if (actor->collisionEnableMask & (1 << GAME_ACTOR_BODY_ROOT)) {
        rootCoord->coord.t[1] = actor->previousPosition.vy + ACTOR_800300_ROOT_FLOOR_LIFT;
    }
    // Publish the composed heading before probing the floor for the shadow.
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    _actor800300PublishMotionDirection(actor, rootCoord, block);
    if (!(coordsModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&rootCoord->workm), &block->shadowCentre) != 0) {
            effectDrawGroundShadow(&block->shadowCentre, ACTOR_800300_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(CompanionMoveScratch);
}

/// Advances two null-terminated texture sequences on separate model texture regions.
///
/// Selectors are signed-byte views: A must be 0..4 and B 0..2 (0 inactive).
/// Frame indices start at zero and advance through the selected list's NULL
/// terminator. X counts two positions per VRAM word; width counts words, and
/// Y/height count rows. Requires a live texture page, writable upload lists
/// and eight free scratch bytes for a RECT. Image pixels survive queued transfers.
static void _actor800300TickTextureSequences(Task* task)
{
    enum {
        ACTOR_800300_TEXTURE_A_Y           = 0x40,
        ACTOR_800300_TEXTURE_A_WIDTH_WORDS = 25,
        ACTOR_800300_TEXTURE_B_X           = 12,
        ACTOR_800300_TEXTURE_B_Y           = 0x60,
        ACTOR_800300_TEXTURE_B_WIDTH_WORDS = 14,
        ACTOR_800300_TEXTURE_HEIGHT        = 20,
        ACTOR_800300_TEXTURE_A_FRAME_TICKS = 4,
        ACTOR_800300_TEXTURE_B_FRAME_TICKS = 8,
    };

    RECT*             uploadRect;
    GameActor*        actor;
    GpuImageUpload*** sequences;
    s32               sequenceIndex;
    u32               secondSequenceIndex;
    GpuImageUpload*   uploadList;

    uploadRect = SCRATCH_STACK_RESERVE_BLOCK(RECT);
    actor      = task->work;

    if ((s8)actor->textureSequenceA != 0) {
        actor->textureDelayA--;
        if ((s8)actor->textureDelayA <= 0) {
            sequences     = D_actor_800300_80168950;
            sequenceIndex = (s8)actor->textureSequenceA - 1;
            uploadList    = sequences[sequenceIndex][(s8)actor->textureFrameA];
            if (uploadList != NULL) {
                uploadRect->x = 0;
                uploadRect->y = ACTOR_800300_TEXTURE_A_Y;
                uploadRect->w = ACTOR_800300_TEXTURE_A_WIDTH_WORDS;
                uploadRect->h = ACTOR_800300_TEXTURE_HEIGHT;
                actorRenderUploadTexture(task, uploadList, uploadRect);
                actor->textureDelayA = ACTOR_800300_TEXTURE_A_FRAME_TICKS;
                actor->textureFrameA++;
            } else {
                actor->textureSequenceA = 0;
            }
        }
    }

    if ((s8)actor->textureSequenceB != 0) {
        actor->textureDelayB--;
        if ((s8)actor->textureDelayB <= 0) {
            sequences           = D_actor_800300_80168960;
            secondSequenceIndex = (s8)actor->textureSequenceB - 1;
            uploadList          = sequences[secondSequenceIndex][(s8)actor->textureFrameB];
            if (uploadList != NULL) {
                uploadRect->x = ACTOR_800300_TEXTURE_B_X;
                uploadRect->y = ACTOR_800300_TEXTURE_B_Y;
                uploadRect->w = ACTOR_800300_TEXTURE_B_WIDTH_WORDS;
                uploadRect->h = ACTOR_800300_TEXTURE_HEIGHT;
                actorRenderUploadTexture(task, uploadList, uploadRect);
                actor->textureDelayB = ACTOR_800300_TEXTURE_B_FRAME_TICKS;
                actor->textureFrameB++;
            } else {
                actor->textureSequenceB = 0;
            }
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(RECT);
}

/// Defers companion teardown to the main task's next dispatch.
static void _actor800300DeferRemoval(Task* task)
{
    enum {
        ACTOR_800300_TASK_TEARDOWN = 3,
    };

    task->state = ACTOR_800300_TASK_TEARDOWN;
}

/// Releases the published companion and unlinks its borrowed collision storage before task destruction.
///
/// Runs as task state 3 or the exit callback. Requires initialized GameActor
/// storage; taskKill owns the remaining model, work and child-task teardown.
static void _actor800300KillTask(Task* task)
{
    GameActor* actor;

    actor                                          = task->work;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] = NULL;
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_ROOT]);
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_PART4]);
    taskKill(task);
}

/// State handlers of the actor's main task, indexed by its state: set-up, the
/// per-frame update, a step that only advances to the last state, and the
/// teardown.
static const TaskFuncTable4 D_actor_800300_80161E24 = { {
    _actor800300Init,
    _actor800300UpdateMove,
    _actor800300DeferRemoval,
    _actor800300KillTask,
} };

void actor800300Task(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_800300_80161E24;
    states.funcs[task->state](task);
}

/// Handlers `_actor800300TickMode` runs, indexed by `mode`.
static const TaskFuncTable3 D_actor_800300_80161E34 = { {
    _actor800300TickNormalMode,
    _actor800300TickDamageMode,
    _actor800300TickScriptedMode,
} };

/// Behaviours `_actor800300TickNormalMode` runs, indexed by `state`.
static const TaskFuncTable9 D_actor_800300_80161E40 = { {
    _actor800300IdleDecisionState,
    _actor800300FollowPlayerState,
    _actor800300TurnToTargetState,
    _actor800300IdleDecisionState,
    _actor800300IdleDecisionState,
    _actor800300FlinchState,
    _actor800300IdleDecisionState,
    func_actor_800300_80162D74,
    _actor800300IdleDecisionState,
} };

/// Runs normal companion behavior, recurring distress and contact reactions, then advances motion.
///
/// Requires live actor/companion work, native playback, contacts and a player
/// root in the same room frame. Normal state must be 0..8. Decision and distress
/// delay counts active normal-mode ticks; flinch intervals count non-flinch
/// ticks. Interval/count comparisons use signed bytes and retain counter wrap.
/// Fatal distress damage stops this tick early.
static void _actor800300TickNormalMode(Task* task)
{
    enum {
        ACTOR_800300_DISTRESS_START_TICKS    = 780,
        ACTOR_800300_DISTRESS_SOUND_BASE     = 0x55170005,
        ACTOR_800300_DISTRESS_ATTACK_KEY     = 0x40010,
        ACTOR_800300_ANIMATION_FLINCH        = 16,
        ACTOR_800300_ANIMATION_SEVERE_FLINCH = 17,
        ACTOR_800300_FLINCH_BLEND_FRAMES     = 3,
        ACTOR_800300_PART4_HIT_SOUND         = 6,
        ACTOR_800300_OTHER_BODY_HIT_SOUND    = 7,
        ACTOR_800300_HIT_REGION_NONE         = 0,
        ACTOR_800300_HIT_REGION_PART4        = 1,
    };

    enum {
        COMPANION_DISTRESS_INTERVAL_DECREMENT  = 7,
        COMPANION_DISTRESS_INTERVAL_THRESHOLD  = 90,
        COMPANION_DISTRESS_RESTART_INTERVAL    = 60,
        COMPANION_DISTRESS_SEVERE_FLINCH_COUNT = 5
    };

    TaskFuncTable9 states;
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      rootCoord;
    s8             nextInterval;
    s32            pan;
    s32            depth;
    s32            animationSet;
    s32            hitSound;

    states    = D_actor_800300_80161E40;
    actor     = task->work;
    companion = actor->companionWork;
    rootCoord = task->extra.tmd->coords;
    if (companion->decisionTimer > 0) {
        companion->decisionTimer = (u16)companion->decisionTimer - 1;
    }
    // Distress interrupts normal behavior once the persistent distress counter reaches its threshold.
    if (D_map_neo_ark_8017A99C >= ACTOR_800300_DISTRESS_START_TICKS) {
        if (actor->state != ACTOR_800300_STATE_FLINCH) {
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
                actor->animationState   = ACTOR_800300_ANIMATION_CONTROLLER_CLIP_END;
                actor->state            = ACTOR_800300_STATE_FLINCH;
                actor->movementMode     = ACTOR_800300_MOVEMENT_STOPPED;
                actor->turnRateIndex    = ACTOR_800300_TURN_DISABLED;
                actor->statePhase       = 0;
                actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
                worldCoordPlaySound(rootCoord, (rand() & 1) + ACTOR_800300_DISTRESS_SOUND_BASE, 0);
                if (companionApplyDamage(task, 0, ACTOR_800300_DISTRESS_ATTACK_KEY, 0) != 0) {
                    return;
                }
                animationSet = ACTOR_800300_ANIMATION_FLINCH;
                if ((s8)companion->activity.distress.flinchCount >= COMPANION_DISTRESS_SEVERE_FLINCH_COUNT) {
                    animationSet = ACTOR_800300_ANIMATION_SEVERE_FLINCH;
                }
                playerActorPlayChildSlotsWithBlend(task, animationSet, 0, ACTOR_800300_FLINCH_BLEND_FRAMES);
            }
        }
    }
    states.funcs[actor->state](task);
    // Resolve contacts after the state step; fatal distress returns before this phase.
    if ((s8)actor->recoveryTicks == 0) {
        playerActorResolveBodyContacts(task, &actor->collisionContacts[0]);
        if ((u16)actor->hitRegion != ACTOR_800300_HIT_REGION_NONE) {
            companionEnterDamageReaction(task);
            pan      = (s8)worldCoordGetOriginAudioPan(rootCoord);
            depth    = (s8)worldCoordGetOriginAudioDepth(rootCoord);
            hitSound = ACTOR_800300_OTHER_BODY_HIT_SOUND;
            if ((u16)actor->hitRegion == ACTOR_800300_HIT_REGION_PART4) {
                hitSound = ACTOR_800300_PART4_HIT_SOUND;
            }
            sndEvtRequestScriptStart(hitSound, pan, depth);
        }
    }
    playerActorTickAnimationState(task);
    playerActorTickChildSlots(task);
    playerActorUpdateFacing(task);
    playerActorStepMovement(task);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
        playerActorEnterStoppedPose(task, 0);
    }
}

/// Follows the player, choosing run or slow walk and returning to idle within 768 game units.
///
/// Requires live actor/native playback and both roots in the same room frame.
/// Forces walking after 180 moving ticks; speed may be reconsidered after the
/// 60-tick delay. Body and aim track the player, and footstep cues advance.
static void _actor800300FollowPlayerState(Task* task)
{
    enum {
        ACTOR_800300_FOLLOW_SELECT_SPEED       = 0,
        ACTOR_800300_FOLLOW_WALK               = 1,
        ACTOR_800300_FOLLOW_RUN                = 2,
        ACTOR_800300_FOLLOW_FORCED_WALK        = 3,
        ACTOR_800300_MOVEMENT_RUN              = 3,
        ACTOR_800300_MOVEMENT_SLOW_WALK        = 7,
        ACTOR_800300_MOVE_FORWARD              = 1,
        ACTOR_800300_ANIMATION_WALK            = 2,
        ACTOR_800300_ANIMATION_RUN             = 4,
        ACTOR_800300_FOLLOW_RUN_DISTANCE       = 0xE00,
        ACTOR_800300_FOLLOW_ARRIVAL_DISTANCE   = 768,
        ACTOR_800300_FOLLOW_FORCE_WALK_TICKS   = 180,
        ACTOR_800300_FOLLOW_SPEED_JITTER_MASK  = 0x3FF,
        ACTOR_800300_FOLLOW_RUN_KEEP_DISTANCE  = 0x800,
        ACTOR_800300_FOLLOW_WALK_KEEP_DISTANCE = 0xC00,
    };

    GameActor* actor;
    GfxCoord*  rootCoord;
    GfxCoord*  playerCoord;
    VECTOR3*   playerPoint;
    s32        distance;
    s32        speedJitter;
    s32        animationSet;

    rootCoord   = task->extra.tmd->coords;
    playerCoord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor       = task->work;
    switch (actor->statePhase) {
        case ACTOR_800300_FOLLOW_SELECT_SPEED:
            actor->stateTimer = 0;
            if (companionGetPlayerPlanarDistance(rootCoord) >= ACTOR_800300_FOLLOW_RUN_DISTANCE) {
                animationSet        = ACTOR_800300_ANIMATION_RUN;
                actor->statePhase   = ACTOR_800300_FOLLOW_RUN;
                actor->movementMode = ACTOR_800300_MOVEMENT_RUN;
            } else {
            selectWalk:
                if (actor->statePhase != ACTOR_800300_FOLLOW_FORCED_WALK) {
                    actor->statePhase = ACTOR_800300_FOLLOW_WALK;
                }
                actor->movementMode = ACTOR_800300_MOVEMENT_SLOW_WALK;
                animationSet        = ACTOR_800300_ANIMATION_WALK;
            }
            playerActorPlayChildSlotsWithBlend(task, animationSet, 0, ACTOR_800300_MOVEMENT_BLEND_FRAMES);
            /* fallthrough */
        case ACTOR_800300_FOLLOW_WALK:
        case ACTOR_800300_FOLLOW_RUN:
        case ACTOR_800300_FOLLOW_FORCED_WALK:
            actor->movementSign = ACTOR_800300_MOVE_FORWARD;
            distance            = companionGetPlayerPlanarDistance(rootCoord);
            if (distance < (ACTOR_800300_FOLLOW_ARRIVAL_DISTANCE + 1)) {
                companionEnterIdle(task, 0);
                break;
            }
            if (actor->statePhase == ACTOR_800300_FOLLOW_FORCED_WALK) {
                break;
            }
            actor->stateTimer++;
            if (actor->stateTimer == ACTOR_800300_FOLLOW_FORCE_WALK_TICKS) {
                actor->statePhase = ACTOR_800300_FOLLOW_FORCED_WALK;
                goto selectWalk;
            }
            if (actor->actionValue > 0) {
                actor->actionValue = (u16)actor->actionValue - 1;
            } else {
                speedJitter = rand() & ACTOR_800300_FOLLOW_SPEED_JITTER_MASK;
                if (((ACTOR_800300_FOLLOW_RUN_KEEP_DISTANCE - speedJitter) >= distance && actor->statePhase == ACTOR_800300_FOLLOW_RUN) || (distance >= speedJitter + ACTOR_800300_FOLLOW_WALK_KEEP_DISTANCE && actor->statePhase == ACTOR_800300_FOLLOW_WALK)) {
                    actor->statePhase  = ACTOR_800300_FOLLOW_SELECT_SPEED;
                    actor->actionValue = ACTOR_800300_FOLLOW_RESELECT_DELAY_TICKS;
                }
            }
            break;
    }
    playerPoint = MATRIX_TRANS(&playerCoord->coord);
    playerActorTurnBodyTowardPoint(task, playerPoint);
    playerActorTurnAimTowardPoint(task, playerPoint);
    playerActorPlayFootstepCue(task);
}

/// Turns in place toward the lock target, or the player when no target is selected.
///
/// Requires live actor/native playback and player/model coordinates. Completes
/// within 128 yaw units (4096 per turn); aim follows the player. An invalid lock
/// skips the turn phases. Borrows 16 scratch bytes for XYZ; the last word is untouched.
static void _actor800300TurnToTargetState(Task* task)
{
    enum {
        ACTOR_800300_TARGET_POINT_SCRATCH_BYTES = 16,
        ACTOR_800300_TARGET_TURN_START          = 0,
        ACTOR_800300_TARGET_TURN_ACTIVE         = 1,
        ACTOR_800300_TARGET_TURN_INVALID        = 2,
        ACTOR_800300_TARGET_TURN_TOLERANCE      = 128,
        ACTOR_800300_TURN_NEGATIVE              = -1,
        ACTOR_800300_TURN_POSITIVE              = 1,
        ACTOR_800300_ANIMATION_TURN_LEFT        = 5,
        ACTOR_800300_ANIMATION_TURN_RIGHT       = 6,
    };

    VECTOR3*         targetPoint;
    GameActor*       actor;
    WorldTargetNode* node;
    TmdObject*       playerModel;
    GfxCoord*        playerCoord;
    s32              yawMagnitude;
    s32              animationSet;
    s32              turnPhase;

    actor       = task->work;
    playerModel = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd;
    targetPoint = SCRATCH_STACK_RESERVE_BYTES(ACTOR_800300_TARGET_POINT_SCRATCH_BYTES);
    node        = actor->targetNode;
    playerCoord = playerModel->coords;
    if (node != NULL) {
        if (!(node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            worldTargetGetBodyPosition(node, targetPoint);
        } else {
            actor->statePhase = ACTOR_800300_TARGET_TURN_INVALID;
        }
    } else {
        targetPoint->vx = playerCoord->coord.t[0];
        targetPoint->vy = playerCoord->coord.t[1];
        targetPoint->vz = playerCoord->coord.t[2];
    }
    switch (actor->statePhase) {
        case ACTOR_800300_TARGET_TURN_START:
            turnPhase         = ACTOR_800300_TARGET_TURN_ACTIVE;
            actor->statePhase = turnPhase;
            if (playerActorGetTurnToPoint(task, targetPoint) < 0) {
                actor->actionValue = ACTOR_800300_TURN_NEGATIVE;
                animationSet       = ACTOR_800300_ANIMATION_TURN_LEFT;
            } else {
                actor->actionValue = ACTOR_800300_TURN_POSITIVE;
                animationSet       = ACTOR_800300_ANIMATION_TURN_RIGHT;
            }
            playerActorPlayChildSlotsWithBlend(task, animationSet, 0, ACTOR_800300_MOVEMENT_BLEND_FRAMES);
            /* fallthrough */
        case ACTOR_800300_TARGET_TURN_ACTIVE:
            actor->turnSign = (u8)actor->actionValue;
            yawMagnitude    = playerActorGetTurnToPoint(task, targetPoint);
            if (yawMagnitude < 0) {
                yawMagnitude = -yawMagnitude;
            }
            if ((yawMagnitude < (ACTOR_800300_TARGET_TURN_TOLERANCE + 1)) || (actor->statePhase == ACTOR_800300_TARGET_TURN_INVALID)) {
                companionEnterIdle(task, 0);
            }
            break;
    }
    playerActorTurnAimTowardPoint(task, MATRIX_TRANS(&playerCoord->coord));
    playerActorPlayFootstepCue(task);
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_800300_TARGET_POINT_SCRATCH_BYTES);
}

/// Clears movement requests, dispatches the selected companion mode and releases the pushback override.
///
/// Requires initialized GameActor work with mode 0..2. The selected mode owns
/// this tick's movement/turn signs; the previous collision pushback heading
/// remains available to it until dispatch returns.
static void _actor800300TickMode(Task* task)
{
    GameActor*     actor;
    TaskFuncTable3 modes;

    modes               = D_actor_800300_80161E34;
    actor               = task->work;
    actor->movementSign = 0;
    actor->turnSign     = 0;
    modes.funcs[actor->mode](task);
    actor->usesPushbackDirection = 0;
}

/// Periodically chooses player-following or an in-place turn while idle.
///
/// Requires live actor/companion work, native playback and the player root.
/// Decisions recur after 20..83 active normal ticks. Follow starts at planar
/// distance 1409..3583 game units; a yaw gap of at least 512/4096 turn overrides
/// it with a turn toward the player. Aim and footstep cues still advance between decisions.
static void _actor800300IdleDecisionState(Task* task)
{
    enum {
        ACTOR_800300_IDLE_DECISION_BASE_TICKS  = 20,
        ACTOR_800300_IDLE_DECISION_RANDOM_MASK = 0x3F,
        ACTOR_800300_IDLE_FOLLOW_MIN_DISTANCE  = 0x580,
        ACTOR_800300_IDLE_FOLLOW_MAX_DISTANCE  = 0xE00,
        ACTOR_800300_IDLE_TURN_THRESHOLD       = 512,
    };

    GameActor* actor;
    GameActor* timerActor;
    GfxCoord*  rootCoord;
    GfxCoord*  playerCoord;
    s32        yawMagnitude;

    actor       = task->work;
    rootCoord   = task->extra.tmd->coords;
    playerCoord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    // Reload work after the player-slot lookup before testing the decision timer.
    timerActor = task->work;
    if (timerActor->companionWork->decisionTimer <= 0) {
        companionSetDecisionDelay(task, ACTOR_800300_IDLE_DECISION_BASE_TICKS, ACTOR_800300_IDLE_DECISION_RANDOM_MASK);
        if ((u32)(companionGetPlayerPlanarDistance(rootCoord) - (ACTOR_800300_IDLE_FOLLOW_MIN_DISTANCE + 1)) < (ACTOR_800300_IDLE_FOLLOW_MAX_DISTANCE - ACTOR_800300_IDLE_FOLLOW_MIN_DISTANCE - 1)) {
            _actor800300EnterFollowPlayer(task);
        }
        yawMagnitude = playerActorGetTurnToPoint(task, MATRIX_TRANS(&playerCoord->coord));
        if (yawMagnitude < 0) {
            yawMagnitude = -yawMagnitude;
        }
        if (yawMagnitude >= ACTOR_800300_IDLE_TURN_THRESHOLD) {
            actor->targetNode = NULL;
            _actor800300EnterTurnToTarget(task);
        }
    }
    playerActorTurnAimTowardPoint(task, MATRIX_TRANS(&playerCoord->coord));
    playerActorPlayFootstepCue(task);
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

/// Returns to normal idle after the distress clip-end controller advances phase 0 to 1.
///
/// Requires live GameActor work and native playback; earlier phases keep the
/// flinch state active without starting another animation.
static void _actor800300FlinchState(Task* task)
{
    enum {
        ACTOR_800300_FLINCH_FINISHED = 1,
    };

    if (((GameActor*)task->work)->statePhase == ACTOR_800300_FLINCH_FINISHED) {
        companionEnterIdle(task, 0);
    }
}

/// Lifts the root once on damage-mode entry, then advances playback, facing and movement.
///
/// Requires live GameActor/model playback. Phase 0 adds 192 local game units
/// to Y and enters phase 1; other phases do nothing. HP and contact damage are
/// handled by the callers that select this mode.
static void _actor800300TickDamageMode(Task* task)
{
    enum {
        ACTOR_800300_DAMAGE_START     = 0,
        ACTOR_800300_DAMAGE_ACTIVE    = 1,
        ACTOR_800300_DAMAGE_ROOT_LIFT = 192,
    };

    GameActor* actor;
    GfxCoord*  rootCoord;
    s32        activePhase;

    actor     = task->work;
    rootCoord = task->extra.tmd->coords;
    switch (actor->statePhase) {
        case ACTOR_800300_DAMAGE_START:
            activePhase            = ACTOR_800300_DAMAGE_ACTIVE;
            actor->statePhase      = activePhase;
            rootCoord->coord.t[1] += ACTOR_800300_DAMAGE_ROOT_LIFT;
            /* fallthrough */
        case ACTOR_800300_DAMAGE_ACTIVE:
            playerActorTickChildSlots(task);
            playerActorUpdateFacing(task);
            playerActorStepMovement(task);
            break;
    }
}

/// Handlers `_actor800300TickScriptedMode` runs, indexed by `state`: the
/// gameplay module's own mode-2 player states.
static const TaskFuncTable7 D_actor_800300_80161E64 = { {
    playerActorScriptedState0,
    Gp_PlayerMode2State1,
    playerActorMode2State2,
    Gp_PlayerMode2State1,
    playerActorTickScriptedMoveTo,
    Gp_PlayerMode2State1,
    playerActorMode2State6,
} };

/// Dispatches the companion's scripted player states, updates facing and selects the zero-HP pose.
///
/// Requires live actor/model playback and a state within this package's seven
/// handlers (0..6). No bound check runs; message-selected states must satisfy
/// that domain. Movement and slot playback belong to the selected handler.
static void _actor800300TickScriptedMode(Task* task)
{
    GameActor*     actor;
    TaskFuncTable7 states;

    states = D_actor_800300_80161E64;
    actor  = task->work;
    states.funcs[(u16)actor->state](task);
    playerActorUpdateFacing(task);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
        playerActorEnterStoppedPose(task, 0);
    }
}

/// Starts native player-following with 64-unit yaw steps and a 60-tick speed-selection delay.
///
/// Requires live GameActor work. Resets the behavior phase, idle counter and
/// animation controller; the follow state selects movement speed and playback.
static void _actor800300EnterFollowPlayer(Task* task)
{
    GameActor* actor;

    actor                 = task->work;
    actor->state          = ACTOR_800300_STATE_FOLLOW_PLAYER;
    actor->turnRateIndex  = ACTOR_800300_TURN_RATE_64;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->animationState = ACTOR_800300_ANIMATION_CONTROLLER_NONE;
    actor->statePhase     = 0;
    actor->idleTicks      = 0;
    actor->actionValue    = ACTOR_800300_FOLLOW_RESELECT_DELAY_TICKS;
}

/// Starts a native in-place turn toward the selected lock target or the player.
///
/// Requires live GameActor work. Stops movement, selects 64-unit yaw steps
/// (4096 per turn) and clears the behavior phase, idle counter and animation controller.
static void _actor800300EnterTurnToTarget(Task* task)
{
    GameActor* actor;

    actor                 = task->work;
    actor->state          = ACTOR_800300_STATE_TURN_TO_TARGET;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode   = ACTOR_800300_MOVEMENT_STOPPED;
    actor->turnRateIndex  = ACTOR_800300_TURN_RATE_64;
    actor->animationState = ACTOR_800300_ANIMATION_CONTROLLER_NONE;
    actor->statePhase     = 0;
    actor->idleTicks      = 0;
}
