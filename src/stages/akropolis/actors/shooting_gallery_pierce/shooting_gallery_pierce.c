#include "shooting_gallery_pierce_private.h"

#include "types.h"

#include "stages/akropolis/actors/shooting_gallery_pierce.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "stages/akropolis/rooms/mist_shooting_gallery.h"

#include "assets/engine/animation/player_schema.h"
#include "assets/rooms/mist/shooting_gallery/pierce_schema.h"

TaskDesc gShootingGalleryPierceGalleryDecisionTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, shootingGalleryPierceGalleryExitDecisionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, shootingGalleryPierceGalleryAbortDecisionTask, { .value = 0 } },
};

/// Puts the player in aim idle as the gallery course-level prompt begins.
static AnimationPlayRequest _gShootingGalleryPierceGalleryLevelPromptPlayerRequest = {
    .source               = { .index = 1 },
    .animationId          = PLAYER_ANIMATION_AIM_IDLE,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained bank-index-1 reset request for clip 0, with no playback consumer.
static const AnimationPlayRequest _gShootingGalleryPierceRetainedBank1Clip0ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = 0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Returns Pierce to his gallery idle twenty-two updates after the course-level reply's clip.
static AnimationPlayRequest _gShootingGalleryPierceGalleryLevelReplyIdleRequest = {
    .source               = { .index = 1 },
    .animationId          = PIERCE_ANIMATION_GALLERY_IDLE,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained reset requests for Pierce's `gallery_ask`, `gallery_talk`, `gallery_tease` and `gallery_react`, between his
/// `gallery_idle` and `level_prompt` requests; nothing plays them.
static const AnimationPlayRequest _gShootingGalleryPierceRetainedPierceGalleryResetRequests[4] SECTION(".data") = {
    { .source = { .index = 1 }, .animationId = PIERCE_ANIMATION_GALLERY_ASK, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 1 }, .animationId = PIERCE_ANIMATION_GALLERY_TALK, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 1 }, .animationId = PIERCE_ANIMATION_GALLERY_TEASE, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 1 }, .animationId = PIERCE_ANIMATION_GALLERY_REACT, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Starts Pierce's level prompt as the gallery course-level prompt begins.
static const AnimationPlayRequest _gShootingGalleryPierceGalleryLevelPromptPierceRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = PIERCE_ANIMATION_LEVEL_PROMPT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Starts Pierce's level reply when the course-level reply countdown reaches zero.
static AnimationPlayRequest _gShootingGalleryPierceGalleryLevelReplyRequest = {
    .source               = { .index = 1 },
    .animationId          = PIERCE_ANIMATION_LEVEL_REPLY,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

s32 gShootingGalleryPierceGalleryTrainingActive = 0;

s32 gShootingGalleryPierceGalleryExitInputBlocked = 0;

/// Courses staged since this gallery overlay was loaded.
static s32 _gShootingGalleryPierceGalleryCoursesStagedCount = 0;

s32 gShootingGalleryPierceGalleryExitInputCooldownTicks = 0;

#include "assets/rooms/mist/shooting_gallery/pierce_clips_1.inc"

/// Advances the caption-reply animation timer without going below -1000.
static inline void _shootingGalleryPierceUpdateLevelReplyAnimations(Task* task)
{
    enum { LAST_DECREMENT        = -999,
           SECOND_ANIMATION_TICK = -22 };
    if (task->killCountdown >= LAST_DECREMENT) {
        task->killCountdown--;
    }
    if (task->killCountdown == 0) {
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(PIERCE_PLACE_INDEX), ACTOR_MESSAGE_PLAY_ANIMATION, &_gShootingGalleryPierceGalleryLevelReplyRequest, 0);
    }
    if (task->killCountdown == SECOND_ANIMATION_TICK) {
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(PIERCE_PLACE_INDEX), ACTOR_MESSAGE_PLAY_ANIMATION, &_gShootingGalleryPierceGalleryLevelReplyIdleRequest, 0);
    }
}

void shootingGalleryPierceGalleryTrainingMenuTask(Task* task)
{
    enum {
        START               = 0,
        WAIT_INTRO          = 1,
        RESOLVE_REPLY       = 2,
        DECLINE             = 4,
        ACCEPT              = 5,
        WAIT_ACCEPT         = 6,
        MUSIC_CAPTION       = 10,
        OPEN_MUSIC          = 11,
        AFTER_MUSIC         = 12,
        WEAPON_CAPTION      = 20,
        OPEN_WEAPON         = 21,
        AFTER_WEAPON        = 22,
        LEVEL_CAPTION       = 30,
        WAIT_LEVEL_REPLY    = 31,
        SAVE_LEVEL          = 32,
        CONFIRM_LEVEL       = 40,
        WAIT_CONFIRM        = 41,
        AFTER_CONFIRM       = 42,
        STAGE_TRAINING      = 50,
        WAIT_TRAINING       = 51,
        SPAWN_TRAINING      = 52,
        ANIMATION_DELAY     = 27,
        FIRST_LOADOUT_LEVEL = 3,
        LEVEL_CAPTION_BASE  = 11,
        MUSIC_SLOT          = 9,
        WEAPON_SLOT         = 10,
        REPEAT_PROMPT_SLOT  = 28,
        FIRST_PROMPT_SLOT   = 21,
        REPEAT_DECLINE_SLOT = 8,
        FIRST_DECLINE_SLOT  = 25,
        REPEAT_ACCEPT_SLOT  = 7,
        FIRST_ACCEPT_SLOT   = 24,
    };
    s16 captionSlot;

    switch (task->state) {
        case START:
            if (gShootingGalleryPierceGalleryTrainingActive != 0) {
                taskKill(task);
                return;
            }
            if (task->spawnArg1.value == 0) {
                evsStartScript(gShootingGalleryPierceGalleryPrepareTrainingMenuScript, EVENT_SCRIPT_HUD_KEEP);
            } else {
                captionSlot = REPEAT_PROMPT_SLOT;
                if (_gShootingGalleryPierceGalleryCoursesStagedCount == 0) {
                    captionSlot = FIRST_PROMPT_SLOT;
                }
                capStartSequenceSlot(captionSlot, 0, 0);
                evsStartScript(gShootingGalleryPierceGalleryOfferTrainingMenuScript, EVENT_SCRIPT_HUD_KEEP);
            }
            task->state++;
            break;
        case RESOLVE_REPLY:
            gGameSession->eventState = 1;
            if (task->spawnArg1.value != 0) {
                if (capGetVariantKey() != 0) {
                    task->state = ACCEPT;
                } else {
                    task->state = DECLINE;
                }
            } else {
                task->state = MUSIC_CAPTION;
            }
            break;
        case DECLINE:
            captionSlot = REPEAT_DECLINE_SLOT;
            if (_gShootingGalleryPierceGalleryCoursesStagedCount == 0) {
                captionSlot = FIRST_DECLINE_SLOT;
            }
            capStartSequenceSlot(captionSlot, 0, 0);
            evsStartScript(gShootingGalleryPierceGalleryDeclineTrainingScript, EVENT_SCRIPT_HUD_KEEP);
            taskKill(task);
            break;
        case ACCEPT:
            captionSlot = REPEAT_ACCEPT_SLOT;
            if (_gShootingGalleryPierceGalleryCoursesStagedCount == 0) {
                captionSlot = FIRST_ACCEPT_SLOT;
            }
            capStartSequenceSlot(captionSlot, 0, 0);
            evsStartScript(gShootingGalleryPierceGalleryWaitTrainingAcceptanceScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case WAIT_ACCEPT:
            if (gGameSession->eventState == 0) {
                task->state = MUSIC_CAPTION;
            }
            break;
        case MUSIC_CAPTION:
            capStartSequenceSlot(MUSIC_SLOT, 0, 0);
            task->state++;
            break;
        case OPEN_MUSIC:
            if (capIsBusy() == 0) {
                mistShootingGalleryOpenJukebox(0);
                task->state++;
            }
            break;
        case AFTER_MUSIC:
            task->state = WEAPON_CAPTION;
            break;
        case WEAPON_CAPTION:
            capStartSequenceSlot(WEAPON_SLOT, 0, 0);
            task->state++;
            break;
        case OPEN_WEAPON:
            if (capIsBusy() == 0) {
                mistShootingGalleryOpenWeaponMenu(0);
                task->state++;
            }
            break;
        case AFTER_WEAPON:
            task->state = LEVEL_CAPTION;
            break;
        case LEVEL_CAPTION:
            playerActorWriteWeaponAnimationBankIndex(&_gShootingGalleryPierceGalleryLevelPromptPlayerRequest.source.index);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &_gShootingGalleryPierceGalleryLevelPromptPlayerRequest, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(PIERCE_PLACE_INDEX), ACTOR_MESSAGE_PLAY_ANIMATION, &_gShootingGalleryPierceGalleryLevelPromptPierceRequest, 0);
            task->killCountdown = ANIMATION_DELAY;
            capStartSequenceSlot(LEVEL_CAPTION_BASE, 0, 0);
            task->state++;
            break;
        case WAIT_LEVEL_REPLY:
            _shootingGalleryPierceUpdateLevelReplyAnimations(task);
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SAVE_LEVEL:
            gShootingGalleryPierceGallerySelectedCourseLevel = capGetVariantKey();
            mistShootingGalleryPrepareTrainingLoadout(gShootingGalleryPierceGallerySelectedCourseLevel);
            task->state = CONFIRM_LEVEL;
            break;
        case CONFIRM_LEVEL:
            capStartSequenceSlot(capGetVariantKey() + LEVEL_CAPTION_BASE, 0, 0);
            evsStartScript(gShootingGalleryPierceGalleryConfirmCourseScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case AFTER_CONFIRM:
            task->state = STAGE_TRAINING;
            break;
        case STAGE_TRAINING:
            _gShootingGalleryPierceGalleryCoursesStagedCount++;
            gAttachmentState.flags |= ATTACHMENT_FLAG_SWAP_LOCK;
            if (gShootingGalleryPierceGallerySelectedCourseLevel < FIRST_LOADOUT_LEVEL) {
                evsStartScript(gShootingGalleryPierceGalleryStartFixedViewTrainingScript, EVENT_SCRIPT_HUD_KEEP);
            } else {
                evsStartScript(gShootingGalleryPierceGalleryStartFreeMovementTrainingScript, EVENT_SCRIPT_HUD_KEEP);
            }
            gShootingGalleryPierceGalleryTrainingActive = 1;
            task->state++;
            break;
        case WAIT_INTRO:
        case WAIT_CONFIRM:
        case WAIT_TRAINING:
            if (gGameSession->eventState == 0) {
                task->state++;
            }
            break;
        case SPAWN_TRAINING:
            gGameSession->flowFlags &= (0xFF ^ GAME_SESSION_FLOW_REEQUIP_WEAPON);
            taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_COURSE_CONTROLLER, gShootingGalleryPierceGallerySelectedCourseLevel - 1, 0);
            gShootingGalleryPierceGalleryExitInputBlocked = 1;
            taskKill(task);
            break;
    }
}
