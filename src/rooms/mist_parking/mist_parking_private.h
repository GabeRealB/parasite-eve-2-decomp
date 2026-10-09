#ifndef SRC_ROOMS_MIST_PARKING_MIST_PARKING_PRIVATE_H
#define SRC_ROOMS_MIST_PARKING_MIST_PARKING_PRIVATE_H

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/inventory.h"
#include "gameplay/message.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

/// Timing of one prize's turn in a parking-lot talk's prize announcements,
/// counted down in `prizeTimer`.
enum {
    MIST_PARKING_PRIZE_ANNOUNCEMENT_FRAMES        = 10, // Idle frames each prize's turn lasts
    MIST_PARKING_PRIZE_ANNOUNCEMENT_CAPTION_FRAME = 5   // Frames left when a waiting prize's caption starts
};

/// Progress through the prize announcements of the parking-lot talk that runs
/// in every area variant but 1.
///
/// When the talk's prize prompt chooses the announcements, each
/// shooting-gallery prize gets a turn in course order and the ones waiting
/// here are captioned. It is cleared when the talk starts.
typedef struct {
    s16 prizeTimer; // Frames left on the current prize's turn
    s16 prizeIndex; // Shooting-gallery course whose prize has the turn (0..4; 5 once all have had one)
} MistParkingPrizeAnnouncementState;
STATIC_ASSERT_SIZEOF(MistParkingPrizeAnnouncementState, 4);

/// The room's hold on the task that turns the player's head toward the
/// index-0 placement for this stage and area.
///
/// The second word is zero and unreferenced. Its role is unproven.
typedef struct {
    Task* task;       // Head-aim task, or NULL when the room holds none
    u8    field_4[4]; // Unreferenced zeros; role unproven
} MistParkingHeadAimHandle;
STATIC_ASSERT_SIZEOF(MistParkingHeadAimHandle, 8);

/// Work of the talk at the parking-lot shop while the area is in variant 1.
///
/// The talk greets, announces each shooting-gallery prize waiting here, then
/// offers a menu that can open the shop. It is cleared when the talk starts.
typedef struct {
    s16 prizeTimer;          // Frames left on the current prize (10 each; a waiting prize's caption starts at 5)
    s16 prizeIndex;          // Shooting-gallery course whose prize is being announced (0..4)
    s16 businessDone;        // Nonzero once prizes came up or a menu choice other than leaving ran; picks the farewell
    s16 prizeClosingCommand; // CAP command closing the prize exchange (2 after the announcements, 3 when skipped)
} MistParkingShopTalkState;
STATIC_ASSERT_SIZEOF(MistParkingShopTalkState, 8);

/// Nine indexed task recipes for the parking conversations and their presentation.
///
/// Entries 0..8: cutscene model, modal two-option dialog, departure-choice
/// continuation, Dryfield departure, player head aim, delayed display exit,
/// shop-choice continuation, Jodie prize/shop talk and departure-menu talk.
/// Priorities are 192 except head aim at 97, which dispatches earlier.
/// There is no terminator. Callback code and model data must remain loaded
/// while spawned tasks use them. Entry 1's external launcher is unproven.
extern TaskDesc gMistParkingConversationTaskDescs[9];

extern TaskDesc D_mist_parking_8018FC24[];

/// Bodyless variant-1 tasks: departure, player head aim, display exit, Jodie talk and menu.
///
/// Indices 0..4 select those roles in order; there is no terminator. Spawning
/// borrows this room's callbacks, so the overlay must stay loaded through each
/// task's lifetime. The head-aim task runs at priority 97, the others at 192.
extern TaskDesc gMistParkingShopTaskDescriptors[5];

/// Tasks the room keeps a handle on while they run.
extern Task* gRoomCutsceneSoundTask;

extern Task* D_mist_parking_80195320;

extern Task* D_mist_parking_80195324;

extern TmdSource gMistParkingModel09B9C;

extern AnimationSet gMistParkingAnimation09FD4;

extern AnimationSet gMistParkingAnimation0A774;

extern AnimationSet gMistParkingAnimation0AC5C;

extern AnimationSet gMistParkingAnimation0B138;

extern AnimationSet gMistParkingAnimation0B700;

extern AnimationSet gMistParkingAnimation0BAB4;

extern AnimationSet gMistParkingAnimation0C1B0;

extern AnimationSet gMistParkingAnimation0C688;

extern AnimationSet gMistParkingAnimation0CD80;

extern AnimationSet gMistParkingAnimation0D060;

extern AnimationSet gMistParkingAnimation0D3E4;

extern AnimationSet gMistParkingAnimation0D6A4;

extern AnimationSet gMistParkingAnimation0DA94;

extern AnimationSet gMistParkingAnimation0DE94;

extern AnimationSet gMistParkingAnimation0E1D0;

extern AnimationSet gMistParkingAnimation0E6E0;

extern AnimationSet gMistParkingAnimation0EA0C;

extern AnimationSet gMistParkingAnimation0EDE0;

extern AnimationSet gMistParkingAnimation0F14C;

extern AnimationSet gMistParkingAnimation0F574;

extern AnimationSet gMistParkingAnimation0F7F0;

extern AnimationSet gMistParkingAnimation0FC60;

extern AnimationSet gMistParkingAnimation0FE5C;

extern AnimationSet gMistParkingAnimation10174;

/// Copies the room's 25 pointer words into the equipped player's animation-bank extension.
///
/// The first copied entry is NULL; extension indices 1..24 select room clips.
/// The request and table are borrowed through synchronous dispatch, and the
/// copied descriptors and clips must remain loaded while playback uses them.
extern AnimationBankCopyRequest gMistParkingPlayerConversationBankCopy;

/// Animation-controlled player head aim, indexed by set index minus the 47 base sets.
///
/// Values are 0 off and 1 aim toward scene placement 0. The head-aim task reads
/// only entries 1..24, bounded by `gMistParkingPlayerConversationBankCopy`;
/// entry 0 and the three trailing zero bytes are not read by that task.
extern s8 gMistParkingPlayerAnimationHeadAimFlags[28];

extern EvsCommand D_mist_parking_8018DF34[155];

extern EvsCommand D_mist_parking_8018EDBC[23];

extern EvsCommand D_mist_parking_8018EFE4[8];

extern EvsCommand D_mist_parking_8018F0A4[10];

extern EvsCommand D_mist_parking_8018F194[10];

extern WorldCollisionGrid D_mist_parking_8018FCB8;

extern AnimationSet gMistParkingAnimation129F8;

extern AnimationSet gMistParkingAnimation12DCC;

extern AnimationSet gMistParkingAnimation1323C;

/// Installs the variant-1 player's four-word shop/departure animation extension.
///
/// Set 47 is NULL; sets 48..50 are talk, departure-menu and stay clips. The
/// equipped weapon bank is selected by the receiver. Copy dispatch borrows
/// this request; the loaded room's clip data must outlive subsequent playback.
extern AnimationBankCopyRequest gMistParkingShopPlayerAnimationBankCopy;

/// Player pose shared by Jodie's variant-1 shop talks, restarting extension clip 48.
///
/// Requires the shop bank extension to be installed. Weapon-animation dispatch
/// resolves the source bank against the current equipment before playback;
/// the stored index is ignored. Takes scripted control and disables grid response.
extern AnimationPlayRequest gMistParkingPlayerShopTalkAnimation;

/// Jodie's resting pose before and after the variant-1 greeting, in bank 0, clip 1.
///
/// Requests a twenty-frame blend when she is already ticking. Her playback
/// handler ignores the collision word; her own loaded bank supplies the clip.
extern AnimationPlayRequest gMistParkingJodieIdleAnimation;

/// Jodie's closing pose after shop use or leaving, in bank 0, clip 5.
///
/// Requests a twenty-frame blend when already ticking; the collision word is
/// ignored. Her own loaded bank supplies the clip throughout playback.
extern AnimationPlayRequest gMistParkingJodieShopExitAnimation;

/// Jodie's greeting/prize-talk pose, restarting bank 0, clip 7 after the initial delay.
///
/// Repeating the same clip preserves playback. Her handler ignores the collision
/// word and uses her own loaded bank through playback.
extern AnimationPlayRequest gMistParkingJodieGreetingAnimation;

/// Jodie's first reply pose for the variant-1 shop menu, in bank 0, clip 8.
///
/// Requests a twenty-frame blend when already ticking. The reply's CAP cue
/// selects the following pose; the handler ignores the collision word.
extern AnimationPlayRequest gMistParkingJodieReplyAnimation;

/// Jodie's pose at the reply's CAP cue, in bank 0, clip 9 with a fifteen-frame blend.
///
/// The scripts wait thirty frames before returning to idle or the closing pose.
/// Her handler ignores the collision word and uses her own loaded bank.
extern AnimationPlayRequest gMistParkingJodieReplyEndAnimation;

extern EvsCommand D_mist_parking_80191154[8];

extern EvsCommand D_mist_parking_80191214[10];

extern EvsCommand D_mist_parking_80191304[8];

extern EvsCommand D_mist_parking_801913C4[8];

extern WorldCollisionGrid D_mist_parking_80192204;

extern s32 Shop_Data_80187628;

extern const EquipmentWeaponSupply* Shop_Data_8018762C;

extern s32 D_mist_parking_8019531C;

extern RoomCutsceneRec D_mist_parking_8019533C;

extern MistParkingPrizeAnnouncementState D_mist_parking_80195328;

extern MistParkingHeadAimHandle D_mist_parking_8019532C;

extern MistParkingShopTalkState D_mist_parking_80195334;

/// Player head-aim controls; STOP releases the room's task handle.
enum {
    MIST_PARKING_HEAD_AIM_STOP             = -1,
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
///
/// Ordinals 1 and 2 select the departure menu and prize dialogue; every other
/// value keeps the default resource selected by the reset. Playback must have
/// stopped. The bundle's CAP data and font images must already be loaded and
/// remain available while CAP uses them; no file is loaded by this call.
void mistParkingSelectDialogueResource(s32 resourceOrdinal);

/// Forgets the cutscene model and player head-aim tasks before the arrival conversation.
///
/// Does not stop or release either task. Use before spawning the conversation's
/// tasks, when the room should hold neither handle. The caller supplies an
/// ignored argument.
void mistParkingResetCutsceneTaskHandles(s32 unused);

/// Forgets the variant-1 player's head-aim task handle before room setup.
///
/// Does not kill the task or change the unused second word of the handle.
/// `unused` is the ignored argument supplied by the room's setup callback.
void mistParkingForgetShopHeadAimTaskHandle(s32 unused);

/// Ramps the player's head aim toward the stage/area's index-zero talk partner.
///
/// State 0 updates; every other state releases the task. Event-script suspension
/// pauses both paths. `spawnArg1.value` is zero for animation-controlled aiming,
/// nonzero to force it; `killCountdown` starts at zero and stores a 1/4096 blend
/// weight clamped to 0..4096, stepped by 256 per active callback. Slot 1's next
/// set selects extension flags 1..24; every other index disables animation aim.
/// Requires the live player work, installed parking animation bank and a live
/// index-zero partner with the head-joint model layout. Borrows both models.
void mistParkingAimPlayerHeadAtTalkPartnerTask(Task* task);

/// Controls the existing task that turns the player's head toward the room's talk partner.
///
/// FOLLOW_ANIMATION lets the current animation select aiming, FORCE keeps aiming
/// enabled, and every other value kills the task and clears the handle. Does
/// nothing when the room holds no task; it never spawns one. The handle must
/// refer to a live task while it is set.
void mistParkingControlPlayerHeadAim(s32 mode);

/// Queues a temporary display mode that exits after the given callback delay.
///
/// A nonnegative `delayTicks` exits on dispatch delayTicks + 1. Uses the reload
/// presentation policy; an already pending mode request silently rejects it.
/// The parking overlay and descriptor table must remain loaded through exit.
void mistParkingQueueDelayedDisplayModeExit(s32 delayTicks);

/// Counts down callback ticks before releasing a temporary display mode.
///
/// `task->spawnArg1.value` is the signed 32-bit countdown, decremented before
/// testing. An initial nonnegative N requests exit on dispatch N + 1. Kills the
/// task before requesting mode exit; requires a live task in that display mode.
void mistParkingDelayDisplayModeExitTask(Task* task);

/// Stores the saved progress through the variant-2 parking-lot conversation.
///
/// The low four bits of `progress` replace the flag's nibble. Both completion
/// and skip of the arrival talk store INTRO_COMPLETE; later interactions advance
/// through the two follow-ups before offering the departure menu.
void mistParkingSetConversationProgress(s32 progress);

/// Runs the arrival conversation's delayed model-pitch animation while actors are running.
///
/// Requires a live TMD task and state 0 (initialize), 1 (advance pitch) or 2
/// (release). The task owns the model body; its root stays in its existing parent
/// frame. State 1 uses `killCountdown` as a signed angle counter, in 4096 units
/// per turn, and retains the last placement between updates. Only one instance
/// may run because its placement is shared. Actor suspension pauses every state.
void mistParkingCutsceneModelPitchTask(Task* task);

#endif // SRC_ROOMS_MIST_PARKING_MIST_PARKING_PRIVATE_H
