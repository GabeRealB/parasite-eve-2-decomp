#include "rooms/akropolis_observatory.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "observatory_private.h"

#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "assets/rooms/akropolis/observatory/declarations.inc"
#include "assets/engine/animation/player_schema.h"

/// Work block of the observatory's one-time scripted player scene, owned by the scene task through `Task::work`.
typedef struct {
    Task* playerTask;    // Player task the scene's animation and release messages are sent to (NULL when the player slot is empty)
    u16   field_4;       // Never accessed; role unproven
    u16   followUpIndex; // Entry of the follow-up clip table to start once the player's current animation has ended
} _AkropolisObservatorySceneWork;
STATIC_ASSERT_SIZEOF(_AkropolisObservatorySceneWork, 8);

static void _akropolisObservatoryScriptedBattleTask(Task* task);

/// Clip-table values and scheduler priority of the scripted battle controller.
enum {
    AKROPOLIS_OBSERVATORY_SCENE_NO_FOLLOW_UP   = -1,
    AKROPOLIS_OBSERVATORY_SCENE_FOLLOW_UP_CLIP = 1,
};

/// Player follow-up clip IDs indexed by the scene work's `followUpIndex`.
static s16 _gAkropolisObservatorySceneFollowUpClips[2] = {
    AKROPOLIS_OBSERVATORY_SCENE_NO_FOLLOW_UP,
    AKROPOLIS_OBSERVATORY_SCENE_FOLLOW_UP_CLIP,
};

TaskDesc gAkropolisObservatoryScriptedBattleTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    .callback = _akropolisObservatoryScriptedBattleTask,
    .data     = { .value = 0 },
};

#include "assets/rooms/akropolis/observatory/markers.inc"
#include "assets/rooms/akropolis/observatory/view_masks.inc"
#include "assets/rooms/akropolis/observatory/header.inc"
#include "assets/rooms/akropolis/observatory/geometry.inc"
#include "assets/rooms/akropolis/observatory/view_boundaries.inc"
#include "assets/rooms/akropolis/observatory/action_triggers.inc"
#include "assets/rooms/akropolis/observatory/placements.inc"
#include "assets/rooms/akropolis/observatory/lights.inc"
#define ROOM_PART _gAkropolisObservatoryView1SpriteBatches
#include "assets/rooms/akropolis/observatory/sprites.inc"
/// Unreferenced; its role is unproven.
u16 gAkropolisObservatoryUnusedHalfwords[4] = {
    256,
    0,
    0,
    0,
};
#define ROOM_PART _gAkropolisObservatoryView4SpriteSources
#include "assets/rooms/akropolis/observatory/sprites.inc"
#include "assets/rooms/akropolis/observatory/cameras.inc"
#include "assets/rooms/akropolis/observatory/vibrations.inc"
#include "assets/rooms/akropolis/observatory/surfaces.inc"

/// Runs the observatory's scripted player pose and enemy-entrance battle handoff.
static void _akropolisObservatoryScriptedBattleTask(Task* task)
{
    enum {
        AKROPOLIS_OBSERVATORY_SCENE_INITIALIZE             = 0,
        AKROPOLIS_OBSERVATORY_SCENE_DEFER_FIRST            = 1,
        AKROPOLIS_OBSERVATORY_SCENE_DEFER_SECOND           = 2,
        AKROPOLIS_OBSERVATORY_SCENE_WAIT_ENTRANCE          = 3,
        AKROPOLIS_OBSERVATORY_SCENE_DEFER_BATTLE           = 4,
        AKROPOLIS_OBSERVATORY_SCENE_RESTORE_PLAYER         = 5,
        AKROPOLIS_OBSERVATORY_SCENE_RELEASE_PLAYER         = 6,
        AKROPOLIS_OBSERVATORY_SCENE_CLIP                   = 1,
        AKROPOLIS_OBSERVATORY_SCENE_HOLD_ENTRANCE          = 0,
        AKROPOLIS_OBSERVATORY_SCENE_FIRST_WAVE             = 2,
        AKROPOLIS_OBSERVATORY_SCENE_FIELD_ROOM             = 1,
        AKROPOLIS_OBSERVATORY_SCENE_FOLLOW_UP_BLEND_FRAMES = 10
    };
    AnimationPlayRequest            playbackRequest;
    AnimationPlayRequest            followUpRequest;
    AnimationPlayRequest*           followUpMessage;
    _AkropolisObservatorySceneWork* sceneWork;
    _AkropolisObservatorySceneWork* idleWork;
    _AkropolisObservatorySceneWork* playbackWork;
    _AkropolisObservatorySceneWork* allocatedWork;
    s16*                            followUpEntry;
    u16                             followUpAnimationId;
    s32                             allocationCheckOrClip;
    s32                             weaponIndex;
    s32                             animationBankIndex;

    sceneWork = task->work;
    switch (task->state) {
        case AKROPOLIS_OBSERVATORY_SCENE_INITIALIZE:
            if (gAttachmentState.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            // Capture the player once; teardown owns the eight-byte work allocation.
            allocatedWork         = memCalloc(sizeof(*allocatedWork), 0);
            allocationCheckOrClip = (allocatedWork == NULL);
            task->work            = allocatedWork;
            if (allocationCheckOrClip) {
                taskKill(task);
            } else {
                memFillBytes(allocatedWork, 0, sizeof(*allocatedWork));
                allocatedWork->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            }
            sceneWork = task->work;
            if (sceneWork->playerTask != NULL) {
                playbackRequest.source.sets          = gAkropolisObservatoryPlayerAnimationSets;
                playbackRequest.animationId          = AKROPOLIS_OBSERVATORY_SCENE_CLIP;
                playbackRequest.blend                = ANIMATION_BLEND_RESET;
                playbackRequest.blendFrames          = 0;
                playbackRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(sceneWork->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &playbackRequest, 0);
            }
            gSceneCombatState.actor03700Wave = AKROPOLIS_OBSERVATORY_SCENE_HOLD_ENTRANCE;
            /* fallthrough */
        case AKROPOLIS_OBSERVATORY_SCENE_DEFER_FIRST:
        case AKROPOLIS_OBSERVATORY_SCENE_DEFER_SECOND:
        case AKROPOLIS_OBSERVATORY_SCENE_DEFER_BATTLE:
            task->state = task->state + 1;
            break;
        case AKROPOLIS_OBSERVATORY_SCENE_WAIT_ENTRANCE:
            if (gSceneCombatState.actor03700Wave == AKROPOLIS_OBSERVATORY_SCENE_FIRST_WAVE) {
                sceneEngageBattle(1);
                task->state = task->state + 1;
            }
            break;
        case AKROPOLIS_OBSERVATORY_SCENE_RESTORE_PLAYER:
            // Restore the equipped weapon bank before returning to the field room.
            weaponIndex                          = gPlayerStatus.weapon;
            animationBankIndex                   = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponIndex);
            playbackRequest.source.index         = animationBankIndex;
            playbackRequest.animationId          = PLAYER_ANIMATION_IDLE;
            playbackRequest.blend                = ANIMATION_BLEND_RESET;
            playbackRequest.blendFrames          = 0;
            playbackRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(sceneWork->playerTask, ANIMATION_MESSAGE_PLAY, &playbackRequest, 0);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = AKROPOLIS_OBSERVATORY_SCENE_FIELD_ROOM;
            gGameSession->location.roomState                            = AKROPOLIS_OBSERVATORY_SCENE_FIELD_ROOM;
            gGameSession->roomObjsDirty                                 = 1;
            gGameSession->viewDirty                                     = 1;
            task->state                                                 = task->state + 1;
            break;
        case AKROPOLIS_OBSERVATORY_SCENE_RELEASE_PLAYER:
            taskMessageDispatch(sceneWork->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            taskKill(task);
            break;
    }

    // The original also runs the idle tail after task teardown.
    idleWork        = task->work;
    followUpMessage = &followUpRequest;
    if (idleWork->playerTask != NULL && taskMessageDispatch(idleWork->playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        followUpEntry         = &_gAkropolisObservatorySceneFollowUpClips[idleWork->followUpIndex];
        allocationCheckOrClip = *followUpEntry;
        followUpAnimationId   = *followUpEntry;
        if (allocationCheckOrClip >= 0) {
            playbackWork = task->work;
            if (playbackWork->playerTask != NULL) {
                followUpRequest.source.sets           = gAkropolisObservatoryPlayerAnimationSets;
                followUpRequest.animationId           = followUpAnimationId;
                followUpMessage->blend                = ANIMATION_BLEND_INTERPOLATE;
                followUpMessage->blendFrames          = AKROPOLIS_OBSERVATORY_SCENE_FOLLOW_UP_BLEND_FRAMES;
                followUpMessage->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(playbackWork->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, followUpMessage, 0);
            }
        }
    }
}

/// Sets the screen-aligned square corners of a glow quad around its projected center.
static inline void _akropolisObservatorySetGlowScreenBounds(POLY_FT4* quad, const EffectCentreScratch* projection)
{
    quad->x0 = quad->x2 = projection->screenX - (u16)projection->screenExtent;
    quad->x1 = quad->x3 = projection->screenX + (u16)projection->screenExtent;
    quad->y0 = quad->y1 = projection->screenY - (u16)projection->screenExtent;
    quad->y2 = quad->y3 = projection->screenY + (u16)projection->screenExtent;
}

void akropolisObservatoryAmbientGlowTask(Task* task)
{
    enum {
        AKROPOLIS_OBSERVATORY_GLOW_DEPTH_BIAS                = 0x40,
        AKROPOLIS_OBSERVATORY_GLOW_MIN_DEPTH                 = 0x10,
        AKROPOLIS_OBSERVATORY_GLOW_HALF_EXTENT_DEPTH_PRODUCT = 0x5D00,
        AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_DEPTH_4BIT        = 0,
        AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_PAGE_X            = 704,
        AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_PAGE_Y            = 0,
        AKROPOLIS_OBSERVATORY_GLOW_CLUT_X                    = 0xE0,
        AKROPOLIS_OBSERVATORY_GLOW_CLUT_Y                    = 0x10F,
        AKROPOLIS_OBSERVATORY_GLOW_PALETTE_STRIDE            = 16,
        AKROPOLIS_OBSERVATORY_GLOW_PALETTE_COUNT             = 2,
        AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_TOP_V             = 0xA0,
        AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_SPAN              = 31,
    };

    void**               scratchCursor;
    u8*                  scratchTop;
    EffectCentreScratch* projection;
    POLY_FT4*            quad;
    GfxCoord*            coord;
    EffectWork*          effectWork;
    u16                  viewZ;

    coord      = task->extra.coordBody->coord;
    effectWork = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);

    // Narrow the composed view position for projection; the quad stays aligned to the screen.
    scratchCursor             = SCRATCH_STACK_CURSOR_SLOT;
    scratchTop                = *scratchCursor;
    projection                = (EffectCentreScratch*)(scratchTop - sizeof(EffectCentreScratch));
    projection->worldPoint.vx = (u16)coord->workm.t[0];
    projection->worldPoint.vy = (u16)coord->workm.t[1];
    viewZ                     = (u16)coord->workm.t[2];
    *scratchCursor            = projection;
    projection->worldPoint.vz = viewZ;

    {
        SVECTOR* viewPoint = &projection->worldPoint;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(viewPoint);
    }
    gte_rtps();
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        // Bias SZ3 / 4 towards the camera and cap the glow's growth at close range.
        gte_stszotz(&projection->depth);
        projection->depth -= AKROPOLIS_OBSERVATORY_GLOW_DEPTH_BIAS;
        if (projection->depth < AKROPOLIS_OBSERVATORY_GLOW_MIN_DEPTH) {
            projection->depth = AKROPOLIS_OBSERVATORY_GLOW_MIN_DEPTH;
        }
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, true);
        setShadeTex(quad, true);
        quad->tpage              = getTPage(AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD,
                                            AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_PAGE_X, AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_PAGE_Y);
        quad->clut               = getClut(AKROPOLIS_OBSERVATORY_GLOW_CLUT_X +
                                               (u32)(gDisplayState.animFrame & (AKROPOLIS_OBSERVATORY_GLOW_PALETTE_COUNT - 1)) * AKROPOLIS_OBSERVATORY_GLOW_PALETTE_STRIDE,
                                           AKROPOLIS_OBSERVATORY_GLOW_CLUT_Y);
        quad->u0                 = 0;
        quad->v0                 = AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_TOP_V;
        quad->u1                 = AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_SPAN;
        quad->v1                 = AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_TOP_V;
        quad->u2                 = 0;
        quad->v2                 = AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_TOP_V + AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_SPAN;
        quad->u3                 = AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_SPAN;
        quad->v3                 = AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_TOP_V + AKROPOLIS_OBSERVATORY_GLOW_TEXTURE_SPAN;
        projection->screenExtent = AKROPOLIS_OBSERVATORY_GLOW_HALF_EXTENT_DEPTH_PRODUCT / projection->depth;
        _akropolisObservatorySetGlowScreenBounds(quad, projection);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
    effectKillTask(effectWork, task);
}

void akropolisObservatoryLensFlareEmitterTask(Task* task)
{
    GfxCoord* coord;
    s32       viewMask;
    s32       anchorIndex;
    SVECTOR*  anchor;
    u16*      anchorViewMask;

    coord    = task->extra.coordBody->coord;
    viewMask = 1 << viewGetMappedIndex();
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        anchorIndex    = 0;
        anchor         = _gAkropolisObservatoryLensFlareAnchors;
        anchorViewMask = _gAkropolisObservatoryLensFlareViewMasks;
        do {
            if (*anchorViewMask & viewMask) {
                effectSpawn(EFFECT_AKROPOLIS_OBSERVATORY_LENS_FLARE, coord, 0, anchor);
            }
            anchor++;
            anchorIndex++;
            anchorViewMask++;
        } while (anchorIndex < ARRAY_SIZE(_gAkropolisObservatoryLensFlareAnchors));
    }
}
