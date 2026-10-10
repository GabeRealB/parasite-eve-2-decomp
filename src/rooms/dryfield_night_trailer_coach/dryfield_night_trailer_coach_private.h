#ifndef SRC_ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_DRYFIELD_NIGHT_TRAILER_COACH_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_DRYFIELD_NIGHT_TRAILER_COACH_PRIVATE_H

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/evs.h"
#include "gameplay/inventory.h"
#include "gameplay/message.h"

#include "main/task_types.h"

#include "rooms/room.h"

/// Player placement shared by arrival dialogue and its skip restoration.
///
/// Position (4870, 0, -900) is in the model root's room-coordinate frame;
/// yaw -1536 uses 4096 units per turn, with zero pitch and roll. Placement
/// copies both vectors synchronously; the event instruction borrows this record
/// until dispatch and requires the room image to remain loaded.
extern ActorTransform gDryfieldNightTrailerCoachPlayerDialogueTransform;

/// Equipped-bank player idle request used by the nighttime trailer dialogue scripts.
///
/// Event playback copies the request and replaces its bank seed from the
/// current equipment. It resets clip 1 without blending and requests world
/// collision participation. The request is borrowed only for dispatch; the
/// selected bank must remain loaded throughout playback.
extern const AnimationPlayRequest gDryfieldNightTrailerCoachPlayerIdleAnimation;

/// Equipped-player clip 47 leading into the story conversation's closing phase.
///
/// Event playback copies the request and resolves its bank seed from equipment.
/// Blends over eight frames with world collision disabled. The script borrows
/// the request until dispatch; the room's installed clip data stays live during playback.
extern const AnimationPlayRequest gDryfieldNightTrailerCoachPlayerStoryClosingLeadInAnimation;

/// Equipped-player clip 52 at the closing CAP cue of the story conversation.
///
/// Event playback copies the request and resolves its bank seed from equipment.
/// Resets playback with world collision disabled. The script borrows the request
/// until dispatch; the room's installed clip data stays live during playback.
extern const AnimationPlayRequest gDryfieldNightTrailerCoachPlayerStoryClosingAnimation;

/// Equipped-player clip 53 repeated at the story conversation's dialogue cues.
///
/// Event playback copies the request and resolves its bank seed from equipment.
/// Resets playback with world collision disabled. The script borrows the request
/// until dispatch; the room's installed clip data stays live during playback.
extern const AnimationPlayRequest gDryfieldNightTrailerCoachPlayerStoryDialogueAnimation;

/// Equipped-player clip 56 at the chapter-4 arrival conversation cue.
///
/// Event playback copies the request and resolves its bank seed from equipment.
/// Resets playback with world collision disabled. The script borrows the request
/// until dispatch; the room's installed clip data stays live during playback.
extern const AnimationPlayRequest gDryfieldNightTrailerCoachPlayerChapter4DialogueAnimation;

/// Equipped-player clip 57 at the middle story-conversation reply cue.
///
/// Event playback copies the request and resolves its bank seed from equipment.
/// Resets playback with world collision disabled. The script borrows the request
/// until dispatch; the room's installed clip data stays live during playback.
extern const AnimationPlayRequest gDryfieldNightTrailerCoachPlayerStoryReplyAnimation;

/// Gary's reset base-bank clip 1 opening the first story conversation.
///
/// Resets playback immediately and ignores blend duration and collision.
/// The script borrows the request until synchronous placement-0 dispatch;
/// the room image must remain loaded until that instruction executes.
extern AnimationPlayRequest gDryfieldNightTrailerCoachGaryStoryLeadInAnimation;

/// Gary's reset base-bank clip 4 between the closing lead-in and talk rest.
///
/// Closing scripts wait nineteen frames before the rest request. The receiver
/// resets immediately and ignores blend duration and collision participation.
/// Event scripts borrow the request through synchronous placement-0 dispatch;
/// the room image must remain loaded until the instruction executes.
extern AnimationPlayRequest gDryfieldNightTrailerCoachGaryTalkClosingAnimation;

/// Gary's reset base-bank clip 5 used to finish or skip nighttime dialogue.
///
/// Shared by arrival restoration and the talk scripts' final phase. The receiver
/// resets immediately and ignores blend duration and collision participation.
/// Event scripts borrow the request through synchronous placement-0 dispatch;
/// the room image must remain loaded until the instruction executes.
extern AnimationPlayRequest gDryfieldNightTrailerCoachGaryTalkRestAnimation;

/// Gary's blended base-bank clip 8 held during the ordinary talk phase.
///
/// Used after the opening sequence and at the start of repeat dialogue. The
/// receiver blends for eight frames and ignores the collision setting.
/// Event scripts borrow the request through synchronous placement-0 dispatch;
/// the room image must remain loaded until the instruction executes.
extern AnimationPlayRequest gDryfieldNightTrailerCoachGaryTalkHoldAnimation;

/// Gary's blended base-bank clip 9 leading into the talk-closing sequence.
///
/// Scripts wait nine frames before the closing request. The receiver blends
/// for eight frames and ignores the collision setting.
/// Event scripts borrow the request through synchronous placement-0 dispatch;
/// the room image must remain loaded until the instruction executes.
extern AnimationPlayRequest gDryfieldNightTrailerCoachGaryTalkClosingLeadInAnimation;

/// Gary's blended base-bank clip 10 for the CAP-11 and repeat CAP-13 replies.
///
/// Blends over eight frames regardless of blendFrames; collision is ignored.
/// The script borrows the request until synchronous placement-0 dispatch;
/// the room image must remain loaded until that instruction executes.
extern AnimationPlayRequest gDryfieldNightTrailerCoachGaryTalkReplyAnimation;

/// Gary's reset bank-1 clip 1 leading into arrival and story dialogue.
///
/// Followed by bank-1 clip 2 after a script-controlled wait. The receiver
/// resets immediately and ignores blend duration and collision participation.
/// Event scripts borrow the request through synchronous placement-0 dispatch;
/// the room image must remain loaded until the instruction executes.
extern AnimationPlayRequest gDryfieldNightTrailerCoachGaryDialogueLeadInAnimation;

/// Gary's reset bank-1 clip 2 held between arrival and story dialogue cues.
///
/// The receiver maps this request to local set 12, resets immediately and
/// ignores blend duration and collision participation.
/// Event scripts borrow the request through synchronous placement-0 dispatch;
/// the room image must remain loaded until the instruction executes.
extern AnimationPlayRequest gDryfieldNightTrailerCoachGaryDialogueHoldAnimation;

/// Gary's blended bank-1 clip 3 between the first story conversation's dialogue cues.
///
/// Maps to local set 13. Blends over eight frames regardless of blendFrames; collision is ignored.
/// The script borrows the request until synchronous placement-0 dispatch;
/// the room image must remain loaded until that instruction executes.
extern AnimationPlayRequest gDryfieldNightTrailerCoachGaryStoryReplyAnimation;

/// Gary's blended bank-1 clip 7 for the next phase of arrival and story dialogue.
///
/// Follows the clip-1/clip-2 sequence. The receiver maps it to local set 17
/// and uses an eight-frame blend despite the request's zero blend duration.
/// Collision participation is ignored.
/// Event scripts borrow the request through synchronous placement-0 dispatch;
/// the room image must remain loaded until the instruction executes.
extern AnimationPlayRequest gDryfieldNightTrailerCoachGaryDialogueFollowupAnimation;

/// Gary's blended bank-2 clip 3 at the first story conversation's closing cue.
///
/// Maps to local set 20. Blends over eight frames regardless of blendFrames; collision is ignored.
/// The script borrows the request until synchronous placement-0 dispatch;
/// the room image must remain loaded until that instruction executes.
extern AnimationPlayRequest gDryfieldNightTrailerCoachGaryStoryClosingAnimation;

/// Installs eleven room-owned dialogue clips into the equipped player's bank extension.
///
/// Copies set-pointer words to indices 47..57 without starting playback. The
/// request is borrowed through synchronous dispatch; copied descriptors and
/// tracks remain borrowed from this room image until the player stops using them.
/// The source table's trailing NULL is excluded from the copy.
extern const AnimationBankCopyRequest gDryfieldNightTrailerCoachPlayerDialogueBankCopyRequest;

extern EvsCommand D_dryfield_night_trailer_coach_80188708[14];

extern EvsCommand D_dryfield_night_trailer_coach_80188858[14];

extern EvsCommand D_dryfield_night_trailer_coach_801889A8[57];

extern EvsCommand D_dryfield_night_trailer_coach_80188F00[16];

extern EvsCommand D_dryfield_night_trailer_coach_80189080[24];

extern EvsCommand D_dryfield_night_trailer_coach_801892C0[13];

extern AreaApplyRec D_dryfield_night_trailer_coach_8018C208[2];

/// Displayed weapon-supply quantity in signed 24.8 fixed point.
///
/// Refill commits the live weapon load to capacity immediately, then this value
/// rises from the old quantity by one quarter-unit per callback and clamps at
/// capacity. The panel uses it for both the number and its meter, through closure.
extern s32 gShopRefillDisplayQuantityFixed;

/// Borrowed weapon-supply catalogue row currently displayed by the refill panel.
///
/// NULL until the first carried supply is selected. Retained while the panel
/// closes; gameplay and its catalogue must remain loaded throughout the session.
extern const EquipmentWeaponSupply* gShopRefillSupply;

extern Task* gRoomCutsceneSoundTask;

extern RoomCutsceneRec D_dryfield_night_trailer_coach_8018C21C;

#endif // SRC_ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_DRYFIELD_NIGHT_TRAILER_COACH_PRIVATE_H
