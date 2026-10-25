#include "rooms/neo_ark_r26.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor_143900.h"

#include "gameplay/companion_load.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

static s32  _neoArkR26RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32  _neoArkR26ResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _neoArkR26IgnoreCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 unusedCommandId, s32 unusedCommandArg);
static s32  _neoArkR26IgnoreRoomActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);
static void _neoArkR26RoomIdleState(Task* unusedTask);

static WorldCollisionGrid         _gNeoArkR26FloorCollisionGrid;
static WorldCoordRoomAmbientEntry _gNeoArkR26ViewAmbient[5];
static WorldCoordRoomLights       _gNeoArkR26RoomLights;

static void _neoArkR26ReturnToShelterTent(void);
static void _neoArkR26InitializeRoom(Task* task);

/// Replay scene whose entry script and departure callback are suppressed.
enum { NEO_ARK_R26_ATTRACT_DEMO = 9 };

/// Receiver-specific keys used by the room's entry scene and actor layout.
enum {
    NEO_ARK_R26_PLAYER_ENTRY_CLIP        = 1,
    NEO_ARK_R26_SECOND_WALKER_ENTRY_CLIP = 3,
    NEO_ARK_R26_WALKER_SHOW_ATTACHMENT1  = 1,
    NEO_ARK_R26_FIRST_WALKER_RESOURCE    = 111,
    NEO_ARK_R26_SECOND_WALKER_RESOURCE   = 112,
    NEO_ARK_R26_VIEW2_MAIN_SPRITE_COUNT  = 54,
};

/// Entry-scene CAP command, actor and weapon selectors, draw flags and fade tag.
enum {
    NEO_ARK_R26_ENTRY_CAP_COMMAND = 1,
    NEO_ARK_R26_FIRST_WALKER      = 0,
    NEO_ARK_R26_SECOND_WALKER     = 1,
    NEO_ARK_R26_FADE_OT_TAG       = 5,
    NEO_ARK_R26_PLAYER_WEAPONS    = 0,
};

/// Restarts the player's entry clip in the equipped weapon bank, without grid response.
///
/// Event playback replaces source.index in a copy before synchronous dispatch;
/// the stored bank selector is therefore not the bank ultimately played.
/// Read-only script arguments retain their original data-section placement.
static const AnimationPlayRequest _gNeoArkR26PlayerEntryAnimation SECTION(".data") = {
    .source               = { .index = 1 },
    .animationId          = NEO_ARK_R26_PLAYER_ENTRY_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced record shaped as command zero with Shelter R36 context tags.
///
/// Its receiver and action are unproven; command zero has different meanings
/// for the two scripted walkers.
static ActorCommand D_neo_ark_r26_8017D794 = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R36 } },
    .command = 0,
};

/// Shows attachment 1 and hides attachment 2 of the second scripted walker.
///
/// The receiver ignores the retained Shelter R36 context tags and reads only
/// command. The entry script borrows this record through synchronous dispatch.
static ActorCommand _gNeoArkR26ShowWalkerAttachment1Command = {
    { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R36 } },
    NEO_ARK_R26_WALKER_SHOW_ATTACHMENT1,
};

/// Unreferenced records shaped as clips 1/2 with ten-frame interpolation requests.
///
/// The receiver, purpose and whether these form an indexed array are unproven.
static AnimationPlayRequest D_neo_ark_r26_8017D79C[2] = {
    { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 2, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Immediately restarts clip 3 of the second scripted walker for the entry scene.
///
/// The actor reads animationId and blend; its fixed clip table ignores the bank
/// selector, blend duration on reset, and grid-participation word.
static AnimationPlayRequest _gNeoArkR26SecondWalkerEntryAnimation = {
    .source               = { .index = 1 },
    .animationId          = NEO_ARK_R26_SECOND_WALKER_ENTRY_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the second scripted walker's clip 4 at four entry-scene cues.
///
/// Borrowed through synchronous dispatch. The receiver uses animationId and
/// blend; bank selector, reset duration and grid participation are ignored.
static AnimationPlayRequest _gNeoArkR26SecondWalkerClip4Animation = {
    .source               = { .index = 1 },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced records shaped as reset requests for clips 5/6/7/8/1/1.
///
/// The receiver, purpose and whether these form an indexed array are unproven.
static AnimationPlayRequest D_neo_ark_r26_8017D7EC[6] = {
    { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Restarts the first scripted walker's clip 9 as the entry fade returns.
///
/// Borrowed through synchronous dispatch. The receiver uses animationId and
/// blend; bank selector, reset duration and grid participation are ignored.
static AnimationPlayRequest _gNeoArkR26FirstWalkerClip9Animation = {
    .source               = { .index = 1 },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced records shaped as reset requests for clips 10 through 16.
///
/// The receiver, purpose and whether these form an indexed array are unproven.
static AnimationPlayRequest D_neo_ark_r26_8017D878[7] = {
    { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Restarts the second scripted walker's clip 9 at two entry-scene cues.
///
/// Borrowed through synchronous dispatch. The receiver uses animationId and
/// blend; bank selector, reset duration and grid participation are ignored.
static AnimationPlayRequest _gNeoArkR26SecondWalkerClip9Animation = {
    .source               = { .index = 1 },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the second scripted walker's clip 10 during the entry scene.
///
/// Borrowed through synchronous dispatch. The receiver uses animationId and
/// blend; bank selector, reset duration and grid participation are ignored.
static AnimationPlayRequest _gNeoArkR26SecondWalkerClip10Animation = {
    .source               = { .index = 1 },
    .animationId          = 10,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced block shaped as eight identical clip-1 reset requests.
///
/// The receiver, purpose and whether these form an indexed array are unproven.
static AnimationPlayRequest D_neo_ark_r26_8017D92C[8] = {
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Restarts the first scripted walker's final entry-scene clip, number 5.
///
/// Borrowed through synchronous dispatch. The receiver uses animationId and
/// blend; bank selector, reset duration and grid participation are ignored.
static AnimationPlayRequest _gNeoArkR26FirstWalkerClip5Animation = {
    .source               = { .index = 1 },
    .animationId          = 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced record shaped as a clip-6 reset request; receiver and purpose unproven.
static AnimationPlayRequest D_neo_ark_r26_8017D9E0 = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends the first scripted walker into entry-scene clip 7 over eight normal-rate frames.
///
/// Borrowed through synchronous dispatch. The receiver latches blendFrames
/// as a signed halfword; bank selector and grid participation are ignored.
static AnimationPlayRequest _gNeoArkR26FirstWalkerClip7Animation = {
    .source               = { .index = 1 },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the first scripted walker's clip 8 during the entry scene.
///
/// Borrowed through synchronous dispatch. The receiver uses animationId and
/// blend; bank selector, reset duration and grid participation are ignored.
static AnimationPlayRequest _gNeoArkR26FirstWalkerClip8Animation = {
    .source               = { .index = 1 },
    .animationId          = 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the first scripted walker's clip 17 during the entry scene.
///
/// Borrowed through synchronous dispatch. The receiver uses animationId and
/// blend; bank selector, reset duration and grid participation are ignored.
static AnimationPlayRequest _gNeoArkR26FirstWalkerClip17Animation = {
    .source               = { .index = 1 },
    .animationId          = 17,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the first scripted walker's clip 18 during the entry scene.
///
/// Borrowed through synchronous dispatch. The receiver uses animationId and
/// blend; bank selector, reset duration and grid participation are ignored.
static AnimationPlayRequest _gNeoArkR26FirstWalkerClip18Animation = {
    .source               = { .index = 1 },
    .animationId          = 18,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Places the player at (3270,-10000,-1630) with zero Euler rotation for scene entry.
///
/// XYZ uses whole units in the model root's parent frame; angles use 4096 units
/// per turn. The receiver reads the complete pose synchronously and retains no pointer.
static ActorTransform _gNeoArkR26PlayerEntryTransform = {
    .pos = { 3270, -10000, -1630, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Places the second scripted walker at (0,0,850) facing zero for scene entry.
///
/// XYZ uses whole units in the model root's parent frame; yaw uses 4096 units
/// per turn. The receiver reads XYZ and yaw synchronously and retains no pointer.
static ActorTransform _gNeoArkR26SecondWalkerEntryTransform = {
    .pos = { 0, 0, 850, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Runs the room-entry dialogue, two walkers' cue-driven clips and return to Shelter Tent.
///
/// Borrows the room, CAP and actor resources until playback ends. The paired
/// skip script may take over until input skipping is disabled immediately after
/// the departure request.
static EvsCommand _gNeoArkR26EntryScript[57] = {
    // Prepare CAP playback, scripted player control and the second walker.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = NEO_ARK_R26_ENTRY_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = NEO_ARK_R26_PLAYER_WEAPONS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 1 }, { .value = NEO_ARK_R26_FADE_OT_TAG }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gNeoArkR26PlayerEntryAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gNeoArkR26PlayerEntryTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_SECOND_WALKER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gNeoArkR26ShowWalkerAttachment1Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_SECOND_WALKER }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gNeoArkR26SecondWalkerEntryTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_SECOND_WALKER }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_SECOND_WALKER }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gNeoArkR26SecondWalkerEntryAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Reveal the scene and advance the walkers in step with CAP cues.
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_FIRST_WALKER }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gNeoArkR26FirstWalkerClip9Animation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_SECOND_WALKER }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gNeoArkR26SecondWalkerClip9Animation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_FIRST_WALKER }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gNeoArkR26FirstWalkerClip17Animation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_SECOND_WALKER }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gNeoArkR26SecondWalkerClip9Animation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_FIRST_WALKER }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gNeoArkR26FirstWalkerClip18Animation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_SECOND_WALKER }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gNeoArkR26SecondWalkerClip4Animation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_SECOND_WALKER }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gNeoArkR26SecondWalkerClip10Animation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_SECOND_WALKER }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gNeoArkR26SecondWalkerClip4Animation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_FIRST_WALKER }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gNeoArkR26FirstWalkerClip7Animation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_FIRST_WALKER }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gNeoArkR26FirstWalkerClip8Animation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_SECOND_WALKER }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gNeoArkR26SecondWalkerClip4Animation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_SECOND_WALKER }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gNeoArkR26SecondWalkerClip4Animation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_FIRST_WALKER }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gNeoArkR26FirstWalkerClip5Animation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Hide the second walker, fade out and reload the tent.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = NEO_ARK_R26_SECOND_WALKER }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 60 }, { .value = NEO_ARK_R26_FADE_OT_TAG }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _neoArkR26ReturnToShelterTent }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Fades out and cleans up a skipped entry scene before returning to Shelter Tent.
///
/// Paired with `_gNeoArkR26EntryScript`; borrows this room's resources through
/// playback. The final player message requests release of scripted control.
static EvsCommand _gNeoArkR26SkipEntryScript[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _neoArkR26ReturnToShelterTent }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Room-controller messages for transitions, key-item refusal and ignored room actions.
///
/// Borrowed by the room task until teardown. The four synchronous callbacks
/// receive ID-specific payloads; the final sentinel returns zero for other IDs.
static TaskMessageEntry _gNeoArkR26RoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkR26ResolveRoomVariant },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkR26RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkR26IgnoreRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, _neoArkR26IgnoreCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources gNeoArkR26CollisionRooms[1] = {
    { &_gNeoArkR26FloorCollisionGrid, NULL, NULL, NULL },
};

WorldCoordRoomLighting gNeoArkR26RoomLighting[1] = {
    { &_gNeoArkR26RoomLights, _gNeoArkR26ViewAmbient },
};

u8* gNeoArkR26ViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gNeoArkR26ViewCounts[1] = { ARRAY_SIZE(gNeoArkR26ViewCameras) };

DirectionWarpEntry gNeoArkR26Warps[1] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 0, 0, 0 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 0, 0, 0 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// The floor's downward unit normal, with 4096 per unit.
static SVECTOR _gNeoArkR26FloorCollisionGridNormals[1] = {
#include "assets/neo_ark_r26_collision_00BDC_normals.inc"
};

/// Four floor corners in signed room-coordinate units.
static SVECTOR _gNeoArkR26FloorCollisionGridVerts[4] = {
#include "assets/neo_ark_r26_collision_00BDC_verts.inc"
};

/// One floor quad selecting normal 0, all four corners and surface class 1.
static WorldCollisionGridFace _gNeoArkR26FloorCollisionGridFaces[1] = {
#include "assets/neo_ark_r26_collision_00BDC_faces.inc"
};

/// Eight two-element face lists, each face 0 followed by WORLD_COLLISION_GRID_CELL_END.
static s16 _gNeoArkR26FloorCollisionGridCells[16] = {
#include "assets/neo_ark_r26_collision_00BDC_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gNeoArkR26FloorCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define NEO_ARK_R26_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkR26FloorCollisionGridCells[(halfwordOffset)])
/// X-major 3x3 cell directory: eight floor lists followed by one empty cell.
static s16* _gNeoArkR26FloorCollisionGridTable[9] = {
#include "assets/neo_ark_r26_collision_00BDC_table.inc"
};
#undef NEO_ARK_R26_CELL_FACE_LIST_AT_OFFSET

/// The room's floor quad at Y=0, spanning -4000..4000 on both horizontal axes.
///
/// The 3x3 X-major index has eight face lists and an empty final cell.
/// Activation binds viewCoord; geometry and cell lists remain room-owned.
static WorldCollisionGrid _gNeoArkR26FloorCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkR26FloorCollisionGridNormals,
    .vertices    = _gNeoArkR26FloorCollisionGridVerts,
    .faces       = _gNeoArkR26FloorCollisionGridFaces,
    .cellFaceIds = _gNeoArkR26FloorCollisionGridTable,
    .xBias       = 4000,
    .zBias       = 4000,
    .cellCountX  = 3,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gNeoArkR26FloorCollisionGridFaces),
};

ViewCamera gNeoArkR26ViewCameras[4] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 13000, 0 } }, 257 },
    { { { { -3897, 0, -1261 }, { 0, ONE, 0 }, { 1261, 0, -3897 } }, { 1260, 1300, -5690 } }, 680 },
    { { { { 3982, 0, -955 }, { 108, 4069, 451 }, { 949, -463, 3957 } }, { 1470, 1070, 2380 } }, 257 },
    { { { { 3157, 0, 2608 }, { 2381, 1673, -2882 }, { -1065, 3738, 1289 } }, { -570, 2000, 760 } }, 257 },
};

/// Sprite image pages in VRAM words, the shared palette row and unity modulation.
enum {
    NEO_ARK_R26_SPRITE_LEFT_TPAGE         = getTPage(1, 0, 896, 0),
    NEO_ARK_R26_SPRITE_RIGHT_TPAGE        = getTPage(1, 0, 960, 0),
    NEO_ARK_R26_SPRITE_CLUT               = getClut(0, 255),
    NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION = 128,
};

/// Empty view-1 list; the leading zero count keeps decoded background strips.
static SpriteBatch _gNeoArkR26View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's textured rectangles, in pixels about the view centre with unscaled OT depths.
static SpriteSource _gNeoArkR26View2SpriteSources[62] = {
    { NEO_ARK_R26_SPRITE_LEFT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 56, 8 } }, 104, 16, 0, { .fields = { 88, 232 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_LEFT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 40, 16 } }, 120, 24, 0, { .fields = { 104, 216 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 24 } }, 128, 40, 0, { .fields = { 56, 232 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_LEFT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, 40, 0, { .fields = { 112, 0 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 16, 16 } }, 112, 72, 0, { .fields = { 24, 184 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 16, 24 } }, 64, 64, 0, { .fields = { 8, 192 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 8 } }, 72, 16, 0, { .fields = { 64, 32 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 64 } }, 56, 16, 0, { .fields = { 120, 0 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 48 } }, 64, 16, 0, { .fields = { 112, 0 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 56 } }, 32, 16, 0, { .fields = { 104, 128 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 56 } }, 8, 16, 0, { .fields = { 104, 184 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 64 } }, 0, 16, 0, { .fields = { 120, 64 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 48 } }, -24, 16, 0, { .fields = { 96, 48 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 40 } }, -48, 16, 0, { .fields = { 80, 136 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 24 } }, -64, 48, 0, { .fields = { 16, 144 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 24 } }, -80, 48, 0, { .fields = { 16, 72 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 24 } }, -96, 48, 0, { .fields = { 16, 48 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 16, 24 } }, -120, 48, 0, { .fields = { 8, 120 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 16, 32 } }, -136, 48, 0, { .fields = { 40, 0 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 16, 24 } }, -152, 48, 0, { .fields = { 8, 96 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 40, 32 } }, -160, 16, 0, { .fields = { 24, 200 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 40, 32 } }, -120, 16, 0, { .fields = { 64, 216 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 32, 32 } }, -80, 16, 0, { .fields = { 64, 176 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 8 } }, 136, 56, 1000, { .fields = { 40, 64 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 32, 32 } }, 128, 64, 1000, { .fields = { 64, 40 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 16 } }, 96, 16, 1000, { .fields = { 112, 240 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 40 } }, 72, 24, 1000, { .fields = { 88, 0 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 16, 40 } }, 104, 24, 1000, { .fields = { 88, 96 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 24 } }, 120, 40, 1000, { .fields = { 16, 216 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 32 } }, 96, 32, 1000, { .fields = { 112, 96 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 48, 8 } }, 80, 64, 1000, { .fields = { 64, 248 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_LEFT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 32, 16 } }, 80, 72, 1000, { .fields = { 104, 96 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_LEFT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 16 } }, 40, 80, 1000, { .fields = { 112, 128 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 16, 8 } }, 40, 72, 1000, { .fields = { 96, 40 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 32, 24 } }, 8, 72, 1000, { .fields = { 24, 232 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 24 } }, -24, 64, 1000, { .fields = { 24, 136 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 32 } }, -48, 56, 1000, { .fields = { 40, 168 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 24 } }, -56, 48, 0, { .fields = { 88, 72 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 24 } }, -72, 48, 0, { .fields = { 24, 48 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 24 } }, -88, 48, 0, { .fields = { 24, 160 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 24 } }, -104, 48, 0, { .fields = { 24, 24 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 16, 8 } }, -136, 80, 1000, { .fields = { 80, 208 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 24 } }, -160, 48, 0, { .fields = { 32, 160 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_LEFT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 16 } }, -160, 72, 1000, { .fields = { 112, 112 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_LEFT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 32, 24 } }, 8, 96, 1000, { .fields = { 120, 24 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 24 } }, 40, 96, 1000, { .fields = { 0, 168 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 32, 32 } }, 64, 88, 1000, { .fields = { 48, 136 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 32, 32 } }, 96, 88, 1000, { .fields = { 32, 32 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 32, 24 } }, 128, 96, 1000, { .fields = { 8, 0 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 64, 32 } }, -160, 88, 1000, { .fields = { 24, 104 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_LEFT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 72, 16 } }, -120, 72, 1000, { .fields = { 72, 144 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 64, 32 } }, -96, 88, 1000, { .fields = { 24, 72 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 32, 32 } }, -32, 88, 1000, { .fields = { 56, 0 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 40 } }, 0, 80, 1000, { .fields = { 96, 176 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 16 } }, -40, 80, 0, { .fields = { 120, 240 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 32 } }, -32, 80, 0, { .fields = { 104, 96 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_LEFT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 64, 8 } }, -88, 112, 250, { .fields = { 80, 160 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_LEFT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 56, 16 } }, -88, 96, 250, { .fields = { 88, 64 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_LEFT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 48, 16 } }, -88, 80, 250, { .fields = { 96, 80 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_LEFT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 56, 16 } }, -144, 80, 250, { .fields = { 96, 240 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_LEFT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 56, 16 } }, -144, 96, 250, { .fields = { 88, 48 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_LEFT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 56, 8 } }, -144, 112, 250, { .fields = { 80, 16 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 2's background selector, 54-rectangle range and remaining eight-rectangle range.
///
/// The main range's retained field_6 bytes are {1,0}; their role is unproven.
static SpriteBatch _gNeoArkR26View2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, NEO_ARK_R26_VIEW2_MAIN_SPRITE_COUNT, 0, 0, { 1, 0 } },
    { NEO_ARK_R26_VIEW2_MAIN_SPRITE_COUNT, ARRAY_SIZE(_gNeoArkR26View2SpriteSources) - NEO_ARK_R26_VIEW2_MAIN_SPRITE_COUNT, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's textured rectangles, in pixels about the view centre with unscaled OT depths.
static SpriteSource _gNeoArkR26View3SpriteSources[14] = {
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 8 } }, -40, 24, 0, { .fields = { 80, 72 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 16 } }, -40, 32, 0, { .fields = { 120, 224 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 88, 8 } }, 8, 24, 0, { .fields = { 16, 64 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 32, 8 } }, 8, 32, 0, { .fields = { 72, 56 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 48, 8 } }, 48, 32, 0, { .fields = { 48, 0 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 16, 8 } }, 80, 40, 0, { .fields = { 112, 248 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 16, 8 } }, 80, 96, 0, { .fields = { 112, 240 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 24, 80 } }, -16, 24, 500, { .fields = { 104, 0 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 32, 64 } }, 8, 40, 1150, { .fields = { 80, 144 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 72 } }, 40, 32, 1375, { .fields = { 120, 80 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 32, 64 } }, 48, 40, 1375, { .fields = { 88, 80 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 16, 48 } }, 80, 48, 1375, { .fields = { 96, 208 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 16, 72 } }, -32, 32, 961, { .fields = { 112, 152 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_R26_SPRITE_RIGHT_TPAGE, NEO_ARK_R26_SPRITE_CLUT, { .fields = { 8, 56 } }, -40, 48, 1131, { .fields = { 96, 0 } }, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, NEO_ARK_R26_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 3's background selector followed by its complete source range and terminator.
static SpriteBatch _gNeoArkR26View3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkR26View3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-4 list; the leading zero count keeps decoded background strips.
static SpriteBatch _gNeoArkR26View4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gNeoArkR26SpriteViews[4] = {
    { { .empty = _gNeoArkR26View1SpriteBatches }, _gNeoArkR26View1SpriteBatches, NULL },
    { { .elements = _gNeoArkR26View2SpriteSources }, _gNeoArkR26View2SpriteBatches, NULL },
    { { .elements = _gNeoArkR26View3SpriteSources }, _gNeoArkR26View3SpriteBatches, NULL },
    { { .empty = _gNeoArkR26View4SpriteBatches }, _gNeoArkR26View4SpriteBatches, NULL },
};

/// One all-view point light at (0,-2500,3000), with 7300/9300-unit falloff radii.
///
/// RGB intensities are 12-fractional-bit values. Coordinate composition and
/// lighting queries mutate the parent, composition cache and attenuation.
static WorldCoordPointLight _gNeoArkR26PointLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2500, 3000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2458, 1640, 820 }, { 0, 0 } }, 7300, 9300 },
};

/// Room 1's single point-light collection, with no directional or cone lights.
static WorldCoordRoomLights _gNeoArkR26RoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gNeoArkR26PointLights),
    .pointLights           = _gNeoArkR26PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Default-layout recipes for the first and second scripted walkers, then END.
///
/// Entry 111 loads actor file 143900; entry 112 loads texture file 660100 and
/// borrows the second walker's code/models from that same actor package.
static AreaResource _gNeoArkR26SceneActorResources[3] = {
    { NEO_ARK_R26_FIRST_WALKER_RESOURCE, 439, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor143900ScriptedWalkerTaskDesc },
    { NEO_ARK_R26_SECOND_WALKER_RESOURCE, 601, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, gActor143900SecondScriptedWalkerTaskDescriptors },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Initial poses and texture offsets for scene actors 0 and 1, then END.
///
/// The first walker starts at Z=2850 facing a half turn, the second at Z=855
/// facing zero. The script subsequently places actor 1 at Z=850. Both spawn
/// with mode/variant zero; offsets are texture-page columns and CLUT rows.
static AreaPlacement _gNeoArkR26SceneActorPlacements[3] = {
    { NEO_ARK_R26_FIRST_WALKER_RESOURCE, 0, 0, 0, 0, 2850, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 0 },
    { NEO_ARK_R26_SECOND_WALKER_RESOURCE, 0, 0, 0, 0, 855, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gNeoArkR26AreaVariants[11] = {
    { NULL, NULL },
    { _gNeoArkR26SceneActorPlacements, _gNeoArkR26SceneActorResources },
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

/// Ambient minima for views 1..4 after the count header, in sixteenths of RGB levels.
///
/// Views 1 and 4 use near-black minima; views 2 and 3 share brighter minima.
/// The fourth colour halfword retains weighted brightness, unused by shading.
static WorldCoordRoomAmbientEntry _gNeoArkR26ViewAmbient[5] = {
    { .viewCount = ARRAY_SIZE(_gNeoArkR26ViewAmbient) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1000, 700, 800, 825 } },
    { .color = { 1000, 700, 800, 825 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Entry indices in the loaded type-1 bank; each names the first of a cue pair.
enum {
    NEO_ARK_R26_FLOOR_WALK_SCRIPT = 0x2D,
    NEO_ARK_R26_FLOOR_RUN_SCRIPT  = 0x2F,
};

/// Loaded type-1-bank cue bases for floor walking, running and scripted jumps.
///
/// Cue 2 uses the base; cue 1 uses base+1. Scripted jumps share the walking pair.
static WorldCollisionFootstepSounds _gNeoArkR26FloorFootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | NEO_ARK_R26_FLOOR_WALK_SCRIPT,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | NEO_ARK_R26_FLOOR_RUN_SCRIPT,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | NEO_ARK_R26_FLOOR_WALK_SCRIPT,
};

/// Blocking surface policy without footstep cues, shared by classes 0 and 2..7.
///
/// field_0 is retained as zero; its role is unproven.
static WorldCollisionSurfaceProperties _gNeoArkR26SilentSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking floor policy for class 1, with walking/running footstep pairs.
///
/// field_0 is retained as zero; its role is unproven.
static WorldCollisionSurfaceProperties _gNeoArkR26FloorSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkR26FloorFootstepSounds,
};

WorldCollisionSurfaceProperties* gNeoArkR26SurfaceProperties[8] = {
    &_gNeoArkR26SilentSurface,
    &_gNeoArkR26FloorSurface,
    &_gNeoArkR26SilentSurface,
    &_gNeoArkR26SilentSurface,
    &_gNeoArkR26SilentSurface,
    &_gNeoArkR26SilentSurface,
    &_gNeoArkR26SilentSurface,
    &_gNeoArkR26SilentSurface,
};

/// Returns to Shelter 1F Tent after the room-entry scene finishes or is skipped.
///
/// The no-argument event-script callback preserves the save during attract demo 9.
/// Otherwise selects tent warp 1 and room 1, chooses sprite variant 1 and starts
/// a frame-capturing session reload with the alternate loading caption.
/// Requires live saved state and the gameplay reload/loading interfaces.
static void _neoArkR26ReturnToShelterTent(void)
{
    enum { NEO_ARK_R26_TENT_ARRIVAL   = 1,
           NEO_ARK_R26_TENT_ROOM      = 1,
           NEO_ARK_R26_SPRITE_VARIANT = 1 };

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != NEO_ARK_R26_ATTRACT_DEMO) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_SHELTER_NEO_ARK;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_SHELTER_1F_TENT;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = NEO_ARK_R26_TENT_ARRIVAL;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = NEO_ARK_R26_TENT_ROOM;
        gDisplayState.spriteVariant                                 = NEO_ARK_R26_SPRITE_VARIANT;
        taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
        gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, GAME_FLOW_LOAD_CAPTION_ALTERNATE);
    }
}

/// Refuses every key-item use request in this room.
///
/// Ignores every argument. The zero result makes the
/// key-item menu report that the item cannot be used here.
static s32 _neoArkR26RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a Neo Ark destination room for an R26 transition request.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`. Copies the complete eight-byte request
/// into writable reply storage before resolving its room from game progress.
/// Query mode preserves the copied record. The pointers may alias and are
/// borrowed only for this call; the Neo Ark map overlay must be loaded.
/// Always returns 1 to allow the transition; task and message ID are unused.
static s32 _neoArkR26ResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { NEO_ARK_R26_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return NEO_ARK_R26_TRANSITION_ALLOWED;
}

/// Ignores room commands and returns zero without changing room state.
///
/// Neither the command selector nor its integer argument is used.
static s32 _neoArkR26IgnoreCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 unusedCommandId, s32 unusedCommandArg)
{
    return 0;
}

/// Ignores trigger-driven room actions and returns zero.
///
/// The borrowed request is neither read nor retained; the second payload word
/// is unused.
static s32 _neoArkR26IgnoreRoomActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the room receiver and starts its skippable entry scene.
///
/// State 0 installs room messages and publishes the borrowed task in the room
/// slot. Starts the room's entry and skip scripts with HUD hold/restoration,
/// except during attract demo 9, then advances to message-only idle state 1.
/// Requires the event-script and room resources to remain loaded during playback.
static void _neoArkR26InitializeRoom(Task* task)
{
    task->msgTable = _gNeoArkR26RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != NEO_ARK_R26_ATTRACT_DEMO) {
        evsStartScriptWithSkip(_gNeoArkR26EntryScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gNeoArkR26SkipEntryScript);
    }
    task->state = task->state + 1;
}

/// Keeps the room task in its idle state after setup.
///
/// State 1 leaves the task, its message table and its room-slot registration
/// intact; state 2 selects the teardown handler.
static void _neoArkR26RoomIdleState(Task* unusedTask)
{
    // Retain the idle callback's unused 16-byte stack frame.
    char unusedStackFrame[0x10];
}

/// Room-controller states: initialize (0), message-only idle (1) and teardown (2).
///
/// Copied by value for dispatch; `Task::state` must be in 0..2. Teardown may
/// release the task, and dispatch makes no further access afterwards.
static const TaskFuncTable3 _gNeoArkR26RoomStateHandlers = {
    {
        _neoArkR26InitializeRoom,
        _neoArkR26RoomIdleState,
        taskKill,
    },
};

void neoArkR26RoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gNeoArkR26RoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

void neoArkR26EffectNoopTask(Task* unusedTask)
{
}
