#include "actors/actor_136100.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/captions.h"
#include "gameplay/attachments.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/dryfield_night_main_street.h"
#include "../../shared/screen/screen_fade_step_up.inc.c"

#include "assets/engine/animation/player_schema.h"

static void _screenFadeInTask(Task* task);
/// Binds the shared fade-in declaration and definition to this file's private task.
///
/// Requires the bare identifier of the preceding static `void (Task*)` callback.
/// Keep defined from `screen_fade.h` through `screen_fade_in.inc.c`, then undefine.
/// This object-like binding has no arguments, captures or token construction.
#define SCREEN_FADE_IN_TASK _screenFadeInTask
#include "../../shared/screen/screen_fade.h"
#include "../../shared/actor/actor_messages.h"

/// Player placements at the two ends of a scripted street-scene entrance.
///
/// The entrance applies `start`'s XYZ and Euler angles, then walks to
/// `destination`'s XYZ without applying its angles. Scene resets and later
/// staging can apply either complete placement. Positions use the player's
/// root-parent frame in game units; angles use 4096 units per turn.
/// Dispatch consumes the values synchronously and retains no record pointer.
typedef struct {
    ActorTransform start;       // Initial placement and facing; also the before-Burner reset placement
    ActorTransform destination; // Walk target XYZ; full placement for the after-Burner reset and before-Burner staging
} _Actor136100PlayerEntrance;
STATIC_ASSERT_SIZEOF(_Actor136100PlayerEntrance, 2 * sizeof(ActorTransform));

/// Which of its two scenes the actor stages, held in `_Actor136100Work::scene`.
///
/// The scenes use different placements, scripts and request handlers; the
/// choice is made once, when the actor spawns.
enum {
    ACTOR_136100_SCENE_AFTER_BURNER  = 0, // `GAME_FLAG_BURNER_DEFEATED` is set
    ACTOR_136100_SCENE_BEFORE_BURNER = 1, // `GAME_FLAG_BURNER_DEFEATED` is clear
};

/// Requests consumed on later ticks; the selected scene determines each code.
///
/// Clip numbers identify loaded sets; their individual motions are unproven.
enum {
    ACTOR_136100_PLAYER_AFTER_ENTER             = 1,
    ACTOR_136100_PLAYER_AFTER_PLAY_ANIMATION_50 = 2,
    ACTOR_136100_PLAYER_AFTER_PLAY_ANIMATION_52 = 3,
    ACTOR_136100_PLAYER_AFTER_RESET_PLACEMENT   = 4,
    ACTOR_136100_PLAYER_AFTER_BLEND_WEAPON      = 5,
    ACTOR_136100_PLAYER_AFTER_RESET_WEAPON      = 6,
};

enum {
    ACTOR_136100_PLAYER_BEFORE_HIDE                  = 1,
    ACTOR_136100_PLAYER_BEFORE_ENTER                 = 2,
    ACTOR_136100_PLAYER_BEFORE_PLAY_ANIMATION_49     = 3,
    ACTOR_136100_PLAYER_BEFORE_PLAY_ANIMATION_53     = 4,
    ACTOR_136100_PLAYER_BEFORE_EXIT                  = 5,
    ACTOR_136100_PLAYER_BEFORE_RESET_SCENE_ANIMATION = 6,
    ACTOR_136100_PLAYER_BEFORE_BLEND_WEAPON          = 7,
};

enum {
    ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_2          = 1,
    ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_5          = 2,
    ACTOR_136100_BODY_AFTER_BLEND_ANIMATION_7         = 3,
    ACTOR_136100_BODY_AFTER_BLEND_ANIMATION_9         = 4,
    ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_10_THEN_11 = 5,
    ACTOR_136100_BODY_AFTER_RESET                     = 6,
};

enum {
    ACTOR_136100_BODY_BEFORE_PLACE_FIRST                = 1,
    ACTOR_136100_BODY_BEFORE_PLACE_SECOND               = 2,
    ACTOR_136100_BODY_BEFORE_BLEND_ANIMATION_4          = 3,
    ACTOR_136100_BODY_BEFORE_RESET_AND_PLACE            = 4,
    ACTOR_136100_BODY_BEFORE_PLAY_ANIMATION_6_AND_SOUND = 5,
    ACTOR_136100_BODY_BEFORE_TURN_HEAD                  = 6,
    ACTOR_136100_BODY_BEFORE_RETURN_HEAD                = 7,
    ACTOR_136100_BODY_BEFORE_RESET                      = 8,
};

enum {
    ACTOR_136100_COMPANION_AFTER_RESET_ANIMATION_1             = 1,
    ACTOR_136100_COMPANION_AFTER_PLAY_REPEATED_CUE             = 2,
    ACTOR_136100_COMPANION_AFTER_RESET_PLACEMENT_AND_ANIMATION = 3,
};

enum {
    ACTOR_136100_COMPANION_BEFORE_PLACE_FIRST     = 1,
    ACTOR_136100_COMPANION_BEFORE_PLACE_SECOND    = 2,
    ACTOR_136100_COMPANION_BEFORE_RESET_PLACEMENT = 3,
};

/// Bank selection and playback defaults of this package's street scenes.
enum {
    ACTOR_136100_PLAYER_WEAPON_ANIMATION  = 1,
    ACTOR_136100_BODY_AFTER_BURNER_START  = 1,
    ACTOR_136100_BODY_BEFORE_BURNER_START = 3,
    ACTOR_136100_ANIMATION_BLEND_FRAMES   = 10,
    ACTOR_136100_REQUEST_NONE             = 0,
};

/// Opening-scene cue classifications and the room actions selecting street scenes.
enum {
    ACTOR_136100_OPENING_CUE_NONE                    = 0,
    ACTOR_136100_OPENING_CUE_AFTER_BURNER            = 1,
    ACTOR_136100_OPENING_CUE_BEFORE_BURNER           = 2,
    ACTOR_136100_ROOM_ACTION_OPEN_AFTER_BURNER       = 0x10,
    ACTOR_136100_ROOM_ACTION_OPEN_BEFORE_BURNER      = 0x11,
    ACTOR_136100_ROOM_ACTION_FOLLOW_UP_AFTER_BURNER  = 0x12,
    ACTOR_136100_ROOM_ACTION_FOLLOW_UP_BEFORE_BURNER = 0x13,
};

/// Work block of Gary Douglas on Dryfield's main street at night, allocated at
/// its full size, zeroed and kept at `Task::work`.
///
/// The actor directs a scene with three performers: the player, its own body
/// and the companion. For each it keeps the task to address, a request the
/// event script posts for the per-frame handlers to carry out, and the
/// animation last started, which selects the follow-on animation once that one
/// has finished. A request code is 0 when none is pending and is otherwise
/// particular to the scene; posting one restarts its step.
///
/// The tasks of the head and rifle models allocate a block of this type as
/// well and use only its `light` and `color`.
///
/// Nothing accesses the four `pad` runs; their roles are unproven.
typedef struct {
    ActorAnimRig20 rig;                    // Body rig; the actor drives slots 1 to 19
    MATRIX         light;                  // Light matrix the task's model is lit with
    MATRIX         color;                  // Colour matrix the task's model is lit with
    Task*          playerTask;             // Player's task
    Task*          headTask;               // Task of the head-and-hat model, which hangs from rig part 4
    Task*          rifleTask;              // Task of the rifle model: placed in the view after the Burner, hung from rig part 8 before it
    Task*          companionTask;          // Companion's task, NULL when none is registered
    s16            playerRequest;          // Request for the player (0 none)
    s16            playerRequestStep;      // Step of the player request in progress, counted from 0; the head turn counts its steps here too
    s16            playerRequestFrames;    // Frames the current step of the player request has waited
    byte           pad_4CA[0x2];
    s16            bodyRequest;            // Request for the actor's own body (0 none)
    s16            bodyRequestStep;        // Step of the body request in progress, counted from 0
    s16            bodyRequestFrames;      // Frames the current step of the body request has waited
    byte           pad_4D2[0x2];
    s16            companionRequest;       // Request for the companion (0 none)
    s16            companionRequestStep;   // Step of the companion request in progress, counted from 0
    s16            companionRepeatCount;   // Times the repeated sound and animation of a companion request have played
    s16            companionRepeatDelay;   // Frames until that sound and animation play again
    u16            followUpSeen;           // Whether a talk after the opening scene has played (0 the next plays the full script, 1 the short one)
    s16            playerAnimation;        // Animation last requested of the player
    s16            bodyAnimation;          // Animation set the rig last started, an index into the actor's set table
    u16            companionAnimation;     // Animation last requested of the companion, an index into the sets the actor installs on it
    u16            scene;                  // Scene staged (0 `ACTOR_136100_SCENE_AFTER_BURNER`, 1 `ACTOR_136100_SCENE_BEFORE_BURNER`)
    byte           pad_4E6[0x4];
    s16            headYaw;                // Yaw the head turn gives rig part 4 in the scene before the Burner, 4096 to a turn
    s16            playerEquipmentRemoved; // 1 while the player's equipment tasks are killed for a scene, so its end spawns the weapon again
    byte           pad_4EE[0x2];
} _Actor136100Work;
STATIC_ASSERT_SIZEOF(_Actor136100Work, 0x4F0);

static void _actor136100UpdateAfterBurnerBodyRequest(Task* task);
static void _actor136100UpdateBeforeBurnerBodyRequest(Task* task);
static void _actor136100CopyPlayerSceneAnimations(Task* task);

static AnimationSet _gDryfieldNightMainStreetGaryRestClip;
static AnimationSet _gDryfieldNightMainStreetGaryTruckFixedClip;
static AnimationSet _gDryfieldNightMainStreetGaryHandOverKeyClip;
static AnimationSet _gDryfieldNightMainStreetGaryTalkClip;
static AnimationSet _gDryfieldNightMainStreetGaryTalkEndClip;
static AnimationSet _gDryfieldNightMainStreetBodyAnim12Clip;
static AnimationSet _gDryfieldNightMainStreetGaryMournFlintClip;
static AnimationSet _gDryfieldNightMainStreetGaryTruckDoneClip;
static AnimationSet _gDryfieldNightMainStreetGaryHandOverKeyMourningClip;
static AnimationSet _gDryfieldNightMainStreetBodyAnim07Clip;
static AnimationSet _gDryfieldNightMainStreetBodyAnim08Clip;
static AnimationSet _gDryfieldNightMainStreetBodyAnim09Clip;
static AnimationSet _gFlintStandClip;
static AnimationSet _gFlintLieRestingClip;
static AnimationSet _gDryfieldNightMainStreetCompanionAnim05Clip;
static AnimationSet _gFlintReactTruckFixedClip;
static AnimationSet _gFlintReactKeyHandoverClip;
static AnimationSet _gFlintBarkClip;

static TmdSource _gGaryBody;
static TmdSource _gGaryHeadHat;
static TmdSource _gGaryShotgun;
static void      _actor136100HeadTask(Task* task);
static void      _actor136100RifleTask(Task* task);
static void      _actor136100ResetAfterBurnerScene(void);
static void      _actor136100ResetBeforeBurnerScene(s32 resetHeadRotation);
static void      _actor136100SceneTask(Task* task);
static void      _actor136100FadeOutTask(Task* task);
static void      _actor136100ResetPlayerWeaponAnimation(void);
static s32       _actor136100SetModelDraw(Task* task, s32 unusedMessageId, s32 visible, s32 unusedSecondArg);
static void      _actor136100ResetBodyAnimation(void);
static void      _actor136100PostPlayerRequest(s16 request);
static void      _actor136100PostBodyRequest(s16 request);
static void      _actor136100PostCompanionRequest(s16 request);
static void      _actor136100StartFadeIn(void);
static void      _actor136100StartFadeOut(void);
static void      _actor136100RestoreDisplay(void);
static void      _actor136100RemovePlayerEquipment(void);
static void      _actor136100RestorePlayerEquipment(void);
static void      _actor136100FinishOpeningScene(s32 beforeBurner);

#include "assets/characters/gary/body.inc"

#include "assets/characters/gary/head_hat.inc"

#include "assets/characters/gary/shotgun.inc"

#include "assets/rooms/dryfield/night_main_street/gary_clips.inc"

/// No follow-on clip after the current animation settles.
enum { ACTOR_136100_ANIMATION_CHAIN_END = -1 };

/// CAP command slots and restored views for Gary Douglas's follow-up talks.
enum {
    ACTOR_136100_CAP_AFTER_BURNER_FIRST_FOLLOW_UP   = 13,
    ACTOR_136100_CAP_AFTER_BURNER_REPEAT_FOLLOW_UP  = 14,
    ACTOR_136100_CAP_BEFORE_BURNER_FIRST_FOLLOW_UP  = 16,
    ACTOR_136100_CAP_BEFORE_BURNER_REPEAT_FOLLOW_UP = 17,
    ACTOR_136100_AFTER_BURNER_FOLLOW_UP_VIEW        = 7,
    ACTOR_136100_BEFORE_BURNER_FOLLOW_UP_VIEW       = 4,
};

/// Exactly this argument requests the before-Burner skip's head-rotation reset.
enum { ACTOR_136100_RESET_HEAD_ROTATION = 1 };

/// Sticky cancellation latch for this load of the street scene's fade-out.
///
/// Zero until display restoration sets it to 1; it is never cleared during the
/// overlay's lifetime. Fade-out checks it after drawing and advancing its ramp.
static u16 _gActor136100FadeOutCancelled = 0;

/// Seven player scene clips followed by a NULL installation terminator.
///
/// Copied as borrowed set pointers into the selected weapon bank at
/// `ANIMATION_BANK_BASE_SET_COUNT`, giving IDs 47..53. The terminator is not
/// copied. Keep this overlay's descriptors and clip data loaded through playback.
#include "assets/rooms/dryfield/night_main_street/player_scene_bank.inc"

/// Default set table for Gary Douglas's body rig in both street scenes.
///
/// Slot 0 is unused; slots 1..12 hold loaded clips for driven tracks 1..19.
/// The rig borrows this table and its descriptors while the actor is live.
#include "assets/rooms/dryfield/night_main_street/gary_bank.inc"

/// Six companion scene clips addressed by set indices 0..5.
///
/// Installation/playback borrows this table without a terminator. Keep its
/// descriptors and clip data loaded while the companion plays these sets.
#include "assets/rooms/dryfield/night_main_street/flint_bank.inc"

/// Relative follow-on clip indices for player scene IDs 47..53.
///
/// Indexed by the current ID minus `ANIMATION_BANK_BASE_SET_COUNT`.
/// `ACTOR_136100_ANIMATION_CHAIN_END` leaves the settled pose; nonnegative
/// values are relative to the same extension base.
static s16 _gActor136100PlayerSceneAnimationSuccessors[7] = {
    ACTOR_136100_ANIMATION_CHAIN_END,
    ACTOR_136100_ANIMATION_CHAIN_END,
    ACTOR_136100_ANIMATION_CHAIN_END,
    4,
    ACTOR_136100_ANIMATION_CHAIN_END,
    0,
    0,
};

/// Follow-on body set indices corresponding to the thirteen-slot body table.
///
/// Slot 0 is unused by playback. `ACTOR_136100_ANIMATION_CHAIN_END` leaves
/// the settled pose; nonnegative values select a loaded body set in 1..12.
static s16 _gActor136100BodyAnimationSuccessors[13] = {
    ACTOR_136100_ANIMATION_CHAIN_END,
    ACTOR_136100_ANIMATION_CHAIN_END,
    ACTOR_136100_ANIMATION_CHAIN_END,
    ACTOR_136100_ANIMATION_CHAIN_END,
    ACTOR_136100_ANIMATION_CHAIN_END,
    1,
    ACTOR_136100_ANIMATION_CHAIN_END,
    8,
    ACTOR_136100_ANIMATION_CHAIN_END,
    1,
    ACTOR_136100_ANIMATION_CHAIN_END,
    1,
    1,
};

/// Follow-on set indices for the six installed companion scene clips.
///
/// Indexed by the current clip in 0..5. Only clip 4 advances, to clip 1;
/// `ACTOR_136100_ANIMATION_CHAIN_END` leaves every other clip's settled pose.
static s16 _gActor136100CompanionAnimationSuccessors[6] = {
    ACTOR_136100_ANIMATION_CHAIN_END,
    ACTOR_136100_ANIMATION_CHAIN_END,
    ACTOR_136100_ANIMATION_CHAIN_END,
    ACTOR_136100_ANIMATION_CHAIN_END,
    1,
    ACTOR_136100_ANIMATION_CHAIN_END,
};

/// Replacement normals for the before-Burner street-scene collision enclosure.
///
/// Four axis normals in the Rooms2And3 grid frame, with 4096 per unit length.
/// Overwrites only the live grid's first four normals; the other entries remain.
static SVECTOR _gActor136100BeforeBurnerCollisionNormals[4] = {
#include "assets/actor_136100_collision_0D404.inc"
};

/// Replacement vertices for the before-Burner street-scene collision enclosure.
///
/// Sixteen Rooms2And3 grid-frame vertices in game units, spanning
/// X -3000..-1800, Y -300..0, Z 7000..8400. Only the first sixteen live vertices
/// are replaced; the remainder of the room pool remains intact.
static SVECTOR _gActor136100BeforeBurnerCollisionVerts[16] = {
#include "assets/actor_136100_collision_0D424.inc"
};

/// Four replacement quads enclosing the before-Burner street-scene space.
///
/// References replacement vertices 0..15 and normals 0..3 with room surface
/// class 0. Only the first four live Rooms2And3 faces are replaced; the grid's
/// existing face-index lists and remaining faces stay intact.
static WorldCollisionGridFace _gActor136100BeforeBurnerCollisionFaces[4] = {
#include "assets/actor_136100_collision_0D4A4.inc"
};

/// Placement and visibility messages for the scene body, head and rifle models.
///
/// No end marker: send only `ACTOR_MESSAGE_PLACE` with a borrowed
/// `ActorTransform*`, or `ACTOR_MESSAGE_SET_MODEL_DRAW` with an integer
/// (0 hide, nonzero show). Both ignore the second payload. Neither supplies a
/// defined result, so direct, CAP and event-script senders must discard it.
/// Placement reparents the model to the view, including an attached child.
static TaskMessageEntry _gActor136100ModelMessages[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor136100SetModelDraw },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceInView },
};

/// Player start and walk destination for the after-Burner opening scene.
///
/// The scene reset also uses `destination`; both records use the player's
/// root-parent frame and 4096 angle units per turn.
static _Actor136100PlayerEntrance _gActor136100AfterBurnerPlayerEntrance = {
    { { -3500, 0, -3700, 0 }, { 0, -1024, 0, 0 } },
    { { -4800, 0, -4100, 0 }, { 0, -1024, 0, 0 } },
};

/// Player start and walk destination for the before-Burner opening scene.
///
/// Scene reset uses `start`; the first follow-up stages the player at
/// `destination`. Both records use the player's root-parent frame and
/// 4096 angle units per turn.
static _Actor136100PlayerEntrance _gActor136100BeforeBurnerPlayerEntrance = {
    { { -3200, 0, 6500, 0 }, { 0, 910, 0, 0 } },
    { { -2520, 0, 7690, 0 }, { 0, 910, 0, 0 } },
};

/// Player placement when leaving the before-Burner opening scene.
///
/// Position is in game units in the player's root-parent frame; Euler angles
/// use 4096 units per turn. The placement is borrowed only during dispatch.
static ActorTransform _gActor136100BeforeBurnerPlayerExitPlacement = { { -3300, 0, 6400, 0 }, { 0, 910, 0, 0 } };

/// Player placement for the first after-Burner follow-up and its skip recovery.
///
/// Position is in game units in the player's root-parent frame; Euler angles
/// use 4096 units per turn. The placement is borrowed only during dispatch.
static ActorTransform _gActor136100AfterBurnerPlayerFollowUpPlacement = { { -5400, 0, -4100, 0 }, { 0, -1024, 0, 0 } };

/// Gary Douglas's body placement after the Burner encounter.
///
/// Position is in game units in the view-parent frame; Euler angles
/// use 4096 units per turn. The placement is borrowed only during dispatch.
static ActorTransform _gActor136100AfterBurnerBodyPlacement = { { -6400, 0, -4000, 0 }, { 0, 1024, 0, 0 } };

/// Gary Douglas's resting body placement before the Burner encounter.
///
/// Position is in game units in the view-parent frame; Euler angles
/// use 4096 units per turn. The placement is borrowed only during dispatch.
static ActorTransform _gActor136100BeforeBurnerBodyRestPlacement = { { -2600, 0, 7400, 0 }, { 0, 420, 0, 0 } };

/// First scripted body placement in the before-Burner opening scene.
///
/// Position is in game units in the view-parent frame; Euler angles
/// use 4096 units per turn. The placement is borrowed only during dispatch.
static ActorTransform _gActor136100BeforeBurnerBodyFirstPlacement = { { -1300, 0, 7000, 0 }, { 0, 420, 0, 0 } };

/// Second body placement, also used for the before-Burner follow-up head turn.
///
/// Position is in game units in the view-parent frame; Euler angles
/// use 4096 units per turn. The placement is borrowed only during dispatch.
static ActorTransform _gActor136100BeforeBurnerBodySecondPlacement = { { -1540, 0, 7600, 0 }, { 0, 420, 0, 0 } };

/// Companion placement used by after-Burner staging and placement resets.
///
/// Position is in game units in the companion's root-parent frame; Euler angles
/// use 4096 units per turn. The placement is borrowed only during dispatch.
static ActorTransform _gActor136100AfterBurnerCompanionPlacement = { { -5800, 0, -3200, 0 }, { 0, 1365, 0, 0 } };

/// Companion resting placement used by before-Burner staging and resets.
///
/// Position is in game units in the companion's root-parent frame; Euler angles
/// use 4096 units per turn. The placement is borrowed only during dispatch.
static ActorTransform _gActor136100BeforeBurnerCompanionRestPlacement = { { -2210, 0, 8000, 0 }, { 0, 2048, 0, 0 } };

/// First scripted companion placement in the before-Burner opening scene.
///
/// Position is in game units in the companion's root-parent frame; Euler angles
/// use 4096 units per turn. The placement is borrowed only during dispatch.
static ActorTransform _gActor136100BeforeBurnerCompanionFirstPlacement = { { -1000, 0, 7725, 0 }, { 0, 2048, 0, 0 } };

/// Second companion placement used by before-Burner opening and follow-up scenes.
///
/// Position is in game units in the companion's root-parent frame; Euler angles
/// use 4096 units per turn. The placement is borrowed only during dispatch.
static ActorTransform _gActor136100BeforeBurnerCompanionSecondPlacement = { { -910, 0, 8130, 0 }, { 0, 2048, 0, 0 } };

/// Free-standing rifle placement after the Burner encounter.
///
/// Position is in game units in the view-parent frame; Euler angles
/// use 4096 units per turn. The placement is borrowed only during dispatch.
static ActorTransform _gActor136100AfterBurnerRiflePlacement = { { -5800, -400, -4400, 0 }, { 1843, 0, 0, 0 } };

enum {
    ACTOR_136100_CAP_AFTER_BURNER_OPENING   = 12,
    ACTOR_136100_CAP_BEFORE_BURNER_OPENING  = 15,
    ACTOR_136100_CAP_OPENING_SKIP_END       = 18,
    ACTOR_136100_AFTER_BURNER_OPENING_VIEW  = 2,
    ACTOR_136100_BEFORE_BURNER_OPENING_VIEW = 4,
};

/// Gary Douglas's opening night-street conversation after the Burner encounter.
///
/// CAP cues stage player, body and companion requests while equipment is
/// suppressed. Restores equipment and display, updates companion schedules,
/// clears requests and releases scripted actor control. All 33 commands
/// include END. The scene singleton, both actors and borrowed night-street
/// CAP/animation resources must remain live until END.
static EvsCommand _gActor136100AfterBurnerOpeningScript[33] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_136100_CAP_AFTER_BURNER_OPENING }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100ResetPlayerWeaponAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RemovePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100StartFadeIn }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_AFTER_ENTER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_COMPANION_AFTER_PLAY_REPEATED_CUE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_10_THEN_11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_AFTER_PLAY_ANIMATION_50 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100StartFadeOut }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_AFTER_RESET_WEAPON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor136100FinishOpeningScene }, { .value = ACTOR_136100_SCENE_AFTER_BURNER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestoreDisplay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip recovery for Gary Douglas's after-Burner opening conversation.
///
/// Clears pending requests, restores equipment and scene poses under the
/// primary fade, applies the completed scene's schedules and view 2, then
/// releases both actors. All 19 commands include END. Requires the normal
/// path's live scene singleton, actors and loaded night-street resources.
static EvsCommand _gActor136100AfterBurnerOpeningSkipScript[19] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100ResetAfterBurnerScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor136100FinishOpeningScene }, { .value = ACTOR_136100_SCENE_AFTER_BURNER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestoreDisplay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_136100_AFTER_BURNER_OPENING_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_136100_CAP_OPENING_SKIP_END }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// First follow-up conversation with Gary Douglas after the Burner encounter.
///
/// Starts the first after-Burner CAP command and coordinates requests
/// with CAP cues before releasing scripted player control. Requires the live
/// scene singleton, player, installed scene clips and night Main Street CAP
/// resources. Command storage and callback code stay borrowed until END.
static EvsCommand _gActor136100AfterBurnerFirstFollowUpScript[17] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_136100_CAP_AFTER_BURNER_FIRST_FOLLOW_UP }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100ResetPlayerWeaponAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_AFTER_RESET_PLACEMENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RemovePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_10_THEN_11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_AFTER_RESET_WEAPON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip recovery for the first after-Burner follow-up conversation.
///
/// Restores equipment and scene poses under the primary fade, clears pending
/// requests, restores the after-Burner follow-up view and releases scripted
/// player control. The script and its callbacks must remain loaded until END.
static EvsCommand _gActor136100AfterBurnerFirstFollowUpSkipScript[16] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor136100AfterBurnerPlayerFollowUpPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100ResetPlayerWeaponAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100ResetBodyAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_136100_AFTER_BURNER_FOLLOW_UP_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Short conversation used on later after-Burner interactions with Gary Douglas.
///
/// Starts the repeat after-Burner CAP command and coordinates requests
/// with CAP cues before releasing scripted player control. Requires the live
/// scene singleton, player, installed scene clips and night Main Street CAP
/// resources. Command storage and callback code stay borrowed until END.
static EvsCommand _gActor136100AfterBurnerRepeatFollowUpScript[12] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_136100_CAP_AFTER_BURNER_REPEAT_FOLLOW_UP }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_AFTER_BLEND_WEAPON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_10_THEN_11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Gary Douglas's opening night-street conversation before the Burner encounter.
///
/// CAP cues stage body and companion placements and player movement while
/// equipment is suppressed. Restores equipment/display, applies the pre-Burner
/// completion schedules and clears pending requests before releasing the
/// player. All 38 commands include END. Requires the live scene singleton,
/// both actors and borrowed night-street CAP/animation resources until END.
static EvsCommand _gActor136100BeforeBurnerOpeningScript[38] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_136100_CAP_BEFORE_BURNER_OPENING }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100ResetPlayerWeaponAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RemovePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100StartFadeIn }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_HIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_PLACE_FIRST }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_COMPANION_BEFORE_PLACE_FIRST }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_PLACE_SECOND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_COMPANION_BEFORE_PLACE_SECOND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_ENTER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_PLAY_ANIMATION_53 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_BLEND_ANIMATION_4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_PLAY_ANIMATION_6_AND_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_PLAY_ANIMATION_49 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100StartFadeOut }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_EXIT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_RESET_AND_PLACE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_COMPANION_BEFORE_RESET_PLACEMENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor136100FinishOpeningScene }, { .value = ACTOR_136100_SCENE_BEFORE_BURNER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestoreDisplay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip recovery for Gary Douglas's before-Burner opening conversation.
///
/// Clears requests, restores equipment and initial scene placements under the
/// primary fade, preserves the head rotation, applies completion schedules
/// and view 4, then releases the player. All 18 commands include END. Requires
/// the normal path's live scene singleton, actors and loaded street resources.
static EvsCommand _gActor136100BeforeBurnerOpeningSkipScript[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor136100ResetBeforeBurnerScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor136100FinishOpeningScene }, { .value = ACTOR_136100_SCENE_BEFORE_BURNER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestoreDisplay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_136100_BEFORE_BURNER_OPENING_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_136100_CAP_OPENING_SKIP_END }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// First follow-up conversation with Gary Douglas before the Burner encounter.
///
/// Starts the first before-Burner CAP command and coordinates requests
/// with CAP cues before releasing scripted player control. Requires the live
/// scene singleton, player, installed scene clips and night Main Street CAP
/// resources. Command storage and callback code stay borrowed until END.
static EvsCommand _gActor136100BeforeBurnerFirstFollowUpScript[23] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_136100_CAP_BEFORE_BURNER_FIRST_FOLLOW_UP }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100ResetPlayerWeaponAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gActor136100BeforeBurnerPlayerEntrance.destination } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RemovePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_RESET_SCENE_ANIMATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_TURN_HEAD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_COMPANION_BEFORE_PLACE_SECOND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_RETURN_HEAD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_EXIT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_RESET_AND_PLACE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_COMPANION_BEFORE_RESET_PLACEMENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip recovery for the first before-Burner follow-up conversation.
///
/// Restores equipment and scene poses under the primary fade, clears pending
/// requests, restores the before-Burner follow-up view and releases scripted
/// player control. The script and its callbacks must remain loaded until END.
static EvsCommand _gActor136100BeforeBurnerFirstFollowUpSkipScript[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor136100ResetBeforeBurnerScene }, { .value = ACTOR_136100_RESET_HEAD_ROTATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_136100_BEFORE_BURNER_FOLLOW_UP_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Short conversation used on later before-Burner interactions with Gary Douglas.
///
/// Starts the repeat before-Burner CAP command and coordinates requests
/// with CAP cues before releasing scripted player control. Requires the live
/// scene singleton, player, installed scene clips and night Main Street CAP
/// resources. Command storage and callback code stay borrowed until END.
static EvsCommand _gActor136100BeforeBurnerRepeatFollowUpScript[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_136100_CAP_BEFORE_BURNER_REPEAT_FOLLOW_UP }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_BLEND_WEAPON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc gActor136100SceneTaskDescriptors[6] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor136100SceneTask, { .model = &_gGaryBody } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, NULL, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor136100HeadTask, { .model = &_gGaryHeadHat } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor136100RifleTask, { .model = &_gGaryShotgun } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _screenFadeInTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor136100FadeOutTask, { .value = 0 } },
};

/// Borrowed singleton task running Gary Douglas's night Main Street scenes.
///
/// Published after body-work allocation and used by script callbacks and child
/// initialization. It owns the body work and is the head/rifle teardown parent.
/// Teardown does not clear this pointer; consumers require the task and overlay
/// to remain live and must finish before the task is released.
static Task* _gActor136100SceneTask = NULL;

static void        _actor136100UpdateAfterBurnerPlayerRequest(Task* task);
static inline void _actor136100ResetBodyTracks(Task* task, s32 setIndex);
static inline void _actor136100PlayCompanionAnimation(Task* task, u16 setIndex, s32 blendChoice, s32 blendFrames);
static void        _actor136100UpdateAfterBurnerCompanionRequest(Task* task);
static void        _actor136100UpdateBeforeBurnerPlayerRequest(Task* task);
static void        _actor136100UpdateBeforeBurnerCompanionRequest(Task* task);
static s16         _actor136100TryStartFollowUpScene(Task* task);
static void        _actor136100InitBody(Task* task);
static inline s16  _actor136100ReadOpeningSceneCue(u16* triggerControl, u8* sceneAction, u8* actionArgument);
static inline void _actor136100UpdateBodyLighting(Task* task, VECTOR* samplePosition);

/// Polls the player's scene animation and starts its configured successor.
///
/// Returns 0 while the player animation is playing, otherwise 1, including
/// when a successor has just been started or no player is registered. Scene
/// animations use the installed extension at `ANIMATION_BANK_BASE_SET_COUNT`;
/// a negative successor ends the chain. Playback blends for ten normal-rate
/// frames and disables world collision. The controller work, player and
/// installed clip data must remain live through synchronous dispatch. Scene
/// animation IDs written by this package span 47 to 53; lower base-bank IDs
/// have no scene successor, and there is no upper-bound check here.
static s32 _actor136100AdvancePlayerAnimation(Task* task)
{
    _Actor136100Work*    work;
    _Actor136100Work*    requestWork;
    AnimationPlayRequest request;
    const s16*           successorEntry;
    s32                  chainIndex;
    u16                  nextAnimationId;
    s32                  weaponId;
    s32                  selection;

    // Poll before choosing a successor from the installed scene clips.
    work = task->work;
    if (work->playerTask == NULL) {
        return 1;
    }
    if (taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        if ((u16)work->playerAnimation >= (u32)ANIMATION_BANK_BASE_SET_COUNT) {
            chainIndex     = (u16)work->playerAnimation - (u32)ANIMATION_BANK_BASE_SET_COUNT;
            successorEntry = &_gActor136100PlayerSceneAnimationSuccessors[chainIndex];
            selection      = *successorEntry;
            if (selection >= 0) {
                nextAnimationId = (u16)*successorEntry + (u32)ANIMATION_BANK_BASE_SET_COUNT;

                requestWork                  = task->work;
                weaponId                     = gPlayerStatus.weapon;
                selection                    = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponId);
                request.source.index         = selection;
                requestWork->playerAnimation = nextAnimationId;
                request.animationId          = nextAnimationId;
                request.blend                = ANIMATION_BLEND_INTERPOLATE;
                request.blendFrames          = ACTOR_136100_ANIMATION_BLEND_FRAMES;
                request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(requestWork->playerTask, ANIMATION_MESSAGE_PLAY, &request, 0);
            }
        }
        return 1;
    }
    return 0;
}

/// Advances body tracks 1 to 19 and blends into their configured successor.
///
/// Returns 0 while any track is unsettled, otherwise 1, including when a
/// successor has just been started. `bodyAnimation` must index the successor
/// table; nonnegative successors must index the rig's loaded set table. A
/// negative entry leaves the settled pose. Blends last ten normal-rate frames;
/// slot 0 is untouched. The initialized rig and its model must remain live.
static s32 _actor136100TickBodyAnimation(Task* task)
{
    _Actor136100Work* work;
    _Actor136100Work* restartWork;
    u16               slotIndex;
    u16               settled;
    u16               nextSetIndex;
    u16               setIndex;

    // Tick every driven track before deciding whether the body has settled.
    work = task->work;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    for (settled = slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        if (!(work->rig.slots[slotIndex].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
            goto unsettled;
        }
    }
checkSettled:
    if (settled) {
        nextSetIndex = _gActor136100BodyAnimationSuccessors[(u16)work->bodyAnimation];
        if (_gActor136100BodyAnimationSuccessors[(u16)work->bodyAnimation] >= 0) {
            setIndex                   = nextSetIndex;
            restartWork                = task->work;
            restartWork->bodyAnimation = setIndex;
            goto restartSlots;
        unsettled:
            settled = 0;
            goto checkSettled;
        restartSlots:
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(restartWork->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&restartWork->rig.anim, slotIndex, setIndex, 0, ACTOR_136100_ANIMATION_BLEND_FRAMES);
            }
        }
        return 1;
    }
    return 0;
}

/// Allocates head lighting, joins the scene teardown tree and relocates textures.
///
/// Requires a live model/controller and borrowed parent coordinate in spawnArg2.
/// Only lighting uses the owned full-size work. Allocation failure retains the
/// remaining texture relocation and state advance; entry 106 falls back to END.
static inline void _actor136100InitializeHead(Task* task)
{
    enum { ACTOR_136100_HEAD_TEXTURE_ENTRY = 106 };
    _Actor136100Work* work;
    AreaPlacement*    place;
    u8                resourceEntryId;
    TmdObject*        model     = task->extra.tmd;
    GfxCoord*         rootCoord = model->coords;

    work       = memMalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
    } else {
        memFillBytes(work, 0, sizeof(*work));
        rootCoord->parent      = task->spawnArg2.pointer;
        task->extra.tmd->flags = 0;
        tmdAllocPrimitiveBuffer(model);
        model->lightMtx = &work->light;
        model->colorMtx = &work->color;
        task->msgTable  = _gActor136100ModelMessages;
        taskReparent(_gActor136100SceneTask, task);
    }
    place           = areaGetVariant(&gGameSession->location)->placements;
    resourceEntryId = place->entryId;
    while (resourceEntryId != AREA_PLACEMENT_END) {
        if (resourceEntryId == ACTOR_136100_HEAD_TEXTURE_ENTRY) {
            break;
        }
        place++;
        resourceEntryId = place->entryId;
    }
    tmdSetTextureOffsets(task->extra.tmd, place->texturePageOffset, place->clutRowOffset);
    task->state += 1;
}

/// Initializes and lights Gary Douglas's head-and-hat model for the street scene.
///
/// Start at state 0 with a live model, controller and borrowed parent coordinate
/// in `spawnArg2`. Owns a full `_Actor136100Work` but uses only its lighting matrices;
/// successful setup joins the controller's teardown tree. Placement entry 106
/// supplies texture offsets, falling back to the end record when absent. Later
/// ticks compose the root and refresh its lighting. Allocation failure kills the
/// task but still performs texture, state and lighting updates; retained behavior
/// requires successful allocation and live resources through this callback.
static void _actor136100HeadTask(Task* task)
{
    enum {
        ACTOR_136100_HEAD_LIGHT_COUNT = 3,
    };
    VECTOR samplePosition;

    if (task->state == 0) {
        _actor136100InitializeHead(task);
    }
    // Compose the attached root before sampling its world translation.
    {
        TmdObject* model = task->extra.tmd;

        actorRenderComposeCoord(model->coords);
        samplePosition.vx = task->extra.tmd->coords->workm.t[0];
        samplePosition.vy = task->extra.tmd->coords->workm.t[1];
        samplePosition.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(model, &samplePosition, 0, ACTOR_136100_HEAD_LIGHT_COUNT);
    }
}

/// Allocates the rifle's lighting work, attaches the model and applies its spawn pose.
///
/// Requires a live TMD model, the live scene controller and a borrowed parent
/// `GfxCoord` in `spawnArg2`. Owns a zeroed primary-heap `_Actor136100Work`,
/// using only its lighting matrices, and joins the controller's teardown tree.
/// Nonzero `spawnArg1` replaces pitch with a quarter turn and sets local Y to
/// 200 units. Allocation failure kills the task but still advances its state
/// and applies that optional pose.
static inline void _actor136100InitializeRifle(Task* task)
{
    _Actor136100Work* work;

    TmdObject* model     = task->extra.tmd;
    GfxCoord*  rootCoord = model->coords;

    work       = memMalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
    } else {
        memFillBytes(work, 0, sizeof(*work));
        rootCoord->parent      = task->spawnArg2.pointer;
        task->extra.tmd->flags = 0;
        tmdAllocPrimitiveBuffer(model);
        model->lightMtx = &work->light;
        model->colorMtx = &work->color;
        task->msgTable  = _gActor136100ModelMessages;
        taskReparent(_gActor136100SceneTask, task);
    }
    task->state += 1;
    if (task->spawnArg1.value != 0) {
        GfxCoord* posedRoot = task->extra.tmd->coords;

        gfxRotMatrixX(&posedRoot->coord, ACTOR_TRANSFORM_ANGLE_TURN / 4, GRAPHICS_ROTATION_REPLACE);
        posedRoot->coord.t[1]   = 200;
        posedRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Initializes and lights Gary Douglas's rifle model for the street scene.
///
/// Start at state 0 with a live model and a borrowed parent `GfxCoord` in
/// `spawnArg2`. The task owns a full `_Actor136100Work` until teardown, using
/// only its lighting matrices, and joins the scene controller's teardown tree.
/// Nonzero `spawnArg1` gives the attached rifle a quarter-turn pitch and a
/// 200-unit local Y offset. Later ticks refresh lighting at the composed root.
/// Allocation failure kills the task but retains the remaining first-tick
/// state and pose updates; callers must preserve that behavior.
static void _actor136100RifleTask(Task* task)
{
    VECTOR worldPosition;

    if (task->state == 0) {
        _actor136100InitializeRifle(task);
    }
    // Refresh lighting in world coordinates after composing the model root.
    {
        TmdObject* model = task->extra.tmd;

        actorRenderComposeCoord(model->coords);
        worldPosition.vx = task->extra.tmd->coords->workm.t[0];
        worldPosition.vy = task->extra.tmd->coords->workm.t[1];
        worldPosition.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(model, &worldPosition, 0, 3);
    }
}

/// Builds and sends the player's equipped-weapon animation request.
///
/// `controllerTask` is evaluated once and must own live controller work with
/// a borrowed player task. `request` is a writable AnimationPlayRequest lvalue,
/// evaluated repeatedly; `clipId` is evaluated twice. Both must have no side
/// effects. Blend mode and duration are evaluated once; duration counts normal
/// animation frames. Writes the unsigned animation cache before filling the
/// signed request clip. Disables world collision and dispatches synchronously.
/// Bank selection is weapon+1 for saved character 1, weapon+34 otherwise;
/// backing banks for the alternate range are unproven. Loaded clip data must
/// outlive playback. Expands to a standalone block with its own work reload.
#define ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(controllerTask, clipId, blendMode, blendDuration, request) \
    {                                                                                                        \
        _Actor136100Work* animationWork;                                                                     \
        s32               weaponId;                                                                          \
        s32               bankIndex;                                                                         \
        animationWork                  = (controllerTask)->work;                                             \
        weaponId                       = gPlayerStatus.weapon;                                               \
        bankIndex                      = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponId);                           \
        (request).source.index         = bankIndex;                                                          \
        animationWork->playerAnimation = (clipId);                                                           \
        (request).animationId          = (clipId);                                                           \
        (request).blend                = (blendMode);                                                        \
        (request).blendFrames          = (blendDuration);                                                    \
        (request).enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                  \
        TASK_MESSAGE_DISPATCH_POINTER(animationWork->playerTask, ANIMATION_MESSAGE_PLAY, &(request), 0);     \
    }

/// Places the player at a scene entrance and starts its scripted walk.
///
/// Requires live controller work and its borrowed player task. `path` contains
/// the immediate start placement and scripted-walk destination in
/// the player's root-parent frame, with Euler angles in 4096 units per turn.
/// Placement consumes XYZ and rotation; walking copies only destination XYZ.
/// Neither message retains the path, which need only survive the call.
/// Clears the entrance delay and advances request step 0 to the walk wait.
static inline void _actor136100BeginPlayerEntrance(_Actor136100Work* work, const _Actor136100PlayerEntrance* path)
{
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &path->start, 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &path->destination, 0);
    work->playerRequestFrames = 0;
    work->playerRequestStep++;
}

/// Advances player requests for the street scene after the Burner encounter.
///
/// Requires live controller work, player and loaded weapon/scene animation
/// banks. During an event, advances the player's animation chain first.
/// Entering places the player, starts a scripted walk, waits for it to finish
/// and waits six more ticks before blending into scene clip 47 for five frames.
/// Other requests place or start a clip immediately. Completed requests clear;
/// pending entrance steps survive to the next tick. Dispatch borrows requests
/// only during each call.
static void _actor136100UpdateAfterBurnerPlayerRequest(Task* task)
{
    enum { PLACE_AND_WALK        = 0,
           WAIT_FOR_WALK         = 1,
           WAIT_BEFORE_ANIMATION = 2,
           POSE_DELAY_FRAMES     = 6,
           POSE_BLEND_FRAMES     = 5 };

    _Actor136100Work*    work = task->work;
    AnimationPlayRequest request;

    if (gGameSession->eventState != 0) {
        _actor136100AdvancePlayerAnimation(task);
    }
    // Keep the entrance request pending through the walk and pose delay.
    switch ((u16)work->playerRequest) {
        case ACTOR_136100_REQUEST_NONE:
            break;
        case ACTOR_136100_PLAYER_AFTER_ENTER:
            switch ((u16)work->playerRequestStep) {
                case PLACE_AND_WALK:
                    _actor136100BeginPlayerEntrance(work, &_gActor136100AfterBurnerPlayerEntrance);
                    return;
                case WAIT_FOR_WALK:
                    if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                        work->playerRequestStep++;
                    }
                    return;
                case WAIT_BEFORE_ANIMATION:
                    if (++work->playerRequestFrames < POSE_DELAY_FRAMES) {
                        return;
                    }
                    ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_INTERPOLATE, POSE_BLEND_FRAMES, request);
                    break;
                default:
                    return;
            }
            break;
        case ACTOR_136100_PLAYER_AFTER_PLAY_ANIMATION_50:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, 50, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
            break;
        case ACTOR_136100_PLAYER_AFTER_PLAY_ANIMATION_52:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, 52, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
            break;
        case ACTOR_136100_PLAYER_AFTER_RESET_PLACEMENT:
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor136100AfterBurnerPlayerFollowUpPlacement, 0);
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, request);
            break;
        case ACTOR_136100_PLAYER_AFTER_BLEND_WEAPON:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, PLAYER_ANIMATION_IDLE, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
            break;
        case ACTOR_136100_PLAYER_AFTER_RESET_WEAPON:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, PLAYER_ANIMATION_IDLE, ANIMATION_BLEND_RESET, 0, request);
            break;
    }
    work->playerRequest = ACTOR_136100_REQUEST_NONE;
}

/// Records the selected body set without changing the rig's playback state.
///
/// Both arguments are evaluated once; bodyWork must point to live writable
/// controller work. The stored selection narrows to a signed halfword. Captures
/// no caller locals and forms one statement, requiring a trailing semicolon.
#define ACTOR_136100_SET_BODY_ANIMATION(bodyWork, setIndex) \
    do {                                                    \
        (bodyWork)->bodyAnimation = (setIndex);             \
    } while (0)

/// Blends body tracks 1 to 19 into `setIndex` and records that selection.
///
/// `task` must own an initialized `_Actor136100Work` rig. The signed halfword
/// set index must select loaded animation data for every driven track; current
/// callers use 2, 4, 5, 6, 7, 9, 10 and 11. Blends last ten normal-rate frames
/// from record 0. Track 0 and playback rates are untouched.
static inline void _actor136100BlendBodyAnimation(Task* task, s16 setIndex)
{
    _Actor136100Work* work = task->work;
    s32               slotIndex;

    ACTOR_136100_SET_BODY_ANIMATION(work, setIndex);
    for (slotIndex = 1; (u16)slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&work->rig.anim, (u16)slotIndex, setIndex, 0, ACTOR_136100_ANIMATION_BLEND_FRAMES);
    }
}

/// Restarts body tracks 1 to 19 at normal rate on a selected animation set.
///
/// Requires an initialized controller rig and loaded data for all driven
/// tracks. `setIndex` selects the rig's set table, not a track count; callers
/// use 1 or 3. The selection is cached as a signed halfword, while the full
/// signed word is passed to slot reset, which uses its low unsigned halfword.
/// Track 0 is untouched. The rig, slots and borrowed set data must stay live.
static inline void _actor136100ResetBodyTracks(Task* task, s32 setIndex)
{
    _Actor136100Work* work = task->work;
    s32               slotIndex;

    work->bodyAnimation = setIndex;
    slotIndex           = 1;
    do {
        work->rig.slots[(u16)slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, (u16)slotIndex, setIndex);
        slotIndex++;
    } while ((u16)slotIndex < ARRAY_SIZE(work->rig.slots));
}

/// Ticks the body rig and advances requests after the Burner encounter.
///
/// Requires initialized body tracks 1..19, loaded scene clips and a live player.
/// Animation messages tolerate an absent companion. Timed requests coordinate
/// body clips, companion clips and sound cues using frame counters; pending
/// steps survive to the next tick and completed requests clear. Track 0 is
/// untouched. Player and companion messages consume stack requests synchronously.
static void _actor136100UpdateAfterBurnerBodyRequest(Task* task)
{
/// Installs the scene companion's animation sets and plays a selected clip.
///
/// Reloads _Actor136100Work from the live task once. A NULL companionTask
/// suppresses the remaining argument evaluations. Otherwise clipId is a valid
/// clip in 0..5 and is evaluated twice; record is caller-owned writable
/// AnimationPlayRequest storage used repeatedly. All must be side-effect-free.
/// blendChoice selects reset or interpolation over frames normal-rate frames;
/// frames is 0..2047 when blending. Caches the companion selection, disables
/// world collision and dispatches synchronously. The companion borrows the
/// loaded set table throughout playback. Invoke within braces.
#define ACTOR_136100_PLAY_COMPANION_ANIMATION(task, clipId, blendChoice, frames, record)                              \
    {                                                                                                                 \
        _Actor136100Work* animWork = (task)->work;                                                                    \
                                                                                                                      \
        if (animWork->companionTask != NULL) {                                                                        \
            (record).source.sets          = _gActor136100CompanionAnimationSets;                                      \
            animWork->companionAnimation  = (clipId);                                                                 \
            (record).animationId          = (clipId);                                                                 \
            (record).blend                = (blendChoice);                                                            \
            (record).blendFrames          = (frames);                                                                 \
            (record).enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                        \
            TASK_MESSAGE_DISPATCH_POINTER(animWork->companionTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &(record), 0); \
        }                                                                                                             \
    }

    enum { BEGIN_REQUEST                   = 0,
           WAIT_FOR_CUE                    = 1,
           COMPANION_CUE_FRAMES            = 61,
           BODY_SOUND_FRAMES               = 17,
           PLAYER_AND_COMPANION_CUE_FRAMES = 21,
           SECOND_BODY_ANIMATION_FRAMES    = 80,
           BODY_SOUND_SCRIPT               = 0x0E };

    _Actor136100Work*    work = task->work;
    AnimationPlayRequest request;

    // Tick the rig before applying this frame's timed performance cues.
    _actor136100TickBodyAnimation(task);
    switch ((u16)work->bodyRequest) {
        case ACTOR_136100_REQUEST_NONE:
            break;
        case ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_2:
            switch ((u16)work->bodyRequestStep) {
                case BEGIN_REQUEST:
                    work->bodyRequestFrames = 0;
                    _actor136100BlendBodyAnimation(task, 2);
                    work->bodyRequestStep++;
                    return;
                case WAIT_FOR_CUE:
                    if (++work->bodyRequestFrames < COMPANION_CUE_FRAMES) {
                        return;
                    }
                    ACTOR_136100_PLAY_COMPANION_ANIMATION(task, 3, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
                    break;
                default:
                    return;
            }
            break;
        case ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_5:
            switch ((u16)work->bodyRequestStep) {
                case BEGIN_REQUEST:
                    work->bodyRequestFrames = 0;
                    _actor136100BlendBodyAnimation(task, 5);
                    work->bodyRequestStep++;
                    return;
                case WAIT_FOR_CUE:
                    if (++work->bodyRequestFrames == BODY_SOUND_FRAMES) {
                        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, BODY_SOUND_SCRIPT), 0, 0);
                    }
                    if (work->bodyRequestFrames < PLAYER_AND_COMPANION_CUE_FRAMES) {
                        return;
                    }
                    ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ANIMATION_BANK_BASE_SET_COUNT + 1, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
                    ACTOR_136100_PLAY_COMPANION_ANIMATION(task, 4, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
                    break;
                default:
                    return;
            }
            break;
        case ACTOR_136100_BODY_AFTER_BLEND_ANIMATION_7:
            _actor136100BlendBodyAnimation(task, 7);
            break;
        case ACTOR_136100_BODY_AFTER_BLEND_ANIMATION_9:
            _actor136100BlendBodyAnimation(task, 9);
            break;
        case ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_10_THEN_11:
            switch ((u16)work->bodyRequestStep) {
                case BEGIN_REQUEST:
                    work->bodyRequestFrames = 0;
                    _actor136100BlendBodyAnimation(task, 10);
                    work->bodyRequestStep++;
                    return;
                case WAIT_FOR_CUE:
                    if (++work->bodyRequestFrames < SECOND_BODY_ANIMATION_FRAMES) {
                        return;
                    }
                    _actor136100BlendBodyAnimation(task, 11);
                    break;
                default:
                    return;
            }
            break;
        case ACTOR_136100_BODY_AFTER_RESET:
            _actor136100ResetBodyTracks(task, ACTOR_136100_BODY_AFTER_BURNER_START);
            break;
    }
    work->bodyRequest = ACTOR_136100_REQUEST_NONE;
}
#undef ACTOR_136100_PLAY_COMPANION_ANIMATION

/// Installs the scene's companion sets and starts the selected animation.
///
/// `setIndex` is 0..5 in the installed table. Zero `blendChoice` resets playback;
/// nonzero requests interpolation over `blendFrames` normal-rate frames.
/// Caches the selection and disables world collision. Does nothing when no
/// companion is registered. The controller and loaded clip data must remain
/// live; dispatch consumes the stack request synchronously and borrows the sets.
static inline void _actor136100PlayCompanionAnimation(Task* task, u16 setIndex, s32 blendChoice, s32 blendFrames)
{
    _Actor136100Work*    work = task->work;
    AnimationPlayRequest request;

    if (work->companionTask != NULL) {
        request.source.sets          = _gActor136100CompanionAnimationSets;
        work->companionAnimation     = setIndex;
        request.animationId          = setIndex;
        request.blend                = blendChoice;
        request.blendFrames          = blendFrames;
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
    }
}

/// Advances the companion's animation chain and after-Burner requests.
///
/// The cached set index must be 0..5; negative successors leave the final pose.
/// Animation updates tolerate an absent companion, but placement requests
/// require a live registered task. The repeated cue plays sound and set 2 three
/// times, fifteen ticks apart, then waits another interval before blending to
/// set 1. Pending repetitions survive and completed requests clear. Loaded
/// companion sets remain borrowed throughout playback.
static void _actor136100UpdateAfterBurnerCompanionRequest(Task* task)
{
    enum { BEGIN_REPEATS          = 0,
           PLAY_REPEATS           = 1,
           REPEAT_COUNT           = 3,
           REPEAT_INTERVAL_FRAMES = 15,
           REPEAT_SOUND_SCRIPT    = 9 };

    _Actor136100Work* work;
    s16               successorAnimation;

    work = task->work;
    if (work->companionTask != NULL && taskMessageDispatch(work->companionTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        successorAnimation = _gActor136100CompanionAnimationSuccessors[work->companionAnimation];
        if (successorAnimation >= 0) {
            _actor136100PlayCompanionAnimation(task, successorAnimation, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES);
        }
    }
    // Each repetition waits a full interval; the final wait returns to set 1.
    switch ((u16)work->companionRequest) {
        case ACTOR_136100_REQUEST_NONE:
            break;
        case ACTOR_136100_COMPANION_AFTER_RESET_ANIMATION_1:
            _actor136100PlayCompanionAnimation(task, 1, ANIMATION_BLEND_RESET, 0);
            break;
        case ACTOR_136100_COMPANION_AFTER_PLAY_REPEATED_CUE:
            switch ((u16)work->companionRequestStep) {
                case BEGIN_REPEATS:
                    work->companionRepeatCount = 0;
                    work->companionRepeatDelay = 0;
                    work->companionRequestStep++;
                    return;
                case PLAY_REPEATS:
                    if (--work->companionRepeatDelay > 0) {
                        return;
                    }
                    if (work->companionRepeatCount >= REPEAT_COUNT) {
                        _actor136100PlayCompanionAnimation(task, 1, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES);
                        break;
                    }
                    sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_ACTOR_800200, REPEAT_SOUND_SCRIPT), 0, 0);
                    _actor136100PlayCompanionAnimation(task, 2, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES);
                    work->companionRepeatDelay = REPEAT_INTERVAL_FRAMES;
                    work->companionRepeatCount++;
                    return;
                default:
                    return;
            }
            break;
        case ACTOR_136100_COMPANION_AFTER_RESET_PLACEMENT_AND_ANIMATION:
            TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor136100AfterBurnerCompanionPlacement, 0);
            _actor136100PlayCompanionAnimation(task, 0, ANIMATION_BLEND_RESET, 0);
            break;
    }
    work->companionRequest = ACTOR_136100_REQUEST_NONE;
}

/// Advances player requests for the street scene before the Burner encounter.
///
/// Requires live controller work, player and loaded weapon/scene animation
/// banks. During an event, advances the player's animation chain first.
/// Entering shows and places the player, starts a scripted walk, waits for it
/// to finish and waits six ticks before blending to scene clip 47 for five
/// frames. Other requests hide, place or start an animation immediately.
/// Completed requests clear; pending entrance steps survive to the next tick.
static void _actor136100UpdateBeforeBurnerPlayerRequest(Task* task)
{
    enum { PLACE_AND_WALK        = 0,
           WAIT_FOR_WALK         = 1,
           WAIT_BEFORE_ANIMATION = 2,
           POSE_DELAY_FRAMES     = 6,
           POSE_BLEND_FRAMES     = 5 };

    _Actor136100Work*    work = task->work;
    AnimationPlayRequest request;

    if (gGameSession->eventState != 0) {
        _actor136100AdvancePlayerAnimation(task);
    }
    // Keep the entrance request pending through the walk and pose delay.
    switch ((u16)work->playerRequest) {
        case ACTOR_136100_REQUEST_NONE:
            break;
        case ACTOR_136100_PLAYER_BEFORE_HIDE:
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE, 0);
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, PLAYER_ANIMATION_IDLE, ANIMATION_BLEND_RESET, 0, request);
            break;
        case ACTOR_136100_PLAYER_BEFORE_ENTER:
            switch ((u16)work->playerRequestStep) {
                case PLACE_AND_WALK:
                    taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO, 0);
                    _actor136100BeginPlayerEntrance(work, &_gActor136100BeforeBurnerPlayerEntrance);
                    return;
                case WAIT_FOR_WALK:
                    if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                        work->playerRequestStep++;
                    }
                    return;
                case WAIT_BEFORE_ANIMATION:
                    if (++work->playerRequestFrames < POSE_DELAY_FRAMES) {
                        return;
                    }
                    ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_INTERPOLATE, POSE_BLEND_FRAMES, request);
                    break;
                default:
                    return;
            }
            break;
        case ACTOR_136100_PLAYER_BEFORE_PLAY_ANIMATION_49:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, 49, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
            break;
        case ACTOR_136100_PLAYER_BEFORE_PLAY_ANIMATION_53:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, 53, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
            break;
        case ACTOR_136100_PLAYER_BEFORE_EXIT:
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor136100BeforeBurnerPlayerExitPlacement, 0);
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, PLAYER_ANIMATION_IDLE, ANIMATION_BLEND_RESET, 0, request);
            break;
        case ACTOR_136100_PLAYER_BEFORE_RESET_SCENE_ANIMATION:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, request);
            break;
        case ACTOR_136100_PLAYER_BEFORE_BLEND_WEAPON:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, PLAYER_ANIMATION_IDLE, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
            break;
    }
    work->playerRequest = ACTOR_136100_REQUEST_NONE;
}

/// Ticks the body rig and advances requests before the Burner encounter.
///
/// Requires initialized tracks 1..19 and loaded sets. Head turns replace part
/// 4's local yaw in 64-unit steps, with 4096 units per turn, and deliberately
/// use `playerRequestStep` rather than `bodyRequestStep`. Returning the head
/// clears the request only after passing a full turn. Other requests place the
/// body, reset or blend its tracks, or wait fifteen ticks before a sound cue.
/// Completed requests clear; pending timed and head-turn requests survive.
static void _actor136100UpdateBeforeBurnerBodyRequest(Task* task)
{
    enum { BEGIN_REQUEST     = 0,
           WAIT_FOR_CUE      = 1,
           BODY_SOUND_FRAMES = 15,
           HEAD_PART_INDEX   = 4,
           HEAD_TURN_STEP    = 64,
           HEAD_TURN_LIMIT   = 0xD56,
           BODY_SOUND_SCRIPT = 0x0F };

    _Actor136100Work* work;
    GfxCoord*         coords;

    work = task->work;
    _actor136100TickBodyAnimation(task);
    switch ((u16)work->bodyRequest) {
        case ACTOR_136100_REQUEST_NONE:
            break;
        case ACTOR_136100_BODY_BEFORE_PLACE_FIRST:
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor136100BeforeBurnerBodyFirstPlacement, 0);
            break;
        case ACTOR_136100_BODY_BEFORE_PLACE_SECOND:
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor136100BeforeBurnerBodySecondPlacement, 0);
            break;
        case ACTOR_136100_BODY_BEFORE_BLEND_ANIMATION_4:
            _actor136100BlendBodyAnimation(task, 4);
            break;
        case ACTOR_136100_BODY_BEFORE_RESET_AND_PLACE:
            _actor136100ResetBodyTracks(task, ACTOR_136100_BODY_BEFORE_BURNER_START);
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor136100BeforeBurnerBodyRestPlacement, 0);
            break;
        case ACTOR_136100_BODY_BEFORE_PLAY_ANIMATION_6_AND_SOUND:
            switch ((u16)work->bodyRequestStep) {
                case BEGIN_REQUEST:
                    _actor136100BlendBodyAnimation(task, 6);
                    work->bodyRequestFrames = 0;
                    work->bodyRequestStep++;
                    return;
                case WAIT_FOR_CUE:
                    if (++work->bodyRequestFrames == BODY_SOUND_FRAMES) {
                        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, BODY_SOUND_SCRIPT), 0, 0);
                        work->bodyRequest = ACTOR_136100_REQUEST_NONE;
                    }
                    return;
            }
            return;
        case ACTOR_136100_BODY_BEFORE_TURN_HEAD:
            // The head turn counts its steps on the player request's step, which
            // the player request the script posts just before it leaves at 0.
            switch ((u16)work->playerRequestStep) {
                case BEGIN_REQUEST:
                    TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor136100BeforeBurnerBodySecondPlacement, 0);
                    work->headYaw = ACTOR_TRANSFORM_ANGLE_TURN;
                    work->playerRequestStep++;
                    return;
                case WAIT_FOR_CUE:
                    gfxRotMatrixY(&task->extra.tmd->coords[HEAD_PART_INDEX].coord, work->headYaw, GRAPHICS_ROTATION_REPLACE);
                    if (work->headYaw >= HEAD_TURN_LIMIT) {
                        work->headYaw -= HEAD_TURN_STEP;
                    }
                    return;
            }
            return;
        case ACTOR_136100_BODY_BEFORE_RETURN_HEAD:
            // Clear only after passing a full turn; equality takes one more tick.
            coords = task->extra.tmd->coords;
            if ((work->headYaw += HEAD_TURN_STEP) > ACTOR_TRANSFORM_ANGLE_TURN) {
                work->headYaw     = 0;
                work->bodyRequest = ACTOR_136100_REQUEST_NONE;
            }
            gfxRotMatrixY(&coords[HEAD_PART_INDEX].coord, work->headYaw, GRAPHICS_ROTATION_REPLACE);
            return;
        case ACTOR_136100_BODY_BEFORE_RESET:
            _actor136100ResetBodyTracks(task, ACTOR_136100_BODY_BEFORE_BURNER_START);
            break;
    }
    work->bodyRequest = ACTOR_136100_REQUEST_NONE;
}

/// Advances the companion's animation chain and before-Burner placements.
///
/// The cached set index must be 0..5 in the loaded companion table; a negative
/// successor leaves the final pose. Animation updates tolerate an absent task,
/// but placement requests require a live registered companion. Each placement
/// is consumed synchronously and clears its request. Loaded sets remain
/// borrowed during playback; blends last ten normal-rate frames.
static void _actor136100UpdateBeforeBurnerCompanionRequest(Task* task)
{
    _Actor136100Work* work;
    s16               nextSetIndex;

    work = task->work;
    if (work->companionTask != NULL && taskMessageDispatch(work->companionTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        nextSetIndex = _gActor136100CompanionAnimationSuccessors[work->companionAnimation];
        if (nextSetIndex >= 0) {
            _actor136100PlayCompanionAnimation(task, nextSetIndex, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES);
        }
    }
    switch ((u16)work->companionRequest) {
        case ACTOR_136100_REQUEST_NONE:
            break;
        case ACTOR_136100_COMPANION_BEFORE_PLACE_FIRST:
            TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor136100BeforeBurnerCompanionFirstPlacement, 0);
            break;
        case ACTOR_136100_COMPANION_BEFORE_PLACE_SECOND:
            TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor136100BeforeBurnerCompanionSecondPlacement, 0);
            break;
        case ACTOR_136100_COMPANION_BEFORE_RESET_PLACEMENT:
            TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor136100BeforeBurnerCompanionRestPlacement, 0);
            break;
    }
    work->companionRequest = ACTOR_136100_REQUEST_NONE;
}

/// Restores the player and body poses used after the Burner encounter.
///
/// Requires the live scene controller and player. Cancels their pending
/// requests without clearing step counters, restarts body tracks 1 to 19 at
/// normal rate, places the player at the walk's destination and resets the
/// equipped-weapon animation with world collision disabled. The companion's
/// request and pose are untouched.
static void _actor136100ResetAfterBurnerScene(void)
{
    Task*                task;
    _Actor136100Work*    work;
    _Actor136100Work*    requestWork;
    SVECTOR              unused; // Unreferenced; removing it changes the compiled frame.
    AnimationPlayRequest request;
    s32                  weaponId;
    s32                  bankIndex;

    task                = _gActor136100SceneTask;
    work                = task->work;
    work->playerRequest = ACTOR_136100_REQUEST_NONE;
    work->bodyRequest   = ACTOR_136100_REQUEST_NONE;

    _actor136100ResetBodyTracks(task, ACTOR_136100_BODY_AFTER_BURNER_START);

    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor136100AfterBurnerPlayerEntrance.destination, 0);

    // Re-read work after placement dispatch before caching the player clip.
    requestWork                  = task->work;
    weaponId                     = gPlayerStatus.weapon;
    bankIndex                    = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponId);
    request.source.index         = bankIndex;
    requestWork->playerAnimation = ACTOR_136100_PLAYER_WEAPON_ANIMATION;
    request.animationId          = PLAYER_ANIMATION_IDLE;
    request.blend                = ANIMATION_BLEND_RESET;
    request.blendFrames          = 0;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(requestWork->playerTask, ANIMATION_MESSAGE_PLAY, &request, 0);
}

/// Restores the performers' starting placement before the Burner encounter.
///
/// Requires the live scene controller, player and companion. Cancels player
/// and body requests without clearing step counters, places the body and
/// companion, restarts body tracks 1 to 19 at normal rate and resets the
/// equipped-weapon animation with world collision disabled. Exactly 1 in
/// `resetHeadRotation` also replaces the head's local rotation with identity;
/// other values preserve its current rotation.
static void _actor136100ResetBeforeBurnerScene(s32 resetHeadRotation)
{
    Task*                task;
    _Actor136100Work*    work;
    _Actor136100Work*    requestWork;
    SVECTOR              unused; // Unreferenced; removing it changes the compiled frame.
    AnimationPlayRequest request;
    s32                  weaponId;
    s32                  bankIndex;

    task                = _gActor136100SceneTask;
    work                = task->work;
    work->playerRequest = ACTOR_136100_REQUEST_NONE;
    work->bodyRequest   = ACTOR_136100_REQUEST_NONE;

    TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor136100BeforeBurnerBodyRestPlacement, 0);

    _actor136100ResetBodyTracks(task, ACTOR_136100_BODY_BEFORE_BURNER_START);

    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor136100BeforeBurnerPlayerEntrance.start, 0);

    // Re-read work after placement dispatch before caching the player clip.
    requestWork                  = task->work;
    weaponId                     = gPlayerStatus.weapon;
    bankIndex                    = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponId);
    request.source.index         = bankIndex;
    requestWork->playerAnimation = ACTOR_136100_PLAYER_WEAPON_ANIMATION;
    request.animationId          = PLAYER_ANIMATION_IDLE;
    request.blend                = ANIMATION_BLEND_RESET;
    request.blendFrames          = 0;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(requestWork->playerTask, ANIMATION_MESSAGE_PLAY, &request, 0);

    TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor136100BeforeBurnerCompanionRestPlacement, 0);

    if (resetHeadRotation == 1) {
        gfxRotMatrixY(&task->extra.tmd->coords[4].coord, 0, GRAPHICS_ROTATION_REPLACE);
    }
}

/// Starts a follow-up street conversation from the matching manual room action.
///
/// Returns 1 when a script starts, otherwise 0. Requires the controller and
/// live player. Automatic hits, an unpressed interaction button, the attachment
/// wheel or a pending display transition prevent starting. Room actions 18/19
/// select the after-/before-Burner scene respectively. Installs the player's
/// scene clip extension first; the first conversation has a skip script and
/// sets `followUpSeen`, while later conversations use the short script.
static s16 _actor136100TryStartFollowUpScene(Task* task)
{
    _Actor136100Work* work = task->work;
    u16               triggerControl;
    u8                sceneAction;
    u8                actionArgument;
    s16               interactionRequested;

    interactionRequested = 0;
    if (worldCollisionReadActionHit(&triggerControl, &sceneAction, &actionArgument) != 0) {
        if (!((s16)triggerControl & WORLD_COLLISION_TRIGGER_AUTOMATIC)) {
            if ((triggerControl & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM) {
                interactionRequested = gPlayerStatus.interactionPressed != 0;
            }
        }
    }
    if (interactionRequested == 0 || gAttachmentState.mode == ATTACHMENT_MODE_WHEEL) {
        return 0;
    }
    if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
        return 0;
    }
    if ((s8)sceneAction == ACTOR_136100_ROOM_ACTION_FOLLOW_UP_AFTER_BURNER && work->scene == ACTOR_136100_SCENE_AFTER_BURNER) {
        _actor136100CopyPlayerSceneAnimations(task);
        if (work->followUpSeen == 0) {
            evsStartScriptWithSkip(_gActor136100AfterBurnerFirstFollowUpScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor136100AfterBurnerFirstFollowUpSkipScript);
            work->followUpSeen++;
        } else {
            evsStartScript(_gActor136100AfterBurnerRepeatFollowUpScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        }
        return 1;
    }
    if ((s8)sceneAction == ACTOR_136100_ROOM_ACTION_FOLLOW_UP_BEFORE_BURNER && work->scene == ACTOR_136100_SCENE_BEFORE_BURNER) {
        _actor136100CopyPlayerSceneAnimations(task);
        if (work->followUpSeen == 0) {
            evsStartScriptWithSkip(_gActor136100BeforeBurnerFirstFollowUpScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor136100BeforeBurnerFirstFollowUpSkipScript);
            work->followUpSeen++;
        } else {
            evsStartScript(_gActor136100BeforeBurnerRepeatFollowUpScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        }
        return 1;
    }
    return 0;
}

/// Initializes the night main-street scene body and its animation/message bindings.
///
/// Allocates zeroed task-owned work, killing the task on failure, and publishes
/// the body task. Parents the root to the view, allocates primitive buffers and
/// lends work matrices to the model. Placement entry 106, or the end record when
/// absent, supplies texture offsets. Binds loaded clips without starting one;
/// the caller selects the burner-dependent scene and starts playback.
static void _actor136100InitBody(Task* task)
{
    enum { TEXTURE_RESOURCE_ENTRY_ID = 0x6A };

    _Actor136100Work* work;
    _Actor136100Work* allocatedWork;
    TmdObject*        tmd;
    GfxCoord*         coord;
    AreaPlacement*    place;
    u8                entryId;

    tmd           = task->extra.tmd;
    coord         = tmd->coords;
    allocatedWork = memMalloc(sizeof(_Actor136100Work), false);
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        taskKill(task);
        return;
    }
    work = allocatedWork;
    memFillBytes(work, 0, sizeof(*work));
    work->playerTask       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    _gActor136100SceneTask = task;
    coord->parent          = &gGfxViewCoord;
    tmdAllocPrimitiveBuffer(tmd);
    tmd->lightMtx = &work->light;
    tmd->colorMtx = &work->color;
    tmd->flags   &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
    place         = areaGetVariant(&gGameSession->location)->placements;
    entryId       = place->entryId;
    while (entryId != AREA_PLACEMENT_END) {
        if (entryId == TEXTURE_RESOURCE_ENTRY_ID) {
            break;
        }
        place++;
        entryId = place->entryId;
    }
    tmdSetTextureOffsets(tmd, place->texturePageOffset, place->clutRowOffset);
    animationInitContext(&work->rig.anim, _gActor136100BodyAnimationSets, tmd, work->rig.poses, work->rig.slots);
    task->msgTable = _gActor136100ModelMessages;
}

/// Classifies the latched room action selecting an opening street scene.
///
/// Returns `ACTOR_136100_OPENING_CUE_AFTER_BURNER` for room action 16,
/// `ACTOR_136100_OPENING_CUE_BEFORE_BURNER` for 17, otherwise
/// `ACTOR_136100_OPENING_CUE_NONE`. Both automatic and manual hits qualify.
/// The non-NULL, disjoint outputs receive the full control word and both action
/// bytes whenever a hit exists; no hit leaves them untouched. Reading retains
/// the latch, so another call before the next collision tick can see it again.
static inline s16 _actor136100ReadOpeningSceneCue(u16* triggerControl, u8* sceneAction, u8* actionArgument)
{
    if (worldCollisionReadActionHit(triggerControl, sceneAction, actionArgument) != 0) {
        if ((*triggerControl & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM) {
            if ((s8)*sceneAction == ACTOR_136100_ROOM_ACTION_OPEN_AFTER_BURNER) {
                return ACTOR_136100_OPENING_CUE_AFTER_BURNER;
            }
            if ((s8)*sceneAction == ACTOR_136100_ROOM_ACTION_OPEN_BEFORE_BURNER) {
                return ACTOR_136100_OPENING_CUE_BEFORE_BURNER;
            }
        }
    }
    return ACTOR_136100_OPENING_CUE_NONE;
}

/// Installs the companion's scene animation table and resets its requested clip.
///
/// `record` is accessed repeatedly and must be a side-effect-free request
/// lvalue. `clipId` is evaluated twice and must be side-effect-free; `task` is
/// evaluated once. Dispatch consumes the request synchronously.
/// A NULL companion suppresses evaluation of the clip and record arguments.
/// Per-expansion work pointers preserve the original call scheduling.
#define ACTOR_136100_RESET_COMPANION_ANIMATION(task, clipId, record)                                                  \
    {                                                                                                                 \
        _Actor136100Work* animWork = (task)->work;                                                                    \
                                                                                                                      \
        if (animWork->companionTask != NULL) {                                                                        \
            (record).source.sets          = _gActor136100CompanionAnimationSets;                                      \
            animWork->companionAnimation  = (clipId);                                                                 \
            (record).animationId          = (clipId);                                                                 \
            (record).blend                = ANIMATION_BLEND_RESET;                                                    \
            (record).blendFrames          = 0;                                                                        \
            (record).enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                        \
            TASK_MESSAGE_DISPATCH_POINTER(animWork->companionTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &(record), 0); \
        }                                                                                                             \
    }

/// Copies seven set addresses into the player's writable animation-bank extension.
///
/// `record` must be a side-effect-free `AnimationBankCopyRequest` lvalue. The table is
/// null-terminated; the receiver copies only the preceding entries.
/// task is evaluated once and reloads the scene work; playerTask must be live.
/// Counts set pointers with an unsigned halfword, excluding the terminator.
/// Dispatch copies seven address words to bank slots 47..53 synchronously.
/// The request is borrowed for the call; clip resources stay loaded for playback.
#define ACTOR_136100_COPY_PLAYER_ANIMATION_SETS(task, record)                                                    \
    {                                                                                                            \
        _Actor136100Work* msgWork = (task)->work;                                                                \
        u16               setCount;                                                                              \
                                                                                                                 \
        setCount = 0;                                                                                            \
        while (_gActor136100PlayerSceneAnimationSets[setCount] != NULL) {                                        \
            setCount += 1;                                                                                       \
        }                                                                                                        \
        (record).sets  = &_gActor136100PlayerSceneAnimationSets[0];                                              \
        (record).count = setCount;                                                                               \
        TASK_MESSAGE_DISPATCH_POINTER(msgWork->playerTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &(record), 0); \
    }

/// Refreshes body lighting at the composed translation of model part 1.
///
/// Requires a live body model with part 1, writable lighting matrices and a
/// writable `VECTOR` in `samplePosition`. Composes the part, copies its three
/// signed translation components in integer coordinate units and passes them
/// unchanged to the full room-light query. The fourth vector word is untouched;
/// the query borrows the buffer only through the call.
static inline void _actor136100UpdateBodyLighting(Task* task, VECTOR* samplePosition)
{
    enum { LIGHT_SAMPLE_PART_INDEX = 1,
           ROOM_LIGHT_COUNT        = 3 };

    TmdObject* model = task->extra.tmd;

    actorRenderComposeCoord(&model->coords[LIGHT_SAMPLE_PART_INDEX]);
    samplePosition->vx = task->extra.tmd->coords[LIGHT_SAMPLE_PART_INDEX].workm.t[0];
    samplePosition->vy = task->extra.tmd->coords[LIGHT_SAMPLE_PART_INDEX].workm.t[1];
    samplePosition->vz = task->extra.tmd->coords[LIGHT_SAMPLE_PART_INDEX].workm.t[2];
    worldCoordSetModelLighting(model, samplePosition, 0, ROOM_LIGHT_COUNT);
}

/// Runs Gary Douglas's opening and repeat conversations on the night main street.
///
/// Start at state 0 with scene resources loaded and the opening unseen. Owns
/// body work and head/rifle children; the selected Burner progression determines
/// their staging and the three player/body/companion request channels. Opening
/// cues wait for attachment and display transitions before starting the script.
/// Afterward, alternate waiting for a follow-up and its completion. Each active
/// tick refreshes part-1 lighting and the ground shadow. Stack request/geometry
/// views are consumed before reuse. Setup requires work allocation to succeed;
/// the retained caller continues after its initializer's failure exit.
/// Opening weapon requests require saved character 1 and weapon 0..32 for the
/// loaded player-bank table; the retained alternate +34 path is unproven.
static void _actor136100SceneTask(Task* task)
{
    enum {
        ACTOR_136100_SCENE_INITIALIZE              = 0,
        ACTOR_136100_SCENE_STAGE_OPENING           = 1,
        ACTOR_136100_SCENE_WAIT_CUE                = 2,
        ACTOR_136100_SCENE_WAIT_OPENING_END        = 3,
        ACTOR_136100_SCENE_WAIT_FOLLOW_UP          = 4,
        ACTOR_136100_SCENE_WAIT_FOLLOW_UP_END      = 5,
        ACTOR_136100_SCENE_HEAD_TASK               = 2,
        ACTOR_136100_SCENE_RIFLE_TASK              = 3,
        ACTOR_136100_SCENE_AFTER_BURNER_OBJECTIVE  = 25,
        ACTOR_136100_SCENE_BEFORE_BURNER_OBJECTIVE = 26,
        ACTOR_136100_SCENE_SHADOW_Y_OFFSET         = 896,
        ACTOR_136100_SCENE_SHADOW_HALF_SIZE        = 768,
        ACTOR_136100_SCENE_HEAD_PART               = 4,
        ACTOR_136100_SCENE_RIFLE_PART              = 8,
        ACTOR_136100_SCENE_AFTER_COMPANION_CLIP    = 1,
        ACTOR_136100_SCENE_BEFORE_COMPANION_CLIP   = 5,
    };
    _Actor136100Work* work = task->work;
    SVECTOR           unused;
    union {
        AnimationPlayRequest     animation;
        AnimationBankCopyRequest copy;
        VECTOR                   shadowPosition;
        SVECTOR                  floorOffset;
    } message;
    s32 cue;
    u16 openingEventId;
    u8  openingEventKind;
    u8  openingEventSubId;
    u16 secondEventId;
    u8  secondEventKind;
    u8  secondEventSubId;

    switch (task->state) {
        case ACTOR_136100_SCENE_INITIALIZE:
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_MAIN_STREET_CUTSCENE_SEEN) != 0) {
                taskKill(task);
                return;
            }
            _actor136100InitBody(task);
            work           = task->work;
            work->scene    = gameFlagGetNibble(GAME_FLAG_BURNER_DEFEATED) == 0;
            work->headTask = taskSpawnFromTable(gActor136100SceneTaskDescriptors, ACTOR_136100_SCENE_HEAD_TASK, 0,
                                                task->extra.tmd->coords + ACTOR_136100_SCENE_HEAD_PART);
            if (work->scene == ACTOR_136100_SCENE_AFTER_BURNER) {
                _actor136100ResetBodyTracks(task, ACTOR_136100_BODY_AFTER_BURNER_START);
                work->rifleTask = taskSpawnFromTable(gActor136100SceneTaskDescriptors, ACTOR_136100_SCENE_RIFLE_TASK, 0, &gGfxViewCoord);
            } else {
                _actor136100ResetBodyTracks(task, ACTOR_136100_BODY_BEFORE_BURNER_START);
                work->rifleTask = taskSpawnFromTable(gActor136100SceneTaskDescriptors, ACTOR_136100_SCENE_RIFLE_TASK, 1,
                                                     task->extra.tmd->coords + ACTOR_136100_SCENE_RIFLE_PART);
                memCopyBytes(&_gActor136100BeforeBurnerCollisionNormals, gDryfieldNightMainStreetRooms2And3CollisionGridNormals, sizeof(_gActor136100BeforeBurnerCollisionNormals));
                memCopyBytes(&_gActor136100BeforeBurnerCollisionFaces, gDryfieldNightMainStreetRooms2And3CollisionGridFaces, sizeof(_gActor136100BeforeBurnerCollisionFaces));
                memCopyBytes(&_gActor136100BeforeBurnerCollisionVerts, gDryfieldNightMainStreetRooms2And3CollisionGridVerts, sizeof(_gActor136100BeforeBurnerCollisionVerts));
            }
            work->companionTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
            task->state++;
            break;
        case ACTOR_136100_SCENE_STAGE_OPENING:
            if (work->scene == ACTOR_136100_SCENE_AFTER_BURNER) {
                gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, ACTOR_136100_SCENE_AFTER_BURNER_OBJECTIVE);
                taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
                taskMessageDispatch(work->headTask, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
                taskMessageDispatch(work->rifleTask, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
                TASK_MESSAGE_DISPATCH_POINTER(work->rifleTask, ACTOR_MESSAGE_PLACE, &_gActor136100AfterBurnerRiflePlacement, 0);
                if (work->companionTask != NULL) {
                    TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor136100AfterBurnerCompanionPlacement, 0);
                }
                TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor136100AfterBurnerBodyPlacement, 0);
                _actor136100ResetBodyTracks(task, ACTOR_136100_BODY_AFTER_BURNER_START);
                ACTOR_136100_RESET_COMPANION_ANIMATION(task, ACTOR_136100_SCENE_AFTER_COMPANION_CLIP, message.animation);
            } else {
                gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, ACTOR_136100_SCENE_BEFORE_BURNER_OBJECTIVE);
                taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
                taskMessageDispatch(work->headTask, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
                taskMessageDispatch(work->rifleTask, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
                if (work->companionTask != NULL) {
                    TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor136100BeforeBurnerCompanionRestPlacement, 0);
                }
                TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor136100BeforeBurnerBodyRestPlacement, 0);
                _actor136100ResetBodyTracks(task, ACTOR_136100_BODY_BEFORE_BURNER_START);
                ACTOR_136100_RESET_COMPANION_ANIMATION(task, ACTOR_136100_SCENE_BEFORE_COMPANION_CLIP, message.animation);
            }
            task->state++;
            break;
        case ACTOR_136100_SCENE_WAIT_CUE:
            cue = _actor136100ReadOpeningSceneCue(&openingEventId, &openingEventKind, &openingEventSubId);
            if (cue == ACTOR_136100_OPENING_CUE_AFTER_BURNER && work->scene == ACTOR_136100_SCENE_AFTER_BURNER) {
                if (gAttachmentState.mode == ATTACHMENT_MODE_WHEEL) {
                    return;
                }
                if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                    return;
                }
                ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, PLAYER_ANIMATION_IDLE, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, message.animation);
                evsStartScriptWithSkip(_gActor136100AfterBurnerOpeningScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor136100AfterBurnerOpeningSkipScript);
                worldCollisionUnlinkTrigger(WORLD_COLLISION_TRIGGER_LIST_ACTION, &gDryfieldNightMainStreetRooms2And3ActionTriggers[DRYFIELD_NIGHT_MAIN_STREET_ACTION_TRIGGER_AFTER_BURNER]);
                ACTOR_136100_COPY_PLAYER_ANIMATION_SETS(task, message.copy);
                gameFlagSetNibble(GAME_FLAG_NIGHT_MAIN_STREET_CUTSCENE_SEEN, 1);
                task->state++;
                break;
            }
            if (_actor136100ReadOpeningSceneCue(&secondEventId, &secondEventKind, &secondEventSubId) == ACTOR_136100_OPENING_CUE_BEFORE_BURNER && work->scene == ACTOR_136100_SCENE_BEFORE_BURNER) {
                if (gAttachmentState.mode == ATTACHMENT_MODE_WHEEL) {
                    return;
                }
                if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                    return;
                }
                ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, PLAYER_ANIMATION_IDLE, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, message.animation);
                evsStartScriptWithSkip(_gActor136100BeforeBurnerOpeningScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor136100BeforeBurnerOpeningSkipScript);
                worldCollisionUnlinkTrigger(WORLD_COLLISION_TRIGGER_LIST_ACTION, &gDryfieldNightMainStreetRooms2And3ActionTriggers[DRYFIELD_NIGHT_MAIN_STREET_ACTION_TRIGGER_BEFORE_BURNER]);
                ACTOR_136100_COPY_PLAYER_ANIMATION_SETS(task, message.copy);
                gameFlagSetNibble(GAME_FLAG_NIGHT_MAIN_STREET_CUTSCENE_SEEN, 1);
                task->state++;
            }
            break;
        case ACTOR_136100_SCENE_WAIT_OPENING_END:
            if (gGameSession->eventState == 0) {
                task->state++;
            }
            break;
        case ACTOR_136100_SCENE_WAIT_FOLLOW_UP:
            if (_actor136100TryStartFollowUpScene(task) != 0) {
                task->state++;
            }
            break;
        case ACTOR_136100_SCENE_WAIT_FOLLOW_UP_END:
            if (gGameSession->eventState == 0) {
                task->state--;
            }
            break;
    }
    // Consume the script requests before lighting and the body ground shadow.
    if (work->scene == ACTOR_136100_SCENE_AFTER_BURNER) {
        _actor136100UpdateAfterBurnerPlayerRequest(task);
        _actor136100UpdateAfterBurnerBodyRequest(task);
        _actor136100UpdateAfterBurnerCompanionRequest(task);
    } else {
        _actor136100UpdateBeforeBurnerPlayerRequest(task);
        _actor136100UpdateBeforeBurnerBodyRequest(task);
        _actor136100UpdateBeforeBurnerCompanionRequest(task);
    }
    _actor136100UpdateBodyLighting(task, &message.shadowPosition);
    message.floorOffset.vx = 0;
    message.floorOffset.vy = ACTOR_136100_SCENE_SHADOW_Y_OFFSET;
    message.floorOffset.vz = 0;
    actorRenderDrawGroundShadow(&task->extra.tmd->coords[1], ACTOR_136100_SCENE_SHADOW_HALF_SIZE, &message.floorOffset);
}

#include "../../shared/screen/screen_fade_in.inc.c"
#undef SCREEN_FADE_IN_TASK

/// Darkens the street scene and blanks the display when the ramp completes.
///
/// Start at state 0 without owned work. Initialization allocates primary-heap
/// `ScreenFadeWork`, then draws and steps in the same tick; teardown releases
/// the work and allocation failure kills the task. The unsigned low halfword
/// of `spawnArg1` is intensity units added per tick (0 holds indefinitely).
/// Drawing uses the low red/green/red bytes before each step. Channels narrow
/// to signed halfwords without clamping; red reaching 256 ends the task and
/// disables display output. The display-restore latch cancels the task after
/// drawing and stepping, without blanking output. Other states do nothing.
static void _actor136100FadeOutTask(Task* task)
{
    enum { INITIALIZE              = 0,
           RAMP                    = 1,
           DISPLAY_BLANK_THRESHOLD = 256 };

    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case INITIALIZE:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                return;
            }
            fade         = allocatedFade;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            task->state += 1;
            // The first tick also queues the overlay and advances the ramp.
            /* fallthrough */
        case RAMP:
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            _screenFadeStepUp(fade, task);
            if (_gActor136100FadeOutCancelled != 0) {
                taskKill(task);
                return;
            }
            if (fade->r < DISPLAY_BLANK_THRESHOLD) {
                return;
            }
            SetDispMask(0);
            taskKill(task);
            break;
    }
}

/// Restarts the registered player's equipped-weapon animation for a scene.
///
/// Requires a live player and loaded weapon bank. Selects the bank from the
/// equipped weapon and saved character, resets its starting animation without
/// blending and disables world collision. The stack request is consumed
/// synchronously; this does not change the controller's animation cache.
static void _actor136100ResetPlayerWeaponAnimation(void)
{
    AnimationPlayRequest request;
    s32                  weaponId;
    s32                  bankIndex;

    weaponId                     = gPlayerStatus.weapon;
    bankIndex                    = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponId);
    request.source.index         = bankIndex;
    request.animationId          = PLAYER_ANIMATION_IDLE;
    request.blend                = ANIMATION_BLEND_RESET;
    request.blendFrames          = 0;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
}

/// Shows the receiving model for a nonzero first payload and hides it for zero.
///
/// Requires a live `TmdObject`; only its active-draw suppression bit changes.
/// The message id and second payload are ignored. Senders must discard the
/// unspecified message result.
static s32 _actor136100SetModelDraw(Task* task, s32 unusedMessageId, s32 visible, s32 unusedSecondArg)
{
    TmdObject* model;

    model = task->extra.tmd;
    if (visible != 0) {
        model->flags = model->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    // The binary leaves the message result unspecified.
}

#include "../../shared/actor/actor_messages_place_in_view.inc.c"

/// Restarts the after-Burner body pose on tracks 1 to 19 at normal rate.
///
/// Requires the live scene controller and initialized rig. Slot 0, pending
/// requests and their step counters are untouched; this does not blend.
static void _actor136100ResetBodyAnimation(void)
{
    _Actor136100Work* work = _gActor136100SceneTask->work;
    SVECTOR           unused; // Unreferenced; removing it changes the compiled frame.
    s32               slotIndex;

    work->bodyAnimation = ACTOR_136100_BODY_AFTER_BURNER_START;
    slotIndex           = 1;
    do {
        work->rig.slots[(u16)slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, (u16)slotIndex, ACTOR_136100_BODY_AFTER_BURNER_START);
        slotIndex++;
    } while ((u16)slotIndex < ARRAY_SIZE(work->rig.slots));
}

/// Posts a player request for the scene controller's next frame update.
///
/// Requires the live controller. Stores the signed halfword request and resets
/// its step to 0, leaving the frame counter intact. Zero cancels; other codes
/// are the `ACTOR_136100_PLAYER_AFTER_*` or `ACTOR_136100_PLAYER_BEFORE_*`
/// commands for the controller's selected scene.
static void _actor136100PostPlayerRequest(s16 request)
{
    _Actor136100Work* work = _gActor136100SceneTask->work;

    work->playerRequest     = request;
    work->playerRequestStep = 0;
}

/// Posts a body request for the scene controller's next frame update.
///
/// Requires the live controller. Stores the signed halfword request and resets
/// its step to 0, leaving the frame counter intact. Zero cancels; other codes
/// are the `ACTOR_136100_BODY_AFTER_*` or `ACTOR_136100_BODY_BEFORE_*`
/// commands for the controller's selected scene.
static void _actor136100PostBodyRequest(s16 request)
{
    _Actor136100Work* work = _gActor136100SceneTask->work;

    work->bodyRequest     = request;
    work->bodyRequestStep = 0;
}

/// Posts a companion request for the scene controller's next frame update.
///
/// Requires the live controller. Stores the signed halfword request and resets
/// its step to 0, leaving repeat count and delay intact. Zero cancels; other
/// codes are the `ACTOR_136100_COMPANION_AFTER_*` or
/// `ACTOR_136100_COMPANION_BEFORE_*` commands for the selected scene.
static void _actor136100PostCompanionRequest(s16 request)
{
    _Actor136100Work* work = _gActor136100SceneTask->work;

    work->companionRequest     = request;
    work->companionRequestStep = 0;
}

/// Starts the street scene's subtractive fade-in at nine intensity units per tick.
///
/// Spawns task-table entry 4 with no parent-coordinate argument. The fade owns
/// its work until completion; this package must remain loaded while it runs.
static void _actor136100StartFadeIn(void)
{
    enum { FADE_TASK_INDEX    = 4,
           INTENSITY_PER_TICK = 9 };

    taskSpawnFromTable(gActor136100SceneTaskDescriptors, FADE_TASK_INDEX, INTENSITY_PER_TICK, NULL);
}

/// Starts the street scene's subtractive fade-out at nine intensity units per tick.
///
/// Spawns task-table entry 5 with no parent-coordinate argument. The fade owns
/// its work and blanks the display on completion unless the restore latch
/// cancels it. This package must remain loaded while the fade runs.
static void _actor136100StartFadeOut(void)
{
    enum { FADE_TASK_INDEX    = 5,
           INTENSITY_PER_TICK = 9 };

    taskSpawnFromTable(gActor136100SceneTaskDescriptors, FADE_TASK_INDEX, INTENSITY_PER_TICK, NULL);
}

/// Enables display output and latches cancellation of the scene's fade-out.
///
/// The latch remains set; a fade task observes it after drawing and stepping
/// its ramp. This does not clear the latch or synchronously destroy that task.
static void _actor136100RestoreDisplay(void)
{
    _gActor136100FadeOutCancelled = 1;
    SetDispMask(1);
}

/// Removes the player's equipment tasks once for the current scene.
///
/// Requires the live controller and player. Latches removal before tearing
/// down equipment; repeated calls do nothing until scene cleanup restores it.
static void _actor136100RemovePlayerEquipment(void)
{
    _Actor136100Work* work = _gActor136100SceneTask->work;

    if (work->playerEquipmentRemoved == 0) {
        work->playerEquipmentRemoved = 1;
        playerActorRemoveEquipment();
    }
}

/// Restores the player's removed equipment once at the end of a street scene.
///
/// Requires the published live controller and player. If removal is latched,
/// respawns equipment, clears the latch, then holds scripted player control.
/// Repeated calls after restoration do nothing.
static void _actor136100RestorePlayerEquipment(void)
{
    _Actor136100Work* work = _gActor136100SceneTask->work;

    if (work->playerEquipmentRemoved != 0) {
        playerActorRestoreEquipment();
        work->playerEquipmentRemoved = 0;
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    }
}

/// Applies companion schedules when the opening street scene finishes or skips.
///
/// Clears the trailer-coach door restriction and selects companion 1's
/// post-scene schedule. Zero `beforeBurner` selects companion 2's night
/// schedule; any nonzero value removes that companion and arms scene event 8
/// for subsequent music selection. The live save and game flags must exist.
static void _actor136100FinishOpeningScene(s32 beforeBurner)
{
    enum { COMPANION_1_POST_SCENE_SCHEDULE    = 3,
           COMPANION_2_AFTER_BURNER_SCHEDULE  = 6,
           BEFORE_BURNER_FINISHED_SCENE_EVENT = 8 };

    gameFlagSetNibble(GAME_FLAG_046, 0);
    gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, COMPANION_1_POST_SCENE_SCHEDULE);
    if (beforeBurner == 0) {
        gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, COMPANION_2_AFTER_BURNER_SCHEDULE);
    } else {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = BEFORE_BURNER_FINISHED_SCENE_EVENT;
        gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 0);
    }
}

/// Installs seven scene clips in the player's writable animation-bank extension.
///
/// Requires a live player with its weapon bank loaded and writable. Counts the
/// leading set pointers up to the NULL terminator and copies their 32-bit
/// address words starting at `ANIMATION_BANK_BASE_SET_COUNT` (47). The count
/// uses an unsigned halfword. Dispatch consumes the stack record and copies
/// the pointers synchronously; the seven descriptors and clip data must remain
/// live for playback. The terminator is excluded from the copy.
static void _actor136100CopyPlayerSceneAnimations(Task* task)
{
    _Actor136100Work*        work = task->work;
    AnimationBankCopyRequest request;
    u16                      setCount;

    setCount = 0;
    while (_gActor136100PlayerSceneAnimationSets[setCount] != NULL) {
        setCount += 1;
    }
    request.sets  = &_gActor136100PlayerSceneAnimationSets[0];
    request.count = setCount;
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &request, 0);
}
