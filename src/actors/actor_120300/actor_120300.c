#include "actors/actor_120300.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/captions.h"
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
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/dryfield_garage.h"
#include "../../shared/screen/screen_fade.h"
#include "../../shared/actor/actor_messages.h"

static void _screenFadeOutTask(Task* task);

/// Gary Douglas's texture resource in the garage area-placement table.
enum { ACTOR_120300_TEXTURE_RESOURCE_ENTRY_ID = 0x6A };

/// Execution priority shared by all five task recipes.
enum { ACTOR_120300_TASK_PRIORITY = 192 };

/// Fade-in duration for the current area's music, in gameplay ticks.
enum { ACTOR_120300_AREA_MUSIC_FADE_TICKS = 10 };

/// Negative follow-on entries retain the settled pose instead of starting a clip.
enum { ACTOR_120300_ANIMATION_NO_FOLLOW_ON = -1 };

/// CAP command slots in the garage's loaded dialogue bundle.
enum {
    ACTOR_120300_CAP_OPENING           = 1,
    ACTOR_120300_CAP_FIRST_TALK        = 2,
    ACTOR_120300_CAP_SECOND_TALK       = 3,
    ACTOR_120300_CAP_REPEAT_TALK       = 4,
    ACTOR_120300_CAP_JUNK_YARD_REFUSAL = 5,
    ACTOR_120300_CAP_OPENING_SKIPPED   = 12,
    ACTOR_120300_CAP_REMARK            = 14,
};

/// Indices of the scene's player, body and rifle placement records.
///
/// Clip names identify animation-table indices, not unproven visual actions.
/// Slots 0 and 4 are retained records with no recovered consumer.
enum {
    ACTOR_120300_PLACEMENT_PLAYER_APPROACH_END   = 1,
    ACTOR_120300_PLACEMENT_PLAYER_CLIP3          = 2,
    ACTOR_120300_PLACEMENT_PLAYER_CLIP11         = 3,
    ACTOR_120300_PLACEMENT_PLAYER_ROOM_PLAY      = 5,
    ACTOR_120300_PLACEMENT_PLAYER_APPROACH_START = 6,
    ACTOR_120300_PLACEMENT_BODY_SLIDE_START      = 7,
    ACTOR_120300_PLACEMENT_BODY_AFTER_SLIDE      = 8,
    ACTOR_120300_PLACEMENT_BODY_ROOM_PLAY        = 9,
    ACTOR_120300_PLACEMENT_BODY_CLIP17           = 10,
    ACTOR_120300_PLACEMENT_RIFLE_ROOM_PLAY       = 11,
    ACTOR_120300_PLACEMENT_BODY_DIMMED           = 12,
};

/// Body requests whose non-animation behavior is established.
enum {
    ACTOR_120300_BODY_REQUEST_SLIDE_LEFT = 1,
    ACTOR_120300_BODY_REQUEST_FULL_LIGHT = 14,
};

/// Values of `_Actor120300Work::interaction`: what the player's action button
/// started.
enum {
    ACTOR_120300_INTERACTION_TALK   = 0, // action trigger 1: the next of the three talks `talkStage` counts through
    ACTOR_120300_INTERACTION_REMARK = 1  // action trigger 2: one exchange during which the player looks toward the actor
};

/// Transition duration in whole normal-rate animation frames.
enum { ACTOR_120300_ANIMATION_BLEND_FRAMES = 10 };

/// Player requests whose actions are established independently of the clip content.
enum {
    ACTOR_120300_PLAYER_REQUEST_NONE         = 0,
    ACTOR_120300_PLAYER_REQUEST_AIM_AT_ACTOR = 20,
    ACTOR_120300_PLAYER_REQUEST_CENTER_AIM   = 21,
};

/// No pending body choreography request.
enum { ACTOR_120300_BODY_REQUEST_NONE = 0 };

/// Work block of Gary Douglas in Dryfield's garage.
///
/// The actor's task allocates it zeroed when it starts and keeps it at
/// `Task::work`. It holds the body's animation rig, storage for the model's
/// matrices, the tasks the scene addresses besides the body, and the state of
/// the scene's choreography. The tasks of the head and rifle models allocate a
/// block of this type as well and use only its `light` and `color`; they read
/// `scale` from this one through the task that spawned them.
///
/// The event scripts drive the scene by posting numbered requests: one for
/// the player in `playerRequest` and one for the actor's body in
/// `bodyRequest`. The tick performs a posted request once and clears it; a
/// request that takes several ticks counts its stages in the step beside it
/// and clears the request when it is done. Request 0 is none. Player request
/// 20 is the exception: it keeps the player's aim on the actor until another
/// request replaces it.
///
/// Angles are 4096ths of a turn. Timers count ticks. Nothing accesses the two
/// `pad` runs; their roles are unproven.
typedef struct {
    ActorAnimRig20 rig;                    // playback of the body's parts; slots 1 to 19 are driven
    MATRIX         light;                  // storage for the model's `TmdObject::lightMtx`
    MATRIX         color;                  // storage for the model's `TmdObject::colorMtx`
    Task*          playerTask;             // the player's task, which the player requests are sent to
    Task*          headTask;               // child task drawing the head-and-hat model, which hangs from the body's part 4
    Task*          rifleTask;              // child task drawing the rifle model: spawned hanging from the body's part 8, then set down at a fixed place in the room
    u16            playerRequest;          // request posted for the player, 0 to 21 (0 none)
    u16            playerRequestStep;      // stage of `playerRequest`, 0 when it is posted
    u16            playerRequestFrames;    // ticks player request 9 has waited; it plays its animation on the sixteenth
    byte           pad_4C6[0x2];           // never accessed
    u16            bodyRequest;            // request posted for the actor's body, 0 to 19 (0 none)
    u16            bodyRequestStep;        // stage of `bodyRequest`, 0 when it is posted
    byte           pad_4CC[0x6];           // never accessed
    u16            playerAnimation;        // animation last asked of the player, an index into the package's player animation sets; selects the animation that follows it
    u16            bodyAnimation;          // animation the body's slots were last started on, an index into the package's animation sets; selects the animation that follows it
    u16            talkStage;              // talks already held, which selects the script of the next (0 first, 1 second, 2 third and every one after); 1 from the start when the room is entered with the motel room 6 door unlocked
    u16            interactionStep;        // stage of the running interaction (0 start its script, 1 wait for the script to end)
    u16            interaction;            // `ACTOR_120300_INTERACTION_*`
    s16            playerAimYaw;           // yaw player requests 20 and 21 step by 48 a tick and store to the player's `GameActor::aimYaw`
    s16            playerEquipmentRemoved; // 1 from the scene killing the player's equipment tasks until it spawns the weapon's again (0 otherwise)
    u16            scale;                  // scale of the body, head and rifle models on all three axes, 0x1000 for 1.0
} _Actor120300Work;
STATIC_ASSERT_SIZEOF(_Actor120300Work, 0x4E4);

static TmdSource _gGaryBody;
static TmdSource _gGaryHeadHat;
static TmdSource _gGaryShotgun;
static void      _actor120300HeadTask(Task* task);
static void      _actor120300RifleTask(Task* task);
static void      _actor120300PrepareRoomPlay(s32 preservePlayer);
static void      _actor120300BodyTask(Task* task);
static void      _actor120300HandleModelDrawMessage(Task* task, s32 unusedMessageId, s32 visible, s32 unusedSecondArg);
static void      _actor120300SetModelsVisible(s32 visible);
static void      _actor120300PrepareScenePlayback(void);
static void      _actor120300StartScenePlayback(void);
static void      _actor120300FinishSceneStream(void);
static void      _actor120300PostPlayerRequest(s16 requestId);
static void      _actor120300PostBodyRequest(s16 requestId);
static void      _actor120300RemovePlayerEquipment(void);
static void      _actor120300RestorePlayerEquipment(void);
static void      _actor120300StartSceneFadeOut(void);
static void      _actor120300JunkYardRefusalTask(Task* task);

#include "assets/characters/gary/body.inc"

#include "assets/characters/gary/head_hat.inc"

#include "assets/characters/gary/shotgun.inc"

#include "assets/rooms/dryfield/garage/gary_clips.inc"

/// Player clip bank borrowed by the garage opening scene's scripted playback.
///
/// All indices 0..16 contain sets; there is no terminator. The player retains
/// the table and clip data until its equipped-weapon bank is restored, so this
/// actor package must remain loaded throughout that playback.
#include "assets/rooms/dryfield/garage/player_bank.inc"

/// Gary Douglas body clips borrowed by the twenty-part garage animation rig.
///
/// Slots 1 and 4..18 contain sets; 0, 2 and 3 are NULL and must not be played.
/// Drives parts 1..19, excluding the root. No terminator; the body rig borrows
/// this table and its sets for the body's lifetime.
#include "assets/rooms/dryfield/garage/gary_bank.inc"

/// Follow-on player clip indexed by the last installed garage clip, 0..16.
///
/// Negative entries keep the pose; nonnegative entries select the player bank.
/// Applied only while an event is active. Entry 17 is retained original storage
/// outside the loaded clip domain, not a terminator or a playable clip.
static s16 _gActor120300PlayerFollowOnAnimations[18] = {
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    10,
    10,
    10,
    10,
    10,
    0,
    0,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    12,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    0,
    0,
    0,
    10,
    0,
};

/// Next body clip once all animated body parts settle, indexed by body animation.
///
/// Played indices are 1 and 4..18; negative entries keep the settled pose.
/// Nonnegative entries within that domain select body sets 14 or 17. Entry 19
/// is retained original storage outside the loaded clip domain; its zero is
/// not a playable body set or a terminator.
static const s16 _gActor120300BodyFollowOnAnimations[20] SECTION(".data") = {
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    17,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    17,
    17,
    ACTOR_120300_ANIMATION_NO_FOLLOW_ON,
    14,
    0,
};

/// Three room-axis normals for Gary's garage collision patch, in signed Q12.
///
/// Replaces only normals 0..2 of the live garage grid during body initialization.
static const SVECTOR _gActor120300GarageCollisionPatchNormals[3] SECTION(".data") = {
#include "assets/actor_120300_collision_0EB88.inc"
};

/// Twelve signed room-coordinate vertices for Gary's garage collision patch.
///
/// Replaces only vertices 0..11 of the live garage grid; the remaining pool stays.
static const SVECTOR _gActor120300GarageCollisionPatchVerts[12] SECTION(".data") = {
#include "assets/actor_120300_collision_0EBA0.inc"
};

/// Three blocking class-0 quads replacing the garage grid's leading faces.
///
/// Index vertices 0..11 and normals 0..2 of the accompanying patch. Existing
/// grid cell lists remain valid; only these three face records are copied.
static const WorldCollisionGridFace _gActor120300GarageCollisionPatchFaces[3] SECTION(".data") = {
#include "assets/actor_120300_collision_0EC00.inc"
};

/// Placement and active-draw messages shared by Gary's body, head and rifle tasks.
///
/// No lookup terminator: send only `ACTOR_MESSAGE_SET_MODEL_DRAW` (integer
/// visibility: zero hides, nonzero shows) or `ACTOR_MESSAGE_PLACE` (borrowed
/// `ActorTransform`, position and all three angles). Both callbacks return void;
/// every sender must discard the dispatch result. The second payload is unused.
/// Models and this package must remain live through synchronous dispatch.
static const TaskMessageEntry _gActor120300ModelMessages[2] SECTION(".data") = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor120300HandleModelDrawMessage },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceInView },
};

/// Player, body and rifle placements for the garage opening and ordinary play.
///
/// `ACTOR_120300_PLACEMENT_*` selects a complete position/Euler-angle record:
/// XYZ uses signed world-coordinate units, angles use 4096 units per turn. Placement and
/// movement messages read the records synchronously. Slots 0 and 4 have no
/// recovered consumer; their purpose is unproven. No terminator.
static const ActorTransform _gActor120300ScenePlacements[13] SECTION(".data") = {
    { { 2414, 0, 2529, 0 }, { 0, 0, 0, 0 } },
    { { 2553, 0, 2396, 0 }, { 0, 1024, 0, 0 } },
    { { 2553, 0, 1700, 0 }, { 0, 1024, 0, 0 } },
    { { 2589, 0, 2068, 0 }, { 0, 1024, 0, 0 } },
    { { 1559, 0, 3583, 0 }, { 0, 1536, 0, 0 } },
    { { 2250, 0, 2200, 0 }, { 0, 1024, 0, 0 } },
    { { 800, 0, 3360, 0 }, { 0, 1536, 0, 0 } },
    { { 5000, 0, 1531, 0 }, { 0, 3072, 0, 0 } },
    { { 3792, 0, 1531, 0 }, { 0, 3072, 0, 0 } },
    { { 3475, 0, 2165, 0 }, { 0, 0, 0, 0 } },
    { { 3475, 0, 2165, 0 }, { 0, 3072, 0, 0 } },
    { { 3900, -300, 2070, 0 }, { 1638, 0, 0, 0 } },
    { { 8500, 0, 1531, 0 }, { 0, 3072, 0, 0 } },
};

/// Scene/audio stream key retained while Gary's first garage encounter plays.
///
/// Group 2, stream 3, first qualifier 11; the absent second qualifier is zero.
static const EvsSceneKey _gActor120300OpeningSceneKey SECTION(".data") = { .group = 2, .streamId = 3, .subId = 11 };

/// First garage encounter, synchronizing player/Gary choreography with CAP cues.
///
/// Starts the scene stream, temporarily removes equipment, then restores it and
/// releases scripted player control. Each numbered request is interpreted by
/// the actor's request channel; player request 6 remains unhandled.
/// Ends at command 101. The actor, player, garage and CAP resources stay loaded
/// through playback; `_gActor120300OpeningSkipScript` handles accepted skips.
static EvsCommand _gActor120300OpeningScript[102] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_120300_CAP_OPENING }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor120300OpeningSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor120300PrepareScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor120300StartScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor120300SetModelsVisible }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor120300RemovePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor120300SetModelsVisible }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = ACTOR_120300_BODY_REQUEST_FULL_LIGHT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor120300SetModelsVisible }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor120300SetModelsVisible }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = ACTOR_120300_BODY_REQUEST_FULL_LIGHT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor120300SetModelsVisible }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor120300SetModelsVisible }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = ACTOR_120300_BODY_REQUEST_SLIDE_LEFT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor120300SetModelsVisible }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor120300SetModelsVisible }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = ACTOR_120300_AREA_MUSIC_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor120300StartSceneFadeOut }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor120300RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor120300FinishSceneStream }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = ACTOR_120300_PLAYER_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = ACTOR_120300_BODY_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip recovery for the opening scene: restore equipment, room poses and control.
///
/// Requires the initialized body/player and child tasks. Fades around view 2 and
/// scene cleanup, starts the skipped-opening CAP command, clears both requests,
/// restores ordinary player control and starts area music with a ten-tick fade.
/// Ends at command 17; keep the actor, garage and CAP resources loaded until then.
static EvsCommand _gActor120300OpeningSkipScript[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor120300RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor120300PrepareRoomPlay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_120300_CAP_OPENING_SKIPPED }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = ACTOR_120300_PLAYER_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = ACTOR_120300_BODY_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = ACTOR_120300_AREA_MUSIC_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// First ordinary garage talk, using CAP command 2 and the player's equipped idle.
///
/// Waits for two CAP cues, clears both requests and releases scripted control.
/// Ends at command 8; requires the body/player and loaded garage/CAP resources.
static EvsCommand _gActor120300FirstTalkScript[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_120300_CAP_FIRST_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = ACTOR_120300_PLAYER_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = ACTOR_120300_BODY_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Second ordinary garage talk, using CAP command 3 and the player's equipped idle.
///
/// Waits for two CAP cues, clears both requests and releases scripted control.
/// Ends at command 8; requires the body/player and loaded garage/CAP resources.
static EvsCommand _gActor120300SecondTalkScript[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_120300_CAP_SECOND_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = ACTOR_120300_PLAYER_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = ACTOR_120300_BODY_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Third and subsequent ordinary garage talks, using CAP command 4.
///
/// Holds the player's equipped idle through two CAP cues, clears both requests
/// and releases scripted control. Ends at command 8; requires the body/player
/// and loaded garage/CAP resources.
static EvsCommand _gActor120300RepeatTalkScript[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_120300_CAP_REPEAT_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = ACTOR_120300_PLAYER_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = ACTOR_120300_BODY_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Garage exchange refusing departure through the junk-yard door, using CAP command 5.
///
/// Holds the player's equipped idle through two CAP cues, clears both requests
/// and releases scripted control. Ends at command 8; requires Gary's body/player
/// and loaded garage/CAP resources even though a separate control task starts it.
static EvsCommand _gActor120300JunkYardRefusalScript[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_120300_CAP_JUNK_YARD_REFUSAL }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = ACTOR_120300_PLAYER_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = ACTOR_120300_BODY_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Garage remark exchange, turning the player's upper-body aim toward Gary then centering it.
///
/// CAP command 14 supplies four cues. Clears both requests and releases scripted
/// control after centering. Ends at command 12; requires the body/player and
/// loaded garage/CAP resources.
static EvsCommand _gActor120300RemarkScript[13] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_120300_CAP_REMARK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = ACTOR_120300_PLAYER_REQUEST_AIM_AT_ACTOR }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = ACTOR_120300_PLAYER_REQUEST_CENTER_AIM }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostPlayerRequest }, { .value = ACTOR_120300_PLAYER_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120300PostBodyRequest }, { .value = ACTOR_120300_BODY_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc gActor120300TaskDescriptors[ACTOR_120300_TASK_COUNT] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_120300_TASK_PRIORITY } }, _actor120300BodyTask, { .model = &_gGaryBody } },
    { { { TASK_BODY_NONE, ACTOR_120300_TASK_PRIORITY } }, _actor120300JunkYardRefusalTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_120300_TASK_PRIORITY } }, _actor120300HeadTask, { .model = &_gGaryHeadHat } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_120300_TASK_PRIORITY } }, _actor120300RifleTask, { .model = &_gGaryShotgun } },
    { { { TASK_BODY_NONE, ACTOR_120300_TASK_PRIORITY } }, _screenFadeOutTask, { .value = 0 } },
};

/// Borrowed live Gary body task used by this package's event-script callbacks.
///
/// Published only after body work allocation succeeds. Supports one body;
/// callbacks require its work and addressed child/player tasks to remain live.
/// The handle is not cleared at teardown and must not be used afterward.
static Task* _gActor120300BodyTask;

static s32         _actor120300TickBodyAnimation(Task* task);
static inline s16  _actor120300InitChild(Task* task, s32 parentPartIndex);
static inline void _actor120300BlendPlayerAnimation(Task* task, u16 animationId);
static inline void _actor120300ResetPlayerAnimation(Task* task, u16 animationId);
static void        _actor120300RunPlayerRequest(Task* task);
static inline void _actor120300BlendBodyAnimation(Task* task, u16 animationId);
static inline void _actor120300ResetBodyAnimation(Task* task, u16 animationId);

/// Samples the body's cached position and rebuilds its room lighting.
///
/// `model` must be `task->extra.tmd`, with coordinate 1 already composed and
/// writable light/colour matrices. Writes signed 32-bit XYZ in game units to
/// caller-owned, word-aligned `samplePosition`; its fourth word is untouched.
/// The lighting query interprets the sample as world coordinates, selects all
/// three lights and retains no pointer. Requires the query's scratch capacity
/// and changes GTE state. This helper does not apply the scene's intensity scale.
static inline void _actor120300UpdateBodyLighting(const Task* task, const TmdObject* model, VECTOR* samplePosition)
{
    samplePosition->vx = model->coords[1].workm.t[0];
    samplePosition->vy = task->extra.tmd->coords[1].workm.t[1];
    samplePosition->vz = task->extra.tmd->coords[1].workm.t[2];
    worldCoordSetModelLighting(model, samplePosition, 0, 3);
}

/// Advances the body's animation and starts its next clip once every track settles.
///
/// Drives slots 1 to 19 of the initialized body rig. Returns 1 when all slots
/// settled on this tick, including when a follow-on clip has just started;
/// returns 0 while any slot is still playing. A negative follow-on entry keeps
/// the settled pose. The rig, model and package animation data must remain live.
static s32 _actor120300TickBodyAnimation(Task* task)
{
    _Actor120300Work* work;
    _Actor120300Work* restartWork;
    u16               nextAnimationId;
    u16               slotIndex;
    u16               allSettled;

    work = task->work;
    // Slot 0 is the model root; the animated body starts at slot 1.
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    slotIndex  = 1;
    allSettled = 1;
    for (; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        if (!(work->rig.slots[slotIndex].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
            goto unsettled;
        }
    }
checkSettled:
    if (allSettled) {
        if (_gActor120300BodyFollowOnAnimations[work->bodyAnimation] >= 0) {
            nextAnimationId            = _gActor120300BodyFollowOnAnimations[work->bodyAnimation];
            restartWork                = task->work;
            restartWork->bodyAnimation = nextAnimationId;
            goto restartSlots;
        unsettled:
            allSettled = 0;
            goto checkSettled;
        restartSlots:
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(restartWork->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&restartWork->rig.anim, slotIndex, nextAnimationId, 0, ACTOR_120300_ANIMATION_BLEND_FRAMES);
            }
        }
        return 1;
    }
    return 0;
}

/// Allocates a child model's lighting storage and attaches it to its parent's model.
///
/// `task` owns a live model and receives a zeroed work block, released by task
/// teardown. Its spawn argument must point to the live body task;
/// `parentPartIndex` is an element index in that body's coordinates (4 for the
/// head, 8 for the rifle). The parent must outlive the attachment. Returns 1 on
/// work-allocation failure and 0 after installing lighting and message handling.
static inline s16 _actor120300InitChild(Task* task, s32 parentPartIndex)
{
    TmdObject*        model     = task->extra.tmd;
    GfxCoord*         rootCoord = model->coords;
    _Actor120300Work* work;
    Task*             parentTask;

    work       = memMalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        return 1;
    }
    memFillBytes(work, 0, sizeof(*work));
    parentTask             = task->spawnArg2.pointer;
    rootCoord->parent      = parentTask->extra.tmd->coords + parentPartIndex;
    task->extra.tmd->flags = 0;
    tmdAllocPrimitiveBuffer(model);
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    task->msgTable  = _gActor120300ModelMessages;
    return 0;
}

/// Finds Gary Douglas's texture placement, falling back to the terminal record.
///
/// Requires the current area's live, end-terminated placement table. The
/// returned record is borrowed from that table; no resource or task is created.
static inline AreaPlacement* _actor120300FindTexturePlacement(void)
{
    AreaPlacement* placement;
    u8             entryId;

    placement = areaGetVariant(&gGameSession->location)->placements;
    entryId   = placement->entryId;
    while (entryId != AREA_PLACEMENT_END) {
        if (entryId == ACTOR_120300_TEXTURE_RESOURCE_ENTRY_ID) {
            break;
        }
        placement++;
        entryId = placement->entryId;
    }
    return placement;
}

/// Initializes Gary Douglas's head-and-hat attachment and updates its lighting.
///
/// Starts at state 0 with a live model and the body task in `spawnArg2.pointer`.
/// Allocates owned work/lighting storage and attaches the root to body part 4;
/// allocation failure kills the task and returns. Uses texture placement entry
/// 0x6A, falling back to the terminal record's texture offsets if it is absent.
/// The body and its work must outlive the attachment. Each tick samples the
/// cached root translation and applies the body's unsigned Q12 directional-light
/// intensity to the colour coefficients, preserving the ambient term.
static void _actor120300HeadTask(Task* task)
{
    enum { ACTOR_120300_HEAD_STATE_INITIALIZE = 0,
           ACTOR_120300_HEAD_PARENT_PART      = 4 };

    VECTOR            lightSample;
    AreaPlacement*    placement;
    u16               lightingScale;
    Task*             bodyTask;
    _Actor120300Work* bodyWork;
    TmdObject*        model;

    if (task->state == ACTOR_120300_HEAD_STATE_INITIALIZE) {
        if (_actor120300InitChild(task, ACTOR_120300_HEAD_PARENT_PART) != 0) {
            taskKill(task);
            return;
        }
        placement = _actor120300FindTexturePlacement();
        tmdSetTextureOffsets(task->extra.tmd, placement->texturePageOffset, placement->clutRowOffset);
        task->state += 1;
    }
    // Scale light coefficients after sampling; keep the ambient term intact.
    model          = task->extra.tmd;
    bodyTask       = task->spawnArg2.pointer;
    bodyWork       = bodyTask->work;
    lightingScale  = bodyWork->scale;
    lightSample.vx = model->coords->workm.t[0];
    lightSample.vy = task->extra.tmd->coords->workm.t[1];
    lightSample.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(model, &lightSample, 0, 3);
    lightSample.vz = lightingScale;
    lightSample.vy = lightingScale;
    lightSample.vx = lightingScale;
    ScaleMatrix(model->colorMtx, &lightSample);
}

/// Initializes the rifle attachment and updates its room lighting each tick.
///
/// The spawn argument borrows the body task. Initialization attaches the rifle
/// to body part 8; later placement messages may put it in the room instead.
/// Work-allocation failure kills the task. Lighting is sampled at the composed
/// rifle root and its directional-light coefficients receive the body's uniform
/// Q12 multiplier (4096 is full intensity); the ambient term is unchanged.
static void _actor120300RifleTask(Task* task)
{
    enum { RIFLE_PARENT_PART = 8 };

    VECTOR            lightingVector;
    u16               lightingScale;
    TmdObject*        model;
    Task*             parentTask;
    _Actor120300Work* parentWork;

    if (task->state == 0) {
        if (_actor120300InitChild(task, RIFLE_PARENT_PART) != 0) {
            taskKill(task);
            return;
        }
        task->state += 1;
    }
    // Sample room lighting before applying the scene's intensity multiplier.
    model             = task->extra.tmd;
    parentTask        = task->spawnArg2.pointer;
    parentWork        = parentTask->work;
    lightingScale     = parentWork->scale;
    lightingVector.vx = model->coords->workm.t[0];
    lightingVector.vy = task->extra.tmd->coords->workm.t[1];
    lightingVector.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(model, &lightingVector, 0, 3);
    lightingVector.vz = lightingScale;
    lightingVector.vy = lightingScale;
    lightingVector.vx = lightingScale;
    ScaleMatrix(model->colorMtx, &lightingVector);
}

/// Installs the garage scene's player clips and blends into the selected animation.
///
/// `animationId` is a loaded player-set index, 0 to 16. Playback enters scripted
/// mode at normal rate with world collision enabled and a ten-frame blend.
/// Records the clip for follow-on playback. Does nothing without a player task;
/// otherwise that task and the package data must remain live through playback.
static inline void _actor120300BlendPlayerAnimation(Task* task, u16 animationId)
{
    _Actor120300Work*    work = task->work;
    AnimationPlayRequest request;

    if (work->playerTask != NULL) {
        request.source.sets          = _gActor120300PlayerAnimationSets;
        work->playerAnimation        = animationId;
        request.animationId          = animationId;
        request.blend                = ANIMATION_BLEND_INTERPOLATE;
        request.blendFrames          = ACTOR_120300_ANIMATION_BLEND_FRAMES;
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
    }
}

/// Installs the garage scene's player clips and resets playback to the selected animation.
///
/// `animationId` is a loaded player-set index, 0 to 16. Enters scripted mode at
/// normal rate with world collision enabled, without blending from the old pose.
/// Records the clip for follow-on playback. Does nothing without a player task;
/// otherwise that task and the package data must remain live through playback.
static inline void _actor120300ResetPlayerAnimation(Task* task, u16 animationId)
{
    _Actor120300Work*    work = task->work;
    AnimationPlayRequest request;

    if (work->playerTask != NULL) {
        request.source.sets          = _gActor120300PlayerAnimationSets;
        work->playerAnimation        = animationId;
        request.animationId          = animationId;
        request.blend                = ANIMATION_BLEND_RESET;
        request.blendFrames          = 0;
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
    }
}

/// Computes the garage scene's relative player aiming yaw toward this actor.
///
/// Returns 4096 angle units per turn for the player's part-4 yaw. Both live
/// roots must use the scene's common parent frame; their rotations are ignored.
/// Full-width X ordering selects the X/Z signs. Differences use low unsigned
/// halfwords, then narrow to signed halfwords for `ratan2(deltaZ, deltaX)`.
/// No coordinate is composed or changed; no pointer is retained.
static inline s16 _actor120300GetPlayerAimYaw(const GfxCoord* actorCoord, const GfxCoord* playerCoord)
{
    s32 deltaX;
    s32 deltaZ;

    if (actorCoord->coord.t[0] > playerCoord->coord.t[0]) {
        deltaX = (u16)actorCoord->coord.t[0] - (u16)playerCoord->coord.t[0];
        deltaZ = (u16)playerCoord->coord.t[2] - (u16)actorCoord->coord.t[2];
    } else {
        deltaX = (u16)playerCoord->coord.t[0] - (u16)actorCoord->coord.t[0];
        deltaZ = (u16)actorCoord->coord.t[2] - (u16)playerCoord->coord.t[2];
    }
    return ratan2((s16)deltaZ, (s16)deltaX);
}

/// Advances player choreography posted by the garage scene's scripts.
///
/// The body work block must be initialized and its player task live whenever a
/// placement, rate or aim request is pending. Clip helpers tolerate no player.
/// During an event, a settled clip starts its nonnegative follow-on animation.
/// Requests normally clear after dispatch; request 9 waits sixteen updates,
/// aiming at the actor remains latched, and centering clears on reaching zero.
/// The other numbered beats are defined by the scripts and animation data.
static void _actor120300RunPlayerRequest(Task* task)
{
/// Restores equipped-weapon clip 1 with world collision disabled.
///
/// Requires a live player target and its loaded weapon package. Character 1 and
/// weapon ids 0..32 select banks 1..33; the retained alternate +34 selector has
/// no established backing in the current bank table. blendChoice is reset or
/// interpolate; frames is 0..2047 whole normal-rate frames when interpolating.
/// Arguments are evaluated once in request-fill/dispatch order. Every expansion
/// owns a separate stack request, borrowed synchronously by dispatch. Invoke
/// within braces; captures the current saved character and equipped weapon.
#define ACTOR_120300_PLAY_PLAYER_WEAPON_ANIMATION(target, blendChoice, frames)                                                                                                                                                                  \
    {                                                                                                                                                                                                                                           \
        AnimationPlayRequest request;                                                                                                                                                                                                           \
        s32                  weaponId;                                                                                                                                                                                                          \
        enum { ACTOR_120300_WEAPON_PRIMARY_CHARACTER   = 1,                                                                                                                                                                                     \
               ACTOR_120300_WEAPON_PRIMARY_BANK_BASE   = 1,                                                                                                                                                                                     \
               ACTOR_120300_WEAPON_ALTERNATE_BANK_BASE = 34,                                                                                                                                                                                    \
               ACTOR_120300_WEAPON_ANIMATION           = 1 };                                                                                                                                                                                             \
                                                                                                                                                                                                                                                \
        weaponId                     = gPlayerStatus.weapon;                                                                                                                                                                                    \
        request.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_120300_WEAPON_PRIMARY_CHARACTER) ? weaponId + ACTOR_120300_WEAPON_PRIMARY_BANK_BASE : weaponId + ACTOR_120300_WEAPON_ALTERNATE_BANK_BASE; \
        request.animationId          = ACTOR_120300_WEAPON_ANIMATION;                                                                                                                                                                           \
        request.blend                = (blendChoice);                                                                                                                                                                                           \
        request.blendFrames          = (frames);                                                                                                                                                                                                \
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                                                                                                                                                       \
        TASK_MESSAGE_DISPATCH_POINTER((target), ANIMATION_MESSAGE_PLAY, &request, 0);                                                                                                                                                           \
    }

    enum {
        PLAYER_ANIMATION_DELAY_TICKS = 16,
        PLAYER_AIM_STEP              = 48, // Angle units per update, 4096 units per turn.
    };
    _Actor120300Work*     work;
    _Actor120300Work*     dispatchWork;
    GameActor*            player;
    GfxCoord*             actorCoord;
    GfxCoord*             playerCoord;
    s32                   yawDistance;
    s16                   targetYaw;
    s16                   currentYaw;
    const ActorTransform* placement;
    Task*                 playerTask;

    work = task->work;
    // A pending script request may replace the follow-on clip started here.
    if (gGameSession->eventState != 0) {
        if ((work->playerTask != NULL) && (taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0)) {
            if (_gActor120300PlayerFollowOnAnimations[work->playerAnimation] >= 0) {
                _actor120300BlendPlayerAnimation(task, _gActor120300PlayerFollowOnAnimations[work->playerAnimation]);
            }
        }
    }
    switch (work->playerRequest) {
        case ACTOR_120300_PLAYER_REQUEST_NONE:
            break;
        case 1:
            _actor120300ResetPlayerAnimation(task, 0);
            break;
        case 2:
            placement    = &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_PLAYER_APPROACH_START];
            dispatchWork = task->work;
            TASK_MESSAGE_DISPATCH_POINTER(dispatchWork->playerTask, GAME_ACTOR_MESSAGE_PLACE, placement, 0);
            // Send the movement destination after placing the player at the approach start.
            dispatchWork = task->work;
            TASK_MESSAGE_DISPATCH_POINTER(dispatchWork->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, placement + (ACTOR_120300_PLACEMENT_PLAYER_APPROACH_END - ACTOR_120300_PLACEMENT_PLAYER_APPROACH_START), 0);
            break;
        case 3:
            dispatchWork = task->work;
            TASK_MESSAGE_DISPATCH_POINTER(dispatchWork->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_PLAYER_APPROACH_END], 0);
            _actor120300ResetPlayerAnimation(task, 1);
            break;
        case 4:
            _actor120300BlendPlayerAnimation(task, 2);
            break;
        case 5:
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            dispatchWork = task->work;
            TASK_MESSAGE_DISPATCH_POINTER(dispatchWork->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_PLAYER_CLIP3], 0);
            _actor120300BlendPlayerAnimation(task, 3);
            break;
        case 7:
            _actor120300BlendPlayerAnimation(task, 5);
            break;
        case 8:
            _actor120300BlendPlayerAnimation(task, 6);
            break;
        case 9:
            switch (work->playerRequestStep) {
                case 0:
                    work->playerRequestFrames = 0;
                    work->playerRequestStep++;
                    break;
                case 1:
                    if (++work->playerRequestFrames >= PLAYER_ANIMATION_DELAY_TICKS) {
                        _actor120300BlendPlayerAnimation(task, 7);
                        work->playerRequest = ACTOR_120300_PLAYER_REQUEST_NONE;
                    }
                    break;
            }
            return;
        case 10:
            _actor120300BlendPlayerAnimation(task, 8);
            taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, ANIMATION_RATE_ONE / 2, 0);
            break;
        case 11:
            dispatchWork = task->work;
            TASK_MESSAGE_DISPATCH_POINTER(dispatchWork->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_PLAYER_CLIP11], 0);
            _actor120300BlendPlayerAnimation(task, 0xB);
            taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, ANIMATION_RATE_ONE / 2, 0);
            break;
        case 12:
            _actor120300BlendPlayerAnimation(task, 9);
            taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, ANIMATION_RATE_ONE * 3 / 2, 0);
            break;
        case 13:
            dispatchWork = task->work;
            TASK_MESSAGE_DISPATCH_POINTER(dispatchWork->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_PLAYER_ROOM_PLAY], 0);
            ACTOR_120300_PLAY_PLAYER_WEAPON_ANIMATION(work->playerTask, ANIMATION_BLEND_RESET, 0);
            break;
        case 14:
            _actor120300BlendPlayerAnimation(task, 0xF);
            taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, ANIMATION_RATE_ONE / 2, 0);
            break;
        case 15:
            _actor120300BlendPlayerAnimation(task, 0xE);
            taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, ANIMATION_RATE_ONE / 2, 0);
            break;
        case 16:
            _actor120300BlendPlayerAnimation(task, 0xD);
            taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, ANIMATION_RATE_ONE / 2, 0);
            break;
        case 17:
            _actor120300BlendPlayerAnimation(task, 0xE);
            taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, ANIMATION_RATE_ONE / 2, 0);
            break;
        case 18:
            ACTOR_120300_PLAY_PLAYER_WEAPON_ANIMATION(work->playerTask, ANIMATION_BLEND_INTERPOLATE, ACTOR_120300_ANIMATION_BLEND_FRAMES);
            break;
        case 19:
            _actor120300BlendPlayerAnimation(task, 0x10);
            break;
        case ACTOR_120300_PLAYER_REQUEST_AIM_AT_ACTOR:
            playerTask  = work->playerTask;
            actorCoord  = task->extra.tmd->coords;
            playerCoord = playerTask->extra.tmd->coords;
            player      = playerTask->work;
            switch (work->playerRequestStep) {
                case 0:
                    work->playerAimYaw = player->aimYaw;
                    work->playerRequestStep++;
                    /* fallthrough */
                case 1:
                    targetYaw   = _actor120300GetPlayerAimYaw(actorCoord, playerCoord);
                    currentYaw  = work->playerAimYaw;
                    yawDistance = targetYaw - currentYaw;
                    if (yawDistance < 0) {
                        yawDistance = -yawDistance;
                    }
                    if (yawDistance < PLAYER_AIM_STEP + 1) {
                        work->playerRequestStep++;
                    } else if (currentYaw < targetYaw) {
                        work->playerAimYaw = currentYaw + PLAYER_AIM_STEP;
                    } else {
                        work->playerAimYaw = currentYaw - PLAYER_AIM_STEP;
                    }
                    /* fallthrough */
                case 2:
                    player->aimYaw = work->playerAimYaw;
                    return;
            }
            return;
        case ACTOR_120300_PLAYER_REQUEST_CENTER_AIM: {
            GameActor* centeringPlayer;

            centeringPlayer = work->playerTask->work;
            if (ABS(work->playerAimYaw) < PLAYER_AIM_STEP + 1) {
                work->playerAimYaw  = 0;
                work->playerRequest = ACTOR_120300_PLAYER_REQUEST_NONE;
            } else if (work->playerAimYaw < 0) {
                work->playerAimYaw += PLAYER_AIM_STEP;
            } else {
                work->playerAimYaw -= PLAYER_AIM_STEP;
            }
            centeringPlayer->aimYaw = work->playerAimYaw;
            return;
        }
    }
    work->playerRequest = ACTOR_120300_PLAYER_REQUEST_NONE;
}
#undef ACTOR_120300_PLAY_PLAYER_WEAPON_ANIMATION

/// Captures the current body poses and blends tracks 1..19 to a selected set's start.
///
/// work, animationId and blendFrames must have no side effects and are evaluated
/// on every iteration; sizeof inspects work unevaluated for the bound. Requires live slots,
/// loaded clip data and pose buffers through the transition. Retains playback
/// rates and slot 0; capture ticks consume the existing rate and retain flags.
/// blendFrames counts normal-rate frames (0 immediate, 0..2047 without signed-time
/// wrapping). Captures no caller identifiers and forms one statement with its
/// own u16 slotIndex; arguments must not refer to that local.
#define ACTOR_120300_BLEND_BODY_SLOTS(work, animationId, blendFrames)                                  \
    do {                                                                                               \
        u16 slotIndex;                                                                                 \
        for (slotIndex = 1; slotIndex < ARRAY_SIZE((work)->rig.slots); slotIndex++) {                  \
            animationSeekSlotWithBlend(&(work)->rig.anim, slotIndex, (animationId), 0, (blendFrames)); \
        }                                                                                              \
    } while (0)

/// Blends the body's animated parts into a selected clip over ten normal-rate frames.
///
/// `task` must have an initialized body rig with live model and animation data.
/// `animationId` selects a non-NULL body set (1 or 4 to 18). Drives slots 1 to 19,
/// preserves their playback rates, and records the clip for follow-on playback.
static inline void _actor120300BlendBodyAnimation(Task* task, u16 animationId)
{
    _Actor120300Work* work = task->work;

    work->bodyAnimation = animationId;
    ACTOR_120300_BLEND_BODY_SLOTS(work, animationId, ACTOR_120300_ANIMATION_BLEND_FRAMES);
}
#undef ACTOR_120300_BLEND_BODY_SLOTS

/// Restarts the body's animated parts on a selected clip at normal playback rate.
///
/// `task` must have an initialized body rig with live model and animation data.
/// `animationId` selects a non-NULL body set (1 or 4 to 18). Resets slots 1 to 19
/// without a transition pose and records the clip for follow-on playback.
static inline void _actor120300ResetBodyAnimation(Task* task, u16 animationId)
{
    _Actor120300Work* work = task->work;
    u16               slotIndex;

    work->bodyAnimation = animationId;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, slotIndex, animationId);
    }
}

/// Advances the body animation and performs its pending scene choreography.
///
/// Requires the initialized body rig and live model/animation data; requests
/// addressing the rifle also require that child task. Requests 0..19 are posted
/// by the event scripts. Zero is none; unknown IDs are cleared. Request 1 places
/// the body, then slides X left by 20 game units per tick until it is below 3901;
/// there is no position clamp. Other requests start clips, place models or set
/// directional-light intensity and clear immediately. Clip indices are 1 or
/// 4..18 in the package's body animation table; their visual actions are unproven.
static void _actor120300RunBodyRequest(Task* task)
{
    enum {
        ACTOR_120300_BODY_REQUEST_STEP_START   = 0,
        ACTOR_120300_BODY_REQUEST_STEP_SLIDE   = 1,
        ACTOR_120300_BODY_SLIDE_UNITS_PER_TICK = 20,
        ACTOR_120300_BODY_SLIDE_X_LIMIT        = 0xF3D,
    };
    TmdObject*            model;
    GfxCoord*             rootCoord;
    _Actor120300Work*     work;
    s32                   positionX;
    const ActorTransform* bodyPlacement;

    model     = task->extra.tmd;
    work      = task->work;
    rootCoord = model->coords;
    _actor120300TickBodyAnimation(task);
    switch (work->bodyRequest) {
        case ACTOR_120300_BODY_REQUEST_SLIDE_LEFT:
            switch (work->bodyRequestStep) {
                case ACTOR_120300_BODY_REQUEST_STEP_START:
                    TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_BODY_SLIDE_START], 0);
                    _actor120300ResetBodyAnimation(task, 1);
                    work->bodyRequestStep++;
                    return;
                case ACTOR_120300_BODY_REQUEST_STEP_SLIDE:
                    positionX               = rootCoord->coord.t[0];
                    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                    positionX              -= ACTOR_120300_BODY_SLIDE_UNITS_PER_TICK;
                    rootCoord->coord.t[0]   = positionX;
                    if (positionX < ACTOR_120300_BODY_SLIDE_X_LIMIT) {
                        _actor120300BlendBodyAnimation(task, 0xE);
                        work->bodyRequest = ACTOR_120300_BODY_REQUEST_NONE;
                    }
                    return;
            }
            return;
        case 2:
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_BODY_AFTER_SLIDE], 0);
            break;
        case 3:
            _actor120300BlendBodyAnimation(task, 4);
            break;
        case 4:
            _actor120300BlendBodyAnimation(task, 0x12);
            break;
        case 5:
            _actor120300BlendBodyAnimation(task, 6);
            break;
        case 6:
            _actor120300BlendBodyAnimation(task, 7);
            break;
        case 7:
            _actor120300BlendBodyAnimation(task, 0xD);
            break;
        case 8:
            bodyPlacement = &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_BODY_ROOM_PLAY];
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, bodyPlacement, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->rifleTask, ACTOR_MESSAGE_PLACE, bodyPlacement + (ACTOR_120300_PLACEMENT_RIFLE_ROOM_PLAY - ACTOR_120300_PLACEMENT_BODY_ROOM_PLAY), 0);
            _actor120300ResetBodyAnimation(task, 8);
            break;
        case 9:
            _actor120300BlendBodyAnimation(task, 0xB);
            break;
        case 10:
            _actor120300BlendBodyAnimation(task, 9);
            break;
        case 11:
            _actor120300BlendBodyAnimation(task, 0xA);
            break;
        case 12:
            _actor120300BlendBodyAnimation(task, 0xC);
            break;
        case 13:
            work->scale = ONE / 4;
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_BODY_DIMMED], 0);
            _actor120300ResetBodyAnimation(task, 0xE);
            break;
        case ACTOR_120300_BODY_REQUEST_FULL_LIGHT:
            work->scale = ONE;
            break;
        case 15:
            _actor120300BlendBodyAnimation(task, 0xF);
            break;
        case 16:
            _actor120300BlendBodyAnimation(task, 0x10);
            break;
        case 17:
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_BODY_CLIP17], 0);
            _actor120300ResetBodyAnimation(task, 0x11);
            break;
        case 18:
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_BODY_ROOM_PLAY], 0);
            _actor120300ResetBodyAnimation(task, 8);
            break;
        case 19:
            _actor120300BlendBodyAnimation(task, 5);
            break;
        case ACTOR_120300_BODY_REQUEST_NONE:
        default:
            break;
    }
    work->bodyRequest = ACTOR_120300_BODY_REQUEST_NONE;
}

/// Prepares Gary Douglas and his rifle for ordinary garage play.
///
/// Requires the published body task, initialized rig and live head/rifle tasks.
/// Places the body and rifle at their room poses, shows all models and restarts
/// body clip 8 at normal rate. Zero preservePlayer also requires a live player:
/// resets its equipped-weapon clip with world collision enabled, restores player
/// placement and the actor models' unit scale, and cancels the pending scene load.
/// Nonzero preservePlayer keeps those values. Clears both choreography request
/// channels; synchronous messages borrow stack and overlay records.
static void _actor120300PrepareRoomPlay(s32 preservePlayer)
{
    enum {
        ACTOR_120300_ROOM_PLAY_BODY_ANIMATION   = 8,
        ACTOR_120300_PRIMARY_CHARACTER          = 1,
        ACTOR_120300_PRIMARY_WEAPON_BANK_BASE   = 1,
        ACTOR_120300_ALTERNATE_WEAPON_BANK_BASE = 0x22,
        ACTOR_120300_WEAPON_ANIMATION           = 1
    };
    Task*                task;
    _Actor120300Work*    work;
    SVECTOR              unused; // Retained stack slot; original role unproven.
    AnimationPlayRequest playerAnimationRequest;
    s32                  weaponId;
    s32                  weaponBank;

    task                   = _gActor120300BodyTask;
    work                   = task->work;
    task->extra.tmd->flags = 0;
    TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_BODY_ROOM_PLAY], 0);

    _actor120300ResetBodyAnimation(task, ACTOR_120300_ROOM_PLAY_BODY_ANIMATION);

    taskMessageDispatch(work->headTask, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
    taskMessageDispatch(work->rifleTask, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->rifleTask, ACTOR_MESSAGE_PLACE, &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_RIFLE_ROOM_PLAY], 0);
    if (preservePlayer == 0) {
        weaponId                                    = gPlayerStatus.weapon;
        weaponBank                                  = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_120300_PRIMARY_CHARACTER) ? weaponId + ACTOR_120300_PRIMARY_WEAPON_BANK_BASE : weaponId + ACTOR_120300_ALTERNATE_WEAPON_BANK_BASE;
        playerAnimationRequest.source.index         = weaponBank;
        playerAnimationRequest.animationId          = ACTOR_120300_WEAPON_ANIMATION;
        playerAnimationRequest.blend                = ANIMATION_BLEND_RESET;
        playerAnimationRequest.blendFrames          = 0;
        playerAnimationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_PLAY, &playerAnimationRequest, 0);
        TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_PLAYER_ROOM_PLAY], 0);
        work->scale = ONE;
        cdCmdCancelScene();
    }
    work->playerRequest = ACTOR_120300_PLAYER_REQUEST_NONE;
    work->bodyRequest   = ACTOR_120300_BODY_REQUEST_NONE;
}

/// Starts the selected garage interaction and reports when its event script ends.
///
/// Requires live body work. Interaction step 0 starts a HUD-hiding script and
/// advances to step 1; that step returns 1 when the session event state is idle,
/// otherwise 0. Talks select the first, second, then recurring third exchange;
/// remarks use a separate script. The event interpreter owns script playback.
static s16 _actor120300RunInteraction(Task* task)
{
    enum { ACTOR_120300_INTERACTION_STEP_START = 0,
           ACTOR_120300_INTERACTION_STEP_WAIT  = 1 };

    _Actor120300Work* work;

    work = task->work;
    switch (work->interaction) {
        case ACTOR_120300_INTERACTION_TALK:
            switch (work->interactionStep) {
                case ACTOR_120300_INTERACTION_STEP_START:
                    switch (work->talkStage) {
                        case 0:
                            evsStartScript(_gActor120300FirstTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                            work->talkStage++;
                            break;
                        case 1:
                            evsStartScript(_gActor120300SecondTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                            work->talkStage++;
                            break;
                        default:
                            evsStartScript(_gActor120300RepeatTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                            break;
                    }
                    work->interactionStep++;
                    break;
                case ACTOR_120300_INTERACTION_STEP_WAIT:
                    if (gGameSession->eventState == 0) {
                        return 1;
                    }
                    break;
            }
            break;
        case ACTOR_120300_INTERACTION_REMARK:
            switch (work->interactionStep) {
                case ACTOR_120300_INTERACTION_STEP_START:
                    evsStartScript(_gActor120300RemarkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                    work->interactionStep++;
                    break;
                case ACTOR_120300_INTERACTION_STEP_WAIT:
                    if (gGameSession->eventState == 0) {
                        return 1;
                    }
                    break;
            }
            break;
    }
    return 0;
}

/// Allocates Gary Douglas's body rig and creates its head and rifle tasks.
///
/// Requires a live body model and player task. Publishes this task for script
/// callbacks, owns its zeroed work and primitive buffer, and parents the root
/// to the view coordinate. Starts body clip 14 at normal rate on slots 1..19.
/// Texture offsets use area resource entry 0x6A or the terminal record if absent.
/// Child tasks borrow this body and join its teardown tree. Work-allocation
/// failure kills the body task; this void initializer does not report failure.
static void _actor120300InitBody(Task* task)
{
    enum { ACTOR_120300_BODY_INITIAL_ANIMATION = 14 };

    _Actor120300Work* work;
    _Actor120300Work* allocatedWork;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    AreaPlacement*    placement;

    model         = task->extra.tmd;
    rootCoord     = model->coords;
    allocatedWork = memMalloc(sizeof(*allocatedWork), false);
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        taskKill(task);
        return;
    }
    // The model borrows matrices from work; child tasks share the body lifetime.
    work = allocatedWork;
    memFillBytes(work, 0, sizeof(*work));
    work->playerTask      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    _gActor120300BodyTask = task;
    rootCoord->parent     = &gGfxViewCoord;
    tmdAllocPrimitiveBuffer(model);
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    model->flags   &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
    placement       = _actor120300FindTexturePlacement();
    tmdSetTextureOffsets(model, placement->texturePageOffset, placement->clutRowOffset);
    animationInitContext(&work->rig.anim, _gActor120300BodyAnimationSets, model, work->rig.poses, work->rig.slots);
    _actor120300ResetBodyAnimation(task, ACTOR_120300_BODY_INITIAL_ANIMATION);
    work->headTask  = taskSpawnFromTable(gActor120300TaskDescriptors, ACTOR_120300_TASK_HEAD, 0, task);
    work->rifleTask = taskSpawnFromTable(gActor120300TaskDescriptors, ACTOR_120300_TASK_RIFLE, 0, task);
    task->msgTable  = _gActor120300ModelMessages;
    work->scale     = ONE;
    taskReparent(task, work->headTask);
    taskReparent(task, work->rifleTask);
}

/// Accepts a manual room-action trigger and starts the selected interaction.
///
/// Requires live body work. Returns 1 for a pressed room action and 0 otherwise;
/// selector bytes 1/2 choose talk/remark, while other selectors retain the prior
/// choice. Trigger reading enables the collision system's next-tick hit clearing.
static inline s16 _actor120300TryStartInteraction(Task* task)
{
    enum { ACTOR_120300_TRIGGER_TALK   = 1,
           ACTOR_120300_TRIGGER_REMARK = 2 };
    _Actor120300Work* interactionWork;
    s16               interactionPressed;
    s16               actionHit;
    u16               triggerControl;
    s16               signedTriggerControl;
    u8                interactionKind;
    u8                unusedTriggerParameter;

    interactionPressed = 0;
    interactionWork    = task->work;
    actionHit          = worldCollisionReadActionHit(&triggerControl, &interactionKind, &unusedTriggerParameter);
    if (actionHit != 0) {
        signedTriggerControl = triggerControl;
        if (!(signedTriggerControl & WORLD_COLLISION_TRIGGER_AUTOMATIC)) {
            if ((triggerControl & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM) {
                interactionPressed = gPlayerStatus.interactionPressed != 0;
            }
        }
    }
    if (interactionPressed != 0) {
        if ((s8)interactionKind == ACTOR_120300_TRIGGER_TALK) {
            interactionWork->interaction = ACTOR_120300_INTERACTION_TALK;
        }
        if ((s8)interactionKind == ACTOR_120300_TRIGGER_REMARK) {
            interactionWork->interaction = ACTOR_120300_INTERACTION_REMARK;
        }
        interactionWork->interactionStep = 0;
        return 1;
    } else {
        return 0;
    }
}

/// Runs Gary Douglas's garage scene, room interactions and body presentation.
///
/// Starts at state 0 with a live body model. Initialization waits for the
/// attachment wheel and pending display transition to clear, then starts the
/// first scene or prepares ordinary room play according to saved progress.
/// The task owns body work and child tasks; scripts borrow its published handle.
/// Room play accepts manual room-action triggers for talk or remark exchanges.
/// Each active update advances both choreography channels, draws the ground
/// shadow and applies unsigned Q12 directional-light intensity without changing
/// model geometry or ambient light. The garage overlay and animation resources
/// must remain live; initialization requires successful work/child allocation.
static void _actor120300BodyTask(Task* task)
{
    enum {
        ACTOR_120300_BODY_STATE_INITIALIZE        = 0,
        ACTOR_120300_BODY_STATE_WAIT_SCENE        = 1,
        ACTOR_120300_BODY_STATE_ROOM_PLAY         = 2,
        ACTOR_120300_BODY_STATE_INTERACTION       = 3,
        ACTOR_120300_BODY_STATE_PLACE_RIFLE       = 4,
        ACTOR_120300_PRIMARY_CHARACTER            = 1,
        ACTOR_120300_PRIMARY_WEAPON_BANK_BASE     = 1,
        ACTOR_120300_ALTERNATE_WEAPON_BANK_BASE   = 0x22,
        ACTOR_120300_PLAYER_WEAPON_IDLE_ANIMATION = 1,
        ACTOR_120300_SHADOW_OFFSET_Y              = 0x380,
        ACTOR_120300_SHADOW_RADIUS                = 0x300,
    };
    union {
        struct {
            SVECTOR shadowOffset;
            VECTOR  lightSample;
        } draw;
        AnimationPlayRequest playerAnimationRequest;
    } scratch;
    _Actor120300Work* work;
    TmdObject*        model;
    s32               state;
    s32               weaponBank;
    u16               lightingScale;

    state = task->state;
    work  = task->work;
    switch (state) {
        case ACTOR_120300_BODY_STATE_INITIALIZE:
            if ((gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                _actor120300InitBody(task);
                work = task->work;
                if (gameFlagGetNibble(GAME_FLAG_GARAGE_GARY_SCENE_SEEN) != 0) {
                    _actor120300PrepareRoomPlay(1);
                    if (gameFlagGetNibble(GAME_FLAG_MOTEL_ROOM_6_DOOR_UNLOCKED) != 0) {
                        work->talkStage = 1;
                    }
                    task->state = ACTOR_120300_BODY_STATE_PLACE_RIFLE;
                } else {
                    weaponBank = gPlayerStatus.weapon;
                    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_120300_PRIMARY_CHARACTER) {
                        weaponBank = weaponBank + ACTOR_120300_PRIMARY_WEAPON_BANK_BASE;
                    } else {
                        weaponBank = weaponBank + ACTOR_120300_ALTERNATE_WEAPON_BANK_BASE;
                    }
                    scratch.playerAnimationRequest.source.index         = weaponBank;
                    scratch.playerAnimationRequest.animationId          = ACTOR_120300_PLAYER_WEAPON_IDLE_ANIMATION;
                    scratch.playerAnimationRequest.blend                = ANIMATION_BLEND_RESET;
                    scratch.playerAnimationRequest.blendFrames          = 0;
                    scratch.playerAnimationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &scratch.playerAnimationRequest, 0);
                    gameFlagSetNibble(GAME_FLAG_02C, 1);
                    gameFlagSetNibble(GAME_FLAG_GARAGE_GARY_SCENE_SEEN, 1);
                    gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0xB);
                    evsStartScriptWithSkip(_gActor120300OpeningScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor120300OpeningSkipScript);
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 2;
                    task->state                                        += 1;
                }
                // Patch the live garage grid from this actor package's collision records.
                memCopyBytes(_gActor120300GarageCollisionPatchNormals, gDryfieldGarageCollisionGridNormals, sizeof(_gActor120300GarageCollisionPatchNormals));
                memCopyBytes(_gActor120300GarageCollisionPatchFaces, gDryfieldGarageCollisionGridFaces, sizeof(_gActor120300GarageCollisionPatchFaces));
                memCopyBytes(_gActor120300GarageCollisionPatchVerts, gDryfieldGarageCollisionGridVerts, sizeof(_gActor120300GarageCollisionPatchVerts));
                break;
            }
            return;
        case ACTOR_120300_BODY_STATE_WAIT_SCENE:
            if (gGameSession->eventState == 0) {
                areaApplySavedUpdates(gDryfieldGarageGarySceneAreaUpdates);
                task->state += 1;
            }
            break;
        case ACTOR_120300_BODY_STATE_ROOM_PLAY:
            if (_actor120300TryStartInteraction(task) != 0) {
                task->state += 1;
            }
            break;
        case ACTOR_120300_BODY_STATE_INTERACTION:
            if (_actor120300RunInteraction(task) != 0) {
                task->state -= 1;
            }
            break;
        case ACTOR_120300_BODY_STATE_PLACE_RIFLE:
            TASK_MESSAGE_DISPATCH_POINTER(work->rifleTask, ACTOR_MESSAGE_PLACE, &_gActor120300ScenePlacements[ACTOR_120300_PLACEMENT_RIFLE_ROOM_PLAY], 0);
            task->state = ACTOR_120300_BODY_STATE_ROOM_PLAY;
            break;
    }

    // Choreography and presentation continue while an event script owns control.
    _actor120300RunPlayerRequest(task);
    _actor120300RunBodyRequest(task);
    scratch.draw.shadowOffset.vx = 0;
    scratch.draw.shadowOffset.vy = ACTOR_120300_SHADOW_OFFSET_Y;
    scratch.draw.shadowOffset.vz = 0;
    actorRenderDrawGroundShadow(&task->extra.tmd->coords[1], ACTOR_120300_SHADOW_RADIUS, &scratch.draw.shadowOffset);
    model         = task->extra.tmd;
    lightingScale = work->scale;
    _actor120300UpdateBodyLighting(task, model, &scratch.draw.lightSample);
    scratch.draw.lightSample.vz = lightingScale;
    scratch.draw.lightSample.vy = lightingScale;
    scratch.draw.lightSample.vx = lightingScale;
    ScaleMatrix(model->colorMtx, &scratch.draw.lightSample);
}

#include "../../shared/screen/screen_fade_out.inc.c"

/// Enables or suppresses active drawing of the receiving body, head or rifle model.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW`: zero hides, any nonzero value shows.
/// The model must be live; buffer ownership and other draw flags are unchanged.
/// The message produces no defined result, so callers must discard it.
static void _actor120300HandleModelDrawMessage(Task* task, s32 unusedMessageId, s32 visible, s32 unusedSecondArg)
{
    TmdObject* model;

    model = task->extra.tmd;
    if (visible != 0) {
        model->flags = model->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    model->flags = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
}

#include "../../shared/actor/actor_messages_place_in_view.inc.c"

/// Sets active drawing of the body, head and rifle together for an event script.
///
/// Accepts 0 to hide or 1 to show; other values do nothing. Requires the
/// published body task and both initialized child tasks to remain live.
static void _actor120300SetModelsVisible(s32 visible)
{
    _Actor120300Work* work = _gActor120300BodyTask->work;

    if (visible == 0) {
        taskMessageDispatch(_gActor120300BodyTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
        taskMessageDispatch(work->headTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
        taskMessageDispatch(work->rifleTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
    } else if (visible == 1) {
        taskMessageDispatch(_gActor120300BodyTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
        taskMessageDispatch(work->headTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
        taskMessageDispatch(work->rifleTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
    }
}

/// Stages the selected scene's audio start and requests a deferred view refresh.
///
/// Called after the script selects its scene stream. The selected descriptor
/// and prepared playback buffers must remain live until the CD request runs.
static void _actor120300PrepareScenePlayback(void)
{
    cdCmdStageSceneAudioStart();
    gGameSession->viewDirty = 1;
}

/// Queues playback of the selected garage scene's prepared stream.
///
/// The event script waits three updates after preparing audio before this call.
/// The selected scene and its playback buffers must remain live through playback.
static void _actor120300StartScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Finishes the garage scene's stream session and restores its saved random state.
///
/// Requires prior scene selection. The event interpreter owns stream teardown;
/// this callback marks completion without releasing buffers or cancelling CD work.
static void _actor120300FinishSceneStream(void)
{
    streamFinishScene();
}

/// Replaces the pending player choreography request and restarts its first stage.
///
/// The event script supplies the low signed halfword of its argument, stored as
/// u16 without validation. Scripts use 0 to 21 (0 clears); request 6 is unhandled
/// and cleared by the tick. Requires the published body task and its work live.
static void _actor120300PostPlayerRequest(s16 requestId)
{
    _Actor120300Work* work = _gActor120300BodyTask->work;

    work->playerRequest     = requestId;
    work->playerRequestStep = 0;
}

/// Replaces the pending body choreography request and restarts its first stage.
///
/// The event script supplies the low signed halfword of its argument, stored as
/// u16 without validation. Scripts use 0 to 19 (0 clears). Requires the published
/// body task and its work live; child tasks must survive requests addressing them.
static void _actor120300PostBodyRequest(s16 requestId)
{
    _Actor120300Work* work = _gActor120300BodyTask->work;

    work->bodyRequest     = requestId;
    work->bodyRequestStep = 0;
}

/// Removes the player's equipment once while the garage scene owns the player.
///
/// Requires the initialized body and player tasks. Marks equipment suppressed
/// before removal; subsequent calls do nothing until the scene restores it.
static void _actor120300RemovePlayerEquipment(void)
{
    _Actor120300Work* work = _gActor120300BodyTask->work;

    if (work->playerEquipmentRemoved == 0) {
        work->playerEquipmentRemoved = 1;
        playerActorRemoveEquipment();
    }
}

/// Restores scene-suppressed equipment while keeping the player under scripted control.
///
/// Requires the published body work and live player/resources. Does nothing if
/// equipment was not removed. Otherwise rebuilds equipment, clears the latch,
/// and holds the player in its equipped-bank idle animation with collision off.
/// The event script releases scripted control later.
static void _actor120300RestorePlayerEquipment(void)
{
    _Actor120300Work* work = _gActor120300BodyTask->work;

    if (work->playerEquipmentRemoved != 0) {
        playerActorRestoreEquipment();
        work->playerEquipmentRemoved = 0;
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    }
}

/// Starts the garage scene's subtractive fade to black.
///
/// Spawns the actor table's fade task with nine intensity units per task tick
/// and no parent argument. The spawned task owns its ramp storage and teardown;
/// this callback does not wait for completion or retain the returned task.
static void _actor120300StartSceneFadeOut(void)
{
    enum { ACTOR_120300_SCENE_FADE_INTENSITY_PER_TICK = 9 };

    taskSpawnFromTable(gActor120300TaskDescriptors, ACTOR_120300_TASK_FADE_OUT, ACTOR_120300_SCENE_FADE_INTENSITY_PER_TICK, 0);
}

/// Runs the garage dialogue that refuses departure through the junk-yard door.
///
/// Starts at state 0. The event interpreter hides/restores the HUD while running
/// the borrowed package script. State 1 kills this control task once the session
/// event state is idle. The actor package must remain loaded through playback.
static void _actor120300JunkYardRefusalTask(Task* task)
{
    enum {
        ACTOR_120300_REFUSAL_STATE_START = 0,
        ACTOR_120300_REFUSAL_STATE_WAIT  = 1,
    };

    switch (task->state) {
        case ACTOR_120300_REFUSAL_STATE_START:
            evsStartScript(_gActor120300JunkYardRefusalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            task->state += 1;
            break;
        case ACTOR_120300_REFUSAL_STATE_WAIT:
            if (gGameSession->eventState == 0) {
                taskKill(task);
            }
            break;
    }
}
