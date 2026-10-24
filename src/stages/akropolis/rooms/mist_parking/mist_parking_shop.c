#include "rooms/mist_parking.h"

#include "../../room_ids.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "mist_parking_private.h"
#include "mist_parking_head_aim.h"

#include "gameplay/actor_presentation.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/companion_load.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#define ROOM_PART declarations_mist_parking_shop
#include "assets/rooms/mist/parking/declarations.inc"

static void _mistParkingSelectShopDialogueResource(s32 resourceOrdinal);

static void _mistParkingAimPlayerHeadAtShopPartnerTask(Task* task);
static void _mistParkingJodieShopTalkTask(Task* task);
static void _mistParkingShopDepartureMenuTask(Task* task);
static void _mistParkingDepartForAkropolisTask(Task* unusedTask);
static void _mistParkingDelayShopDisplayModeExitTask(Task* task);

static void _mistParkingRunShopDialogueCommand(s32 commandIndex);
static void _mistParkingStartShopDepartureMovie(s32 movieSelector);
static void _mistParkingReloadShopAkropolisArea(s32 areaId);
static void _mistParkingQueueAkropolisDeparture(void);
static void _mistParkingStartShopPlayerHeadAim(void);
static void _mistParkingControlShopPlayerHeadAim(s32 mode);
static void _mistParkingQueueDelayedShopDisplayModeExit(s32 delayTicks);

/// Ordinals of the already-loaded CAP resources used by the variant-1 talks.
enum {
    MIST_PARKING_SHOP_DIALOGUE_DEFAULT = 0,
    MIST_PARKING_SHOP_DIALOGUE_MENU    = 1,
    MIST_PARKING_SHOP_DIALOGUE_PRIZES  = 2
};

/// The five course prizes occupy consecutive saved flags and placed-item states.
enum {
    MIST_PARKING_SHOP_PRIZE_COUNT            = 5,
    MIST_PARKING_SHOP_PRIZE_FLAG_FIRST       = EVENT_FLAG_MIST_SHOOTING_GALLERY_PRIZE_FIRST,
    MIST_PARKING_SHOP_PRIZE_WAITING          = 2,
    MIST_PARKING_SHOP_PRIZE_CLOSED           = 3,
    MIST_PARKING_SHOP_PRIZE_OBJECT_FIRST     = 0x20,
    MIST_PARKING_SHOP_PRIZE_OBJECT_AVAILABLE = 1,
    MIST_PARKING_SHOP_PRIZE_CAPTION_SLOT     = 5
};

/// Destination selectors used by the variant-1 departure callbacks.
enum {
    MIST_PARKING_SHOP_RELOAD_WARP           = 1,
    MIST_PARKING_SHOP_RELOAD_ROOM           = 1,
    MIST_PARKING_SHOP_RELOAD_SPRITE_VARIANT = 1
};

/// Execution priorities of the shop controllers and the player's head-aim update.
enum {
    MIST_PARKING_SHOP_HEAD_AIM_PRIORITY = 97
};

/// Pierce's variant-1 departure animations and head-turn commands.
enum {
    MIST_PARKING_PIERCE_COMMAND_AIM_HEAD     = 2,
    MIST_PARKING_PIERCE_COMMAND_RELEASE_HEAD = 3
};

TaskDesc gMistParkingShopTaskDescriptors[5] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mistParkingDepartForAkropolisTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_SHOP_HEAD_AIM_PRIORITY } }, _mistParkingAimPlayerHeadAtShopPartnerTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mistParkingDelayShopDisplayModeExitTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mistParkingJodieShopTalkTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mistParkingShopDepartureMenuTask, { .value = 0 } },
};

/// Variant-1 player bank extension: empty slot 47, then talk, menu and stay clips.
static AnimationSet* _gMistParkingShopPlayerAnimationSets[] = {
    NULL,
    &gMistParkingStandCopyClip,
    &gMistParkingTalkStartCopyClip,
    &gMistParkingPlayerConversationAnim21CopyClip,
};

#define ROOM_PART gMistParkingShopPlayerAnimationBankCopy
#include "assets/rooms/mist/parking/events.inc"
/// Player head-aim requests by extension index (0 disabled, 1 enabled).
static s8 _gMistParkingShopPlayerHeadAimFlags[ARRAY_SIZE(_gMistParkingShopPlayerAnimationSets)] = { 0, 1, 1, 0 };

#define ROOM_PART _gMistParkingUnusedBank0Clip1ResetAnimation
#include "assets/rooms/mist/parking/events.inc"
/// Unreferenced; its role is unproven.
static u8 _gMistParkingUnusedBytes[8] = { 0, 0, 0, 0, 0, 0, 1, 0 };
#define ROOM_PART _gMistParkingShopPierceAimHeadCommand
#include "assets/rooms/mist/parking/events.inc"

void mistParkingSetPierceCollisionPatchLowered(s32 lowerPatch)
{
    enum {
        MIST_PARKING_PIERCE_PATCH_FACE_COUNT   = 2,
        MIST_PARKING_PIERCE_PATCH_VERTEX_COUNT = 6,
        MIST_PARKING_PIERCE_PATCH_LOWER_Y      = 2000
    };
    WorldCollisionGrid*       liveGrid;
    const WorldCollisionGrid* sourcePatch;
    SVECTOR                   translation;
    s32                       elementIndex;

    liveGrid    = &gMistParkingCollisionGrid;
    sourcePatch = &gMistParkingPierceCollisionPatch;

    // Rebuild the reserved grid prefix without overwriting vector fourth halfwords.
    for (elementIndex = 0; elementIndex < MIST_PARKING_PIERCE_PATCH_FACE_COUNT; elementIndex++) {
        liveGrid->normals[elementIndex].vx = sourcePatch->normals[elementIndex].vx;
        liveGrid->normals[elementIndex].vy = sourcePatch->normals[elementIndex].vy;
        liveGrid->normals[elementIndex].vz = sourcePatch->normals[elementIndex].vz;
        liveGrid->faces[elementIndex]      = sourcePatch->faces[elementIndex];
    }

    for (elementIndex = 0; elementIndex < MIST_PARKING_PIERCE_PATCH_VERTEX_COUNT; elementIndex++) {
        liveGrid->vertices[elementIndex].vx = sourcePatch->vertices[elementIndex].vx;
        liveGrid->vertices[elementIndex].vy = sourcePatch->vertices[elementIndex].vy;
        liveGrid->vertices[elementIndex].vz = sourcePatch->vertices[elementIndex].vz;
    }

    if (lowerPatch == MIST_PARKING_PIERCE_PATCH_RESTORED) {
        translation.vx = 0;
        translation.vy = 0;
    } else {
        translation.vx = 0;
        translation.vy = MIST_PARKING_PIERCE_PATCH_LOWER_Y;
    }
    translation.vz = 0;

    for (elementIndex = 0; elementIndex < MIST_PARKING_PIERCE_PATCH_VERTEX_COUNT; elementIndex++) {
        liveGrid->vertices[elementIndex].vx += translation.vx;
        liveGrid->vertices[elementIndex].vy += translation.vy;
        liveGrid->vertices[elementIndex].vz += translation.vz;
    }
}

/// Ramps the player's head aim toward the variant-1 shop/departure talk partner.
static void _mistParkingAimPlayerHeadAtShopPartnerTask(Task* task)
{
    const GameActor* player;
    Enemy*           talkPartner;
    s32              extensionIndex;
    s32              animationRequestsAim;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    if (gEvsPlaybackPaused == 0) {
        // Extension index zero has no aim flag; the copied bank bounds the lookup.
        extensionIndex = player->animationSlots[1].nextPose.indices.setIndex - ANIMATION_BANK_BASE_SET_COUNT;
        if ((extensionIndex > 0) && (extensionIndex < gMistParkingShopPlayerAnimationBankCopy.count)) {
            animationRequestsAim = _gMistParkingShopPlayerHeadAimFlags[extensionIndex];
        } else {
            animationRequestsAim = 0;
        }
        if (task->state == MIST_PARKING_HEAD_AIM_UPDATE) {
            _mistParkingRampPlayerHeadAimBlend(task, animationRequestsAim);
            // Placement index zero supplies the talk partner for this stage and area.
            talkPartner = sceneFindEnemyByPlaceKey(gGameSession->location.area | (gGameSession->location.stage << 8));
            animationAimHeadAtTask(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), talkPartner->task,
                                   MIST_PARKING_HEAD_AIM_MAX_YAW, MIST_PARKING_HEAD_AIM_MAX_PITCH, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}

/// Advances one prize's announcement in the variant-1 shop conversation.
static inline void _mistParkingAdvanceShopPrizeAnnouncement(Task* task, MistParkingShopTalkState* talk)
{
    s16 courseIndex;

    if (capIsBusy() != 0) {
        return;
    }
    talk->prizeTimer--;
    if (talk->prizeTimer == MIST_PARKING_PRIZE_ANNOUNCEMENT_CAPTION_FRAME) {
        courseIndex = talk->prizeIndex;
        if (gameFlagGetNibble(courseIndex + MIST_PARKING_SHOP_PRIZE_FLAG_FIRST) == MIST_PARKING_SHOP_PRIZE_WAITING) {
            capStartSequenceSlot(MIST_PARKING_SHOP_PRIZE_CAPTION_SLOT, CAP_PLAYBACK_IN_PLACE, courseIndex);
        }
        return;
    }
    if (talk->prizeTimer != 0) {
        return;
    }
    courseIndex = talk->prizeIndex;
    if (areaGetCurrentObjectState(courseIndex + MIST_PARKING_SHOP_PRIZE_OBJECT_FIRST) != MIST_PARKING_SHOP_PRIZE_OBJECT_AVAILABLE) {
        gameFlagSetNibble(courseIndex + MIST_PARKING_SHOP_PRIZE_FLAG_FIRST, MIST_PARKING_SHOP_PRIZE_CLOSED);
    }
    talk->prizeTimer = MIST_PARKING_PRIZE_ANNOUNCEMENT_FRAMES;
    talk->prizeIndex++;
    if (talk->prizeIndex >= MIST_PARKING_SHOP_PRIZE_COUNT) {
        task->state++;
    }
}

/// Runs Jodie's variant-1 prize announcements and shop conversation.
static void _mistParkingJodieShopTalkTask(Task* task)
{
    enum {
        MIST_PARKING_SHOP_TALK_GREET                = 0,
        MIST_PARKING_SHOP_TALK_CHECK_PRIZES         = 1,
        MIST_PARKING_SHOP_TALK_PRIZE_PROMPT         = 2,
        MIST_PARKING_SHOP_TALK_PRIZE_CHOICE         = 3,
        MIST_PARKING_SHOP_TALK_ANNOUNCE_PRIZES      = 4,
        MIST_PARKING_SHOP_TALK_CLOSE_PRIZES         = 5,
        MIST_PARKING_SHOP_TALK_MENU_DELAY           = 6,
        MIST_PARKING_SHOP_TALK_WAIT_MENU            = 7,
        MIST_PARKING_SHOP_TALK_MENU_CHOICE          = 8,
        MIST_PARKING_SHOP_TALK_WAIT_REPLY           = 9,
        MIST_PARKING_SHOP_TALK_FINISH               = 10,
        MIST_PARKING_SHOP_TALK_GREETING_COMMAND     = 6,
        MIST_PARKING_SHOP_TALK_PRIZE_PROMPT_COMMAND = 1,
        MIST_PARKING_SHOP_TALK_ANNOUNCE_KEY         = 1,
        MIST_PARKING_SHOP_TALK_BRIEF_REPLY_COMMAND  = 7,
        MIST_PARKING_SHOP_TALK_ANNOUNCED_COMMAND    = 2,
        MIST_PARKING_SHOP_TALK_SKIPPED_COMMAND      = 3,
        MIST_PARKING_SHOP_TALK_MENU_COMMAND         = 9,
        MIST_PARKING_SHOP_TALK_CONTINUE_COMMAND     = 8,
        MIST_PARKING_SHOP_TALK_NO_BUSINESS_COMMAND  = 10,
        MIST_PARKING_SHOP_TALK_OPEN_SHOP_KEY        = 6,
        MIST_PARKING_SHOP_TALK_CONTINUE_KEY         = 7,
        MIST_PARKING_SHOP_TALK_LEAVE_KEY            = 8,
        MIST_PARKING_SHOP_TALK_REPLY_DELAY_TICKS    = 10,
        MIST_PARKING_SHOP_TALK_BUSINESS_DONE        = 1
    };
    MistParkingShopTalkState* talk = &gMistParkingShopTalk;
    s32                       nextState;
    s32                       courseIndex;
    s32                       choiceKey;

    switch (task->state) {
        case MIST_PARKING_SHOP_TALK_GREET:
            memFillBytes(talk, 0, sizeof(*talk));
            _mistParkingSelectShopDialogueResource(MIST_PARKING_SHOP_DIALOGUE_MENU);
            evsStartScript(gMistParkingJodieShopTalkScript, EVENT_SCRIPT_HUD_KEEP);
            capRunCommand(MIST_PARKING_SHOP_TALK_GREETING_COMMAND, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case MIST_PARKING_SHOP_TALK_CHECK_PRIZES:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            for (courseIndex = 0; courseIndex < MIST_PARKING_SHOP_PRIZE_COUNT; courseIndex++) {
                if (gameFlagGetNibble(courseIndex + MIST_PARKING_SHOP_PRIZE_FLAG_FIRST) == MIST_PARKING_SHOP_PRIZE_WAITING) {
                    task->state = MIST_PARKING_SHOP_TALK_PRIZE_PROMPT;
                    return;
                }
            }
            task->state = MIST_PARKING_SHOP_TALK_MENU_DELAY;
            break;
        case MIST_PARKING_SHOP_TALK_PRIZE_PROMPT:
            _mistParkingSelectShopDialogueResource(MIST_PARKING_SHOP_DIALOGUE_PRIZES);
            evsStartScript(gMistParkingJodieShopTalkScript, EVENT_SCRIPT_HUD_KEEP);
            capRunCommand(MIST_PARKING_SHOP_TALK_PRIZE_PROMPT_COMMAND, CAP_PLAYBACK_IN_PLACE);
            talk->businessDone = MIST_PARKING_SHOP_TALK_BUSINESS_DONE;
            task->state++;
            break;
        case MIST_PARKING_SHOP_TALK_PRIZE_CHOICE:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            if (capGetVariantKey() == MIST_PARKING_SHOP_TALK_ANNOUNCE_KEY) {
                capRunCommand(MIST_PARKING_SHOP_TALK_BRIEF_REPLY_COMMAND, CAP_PLAYBACK_IN_PLACE);
                talk->prizeTimer          = MIST_PARKING_PRIZE_ANNOUNCEMENT_FRAMES;
                talk->prizeClosingCommand = MIST_PARKING_SHOP_TALK_ANNOUNCED_COMMAND;
                task->state               = MIST_PARKING_SHOP_TALK_ANNOUNCE_PRIZES;
            } else {
                talk->prizeClosingCommand = MIST_PARKING_SHOP_TALK_SKIPPED_COMMAND;
                task->state               = MIST_PARKING_SHOP_TALK_CLOSE_PRIZES;
            }
            break;
        case MIST_PARKING_SHOP_TALK_ANNOUNCE_PRIZES:
            _mistParkingAdvanceShopPrizeAnnouncement(task, talk);
            break;
        case MIST_PARKING_SHOP_TALK_CLOSE_PRIZES:
            evsStartScript(gMistParkingJodieShopTalkScript, EVENT_SCRIPT_HUD_KEEP);
            capRunCommand(talk->prizeClosingCommand, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case MIST_PARKING_SHOP_TALK_MENU_DELAY:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            task->killCountdown++;
            if (task->killCountdown == MIST_PARKING_SHOP_TALK_REPLY_DELAY_TICKS) {
                _mistParkingSelectShopDialogueResource(MIST_PARKING_SHOP_DIALOGUE_MENU);
                capRunCommand(MIST_PARKING_SHOP_TALK_MENU_COMMAND, CAP_PLAYBACK_IN_PLACE);
                task->killCountdown = 0;
                task->state++;
            }
            break;
        case MIST_PARKING_SHOP_TALK_WAIT_MENU:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case MIST_PARKING_SHOP_TALK_MENU_CHOICE:
            // Keep the menu key while its script changes the CAP resource/selection.
            choiceKey             = capGetVariantKey();
            task->spawnArg1.value = choiceKey;
            if (choiceKey == MIST_PARKING_SHOP_TALK_OPEN_SHOP_KEY) {
                evsStartScript(gMistParkingJodieOpenShopScript, EVENT_SCRIPT_HUD_KEEP);
                talk->businessDone = MIST_PARKING_SHOP_TALK_BUSINESS_DONE;
            } else if (choiceKey == MIST_PARKING_SHOP_TALK_CONTINUE_KEY) {
                evsStartScript(gMistParkingJodieShopReplyScript, EVENT_SCRIPT_HUD_KEEP);
                talk->businessDone = MIST_PARKING_SHOP_TALK_BUSINESS_DONE;
            } else {
                evsStartScript(gMistParkingJodieShopFarewellScript, EVENT_SCRIPT_HUD_KEEP);
            }
            task->state++;
            break;
        case MIST_PARKING_SHOP_TALK_WAIT_REPLY:
            task->killCountdown++;
            if (task->killCountdown == MIST_PARKING_SHOP_TALK_REPLY_DELAY_TICKS) {
                switch (task->spawnArg1.value) {
                    case MIST_PARKING_SHOP_TALK_OPEN_SHOP_KEY:
                        capRunCommand(MIST_PARKING_SHOP_TALK_BRIEF_REPLY_COMMAND, CAP_PLAYBACK_IN_PLACE);
                        break;
                    case MIST_PARKING_SHOP_TALK_CONTINUE_KEY:
                        capRunCommand(MIST_PARKING_SHOP_TALK_CONTINUE_COMMAND, CAP_PLAYBACK_IN_PLACE);
                        break;
                    case MIST_PARKING_SHOP_TALK_LEAVE_KEY:
                        if (talk->businessDone != 0) {
                            capRunCommand(MIST_PARKING_SHOP_TALK_BRIEF_REPLY_COMMAND, CAP_PLAYBACK_IN_PLACE);
                        } else {
                            capRunCommand(MIST_PARKING_SHOP_TALK_NO_BUSINESS_COMMAND, CAP_PLAYBACK_IN_PLACE);
                        }
                        break;
                }
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            nextState           = MIST_PARKING_SHOP_TALK_FINISH;
            task->killCountdown = 0;
            if (task->spawnArg1.value == MIST_PARKING_SHOP_TALK_CONTINUE_KEY) {
                nextState = MIST_PARKING_SHOP_TALK_MENU_DELAY;
            }
            task->state = nextState;
            break;
        case MIST_PARKING_SHOP_TALK_FINISH:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            _mistParkingSelectShopDialogueResource(MIST_PARKING_SHOP_DIALOGUE_DEFAULT);
            taskKill(task);
            break;
    }
}

/// Runs the variant-1 departure menu and restores the default CAP selection.
static void _mistParkingShopDepartureMenuTask(Task* task)
{
    enum {
        MIST_PARKING_SHOP_DEPARTURE_START            = 0,
        MIST_PARKING_SHOP_DEPARTURE_WAIT_PROMPT      = 1,
        MIST_PARKING_SHOP_DEPARTURE_SELECT           = 2,
        MIST_PARKING_SHOP_DEPARTURE_WAIT_SCRIPT      = 3,
        MIST_PARKING_SHOP_DEPARTURE_FINISH           = 4,
        MIST_PARKING_SHOP_DEPARTURE_STAY             = 1,
        MIST_PARKING_SHOP_DEPARTURE_PLAZA            = 2,
        MIST_PARKING_SHOP_DEPARTURE_SHOOTING_GALLERY = 3
    };
    s32 choiceKey;

    switch (task->state) {
        case MIST_PARKING_SHOP_DEPARTURE_START:
            _mistParkingSelectShopDialogueResource(MIST_PARKING_SHOP_DIALOGUE_MENU);
            evsStartScript(_gMistParkingShopDepartureMenuScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case MIST_PARKING_SHOP_DEPARTURE_WAIT_PROMPT:
        case MIST_PARKING_SHOP_DEPARTURE_WAIT_SCRIPT:
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state++;
            break;
        case MIST_PARKING_SHOP_DEPARTURE_SELECT:
            // Retain the choice while its script runs and changes CAP selection.
            choiceKey             = capGetVariantKey();
            task->spawnArg1.value = choiceKey;
            switch (choiceKey) {
                case MIST_PARKING_SHOP_DEPARTURE_STAY:
                    evsStartScript(_gMistParkingShopStayScript, EVENT_SCRIPT_HUD_KEEP);
                    break;
                case MIST_PARKING_SHOP_DEPARTURE_PLAZA:
                    evsStartScript(_gMistParkingShopDepartForAkropolisScript, EVENT_SCRIPT_HUD_KEEP);
                    break;
                case MIST_PARKING_SHOP_DEPARTURE_SHOOTING_GALLERY:
                    evsStartScript(_gMistParkingShopDepartForShootingGalleryScript, EVENT_SCRIPT_HUD_KEEP);
                    break;
            }
            task->state++;
            break;
        case MIST_PARKING_SHOP_DEPARTURE_FINISH:
            if (task->spawnArg1.value == MIST_PARKING_SHOP_DEPARTURE_STAY) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            }
            _mistParkingSelectShopDialogueResource(MIST_PARKING_SHOP_DIALOGUE_DEFAULT);
            taskKill(task);
            break;
    }
}

/// Runs the selected variant-1 CAP command in place.
static void _mistParkingRunShopDialogueCommand(s32 commandIndex)
{
    capRunCommand(commandIndex, CAP_PLAYBACK_IN_PLACE);
}

/// Queues the variant-1 departure movie and freezes room objects.
static void _mistParkingStartShopDepartureMovie(s32 movieSelector)
{
    enum { MIST_PARKING_SHOP_MOVIE_LAUNCHER_DESCRIPTOR = 0 };

    taskSpawnFromTable(gMistParkingDepartureMovieTaskDescs, MIST_PARKING_SHOP_MOVIE_LAUNCHER_DESCRIPTOR, movieSelector, 0);
    gGameSession->freezeRoomObjs = 1;
}

/// Installs the variant-1 Akropolis destination and reloads from the current frame.
static void _mistParkingReloadShopAkropolisArea(s32 areaId)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = MIST_PARKING_SHOP_RELOAD_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = MIST_PARKING_SHOP_RELOAD_ROOM;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = areaId;
    gDisplayState.spriteVariant                                 = MIST_PARKING_SHOP_RELOAD_SPRITE_VARIANT;
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    if (areaId == AKROPOLIS_ROOM_ID_PLAZA) {
        gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, GAME_FLOW_LOAD_CAPTION_NORMAL);
    }
}

/// Queues the loadout-preparing Akropolis departure after the variant-1 movie.
static void _mistParkingQueueAkropolisDeparture(void)
{
    enum { MIST_PARKING_AKROPOLIS_DEPARTURE_DESCRIPTOR = 0 };

    taskSpawnFromTable(gMistParkingShopTaskDescriptors, MIST_PARKING_AKROPOLIS_DEPARTURE_DESCRIPTOR, 0, 0);
}

/// Prepares the player's Akropolis loadout and reloads into the plaza.
static void _mistParkingDepartForAkropolisTask(Task* unusedTask)
{
    enum { MIST_PARKING_AKROPOLIS_PLAYER_RESOURCE_VARIANT = 1 };

    playerActorPrepareAcropolisLoadout();
    gPlayerStatus.resourceVariant                               = MIST_PARKING_AKROPOLIS_PLAYER_RESOURCE_VARIANT;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = AKROPOLIS_ROOM_ID_PLAZA;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = MIST_PARKING_SHOP_RELOAD_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = MIST_PARKING_SHOP_RELOAD_ROOM;
    gDisplayState.spriteVariant                                 = MIST_PARKING_SHOP_RELOAD_SPRITE_VARIANT;
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, GAME_FLOW_LOAD_CAPTION_NORMAL);
}

/// Starts animation-driven player head aiming for the variant-1 shop talk.
static void _mistParkingStartShopPlayerHeadAim(void)
{
    enum {
        MIST_PARKING_SHOP_HEAD_AIM_DESCRIPTOR = 1,
    };
    gMistParkingShopPlayerHeadAimHandle.task = taskSpawnFromTable(gMistParkingShopTaskDescriptors, MIST_PARKING_SHOP_HEAD_AIM_DESCRIPTOR, MIST_PARKING_HEAD_AIM_FOLLOW_ANIMATION, 0);
}

/// Controls the existing head-aim task used by the variant-1 conversations.
static void _mistParkingControlShopPlayerHeadAim(s32 mode)
{
    Task* task = gMistParkingShopPlayerHeadAimHandle.task;

    if (task == NULL) {
        return;
    }
    switch (mode) {
        case MIST_PARKING_HEAD_AIM_FOLLOW_ANIMATION:
        case MIST_PARKING_HEAD_AIM_FORCE:
            task->spawnArg1.value = mode;
            break;
        default:
            taskKill(gMistParkingShopPlayerHeadAimHandle.task);
            gMistParkingShopPlayerHeadAimHandle.task = NULL;
            break;
    }
}

/// Queues the variant-1 talk's display mode with a callback-tick exit delay.
static void _mistParkingQueueDelayedShopDisplayModeExit(s32 delayTicks)
{
    enum { MIST_PARKING_SHOP_DISPLAY_EXIT_DESCRIPTOR_INDEX = 2 };

    displayQueueModeTask(taskGetDescAt(gMistParkingShopTaskDescriptors, MIST_PARKING_SHOP_DISPLAY_EXIT_DESCRIPTOR_INDEX), delayTicks, 0, STAGE_ENTRY_RELOAD);
}

#include "mist_parking_display_mode_exit.inc.c"

/// Counts down callback ticks before releasing the variant-1 talk's display mode.
static void _mistParkingDelayShopDisplayModeExitTask(Task* task)
{
    _mistParkingDelayDisplayModeExit(task);
}

/// Selects already-loaded CAP data and its font page for the variant-1 conversations.
static void _mistParkingSelectShopDialogueResource(s32 resourceOrdinal)
{
    enum {
        MIST_PARKING_SHOP_MENU_FONT_VRAM_X  = 320,
        MIST_PARKING_SHOP_MENU_FONT_VRAM_Y  = 256,
        MIST_PARKING_SHOP_PRIZE_FONT_VRAM_X = 704,
        MIST_PARKING_SHOP_PRIZE_FONT_VRAM_Y = 0
    };

    capReset();
    switch (resourceOrdinal) {
        case MIST_PARKING_SHOP_DIALOGUE_MENU:
            gCapFile = NULL;
            capSelectLoadedFile(MIST_PARKING_SHOP_DIALOGUE_MENU);
            capSetTexturePage(MIST_PARKING_SHOP_MENU_FONT_VRAM_X, MIST_PARKING_SHOP_MENU_FONT_VRAM_Y);
            break;
        case MIST_PARKING_SHOP_DIALOGUE_PRIZES:
            gCapFile = NULL;
            capSelectLoadedFile(MIST_PARKING_SHOP_DIALOGUE_PRIZES);
            capSetTexturePage(MIST_PARKING_SHOP_PRIZE_FONT_VRAM_X, MIST_PARKING_SHOP_PRIZE_FONT_VRAM_Y);
            break;
    }
}

void mistParkingForgetShopHeadAimTaskHandle(s32 unused)
{
    gMistParkingShopPlayerHeadAimHandle.task = NULL;
}
