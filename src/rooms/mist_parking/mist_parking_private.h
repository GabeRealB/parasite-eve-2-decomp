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

/// Two bodyless priority-192 recipes for departure-movie launch and display playback.
///
/// Index 0 forwards spawnArg1 to a display task using index 1. Zero selects
/// movie 100 in the current room, every nonzero word movie 101; spawnArg2 is
/// unused. There is no terminator. Spawning copies the recipe, but the room's
/// callback code and loaded movie resources must outlive playback and restoration.
extern TaskDesc gMistParkingDepartureMovieTaskDescs[2];

/// Bodyless variant-1 tasks: departure, player head aim, display exit, Jodie talk and menu.
///
/// Indices 0..4 select those roles in order; there is no terminator. Spawning
/// borrows this room's callbacks, so the overlay must stay loaded through each
/// task's lifetime. The head-aim task runs at priority 97, the others at 192.
extern TaskDesc gMistParkingShopTaskDescriptors[5];

/// Tasks the room keeps a handle on while they run.
extern Task* gRoomCutsceneSoundTask;

/// Held arrival-conversation model task, or NULL when absent or allocation failed.
///
/// Release kills the task and clears this handle; reset only forgets it.
/// A held pointer must identify a live task with its room-owned model loaded.
extern Task* gMistParkingCutsceneModelTask;

/// Held general-conversation player head-aim task, or NULL when none is held.
///
/// Spawn replaces the handle, control changes its aiming mode or kills/clears
/// it, and reset only forgets it. Keep the task and borrowed player/partner
/// models live while held; starting twice does not release the earlier task.
extern Task* gMistParkingPlayerHeadAimTask;

extern TmdSource gMistParkingMistParkingModel09B9C;

extern AnimationSet gAyaStandClip;

extern AnimationSet gShelterR47PlayerLookAtMapClip;

extern AnimationSet gShelterB3IncineratorControlRoomPlayerFlusteredClip;

extern AnimationSet gMistParkingMistParkingPlayerConversationAnim04Clip;

extern AnimationSet gMistParkingMistParkingPlayerConversationAnim05Clip;

extern AnimationSet gMistParkingMistParkingPlayerConversationAnim06Clip;

extern AnimationSet gMistParkingMistParkingPlayerConversationAnim07Clip;

extern AnimationSet gMistParkingMistParkingPlayerConversationAnim08Clip;

extern AnimationSet gMistParkingMistParkingPlayerConversationAnim09Clip;

extern AnimationSet gMistParkingMistParkingPlayerConversationAnim10Clip;

extern AnimationSet gAyaIntroduceHerselfClip;

extern AnimationSet gAyaAnim07Clip;

extern AnimationSet gAyaTalkClip;

extern AnimationSet gShelterB6GrowthRoomPlayerChoiceStartClip;

extern AnimationSet gAyaItsNothingClip;

extern AnimationSet gMistParkingMistParkingPlayerConversationAnim15Clip;

extern AnimationSet gAyaOnMyWayClip;

extern AnimationSet gAyaTalkStartClip;

extern AnimationSet gAyaTalkEndClip;

extern AnimationSet gAyaReplyClip;

extern AnimationSet gMistParkingMistParkingPlayerConversationAnim20Clip;

extern AnimationSet gMistParkingMistParkingPlayerConversationAnim21Clip;

extern AnimationSet gAyaTurnPaleClip;

extern AnimationSet gShelterB1PodServiceGantryPlayerGantryScene7Clip;

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

/// Skippable arrival conversation between the player and Pierce in area variant 2.
///
/// Runs at the initial warp-3 entry: installs the player extension, stages
/// the cutscene model and CAP slot 14 exchanges, then restores visibility,
/// weapons, collision patch, head aim and the normal view. Ends by marking
/// the intro complete. Use `gMistParkingArrivalSkipScript` as its skip target.
/// END terminates the fixed command array. Commands, callbacks and payloads
/// stay borrowed while EVS runs; actor animation/CAP resources must outlive playback.
extern EvsCommand gMistParkingArrivalConversationScript[155];

/// Finishes arrival setup when the player skips the conversation.
///
/// Releases the cutscene model and head aim, restores the collision patch,
/// places and shows the actors, restores weapons/control and marks the intro
/// complete. Passed as the skip target of `gMistParkingArrivalConversationScript`.
/// END terminates the fixed command array. Commands, callbacks and payloads
/// stay borrowed while EVS runs; actor animation/CAP resources must outlive playback.
extern EvsCommand gMistParkingArrivalSkipScript[23];

/// Places Pierce and Jodie for a non-warp-3 entry while the intro is pending.
///
/// Shows both actors, sends Pierce to his post-arrival position and leaves
/// Jodie at the shop in body-bank clip 5. Does not mark the intro complete.
/// END terminates the fixed command array. Commands, callbacks and payloads
/// stay borrowed while EVS runs; actor animation/CAP resources must outlive playback.
extern EvsCommand gMistParkingArrivalRoomEntryScript[8];

/// Plays Pierce's first post-arrival conversation before the departure menu.
///
/// Starts CAP command 15/key 0, player clip 64 and Pierce clip 8, then
/// releases head aim, resumes player control and returns Pierce to clip 1.
/// END terminates the fixed command array. Commands, callbacks and payloads
/// stay borrowed while EVS runs; actor animation/CAP resources must outlive playback.
extern EvsCommand gMistParkingFirstFollowupScript[10];

/// Plays Pierce's second post-arrival conversation before the departure menu.
///
/// Starts CAP command 15/key 1 with the same talk poses, then releases
/// head aim, resumes player control and returns Pierce to clip 1.
/// END terminates the fixed command array. Commands, callbacks and payloads
/// stay borrowed while EVS runs; actor animation/CAP resources must outlive playback.
extern EvsCommand gMistParkingSecondFollowupScript[10];

/// Authored collision patch restored into the live room grid with Pierce's visibility.
///
/// Two normals, six vertices and two quads supply the live grid's reserved
/// prefix. Vertices use signed world-coordinate units; normals use 4096 per
/// unit length. The source remains unchanged when the live patch is lowered.
/// Its one 4000-unit XZ cell begins at (4800, -6558) and lists faces 0/1 then
/// WORLD_COLLISION_GRID_CELL_END. This template is never activated as a grid;
/// its view pointer stays NULL. The room owns the record and borrowed pools.
extern WorldCollisionGrid gMistParkingPierceCollisionPatch;

extern AnimationSet gMistParkingStandCopyClip;

extern AnimationSet gMistParkingTalkStartCopyClip;

extern AnimationSet gMistParkingMistParkingPlayerConversationAnim21CopyClip;

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

/// Player/Jodie pose sequence shared by variant-1 greeting and prize exchanges.
///
/// Installs the player shop extension and restarts its talk pose. A twenty-update
/// countdown precedes Jodie's greeting; the CAP cue returns her to idle.
/// Borrows the room's animation data and the live scene-placement-1 Jodie task.
extern EvsCommand gMistParkingJodieShopTalkScript[8];

/// Variant-1 menu key 6's shop session followed by Jodie's closing reply poses.
///
/// Installs the player extension and queues MIST stock, then uses a twenty-update
/// countdown before restoring the talk pose. The CAP cue selects Jodie's reply-end
/// pose; a thirty-update countdown precedes closure. Requires loaded shop/CAP/actors.
extern EvsCommand gMistParkingJodieOpenShopScript[10];

/// Variant-1 menu key 7's reply sequence, returning Jodie to idle after the CAP cue.
///
/// Installs/restarts the player's shop talk pose and starts Jodie's reply.
/// The cue selects her reply-end pose; a thirty-update countdown precedes idle.
/// Borrows the loaded room clips and the live player and scene-placement-1 tasks.
extern EvsCommand gMistParkingJodieShopReplyScript[8];

/// Variant-1 shop menu's farewell sequence for choices other than keys 6 and 7.
///
/// Installs/restarts the player's talk pose and starts Jodie's reply. The CAP
/// cue selects her reply-end pose; a thirty-update countdown precedes closure.
/// Borrows the loaded room clips and the live player and scene-placement-1 tasks.
extern EvsCommand gMistParkingJodieShopFarewellScript[8];

/// Writable parking collision mesh and 7-by-4 XZ cell index, shared by all room variants.
///
/// Has 13 normals, 125 vertices and 66 faces. Cells span 4000 world units from
/// origin (-11000, -7000); each cell list ends with WORLD_COLLISION_GRID_CELL_END.
/// Room setup binds the current view. Pierce's patch replaces the first two
/// normals/faces and six vertices without rebuilding the cell lists. The grid
/// and all borrowed pools must stay loaded while active.
extern WorldCollisionGrid gMistParkingCollisionGrid;

/// Displayed weapon-supply quantity in signed 24.8 fixed point.
///
/// Refill commits the live weapon load to capacity immediately, then this value
/// rises from the old quantity by one quarter-unit per callback and clamps at
/// capacity. The panel uses it for both the number and its meter, through closure.
extern s32 gShopRefillDisplayQuantityFixed;

/// Borrowed weapon-supply catalog row currently displayed by the refill panel.
///
/// NULL until the first carried supply is selected. Retained while the panel
/// closes; gameplay and its catalog must remain loaded throughout the session.
extern const EquipmentWeaponSupply* gShopRefillSupply;

/// Unproven-origin word read by the departure and shop choice continuations.
///
/// Initialized to zero; 2 selects departure or skipping the shop, respectively.
/// Every other value selects staying or opening the shop. No writer has been
/// identified, so its full value domain and connection to the option dialog
/// are unproven. Storage remains a signed, aligned 32-bit word.
extern s32 D_mist_parking_8019531C;

/// Singleton playback record for parking command 8's view-9 cutscene.
///
/// Configured before spawn for CAP file 3, slot 1 and four area sound events.
/// Zero texture-page coordinates select the runner's default (960, 0).
/// Borrowed until the runner and follow-up dialogue finish: do not overwrite
/// it or unload the room while that task is live.
extern RoomCutsceneRec gMistParkingCommandCutscene;

/// Singleton prize-announcement cursor and timer for Jodie's talk outside variant 1.
///
/// Cleared as the talk starts. The signed halfwords track courses 0..4, ending
/// at cursor 5, and ten idle callback ticks per course, captioning at five left.
/// No allocation or release occurs; only one such talk may use it at a time.
extern MistParkingPrizeAnnouncementState gMistParkingPrizeAnnouncement;

/// Variant-1 shop conversation's held player head-aim task and retained unknown word.
///
/// Spawn stores a live task or NULL; control kills/clears it, while room setup
/// only forgets it. The trailing four bytes remain zero and unreferenced.
/// Requires a live held task and loaded player/partner models until stopped.
extern MistParkingHeadAimHandle gMistParkingShopPlayerHeadAimHandle;

/// Singleton work of Jodie's variant-1 prize and shop-menu conversation.
///
/// All eight bytes are cleared at greeting. Keeps the prize cursor/timer,
/// whether business occurred, and the prize-closing CAP command through the
/// farewell. Only one talk may use it at a time; the room owns its lifetime.
extern MistParkingShopTalkState gMistParkingShopTalk;

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
