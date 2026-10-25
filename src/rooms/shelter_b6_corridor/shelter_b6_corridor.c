#include "rooms/shelter_b6_corridor.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_01100.h"
#include "actors/actor_205200.h"
#include "actors/actor_350500.h"

#include "gameplay/display.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
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

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"
#include "../../shared/effects/glow_draw.h"

/// This room's grid object continues four unread bytes past the quad array,
/// so `_screenWaveGridTask` indexes `quads` rather than the whole object.
#define SCREEN_WAVE_GRID gScreenWaveGrid.quads
#include "../../shared/screen/screen_wave.h"

static s32 _gScreenWaveAmplitude;

static ScreenWaveCtx* _gScreenWaveContext;

static ScreenWaveGridOscillator _gScreenWaveColumnWaves[10];
static ScreenWaveGridOscillator _gScreenWaveRowWaves[30];

/// Screen-wave quad meshes for this room, plus the unread bytes after them.
///
/// `quads` is the double-buffered 8 by 30 mesh. Nothing reads `pad`; those
/// four bytes fill the gap to the next object, which begins on an eight-byte
/// boundary.
typedef struct {
    POLY_FT4 quads[2][30][8]; // one mesh per frame buffer, 30 rows by 8 quads
    u8       pad[4];          // unread; fills the gap to the next eight-byte boundary
} _ShelterB6CorridorScreenWaveGrid;
STATIC_ASSERT_SIZEOF(_ShelterB6CorridorScreenWaveGrid, 19204);

/// This room's screen-wave meshes. `_screenWaveGridTask` indexes `quads`
/// through `SCREEN_WAVE_GRID`.
extern _ShelterB6CorridorScreenWaveGrid gScreenWaveGrid;

/// Eight bytes of room work storage whose role is unproven.
///
/// The room task writes `field_0` once, while setting the room up, and nothing
/// reads it back. `unknown_2` has no recovered access at all: it stands for the
/// zero bytes between that value and the next object, which begins on an
/// eight-byte boundary. Whether those bytes are further members, storage of
/// their own or alignment is unresolved.
typedef struct {
    s16 field_0;      // Set to 2 when the room task starts; never read, role unproven
    u8  unknown_2[6]; // Zero image bytes; role and grouping unproven
} _ShelterB6CorridorStorage51B0;
STATIC_ASSERT_SIZEOF(_ShelterB6CorridorStorage51B0, 8);

static void _shelterB6CorridorFinishScriptedBattle(s32 endDelayFrames);
static void _shelterB6CorridorLockAttachmentsAndCancelEffects(void);

/// Requested battle-end hold in frames, before any final-release override.
enum {
    SHELTER_B6_CORRIDOR_BATTLE_END_NORMAL_FRAMES = 15,
    SHELTER_B6_CORRIDOR_BATTLE_END_SKIP_FRAMES   = 11,
};

static s32 _shelterB6CorridorRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedArg);
static s32 _shelterB6CorridorResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB6CorridorHandlePartCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32 _shelterB6CorridorIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, DirectionActionRequest* unusedRequest, s32 unusedArg);
static s32 _shelterB6CorridorStartBattleEndScene(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

/// Retained bodyless screen-wave recipe at priority `TASK_PRIORITY_CONTROL`, followed by a terminator.
///
/// No recovered caller spawns this table. Entry 0 requires a persistent
/// `ScreenWaveCtx` in spawn argument 2; entry 1 must never be spawned.
static TaskDesc _gShelterB6CorridorScreenWaveTasks[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _screenWaveGridTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Current sine-displacement strength, recomputed as frame * scale / span.
///
/// Strength 32 corresponds to one pixel at the sine peak. The seed tick resets
/// the initial 256 to zero; the context must have a positive span while drawing.
static s32 _gScreenWaveAmplitude = 256;

/// Five room-message handlers and the end marker for unsupported requests.
///
/// Resolves departures, rejects key items, ignores room actions, selects
/// Eve-part dialogue and starts the battle-end scene. Borrowed by the room task
/// until teardown; the records and callbacks must remain loaded.
static TaskMessageEntry _gShelterB6CorridorRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB6CorridorResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB6CorridorRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB6CorridorIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB6CorridorHandlePartCommand },
    { ROOM_MESSAGE_ACTOR_EVENT, _shelterB6CorridorStartBattleEndScene },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static AnimationPackedPose _gShelterB6CorridorAnimation01C70Bank1[6] = {
#include "assets/shelter_b6_corridor_animation_01C70_bank1.inc"
};

static AnimationPackedRotation _gShelterB6CorridorAnimation01C70Bank4[46] = {
#include "assets/shelter_b6_corridor_animation_01C70_bank4.inc"
};

static AnimationRecord _gShelterB6CorridorAnimation01C70Records[109] = {
#include "assets/shelter_b6_corridor_animation_01C70_records.inc"
};

static u16 _gShelterB6CorridorAnimation01C70Indices[20] = {
#include "assets/shelter_b6_corridor_animation_01C70_indices.inc"
};

static AnimationSet _gShelterB6CorridorAnimation01C70 = {
    _gShelterB6CorridorAnimation01C70Records,
    _gShelterB6CorridorAnimation01C70Indices,
    { NULL, _gShelterB6CorridorAnimation01C70Bank1, NULL, NULL, _gShelterB6CorridorAnimation01C70Bank4, NULL, NULL, NULL },
};

/// Two player-bank extension words: absent clip 47 and the battle-end clip 48.
///
/// Both scripts copy the complete table; its clip data must stay loaded
/// through player playback. Later extension entries are left unchanged.
static AnimationSet* _gShelterB6CorridorBattleEndAnimationSets[2] = {
    NULL,
    &_gShelterB6CorridorAnimation01C70,
};

/// Installs the two battle-end extension words in the equipped player bank.
static AnimationBankCopyRequest _gShelterB6CorridorBattleEndAnimationBankCopy = {
    .sets  = _gShelterB6CorridorBattleEndAnimationSets,
    .count = ARRAY_SIZE(_gShelterB6CorridorBattleEndAnimationSets),
};

/// Unreferenced playback-shaped record: bank 1, clip 1, reset, grid disabled.
///
/// Its intended receiver and interpretation are unproven.
static AnimationPlayRequest D_shelter_b6_corridor_8017F268 = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum { SHELTER_B6_CORRIDOR_BATTLE_END_PLAYER_CLIP = ANIMATION_BANK_BASE_SET_COUNT + 1 };

/// Player battle-end clip, blended over 20 frames with grid participation disabled.
///
/// Both scripts resolve the equipped weapon bank in a copy, overwriting
/// `source.index`. Clip 48 is supplied by the preceding extension-copy request;
/// the room animation data must remain loaded through playback.
static AnimationPlayRequest _gShelterB6CorridorBattleEndPlayerAnimation = {
    .source               = { .index = 1 },
    .animationId          = SHELTER_B6_CORRIDOR_BATTLE_END_PLAYER_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Five unreferenced playback-shaped records: bank 0 and clips 1, 1, 2, 3 and 4.
///
/// The intended receiver and ordering contract are unproven. Retains reset
/// and blend choices, whole-frame durations and disabled grid participation.
static AnimationPlayRequest D_shelter_b6_corridor_8017F290[5] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Masked Eve's battle-end start: room XYZ (7000, 0, 0), yaw -ONE/4.
///
/// Placement copies the position and Euler angles before the backward walk.
static ActorTransform _gShelterB6CorridorBattleEndEveStartTransform = {
    .pos = { 7000, 0, 0, 0 },
    .rot = { 0, -ONE / 4, 0, 0 },
};

/// Masked Eve's backward-walk target: room XYZ (7800, 0, 0), final yaw ONE/4.
///
/// Walk dispatch copies the target; the optional clip pair selects clips 3 and 4.
static ActorTransform _gShelterB6CorridorBattleEndEveBackingTarget = {
    .pos = { 7800, 0, 0, 0 },
    .rot = { 0, ONE / 4, 0, 0 },
};

/// Masked Eve's forward exit target: room XYZ (20000, 0, 0), final yaw ONE/4.
///
/// Walk dispatch copies the target and selects the walker's default clips 2 and 1.
static ActorTransform _gShelterB6CorridorBattleEndEveExitTarget = {
    .pos = { 20000, 0, 0, 0 },
    .rot = { 0, ONE / 4, 0, 0 },
};

/// Actor command values and bank-0 clips used by the battle-end scene.
enum {
    SHELTER_B6_CORRIDOR_EVE_WALK_BACKWARD        = 1,
    SHELTER_B6_CORRIDOR_EVE_WALK_FORWARD         = 2,
    SHELTER_B6_CORRIDOR_EVE_CONTROLLER_STOP      = 1,
    SHELTER_B6_CORRIDOR_EVE_BODY_DESTROY         = 1,
    SHELTER_B6_CORRIDOR_EVE_BACKING_CLIP         = 3,
    SHELTER_B6_CORRIDOR_EVE_BACKING_ARRIVAL_CLIP = 4,
};

/// Selects backward motion for masked Eve's subsequent walk requests.
static ActorCommand _gShelterB6CorridorEveWalkBackwardCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_CORRIDOR } },
    .command = SHELTER_B6_CORRIDOR_EVE_WALK_BACKWARD,
};

/// Selects forward motion for masked Eve's subsequent walk requests.
static ActorCommand _gShelterB6CorridorEveWalkForwardCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_CORRIDOR } },
    .command = SHELTER_B6_CORRIDOR_EVE_WALK_FORWARD,
};

/// Bank-0 backward clip 3 and arrival clip 4 for masked Eve's first scene walk.
///
/// Dispatch copies the IDs into playback and the queued arrival byte; it
/// retains no pointer. The actor's clip data must remain loaded while playing.
static ActorMotionWalkAnim _gShelterB6CorridorBattleEndEveWalkAnimations = {
    .animationId = SHELTER_B6_CORRIDOR_EVE_BACKING_CLIP,
    .nextAnimId  = SHELTER_B6_CORRIDOR_EVE_BACKING_ARRIVAL_CLIP,
};

/// Latches a stop request in the corridor's Eve-part controller.
static ActorCommand _gShelterB6CorridorStopEveControllerCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_CORRIDOR } },
    .command = SHELTER_B6_CORRIDOR_EVE_CONTROLLER_STOP,
};

/// Latches teardown of the stationary Eve battle body after it is hidden.
static ActorCommand _gShelterB6CorridorDestroyEveBodyCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_CORRIDOR } },
    .command = SHELTER_B6_CORRIDOR_EVE_BODY_DESTROY,
};

/// Placement selectors, visibility choices and authored cues for the battle-end scene.
enum {
    SHELTER_B6_CORRIDOR_PLACED_EVE_WALKER     = 0,
    SHELTER_B6_CORRIDOR_PLACED_EVE_CONTROLLER = 2,
    SHELTER_B6_CORRIDOR_PLACED_EVE_BODY       = 3,
    SHELTER_B6_CORRIDOR_BATTLE_END_VIEW       = 5,
    SHELTER_B6_CORRIDOR_BATTLE_END_SOUND      = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_CORRIDOR, 4),
    SHELTER_B6_CORRIDOR_WAIT_NEXT_TICK        = 0,
    SHELTER_B6_CORRIDOR_FADE_DEFAULT_DURATION = 0, // Select the interpreter's seven-frame ramp
    SHELTER_B6_CORRIDOR_FADE_KEEP_DURATION    = 0, // Return with the current ramp duration
};

/// Normal battle-end scene: replace Eve's battle body with her walker, then restore play.
///
/// Borrows 34 commands through END and their room-owned payloads through
/// execution. Placed actors 0, 2 and 3 are the walker, part controller and
/// battle body. Player playback additionally borrows this room's animation data.
static EvsCommand _gShelterB6CorridorBattleEndScript[34] = {
    // Hold the player in the room-supplied pose before the body swap.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gShelterB6CorridorBattleEndAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gShelterB6CorridorBattleEndPlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB6CorridorLockAttachmentsAndCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = SHELTER_B6_CORRIDOR_FADE_KEEP_DURATION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Replace the battle body with the walker and stage its backward approach.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_B6_CORRIDOR_PLACED_EVE_WALKER }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_B6_CORRIDOR_PLACED_EVE_CONTROLLER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gShelterB6CorridorStopEveControllerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_B6_CORRIDOR_PLACED_EVE_BODY }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_B6_CORRIDOR_PLACED_EVE_BODY }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gShelterB6CorridorDestroyEveBodyCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = SHELTER_B6_CORRIDOR_BATTLE_END_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_B6_CORRIDOR_PLACED_EVE_WALKER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gShelterB6CorridorEveWalkBackwardCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_B6_CORRIDOR_PLACED_EVE_WALKER }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gShelterB6CorridorBattleEndEveStartTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_B6_CORRIDOR_PLACED_EVE_WALKER }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gShelterB6CorridorBattleEndEveBackingTarget } }, { .message = { .pointer = &_gShelterB6CorridorBattleEndEveWalkAnimations } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = SHELTER_B6_CORRIDOR_WAIT_NEXT_TICK }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SHELTER_B6_CORRIDOR_BATTLE_END_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_B6_CORRIDOR_PLACED_EVE_WALKER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gShelterB6CorridorEveWalkForwardCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_B6_CORRIDOR_PLACED_EVE_WALKER }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gShelterB6CorridorBattleEndEveExitTarget } }, { .message = { .pointer = NULL } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = SHELTER_B6_CORRIDOR_FADE_KEEP_DURATION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_B6_CORRIDOR_PLACED_EVE_WALKER }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Disable skipping before releasing combat and restoring player control.
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB6CorridorFinishScriptedBattle }, { .value = SHELTER_B6_CORRIDOR_BATTLE_END_NORMAL_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip path restoring the battle-end scene's actor visibility and gameplay state.
///
/// Borrows 18 commands through END, with the same actor and animation lifetimes
/// as the normal script. Hides the walker, stops the controller and destroys
/// the battle body, omitting the walk and sound cue before releasing combat.
static EvsCommand _gShelterB6CorridorBattleEndSkipScript[18] = {
    // Restore the post-scene actor state under the primary fade.
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = SHELTER_B6_CORRIDOR_FADE_DEFAULT_DURATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB6CorridorLockAttachmentsAndCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gShelterB6CorridorBattleEndAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gShelterB6CorridorBattleEndPlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_B6_CORRIDOR_PLACED_EVE_WALKER }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_B6_CORRIDOR_PLACED_EVE_CONTROLLER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gShelterB6CorridorStopEveControllerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_B6_CORRIDOR_PLACED_EVE_BODY }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = SHELTER_B6_CORRIDOR_PLACED_EVE_BODY }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gShelterB6CorridorDestroyEveBodyCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = SHELTER_B6_CORRIDOR_WAIT_NEXT_TICK }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB6CorridorFinishScriptedBattle }, { .value = SHELTER_B6_CORRIDOR_BATTLE_END_SKIP_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// World-space endpoints of four vertical capsule lights on each corridor wall.
///
/// Wall 0 is at Z=1450, wall 1 at Z=-1450; each row stores consecutive
/// top/bottom pairs in increasing X order. Four-point draws stay within a row.
static SVECTOR _gShelterB6CorridorWallLightEndpoints[2][8] = {
    {
        { 2000, -3250, 1450, 0 },
        { 2000, -400, 1450, 0 },
        { 4000, -3250, 1450, 0 },
        { 4000, -400, 1450, 0 },
        { 6000, -3250, 1450, 0 },
        { 6000, -400, 1450, 0 },
        { 8000, -3250, 1450, 0 },
        { 8000, -400, 1450, 0 },
    },
    {
        { 2000, -3250, -1450, 0 },
        { 2000, -400, -1450, 0 },
        { 4000, -3250, -1450, 0 },
        { 4000, -400, -1450, 0 },
        { 6000, -3250, -1450, 0 },
        { 6000, -400, -1450, 0 },
        { 8000, -3250, -1450, 0 },
        { 8000, -400, -1450, 0 },
    },
};

u8* gShelterB6CorridorViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB6CorridorViewCounts[1] = { SHELTER_B6_CORRIDOR_VIEW_COUNT };

/// Sound-script requests carried by the corridor's two warp endpoints.
enum {
    SHELTER_B6_CORRIDOR_ELEVATOR_ARRIVAL_SOUND   = SOUND_AREA(5, GAME_AREA_SHELTER_B6_CORRIDOR, 1),
    SHELTER_B6_CORRIDOR_TRAINING_DEPARTURE_SOUND = SOUND_AREA(5, GAME_AREA_SHELTER_B6_CORRIDOR, 3),
};

DirectionWarpEntry gShelterB6CorridorWarpEntries[2] = {
    { { { .word = ONE / 4 }, 430, 0, 0 }, { 0, 0, 0, 0 }, { { .word = ONE / 4 }, 430, 0, 0 }, { 0, 0, 0, 0 }, SHELTER_B6_CORRIDOR_ELEVATOR_ARRIVAL_SOUND, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3 * ONE / 4 }, 8400, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 3 * ONE / 4 }, 8400, 0, 0 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, SHELTER_B6_CORRIDOR_TRAINING_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Grid-local unit normals of the corridor mesh, with 4096 per unit.
static SVECTOR _gShelterB6CorridorCollisionGridNormals[6] = {
#include "assets/shelter_b6_corridor_collision_024D0_normals.inc"
};

/// Corridor mesh vertices in signed game-coordinate units.
static SVECTOR _gShelterB6CorridorCollisionGridVerts[12] = {
#include "assets/shelter_b6_corridor_collision_024D0_verts.inc"
};

/// Corridor triangles and quads indexing the vertex, normal and surface tables.
static WorldCollisionGridFace _gShelterB6CorridorCollisionGridFaces[12] = {
#include "assets/shelter_b6_corridor_collision_024D0_faces.inc"
};

/// Signed face-index storage for three cell lists, each ending with -1.
///
/// The directory selects starts 0, 10 and 18; entry 17 is retained without
/// a directory reference. Its role is unproven.
static s16 _gShelterB6CorridorCollisionGridCells[28] = {
#include "assets/shelter_b6_corridor_collision_024D0_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterB6CorridorCollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..27
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_B6_CORRIDOR_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB6CorridorCollisionGridCells[(halfwordOffset)])
/// X-major cell directory borrowing the three lists at element offsets 0, 10 and 18.
static s16* _gShelterB6CorridorCollisionGridTable[3] = {
#include "assets/shelter_b6_corridor_collision_024D0_table.inc"
};
#undef SHELTER_B6_CORRIDOR_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB6CorridorCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB6CorridorCollisionGridNormals,
    .vertices    = _gShelterB6CorridorCollisionGridVerts,
    .faces       = _gShelterB6CorridorCollisionGridFaces,
    .cellFaceIds = _gShelterB6CorridorCollisionGridTable,
    .xBias       = 0,
    .zBias       = 1350,
    .cellCountX  = ARRAY_SIZE(_gShelterB6CorridorCollisionGridTable),
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB6CorridorCollisionGridFaces),
};

ViewCamera gShelterB6CorridorViewCameras[SHELTER_B6_CORRIDOR_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -4500, 25000, 0 } }, 680 },
    { { { { 715, 0, 4033 }, { 264, 4087, -46 }, { -4024, 268, 713 } }, { -7040, 1470, 560 } }, 230 },
    { { { { 682, 0, -4038 }, { -252, 4087, -42 }, { 4030, 256, 681 } }, { -1740, 1470, 560 } }, 230 },
    { { { { 955, 0, 3982 }, { -1522, 3784, 365 }, { -3680, -1565, 883 } }, { -3190, 170, 810 } }, 230 },
    { { { { 0, 0, -ONE }, { -860, 4004, 0 }, { 4004, 860, 0 } }, { -6235, 1350, 0 } }, 329 },
};

/// Empty view-1 drawing list retaining the decoded background and a terminal record.
static SpriteBatch _gShelterB6CorridorView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// GPU encodings and neutral modulation for the two destroyed-scenery sprite views.
enum {
    SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE  = getTPage(1, 0, 960, 0), // 8-bit texture page, blend mode 0
    SHELTER_B6_CORRIDOR_SPRITE_CLUT          = getClut(0, 255),
    SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR = 128,
};

/// View-2 destroyed scenery: 19 rectangles for part slot 0, then 16 for slot 1.
///
/// Positions and sizes are pixels about the view centre; depths sort the
/// rectangles against scene geometry. Texture and palette addresses are encoded GPU values.
static SpriteSource _gShelterB6CorridorView2SpriteSources[35] = {
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 8 } }, 48, 32, 1246, { .fields = { 104, 72 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, 80, -120, 906, { .fields = { 88, 96 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, 80, -88, 1012, { .fields = { 88, 128 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, 80, -56, 1009, { .fields = { 88, 64 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, 80, -24, 1100, { .fields = { 88, 0 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, 80, 8, 1037, { .fields = { 88, 32 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 80, 40, 1007, { .fields = { 64, 64 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, 64, -112, 1025, { .fields = { 72, 0 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, 64, -80, 1170, { .fields = { 72, 32 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, 64, -48, 1167, { .fields = { 88, 224 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, 64, -16, 1162, { .fields = { 88, 160 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, 64, 16, 1162, { .fields = { 88, 192 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, 56, -96, 1174, { .fields = { 80, 112 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, 56, -72, 1278, { .fields = { 80, 136 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, 56, -48, 1287, { .fields = { 80, 64 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, 56, -24, 1296, { .fields = { 80, 88 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, 56, 0, 1303, { .fields = { 80, 208 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 56, 24, 1288, { .fields = { 112, 240 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 48, -88, 1256, { .fields = { 104, 240 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, -120, 615, { .fields = { 104, 80 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, -80, 586, { .fields = { 104, 120 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, -40, 591, { .fields = { 104, 0 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, 0, 605, { .fields = { 104, 200 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, 40, 614, { .fields = { 104, 160 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, 80, 556, { .fields = { 112, 40 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, -120, 649, { .fields = { 120, 0 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, -80, 656, { .fields = { 120, 80 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, -40, 662, { .fields = { 120, 160 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, 0, 661, { .fields = { 120, 120 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, 40, 660, { .fields = { 120, 200 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -144, 80, 598, { .fields = { 80, 184 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -136, 16, 672, { .fields = { 80, 160 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -136, 40, 677, { .fields = { 80, 232 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 32 } }, -136, 64, 672, { .fields = { 104, 40 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -128, 72, 643, { .fields = { 120, 240 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite counts of the first destroyed-part group in each populated view.
enum {
    SHELTER_B6_CORRIDOR_VIEW2_PART0_SPRITE_COUNT = 19,
    SHELTER_B6_CORRIDOR_VIEW3_PART2_SPRITE_COUNT = 11,
};

/// View-2 background selector, part-0 and part-1 ranges, then the batch terminator.
///
/// The controller hides both groups on setup and shows each destroyed part.
/// The retained {1, 0} bytes of the first group have no established consumer.
static SpriteBatch _gShelterB6CorridorView2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, SHELTER_B6_CORRIDOR_VIEW2_PART0_SPRITE_COUNT, 0, 0, { 1, 0 } },
    { SHELTER_B6_CORRIDOR_VIEW2_PART0_SPRITE_COUNT, ARRAY_SIZE(_gShelterB6CorridorView2SpriteSources) - SHELTER_B6_CORRIDOR_VIEW2_PART0_SPRITE_COUNT, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-3 destroyed scenery: 11 rectangles for part slot 2, then 11 for slot 1.
///
/// Positions and sizes are pixels about the view centre; depths sort the
/// rectangles against scene geometry. Texture and palette addresses are encoded GPU values.
static SpriteSource _gShelterB6CorridorView3SpriteSources[22] = {
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, -64, -104, 1199, { .fields = { 96, 144 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, -64, -72, 1396, { .fields = { 96, 112 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, -64, -40, 1450, { .fields = { 96, 80 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 32 } }, -64, -8, 1382, { .fields = { 96, 176 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -64, 24, 1363, { .fields = { 96, 240 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -48, -96, 1422, { .fields = { 112, 240 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, -80, 1503, { .fields = { 80, 56 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -48, -56, 1564, { .fields = { 88, 32 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -48, -32, 1581, { .fields = { 88, 104 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -48, -8, 1590, { .fields = { 88, 80 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -48, 16, 1563, { .fields = { 88, 128 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 40 } }, 120, -120, 745, { .fields = { 96, 0 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 40 } }, 120, -80, 745, { .fields = { 112, 200 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 40 } }, 120, -40, 702, { .fields = { 112, 160 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 40 } }, 104, -120, 830, { .fields = { 112, 0 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 40 } }, 104, -80, 926, { .fields = { 112, 40 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 40 } }, 104, -40, 895, { .fields = { 112, 80 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 40 } }, 112, 0, 822, { .fields = { 96, 40 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 16, 40 } }, 112, 40, 799, { .fields = { 112, 120 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 32 } }, 104, 0, 951, { .fields = { 88, 0 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 32 } }, 104, 32, 944, { .fields = { 104, 208 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_CORRIDOR_SPRITE_TEXTURE_PAGE, SHELTER_B6_CORRIDOR_SPRITE_CLUT, { .fields = { 8, 32 } }, 96, -120, 927, { .fields = { 96, 208 } }, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, SHELTER_B6_CORRIDOR_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View-3 background selector, part-2 and part-1 ranges, then the batch terminator.
///
/// The controller hides both groups on setup and shows each destroyed part.
/// The retained {1, 0} bytes of the first group have no established consumer.
static SpriteBatch _gShelterB6CorridorView3SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, SHELTER_B6_CORRIDOR_VIEW3_PART2_SPRITE_COUNT, 0, 0, { 1, 0 } },
    { SHELTER_B6_CORRIDOR_VIEW3_PART2_SPRITE_COUNT, ARRAY_SIZE(_gShelterB6CorridorView3SpriteSources) - SHELTER_B6_CORRIDOR_VIEW3_PART2_SPRITE_COUNT, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-4 drawing list retaining the decoded background and a terminal record.
static SpriteBatch _gShelterB6CorridorView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-5 drawing list retaining the decoded background and a terminal record.
static SpriteBatch _gShelterB6CorridorView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB6CorridorSpriteViews[SHELTER_B6_CORRIDOR_VIEW_COUNT] = {
    { { .empty = _gShelterB6CorridorView1SpriteBatches }, _gShelterB6CorridorView1SpriteBatches, NULL },
    { { .elements = _gShelterB6CorridorView2SpriteSources }, _gShelterB6CorridorView2SpriteBatches, NULL },
    { { .elements = _gShelterB6CorridorView3SpriteSources }, _gShelterB6CorridorView3SpriteBatches, NULL },
    { { .empty = _gShelterB6CorridorView4SpriteBatches }, _gShelterB6CorridorView4SpriteBatches, NULL },
    { { .empty = _gShelterB6CorridorView5SpriteBatches }, _gShelterB6CorridorView5SpriteBatches, NULL },
};

/// White point light at (4600, -2750, -50), in world game-coordinate units.
///
/// Full strength through radius 5000, fading to zero at radius 10000, in every
/// view. Room setup and shading mutate its coordinate and attenuation cache.
static WorldCoordPointLight _gShelterB6CorridorRoomPointLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4600, -2750, -50 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 5000, 10000 },
};

WorldCoordRoomLights gShelterB6CorridorRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB6CorridorRoomPointLights),
    .pointLights           = _gShelterB6CorridorRoomPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB6CorridorViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { 4576, -2065, 96, 0 }, { { 0, -2528, 2144, 0 }, { 0, -2528, -2144, 0 }, { 0, 2528, 2144, 0 }, { 0, 2528, -2144, 0 } }, { -4112, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4384, -2177, 128, 0 }, { { 0, -2480, -2256, 0 }, { 0, -2480, 2256, 0 }, { 0, 2480, -2256, 0 }, { 0, 2480, 2256, 0 } }, { 4110, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1245, -1952, 924, 0 }, { { -571, -2528, 1161, 0 }, { 572, -2528, -1161, 0 }, { -571, 2528, 1161, 0 }, { 572, 2528, -1161, 0 } }, { -3684, 0, -1813, 0 }, { 0, 0, 4096, 0 }, 2839, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1048, -1952, 1047, 0 }, { { 611, -2480, -1198, 0 }, { -612, -2480, 1198, 0 }, { 611, 2480, -1198, 0 }, { -612, 2480, 1198, 0 } }, { 3657, 0, 1865, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1567, -2080, -1409, 0 }, { { 233, -2528, 1273, 0 }, { -232, -2528, -1273, 0 }, { 233, 2528, 1273, 0 }, { -232, 2528, -1273, 0 } }, { -4038, 0, 736, 0 }, { 0, 0, 4096, 0 }, 2839, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1439, -2016, -1409, 0 }, { { -222, -2480, -1327, 0 }, { 223, -2480, 1327, 0 }, { -222, 2480, -1327, 0 }, { 223, 2480, 1327, 0 } }, { 4050, 0, -680, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Resources for the masked Eve walker, Mossback, Eve-part controller and battle body.
///
/// The default layout's four placement entry IDs select these descriptors;
/// AREA_PLACEMENT_END terminates the table. Overlays are loaded before task recipes run.
static AreaResource _gShelterB6CorridorAreaResources[5] = {
    { 131, 505, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor350500MaskedEveReverseWalkTaskDesc },
    { 49, 49, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor101100Tasks },
    { 52, 52, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor205200ControllerPartTasks },
    { 60, 60, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor205200BodyTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gShelterB6CorridorAreaVariants[SHELTER_B6_CORRIDOR_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapNeoArkCorridorDefaultPlacements, .resources = _gShelterB6CorridorAreaResources },
};

/// Warp keys pack the corridor's departure slot above the destination's arrival slot.
enum {
    SHELTER_B6_CORRIDOR_ELEVATOR_WARP_KEY         = (1 << 4) | 1,
    SHELTER_B6_CORRIDOR_TRAINING_WARP_KEY         = (2 << 4) | 1,
    SHELTER_B6_CORRIDOR_PART0_INTERACTION_COMMAND = 2,
    SHELTER_B6_CORRIDOR_PART1_INTERACTION_COMMAND = 3,
    SHELTER_B6_CORRIDOR_PART2_INTERACTION_COMMAND = 4,
};

WorldCollisionTrigger gShelterB6CorridorActionTriggers[5] = {
    { NULL, NULL, NULL, { 512, -48, 0, 0 }, { { -512, 0, -1024, 0 }, { 512, 0, -1024, 0 }, { -512, 0, 1024, 0 }, { 512, 0, 1024, 0 } }, { 0, ONE, 0, 0 }, { ONE, 0, 0, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_EVE_ELEVATOR, SHELTER_B6_CORRIDOR_ELEVATOR_WARP_KEY, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8512, -48, 0, 0 }, { { -512, 0, -1024, 0 }, { 512, 0, -1024, 0 }, { -512, 0, 1024, 0 }, { 512, 0, 1024, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B6_TRAINING_ROOM, SHELTER_B6_CORRIDOR_TRAINING_WARP_KEY, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2752, -64, 704, 0 }, { { -1056, 0, -608, 0 }, { 1056, 0, -608, 0 }, { -1056, 0, 608, 0 }, { 1056, 0, 608, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, -ONE, 0 }, 1214, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B6_CORRIDOR_PART0_INTERACTION_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, -688, 0 }, { { -1056, 0, -688, 0 }, { 1056, 0, -688, 0 }, { -1056, 0, 688, 0 }, { 1056, 0, 688, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, ONE, 0 }, 1254, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B6_CORRIDOR_PART1_INTERACTION_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6720, -64, 688, 0 }, { { -1056, 0, -656, 0 }, { 1056, 0, -656, 0 }, { -1056, 0, 656, 0 }, { 1056, 0, 656, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, -ONE, 0 }, 1241, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B6_CORRIDOR_PART2_INTERACTION_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry gShelterB6CorridorRoomAmbientByView[SHELTER_B6_CORRIDOR_VIEW_COUNT + 1] = {
    { .viewCount = ARRAY_SIZE(gShelterB6CorridorRoomAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 392, 437, 278, 400 } },
    { .color = { 393, 435, 277, 399 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Cue-2 footstep requests; cue 1 adds one to select the paired script entry.
enum {
    SHELTER_B6_CORRIDOR_SURFACE1_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x59,
    SHELTER_B6_CORRIDOR_SURFACE1_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5B,
    SHELTER_B6_CORRIDOR_SURFACE2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5D,
    SHELTER_B6_CORRIDOR_SURFACE2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5F,
};

/// Footstep cue bases for surface class 1, the corridor's central floor strip.
///
/// Walking and scripted jumps share a pair; running selects a separate pair
/// in the currently loaded type-1 sound bank. Surface properties borrow this record.
static WorldCollisionFootstepSounds _gShelterB6CorridorSurface1FootstepSounds = {
    .walk         = SHELTER_B6_CORRIDOR_SURFACE1_WALK_SOUND,
    .run          = SHELTER_B6_CORRIDOR_SURFACE1_RUN_SOUND,
    .scriptedJump = SHELTER_B6_CORRIDOR_SURFACE1_WALK_SOUND,
};

/// Footstep cue bases for surface class 2, the corridor's two side floor strips.
///
/// Walking and scripted jumps share a pair; running selects a separate pair
/// in the currently loaded type-1 sound bank. Surface properties borrow this record.
static WorldCollisionFootstepSounds _gShelterB6CorridorSurface2FootstepSounds = {
    .walk         = SHELTER_B6_CORRIDOR_SURFACE2_WALK_SOUND,
    .run          = SHELTER_B6_CORRIDOR_SURFACE2_RUN_SOUND,
    .scriptedJump = SHELTER_B6_CORRIDOR_SURFACE2_WALK_SOUND,
};

/// Blocking, impact-enabled, pushback-enabled policy without footstep cues for classes 0 and 3..7.
static WorldCollisionSurfaceProperties _gShelterB6CorridorDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking, impact-enabled, pushback-enabled central floor policy with surface-1 footstep cues.
static WorldCollisionSurfaceProperties _gShelterB6CorridorSurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB6CorridorSurface1FootstepSounds,
};

/// Blocking, impact-enabled, pushback-enabled side floor policy with surface-2 footstep cues.
static WorldCollisionSurfaceProperties _gShelterB6CorridorSurface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB6CorridorSurface2FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB6CorridorSurfaceProperties[8] = {
    &_gShelterB6CorridorDefaultSurfaceProperties,
    &_gShelterB6CorridorSurface1Properties,
    &_gShelterB6CorridorSurface2Properties,
    &_gShelterB6CorridorDefaultSurfaceProperties,
    &_gShelterB6CorridorDefaultSurfaceProperties,
    &_gShelterB6CorridorDefaultSurfaceProperties,
    &_gShelterB6CorridorDefaultSurfaceProperties,
    &_gShelterB6CorridorDefaultSurfaceProperties,
};

/// Borrowed context of this renderer's active wave task.
///
/// Set on the seed tick and left unchanged at teardown. Only one wave may use
/// this storage; the context must stay live through its final drawing tick.
static ScreenWaveCtx* _gScreenWaveContext = NULL;

/// Column sine oscillators for horizontal screen displacement.
///
/// The per-frame renderer uses entries 0..10; the prebuilt-grid renderer uses
/// 0..8. Any remaining declared capacity is unaccessed; its role is unproven.
static ScreenWaveGridOscillator _gScreenWaveColumnWaves[10] = { 0 };

/// Thirty active row sine oscillators for vertical screen displacement.
///
/// Seeded at wave startup and advanced on drawing updates. Any declared
/// entries beyond 29 are unaccessed; their role is unproven.
static ScreenWaveGridOscillator _gScreenWaveRowWaves[30] = { 0 };

_ShelterB6CorridorScreenWaveGrid gScreenWaveGrid = { { 0 }, { 0 } };

/// Room-entry storage written with the halfword 2; gameplay role and extent are unproven.
static _ShelterB6CorridorStorage51B0 D_shelter_b6_corridor_801851B0;

/// Serial of the newest player-hit glow to start running in this room.
///
/// Each starting instance increments and caches this value; superseded
/// instances retire. The room's wall-glow task resets it on its first tick.
static s32 _gShelterB6CorridorPlayerHitGlowGeneration;

static void _shelterB6CorridorInitRoom(Task* task);
static void _shelterB6CorridorSetImageMaskMode(Task* unusedTask);

#include "../../shared/screen/screen_wave_grid.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Refuses key-item use in this room without changing the scene.
///
/// Message 0x13F1 carries the collected item ID and a zero second payload.
/// All arguments are ignored; returning 0 reports that the item cannot be used.
static s32 _shelterB6CorridorRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedArg)
{
    return 0;
}

/// Resolves a transition and gates departure from the corridor during battle.
///
/// `ROOM_EVENT_MESSAGE_RESOLVE` borrows complete eight-byte, two-byte-aligned
/// records; the reply is writable and may alias the request. Copies the request
/// and resolves the destination variant before testing its original area ID.
/// Eve Elevator requests return zero, running CAP command 1 only on execution.
/// Training Room requests return one only outside an engaged battle; other
/// destinations return one. Retains neither record. Queries never start CAP.
static s32 _shelterB6CorridorResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        SHELTER_B6_CORRIDOR_CAP_BLOCKED_ELEVATOR = 1,
        SHELTER_B6_CORRIDOR_TRANSITION_BLOCKED   = 0,
        SHELTER_B6_CORRIDOR_TRANSITION_ALLOWED   = 1
    };
    u16 destinationArea;
    s32 areaComparison;

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    // The matching code shares this temporary between the load and later comparison.
    areaComparison  = request->areaId;
    destinationArea = areaComparison;
    areaComparison  = GAME_AREA_SHELTER_B6_TRAINING_ROOM;
    if (destinationArea == GAME_AREA_NEO_ARK_EVE_ELEVATOR) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            capRunCommandWithTransition(SHELTER_B6_CORRIDOR_CAP_BLOCKED_ELEVATOR);
        }
        return SHELTER_B6_CORRIDOR_TRANSITION_BLOCKED;
    }
    if (destinationArea == areaComparison) {
        return gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED;
    }
    return SHELTER_B6_CORRIDOR_TRANSITION_ALLOWED;
}

/// Chooses dialogue for the corridor's three Eve parts from destruction and battle state.
///
/// `ROOM_MESSAGE_COMMAND` selects part 0, 1 or 2 with commands 2, 3 or 4.
/// Destroyed parts use CAP commands 5..7, live parts in battle use 2..4, and
/// live parts outside battle use 8..10. Other commands do nothing. Returns zero;
/// the receiver, message ID and second payload word are unused.
static s32 _shelterB6CorridorHandlePartCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum {
        SHELTER_B6_CORRIDOR_COMMAND_PART_0       = 2,
        SHELTER_B6_CORRIDOR_COMMAND_PART_1       = 3,
        SHELTER_B6_CORRIDOR_COMMAND_PART_2       = 4,
        SHELTER_B6_CORRIDOR_CAP_PART_0_BATTLE    = 2,
        SHELTER_B6_CORRIDOR_CAP_PART_1_BATTLE    = 3,
        SHELTER_B6_CORRIDOR_CAP_PART_2_BATTLE    = 4,
        SHELTER_B6_CORRIDOR_CAP_PART_0_DESTROYED = 5,
        SHELTER_B6_CORRIDOR_CAP_PART_1_DESTROYED = 6,
        SHELTER_B6_CORRIDOR_CAP_PART_2_DESTROYED = 7,
        SHELTER_B6_CORRIDOR_CAP_PART_0_IDLE      = 8,
        SHELTER_B6_CORRIDOR_CAP_PART_1_IDLE      = 9,
        SHELTER_B6_CORRIDOR_CAP_PART_2_IDLE      = 10
    };
    switch (command) {
        case SHELTER_B6_CORRIDOR_COMMAND_PART_0:
            if (gameFlagGetNibble(GAME_FLAG_B6_CORRIDOR_EVE_PART_0_DOWN) != 0) {
                capRunCommandWithTransition(SHELTER_B6_CORRIDOR_CAP_PART_0_DESTROYED);
            } else if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                capRunCommandWithTransition(SHELTER_B6_CORRIDOR_CAP_PART_0_BATTLE);
            } else {
                capRunCommandWithTransition(SHELTER_B6_CORRIDOR_CAP_PART_0_IDLE);
            }
            break;
        case SHELTER_B6_CORRIDOR_COMMAND_PART_1:
            if (gameFlagGetNibble(GAME_FLAG_B6_CORRIDOR_EVE_PART_1_DOWN) != 0) {
                capRunCommandWithTransition(SHELTER_B6_CORRIDOR_CAP_PART_1_DESTROYED);
            } else if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                capRunCommandWithTransition(SHELTER_B6_CORRIDOR_CAP_PART_1_BATTLE);
            } else {
                capRunCommandWithTransition(SHELTER_B6_CORRIDOR_CAP_PART_1_IDLE);
            }
            break;
        case SHELTER_B6_CORRIDOR_COMMAND_PART_2:
            if (gameFlagGetNibble(GAME_FLAG_B6_CORRIDOR_EVE_PART_2_DOWN) != 0) {
                capRunCommandWithTransition(SHELTER_B6_CORRIDOR_CAP_PART_2_DESTROYED);
            } else if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
                capRunCommandWithTransition(SHELTER_B6_CORRIDOR_CAP_PART_2_IDLE);
            } else {
                capRunCommandWithTransition(SHELTER_B6_CORRIDOR_CAP_PART_2_BATTLE);
            }
            break;
    }
    return 0;
}

/// Ignores room-action requests and returns 0 without changing the scene.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a request for this dispatch and
/// supplies a zero second payload. This room reads neither payload.
static s32 _shelterB6CorridorIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, DirectionActionRequest* unusedRequest, s32 unusedArg)
{
    return 0;
}

/// Starts the skippable battle-end scene and records its objective.
///
/// Handles every `ROOM_MESSAGE_ACTOR_EVENT` identically, ignoring both words
/// and returning zero. The scene hides/restores the HUD and borrows room-owned
/// normal and skip scripts, which must remain loaded through playback.
static s32 _shelterB6CorridorStartBattleEndScene(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    enum { SHELTER_B6_CORRIDOR_OBJECTIVE_AFTER_BATTLE = 0x2F };

    evsStartScriptWithSkip(_gShelterB6CorridorBattleEndScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gShelterB6CorridorBattleEndSkipScript);
    gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, SHELTER_B6_CORRIDOR_OBJECTIVE_AFTER_BATTLE);
    return 0;
}

/// Sets bit 15 in all 320-by-240 decoded background pixels, preserving their RGB bits.
///
/// `gFsImageWorkspacePtr` must contain the completed RGB16 frame and remain writable
/// without concurrent decoding, DMA or reuse. It updates the resident RAM
/// image for subsequent uploads. The halfword view visits both pixels of each
/// packed GPU word across the complete workspace.
static inline void _shelterB6CorridorMaskImagePixels(void)
{
    u16* pixel;
    s32  pixelIndex;

    pixel = (u16*)gFsImageWorkspacePtr;
    for (pixelIndex = 0; pixelIndex < (s32)(sizeof(*gFsImageWorkspacePtr) / sizeof(*pixel)); pixelIndex++) {
        *pixel |= FILE_SYSTEM_IMAGE_PIXEL_MASK;
        pixel++;
    }
}

/// Registers the room task, masks its loaded background and selects scene music.
///
/// State 0 requires the room's decoded image and session to be loaded. Variant
/// 1 selects scene-music entry 2 and suppresses area/ending music selection.
/// Advances to state 1 after initialization. The write-only room halfword's
/// role remains unproven.
static void _shelterB6CorridorInitRoom(Task* task)
{
    enum {
        SHELTER_B6_CORRIDOR_SCENE_MUSIC_VARIANT = 1,
        SHELTER_B6_CORRIDOR_SCENE_MUSIC_ENTRY   = 2,
    };

    task->msgTable = _gShelterB6CorridorRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _shelterB6CorridorMaskImagePixels();
    D_shelter_b6_corridor_801851B0.field_0 = 2;
    if (gGameSession->location.variant == SHELTER_B6_CORRIDOR_SCENE_MUSIC_VARIANT) {
        gStageCountdownMusicEntry = SHELTER_B6_CORRIDOR_SCENE_MUSIC_ENTRY;
        gGameSession->flowFlags  |= GAME_SESSION_FLOW_SKIP_ENDING_MUSIC;
        gGameSession->flowFlags  |= GAME_SESSION_FLOW_SKIP_AREA_MUSIC;
    }
    task->state++;
}

/// Re-arms pixel bit 15 for the next background-image MDEC decode.
///
/// The decoder resets this one-image mode after starting each decode, so the
/// room's state 1 refreshes it every tick. The task argument is unused.
static void _shelterB6CorridorSetImageMaskMode(Task* unusedTask)
{
    char stackReservation[0x10]; // Unaccessed frame storage; original contents unproven

    gCdCmdQueue.imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
}

/// Room-state dispatch: 0 initialize, 1 maintain the image-mask mode, 2 kill the task.
///
/// Copied as a complete three-callback value and indexed by `Task::state`;
/// no terminator or bounds check. The room must stay loaded through dispatch.
static const TaskFuncTable3 _gShelterB6CorridorRoomStates = {
    { _shelterB6CorridorInitRoom, _shelterB6CorridorSetImageMaskMode, taskKill },
};

void shelterB6CorridorRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gShelterB6CorridorRoomStates;
    states.funcs[task->state](task);
}

/// Releases the scripted enemy's battle hold and requests its exit once per weapon restore.
///
/// Requires placed actor 1 to be live and, for an outstanding battle hold, to
/// carry enemy parameters. If weapon reequip is already pending, does nothing.
/// Otherwise stores the low byte of `endDelayFrames`, requests weapon reequip,
/// credits the enemy's rewards and requests its task exit. Scripts pass 15 on
/// normal completion and 11 on skip. The last battle-hold release replaces the
/// supplied delay with the standard battle-end delay.
static void _shelterB6CorridorFinishScriptedBattle(s32 endDelayFrames)
{
    enum { SCRIPTED_ENEMY_PLACEMENT = 1,
           UNUSED_ENEMY_ID          = 0x31 };

    if (!(gGameSession->flowFlags & GAME_SESSION_FLOW_REEQUIP_WEAPON)) {
        gGameSession->flowFlags                       |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
        gSceneCombatState.signals.bytes.endDelayFrames = endDelayFrames;
        sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(SCRIPTED_ENEMY_PLACEMENT), UNUSED_ENEMY_ID);
        taskCallExit(sceneFindPlacedActor(SCRIPTED_ENEMY_PLACEMENT));
    }
}

/// Locks attachment actions and requests effect cancellation before cutscene staging.
///
/// Used by the normal and skip scripts. Requires live attachment and room-effect
/// state; cancellation is published on the next room-effect update. The lock
/// stays set until ordinary HUD updates resume after the event.
static void _shelterB6CorridorLockAttachmentsAndCancelEffects(void)
{
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    roomEffectRequestCancelAll();
}

/// Draws two wall-light capsule glows from consecutive pairs of world endpoints.
///
/// Borrows four word-aligned `worldPoints`, pairing [0, 1] before [2, 3].
/// The signed low halfword of `radiusScale` gives each end's pixel radius as
/// radiusScale * 64 / (camera Z / 4). `packedColor` bits 8..11, 4..7 and 0..3
/// are RGB nibbles scaled by 16, with bit 3 inserted on odd animation frames.
/// Requires the current view transform, scratch stack, frame arena and ordering
/// table. Each capsule independently rejects negative projection flags and
/// requires nonzero endpoint depths; an accepted capsule queues six additive
/// Gouraud quads and their blend commands, retained until GPU drawing completes.
static inline void _shelterB6CorridorDrawGlowPair(const SVECTOR worldPoints[4], s32 radiusScale, s32 packedColor)
{
    _glowDrawCapsule(&worldPoints[0], radiusScale, packedColor);
    _glowDrawCapsule(&worldPoints[2], radiusScale, packedColor);
}

void shelterB6CorridorDrawViewGlowsTask(Task* task)
{
    enum {
        SHELTER_B6_CORRIDOR_GLOWS_START       = 0,
        SHELTER_B6_CORRIDOR_GLOWS_ACTIVE      = 1,
        SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE = 0x140, // Pixel radius = scale * 64 / (camera Z / 4)
        SHELTER_B6_CORRIDOR_GLOW_COLOR        = 0x442, // RGB nibbles: yellow, with odd-frame bit-3 flicker
    };
    u8 mappedViewIndex;

    if (task->state == SHELTER_B6_CORRIDOR_GLOWS_START) {
        _gShelterB6CorridorPlayerHitGlowGeneration = 0;
        task->state                                = SHELTER_B6_CORRIDOR_GLOWS_ACTIVE;
    }

    // Draw only the wall-light endpoint pairs visible from this camera.
    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 2:
            _shelterB6CorridorDrawGlowPair(&_gShelterB6CorridorWallLightEndpoints[0][0], SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE, SHELTER_B6_CORRIDOR_GLOW_COLOR);
            _shelterB6CorridorDrawGlowPair(&_gShelterB6CorridorWallLightEndpoints[1][0], SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE, SHELTER_B6_CORRIDOR_GLOW_COLOR);
            break;
        case 3:
            _shelterB6CorridorDrawGlowPair(&_gShelterB6CorridorWallLightEndpoints[0][2], SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE, SHELTER_B6_CORRIDOR_GLOW_COLOR);
            _glowDrawCapsule(&_gShelterB6CorridorWallLightEndpoints[0][6], SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE, SHELTER_B6_CORRIDOR_GLOW_COLOR);
            _shelterB6CorridorDrawGlowPair(&_gShelterB6CorridorWallLightEndpoints[1][2], SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE, SHELTER_B6_CORRIDOR_GLOW_COLOR);
            _glowDrawCapsule(&_gShelterB6CorridorWallLightEndpoints[1][6], SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE, SHELTER_B6_CORRIDOR_GLOW_COLOR);
            break;
        case 4:
            _glowDrawCapsule(&_gShelterB6CorridorWallLightEndpoints[1][0], SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE, SHELTER_B6_CORRIDOR_GLOW_COLOR);
            break;
    }
}

#include "../../shared/effects/glow_draw_capsule.inc.c"

void shelterB6CorridorDrawBodyGlow(Task* task)
{
    enum {
        SHELTER_B6_CORRIDOR_BODY_GLOW_ANCHOR_JOINT       = 1,
        SHELTER_B6_CORRIDOR_BODY_GLOW_FIRST_FLASH_JOINT  = 3,
        SHELTER_B6_CORRIDOR_BODY_GLOW_FLASH_JOINT_MASK   = 15,
        SHELTER_B6_CORRIDOR_BODY_GLOW_FLASH_CHANCE_MASK  = 3,
        SHELTER_B6_CORRIDOR_BODY_GLOW_BASE_BRIGHTNESS    = 0x40,
        SHELTER_B6_CORRIDOR_BODY_GLOW_INNER_RADIUS       = 0x200,
        SHELTER_B6_CORRIDOR_BODY_GLOW_OUTER_RADIUS       = 0x400,
        SHELTER_B6_CORRIDOR_BODY_GLOW_FLASH_SIZE_PALETTE = (1 << 16) | 0x80
    };
    GfxCoord* anchorCoord;
    u8        rgb[3];
    u32       brightness;

    anchorCoord = task->extra.tmd->coords + SHELTER_B6_CORRIDOR_BODY_GLOW_ANCHOR_JOINT;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        brightness = ((gDisplayState.animFrame & 1) << 4) + SHELTER_B6_CORRIDOR_BODY_GLOW_BASE_BRIGHTNESS;
        rgb[0]     = brightness;
        rgb[1]     = brightness;
        rgb[2]     = brightness >> 1;
        effectDrawGouraudDisc(anchorCoord, SHELTER_B6_CORRIDOR_BODY_GLOW_INNER_RADIUS, rgb);
        effectDrawGouraudDisc(anchorCoord, SHELTER_B6_CORRIDOR_BODY_GLOW_OUTER_RADIUS, rgb);
        // Advance once for eligibility, and only on success for the joint choice.
        RANDOM_LCG_ADVANCE();
        if ((RANDOM_LCG_DRAW() & SHELTER_B6_CORRIDOR_BODY_GLOW_FLASH_CHANCE_MASK) == 0) {
            RANDOM_LCG_ADVANCE();
            effectSpawn(EFFECT_FLASH_BURST, task->extra.tmd->coords + ((RANDOM_LCG_DRAW() & SHELTER_B6_CORRIDOR_BODY_GLOW_FLASH_JOINT_MASK) + SHELTER_B6_CORRIDOR_BODY_GLOW_FIRST_FLASH_JOINT), SHELTER_B6_CORRIDOR_BODY_GLOW_FLASH_SIZE_PALETTE, NULL);
        }
    }
}

/// Advances the player-hit glow's radius and draws its yellow disc at a composed centre.
///
/// Borrows writable `work`: `scale` is brightness (24..192 in the live task)
/// and `angle` is the base radius numerator, increased by 24 even if culled.
/// RGB is (brightness, brightness, brightness / 2); this call does not fade it.
/// Doubled and quadrupled radii narrow to s16 before perspective scaling by
/// 64 / (camera Z / 4 + 1). `centreCoord` supplies a cached translation in the
/// current view's space; its rotation is unused. Requires the scratch stack,
/// frame arena and ordering table. Accepted projections append 24 additive
/// Gouraud quads and their blend commands, retained until GPU drawing completes.
static inline void _shelterB6CorridorDrawPlayerHitGlow(const GfxCoord* centreCoord, EffectWork* work)
{
    enum { SHELTER_B6_CORRIDOR_HIT_GLOW_RADIUS_STEP = 0x18 };
    u8  yellowRgb[3];
    u16 baseRadius;

    yellowRgb[0] = work->scale;
    yellowRgb[1] = work->scale;
    yellowRgb[2] = work->scale >> 1;
    work->angle += SHELTER_B6_CORRIDOR_HIT_GLOW_RADIUS_STEP;

    // Keep the zero-width band's packets even though its two edges coincide.
    effectDrawOuterGlowBand(centreCoord, (s16)(work->angle * 2), 0, yellowRgb);
    baseRadius = work->angle;
    effectDrawGouraudDisc(centreCoord, (s16)(baseRadius * 4), yellowRgb);
}

void shelterB6CorridorPlayerHitGlowTask(Task* task)
{
    enum {
        SHELTER_B6_CORRIDOR_HIT_GLOW_START              = 0,
        SHELTER_B6_CORRIDOR_HIT_GLOW_ACTIVE             = 1,
        SHELTER_B6_CORRIDOR_HIT_GLOW_INITIAL_BRIGHTNESS = 0xC0,
        SHELTER_B6_CORRIDOR_HIT_GLOW_INITIAL_RADIUS     = 0x200,
        SHELTER_B6_CORRIDOR_HIT_GLOW_FADE_START_TICK    = 9,
        SHELTER_B6_CORRIDOR_HIT_GLOW_BRIGHTNESS_STEP    = 0x18,
    };
    EffectWork* work;
    GfxCoord*   centreCoord;
    s16         effectControl;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    centreCoord   = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    work->age++;
    // Claim the shared generation only when this instance starts running.
    if (task->state == SHELTER_B6_CORRIDOR_HIT_GLOW_START) {
        work->scale = SHELTER_B6_CORRIDOR_HIT_GLOW_INITIAL_BRIGHTNESS;
        work->angle = SHELTER_B6_CORRIDOR_HIT_GLOW_INITIAL_RADIUS;
        _gShelterB6CorridorPlayerHitGlowGeneration++;
        task->state           = SHELTER_B6_CORRIDOR_HIT_GLOW_ACTIVE;
        task->spawnArg1.value = _gShelterB6CorridorPlayerHitGlowGeneration;
    }
    if (task->spawnArg1.value != _gShelterB6CorridorPlayerHitGlowGeneration) {
        effectKillTask(work, task);
        return;
    }
    _shelterB6CorridorDrawPlayerHitGlow(centreCoord, work);
    if (work->age < SHELTER_B6_CORRIDOR_HIT_GLOW_FADE_START_TICK) {
        return;
    }
    // Draw this tick at the old brightness, then fade or retire it.
    work->scale -= SHELTER_B6_CORRIDOR_HIT_GLOW_BRIGHTNESS_STEP;
    if (work->scale < SHELTER_B6_CORRIDOR_HIT_GLOW_BRIGHTNESS_STEP) {
        effectKillTask(work, task);
    }
}

void shelterB6CorridorSetPartDestroyedSprites(u8 partSlot, u8 destroyed)
{
    const GameLocationKey* location = &gGameSession->location;
    SpriteView*            views    = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1];
    SpriteBatch*           batches;
    s32                    slot = partSlot;
    s32                    destroyedState;

    /// Sets the middle part's destroyed-scenery batch visibility in both views.
    ///
    /// Captures SpriteView* views and writable SpriteBatch* batches, leaving
    /// batches at view 2's table. Requires at least three views, with batch 2
    /// writable in views 1 and 2. isHidden is a stable 0/1 expression evaluated
    /// twice: intact parts hide these batches; destroyed parts reveal them.
#define SHELTER_B6_CORRIDOR_SET_PART_1_SPRITES_HIDDEN(isHidden) \
    do {                                                        \
        batches           = views[1].batches;                   \
        batches[2].hidden = (isHidden);                         \
        batches           = views[2].batches;                   \
        batches[2].hidden = (isHidden);                         \
    } while (0)

    // Expose each part's destroyed scenery in the views that can show it.
    if (slot == 0) {
        destroyedState = destroyed;
        if (destroyedState == SHELTER_B6_CORRIDOR_PART_INTACT) {
            batches           = views[1].batches;
            batches[1].hidden = true;
            return;
        }
        if (destroyedState == SHELTER_B6_CORRIDOR_PART_DESTROYED) {
            batches           = views[1].batches;
            batches[1].hidden = false;
            return;
        }
    } else if (slot == 1) {
        destroyedState = destroyed;
        if (destroyedState == SHELTER_B6_CORRIDOR_PART_INTACT) {
            SHELTER_B6_CORRIDOR_SET_PART_1_SPRITES_HIDDEN(true);
            return;
        }
        if (destroyedState == SHELTER_B6_CORRIDOR_PART_DESTROYED) {
            SHELTER_B6_CORRIDOR_SET_PART_1_SPRITES_HIDDEN(false);
            return;
        }
    } else if (slot == 2) {
        destroyedState = destroyed;
        if (destroyedState == SHELTER_B6_CORRIDOR_PART_INTACT) {
            batches           = views[2].batches;
            batches[1].hidden = true;
            return;
        }
        if (destroyedState == SHELTER_B6_CORRIDOR_PART_DESTROYED) {
            batches           = views[2].batches;
            batches[1].hidden = false;
        }
    }
#undef SHELTER_B6_CORRIDOR_SET_PART_1_SPRITES_HIDDEN
}
