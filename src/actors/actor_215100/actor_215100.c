#include "actor_215100_private.h"

#include "types.h"

#include "actors/actor_215100.h"

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

#include "rooms/mist_shooting_gallery.h"

TaskDesc gActor215100GalleryDecisionTaskDescs[2] = {
    { { { TASK_BODY_NONE, 32 } }, actor215100GalleryExitDecisionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, actor215100GalleryAbortDecisionTask, { .value = 0 } },
};

enum { ACTOR_215100_LEVEL_PROMPT_PLAYER_CLIP = 9 };

/// Resets player clip 9 as the gallery course-level prompt begins.
///
/// The menu rewrites source.index to the equipped weapon bank before each
/// synchronous dispatch. Drops player grid participation; reset ignores the
/// zero blend duration. Keep the live player and its weapon clips loaded.
static AnimationPlayRequest _gActor215100GalleryLevelPromptPlayerAnimation9Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_215100_LEVEL_PROMPT_PLAYER_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained bank-index-1 reset request for clip 0, with no playback consumer.
///
/// Its recipient and intended use are unproven. The collision selector is
/// disabled and the reset choice ignores the zero blend duration.
static const AnimationPlayRequest _gActor215100RetainedBank1Clip0ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = 0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum {
    ACTOR_215100_LEVEL_REPLY_FIRST_CLIP  = 24,
    ACTOR_215100_LEVEL_REPLY_SECOND_CLIP = 18,
};

/// Resets Pierce to clip 18 twenty-two updates after the course-level reply's first clip.
///
/// Sent synchronously at reply countdown -22; the live actor and clip bank
/// must remain loaded. Pierce ignores the stored bank and collision selectors.
static AnimationPlayRequest _gActor215100GalleryLevelReplyAnimation18Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_215100_LEVEL_REPLY_SECOND_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Four retained bank-index-1 reset requests for clips 19..22 in that order.
///
/// No consumer establishes a recipient or an indexing contract. Each complete
/// five-word request disables collision and ignores its zero blend duration.
static const AnimationPlayRequest _gActor215100RetainedBank1Clip19To22ResetRequests[4] SECTION(".data") = {
    { .source = { .index = 1 }, .animationId = 19, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 1 }, .animationId = 20, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 1 }, .animationId = 21, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 1 }, .animationId = 22, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
};

enum { ACTOR_215100_LEVEL_PROMPT_PIERCE_CLIP = 23 };

/// Resets Pierce to clip 23 as the gallery course-level prompt begins.
///
/// Placed actor 1 borrows this request during synchronous dispatch and ignores
/// the stored bank and collision selectors. Keep Pierce and his clips loaded.
static const AnimationPlayRequest _gActor215100GalleryLevelPromptPierceAnimation23Request SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_215100_LEVEL_PROMPT_PIERCE_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets Pierce to clip 24 when the course-level reply countdown reaches zero.
///
/// The borrowed request is consumed synchronously; Pierce ignores its stored
/// bank and collision selectors. The live actor and clip bank must stay loaded.
static AnimationPlayRequest _gActor215100GalleryLevelReplyAnimation24Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_215100_LEVEL_REPLY_FIRST_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

s32 gActor215100GalleryTrainingActive = 0;

s32 gActor215100GalleryExitInputBlocked = 0;

/// Courses staged since this gallery overlay was loaded.
///
/// Incremented when course startup is staged, before its script completes.
/// Zero selects the first-course offer, acceptance and decline captions;
/// any nonzero value selects their repeat variants. This remains a signed
/// word counter, rather than a completion flag or a persistent save tally.
static s32 _gActor215100GalleryCoursesStagedCount = 0;

s32 gActor215100GalleryExitInputCooldownTicks = 0;

#include "assets/rooms/mist/shooting_gallery/pierce_clips_1.inc"

/// Advances the caption-reply animation timer without going below -1000.
///
/// killCountdown counts callback updates, beginning at 27 in the caller. The
/// comparisons use its value after decrementing. Requires the loaded gallery
/// actor at placement 1; sends borrowed static requests at 0 and -22. A value
/// already below -1000 stays unchanged. Caption completion remains with the caller.
static inline void _actor215100UpdateLevelReplyAnimations(Task* task)
{
    enum { ACTOR_215100_LEVEL_REPLY_LAST_DECREMENT        = -999,
           ACTOR_215100_LEVEL_REPLY_SECOND_ANIMATION_TICK = -22 };
    if (task->killCountdown >= ACTOR_215100_LEVEL_REPLY_LAST_DECREMENT) {
        task->killCountdown--;
    }
    if (task->killCountdown == 0) {
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(1), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor215100GalleryLevelReplyAnimation24Request, 0);
    }
    if (task->killCountdown == ACTOR_215100_LEVEL_REPLY_SECOND_ANIMATION_TICK) {
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(1), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor215100GalleryLevelReplyAnimation18Request, 0);
    }
}

void actor215100GalleryTrainingMenuTask(Task* task)
{
    enum {
        ACTOR_215100_MENU_START               = 0,
        ACTOR_215100_MENU_WAIT_INTRO          = 1,
        ACTOR_215100_MENU_RESOLVE_REPLY       = 2,
        ACTOR_215100_MENU_DECLINE             = 4,
        ACTOR_215100_MENU_ACCEPT              = 5,
        ACTOR_215100_MENU_WAIT_ACCEPT         = 6,
        ACTOR_215100_MENU_MUSIC_CAPTION       = 10,
        ACTOR_215100_MENU_OPEN_MUSIC          = 11,
        ACTOR_215100_MENU_AFTER_MUSIC         = 12,
        ACTOR_215100_MENU_WEAPON_CAPTION      = 20,
        ACTOR_215100_MENU_OPEN_WEAPON         = 21,
        ACTOR_215100_MENU_AFTER_WEAPON        = 22,
        ACTOR_215100_MENU_LEVEL_CAPTION       = 30,
        ACTOR_215100_MENU_WAIT_LEVEL_REPLY    = 31,
        ACTOR_215100_MENU_SAVE_LEVEL          = 32,
        ACTOR_215100_MENU_CONFIRM_LEVEL       = 40,
        ACTOR_215100_MENU_WAIT_CONFIRM        = 41,
        ACTOR_215100_MENU_AFTER_CONFIRM       = 42,
        ACTOR_215100_MENU_STAGE_TRAINING      = 50,
        ACTOR_215100_MENU_WAIT_TRAINING       = 51,
        ACTOR_215100_MENU_SPAWN_TRAINING      = 52,
        ACTOR_215100_MENU_ANIMATION_DELAY     = 27,
        ACTOR_215100_MENU_FIRST_LOADOUT_LEVEL = 3,
        ACTOR_215100_MENU_LEVEL_CAPTION_BASE  = 11,
        ACTOR_215100_MENU_MUSIC_SLOT          = 9,
        ACTOR_215100_MENU_WEAPON_SLOT         = 10,
        ACTOR_215100_MENU_REPEAT_PROMPT_SLOT  = 28,
        ACTOR_215100_MENU_FIRST_PROMPT_SLOT   = 21,
        ACTOR_215100_MENU_REPEAT_DECLINE_SLOT = 8,
        ACTOR_215100_MENU_FIRST_DECLINE_SLOT  = 25,
        ACTOR_215100_MENU_REPEAT_ACCEPT_SLOT  = 7,
        ACTOR_215100_MENU_FIRST_ACCEPT_SLOT   = 24,
    };
    s16 captionSlot;

    switch (task->state) {
        case ACTOR_215100_MENU_START:
            if (gActor215100GalleryTrainingActive != 0) {
                taskKill(task);
                return;
            }
            if (task->spawnArg1.value == 0) {
                evsStartScript(gActor215100GalleryPrepareTrainingMenuScript, EVENT_SCRIPT_HUD_KEEP);
            } else {
                captionSlot = ACTOR_215100_MENU_REPEAT_PROMPT_SLOT;
                if (_gActor215100GalleryCoursesStagedCount == 0) {
                    captionSlot = ACTOR_215100_MENU_FIRST_PROMPT_SLOT;
                }
                capStartSequenceSlot(captionSlot, 0, 0);
                evsStartScript(gActor215100GalleryOfferTrainingMenuScript, EVENT_SCRIPT_HUD_KEEP);
            }
            task->state++;
            break;
        case ACTOR_215100_MENU_RESOLVE_REPLY:
            gGameSession->eventState = 1;
            if (task->spawnArg1.value != 0) {
                if (capGetVariantKey() != 0) {
                    task->state = ACTOR_215100_MENU_ACCEPT;
                } else {
                    task->state = ACTOR_215100_MENU_DECLINE;
                }
            } else {
                task->state = ACTOR_215100_MENU_MUSIC_CAPTION;
            }
            break;
        case ACTOR_215100_MENU_DECLINE:
            captionSlot = ACTOR_215100_MENU_REPEAT_DECLINE_SLOT;
            if (_gActor215100GalleryCoursesStagedCount == 0) {
                captionSlot = ACTOR_215100_MENU_FIRST_DECLINE_SLOT;
            }
            capStartSequenceSlot(captionSlot, 0, 0);
            evsStartScript(gActor215100GalleryDeclineTrainingScript, EVENT_SCRIPT_HUD_KEEP);
            taskKill(task);
            break;
        case ACTOR_215100_MENU_ACCEPT:
            captionSlot = ACTOR_215100_MENU_REPEAT_ACCEPT_SLOT;
            if (_gActor215100GalleryCoursesStagedCount == 0) {
                captionSlot = ACTOR_215100_MENU_FIRST_ACCEPT_SLOT;
            }
            capStartSequenceSlot(captionSlot, 0, 0);
            evsStartScript(gActor215100GalleryWaitTrainingAcceptanceScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR_215100_MENU_WAIT_ACCEPT:
            if (gGameSession->eventState == 0) {
                task->state = ACTOR_215100_MENU_MUSIC_CAPTION;
            }
            break;
        case ACTOR_215100_MENU_MUSIC_CAPTION:
            capStartSequenceSlot(ACTOR_215100_MENU_MUSIC_SLOT, 0, 0);
            task->state++;
            break;
        case ACTOR_215100_MENU_OPEN_MUSIC:
            if (capIsBusy() == 0) {
                mistShootingGalleryOpenJukebox(0);
                task->state++;
            }
            break;
        case ACTOR_215100_MENU_AFTER_MUSIC:
            task->state = ACTOR_215100_MENU_WEAPON_CAPTION;
            break;
        case ACTOR_215100_MENU_WEAPON_CAPTION:
            capStartSequenceSlot(ACTOR_215100_MENU_WEAPON_SLOT, 0, 0);
            task->state++;
            break;
        case ACTOR_215100_MENU_OPEN_WEAPON:
            if (capIsBusy() == 0) {
                mistShootingGalleryOpenWeaponMenu(0);
                task->state++;
            }
            break;
        case ACTOR_215100_MENU_AFTER_WEAPON:
            task->state = ACTOR_215100_MENU_LEVEL_CAPTION;
            break;
        case ACTOR_215100_MENU_LEVEL_CAPTION:
            playerActorWriteWeaponAnimationBankIndex(&_gActor215100GalleryLevelPromptPlayerAnimation9Request.source.index);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &_gActor215100GalleryLevelPromptPlayerAnimation9Request, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(1), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor215100GalleryLevelPromptPierceAnimation23Request, 0);
            task->killCountdown = ACTOR_215100_MENU_ANIMATION_DELAY;
            capStartSequenceSlot(ACTOR_215100_MENU_LEVEL_CAPTION_BASE, 0, 0);
            task->state++;
            break;
        case ACTOR_215100_MENU_WAIT_LEVEL_REPLY:
            _actor215100UpdateLevelReplyAnimations(task);
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case ACTOR_215100_MENU_SAVE_LEVEL:
            gActor215100GallerySelectedCourseLevel = capGetVariantKey();
            mistShootingGalleryPrepareTrainingLoadout(gActor215100GallerySelectedCourseLevel);
            task->state = ACTOR_215100_MENU_CONFIRM_LEVEL;
            break;
        case ACTOR_215100_MENU_CONFIRM_LEVEL:
            capStartSequenceSlot(capGetVariantKey() + ACTOR_215100_MENU_LEVEL_CAPTION_BASE, 0, 0);
            evsStartScript(gActor215100GalleryConfirmCourseScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR_215100_MENU_AFTER_CONFIRM:
            task->state = ACTOR_215100_MENU_STAGE_TRAINING;
            break;
        case ACTOR_215100_MENU_STAGE_TRAINING:
            _gActor215100GalleryCoursesStagedCount++;
            gAttachmentState.flags |= ATTACHMENT_FLAG_SWAP_LOCK;
            if (gActor215100GallerySelectedCourseLevel < ACTOR_215100_MENU_FIRST_LOADOUT_LEVEL) {
                evsStartScript(gActor215100GalleryStartFixedViewTrainingScript, EVENT_SCRIPT_HUD_KEEP);
            } else {
                evsStartScript(gActor215100GalleryStartFreeMovementTrainingScript, EVENT_SCRIPT_HUD_KEEP);
            }
            gActor215100GalleryTrainingActive = 1;
            task->state++;
            break;
        case ACTOR_215100_MENU_WAIT_INTRO:
        case ACTOR_215100_MENU_WAIT_CONFIRM:
        case ACTOR_215100_MENU_WAIT_TRAINING:
            if (gGameSession->eventState == 0) {
                task->state++;
            }
            break;
        case ACTOR_215100_MENU_SPAWN_TRAINING:
            gGameSession->flowFlags &= (0xFF ^ GAME_SESSION_FLOW_REEQUIP_WEAPON);
            taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_COURSE_CONTROLLER, gActor215100GallerySelectedCourseLevel - 1, 0);
            gActor215100GalleryExitInputBlocked = 1;
            taskKill(task);
            break;
    }
}
