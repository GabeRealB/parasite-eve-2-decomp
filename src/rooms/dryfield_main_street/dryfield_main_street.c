#include "rooms/dryfield_main_street.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_00100.h"
#include "actors/actor_02000.h"
#include "actors/actor_02300.h"
#include "actors/actor_02500.h"
#include "actors/actor_323000.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
// The latched-event symbol carries four unproven bytes after the event.
#define ROOM_EVENT_LATCHED gRoomEventLatched.event
/// Selects the gate's TU-local room-transition task descriptor.
///
/// Addressable `TaskDesc` lvalue; see `room_events.h` for the spawn contract.
/// Bind before that header and keep through `room_event.inc.c`.
#define ROOM_EVENT_TASK_DESC (_gRoomEventTaskDesc)
#include "../../shared/room/room_events.h"
/// Binds the shared puff task to this package's public void (Task*) effect callback.
/// Must precede main_street.h and remain defined through main_street_puff_task.inc.c.
#define MAIN_STREET_PUFF_TASK dryfieldMainStreetPuffTask
#include "../../shared/main_street.h"

static s32 _mainStreetResolveMessage(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _mainStreetTalkMessage(Task* task, s32 messageId, s32 command, s32 unusedSecondArg);

static void _mainStreetFinishIceBagEvent(Task* task);

static s32 _mainStreetCapSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

/// Advances the shared random sequence once and yields an unsigned draw in 0..65535.
///
/// Captures `gRandomLcgState`; multiplication and addition wrap modulo 2^32.
/// Each invocation consumes one draw, so callers must preserve draw order.
#define DRYFIELD_MAIN_STREET_RAND() ((RANDOM_LCG_ADVANCE()) >> 16)

extern RoomEventActiveBytes gRoomEventActive;

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern RoomFadeStorage gRoomEventFade;

/// The message and event the message handler latched for the room's event
/// task.
static RoomEventMsg            _gRoomEventStagedMsg;
extern RoomLatchedEventStorage gRoomEventLatched;

static RoomEventStartStorage _gMainStreetStagedSceneStartStorage;

/// The request the event gate latched for its event task.
extern RoomEventReq gRoomEventReq;

static Task* _gDryfieldMainStreetPlayerHeadAimTask;

static void _dryfieldMainStreetInitialize(Task* task);
static void _dryfieldMainStreetIdle(Task* task);
static void _dryfieldMainStreetForgetPlayerHeadAimTask(s32 unusedArg);

static WorldCollisionGrid    _gDryfieldMainStreetCollisionGrid;
static WorldCollisionTrigger _gDryfieldMainStreetViewBoundaryTriggers[26];
static WorldCollisionTrigger _gDryfieldMainStreetActionTriggers[10];
static WorldCoordRoomLights  _gDryfieldMainStreetLights;

static void _dryfieldMainStreetBeginFirstVisitBattle(s32 unusedBattleArg);
static void _dryfieldMainStreetSpawnPlayerHeadAimTask(void);
static void _dryfieldMainStreetSetPlayerHeadAim(s32 command);

static s32  _dryfieldMainStreetRejectKeyItemUse(Task* receiver, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _dryfieldMainStreetStartFirstVisitCutscene(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _dryfieldMainStreetTurnPlayerTowardAreaActorTask(Task* task);
static void _dryfieldMainStreetPlayerHeadAimTask(Task* task);

/// Script controls for the player head-aim task; other values also stop it.
enum {
    DRYFIELD_MAIN_STREET_HEAD_AIM_DISABLE = 0,
    DRYFIELD_MAIN_STREET_HEAD_AIM_ENABLE  = 1,
    DRYFIELD_MAIN_STREET_HEAD_AIM_STOP    = -1,
};

/// Bodyless staged motel-scene recipe, using the room's latched event and destination.
///
/// Priority `TASK_PRIORITY_SCENE`; spawn payloads and metadata are unused. Spawn reads the recipe
/// synchronously, and the callback and singleton snapshots stay live through reload.
static TaskDesc _gMainStreetStagedSceneTaskDescriptor = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventStagedTask, { .value = 0 } };

/// Bodyless transition task spawned by this room's event gate at priority 32.
///
/// Uses the latched request and destination to play CAP and optional sounds,
/// then reload the session. Spawn payloads and metadata are unused. The gate
/// reads the recipe synchronously; room snapshots and callback code must remain
/// live through the deferred transition.
static TaskDesc _gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

/// Bodyless Ice Bag handover-completion recipe at priority 32.
///
/// Waits for CAP to become idle, then updates the saved timer and water-bag
/// possession. Spawn payloads and metadata are unused; keep callback code loaded.
static TaskDesc _gMainStreetIceBagCompletionTaskDescriptor = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _mainStreetFinishIceBagEvent, { .value = 0 } };

/// Main Street's room-message interface, borrowed until receiver teardown.
///
/// Resolves departures, refuses key items, starts the first-visit action, handles
/// talk commands and routes CAP sound cues. The sixth record ends unsupported-ID lookup.
static const TaskMessageEntry _gDryfieldMainStreetRoomMessages[6] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _mainStreetResolveMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldMainStreetRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldMainStreetStartFirstVisitCutscene },
    { ROOM_MESSAGE_COMMAND, _mainStreetTalkMessage },
    { ROOM_MESSAGE_SOUND, _mainStreetCapSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

enum { DRYFIELD_MAIN_STREET_INITIAL_CHASER_HIDE = 0 };

/// Hides the cutscene chaser while the first-visit scene is still unseen.
///
/// The room initializer broadcasts this complete record synchronously in the
/// Dryfield/Main Street command namespace. Preserve its authored word alignment.
static ActorCommand _gDryfieldMainStreetInitialChaserHideCommand __attribute__((aligned(4))) = {
    .context = { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET } },
    .command = DRYFIELD_MAIN_STREET_INITIAL_CHASER_HIDE,
};

static AnimationPackedPose _gDryfieldMainStreetAnimation03BF0Bank1[6] = {
#include "assets/dryfield_main_street_animation_03BF0_bank1.inc"
};

static AnimationPackedRotation _gDryfieldMainStreetAnimation03BF0Bank4[46] = {
#include "assets/dryfield_main_street_animation_03BF0_bank4.inc"
};

static AnimationRecord _gDryfieldMainStreetAnimation03BF0Records[109] = {
#include "assets/dryfield_main_street_animation_03BF0_records.inc"
};

static u16 _gDryfieldMainStreetAnimation03BF0Indices[20] = {
#include "assets/dryfield_main_street_animation_03BF0_indices.inc"
};

static AnimationSet _gDryfieldMainStreetAnimation03BF0 = {
    _gDryfieldMainStreetAnimation03BF0Records,
    _gDryfieldMainStreetAnimation03BF0Indices,
    { NULL, _gDryfieldMainStreetAnimation03BF0Bank1, NULL, NULL, _gDryfieldMainStreetAnimation03BF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldMainStreetAnimation03F84Bank1[7] = {
#include "assets/dryfield_main_street_animation_03F84_bank1.inc"
};

static AnimationPackedRotation _gDryfieldMainStreetAnimation03F84Bank4[65] = {
#include "assets/dryfield_main_street_animation_03F84_bank4.inc"
};

static AnimationRecord _gDryfieldMainStreetAnimation03F84Records[123] = {
#include "assets/dryfield_main_street_animation_03F84_records.inc"
};

static u16 _gDryfieldMainStreetAnimation03F84Indices[20] = {
#include "assets/dryfield_main_street_animation_03F84_indices.inc"
};

static AnimationSet _gDryfieldMainStreetAnimation03F84 = {
    _gDryfieldMainStreetAnimation03F84Records,
    _gDryfieldMainStreetAnimation03F84Indices,
    { NULL, _gDryfieldMainStreetAnimation03F84Bank1, NULL, NULL, _gDryfieldMainStreetAnimation03F84Bank4, NULL, NULL, NULL },
};

/// Bodyless first-visit scene helpers: slot 0 turns the player, slot 1 aims their head.
///
/// Both run at priority `TASK_PRIORITY_CONTROL` and borrow the live player and placement-zero actor.
/// Only slot 1 is spawned here; the descriptor is read synchronously at spawn.
static TaskDesc _gDryfieldMainStreetCutsceneTaskDescriptors[2] = {
    { .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
      .callback = _dryfieldMainStreetTurnPlayerTowardAreaActorTask,
      .data     = { .value = 0 } },
    { .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
      .callback = _dryfieldMainStreetPlayerHeadAimTask,
      .data     = { .value = 0 } },
};

/// First-visit player clips installed at bank indices 47..49, with NULL at 47.
///
/// The four-word copy also reads the following request's source-pointer word,
/// one word past this array, into unused bank index 50. Preserve that authored
/// overread and adjacency. Playback borrows these sets while the room is loaded;
/// the scene plays 48 followed by 49 after twenty ticks, and never plays 47 or 50.
static AnimationSet* _gDryfieldMainStreetPlayerAnimationBankExtension[3] = { NULL, &_gDryfieldMainStreetAnimation03BF0, &_gDryfieldMainStreetAnimation03F84 };

/// Authored transfer length, including the word after the three set pointers.
enum { DRYFIELD_MAIN_STREET_ANIMATION_COPY_WORD_COUNT = 4 };

/// Installs the first-visit clips in the equipped player's writable bank extension.
///
/// Dispatch consumes the request and four source words synchronously. Its source
/// pointer is also the fourth transferred word; keep it immediately after the
/// set array. The copied clips require this room overlay through playback.
static AnimationBankCopyRequest _gDryfieldMainStreetPlayerAnimationBankCopy = {
    .sets  = _gDryfieldMainStreetPlayerAnimationBankExtension,
    .count = DRYFIELD_MAIN_STREET_ANIMATION_COPY_WORD_COUNT,
};

/// Retained five-word record matching the neighboring animation-play requests.
///
/// Encodes bank index 1, clip 1, reset blending and disabled grid participation.
/// No recovered consumer establishes its intended actor or playback purpose.
static AnimationPlayRequest D_dryfield_main_street_80181598 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Opening player clip 48, blended over ten frames with grid participation disabled.
///
/// The event script resolves the equipped bank in a copy of this request.
/// Requires the room's installed animation extension through playback.
static AnimationPlayRequest _gDryfieldMainStreetPlayerClip48Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Second player clip 49, blended over twenty frames with grid participation disabled.
///
/// The event script resolves the equipped bank in a copy of this request.
/// Requires the room's installed animation extension through playback.
static AnimationPlayRequest _gDryfieldMainStreetPlayerClip49Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 2,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum { DRYFIELD_MAIN_STREET_PLAYER_SCENE_BASE_CLIP = 9 };

/// Base-bank player clip 9 shared by the first-visit scene and its skip continuation.
///
/// Blends over ten frames and disables grid participation. The event script
/// resolves the equipped bank in a copy; the base clip does not use room sets.
static AnimationPlayRequest _gDryfieldMainStreetPlayerClip9Request = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_MAIN_STREET_PLAYER_SCENE_BASE_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player placement shared by the first-visit scene and its skip continuation.
///
/// XYZ uses room coordinates and all Euler angles are zero (4096 per turn).
/// The player consumes XYZ and all three angles synchronously during dispatch.
static ActorTransform _gDryfieldMainStreetFirstVisitPlayerPlacement = {
    .pos = { -2237, 0, -2100, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Retained 24-byte record with a candidate position (-3000, 0, 6000) and zero angles.
///
/// Its layout agrees with the adjacent live placement record, but no recovered
/// consumer establishes that it is an actor transform or which actor would use it.
static ActorTransform D_dryfield_main_street_80181600 = { { -3000, 0, 6000, 0 }, { 0, 0, 0, 0 } };

/// Cutscene-chaser selectors in the Dryfield/Main Street command namespace.
enum {
    DRYFIELD_MAIN_STREET_CHASER_PLAY_CLIP13 = 1,
    DRYFIELD_MAIN_STREET_CHASER_HIDE        = 2,
    DRYFIELD_MAIN_STREET_CHASER_PLAY_CLIP14 = 3,
};

/// Starts the cutscene chaser's clip 13 when the player has been placed.
///
/// Borrowed by the scene's synchronous broadcast; other actors receive it too.
static ActorCommand _gDryfieldMainStreetChaserPlayClip13Command = {
    .context = { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET } },
    .command = DRYFIELD_MAIN_STREET_CHASER_PLAY_CLIP13,
};

/// Hides the cutscene chaser at normal or skipped first-visit scene completion.
///
/// Borrowed by the scene's synchronous broadcast in the Main Street namespace.
static ActorCommand _gDryfieldMainStreetChaserHideCommand = {
    .context = { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET } },
    .command = DRYFIELD_MAIN_STREET_CHASER_HIDE,
};

/// Starts the cutscene chaser's clip 14 and its timed dust and sound cues.
///
/// Borrowed by the scene's synchronous broadcast in the Main Street namespace.
static ActorCommand _gDryfieldMainStreetChaserPlayClip14Command = {
    .context = { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET } },
    .command = DRYFIELD_MAIN_STREET_CHASER_PLAY_CLIP14,
};

/// First-visit chaser scene, ending with player control in battle view 2.
///
/// Forty-two instructions including END. Borrows room animation requests,
/// placement and actor-command records until their instructions execute; the
/// installed clips and callback code remain live through the scene. Paired with
/// the skip script by the room-action handler, with HUD hide/restore enabled.
static EvsCommand _gDryfieldMainStreetFirstVisitScript[42] = {
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 15) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldMainStreetPlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldMainStreetPlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldMainStreetPlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_SET_RATE }, { .value = 3 * ANIMATION_RATE_ONE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldMainStreetFirstVisitPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldMainStreetChaserPlayClip13Command } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldMainStreetSpawnPlayerHeadAimTask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldMainStreetSetPlayerHeadAim }, { .value = DRYFIELD_MAIN_STREET_HEAD_AIM_ENABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldMainStreetPlayerClip9Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldMainStreetChaserPlayClip14Command } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldMainStreetPlayerClip9Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 14) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 48 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldMainStreetBeginFirstVisitBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldMainStreetSetPlayerHeadAim }, { .value = DRYFIELD_MAIN_STREET_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldMainStreetChaserHideCommand } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// First-visit skip continuation restoring placement and starting the same battle.
///
/// Sixteen instructions including END. Releases framebuffer blending, fades out,
/// places the player, hides the chaser and stops head aiming before restoring view
/// 2 and control. Borrows room payloads and callbacks through interpreter completion.
static EvsCommand _gDryfieldMainStreetFirstVisitSkipScript[16] = {
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldMainStreetFirstVisitPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldMainStreetPlayerClip9Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldMainStreetChaserHideCommand } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldMainStreetSetPlayerHeadAim }, { .value = DRYFIELD_MAIN_STREET_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldMainStreetBeginFirstVisitBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Ambient-effect gates for mapped views 1..8; views 2..4 enable the effect.
///
/// Indexed by mapped view minus one. The controller also admits the room's
/// views 9..13, whose reads exceed this eight-halfword object; their intended
/// access model is unproven. Preserve the separate following mutable vector.
static const u16 _gDryfieldMainStreetViewEffectModes[8] SECTION(".data") = {
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
};

/// Reusable smoke placement sampled separately for every view-eight puff.
///
/// XYZ uses game units in the input space of GsWSMATRIX; sampling writes X/Y/Z
/// in that order and leaves the fourth halfword zero. Spawning snapshots XYZ
/// and retains a borrowed pointer; the room storage stays live with the effects.
static SVECTOR _gDryfieldMainStreetSmokeSpawnPosition = { -850, -1320, 9770, 0 };

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources gDryfieldMainStreetRoomCollisionResources[1] = {
    { .grid                 = &_gDryfieldMainStreetCollisionGrid,
      .viewBoundaryTriggers = _gDryfieldMainStreetViewBoundaryTriggers,
      .actionTriggers       = _gDryfieldMainStreetActionTriggers,
      .occluders            = NULL },
};

u8* gDryfieldMainStreetRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldMainStreetRoomViewCounts[1] = { ARRAY_SIZE(gDryfieldMainStreetViewCameras) };

WorldCoordRoomLighting gDryfieldMainStreetRoomLighting[1] = {
    { .lights = &_gDryfieldMainStreetLights, .ambientTable = NULL },
};

/// One-based endpoint slots and their map-marker flags.
enum {
    DRYFIELD_MAIN_STREET_WARP_GAS_STATION     = 1,
    DRYFIELD_MAIN_STREET_WARP_MOTEL_ROOM1     = 2,
    DRYFIELD_MAIN_STREET_WARP_PARKING_LOT     = 3,
    DRYFIELD_MAIN_STREET_WARP_MOTEL_ROOM2     = 4,
    DRYFIELD_MAIN_STREET_WARP_MOTEL_ROOM3     = 5,
    DRYFIELD_MAIN_STREET_WARP_MOTEL_ROOM4     = 6,
    DRYFIELD_MAIN_STREET_WARP_DRIVEWAY        = 7,
    DRYFIELD_MAIN_STREET_GAS_STATION_MAP_FLAG = 488,
    DRYFIELD_MAIN_STREET_MOTEL_ROOM1_MAP_FLAG = 487,
    DRYFIELD_MAIN_STREET_PARKING_LOT_MAP_FLAG = 486,
    DRYFIELD_MAIN_STREET_MOTEL_ROOM2_MAP_FLAG = 485,
    DRYFIELD_MAIN_STREET_MOTEL_ROOM3_MAP_FLAG = 484,
    DRYFIELD_MAIN_STREET_MOTEL_ROOM4_MAP_FLAG = 483,
    DRYFIELD_MAIN_STREET_DRIVEWAY_MAP_FLAG    = 482,
};

DirectionWarpEntry gDryfieldMainStreetWarpEntries[7] = {
    { .player         = { { .word = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -450, 0, -3435 },
      .unknown_10     = { 0, 0, 0, 0 },
      .companion      = { { .word = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -450, 0, -3435 },
      .unknown_24     = { 0, 0, 0, 0 },
      .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 2),
      .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 1),
      .blockedSound   = DIRECTION_WARP_SOUND_NONE,
      .initialView    = 13,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DRYFIELD_MAIN_STREET_GAS_STATION_MAP_FLAG },
    { .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6588, 0, -2500 },
      .unknown_10     = { 0, 0, 0, 0 },
      .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6588, 0, -2500 },
      .unknown_24     = { 0, 0, 0, 0 },
      .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 6),
      .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 5),
      .blockedSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 9),
      .initialView    = 7,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DRYFIELD_MAIN_STREET_MOTEL_ROOM1_MAP_FLAG },
    { .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6673, 0, 2155 },
      .unknown_10     = { 0, 0, 0, 0 },
      .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6673, 0, 2155 },
      .unknown_24     = { 0, 0, 0, 0 },
      .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 4),
      .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 3),
      .blockedSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 7),
      .initialView    = 6,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DRYFIELD_MAIN_STREET_PARKING_LOT_MAP_FLAG },
    { .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6588, 0, 5900 },
      .unknown_10     = { 0, 0, 0, 0 },
      .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6588, 0, 5900 },
      .unknown_24     = { 0, 0, 0, 0 },
      .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 6),
      .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 5),
      .blockedSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 9),
      .initialView    = 5,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DRYFIELD_MAIN_STREET_MOTEL_ROOM2_MAP_FLAG },
    { .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6652, 0, 0x2956 },
      .unknown_10     = { 0, 0, 0, 0 },
      .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6652, 0, 0x2956 },
      .unknown_24     = { 0, 0, 0, 0 },
      .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 6),
      .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 5),
      .blockedSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 9),
      .initialView    = 5,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DRYFIELD_MAIN_STREET_MOTEL_ROOM3_MAP_FLAG },
    { .player         = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -3478, 0, 0x29B5 },
      .unknown_10     = { 0, 0, 0, 0 },
      .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -3478, 0, 0x29B5 },
      .unknown_24     = { 0, 0, 0, 0 },
      .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 6),
      .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 5),
      .blockedSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 9),
      .initialView    = 4,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DRYFIELD_MAIN_STREET_MOTEL_ROOM4_MAP_FLAG },
    { .player         = { { .word = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -488, 0, 8495 },
      .unknown_10     = { 0, 0, 0, 0 },
      .companion      = { { .word = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -488, 0, 8495 },
      .unknown_24     = { 0, 0, 0, 0 },
      .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 2),
      .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 1),
      .blockedSound   = DIRECTION_WARP_SOUND_NONE,
      .initialView    = 9,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DRYFIELD_MAIN_STREET_DRIVEWAY_MAP_FLAG },
};

/// Collision face normals in room space, with ONE (4096) for unit length.
static SVECTOR _gDryfieldMainStreetCollisionGridNormals[20] = {
#include "assets/dryfield_main_street_collision_056DC_normals.inc"
};

/// Collision mesh vertices in signed room-coordinate game units.
static SVECTOR _gDryfieldMainStreetCollisionGridVerts[180] = {
#include "assets/dryfield_main_street_collision_056DC_verts.inc"
};

/// Indexed collision faces selecting room-local surface classes 0..4.
static WorldCollisionGridFace _gDryfieldMainStreetCollisionGridFaces[85] = {
#include "assets/dryfield_main_street_collision_056DC_faces.inc"
};

/// Halfword face-index pool containing lists ended by WORLD_COLLISION_GRID_CELL_END.
static s16 _gDryfieldMainStreetCollisionGridCells[554] = {
#include "assets/dryfield_main_street_collision_056DC_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldMainStreetCollisionGridCells` (554 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_MAIN_STREET_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldMainStreetCollisionGridCells[(halfwordOffset)])

/// X-major cell pointers into the face-index pool for the 6 by 7 grid.
static s16* _gDryfieldMainStreetCollisionGridTable[42] = {
#include "assets/dryfield_main_street_collision_056DC_table.inc"
};
#undef DRYFIELD_MAIN_STREET_CELL_FACE_LIST_AT_OFFSET

/// Main Street's collision mesh and its 6 by 7 XZ cell index.
///
/// Cells are 4000 game units wide, with origin (-12006, -11340) on XZ.
/// Activation binds the mutable descriptor to the current view coordinate.
/// The mesh pools and cell lists live with the Main Street overlay.
static WorldCollisionGrid _gDryfieldMainStreetCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldMainStreetCollisionGridNormals,
    .vertices    = _gDryfieldMainStreetCollisionGridVerts,
    .faces       = _gDryfieldMainStreetCollisionGridFaces,
    .cellFaceIds = _gDryfieldMainStreetCollisionGridTable,
    .xBias       = 12006,
    .zBias       = 11340,
    .cellCountX  = 6,
    .cellCountZ  = 7,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldMainStreetCollisionGridFaces),
};

ViewCamera gDryfieldMainStreetViewCameras[13] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 4112, 0x6D60, -2748 } }, 289 },
    { { { { -3906, 0, 1231 }, { 39, 4093, 125 }, { -1231, 131, -3904 } }, { 1214, 1299, -3483 } }, 230 },
    { { { { 3809, 0, 1504 }, { 102, 4086, -259 }, { -1500, 279, 3800 } }, { 1205, 1259, 2882 } }, 230 },
    { { { { 3962, 0, 1037 }, { 65, 4087, -250 }, { -1035, 258, 3954 } }, { 1414, 1228, -1924 } }, 230 },
    { { { { 3997, 0, -893 }, { -382, 3702, -1708 }, { 807, 1750, 3613 } }, { 6425, 2237, -3705 } }, 230 },
    { { { { 3980, 0, -964 }, { 9, 4095, 40 }, { 964, -42, 3980 } }, { 6489, 989, 2493 } }, 230 },
    { { { { -3989, 0, -928 }, { 63, 4086, -271 }, { 926, -278, -3980 } }, { 6440, 837, -1624 } }, 230 },
    { { { { 1028, 0, -3964 }, { -2496, 3181, -647 }, { 3079, 2579, 798 } }, { 2261, 1919, -9779 } }, 230 },
    { { { { 3973, 0, -993 }, { -593, 3285, -2373 }, { 796, 2446, 3187 } }, { 1213, 2874, -5701 } }, 230 },
    { { { { 3637, 0, -1883 }, { 328, 4033, 634 }, { 1854, -714, 3581 } }, { 6170, 490, 4710 } }, 230 },
    { { { { 3848, 0, -1401 }, { -215, 4047, -592 }, { 1385, 631, 3802 } }, { 2050, 1920, -3970 } }, 680 },
    { { { { -4092, 0, -170 }, { -1, 4095, 40 }, { 170, 40, -4092 } }, { 1306, 433, -8467 } }, 557 },
    { { { { -4080, 0, 356 }, { 15, 4092, 177 }, { -355, 177, -4076 } }, { 1060, 1050, 30 } }, 257 },
};

/// View 1's empty sprite list, retaining decoded background strips.
///
/// The zero-count first record precedes SPRITE_BATCH_END; no source elements
/// are selected, even though the view retains this list in its source slot.
static SpriteBatch _gDryfieldMainStreetView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Encoded 8-bit sprite pages, palette row 255 and neutral RGB modulation.
enum {
    DRYFIELD_MAIN_STREET_SPRITE_PAGE_832           = getTPage(1, 0, 832, 0),
    DRYFIELD_MAIN_STREET_SPRITE_PAGE_896           = getTPage(1, 0, 896, 0),
    DRYFIELD_MAIN_STREET_SPRITE_PAGE_960           = getTPage(1, 0, 960, 0),
    DRYFIELD_MAIN_STREET_SPRITE_CLUT               = getClut(0, 255),
    DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION = 128,
};

/// View 2's 51 mutable textured rectangles for depth-sorted scene drawing.
///
/// Positions and dimensions use drawing pixels, UVs use texels and depth is
/// an unscaled sorting value. Keep the sources live while packets are built
/// and linked; cached packets snapshot geometry but read source depth each frame.
static SpriteSource _gDryfieldMainStreetView2SpriteSources[51] = {
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 104 } }, -160, -120, 1250, { .fields = { 48, 152 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 80 } }, -160, -16, 1250, { .fields = { 96, 176 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 152 } }, -72, -120, 2350, { .fields = { 80, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 96 } }, -112, -120, 2250, { .fields = { 32, 152 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 80 } }, -160, -24, 1250, { .fields = { 120, 152 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 80 } }, -128, -24, 1750, { .fields = { 104, 152 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 72 } }, -112, -24, 1875, { .fields = { 120, 184 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 64 } }, -104, -24, 2350, { .fields = { 96, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, 16, 2375, { .fields = { 112, 232 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, -24, 1131, { .fields = { 104, 232 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, -144, -16, 2375, { .fields = { 104, 240 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, 16, -16, 2500, { .fields = { 88, 240 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, -16, 1133, { .fields = { 88, 232 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, -8, 1136, { .fields = { 120, 248 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, -104, 0, 2375, { .fields = { 0, 248 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 0, 1138, { .fields = { 112, 248 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, -152, 8, 2375, { .fields = { 40, 248 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 8, 1091, { .fields = { 96, 168 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 16, 1112, { .fields = { 8, 248 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 64 } }, -160, -32, 2375, { .fields = { 96, 128 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 8 } }, -136, -24, 2250, { .fields = { 64, 192 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 8 } }, -128, 0, 2375, { .fields = { 64, 200 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 16 } }, -128, 16, 2375, { .fields = { 56, 176 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 8 } }, -112, -24, 2375, { .fields = { 24, 248 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 64 } }, -96, -32, 2375, { .fields = { 96, 64 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 40, 32 } }, -88, 0, 2375, { .fields = { 40, 112 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 8 } }, -80, -16, 2375, { .fields = { 72, 248 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 64 } }, -48, -32, 2375, { .fields = { 96, 192 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 16 } }, -40, 8, 2375, { .fields = { 120, 232 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 32 } }, -16, -24, 2500, { .fields = { 64, 144 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 16 } }, 8, 8, 2500, { .fields = { 0, 232 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 48 } }, 32, -24, 2500, { .fields = { 72, 64 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 48 } }, 0, -24, 2500, { .fields = { 80, 160 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, -80, 2250, { .fields = { 104, 248 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 104 } }, 16, -72, 2275, { .fields = { 72, 152 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 40 } }, 40, -88, 2250, { .fields = { 72, 208 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 40, 152 } }, 56, -120, 2344, { .fields = { 104, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 40 } }, 40, -8, 2425, { .fields = { 64, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 64, 152 } }, 96, -120, 2300, { .fields = { 16, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 24 } }, 96, -96, 1294, { .fields = { 16, 232 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 56 } }, 104, -112, 1237, { .fields = { 88, 176 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 168 } }, 112, -112, 1190, { .fields = { 96, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 56 } }, 96, 0, 1339, { .fields = { 88, 120 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 56 } }, 104, 0, 1311, { .fields = { 88, 64 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 176 } }, 120, -112, 1166, { .fields = { 112, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 176 } }, 128, -112, 1128, { .fields = { 104, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 184 } }, 136, -120, 1130, { .fields = { 120, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 96 } }, 144, -120, 1094, { .fields = { 24, 152 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 48 } }, 152, -120, 1006, { .fields = { 80, 112 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 64 } }, 144, 8, 1039, { .fields = { 80, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 56, 24 } }, 88, 32, 1250, { .fields = { 24, 40 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 2's six sprite ranges covering all 51 source elements in order.
///
/// The zero-count first record retains decoded background strips; the last
/// record is SPRITE_BATCH_END. Ranges start visible and included in cached
/// packet processing. The final two bytes retain values whose role is unproven.
static SpriteBatch _gDryfieldMainStreetView2SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 7, 0, 0, { 0, 0 } },
    { 9, 24, 0, 0, { 5, 0 } },
    { 33, 6, 0, 0, { 1, 0 } },
    { 39, 11, 0, 0, { 4, 0 } },
    { 50, ARRAY_SIZE(_gDryfieldMainStreetView2SpriteSources) - 50, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's 26 mutable rectangles: drawing pixels, texture texels and sorting depths.
///
/// Sources live with the room while cached packets are built and linked.
static SpriteSource _gDryfieldMainStreetView3SpriteSources[26] = {
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, -120, 920, { .fields = { 112, 216 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 40 } }, -144, -120, 1006, { .fields = { 80, 104 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 24 } }, -128, -120, 1060, { .fields = { 72, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 72 } }, -160, -80, 931, { .fields = { 104, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 72 } }, -152, -80, 936, { .fields = { 104, 72 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 72 } }, -144, -80, 991, { .fields = { 112, 144 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 72 } }, -160, -8, 938, { .fields = { 120, 72 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 72 } }, -152, -8, 967, { .fields = { 120, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 72 } }, -144, -8, 1000, { .fields = { 120, 144 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 72 } }, -136, -8, 1088, { .fields = { 112, 72 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 64 } }, -128, -8, 1117, { .fields = { 96, 192 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 16 } }, -120, -8, 1132, { .fields = { 88, 240 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 32 } }, -120, 16, 1142, { .fields = { 88, 176 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 72 } }, -32, -80, 2087, { .fields = { 112, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 64 } }, -24, -72, 2128, { .fields = { 104, 144 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 64 } }, -16, -72, 2165, { .fields = { 96, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 32 } }, -40, -8, 2002, { .fields = { 80, 208 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 32 } }, -24, -8, 2138, { .fields = { 80, 176 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 32 } }, -16, -8, 2178, { .fields = { 104, 208 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 64 } }, 8, -56, 3120, { .fields = { 96, 128 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 64 } }, 16, -56, 3144, { .fields = { 96, 64 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 16 } }, 24, -56, 3099, { .fields = { 104, 240 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 24 } }, 24, -16, 3102, { .fields = { 72, 24 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 48 } }, 104, -64, 2647, { .fields = { 72, 56 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 32 } }, 96, -16, 2685, { .fields = { 64, 144 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 56 } }, 144, 48, 696, { .fields = { 80, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 3's range list: decoded background strips, all sources, then SPRITE_BATCH_END.
static SpriteBatch _gDryfieldMainStreetView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldMainStreetView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's 17 mutable rectangles: drawing pixels, texture texels and sorting depths.
///
/// Sources live with the room while cached packets are built and linked.
static SpriteSource _gDryfieldMainStreetView4SpriteSources[17] = {
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 112 } }, -160, -120, 960, { .fields = { 120, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 32 } }, -152, -120, 998, { .fields = { 104, 216 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 24 } }, -144, -120, 1037, { .fields = { 96, 96 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 72 } }, -160, -8, 968, { .fields = { 112, 80 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 64 } }, -152, -8, 1071, { .fields = { 104, 64 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 64 } }, -144, -8, 1089, { .fields = { 104, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 24 } }, -136, 32, 1107, { .fields = { 96, 216 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 80 } }, -64, -88, 1943, { .fields = { 112, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 72 } }, -56, -80, 1928, { .fields = { 112, 152 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 32 } }, -64, -8, 1842, { .fields = { 120, 216 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 32 } }, -56, -8, 1952, { .fields = { 112, 224 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 104 } }, -48, -80, 1950, { .fields = { 120, 112 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 32 } }, -40, -8, 1970, { .fields = { 88, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 32 } }, -40, -80, 1907, { .fields = { 96, 32 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 40, 32 } }, 80, -88, 1620, { .fields = { 64, 64 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 48 } }, 96, -56, 1623, { .fields = { 88, 128 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 48, 40 } }, 72, -8, 1706, { .fields = { 64, 176 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 4's range list: decoded background strips, all sources, then SPRITE_BATCH_END.
static SpriteBatch _gDryfieldMainStreetView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldMainStreetView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's 27 mutable rectangles: drawing pixels, texture texels and sorting depths.
///
/// Sources live with the room while cached packets are built and linked.
static SpriteSource _gDryfieldMainStreetView5SpriteSources[27] = {
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 48 } }, -8, -120, 1375, { .fields = { 104, 96 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 8 } }, 0, -8, 1475, { .fields = { 104, 40 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 48 } }, 16, -56, 1489, { .fields = { 120, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 40 } }, 24, -56, 1482, { .fields = { 96, 96 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 40, 48 } }, 120, -120, 329, { .fields = { 88, 48 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 32 } }, 112, -72, 366, { .fields = { 64, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 32 } }, 136, -72, 322, { .fields = { 72, 184 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 24 } }, 72, 24, 612, { .fields = { 56, 56 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 24 } }, 96, 24, 600, { .fields = { 48, 32 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 24 } }, 128, 24, 500, { .fields = { 48, 136 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 16 } }, 72, 48, 625, { .fields = { 48, 184 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 16 } }, 96, 48, 600, { .fields = { 48, 240 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 16 } }, 128, 48, 500, { .fields = { 48, 80 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 16 } }, 64, 64, 650, { .fields = { 40, 16 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 40, 16 } }, 88, 64, 600, { .fields = { 24, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 16 } }, 128, 64, 500, { .fields = { 40, 200 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 24 } }, 56, 80, 690, { .fields = { 80, 64 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 32 } }, 64, 80, 675, { .fields = { 80, 32 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 40 } }, 72, 80, 675, { .fields = { 120, 184 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 40 } }, 80, 80, 650, { .fields = { 88, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 48, 40 } }, 112, 80, 650, { .fields = { 80, 144 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 32 } }, -8, -72, 1412, { .fields = { 104, 224 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 32 } }, -8, -40, 1475, { .fields = { 80, 224 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 24 } }, 96, 0, 600, { .fields = { 48, 160 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 24 } }, 128, 0, 550, { .fields = { 48, 216 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 40 } }, 104, -40, 550, { .fields = { 64, 96 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 40 } }, 136, -40, 500, { .fields = { 96, 184 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 5's range list: decoded background strips, all sources, then SPRITE_BATCH_END.
static SpriteBatch _gDryfieldMainStreetView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldMainStreetView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's 26 mutable rectangles: drawing pixels, texture texels and sorting depths.
///
/// Sources live with the room while cached packets are built and linked.
static SpriteSource _gDryfieldMainStreetView6SpriteSources[26] = {
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 104 } }, -24, -64, 1801, { .fields = { 48, 120 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 8 } }, -24, -72, 1617, { .fields = { 16, 224 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 8 } }, -16, -80, 1450, { .fields = { 24, 240 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 40, 8 } }, -16, -88, 1314, { .fields = { 8, 208 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 8 } }, -8, -96, 1201, { .fields = { 40, 248 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 40, 8 } }, -8, -104, 1106, { .fields = { 8, 216 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 40, 8 } }, 0, -112, 1026, { .fields = { 0, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 48, 8 } }, 0, -120, 956, { .fields = { 0, 232 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 72 } }, -40, -48, 2867, { .fields = { 32, 88 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 24 } }, -24, 0, 2867, { .fields = { 104, 232 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 24 } }, 40, -120, 750, { .fields = { 120, 232 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 224 } }, 48, -120, 775, { .fields = { 72, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 8 } }, 88, 112, 550, { .fields = { 56, 240 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 120 } }, 104, 0, 512, { .fields = { 48, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 24 } }, 128, 0, 512, { .fields = { 80, 224 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 32 } }, 128, 88, 512, { .fields = { 88, 224 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 16 } }, 32, 0, 797, { .fields = { 72, 224 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 88 } }, 40, 0, 800, { .fields = { 40, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 16 } }, 32, 24, 875, { .fields = { 72, 240 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 32 } }, 32, 48, 873, { .fields = { 96, 224 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 224 } }, 64, -120, 700, { .fields = { 88, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 232 } }, 80, -120, 600, { .fields = { 104, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 232 } }, 88, -120, 525, { .fields = { 112, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 96, 568, { .fields = { 80, 248 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 16 } }, -8, 16, 1791, { .fields = { 48, 224 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 48 } }, 120, 56, 623, { .fields = { 16, 160 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 6's three sprite ranges and retained empty range at the source-array end.
///
/// The leading zero count retains background strips; SPRITE_BATCH_END ends
/// the list. All ranges initially participate in cached drawing. The final
/// two bytes retain values whose role is unproven.
static SpriteBatch _gDryfieldMainStreetView6SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 15, 0, 0, { 0, 0 } },
    { 23, ARRAY_SIZE(_gDryfieldMainStreetView6SpriteSources) - 23, 0, 0, { 2, 0 } },
    { ARRAY_SIZE(_gDryfieldMainStreetView6SpriteSources), 0, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's 31 mutable rectangles: drawing pixels, texture texels and sorting depths.
///
/// Sources live with the room while cached packets are built and linked.
static SpriteSource _gDryfieldMainStreetView7SpriteSources[31] = {
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, 16, 0, 2025, { .fields = { 64, 240 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 40, 64 } }, -80, -24, 2025, { .fields = { 104, 184 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 24 } }, -40, 8, 2041, { .fields = { 56, 200 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 40, 72 } }, -24, -24, 2025, { .fields = { 16, 184 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 64, 72 } }, 24, -24, 2025, { .fields = { 0, 96 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, -136, 32, 2000, { .fields = { 64, 224 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, -136, 88, 2000, { .fields = { 64, 232 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 32 } }, -160, -80, 1875, { .fields = { 24, 208 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 48, 16 } }, -152, -120, 2004, { .fields = { 80, 240 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 200 } }, -128, -104, 2000, { .fields = { 56, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 136 } }, -160, -16, 358, { .fields = { 80, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 128 } }, -136, -8, 402, { .fields = { 72, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 40, 240 } }, -128, -120, 468, { .fields = { 72, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 240 } }, -88, -120, 565, { .fields = { 112, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 104 } }, -72, -120, 574, { .fields = { 72, 136 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 16 } }, -72, -16, 581, { .fields = { 72, 240 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 112 } }, -72, 0, 582, { .fields = { 88, 136 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 104 } }, -64, 0, 642, { .fields = { 64, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 96 } }, -56, 0, 669, { .fields = { 64, 104 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 128, 16 } }, -48, -120, 1053, { .fields = { 88, 240 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 120, 16 } }, -40, -104, 1206, { .fields = { 56, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 104, 16 } }, -24, -88, 1221, { .fields = { 72, 16 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -16, -72, 1400, { .fields = { 80, 56 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -8, -64, 1678, { .fields = { 88, 48 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 8 } }, -8, -56, 1653, { .fields = { 120, 248 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 96 } }, 0, -48, 1750, { .fields = { 48, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 64 } }, 16, -16, 1750, { .fields = { 56, 168 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 48, 40 } }, 24, 8, 1750, { .fields = { 8, 168 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 16 } }, 40, 48, 1750, { .fields = { 16, 32 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 120 } }, 72, -56, 1750, { .fields = { 96, 136 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 80, 184 } }, 80, -120, 1750, { .fields = { 104, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 7's three sprite ranges covering all 31 source elements in order.
///
/// The leading zero count retains background strips; SPRITE_BATCH_END ends
/// the list. All ranges initially participate in cached drawing. The final
/// two bytes retain values whose role is unproven.
static SpriteBatch _gDryfieldMainStreetView7SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 5, 0, 0, { 2, 0 } },
    { 10, ARRAY_SIZE(_gDryfieldMainStreetView7SpriteSources) - 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8's empty sprite list, retaining decoded background strips.
///
/// The zero-count range selects no sources; SPRITE_BATCH_END ends the list.
/// This view stores the batch address in both its empty-source and batch slots.
static SpriteBatch _gDryfieldMainStreetView8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 9's empty sprite list, retaining decoded background strips.
///
/// The zero-count range selects no sources; SPRITE_BATCH_END ends the list.
/// This view stores the batch address in both its empty-source and batch slots.
static SpriteBatch _gDryfieldMainStreetView9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 10's twelve mutable sprite rectangles, selected as one complete range.
///
/// Positions and dimensions use pixels, UVs use texels, and depth is an unscaled
/// sorting value. Textures use the 8-bit page at (832, 0) and palette row 255.
/// RGB is neutral for direct drawing; cached packets initially use raw texture.
static SpriteSource _gDryfieldMainStreetView10SpriteSources[12] = {
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_832, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 96 } }, 80, -40, 1977, { .fields = { 88, 8 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_832, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 96 } }, 72, -40, 2058, { .fields = { 80, 8 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_832, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 88 } }, 64, -32, 2142, { .fields = { 72, 16 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_832, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 88 } }, 56, -32, 2238, { .fields = { 64, 16 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_832, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 88 } }, 48, -32, 2343, { .fields = { 56, 16 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_832, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 80 } }, 40, -24, 2453, { .fields = { 48, 24 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_832, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 80 } }, 32, -24, 2579, { .fields = { 40, 24 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_832, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 72 } }, 24, -16, 2713, { .fields = { 32, 32 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_832, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 72 } }, 16, -16, 2869, { .fields = { 24, 32 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_832, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 72 } }, 8, -16, 3043, { .fields = { 16, 32 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_832, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 64 } }, 0, -8, 3231, { .fields = { 8, 40 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_832, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 64 } }, -8, -8, 3454, { .fields = { 0, 40 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 10's complete sprite range, preceded by the background-strip selector.
///
/// All twelve sources initially participate in cached drawing. The list ends
/// with SPRITE_BATCH_END; the two trailing authored bytes have unproven roles.
static SpriteBatch _gDryfieldMainStreetView10SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldMainStreetView10SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 11's empty sprite list, retaining decoded background strips.
///
/// The zero-count range selects no sources; SPRITE_BATCH_END ends the list.
/// This view stores the batch address in both its empty-source and batch slots.
static SpriteBatch _gDryfieldMainStreetView11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 12's empty sprite list, retaining decoded background strips.
///
/// The zero-count range selects no sources; SPRITE_BATCH_END ends the list.
/// This view stores the batch address in both its empty-source and batch slots.
static SpriteBatch _gDryfieldMainStreetView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 13's 53 mutable sprite rectangles, selected by four consecutive ranges.
///
/// Positions and dimensions use pixels, UVs use texels, and depth is an unscaled
/// sorting value. Textures use 8-bit pages at (896, 0)/(960, 0), palette row 255.
/// RGB is neutral for direct drawing; cached packets initially use raw texture.
static SpriteSource _gDryfieldMainStreetView13SpriteSources[53] = {
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, -152, -48, 1675, { .fields = { 56, 208 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, -104, -24, 1675, { .fields = { 56, 176 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, -96, 0, 1675, { .fields = { 56, 168 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, -160, 8, 1675, { .fields = { 56, 184 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, -152, 32, 1675, { .fields = { 56, 200 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, -64, 32, 1675, { .fields = { 56, 192 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 104 } }, -144, -64, 1675, { .fields = { 0, 112 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 72, 8 } }, -128, -48, 1675, { .fields = { 40, 48 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 16 } }, -128, -32, 1675, { .fields = { 64, 192 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 72, 8 } }, -128, -8, 1675, { .fields = { 40, 40 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 72, 16 } }, -128, 8, 1675, { .fields = { 72, 216 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 96 } }, -56, -56, 1675, { .fields = { 112, 96 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 16 } }, -48, 24, 1675, { .fields = { 112, 184 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 16 } }, -80, -32, 1675, { .fields = { 104, 240 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 16 } }, -160, -32, 1675, { .fields = { 96, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 8 } }, -160, -8, 1675, { .fields = { 96, 56 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 64, 8 } }, -40, -48, 1675, { .fields = { 48, 72 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 64, 16 } }, -40, -32, 1675, { .fields = { 64, 192 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 64, 8 } }, -40, -8, 1675, { .fields = { 48, 64 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 64, 16 } }, -40, 8, 1675, { .fields = { 80, 232 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 96 } }, 24, -56, 1675, { .fields = { 32, 160 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 48, 8 } }, 56, -48, 1675, { .fields = { 64, 32 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 48, 8 } }, 56, -32, 1675, { .fields = { 80, 208 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 48, 8 } }, 56, -8, 1675, { .fields = { 96, 248 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 48, 8 } }, 56, 8, 1675, { .fields = { 64, 16 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 96 } }, 104, -56, 1675, { .fields = { 112, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 8 } }, 128, -48, 1675, { .fields = { 80, 24 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 8 } }, 128, -24, 1675, { .fields = { 56, 248 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 8 } }, 136, -8, 1675, { .fields = { 88, 88 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 8 } }, 136, 8, 1675, { .fields = { 88, 80 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 208 } }, -160, -88, 701, { .fields = { 120, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 184 } }, -152, -64, 753, { .fields = { 104, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 184 } }, -136, -64, 803, { .fields = { 88, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 168 } }, -120, -56, 857, { .fields = { 56, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 136 } }, -112, -40, 899, { .fields = { 24, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 128 } }, -104, -40, 937, { .fields = { 16, 128 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 128 } }, -96, -40, 1005, { .fields = { 16, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 120 } }, -88, -40, 1078, { .fields = { 24, 136 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 112 } }, -80, -40, 1155, { .fields = { 8, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_896, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 96 } }, -72, -40, 1179, { .fields = { 120, 96 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 168 } }, -72, -120, 1625, { .fields = { 64, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 24, 160 } }, -16, -120, 1641, { .fields = { 32, 0 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 24, 1675, { .fields = { 112, 200 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 16 } }, -88, 24, 1675, { .fields = { 64, 232 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 32, 24 } }, -72, 24, 1675, { .fields = { 64, 208 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 24 } }, -40, 16, 1682, { .fields = { 64, 168 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 16 } }, -24, 16, 1675, { .fields = { 80, 168 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 32 } }, -16, 8, 1674, { .fields = { 112, 208 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 24 } }, 0, 16, 1682, { .fields = { 80, 232 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 32 } }, 16, 8, 1675, { .fields = { 96, 184 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 24 } }, 40, 16, 1675, { .fields = { 96, 216 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 16, 24 } }, 128, 16, 1682, { .fields = { 80, 184 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_MAIN_STREET_SPRITE_PAGE_960, DRYFIELD_MAIN_STREET_SPRITE_CLUT, { .fields = { 8, 16 } }, 144, 16, 1675, { .fields = { 96, 240 } }, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MAIN_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 13's four sprite ranges covering all 53 sources in order.
///
/// The leading zero count retains background strips; SPRITE_BATCH_END ends
/// the list. All ranges initially participate in cached drawing. The final
/// two bytes retain authored values whose role is unproven.
static SpriteBatch _gDryfieldMainStreetView13SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 3, 0 } },
    { 30, 10, 0, 0, { 0, 0 } },
    { 40, 2, 0, 0, { 2, 0 } },
    { 42, ARRAY_SIZE(_gDryfieldMainStreetView13SpriteSources) - 42, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldMainStreetSpriteViews[13] = {
    { { .empty = _gDryfieldMainStreetView1SpriteBatches }, _gDryfieldMainStreetView1SpriteBatches, NULL },
    { { .elements = _gDryfieldMainStreetView2SpriteSources }, _gDryfieldMainStreetView2SpriteBatches, NULL },
    { { .elements = _gDryfieldMainStreetView3SpriteSources }, _gDryfieldMainStreetView3SpriteBatches, NULL },
    { { .elements = _gDryfieldMainStreetView4SpriteSources }, _gDryfieldMainStreetView4SpriteBatches, NULL },
    { { .elements = _gDryfieldMainStreetView5SpriteSources }, _gDryfieldMainStreetView5SpriteBatches, NULL },
    { { .elements = _gDryfieldMainStreetView6SpriteSources }, _gDryfieldMainStreetView6SpriteBatches, NULL },
    { { .elements = _gDryfieldMainStreetView7SpriteSources }, _gDryfieldMainStreetView7SpriteBatches, NULL },
    { { .empty = _gDryfieldMainStreetView8SpriteBatches }, _gDryfieldMainStreetView8SpriteBatches, NULL },
    { { .empty = _gDryfieldMainStreetView9SpriteBatches }, _gDryfieldMainStreetView9SpriteBatches, NULL },
    { { .elements = _gDryfieldMainStreetView10SpriteSources }, _gDryfieldMainStreetView10SpriteBatches, NULL },
    { { .empty = _gDryfieldMainStreetView11SpriteBatches }, _gDryfieldMainStreetView11SpriteBatches, NULL },
    { { .empty = _gDryfieldMainStreetView12SpriteBatches }, _gDryfieldMainStreetView12SpriteBatches, NULL },
    { { .elements = _gDryfieldMainStreetView13SpriteSources }, _gDryfieldMainStreetView13SpriteBatches, NULL },
};

/// Main Street's 26 mutable view-boundary quads, including the final LAST record.
///
/// Parameter bytes select source and destination logical views in 2..13.
/// Setup binds room-space geometry to the current view and links every record.
/// Normals use ONE (4096); positions and radii use whole room-coordinate units.
static WorldCollisionTrigger _gDryfieldMainStreetViewBoundaryTriggers[26] = {
    { NULL, NULL, NULL, { -2385, -3392, 351, 0 }, { { -2863, -4416, -165, 0 }, { 2863, -4416, 165, 0 }, { -2863, 4416, -165, 0 }, { 2863, 4416, 165, 0 } }, { 236, 0, -4102, 0 }, { 0, 0, ONE, 0 }, 5246, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2337, -3376, 239, 0 }, { { 2868, -4400, 137, 0 }, { -2867, -4400, -136, 0 }, { 2868, 4400, 137, 0 }, { -2867, 4400, -136, 0 } }, { -195, 0, 4092, 0 }, { 0, 0, ONE, 0 }, 5246, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3665, -3232, 5071, 0 }, { { -1383, -4256, 519, 0 }, { 1381, -4256, -521, 0 }, { -1383, 4256, 519, 0 }, { 1381, 4256, -521, 0 } }, { -1447, 0, -3843, 0 }, { 0, 0, ONE, 0 }, 4492, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3764, -3216, 4924, 0 }, { { 1447, -4240, -514, 0 }, { -1449, -4240, 512, 0 }, { 1447, 4240, -514, 0 }, { -1449, 4240, 512, 0 } }, { 1368, 0, 3863, 0 }, { 0, 0, ONE, 0 }, 4492, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6177, -3200, 5327, 0 }, { { 980, -4224, 41, 0 }, { -979, -4224, -40, 0 }, { 980, 4224, 41, 0 }, { -979, 4224, -40, 0 } }, { -171, 0, 4095, 0 }, { 0, 0, ONE, 0 }, 4314, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6210, -3280, 5598, 0 }, { { -979, -4304, 7, 0 }, { 980, -4304, -7, 0 }, { -979, 4304, 7, 0 }, { 980, 4304, -7, 0 } }, { -30, 0, -4117, 0 }, { 0, 0, ONE, 0 }, 4404, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5988, -3552, -292, 0 }, { { -959, -4576, 198, 0 }, { 960, -4576, -198, 0 }, { -959, 4576, 198, 0 }, { 960, 4576, -198, 0 } }, { -829, 0, -4015, 0 }, { 0, 0, ONE, 0 }, 4664, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6051, -3536, -452, 0 }, { { 949, -4560, -244, 0 }, { -949, -4560, 245, 0 }, { 949, 4560, -244, 0 }, { -949, 4560, 245, 0 } }, { 1025, 0, 3983, 0 }, { 0, 0, ONE, 0 }, 4636, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5216, -3488, -2833, 0 }, { { -8, -4224, -2308, 0 }, { 8, -4224, 2308, 0 }, { -8, 4224, -2308, 0 }, { 8, 4224, 2308, 0 } }, { 4105, 0, -15, 0 }, { 0, 0, ONE, 0 }, 4802, 0, 2, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5072, -3488, -2960, 0 }, { { 8, -4224, 2292, 0 }, { -8, -4224, -2292, 0 }, { 8, 4224, 2292, 0 }, { -8, 4224, -2292, 0 } }, { -4102, 0, 14, 0 }, { 0, 0, ONE, 0 }, 4802, 0, 7, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5312, -2848, 0x28E0, 0 }, { { 235, -4224, -1330, 0 }, { -240, -4224, 1325, 0 }, { 235, 4224, -1330, 0 }, { -240, 4224, 1325, 0 } }, { 4031, 0, 720, 0 }, { 0, 0, ONE, 0 }, 4434, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5216, -3008, 0x2940, 0 }, { { -239, -4224, 1326, 0 }, { 236, -4224, -1329, 0 }, { -239, 4224, 1326, 0 }, { 236, 4224, -1329, 0 } }, { -4033, 0, -722, 0 }, { 0, 0, ONE, 0 }, 4434, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1314, -3296, 4926, 0 }, { { 1314, -4256, 675, 0 }, { -1313, -4256, -675, 0 }, { 1314, 4256, 675, 0 }, { -1313, 4256, -675, 0 } }, { -1878, 0, 3651, 0 }, { 0, 0, ONE, 0 }, 4492, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1076, -3264, 5229, 0 }, { { -1235, -4256, -651, 0 }, { 1236, -4256, 651, 0 }, { -1235, 4256, -651, 0 }, { 1236, 4256, 651, 0 } }, { 1912, 0, -3632, 0 }, { 0, 0, ONE, 0 }, 4463, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5056, -3232, 2256, 0 }, { { 24, -4224, 2036, 0 }, { -24, -4224, -2036, 0 }, { 24, 4224, 2036, 0 }, { -24, 4224, -2036, 0 } }, { -4105, 0, 48, 0 }, { 0, 0, ONE, 0 }, 4664, 0, 6, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5216, -2944, 2272, 0 }, { { -24, -4224, -2100, 0 }, { 24, -4224, 2100, 0 }, { -24, 4224, -2100, 0 }, { 24, 4224, 2100, 0 } }, { 4111, 0, -47, 0 }, { 0, 0, ONE, 0 }, 4692, 0, 3, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5248, -2912, 6816, 0 }, { { -24, -4224, -1844, 0 }, { 24, -4224, 1844, 0 }, { -24, 4224, -1844, 0 }, { 24, 4224, 1844, 0 } }, { 4101, 0, -54, 0 }, { 0, 0, ONE, 0 }, 4608, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5040, -3008, 6912, 0 }, { { -24, -4224, 1844, 0 }, { 24, -4224, -1844, 0 }, { -24, 4224, 1844, 0 }, { 24, 4224, -1844, 0 } }, { -4103, 0, -54, 0 }, { 0, 0, ONE, 0 }, 4608, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -384, -2624, 7537, 0 }, { { -1800, -4416, 722, 0 }, { 1793, -4416, -730, 0 }, { -1800, 4416, 722, 0 }, { 1793, 4416, -730, 0 } }, { -1542, 0, -3815, 0 }, { 0, 0, ONE, 0 }, 4802, 0, 4, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -448, -2816, 7328, 0 }, { { 1841, -4416, -751, 0 }, { -1852, -4416, 741, 0 }, { 1841, 4416, -751, 0 }, { -1852, 4416, 741, 0 } }, { 1537, 0, 3806, 0 }, { 0, 0, ONE, 0 }, 4830, 0, 9, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2465, -2944, 9983, 0 }, { { 211, -4416, -1982, 0 }, { -215, -4416, 1978, 0 }, { 211, 4416, -1982, 0 }, { -215, 4416, 1978, 0 } }, { 4081, 0, 438, 0 }, { 0, 0, ONE, 0 }, 4830, 0, 9, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2370, -2976, 0x27BE, 0 }, { { -217, -4416, 1976, 0 }, { 209, -4416, -1985, 0 }, { -217, 4416, 1976, 0 }, { 209, 4416, -1985, 0 } }, { -4083, 0, -440, 0 }, { 0, 0, ONE, 0 }, 4830, 0, 4, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 62, -3264, -1955, 0 }, { { -2049, -4416, -351, 0 }, { 2050, -4416, 351, 0 }, { -2049, 4416, -351, 0 }, { 2050, 4416, 351, 0 } }, { 694, 0, -4055, 0 }, { 0, 0, ONE, 0 }, 4857, 0, 13, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -192, -3264, -2146, 0 }, { { 1607, -4416, 267, 0 }, { -1607, -4416, -266, 0 }, { 1607, 4416, 267, 0 }, { -1607, 4416, -266, 0 } }, { -671, 0, 4041, 0 }, { 0, 0, ONE, 0 }, 4692, 0, 2, 13, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1602, -3296, -4498, 0 }, { { -133, -4416, 2152, 0 }, { 134, -4416, -2152, 0 }, { -133, 4416, 2152, 0 }, { 134, 4416, -2152, 0 } }, { -4099, 0, -255, 0 }, { 0, 0, ONE, 0 }, 4910, 0, 2, 13, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1793, -3360, -4513, 0 }, { { 139, -4416, -2237, 0 }, { -139, -4416, 2238, 0 }, { 139, 4416, -2237, 0 }, { -139, 4416, 2238, 0 } }, { 4090, 0, 253, 0 }, { 0, 0, ONE, 0 }, 4937, 0, 13, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room commands selected by Main Street's non-warp triggers.
enum {
    DRYFIELD_MAIN_STREET_TRIGGER_TALK_COMMAND      = 1,
    DRYFIELD_MAIN_STREET_TRIGGER_FIRST_VISIT_SCENE = 1,
};

/// Seven departures, one room talk action and two automatic first-visit scene regions.
///
/// Setup binds and links all ten mutable room-space quads, including LAST.
/// Warp parameters pack the one-based local endpoint and destination arrival
/// slots into high/low nibbles. The CAP action sends room command 1; both
/// automatic regions send room action 1. Normals use ONE, geometry uses room units.
static WorldCollisionTrigger _gDryfieldMainStreetActionTriggers[10] = {
    { NULL, NULL, NULL, { -384, 0, -3392, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -ONE, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_GAS_STATION, (DRYFIELD_MAIN_STREET_WARP_GAS_STATION << 4) | 2, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6912, -48, -2672, 0 }, { { -416, 0, -848, 0 }, { 416, 0, -848, 0 }, { -416, 0, 848, 0 }, { 416, 0, 848, 0 } }, { 0, 4105, 0, 0 }, { ONE, 0, 0, 0 }, 942, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MOTEL_ROOM_1, (DRYFIELD_MAIN_STREET_WARP_MOTEL_ROOM1 << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6848, -48, 2016, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_PARKING_LOT, (DRYFIELD_MAIN_STREET_WARP_PARKING_LOT << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6848, -48, 5824, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MOTEL_ROOM_2, (DRYFIELD_MAIN_STREET_WARP_MOTEL_ROOM2 << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6816, -48, 0x2870, 0 }, { { -416, 0, -848, 0 }, { 416, 0, -848, 0 }, { -416, 0, 848, 0 }, { 416, 0, 848, 0 } }, { 0, 4105, 0, 0 }, { ONE, 0, 0, 0 }, 942, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MOTEL_ROOM_3, (DRYFIELD_MAIN_STREET_WARP_MOTEL_ROOM3 << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3536, -48, 0x2A00, 0 }, { { 816, 0, -416, 0 }, { 816, 0, 416, 0 }, { -816, 0, -416, 0 }, { -816, 0, 416, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, -ONE, 0 }, 914, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MOTEL_ROOM_4, (DRYFIELD_MAIN_STREET_WARP_MOTEL_ROOM4 << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -800, -64, 0x2880, 0 }, { { 1024, 0, -960, 0 }, { 1024, 0, 960, 0 }, { -1024, 0, -960, 0 }, { -1024, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { -ONE, 0, 0, 0 }, 1402, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_MAIN_STREET_TRIGGER_TALK_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -257, -48, 8703, 0 }, { { -415, 0, -511, 0 }, { 417, 0, -511, 0 }, { -416, 0, 512, 0 }, { 416, 0, 512, 0 } }, { 0, 4099, 0, 0 }, { -ONE, 0, 0, 0 }, 658, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_DRIVEWAY, (DRYFIELD_MAIN_STREET_WARP_DRIVEWAY << 4) | 3, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 320, -64, -1520, 0 }, { { -2880, 0, -880, 0 }, { 2880, 0, -624, 0 }, { -2880, 0, 624, 0 }, { 2880, 0, 880, 0 } }, { 0, 4102, 0, 0 }, { -ONE, 0, 0, 0 }, 3007, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_MAIN_STREET_TRIGGER_FIRST_VISIT_SCENE, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -2144, -64, -3952, 0 }, { { -640, 0, -3136, 0 }, { 544, 0, -3104, 0 }, { -544, 0, 3104, 0 }, { 640, 0, 3136, 0 } }, { 0, 4099, 0, 0 }, { -ONE, 0, 0, 0 }, 3197, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_MAIN_STREET_TRIGGER_FIRST_VISIT_SCENE, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Placement resource IDs shared by Main Street's area variants.
enum {
    DRYFIELD_MAIN_STREET_CUTSCENE_CHASER_ENTRY = 101,
    DRYFIELD_MAIN_STREET_CHASER_ENTRY          = 1,
    DRYFIELD_MAIN_STREET_SCORPION_ENTRY        = 25,
    DRYFIELD_MAIN_STREET_PAWN_GOLEM_ENTRY      = 20,
    DRYFIELD_MAIN_STREET_ROOK_GOLEM_ENTRY      = 23,
};

/// Variant 1's cutscene and regular Desert Chaser resources, ending at AREA_PLACEMENT_END.
///
/// Loads files 323000 and 400100 for entries 101 and 1; each uses descriptor 0.
/// Placements are borrowed from the Dryfield map. Resource recipes and task
/// callbacks require their room and actor overlays during loading and spawning.
static AreaResource _gDryfieldMainStreetVariant1Resources[3] = {
    { .entryId = DRYFIELD_MAIN_STREET_CUTSCENE_CHASER_ENTRY, .fileNumber = 230, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor323000DesertChaserTaskDesc },
    { .entryId = DRYFIELD_MAIN_STREET_CHASER_ENTRY, .fileNumber = 1, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor00100DesertChaserTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's scorpion resource, shared by six map-owned placements.
///
/// Entry 25 loads file 102500 and spawns descriptor 0; AREA_PLACEMENT_END ends
/// the recipes. Keep the room/map records live through loading and spawning,
/// and the actor overlay through task execution.
static AreaResource _gDryfieldMainStreetVariant2Resources[2] = {
    { .entryId = DRYFIELD_MAIN_STREET_SCORPION_ENTRY, .fileNumber = 25, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor102500Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 3's regular Desert Chaser resource, shared by two map-owned placements.
///
/// Entry 1 loads file 400100 and spawns descriptor 0; AREA_PLACEMENT_END ends
/// the recipes. Keep the room/map records live through loading and spawning,
/// and the actor overlay through task execution.
static AreaResource _gDryfieldMainStreetVariant3Resources[2] = {
    { .entryId = DRYFIELD_MAIN_STREET_CHASER_ENTRY, .fileNumber = 1, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor00100DesertChaserTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 7's Pawn and Rook GOLEM resources for two map-owned placements.
///
/// Entries 20 and 23 load files 102000 and 202300 and spawn descriptor 0.
/// AREA_PLACEMENT_END ends the recipes. Keep room/map records live through
/// loading and spawning, and both actor overlays through task execution.
static AreaResource _gDryfieldMainStreetVariant7Resources[3] = {
    { .entryId = DRYFIELD_MAIN_STREET_PAWN_GOLEM_ENTRY, .fileNumber = 20, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor102000PawnGolemTasks },
    { .entryId = DRYFIELD_MAIN_STREET_ROOK_GOLEM_ENTRY, .fileNumber = 23, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor202300RookGolemTasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 10's cutscene and regular Desert Chaser resources for room-owned placements.
///
/// Entries 101 and 1 load files 323000 and 400100 and spawn descriptor 0.
/// AREA_PLACEMENT_END ends the recipes. Storage is distinct from variant 1's;
/// records and actor overlays must remain live during loading and spawning.
static AreaResource _gDryfieldMainStreetVariant10Resources[3] = {
    { .entryId = DRYFIELD_MAIN_STREET_CUTSCENE_CHASER_ENTRY, .fileNumber = 230, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor323000DesertChaserTaskDesc },
    { .entryId = DRYFIELD_MAIN_STREET_CHASER_ENTRY, .fileNumber = 1, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor00100DesertChaserTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 10's cutscene chaser at the origin and regular chaser on Main Street.
///
/// Positions use room units and yaw uses 4096 per turn. Both select base files,
/// actor variant/mode/row zero, unchanged texture pages and CLUT rows shifted by
/// two. AREA_PLACEMENT_END ends the table; actors borrow records while live.
static AreaPlacement _gDryfieldMainStreetVariant10Placements[3] = {
    { .entryId = DRYFIELD_MAIN_STREET_CUTSCENE_CHASER_ENTRY, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = DRYFIELD_MAIN_STREET_CHASER_ENTRY, .variant = 0, .mode = 0, .x = -1638, .y = 0, .z = 4280, .yaw = -1012, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldMainStreetAreaVariants[13] = {
    [1]  = { .placements = gMapDryfieldMainStreetVariant1Placements, .resources = _gDryfieldMainStreetVariant1Resources },
    [2]  = { .placements = gMapDryfieldMainStreetVariant2Placements, .resources = _gDryfieldMainStreetVariant2Resources },
    [3]  = { .placements = gMapDryfieldMainStreetVariant3Placements, .resources = _gDryfieldMainStreetVariant3Resources },
    [7]  = { .placements = gMapDryfieldMainStreetVariant7Placements, .resources = _gDryfieldMainStreetVariant7Resources },
    [10] = { .placements = _gDryfieldMainStreetVariant10Placements, .resources = _gDryfieldMainStreetVariant10Resources },
};

/// Sixteen authored point lights eligible in every Main Street view.
///
/// Placements and falloff radii use integer world-coordinate units; RGB uses
/// ONE (4096) for unit intensity. Coordinate parents, composition caches and
/// attenuation are mutable gameplay state. Storage lives with the room overlay.
static WorldCoordPointLight _gDryfieldMainStreetPointLights[16] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3910, -4583, -2531 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4642, 3808, 3733 }, { 0, 0 } }, 2050, 0x312B },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5600, -2355, -4127 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4097, 3734, 3714 }, { 0, 0 } }, 1303, 3024 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5140, -2738, -5045 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3502, 3033, 2983 }, { 0, 0 } }, 1421, 3621 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4637, -2215, -5198 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3359, 2922, 2821 }, { 0, 0 } }, 701, 1211 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4405, -2155, -5380 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3274, 2784, 2741 }, { 0, 0 } }, 1063, 2153 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5104, -900, -5740 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3163, 2621, 2539 }, { 0, 0 } }, 100, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5037, -900, -5783 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3003, 2621, 2539 }, { 0, 0 } }, 100, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2794, -4433, 5 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4193, 3948, 3953 }, { 0, 0 } }, 1792, 7511 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5464, -4865, -456 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4304, 3842, 3575 }, { 0, 0 } }, 1755, 6785 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1367, -4865, 872 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3848, 2949, 2931 }, { 0, 0 } }, 9220, 0x450B },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1162, -3422, -5474 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3969, 3722, 3700 }, { 0, 0 } }, 283, 4826 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3332, -3624, -6033 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4112, 3810, 3716 }, { 0, 0 } }, 1463, 4128 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4925, -2543, -2722 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3254, 2855, 2829 }, { 0, 0 } }, 705, 2774 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4029, -3246, -2729 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2792, 2483, 2445 }, { 0, 0 } }, 1132, 2911 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3765, -3608, -7108 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3324, 2700, 2611 }, { 0, 0 } }, 480, 1328 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3265, -2641, -2536 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3170, 2745, 2730 }, { 0, 0 } }, 822, 2975 },
};

/// Main Street's mutable point-light collection, without directional or cone lights.
///
/// Light queries borrow the complete point-light array while this overlay is loaded.
static WorldCoordRoomLights _gDryfieldMainStreetLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldMainStreetPointLights),
    .pointLights           = _gDryfieldMainStreetPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Type-1 footstep cue bases selecting scripts from the currently loaded bank.
enum {
    DRYFIELD_MAIN_STREET_CLASS4_WALK_FOOTSTEP_SOUND = 0x10000049,
    DRYFIELD_MAIN_STREET_CLASS4_RUN_FOOTSTEP_SOUND  = 0x1000004B,
    DRYFIELD_MAIN_STREET_CLASS2_WALK_FOOTSTEP_SOUND = 0x1000003D,
    DRYFIELD_MAIN_STREET_CLASS2_RUN_FOOTSTEP_SOUND  = 0x1000003F,
    DRYFIELD_MAIN_STREET_CLASS3_WALK_FOOTSTEP_SOUND = 0x1000002D,
    DRYFIELD_MAIN_STREET_CLASS3_RUN_FOOTSTEP_SOUND  = 0x1000002F,
};

/// Surface class 4's walking, running and scripted-jump cue bases.
///
/// Walk and jump use entry 0x49; running uses 0x4B. Animation cue 1 adds one,
/// and a companion adds 100. Keep this table and the current sound bank loaded.
static WorldCollisionFootstepSounds _gDryfieldMainStreetSurfaceClass4FootstepSounds = {
    .walk         = DRYFIELD_MAIN_STREET_CLASS4_WALK_FOOTSTEP_SOUND,
    .run          = DRYFIELD_MAIN_STREET_CLASS4_RUN_FOOTSTEP_SOUND,
    .scriptedJump = DRYFIELD_MAIN_STREET_CLASS4_WALK_FOOTSTEP_SOUND,
};

/// Surface class 2's walking, running and scripted-jump cue bases.
///
/// Walk and jump use entry 0x3D; running uses 0x3F. Animation cue 1 adds one,
/// and a companion adds 100. Keep this table and the current sound bank loaded.
static WorldCollisionFootstepSounds _gDryfieldMainStreetSurfaceClass2FootstepSounds = {
    .walk         = DRYFIELD_MAIN_STREET_CLASS2_WALK_FOOTSTEP_SOUND,
    .run          = DRYFIELD_MAIN_STREET_CLASS2_RUN_FOOTSTEP_SOUND,
    .scriptedJump = DRYFIELD_MAIN_STREET_CLASS2_WALK_FOOTSTEP_SOUND,
};

/// Surface class 3's walking, running and scripted-jump cue bases.
///
/// Walk and jump use entry 0x2D; running uses 0x2F. Animation cue 1 adds one,
/// and a companion adds 100. Keep this table and the current sound bank loaded.
static WorldCollisionFootstepSounds _gDryfieldMainStreetSurfaceClass3FootstepSounds = {
    .walk         = DRYFIELD_MAIN_STREET_CLASS3_WALK_FOOTSTEP_SOUND,
    .run          = DRYFIELD_MAIN_STREET_CLASS3_RUN_FOOTSTEP_SOUND,
    .scriptedJump = DRYFIELD_MAIN_STREET_CLASS3_WALK_FOOTSTEP_SOUND,
};

/// Classes 0 and 5..7: blocked probes, enabled weapon impacts and silent footsteps.
///
/// Applies pushback. The unused first byte retains its authored zero; its role
/// is unproven. The room overlay owns this shared policy record.
static WorldCollisionSurfaceProperties _gDryfieldMainStreetSilentBlockingSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 4: blocked probes, enabled weapon impacts, pushback and class-4 footstep cues.
///
/// The unused first byte retains zero with unproven meaning. Both this policy
/// and its borrowed footstep table live with the room overlay.
static WorldCollisionSurfaceProperties _gDryfieldMainStreetSurfaceClass4Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMainStreetSurfaceClass4FootstepSounds,
};

/// Class 2: blocked probes, enabled weapon impacts, pushback and class-2 footstep cues.
///
/// The unused first byte retains zero with unproven meaning. Both this policy
/// and its borrowed footstep table live with the room overlay.
static WorldCollisionSurfaceProperties _gDryfieldMainStreetSurfaceClass2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMainStreetSurfaceClass2FootstepSounds,
};

/// Class 3: blocked probes, enabled weapon impacts, pushback and class-3 footstep cues.
///
/// The unused first byte retains zero with unproven meaning. Both this policy
/// and its borrowed footstep table live with the room overlay.
static WorldCollisionSurfaceProperties _gDryfieldMainStreetSurfaceClass3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMainStreetSurfaceClass3FootstepSounds,
};

/// Class 1: probes pass, weapon impacts are ignored, pushback applies and footsteps are silent.
///
/// The unused first byte retains zero with unproven meaning. This policy record
/// lives with the room overlay; probe passage does not suppress grid pushback.
static WorldCollisionSurfaceProperties _gDryfieldMainStreetProbePassThroughSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gDryfieldMainStreetSurfacePropertiesByClass[8] = {
    &_gDryfieldMainStreetSilentBlockingSurface,
    &_gDryfieldMainStreetProbePassThroughSurface,
    &_gDryfieldMainStreetSurfaceClass2Properties,
    &_gDryfieldMainStreetSurfaceClass3Properties,
    &_gDryfieldMainStreetSurfaceClass4Properties,
    &_gDryfieldMainStreetSilentBlockingSurface,
    &_gDryfieldMainStreetSilentBlockingSurface,
    &_gDryfieldMainStreetSilentBlockingSurface,
};

RoomFadeStorage gRoomEventFade = { 0 };

/// Destination snapshot retained from event acceptance through staged scene reload.
///
/// The start handler copies all eight bytes; playback commits area, warp and
/// room to the live save. Keep the snapshot unchanged until reload tears down
/// the room. A later accepted start replaces it; no separate queue is stored.
static RoomEventMsg _gRoomEventStagedMsg = { 0 };

/// Latest staged motel-scene start indication (0 no accepted start, 1 requested).
///
/// Each eligible resolver branch clears it, including queries, then sets it
/// after requesting a task even if spawn fails. It is not a live-task handle.
/// Retains seven zero bytes whose role and grouping remain unproven.
static RoomEventStartStorage _gMainStreetStagedSceneStartStorage = { 0 };

/// Snapshot of the accepted transition for the deferred room event.
///
/// The gate copies all eight bytes before spawning; another accepted start
/// replaces them. The task commits area, arrival and the room byte on reload.
/// Keep this room loaded and the snapshot unchanged until that task finishes.
static RoomEventMsg _gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 3, 190, 28 } };

/// Retained, non-owning handle for the first-visit scene's player head aiming.
///
/// Spawn replaces it, including with NULL on failure. Script stop kills the task
/// and clears it; scene-start reset only forgets it and leaves any task alive.
/// A non-NULL handle must still be live when controlled; task teardown alone does
/// not clear this slot. Keep the room loaded while the task can be dispatched.
static Task* _gDryfieldMainStreetPlayerHeadAimTask = NULL;

RoomLatchedEventStorage gRoomEventLatched = { { 0 }, { 0 } };

RoomEventReq gRoomEventReq = { 0, 0, 0, 0, 0, 0 };

#include "../../shared/room/room_event_staged_task.inc.c"

#include "../../shared/room/room_event.inc.c"

/// Room-task lifecycle: initialize, idle, release; indexed by unchecked state 0..2.
static const TaskFuncTable3 _gDryfieldMainStreetRoomTaskStates = {
    { _dryfieldMainStreetInitialize, _dryfieldMainStreetIdle, taskKill },
};

#include "../../shared/main_street_resolve_msg.inc.c"

#include "../../shared/main_street_play_time_task.inc.c"

#include "../../shared/main_street_cap_sound_cue.inc.c"

#include "../../shared/main_street_talk_msg.inc.c"

/// Refuses every key-item-use request, selecting the inventory's cannot-use notice.
///
/// Handles message 0x13F1; `itemId` is the collected item ID and `unusedArg`
/// is normally zero. No argument is read and the result is always zero.
static s32 _dryfieldMainStreetRejectKeyItemUse(Task* receiver, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Starts the first-visit scene when its room-action trigger has not been seen.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`, borrowing `request` only through
/// dispatch. Action 1 starts the scene and its skip continuation, records it
/// as seen and resets the follow-up dialogue state; other actions do nothing.
/// The remaining arguments are ignored and the reply is always zero.
static s32 _dryfieldMainStreetStartFirstVisitCutscene(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { DRYFIELD_MAIN_STREET_ACTION_FIRST_VISIT_SCENE = 1 };

    if ((request->actionId == DRYFIELD_MAIN_STREET_ACTION_FIRST_VISIT_SCENE) && (gameFlagGetNibble(GAME_FLAG_MAIN_STREET_CUTSCENE_SEEN) == 0)) {
        // Begin with no retained head-aim handle; the script may spawn a new one.
        _dryfieldMainStreetForgetPlayerHeadAimTask(0);
        evsStartScriptWithSkip(_gDryfieldMainStreetFirstVisitScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldMainStreetFirstVisitSkipScript);
        gameFlagSetNibble(GAME_FLAG_MAIN_STREET_CUTSCENE_SEEN, 1);
        gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
        gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, 1);
    }
    return 0;
}

/// Registers the room's message receiver and enables CAP completion sound cues.
///
/// Requires state zero and a live scene task. Before the first-visit scene has
/// been seen, broadcasts the initial actor command in the stage-2/area-2
/// namespace. Advances to the idle state; the room overlay must stay loaded
/// while its task can receive messages.
static void _dryfieldMainStreetInitialize(Task* task)
{
    task->msgTable = _gDryfieldMainStreetRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    gCapRoomCompletionSoundsEnabled = CAP_ROOM_COMPLETION_SOUNDS_ENABLED;
    if (gameFlagGetNibble(GAME_FLAG_MAIN_STREET_CUTSCENE_SEEN) == 0) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &_gDryfieldMainStreetInitialChaserHideCommand, ACTOR_COMMAND_MESSAGE_APPLY);
    }
    task->state++;
}

/// Keeps the initialized room entry task alive without changing its state.
static void _dryfieldMainStreetIdle(Task* task)
{
    // Retain the idle callback's 16-byte stack frame; no bytes are accessed.
    char retainedFrame[16];
}

void dryfieldMainStreetTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldMainStreetRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

/// Turns the player toward placement zero in the current area during a scripted event.
///
/// Steps yaw by 128 angle units per callback (4096 per turn), choosing the
/// shorter turn and snapping when at most one step remains. Ends on arrival,
/// a missing placement, or idle event state. Requires a live player task and
/// `GameActor` work, a player model with coordinate zero, and a target model
/// with at least two coordinates. Uses the stored local translations of the
/// player root and target coordinate one without composing their transforms.
static void _dryfieldMainStreetTurnPlayerTowardAreaActorTask(Task* task)
{
    enum { DRYFIELD_MAIN_STREET_PLAYER_YAW_STEP = 128 };

    Task*      playerTask;
    GameActor* playerActor;
    Enemy*     areaActor;
    GfxCoord*  playerCoord;
    GfxCoord*  targetCoord;
    s32        targetYaw;
    s32        yawDelta;
    s32        absoluteYawDelta;
    s32        yawStep;
    s32        wrappedYawDelta;

    playerTask  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerActor = playerTask->work;
    areaActor   = sceneFindEnemyByPlaceKey(gGameSession->location.area | (gGameSession->location.stage << 8));
    if ((areaActor != NULL) && (gGameSession->eventState != 0)) {
        playerCoord = playerTask->extra.tmd->coords;
        targetCoord = &areaActor->task->extra.tmd->coords[1];
        targetYaw   = ratan2(targetCoord->coord.t[0] - playerCoord->coord.t[0], targetCoord->coord.t[2] - playerCoord->coord.t[2]);
        yawDelta    = targetYaw - playerActor->rotation.vy;
        // Choose the shorter turn with one correction, retaining both half-turn ties.
        absoluteYawDelta = ABS(yawDelta);
        if (absoluteYawDelta >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
            wrappedYawDelta = yawDelta - ACTOR_TRANSFORM_ANGLE_TURN;
            if (yawDelta < 0) {
                wrappedYawDelta = yawDelta + ACTOR_TRANSFORM_ANGLE_TURN;
            }
            yawDelta = wrappedYawDelta;
        }
        absoluteYawDelta = ABS(yawDelta);
        if (absoluteYawDelta >= DRYFIELD_MAIN_STREET_PLAYER_YAW_STEP + 1) {
            yawStep = DRYFIELD_MAIN_STREET_PLAYER_YAW_STEP;
            if (yawDelta < 0) {
                yawStep = -DRYFIELD_MAIN_STREET_PLAYER_YAW_STEP;
            }
            // Keep the stored yaw's 16-bit wrap when applying the signed step.
            playerActor->rotation.vy = (s16)((u16)playerActor->rotation.vy + yawStep);
            return;
        }
        playerActor->rotation.vy = targetYaw;
    }
    taskKill(task);
}

/// Engages the first-visit battle and identifies the NMC photo in the live save.
///
/// Both the normal scene and its skip continuation call this callback. Its
/// script argument is forwarded to the battle API, which leaves it unread.
static void _dryfieldMainStreetBeginFirstVisitBattle(s32 unusedBattleArg)
{
    sceneEngageBattle(unusedBattleArg);
    itemSetIdentified(INVENTORY_COLLECTION_ID_NMC_PHOTO, 1);
}

/// Spawns and retains the first-visit scene's player head-aim task.
///
/// Requires the room resources and live player model while the task runs.
/// Each call replaces the retained handle, including on allocation failure;
/// the script must stop or forget the previous task before another spawn.
static void _dryfieldMainStreetSpawnPlayerHeadAimTask(void)
{
    enum { DRYFIELD_MAIN_STREET_HEAD_AIM_TASK_INDEX = 1 };

    _gDryfieldMainStreetPlayerHeadAimTask = taskSpawnFromTable(_gDryfieldMainStreetCutsceneTaskDescriptors, DRYFIELD_MAIN_STREET_HEAD_AIM_TASK_INDEX, 0, NULL);
}

/// Enables, disables or stops the retained player head-aim task.
///
/// Zero fades aiming out, one fades it in, and any other value kills the task
/// and clears its handle. A missing handle is a no-op. The script uses -1
/// to stop; the retained handle must refer to a live task when non-NULL.
static void _dryfieldMainStreetSetPlayerHeadAim(s32 command)
{
    Task* headAimTask = _gDryfieldMainStreetPlayerHeadAimTask;

    if (headAimTask == NULL) {
        return;
    }
    switch (command) {
        case DRYFIELD_MAIN_STREET_HEAD_AIM_DISABLE:
        case DRYFIELD_MAIN_STREET_HEAD_AIM_ENABLE:
            headAimTask->spawnArg1.value = command;
            break;
        default:
            taskKill(_gDryfieldMainStreetPlayerHeadAimTask);
            _gDryfieldMainStreetPlayerHeadAimTask = NULL;
            break;
    }
}

/// Steps the task's signed Q12 head-aim weight toward zero or full strength.
///
/// Starts with a weight in 0..ONE, takes one sixteenth-strength step and clamps
/// at the destination. Narrows the step to a signed halfword before comparing.
static inline void _dryfieldMainStreetStepHeadAimWeight(Task* task)
{
    enum { DRYFIELD_MAIN_STREET_HEAD_AIM_STEP = ONE / 16 };
    s16 nextBlendWeight;

    if (task->spawnArg1.value != 0) {
        nextBlendWeight     = task->killCountdown + DRYFIELD_MAIN_STREET_HEAD_AIM_STEP;
        task->killCountdown = nextBlendWeight;
        if (nextBlendWeight >= ONE + 1) {
            task->killCountdown = ONE;
        }
    } else {
        nextBlendWeight     = task->killCountdown - DRYFIELD_MAIN_STREET_HEAD_AIM_STEP;
        task->killCountdown = nextBlendWeight;
        if (nextBlendWeight < 0) {
            task->killCountdown = 0;
        }
    }
}

/// Blends the player's head toward placement zero in the current area.
///
/// State zero ramps the Q12 weight held in `killCountdown` by 256 per active
/// tick, clamped to 0..4096: zero `spawnArg1.value` fades out, nonzero fades in.
/// Both directions still apply head aiming, with yaw/pitch limits of 768/512
/// angle units (4096 per turn). Event-script suspension pauses all work,
/// including teardown; any other task state ends it when updates resume.
/// Requires a live placement-zero actor, a live player and both TMD rigs with
/// at least five coordinates in root-to-head order. Borrows the actor models.
static void _dryfieldMainStreetPlayerHeadAimTask(Task* task)
{
    enum {
        DRYFIELD_MAIN_STREET_HEAD_AIM_ACTIVE    = 0,
        DRYFIELD_MAIN_STREET_HEAD_AIM_MAX_YAW   = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 16,
        DRYFIELD_MAIN_STREET_HEAD_AIM_MAX_PITCH = ACTOR_TRANSFORM_ANGLE_TURN / 8,
    };
    Enemy* areaActor;

    if (gEvsPlaybackPaused == 0) {
        if (task->state == DRYFIELD_MAIN_STREET_HEAD_AIM_ACTIVE) {
            _dryfieldMainStreetStepHeadAimWeight(task);
            areaActor = sceneFindEnemyByPlaceKey(gGameSession->location.area | (gGameSession->location.stage << 8));
            animationAimHeadAtTask(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), areaActor->task,
                                   DRYFIELD_MAIN_STREET_HEAD_AIM_MAX_YAW, DRYFIELD_MAIN_STREET_HEAD_AIM_MAX_PITCH, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}

/// Clears the retained player head-aim handle before starting the first-visit scene.
///
/// Leaves any live task running independently. The caller passes zero through
/// the unused argument register.
static void _dryfieldMainStreetForgetPlayerHeadAimTask(s32 unusedArg)
{
    _gDryfieldMainStreetPlayerHeadAimTask = NULL;
}

/// Samples one smoke position from three successive LCG draws in X/Y/Z order.
///
/// Writes XYZ in -1185..-886, -1255..-656 and 9836..10535 game units, in the
/// input space of `GsWSMATRIX`. Borrows a writable vector for this call and
/// leaves its fourth halfword untouched. Advances the shared random sequence
/// exactly three times, using each unsigned state's upper halfword modulo
/// 300, 600 and 700 respectively; no output storage is retained.
static inline void _dryfieldMainStreetChooseSmokePosition(SVECTOR* position)
{
    position->vx = DRYFIELD_MAIN_STREET_RAND() % 300 - 0x4A1;
    position->vy = DRYFIELD_MAIN_STREET_RAND() % 600 - 0x4E7;
    position->vz = 0x2927 - DRYFIELD_MAIN_STREET_RAND() % 700;
}

void dryfieldMainStreetRoomEffectsTask(Task* task)
{
    enum {
        DRYFIELD_MAIN_STREET_ROOM_EFFECTS_INITIALIZE = 0,
        DRYFIELD_MAIN_STREET_ROOM_EFFECTS_ACTIVE     = 1,
        DRYFIELD_MAIN_STREET_SMOKE_VIEW              = 8,
        DRYFIELD_MAIN_STREET_INITIAL_PUFF_COUNT      = 48,
        DRYFIELD_MAIN_STREET_PUFF_RANDOM_MASK        = 0x10FF,
        // Size 256..511, period 3..4 ticks, drift speed 16 units/tick; add random bits.
        DRYFIELD_MAIN_STREET_INITIAL_PUFF_ARG = 0x103100,
        // Size 256..511, period 2..3 ticks, drift speed 8 units/tick; OR random bits.
        DRYFIELD_MAIN_STREET_STEADY_PUFF_ARG = 0x82100,
    };
    s32 puffIndex;

    if (task->state == DRYFIELD_MAIN_STREET_ROOM_EFFECTS_INITIALIZE) {
        gRoomEffectFlashId      = EFFECT_DRYFIELD_MAIN_STREET_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_DRYFIELD_MAIN_STREET_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_DRYFIELD_MAIN_STREET_SPARK_BURST;
        task->state             = DRYFIELD_MAIN_STREET_ROOM_EFFECTS_ACTIVE;
    }
    gRoomEffectState->roomEffectMode = _gDryfieldMainStreetViewEffectModes[(viewGetMappedIndex() & 0xFF) - 1];
    if ((viewGetMappedIndex() & 0xFF) == DRYFIELD_MAIN_STREET_SMOKE_VIEW) {
        if (task->spawnArg1.value != (viewGetMappedIndex() & 0xFF)) {
            // Seed the view with fast puffs, then replenish it on odd animation frames.
            for (puffIndex = 0; puffIndex < DRYFIELD_MAIN_STREET_INITIAL_PUFF_COUNT; puffIndex++) {
                _dryfieldMainStreetChooseSmokePosition(&_gDryfieldMainStreetSmokeSpawnPosition);
                effectSpawn(EFFECT_DRYFIELD_MAIN_STREET_SMOKE_PUFF, NULL, (DRYFIELD_MAIN_STREET_RAND() & DRYFIELD_MAIN_STREET_PUFF_RANDOM_MASK) + DRYFIELD_MAIN_STREET_INITIAL_PUFF_ARG,
                            &_gDryfieldMainStreetSmokeSpawnPosition);
            }
        } else if (gDisplayState.animFrame & 1) {
            _dryfieldMainStreetChooseSmokePosition(&_gDryfieldMainStreetSmokeSpawnPosition);
            effectSpawn(EFFECT_DRYFIELD_MAIN_STREET_SMOKE_PUFF, NULL, (DRYFIELD_MAIN_STREET_RAND() & DRYFIELD_MAIN_STREET_PUFF_RANDOM_MASK) | DRYFIELD_MAIN_STREET_STEADY_PUFF_ARG,
                        &_gDryfieldMainStreetSmokeSpawnPosition);
        }
    }
    task->spawnArg1.value = viewGetMappedIndex() & 0xFF;
}

#undef DRYFIELD_MAIN_STREET_RAND

#include "../../shared/main_street_puff_task.inc.c"

#include "../../shared/main_street_draw_puff.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void dryfieldMainStreetRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void dryfieldMainStreetRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void dryfieldMainStreetRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
