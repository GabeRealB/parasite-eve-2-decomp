#ifndef STAGES_AKROPOLIS_ROOMS_MIST_PARKING_MIST_PARKING_PRIVATE_H
#define STAGES_AKROPOLIS_ROOMS_MIST_PARKING_MIST_PARKING_PRIVATE_H

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/inventory.h"
#include "gameplay/message.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

/// Timing of one prize's turn in a parking-lot talk's prize announcements, counted down in `prizeTimer`.
enum {
    MIST_PARKING_PRIZE_ANNOUNCEMENT_FRAMES        = 10, // Idle frames each prize's turn lasts
    MIST_PARKING_PRIZE_ANNOUNCEMENT_CAPTION_FRAME = 5   // Frames left when a waiting prize's caption starts
};

/// Progress through the prize announcements of the parking-lot talk that runs in every area variant but 1.
typedef struct {
    s16 prizeTimer; // Frames left on the current prize's turn
    s16 prizeIndex; // Shooting-gallery course whose prize has the turn (0..4; 5 once all have had one)
} MistParkingPrizeAnnouncementState;
STATIC_ASSERT_SIZEOF(MistParkingPrizeAnnouncementState, 4);

/// The room's hold on the task that turns the player's head toward the index-0 placement for this stage and area.
typedef struct {
    Task* task;       // Head-aim task, or NULL when the room holds none
    u8    field_4[4]; // Unreferenced zeros; role unproven
} MistParkingHeadAimHandle;
STATIC_ASSERT_SIZEOF(MistParkingHeadAimHandle, 8);

/// Work of the talk at the parking-lot shop while the area is in variant 1.
typedef struct {
    s16 prizeTimer;          // Frames left on the current prize (10 each; a waiting prize's caption starts at 5)
    s16 prizeIndex;          // Shooting-gallery course whose prize is being announced (0..4)
    s16 businessDone;        // Nonzero once prizes came up or a menu choice other than leaving ran; picks the farewell
    s16 prizeClosingCommand; // CAP command closing the prize exchange (2 after the announcements, 3 when skipped)
} MistParkingShopTalkState;
STATIC_ASSERT_SIZEOF(MistParkingShopTalkState, 8);

/// Nine indexed task recipes for the parking conversations and their presentation.
extern TaskDesc gMistParkingConversationTaskDescs[9];

/// Two bodyless priority-192 recipes for departure-movie launch and display playback.
extern TaskDesc gMistParkingDepartureMovieTaskDescs[2];

/// Bodyless variant-1 tasks: departure, player head aim, display exit, Jodie talk and menu.
extern TaskDesc gMistParkingShopTaskDescriptors[5];

/// Tasks the room keeps a handle on while they run.
extern Task* gRoomCutsceneSoundTask;

/// Held arrival-conversation model task, or NULL when absent or allocation failed.
extern Task* gMistParkingCutsceneModelTask;

/// Held general-conversation player head-aim task, or NULL when none is held.
extern Task* gMistParkingPlayerHeadAimTask;

extern TmdSource gMistParkingHingedPanelModel;

extern AnimationSet gAyaStandClip;

extern AnimationSet gShelterR47PlayerLookAtMapClip;

extern AnimationSet gShelterB3IncineratorControlRoomPlayerFlusteredClip;

extern AnimationSet gMistParkingPlayerConversationAnim04Clip;

extern AnimationSet gMistParkingPlayerConversationAnim05Clip;

extern AnimationSet gMistParkingPlayerConversationAnim06Clip;

extern AnimationSet gMistParkingPlayerConversationAnim07Clip;

extern AnimationSet gMistParkingPlayerConversationAnim08Clip;

extern AnimationSet gMistParkingPlayerConversationAnim09Clip;

extern AnimationSet gMistParkingPlayerConversationAnim10Clip;

extern AnimationSet gAyaIntroduceHerselfClip;

extern AnimationSet gAyaAnim07Clip;

extern AnimationSet gAyaTalkClip;

extern AnimationSet gShelterB6GrowthRoomPlayerChoiceStartClip;

extern AnimationSet gAyaItsNothingClip;

extern AnimationSet gMistParkingPlayerConversationAnim15Clip;

extern AnimationSet gAyaOnMyWayClip;

extern AnimationSet gAyaTalkStartClip;

extern AnimationSet gAyaTalkEndClip;

extern AnimationSet gAyaReplyClip;

extern AnimationSet gMistParkingPlayerConversationAnim20Clip;

extern AnimationSet gMistParkingPlayerConversationAnim21Clip;

extern AnimationSet gAyaTurnPaleClip;

extern AnimationSet gShelterB1PodServiceGantryPlayerGantryScene7Clip;

/// Copies the room's 25 pointer words into the equipped player's animation-bank extension.
extern AnimationBankCopyRequest gMistParkingPlayerConversationBankCopy;

/// Animation-controlled player head aim, indexed by set index minus the 47 base sets.
extern s8 gMistParkingPlayerAnimationHeadAimFlags[28];

/// Skippable arrival conversation between the player and Pierce in area variant 2.
extern EvsCommand gMistParkingArrivalConversationScript[155];

/// Finishes arrival setup when the player skips the conversation.
extern EvsCommand gMistParkingArrivalSkipScript[23];

/// Places Pierce and Jodie for a non-warp-3 entry while the intro is pending.
extern EvsCommand gMistParkingArrivalRoomEntryScript[8];

/// Plays Pierce's first post-arrival conversation before the departure menu.
extern EvsCommand gMistParkingFirstFollowupScript[10];

/// Plays Pierce's second post-arrival conversation before the departure menu.
extern EvsCommand gMistParkingSecondFollowupScript[10];

/// Authored collision patch restored into the live room grid with Pierce's visibility.
extern WorldCollisionGrid gMistParkingPierceCollisionPatch;

extern AnimationSet gMistParkingStandCopyClip;

extern AnimationSet gMistParkingTalkStartCopyClip;

extern AnimationSet gMistParkingPlayerConversationAnim21CopyClip;

/// Installs the variant-1 player's four-word shop/departure animation extension.
extern AnimationBankCopyRequest gMistParkingShopPlayerAnimationBankCopy;

/// Player pose shared by Jodie's variant-1 shop talks, restarting extension clip 48.
extern AnimationPlayRequest gMistParkingPlayerShopTalkAnimation;

/// Jodie's resting pose before and after the variant-1 greeting, in bank 0, clip 1.
extern AnimationPlayRequest gMistParkingJodieIdleAnimation;

/// Jodie's closing pose after shop use or leaving, in bank 0, clip 5.
extern AnimationPlayRequest gMistParkingJodieShopExitAnimation;

/// Jodie's greeting/prize-talk pose, restarting bank 0, clip 7 after the initial delay.
extern AnimationPlayRequest gMistParkingJodieGreetingAnimation;

/// Jodie's first reply pose for the variant-1 shop menu, in bank 0, clip 8.
extern AnimationPlayRequest gMistParkingJodieReplyAnimation;

/// Jodie's pose at the reply's CAP cue, in bank 0, clip 9 with a fifteen-frame blend.
extern AnimationPlayRequest gMistParkingJodieReplyEndAnimation;

/// Player/Jodie pose sequence shared by variant-1 greeting and prize exchanges.
extern EvsCommand gMistParkingJodieShopTalkScript[8];

/// Variant-1 menu key 6's shop session followed by Jodie's closing reply poses.
extern EvsCommand gMistParkingJodieOpenShopScript[10];

/// Variant-1 menu key 7's reply sequence, returning Jodie to idle after the CAP cue.
extern EvsCommand gMistParkingJodieShopReplyScript[8];

/// Variant-1 shop menu's farewell sequence for choices other than keys 6 and 7.
extern EvsCommand gMistParkingJodieShopFarewellScript[8];

/// Writable parking collision mesh and 7-by-4 XZ cell index, shared by all room variants.
extern WorldCollisionGrid gMistParkingCollisionGrid;

/// Displayed weapon-supply quantity in signed 24.8 fixed point.
extern s32 gShopRefillDisplayQuantityFixed;

/// Borrowed weapon-supply catalog row currently displayed by the refill panel.
extern const EquipmentWeaponSupply* gShopRefillSupply;

/// The departure and shop dialogs' answer, as their continuations read it; its writer is not found.
extern s32 gMistParkingDialogChoice;

/// Singleton playback record for parking command 8's view-9 cutscene.
extern RoomCutsceneRec gMistParkingCommandCutscene;

/// Singleton prize-announcement cursor and timer for Jodie's talk outside variant 1.
extern MistParkingPrizeAnnouncementState gMistParkingPrizeAnnouncement;

/// Variant-1 shop conversation's held player head-aim task and retained unknown word.
extern MistParkingHeadAimHandle gMistParkingShopPlayerHeadAimHandle;

/// Singleton work of Jodie's variant-1 prize and shop-menu conversation.
extern MistParkingShopTalkState gMistParkingShopTalk;

/// Player head-aim controls; STOP releases the room's task handle.
enum {
    MIST_PARKING_HEAD_AIM_FOLLOW_ANIMATION = 0,
    MIST_PARKING_HEAD_AIM_FORCE            = 1
};

/// Ordinals among the loaded bundle's CAP data resources.
enum {
    MIST_PARKING_DIALOGUE_DEFAULT        = 0,
    MIST_PARKING_DIALOGUE_DEPARTURE_MENU = 1,
    MIST_PARKING_DIALOGUE_PRIZES         = 2
};

/// Saved progress through the variant-2 parking-lot conversation.
enum {
    MIST_PARKING_CONVERSATION_INTRO_PENDING           = 0,
    MIST_PARKING_CONVERSATION_INTRO_COMPLETE          = 1,
    MIST_PARKING_CONVERSATION_FIRST_FOLLOWUP_STARTED  = 2,
    MIST_PARKING_CONVERSATION_SECOND_FOLLOWUP_STARTED = 3
};

/// Resets CAP selection and selects the dialogue resource and font page for this talk.
void mistParkingSelectDialogueResource(s32 resourceOrdinal);

/// Forgets the cutscene model and player head-aim tasks before the arrival conversation.
void mistParkingResetCutsceneTaskHandles(s32 unused);

/// Forgets the variant-1 player's head-aim task handle before room setup.
void mistParkingForgetShopHeadAimTaskHandle(s32 unused);

/// Ramps the player's head aim toward the stage/area's index-zero talk partner.
void mistParkingAimPlayerHeadAtTalkPartnerTask(Task* task);

/// Controls the existing task that turns the player's head toward the room's talk partner.
void mistParkingControlPlayerHeadAim(s32 mode);

/// Queues a temporary display mode that exits after the given callback delay.
void mistParkingQueueDelayedDisplayModeExit(s32 delayTicks);

/// Counts down callback ticks before releasing a temporary display mode.
void mistParkingDelayDisplayModeExitTask(Task* task);

/// Stores the saved progress through the variant-2 parking-lot conversation.
void mistParkingSetConversationProgress(s32 progress);

/// Runs the arrival conversation's delayed model-pitch animation while actors are running.
void mistParkingCutsceneModelPitchTask(Task* task);

#endif // STAGES_AKROPOLIS_ROOMS_MIST_PARKING_MIST_PARKING_PRIVATE_H
