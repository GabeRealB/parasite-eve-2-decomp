#include "actors/actor_215100.h"
#include "actor_215100_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/strings.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/companion_load.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/cap.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/mist_shooting_gallery.h"
#include "../../shared/walk/paced_walk.h"
#include "../../shared/room/cap_captions_types.h"

// Course selections are 1-based; the room controller receives selection minus one.
enum {
    ACTOR_215100_GALLERY_FIRST_FREE_MOVEMENT_COURSE = 3,
    ACTOR_215100_GALLERY_FIRST_SWAP_COURSE          = 4,
    ACTOR_215100_GALLERY_BONUS_COURSE               = 5,
    ACTOR_215100_GALLERY_EXIT_DECISION_TASK         = 0,
    ACTOR_215100_GALLERY_ABORT_DECISION_TASK        = 1,
    ACTOR_215100_GALLERY_END_TRAINING               = 0,
    ACTOR_215100_GALLERY_CONFIRM_ROOM_EXIT          = 1,
    ACTOR_215100_GALLERY_COMMIT_ROOM_EXIT           = 2,
    ACTOR_215100_GALLERY_EXIT_CONFIRM_COMMAND       = 0x14,
    ACTOR_215100_GALLERY_EXIT_TRANSITION_COMMAND    = 0x17,
    ACTOR_215100_GALLERY_ABORT_CONFIRM_COMMAND      = 0x1D,
    ACTOR_215100_GALLERY_EXIT_KEEP_ACTORS_HELD      = 1,
    ACTOR_215100_GALLERY_MUSIC_FADE_AUDIO_TICKS     = 30,
    ACTOR_215100_GALLERY_RETURN_VIEW                = 8,
};

static CapCaptionCaretDelayStorage _gCapCaptionCaretDelayStorage;

static void _actor215100SpawnPierce(Enemy* enemy, Task* task);
static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _pacedWalkExit(Task* task);

static void _actor215100ShowTimedCaption(s16 commandIndex, s16 key, s16 durationTicks);
/// Gives this actor's caption record selector external linkage for the gallery.
///
/// The empty replacement supplies no storage-class specifier to the shared
/// prototype and definition. Define before the first `cap_captions.h` inclusion
/// and retain through `cap_captions.inc.c`. `CAP_CAPTION_SELECT_RECORD` names the
/// s32(s16, s16, s32) export declared in `actors/actor_215100.h`; the gallery uses
/// this actor's loaded CAP state. This object-like binding has no arguments,
/// captured identifiers, runtime evaluation or token construction.
#define CAP_CAPTION_SELECT_RECORD_LINKAGE
/// Binds shared record selection to this actor's exported selector.
///
/// Define before `cap_captions.h` and retain through both caption source fragments.
/// The empty linkage binding exports the s32(s16, s16, s32) instance declared in
/// `actors/actor_215100.h`. This object-like alias captures no arguments and
/// constructs no tokens.
#define CAP_CAPTION_SELECT_RECORD actor215100CapCaptionSelectRecord
/// Exports this actor's current-caption drawer to the shooting-gallery room.
///
/// Supplies `extern` to the shared void(void) prototype and definition selected
/// by `CAP_CAPTION_DRAW_CURRENT`, matching `actors/actor_215100.h`. Define before
/// `cap_captions.h` and retain through `cap_captions.inc.c`. This object-like
/// binding has no arguments, captured identifiers or token construction.
#define CAP_CAPTION_DRAW_CURRENT_LINKAGE extern
/// Binds shared caption drawing to this actor's exported void(void) instance.
///
/// Define before cap_captions.h and retain through cap_captions.inc.c. The
/// linkage binding exports the copy called by mist_shooting_gallery.
/// This object-like alias captures no arguments and constructs no tokens.
#define CAP_CAPTION_DRAW_CURRENT actor215100CapCaptionDrawCurrent
#include "../../shared/room/cap_captions.h"

static void _capCaptionRunSchedule(Task* task);
#include "../../shared/walk/walker.h"

static TaskDesc             _gCapCaptionTimedTaskDesc;
static TaskDesc             _gCapCaptionCancelableTaskDesc;
static const TextGlyphCell* _gCapCaptionGlyphCells;

/// Caption script table.
static const CapCommandRef*     _gCapCaptionCommandRefs;
static CapSequenceRecord*       _gCapCaptionSequence;
static s16                      _gCapCaptionBlockLeftX;
static s16                      _gCapCaptionFirstBaselineY;
static s16                      _gCapCaptionBottomBaselineY;
static s16                      _gCapCaptionRecordIndex;
static s16                      _gCapCaptionBlockHeight;
static s16                      _gCapCaptionSelectedKey;
static s32                      _gCapCaptionCaretPulseLevel;
static s32                      _gCapCaptionCaretPulseFalling;
static u16                      _gCapCaptionCaretLeftX;
static u16                      _gCapCaptionCaretTipY;
static s16                      _gCapCaptionTexturePageX;
static s16                      _gCapCaptionTexturePageY;
RoomEventMsg                    _gActor215100GalleryPendingExitRequest;
static CapCaptionScheduleWindow _gCapCaptionSceneClockSchedule[];

static TmdSource _gPierceBody;
static TmdSource _gPierceGlasses;
static s32       _pacedWalkSetPairModelDraw(Task* task, s32 messageId, s32 requestFlags, s32 unusedArgument);
static s32       _actor215100IgnoreCommand(Task* task, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument);
static void      _actor215100PierceTask(Task* task);
static void      _pacedWalkSubModelTask(Task* task);

static AnimationSet _gMistShootingGalleryPiercePhoneListenClip;
static AnimationSet _gMistShootingGalleryPiercePhoneTalkClip;
static AnimationSet _gMistShootingGalleryPierceQuipClip;
static AnimationSet _gMistShootingGalleryPierceTeaseClip;
static AnimationSet _gMistShootingGalleryPierceWarnClip;
static AnimationSet _gMistShootingGalleryPierceStory3Clip;
static AnimationSet _gMistShootingGalleryPierceStory4Clip;
static AnimationSet _gMistShootingGalleryPierceTalkClip;
static AnimationSet _gMistShootingGalleryPierceTalkEndClip;
static AnimationSet _gMistShootingGalleryPierceCorrectHimselfClip;
static AnimationSet _gMistShootingGalleryPierceCorrectHimselfEndClip;
static AnimationSet _gSoldierBStrideWalkAnimAnim08Clip;
static AnimationSet _gSoldierBStrideWalkAnimAnim09Clip;
static AnimationSet _gSoldierBStrideWalkAnimAnim10Clip;
static AnimationSet _gPierceStory17Clip;
static AnimationSet _gMistShootingGalleryPierceGalleryIdleClip;
static AnimationSet _gMistShootingGalleryPierceGalleryAskClip;
static AnimationSet _gMistShootingGalleryPierceGalleryTalkClip;
static AnimationSet _gMistShootingGalleryPierceGalleryTeaseClip;
static AnimationSet _gMistShootingGalleryPierceGalleryReactClip;
static AnimationSet _gMistShootingGalleryPierceLevelPromptClip;
static AnimationSet _gMistShootingGalleryPierceLevelReplyClip;

static void _actor215100SetConversationCap(s32 enabled);

static void _actor215100GalleryIntroTask(Task* task);
static void _actor215100GalleryRedFlashLoopTask(Task* task);
static void _actor215100StartGalleryMovie(void);
static void _actor215100SpawnGalleryDemoTargetIfEnabled(s32 enabled);
static void _actor215100SetGalleryRedFlashLoop(s32 enabled);
static void _actor215100SelectGalleryRoomLights(s16 useAlternate);
static void _actor215100SetViewRespawnPending(s16 pending);

enum {
    ACTOR_215100_GALLERY_TASK_PRIORITY          = 32,
    ACTOR_215100_PIERCE_BODY_TASK_PRIORITY      = 96,
    ACTOR_215100_PIERCE_PAIR_TASK_PRIORITY      = 192,
    ACTOR_215100_CONVERSATION_BLEND_FRAMES      = 10,
    ACTOR_215100_CONVERSATION_IDLE_BLEND_FRAMES = 8,
};

// CAP command slots and mapped views used by these event scripts.
enum {
    ACTOR_215100_PIERCE_PLACE_INDEX                  = 1,
    ACTOR_215100_SCRIPT_PLAYER_WEAPONS               = 0,
    ACTOR_215100_GALLERY_INTRO_CAP_COMMAND           = 5,
    ACTOR_215100_GALLERY_TRAINING_OFFER_CAP_COMMAND  = 6,
    ACTOR_215100_GALLERY_TRAINING_ACCEPT_CAP_COMMAND = 7,
    ACTOR_215100_PIERCE_STORY_CAP_COMMAND            = 1,
    ACTOR_215100_PIERCE_FIRST_TALK_CAP_COMMAND       = 2,
    ACTOR_215100_PIERCE_SECOND_TALK_CAP_COMMAND      = 3,
    ACTOR_215100_PIERCE_REPEAT_TALK_CAP_COMMAND      = 4,
    ACTOR_215100_GALLERY_MENU_VIEW                   = 15,
    ACTOR_215100_GALLERY_FIXED_TRAINING_VIEW         = 14,
    ACTOR_215100_STORY_RETURN_VIEW                   = 2,
    ACTOR_215100_GALLERY_AMBIENT_COMPONENT           = 80,
    ACTOR_215100_SCRIPT_FADE_FRAMES                  = 30,
    ACTOR_215100_STORY_FADE_RETURN_FRAMES            = 60,
    ACTOR_215100_GALLERY_INTRO_SOUND_SCRIPT          = 0x5114000C,
    ACTOR_215100_GALLERY_DEMO_SOUND_SCRIPT           = 0x5114000D,
    ACTOR_215100_STORY_OPENING_SOUND_SCRIPT          = 0x5114000A,
    ACTOR_215100_STORY_PIERCE_SOUND_SCRIPT           = 0x5114000B,
};

TaskDesc gActor215100GalleryTaskDescs[ACTOR_215100_GALLERY_TASK_COUNT] = {
    { { { TASK_BODY_NONE, ACTOR_215100_GALLERY_TASK_PRIORITY } }, _actor215100GalleryIntroTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_215100_GALLERY_TASK_PRIORITY } }, actor215100GalleryTrainingMenuTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_215100_GALLERY_TASK_PRIORITY } }, _actor215100GalleryRedFlashLoopTask, { .value = 0 } },
};

/// Player animation extension for the gallery's introduction and course menus.
///
/// Entries map to player clip IDs 47..57; 53..56 are NULL and 57 is retained
/// without a live request. Scripts play only 47..52. The native pointer table
/// occupies eleven words; the bank-copy request reads a larger raw image span.
/// Keep these borrowed clips loaded while the equipped weapon bank uses them.
#include "assets/rooms/mist/shooting_gallery/player_gallery_bank.inc"

/// Retained clip-zero request at the start of the gallery's Pierce requests.
///
/// No script plays it, and Pierce's bank slot zero is NULL. The player-bank
/// extension transfer reads all five words as raw data after the clip table;
/// keep this record in its native position without treating it as a set pointer.
static AnimationPlayRequest _gActor215100GalleryPierceClip0Request = {
    .source               = { .index = 1 },
    .animationId          = 0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gallery reset request for Pierce's body clip 18.
///
/// The placed-actor handler reseeds tracks 1..19 without moving the root.
/// It ignores the stored bank selector and collision choice; the actor
/// overlay and its clip data must remain loaded during playback.
static AnimationPlayRequest _gActor215100PierceClip18Request = {
    .source               = { .index = 1 },
    .animationId          = 18,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gallery reset request for Pierce's body clip 19.
///
/// The placed-actor handler reseeds tracks 1..19 without moving the root.
/// It ignores the stored bank selector and collision choice; the actor
/// overlay and its clip data must remain loaded during playback.
static AnimationPlayRequest _gActor215100PierceClip19Request = {
    .source               = { .index = 1 },
    .animationId          = 19,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gallery reset request for Pierce's body clip 20.
///
/// The placed-actor handler reseeds tracks 1..19 without moving the root.
/// It ignores the stored bank selector and collision choice; the actor
/// overlay and its clip data must remain loaded during playback.
static AnimationPlayRequest _gActor215100PierceClip20Request = {
    .source               = { .index = 1 },
    .animationId          = 20,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gallery request for Pierce's body clip 21 with a reset.
///
/// Reseeds body tracks 1..19 without moving the root. The bank selector
/// and collision word are unused by this receiver; the request is consumed
/// during dispatch, and the actor's clip data must stay loaded for playback.
/// The gallery bank-extension transfer also copies this record's first word;
/// retain its native position after the preceding four body requests.
static AnimationPlayRequest _gActor215100PierceClip21Request = {
    .source               = { .index = 1 },
    .animationId          = 21,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gallery request for Pierce's body clip 22 with a reset.
///
/// Reseeds body tracks 1..19 without moving the root. The bank selector
/// and collision word are unused by this receiver; the request is consumed
/// during dispatch, and the actor's clip data must stay loaded for playback.
static AnimationPlayRequest _gActor215100PierceClip22Request = {
    .source               = { .index = 1 },
    .animationId          = 22,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained reset request for Pierce body clip 23, with no playback consumer.
///
/// Follows the live body requests and selects a loaded Pierce bank entry.
/// Keep its five-word record separate from the following player request.
static AnimationPlayRequest _gActor215100PierceClip23Request = {
    .source               = { .index = 1 },
    .animationId          = 23,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained reset preset for Pierce's loaded body clip 24.
///
/// This separate image record has no playback consumer. Reset ignores the
/// zero blend duration; the body receiver ignores bank and collision selectors.
static const AnimationPlayRequest _gActor215100PierceClip24Request SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = 24,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained reset preset for the first gallery player extension clip.
///
/// Clip 47 follows the native bank's clips and selects gallery extension entry
/// zero. This separate image record has no playback consumer; the live gallery
/// request for the same clip uses a six-frame blend. Reset ignores the zero
/// blend duration, and the stored collision choice disables grid participation.
static const AnimationPlayRequest _gActor215100GalleryPlayerClip47ResetRequest SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum { ACTOR_215100_GALLERY_MENU_BLEND_FRAMES = 6 };

/// Gallery player clip 47 request with a 6-frame blend.
///
/// The event interpreter copies the record and selects the equipped weapon
/// bank. Playback takes scripted control and disables world collision.
/// Keep the weapon bank and its borrowed clip data loaded through playback.
/// Requires the extension installed by `_gActor215100GalleryPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100GalleryPlayerClip47Request = {
    .source               = { .index = 1 },
    .animationId          = 47,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_215100_GALLERY_MENU_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gallery player clip 48 request with a reset.
///
/// The event interpreter copies this record and resolves the current weapon
/// bank before dispatch. Playback takes scripted control and disables world
/// collision. Keep the weapon bank and any copied clip data loaded.
/// Requires the extension installed by `_gActor215100GalleryPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100GalleryPlayerClip48Request = {
    .source               = { .index = 1 },
    .animationId          = 48,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gallery player clip 49 request with a reset.
///
/// The event interpreter copies the record and selects the equipped weapon
/// bank. Playback takes scripted control and disables world collision.
/// Keep the weapon bank and its borrowed clip data loaded through playback.
/// Requires the extension installed by `_gActor215100GalleryPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100GalleryPlayerClip49Request = {
    .source               = { .index = 1 },
    .animationId          = 49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gallery player clip 50 request with a reset.
///
/// The event interpreter copies the record and selects the equipped weapon
/// bank. Playback takes scripted control and disables world collision.
/// Keep the weapon bank and its borrowed clip data loaded through playback.
/// Requires the extension installed by `_gActor215100GalleryPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100GalleryPlayerClip50Request = {
    .source               = { .index = 1 },
    .animationId          = 50,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gallery player clip 51 request with a reset.
///
/// The event interpreter copies the record and selects the equipped weapon
/// bank. Playback takes scripted control and disables world collision.
/// Keep the weapon bank and its borrowed clip data loaded through playback.
/// Requires the extension installed by `_gActor215100GalleryPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100GalleryPlayerClip51Request = {
    .source               = { .index = 1 },
    .animationId          = 51,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gallery player clip 52 request with a reset.
///
/// The event interpreter copies this record and resolves the current weapon
/// bank before dispatch. Playback takes scripted control and disables world
/// collision. Keep the weapon bank and any copied clip data loaded.
/// Requires the extension installed by `_gActor215100GalleryPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100GalleryPlayerClip52Request = {
    .source               = { .index = 1 },
    .animationId          = 52,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Installs the gallery's player clips and retained adjacent words into the weapon bank.
///
/// Copies 32 raw words to bank indices 47..78, without starting playback.
/// The first eleven are the clip table; the next 21 cover four complete
/// five-word Pierce requests and the first word of the next request.
/// Their native order and contents are part of this transfer. Those trailing
/// words are not playable clips. The source overlay must stay loaded through
/// dispatch and through playback of the copied pointers.
static AnimationBankCopyRequest _gActor215100GalleryPlayerBankCopyRequest = {
    .sets  = _gActor215100GalleryPlayerAnimationSets,
    .count = ANIMATION_BANK_EXTENSION_CAPACITY,
};

/// Retained reset request for gallery player extension clip 57.
///
/// The gallery extension supplies the clip, but no script dispatches this record.
static AnimationPlayRequest _gActor215100GalleryPlayerClip57Request = {
    .source               = { .index = 1 },
    .animationId          = 57,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gallery player clip 1 request with a reset.
///
/// The event interpreter copies this record and resolves the current weapon
/// bank before dispatch. Playback takes scripted control and disables world
/// collision. Keep the weapon bank and any copied clip data loaded.
static AnimationPlayRequest _gActor215100GalleryPlayerClip1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Gallery player clip 9 request with a reset.
///
/// The event interpreter copies this record and resolves the current weapon
/// bank before dispatch. Playback takes scripted control and disables world
/// collision. Keep the weapon bank and any copied clip data loaded.
static AnimationPlayRequest _gActor215100GalleryPlayerClip9Request = {
    .source               = { .index = 1 },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained gallery animation request for clip 7 with a reset.
///
/// No source or assembly consumer establishes its receiver or visible motion.
/// The stored bank selector is 1 and world-collision choice is disabled.
static AnimationPlayRequest _gActor215100GalleryClip7Request = {
    .source               = { .index = 1 },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Pierce's placement during the gallery briefing and course-menu scenes.
///
/// XYZ are whole room-coordinate units; yaw is a positive quarter turn.
/// The placed-actor handler applies XYZ and yaw without retaining the record.
static ActorTransform _gActor215100GalleryPiercePlacement = {
    .pos = { -0x27CE, 0, 4100, 0 },
    .rot = { 0, ONE / 4, 0, 0 },
};

/// Pierce's placement for the later gallery-demonstration dialogue cues.
///
/// XYZ are whole room-coordinate units; yaw is a positive quarter turn.
/// The actor applies XYZ and yaw during dispatch without retaining the record.
static ActorTransform _gActor215100GalleryPierceDemoPlacement = {
    .pos = { -10140, 0, 4720, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Player placement for gallery briefings and course-menu selection.
///
/// XYZ are whole world-coordinate units; yaw is a positive quarter turn.
/// The player applies XYZ and all three angles during synchronous dispatch.
static ActorTransform _gActor215100GalleryPlayerMenuPlacement = {
    .pos = { -6550, 0, 2950, 0 },
    .rot = { 0, ONE / 4, 0, 0 },
};

/// Player placement for starting a fixed-view course or declining the gallery offer.
///
/// XYZ are whole world-coordinate units; yaw is a positive quarter turn.
/// The player applies XYZ and all three angles during synchronous dispatch.
static ActorTransform _gActor215100GalleryPlayerCoursePlacement = {
    .pos = { -6800, 0, 2950, 0 },
    .rot = { 0, ONE / 4, 0, 0 },
};

/// First-visit gallery movie and CAP-cued training demonstration.
///
/// Fifty-nine commands include END. Installs gallery player clips, starts the
/// movie and CAP command 5, and stages the target, red flashes and both actors.
/// Leaves menu view 15 ready for the offer. Uses `EVENT_SCRIPT_HUD_KEEP`;
/// the actor/gallery overlays, callbacks and borrowed data stay loaded.
static EvsCommand _gActor215100GalleryIntroScript[59] = {
    // Install player clips and start the movie before the CAP-cued demonstration.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_HIDE_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor215100GalleryPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor215100StartGalleryMovie }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_215100_GALLERY_INTRO_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip9Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100GalleryPlayerMenuPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100GalleryPiercePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = ACTOR_215100_SCRIPT_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_215100_GALLERY_INTRO_SOUND_SCRIPT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SpawnGalleryDemoTargetIfEnabled }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetGalleryRedFlashLoop }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_215100_GALLERY_DEMO_SOUND_SCRIPT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetGalleryRedFlashLoop }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 44 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip21Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip22Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100GalleryPierceDemoPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 44 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_215100_GALLERY_MENU_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100GalleryPiercePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip path restoring the gallery introduction's menu staging under a fade.
///
/// Seventeen commands include END. Cleans up playback, requests view respawn,
/// stops red flashes and restores actor placement/animation in view 15.
/// Runs with the introduction's resources loaded and keeps the HUD suppressed.
static EvsCommand _gActor215100GalleryIntroSkipScript[17] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_215100_GALLERY_MENU_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor215100SetViewRespawnPending }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetGalleryRedFlashLoop }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100GalleryPiercePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100GalleryPlayerMenuPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Starts the training-offer caption after the gallery introduction.
///
/// Five commands include END. CAP command 6 supplies the reply examined by
/// the introduction task. Retains ambient/HUD setup through the next handoff;
/// CAP and both overlays must remain loaded until the cue and wait complete.
static EvsCommand _gActor215100GalleryTrainingOfferScript[5] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_215100_GALLERY_TRAINING_OFFER_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Plays the accepted introduction offer and prepares the player's menu pose.
///
/// Six commands include END. Starts CAP command 7, installs gallery player
/// clips and blends into clip 47 before waiting for the cue. Requires live
/// gallery/player/CAP resources and keeps the HUD suppressed for the menu.
static EvsCommand _gActor215100GalleryIntroAcceptTrainingScript[6] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_215100_GALLERY_TRAINING_ACCEPT_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor215100GalleryPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor215100GalleryWaitTrainingAcceptanceScript[3] = {
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor215100GalleryDeclineTrainingScript[18] = {
    // Finish the CAP-cued refusal exchange with Pierce.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor215100GalleryPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Return to the ordinary room view and placement under the fade.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_215100_GALLERY_RETURN_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100GalleryPlayerCoursePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Restore the HUD and end scripted player control after the room is visible.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor215100GalleryPrepareTrainingMenuScript[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_HIDE_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip9Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100GalleryPlayerMenuPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100GalleryPiercePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_215100_GALLERY_MENU_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor215100GalleryOfferTrainingMenuScript[13] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_HIDE_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor215100GalleryPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100GalleryPlayerMenuPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100GalleryPiercePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor215100GalleryStartFixedViewTrainingScript[8] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100GalleryPlayerCoursePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_215100_GALLERY_FIXED_TRAINING_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor215100SelectGalleryRoomLights }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor215100GalleryStartFreeMovementTrainingScript[9] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100GalleryPlayerClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100GalleryPlayerCoursePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_215100_GALLERY_RETURN_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor215100GalleryConfirmCourseScript[6] = {
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/rooms/mist/shooting_gallery/pierce_clips_2.inc"

/// Player animation extension for Pierce's story and conversation scenes.
///
/// Entries map to player clip IDs 47..66; 57..58 are NULL and 59 is retained
/// without a live request. Scripts play 47..56 or 60..66. The native pointer
/// table occupies twenty words; the bank-copy request reads a larger raw image
/// span. Keep these borrowed clips loaded while the weapon bank uses them.
#include "assets/rooms/mist/shooting_gallery/player_conversation_bank.inc"

/// Retained clip-zero request at the start of the conversation's Pierce requests.
///
/// No script plays it, and Pierce's bank slot zero is NULL. The conversation
/// bank-extension transfer reads all five words as raw data after the clip table;
/// retain this record's native position without treating it as a set pointer.
static AnimationPlayRequest _gActor215100ConversationPierceClip0Request = {
    .source               = { .index = 1 },
    .animationId          = 0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation request for Pierce's body clip 1 with a reset.
///
/// Reseeds body tracks 1..19 without moving the root. The bank selector
/// and collision word are unused by this receiver; the request is consumed
/// during dispatch, and the actor's clip data must stay loaded for playback.
/// The conversation bank-extension transfer also copies this complete record;
/// retain its native position after the player clip table and prior request.
static AnimationPlayRequest _gActor215100PierceClip1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation request for Pierce's body clip 2 with a reset.
///
/// Reseeds body tracks 1..19 without moving the root. The bank selector
/// and collision word are unused by this receiver; the request is consumed
/// during dispatch, and the actor's clip data must stay loaded for playback.
/// The conversation bank-extension transfer also copies the first two words;
/// retain its native position after the player clip table and prior request.
static AnimationPlayRequest _gActor215100PierceClip2Request = {
    .source               = { .index = 1 },
    .animationId          = 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation reset request for Pierce's body clip 3.
///
/// The placed-actor handler reseeds tracks 1..19 without moving the root.
/// It ignores the stored bank selector and collision choice; the actor
/// overlay and its clip data must remain loaded during playback.
static AnimationPlayRequest _gActor215100PierceClip3Request = {
    .source               = { .index = 1 },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation request for Pierce's body clip 4 with a reset.
///
/// Reseeds body tracks 1..19 without moving the root. The bank selector
/// and collision word are unused by this receiver; the request is consumed
/// during dispatch, and the actor's clip data must stay loaded for playback.
static AnimationPlayRequest _gActor215100PierceClip4Request = {
    .source               = { .index = 1 },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation request for Pierce's body clip 5 with a reset.
///
/// Reseeds body tracks 1..19 without moving the root. The bank selector
/// and collision word are unused by this receiver; the request is consumed
/// during dispatch, and the actor's clip data must stay loaded for playback.
static AnimationPlayRequest _gActor215100PierceClip5Request = {
    .source               = { .index = 1 },
    .animationId          = 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation request for Pierce's body clip 6 with a reset.
///
/// Reseeds body tracks 1..19 without moving the root. The bank selector
/// and collision word are unused by this receiver; the request is consumed
/// during dispatch, and the actor's clip data must stay loaded for playback.
static AnimationPlayRequest _gActor215100PierceClip6Request = {
    .source               = { .index = 1 },
    .animationId          = 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation request for Pierce's body clip 7 with a reset.
///
/// Reseeds body tracks 1..19 without moving the root. The bank selector
/// and collision word are unused by this receiver; the request is consumed
/// during dispatch, and the actor's clip data must stay loaded for playback.
static AnimationPlayRequest _gActor215100PierceClip7Request = {
    .source               = { .index = 1 },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation request for Pierce's body clip 8 with a reset.
///
/// Reseeds body tracks 1..19 without moving the root. The bank selector
/// and collision word are unused by this receiver; the request is consumed
/// during dispatch, and the actor's clip data must stay loaded for playback.
static AnimationPlayRequest _gActor215100PierceClip8Request = {
    .source               = { .index = 1 },
    .animationId          = 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum { ACTOR_215100_PIERCE_CONVERSATION_BLEND_FRAMES = 8 };

/// Conversation request for Pierce's body clip 9 with an 8-frame blend.
///
/// Reseeds body tracks 1..19 without moving the root. The bank selector
/// and collision word are unused by this receiver; the request is consumed
/// during dispatch, and the actor's clip data must stay loaded for playback.
static AnimationPlayRequest _gActor215100PierceClip9BlendRequest = {
    .source               = { .index = 1 },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_215100_PIERCE_CONVERSATION_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation request for Pierce's body clip 10 with an 8-frame blend.
///
/// Reseeds body tracks 1..19 without moving the root. The bank selector
/// and collision word are unused by this receiver; the request is consumed
/// during dispatch, and the actor's clip data must stay loaded for playback.
static AnimationPlayRequest _gActor215100PierceClip10BlendRequest = {
    .source               = { .index = 1 },
    .animationId          = 10,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_215100_PIERCE_CONVERSATION_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation request for Pierce's body clip 11 with an 8-frame blend.
///
/// Reseeds body tracks 1..19 without moving the root. The bank selector
/// and collision word are unused by this receiver; the request is consumed
/// during dispatch, and the actor's clip data must stay loaded for playback.
static AnimationPlayRequest _gActor215100PierceClip11BlendRequest = {
    .source               = { .index = 1 },
    .animationId          = 11,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_215100_PIERCE_CONVERSATION_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation reset request for Pierce's body clip 12.
///
/// The placed-actor handler reseeds tracks 1..19 without moving the root.
/// It ignores the stored bank selector and collision choice; the actor
/// overlay and its clip data must remain loaded during playback.
static AnimationPlayRequest _gActor215100PierceClip12Request = {
    .source               = { .index = 1 },
    .animationId          = 12,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation request for Pierce's body clip 12 with a 10-frame blend.
///
/// Reseeds body tracks 1..19 without moving the root. The bank selector
/// and collision word are unused by this receiver; the request is consumed
/// during dispatch, and the actor's clip data must stay loaded for playback.
static AnimationPlayRequest _gActor215100PierceClip12BlendRequest = {
    .source               = { .index = 1 },
    .animationId          = 12,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_215100_CONVERSATION_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation reset request for Pierce's body clip 13.
///
/// The placed-actor handler reseeds tracks 1..19 without moving the root.
/// It ignores the stored bank selector and collision choice; the actor
/// overlay and its clip data must remain loaded during playback.
static AnimationPlayRequest _gActor215100PierceClip13Request = {
    .source               = { .index = 1 },
    .animationId          = 13,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation reset request for Pierce's body clip 14.
///
/// The placed-actor handler reseeds tracks 1..19 without moving the root.
/// It ignores the stored bank selector and collision choice; the actor
/// overlay and its clip data must remain loaded during playback.
static AnimationPlayRequest _gActor215100PierceClip14Request = {
    .source               = { .index = 1 },
    .animationId          = 14,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation request for Pierce's body clip 15 with a reset.
///
/// Reseeds body tracks 1..19 without moving the root. The bank selector
/// and collision word are unused by this receiver; the request is consumed
/// during dispatch, and the actor's clip data must stay loaded for playback.
static AnimationPlayRequest _gActor215100PierceClip15Request = {
    .source               = { .index = 1 },
    .animationId          = 15,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained duplicate reset request for conversation player extension clip 47.
///
/// No script dispatches this record; the following live request has identical
/// contents. They are distinct storage and remain separate in the native image.
static AnimationPlayRequest _gActor215100ConversationPlayerClip47RetainedRequest = {
    .source               = { .index = 1 },
    .animationId          = 47,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 47 request with a reset.
///
/// The event interpreter copies the record and selects the equipped weapon
/// bank. Playback takes scripted control and disables world collision.
/// Keep the weapon bank and its borrowed clip data loaded through playback.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip47Request = {
    .source               = { .index = 1 },
    .animationId          = 47,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 48 request with a reset.
///
/// The event interpreter copies the record and selects the equipped weapon
/// bank. Playback takes scripted control and disables world collision.
/// Keep the weapon bank and its borrowed clip data loaded through playback.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip48Request = {
    .source               = { .index = 1 },
    .animationId          = 48,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 49 request with a reset.
///
/// The event interpreter copies the record and selects the equipped weapon
/// bank. Playback takes scripted control and disables world collision.
/// Keep the weapon bank and its borrowed clip data loaded through playback.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip49Request = {
    .source               = { .index = 1 },
    .animationId          = 49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 50 request with a reset.
///
/// The event interpreter copies the record and selects the equipped weapon
/// bank. Playback takes scripted control and disables world collision.
/// Keep the weapon bank and its borrowed clip data loaded through playback.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip50Request = {
    .source               = { .index = 1 },
    .animationId          = 50,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 51 request with a reset.
///
/// The event interpreter copies the record and selects the equipped weapon
/// bank. Playback takes scripted control and disables world collision.
/// Keep the weapon bank and its borrowed clip data loaded through playback.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip51Request = {
    .source               = { .index = 1 },
    .animationId          = 51,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 52 request with a reset.
///
/// The event interpreter copies the record and selects the equipped weapon
/// bank. Playback takes scripted control and disables world collision.
/// Keep the weapon bank and its borrowed clip data loaded through playback.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip52Request = {
    .source               = { .index = 1 },
    .animationId          = 52,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 53 request with a reset.
///
/// The event interpreter copies the record and selects the equipped weapon
/// bank. Playback takes scripted control and disables world collision.
/// Keep the weapon bank and its borrowed clip data loaded through playback.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip53Request = {
    .source               = { .index = 1 },
    .animationId          = 53,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 54 request with a reset.
///
/// The event interpreter copies the record and selects the equipped weapon
/// bank. Playback takes scripted control and disables world collision.
/// Keep the weapon bank and its borrowed clip data loaded through playback.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip54Request = {
    .source               = { .index = 1 },
    .animationId          = 54,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Story-scene reset request for conversation player extension clip 55.
///
/// Event playback copies it and resolves the equipped weapon bank before
/// dispatch, taking scripted control and disabling world collision. The
/// conversation extension and borrowed clip data must remain loaded.
static AnimationPlayRequest _gActor215100ConversationPlayerClip55Request = {
    .source               = { .index = 1 },
    .animationId          = 55,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Story-scene reset request for conversation player extension clip 56.
///
/// Event playback copies it and resolves the equipped weapon bank before
/// dispatch, taking scripted control and disabling world collision. The
/// conversation extension and borrowed clip data must remain loaded.
static AnimationPlayRequest _gActor215100ConversationPlayerClip56Request = {
    .source               = { .index = 1 },
    .animationId          = 56,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained conversation player clip 59 request with a ten-frame blend.
///
/// The conversation extension supplies this clip, but no script dispatches it.
static AnimationPlayRequest _gActor215100ConversationPlayerClip59Request = {
    .source               = { .index = 1 },
    .animationId          = 59,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_215100_CONVERSATION_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 60 request with a 10-frame blend.
///
/// The event interpreter copies this record and resolves the current weapon
/// bank before dispatch. Playback takes scripted control and disables world
/// collision. Keep the weapon bank and any copied clip data loaded.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip60Request = {
    .source               = { .index = 1 },
    .animationId          = 60,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_215100_CONVERSATION_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 61 request with a 10-frame blend.
///
/// The event interpreter copies this record and resolves the current weapon
/// bank before dispatch. Playback takes scripted control and disables world
/// collision. Keep the weapon bank and any copied clip data loaded.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip61Request = {
    .source               = { .index = 1 },
    .animationId          = 61,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_215100_CONVERSATION_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 62 request with a 10-frame blend.
///
/// The event interpreter copies this record and resolves the current weapon
/// bank before dispatch. Playback takes scripted control and disables world
/// collision. Keep the weapon bank and any copied clip data loaded.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip62Request = {
    .source               = { .index = 1 },
    .animationId          = 62,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_215100_CONVERSATION_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 63 request with a 10-frame blend.
///
/// The event interpreter copies this record and resolves the current weapon
/// bank before dispatch. Playback takes scripted control and disables world
/// collision. Keep the weapon bank and any copied clip data loaded.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip63Request = {
    .source               = { .index = 1 },
    .animationId          = 63,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_215100_CONVERSATION_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 64 request with a 10-frame blend.
///
/// The event interpreter copies this record and resolves the current weapon
/// bank before dispatch. Playback takes scripted control and disables world
/// collision. Keep the weapon bank and any copied clip data loaded.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip64Request = {
    .source               = { .index = 1 },
    .animationId          = 64,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_215100_CONVERSATION_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 65 request with a 10-frame blend.
///
/// The event interpreter copies this record and resolves the current weapon
/// bank before dispatch. Playback takes scripted control and disables world
/// collision. Keep the weapon bank and any copied clip data loaded.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip65Request = {
    .source               = { .index = 1 },
    .animationId          = 65,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_215100_CONVERSATION_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 66 request with a 10-frame blend.
///
/// The event interpreter copies this record and resolves the current weapon
/// bank before dispatch. Playback takes scripted control and disables world
/// collision. Keep the weapon bank and any copied clip data loaded.
/// Requires the extension installed by `_gActor215100ConversationPlayerBankCopyRequest`.
static AnimationPlayRequest _gActor215100ConversationPlayerClip66Request = {
    .source               = { .index = 1 },
    .animationId          = 66,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_215100_CONVERSATION_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Installs the conversation's player clips and retained adjacent words into the weapon bank.
///
/// Copies 32 raw words to bank indices 47..78, without starting playback.
/// The first twenty are the clip table; the next twelve cover two complete
/// five-word Pierce requests and the first two words of the next request.
/// Their native order and contents are part of this transfer. Those trailing
/// words are not playable clips. The source overlay must stay loaded through
/// dispatch and through playback of the copied pointers.
static AnimationBankCopyRequest _gActor215100ConversationPlayerBankCopyRequest = {
    .sets  = _gActor215100ConversationPlayerAnimationSets,
    .count = ANIMATION_BANK_EXTENSION_CAPACITY,
};

/// Pierce's opening placement in the gallery story scene.
///
/// XYZ are whole room units; yaw is a positive quarter turn. The placed-actor
/// receiver applies XYZ and yaw during dispatch without retaining the record.
static ActorTransform _gActor215100StoryPierceOpeningPlacement = {
    .pos = { -10230, 0, 4640, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Pierce's placement for the story scene's exchange with the player.
///
/// XYZ are whole room units; signed yaw uses 4096 units per turn. The actor
/// applies XYZ and yaw during dispatch without retaining the record.
static ActorTransform _gActor215100StoryPierceDialoguePlacement = {
    .pos = { -11200, 0, 3000, 0 },
    .rot = { 0, -2218, 0, 0 },
};

/// Pierce's placement after the full or skipped story conversation.
///
/// XYZ are whole room-coordinate units; yaw is a negative half turn.
/// The actor applies XYZ and yaw during dispatch without retaining the record.
static ActorTransform _gActor215100ConversationPierceReturnPlacement = {
    .pos = { -10230, 0, 5000, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 },
};

/// Player placement for the story scene's exchange with Pierce.
///
/// XYZ are whole world units and signed Euler angles use 4096 units per turn.
/// The player applies both vectors synchronously without retaining the record.
static ActorTransform _gActor215100StoryPlayerDialoguePlacement = {
    .pos = { -10590, 0, 2100, 0 },
    .rot = { 0, -56, 0, 0 },
};

/// Player placement after the full or skipped story conversation.
///
/// XYZ are whole world-coordinate units and all Euler angles are zero.
/// The player consumes both vectors during dispatch before scripted control ends.
static ActorTransform _gActor215100ConversationPlayerReturnPlacement = {
    .pos = { -10746, 0, 3972, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Conversation player clip 1 request with a reset.
///
/// The event interpreter copies the record and selects the equipped weapon
/// bank. Playback takes scripted control and disables world collision.
/// Keep the weapon bank and its borrowed clip data loaded through playback.
static AnimationPlayRequest _gActor215100ConversationPlayerClip1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Conversation player clip 1 request with a 8-frame blend.
///
/// The event interpreter copies this record and resolves the current weapon
/// bank before dispatch. Playback takes scripted control and disables world
/// collision. Keep the weapon bank and any copied clip data loaded.
static AnimationPlayRequest _gActor215100ConversationPlayerClip1BlendRequest = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_215100_CONVERSATION_IDLE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

EvsCommand gActor215100PierceStoryScript[117] = {
    // Switch CAP resources and install the conversation's player clips under the fade.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_215100_PIERCE_STORY_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip1BlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = ACTOR_215100_SCRIPT_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_215100_SCRIPT_PLAYER_WEAPONS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = ACTOR_215100_STORY_FADE_RETURN_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor215100ConversationPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100StoryPierceOpeningPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_215100_STORY_OPENING_SOUND_SCRIPT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 112 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_215100_STORY_PIERCE_SOUND_SCRIPT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip12Request } }, { .value = 0 } },
    // Reposition both actors for the player's CAP-cued replies.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100StoryPierceDialoguePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100StoryPlayerDialoguePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 42 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 42 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip55Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip10BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip11BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip12BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip15Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_215100_GALLERY_AMBIENT_COMPONENT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip12BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip55Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip9BlendRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_215100_SCRIPT_PLAYER_WEAPONS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_215100_STORY_RETURN_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100ConversationPierceReturnPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100ConversationPlayerReturnPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor215100PierceStorySkipScript[15] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_215100_STORY_RETURN_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_PAIR_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_215100_SCRIPT_PLAYER_WEAPONS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100ConversationPierceReturnPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor215100ConversationPlayerReturnPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// First optional Pierce conversation, using the alternate CAP resource's command 2.
///
/// Eleven commands include END. Blends the player to clip 1, sequences Pierce
/// clips 13/14/12 at the caption cue, then ends scripted control and resets CAP.
/// Start with `EVENT_SCRIPT_HUD_HIDE_RESTORE` and keep both overlays loaded.
static EvsCommand _gActor215100PierceFirstTalkScript[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_215100_PIERCE_FIRST_TALK_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip1BlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip14Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Second optional Pierce conversation with the conversation player clip extension.
///
/// Forty-three commands include END. Starts alternate CAP command 3 and
/// sequences player clips 60..66 with Pierce's replies. Ends scripted control
/// and resets CAP; `EVENT_SCRIPT_HUD_HIDE_RESTORE` restores the HUD. Both
/// overlays and the weapon bank's borrowed clip data stay loaded.
static EvsCommand _gActor215100PierceSecondTalkScript[43] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor215100ConversationPlayerBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_215100_PIERCE_SECOND_TALK_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip1BlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip14Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip60Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip61Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip14Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip62Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip63Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip64Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip14Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip65Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip66Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Repeat optional Pierce conversation, using the alternate CAP resource's command 4.
///
/// Eleven commands include END. Blends the player to clip 1, sequences Pierce
/// clips 13/14/12 at the caption cue, then ends scripted control and resets CAP.
/// Start with `EVENT_SCRIPT_HUD_HIDE_RESTORE` and keep both overlays loaded.
static EvsCommand _gActor215100PierceRepeatTalkScript[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_215100_PIERCE_REPEAT_TALK_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor215100ConversationPlayerClip1BlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip14Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_215100_PIERCE_PLACE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor215100PierceClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "../../shared/room/cap_captions_settings.inc.c"

enum { CAP_CAPTION_SCHEDULE_TASK_PRIORITY = 32 };

/// Retained bodyless task recipe for the scene-clock caption schedule.
///
/// No code spawns it. The first spawn argument's low signed halfword would
/// select the bottom text baseline in screen pixels; the second is unused.
/// It needs a loaded caption file with every scheduled command/key, which the
/// gallery's selected caption resource does not provide. The owner must kill
/// the task before releasing that file or this overlay; it never ends itself.
static TaskDesc _gCapCaptionScheduleTaskDesc = {
    { { TASK_BODY_NONE, CAP_CAPTION_SCHEDULE_TASK_PRIORITY } }, _capCaptionRunSchedule, { .value = 0 }
};

#include "../../shared/room/cap_captions_schedule.inc.c"

#include "assets/characters/pierce/body.inc"

#include "assets/characters/pierce/glasses.inc"

#include "assets/rooms/mist/shooting_gallery/pierce_clips_3.inc"

/// Pierce's synchronous animation, paired-draw, placement, command and walk messages.
///
/// Five handlers followed by `TASK_MESSAGE_TABLE_END`. Installed on the body task
/// after its work and paired model exist; borrowed for that task's lifetime.
/// Animation IDs must name loaded body clips (1..15 or 18..24). Draw requests
/// use `ACTOR_MESSAGE_PAIR_`* bits. Placement applies XYZ and yaw; a walk target
/// records heading/travel in work without starting a clip. Both take borrowed
/// `ActorTransform` payloads in whole room units, with 4096 angle units per turn.
/// Actor commands are accepted without action; unsupported IDs return zero.
static TaskMessageEntry _gActor215100PierceMessageTable[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _pacedWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _pacedWalkSetPairModelDraw },
    { ACTOR_MESSAGE_PLACE, _pacedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor215100IgnoreCommand },
    { ACTOR_MESSAGE_WALK_TO, _pacedWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor215100PierceTaskDescs[ACTOR_215100_PIERCE_TASK_COUNT] = {
    { { { TASK_BODY_TMD, ACTOR_215100_PIERCE_BODY_TASK_PRIORITY } }, _actor215100PierceTask, { .model = &_gPierceBody } },
    { { { TASK_BODY_TMD, ACTOR_215100_PIERCE_PAIR_TASK_PRIORITY } }, _pacedWalkSubModelTask, { .model = &_gPierceGlasses } },
};

/// Pierce's body animation bank, indexed directly by scripted clip ID.
///
/// Clips 1..15 and 18..24 are loaded; 0, 16 and 17 are NULL. Spawn starts
/// clip 12, and message playback rejects only IDs >=25, so callers must also
/// exclude negative and unloaded IDs. The context and slots borrow this table
/// and its twenty-track clips for the lifetime of Pierce's task.
#include "assets/rooms/mist/shooting_gallery/pierce_bank.inc"

/// Owned handle of the gallery introduction's repeated red-flash task.
///
/// NULL means no published loop. Stop kills the published task and clears this
/// pointer; another start overwrites it without killing an existing task.
/// Keep both actor and gallery overlays loaded until the owner stops the loop.
static Task* _gActor215100GalleryRedFlashLoopTask = NULL;

/// Borrowed relocated command-reference entries of the loaded CAP resource.
///
/// Indexed by a nonnegative signed-halfword command index below the file count.
/// The file owns the entries and all referenced sequences through caption use.
static const CapCommandRef* _gCapCaptionCommandRefs = NULL;

/// Glyph and title cells borrowed read-only from the selected loaded CAP file.
///
/// Text uses low-ten-bit indices; titles use low-byte selectors minus one.
/// Every used index must exist in the file, which must remain loaded while
/// captions are measured or drawn. Relocation republishes this pointer.
static const TextGlyphCell* _gCapCaptionGlyphCells = NULL;

/// Selected CAP sequence command and its following text records.
///
/// Borrowed from the loaded CAP file, which must remain live through selection
/// and drawing. Slot zero is the sequence command; text records start at one
/// and end at a record whose text reference is `CAP_TEXT_REF_END`. NULL hides
/// captions. Selection requires a matching nonterminal key for measurement.
static CapSequenceRecord* _gCapCaptionSequence = NULL;

/// Biased left X of the selected caption's widest closed line, in screen pixels.
///
/// Cached as a signed halfword from (320 - width) / 2 - 5. Drawing uses its low
/// halfword for packet coordinates; long lines may place it left of the screen.
static s16 _gCapCaptionBlockLeftX = 0;

/// First baseline of the selected caption block, in screen pixels.
///
/// Cached as a signed halfword from the bottom baseline minus subsequent
/// closed-line heights. Drawing subtracts the caption draw-origin Y.
static s16 _gCapCaptionFirstBaselineY = 0;

/// Selected caption's bottom baseline in screen pixels.
///
/// Selection narrows the caller's word to a signed halfword. This anchors the
/// first-baseline calculation and the caption box's bottom edge.
static s16 _gCapCaptionBottomBaselineY = 0;

/// Selected text-record slot within the current CAP sequence.
///
/// A signed-halfword index counting twelve-byte records; slot zero is the
/// command, so selected text starts at one. The selected key must exist before
/// the terminal slot and its index must fit in 1..32767 for metric calculation.
static s16 _gCapCaptionRecordIndex = 0;

/// Height of the selected caption's closed lines, in screen pixels.
///
/// Each line break commits its tallest nonnegative glyph height plus two,
/// or two pixels for an empty line. The cached sum narrows to a signed halfword;
/// an unfinished last line contributes nothing. Used to size the caption box.
static s16 _gCapCaptionBlockHeight = 0;

/// Selected caption variant key, compared with each record's unsigned byte key.
///
/// Valid text selections use 0..255; the signed-halfword input is retained.
static s16 _gCapCaptionSelectedKey = 0;

/// Continuation triangle's left X, retaining the low halfword of draw-coordinate pixels.
///
/// Updated at each text line break to the preceding pen X plus four. The
/// triangle spans seven pixels to its right; no position is updated without a break.
static u16 _gCapCaptionCaretLeftX = 0;

/// Continuation triangle's tip Y, retaining the low halfword of draw-coordinate pixels.
///
/// Updated at each text line break to the preceding baseline minus two.
/// Its upper edge is seven pixels above this tip; no VRAM Y offset is applied.
static u16 _gCapCaptionCaretTipY = 0;

/// Per-instance caret countdown and its uninterpreted retained bytes.
///
/// Selection resets `drawsLeft` to `CAP_CAPTION_CARET_DELAY_DRAWS`. Only eligible
/// caret calls consume it; all initializer bytes remain stored verbatim.
static CapCaptionCaretDelayStorage _gCapCaptionCaretDelayStorage = { 0, { 35, 192, 0 } };

s32 gActor215100GallerySelectedCourseLevel;

/// Owned copy of the deferred gallery room-exit request.
///
/// Executing exit requests replace all eight bytes before confirmation or
/// transition playback. The queued room-exit decision uses area, warp and
/// room once CAP finishes; queries never replace this copy. Its destination
/// is valid only for that pending decision, while this actor overlay is loaded.
RoomEventMsg _gActor215100GalleryPendingExitRequest;

/// Holds the training scene through its exit reply and queues the decision task.
static inline void _actor215100StartGalleryExitPrompt(void)
{
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
    capRunCommand(ACTOR_215100_GALLERY_EXIT_CONFIRM_COMMAND, CAP_PLAYBACK_IN_PLACE);
    gCapPreserveActorControl = ACTOR_215100_GALLERY_EXIT_KEEP_ACTORS_HELD;
    taskSpawnFromTable(gActor215100GalleryDecisionTaskDescs, ACTOR_215100_GALLERY_EXIT_DECISION_TASK, ACTOR_215100_GALLERY_END_TRAINING, 0);
}

void actor215100CheckGalleryExitInput(void)
{
    enum {
        ACTOR_215100_GALLERY_EXIT_INPUT_COOLDOWN_TICKS = 60,
        ACTOR_215100_GALLERY_EXIT_INPUT_VIEW           = 18,
        ACTOR_215100_GALLERY_EXIT_ZONE_MAX_X           = -6150,
        ACTOR_215100_GALLERY_EXIT_ZONE_MIN_Z           = 4301,
        ACTOR_215100_GALLERY_EXIT_ZONE_END_Z           = 5700,
        ACTOR_215100_GALLERY_EXIT_UP_YAW_START         = 0xA01,
        ACTOR_215100_GALLERY_EXIT_DOWN_YAW_START       = 0x201,
        ACTOR_215100_GALLERY_EXIT_YAW_WINDOW           = 0x3FF,
        ACTOR_215100_GALLERY_YAW_MASK                  = 0xFFF,
    };
    Task*      playerTask;
    GameActor* player;
    GfxCoord*  rootCoord;
    s32        rootZ;
    s32        yaw;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    player     = playerTask->work;
    rootCoord  = playerTask->extra.tmd->coords;
    if (gActor215100GalleryTrainingActive != 0) {
        if (gActor215100GallerySelectedCourseLevel >= ACTOR_215100_GALLERY_FIRST_FREE_MOVEMENT_COURSE) {
            if (gGameSession->location.view == ACTOR_215100_GALLERY_EXIT_INPUT_VIEW) {
                mistShootingGallerySetTrainingBarrierLowered(0);
                gActor215100GalleryExitInputBlocked = 0;
            }
            // Rearm the interaction only after the attachment action has settled.
            if ((u32)((u8)gAttachmentState.mode - ATTACHMENT_MODE_ARMED) < 2U) {
                gActor215100GalleryExitInputCooldownTicks = ACTOR_215100_GALLERY_EXIT_INPUT_COOLDOWN_TICKS;
            }
            if (gActor215100GalleryExitInputCooldownTicks != 0) {
                gActor215100GalleryExitInputCooldownTicks -= 1;
                return;
            }
            if ((player->mode != GAME_ACTOR_MODE_SCRIPTED) && (capIsBusy() == 0) && (gActor215100GalleryExitInputBlocked == 0) &&
                (gPlayerActorStateTickHeld == 0) && (rootCoord->coord.t[0] < ACTOR_215100_GALLERY_EXIT_ZONE_MAX_X)) {
                rootZ = rootCoord->coord.t[2];
                if (rootZ < ACTOR_215100_GALLERY_EXIT_ZONE_END_Z) {
                    if ((rootZ >= ACTOR_215100_GALLERY_EXIT_ZONE_MIN_Z) && (gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                        yaw = (u16)player->rotation.vy & ACTOR_215100_GALLERY_YAW_MASK;
                        if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_UP) != 0) {
                            if ((u32)(yaw - ACTOR_215100_GALLERY_EXIT_UP_YAW_START) < (u32)ACTOR_215100_GALLERY_EXIT_YAW_WINDOW) {
                                _actor215100StartGalleryExitPrompt();
                            }
                        }
                        if ((padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_DOWN) != 0) && ((u32)(yaw - ACTOR_215100_GALLERY_EXIT_DOWN_YAW_START) < (u32)ACTOR_215100_GALLERY_EXIT_YAW_WINDOW)) {
                            _actor215100StartGalleryExitPrompt();
                        }
                    }
                }
            }
        }
    }
}

void actor215100GalleryExitDecisionTask(Task* task)
{
    enum {
        ACTOR_215100_GALLERY_EXIT_WAIT_REPLY         = 0,
        ACTOR_215100_GALLERY_EXIT_WAIT_TRANSITION    = 1,
        ACTOR_215100_GALLERY_EXIT_REPLY_CONTINUE     = 1,
        ACTOR_215100_GALLERY_RETURN_RESOURCE_VARIANT = 3,
        ACTOR_215100_GALLERY_RETURN_SCENE_EVENT      = 1,
        ACTOR_215100_GALLERY_RETURN_SPRITE_VARIANT   = 1,
        ACTOR_215100_GALLERY_RETURN_SOUND            = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 5),
    };
    switch (task->state) {
        case ACTOR_215100_GALLERY_EXIT_WAIT_REPLY:
            if (task->spawnArg1.value == ACTOR_215100_GALLERY_COMMIT_ROOM_EXIT) {
                task->state = ACTOR_215100_GALLERY_EXIT_WAIT_TRANSITION;
                break;
            }
            if (capIsBusy() != 0) {
                break;
            }
            if (capGetVariantKey() == ACTOR_215100_GALLERY_EXIT_REPLY_CONTINUE) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
                break;
            }
            if (task->spawnArg1.value != 0) {
                if (gameFlagGetNibble(GAME_FLAG_0ED) != 0) {
                    capRunCommandWithTransition(ACTOR_215100_GALLERY_EXIT_TRANSITION_COMMAND);
                }
                gGameSession->battleResetPending = 1;
                task->state                     += 1;
                break;
            }
            if (gActor215100GallerySelectedCourseLevel == ACTOR_215100_GALLERY_FIRST_FREE_MOVEMENT_COURSE) {
                gAttachmentState.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
            }
            gActor215100GalleryTrainingActive = 0;
            mistShootingGallerySetTrainingBarrierLowered(1);
            gActor215100GalleryExitInputBlocked = 1;
            taskCallExit(gMistShootingGalleryControllerTask);
            gGameSession->battleResetPending = 1;
            gGameSession->flowFlags         |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
            sndEvtRequestMidiStop(0, ACTOR_215100_GALLERY_MUSIC_FADE_AUDIO_TICKS);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            taskKill(task);
            break;
        case ACTOR_215100_GALLERY_EXIT_WAIT_TRANSITION:
            // Restore the carried loadout before reloading the retained destination.
            if (capIsBusy() == 0) {
                gPlayerStatus.resourceVariant                       = ACTOR_215100_GALLERY_RETURN_RESOURCE_VARIANT;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACTOR_215100_GALLERY_RETURN_SCENE_EVENT;
                inventoryRestoreCarriedLoadout();
                gGameSession->hideHud = 1;
                sndEvtRequestScriptStart(ACTOR_215100_GALLERY_RETURN_SOUND, 0, 0);
                gDisplayState.spriteVariant                                 = ACTOR_215100_GALLERY_RETURN_SPRITE_VARIANT;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = _gActor215100GalleryPendingExitRequest.areaId;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = _gActor215100GalleryPendingExitRequest.warp;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = _gActor215100GalleryPendingExitRequest.roomState;
                taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
                taskKill(task);
            }
            break;
    }
}

void actor215100GalleryAbortDecisionTask(Task* task)
{
    enum {
        ACTOR_215100_GALLERY_ABORT_WAIT_REPLY  = 0,
        ACTOR_215100_GALLERY_ABORT_RETURN_VIEW = 1,
        ACTOR_215100_GALLERY_ABORT_FINISH      = 2,
        ACTOR_215100_GALLERY_ABORT_REPLY_STOP  = 2,
    };
    GameActor* player;

    player = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work;
    switch (task->state) {
        case ACTOR_215100_GALLERY_ABORT_WAIT_REPLY:
            if (capIsBusy() != 0) {
                break;
            }
            if (capGetVariantKey() == ACTOR_215100_GALLERY_ABORT_REPLY_STOP) {
                taskCallExit(gMistShootingGalleryControllerTask);
                task->state++;
            } else {
                taskKill(task);
            }
            break;
        case ACTOR_215100_GALLERY_ABORT_RETURN_VIEW:
            // Let the normal room update observe the saved return view on the next tick.
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = ACTOR_215100_GALLERY_RETURN_VIEW;
            mistShootingGallerySelectRoomLights(0);
            task->state++;
            break;
        case ACTOR_215100_GALLERY_ABORT_FINISH:
            gGameSession->battleResetPending = 1;
            gGameSession->flowFlags         |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
            sndEvtRequestMidiStop(0, ACTOR_215100_GALLERY_MUSIC_FADE_AUDIO_TICKS);
            player->movementInputDisabled     = 0;
            gActor215100GalleryTrainingActive = 0;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            gAttachmentState.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
            if (gDisplayState.holdCount != 0) {
                displayReleaseMenuHold();
            }
            taskKill(task);
            break;
    }
}

void actor215100FinishGalleryCourse(void)
{
    gActor215100GalleryTrainingActive = 0;
    if (gActor215100GallerySelectedCourseLevel < ACTOR_215100_GALLERY_FIRST_FREE_MOVEMENT_COURSE) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = ACTOR_215100_GALLERY_RETURN_VIEW;
        mistShootingGallerySelectRoomLights(0);
    } else {
        mistShootingGallerySetTrainingBarrierLowered(1);
        gActor215100GalleryExitInputBlocked = 1;
    }
    if (gActor215100GallerySelectedCourseLevel < ACTOR_215100_GALLERY_FIRST_SWAP_COURSE) {
        gAttachmentState.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
    }
    sndEvtRequestMidiStop(0, ACTOR_215100_GALLERY_MUSIC_FADE_AUDIO_TICKS);
}

void actor215100RequestGalleryAbort(void)
{
    if (gActor215100GallerySelectedCourseLevel == ACTOR_215100_GALLERY_BONUS_COURSE) {
        gActor215100GalleryTrainingActive = 0;
        mistShootingGallerySetTrainingBarrierLowered(1);
        gActor215100GalleryExitInputBlocked = 1;
        gGameSession->battleResetPending    = 1;
        sndEvtRequestMidiStop(0, ACTOR_215100_GALLERY_MUSIC_FADE_AUDIO_TICKS);
        gGameSession->flowFlags |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
    }
    if (gActor215100GallerySelectedCourseLevel < ACTOR_215100_GALLERY_FIRST_FREE_MOVEMENT_COURSE) {
        capRunCommand(ACTOR_215100_GALLERY_ABORT_CONFIRM_COMMAND, CAP_PLAYBACK_CLEAR_IF_UNSTARTED);
        taskSpawnFromTable(gActor215100GalleryDecisionTaskDescs, ACTOR_215100_GALLERY_ABORT_DECISION_TASK, 0, 0);
    }
}

s32 actor215100ResolveGalleryExit(const RoomEventMsg* request)
{
    if (gActor215100GalleryTrainingActive != 0) {
        if (request->queryOnly != ROOM_EVENT_EXECUTE) {
            return ACTOR_215100_GALLERY_EXIT_DEFER;
        }
        // The decision task owns this full request copy until its deferred reload.
        _gActor215100GalleryPendingExitRequest = *request;
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
        capRunCommand(ACTOR_215100_GALLERY_EXIT_CONFIRM_COMMAND, CAP_PLAYBACK_IN_PLACE);
        gCapPreserveActorControl = ACTOR_215100_GALLERY_EXIT_KEEP_ACTORS_HELD;
        taskSpawnFromTable(gActor215100GalleryDecisionTaskDescs, ACTOR_215100_GALLERY_EXIT_DECISION_TASK, ACTOR_215100_GALLERY_CONFIRM_ROOM_EXIT, 0);
    } else {
        // Keep the one-shot scope for the original query branch's delay-slot layout.
        do {
            if (gameFlagGetNibble(GAME_FLAG_0ED) == 0) {
                return ACTOR_215100_GALLERY_EXIT_ALLOW;
            }
            if (request->queryOnly != ROOM_EVENT_EXECUTE) {
                break;
            }
            _gActor215100GalleryPendingExitRequest = *request;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommandWithTransition(ACTOR_215100_GALLERY_EXIT_TRANSITION_COMMAND);
            taskSpawnFromTable(gActor215100GalleryDecisionTaskDescs, ACTOR_215100_GALLERY_EXIT_DECISION_TASK, ACTOR_215100_GALLERY_COMMIT_ROOM_EXIT, 0);
        } while (0);
    }
    return ACTOR_215100_GALLERY_EXIT_DEFER;
}

void actor215100HandleGalleryAction(void)
{
    enum { ACTOR_215100_GALLERY_ACTION_COMMAND = 0x11 };

    if (gActor215100GalleryTrainingActive != 0) {
        mistShootingGallerySignalAction();
        return;
    }
    capSpawnEventIfIdle(ACTOR_215100_GALLERY_ACTION_COMMAND, CAP_EVENT_PAUSE_ACTORS);
}

/// Runs the first-visit gallery introduction and hands control to its menu or exit scene.
///
/// Spawned on gallery entry with a live room overlay. Hides the HUD immediately;
/// an already-seen introduction goes straight to the menu. Otherwise the task
/// waits for each event script, uses the CAP reply to choose the menu or exit,
/// and kills itself after the handoff. Event scripts own HUD/control restoration.
static void _actor215100GalleryIntroTask(Task* task)
{
    enum {
        ACTOR_215100_GALLERY_INTRO_START         = 0,
        ACTOR_215100_GALLERY_INTRO_WAIT          = 1,
        ACTOR_215100_GALLERY_REPLY_START         = 2,
        ACTOR_215100_GALLERY_REPLY_WAIT          = 3,
        ACTOR_215100_GALLERY_EXIT_START          = 4,
        ACTOR_215100_GALLERY_MENU_REPLY_START    = 10,
        ACTOR_215100_GALLERY_MENU_REPLY_WAIT     = 11,
        ACTOR_215100_GALLERY_MENU_REPLY_SEQUENCE = 7,
        ACTOR_215100_GALLERY_EXIT_REPLY_SEQUENCE = 8,
        ACTOR_215100_GALLERY_INTRO_OBJECTIVE     = 0x3A,
    };

    switch (task->state) {
        case ACTOR_215100_GALLERY_INTRO_START:
            // Mark the introduction before playback, so skipping still consumes it.
            gGameSession->hideHud = 1;
            if (gameFlagGetNibble(GAME_FLAG_SHOOTING_GALLERY_INTRO_SEEN) == 0) {
                gameFlagSetNibble(GAME_FLAG_SHOOTING_GALLERY_INTRO_SEEN, 1);
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACTOR_215100_GALLERY_INTRO_OBJECTIVE);
                evsStartScriptWithSkip(_gActor215100GalleryIntroScript, EVENT_SCRIPT_HUD_KEEP, _gActor215100GalleryIntroSkipScript);
                task->state++;
            } else {
                taskSpawnFromTable(gActor215100GalleryTaskDescs, ACTOR_215100_GALLERY_MENU_TASK, 0, 0);
                taskKill(task);
            }
            break;
        case ACTOR_215100_GALLERY_INTRO_WAIT:
            if (gGameSession->eventState == 0) {
                task->state++;
            }
            break;
        case ACTOR_215100_GALLERY_REPLY_START:
            evsStartScript(_gActor215100GalleryTrainingOfferScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR_215100_GALLERY_REPLY_WAIT:
            if (gGameSession->eventState == 0) {
                if (capGetVariantKey() != 0) {
                    task->state = ACTOR_215100_GALLERY_MENU_REPLY_START;
                } else {
                    task->state++;
                }
            }
            break;
        case ACTOR_215100_GALLERY_EXIT_START:
            capStartSequenceSlot(ACTOR_215100_GALLERY_EXIT_REPLY_SEQUENCE, 0, 0);
            evsStartScript(gActor215100GalleryDeclineTrainingScript, EVENT_SCRIPT_HUD_KEEP);
            taskKill(task);
            break;
        case ACTOR_215100_GALLERY_MENU_REPLY_START:
            capStartSequenceSlot(ACTOR_215100_GALLERY_MENU_REPLY_SEQUENCE, 0, 0);
            evsStartScript(_gActor215100GalleryIntroAcceptTrainingScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR_215100_GALLERY_MENU_REPLY_WAIT:
            if (gGameSession->eventState == 0) {
                taskSpawnFromTable(gActor215100GalleryTaskDescs, ACTOR_215100_GALLERY_MENU_TASK, 0, 0);
                taskKill(task);
            }
            break;
    }
}

/// Spawns a gallery red flash whenever its elapsed counter is divisible by 48.
///
/// Includes the first tick. Uses the task's zero-initialized signed-halfword
/// counter as elapsed ticks;
/// increments retain halfword wrapping. Requires the gallery flash table to
/// remain loaded. No work is allocated; the event callback kills this task.
static void _actor215100GalleryRedFlashLoopTask(Task* task)
{
    enum {
        ACTOR_215100_GALLERY_RED_FLASH_PERIOD_TICKS = 48,
    };

    if (task->killCountdown % ACTOR_215100_GALLERY_RED_FLASH_PERIOD_TICKS == 0) {
        taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_RED_FLASH, 0, 0);
    }
    task->killCountdown = task->killCountdown + 1;
}

/// Starts the gallery introduction's movie session, including the cleared-run splash.
///
/// Event-script callback; the gallery overlay must stay loaded through the session.
static void _actor215100StartGalleryMovie(void)
{
    enum { ACTOR_215100_GALLERY_MOVIE_SESSION_TASK = 0 };

    taskSpawnFromTable(gMistShootingGalleryMovieTaskDescs, ACTOR_215100_GALLERY_MOVIE_SESSION_TASK, 0, 0);
}

/// Spawns the introduction's demonstration target for a nonzero enable word.
///
/// Zero does nothing. Requires the gallery and target actor overlays and scene root.
static void _actor215100SpawnGalleryDemoTargetIfEnabled(s32 enabled)
{
    if (enabled != 0) {
        mistShootingGallerySpawnDemoTarget();
    }
}

/// Starts or stops the repeated red flashes used during the gallery introduction.
///
/// Nonzero spawns and publishes a loop task; zero kills the published task and
/// clears its handle. The script must stop an existing loop before starting
/// another: a repeated start overwrites the handle without killing its task.
/// Both actor and gallery overlays must remain loaded until the loop is stopped.
static void _actor215100SetGalleryRedFlashLoop(s32 enabled)
{
    if (enabled != 0) {
        _gActor215100GalleryRedFlashLoopTask = taskSpawnFromTable(gActor215100GalleryTaskDescs, ACTOR_215100_GALLERY_RED_FLASH_LOOP_TASK, 0, 0);
        return;
    }
    if (_gActor215100GalleryRedFlashLoopTask != NULL) {
        taskKill(_gActor215100GalleryRedFlashLoopTask);
        _gActor215100GalleryRedFlashLoopTask = NULL;
    }
}

/// Selects default room lights for zero, or alternate lights for a nonzero halfword.
///
/// Event-script callback; the gallery overlay must be loaded.
static void _actor215100SelectGalleryRoomLights(s16 useAlternate)
{
    mistShootingGallerySelectRoomLights(useAlternate);
}

/// Sets the deferred view-respawn request when the introduction is skipped.
///
/// Stores the signed halfword unchanged; nonzero asks gameplay to respawn the
/// saved view when scene loading permits, and zero clears the request.
static void _actor215100SetViewRespawnPending(s16 pending)
{
    gGameSession->viewDirty = pending;
}

/// Selects the CAP resource and texture page for Pierce's conversation scenes.
///
/// Nonzero selects data-resource ordinal 1 and VRAM origin (768, 0); zero
/// restores the bundle's default CAP selection. Requires those resources loaded.
static void _actor215100SetConversationCap(s32 enabled)
{
    enum {
        ACTOR_215100_CONVERSATION_CAP_RESOURCE_ORDINAL = 1,
        ACTOR_215100_CONVERSATION_CAP_VRAM_X           = 768,
        ACTOR_215100_CONVERSATION_CAP_VRAM_Y           = 0,
    };

    if (enabled != 0) {
        gCapFile = NULL;
        capSelectLoadedFile(ACTOR_215100_CONVERSATION_CAP_RESOURCE_ORDINAL);
        capSetTexturePage(ACTOR_215100_CONVERSATION_CAP_VRAM_X, ACTOR_215100_CONVERSATION_CAP_VRAM_Y);
        return;
    }
    capReset();
}

void actor215100StartPierceConversation(void)
{
    enum {
        ACTOR_215100_PIERCE_TALK_FIRST  = 0,
        ACTOR_215100_PIERCE_TALK_SECOND = 1,
        ACTOR_215100_PIERCE_TALK_REPEAT = 2,
    };

    switch (gameFlagGetNibble(GAME_FLAG_PIERCE_TALK_PROGRESS)) {
        case ACTOR_215100_PIERCE_TALK_FIRST:
            gameFlagSetNibble(GAME_FLAG_PIERCE_TALK_PROGRESS, ACTOR_215100_PIERCE_TALK_SECOND);
            evsStartScript(_gActor215100PierceFirstTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
        case ACTOR_215100_PIERCE_TALK_SECOND:
            evsStartScript(_gActor215100PierceSecondTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_PIERCE_TALK_PROGRESS, ACTOR_215100_PIERCE_TALK_REPEAT);
            break;
        case ACTOR_215100_PIERCE_TALK_REPEAT:
            evsStartScript(_gActor215100PierceRepeatTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
    }
}

#include "../../shared/room/cap_captions.inc.c"

/// Selects a keyed caption and spawns its drawing task for signed tick duration.
///
/// Retained out-of-line wrapper with no callers. A valid loaded command/key and
/// font storage must outlive the task. Zero or negative duration still draws once;
/// later selections replace its text because caption state is shared per overlay.
static void _actor215100ShowTimedCaption(s16 commandIndex, s16 key, s16 durationTicks)
{
    _capCaptionShowTimed(commandIndex, key, durationTicks);
}

#include "../../shared/room/cap_captions_resource.inc.c"

void actor215100SelectCaptionResource(s16 texturePageX, s16 texturePageY, s16 dataResourceIndex)
{
    _capCaptionLoadResource(texturePageX, texturePageY, dataResourceIndex);
}

/// Spawns and adopts Pierce's paired model with the parent's placement textures.
///
/// Requires a live parent and successful child allocation. The task tree owns
/// the returned child; the helper preserves the retained unchecked failure path.
static inline Task* _actor215100SpawnPiercePair(Enemy* enemy, Task* task)
{
    Enemy* pairedEnemy;

    pairedEnemy = enemySpawnFromTable(gActor215100PierceTaskDescs, ACTOR_215100_PIERCE_PAIR_TASK, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(pairedEnemy->task->extra.tmd, enemy);
    taskReparent(task, pairedEnemy->task);
    return pairedEnemy->task;
}

/// Allocates Pierce's paced-walk state, adopts his paired model and starts clip 12.
///
/// Requires the loaded twenty-part body, 25-slot clip bank and pair descriptor.
/// Work-allocation failure destroys the owning enemy/task. The retained child
/// spawn path requires allocation to succeed before it dereferences the child.
/// The task owns the work and child; model lighting matrices borrow that work.
static void _actor215100SpawnPierce(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_215100_PIERCE_INITIAL_CLIP = 12,
        ACTOR_215100_PIERCE_LIGHT_HEIGHT = 800,
        ACTOR_215100_PIERCE_LIGHT_COUNT  = 3,
    };
    VECTOR         lightingPosition;
    PacedWalkWork* work;
    GfxCoord*      rootCoord;
    TmdObject*     model;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    if ((task->work = work = memCalloc(sizeof(*work), false)) == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _pacedWalkExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->flags                     = 0;
    model->otOffset                  = 1;
    work->enemy                      = enemy;
    // The paired model becomes a child released with the parent task tree.
    work->pairTask  = _actor215100SpawnPiercePair(enemy, task);
    work->st.animId = ACTOR_215100_PIERCE_INITIAL_CLIP;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    // Sample lighting 800 world units above the root, then seed the actor rig.
    lightingPosition.vx = rootCoord->workm.t[0];
    lightingPosition.vy = rootCoord->workm.t[1] - ACTOR_215100_PIERCE_LIGHT_HEIGHT;
    lightingPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingPosition, 0, ACTOR_215100_PIERCE_LIGHT_COUNT);
    animationInitContext(&work->rig.anim, _gActor215100PierceAnimationSets, model,
                         work->rig.poses, work->rig.slots);
    work->st.state = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable = _gActor215100PierceMessageTable;
    _pacedWalkUpdate(task);
    task->state++;
}

#include "../../shared/walk/paced_walk_update.inc.c"

/// Dispatches Pierce's spawn state or paced movement, animation, lighting and shadow.
///
/// State must be 0 or 1; spawn increments it once and the frame handler retains 1.
/// The second spawn payload is the live owning Enemy. The actor resources and
/// task model stay loaded; spawn failure may destroy both before dispatch returns.
static void _actor215100PierceTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _actor215100SpawnPierce,
        _actorRenderWalkerFrame,
    };

    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrame
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER _pacedWalkUpdate
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the walker's enemy record and tears down its task tree.
///
/// Installed after the walker allocates its work. `spawnArg2.pointer` must
/// retain the live owning enemy through this callback; task teardown releases
/// the work, model and carried child. Neither pointer survives teardown.
static void _pacedWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walk/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/walk/paced_walk_tick_anim.inc.c"

#include "../../shared/walk/paced_walk_reset_anim.inc.c"

#include "../../shared/walk/paced_walk_blend_anim.inc.c"

#include "../../shared/walk/paced_walk_apply_animation_request.inc.c"

/// Immediately reseeds the walker's non-root tracks from a scripted play request.
///
/// Requires a spawned TMD walker with live `PacedWalkWork`. The bank has loaded
/// clips 1..15 and 18..24; entries 0, 16 and 17 are NULL. The signed check rejects
/// only IDs >=25, so callers must exclude negative and unloaded IDs. The request
/// is borrowed through dispatch; clip storage must outlive playback on parts 1..19.
/// Nonzero blend captures old poses and narrows the duration to a signed halfword
/// in normal-rate frames; 0..2047 keeps blend time nonnegative. Reset ignores it.
/// Ignores the bank selector, collision choice, message ID and second payload.
/// Returns 0 after reseeding or -1 for a rejected ID without changing playback.
static s32 _pacedWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum {
        PACED_WALK_ANIMATION_ID_LIMIT         = ARRAY_SIZE(_gActor215100PierceAnimationSets),
        PACED_WALK_ANIMATION_REQUEST_APPLIED  = 0,
        PACED_WALK_ANIMATION_REQUEST_REJECTED = -1,
    };
    PacedWalkWork* work;

    work = task->work;
    if (request->animationId < PACED_WALK_ANIMATION_ID_LIMIT) {
        _pacedWalkApplyAnimationRequest(work, request);
        _pacedWalkUpdate(task);
        return PACED_WALK_ANIMATION_REQUEST_APPLIED;
    }
    return PACED_WALK_ANIMATION_REQUEST_REJECTED;
}

/// Replaces both model flags from paired-draw request bits, in receiver-first order.
///
/// Both TMD objects must be live and writable; pointers may alias. SHOW clears
/// all flags, its absence excludes active drawing, and SKIP_AUTO_BUFFER adds
/// automatic-buffer suppression. Other bits are ignored; no buffers are changed.
static inline void _pacedWalkReplacePairDrawFlags(TmdObject* walkerModel, TmdObject* pairModel, s32 requestFlags)
{
    enum { PACED_WALK_PAIR_MODEL_NO_FLAGS = 0 };

    if (requestFlags & ACTOR_MESSAGE_PAIR_SHOW) {
        walkerModel->flags = PACED_WALK_PAIR_MODEL_NO_FLAGS;
        pairModel->flags   = PACED_WALK_PAIR_MODEL_NO_FLAGS;
    } else {
        walkerModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        pairModel->flags   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (requestFlags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
        walkerModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        pairModel->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
}

/// Replaces the walker and its carried model's draw flags together.
///
/// Requires live TMD models on the receiver and its `PacedWalkWork::pairTask`,
/// regardless of spawn arguments. Uses `ACTOR_MESSAGE_PAIR_*` request bits:
/// SHOW clears all flags; without it, both exclude active drawing. The separate
/// SKIP_AUTO_BUFFER bit adds that object flag. Other bits are ignored. Does not
/// allocate or release buffers. Ignores the message ID and second payload; returns 0.
static s32 _pacedWalkSetPairModelDraw(Task* task, s32 messageId, s32 requestFlags, s32 unusedArgument)
{
    PacedWalkWork* work;
    TmdObject*     walkerModel;
    TmdObject*     pairModel;

    walkerModel = task->extra.tmd;
    work        = task->work;
    pairModel   = work->pairTask->extra.tmd;
    _pacedWalkReplacePairDrawFlags(walkerModel, pairModel, requestFlags);
    return 0;
}

#include "../../shared/walk/paced_walk_place.inc.c"

/// Ignores actor commands without changing the walker.
///
/// Retains the four-word ABI of `ACTOR_COMMAND_MESSAGE_APPLY`. The receiver,
/// borrowed command and other argument words are never read or retained; returns 0.
static s32 _actor215100IgnoreCommand(Task* task, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument)
{
    return 0;
}

#include "../../shared/walk/paced_walk_to.inc.c"

#include "../../shared/walk/paced_walk_attach_carried_model.inc.c"

/// Attaches the carried model to walker part 4 and refreshes its root composition.
///
/// Requires a live TMD child task under a spawned walker with `PacedWalkWork`
/// and at least five model coordinates. State 0 borrows part 4 and the parent's
/// light matrices, clears all flags and enters state 1. Both states mark the
/// root dirty; other states do nothing. The parent's model and work must outlive
/// this child, which belongs to the parent's task teardown tree.
static void _pacedWalkSubModelTask(Task* task)
{
    enum {
        PACED_WALK_SUB_MODEL_ATTACH = 0,
        PACED_WALK_SUB_MODEL_FOLLOW = 1,
        PACED_WALK_ATTACHMENT_PART  = 4,
    };
    // The original unused reservation retains the shared return epilogue.
    char           unusedStackFrame[0x10];
    Task*          parentTask      = task->parent;
    TmdObject*     carriedModel    = task->extra.tmd;
    GfxCoord*      rootCoord       = carriedModel->coords;
    GfxCoord*      attachmentCoord = &parentTask->extra.tmd->coords[PACED_WALK_ATTACHMENT_PART];
    PacedWalkWork* parentWork      = parentTask->work;

    switch (task->state) {
        case PACED_WALK_SUB_MODEL_ATTACH:
            _pacedWalkAttachCarriedModel(carriedModel, attachmentCoord, parentWork);
            task->state++;
            break;
        case PACED_WALK_SUB_MODEL_FOLLOW:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
