#include "mist_parking_private.h"

#include "../../room_ids.h"

#include "types.h"

#include "rooms/mist_parking.h"

#include "gameplay/actor_presentation.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/companion_load.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/starter_inventory.h"

#include "main/coord.h"
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
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "rooms/room_common.h"

#define ROOM_PART declarations_mist_parking_conversation
#include "assets/rooms/mist/parking/declarations.inc"

static void _modelPlacementAttachPartTask(Task* childTask);
static void _mistParkingWaitOptionDialogAnswer(Task* task);

static void _mistParkingIdleAttachedModelState(Task* unusedTask);
static void _mistParkingOpenOptionDialog(Task* task);
static void _mistParkingExitOptionDialogTask(Task* task);

/// Unreferenced three-state recipe for attaching a child model to a parent part.
static const TaskFuncTable3 _gMistParkingAttachedModelStates = {
    {
        _modelPlacementAttachPartTask,
        _mistParkingIdleAttachedModelState,
        taskKill,
    },
};
/// Open, wait-for-answer and exit callbacks indexed by the option task's state (0..2).
static const TaskFuncTable3 _gMistParkingOptionDialogStates = {
    {
        _mistParkingOpenOptionDialog,
        _mistParkingWaitOptionDialogAnswer,
        _mistParkingExitOptionDialogTask,
    },
};

static void _mistParkingJodiePrizeAndShopTalkTask(Task* task);
static void _mistParkingDepartureMenuTask(Task* task);
static void _mistParkingStartPackedDialogue(s32 packedCommandAndVariant);
static void _mistParkingStartDepartureMovie(s32 movieSelector);
static void _mistParkingReloadAkropolisArea(s32 areaId);
static void _mistParkingSpawnCutsceneModel(s32 descriptorIndex);
static void _mistParkingReleaseCutsceneModel(s32 descriptorIndex);

/// Descriptor selected by the arrival conversation's model-release callback.
enum { MIST_PARKING_CUTSCENE_MODEL_DESCRIPTOR_INDEX = 0 };
static void _mistParkingOptionDialogTask(Task* task);
static void _mistParkingContinueShopChoiceTask(Task* task);
static void _mistParkingQueueDryfieldDeparture(void);
static void _mistParkingDepartForDryfieldTask(Task* task);
static void _mistParkingStartPlayerHeadAim(void);

/// CAP slots and variant keys packed into the room's dialogue callback words.
enum {
    MIST_PARKING_CAP_COMMAND_SHIFT = 16,
};

/// Resource and entrance selectors installed for a departure reload.
enum {
    MIST_PARKING_RELOAD_WARP           = 1,
    MIST_PARKING_RELOAD_ROOM           = 1,
    MIST_PARKING_RELOAD_SPRITE_VARIANT = 1
};

/// The five course prizes share consecutive saved flags and placed-item states.
enum {
    MIST_PARKING_PRIZE_FLAG_FIRST       = EVENT_FLAG_MIST_SHOOTING_GALLERY_PRIZE_FIRST,
    MIST_PARKING_PRIZE_WAITING          = 2,
    MIST_PARKING_PRIZE_CLOSED           = 3,
    MIST_PARKING_PRIZE_OBJECT_FIRST     = 0x20,
    MIST_PARKING_PRIZE_OBJECT_AVAILABLE = 1,
    MIST_PARKING_PRIZE_OBJECT_COLLECTED = 2,
    MIST_PARKING_PRIZE_CAPTION_SLOT     = 5
};

/// Clip indices and actor actions used by the parking conversation requests.
enum {
    MIST_PARKING_CONVERSATION_TASK_PRIORITY    = 192,
    MIST_PARKING_PLAYER_HEAD_AIM_TASK_PRIORITY = 97,
    MIST_PARKING_PIERCE_COMMAND_AIM_HEAD       = 2,
    MIST_PARKING_PIERCE_COMMAND_RELEASE_HEAD   = 3,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF = 0,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON  = 1,
};

/// Script targets, CAP slots, view selectors and the room's MIST stock profile.
enum {
    MIST_PARKING_JODIE_SCENE_PLACEMENT   = 1,
    MIST_PARKING_SHOP_STOCK_PROFILE      = 0x10,
    MIST_PARKING_SHOP_POSE_DELAY_UPDATES = 20,
    MIST_PARKING_SHOP_REPLY_END_UPDATES  = 30,
};

/// Catalog ids and quantities awarded for the five shooting-gallery courses.
enum {
    MIST_PARKING_COURSE_PRIZE_9MM_HYDRA        = 0xA1,
    MIST_PARKING_COURSE_PRIZE_RINGERS_SOLUTION = 0x3D,
    MIST_PARKING_COURSE_PRIZE_HUNTER_GOGGLES   = 0x3F,
    MIST_PARKING_COURSE_PRIZE_LIPSTICK         = 0x0B,
    MIST_PARKING_COURSE_PRIZE_SHOULDER_HOLSTER = 0x6C,
    MIST_PARKING_COURSE_PRIZE_AMMO_ROUNDS      = 50,
    MIST_PARKING_COURSE_PRIZE_ONE_ITEM         = 1
};

TaskDesc gMistParkingConversationTaskDescs[9] = {
    { { { TASK_BODY_TMD, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, mistParkingCutsceneModelPitchTask, { .model = &gMistParkingHingedPanelModel } },
    { { { TASK_BODY_NONE, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, _mistParkingOptionDialogTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, mistParkingContinueDepartureChoiceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, _mistParkingDepartForDryfieldTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_PLAYER_HEAD_AIM_TASK_PRIORITY } }, mistParkingAimPlayerHeadAtTalkPartnerTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, mistParkingDelayDisplayModeExitTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, _mistParkingContinueShopChoiceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, _mistParkingJodiePrizeAndShopTalkTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, _mistParkingDepartureMenuTask, { .value = 0 } },
};

/// Player-bank extension for the arrival, follow-up, departure and Jodie shop scripts.
static AnimationSet* _gMistParkingPlayerConversationAnimationSets[25] = {
    NULL,
    &gAyaStandClip,
    &gShelterR47PlayerLookAtMapClip,
    &gShelterB3IncineratorControlRoomPlayerFlusteredClip,
    &gMistParkingPlayerConversationAnim04Clip,
    &gMistParkingPlayerConversationAnim05Clip,
    &gMistParkingPlayerConversationAnim06Clip,
    &gMistParkingPlayerConversationAnim07Clip,
    &gMistParkingPlayerConversationAnim08Clip,
    &gMistParkingPlayerConversationAnim09Clip,
    &gMistParkingPlayerConversationAnim10Clip,
    &gAyaAnim07Clip,
    &gAyaTalkClip,
    &gShelterB6GrowthRoomPlayerChoiceStartClip,
    &gAyaItsNothingClip,
    &gMistParkingPlayerConversationAnim15Clip,
    &gAyaOnMyWayClip,
    &gAyaTalkStartClip,
    &gAyaTalkEndClip,
    &gAyaReplyClip,
    &gMistParkingPlayerConversationAnim20Clip,
    &gMistParkingPlayerConversationAnim21Clip,
    &gAyaTurnPaleClip,
    &gShelterB1PodServiceGantryPlayerGantryScene7Clip,
    &gAyaIntroduceHerselfClip,
};

#define ROOM_PART gMistParkingPlayerConversationBankCopy
#include "assets/rooms/mist/parking/events.inc"
s8 gMistParkingPlayerAnimationHeadAimFlags[28] = {
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
};

#define ROOM_PART _gMistParkingPlayerArrivalAndDepartureTransform
#include "assets/rooms/mist/parking/events.inc"
/// Null-terminated Shift-JIS option label "まだよ (not yet)"; eight bytes of storage.
static u8 _gMistParkingOptionNotYetLabel[8] = "\x82\xdc\x82\xbe\x82\xe6";

/// Null-terminated Shift-JIS option label "ええ (yes)"; eight bytes of storage.
static u8 _gMistParkingOptionEeLabel[8] = "\x82\xa6\x82\xa6";

/// Null-terminated Shift-JIS option label "はい (yes)"; eight bytes of storage.
static u8 _gMistParkingOptionHaiLabel[8] = "\x82\xcd\x82\xa2";

/// Null-terminated Shift-JIS option label "いいえ (no)"; eight bytes of storage.
static u8 _gMistParkingOptionNoLabel[8] = "\x82\xa2\x82\xa2\x82\xa6";

/// Two ordered label pairs borrowed by the modal option dialog.
static u8* _gMistParkingOptionLabelPairs[2][2] = {
    { _gMistParkingOptionNotYetLabel, _gMistParkingOptionEeLabel },
    { _gMistParkingOptionHaiLabel, _gMistParkingOptionNoLabel },
};

#define ROOM_PART gMistParkingArrivalConversationScript
#include "assets/rooms/mist/parking/events.inc"
/// Item ids for shooting-gallery courses 0..4, paired with their quantities.
static s32 _gMistParkingCoursePrizeItemIds[5] = {
    MIST_PARKING_COURSE_PRIZE_9MM_HYDRA,
    MIST_PARKING_COURSE_PRIZE_RINGERS_SOLUTION,
    MIST_PARKING_COURSE_PRIZE_HUNTER_GOGGLES,
    MIST_PARKING_COURSE_PRIZE_LIPSTICK,
    MIST_PARKING_COURSE_PRIZE_SHOULDER_HOLSTER,
};

/// Item units per course prize: fifty Hydra rounds, then one of each item.
static s32 _gMistParkingCoursePrizeQuantities[ARRAY_SIZE(_gMistParkingCoursePrizeItemIds)] = {
    MIST_PARKING_COURSE_PRIZE_AMMO_ROUNDS,
    MIST_PARKING_COURSE_PRIZE_ONE_ITEM,
    MIST_PARKING_COURSE_PRIZE_ONE_ITEM,
    MIST_PARKING_COURSE_PRIZE_ONE_ITEM,
    MIST_PARKING_COURSE_PRIZE_ONE_ITEM,
};

/// Advances one course prize's announcement while CAP is idle.
static inline void _mistParkingAdvancePrizeAnnouncement(Task* task, MistParkingPrizeAnnouncementState* announcement)
{
    s16 announcementCourse;

    if (capIsBusy() != 0) {
        return;
    }
    announcement->prizeTimer--;
    if (announcement->prizeTimer == MIST_PARKING_PRIZE_ANNOUNCEMENT_CAPTION_FRAME) {
        announcementCourse = announcement->prizeIndex;
        if (gameFlagGetNibble(announcementCourse + MIST_PARKING_PRIZE_FLAG_FIRST) == MIST_PARKING_PRIZE_WAITING) {
            capStartSequenceSlot(MIST_PARKING_PRIZE_CAPTION_SLOT, CAP_PLAYBACK_IN_PLACE, announcementCourse);
        }
        return;
    }
    if (announcement->prizeTimer != 0) {
        return;
    }
    announcementCourse = announcement->prizeIndex;
    if (areaGetCurrentObjectState(announcementCourse + MIST_PARKING_PRIZE_OBJECT_FIRST) != MIST_PARKING_PRIZE_OBJECT_AVAILABLE) {
        gameFlagSetNibble(announcementCourse + MIST_PARKING_PRIZE_FLAG_FIRST, MIST_PARKING_PRIZE_CLOSED);
    }
    announcement->prizeTimer = MIST_PARKING_PRIZE_ANNOUNCEMENT_FRAMES;
    announcement->prizeIndex++;
    if (announcement->prizeIndex >= (s32)ARRAY_SIZE(_gMistParkingCoursePrizeItemIds)) {
        task->state++;
    }
}

/// Runs Jodie's prize collection and shop conversation outside area variant 1.
static void _mistParkingJodiePrizeAndShopTalkTask(Task* task)
{
    enum {
        MIST_PARKING_JODIE_TALK_GREET               = 0,
        MIST_PARKING_JODIE_TALK_CHECK_PRIZES        = 1,
        MIST_PARKING_JODIE_TALK_START_PRIZE_PROMPT  = 2,
        MIST_PARKING_JODIE_TALK_PRIZE_CHOICE        = 3,
        MIST_PARKING_JODIE_TALK_ANNOUNCE_PRIZES     = 4,
        MIST_PARKING_JODIE_TALK_CLOSE_ANNOUNCEMENTS = 5,
        MIST_PARKING_JODIE_TALK_WAIT_PRIZE_CLOSING  = 6,
        MIST_PARKING_JODIE_TALK_DELAY_MENU          = 7,
        MIST_PARKING_JODIE_TALK_SHOP_CHOICE         = 8,
        MIST_PARKING_JODIE_TALK_WAIT_SHOP_EXIT      = 9,
        MIST_PARKING_JODIE_TALK_FINISH              = 10,
        MIST_PARKING_JODIE_CHOICE_ANNOUNCE_PRIZES   = 1,
        MIST_PARKING_JODIE_CHOICE_STORE_PRIZES      = 3,
        MIST_PARKING_JODIE_CHOICE_CLOSE_PRIZES      = 4,
        MIST_PARKING_JODIE_CHOICE_OPEN_SHOP         = 1,
        MIST_PARKING_JODIE_CAP_GREETING             = 2,
        MIST_PARKING_JODIE_CAP_MENU                 = 4,
        MIST_PARKING_JODIE_CAP_FAREWELL             = 3,
        MIST_PARKING_PRIZE_CAP_PROMPT               = 6,
        MIST_PARKING_PRIZE_CAP_BEGIN_ANNOUNCEMENTS  = 7,
        MIST_PARKING_PRIZE_CAP_CLOSE_WITHOUT_ITEMS  = 8,
        MIST_PARKING_PRIZE_CAP_ITEMS_STORED         = 4,
        MIST_PARKING_PRIZE_CAP_END_ANNOUNCEMENTS    = 2,
        MIST_PARKING_PRIZE_COUNT                    = ARRAY_SIZE(_gMistParkingCoursePrizeItemIds),
        MIST_PARKING_PRIZE_STORAGE_RANGE            = 3,
        MIST_PARKING_PRIZE_SHOULDER_HOLSTER_ITEM_ID = 0x6C,
        MIST_PARKING_PRIZE_ARMOR_QUANTITY           = 1,
        MIST_PARKING_JODIE_MENU_DELAY_TICKS         = 11,
        MIST_PARKING_JODIE_FAREWELL_TICK            = 10
    };
    s32                                courseIndex;
    s32                                prizeFlag;
    MistParkingPrizeAnnouncementState* announcement = &gMistParkingPrizeAnnouncement;

    switch (task->state) {
        case MIST_PARKING_JODIE_TALK_GREET:
            memFillBytes(announcement, 0, sizeof(*announcement));
            evsStartScript(_gMistParkingJodieTalkPoseScript, EVENT_SCRIPT_HUD_KEEP);
            capRunCommand(MIST_PARKING_JODIE_CAP_GREETING, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case MIST_PARKING_JODIE_TALK_CHECK_PRIZES:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            for (courseIndex = 0; courseIndex < MIST_PARKING_PRIZE_COUNT; courseIndex++) {
                if (gameFlagGetNibble(courseIndex + MIST_PARKING_PRIZE_FLAG_FIRST) == MIST_PARKING_PRIZE_WAITING) {
                    task->state = MIST_PARKING_JODIE_TALK_START_PRIZE_PROMPT;
                    return;
                }
            }
            task->state = MIST_PARKING_JODIE_TALK_WAIT_PRIZE_CLOSING;
            break;
        case MIST_PARKING_JODIE_TALK_START_PRIZE_PROMPT:
            mistParkingSelectDialogueResource(MIST_PARKING_DIALOGUE_PRIZES);
            evsStartScript(_gMistParkingJodieTalkPoseScript, EVENT_SCRIPT_HUD_KEEP);
            capRunCommand(MIST_PARKING_PRIZE_CAP_PROMPT, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case MIST_PARKING_JODIE_TALK_PRIZE_CHOICE:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            switch (capGetVariantKey()) {
                case MIST_PARKING_JODIE_CHOICE_ANNOUNCE_PRIZES:
                    announcement->prizeTimer = MIST_PARKING_PRIZE_ANNOUNCEMENT_FRAMES;
                    capRunCommand(MIST_PARKING_PRIZE_CAP_BEGIN_ANNOUNCEMENTS, CAP_PLAYBACK_IN_PLACE);
                    task->state = MIST_PARKING_JODIE_TALK_ANNOUNCE_PRIZES;
                    break;
                case MIST_PARKING_JODIE_CHOICE_CLOSE_PRIZES:
                    for (courseIndex = 0; courseIndex < MIST_PARKING_PRIZE_COUNT; courseIndex++) {
                        prizeFlag = courseIndex + MIST_PARKING_PRIZE_FLAG_FIRST;
                        if (gameFlagGetNibble(prizeFlag) == MIST_PARKING_PRIZE_WAITING) {
                            gameFlagSetNibble(prizeFlag, MIST_PARKING_PRIZE_CLOSED);
                        }
                    }
                    evsStartScript(_gMistParkingJodieTalkPoseScript, EVENT_SCRIPT_HUD_KEEP);
                    capRunCommand(MIST_PARKING_PRIZE_CAP_CLOSE_WITHOUT_ITEMS, CAP_PLAYBACK_IN_PLACE);
                    task->state = MIST_PARKING_JODIE_TALK_WAIT_PRIZE_CLOSING;
                    break;
                case MIST_PARKING_JODIE_CHOICE_STORE_PRIZES:
                    // The final armor prize also checks its global ownership limit.
                    for (courseIndex = 0; courseIndex < MIST_PARKING_PRIZE_COUNT - 1; courseIndex++) {
                        prizeFlag = courseIndex + MIST_PARKING_PRIZE_FLAG_FIRST;
                        if (gameFlagGetNibble(prizeFlag) == MIST_PARKING_PRIZE_WAITING && inventoryGiveItem(gInventoryContainerRanges[MIST_PARKING_PRIZE_STORAGE_RANGE], _gMistParkingCoursePrizeItemIds[courseIndex], _gMistParkingCoursePrizeQuantities[courseIndex]) != 0) {
                            gameFlagSetNibble(prizeFlag, MIST_PARKING_PRIZE_CLOSED);
                            areaSetCurrentObjectState(courseIndex + MIST_PARKING_PRIZE_OBJECT_FIRST, MIST_PARKING_PRIZE_OBJECT_COLLECTED);
                        }
                    }
                    if (gameFlagGetNibble(EVENT_FLAG_MIST_SHOOTING_GALLERY_PRIZE_LAST) == MIST_PARKING_PRIZE_WAITING && inventoryIsItemLimitReached(MIST_PARKING_PRIZE_SHOULDER_HOLSTER_ITEM_ID) == 0) {
                        if (inventoryGiveItem(gInventoryContainerRanges[MIST_PARKING_PRIZE_STORAGE_RANGE], MIST_PARKING_PRIZE_SHOULDER_HOLSTER_ITEM_ID, MIST_PARKING_PRIZE_ARMOR_QUANTITY) != 0) {
                            gameFlagSetNibble(EVENT_FLAG_MIST_SHOOTING_GALLERY_PRIZE_LAST, MIST_PARKING_PRIZE_CLOSED);
                            areaSetCurrentObjectState(MIST_PARKING_PRIZE_OBJECT_FIRST + MIST_PARKING_PRIZE_COUNT - 1, MIST_PARKING_PRIZE_OBJECT_COLLECTED);
                        }
                    }
                    evsStartScript(_gMistParkingJodieTalkPoseScript, EVENT_SCRIPT_HUD_KEEP);
                    capRunCommand(MIST_PARKING_PRIZE_CAP_ITEMS_STORED, CAP_PLAYBACK_IN_PLACE);
                    task->state = MIST_PARKING_JODIE_TALK_WAIT_PRIZE_CLOSING;
                    break;
            }
            break;
        case MIST_PARKING_JODIE_TALK_ANNOUNCE_PRIZES:
            _mistParkingAdvancePrizeAnnouncement(task, announcement);
            break;
        case MIST_PARKING_JODIE_TALK_CLOSE_ANNOUNCEMENTS:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() == 0) {
                evsStartScript(_gMistParkingJodieTalkPoseScript, EVENT_SCRIPT_HUD_KEEP);
                capRunCommand(MIST_PARKING_PRIZE_CAP_END_ANNOUNCEMENTS, CAP_PLAYBACK_IN_PLACE);
                task->state++;
            }
            // Keep polling the closing script on the same callback tick.
            /* fallthrough */
        case MIST_PARKING_JODIE_TALK_WAIT_PRIZE_CLOSING:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            task->state++;
            break;
        case MIST_PARKING_JODIE_TALK_DELAY_MENU:
            task->killCountdown++;
            if (task->killCountdown >= MIST_PARKING_JODIE_MENU_DELAY_TICKS) {
                mistParkingSelectDialogueResource(MIST_PARKING_DIALOGUE_DEFAULT);
                capRunCommand(MIST_PARKING_JODIE_CAP_MENU, CAP_PLAYBACK_IN_PLACE);
                task->killCountdown = 0;
                task->state++;
            }
            break;
        case MIST_PARKING_JODIE_TALK_SHOP_CHOICE:
            if (capIsBusy() != 0) {
                return;
            }
            if (capGetVariantKey() == MIST_PARKING_JODIE_CHOICE_OPEN_SHOP) {
                evsStartScript(_gMistParkingJodieShopSessionScript, EVENT_SCRIPT_HUD_KEEP);
            } else {
                evsStartScript(_gMistParkingJodieShopDeclineScript, EVENT_SCRIPT_HUD_KEEP);
            }
            task->state++;
            break;
        case MIST_PARKING_JODIE_TALK_WAIT_SHOP_EXIT:
            task->killCountdown++;
            if (task->killCountdown == MIST_PARKING_JODIE_FAREWELL_TICK) {
                capRunCommand(MIST_PARKING_JODIE_CAP_FAREWELL, CAP_PLAYBACK_IN_PLACE);
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state++;
            break;
        case MIST_PARKING_JODIE_TALK_FINISH:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskKill(task);
            break;
    }
}

/// Runs the departure menu used after the variant-2 conversation follow-ups.
static void _mistParkingDepartureMenuTask(Task* task)
{
    enum {
        MIST_PARKING_DEPARTURE_START            = 0,
        MIST_PARKING_DEPARTURE_WAIT_PROMPT      = 1,
        MIST_PARKING_DEPARTURE_SELECT           = 2,
        MIST_PARKING_DEPARTURE_WAIT_SCRIPT      = 3,
        MIST_PARKING_DEPARTURE_FINISH           = 4,
        MIST_PARKING_DEPARTURE_STAY             = 4,
        MIST_PARKING_DEPARTURE_DRYFIELD         = 5,
        MIST_PARKING_DEPARTURE_SHOOTING_GALLERY = 6
    };
    s32 choiceKey;

    switch (task->state) {
        case MIST_PARKING_DEPARTURE_START:
            evsStartScript(_gMistParkingDeparturePromptScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case MIST_PARKING_DEPARTURE_WAIT_PROMPT:
        case MIST_PARKING_DEPARTURE_WAIT_SCRIPT:
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state++;
            break;
        case MIST_PARKING_DEPARTURE_SELECT:
            // Retain the choice while its script runs and changes CAP selection.
            choiceKey             = capGetVariantKey();
            task->spawnArg1.value = choiceKey;
            switch (choiceKey) {
                case MIST_PARKING_DEPARTURE_STAY:
                    evsStartScript(_gMistParkingStayReplyScript, EVENT_SCRIPT_HUD_KEEP);
                    break;
                case MIST_PARKING_DEPARTURE_DRYFIELD:
                    evsStartScript(_gMistParkingDryfieldDepartureScript, EVENT_SCRIPT_HUD_KEEP);
                    break;
                case MIST_PARKING_DEPARTURE_SHOOTING_GALLERY:
                    evsStartScript(_gMistParkingShootingGalleryDepartureScript, EVENT_SCRIPT_HUD_KEEP);
                    break;
            }
            task->state++;
            break;
        case MIST_PARKING_DEPARTURE_FINISH:
            if (task->spawnArg1.value == MIST_PARKING_DEPARTURE_STAY) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            }
            taskKill(task);
            break;
    }
}

#include "../../../../shared/actor/model_placement_attach_part.inc.c"

/// Keeps an attached model task idle between attachment and teardown.
static void _mistParkingIdleAttachedModelState(Task* unusedTask)
{
}

/// Starts in-place CAP playback from a packed command slot and variant key.
static void _mistParkingStartPackedDialogue(s32 packedCommandAndVariant)
{
    capStartSequenceSlot(packedCommandAndVariant >> MIST_PARKING_CAP_COMMAND_SHIFT, CAP_PLAYBACK_IN_PLACE, packedCommandAndVariant);
}

/// Queues the departure movie launcher and freezes room objects for the transition.
static void _mistParkingStartDepartureMovie(s32 movieSelector)
{
    enum { MIST_PARKING_DEPARTURE_MOVIE_LAUNCHER_DESCRIPTOR = 0 };

    taskSpawnFromTable(gMistParkingDepartureMovieTaskDescs, MIST_PARKING_DEPARTURE_MOVIE_LAUNCHER_DESCRIPTOR, movieSelector, 0);
    gGameSession->freezeRoomObjs = 1;
}

/// Installs an Akropolis destination and queues a reload capturing the current frame.
static void _mistParkingReloadAkropolisArea(s32 areaId)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = MIST_PARKING_RELOAD_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = MIST_PARKING_RELOAD_ROOM;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = areaId;
    gDisplayState.spriteVariant                                 = MIST_PARKING_RELOAD_SPRITE_VARIANT;
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    if (areaId == AKROPOLIS_ROOM_ID_PLAZA) {
        gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, GAME_FLOW_LOAD_CAPTION_NORMAL);
    }
}

/// Shows the arrival conversation's model task if the room holds none.
static void _mistParkingSpawnCutsceneModel(s32 descriptorIndex)
{
    Task**     modelTaskHandle = descriptorIndex == MIST_PARKING_CUTSCENE_MODEL_DESCRIPTOR_INDEX ? &gMistParkingCutsceneModelTask : NULL;
    Task*      modelTask;
    TmdObject* model;

    if (modelTaskHandle != NULL && *modelTaskHandle == NULL) {
        modelTask        = taskSpawnFromTable(gMistParkingConversationTaskDescs, descriptorIndex, 0, 0);
        *modelTaskHandle = modelTask;
        if (modelTask != NULL) {
            model         = modelTask->extra.tmd;
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
}

/// Releases the arrival conversation's model task and forgets its handle.
static void _mistParkingReleaseCutsceneModel(s32 descriptorIndex)
{
    if (descriptorIndex == MIST_PARKING_CUTSCENE_MODEL_DESCRIPTOR_INDEX) {
        if (gMistParkingCutsceneModelTask != NULL) {
            taskKill(gMistParkingCutsceneModelTask);
        }
        gMistParkingCutsceneModelTask = NULL;
    }
}

/// Runs a modal two-option choice and forwards its one-based answer.
static void _mistParkingOptionDialogTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gMistParkingOptionDialogStates;
    stateHandlers.funcs[task->state](task);
}

/// Allocates and opens the room's two-option dialog with cancellation disabled.
static void _mistParkingOpenOptionDialog(Task* task)
{
    enum { MIST_PARKING_OPTION_ALTERNATE_LABEL_PAIR = 1 };
    RoomOptionDialog* dialog;
    UiDialogOption*   option;
    u8**              labelCursor;
    s32               alternatePairSelector;
    s32               optionIndex;

    dialog = memCalloc(sizeof(RoomOptionDialog), 0);
    if (dialog == NULL) {
        taskKill(task);
        return;
    }

    option                = dialog->options;
    optionIndex           = 0;
    alternatePairSelector = MIST_PARKING_OPTION_ALTERNATE_LABEL_PAIR;
    labelCursor           = _gMistParkingOptionLabelPairs[0];
    task->work            = dialog;
    task->exitCallback    = _mistParkingExitOptionDialogTask;

    // Link both labelled options before the UI borrows the dialog.
    for (; optionIndex < ARRAY_SIZE(dialog->options); optionIndex++) {
        if (task->spawnArg1.value == alternatePairSelector) {
            option->text = _gMistParkingOptionLabelPairs[alternatePairSelector][optionIndex];
        } else {
            option->text = *labelCursor;
        }
        option->next = option + 1;
        option++;
        labelCursor++;
    }
    option[-1].next = NULL;

    dialog->request.optionCount = ARRAY_SIZE(dialog->options);
    dialog->request.options     = dialog->options;
    dialog->request.title       = NULL;
    dialog->request.flags       = 0;
    uiSpawnOptionDialog(&dialog->request, 0, 0, 0);
    task->state++;
}

/// Publishes the dialog's one-based option number and advances to exit.
static void _mistParkingWaitOptionDialogAnswer(Task* task)
{
    const RoomOptionDialog* dialog;
    s16                     choiceResult;

    dialog       = task->work;
    choiceResult = dialog->request.result;
    if (choiceResult != USER_INTERFACE_RESULT_NONE) {
        s32* choiceDestination = task->spawnArg2.pointer;

        *choiceDestination = choiceResult;
        task->state        = task->state + 1;
    }
}

/// Releases the room's option-dialog request task and requests modal exit.
static void _mistParkingExitOptionDialogTask(Task* task)
{
    taskKill(task);
    stageRequestModeTaskExit();
}

void mistParkingContinueDepartureChoiceTask(Task* task)
{
    enum { MIST_PARKING_DEPARTURE_CHOICE_LEAVE = 2 };

    if (gGameSession->eventState == 0 && capIsBusy() == 0) {
        if (gMistParkingDialogChoice == MIST_PARKING_DEPARTURE_CHOICE_LEAVE) {
            evsStartScript(_gMistParkingDryfieldDepartureScript, EVENT_SCRIPT_HUD_KEEP);
        } else {
            evsStartScript(_gMistParkingStayReplyScript, EVENT_SCRIPT_HUD_KEEP);
        }
        taskKill(task);
    }
}

/// Starts or skips the shop once the room's current EVS and CAP have finished.
static void _mistParkingContinueShopChoiceTask(Task* task)
{
    enum { MIST_PARKING_SHOP_CHOICE_SKIP = 2 };

    if (gGameSession->eventState == 0 && capIsBusy() == 0) {
        if (gMistParkingDialogChoice == MIST_PARKING_SHOP_CHOICE_SKIP) {
            evsStartScript(_gMistParkingJodieShopDeclineScript, EVENT_SCRIPT_HUD_KEEP);
        } else {
            evsStartScript(_gMistParkingJodieShopSessionScript, EVENT_SCRIPT_HUD_KEEP);
        }
        taskKill(task);
    }
}

/// Queues the task that prepares the field loadout and departs for Dryfield.
static void _mistParkingQueueDryfieldDeparture(void)
{
    enum { MIST_PARKING_DRYFIELD_DEPARTURE_DESCRIPTOR = 3 };

    taskSpawnFromTable(gMistParkingConversationTaskDescs, MIST_PARKING_DRYFIELD_DEPARTURE_DESCRIPTOR, 0, 0);
}

/// Prepares the field loadout and queues departure to Dryfield's gas station.
static void _mistParkingDepartForDryfieldTask(Task* task)
{
    playerActorPrepareDryfieldLoadout();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_DRYFIELD;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_DRYFIELD_GAS_STATION;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = MIST_PARKING_RELOAD_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = MIST_PARKING_RELOAD_ROOM;
    gDisplayState.spriteVariant                                 = MIST_PARKING_RELOAD_SPRITE_VARIANT;
    gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, GAME_FLOW_LOAD_CAPTION_ALTERNATE);
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    taskKill(task);
}

/// Starts animation-controlled player head aiming toward the room's talk partner.
static void _mistParkingStartPlayerHeadAim(void)
{
    enum { MIST_PARKING_PLAYER_HEAD_AIM_DESCRIPTOR = 4 };

    gMistParkingPlayerHeadAimTask = taskSpawnFromTable(gMistParkingConversationTaskDescs, MIST_PARKING_PLAYER_HEAD_AIM_DESCRIPTOR, MIST_PARKING_HEAD_AIM_FOLLOW_ANIMATION, 0);
}
