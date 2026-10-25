#include "shooting_gallery_pierce_private.h"

#include "types.h"

#include "stages/akropolis/actors/shooting_gallery_pierce.h"

#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "stages/akropolis/rooms/mist_shooting_gallery.h"

TaskDesc gShootingGalleryPierceGalleryDecisionTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, shootingGalleryPierceGalleryExitDecisionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, shootingGalleryPierceGalleryAbortDecisionTask, { .value = 0 } },
};

enum { SHOOTING_GALLERY_PIERCE_LEVEL_PROMPT_PLAYER_CLIP = 9 };

/// Resets player clip 9 as the gallery course-level prompt begins.
///
/// The menu rewrites source.index to the equipped weapon bank before each
/// synchronous dispatch. Drops player grid participation; reset ignores the
/// zero blend duration. Keep the live player and its weapon clips loaded.
static AnimationPlayRequest _gShootingGalleryPierceGalleryLevelPromptPlayerAnimation9Request = {
    .source               = { .index = 1 },
    .animationId          = SHOOTING_GALLERY_PIERCE_LEVEL_PROMPT_PLAYER_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained bank-index-1 reset request for clip 0, with no playback consumer.
///
/// Its recipient and intended use are unproven. The collision selector is
/// disabled and the reset choice ignores the zero blend duration.
static const AnimationPlayRequest _gShootingGalleryPierceRetainedBank1Clip0ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = 0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum {
    SHOOTING_GALLERY_PIERCE_LEVEL_REPLY_FIRST_CLIP  = 24,
    SHOOTING_GALLERY_PIERCE_LEVEL_REPLY_SECOND_CLIP = 18,
};

/// Resets Pierce to clip 18 twenty-two updates after the course-level reply's first clip.
///
/// Sent synchronously at reply countdown -22; the live actor and clip bank
/// must remain loaded. Pierce ignores the stored bank and collision selectors.
static AnimationPlayRequest _gShootingGalleryPierceGalleryLevelReplyAnimation18Request = {
    .source               = { .index = 1 },
    .animationId          = SHOOTING_GALLERY_PIERCE_LEVEL_REPLY_SECOND_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Four retained bank-index-1 reset requests for clips 19..22 in that order.
///
/// No consumer establishes a recipient or an indexing contract. Each complete
/// five-word request disables collision and ignores its zero blend duration.
static const AnimationPlayRequest _gShootingGalleryPierceRetainedBank1Clip19To22ResetRequests[4] SECTION(".data") = {
    { .source = { .index = 1 }, .animationId = 19, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 1 }, .animationId = 20, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 1 }, .animationId = 21, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 1 }, .animationId = 22, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
};

enum { SHOOTING_GALLERY_PIERCE_LEVEL_PROMPT_PIERCE_CLIP = 23 };

/// Resets Pierce to clip 23 as the gallery course-level prompt begins.
///
/// Placed actor 1 borrows this request during synchronous dispatch and ignores
/// the stored bank and collision selectors. Keep Pierce and his clips loaded.
static const AnimationPlayRequest _gShootingGalleryPierceGalleryLevelPromptPierceAnimation23Request SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = SHOOTING_GALLERY_PIERCE_LEVEL_PROMPT_PIERCE_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets Pierce to clip 24 when the course-level reply countdown reaches zero.
///
/// The borrowed request is consumed synchronously; Pierce ignores its stored
/// bank and collision selectors. The live actor and clip bank must stay loaded.
static AnimationPlayRequest _gShootingGalleryPierceGalleryLevelReplyAnimation24Request = {
    .source               = { .index = 1 },
    .animationId          = SHOOTING_GALLERY_PIERCE_LEVEL_REPLY_FIRST_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

s32 gShootingGalleryPierceGalleryTrainingActive = 0;

s32 gShootingGalleryPierceGalleryExitInputBlocked = 0;

/// Courses staged since this gallery overlay was loaded.
///
/// Incremented when course startup is staged, before its script completes.
/// Zero selects the first-course offer, acceptance and decline captions;
/// any nonzero value selects their repeat variants. This remains a signed
/// word counter, rather than a completion flag or a persistent save tally.
static s32 _gShootingGalleryPierceGalleryCoursesStagedCount = 0;

s32 gShootingGalleryPierceGalleryExitInputCooldownTicks = 0;

#include "assets/rooms/mist/shooting_gallery/pierce_clips_1.inc"

/// Advances the caption-reply animation timer without going below -1000.
///
/// killCountdown counts callback updates, beginning at 27 in the caller. The
/// comparisons use its value after decrementing. Requires the loaded gallery
/// actor at placement 1; sends borrowed static requests at 0 and -22. A value
/// already below -1000 stays unchanged. Caption completion remains with the caller.
static inline void _shootingGalleryPierceUpdateLevelReplyAnimations(Task* task)
{
    enum { SHOOTING_GALLERY_PIERCE_LEVEL_REPLY_LAST_DECREMENT        = -999,
           SHOOTING_GALLERY_PIERCE_LEVEL_REPLY_SECOND_ANIMATION_TICK = -22 };
    if (task->killCountdown >= SHOOTING_GALLERY_PIERCE_LEVEL_REPLY_LAST_DECREMENT) {
        task->killCountdown--;
    }
    if (task->killCountdown == 0) {
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(1), ACTOR_MESSAGE_PLAY_ANIMATION, &_gShootingGalleryPierceGalleryLevelReplyAnimation24Request, 0);
    }
    if (task->killCountdown == SHOOTING_GALLERY_PIERCE_LEVEL_REPLY_SECOND_ANIMATION_TICK) {
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(1), ACTOR_MESSAGE_PLAY_ANIMATION, &_gShootingGalleryPierceGalleryLevelReplyAnimation18Request, 0);
    }
}

void shootingGalleryPierceGalleryTrainingMenuTask(Task* task)
{
    enum {
        SHOOTING_GALLERY_PIERCE_MENU_START               = 0,
        SHOOTING_GALLERY_PIERCE_MENU_WAIT_INTRO          = 1,
        SHOOTING_GALLERY_PIERCE_MENU_RESOLVE_REPLY       = 2,
        SHOOTING_GALLERY_PIERCE_MENU_DECLINE             = 4,
        SHOOTING_GALLERY_PIERCE_MENU_ACCEPT              = 5,
        SHOOTING_GALLERY_PIERCE_MENU_WAIT_ACCEPT         = 6,
        SHOOTING_GALLERY_PIERCE_MENU_MUSIC_CAPTION       = 10,
        SHOOTING_GALLERY_PIERCE_MENU_OPEN_MUSIC          = 11,
        SHOOTING_GALLERY_PIERCE_MENU_AFTER_MUSIC         = 12,
        SHOOTING_GALLERY_PIERCE_MENU_WEAPON_CAPTION      = 20,
        SHOOTING_GALLERY_PIERCE_MENU_OPEN_WEAPON         = 21,
        SHOOTING_GALLERY_PIERCE_MENU_AFTER_WEAPON        = 22,
        SHOOTING_GALLERY_PIERCE_MENU_LEVEL_CAPTION       = 30,
        SHOOTING_GALLERY_PIERCE_MENU_WAIT_LEVEL_REPLY    = 31,
        SHOOTING_GALLERY_PIERCE_MENU_SAVE_LEVEL          = 32,
        SHOOTING_GALLERY_PIERCE_MENU_CONFIRM_LEVEL       = 40,
        SHOOTING_GALLERY_PIERCE_MENU_WAIT_CONFIRM        = 41,
        SHOOTING_GALLERY_PIERCE_MENU_AFTER_CONFIRM       = 42,
        SHOOTING_GALLERY_PIERCE_MENU_STAGE_TRAINING      = 50,
        SHOOTING_GALLERY_PIERCE_MENU_WAIT_TRAINING       = 51,
        SHOOTING_GALLERY_PIERCE_MENU_SPAWN_TRAINING      = 52,
        SHOOTING_GALLERY_PIERCE_MENU_ANIMATION_DELAY     = 27,
        SHOOTING_GALLERY_PIERCE_MENU_FIRST_LOADOUT_LEVEL = 3,
        SHOOTING_GALLERY_PIERCE_MENU_LEVEL_CAPTION_BASE  = 11,
        SHOOTING_GALLERY_PIERCE_MENU_MUSIC_SLOT          = 9,
        SHOOTING_GALLERY_PIERCE_MENU_WEAPON_SLOT         = 10,
        SHOOTING_GALLERY_PIERCE_MENU_REPEAT_PROMPT_SLOT  = 28,
        SHOOTING_GALLERY_PIERCE_MENU_FIRST_PROMPT_SLOT   = 21,
        SHOOTING_GALLERY_PIERCE_MENU_REPEAT_DECLINE_SLOT = 8,
        SHOOTING_GALLERY_PIERCE_MENU_FIRST_DECLINE_SLOT  = 25,
        SHOOTING_GALLERY_PIERCE_MENU_REPEAT_ACCEPT_SLOT  = 7,
        SHOOTING_GALLERY_PIERCE_MENU_FIRST_ACCEPT_SLOT   = 24,
    };
    s16 captionSlot;

    switch (task->state) {
        case SHOOTING_GALLERY_PIERCE_MENU_START:
            if (gShootingGalleryPierceGalleryTrainingActive != 0) {
                taskKill(task);
                return;
            }
            if (task->spawnArg1.value == 0) {
                evsStartScript(gShootingGalleryPierceGalleryPrepareTrainingMenuScript, EVENT_SCRIPT_HUD_KEEP);
            } else {
                captionSlot = SHOOTING_GALLERY_PIERCE_MENU_REPEAT_PROMPT_SLOT;
                if (_gShootingGalleryPierceGalleryCoursesStagedCount == 0) {
                    captionSlot = SHOOTING_GALLERY_PIERCE_MENU_FIRST_PROMPT_SLOT;
                }
                capStartSequenceSlot(captionSlot, 0, 0);
                evsStartScript(gShootingGalleryPierceGalleryOfferTrainingMenuScript, EVENT_SCRIPT_HUD_KEEP);
            }
            task->state++;
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_RESOLVE_REPLY:
            gGameSession->eventState = 1;
            if (task->spawnArg1.value != 0) {
                if (capGetVariantKey() != 0) {
                    task->state = SHOOTING_GALLERY_PIERCE_MENU_ACCEPT;
                } else {
                    task->state = SHOOTING_GALLERY_PIERCE_MENU_DECLINE;
                }
            } else {
                task->state = SHOOTING_GALLERY_PIERCE_MENU_MUSIC_CAPTION;
            }
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_DECLINE:
            captionSlot = SHOOTING_GALLERY_PIERCE_MENU_REPEAT_DECLINE_SLOT;
            if (_gShootingGalleryPierceGalleryCoursesStagedCount == 0) {
                captionSlot = SHOOTING_GALLERY_PIERCE_MENU_FIRST_DECLINE_SLOT;
            }
            capStartSequenceSlot(captionSlot, 0, 0);
            evsStartScript(gShootingGalleryPierceGalleryDeclineTrainingScript, EVENT_SCRIPT_HUD_KEEP);
            taskKill(task);
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_ACCEPT:
            captionSlot = SHOOTING_GALLERY_PIERCE_MENU_REPEAT_ACCEPT_SLOT;
            if (_gShootingGalleryPierceGalleryCoursesStagedCount == 0) {
                captionSlot = SHOOTING_GALLERY_PIERCE_MENU_FIRST_ACCEPT_SLOT;
            }
            capStartSequenceSlot(captionSlot, 0, 0);
            evsStartScript(gShootingGalleryPierceGalleryWaitTrainingAcceptanceScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_WAIT_ACCEPT:
            if (gGameSession->eventState == 0) {
                task->state = SHOOTING_GALLERY_PIERCE_MENU_MUSIC_CAPTION;
            }
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_MUSIC_CAPTION:
            capStartSequenceSlot(SHOOTING_GALLERY_PIERCE_MENU_MUSIC_SLOT, 0, 0);
            task->state++;
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_OPEN_MUSIC:
            if (capIsBusy() == 0) {
                mistShootingGalleryOpenJukebox(0);
                task->state++;
            }
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_AFTER_MUSIC:
            task->state = SHOOTING_GALLERY_PIERCE_MENU_WEAPON_CAPTION;
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_WEAPON_CAPTION:
            capStartSequenceSlot(SHOOTING_GALLERY_PIERCE_MENU_WEAPON_SLOT, 0, 0);
            task->state++;
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_OPEN_WEAPON:
            if (capIsBusy() == 0) {
                mistShootingGalleryOpenWeaponMenu(0);
                task->state++;
            }
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_AFTER_WEAPON:
            task->state = SHOOTING_GALLERY_PIERCE_MENU_LEVEL_CAPTION;
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_LEVEL_CAPTION:
            playerActorWriteWeaponAnimationBankIndex(&_gShootingGalleryPierceGalleryLevelPromptPlayerAnimation9Request.source.index);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &_gShootingGalleryPierceGalleryLevelPromptPlayerAnimation9Request, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(1), ACTOR_MESSAGE_PLAY_ANIMATION, &_gShootingGalleryPierceGalleryLevelPromptPierceAnimation23Request, 0);
            task->killCountdown = SHOOTING_GALLERY_PIERCE_MENU_ANIMATION_DELAY;
            capStartSequenceSlot(SHOOTING_GALLERY_PIERCE_MENU_LEVEL_CAPTION_BASE, 0, 0);
            task->state++;
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_WAIT_LEVEL_REPLY:
            _shootingGalleryPierceUpdateLevelReplyAnimations(task);
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_SAVE_LEVEL:
            gShootingGalleryPierceGallerySelectedCourseLevel = capGetVariantKey();
            mistShootingGalleryPrepareTrainingLoadout(gShootingGalleryPierceGallerySelectedCourseLevel);
            task->state = SHOOTING_GALLERY_PIERCE_MENU_CONFIRM_LEVEL;
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_CONFIRM_LEVEL:
            capStartSequenceSlot(capGetVariantKey() + SHOOTING_GALLERY_PIERCE_MENU_LEVEL_CAPTION_BASE, 0, 0);
            evsStartScript(gShootingGalleryPierceGalleryConfirmCourseScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_AFTER_CONFIRM:
            task->state = SHOOTING_GALLERY_PIERCE_MENU_STAGE_TRAINING;
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_STAGE_TRAINING:
            _gShootingGalleryPierceGalleryCoursesStagedCount++;
            gAttachmentState.flags |= ATTACHMENT_FLAG_SWAP_LOCK;
            if (gShootingGalleryPierceGallerySelectedCourseLevel < SHOOTING_GALLERY_PIERCE_MENU_FIRST_LOADOUT_LEVEL) {
                evsStartScript(gShootingGalleryPierceGalleryStartFixedViewTrainingScript, EVENT_SCRIPT_HUD_KEEP);
            } else {
                evsStartScript(gShootingGalleryPierceGalleryStartFreeMovementTrainingScript, EVENT_SCRIPT_HUD_KEEP);
            }
            gShootingGalleryPierceGalleryTrainingActive = 1;
            task->state++;
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_WAIT_INTRO:
        case SHOOTING_GALLERY_PIERCE_MENU_WAIT_CONFIRM:
        case SHOOTING_GALLERY_PIERCE_MENU_WAIT_TRAINING:
            if (gGameSession->eventState == 0) {
                task->state++;
            }
            break;
        case SHOOTING_GALLERY_PIERCE_MENU_SPAWN_TRAINING:
            gGameSession->flowFlags &= (0xFF ^ GAME_SESSION_FLOW_REEQUIP_WEAPON);
            taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_COURSE_CONTROLLER, gShootingGalleryPierceGallerySelectedCourseLevel - 1, 0);
            gShootingGalleryPierceGalleryExitInputBlocked = 1;
            taskKill(task);
            break;
    }
}
