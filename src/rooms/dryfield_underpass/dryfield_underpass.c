#include "rooms/dryfield_underpass.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_400500.h"

#include "gameplay/display.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_variants.h"

static void _underpassSwitchTask(Task* task);
static s32  _underpassSwitchMsg(Task* unusedTask, s32 unusedMessageId, s32 switchId, s32 unusedSecondArg);

static s32 _underpassSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

static WorldCollisionGrid     _gDryfieldUnderpassCollisionGrid;
static WorldCollisionOccluder _gDryfieldUnderpassOccluders[3];
static WorldCollisionTrigger  _gDryfieldUnderpassPostEventViewBoundaries[16];
static WorldCollisionTrigger  _gDryfieldUnderpassPreEventViewBoundaries[2];
static WorldCollisionTrigger  _gDryfieldUnderpassActionTriggers[6];
static WorldCoordRoomLights   _gDryfieldUnderpassDefaultRoomLights;
static WorldCoordRoomLights   _gDryfieldUnderpassAlternateRoomLights;

static void _dryfieldUnderpassRefreshRoomVariant(void);

static s32  _dryfieldUnderpassRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedSecondArg);
static void _dryfieldUnderpassIdleTask(Task* unusedTask);
static s32  _dryfieldUnderpassHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static AnimationPackedPose _gDryfieldUnderpassAnimation00E64Bank1[10] = {
#include "assets/dryfield_underpass_animation_00E64_bank1.inc"
};

static AnimationPackedRotation _gDryfieldUnderpassAnimation00E64Bank4[126] = {
#include "assets/dryfield_underpass_animation_00E64_bank4.inc"
};

static AnimationRecord _gDryfieldUnderpassAnimation00E64Records[171] = {
#include "assets/dryfield_underpass_animation_00E64_records.inc"
};

static u16 _gDryfieldUnderpassAnimation00E64Indices[20] = {
#include "assets/dryfield_underpass_animation_00E64_indices.inc"
};

static AnimationSet _gDryfieldUnderpassAnimation00E64 = {
    _gDryfieldUnderpassAnimation00E64Records,
    _gDryfieldUnderpassAnimation00E64Indices,
    { NULL, _gDryfieldUnderpassAnimation00E64Bank1, NULL, NULL, _gDryfieldUnderpassAnimation00E64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldUnderpassAnimation01230Bank1[6] = {
#include "assets/dryfield_underpass_animation_01230_bank1.inc"
};

static AnimationPackedRotation _gDryfieldUnderpassAnimation01230Bank4[64] = {
#include "assets/dryfield_underpass_animation_01230_bank4.inc"
};

static AnimationRecord _gDryfieldUnderpassAnimation01230Records[141] = {
#include "assets/dryfield_underpass_animation_01230_records.inc"
};

static u16 _gDryfieldUnderpassAnimation01230Indices[20] = {
#include "assets/dryfield_underpass_animation_01230_indices.inc"
};

static AnimationSet _gDryfieldUnderpassAnimation01230 = {
    _gDryfieldUnderpassAnimation01230Records,
    _gDryfieldUnderpassAnimation01230Indices,
    { NULL, _gDryfieldUnderpassAnimation01230Bank1, NULL, NULL, _gDryfieldUnderpassAnimation01230Bank4, NULL, NULL, NULL },
};

/// Bodyless switch-prompt recipe at index zero, followed by the descriptor end marker.
///
/// The command handler supplies a flag nibble index and CAP command in the
/// two spawn words. Spawning reads the recipe synchronously; the room and CAP
/// resources must remain loaded until the prompt task completes.
static TaskDesc _gUnderpassSwitchTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _underpassSwitchTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Room receiver's five message bindings, followed by the lookup terminator.
///
/// Resolves room transitions, refuses key items, starts the Gray Stalker event
/// and handles switch and sound commands. The live room task borrows this table;
/// unsupported IDs return zero at `TASK_MESSAGE_TABLE_END`. Callback payloads
/// follow their individual message contracts and are consumed synchronously.
static TaskMessageEntry _gDryfieldUnderpassRoomMessages[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantUnderpassMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldUnderpassRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldUnderpassHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _underpassSwitchMsg },
    { ROOM_MESSAGE_SOUND, _underpassSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Two room-owned clips installed in the player's equipped-bank extension.
///
/// Entries become animation IDs 47 and 48, in that order; there is no terminator.
/// The copy request transfers only these two pointer words. Descriptors and
/// pose data remain borrowed from this room through scripted playback.
static AnimationSet* _gDryfieldUnderpassEventPlayerAnimationSets[] = {
    &_gDryfieldUnderpassAnimation00E64,
    &_gDryfieldUnderpassAnimation01230,
};

/// Installs both event clips without starting playback or changing the player's bank.
///
/// The loaded character/equipped-weapon bank must have a writable extension.
/// Dispatch borrows the request and source; the copied set pointers remain
/// borrowed from this room until playback no longer uses them.
static AnimationBankCopyRequest _gDryfieldUnderpassEventPlayerAnimationBankCopy = {
    .sets  = _gDryfieldUnderpassEventPlayerAnimationSets,
    .count = ARRAY_SIZE(_gDryfieldUnderpassEventPlayerAnimationSets),
};

/// Player animation slots and blend duration used by the Gray Stalker event.
enum {
    DRYFIELD_UNDERPASS_EVENT_PLAYER_ANIMATION_47 = ANIMATION_BANK_BASE_SET_COUNT,
    DRYFIELD_UNDERPASS_EVENT_PLAYER_ANIMATION_48 = ANIMATION_BANK_BASE_SET_COUNT + 1,
    DRYFIELD_UNDERPASS_EVENT_PLAYER_BLEND_FRAMES = 8,
};

/// Plays the first installed event clip after the event's half-rate request.
///
/// The event interpreter resolves `source.index` to the equipped weapon in a
/// temporary copy. Blends for eight normal-rate frames, keeps grid collision
/// enabled and takes scripted control. Playback restores the normal rate after
/// capturing the preceding pose. The room's clip data must remain loaded.
static AnimationPlayRequest _gDryfieldUnderpassEventPlayerAnimation47 = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_UNDERPASS_EVENT_PLAYER_ANIMATION_47,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_UNDERPASS_EVENT_PLAYER_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Plays the second installed event clip at the start of the Gray Stalker event.
///
/// The event interpreter resolves `source.index` to the equipped weapon in a
/// temporary copy. Blends for eight normal-rate frames, keeps grid collision
/// enabled and takes scripted control. The room's clip data must remain loaded.
static AnimationPlayRequest _gDryfieldUnderpassEventPlayerAnimation48 = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_UNDERPASS_EVENT_PLAYER_ANIMATION_48,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_UNDERPASS_EVENT_PLAYER_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Retained command-context tags and Gray Stalker sequence actions.
///
/// The Stalker's receiver ignores both tags; they are not the room's location.
enum {
    DRYFIELD_UNDERPASS_COMMAND_STAGE           = 5,
    DRYFIELD_UNDERPASS_COMMAND_AREA            = 11,
    DRYFIELD_UNDERPASS_STALKER_COMMAND_IGNORED = 0,
    DRYFIELD_UNDERPASS_STALKER_COMMAND_PREPARE = 1,
    DRYFIELD_UNDERPASS_STALKER_COMMAND_CRAWL   = 2,
    DRYFIELD_UNDERPASS_STALKER_COMMAND_REVEAL  = 3,
    DRYFIELD_UNDERPASS_STALKER_COMMAND_RELEASE = 4,
};

/// Unreferenced actor-command record with action zero and the event's context tags.
///
/// Action zero is ignored by the Gray Stalker receiver. No delivery or intended
/// use of this retained record is established.
static ActorCommand _gDryfieldUnderpassUnusedActorCommand0 = {
    .context = { .loc = { DRYFIELD_UNDERPASS_COMMAND_STAGE, DRYFIELD_UNDERPASS_COMMAND_AREA } },
    .command = DRYFIELD_UNDERPASS_STALKER_COMMAND_IGNORED,
};

/// Prepares the placed Gray Stalker for its ceiling sequence before the event.
///
/// Broadcast synchronously on entry to unseen placement variant 1. Command 1
/// restarts the sequence with a thirty-tick hide hold; the next actor tick places
/// it at the ceiling start and waits for the crawl command. Context tags are
/// ignored by this receiver, and no payload pointer is retained.
static ActorCommand _gDryfieldUnderpassPrepareGrayStalkerCommand = {
    .context = { .loc = { DRYFIELD_UNDERPASS_COMMAND_STAGE, DRYFIELD_UNDERPASS_COMMAND_AREA } },
    .command = DRYFIELD_UNDERPASS_STALKER_COMMAND_PREPARE,
};

/// Releases the Gray Stalker's first sequence wait to reveal it and start crawling.
///
/// The event sends command 2 to placement index zero after its first CAP cue.
/// The receiver copies the action word and ignores the context tags.
static ActorCommand _gDryfieldUnderpassCrawlGrayStalkerCommand = {
    .context = { .loc = { DRYFIELD_UNDERPASS_COMMAND_STAGE, DRYFIELD_UNDERPASS_COMMAND_AREA } },
    .command = DRYFIELD_UNDERPASS_STALKER_COMMAND_CRAWL,
};

/// Reveals the Gray Stalker at the ceiling crawl's stop point for its final wait.
///
/// The event sends command 3 to placement index zero after its third CAP cue.
/// The receiver copies the action word and ignores the context tags.
static ActorCommand _gDryfieldUnderpassRevealGrayStalkerCommand = {
    .context = { .loc = { DRYFIELD_UNDERPASS_COMMAND_STAGE, DRYFIELD_UNDERPASS_COMMAND_AREA } },
    .command = DRYFIELD_UNDERPASS_STALKER_COMMAND_REVEAL,
};

/// Releases the Gray Stalker's scripted sequence into its normal crawling behavior.
///
/// The event sends command 4 to placement index zero after positioning the
/// player. The receiver copies the action word and ignores the context tags.
static ActorCommand _gDryfieldUnderpassReleaseGrayStalkerCommand = {
    .context = { .loc = { DRYFIELD_UNDERPASS_COMMAND_STAGE, DRYFIELD_UNDERPASS_COMMAND_AREA } },
    .command = DRYFIELD_UNDERPASS_STALKER_COMMAND_RELEASE,
};

/// Unreferenced reset request for animation 1 in bank 1, with grid collision enabled.
///
/// No recipient or intended use of this retained request is established.
static AnimationPlayRequest _gDryfieldUnderpassUnusedAnimation1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player's world placement immediately before the event releases the Stalker.
///
/// Position is (16096, -1000, -3624) game units; yaw is a negative half turn
/// in the 4096-unit angle scale, with zero pitch and roll. The placement message
/// consumes both vectors synchronously and retains no pointer.
static ActorTransform _gDryfieldUnderpassEventPlayerBattleTransform = {
    .pos = { 16096, -1000, -3624, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 },
};

/// CAP slot, actor placement and final view selected by the Gray Stalker event.
enum {
    DRYFIELD_UNDERPASS_EVENT_CAP_DIRECT_VIEW_IDS     = 1,
    DRYFIELD_UNDERPASS_EVENT_CAP_COMMAND             = 4,
    DRYFIELD_UNDERPASS_EVENT_STALKER_PLACEMENT_INDEX = 0,
    DRYFIELD_UNDERPASS_EVENT_BATTLE_VIEW             = 2,
    DRYFIELD_UNDERPASS_EVENT_FINAL_WAIT_TICKS        = 3,
};

/// One-shot Gray Stalker introduction, synchronized to four CAP cues.
///
/// Installs player clips 47/48, starts CAP command 4, releases the Stalker's
/// crawl and reveal waits, places the player, releases the Stalker and engages
/// battle. Then refreshes the progress-selected room, yields, selects view 2
/// and restores player control while preserving its root offset. The 21 records
/// include `EVENT_SCRIPT_OPCODE_END`. The room, CAP resources, player bank and
/// placement-index-zero Stalker must remain loaded throughout execution.
static EvsCommand _gDryfieldUnderpassGrayStalkerEventScript[] = {
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = DRYFIELD_UNDERPASS_EVENT_CAP_DIRECT_VIEW_IDS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldUnderpassEventPlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_UNDERPASS_EVENT_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldUnderpassEventPlayerAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_UNDERPASS_EVENT_STALKER_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldUnderpassCrawlGrayStalkerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_SET_RATE }, { .value = ANIMATION_RATE_ONE / 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldUnderpassEventPlayerAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_UNDERPASS_EVENT_STALKER_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldUnderpassRevealGrayStalkerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldUnderpassEventPlayerBattleTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_UNDERPASS_EVENT_STALKER_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldUnderpassReleaseGrayStalkerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldUnderpassRefreshRoomVariant }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_UNDERPASS_EVENT_BATTLE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_UNDERPASS_EVENT_FINAL_WAIT_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Eight flare centres in signed game units local to the drawing task's coordinate.
///
/// Entries pair by index with `_gDryfieldUnderpassFlareViewMasks`; the drawer
/// rotates and translates each selected point through the task's composed frame.
static SVECTOR _gDryfieldUnderpassFlarePositions[8] = {
    { 0x4588, -3480, -1500, 0 },
    { 0x3B60, -3480, -6000, 0 },
    { 0x4588, -3480, -8700, 0 },
    { 0x34BC, -3480, -7200, 0 },
    { 9300, -3480, -9800, 0 },
    { 6200, -3480, -0x29CC, 0 },
    { 4850, -3420, -7200, 0 },
    { 500, -3480, -9700, 0 },
};

/// Logical-view bit masks paired with `_gDryfieldUnderpassFlarePositions`.
///
/// Bit n selects 1-based logical view n directly, without subtracting one.
/// View IDs 1..26 are tested as a 32-bit mask against each signed halfword;
/// these entries select only views 2..7, 9 and 11 and never set the sign bit.
static s16 _gDryfieldUnderpassFlareViewMasks[ARRAY_SIZE(_gDryfieldUnderpassFlarePositions)] = {
    (1 << 2) | (1 << 11),
    (1 << 3),
    (1 << 3) | (1 << 4) | (1 << 9),
    (1 << 4) | (1 << 9),
    (1 << 4),
    (1 << 6),
    (1 << 5),
    (1 << 5) | (1 << 7),
};

WorldCollisionRoomResources gDryfieldUnderpassRoomCollisionResources[DRYFIELD_UNDERPASS_ROOM_COUNT] = {
    { &_gDryfieldUnderpassCollisionGrid, _gDryfieldUnderpassPostEventViewBoundaries, _gDryfieldUnderpassActionTriggers, _gDryfieldUnderpassOccluders },
    { &_gDryfieldUnderpassCollisionGrid, _gDryfieldUnderpassPostEventViewBoundaries, _gDryfieldUnderpassActionTriggers, _gDryfieldUnderpassOccluders },
    { &_gDryfieldUnderpassCollisionGrid, _gDryfieldUnderpassPostEventViewBoundaries, _gDryfieldUnderpassActionTriggers, _gDryfieldUnderpassOccluders },
    { &_gDryfieldUnderpassCollisionGrid, _gDryfieldUnderpassPostEventViewBoundaries, _gDryfieldUnderpassActionTriggers, _gDryfieldUnderpassOccluders },
    { &_gDryfieldUnderpassCollisionGrid, _gDryfieldUnderpassPreEventViewBoundaries, _gDryfieldUnderpassActionTriggers, _gDryfieldUnderpassOccluders },
    { &_gDryfieldUnderpassCollisionGrid, _gDryfieldUnderpassPreEventViewBoundaries, _gDryfieldUnderpassActionTriggers, _gDryfieldUnderpassOccluders },
};

/// Room 2's eleven logical-view resource indices, followed by a retained zero byte.
static u8 _gDryfieldUnderpassRoom2ViewMap[12] = {
    1,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    26,
    0,
};

/// Room 3's eleven logical-view resource indices, followed by a retained zero byte.
static u8 _gDryfieldUnderpassRoom3ViewMap[12] = {
    1,
    11,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

/// Room 4's eleven logical-view resource indices, followed by a retained zero byte.
static u8 _gDryfieldUnderpassRoom4ViewMap[12] = {
    1,
    21,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    26,
    0,
};

/// Room 5's eleven logical-view resource indices, followed by a retained zero byte.
static u8 _gDryfieldUnderpassRoom5ViewMap[12] = {
    1,
    22,
    23,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

/// Room 6's eleven logical-view resource indices, followed by a retained zero byte.
static u8 _gDryfieldUnderpassRoom6ViewMap[12] = {
    1,
    24,
    25,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

u8* gDryfieldUnderpassRoomViewMaps[DRYFIELD_UNDERPASS_ROOM_COUNT] = {
    gViewIdentityMap,
    _gDryfieldUnderpassRoom2ViewMap,
    _gDryfieldUnderpassRoom3ViewMap,
    _gDryfieldUnderpassRoom4ViewMap,
    _gDryfieldUnderpassRoom5ViewMap,
    _gDryfieldUnderpassRoom6ViewMap,
};

ViewCount gDryfieldUnderpassRoomViewCounts[DRYFIELD_UNDERPASS_ROOM_COUNT] = {
    DRYFIELD_UNDERPASS_MAPPED_VIEW_COUNT,
    11,
    11,
    11,
    11,
    11,
};

WorldCoordRoomLighting gDryfieldUnderpassRoomLighting[DRYFIELD_UNDERPASS_ROOM_COUNT] = {
    { &_gDryfieldUnderpassDefaultRoomLights, NULL },
    { &_gDryfieldUnderpassAlternateRoomLights, NULL },
    { &_gDryfieldUnderpassDefaultRoomLights, NULL },
    { &_gDryfieldUnderpassAlternateRoomLights, NULL },
    { &_gDryfieldUnderpassDefaultRoomLights, NULL },
    { &_gDryfieldUnderpassDefaultRoomLights, NULL },
};

/// Warp slots and map-marker nibbles for the underpass's three exits.
enum {
    DRYFIELD_UNDERPASS_CELLAR_WARP                = 1,
    DRYFIELD_UNDERPASS_WATER_HOLE_WARP            = 2,
    DRYFIELD_UNDERPASS_GENERAL_STORE_WARP         = 3,
    DRYFIELD_UNDERPASS_CELLAR_MAP_FLAG            = 463,
    DRYFIELD_UNDERPASS_WATER_HOLE_MAP_FLAG        = 462,
    DRYFIELD_UNDERPASS_CELLAR_INITIAL_VIEW        = 6,
    DRYFIELD_UNDERPASS_WATER_HOLE_INITIAL_VIEW    = 2,
    DRYFIELD_UNDERPASS_GENERAL_STORE_INITIAL_VIEW = 7,
};

DirectionWarpEntry gDryfieldUnderpassWarpEntries[DRYFIELD_UNDERPASS_WARP_COUNT] = {
    {
        .player         = { { .word = 0 }, 5303, -997, -11412 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, 5303, -997, -11412 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_UNDERPASS, 8),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_UNDERPASS, 7),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = DRYFIELD_UNDERPASS_CELLAR_INITIAL_VIEW,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_UNDERPASS_CELLAR_MAP_FLAG,
    },
    {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 15665, -1000, -4216 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 15665, -1000, -4216 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_UNDERPASS, 6),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_UNDERPASS, 5),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = DRYFIELD_UNDERPASS_WATER_HOLE_INITIAL_VIEW,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_UNDERPASS_WATER_HOLE_MAP_FLAG,
    },
    {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 1212, -1000, -7543 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 1212, -1000, -7543 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_UNDERPASS, 1),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_UNDERPASS, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = DRYFIELD_UNDERPASS_GENERAL_STORE_INITIAL_VIEW,
        .flags          = DIRECTION_WARP_FLAG_FADE_DEPARTURE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Collision-grid face normals in room axes, with `ONE` units per unit length.
static SVECTOR _gDryfieldUnderpassCollisionGridNormals[12] = {
#include "assets/dryfield_underpass_collision_01EC4_normals.inc"
};

/// Collision-grid vertex positions in signed room-coordinate units.
static SVECTOR _gDryfieldUnderpassCollisionGridVerts[70] = {
#include "assets/dryfield_underpass_collision_01EC4_verts.inc"
};

/// Indexed collision triangles and quads, with room-local surface classes.
static WorldCollisionGridFace _gDryfieldUnderpassCollisionGridFaces[46] = {
#include "assets/dryfield_underpass_collision_01EC4_faces.inc"
};

/// Serialized cell face-index lists ending at `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gDryfieldUnderpassCollisionGridCells[346] = {
#include "assets/dryfield_underpass_collision_01EC4_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gDryfieldUnderpassCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define DRYFIELD_UNDERPASS_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldUnderpassCollisionGridCells[(halfwordOffset)])
/// X-major cell-list pointers for the grid's six-by-four XZ cells.
static s16* _gDryfieldUnderpassCollisionGridTable[24] = {
#include "assets/dryfield_underpass_collision_01EC4_table.inc"
};
#undef DRYFIELD_UNDERPASS_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh shared by all six underpass rooms.
///
/// The XZ index has six-by-four cells, each 4000 game units wide, with origin
/// (-3000, -13000). Gameplay binds the borrowed view coordinate on activation;
/// the descriptor and all mesh pools remain writable while the room is loaded.
static WorldCollisionGrid _gDryfieldUnderpassCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldUnderpassCollisionGridNormals,
    .vertices    = _gDryfieldUnderpassCollisionGridVerts,
    .faces       = _gDryfieldUnderpassCollisionGridFaces,
    .cellFaceIds = _gDryfieldUnderpassCollisionGridTable,
    .xBias       = 3000,
    .zBias       = 13000,
    .cellCountX  = 6,
    .cellCountZ  = 4,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldUnderpassCollisionGridFaces),
};

ViewCamera gDryfieldUnderpassViewCameras[DRYFIELD_UNDERPASS_MAPPED_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -7864, 0x6671, 5100 } }, 329 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { -4007, 0, 848 }, { -132, 4045, -626 }, { -837, -639, -3958 } }, { -0x42CB, 1690, 3166 } }, 230 },
    { { { { 998, 0, -3972 }, { 996, 3964, 250 }, { 3845, -1027, 966 } }, { -6656, 1455, 9182 } }, 230 },
    { { { { 994, 0, 3973 }, { -803, 4011, 201 }, { -3891, -828, 974 } }, { -0x2AAE, 1498, 9252 } }, 230 },
    { { { { -3946, 0, 1096 }, { -240, 3996, -865 }, { -1069, -898, -3850 } }, { -6279, 1492, 7705 } }, 230 },
    { { { { 951, 0, 3983 }, { -763, 4020, 182 }, { -3910, -784, 934 } }, { -5505, 1532, 9129 } }, 230 },
    { { { { 1678, 0, 3736 }, { 2611, 2928, -1173 }, { -2671, 2863, 1200 } }, { -444, 3936, 9337 } }, 230 },
    { { { { 1312, 0, -3879 }, { 1085, 3932, 367 }, { 3724, -1146, 1260 } }, { -0x293A, 1292, 9364 } }, 230 },
    { { { { 3773, 0, 1592 }, { 1116, 2920, -2645 }, { -1135, 2871, 2690 } }, { -0x43B0, 3973, 2327 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { -4007, 0, 848 }, { -132, 4045, -626 }, { -837, -639, -3958 } }, { -0x42CB, 1690, 3166 } }, 230 },
    { { { { 998, 0, -3972 }, { 996, 3964, 250 }, { 3845, -1027, 966 } }, { -6656, 1455, 9182 } }, 230 },
    { { { { 994, 0, 3973 }, { -803, 4011, 201 }, { -3891, -828, 974 } }, { -0x2AAE, 1498, 9252 } }, 230 },
    { { { { -3946, 0, 1096 }, { -240, 3996, -865 }, { -1069, -898, -3850 } }, { -6279, 1492, 7705 } }, 230 },
    { { { { 951, 0, 3983 }, { -763, 4020, 182 }, { -3910, -784, 934 } }, { -5505, 1532, 9129 } }, 230 },
    { { { { 1678, 0, 3736 }, { 2611, 2928, -1173 }, { -2671, 2863, 1200 } }, { -444, 3936, 9337 } }, 230 },
    { { { { 1312, 0, -3879 }, { 1085, 3932, 367 }, { 3724, -1146, 1260 } }, { -0x293A, 1292, 9364 } }, 230 },
    { { { { 3773, 0, 1592 }, { 1116, 2920, -2645 }, { -1135, 2871, 2690 } }, { -0x43B0, 3973, 2327 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { 4042, 0, 659 }, { 359, 3434, -2202 }, { -553, 2231, 3389 } }, { -0x421F, 3970, 8993 } }, 230 },
    { { { { 4075, 0, 405 }, { 186, 3640, -1868 }, { -360, 1877, 3622 } }, { -0x4162, 3823, 5117 } }, 230 },
    { { { { 4042, 0, 659 }, { 359, 3434, -2202 }, { -553, 2231, 3389 } }, { -0x421F, 3970, 8993 } }, 230 },
    { { { { 4075, 0, 405 }, { 186, 3640, -1868 }, { -360, 1877, 3622 } }, { -0x4162, 3823, 5117 } }, 230 },
    { { { { -4047, 0, 631 }, { -451, 2858, -2898 }, { -440, -2933, -2824 } }, { -0x4229, 2000, 6853 } }, 348 },
};

/// Placement variant 1's Gray Stalker resource, followed by its ending record.
///
/// Placement entry 5 selects file 400500 (group base 40, file number 5, low
/// index zero) and descriptor zero. The table borrows the actor overlay's
/// descriptor and remains live while the underpass's placements are active.
static AreaResource _gDryfieldUnderpassGrayStalkerResources[2] = {
    { .entryId = 5, .fileNumber = 5, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor400500TaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldUnderpassAreaVariants[DRYFIELD_UNDERPASS_VARIANT_SLOT_COUNT] = {
    { NULL, NULL },
    { gMapDryfieldUnderpassGrayStalkerPlacements, _gDryfieldUnderpassGrayStalkerResources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

/// Mapped view 1's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 2's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 3's sixteen textured rectangles, positioned in view-centred pixels.
static SpriteSource _gDryfieldUnderpassView3SpriteSources[16] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 48, -120, 1016, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 80, 964, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, -120, 1000, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, -120, 1000, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, 24, 975, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, 24, 975, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 88, -24, 975, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -24, 975, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -64, 975, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, -64, 975, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 80, 975, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 80, 975, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, 32, 993, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, -64, 1005, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -24, 1032, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 8, 1014, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
};

/// Mapped view 3's complete sprite range over decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldUnderpassView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 4's eleven textured rectangles, positioned in view-centred pixels.
static SpriteSource _gDryfieldUnderpassView4SpriteSources[11] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 96, 2125, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -24, 2129, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, 56, 1860, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, 48, 2000, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -16, 16, 2058, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 48 } }, -96, -72, 2125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -96, -120, 2125, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 120 } }, -96, -24, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, 40, 2125, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 80 } }, -160, -120, 2125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, -40, 2125, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
};

/// Mapped view 4's complete sprite range over decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldUnderpassView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 5's ten textured rectangles, positioned in view-centred pixels.
static SpriteSource _gDryfieldUnderpassView5SpriteSources[10] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -120, 910, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -48, 913, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -96, 995, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, 8, 901, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 56, 896, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -160, -120, 897, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, -48, 897, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, 8, 897, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 56, 897, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 897, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
};

/// Mapped view 5's complete sprite range over decoded-image background strips.
///
/// Retains a zero-count range at the source end. The full range stores 1 in
/// the first serialized field_6 byte; that byte's role remains unproven.
static SpriteBatch _gDryfieldUnderpassView5SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldUnderpassView5SpriteSources), 0, 0, { 1, 0 } },
    { ARRAY_SIZE(_gDryfieldUnderpassView5SpriteSources), 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 6's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView6SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 7's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView7SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 8's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView8SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture depth selector and neutral colour-modulation scale for view sprites.
enum {
    DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT     = 1,
    DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY = 128,
};

/// Mapped view 9's 11 textured rectangles in view-centred pixels.
///
/// Dimensions are pixels and UVs are texels in the eight-bit pages; RGB is
/// neutral modulation. Depths are unscaled OT sorting values. Batch ranges index
/// source elements without a sentinel, and cached packets snapshot geometry.
static SpriteSource _gDryfieldUnderpassView9SpriteSources[11] = {
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -72, -112, 1325, { .fields = { 40, 192 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 56 } }, -120, -120, 1325, { .fields = { 24, 136 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 56 } }, -160, -120, 1325, { .fields = { 32, 80 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -56, -16, 1197, { .fields = { 0, 0 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -56, -64, 1250, { .fields = { 8, 192 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -56, 24, 1162, { .fields = { 8, 72 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -56, 56, 1133, { .fields = { 8, 40 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 80 } }, -112, -64, 1175, { .fields = { 72, 176 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 80 } }, -160, -64, 1175, { .fields = { 32, 0 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 88 } }, -112, 16, 1125, { .fields = { 72, 88 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 88 } }, -160, 16, 1125, { .fields = { 80, 0 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
};

/// Mapped view 9's full sprite range over decoded-image background strips.
///
/// Retains an interior zero-count range with serialized `field_6` bytes
/// {1, 0}; their role is unproven. Zero-count ranges do not end the list.
static SpriteBatch _gDryfieldUnderpassView9SpriteBatches[4] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldUnderpassView9SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 10's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView10SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 11's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView11SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 12's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView12SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 13's 16 textured rectangles in view-centred pixels.
///
/// Dimensions are pixels and UVs are texels in the eight-bit pages; RGB is
/// neutral modulation. Depths are unscaled OT sorting values. Batch ranges index
/// source elements without a sentinel, and cached packets snapshot geometry.
static SpriteSource _gDryfieldUnderpassView13SpriteSources[16] = {
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 56 } }, 48, -120, 1016, { .fields = { 88, 0 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 72, 80, 964, { .fields = { 32, 80 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 56 } }, 88, -120, 1000, { .fields = { 88, 56 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, 128, -120, 1000, { .fields = { 96, 168 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 56 } }, 88, 24, 975, { .fields = { 56, 112 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, 128, 24, 975, { .fields = { 96, 112 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, 88, -24, 975, { .fields = { 56, 168 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, 128, -24, 975, { .fields = { 56, 48 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 128, -64, 975, { .fields = { 64, 216 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 88, -64, 975, { .fields = { 24, 0 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 96, 80, 975, { .fields = { 32, 216 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 128, 80, 975, { .fields = { 24, 40 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, 64, 32, 993, { .fields = { 64, 0 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 56, -64, 1005, { .fields = { 24, 120 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 56, -24, 1032, { .fields = { 96, 224 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, 56, 8, 1014, { .fields = { 24, 160 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
};

/// Mapped view 13's full sprite range over decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView13SpriteBatches[3] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldUnderpassView13SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 14's 11 textured rectangles in view-centred pixels.
///
/// Dimensions are pixels and UVs are texels in the eight-bit pages; RGB is
/// neutral modulation. Depths are unscaled OT sorting values. Batch ranges index
/// source elements without a sentinel, and cached packets snapshot geometry.
static SpriteSource _gDryfieldUnderpassView14SpriteSources[11] = {
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -96, 96, 2125, { .fields = { 120, 248 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -16, -24, 2129, { .fields = { 24, 80 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 0, 56, 1860, { .fields = { 112, 112 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -16, 48, 2000, { .fields = { 8, 80 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, -16, 16, 2058, { .fields = { 112, 80 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 88, 48 } }, -96, -72, 2125, { .fields = { 120, 200 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 48 } }, -96, -120, 2125, { .fields = { 80, 200 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 80, 120 } }, -96, -24, 2125, { .fields = { 48, 0 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 80 } }, -160, 40, 2125, { .fields = { 64, 120 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 80 } }, -160, -120, 2125, { .fields = { 112, 0 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 80 } }, -160, -40, 2125, { .fields = { 0, 120 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
};

/// Mapped view 14's full sprite range over decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView14SpriteBatches[3] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldUnderpassView14SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 15's 10 textured rectangles in view-centred pixels.
///
/// Dimensions are pixels and UVs are texels in the eight-bit pages; RGB is
/// neutral modulation. Depths are unscaled OT sorting values. Batch ranges index
/// source elements without a sentinel, and cached packets snapshot geometry.
static SpriteSource _gDryfieldUnderpassView15SpriteSources[10] = {
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -104, -120, 910, { .fields = { 72, 72 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -112, -48, 913, { .fields = { 112, 128 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -104, -96, 995, { .fields = { 112, 184 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -120, 8, 901, { .fields = { 96, 176 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -128, 56, 896, { .fields = { 80, 216 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 72 } }, -160, -120, 897, { .fields = { 72, 0 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 56 } }, -160, -48, 897, { .fields = { 80, 72 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, -160, 8, 897, { .fields = { 72, 128 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -160, 56, 897, { .fields = { 64, 176 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -160, 96, 897, { .fields = { 96, 232 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
};

/// Mapped view 15's full sprite range over decoded-image background strips.
///
/// Retains a zero-count range at the source end and serialized `field_6`
/// bytes {1, 0} on the full range; those bytes' role is unproven.
static SpriteBatch _gDryfieldUnderpassView15SpriteBatches[4] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldUnderpassView15SpriteSources), .field_6 = { 1, 0 } },
    { .firstSprite = ARRAY_SIZE(_gDryfieldUnderpassView15SpriteSources), .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 16's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView16SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 17's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView17SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 18's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView18SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 19's 11 textured rectangles in view-centred pixels.
///
/// Dimensions are pixels and UVs are texels in the eight-bit pages; RGB is
/// neutral modulation. Depths are unscaled OT sorting values. Batch ranges index
/// source elements without a sentinel, and cached packets snapshot geometry.
static SpriteSource _gDryfieldUnderpassView19SpriteSources[11] = {
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -72, -112, 1325, { .fields = { 40, 192 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 56 } }, -120, -120, 1325, { .fields = { 24, 136 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 56 } }, -160, -120, 1325, { .fields = { 32, 80 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -56, -16, 1197, { .fields = { 0, 0 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -56, -64, 1250, { .fields = { 8, 192 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -56, 24, 1162, { .fields = { 8, 72 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -56, 56, 1133, { .fields = { 8, 40 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 80 } }, -112, -64, 1175, { .fields = { 72, 176 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 80 } }, -160, -64, 1175, { .fields = { 32, 0 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 88 } }, -112, 16, 1125, { .fields = { 72, 88 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
    { getTPage(DRYFIELD_UNDERPASS_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 88 } }, -160, 16, 1125, { .fields = { 80, 0 } }, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, DRYFIELD_UNDERPASS_SPRITE_NEUTRAL_INTENSITY, 0 },
};

/// Mapped view 19's full sprite range over decoded-image background strips.
///
/// Retains an interior zero-count range with serialized `field_6` bytes
/// {1, 0}; their role is unproven. Zero-count ranges do not end the list.
static SpriteBatch _gDryfieldUnderpassView19SpriteBatches[4] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldUnderpassView19SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 20's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView20SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 21's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView21SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 22's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView22SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 23's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView23SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 24's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView24SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 25's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView25SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 26's empty sprite list, retaining decoded-image background strips.
static SpriteBatch _gDryfieldUnderpassView26SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gDryfieldUnderpassSpriteViews[DRYFIELD_UNDERPASS_MAPPED_VIEW_COUNT] = {
    { { .empty = _gDryfieldUnderpassView1SpriteBatches }, _gDryfieldUnderpassView1SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView2SpriteBatches }, _gDryfieldUnderpassView2SpriteBatches, NULL },
    { { .elements = _gDryfieldUnderpassView3SpriteSources }, _gDryfieldUnderpassView3SpriteBatches, NULL },
    { { .elements = _gDryfieldUnderpassView4SpriteSources }, _gDryfieldUnderpassView4SpriteBatches, NULL },
    { { .elements = _gDryfieldUnderpassView5SpriteSources }, _gDryfieldUnderpassView5SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView6SpriteBatches }, _gDryfieldUnderpassView6SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView7SpriteBatches }, _gDryfieldUnderpassView7SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView8SpriteBatches }, _gDryfieldUnderpassView8SpriteBatches, NULL },
    { { .elements = _gDryfieldUnderpassView9SpriteSources }, _gDryfieldUnderpassView9SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView10SpriteBatches }, _gDryfieldUnderpassView10SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView11SpriteBatches }, _gDryfieldUnderpassView11SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView12SpriteBatches }, _gDryfieldUnderpassView12SpriteBatches, NULL },
    { { .elements = _gDryfieldUnderpassView13SpriteSources }, _gDryfieldUnderpassView13SpriteBatches, NULL },
    { { .elements = _gDryfieldUnderpassView14SpriteSources }, _gDryfieldUnderpassView14SpriteBatches, NULL },
    { { .elements = _gDryfieldUnderpassView15SpriteSources }, _gDryfieldUnderpassView15SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView16SpriteBatches }, _gDryfieldUnderpassView16SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView17SpriteBatches }, _gDryfieldUnderpassView17SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView18SpriteBatches }, _gDryfieldUnderpassView18SpriteBatches, NULL },
    { { .elements = _gDryfieldUnderpassView19SpriteSources }, _gDryfieldUnderpassView19SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView20SpriteBatches }, _gDryfieldUnderpassView20SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView21SpriteBatches }, _gDryfieldUnderpassView21SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView22SpriteBatches }, _gDryfieldUnderpassView22SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView23SpriteBatches }, _gDryfieldUnderpassView23SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView24SpriteBatches }, _gDryfieldUnderpassView24SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView25SpriteBatches }, _gDryfieldUnderpassView25SpriteBatches, NULL },
    { { .empty = _gDryfieldUnderpassView26SpriteBatches }, _gDryfieldUnderpassView26SpriteBatches, NULL },
};

/// Sixteen directed view boundaries shared by post-event rooms 1..4.
///
/// Source/destination IDs use logical views 2..10. Positions and radii are
/// room game units and normals use 4096 per unit. Gameplay binds their view
/// coordinate and mutable links; the final live boundary carries LAST.
static WorldCollisionTrigger _gDryfieldUnderpassPostEventViewBoundaries[16] = {
    { NULL, NULL, NULL, { 0x4070, -2560, -5280, 0 }, { { -1616, -1888, 0, 0 }, { 1616, -1888, 0, 0 }, { -1616, 1888, 0, 0 }, { 1616, 1888, 0, 0 } }, { 0, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40C0, -2544, -5408, 0 }, { { 1616, -1840, 0, 0 }, { -1616, -1840, 0, 0 }, { 1616, 1840, 0, 0 }, { -1616, 1840, 0, 0 } }, { 0, 0, 4105, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3A3F, -2720, -8402, 0 }, { { 65, -1824, -1727, 0 }, { -64, -1824, 1728, 0 }, { 65, 1824, -1727, 0 }, { -64, 1824, 1728, 0 } }, { 4101, 0, 151, 0 }, { 0, 0, 4096, 0 }, 2508, 0, 3, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3ABE, -2736, -8307, 0 }, { { -81, -1808, 1758, 0 }, { 81, -1808, -1757, 0 }, { -81, 1808, 1758, 0 }, { 81, 1808, -1757, 0 } }, { -4096, 0, -190, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 9, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9023, -2688, -8449, 0 }, { { 0, -1824, 1615, 0 }, { 1, -1824, -1616, 0 }, { 0, 1824, 1615, 0 }, { 1, 1824, -1616, 0 } }, { -4102, 0, -2, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8799, -2736, -8577, 0 }, { { -79, -1808, -1614, 0 }, { 78, -1808, 1613, 0 }, { -79, 1808, -1614, 0 }, { 78, 1808, 1613, 0 } }, { 4091, 0, -200, 0 }, { 0, 0, 4096, 0 }, 2415, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5214, -2656, -0x27A2, 0 }, { { 1616, -1824, 1, 0 }, { -1615, -1824, 0, 0 }, { 1616, 1824, 1, 0 }, { -1615, 1824, 0, 0 } }, { -2, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5150, -2624, -9954, 0 }, { { -1615, -1888, 0, 0 }, { 1616, -1888, 1, 0 }, { -1615, 1888, 0, 0 }, { 1616, 1888, 1, 0 } }, { 0, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3230, -2624, -8482, 0 }, { { -79, -1792, -2412, 0 }, { 79, -1792, 2413, 0 }, { -79, 1792, -2412, 0 }, { 79, 1792, 2413, 0 } }, { 4094, 0, -135, 0 }, { 0, 0, 4096, 0 }, 2996, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3422, -2624, -8513, 0 }, { { 63, -1856, 2224, 0 }, { -63, -1856, -2223, 0 }, { 63, 1856, 2224, 0 }, { -63, 1856, -2223, 0 } }, { -4094, 0, 115, 0 }, { 0, 0, 4096, 0 }, 2896, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -960, -2592, -8577, 0 }, { { 306, -1856, 1579, 0 }, { -322, -1856, -1589, 0 }, { 306, 1856, 1579, 0 }, { -322, 1856, -1589, 0 } }, { -4028, 0, 798, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1056, -2528, -8385, 0 }, { { -322, -1856, -1592, 0 }, { 308, -1856, 1578, 0 }, { -322, 1856, -1592, 0 }, { 308, 1856, 1578, 0 } }, { 4028, 0, -802, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3130, -2592, -8513, 0 }, { { 17, -1824, 1905, 0 }, { -16, -1824, -1904, 0 }, { 17, 1824, 1905, 0 }, { -16, 1824, -1904, 0 } }, { -4104, 0, 35, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 4, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3061, -2528, -8577, 0 }, { { 1, -1824, -1615, 0 }, { 0, -1824, 1616, 0 }, { 1, 1824, -1615, 0 }, { 0, 1824, 1616, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 9, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40C0, -2624, -1029, 0 }, { { 1577, -1840, 182, 0 }, { -1630, -1840, -237, 0 }, { 1577, 1840, 182, 0 }, { -1630, 1840, -237, 0 } }, { -534, 0, 4073, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 10, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40F1, -2624, -962, 0 }, { { -1854, -1840, -209, 0 }, { 1834, -1840, 184, 0 }, { -1854, 1840, -209, 0 }, { 1834, 1840, 184, 0 } }, { 434, 0, -4078, 0 }, { 0, 0, 4096, 0 }, 2610, 0, 2, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Opposing view-2/view-3 boundaries shared by pre-event rooms 5 and 6.
///
/// Positions and radii are room game units and normals use 4096 per unit.
/// Gameplay binds their view coordinate and mutable links; the second live
/// boundary carries LAST and is included in setup and traversal.
static WorldCollisionTrigger _gDryfieldUnderpassPreEventViewBoundaries[2] = {
    { NULL, NULL, NULL, { 0x4090, -2560, -2752, 0 }, { { 2096, -1888, 0, 0 }, { -2096, -1888, 0, 0 }, { 2096, 1888, 0, 0 }, { -2096, 1888, 0, 0 } }, { 0, 0, 4101, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4090, -2544, -2656, 0 }, { { -2144, -1840, 0, 0 }, { 2144, -1840, 0, 0 }, { -2144, 1840, 0, 0 }, { 2144, 1840, 0, 0 } }, { 0, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Destination warp slots and room commands encoded by the action quads.
enum {
    DRYFIELD_UNDERPASS_CELLAR_ARRIVAL_WARP        = 1,
    DRYFIELD_UNDERPASS_WATER_HOLE_ARRIVAL_WARP    = 2,
    DRYFIELD_UNDERPASS_GENERAL_STORE_ARRIVAL_WARP = 3,
    DRYFIELD_UNDERPASS_SWITCH_COMMAND_FIRST       = 1,
    DRYFIELD_UNDERPASS_SWITCH_COMMAND_SECOND      = 2,
    DRYFIELD_UNDERPASS_ACTION_START_STALKER_EVENT = 1,
};

/// Six action quads shared by all underpass rooms, including the final automatic event quad.
///
/// The first three warp to the cellar, general store and water hole, using
/// departure slots 1, 3 and 2 respectively. The next two send room commands 2
/// and 1 for the switches. The final quad automatically sends room action 1;
/// its handler gates the one-shot Stalker event by variant and seen flag.
/// Positions/radii use room game units and normals use 4096 per unit. Gameplay
/// binds and mutates the records; LAST belongs to the final live quad.
static WorldCollisionTrigger _gDryfieldUnderpassActionTriggers[6] = {
    { NULL, NULL, NULL, { 5184, -1088, -0x2C70, 0 }, { { -1024, 0, -432, 0 }, { 1024, 0, -432, 0 }, { -1024, 0, 432, 0 }, { 1024, 0, 432, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, ONE, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_CELLAR, (DRYFIELD_UNDERPASS_CELLAR_WARP << 4) | DRYFIELD_UNDERPASS_CELLAR_ARRIVAL_WARP, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1280, -1056, -7456, 0 }, { { -896, 0, -432, 0 }, { 896, 0, -432, 0 }, { -896, 0, 432, 0 }, { 896, 0, 432, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -ONE, 0 }, 993, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_GENERAL_STORE, (DRYFIELD_UNDERPASS_GENERAL_STORE_WARP << 4) | DRYFIELD_UNDERPASS_GENERAL_STORE_ARRIVAL_WARP, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3C00, -1088, -4096, 0 }, { { 368, 0, -928, 0 }, { 368, 0, 928, 0 }, { -368, 0, -928, 0 }, { -368, 0, 928, 0 } }, { 0, 4101, 0, 0 }, { ONE, 0, 0, 0 }, 997, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_WATER_HOLE, (DRYFIELD_UNDERPASS_WATER_HOLE_WARP << 4) | DRYFIELD_UNDERPASS_WATER_HOLE_ARRIVAL_WARP, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1522, -1056, -8433, 0 }, { { -431, 0, -831, 0 }, { 433, 0, -832, 0 }, { -431, 0, 832, 0 }, { 432, 0, 832, 0 } }, { 0, 4105, 0, 0 }, { ONE, 0, 0, 0 }, 936, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_UNDERPASS_SWITCH_COMMAND_SECOND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x40E0, -1056, -576, 0 }, { { -1024, 0, -304, 0 }, { 1024, 0, -304, 0 }, { -1024, 0, 304, 0 }, { 1024, 0, 304, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -ONE, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_UNDERPASS_SWITCH_COMMAND_FIRST, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x40C0, -1056, -6240, 0 }, { { 1808, 0, -384, 0 }, { 1808, 0, 384, 0 }, { -1808, 0, -384, 0 }, { -1808, 0, 384, 0 } }, { 0, 4099, 0, 0 }, { 3166, 0, -2598, 0 }, 1846, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_UNDERPASS_ACTION_START_STALKER_EVENT, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Three line-of-sight blocking quads shared by all six underpass rooms.
///
/// Origins and corner offsets use room game units and normals use 4096 per unit.
/// Gameplay mutates their links and enable state while the room remains loaded;
/// LAST marks the third live quad. Retained low flag bit 0 has no proven meaning.
static WorldCollisionOccluder _gDryfieldUnderpassOccluders[3] = {
    { NULL, NULL, { 1184, -2304, -0x2840, 0 }, { { -2912, -3328, 0, 0 }, { 2912, -3328, 0, 0 }, { -2912, 3328, 0, 0 }, { 2912, 3328, 0, 0 } }, { 0, 0, -4106, 0 }, 4404, 1, 0 },
    { NULL, NULL, { 0x2FB0, -2240, -0x2820, 0 }, { { -5840, -3264, 0, 0 }, { 5840, -3264, 0, 0 }, { -5840, 3264, 0, 0 }, { 5840, 3264, 0, 0 } }, { 0, 0, -4111, 0 }, 6675, 1, 0 },
    { NULL, NULL, { 8992, -3328, -4416, 0 }, { { -5840, -2848, 2496, 0 }, { 5840, -2848, -2496, 0 }, { -5840, 2848, 2496, 0 }, { 5840, 2848, -2496, 0 } }, { -1614, 0, -3777, 0 }, 6945, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Nine room-space point lights shared by rooms 1, 3, 5 and 6.
///
/// Positions and falloff radii use game units; colours use `ONE` per unit
/// intensity. Every point admits all logical views. Gameplay parents and
/// composes the writable transforms and updates attenuation during shading.
static WorldCoordPointLight _gDryfieldUnderpassDefaultPointLights[9] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3214, -1973, -3920 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1966, 2621, 3276 }, { 0, 0 } }, 2500, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x44C0, -3483, -1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3C28, -3483, -6000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x44C0, -3483, -8500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9300, -3483, -9600 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x34BC, -3483, -7400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6200, -3483, -0x2904 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 500, -3483, -9500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4850, -3423, -7400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
};

/// Point-light collection shared by rooms 1, 3, 5 and 6, without directional or cone lights.
static WorldCoordRoomLights _gDryfieldUnderpassDefaultRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldUnderpassDefaultPointLights),
    .pointLights           = _gDryfieldUnderpassDefaultPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Six room-space point lights shared by rooms 2 and 4.
///
/// Positions and falloff radii use game units; colours use `ONE` per unit
/// intensity. Every point admits all logical views. Gameplay parents and
/// composes the writable transforms and updates attenuation during shading.
static WorldCoordPointLight _gDryfieldUnderpassAlternatePointLights[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x4268, -2483, -2500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 500, -2483, -8500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3F48, -2483, -8000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2AF8, -2483, -8300 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5850, -2423, -9100 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x314C, -1973, -3920 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, ONE, ONE }, { 0, 0 } }, 0, 8000 },
};

/// Point-light collection shared by rooms 2 and 4, without directional or cone lights.
static WorldCoordRoomLights _gDryfieldUnderpassAlternateRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldUnderpassAlternatePointLights),
    .pointLights           = _gDryfieldUnderpassAlternatePointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Cue-2 footstep requests in the loaded type-1 sound bank; cue 1 uses the next entry.
enum {
    DRYFIELD_UNDERPASS_SURFACE_CLASS4_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x39,
    DRYFIELD_UNDERPASS_SURFACE_CLASS4_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3B,
    DRYFIELD_UNDERPASS_SURFACE_CLASS3_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_UNDERPASS_SURFACE_CLASS3_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
};

/// Surface class 4's loaded-bank walk/run cues, with scripted jumps using the walk pair.
static WorldCollisionFootstepSounds _gDryfieldUnderpassSurfaceClass4FootstepSounds = {
    .walk         = DRYFIELD_UNDERPASS_SURFACE_CLASS4_WALK_SOUND,
    .run          = DRYFIELD_UNDERPASS_SURFACE_CLASS4_RUN_SOUND,
    .scriptedJump = DRYFIELD_UNDERPASS_SURFACE_CLASS4_WALK_SOUND,
};

/// Surface class 3's loaded-bank walk/run cues, with scripted jumps using the walk pair.
static WorldCollisionFootstepSounds _gDryfieldUnderpassSurfaceClass3FootstepSounds = {
    .walk         = DRYFIELD_UNDERPASS_SURFACE_CLASS3_WALK_SOUND,
    .run          = DRYFIELD_UNDERPASS_SURFACE_CLASS3_RUN_SOUND,
    .scriptedJump = DRYFIELD_UNDERPASS_SURFACE_CLASS3_WALK_SOUND,
};

/// Silent blocking surface properties shared by classes 0, 1 and 5..7.
static WorldCollisionSurfaceProperties _gDryfieldUnderpassDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface class 4's blocking properties and footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldUnderpassSurfaceClass4Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldUnderpassSurfaceClass4FootstepSounds,
};

/// Surface class 2's separate silent blocking record.
static WorldCollisionSurfaceProperties _gDryfieldUnderpassSurfaceClass2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface class 3's blocking properties and footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldUnderpassSurfaceClass3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldUnderpassSurfaceClass3FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldUnderpassSurfacePropertiesByClass[DRYFIELD_UNDERPASS_SURFACE_CLASS_COUNT] = {
    &_gDryfieldUnderpassDefaultSurfaceProperties,
    &_gDryfieldUnderpassDefaultSurfaceProperties,
    &_gDryfieldUnderpassSurfaceClass2Properties,
    &_gDryfieldUnderpassSurfaceClass3Properties,
    &_gDryfieldUnderpassSurfaceClass4Properties,
    &_gDryfieldUnderpassDefaultSurfaceProperties,
    &_gDryfieldUnderpassDefaultSurfaceProperties,
    &_gDryfieldUnderpassDefaultSurfaceProperties,
};

static void _dryfieldUnderpassInitializeRoomTask(Task* task);

#include "../../shared/underpass_switches_task.inc.c"

#include "../../shared/room/room_variants_underpass.inc.c"

#include "../../shared/underpass_switches_msg.inc.c"

#include "../../shared/underpass_sound_msg.inc.c"

static void _glowDrawFlareLocal(const GfxCoord* coord, const SVECTOR* localPoint, s32 textureIndex, s32 radiusScale);

/// Refuses every key-item use in this room without consuming the item.
///
/// Returns 0 so inventory displays its "No use now" notice. `itemId` is the
/// inventory item's integer ID; all callback arguments are ignored.
static s32 _dryfieldUnderpassRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Starts the underpass's one-shot event for direction action 1 in placement variant 1.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows the four-byte request until dispatch
/// returns and supplies an ignored zero second word. Only `actionId` is read;
/// other actions, other variants and a seen event do nothing. Sets the seen flag
/// before starting the event script with temporary HUD hiding. Always returns
/// zero and retains no payload pointer. Requires live session state and loaded
/// room event scripts.
static s32 _dryfieldUnderpassHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { UNDERPASS_EVENT_ACTION = 1 };
    u8 actionId;

    actionId = request->actionId;
    if ((actionId == UNDERPASS_EVENT_ACTION) && (gGameSession->location.variant == actionId) && (gameFlagGetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN) == 0)) {
        gameFlagSetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN, 1);
        evsStartScript(_gDryfieldUnderpassGrayStalkerEventScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    return 0;
}

/// Registers the underpass room receiver and prepares its unseen variant-1 event.
///
/// Installs the room message table, claims `GAME_TASK_SLOT_ROOM` and advances to
/// state 1. Before the event has been seen, placement variant 1 broadcasts the
/// Gray Stalker preparation command through the scene task. Both room and scene
/// resources must remain loaded for message delivery; no work allocation is acquired.
static void _dryfieldUnderpassInitializeRoomTask(Task* task)
{
    enum { UNDERPASS_EVENT_VARIANT = 1 };

    task->msgTable = _gDryfieldUnderpassRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.variant == UNDERPASS_EVENT_VARIANT) && (gameFlagGetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN) == 0)) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &_gDryfieldUnderpassPrepareGrayStalkerCommand, ACTOR_COMMAND_MESSAGE_APPLY);
    }
    task->state = task->state + 1;
}

/// Keeps the initialized underpass room task alive and idle.
static void _dryfieldUnderpassIdleTask(Task* unusedTask)
{
}

/// Commits the underpass's progress-dependent room variant after its event.
///
/// The event script invokes this callback after engaging battle. Requires a live
/// session and live save. Selects room 1..6 from the event flag, flag 0x53 and
/// switch 1, writes both locations and requests deferred room-object relinking.
/// Flag 0x53's story meaning is unproven.
static void _dryfieldUnderpassRefreshRoomVariant(void)
{
    enum {
        DRYFIELD_UNDERPASS_ROOM_AFTER_EVENT             = 1,
        DRYFIELD_UNDERPASS_ROOM_AFTER_EVENT_FLAG_053    = 2,
        DRYFIELD_UNDERPASS_SWITCH_OFF_ROOM_OFFSET       = 2,
        DRYFIELD_UNDERPASS_ROOM_BEFORE_EVENT_SWITCH_ON  = 5,
        DRYFIELD_UNDERPASS_ROOM_BEFORE_EVENT_SWITCH_OFF = 6,
    };

    RoomEventMsg  request;
    RoomEventMsg  destination;
    RoomEventMsg* requestPtr;
    RoomEventMsg* destinationPtr;
    GameSession*  session;
    u8            roomId;

    /// Selects the underpass room for an execution request.
    ///
    /// `requestPtr` and `destinationPtr` are stable RoomEventMsg pointers, and
    /// `destination` is the writable object addressed by destinationPtr. Arguments
    /// are evaluated repeatedly and must be side-effect-free. Writes only the
    /// room byte for execution requests: 1/2 after the event, plus two with
    /// switch 1 off, or 5/6 before the event with switch 1 on/off. Flag 0x53
    /// chooses the second post-event room; its story meaning is unproven.
    /// The pointer and direct accesses preserve query and room reload order.
#define DRYFIELD_UNDERPASS_RESOLVE_ROOM(requestPtr, destinationPtr, destination)                               \
    if ((requestPtr)->queryOnly == ROOM_EVENT_EXECUTE) {                                                       \
        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN) != 0) {                                          \
            if (gameFlagGetNibble(GAME_FLAG_053) != 0) {                                                       \
                (destinationPtr)->roomState = DRYFIELD_UNDERPASS_ROOM_AFTER_EVENT_FLAG_053;                    \
            } else {                                                                                           \
                (destinationPtr)->roomState = DRYFIELD_UNDERPASS_ROOM_AFTER_EVENT;                             \
            }                                                                                                  \
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {                                        \
                (destination).roomState = (destination).roomState + DRYFIELD_UNDERPASS_SWITCH_OFF_ROOM_OFFSET; \
            }                                                                                                  \
        } else {                                                                                               \
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) != 0) {                                        \
                (destinationPtr)->roomState = DRYFIELD_UNDERPASS_ROOM_BEFORE_EVENT_SWITCH_ON;                  \
            } else {                                                                                           \
                (destinationPtr)->roomState = DRYFIELD_UNDERPASS_ROOM_BEFORE_EVENT_SWITCH_OFF;                 \
            }                                                                                                  \
        }                                                                                                      \
    }

    destinationPtr    = &destination;
    requestPtr        = &request;
    request.areaId    = GAME_AREA_DRYFIELD_UNDERPASS;
    request.queryOnly = ROOM_EVENT_EXECUTE;
    DRYFIELD_UNDERPASS_RESOLVE_ROOM(requestPtr, destinationPtr, destination);
#undef DRYFIELD_UNDERPASS_RESOLVE_ROOM

    // Commit the same selector to the live session and its save before relinking.
    session                                                     = gGameSession;
    roomId                                                      = destination.roomState;
    session->location.roomState                                 = roomId;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = roomId;
    gGameSession->roomObjsDirty                                 = true;
}

/// Room-task lifecycle callbacks indexed by `Task::state` in 0..2.
///
/// Entry registers the receiver and prepares the unseen event, state 1 idles
/// and state 2 releases the task. The dispatcher copies all three pointers
/// to the stack before calling the selected handler; there is no terminator.
static const TaskFuncTable3 _gDryfieldUnderpassRoomTaskStates = {
    { _dryfieldUnderpassInitializeRoomTask, _dryfieldUnderpassIdleTask, taskKill },
};

void dryfieldUnderpassRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldUnderpassRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_flare_local.inc.c"

void dryfieldUnderpassDrawFlaresTask(Task* task)
{
    enum {
        DRYFIELD_UNDERPASS_FLARE_TEXTURE_INDEX = 0,
        DRYFIELD_UNDERPASS_FLARE_RADIUS_SCALE  = 640,
    };

    const GfxCoord* coord;
    s32             viewMask;
    s32             flareIndex;
    const SVECTOR*  flarePosition;
    const s16*      flareViewMasks;

    coord    = task->extra.coordBody->coord;
    viewMask = 1 << gGameSession->location.view;
    if (gameFlagGetNibble(GAME_FLAG_053) == 0) {
        // Each local position's mask uses the view ID directly as its bit index.
        flareIndex     = 0;
        flarePosition  = _gDryfieldUnderpassFlarePositions;
        flareViewMasks = _gDryfieldUnderpassFlareViewMasks;
        for (; flareIndex < (s32)ARRAY_SIZE(_gDryfieldUnderpassFlarePositions); flarePosition++, flareIndex++, flareViewMasks++) {
            if (viewMask & *flareViewMasks) {
                _glowDrawFlareLocal(coord, flarePosition, DRYFIELD_UNDERPASS_FLARE_TEXTURE_INDEX, DRYFIELD_UNDERPASS_FLARE_RADIUS_SCALE);
            }
        }
    }
}
