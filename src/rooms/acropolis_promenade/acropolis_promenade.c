#include "rooms/acropolis_promenade.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/stream_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/bridge_model.h"
#include "../../shared/effects/acropolis_glows.h"

static void _acropolisPromenadeInitializeRoomTask(Task* task);
static void _acropolisPromenadeUpdateRoomProgress(Task* unusedTask);
static void _acropolisPromenadeUpdateBridgeVisibility(Task* task);

static void _acropolisPromenadeMoviePathTask(Task* task);
static void _acropolisPromenadeMovieSkipFadeOutTask(Task* task);
static void _acropolisPromenadeSceneFadeInTask(Task* task);

static WorldCollisionGrid    _gAcropolisPromenadeRoom1CollisionGrid;
static WorldCollisionGrid    _gAcropolisPromenadeRoom2CollisionGrid;
static WorldCollisionTrigger _gAcropolisPromenadeViewBoundaryTriggers[6];
static WorldCollisionTrigger _gAcropolisPromenadeActionTriggers[6];
static WorldCoordRoomLights  _gAcropolisPromenadeRoomLights;

static s32  _acropolisPromenadeResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _acropolisPromenadeRefuseKeyItem(Task* task, s32 messageId, s32 itemId, s32 secondArg);
static s32  _acropolisPromenadeHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32  _acropolisPromenadeHandleSoundCue(Task* unusedTask, s32 messageId, s32 soundCue, s32 unusedArg);
static s32  _acropolisPromenadeIgnoreRoomAction(Task* task, s32 messageId, s32 firstArg, s32 secondArg);
static void _acropolisPromenadeTranslucentPlaneTask(Task* task);

/// Room-local binding of the key-item-use request; the first payload is an item ID.
enum { ACROPOLIS_PROMENADE_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Overlay-lifetime arrival latch and battle follow-up states, stored as s32.
enum {
    ACROPOLIS_PROMENADE_ARRIVAL_MOVIE_NOT_STARTED = 0,
    ACROPOLIS_PROMENADE_ARRIVAL_MOVIE_STARTED     = 1,
    ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_INACTIVE  = 0,
    ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_PENDING   = 2,
};

/// CAP command and signed pan offset used by the variant-1 battle follow-up.
enum {
    ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_CAP_COMMAND = 10,
    ACROPOLIS_PROMENADE_BATTLE_SOUND_PAN_OFFSET     = 13,
};

/// Fixed plane scheduling priority and roles of the glow group's position slots.
enum {
    ACROPOLIS_PROMENADE_PLANE_TASK_PRIORITY = 192,
    ACROPOLIS_PROMENADE_GLOW_DISC_POSITION  = 0,
    ACROPOLIS_PROMENADE_GLOW_FIRST_STAR     = 1,
    ACROPOLIS_PROMENADE_GLOW_SECOND_STAR    = 2,
};

/// Mapped camera/image view bits used by the room effect selectors.
enum {
    ACROPOLIS_PROMENADE_EFFECT_VIEW_1  = 1 << 0,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_2  = 1 << 1,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_3  = 1 << 2,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_4  = 1 << 3,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_5  = 1 << 4,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_6  = 1 << 5,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_7  = 1 << 6,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_8  = 1 << 7,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_9  = 1 << 8,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_10 = 1 << 9,
};

/// Live indices in the movie task descriptor table; slots 0 and 1 are unused.
enum {
    ACROPOLIS_PROMENADE_MOVIE_PATH_TASK       = 2,
    ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_TASK  = 3,
    ACROPOLIS_PROMENADE_MOVIE_SCENE_FADE_TASK = 4,
};

static TmdBone _gAcropolisPromenadeAcropolisBridgeModel0AD9CSkeleton[1] = {
#include "assets/acropolis_bridge_model_0AD9C_skeleton.inc"
};

static u32 _gAcropolisPromenadeAcropolisBridgeModel0AD9CPartVerts[1] = {
#include "assets/acropolis_bridge_model_0AD9C_partVerts.inc"
};

static SVECTOR _gAcropolisPromenadeAcropolisBridgeModel0AD9CVerts[171] = {
#include "assets/acropolis_bridge_model_0AD9C_verts.inc"
};

static u32 _gAcropolisPromenadeAcropolisBridgeModel0AD9CStream[691] = {
#include "assets/acropolis_bridge_model_0AD9C_stream.inc"
};

static TmdSource _gAcropolisPromenadeAcropolisBridgeModel0AD9C = {
    0,
    5480,
    0,
    1,
    _gAcropolisPromenadeAcropolisBridgeModel0AD9CPartVerts,
    _gAcropolisPromenadeAcropolisBridgeModel0AD9CVerts,
    &_gAcropolisPromenadeAcropolisBridgeModel0AD9CVerts[171],
    _gAcropolisPromenadeAcropolisBridgeModel0AD9CSkeleton,
    _gAcropolisPromenadeAcropolisBridgeModel0AD9CStream,
};

/// Room-message handlers for exits, CAP commands, actions, key items and sounds.
///
/// Borrowed by the room receiver until it is released; this overlay and its
/// callbacks must remain loaded. TASK_MESSAGE_TABLE_END makes unknown IDs
/// return zero. The room-action entry ignores its payload and leaves the
/// result unset, so its sender must discard that result.
static TaskMessageEntry _gAcropolisPromenadeRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisPromenadeResolveRoomTransition },
    { ROOM_MESSAGE_COMMAND, _acropolisPromenadeHandleRoomCommand },
    { DIRECTION_MESSAGE_ROOM_ACTION, _acropolisPromenadeIgnoreRoomAction },
    { ACROPOLIS_PROMENADE_MESSAGE_USE_KEY_ITEM, _acropolisPromenadeRefuseKeyItem },
    { ROOM_MESSAGE_SOUND, _acropolisPromenadeHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Spawn recipe for the fixed translucent plane, followed by TASK_DESC_END.
///
/// Index 0 creates a priority-192 TMD task borrowing this overlay's bridge
/// model. Spawning copies the recipe; its callback and model must stay loaded
/// through teardown. The room initializer retains the result, including NULL.
static TaskDesc _gAcropolisPromenadeTranslucentPlaneTaskDescriptors[2] = {
    { { { TASK_BODY_TMD, ACROPOLIS_PROMENADE_PLANE_TASK_PRIORITY } }, _acropolisPromenadeTranslucentPlaneTask, { .model = &_gAcropolisPromenadeAcropolisBridgeModel0AD9C } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Player animation request starting the variant-1 battle's follow-up script.
///
/// Plays clip 1 with a reset blend and world collision disabled. The interpreter
/// copies this request and replaces its bank index with the equipped weapon's
/// bank; the stored index is retained serialization, not a fixed bank choice.
/// Keep the overlay loaded while the event script borrows the request.
static const AnimationPlayRequest _gAcropolisPromenadeBattleFollowupAnimationRequest __attribute__((section(".data"))) = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Two retained records with word XYZ and halfword Euler-angle layout.
///
/// No consumer establishes their intended actors, coordinate frame or purpose.
static ActorTransform D_acropolis_promenade_80180ED0[2] = {
    { { -1454, 171, -1843, 0 }, { 0, 2048, 0, 0 } },
    { { -999, 171, -2799, 0 }, { 0, 1324, 0, 0 } },
};

/// Variant-1 post-battle scene, synchronized to CAP command 10's four cues.
///
/// Starts after an observed engaged battle ends and battle reset is pending.
/// Plays the player's weapon-bank clip, hides its model between CAP cues,
/// starts room sound entries 12 and 15, then resumes control, restores the
/// saved view and shows the model. The interpreter borrows this END-terminated
/// stream and its animation request; keep the overlay loaded until it ends.
static EvsCommand _gAcropolisPromenadeBattleFollowupScript[15] = {
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisPromenadeBattleFollowupAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 12) }, { .value = ACROPOLIS_PROMENADE_BATTLE_SOUND_PAN_OFFSET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 15) }, { .value = ACROPOLIS_PROMENADE_BATTLE_SOUND_PAN_OFFSET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip exit for the battle follow-up: conceal cleanup, then resume the player.
///
/// Fades with GPU_BLEND_AVERAGE, restores the saved view, aborts CAP scene
/// playback, returns the fade and ends scripted player control. This borrowed
/// nine-command stream ends with END and must stay loaded through completion.
static EvsCommand _gAcropolisPromenadeBattleFollowupSkipScript[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Warp-4 arrival movie latch (0 not started, 1 spawn already attempted).
///
/// Reset by loading this overlay; set before spawning, even if spawning fails.
static s32 _gAcropolisPromenadeArrivalMovieStarted = ACROPOLIS_PROMENADE_ARRIVAL_MOVIE_NOT_STARTED;

/// Battle follow-up phase (0 inactive, 1 engaged observed, 2 awaiting reset).
///
/// Variant 1 mirrors SCENE_COMBAT_BATTLE_ENGAGED, then waits for battle reset
/// after that phase changes. Reset to inactive before starting the scene.
/// Stored as a signed word and initialized again when this overlay is loaded.
static s32 _gAcropolisPromenadeBattleFollowupState = ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_INACTIVE;

/// Bodyless movie-path and skip-fade spawn recipes, indexed by MOVIE_*_TASK.
///
/// Slots 0 and 1 retain unused null callbacks; spawn only slots 2..4. All use
/// priority 192. This fixed-size indexed table has no TASK_DESC_END record.
/// Spawning copies a recipe synchronously; its callback requires this overlay
/// to remain loaded through task teardown.
static TaskDesc _gAcropolisPromenadeMovieTaskDescriptors[5] = {
    { { { TASK_BODY_NONE, 192 } }, NULL, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, NULL, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPromenadeMoviePathTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPromenadeMovieSkipFadeOutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisPromenadeSceneFadeInTask, { .value = 0 } },
};

/// Player-root positions for the warp-4 arrival movie, in whole room units.
///
/// Index by 69 - movieFrame and subtract 200 from Z. The consumer requires
/// movieFrame in 1..69 (sample indices 0..68) and hands off to scripted walking
/// at frame 64 or later. All 300 authored samples remain stored; the movie's
/// frame limit is 300, so its limit alone does not guarantee safe indexing.
/// Borrowed only during the movie-path task; keep this overlay loaded.
static SVECTOR _gAcropolisPromenadeArrivalMoviePlayerPositions[300] = {
#include "assets/acropolis_promenade_path_03BC4.inc"
};

/// X/Z unit signs of the ground-glow square's four corners in GPU strip order.
///
/// Axis 0 is local X and axis 1 is local Z. The drawer scales each sign by its
/// 768-game-unit half-side before applying the body's composed transform.
static const EffectUnitQuadCorner _gAcropolisPromenadeGroundGlowUnitCorners[4] __attribute__((section(".data"))) = {
    { -1, 1 },
    { 1, 1 },
    { -1, -1 },
    { 1, -1 },
};

/// Centres of the tinted disc (index 0) and two star glows (indices 1 and 2).
///
/// XYZ use whole room units. The disc drawer reads a world point directly;
/// effect spawning copies each star position under the room coordinate.
/// The glow view mask selects this group, initially mapped view 6 only.
static SVECTOR _gAcropolisPromenadeGlowPositions[3] = {
    { -4700, -1000, -620, 0 },
    { -3920, -1494, -3450, 0 },
    { -6900, -1494, -3564, 0 },
};

/// Ground-glow centre in whole room units, copied under the room coordinate.
///
/// Spawned with the star/disc group selected by the glow view mask.
static SVECTOR _gAcropolisPromenadeGroundGlowPosition = { -6900, 300, -2543, 0 };

/// Twelve room-local lamp-glow centres, paired with the lamp view masks.
///
/// XYZ use whole room units and are copied by effect spawning. Slots 0..2
/// select texture variant 0; 3/4, 7/8 and 11 select variant 1; 5/6 and 9/10
/// select variant 2. Preserve this grouping when indexing the parallel masks.
static SVECTOR _gAcropolisPromenadeLampGlowPositions[12] = {
    { 810, -3000, -3720, 0 },
    { 650, -3000, -4000, 0 },
    { 810, -3000, -4270, 0 },
    { -2470, -2050, -740, 0 },
    { -2520, -2050, -3120, 0 },
    { -16000, -1700, -1330, 0 },
    { -16000, -1700, -2700, 0 },
    { 1680, -1700, 10080, 0 },
    { 1680, -1700, 8430, 0 },
    { 1640, -360, 7020, 0 },
    { -2150, -360, 8460, 0 },
    { -3400, -370, 2300, 0 },
};

/// Mapped-view bits enabling the disc, stars and ground glow; initially view 6.
///
/// Bit (mappedView - 1) selects a view, independently of logical view IDs.
static u16 _gAcropolisPromenadeGlowViewMask = ACROPOLIS_PROMENADE_EFFECT_VIEW_6;

/// Mapped-view bits enabling screen-drip seeding and renewal; initially view 6.
///
/// Bit (mappedView - 1) selects a view. Entering a selected view seeds forty
/// drips; subsequent updates spawn two. Kept separate from the glow selector.
static u16 _gAcropolisPromenadeDripViewMask = ACROPOLIS_PROMENADE_EFFECT_VIEW_6;

/// Mapped-view visibility bits for the twelve corresponding lamp centres.
///
/// Bit (mappedView - 1) selects a view; this table has no terminator. The room
/// effect task suppresses mapped view 7 before testing masks, so slot 9 never
/// emits and the view-7 bits in slots 7 and 8 also remain suppressed.
static u16 _gAcropolisPromenadeLampGlowViewMasks[12] = {
    ACROPOLIS_PROMENADE_EFFECT_VIEW_2 | ACROPOLIS_PROMENADE_EFFECT_VIEW_4 | ACROPOLIS_PROMENADE_EFFECT_VIEW_5 | ACROPOLIS_PROMENADE_EFFECT_VIEW_6 | ACROPOLIS_PROMENADE_EFFECT_VIEW_8 | ACROPOLIS_PROMENADE_EFFECT_VIEW_9,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_4 | ACROPOLIS_PROMENADE_EFFECT_VIEW_5 | ACROPOLIS_PROMENADE_EFFECT_VIEW_6 | ACROPOLIS_PROMENADE_EFFECT_VIEW_8 | ACROPOLIS_PROMENADE_EFFECT_VIEW_9,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_4 | ACROPOLIS_PROMENADE_EFFECT_VIEW_5 | ACROPOLIS_PROMENADE_EFFECT_VIEW_6 | ACROPOLIS_PROMENADE_EFFECT_VIEW_8 | ACROPOLIS_PROMENADE_EFFECT_VIEW_9,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_1 | ACROPOLIS_PROMENADE_EFFECT_VIEW_5 | ACROPOLIS_PROMENADE_EFFECT_VIEW_8 | ACROPOLIS_PROMENADE_EFFECT_VIEW_9,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_1 | ACROPOLIS_PROMENADE_EFFECT_VIEW_5 | ACROPOLIS_PROMENADE_EFFECT_VIEW_8 | ACROPOLIS_PROMENADE_EFFECT_VIEW_9,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_2 | ACROPOLIS_PROMENADE_EFFECT_VIEW_5,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_2 | ACROPOLIS_PROMENADE_EFFECT_VIEW_5,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_2 | ACROPOLIS_PROMENADE_EFFECT_VIEW_3 | ACROPOLIS_PROMENADE_EFFECT_VIEW_4 | ACROPOLIS_PROMENADE_EFFECT_VIEW_5 | ACROPOLIS_PROMENADE_EFFECT_VIEW_7 | ACROPOLIS_PROMENADE_EFFECT_VIEW_8 | ACROPOLIS_PROMENADE_EFFECT_VIEW_9,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_2 | ACROPOLIS_PROMENADE_EFFECT_VIEW_3 | ACROPOLIS_PROMENADE_EFFECT_VIEW_4 | ACROPOLIS_PROMENADE_EFFECT_VIEW_5 | ACROPOLIS_PROMENADE_EFFECT_VIEW_6 | ACROPOLIS_PROMENADE_EFFECT_VIEW_7 | ACROPOLIS_PROMENADE_EFFECT_VIEW_8 | ACROPOLIS_PROMENADE_EFFECT_VIEW_9,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_7,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_10,
    ACROPOLIS_PROMENADE_EFFECT_VIEW_6,
};

WorldCollisionRoomResources gAcropolisPromenadeRoomCollisionResources[2] = {
    { &_gAcropolisPromenadeRoom1CollisionGrid, _gAcropolisPromenadeViewBoundaryTriggers, _gAcropolisPromenadeActionTriggers, NULL },
    { &_gAcropolisPromenadeRoom2CollisionGrid, _gAcropolisPromenadeViewBoundaryTriggers, _gAcropolisPromenadeActionTriggers, NULL },
};

/// Room 2's logical-view to camera/image/sprite mapping.
///
/// Only the first thirteen bytes are view entries, indexed by logical view
/// minus one. The final three serialized zero bytes lie outside that domain.
static u8 _gAcropolisPromenadeRoom2ViewMap[16] = {
    1,
    2,
    3,
    8,
    9,
    6,
    7,
    4,
    5,
    10,
    11,
    12,
    13,
    0,
    0,
    0,
};

u8* gAcropolisPromenadeViewMaps[2] = {
    gViewIdentityMap,
    _gAcropolisPromenadeRoom2ViewMap,
};

ViewCount gAcropolisPromenadeViewCounts[2] = { 13, 13 };

WorldCoordRoomLighting gAcropolisPromenadeRoomLightingTable[2] = {
    { &_gAcropolisPromenadeRoomLights, NULL },
    { &_gAcropolisPromenadeRoomLights, NULL },
};

/// Map markers shared with the destination endpoints of the two marked exits.
enum {
    ACROPOLIS_PROMENADE_SANCTUARY_EXIT_MAP_FLAG = 493,
    ACROPOLIS_PROMENADE_BRIDGE_EXIT_MAP_FLAG    = 491,
};

DirectionWarpEntry gAcropolisPromenadeWarpEntries[5] = {
    { { { .word = 3072 }, 642, 42, 9120 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 642, 42, 9120 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 373, 42, -4230 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 373, 42, -4230 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 6), SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 5), DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, ACROPOLIS_PROMENADE_SANCTUARY_EXIT_MAP_FLAG },
    { { { .word = 1024 }, -0x3D4E, 0, -2098 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -0x3D4E, 0, -2098 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 8), SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 7), DIRECTION_WARP_SOUND_NONE, 8, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 642, 42, 9120 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 642, 42, 9120 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -2197, 44, -1977 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -2197, 44, -1977 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 0x0E), SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 0x0D), DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, ACROPOLIS_PROMENADE_BRIDGE_EXIT_MAP_FLAG },
};

/// Room 1 grid's room-axis normals, with 4096 representing unit length.
static SVECTOR _gAcropolisPromenadeRoom1CollisionGridNormals[30] = {
#include "assets/acropolis_promenade_collision_04E1C_normals.inc"
};

/// Room 1 grid's vertices in signed game-coordinate units.
static SVECTOR _gAcropolisPromenadeRoom1CollisionGridVerts[93] = {
#include "assets/acropolis_promenade_collision_04E1C_verts.inc"
};

/// Room 1 grid's quads, indexing its vertex/normal pools and surface classes 0/1.
static WorldCollisionGridFace _gAcropolisPromenadeRoom1CollisionGridFaces[38] = {
#include "assets/acropolis_promenade_collision_04E1C_faces.inc"
};

/// Serialized room 1 cell face-index lists, each ending at a signed -1 marker.
static s16 _gAcropolisPromenadeRoom1CollisionGridCells[144] = {
#include "assets/acropolis_promenade_collision_04E1C_cells.inc"
};

/// Resolves a generated face-list start in this grid's packed halfword pool.
///
/// halfwordOffset counts signed-halfword elements and is evaluated once.
/// It must select an in-bounds list start in the captured pool.
/// Captures `_gAcropolisPromenadeRoom1CollisionGridCells`; returns a borrowed s16* without reading it.
/// The table include supplies list-start constants; empty cells use NULL.
/// Define only for that include and undefine immediately afterward.
#define ACROPOLIS_PROMENADE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gAcropolisPromenadeRoom1CollisionGridCells[(halfwordOffset)])
/// Room 1 grid's ten cell lists, indexed as x * 5 + z.
static s16* _gAcropolisPromenadeRoom1CollisionGridTable[10] = {
#include "assets/acropolis_promenade_collision_04E1C_table.inc"
};
#undef ACROPOLIS_PROMENADE_CELL_FACE_LIST_AT_OFFSET

/// Mutable collision grid for promenade room 1, bound to the view on activation.
///
/// The 2-by-5 X-major grid uses 4000-unit square cells and room-axis biases
/// (2780, 8590). Its mesh and cell lists remain borrowed while the room is loaded.
static WorldCollisionGrid _gAcropolisPromenadeRoom1CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gAcropolisPromenadeRoom1CollisionGridNormals,
    .vertices    = _gAcropolisPromenadeRoom1CollisionGridVerts,
    .faces       = _gAcropolisPromenadeRoom1CollisionGridFaces,
    .cellFaceIds = _gAcropolisPromenadeRoom1CollisionGridTable,
    .xBias       = 2780,
    .zBias       = 8590,
    .cellCountX  = 2,
    .cellCountZ  = 5,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gAcropolisPromenadeRoom1CollisionGridFaces),
};

/// Room 2 grid's room-axis normals, with 4096 representing unit length.
static SVECTOR _gAcropolisPromenadeRoom2CollisionGridNormals[30] = {
#include "assets/acropolis_promenade_collision_05610_normals.inc"
};

/// Room 2 grid's vertices in signed game-coordinate units.
static SVECTOR _gAcropolisPromenadeRoom2CollisionGridVerts[103] = {
#include "assets/acropolis_promenade_collision_05610_verts.inc"
};

/// Room 2 grid's indexed triangles/quads, using surface classes 0, 1, 2 and 4.
static WorldCollisionGridFace _gAcropolisPromenadeRoom2CollisionGridFaces[46] = {
#include "assets/acropolis_promenade_collision_05610_faces.inc"
};

/// Serialized room 2 cell face-index lists, ending at signed -1 markers.
static s16 _gAcropolisPromenadeRoom2CollisionGridCells[172] = {
#include "assets/acropolis_promenade_collision_05610_cells.inc"
};

/// Resolves a generated face-list start in this grid's packed halfword pool.
///
/// halfwordOffset counts signed-halfword elements and is evaluated once.
/// It must select an in-bounds list start in the captured pool.
/// Captures `_gAcropolisPromenadeRoom2CollisionGridCells`; returns a borrowed s16* without reading it.
/// The table include supplies list-start constants; empty cells use NULL.
/// Define only for that include and undefine immediately afterward.
#define ACROPOLIS_PROMENADE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gAcropolisPromenadeRoom2CollisionGridCells[(halfwordOffset)])
/// Room 2 grid's ten cell lists, indexed as x * 5 + z.
static s16* _gAcropolisPromenadeRoom2CollisionGridTable[10] = {
#include "assets/acropolis_promenade_collision_05610_table.inc"
};
#undef ACROPOLIS_PROMENADE_CELL_FACE_LIST_AT_OFFSET

/// Mutable collision grid for promenade room 2, bound to the view on activation.
///
/// The 2-by-5 X-major grid uses 4000-unit square cells and room-axis biases
/// (2780, 8590). Its mesh and cell lists remain borrowed while the room is loaded.
static WorldCollisionGrid _gAcropolisPromenadeRoom2CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gAcropolisPromenadeRoom2CollisionGridNormals,
    .vertices    = _gAcropolisPromenadeRoom2CollisionGridVerts,
    .faces       = _gAcropolisPromenadeRoom2CollisionGridFaces,
    .cellFaceIds = _gAcropolisPromenadeRoom2CollisionGridTable,
    .xBias       = 2780,
    .zBias       = 8590,
    .cellCountX  = 2,
    .cellCountZ  = 5,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gAcropolisPromenadeRoom2CollisionGridFaces),
};

/// Directed view boundaries shared by promenade rooms 1 and 2.
///
/// The six quads connect logical views 2/3, 3/4 and 4/5 in both directions.
/// Positions/radii use game units and normals use 4096 per unit. The last quad
/// carries LAST and is included in the scan. Setup and collision mutate the
/// links, view parent, enable flags and hit latch; keep storage live until unlinked.
static WorldCollisionTrigger _gAcropolisPromenadeViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { -802, -2544, 510, 0 }, { { -1820, -4175, 362, 0 }, { 1820, -4209, -362, 0 }, { -1820, 4209, 362, 0 }, { 1820, 4175, -362, 0 } }, { -800, 0, -4019, 0 }, { 0, 0, ONE, 0 }, 4579, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -804, -2432, 91, 0 }, { { 1820, -4175, -361, 0 }, { -1820, -4209, 362, 0 }, { 1820, 4209, -361, 0 }, { -1820, 4175, 362, 0 } }, { 797, 0, 4017, 0 }, { 0, 0, ONE, 0 }, 4579, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -288, -2272, 4160, 0 }, { { -1856, -4175, 0, 0 }, { 1856, -4209, 0, 0 }, { -1856, 4209, 0, 0 }, { 1856, 4175, 0, 0 } }, { 0, 0, -4098, 0 }, { 0, 0, ONE, 0 }, 4579, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -256, -2432, 3681, 0 }, { { 1856, -4175, 0, 0 }, { -1856, -4209, 0, 0 }, { 1856, 4209, 0, 0 }, { -1856, 4175, 0, 0 } }, { 0, 0, 4097, 0 }, { 0, 0, ONE, 0 }, 4579, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -997, -2624, -5124, 0 }, { { 1843, -4175, -185, 0 }, { -1850, -4209, 179, 0 }, { 1843, 4209, -185, 0 }, { -1850, 4175, 179, 0 } }, { 401, 0, 4076, 0 }, { 0, 0, ONE, 0 }, 4579, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -999, -2560, -4839, 0 }, { { -1854, -4175, 90, 0 }, { 1851, -4209, -93, 0 }, { -1854, 4209, 90, 0 }, { 1851, 4175, -93, 0 } }, { -203, 0, -4100, 0 }, { 0, 0, ONE, 0 }, 4579, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Interaction quads shared by promenade rooms 1 and 2.
///
/// Three warps target the observatory, sanctuary and bridge; other quads request
/// CAP commands 3 and 5 and automatic room action 6. Warp parameter1 packs the
/// local departure slot in its high nibble and the destination arrival slot in
/// its low nibble. Positions/radii use game units and normals use 4096 per unit.
/// The LAST-marked sixth quad is live. Setup and collision mutate list state,
/// coordinates and hits; keep storage live until unlinked.
static WorldCollisionTrigger _gAcropolisPromenadeActionTriggers[6] = {
    { NULL, NULL, NULL, { 989, -112, 9028, 0 }, { { -1024, 0, -1024, 0 }, { 1024, 0, -1024, 0 }, { -1024, 0, 1024, 0 }, { 1024, 0, 1024, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 1448, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_ACROPOLIS_OBSERVATORY, (4 << 4) | 3, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 752, -112, -3903, 0 }, { { -496, 0, -1215, 0 }, { 496, 0, -1215, 0 }, { -496, 0, 1216, 0 }, { 496, 0, 1216, 0 } }, { 0, 4105, 0, 0 }, { -ONE, 0, 0, 0 }, 1311, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_ACROPOLIS_SANCTUARY, (2 << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1408, -105, 7680, 0 }, { { -1024, 0, -1088, 0 }, { 1024, 0, -1088, 0 }, { -1024, 0, 1088, 0 }, { 1024, 0, 1088, 0 } }, { 0, 4102, 0, 0 }, { 4091, 0, 201, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2432, -96, -8512, 0 }, { { -1024, 0, -1024, 0 }, { 1568, 0, -1024, 0 }, { -1024, 0, 1472, 0 }, { 1568, 0, 1472, 0 } }, { 0, 4097, 0, 0 }, { 600, 0, 4051, 0 }, 2141, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2208, -128, -2048, 0 }, { { -576, 0, -1024, 0 }, { 576, 0, -1024, 0 }, { -576, 0, 1024, 0 }, { 576, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4094, 0, -1, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_ACROPOLIS_BRIDGE, (5 << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1152, -64, -2608, 0 }, { { -2336, 0, -272, 0 }, { 2336, 0, -752, 0 }, { -2336, 0, 752, 0 }, { 2336, 0, 272, 0 } }, { 0, 4101, 0, 0 }, { -ONE, 0, 0, 0 }, 2442, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 6, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Variant 1's actor-301100 resource, keyed by placement entry 11.
///
/// Selects base-30 file 11 and task slot 0 for both matching placements.
/// AREA_PLACEMENT_END terminates this two-record list; all pointers are borrowed
/// from loaded overlays through area loading and spawning.
static AreaResource _gAcropolisPromenadeVariant1Resources[2] = {
    { .entryId = 11, .fileNumber = 11, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor301100Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's Rat and Moth resources, keyed by placement entries 7 and 8.
///
/// Both load base-20 file 7 and spawn task slot 0 from their respective recipes.
/// AREA_PLACEMENT_END terminates this three-record list. Keep resource and
/// task-descriptor overlays loaded through area loading and spawning.
static AreaResource _gAcropolisPromenadeVariant2Resources[3] = {
    { .entryId = 7, .fileNumber = 7, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor200700RatTaskDesc },
    { .entryId = 8, .fileNumber = 7, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor00700MothTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 3's Maggot/Caterpillar and Moth resources, keyed by entries 55 and 8.
///
/// Entry 55 loads base-10 file 55; entry 8 loads base-20 file 7. Both spawn task
/// slot 0. AREA_PLACEMENT_END terminates this three-record list; resource and
/// task pointers require their overlays loaded through loading and spawning.
static AreaResource _gAcropolisPromenadeVariant3Resources[3] = {
    { .entryId = 55, .fileNumber = 55, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor105500MaggotCaterpillarTasks },
    { .entryId = 8, .fileNumber = 7, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor00700MothTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 7's GOLEM resource, keyed by placement entry 22.
///
/// Loads base-40 file 22 and spawns task slot 0. AREA_PLACEMENT_END terminates
/// this two-record list; resource and task pointers borrow loaded overlays.
static AreaResource _gAcropolisPromenadeVariant7Resources[2] = {
    { .entryId = 22, .fileNumber = 22, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor402200GolemTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 8's Maggot/Caterpillar and Moth resources, keyed by entries 26 and 8.
///
/// Entry 26 loads base-10 file 26; entry 8 loads base-20 file 7. Both spawn task
/// slot 0. AREA_PLACEMENT_END terminates this three-record list; resource and
/// task pointers require their overlays loaded through loading and spawning.
static AreaResource _gAcropolisPromenadeVariant8Resources[3] = {
    { .entryId = 26, .fileNumber = 26, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor02600MaggotCaterpillarBodyTask },
    { .entryId = 8, .fileNumber = 7, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor00700MothTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gAcropolisPromenadeAreaVariants[17] = {
    { NULL, NULL },
    { gMapAkropolisPromenadeVariant1Placements, _gAcropolisPromenadeVariant1Resources },
    { gMapAkropolisPromenadeVariant2Placements, _gAcropolisPromenadeVariant2Resources },
    { gMapAkropolisPromenadeVariant3Placements, _gAcropolisPromenadeVariant3Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { gMapAkropolisPromenadeVariant7Placements, _gAcropolisPromenadeVariant7Resources },
    { gMapAkropolisPromenadeVariant8Placements, _gAcropolisPromenadeVariant8Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

/// Authored point lights for model shading in every promenade view.
///
/// Positions and falloff radii use world units; RGB intensities have 12 fractional
/// bits (`ONE` is full strength). The room overlay owns this writable storage:
/// coordinate updates set its view parent and cached transforms, and lighting
/// queries overwrite attenuation. Borrowed pointers expire when the room unloads.
static WorldCoordPointLight _gAcropolisPromenadePointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1367, -507, 6799 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { ONE, 3686, 2457 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -335, -2500, 3516 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, ONE, ONE },
        },
        .inner = 0,
        .outer = 0x2B5C,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1970, -507, 8207 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { ONE, 3686, 2457 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1342, -2000, 0x2854 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, ONE, ONE },
        },
        .inner = 200,
        .outer = 9000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -80, -2500, -3995 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, ONE, ONE },
        },
        .inner = 500,
        .outer = 0x3714,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2333, -2000, -763 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 3276, 2867 },
        },
        .inner = 200,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2333, -2000, -3192 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 3276, 2867 },
        },
        .inner = 200,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6854, -1500, -3373 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 2457, 2457 },
        },
        .inner = 500,
        .outer = 3955,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -9907, -1500, -3373 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 2457, 2457 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x324D, -1500, -3373 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 2457, 2457 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x33A3, -1500, -719 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 2457, 2457 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2808, -1500, -719 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 2457, 2457 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7195, -1500, -719 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3604, 2457, 2457 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x38AB, 1600, -4070 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2D1E, 1600, -4070 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5464, 1600, -4070 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 500,
        .outer = 5179,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8422, 1600, -4070 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8422, 1600, -22 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5464, 1600, -22 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 501,
        .outer = 3961,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2D1E, 1600, -22 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x38AB, 1600, -22 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2457, 3686, ONE },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x3D7E, 2000, -3035 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { ONE, ONE, 3276 },
        },
        .inner = 200,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x3D7E, 2000, -994 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { ONE, ONE, 3276 },
        },
        .inner = 200,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -80, -2500, -8518 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, ONE, ONE },
        },
        .inner = 500,
        .outer = 0x3714,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4630, -1500, -3219 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 3686, 2457, 2457 },
        },
        .inner = 0,
        .outer = 2,
    },
};

/// Point-light collection shared by both promenade rooms for model shading.
///
/// No directional or cone lights are supplied. The room owns the mutable light
/// array, whose coordinate caches and attenuation change during lighting updates.
static WorldCoordRoomLights _gAcropolisPromenadeRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gAcropolisPromenadePointLights),
    .pointLights           = _gAcropolisPromenadePointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Empty sprite list for mapped views 1 and 13, retaining decoded image strips.
static SpriteBatch _gAcropolisPromenadeViews1And13SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 2's 74 sprite rectangles, with pixel positions and unscaled OT depths.
static SpriteSource _gAcropolisPromenadeView2SpriteSources[74] = {
    { 141, 0x3FC0, { .fields = { 72, 24 } }, -160, 96, 450, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, -160, -32, 450, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -160, 24, 450, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, -120, 427, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -32, -112, 435, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -160, -120, 750, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -152, -120, 875, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -120, -104, 951, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -96, -80, 988, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -152, -80, 1148, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, -72, 1096, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, -56, 1040, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, -48, 994, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, -72, -48, 963, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -40, -56, 950, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -40, -120, 950, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -32, -104, 950, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, -72, 950, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -80, -24, 973, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -144, -24, 875, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, -128, -8, 875, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -104, 8, 925, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -56, 8, 925, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -48, 32, 925, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, -64, 56, 925, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -160, 16, 875, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -144, 32, 875, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, 48, 875, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 40 } }, -112, 56, 925, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -160, -40, 875, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, -120, 487, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -48, 950, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -160, -8, 462, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, -112, 8, 462, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -128, 64, 462, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -160, 80, 462, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, -152, 96, 450, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -160, -112, 450, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -144, -112, 450, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 40 } }, -136, -120, 450, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -88, -120, 437, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -48, -120, 625, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, -120, 750, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -32, -120, 875, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -24, -96, 925, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -16, -88, 925, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -160, 80, 465, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -88, -32, 950, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 88 } }, -96, 0, 950, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 64, 509, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 72, 524, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -96, -120, 974, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, -104, -24, 962, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 128 } }, 104, -40, 1500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, -32, 1750, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 80, -8, 1750, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -160, -120, 950, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 56 } }, -144, -120, 950, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 72 } }, -128, -120, 950, { .fields = { 8, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 64, 88 } }, -104, -120, 950, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 80 } }, -40, -120, 950, { .fields = { 104, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 72 } }, -160, -32, 876, { .fields = { 64, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 80 } }, -144, -24, 876, { .fields = { 96, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 88 } }, -128, -8, 876, { .fields = { 8, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 64, 72 } }, -104, 8, 926, { .fields = { 32, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 8 } }, -8, -48, 925, { .fields = { 64, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 16, 32 } }, -160, -120, 500, { .fields = { 104, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 40, 24 } }, -144, -120, 500, { .fields = { 64, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 40, 16 } }, -104, -120, 500, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 32, 40 } }, -64, -120, 750, { .fields = { 120, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 80 } }, -32, -120, 875, { .fields = { 80, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 32, 104 } }, -160, -8, 463, { .fields = { 96, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 24, 96 } }, -128, -8, 463, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 64 } }, -104, 8, 463, { .fields = { 56, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Mapped view 2's source ranges, after an empty image-strip batch and before the end marker.
static SpriteBatch _gAcropolisPromenadeView2SpriteBatches[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { 3, 2, 0, 0, { 5, 0 } },
    { 5, 25, 0, 0, { 4, 0 } },
    { 30, 16, 0, 0, { 6, 0 } },
    { 46, 3, 0, 0, { 1, 0 } },
    { 49, 4, 0, 0, { 7, 0 } },
    { 53, 3, 0, 0, { 3, 0 } },
    { 56, 9, 0, 0, { 8, 0 } },
    { 65, ARRAY_SIZE(_gAcropolisPromenadeView2SpriteSources) - 65, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 3's 17 sprite rectangles, with pixel positions and unscaled OT depths.
static SpriteSource _gAcropolisPromenadeView3SpriteSources[17] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -72, -48, 1525, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -40, 1568, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 120 } }, -160, -120, 1450, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 104 } }, -160, 0, 1450, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 120 } }, -112, -120, 1450, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -112, 0, 1450, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -72, 0, 1525, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -56, 0, 1558, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, -120, 1083, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 80, -120, 988, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, 40, 930, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, 40, 925, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 96 } }, 80, -96, 968, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, 0, 925, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 24, 2499, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, 24, 2498, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 24, 2449, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
};

/// Mapped view 3's source ranges, after an empty image-strip batch and before the end marker.
static SpriteBatch _gAcropolisPromenadeView3SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 6, 0, 0, { 2, 0 } },
    { 14, ARRAY_SIZE(_gAcropolisPromenadeView3SpriteSources) - 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 4's 56 sprite rectangles, with pixel positions and unscaled OT depths.
static SpriteSource _gAcropolisPromenadeView4SpriteSources[56] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 64, 1067, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -160, -112, 587, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -152, -112, 659, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -144, -112, 731, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -136, -112, 800, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -128, -112, 877, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -120, -112, 954, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -112, -112, 1112, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -56, 1112, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -160, 24, 591, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -152, 24, 664, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -144, 24, 707, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -136, 24, 800, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, 24, 882, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -120, 24, 959, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, 24, 1087, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 8, 1250, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 152 } }, -112, -88, 1314, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -96, -72, 1826, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -88, -40, 2217, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -104, -72, 1465, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -32, 2776, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -32, 2844, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -32, 2932, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -32, 2842, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 16, -56, 1500, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, 24, -72, 1500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 40, 1400, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 128, 32, 720, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 136, 32, 715, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 144, 32, 707, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 136, 104, 609, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 152, -120, 595, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 152, 8, 583, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 112, 24, 741, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 16, 2404, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, -48, 2192, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, -16, 2214, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 32, 1700, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -160, 40, 1600, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -160, 48, 1450, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -160, 56, 1300, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -160, 72, 1075, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -144, 80, 975, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -152, 88, 862, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 96, 787, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 104, 750, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 112, 700, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -160, 64, 1175, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, 16, 1250, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, 32, 1250, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 40, 2200, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -160, 8, 2200, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 16, 2200, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 16, 2200, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -112, 16, 2200, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
};

/// Mapped view 4's source ranges, after an empty image-strip batch and before the end marker.
static SpriteBatch _gAcropolisPromenadeView4SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 3, 0 } },
    { 16, 9, 0, 0, { 4, 0 } },
    { 25, 10, 0, 0, { 1, 0 } },
    { 35, 3, 0, 0, { 5, 0 } },
    { 38, 11, 0, 0, { 0, 0 } },
    { 49, 2, 0, 0, { 6, 0 } },
    { 51, ARRAY_SIZE(_gAcropolisPromenadeView4SpriteSources) - 51, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 5's 83 sprite rectangles, with pixel positions and unscaled OT depths.
static SpriteSource _gAcropolisPromenadeView5SpriteSources[83] = {
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -80, -80, 3667, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -56, -80, 3667, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, -80, 3667, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -152, 80, 625, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -112, 88, 412, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -80, 88, 609, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, -56, 80, 618, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, 72, 637, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 64 } }, 8, 56, 631, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 32, 72, 625, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 56 } }, 48, 64, 625, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 96, 48, 625, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 128, 40, 625, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 56, 936, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 56, 940, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 64, 830, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 64, 854, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 64, 911, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -24, 64, 972, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -16, 64, 882, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 795, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -64, 72, 821, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 72, 924, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -32, 72, 849, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -24, 72, 851, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 72, 865, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 72, 625, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -96, 96, 625, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 112, 625, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -56, 112, 625, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -96, 72, 625, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 80, 685, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, 88, 725, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -56, 88, 739, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -40, 80, 850, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -24, 1500, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 144 } }, 40, -112, 1375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -16, 2200, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -8, 1975, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 0, 1752, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -32, 1975, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -144, -40, 1975, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -96, -48, 1760, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, -40, 3311, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -80, -32, 2853, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -144, 80, 974, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -152, 88, 925, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -152, 96, 900, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -152, 104, 875, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 32, 1272, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 24, 1355, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 16, 1467, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 8, 1622, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, 40, 1170, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 48, 1094, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 56, 1036, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 64, 1006, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -120, 72, 975, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, -24, 2750, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -160, -64, 1100, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -160, 8, 1100, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -152, 40, 1100, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -160, -120, 301, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -72, -64, 3148, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -80, -96, 2375, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -88, -104, 1906, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, -96, -104, 1375, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 192 } }, -104, -104, 1100, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, -144, -104, 855, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, -152, -104, 755, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, -160, -104, 855, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -112, -104, 1031, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -120, -80, 1025, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -128, 0, 811, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -120, -8, 850, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -128, -80, 850, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -136, 8, 750, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -136, -88, 800, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -112, -8, 950, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -152, 48, 1025, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -160, 56, 1000, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -160, 64, 1000, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -160, 88, 875, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
};

/// Mapped view 5's source ranges, after an empty image-strip batch and before the end marker.
static SpriteBatch _gAcropolisPromenadeView5SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 3, 0 } },
    { 13, 22, 0, 0, { 4, 0 } },
    { 35, 2, 0, 0, { 1, 0 } },
    { 37, 22, 0, 0, { 5, 0 } },
    { 59, 3, 0, 0, { 0, 0 } },
    { 62, 17, 0, 0, { 6, 0 } },
    { 79, ARRAY_SIZE(_gAcropolisPromenadeView5SpriteSources) - 79, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 6's 63 sprite rectangles, with pixel positions and unscaled OT depths.
static SpriteSource _gAcropolisPromenadeView6SpriteSources[63] = {
    { 143, 0x3FC0, { .fields = { 24, 176 } }, -160, -120, 1475, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -136, -56, 1500, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 96 } }, -120, -48, 1525, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 88 } }, -96, -40, 1525, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, 8, -64, 1525, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, 40, -72, 1525, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 64, -48, 1525, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 72, -24, 1525, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, 88, -48, 1525, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 96, -24, 1525, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, 112, -48, 1525, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 120, -32, 1525, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 112 } }, -40, -64, 1525, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 88 } }, -72, -8, 750, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 64 } }, -48, 8, 750, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 40 } }, -24, 40, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 80 } }, 24, 8, 750, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 64 } }, 56, -48, 1150, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -24, 1150, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 112, -48, 1150, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, 64, 16, 1125, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 72 } }, -104, -8, 750, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -160, 24, 788, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 32 } }, 80, -120, 461, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 32 } }, 64, -88, 457, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 24 } }, 72, -56, 465, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 32 } }, 88, -32, 472, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 104, 0, 452, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 96 } }, 88, 24, 416, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 24, 825, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -72, 40, 825, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -80, 64, 825, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, 72, 825, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 72, 825, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -32, 80, 825, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 24, 32, 825, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 24, 80, 825, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, 96, 825, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 96, 920, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -88, 40, 800, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -80, 40, 800, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -72, 88, 889, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -48, 96, 857, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, 96, 825, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 24, 40, 798, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 32, 96, 818, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 32, 40, 868, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 112, 800, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -96, 56, 700, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -80, 112, 869, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 24, 56, 775, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 80, 739, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, 112, 450, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 88, 609, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -152, 88, 638, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -144, 88, 639, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 80, 677, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 80, 707, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, 72, 780, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -104, 72, 829, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 8, 96, 500, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 80, 580, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, 80, 671, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
};

/// Mapped view 6's seven source ranges after an empty image-strip batch.
///
/// Ranges cover all 63 source elements once, ending with SPRITE_BATCH_END.
/// Cached packets start visible and included; the last two bytes of each live
/// range retain serialized data whose role is unproven.
static SpriteBatch _gAcropolisPromenadeView6SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 3, 0 } },
    { 13, 9, 0, 0, { 4, 0 } },
    { 22, 7, 0, 0, { 1, 0 } },
    { 29, 9, 0, 0, { 5, 0 } },
    { 38, 9, 0, 0, { 0, 0 } },
    { 47, 4, 0, 0, { 6, 0 } },
    { 51, 12, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture-page encodings, palette rows and colour flags for mapped views 7..9.
///
/// Sources use 8-bit indexed textures and average blending when semitransparent.
enum {
    ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832 = getTPage(1, GPU_BLEND_AVERAGE, 832, 0),
    ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896 = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960 = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255     = getClut(0, 255),
    ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_256     = getClut(0, 256),
    ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB      = 128,
    ACROPOLIS_PROMENADE_SPRITE_OPAQUE           = 0,
};

/// Mapped view 7's 32 texture rectangles and sorting depths.
///
/// Positions/dimensions use pixels, UV origins use texels and depths are unscaled
/// OT values. RGB 128 is neutral modulation; the final five sources blend
/// semitransparently. Batches supply bounds; this array has no terminator and
/// remains borrowed while the promenade overlay is loaded.
static SpriteSource _gAcropolisPromenadeView7SpriteSources[32] = {
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -80, -56, 550, { .fields = { 112, 248 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 56, 32 } }, -40, -120, 550, { .fields = { 72, 176 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, -56, -112, 550, { .fields = { 64, 208 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 88 } }, -72, -96, 550, { .fields = { 104, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 112 } }, -56, -120, 500, { .fields = { 8, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 80, 104 } }, -8, -120, 500, { .fields = { 80, 112 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 88 } }, -72, -80, 500, { .fields = { 120, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -80, -56, 500, { .fields = { 120, 240 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 80 } }, 72, -104, 500, { .fields = { 80, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 40, 48 } }, 24, -72, 875, { .fields = { 104, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 32, 48 } }, 64, -56, 750, { .fields = { 112, 48 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 96, 120 } }, -160, -120, 475, { .fields = { 32, 120 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -64, 40, 875, { .fields = { 104, 88 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 72, 120 } }, -160, 0, 875, { .fields = { 56, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -88, 96, 137, { .fields = { 120, 88 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 72 } }, -88, 8, 750, { .fields = { 56, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 40, 8, 1125, { .fields = { 112, 240 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 48, 8, 1096, { .fields = { 112, 96 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, 56, 8, 1050, { .fields = { 48, 208 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, 64, 16, 1012, { .fields = { 56, 208 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 72, 24, 975, { .fields = { 112, 136 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 80, 32, 950, { .fields = { 8, 96 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 88, 32, 935, { .fields = { 120, 136 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, 96, 32, 925, { .fields = { 32, 208 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 56 } }, 112, 24, 913, { .fields = { 16, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, 128, 24, 925, { .fields = { 16, 208 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 144, 24, 950, { .fields = { 120, 96 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_256, { .fields = { 64, 72 } }, -72, -80, 487, { .fields = { 16, 72 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_256, { .fields = { 80, 64 } }, -8, -80, 487, { .fields = { 0, 144 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_256, { .fields = { 24, 56 } }, 72, -80, 487, { .fields = { 32, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_256, { .fields = { 88, 40 } }, -80, -120, 487, { .fields = { 56, 208 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_256, { .fields = { 80, 40 } }, 8, -120, 487, { .fields = { 80, 216 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Mapped view 7's six source ranges after an empty image-strip batch.
///
/// Ranges cover all 32 sources once; the last range contains the five blended
/// sources. SPRITE_BATCH_END terminates the list. Cached packets start visible
/// and included; the final two bytes of each range have an unproven role.
static SpriteBatch _gAcropolisPromenadeView7SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 3, 0 } },
    { 4, 5, 0, 0, { 0, 0 } },
    { 9, 2, 0, 0, { 5, 0 } },
    { 11, 5, 0, 0, { 1, 0 } },
    { 16, 11, 0, 0, { 4, 0 } },
    { 27, 5, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 8's 46 opaque texture rectangles and sorting depths.
///
/// Positions/dimensions use pixels, UV origins use texels and depths are unscaled
/// OT values. RGB 128 is neutral modulation. Batches supply element bounds;
/// this array has no terminator. The renderer borrows it while this overlay is loaded.
static SpriteSource _gAcropolisPromenadeView8SpriteSources[46] = {
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -80, -48, 1125, { .fields = { 64, 208 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -160, -96, 250, { .fields = { 120, 224 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 224 } }, -152, -104, 625, { .fields = { 120, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 200 } }, -144, -96, 725, { .fields = { 104, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 200 } }, -136, -96, 775, { .fields = { 112, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 184 } }, -128, -88, 850, { .fields = { 88, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 184 } }, -120, -96, 875, { .fields = { 96, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 176 } }, -112, -96, 1057, { .fields = { 80, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 168 } }, -104, -96, 1087, { .fields = { 64, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -88, 40, 1087, { .fields = { 40, 224 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -88, -48, 1125, { .fields = { 8, 72 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, -88, -96, 1125, { .fields = { 64, 168 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, -136, -56, 1600, { .fields = { 48, 224 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 72 } }, -112, -64, 1600, { .fields = { 120, 72 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 112 } }, -96, -64, 1600, { .fields = { 40, 112 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 112 } }, -72, -64, 2000, { .fields = { 56, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 80 } }, -64, -32, 2500, { .fields = { 80, 176 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 64 } }, -56, -24, 3050, { .fields = { 120, 144 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 56 } }, -40, -24, 3075, { .fields = { 8, 200 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 56 } }, -24, -24, 3100, { .fields = { 112, 200 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 88 } }, 152, -120, 700, { .fields = { 72, 168 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 64 } }, 144, -32, 700, { .fields = { 104, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 88 } }, 144, 32, 700, { .fields = { 8, 112 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 72 } }, 120, 32, 750, { .fields = { 120, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 96 } }, 24, -56, 2000, { .fields = { 24, 112 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, 16, 16, 2025, { .fields = { 104, 240 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -160, 40, 1775, { .fields = { 72, 136 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -112, 40, 1775, { .fields = { 72, 120 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -160, 48, 1512, { .fields = { 72, 128 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -112, 48, 1512, { .fields = { 72, 168 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -112, 56, 1275, { .fields = { 72, 160 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -160, 56, 1275, { .fields = { 72, 144 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -112, 64, 1175, { .fields = { 72, 152 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -160, 104, 750, { .fields = { 72, 112 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -160, 112, 700, { .fields = { 72, 80 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -160, 64, 994, { .fields = { 72, 72 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -128, 72, 1075, { .fields = { 80, 64 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -136, 80, 975, { .fields = { 80, 104 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -144, 88, 862, { .fields = { 72, 96 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -152, 96, 787, { .fields = { 72, 88 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 64, 16 } }, -160, 16, 1087, { .fields = { 72, 240 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, -144, 32, 1087, { .fields = { 104, 200 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -120, 32, 1087, { .fields = { 24, 208 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, -136, -56, 1600, { .fields = { 112, 208 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 72 } }, -112, -64, 1750, { .fields = { 88, 184 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 40, 112 } }, -96, -64, 1750, { .fields = { 16, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
};

/// Mapped view 8's seven source ranges after an empty image-strip batch.
///
/// Ranges cover all 46 sources once and end with SPRITE_BATCH_END. Cached
/// packets start visible and included; each range's final two bytes retain
/// serialized data whose role is unproven.
static SpriteBatch _gAcropolisPromenadeView8SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 3, 0 } },
    { 12, 8, 0, 0, { 4, 0 } },
    { 20, 4, 0, 0, { 1, 0 } },
    { 24, 2, 0, 0, { 5, 0 } },
    { 26, 14, 0, 0, { 0, 0 } },
    { 40, 3, 0, 0, { 6, 0 } },
    { 43, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 9's 78 opaque texture rectangles and sorting depths.
///
/// Positions/dimensions use pixels, UV origins use texels and depths are unscaled
/// OT values. RGB 128 is neutral modulation. Batches supply element bounds;
/// this array has no terminator. The renderer borrows it while this overlay is loaded.
static SpriteSource _gAcropolisPromenadeView9SpriteSources[78] = {
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -152, 80, 600, { .fields = { 120, 216 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 56, 32 } }, -112, 88, 600, { .fields = { 32, 168 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 40 } }, -56, 80, 600, { .fields = { 72, 216 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, -8, 72, 600, { .fields = { 80, 56 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 64 } }, 8, 56, 600, { .fields = { 64, 192 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, 32, 72, 600, { .fields = { 72, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 56 } }, 48, 64, 600, { .fields = { 56, 112 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 56, 80 } }, 96, 40, 600, { .fields = { 120, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -64, 56, 936, { .fields = { 32, 240 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -56, 56, 940, { .fields = { 48, 152 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -72, 64, 830, { .fields = { 96, 248 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -64, 64, 854, { .fields = { 32, 248 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -56, 64, 911, { .fields = { 112, 56 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -24, 64, 972, { .fields = { 96, 240 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -16, 64, 882, { .fields = { 96, 232 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -72, 72, 795, { .fields = { 64, 184 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -64, 72, 821, { .fields = { 48, 136 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -56, 72, 924, { .fields = { 112, 64 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -32, 72, 849, { .fields = { 112, 72 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -24, 72, 851, { .fields = { 48, 144 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -16, 72, 865, { .fields = { 104, 112 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, -112, 72, 625, { .fields = { 56, 80 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -88, 80, 612, { .fields = { 56, 200 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -72, 80, 612, { .fields = { 56, 24 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -96, 104, 612, { .fields = { 32, 240 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -56, 104, 612, { .fields = { 24, 96 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -56, 88, 596, { .fields = { 40, 80 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -40, 88, 792, { .fields = { 56, 224 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -40, 80, 849, { .fields = { 40, 160 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -24, 80, 865, { .fields = { 72, 200 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -88, 72, 661, { .fields = { 16, 112 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 88 } }, 40, -112, 1350, { .fields = { 40, 168 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 32, 56 } }, 32, -24, 1375, { .fields = { 88, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -96, -48, 3275, { .fields = { 16, 168 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 80, 8 } }, -144, -40, 3250, { .fields = { 104, 160 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 96, 8 } }, -160, -32, 3075, { .fields = { 88, 216 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 96, 8 } }, -160, -24, 2600, { .fields = { 88, 144 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 96, 8 } }, -160, -16, 2275, { .fields = { 88, 136 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 96, 8 } }, -160, -8, 1975, { .fields = { 88, 224 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_832, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 96, 8 } }, -160, 0, 1800, { .fields = { 88, 40 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -96, 8, 1752, { .fields = { 32, 112 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -96, 16, 1622, { .fields = { 32, 200 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 56, 8 } }, -152, 88, 974, { .fields = { 0, 208 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 56, 8 } }, -152, 96, 925, { .fields = { 0, 120 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 56, 8 } }, -152, 104, 900, { .fields = { 0, 128 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -144, 112, 875, { .fields = { 16, 152 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -104, 24, 1467, { .fields = { 16, 80 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -104, 32, 1355, { .fields = { 8, 72 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -104, 40, 1272, { .fields = { 16, 48 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -112, 48, 1170, { .fields = { 8, 56 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -112, 56, 1094, { .fields = { 8, 64 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -112, 64, 1036, { .fields = { 80, 104 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -120, 72, 1006, { .fields = { 32, 232 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -144, 80, 975, { .fields = { 0, 88 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 56, 64 } }, -160, 8, 1125, { .fields = { 104, 152 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 72 } }, -160, -64, 1125, { .fields = { 112, 80 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, -160, 48, 900, { .fields = { 16, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, -160, 72, 850, { .fields = { 0, 24 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 48, 32 } }, -160, 88, 800, { .fields = { 32, 48 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, -56, -80, 3825, { .fields = { 88, 168 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 56 } }, -64, -80, 3825, { .fields = { 104, 200 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 64 } }, -72, -80, 3800, { .fields = { 88, 192 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 80 } }, -80, -96, 3000, { .fields = { 32, 160 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 80 } }, -96, -96, 2500, { .fields = { 32, 80 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_896, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 56 } }, -112, -96, 2500, { .fields = { 96, 56 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -128, -96, 2500, { .fields = { 112, 224 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -80, -72, 1665, { .fields = { 112, 208 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -80, -24, 1865, { .fields = { 96, 200 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 136 } }, -88, -104, 1750, { .fields = { 48, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 168 } }, -96, -104, 1625, { .fields = { 56, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 192 } }, -104, -104, 1550, { .fields = { 88, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 224 } }, -112, -104, 1475, { .fields = { 120, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 200 } }, -120, -80, 1300, { .fields = { 104, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 200 } }, -128, -80, 1200, { .fields = { 96, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 208 } }, -136, -88, 1100, { .fields = { 112, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 184 } }, -144, -88, 1000, { .fields = { 64, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 192 } }, -152, -88, 900, { .fields = { 80, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
    { ACROPOLIS_PROMENADE_SPRITE_TEXTURE_PAGE_960, ACROPOLIS_PROMENADE_SPRITE_CLUT_ROW_255, { .fields = { 8, 192 } }, -160, -88, 800, { .fields = { 72, 0 } }, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_NEUTRAL_RGB, ACROPOLIS_PROMENADE_SPRITE_OPAQUE },
};

/// Mapped view 9's eight source ranges after an empty image-strip batch.
///
/// Ranges cover all 78 sources once and end with SPRITE_BATCH_END. Cached
/// packets start visible and included; each range's final two bytes retain
/// serialized data whose role is unproven.
static SpriteBatch _gAcropolisPromenadeView9SpriteBatches[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 6, 0 } },
    { 8, 23, 0, 0, { 1, 0 } },
    { 31, 2, 0, 0, { 4, 0 } },
    { 33, 21, 0, 0, { 0, 0 } },
    { 54, 2, 0, 0, { 5, 0 } },
    { 56, 3, 0, 0, { 3, 0 } },
    { 59, 7, 0, 0, { 7, 0 } },
    { 66, 12, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 10's empty sprite list, enabling decoded image strips.
///
/// The zero-count first batch contributes no source elements; the second is
/// SPRITE_BATCH_END. The view retains this list's address in its empty source slot.
static SpriteBatch _gAcropolisPromenadeView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 11's empty sprite list, enabling decoded image strips.
///
/// The zero-count first batch contributes no source elements; the second is
/// SPRITE_BATCH_END. The view retains this list's address in its empty source slot.
static SpriteBatch _gAcropolisPromenadeView11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 12's empty sprite list, enabling decoded image strips.
///
/// The zero-count first batch contributes no source elements; the second is
/// SPRITE_BATCH_END. The view retains this list's address in its empty source slot.
static SpriteBatch _gAcropolisPromenadeView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Retained empty sprite list with no view or other consumer.
///
/// The zero-count first batch is followed by SPRITE_BATCH_END. Views 1 and 13
/// instead share their own list; this separate storage remains in the image.
static SpriteBatch _gAcropolisPromenadeUnusedEmptySpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gAcropolisPromenadeSpriteViews[13] = {
    { { .empty = _gAcropolisPromenadeViews1And13SpriteBatches }, _gAcropolisPromenadeViews1And13SpriteBatches, NULL },
    { { .elements = _gAcropolisPromenadeView2SpriteSources }, _gAcropolisPromenadeView2SpriteBatches, NULL },
    { { .elements = _gAcropolisPromenadeView3SpriteSources }, _gAcropolisPromenadeView3SpriteBatches, NULL },
    { { .elements = _gAcropolisPromenadeView4SpriteSources }, _gAcropolisPromenadeView4SpriteBatches, NULL },
    { { .elements = _gAcropolisPromenadeView5SpriteSources }, _gAcropolisPromenadeView5SpriteBatches, NULL },
    { { .elements = _gAcropolisPromenadeView6SpriteSources }, _gAcropolisPromenadeView6SpriteBatches, NULL },
    { { .elements = _gAcropolisPromenadeView7SpriteSources }, _gAcropolisPromenadeView7SpriteBatches, NULL },
    { { .elements = _gAcropolisPromenadeView8SpriteSources }, _gAcropolisPromenadeView8SpriteBatches, NULL },
    { { .elements = _gAcropolisPromenadeView9SpriteSources }, _gAcropolisPromenadeView9SpriteBatches, NULL },
    { { .empty = _gAcropolisPromenadeView10SpriteBatches }, _gAcropolisPromenadeView10SpriteBatches, NULL },
    { { .empty = _gAcropolisPromenadeView11SpriteBatches }, _gAcropolisPromenadeView11SpriteBatches, NULL },
    { { .empty = _gAcropolisPromenadeView12SpriteBatches }, _gAcropolisPromenadeView12SpriteBatches, NULL },
    { { .empty = _gAcropolisPromenadeViews1And13SpriteBatches }, _gAcropolisPromenadeViews1And13SpriteBatches, NULL },
};

ViewCamera gAcropolisPromenadeViewCameras[13] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 607, 0x7530, -6098 } }, 499 },
    { { { { -3956, 0, -1059 }, { 241, 3987, -903 }, { 1031, -935, -3852 } }, { 372, 488, -0x2DD6 } }, 230 },
    { { { { 3958, 0, -1053 }, { 187, 4030, 703 }, { 1036, -728, 3895 } }, { 1617, 462, 2297 } }, 230 },
    { { { { 3947, 0, -1092 }, { -16, 4095, -57 }, { 1092, 60, 3947 } }, { 1820, 1322, 7620 } }, 230 },
    { { { { 3977, 0, -977 }, { -306, 3889, -1247 }, { 928, 1284, 3776 } }, { 1842, 2326, 0x29C5 } }, 230 },
    { { { { 1010, 0, -3969 }, { 299, 4084, 76 }, { 3957, -309, 1007 } }, { 9221, 696, 3327 } }, 230 },
    { { { { -1131, 0, -3936 }, { -1680, 3703, 483 }, { 3559, 1748, -1023 } }, { 1524, 2795, -0x2739 } }, 230 },
    { { { { 4061, 0, -530 }, { 2, 4095, 21 }, { 530, -21, 4061 } }, { 1461, 1274, 7835 } }, 230 },
    { { { { 3977, 0, -977 }, { -306, 3889, -1247 }, { 928, 1284, 3776 } }, { 1842, 2326, 0x29C5 } }, 230 },
    { { { { -1592, 0, 3773 }, { 1874, 3555, 790 }, { -3275, 2034, -1381 } }, { 112, 2168, -7895 } }, 230 },
    { { { { -1151, 0, 3930 }, { 1811, 3634, 530 }, { -3488, 1887, -1022 } }, { 1036, 1013, 7696 } }, 230 },
    { { { { 4078, 0, -382 }, { -286, 2709, -3058 }, { 252, 3072, 2697 } }, { 1927, 1344, -1461 } }, 230 },
    { { { { 3076, 0, -2704 }, { -116, 4092, -133 }, { 2701, 177, 3073 } }, { 1791, 1265, 5187 } }, 257 },
};

/// Binary-motor pulse sequence for the warp-4 movie; the variable lane stays off.
///
/// The binary lane plays segment 0, waits two script frames, plays segment 1,
/// then loops to command 0 with operand 22 before finally reaching STOP at 5.
/// Reachable command indices are 0..5 and segment indices are 0/1. The variable
/// lane's STOP operand 1 is ignored. The script task borrows both arrays until
/// teardown and is parented to the movie-path task, without owning the arrays.
static const PadScriptCmd _gAcropolisPromenadeArrivalMovieVibrationCommands[6] __attribute__((section(".data"))) = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 2), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 22), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Two- and one-script-frame binary-motor holds used by the arrival movie.
///
/// The binary lane ignores intensity endpoints; the variable lane never plays
/// these records. The final byte is retained as zero with no interpreter read.
/// The vibration task borrows both records through teardown.
static const PadScriptVibrationSegment _gAcropolisPromenadeArrivalMovieVibrationSegments[2] __attribute__((section(".data"))) = {
    { .startIntensity = 0, .endIntensity = 0, .durationFrames = 2, .field_3 = 0 },
    { .startIntensity = 60, .endIntensity = 60, .durationFrames = 1, .field_3 = 0 },
};

/// Class 1's cue-2 bases in the loaded type-1 bank for walking, running and stair climbing.
static WorldCollisionFootstepSounds _gAcropolisPromenadeSurfaceClass1FootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x09,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x0B,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x09,
};

/// Class 2's cue-2 bases in the loaded type-1 bank for walking, running and stair climbing.
static WorldCollisionFootstepSounds _gAcropolisPromenadeSurfaceClass2FootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x31,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x33,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x31,
};

/// Class 3's cue-2 bases in the loaded type-1 bank for walking, running and stair climbing.
///
/// These entries have no scripts in the promenade's sound bank.
static WorldCollisionFootstepSounds _gAcropolisPromenadeSurfaceClass3FootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x25,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x27,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x25,
};

/// Class 4's cue-2 bases in the loaded type-1 bank for walking, running and stair climbing.
static WorldCollisionFootstepSounds _gAcropolisPromenadeSurfaceClass4FootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
};

/// Class 7's cue-2 bases, matching class 3's cues while retaining separate storage.
///
/// These entries have no scripts in the promenade's sound bank.
static WorldCollisionFootstepSounds _gAcropolisPromenadeSurfaceClass7FootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x25,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x27,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x25,
};

/// Blocking and pushback-enabled properties without footstep cues for classes 0, 5 and 6.
static WorldCollisionSurfaceProperties _gAcropolisPromenadeDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1's blocking, impact-enabled and pushback-enabled properties with footstep cues.
static WorldCollisionSurfaceProperties _gAcropolisPromenadeSurfaceClass1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisPromenadeSurfaceClass1FootstepSounds,
};

/// Class 2's blocking, impact-enabled and pushback-enabled properties with footstep cues.
static WorldCollisionSurfaceProperties _gAcropolisPromenadeSurfaceClass2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisPromenadeSurfaceClass2FootstepSounds,
};

/// Class 3's blocking, impact-enabled and pushback-enabled properties with footstep cues.
static WorldCollisionSurfaceProperties _gAcropolisPromenadeSurfaceClass3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisPromenadeSurfaceClass3FootstepSounds,
};

/// Class 4's blocking, impact-enabled and pushback-enabled properties with footstep cues.
static WorldCollisionSurfaceProperties _gAcropolisPromenadeSurfaceClass4Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisPromenadeSurfaceClass4FootstepSounds,
};

/// Class 7's probe-passable, impact-enabled properties with suppressed pushback and footstep cues.
///
/// The first byte is retained as 1; its role remains unproven.
static WorldCollisionSurfaceProperties _gAcropolisPromenadeSurfaceClass7Properties = {
    .field_0             = 1,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_SUPPRESS_PUSHBACK,
    .footstepSounds      = &_gAcropolisPromenadeSurfaceClass7FootstepSounds,
};

WorldCollisionSurfaceProperties* gAcropolisPromenadeSurfacePropertiesByClass[8] = {
    &_gAcropolisPromenadeDefaultSurfaceProperties,
    &_gAcropolisPromenadeSurfaceClass1Properties,
    &_gAcropolisPromenadeSurfaceClass2Properties,
    &_gAcropolisPromenadeSurfaceClass3Properties,
    &_gAcropolisPromenadeSurfaceClass4Properties,
    &_gAcropolisPromenadeDefaultSurfaceProperties,
    &_gAcropolisPromenadeDefaultSurfaceProperties,
    &_gAcropolisPromenadeSurfaceClass7Properties,
};

/// Last executed sanctuary-exit reply before bridge progress, with warp 3.
///
/// Stores the complete eight-byte resolved message. No reader is established;
/// this storage does not own or defer a transition. Reloading clears it.
static RoomEventMsg _gAcropolisPromenadeSanctuaryExitReply;

/// Saved result of the room initializer's translucent-plane spawn, or NULL.
///
/// Borrowed task pointer, written once during setup and never read here.
/// It neither owns teardown nor tracks whether that task is still live.
static Task* _gAcropolisPromenadeTranslucentPlaneTask;

/// Starts the warp-4 movie once and schedules the variant-1 battle follow-up.
///
/// Runs in the room task's active state without advancing it. Normalizes saved
/// scene event 6 to 5, holds area-music loading during variant 1, and fades MIDI
/// over 60 audio updates when an observed engaged battle ends. Starts the skippable
/// follow-up only after battle reset is pending. The receiver is unused.
static void _acropolisPromenadeUpdateRoomProgress(Task* unusedTask)
{
    enum {
        ACROPOLIS_PROMENADE_MOVIE_ARRIVAL_WARP     = 4,
        ACROPOLIS_PROMENADE_SCENE_EVENT_SANCTUARY  = 6,
        ACROPOLIS_PROMENADE_SCENE_EVENT_PROMENADE  = 5,
        ACROPOLIS_PROMENADE_BATTLE_VARIANT         = 1,
        ACROPOLIS_PROMENADE_ALL_MIDI_SEQUENCES     = 0,
        ACROPOLIS_PROMENADE_BATTLE_MIDI_FADE_TICKS = 60,
    };
    u8 placementVariant;
    u8 battlePhase;

    if (_gAcropolisPromenadeArrivalMovieStarted == ACROPOLIS_PROMENADE_ARRIVAL_MOVIE_NOT_STARTED) {
        if (gGameSession->location.warp == ACROPOLIS_PROMENADE_MOVIE_ARRIVAL_WARP) {
            _gAcropolisPromenadeArrivalMovieStarted = ACROPOLIS_PROMENADE_ARRIVAL_MOVIE_STARTED;
            taskSpawnFromTable(_gAcropolisPromenadeMovieTaskDescriptors, ACROPOLIS_PROMENADE_MOVIE_PATH_TASK, 0, 0);
        }
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == ACROPOLIS_PROMENADE_SCENE_EVENT_SANCTUARY) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACROPOLIS_PROMENADE_SCENE_EVENT_PROMENADE;
    }
    placementVariant = gGameSession->location.variant;
    if (placementVariant == ACROPOLIS_PROMENADE_BATTLE_VARIANT) {
        gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_AREA_MUSIC | GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY);
        battlePhase             = gSceneCombatState.signals.bytes.battlePhase;
        if (battlePhase == placementVariant) {
            _gAcropolisPromenadeBattleFollowupState = battlePhase;
        }
        if ((_gAcropolisPromenadeBattleFollowupState == placementVariant) && (battlePhase != _gAcropolisPromenadeBattleFollowupState)) {
            _gAcropolisPromenadeBattleFollowupState = ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_PENDING;
            sndEvtRequestMidiStop(ACROPOLIS_PROMENADE_ALL_MIDI_SEQUENCES, ACROPOLIS_PROMENADE_BATTLE_MIDI_FADE_TICKS);
        }
        if ((_gAcropolisPromenadeBattleFollowupState == ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_PENDING) && (gGameSession->battleResetPending != 0)) {
            _gAcropolisPromenadeBattleFollowupState = ACROPOLIS_PROMENADE_BATTLE_FOLLOWUP_INACTIVE;
            evsStartScriptWithSkip(_gAcropolisPromenadeBattleFollowupScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisPromenadeBattleFollowupSkipScript);
        }
    }
}

/// Resolves promenade exits from movie availability and route progress.
///
/// Receives `ROOM_EVENT_MESSAGE_RESOLVE` with an eight-byte borrowed request and
/// writable reply, which may alias. Copies the complete record. Execution
/// selects observatory arrival 1 when the movie or route is unavailable; it
/// stores sanctuary arrival 3 before bridge progress and selects scene event 4.
/// The bridge exit returns 0 and starts CAP command 2 before progress, then
/// returns 1 and selects room 1 or 2. Queries suppress these side effects.
static s32 _acropolisPromenadeResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_PROMENADE_ROUTE_FROM_OBSERVATORY         = 4,
        ACROPOLIS_PROMENADE_ROUTE_AT_SANCTUARY             = 5,
        ACROPOLIS_PROMENADE_OBSERVATORY_BLOCKED_ARRIVAL    = 1,
        ACROPOLIS_PROMENADE_SANCTUARY_SCENE_ARRIVAL        = 3,
        ACROPOLIS_PROMENADE_SCENE_EVENT_SANCTUARY_APPROACH = 4,
        ACROPOLIS_PROMENADE_BRIDGE_LOCKED_CAP_COMMAND      = 2,
        ACROPOLIS_PROMENADE_EXIT_MAP_MARKED                = 2,
        ACROPOLIS_PROMENADE_BRIDGE_SCENE_COMPLETE          = 3,
        ACROPOLIS_PROMENADE_BRIDGE_ROOM_AFTER_SCENE        = 2,
        ACROPOLIS_PROMENADE_BRIDGE_ROOM_BEFORE_SCENE       = 1,
    };
    // The unused record retains the original stack reservation.
    RoomEventMsg retainedEventSpace;
    u16          destinationArea;

    *reply = *request;
    if (request->areaId == GAME_AREA_ACROPOLIS_OBSERVATORY && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gDisplayState.debugMode < 0 || gStreamInterFile.startSector == 0) {
            reply->warp = ACROPOLIS_PROMENADE_OBSERVATORY_BLOCKED_ARRIVAL;
        }
        if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) == ACROPOLIS_PROMENADE_ROUTE_FROM_OBSERVATORY) {
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS, ACROPOLIS_PROMENADE_ROUTE_AT_SANCTUARY);
        } else {
            reply->warp = ACROPOLIS_PROMENADE_OBSERVATORY_BLOCKED_ARRIVAL;
        }
    }
    if (request->areaId == GAME_AREA_ACROPOLIS_SANCTUARY && gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS) == 0) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            reply->warp                                         = ACROPOLIS_PROMENADE_SANCTUARY_SCENE_ARRIVAL;
            _gAcropolisPromenadeSanctuaryExitReply              = *reply;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACROPOLIS_PROMENADE_SCENE_EVENT_SANCTUARY_APPROACH;
        }
        return 1;
    }
    destinationArea = request->areaId;
    if (destinationArea == GAME_AREA_ACROPOLIS_BRIDGE) {
        if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS) == 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capSpawnEventIfIdle(ACROPOLIS_PROMENADE_BRIDGE_LOCKED_CAP_COMMAND, CAP_EVENT_PAUSE_ACTORS);
                gameFlagSetNibbleIfPresent(request->flagId, ACROPOLIS_PROMENADE_EXIT_MAP_MARKED);
            }
            return 0;
        }
        if (request->areaId == destinationArea) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS) == ACROPOLIS_PROMENADE_BRIDGE_SCENE_COMPLETE) {
                    reply->room = ACROPOLIS_PROMENADE_BRIDGE_ROOM_AFTER_SCENE;
                } else {
                    reply->room = ACROPOLIS_PROMENADE_BRIDGE_ROOM_BEFORE_SCENE;
                }
            }
        }
    }
    return 1;
}

/// Refuses every key-item-use request in the promenade.
///
/// The first payload is the selected inventory item ID; the second is unused.
/// Ignores all arguments and returns zero, selecting the item's refusal text.
static s32 _acropolisPromenadeRefuseKeyItem(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    enum { ACROPOLIS_PROMENADE_KEY_ITEM_REFUSED = 0 };
    return ACROPOLIS_PROMENADE_KEY_ITEM_REFUSED;
}

/// Selects CAP playback for promenade room command 5.
///
/// The first payload is the integer command; other arguments are unused.
/// Object state 2 for index 21 selects CAP command 9 with a display transition;
/// otherwise starts CAP slot 5 in transition mode. Always returns 0.
static s32 _acropolisPromenadeHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum {
        ACROPOLIS_PROMENADE_ROOM_COMMAND_CAP_5    = 5,
        ACROPOLIS_PROMENADE_CAP_OBJECT_INDEX      = 21,
        ACROPOLIS_PROMENADE_CAP_OBJECT_COMPLETE   = 2,
        ACROPOLIS_PROMENADE_CAP_SLOT              = 5,
        ACROPOLIS_PROMENADE_CAP_COMPLETED_COMMAND = 9,
    };
    if (command == ACROPOLIS_PROMENADE_ROOM_COMMAND_CAP_5) {
        if (areaGetCurrentObjectState(ACROPOLIS_PROMENADE_CAP_OBJECT_INDEX) != ACROPOLIS_PROMENADE_CAP_OBJECT_COMPLETE) {
            capStartSequenceSlot(ACROPOLIS_PROMENADE_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
        } else {
            capRunCommandWithTransition(ACROPOLIS_PROMENADE_CAP_COMPLETED_COMMAND);
        }
    }
    return 0;
}

/// Ignores the promenade's room-action request.
///
/// The first payload transports a borrowed `DirectionActionRequest` address;
/// the second is zero. Neither is read, so the unused payloads remain words.
/// Leaves the result unset; callers must discard the dispatch result.
static s32 _acropolisPromenadeIgnoreRoomAction(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
}

/// Queues promenade sound-bank entries for direct and completed CAP sound cues.
///
/// Cue 10 starts entry 9; cue 103 (CAP command 3 completed) starts entry 10.
/// Uses each script's base mix and ignores other cues and queue failures.
/// Receiver, ID and second payload are unused; always returns zero.
static s32 _acropolisPromenadeHandleSoundCue(Task* unusedTask, s32 messageId, s32 soundCue, s32 unusedArg)
{
    enum {
        ACROPOLIS_PROMENADE_SOUND_CUE_ENTRY_9            = 10,
        ACROPOLIS_PROMENADE_SOUND_CUE_CAP_COMMAND_3_DONE = 103
    };

    switch (soundCue) {
        case ACROPOLIS_PROMENADE_SOUND_CUE_ENTRY_9:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 9), 0, 0);
            break;
        case ACROPOLIS_PROMENADE_SOUND_CUE_CAP_COMMAND_3_DONE:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 0x0A), 0, 0);
            break;
    }
    return 0;
}

/// Room-task handlers indexed by state: initialize, update progress, release.
///
/// States must stay in 0..2. Dispatch copies all three callbacks to the stack,
/// then indexes without bounds checking or a sentinel. Keep this overlay
/// loaded through the selected callback, which may release the task.
static const TaskFuncTable3 _gAcropolisPromenadeRoomStates = {
    { _acropolisPromenadeInitializeRoomTask, _acropolisPromenadeUpdateRoomProgress, taskKill },
};

/// Translucent-plane handlers indexed by task state: setup, visibility, teardown.
///
/// States 0..2 respectively allocate/place the TMD model, update its visibility
/// and release its task/work. Dispatch copies the three handlers to the stack
/// before indexing; there is no sentinel or bounds check. Keep the overlay
/// loaded through the selected handler.
static const TaskFuncTable3 _gAcropolisPromenadeTranslucentPlaneStates = {
    { _bridgeModelSetup, _acropolisPromenadeUpdateBridgeVisibility, taskKill },
};

/// Runs the promenade's translucent plane model task.
///
/// Requires a live TMD task with state 0 (allocate work and place the model),
/// 1 (hide in mapped view 5, show in other views), or 2 (release the task and its work).
/// The state is an unchecked table index. Keep the room and model resources
/// loaded until teardown; allocation failure also tears down the task.
/// The plane's visual purpose is unproven.
static void _acropolisPromenadeTranslucentPlaneTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAcropolisPromenadeTranslucentPlaneStates;
    stateHandlers.funcs[task->state](task);
}

/// Registers the promenade room receiver and spawns its translucent plane.
///
/// Called in state 0 with this overlay's resources loaded. Publishes the room
/// task, saves the spawned plane task, advances to the active state and enables
/// CAP-completion sound forwarding. Spawn failure leaves the saved pointer NULL.
static void _acropolisPromenadeInitializeRoomTask(Task* task)
{
    enum { ACROPOLIS_PROMENADE_CAP_COMPLETION_SOUNDS_ENABLED = 1 };
    task->msgTable = _gAcropolisPromenadeRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _gAcropolisPromenadeTranslucentPlaneTask = taskSpawnFromTable(_gAcropolisPromenadeTranslucentPlaneTaskDescriptors, 0, 0, 0);
    task->state                              = task->state + 1;
    gCapRoomCompletionSoundsEnabled          = ACROPOLIS_PROMENADE_CAP_COMPLETION_SOUNDS_ENABLED;
}

void acropolisPromenadeRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAcropolisPromenadeRoomStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/room/bridge_model_setup.inc.c"

/// Hides the bridge model in promenade view 5 and enables it in the other views.
///
/// Requires the model task's initialized TMD body. Replaces its draw flags and
/// marks the first coordinate dirty so the next draw recomposes its transform.
static void _acropolisPromenadeUpdateBridgeVisibility(Task* task)
{
    enum { ACROPOLIS_PROMENADE_BRIDGE_HIDDEN_VIEW = 5 };
    TmdObject* bridge;
    GfxCoord*  coord;

    bridge = task->extra.tmd;
    coord  = bridge->coords;
    if (viewGetMappedIndex() == ACROPOLIS_PROMENADE_BRIDGE_HIDDEN_VIEW) {
        bridge->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        bridge->flags = 0;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Copies a movie-path sample into the player root, with the promenade Z offset.
///
/// Requires live borrowed work/root and path index 0..299. Positions use whole
/// room units; rotation and coordinate cache stamps are unchanged.
/// Work and index are evaluated three times, once per axis, and must be
/// stable and side-effect-free; zOffset is evaluated once. Captures the room's
/// path array and expands to a braced statement block inside a braced scope.
#define ACROPOLIS_PROMENADE_APPLY_MOVIE_PATH_POSITION(movieWork, pathIndex, zOffset)                                \
    {                                                                                                               \
        (movieWork)->playerMtx->t[0] = _gAcropolisPromenadeArrivalMoviePlayerPositions[(pathIndex)].vx;             \
        (movieWork)->playerMtx->t[1] = _gAcropolisPromenadeArrivalMoviePlayerPositions[(pathIndex)].vy;             \
        (movieWork)->playerMtx->t[2] = _gAcropolisPromenadeArrivalMoviePlayerPositions[(pathIndex)].vz - (zOffset); \
    }

/// Moves the player along the promenade movie path and restores player control.
///
/// Starts bodyless in state 0 with a live player and loaded movie resources.
/// Owns `RoomMoviePathWork`; allocation failure kills the task. Waits for movie
/// readiness, adopts its vibration task, then copies the player root position
/// from path index 69 - movieFrame, subtracting 200 whole room units from Z.
/// Every access requires movieFrame <= 69. Start initiates a polled skip fade;
/// skip places the player with yaw 3072, while the final six samples start a
/// scripted walk using position only. Waits for that motion before restoring
/// HUD/input/actors and saved view 2. Resources must remain live through exit.
static void _acropolisPromenadeMoviePathTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_MOVIE_INITIAL              = 0,
        ACROPOLIS_PROMENADE_MOVIE_WAIT_READY           = 1,
        ACROPOLIS_PROMENADE_MOVIE_FOLLOW_PATH          = 2,
        ACROPOLIS_PROMENADE_MOVIE_WAIT_PLAYER          = 3,
        ACROPOLIS_PROMENADE_MOVIE_FRAME_ORIGIN         = 69,
        ACROPOLIS_PROMENADE_MOVIE_Z_OFFSET             = 200,
        ACROPOLIS_PROMENADE_MOVIE_EXIT_X               = 642,
        ACROPOLIS_PROMENADE_MOVIE_EXIT_Y               = 41,
        ACROPOLIS_PROMENADE_MOVIE_EXIT_YAW             = 3072,
        ACROPOLIS_PROMENADE_MOVIE_EXIT_VIEW            = 2,
        ACROPOLIS_PROMENADE_MOVIE_WALK_HANDOFF_SAMPLES = 6,
        ACROPOLIS_PROMENADE_PRIMARY_WEAPON_BANK_BASE   = 1,
        ACROPOLIS_PROMENADE_ALTERNATE_WEAPON_BANK_BASE = 34,
    };
    AnimationPlayRequest animationRequest;
    ActorTransform       playerPlacement;
    s32                  skipFadeResult;
    RoomMoviePathWork*   work;
    CdCmdQueue*          cdQueue;
    s32                  equippedWeapon;

    cdQueue = &gCdCmdQueue;
    work    = task->work;
    switch (task->state) {
        case ACROPOLIS_PROMENADE_MOVIE_INITIAL:
            // Capture the new work through task->work; the early local predates allocation.
            task->work = memCalloc(sizeof(RoomMoviePathWork), 0);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            equippedWeapon                               = gPlayerStatus.weapon;
            animationRequest.source.index                = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? equippedWeapon + ACROPOLIS_PROMENADE_PRIMARY_WEAPON_BANK_BASE : equippedWeapon + ACROPOLIS_PROMENADE_ALTERNATE_WEAPON_BANK_BASE;
            animationRequest.animationId                 = 1;
            animationRequest.blend                       = ANIMATION_BLEND_RESET;
            animationRequest.blendFrames                 = 0;
            animationRequest.enableWorldCollision        = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, ANIMATION_MESSAGE_PLAY, &animationRequest, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state                    = task->state + 1;
            break;

        case ACROPOLIS_PROMENADE_MOVIE_WAIT_READY:
            if (cdQueue->movieReady != 0) {
                work->padScriptTask           = padScriptSpawn(_gAcropolisPromenadeArrivalMovieVibrationCommands,
                                                               _gAcropolisPromenadeArrivalMovieVibrationSegments);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, work->padScriptTask);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_PROMENADE_MOVIE_FOLLOW_PATH:
            // Follow decoded movie frames, giving a completed skip fade priority over walking.
            ACROPOLIS_PROMENADE_APPLY_MOVIE_PATH_POSITION(work, ACROPOLIS_PROMENADE_MOVIE_FRAME_ORIGIN - cdQueue->movieFrame, ACROPOLIS_PROMENADE_MOVIE_Z_OFFSET);
            if (work->skipFadeStarted != 0) {
                if (taskPollKill(work->skipFadeTask, &skipFadeResult) != 0) {
                    playerPlacement.pos.vx = ACROPOLIS_PROMENADE_MOVIE_EXIT_X;
                    playerPlacement.pos.vy = ACROPOLIS_PROMENADE_MOVIE_EXIT_Y;
                    playerPlacement.pos.vz = _gAcropolisPromenadeArrivalMoviePlayerPositions[ACROPOLIS_PROMENADE_MOVIE_FRAME_ORIGIN - cdQueue->movieFrame].vz - ACROPOLIS_PROMENADE_MOVIE_Z_OFFSET;
                    playerPlacement.rot.vz = 0;
                    playerPlacement.rot.vx = 0;
                    playerPlacement.rot.vy = ACROPOLIS_PROMENADE_MOVIE_EXIT_YAW;
                    TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &playerPlacement, 0);
                    taskSpawnFromTable(_gAcropolisPromenadeMovieTaskDescriptors, ACROPOLIS_PROMENADE_MOVIE_SCENE_FADE_TASK, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (padIsStartPressed() != 0) {
                work->skipFadeTask    = taskSpawnFromTable(_gAcropolisPromenadeMovieTaskDescriptors, ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_TASK, 0, 0);
                work->skipFadeStarted = 1;
            }
            // The walk handler reads XYZ only; rotation need not be initialized here.
            if ((ACROPOLIS_PROMENADE_MOVIE_FRAME_ORIGIN - cdQueue->movieFrame) < ACROPOLIS_PROMENADE_MOVIE_WALK_HANDOFF_SAMPLES) {
                playerPlacement.pos.vx = ACROPOLIS_PROMENADE_MOVIE_EXIT_X;
                playerPlacement.pos.vy = ACROPOLIS_PROMENADE_MOVIE_EXIT_Y;
                playerPlacement.pos.vz = _gAcropolisPromenadeArrivalMoviePlayerPositions[ACROPOLIS_PROMENADE_MOVIE_FRAME_ORIGIN - cdQueue->movieFrame].vz - ACROPOLIS_PROMENADE_MOVIE_Z_OFFSET;
                TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &playerPlacement, 0);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_PROMENADE_MOVIE_WAIT_PLAYER:
            // Release scene holds only after the scripted player motion finishes.
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = ACROPOLIS_PROMENADE_MOVIE_EXIT_VIEW;
                padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR_ALIAS, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
                taskKill(task);
            }
            break;
    }
}

#undef ACROPOLIS_PROMENADE_APPLY_MOVIE_PATH_POSITION

/// Darkens the promenade movie before its skip transition.
///
/// Start this bodyless task with `killCountdown` zero. Draws subtractive grey
/// levels 0, 32, ... 224, advancing the signed 16-bit counter after each draw.
/// At 256 requests deferred teardown with result zero; the spawning movie
/// task polls that completion before placing the player and starting fade-in.
/// Requires a live overlay and the current frame's primitive arena.
static void _acropolisPromenadeMovieSkipFadeOutTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_STEP = 32,
        ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_END  = 256
    };
    u8  darkness;
    s16 nextFadeCounter;

    darkness = (u8)task->killCountdown;
    fadeDrawOverlay(darkness, darkness, darkness, GPU_BLEND_SUBTRACT);
    nextFadeCounter     = (u16)task->killCountdown + ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_STEP;
    task->killCountdown = nextFadeCounter;
    if (nextFadeCounter >= ACROPOLIS_PROMENADE_MOVIE_SKIP_FADE_END) {
        taskRequestKill(task, 0);
    }
}

/// Reveals the scene after a movie skip by reducing a subtractive black overlay.
///
/// Requires a bodyless task with `killCountdown` initially zero. Draws grey
/// levels 255, 223, ... 31 on successive frames, advancing the signed counter
/// by 32 after each draw and releasing the task when it reaches 256.
static void _acropolisPromenadeSceneFadeInTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_FADE_STEP = 32,
        ACROPOLIS_PROMENADE_FADE_END  = 256
    };
    u8  darkness;
    s16 nextFadeCounter;

    darkness = ~(u8)task->killCountdown;
    fadeDrawOverlay(darkness, darkness, darkness, GPU_BLEND_SUBTRACT);
    nextFadeCounter     = (u16)task->killCountdown + ACROPOLIS_PROMENADE_FADE_STEP;
    task->killCountdown = nextFadeCounter;
    if (nextFadeCounter >= ACROPOLIS_PROMENADE_FADE_END) {
        taskKill(task);
    }
}

void acropolisPromenadeRoomEffectTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_EFFECTS_SUPPRESSED_VIEW = 7,
        ACROPOLIS_PROMENADE_LAMP_SINGLE_END         = 3,
        ACROPOLIS_PROMENADE_LAMP_GROUP_FIRST        = 3,
        ACROPOLIS_PROMENADE_LAMP_GROUP_END          = 5,
        ACROPOLIS_PROMENADE_DRIP_VIEW_ENTRY_COUNT   = 40,
        ACROPOLIS_PROMENADE_DISC_RADIUS_SCALE       = 256,
        ACROPOLIS_PROMENADE_DISC_PACKED_COLOR       = 0x5C40,
    };
    GfxCoord*   roomCoord;
    EffectWork* work;
    u8          mappedView;
    s32         placementIndex;
    s32         viewMask;
    s16         previousView;

    roomCoord  = task->extra.coordBody->coord;
    work       = task->spawnArg2.pointer;
    mappedView = viewGetMappedIndex();
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    work->age++;
    if (mappedView == ACROPOLIS_PROMENADE_EFFECTS_SUPPRESSED_VIEW) {
        return;
    }
    viewMask = 1 << (mappedView - 1);
    if (_gAcropolisPromenadeGlowViewMask & viewMask) {
        effectSpawn((EFFECT_ACROPOLIS_PROMENADE_GLOW_STAR | EFFECT_SPAWN_UNLIMITED), roomCoord, (s32)(work->age), &_gAcropolisPromenadeGlowPositions[ACROPOLIS_PROMENADE_GLOW_FIRST_STAR]);
        effectSpawn((EFFECT_ACROPOLIS_PROMENADE_GLOW_STAR | EFFECT_SPAWN_UNLIMITED), roomCoord, (s32)(work->age), &_gAcropolisPromenadeGlowPositions[ACROPOLIS_PROMENADE_GLOW_SECOND_STAR]);
        effectSpawn(EFFECT_ACROPOLIS_PROMENADE_GROUND_GLOW, roomCoord, (s32)(work->age), &_gAcropolisPromenadeGroundGlowPosition);
        glowDrawTintedDiscNoBias(&_gAcropolisPromenadeGlowPositions[ACROPOLIS_PROMENADE_GLOW_DISC_POSITION], ACROPOLIS_PROMENADE_DISC_RADIUS_SCALE, ACROPOLIS_PROMENADE_DISC_PACKED_COLOR);
    }
    // The grouped pairs cover lamp placements 3..10; placement 11 follows separately.
    for (placementIndex = 0; placementIndex < ACROPOLIS_PROMENADE_LAMP_SINGLE_END; placementIndex++) {
        if (_gAcropolisPromenadeLampGlowViewMasks[placementIndex] & viewMask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 0, &_gAcropolisPromenadeLampGlowPositions[placementIndex]);
        }
    }
    for (placementIndex = ACROPOLIS_PROMENADE_LAMP_GROUP_FIRST; placementIndex < ACROPOLIS_PROMENADE_LAMP_GROUP_END; placementIndex++) {
        if (_gAcropolisPromenadeLampGlowViewMasks[placementIndex] & viewMask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 1, &_gAcropolisPromenadeLampGlowPositions[placementIndex]);
        }
        if (_gAcropolisPromenadeLampGlowViewMasks[placementIndex + 2] & viewMask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 2, &_gAcropolisPromenadeLampGlowPositions[placementIndex + 2]);
        }
        if (_gAcropolisPromenadeLampGlowViewMasks[placementIndex + 4] & viewMask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 1, &_gAcropolisPromenadeLampGlowPositions[placementIndex + 4]);
        }
        if (_gAcropolisPromenadeLampGlowViewMasks[placementIndex + 6] & viewMask) {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 2, &_gAcropolisPromenadeLampGlowPositions[placementIndex + 6]);
        }
    }
    if (_gAcropolisPromenadeLampGlowViewMasks[11] & viewMask) {
        effectSpawn(EFFECT_ACROPOLIS_PROMENADE_LAMP_GLOW, roomCoord, 1, &_gAcropolisPromenadeLampGlowPositions[11]);
    }
    // Seed a drip field on entry; subsequent ticks add two replacements.
    if (_gAcropolisPromenadeDripViewMask & viewMask) {
        previousView = work->scale;
        if (previousView != mappedView) {
            for (placementIndex = 0; placementIndex < ACROPOLIS_PROMENADE_DRIP_VIEW_ENTRY_COUNT; placementIndex++) {
                effectSpawn(EFFECT_ACROPOLIS_PROMENADE_SCREEN_DRIP, roomCoord, (s32)(mappedView), NULL);
            }
        } else {
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_SCREEN_DRIP, roomCoord, (s32)(previousView), NULL);
            effectSpawn(EFFECT_ACROPOLIS_PROMENADE_SCREEN_DRIP, roomCoord, (s32)(previousView), NULL);
        }
    }
    work->scale = mappedView;
}

/// Seeds one screen drip's pixel position, lifetime, width and descent period.
///
/// Requires a live writable task and its zero-age effect work. Sets `move.vx`
/// to X in 0..239 pixels, `move.vy` to Y in 176..239 pixels, `scale` to the
/// lifetime cutoff in 30..119 frames, `angle` to width in 16..79 pixels and
/// `period` to 1..4 frames per row. Advances task state without changing age.
/// Consumes five shared LCG draws in X/Y/lifetime/width/period order, using
/// unsigned lifetime reduction. Borrows both objects and retains no pointer.
static __inline__ void _acropolisPromenadeInitializeScreenDrip(Task* task, EffectWork* work)
{
    enum {
        ACROPOLIS_PROMENADE_DRIP_COLUMN_COUNT          = 240,
        ACROPOLIS_PROMENADE_DRIP_FIRST_ROW             = 176,
        ACROPOLIS_PROMENADE_DRIP_ROW_RANGE             = 64,
        ACROPOLIS_PROMENADE_DRIP_MIN_LIFETIME_FRAMES   = 30,
        ACROPOLIS_PROMENADE_DRIP_LIFETIME_RANGE_FRAMES = 90,
        ACROPOLIS_PROMENADE_DRIP_MIN_WIDTH_PIXELS      = 16,
        ACROPOLIS_PROMENADE_DRIP_WIDTH_RANGE_PIXELS    = 64,
        ACROPOLIS_PROMENADE_DRIP_MAX_FRAMES_PER_ROW    = 4
    };
    u16 randomBits;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vx   = (gRandomLcgState >> 16) % ACROPOLIS_PROMENADE_DRIP_COLUMN_COUNT;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vy   = ((gRandomLcgState >> 16) & (ACROPOLIS_PROMENADE_DRIP_ROW_RANGE - 1)) + ACROPOLIS_PROMENADE_DRIP_FIRST_ROW;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    randomBits      = gRandomLcgState >> 16;
    work->scale     = (u32)randomBits % ACROPOLIS_PROMENADE_DRIP_LIFETIME_RANGE_FRAMES + ACROPOLIS_PROMENADE_DRIP_MIN_LIFETIME_FRAMES;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->angle     = ((gRandomLcgState >> 16) & (ACROPOLIS_PROMENADE_DRIP_WIDTH_RANGE_PIXELS - 1)) + ACROPOLIS_PROMENADE_DRIP_MIN_WIDTH_PIXELS;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->period    = ((gRandomLcgState >> 16) & (ACROPOLIS_PROMENADE_DRIP_MAX_FRAMES_PER_ROW - 1)) + 1;
    task->state++;
}

void acropolisPromenadeScreenDripTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_DRAW_BUFFER_STRIDE_PIXELS = 272,
        ACROPOLIS_PROMENADE_DRIP_DEPTH_FIRST_ROW      = 176,
        ACROPOLIS_PROMENADE_DRIP_LAST_SOURCE_ROW      = 239,
        ACROPOLIS_PROMENADE_DRIP_START_DEPTH          = 0x500,
        ACROPOLIS_PROMENADE_DRIP_DEPTH_PER_ROW        = 10,
        ACROPOLIS_PROMENADE_DRIP_OT_DEPTH_SHIFT       = 4
    };
    EffectWork* work;
    RECT        sourceStrip;
    DR_MOVE*    movePacket;
    s32         bufferY;
    s32         x;
    s32         y;
    s32         onScreen;
    s32         depth;

    work    = task->spawnArg2.pointer;
    bufferY = gDisplayState.drawBuffer * ACROPOLIS_PROMENADE_DRAW_BUFFER_STRIDE_PIXELS;
    if ((u8)viewGetMappedIndex() == task->spawnArg1.value) {
        // The work fields hold pixels and frame counts for this screen-space effect.
        if (work->age == 0) {
            _acropolisPromenadeInitializeScreenDrip(task, work);
        }
        y        = work->move.vy + work->age / work->period;
        x        = work->move.vx;
        depth    = ACROPOLIS_PROMENADE_DRIP_START_DEPTH - (y - ACROPOLIS_PROMENADE_DRIP_DEPTH_FIRST_ROW) * ACROPOLIS_PROMENADE_DRIP_DEPTH_PER_ROW;
        onScreen = y < ACROPOLIS_PROMENADE_DRIP_LAST_SOURCE_ROW;
        // Copy one row down within the current buffer, sorting by its screen row.
        if (onScreen) {
            sourceStrip.x  = x;
            sourceStrip.y  = y + bufferY;
            sourceStrip.w  = work->angle;
            sourceStrip.h  = 1;
            movePacket     = gGpuPrimCursor;
            gGpuPrimCursor = movePacket + 1;
            SetDrawMove(movePacket, &sourceStrip, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> ACROPOLIS_PROMENADE_DRIP_OT_DEPTH_SHIFT), movePacket);
        }
        work->age++;
        if (work->age <= work->scale && onScreen) {
            return;
        }
    }
    effectKillTask(work, task);
}

/// Binds the shared star glow to the promenade's exported `void (Task*)` callback.
///
/// Define an unqualified function identifier before including the star fragment;
/// the fragment defines that function and undefines this binding afterwards.
#define GLOW_STAR_TASK acropolisPromenadeGlowStarTask
/// Stores the star core's pixel half-height separately from its equal half-width.
///
/// Numeric configuration for `acropolis_glows_star.inc.c`: nonzero stores
/// `0x1680 / otz` in `OverlaySpriteScratch::cornerDy` and uses it for the core's
/// Y extent; zero or omission reuses `cornerDx` for both axes. The Promenade enables
/// the separate storage; the Bridge omits it. The fragment undefines the
/// switch after compiling this instance.
#define GLOW_STAR_STORE_HALF_HEIGHT 1
#include "../../shared/effects/acropolis_glows_star.inc.c"

/// Places one corner of the ground glow in the task's composed coordinate space.
///
/// `cornerIndex` is 0..3 in GPU strip order; the local XZ half-side is 768
/// game-coordinate units. Requires a live word-aligned scratch block and a
/// coordinate whose `workm` is composed, with 12-fractional-bit rotation.
/// Rotates the local corner through GTE IR, then adds the cached translation,
/// retaining the low 16 bits of each component. Leaves `depth` and the vector's
/// fourth halfword untouched; changes GTE rotation, V0 and result registers.
/// Borrows both objects and retains no pointer.
static __inline__ void _acropolisPromenadeTransformGroundGlowCorner(EffectQuadCornersScratch* quadScratch, s32 cornerIndex, const GfxCoord* coord)
{
    enum { ACROPOLIS_PROMENADE_GROUND_GLOW_HALF_SIDE = 768 };
    quadScratch->vertices[cornerIndex].vx     = _gAcropolisPromenadeGroundGlowUnitCorners[cornerIndex].axis0Sign * ACROPOLIS_PROMENADE_GROUND_GLOW_HALF_SIDE;
    (&quadScratch->vertices[cornerIndex])->vy = 0;
    (&quadScratch->vertices[cornerIndex])->vz = _gAcropolisPromenadeGroundGlowUnitCorners[cornerIndex].axis1Sign * ACROPOLIS_PROMENADE_GROUND_GLOW_HALF_SIDE;
    // Rotate the local corner before adding the composition-root translation.
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&quadScratch->vertices[cornerIndex]);
    gte_rtv0();
    gte_stsv(&quadScratch->vertices[cornerIndex]);
    quadScratch->vertices[cornerIndex].vx     += coord->workm.t[0];
    (&quadScratch->vertices[cornerIndex])->vy += coord->workm.t[1];
    (&quadScratch->vertices[cornerIndex])->vz += coord->workm.t[2];
}

/// Reserves and initializes one ground-glow quad in the current frame's packet arena.
///
/// Requires aligned space for a `POLY_FT4` at `gGpuPrimCursor`. Advances the
/// cursor and returns the packet with its DMA length and textured-quad code
/// set; coordinates, colour and texture fields remain uninitialized. Does not
/// check capacity. The caller fills and links it; storage is borrowed from the
/// current frame arena and must live until GPU completion.
static __inline__ POLY_FT4* _acropolisPromenadeReserveGroundGlowQuad(void)
{
    POLY_FT4* quad;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    return quad;
}

void acropolisPromenadeGroundGlowTask(Task* task)
{
    enum {
        ACROPOLIS_PROMENADE_GROUND_GLOW_DEPTH_BIAS       = 32,
        ACROPOLIS_PROMENADE_GROUND_GLOW_MIN_DEPTH        = 17,
        ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V        = 16,
        ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL       = 39,
        ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_PAGE     = getTPage(0, GPU_BLEND_ADD, 704, 0),
        ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_CLUT     = getClut(16, 270),
        ACROPOLIS_PROMENADE_GROUND_GLOW_GREY_LEVEL_COUNT = 16
    };
    GfxCoord*                 coord;
    EffectWork*               work;
    EffectQuadCornersScratch* quadScratch;
    POLY_FT4*                 quad;
    s32                       cornerIndex;
    s32                       grey;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    work->age   = task->spawnArg1.value;
    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    // Rotate the local XZ square, then translate its signed 16-bit corners.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(_gAcropolisPromenadeGroundGlowUnitCorners); cornerIndex++) {
        _acropolisPromenadeTransformGroundGlowCorner(quadScratch, cornerIndex, coord);
    }
    // Project into the packet; a rejected quad still consumes packet space.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    quad = _acropolisPromenadeReserveGroundGlowQuad();
    gte_stsxy(&quad->x0);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    quad->u0 = 0;
    quad->v0 = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V;
    quad->u1 = ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL;
    quad->v1 = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V;
    quad->u2 = 0;
    quad->v2 = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V + ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL;
    quad->u3 = ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL;
    quad->v3 = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_V + ACROPOLIS_PROMENADE_GROUND_GLOW_LAST_TEXEL;
    gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
    gte_stszotz(&quadScratch->depth);
    quadScratch->depth += ACROPOLIS_PROMENADE_GROUND_GLOW_DEPTH_BIAS;
    if (quadScratch->depth >= ACROPOLIS_PROMENADE_GROUND_GLOW_MIN_DEPTH) {
        quad->tpage     = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_PAGE;
        quad->clut      = ACROPOLIS_PROMENADE_GROUND_GLOW_TEXTURE_CLUT;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        grey            = (gRandomLcgState >> 16) & (ACROPOLIS_PROMENADE_GROUND_GLOW_GREY_LEVEL_COUNT - 1);
        quad->r0        = grey;
        quad->g0        = grey;
        quad->b0        = grey;
        setSemiTrans(quad, 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
    effectKillTask(work, task);
}

/// Binds the included lamp glow to the promenade's exported `void (Task*)` callback.
///
/// Define this function identifier immediately before the lamp fragment, which
/// undefines it. No arguments, conditional alternatives or tokens are constructed.
#define GLOW_LAMP_TASK acropolisPromenadeGlowLampTask
#include "../../shared/effects/acropolis_glows_lamp.inc.c"

#include "../../shared/effects/glow_draw_tinted_disc_no_bias.inc.c"
