#include "actor_215100_private.h"

#include "types.h"

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

extern AnimationPlayRequest D_actor_215100_8014CF84;
extern AnimationPlayRequest D_actor_215100_8014CFAC;
extern AnimationPlayRequest D_actor_215100_8014D010;
extern AnimationPlayRequest D_actor_215100_8014D024;
extern s32                  D_actor_215100_8014D040;

TaskDesc D_actor_215100_8014CF6C[2] = {
    { { { TASK_BODY_NONE, 32 } }, actor215100GalleryExitDecisionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, actor215100GalleryAbortDecisionTask, { .value = 0 } },
};

AnimationPlayRequest D_actor_215100_8014CF84 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014CF98 = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014CFAC = { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014CFC0[4] = {
    { { .index = 1 }, 19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 21, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 22, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_215100_8014D010 = { { .index = 1 }, 23, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014D024 = { { .index = 1 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

s32 D_actor_215100_8014D038 = 0;

s32 D_actor_215100_8014D03C = 0;

s32 D_actor_215100_8014D040 = 0;

s32 D_actor_215100_8014D044 = 0;

static AnimationPackedPose _gActor215100Animation034E4Bank1[5] = {
#include "assets/actor_215100_animation_034E4_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation034E4Bank4[61] = {
#include "assets/actor_215100_animation_034E4_bank4.inc"
};

static AnimationRecord _gActor215100Animation034E4Records[89] = {
#include "assets/actor_215100_animation_034E4_records.inc"
};

static u16 _gActor215100Animation034E4Indices[20] = {
#include "assets/actor_215100_animation_034E4_indices.inc"
};

AnimationSet gActor215100Animation034E4 = {
    _gActor215100Animation034E4Records,
    _gActor215100Animation034E4Indices,
    { NULL, _gActor215100Animation034E4Bank1, NULL, NULL, _gActor215100Animation034E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation03754Bank1[2] = {
#include "assets/actor_215100_animation_03754_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation03754Bank4[26] = {
#include "assets/actor_215100_animation_03754_bank4.inc"
};

static AnimationRecord _gActor215100Animation03754Records[104] = {
#include "assets/actor_215100_animation_03754_records.inc"
};

static u16 _gActor215100Animation03754Indices[20] = {
#include "assets/actor_215100_animation_03754_indices.inc"
};

AnimationSet gActor215100Animation03754 = {
    _gActor215100Animation03754Records,
    _gActor215100Animation03754Indices,
    { NULL, _gActor215100Animation03754Bank1, NULL, NULL, _gActor215100Animation03754Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation039ACBank1[2] = {
#include "assets/actor_215100_animation_039AC_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation039ACBank4[35] = {
#include "assets/actor_215100_animation_039AC_bank4.inc"
};

static AnimationRecord _gActor215100Animation039ACRecords[89] = {
#include "assets/actor_215100_animation_039AC_records.inc"
};

static u16 _gActor215100Animation039ACIndices[20] = {
#include "assets/actor_215100_animation_039AC_indices.inc"
};

AnimationSet gActor215100Animation039AC = {
    _gActor215100Animation039ACRecords,
    _gActor215100Animation039ACIndices,
    { NULL, _gActor215100Animation039ACBank1, NULL, NULL, _gActor215100Animation039ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation03B48Bank1[2] = {
#include "assets/actor_215100_animation_03B48_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation03B48Bank4[20] = {
#include "assets/actor_215100_animation_03B48_bank4.inc"
};

static AnimationRecord _gActor215100Animation03B48Records[57] = {
#include "assets/actor_215100_animation_03B48_records.inc"
};

static u16 _gActor215100Animation03B48Indices[20] = {
#include "assets/actor_215100_animation_03B48_indices.inc"
};

AnimationSet gActor215100Animation03B48 = {
    _gActor215100Animation03B48Records,
    _gActor215100Animation03B48Indices,
    { NULL, _gActor215100Animation03B48Bank1, NULL, NULL, _gActor215100Animation03B48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation03D98Bank1[2] = {
#include "assets/actor_215100_animation_03D98_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation03D98Bank4[45] = {
#include "assets/actor_215100_animation_03D98_bank4.inc"
};

static AnimationRecord _gActor215100Animation03D98Records[77] = {
#include "assets/actor_215100_animation_03D98_records.inc"
};

static u16 _gActor215100Animation03D98Indices[20] = {
#include "assets/actor_215100_animation_03D98_indices.inc"
};

AnimationSet gActor215100Animation03D98 = {
    _gActor215100Animation03D98Records,
    _gActor215100Animation03D98Indices,
    { NULL, _gActor215100Animation03D98Bank1, NULL, NULL, _gActor215100Animation03D98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation03FF0Bank1[2] = {
#include "assets/actor_215100_animation_03FF0_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation03FF0Bank4[46] = {
#include "assets/actor_215100_animation_03FF0_bank4.inc"
};

static AnimationRecord _gActor215100Animation03FF0Records[78] = {
#include "assets/actor_215100_animation_03FF0_records.inc"
};

static u16 _gActor215100Animation03FF0Indices[20] = {
#include "assets/actor_215100_animation_03FF0_indices.inc"
};

AnimationSet gActor215100Animation03FF0 = {
    _gActor215100Animation03FF0Records,
    _gActor215100Animation03FF0Indices,
    { NULL, _gActor215100Animation03FF0Bank1, NULL, NULL, _gActor215100Animation03FF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation042F4Bank1[6] = {
#include "assets/actor_215100_animation_042F4_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation042F4Bank4[46] = {
#include "assets/actor_215100_animation_042F4_bank4.inc"
};

static AnimationRecord _gActor215100Animation042F4Records[109] = {
#include "assets/actor_215100_animation_042F4_records.inc"
};

static u16 _gActor215100Animation042F4Indices[20] = {
#include "assets/actor_215100_animation_042F4_indices.inc"
};

AnimationSet gActor215100Animation042F4 = {
    _gActor215100Animation042F4Records,
    _gActor215100Animation042F4Indices,
    { NULL, _gActor215100Animation042F4Bank1, NULL, NULL, _gActor215100Animation042F4Bank4, NULL, NULL, NULL },
};

/// Advances the caption-reply animation timer without going below -1000.
///
/// Requires the loaded gallery actor at placement 1. Sends borrowed static
/// requests at timer 0 and -22; caption completion remains with the caller.
static inline void _actor215100UpdateLevelReplyAnimations(Task* task)
{
    enum { LAST_DECREMENT        = -999,
           SECOND_ANIMATION_TICK = -22 };
    if (task->killCountdown >= LAST_DECREMENT) {
        task->killCountdown--;
    }
    if (task->killCountdown == 0) {
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(1), ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_215100_8014D024, 0);
    }
    if (task->killCountdown == SECOND_ANIMATION_TICK) {
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(1), ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_215100_8014CFAC, 0);
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
            if (D_actor_215100_8014D038 != 0) {
                taskKill(task);
                return;
            }
            if (task->spawnArg1.value == 0) {
                evsStartScript(D_actor_215100_8014ED90, EVENT_SCRIPT_HUD_KEEP);
            } else {
                captionSlot = ACTOR_215100_MENU_REPEAT_PROMPT_SLOT;
                if (D_actor_215100_8014D040 == 0) {
                    captionSlot = ACTOR_215100_MENU_FIRST_PROMPT_SLOT;
                }
                capStartSequenceSlot(captionSlot, 0, 0);
                evsStartScript(D_actor_215100_8014EE68, EVENT_SCRIPT_HUD_KEEP);
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
            if (D_actor_215100_8014D040 == 0) {
                captionSlot = ACTOR_215100_MENU_FIRST_DECLINE_SLOT;
            }
            capStartSequenceSlot(captionSlot, 0, 0);
            evsStartScript(D_actor_215100_8014EBE0, EVENT_SCRIPT_HUD_KEEP);
            taskKill(task);
            break;
        case ACTOR_215100_MENU_ACCEPT:
            captionSlot = ACTOR_215100_MENU_REPEAT_ACCEPT_SLOT;
            if (D_actor_215100_8014D040 == 0) {
                captionSlot = ACTOR_215100_MENU_FIRST_ACCEPT_SLOT;
            }
            capStartSequenceSlot(captionSlot, 0, 0);
            evsStartScript(D_actor_215100_8014EB98, EVENT_SCRIPT_HUD_KEEP);
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
            playerActorWriteWeaponAnimationBankIndex(&D_actor_215100_8014CF84.source.index);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &D_actor_215100_8014CF84, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(1), ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_215100_8014D010, 0);
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
            D_actor_215100_8015E670 = capGetVariantKey();
            mistShootingGalleryPrepareTrainingLoadout(D_actor_215100_8015E670);
            task->state = ACTOR_215100_MENU_CONFIRM_LEVEL;
            break;
        case ACTOR_215100_MENU_CONFIRM_LEVEL:
            capStartSequenceSlot(capGetVariantKey() + ACTOR_215100_MENU_LEVEL_CAPTION_BASE, 0, 0);
            evsStartScript(D_actor_215100_8014F138, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR_215100_MENU_AFTER_CONFIRM:
            task->state = ACTOR_215100_MENU_STAGE_TRAINING;
            break;
        case ACTOR_215100_MENU_STAGE_TRAINING:
            D_actor_215100_8014D040++;
            Gp_StateC08.flags |= ATTACHMENT_FLAG_SWAP_LOCK;
            if (D_actor_215100_8015E670 < ACTOR_215100_MENU_FIRST_LOADOUT_LEVEL) {
                evsStartScript(D_actor_215100_8014EFA0, EVENT_SCRIPT_HUD_KEEP);
            } else {
                evsStartScript(D_actor_215100_8014F060, EVENT_SCRIPT_HUD_KEEP);
            }
            D_actor_215100_8014D038 = 1;
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
            taskSpawnFromTable(D_mist_shooting_gallery_801856B8, 0, D_actor_215100_8015E670 - 1, 0);
            D_actor_215100_8014D03C = 1;
            taskKill(task);
            break;
    }
}
