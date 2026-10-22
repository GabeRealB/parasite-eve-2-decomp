#include "rooms/acropolis_observatory.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "acropolis_observatory_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag_ids.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "assets/rooms/akropolis/observatory/declarations.inc"

/// Work block of the observatory's one-time scripted player scene, owned by
/// the scene task through `Task::work`.
///
/// The block is allocated zeroed and only `playerTask` is ever stored, so
/// `followUpIndex` keeps selecting entry 0 of the follow-up clip table, the
/// entry that means "nothing to play".
typedef struct {
    Task* playerTask;    // Player task the scene's animation and release messages are sent to (NULL when the player slot is empty)
    u16   field_4;       // Never accessed; role unproven
    u16   followUpIndex; // Entry of the follow-up clip table to start once the player's current animation has ended
} _AcropolisObservatorySceneWork;
STATIC_ASSERT_SIZEOF(_AcropolisObservatorySceneWork, 8);

static void _acropolisObservatoryScriptedBattleTask(Task* task);

/// Clip-table values and scheduler priority of the scripted battle controller.
enum {
    ACROPOLIS_OBSERVATORY_SCENE_NO_FOLLOW_UP       = -1,
    ACROPOLIS_OBSERVATORY_SCENE_FOLLOW_UP_CLIP     = 1,
    ACROPOLIS_OBSERVATORY_SCRIPTED_BATTLE_PRIORITY = 192,
};

/// Player follow-up clip IDs indexed by the scene work's `followUpIndex`.
///
/// A negative signed halfword suppresses playback; entry 1 selects clip 1 of
/// `gAcropolisObservatoryPlayerAnimationSets`. The zeroed, unwritten index
/// selects entry 0 in this build. The table is borrowed while this overlay is loaded.
static s16 _gAcropolisObservatorySceneFollowUpClips[2] = {
    ACROPOLIS_OBSERVATORY_SCENE_NO_FOLLOW_UP,
    ACROPOLIS_OBSERVATORY_SCENE_FOLLOW_UP_CLIP,
};

TaskDesc gAcropolisObservatoryScriptedBattleTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, ACROPOLIS_OBSERVATORY_SCRIPTED_BATTLE_PRIORITY } },
    .callback = _acropolisObservatoryScriptedBattleTask,
    .data     = { .value = 0 },
};

/// Lens-flare table extent and enabled mapped-view bits (view IDs are 1-based).
enum {
    ACROPOLIS_OBSERVATORY_LENS_FLARE_ANCHOR_COUNT = 8,
    ACROPOLIS_OBSERVATORY_LENS_FLARE_VIEW_2       = 1 << 2,
    ACROPOLIS_OBSERVATORY_LENS_FLARE_VIEW_3       = 1 << 3,
    ACROPOLIS_OBSERVATORY_LENS_FLARE_VIEW_4       = 1 << 4,
    ACROPOLIS_OBSERVATORY_LENS_FLARE_VIEW_5       = 1 << 5,
    ACROPOLIS_OBSERVATORY_LENS_FLARE_VIEW_6       = 1 << 6,
    ACROPOLIS_OBSERVATORY_LENS_FLARE_VIEW_7       = 1 << 7,
    ACROPOLIS_OBSERVATORY_LENS_FLARE_VIEW_8       = 1 << 8,
};

#include "assets/rooms/akropolis/observatory/markers.inc"
#include "assets/rooms/akropolis/observatory/view_masks.inc"
#include "assets/rooms/akropolis/observatory/header.inc"
#include "assets/rooms/akropolis/observatory/geometry.inc"
#include "assets/rooms/akropolis/observatory/view_boundaries.inc"
#include "assets/rooms/akropolis/observatory/action_triggers.inc"
#include "assets/rooms/akropolis/observatory/placements.inc"
#include "assets/rooms/akropolis/observatory/lights.inc"
#define ROOM_PART _gAcropolisObservatoryView1SpriteBatches
#include "assets/rooms/akropolis/observatory/sprites.inc"
/// Retained eight-byte initializer with no established consumer; role unproven.
u16 D_acropolis_observatory_80182124[4] = {
    256,
    0,
    0,
    0,
};
#define ROOM_PART _gAcropolisObservatoryView4SpriteSources
#include "assets/rooms/akropolis/observatory/sprites.inc"
#include "assets/rooms/akropolis/observatory/cameras.inc"
#include "assets/rooms/akropolis/observatory/vibrations.inc"
#include "assets/rooms/akropolis/observatory/surfaces.inc"

/// All defined arrival headings face a quarter turn from +Z.
enum { ACROPOLIS_OBSERVATORY_WARP_ARRIVAL_YAW = 1024 };

/// Packed departure/arrival slots and the room-specific action sent by battle gates.
enum {
    ACROPOLIS_OBSERVATORY_FORKED_ROAD_WARP_SLOTS       = (2 << 4) | 2,
    ACROPOLIS_OBSERVATORY_PROMENADE_WARP_SLOTS         = (1 << 4) | 4,
    ACROPOLIS_OBSERVATORY_ACTION_START_SCRIPTED_BATTLE = 1,
};

/// GPU page and palette encodings for the view sprites' 8-bit texture atlas.
///
/// Page names give the VRAM X origin; every page starts at Y=0 and uses average
/// blending for semitransparent sprites. Palette names give the VRAM row at X=0.
enum {
    ACROPOLIS_OBSERVATORY_SPRITE_TEXTURE_DEPTH_8BIT = 1,
    ACROPOLIS_OBSERVATORY_SPRITE_TPAGE_X832         = getTPage(ACROPOLIS_OBSERVATORY_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 832, 0),
    ACROPOLIS_OBSERVATORY_SPRITE_TPAGE_X896         = getTPage(ACROPOLIS_OBSERVATORY_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0),
    ACROPOLIS_OBSERVATORY_SPRITE_TPAGE_X960         = getTPage(ACROPOLIS_OBSERVATORY_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0),
    ACROPOLIS_OBSERVATORY_SPRITE_CLUT_Y255          = getClut(0, 255),
    ACROPOLIS_OBSERVATORY_SPRITE_CLUT_Y256          = getClut(0, 256),
    ACROPOLIS_OBSERVATORY_SPRITE_CLUT_Y257          = getClut(0, 257),
    ACROPOLIS_OBSERVATORY_SPRITE_CLUT_Y258          = getClut(0, 258),
};

/// Binary-lane segment selectors, eligible script-frame durations and loop count.
enum {
    ACROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_FIRST_HOLD         = 0,
    ACROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_SECOND_HOLD        = 1,
    ACROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_FIRST_HOLD_FRAMES  = 2,
    ACROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_SECOND_HOLD_FRAMES = 1,
    ACROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_GAP_FRAMES         = 2,
    ACROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_CYCLE_COUNT        = 20,
};

/// Type-1 sound requests: cue-2 entries in the currently loaded bank.
enum {
    ACROPOLIS_OBSERVATORY_FOOTSTEP_WALK_BASE = 0x10000011,
    ACROPOLIS_OBSERVATORY_FOOTSTEP_RUN_BASE  = 0x10000013,
};

/// Runs the observatory's scripted player pose and enemy-entrance battle handoff.
///
/// Starts at state 0; states 0..6 install scene clip 1, defer, wait for the first
/// enemy wave, restore the equipped weapon's animation bank and field room,
/// then send `GAME_ACTOR_MESSAGE_END_SCRIPTED` and retire. Initialization waits
/// while the attachment wheel or a display transition is active. Requires the
/// player in `GAME_TASK_SLOT_PLAYER` and loaded scene/weapon animation data.
///
/// Owns eight bytes at `Task::work`; later states borrow the captured player.
/// An idle player may receive a follow-up clip with a ten-frame blend, but the
/// zeroed follow-up index selects a negative entry in this build. Retains the
/// original continuation after allocation failure and task teardown.
static void _acropolisObservatoryScriptedBattleTask(Task* task)
{
    enum {
        ACROPOLIS_OBSERVATORY_SCENE_INITIALIZE                 = 0,
        ACROPOLIS_OBSERVATORY_SCENE_DEFER_FIRST                = 1,
        ACROPOLIS_OBSERVATORY_SCENE_DEFER_SECOND               = 2,
        ACROPOLIS_OBSERVATORY_SCENE_WAIT_ENTRANCE              = 3,
        ACROPOLIS_OBSERVATORY_SCENE_DEFER_BATTLE               = 4,
        ACROPOLIS_OBSERVATORY_SCENE_RESTORE_PLAYER             = 5,
        ACROPOLIS_OBSERVATORY_SCENE_RELEASE_PLAYER             = 6,
        ACROPOLIS_OBSERVATORY_SCENE_CLIP                       = 1,
        ACROPOLIS_OBSERVATORY_SCENE_HOLD_ENTRANCE              = 0,
        ACROPOLIS_OBSERVATORY_SCENE_FIRST_WAVE                 = 2,
        ACROPOLIS_OBSERVATORY_SCENE_PRIMARY_CHARACTER          = 1,
        ACROPOLIS_OBSERVATORY_SCENE_PRIMARY_WEAPON_BANK_BASE   = 1,
        ACROPOLIS_OBSERVATORY_SCENE_ALTERNATE_WEAPON_BANK_BASE = 34,
        ACROPOLIS_OBSERVATORY_SCENE_FIELD_ROOM                 = 1,
        ACROPOLIS_OBSERVATORY_SCENE_FOLLOW_UP_BLEND_FRAMES     = 10
    };
    AnimationPlayRequest            playbackRequest;
    AnimationPlayRequest            followUpRequest;
    AnimationPlayRequest*           followUpMessage;
    _AcropolisObservatorySceneWork* sceneWork;
    _AcropolisObservatorySceneWork* idleWork;
    _AcropolisObservatorySceneWork* playbackWork;
    _AcropolisObservatorySceneWork* allocatedWork;
    s16*                            followUpEntry;
    u16                             followUpAnimationId;
    s32                             allocationCheckOrClip;
    s32                             weaponIndex;
    s32                             animationBankIndex;

    sceneWork = task->work;
    switch (task->state) {
        case ACROPOLIS_OBSERVATORY_SCENE_INITIALIZE:
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
                playbackRequest.source.sets          = gAcropolisObservatoryPlayerAnimationSets;
                playbackRequest.animationId          = ACROPOLIS_OBSERVATORY_SCENE_CLIP;
                playbackRequest.blend                = ANIMATION_BLEND_RESET;
                playbackRequest.blendFrames          = 0;
                playbackRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(sceneWork->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &playbackRequest, 0);
            }
            gSceneCombatState.actor03700Wave = ACROPOLIS_OBSERVATORY_SCENE_HOLD_ENTRANCE;
            /* fallthrough */
        case ACROPOLIS_OBSERVATORY_SCENE_DEFER_FIRST:
        case ACROPOLIS_OBSERVATORY_SCENE_DEFER_SECOND:
        case ACROPOLIS_OBSERVATORY_SCENE_DEFER_BATTLE:
            task->state = task->state + 1;
            break;
        case ACROPOLIS_OBSERVATORY_SCENE_WAIT_ENTRANCE:
            if (gSceneCombatState.actor03700Wave == ACROPOLIS_OBSERVATORY_SCENE_FIRST_WAVE) {
                sceneEngageBattle(1);
                task->state = task->state + 1;
            }
            break;
        case ACROPOLIS_OBSERVATORY_SCENE_RESTORE_PLAYER:
            // Restore the equipped weapon bank before returning to the field room.
            weaponIndex                          = gPlayerStatus.weapon;
            animationBankIndex                   = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACROPOLIS_OBSERVATORY_SCENE_PRIMARY_CHARACTER) ? weaponIndex + ACROPOLIS_OBSERVATORY_SCENE_PRIMARY_WEAPON_BANK_BASE : weaponIndex + ACROPOLIS_OBSERVATORY_SCENE_ALTERNATE_WEAPON_BANK_BASE;
            playbackRequest.source.index         = animationBankIndex;
            playbackRequest.animationId          = ACROPOLIS_OBSERVATORY_SCENE_CLIP;
            playbackRequest.blend                = ANIMATION_BLEND_RESET;
            playbackRequest.blendFrames          = 0;
            playbackRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(sceneWork->playerTask, ANIMATION_MESSAGE_PLAY, &playbackRequest, 0);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACROPOLIS_OBSERVATORY_SCENE_FIELD_ROOM;
            gGameSession->location.roomState                            = ACROPOLIS_OBSERVATORY_SCENE_FIELD_ROOM;
            gGameSession->roomObjsDirty                                 = 1;
            gGameSession->viewDirty                                     = 1;
            task->state                                                 = task->state + 1;
            break;
        case ACROPOLIS_OBSERVATORY_SCENE_RELEASE_PLAYER:
            taskMessageDispatch(sceneWork->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            taskKill(task);
            break;
    }

    // The original also runs the idle tail after task teardown.
    idleWork        = task->work;
    followUpMessage = &followUpRequest;
    if (idleWork->playerTask != NULL && taskMessageDispatch(idleWork->playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        followUpEntry         = &_gAcropolisObservatorySceneFollowUpClips[idleWork->followUpIndex];
        allocationCheckOrClip = *followUpEntry;
        followUpAnimationId   = *followUpEntry;
        if (allocationCheckOrClip >= 0) {
            playbackWork = task->work;
            if (playbackWork->playerTask != NULL) {
                followUpRequest.source.sets           = gAcropolisObservatoryPlayerAnimationSets;
                followUpRequest.animationId           = followUpAnimationId;
                followUpMessage->blend                = ANIMATION_BLEND_INTERPOLATE;
                followUpMessage->blendFrames          = ACROPOLIS_OBSERVATORY_SCENE_FOLLOW_UP_BLEND_FRAMES;
                followUpMessage->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(playbackWork->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, followUpMessage, 0);
            }
        }
    }
}

/// Sets the screen-aligned square corners of a glow quad around its projected centre.
///
/// Only `projection`'s `screenX`, `screenY` and `screenExtent` need initialization.
/// The centre fields encode signed pixel coordinates; the integer pixel
/// half-extent is narrowed to unsigned 16 bits before computing the edges.
/// Corners 0/1 form the top row and 2/3 the bottom row; 0/2 form the left
/// column and 1/3 the right column. Screen X increases rightward, Y downward.
///
/// Borrows a live read-only projection and a disjoint writable `POLY_FT4`.
/// Writes only the eight XY halfwords, retaining the low 16 bits as signed
/// screen coordinates without clipping. Neither pointer is retained.
static inline void _acropolisObservatorySetGlowScreenBounds(POLY_FT4* quad, const EffectCentreScratch* projection)
{
    quad->x0 = quad->x2 = projection->screenX - (u16)projection->screenExtent;
    quad->x1 = quad->x3 = projection->screenX + (u16)projection->screenExtent;
    quad->y0 = quad->y1 = projection->screenY - (u16)projection->screenExtent;
    quad->y2 = quad->y3 = projection->screenY + (u16)projection->screenExtent;
}

void acropolisObservatoryAmbientGlowTask(Task* task)
{
    enum {
        ACROPOLIS_OBSERVATORY_GLOW_DEPTH_BIAS                = 0x40,
        ACROPOLIS_OBSERVATORY_GLOW_MIN_DEPTH                 = 0x10,
        ACROPOLIS_OBSERVATORY_GLOW_HALF_EXTENT_DEPTH_PRODUCT = 0x5D00,
        ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_DEPTH_4BIT        = 0,
        ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_PAGE_X            = 704,
        ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_PAGE_Y            = 0,
        ACROPOLIS_OBSERVATORY_GLOW_CLUT_X                    = 0xE0,
        ACROPOLIS_OBSERVATORY_GLOW_CLUT_Y                    = 0x10F,
        ACROPOLIS_OBSERVATORY_GLOW_PALETTE_STRIDE            = 16,
        ACROPOLIS_OBSERVATORY_GLOW_PALETTE_COUNT             = 2,
        ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_TOP_V             = 0xA0,
        ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_SPAN              = 31,
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
        projection->depth -= ACROPOLIS_OBSERVATORY_GLOW_DEPTH_BIAS;
        if (projection->depth < ACROPOLIS_OBSERVATORY_GLOW_MIN_DEPTH) {
            projection->depth = ACROPOLIS_OBSERVATORY_GLOW_MIN_DEPTH;
        }
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, true);
        setShadeTex(quad, true);
        quad->tpage              = getTPage(ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD,
                                            ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_PAGE_X, ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_PAGE_Y);
        quad->clut               = getClut(ACROPOLIS_OBSERVATORY_GLOW_CLUT_X +
                                               (u32)(gDisplayState.animFrame & (ACROPOLIS_OBSERVATORY_GLOW_PALETTE_COUNT - 1)) * ACROPOLIS_OBSERVATORY_GLOW_PALETTE_STRIDE,
                                           ACROPOLIS_OBSERVATORY_GLOW_CLUT_Y);
        quad->u0                 = 0;
        quad->v0                 = ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_TOP_V;
        quad->u1                 = ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_SPAN;
        quad->v1                 = ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_TOP_V;
        quad->u2                 = 0;
        quad->v2                 = ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_TOP_V + ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_SPAN;
        quad->u3                 = ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_SPAN;
        quad->v3                 = ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_TOP_V + ACROPOLIS_OBSERVATORY_GLOW_TEXTURE_SPAN;
        projection->screenExtent = ACROPOLIS_OBSERVATORY_GLOW_HALF_EXTENT_DEPTH_PRODUCT / projection->depth;
        _acropolisObservatorySetGlowScreenBounds(quad, projection);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
    effectKillTask(effectWork, task);
}

void acropolisObservatoryLensFlareEmitterTask(Task* task)
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
        anchor         = _gAcropolisObservatoryLensFlareAnchors;
        anchorViewMask = _gAcropolisObservatoryLensFlareViewMasks;
        do {
            if (*anchorViewMask & viewMask) {
                effectSpawn(EFFECT_ACROPOLIS_OBSERVATORY_LENS_FLARE, coord, 0, anchor);
            }
            anchor++;
            anchorIndex++;
            anchorViewMask++;
        } while (anchorIndex < ARRAY_SIZE(_gAcropolisObservatoryLensFlareAnchors));
    }
}
