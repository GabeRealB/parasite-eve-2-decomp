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

/// CAP-11 reply selected by conversation variant key 12.
///
/// Cues Gary's reply and closing animations, then restores player control and
/// the HUD. The interpreter borrows all fourteen commands, including END,
/// and their animation requests until completion; keep this room loaded.
extern EvsCommand gDryfieldNightTrailerCoachCap11ReplyScript[14];

/// CAP-13 reply after the story conversation has already been seen.
///
/// Cues Gary's reply and closing animations, then restores player control and
/// the HUD. The interpreter borrows all fourteen commands, including END,
/// and their animation requests until completion; keep this room loaded.
extern EvsCommand gDryfieldNightTrailerCoachStoryRepeatScript[14];

/// First story conversation, selected by variant key 13 and playing CAP 12.
///
/// Installs player dialogue clips, places the player and cues both actors under
/// a temporary ambient/weapon override. Finishes in view 3 with control and HUD
/// restored. The interpreter borrows 57 commands through END and their payloads;
/// installed animation tracks also require this room to remain loaded.
extern EvsCommand gDryfieldNightTrailerCoachStoryScript[57];

/// Restoration script when the first story conversation is skipped.
///
/// Fades through player placement and actor/weapon/ambient restoration, selects
/// view 3, releases event control and starts CAP 17. Its sixteen commands and
/// animation payloads remain borrowed until the interpreter reaches END.
extern EvsCommand gDryfieldNightTrailerCoachStorySkipScript[16];

/// CAP-25 arrival conversation for saved warp 3.
///
/// Installs player dialogue clips, places the player and cues both actors, then
/// restores weapons, ambient lighting, view 3 and player control. Borrows all
/// 24 commands through END and their payloads; installed tracks stay room-owned.
extern EvsCommand gDryfieldNightTrailerCoachWarp3ArrivalScript[24];

/// Restoration script when the saved-warp-3 arrival conversation is skipped.
///
/// Fades through actor, weapon and ambient restoration, returns to view 3 and
/// releases event/player control. The thirteen commands, including END, and
/// animation payloads require this room to stay loaded through completion.
extern EvsCommand gDryfieldNightTrailerCoachWarp3ArrivalSkipScript[13];

/// Saved garage update committed before the first story conversation starts.
///
/// In every save mode, selects night garage placement variant 4, removes its
/// saved enemy poses and sets its map mark. One update precedes AREA_APPLY_END;
/// the list is borrowed only during synchronous application.
extern AreaApplyRec gDryfieldNightTrailerCoachStoryAreaUpdates[2];

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

extern Task* gRoomCutsceneSoundTask;

/// Persistent playback record for the nighttime coach's view-8 cutscene.
///
/// The room configures CAP slot 1 and file/page X by chapter, with sounds 3..6.
/// Texture-page Y retains its initialized zero. The runner borrows this record
/// until completion: keep the room loaded and do not overwrite it meanwhile.
extern RoomCutsceneRec gDryfieldNightTrailerCoachCutscene;

#endif // SRC_ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_DRYFIELD_NIGHT_TRAILER_COACH_PRIVATE_H
