#include "rooms/shelter_b1_pod_access_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_01100.h"
#include "actors/actor_02100.h"
#include "actors/actor_02300.h"
#include "actors/actor_141000.h"

#include "gameplay/companion_load.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
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
#include "main/event_flags.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_events.h"
#include "../../shared/actor/actor_messages.h"

#include "assets/engine/animation/player_schema.h"

/// Logical view containing the two-image pod-chamber pan.
enum { SHELTER_B1_POD_ACCESS_TUNNEL_POD_PAN_VIEW               = 11,
       SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_FRACTION_BITS = 16 };

enum {
    SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_DELAY_FRAMES = 46,  // Frames the lower image is held still before the scroll starts
    SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_LINES        = 240, // Screen lines the seam travels, the height of either image
};

/// Replies to the room-transition dispatcher: refuse, use its warp, or handle it here.
enum { SHELTER_B1_POD_ACCESS_TUNNEL_EVENT_BLOCKED           = 0,
       SHELTER_B1_POD_ACCESS_TUNNEL_EVENT_DEFAULT_DEPARTURE = 1,
       SHELTER_B1_POD_ACCESS_TUNNEL_EVENT_HANDLED           = 2 };

/// Stages of the return from the gantry, stored in `GAME_FLAG_118`.
enum { SHELTER_B1_POD_ACCESS_TUNNEL_GANTRY_RETURN_PENDING  = 1,
       SHELTER_B1_POD_ACCESS_TUNNEL_GANTRY_RETURN_COMPLETE = 2 };

/// Travel-task descriptor indices in the room's two-entry table.
enum { SHELTER_B1_POD_ACCESS_TUNNEL_TRAVEL_TO_B2     = 0,
       SHELTER_B1_POD_ACCESS_TUNNEL_TRAVEL_TO_GANTRY = 1 };

enum {
    SHELTER_B1_POD_ACCESS_TUNNEL_PLAYER_SCENE_FIRST_CLIP = ANIMATION_BANK_BASE_SET_COUNT + 1,
};

/// CAP selections and placed-Eve commands used by the room's event scripts.
enum {
    SHELTER_B1_POD_ACCESS_TUNNEL_CAP_ENTRY_SCENE       = 2,
    SHELTER_B1_POD_ACCESS_TUNNEL_CAP_POST_GANTRY       = 7,
    SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX       = 0,
    SHELTER_B1_POD_ACCESS_TUNNEL_EVE_COMMAND_SLOW_WALK = 1,
    SHELTER_B1_POD_ACCESS_TUNNEL_EVE_COMMAND_FAST_WALK = 2,
};

/// Resource keys shared by this room's placement and loading tables.
enum {
    SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_MOSSBACK       = 11,
    SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER        = 21,
    SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_ROOK           = 23,
    SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_BRUTE_MOSSBACK = 49,
    SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_EVE            = 132,
};

/// Watcher variants pack patrol range in decimal tens and weapon choice in units.
enum {
    SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_STATIONARY_BEAM0 = 0,
    SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_STATIONARY_BEAM1 = 1,
    SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_STATIONARY_GUN   = 4,
    SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_PATROL_BEAM0     = 10,
};

/// Rook placement-mode bit enabling patrol; the variant gives thousands of game units.
enum { SHELTER_B1_POD_ACCESS_TUNNEL_ROOK_PATROLS = 1 };

/// Pod interaction ID sent by the action quad to the room command handler.
enum { SHELTER_B1_POD_ACCESS_TUNNEL_POD_COMMAND_USE = 1 };

/// Work block of the task that scrolls one full-screen image vertically into
/// another: after a fixed delay the seam between the two moves down the screen
/// at a constant rate.
///
/// The task's set-up state allocates it zeroed and derives `speed` from the
/// spawn argument, a nonzero frame count the scroll is to take; the default
/// teardown frees it.
typedef struct {
    s32 speed;  // Lines the seam moves per frame, 16.16 fixed point
    s32 offset; // Lines scrolled so far, 16.16 fixed point; the seam is drawn at its integer part, capped at the full distance
    s16 timer;  // Frames drawn so far, counted up from 0; scrolling starts once the delay has passed
} _ShelterB1PodAccessTunnelImageScrollWork;
STATIC_ASSERT_SIZEOF(_ShelterB1PodAccessTunnelImageScrollWork, 0xC);

extern RoomFadeStorage  gRoomEventFade;
static RoomEventMsg     _gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

static void _shelterB1PodAccessTunnelInitializeRoomTask(Task* task);
static void _shelterB1PodAccessTunnelIdleTaskState(Task* task);
static void _shelterB1PodAccessTunnelDrawImageScroll(Task* task);
static void _shelterB1PodAccessTunnelInitImageScroll(Task* task);
static void _shelterB1PodAccessTunnelQueueImageScrollTexturePage(s32 vramX, s16 vramY);

static void _shelterB1PodAccessTunnelPostGantryDialogueTask(Task* task);

static void _shelterB1PodAccessTunnelRideToB2Task(Task* task);
static void _shelterB1PodAccessTunnelTravelToGantryTask(Task* task);

static s32 _shelterB1PodAccessTunnelResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB1PodAccessTunnelRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg);
static s32 _shelterB1PodAccessTunnelHandleCommandMessage(Task* task, s32 messageId, s32 command, s32 secondArg);
static s32 _shelterB1PodAccessTunnelIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* actionRequest, s32 secondArg);
static s32 _shelterB1PodAccessTunnelHandleSoundMessage(Task* task, s32 messageId, s32 soundCommand, s32 secondArg);

static void _shelterB1PodAccessTunnelPanSceneCameraTask(Task* task);
static void _shelterB1PodAccessTunnelImageScrollTask(Task* task);
static void _shelterB1PodAccessTunnelGrayCaptureCountdownTask(Task* task);

static void _shelterB1PodAccessTunnelStageSceneAudioStart(void);
static void _shelterB1PodAccessTunnelStartScenePlayback(void);
static void _shelterB1PodAccessTunnelFinishScene(void);
static void _shelterB1PodAccessTunnelCancelScene(void);
static void _shelterB1PodAccessTunnelStartSceneCameraPan(s32 frameCount);
static void _shelterB1PodAccessTunnelStartImageScroll(s32 frameCount);
static void _shelterB1PodAccessTunnelStartFlightController(void);
static void _shelterB1PodAccessTunnelStartGrayCapture(s32 frameCount);
static void _shelterB1PodAccessTunnelCancelRoomEffects(void);

/// Bodyless recipe for a latched CAP/sound departure followed by session reload.
///
/// Spawn at index 0 with unused payloads after filling the room's staged-event
/// records. Only one staged departure may use those records at a time; keep
/// them and the room overlay loaded until reload discards the old task list.
static TaskDesc _gShelterB1PodAccessTunnelStagedDepartureTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_SCENE } },
    .callback = roomEventStagedTask,
    .data     = { .value = 0 },
};

/// Room event, key-item, action, command and sound handlers, ending at TASK_MESSAGE_TABLE_END.
///
/// The room task borrows this table while registered. Each callback uses the
/// payload types selected by its message ID; this overlay must remain loaded.
static TaskMessageEntry _gShelterB1PodAccessTunnelRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB1PodAccessTunnelResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB1PodAccessTunnelRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1PodAccessTunnelIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, _shelterB1PodAccessTunnelHandleCommandMessage },
    { ROOM_MESSAGE_SOUND, _shelterB1PodAccessTunnelHandleSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless travel recipes: index 0 rides to B2, index 1 travels to the gantry.
///
/// No terminator. Both start at state 0 with unused payloads and held player
/// control; their CAP choices either restore control or commit a session reload.
/// Keep this room overlay loaded through the travel task's completion.
static TaskDesc _gShelterB1PodAccessTunnelTravelTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB1PodAccessTunnelRideToB2Task, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB1PodAccessTunnelTravelToGantryTask, { .value = 0 } },
};

/// Return-from-gantry dialogue: CAP selection 7, area music and player-control restoration.
///
/// Seven instructions including END; each CAP wait yields at its cue. The
/// interpreter borrows the stream through completion. Requires this room's
/// CAP resources and live CAP/player task slots; invoked with HUD restoration.
static EvsCommand _gShelterB1PodAccessTunnelPostGantryDialogueScript[7] = {
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_CAP_CONTROL }, .operand2 = { .value = CAP_CONTROL_MESSAGE_START }, .operand3 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_CAP_POST_GANTRY } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_START_AREA_MUSIC },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, .operand3 = { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 3 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Bodyless return-from-gantry dialogue recipe, starting at state 0 with unused payloads.
///
/// Requires the player, its weapon bank and room CAP resources to stay loaded
/// until dialogue completion. The spawned task waits for pending displays to close.
static TaskDesc _gShelterB1PodAccessTunnelPostGantryDialogueTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    .callback = _shelterB1PodAccessTunnelPostGantryDialogueTask,
    .data     = { .value = 0 },
};

static AnimationPackedPose _gShelterB1PodAccessTunnelAnimation03EF0Bank1[6] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_03EF0_bank1.inc"
};

static AnimationPackedRotation _gShelterB1PodAccessTunnelAnimation03EF0Bank4[46] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_03EF0_bank4.inc"
};

static AnimationRecord _gShelterB1PodAccessTunnelAnimation03EF0Records[109] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_03EF0_records.inc"
};

static u16 _gShelterB1PodAccessTunnelAnimation03EF0Indices[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_03EF0_indices.inc"
};

static AnimationSet _gShelterB1PodAccessTunnelAnimation03EF0 = {
    _gShelterB1PodAccessTunnelAnimation03EF0Records,
    _gShelterB1PodAccessTunnelAnimation03EF0Indices,
    { NULL, _gShelterB1PodAccessTunnelAnimation03EF0Bank1, NULL, NULL, _gShelterB1PodAccessTunnelAnimation03EF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1PodAccessTunnelAnimation041A4Bank1[4] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_041A4_bank1.inc"
};

static AnimationPackedRotation _gShelterB1PodAccessTunnelAnimation041A4Bank4[49] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_041A4_bank4.inc"
};

static AnimationRecord _gShelterB1PodAccessTunnelAnimation041A4Records[92] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_041A4_records.inc"
};

static u16 _gShelterB1PodAccessTunnelAnimation041A4Indices[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_041A4_indices.inc"
};

static AnimationSet _gShelterB1PodAccessTunnelAnimation041A4 = {
    _gShelterB1PodAccessTunnelAnimation041A4Records,
    _gShelterB1PodAccessTunnelAnimation041A4Indices,
    { NULL, _gShelterB1PodAccessTunnelAnimation041A4Bank1, NULL, NULL, _gShelterB1PodAccessTunnelAnimation041A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1PodAccessTunnelAnimation048DCBank1[14] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_048DC_bank1.inc"
};

static AnimationPackedRotation _gShelterB1PodAccessTunnelAnimation048DCBank4[159] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_048DC_bank4.inc"
};

static AnimationRecord _gShelterB1PodAccessTunnelAnimation048DCRecords[241] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_048DC_records.inc"
};

static u16 _gShelterB1PodAccessTunnelAnimation048DCIndices[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_048DC_indices.inc"
};

static AnimationSet _gShelterB1PodAccessTunnelAnimation048DC = {
    _gShelterB1PodAccessTunnelAnimation048DCRecords,
    _gShelterB1PodAccessTunnelAnimation048DCIndices,
    { NULL, _gShelterB1PodAccessTunnelAnimation048DCBank1, NULL, NULL, _gShelterB1PodAccessTunnelAnimation048DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1PodAccessTunnelAnimation04BA0Bank1[4] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04BA0_bank1.inc"
};

static AnimationPackedRotation _gShelterB1PodAccessTunnelAnimation04BA0Bank4[35] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04BA0_bank4.inc"
};

static AnimationRecord _gShelterB1PodAccessTunnelAnimation04BA0Records[110] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04BA0_records.inc"
};

static u16 _gShelterB1PodAccessTunnelAnimation04BA0Indices[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04BA0_indices.inc"
};

static AnimationSet _gShelterB1PodAccessTunnelAnimation04BA0 = {
    _gShelterB1PodAccessTunnelAnimation04BA0Records,
    _gShelterB1PodAccessTunnelAnimation04BA0Indices,
    { NULL, _gShelterB1PodAccessTunnelAnimation04BA0Bank1, NULL, NULL, _gShelterB1PodAccessTunnelAnimation04BA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1PodAccessTunnelAnimation04DF4Bank1[2] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04DF4_bank1.inc"
};

static AnimationPackedRotation _gShelterB1PodAccessTunnelAnimation04DF4Bank4[29] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04DF4_bank4.inc"
};

static AnimationRecord _gShelterB1PodAccessTunnelAnimation04DF4Records[94] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04DF4_records.inc"
};

static u16 _gShelterB1PodAccessTunnelAnimation04DF4Indices[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_04DF4_indices.inc"
};

static AnimationSet _gShelterB1PodAccessTunnelAnimation04DF4 = {
    _gShelterB1PodAccessTunnelAnimation04DF4Records,
    _gShelterB1PodAccessTunnelAnimation04DF4Indices,
    { NULL, _gShelterB1PodAccessTunnelAnimation04DF4Bank1, NULL, NULL, _gShelterB1PodAccessTunnelAnimation04DF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1PodAccessTunnelAnimation05378Bank1[10] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05378_bank1.inc"
};

static AnimationPackedRotation _gShelterB1PodAccessTunnelAnimation05378Bank4[135] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05378_bank4.inc"
};

static AnimationRecord _gShelterB1PodAccessTunnelAnimation05378Records[168] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05378_records.inc"
};

static u16 _gShelterB1PodAccessTunnelAnimation05378Indices[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05378_indices.inc"
};

static AnimationSet _gShelterB1PodAccessTunnelAnimation05378 = {
    _gShelterB1PodAccessTunnelAnimation05378Records,
    _gShelterB1PodAccessTunnelAnimation05378Indices,
    { NULL, _gShelterB1PodAccessTunnelAnimation05378Bank1, NULL, NULL, _gShelterB1PodAccessTunnelAnimation05378Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1PodAccessTunnelAnimation05744Bank1[6] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05744_bank1.inc"
};

static AnimationPackedRotation _gShelterB1PodAccessTunnelAnimation05744Bank4[64] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05744_bank4.inc"
};

static AnimationRecord _gShelterB1PodAccessTunnelAnimation05744Records[141] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05744_records.inc"
};

static u16 _gShelterB1PodAccessTunnelAnimation05744Indices[20] = {
#include "assets/shelter_b1_pod_access_tunnel_animation_05744_indices.inc"
};

static AnimationSet _gShelterB1PodAccessTunnelAnimation05744 = {
    _gShelterB1PodAccessTunnelAnimation05744Records,
    _gShelterB1PodAccessTunnelAnimation05744Indices,
    { NULL, _gShelterB1PodAccessTunnelAnimation05744Bank1, NULL, NULL, _gShelterB1PodAccessTunnelAnimation05744Bank4, NULL, NULL, NULL },
};

/// Bodyless entry-scene recipes: camera pan, image scroll and gray-frame countdown.
///
/// Indices 0..2 have no terminator. The first payload is a frame count and the
/// second is unused: pan accepts 1..32767, scroll requires a positive count,
/// and gray capture accepts a nonnegative count and lasts one extra tick.
/// The gray-countdown recipe runs on the display-mode list; the others use the
/// current list. Keep this room overlay loaded while any callback is active.
static TaskDesc _gShelterB1PodAccessTunnelSceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _shelterB1PodAccessTunnelPanSceneCameraTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _shelterB1PodAccessTunnelImageScrollTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _shelterB1PodAccessTunnelGrayCaptureCountdownTask, { .value = 0 } },
};

/// Player entry-scene bank extension: null slot 47 followed by clips 48..54.
///
/// Eight set-pointer words, with no terminator; the leading NULL is a vacant
/// bank entry. The copy request borrows the table synchronously, and playback
/// borrows the seven clip descriptors and their data while this room is loaded.
static AnimationSet* _gShelterB1PodAccessTunnelPlayerSceneAnimationSets[8] = {
    NULL,
    &_gShelterB1PodAccessTunnelAnimation03EF0,
    &_gShelterB1PodAccessTunnelAnimation041A4,
    &_gShelterB1PodAccessTunnelAnimation048DC,
    &_gShelterB1PodAccessTunnelAnimation04BA0,
    &_gShelterB1PodAccessTunnelAnimation04DF4,
    &_gShelterB1PodAccessTunnelAnimation05378,
    &_gShelterB1PodAccessTunnelAnimation05744,
};

/// Installs the eight entry-scene words in the equipped weapon's bank extension.
///
/// Copies slots 47..54 without changing later extension entries or starting
/// playback. Dispatch borrows the request and source table synchronously;
/// copied clip pointers require this room overlay to remain loaded while used.
static AnimationBankCopyRequest _gShelterB1PodAccessTunnelPlayerSceneAnimationBankCopy = {
    .sets  = _gShelterB1PodAccessTunnelPlayerSceneAnimationSets,
    .count = ARRAY_SIZE(_gShelterB1PodAccessTunnelPlayerSceneAnimationSets),
};

/// Unreferenced playback-shaped record: source index 1, clip 1, reset and collision enabled.
///
/// No consumer establishes the intended receiver or whether the source word
/// would be replaced with a weapon-bank index before playback.
static AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182D78 = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene clip 48 request, interpolated over 20 frames before control returns.
///
/// The script resolves the equipped weapon's bank in a copy of this request;
/// source index 1 is replaced before dispatch. Restores world collision.
/// The extended clip data must remain loaded through playback.
static AnimationPlayRequest _gShelterB1PodAccessTunnelPlayerSceneClip48Request = {
    .source               = { .index = 1 },
    .animationId          = SHELTER_B1_POD_ACCESS_TUNNEL_PLAYER_SCENE_FIRST_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene clip 49 request before the first gray captured-frame transition.
///
/// Resets playback and restores world collision. The script replaces source
/// index 1 in a copy with the equipped weapon's bank before dispatch; the
/// extended clip data must remain loaded through playback.
static AnimationPlayRequest _gShelterB1PodAccessTunnelPlayerSceneClip49Request = {
    .source               = { .index = 1 },
    .animationId          = SHELTER_B1_POD_ACCESS_TUNNEL_PLAYER_SCENE_FIRST_CLIP + 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene clip 50 request when the live player replaces the scripted actor.
///
/// Resets playback and restores world collision. The script replaces source
/// index 1 in a copy with the equipped weapon's bank before dispatch; the
/// extended clip data must remain loaded through playback.
static AnimationPlayRequest _gShelterB1PodAccessTunnelPlayerSceneClip50Request = {
    .source               = { .index = 1 },
    .animationId          = SHELTER_B1_POD_ACCESS_TUNNEL_PLAYER_SCENE_FIRST_CLIP + 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene clip 51 request after framebuffer blending ends.
///
/// Resets playback and restores world collision. The script replaces source
/// index 1 in a copy with the equipped weapon's bank before dispatch; the
/// extended clip data must remain loaded through playback.
static AnimationPlayRequest _gShelterB1PodAccessTunnelPlayerSceneClip51Request = {
    .source               = { .index = 1 },
    .animationId          = SHELTER_B1_POD_ACCESS_TUNNEL_PLAYER_SCENE_FIRST_CLIP + 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene clip 52 request at the CAP cue following clip 51.
///
/// Resets playback and restores world collision. The script replaces source
/// index 1 in a copy with the equipped weapon's bank before dispatch; the
/// extended clip data must remain loaded through playback.
static AnimationPlayRequest _gShelterB1PodAccessTunnelPlayerSceneClip52Request = {
    .source               = { .index = 1 },
    .animationId          = SHELTER_B1_POD_ACCESS_TUNNEL_PLAYER_SCENE_FIRST_CLIP + 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene player clip 53 before the final return to clip 48.
///
/// The interpreter replaces source index 1 in a copy with the equipped
/// weapon's bank before dispatch. Restores world collision; extended clip
/// storage must remain loaded through playback.
static AnimationPlayRequest _gShelterB1PodAccessTunnelPlayerSceneClip53Request = {
    .source               = { .index = 1 },
    .animationId          = SHELTER_B1_POD_ACCESS_TUNNEL_PLAYER_SCENE_FIRST_CLIP + 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Entry-scene opening player clip 54, blended over ten frames.
///
/// The interpreter replaces source index 1 in a copy with the equipped
/// weapon's bank before dispatch. Restores world collision; extended clip
/// storage must remain loaded through playback.
static AnimationPlayRequest _gShelterB1PodAccessTunnelPlayerSceneClip54Request = {
    .source               = { .index = 1 },
    .animationId          = SHELTER_B1_POD_ACCESS_TUNNEL_PLAYER_SCENE_FIRST_CLIP + 6,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Live-player placement for the opening clip 54.
///
/// XYZ are room-coordinate game units and Euler angles use 4096 units per
/// turn. Placement copies XYZ and Euler angles synchronously.
static ActorTransform _gShelterB1PodAccessTunnelSceneOpeningPlayerTransform = {
    .pos = { 1740, 0, -5620, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Live-player placement for clip 49 before the gray-frame capture.
///
/// XYZ are room-coordinate game units and Euler angles use 4096 units per
/// turn. Placement copies XYZ and Euler angles synchronously.
static ActorTransform _gShelterB1PodAccessTunnelSceneGrayCapturePlayerTransform = {
    .pos = { 1690, 0, -6060, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Live-player placement shared by normal entry-scene completion and scene skip.
///
/// World position (2350, 0, -4800), with zero pitch, yaw and roll in 4096-unit
/// turns. Placement dispatch consumes the record synchronously.
static ActorTransform _gShelterB1PodAccessTunnelSceneExitPlayerTransform = {
    .pos = { 2350, 0, -4800, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Four unreferenced playback-shaped records: source index 0 and clips 0..3.
///
/// Stores reset/blend choices and whole-frame durations with collision disabled.
/// No consumer establishes a receiver, valid clip domain or playback order.
static AnimationPlayRequest D_shelter_b1_pod_access_tunnel_80182E60[4] = {
    { .source = { .index = 0 }, .animationId = 0, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 0 }, .animationId = 1, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = 20, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 0 }, .animationId = 2, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = 20, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
    { .source = { .index = 0 }, .animationId = 3, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = 10, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Plays scripted Eve's bank-0 clip 4 after the slow-walk delay.
///
/// Borrowed through synchronous dispatch. The actor reads the bank, clip and
/// whole-frame blend duration, ignoring the world-collision word. Its actor
/// overlay and clip data must remain loaded through playback.
static AnimationPlayRequest _gShelterB1PodAccessTunnelSceneEveClip4Request = {
    .source               = { .index = 0 },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 5,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays scripted Eve's bank-0 clip 5 six interpreter-wait frames after clip 4.
///
/// Borrowed through synchronous dispatch. The actor reads the bank, clip and
/// whole-frame blend duration, ignoring the world-collision word. Its actor
/// overlay and clip data must remain loaded through playback.
static AnimationPlayRequest _gShelterB1PodAccessTunnelSceneEveClip5Request = {
    .source               = { .index = 0 },
    .animationId          = 5,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 5,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays scripted Eve's bank-0 clip 6 after the flying model starts.
///
/// Borrowed through synchronous dispatch. The actor reads the bank, clip and
/// whole-frame blend duration, ignoring the world-collision word. Its actor
/// overlay and clip data must remain loaded through playback.
static AnimationPlayRequest _gShelterB1PodAccessTunnelSceneEveClip6Request = {
    .source               = { .index = 0 },
    .animationId          = 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays scripted Eve's bank-0 clip 7 before the pod-chamber image scroll.
///
/// Borrowed through synchronous dispatch. The actor reads the bank, clip and
/// whole-frame blend duration, ignoring the world-collision word. Its actor
/// overlay and clip data must remain loaded through playback.
static AnimationPlayRequest _gShelterB1PodAccessTunnelSceneEveClip7Request = {
    .source               = { .index = 0 },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays scripted Eve's bank-0 clip 8 during the pod-chamber camera pan.
///
/// Borrowed through synchronous dispatch. The actor reads the bank, clip and
/// whole-frame blend duration, ignoring the world-collision word. Its actor
/// overlay and clip data must remain loaded through playback.
static AnimationPlayRequest _gShelterB1PodAccessTunnelSceneEveClip8Request = {
    .source               = { .index = 0 },
    .animationId          = 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays scripted Eve's bank-0 clip 9 at the CAP cue following clip 8.
///
/// Borrowed through synchronous dispatch. The actor reads the bank, clip and
/// whole-frame blend duration, ignoring the world-collision word. Its actor
/// overlay and clip data must remain loaded through playback.
static AnimationPlayRequest _gShelterB1PodAccessTunnelSceneEveClip9Request = {
    .source               = { .index = 0 },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced bank-0 clip-10 request with a twenty-frame blend and collision disabled.
///
/// The intended receiver is unproven.
static AnimationPlayRequest _gShelterB1PodAccessTunnelUnreferencedClip10Request = {
    .source               = { .index = 0 },
    .animationId          = 10,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Scripted Eve's placement before her slow walk.
///
/// XYZ are room-coordinate game units and Euler angles use 4096 units per
/// turn. Placement copies XYZ and Euler angles synchronously.
static ActorTransform _gShelterB1PodAccessTunnelSceneEveSlowWalkStartTransform = {
    .pos = { 1760, 0, -5100, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Scripted Eve's placement for bank-0 clip 6.
///
/// XYZ are room-coordinate game units and Euler angles use 4096 units per
/// turn. Placement copies XYZ and Euler angles synchronously; authored yaw is 2047.
static ActorTransform _gShelterB1PodAccessTunnelSceneEveClip6Transform = {
    .pos = { 1740, 0, -3970, 0 },
    .rot = { 0, 2047, 0, 0 },
};

/// Scripted Eve's placement before her fast walk.
///
/// XYZ are room-coordinate game units and Euler angles use 4096 units per
/// turn. Placement copies XYZ and Euler angles synchronously.
static ActorTransform _gShelterB1PodAccessTunnelSceneEveFastWalkStartTransform = {
    .pos = { 4000, 0, -1650, 0 },
    .rot = { 0, ONE / 4, 0, 0 },
};

/// Scripted Eve's fast-walk destination and final facing.
///
/// XYZ are room-coordinate game units and Euler angles use 4096 units per
/// turn. Walk dispatch copies XYZ and Euler angles and selects the pace-default clips.
static ActorTransform _gShelterB1PodAccessTunnelSceneEveFastWalkTarget = {
    .pos = { 6700, 0, -1650, 0 },
    .rot = { 0, ONE / 4, 0, 0 },
};

/// Scripted Eve's placement for bank-0 clip 7 and the pod panorama.
///
/// XYZ are room-coordinate game units and Euler angles use 4096 units per
/// turn. Placement copies XYZ and Euler angles synchronously.
static ActorTransform _gShelterB1PodAccessTunnelSceneEveClip7Transform = {
    .pos = { 7100, 0, 2920, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Scripted Eve's placement for bank-0 clip 8 during the camera pan.
///
/// XYZ are room-coordinate game units and Euler angles use 4096 units per
/// turn. Placement copies XYZ and Euler angles synchronously.
static ActorTransform _gShelterB1PodAccessTunnelSceneEveClip8Transform = {
    .pos = { 6930, 0, 3710, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Scripted Eve's slow-walk destination and final facing.
///
/// XYZ are room-coordinate game units and Euler angles use 4096 units per
/// turn. Walk dispatch copies XYZ and Euler angles and uses the supplied clip pair.
static ActorTransform _gShelterB1PodAccessTunnelSceneEveSlowWalkTarget = {
    .pos = { 1760, 0, -4100, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Selects half displacement and default walk clip 10 for scripted Eve's later walks.
///
/// Does not alter an ongoing walk. Dispatch borrows the command synchronously;
/// the stored stage/area context is ignored by Eve's pace handler.
static ActorCommand _gShelterB1PodAccessTunnelSceneEveSlowWalkCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL } },
    .command = SHELTER_B1_POD_ACCESS_TUNNEL_EVE_COMMAND_SLOW_WALK,
};

/// Selects full displacement and default walk clip 2 for scripted Eve's later walks.
///
/// Does not alter an ongoing walk. Dispatch borrows the command synchronously;
/// the stored stage/area context is ignored by Eve's pace handler.
static ActorCommand _gShelterB1PodAccessTunnelSceneEveFastWalkCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL } },
    .command = SHELTER_B1_POD_ACCESS_TUNNEL_EVE_COMMAND_FAST_WALK,
};

/// Scripted Eve's slow-walk clips: bank-0 clip 10 while walking, clip 3 on arrival.
///
/// Borrowed only through synchronous dispatch; the receiver keeps both clip
/// choices in its work. The actor's clip data must remain loaded for the walk.
static ActorMotionWalkAnim _gShelterB1PodAccessTunnelSceneEveSlowWalkClips = {
    .animationId = 10,
    .nextAnimId  = 3,
};

/// Scene/audio stream key for the tunnel's first-entry pod scene.
///
/// Selects group 4, stream 10, first qualifier 11 and implicit second qualifier 0.
/// Caption playback retains this record until the event ends or replaces it.
static EvsSceneKey _gShelterB1PodAccessTunnelEntrySceneKey = {
    .group    = 4,
    .streamId = 10,
    .subId    = 11,
};

/// One-time pod entry scene, replacing the live player with scripted Eve for the panorama.
///
/// Eighty-six instructions including END, with no calls or branches. Installs
/// player clips 47..54, synchronizes CAP/audio and fade effects, then restores
/// the player, ambient light, music and logical view 2. The interpreter borrows
/// this stream and its records through completion; actor 141000 and the room's
/// clip/CAP/image resources must remain loaded. Scene messages select placed
/// Eve at index 0, not the scene manager.
static EvsCommand _gShelterB1PodAccessTunnelEntrySceneScript[86] = {
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, .operand3 = { .message = { .animationBankCopy = &_gShelterB1PodAccessTunnelPlayerSceneAnimationBankCopy } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, .operand0 = { .value = 100 }, .operand1 = { .value = 100 }, .operand2 = { .value = 100 } },
    { .opcode = EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .value = ACTOR_MESSAGE_DRAW_HIDE } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_CAP_CONTROL }, .operand2 = { .value = CAP_CONTROL_MESSAGE_START }, .operand3 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_CAP_ENTRY_SCENE } },
    { .opcode = EVENT_SCRIPT_OPCODE_SELECT_SCENE, .operand0 = { .sceneKey = &_gShelterB1PodAccessTunnelEntrySceneKey } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _shelterB1PodAccessTunnelStageSceneAudioStart } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gShelterB1PodAccessTunnelPlayerSceneClip54Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gShelterB1PodAccessTunnelSceneOpeningPlayerTransform } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _shelterB1PodAccessTunnelStartScenePlayback } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gShelterB1PodAccessTunnelPlayerSceneClip49Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gShelterB1PodAccessTunnelSceneGrayCapturePlayerTransform } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 4 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _shelterB1PodAccessTunnelStartGrayCapture }, .operand1 = { .value = 12 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, .operand0 = { .value = GPU_BLEND_AVERAGE }, .operand1 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .value = ACTOR_MESSAGE_DRAW_SHOW } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_COMMAND_MESSAGE_APPLY }, .operand3 = { .message = { .command = &_gShelterB1PodAccessTunnelSceneEveSlowWalkCommand } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gShelterB1PodAccessTunnelSceneEveSlowWalkStartTransform } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_WALK_TO }, .operand3 = { .message = { .transform = &_gShelterB1PodAccessTunnelSceneEveSlowWalkTarget } }, .operand4 = { .message = { .pointer = &_gShelterB1PodAccessTunnelSceneEveSlowWalkClips } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 110 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, .operand3 = { .message = { .animation = &_gShelterB1PodAccessTunnelSceneEveClip4Request } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 6 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, .operand3 = { .message = { .animation = &_gShelterB1PodAccessTunnelSceneEveClip5Request } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _shelterB1PodAccessTunnelStartFlightController } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gShelterB1PodAccessTunnelSceneEveClip6Transform } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, .operand3 = { .message = { .animation = &_gShelterB1PodAccessTunnelSceneEveClip6Request } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_COMMAND_MESSAGE_APPLY }, .operand3 = { .message = { .command = &_gShelterB1PodAccessTunnelSceneEveFastWalkCommand } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gShelterB1PodAccessTunnelSceneEveFastWalkStartTransform } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_WALK_TO }, .operand3 = { .message = { .transform = &_gShelterB1PodAccessTunnelSceneEveFastWalkTarget } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 20 } },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, .operand0 = { .value = GPU_BLEND_AVERAGE }, .operand1 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, .operand3 = { .message = { .animation = &_gShelterB1PodAccessTunnelSceneEveClip7Request } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gShelterB1PodAccessTunnelSceneEveClip7Transform } } },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _shelterB1PodAccessTunnelStartImageScroll }, .operand1 = { .value = 240 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 44 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _shelterB1PodAccessTunnelStartSceneCameraPan }, .operand1 = { .value = 150 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, .operand3 = { .message = { .animation = &_gShelterB1PodAccessTunnelSceneEveClip8Request } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gShelterB1PodAccessTunnelSceneEveClip8Transform } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, .operand3 = { .message = { .animation = &_gShelterB1PodAccessTunnelSceneEveClip9Request } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, .operand0 = { .value = GPU_BLEND_AVERAGE }, .operand1 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .value = ACTOR_MESSAGE_DRAW_HIDE } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gShelterB1PodAccessTunnelPlayerSceneClip50Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gShelterB1PodAccessTunnelSceneExitPlayerTransform } } },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, .operand0 = { .value = 1 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 35 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gShelterB1PodAccessTunnelPlayerSceneClip51Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gShelterB1PodAccessTunnelPlayerSceneClip52Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gShelterB1PodAccessTunnelPlayerSceneClip53Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 30 } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gShelterB1PodAccessTunnelPlayerSceneClip48Request } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, .operand3 = { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT } },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _shelterB1PodAccessTunnelFinishScene } },
    { .opcode = EVENT_SCRIPT_OPCODE_START_AREA_MUSIC },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_VIEW, .operand0 = { .value = 2 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 3 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip path restoring the player, view and room effects after the pod entry scene.
///
/// Seventeen instructions including END. Runs from any accepted entry-scene
/// skip: places and shows the player, resumes control, hides scripted Eve and
/// clears the ambient override, then cancels effects and returns the primary
/// fade. Borrows this room's stream and records through completion; requires
/// live player, CAP and room-effect tasks and loaded scene resources.
static EvsCommand _gShelterB1PodAccessTunnelEntrySceneSkipScript[17] = {
    { .opcode = EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, .operand0 = { .value = GPU_BLEND_AVERAGE } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 8 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gShelterB1PodAccessTunnelSceneExitPlayerTransform } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, .operand3 = { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = SHELTER_B1_POD_ACCESS_TUNNEL_SCENE_EVE_INDEX }, .operand2 = { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, .operand3 = { .value = ACTOR_MESSAGE_DRAW_HIDE } },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _shelterB1PodAccessTunnelCancelScene } },
    { .opcode = EVENT_SCRIPT_OPCODE_START_AREA_MUSIC },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, .operand0 = { .value = 2 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 2 } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _shelterB1PodAccessTunnelCancelRoomEffects } },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEANUP_SCENE },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 8 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Four transverse glow segments, each an ordered pair of world-space endpoints.
///
/// Every segment is 1000 game units along X at Y=-2726. Views 3 and 7 draw
/// all four; view 4 draws the first two. The capsule drawer borrows one pair.
static SVECTOR _gShelterB1PodAccessTunnelTransverseLightSegments[4][2] = {
    { { 6073, -2726, -1860, 0 }, { 5073, -2726, -1860, 0 } },
    { { 6073, -2726, -1628, 0 }, { 5073, -2726, -1628, 0 } },
    { { 3262, -2726, -1860, 0 }, { 2262, -2726, -1860, 0 } },
    { { 3262, -2726, -1628, 0 }, { 2262, -2726, -1628, 0 } },
};

/// Two longitudinal glow segments, each an ordered pair of world-space endpoints.
///
/// Mapped view 2 draws both 1000-game-unit Z segments at Y=-2726 and
/// X=1859/1621. The capsule drawer borrows one pair for the duration of a call.
static SVECTOR _gShelterB1PodAccessTunnelLongitudinalLightSegments[2][2] = {
    { { 1859, -2726, -4216, 0 }, { 1859, -2726, -5216, 0 } },
    { { 1621, -2726, -4216, 0 }, { 1621, -2726, -5216, 0 } },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

u8* gShelterB1PodAccessTunnelRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB1PodAccessTunnelRoomViewCounts[1] = { SHELTER_B1_POD_ACCESS_TUNNEL_VIEW_COUNT };

DirectionWarpEntry gShelterB1PodAccessTunnelWarpEntries[3] = {
    { { { .word = 0 }, 1698, 0, -6028 }, { 0, 0, 0, 0 }, { { .word = 0 }, 1698, 0, -6028 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 2), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 1), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 6834, 0, -1772 }, { 0, 0, 0, 0 }, { { .word = 3840 }, 6150, 0, -2450 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 5), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 4), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 3), 4, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_B2_LABORATORY },
    { { { .word = 2048 }, 4530, 0, -1050 }, { 0, 0, 0, 0 }, { { .word = 3328 }, 5350, 0, -1750 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 7), DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_POD },
};

/// Axis-aligned collision normals in Q12 (ONE is unit length).
static SVECTOR _gShelterB1PodAccessTunnelCollisionGridNormals[6] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_normals.inc"
};

/// Tunnel boundary vertices at Y=0 and Y=-3200, in game-coordinate units.
static SVECTOR _gShelterB1PodAccessTunnelCollisionGridVerts[12] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_verts.inc"
};

/// Ten tunnel quads: class-2 floor faces and class-0 walls and ceiling.
static WorldCollisionGridFace _gShelterB1PodAccessTunnelCollisionGridFaces[10] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_faces.inc"
};

/// Signed face-index pool for four lists ending at WORLD_COLLISION_GRID_CELL_END.
///
/// Lists at offsets 0, 10, 20 and 30 contain 10, 9, 9 and 6 halfwords including
/// their end markers. The stored face-zero entries at offsets 19 and 29 have
/// no cell pointer; retain them as authored data.
static s16 _gShelterB1PodAccessTunnelCollisionGridCells[36] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_cells.inc"
};

// The generated table supplies halfword offsets into the contiguous face-index lists.
/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelterB1PodAccessTunnelCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..35,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_B1_POD_ACCESS_TUNNEL_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB1PodAccessTunnelCollisionGridCells[(halfwordOffset)])
/// X-major directory for two X cells by two Z cells; all four cells are populated.
static s16* _gShelterB1PodAccessTunnelCollisionGridTable[4] = {
#include "assets/shelter_b1_pod_access_tunnel_collision_06664_table.inc"
};
#undef SHELTER_B1_POD_ACCESS_TUNNEL_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB1PodAccessTunnelCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB1PodAccessTunnelCollisionGridNormals,
    .vertices    = _gShelterB1PodAccessTunnelCollisionGridVerts,
    .faces       = _gShelterB1PodAccessTunnelCollisionGridFaces,
    .cellFaceIds = _gShelterB1PodAccessTunnelCollisionGridTable,
    .xBias       = -750,
    .zBias       = 6500,
    .cellCountX  = 2,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB1PodAccessTunnelCollisionGridFaces),
};

ViewCamera gShelterB1PodAccessTunnelViewCameras[SHELTER_B1_POD_ACCESS_TUNNEL_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -4000, 0x37D3, 2550 } }, 296 },
    { { { { -3999, 0, -882 }, { -236, 3946, 1070 }, { 850, 1096, -3853 } }, { -839, 2300, 805 } }, 230 },
    { { { { 3745, 0, -1658 }, { -128, 4083, -289 }, { 1653, 316, 3734 } }, { -769, 1450, 6475 } }, 230 },
    { { { { -824, 0, -4012 }, { 670, 4038, -137 }, { 3955, -684, -812 } }, { -759, 700, 995 } }, 225 },
    { { { { -1017, 0, -3967 }, { -2801, 2900, 718 }, { 2809, 2891, -720 } }, { -539, 2450, 5405 } }, 246 },
    { { { { -4005, 0, -854 }, { 48, 4089, -228 }, { 852, -233, -3999 } }, { -1167, 1170, 3136 } }, 447 },
    { { { { 4052, 0, -596 }, { 24, 4092, 167 }, { 595, -168, 4048 } }, { -1429, 960, 6385 } }, 230 },
    { { { { -4007, 0, -844 }, { 555, 3083, -2637 }, { 635, -2695, -3017 } }, { -1539, 160, 5455 } }, 230 },
    { { { { 4095, 0, -11 }, { 0, 4091, 189 }, { 11, -189, 4091 } }, { -1749, 950, 4365 } }, 230 },
    { { { { -1919, 0, -3618 }, { 554, 4047, -294 }, { 3575, -627, -1897 } }, { -3880, 991, 576 } }, 257 },
    { { { { 4089, 0, -229 }, { 137, 3281, 2447 }, { 183, -2450, 3276 } }, { -6550, 170, -1790 } }, 207 },
    { { { { -4019, 0, -790 }, { -621, 2528, 3162 }, { 487, 3222, -2480 } }, { -6710, 2630, -5070 } }, 329 },
    { { { { -3158, 0, -2608 }, { 29, 4095, -35 }, { 2607, -45, -3158 } }, { -1280, 1287, 2923 } }, 447 },
    { { { { 3985, 0, -946 }, { -429, 3649, -1809 }, { 843, 1859, 3550 } }, { -4527, 1724, 1418 } }, 251 },
};

/// View 1's empty sprite list: image-strip header followed by SPRITE_BATCH_END.
static SpriteBatch _gShelterB1PodAccessTunnelView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's empty sprite list: image-strip header followed by SPRITE_BATCH_END.
static SpriteBatch _gShelterB1PodAccessTunnelView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's 68 textured rectangles in view-centred pixels and authored sorting depths.
///
/// Batches address these elements without a source-array sentinel. RGB 128
/// is neutral modulation; packet caching snapshots geometry and texture state.
static SpriteSource _gShelterB1PodAccessTunnelView3SpriteSources[68] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 32, -16, 1004, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, -48, 991, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, -80, 980, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 112, 24, 825, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 112, 40, 825, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 112, -72, 825, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, -56, 825, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, -56, 825, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -56, 825, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 112, -32, 825, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 112, -16, 825, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 136, -16, 825, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 0, 825, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 0, 825, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 0, 825, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -8, 875, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -32, 875, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, -64, 875, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, -64, 875, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 64, 40, 830, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 40, 835, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 40, 898, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, 40, 1001, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, 8, 982, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 8, 883, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, 8, 830, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 88, 8, 837, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 104, 40, 850, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 8, 850, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 8, 875, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 8, 875, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 128, 40, 875, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 40, 875, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, 128, -88, 875, { .fields = { 24, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, 128, -64, 875, { .fields = { 24, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 128, -40, 875, { .fields = { 112, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, 128, -24, 875, { .fields = { 96, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 128, -16, 875, { .fields = { 104, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, 128, 0, 875, { .fields = { 8, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, 96, 0, 875, { .fields = { 16, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, 96, -24, 875, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, 96, -32, 875, { .fields = { 24, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 96, -48, 875, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 96, -64, 875, { .fields = { 112, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, 96, -88, 875, { .fields = { 8, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 64, -88, 850, { .fields = { 120, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, 64, -72, 850, { .fields = { 0, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 64, -48, 850, { .fields = { 120, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 88, -32, 850, { .fields = { 88, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 32 } }, 64, -32, 850, { .fields = { 80, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 80, -32, 850, { .fields = { 88, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 24 } }, 80, 0, 850, { .fields = { 8, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 24 } }, 64, 0, 837, { .fields = { 16, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 40, -88, 964, { .fields = { 72, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 48, -88, 915, { .fields = { 72, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 56, -88, 903, { .fields = { 88, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, 40, -56, 938, { .fields = { 96, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, 48, -56, 922, { .fields = { 96, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, 56, -56, 909, { .fields = { 104, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, 56, -16, 917, { .fields = { 104, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, 48, -16, 931, { .fields = { 112, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, 40, -16, 933, { .fields = { 120, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 32, 64 } }, 128, 40, 640, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 32, 56 } }, 96, 48, 728, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 24, 48 } }, 72, 56, 814, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 32 } }, 56, 56, 906, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 24 } }, 48, 56, 973, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 16 } }, 32, 56, 992, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
};

/// View 3's image-strip header, six source ranges covering all 68 sprites, and terminator.
///
/// Ranges start at 0, 3, 15, 19, 33 and 62; all are visible and included in
/// cached packets. The two serialized tail bytes retain their unproven roles.
static SpriteBatch _gShelterB1PodAccessTunnelView3SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 12, 0, 0, { 4, 0 } },
    { 15, 4, 0, 0, { 3, 0 } },
    { 19, 14, 0, 0, { 5, 0 } },
    { 33, 29, 0, 0, { 2, 0 } },
    { 62, ARRAY_SIZE(_gShelterB1PodAccessTunnelView3SpriteSources) - 62, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's 19 textured rectangles in view-centred pixels and authored sorting depths.
///
/// Batches address these elements without a source-array sentinel. RGB 128
/// is neutral modulation; packet caching snapshots geometry and texture state.
static SpriteSource _gShelterB1PodAccessTunnelView4SpriteSources[19] = {
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -8, -32, 1629, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, 24, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 8, 0, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -72, -72, 0, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 72, 685, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 40, 695, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 8, 709, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 8, 675, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, -24, 711, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, -24, 725, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -48, 730, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -72, 739, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, -24, 723, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, -72, 734, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -120, -72, 665, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, -24, 660, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, 8, 604, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, 40, 684, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, 72, 661, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
};

/// View 4's image-strip header, two source ranges covering all 19 sprites, and terminator.
///
/// Source 0 forms the first range; sources 1..18 form the second. Both are
/// visible and included in cached packets; serialized tail-byte roles are unproven.
static SpriteBatch _gShelterB1PodAccessTunnelView4SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, ARRAY_SIZE(_gShelterB1PodAccessTunnelView4SpriteSources) - 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's empty sprite list: image-strip header followed by SPRITE_BATCH_END.
static SpriteBatch _gShelterB1PodAccessTunnelView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's empty sprite list: image-strip header followed by SPRITE_BATCH_END.
static SpriteBatch _gShelterB1PodAccessTunnelView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's empty sprite list: image-strip header followed by SPRITE_BATCH_END.
static SpriteBatch _gShelterB1PodAccessTunnelView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8's empty sprite list: image-strip header followed by SPRITE_BATCH_END.
static SpriteBatch _gShelterB1PodAccessTunnelView8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 9's empty sprite list: image-strip header followed by SPRITE_BATCH_END.
static SpriteBatch _gShelterB1PodAccessTunnelView9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 10's empty sprite list: image-strip header followed by SPRITE_BATCH_END.
static SpriteBatch _gShelterB1PodAccessTunnelView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 11's empty sprite list: image-strip header followed by SPRITE_BATCH_END.
static SpriteBatch _gShelterB1PodAccessTunnelView11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 12's empty sprite list: image-strip header followed by SPRITE_BATCH_END.
static SpriteBatch _gShelterB1PodAccessTunnelView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 13's empty sprite list: image-strip header followed by SPRITE_BATCH_END.
static SpriteBatch _gShelterB1PodAccessTunnelView13SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 14's empty sprite list: image-strip header followed by SPRITE_BATCH_END.
static SpriteBatch _gShelterB1PodAccessTunnelView14SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB1PodAccessTunnelSpriteViews[SHELTER_B1_POD_ACCESS_TUNNEL_VIEW_COUNT] = {
    { { .empty = _gShelterB1PodAccessTunnelView1SpriteBatches }, _gShelterB1PodAccessTunnelView1SpriteBatches, NULL },
    { { .empty = _gShelterB1PodAccessTunnelView2SpriteBatches }, _gShelterB1PodAccessTunnelView2SpriteBatches, NULL },
    { { .elements = _gShelterB1PodAccessTunnelView3SpriteSources }, _gShelterB1PodAccessTunnelView3SpriteBatches, NULL },
    { { .elements = _gShelterB1PodAccessTunnelView4SpriteSources }, _gShelterB1PodAccessTunnelView4SpriteBatches, NULL },
    { { .empty = _gShelterB1PodAccessTunnelView5SpriteBatches }, _gShelterB1PodAccessTunnelView5SpriteBatches, NULL },
    { { .empty = _gShelterB1PodAccessTunnelView6SpriteBatches }, _gShelterB1PodAccessTunnelView6SpriteBatches, NULL },
    { { .empty = _gShelterB1PodAccessTunnelView7SpriteBatches }, _gShelterB1PodAccessTunnelView7SpriteBatches, NULL },
    { { .empty = _gShelterB1PodAccessTunnelView8SpriteBatches }, _gShelterB1PodAccessTunnelView8SpriteBatches, NULL },
    { { .empty = _gShelterB1PodAccessTunnelView9SpriteBatches }, _gShelterB1PodAccessTunnelView9SpriteBatches, NULL },
    { { .empty = _gShelterB1PodAccessTunnelView10SpriteBatches }, _gShelterB1PodAccessTunnelView10SpriteBatches, NULL },
    { { .empty = _gShelterB1PodAccessTunnelView11SpriteBatches }, _gShelterB1PodAccessTunnelView11SpriteBatches, NULL },
    { { .empty = _gShelterB1PodAccessTunnelView12SpriteBatches }, _gShelterB1PodAccessTunnelView12SpriteBatches, NULL },
    { { .empty = _gShelterB1PodAccessTunnelView13SpriteBatches }, _gShelterB1PodAccessTunnelView13SpriteBatches, NULL },
    { { .empty = _gShelterB1PodAccessTunnelView14SpriteBatches }, _gShelterB1PodAccessTunnelView14SpriteBatches, NULL },
};

/// White point light at world (1299, -2529, -3253), contributing in every view.
///
/// Full intensity uses ONE per RGB channel; falloff spans radii 9581..15681
/// game units. Room setup binds the parent and refreshes the composition cache;
/// queries overwrite attenuation, so the single-element array remains writable.
static WorldCoordPointLight _gShelterB1PodAccessTunnelPointLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1299, -2529, -3253 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 9581, 15681 },
};

WorldCoordRoomLights gShelterB1PodAccessTunnelRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB1PodAccessTunnelPointLights),
    .pointLights           = _gShelterB1PodAccessTunnelPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB1PodAccessTunnelViewBoundaryTriggers[4] = {
    { NULL, NULL, NULL, { 1296, -1536, -3328, 0 }, { { -1936, -2016, 672, 0 }, { 1936, -2016, -672, 0 }, { -1936, 2016, 672, 0 }, { 1936, 2016, -672, 0 } }, { -1344, 0, -3872, 0 }, { 0, 0, ONE, 0 }, 2873, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1168, -1536, -3408, 0 }, { { 2080, -2016, -752, 0 }, { -2080, -2016, 752, 0 }, { 2080, 2016, -752, 0 }, { -2080, 2016, 752, 0 } }, { 1395, 0, 3860, 0 }, { 0, 0, ONE, 0 }, 2985, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3136, -1536, -1312, 0 }, { { -119, -2016, -1591, 0 }, { 109, -2016, 1584, 0 }, { -119, 2016, -1591, 0 }, { 109, 2016, 1584, 0 } }, { 4095, 0, -295, 0 }, { 0, 0, ONE, 0 }, 2560, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3265, -1504, -1567, 0 }, { { 84, -2016, 1287, 0 }, { -98, -2016, -1299, 0 }, { 84, 2016, 1287, 0 }, { -98, 2016, -1299, 0 } }, { -4094, 0, 287, 0 }, { 0, 0, ONE, 0 }, 2387, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder gShelterB1PodAccessTunnelSightOccluders[1] = {
    { NULL, NULL, { 5024, -1296, -5152, 0 }, { { -2064, 2320, 2208, 0 }, { 2064, 2320, -2208, 0 }, { -2064, -2320, 2208, 0 }, { 2064, -2320, -2208, 0 } }, { 2999, 0, 2803, 0 }, 3805, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionTrigger gShelterB1PodAccessTunnelActionTriggers[3] = {
    { NULL, NULL, NULL, { 1856, -48, -6496, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, (1 << 4) | 2, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7264, -48, -1760, 0 }, { { -704, 0, -1024, 0 }, { 704, 0, -1024, 0 }, { -704, 0, 1024, 0 }, { 704, 0, 1024, 0 } }, { 0, 4094, 0, 0 }, { -ONE, 0, 0, 0 }, 1241, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_R47, (2 << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4721, -64, -672, 0 }, { { -880, 0, -480, 0 }, { 880, 0, -480, 0 }, { -880, 0, 480, 0 }, { 880, 0, 480, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -ONE, 0 }, 1001, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_POD_ACCESS_TUNNEL_POD_COMMAND_USE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Variant 1's scripted Eve file and spawn recipe, followed by AREA_PLACEMENT_END.
///
/// Entry 132 loads base file 141000 and selects the walker descriptor at index 0.
/// Its placement borrows this table and the loaded actor package.
static AreaResource _gShelterB1PodAccessTunnelVariant1EveResources[2] = {
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_EVE, .fileNumber = 410, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor141000EveWalkerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's Watcher file and spawn recipe, followed by AREA_PLACEMENT_END.
///
/// Entry 21 loads base file 102100 and selects its single descriptor at index 0.
/// The four matching placements borrow this table and the loaded actor package.
static AreaResource _gShelterB1PodAccessTunnelVariant2WatcherResources[2] = {
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .fileNumber = 21, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor102100WatcherTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 3's Mossback file and spawn recipe, followed by AREA_PLACEMENT_END.
///
/// Entry 11 loads base file 101100 and selects descriptor 0. The two matching
/// placements borrow this table and the loaded actor package.
static AreaResource _gShelterB1PodAccessTunnelVariant3MossbackResources[2] = {
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_MOSSBACK, .fileNumber = 11, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor101100Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 4's Brute Mossback file and spawn recipe, followed by AREA_PLACEMENT_END.
///
/// Entry 49 loads file 104900 and uses descriptor 0 of the Mossback interface
/// at the same actor slot. The entry key selects the larger model and tuning;
/// the two matching placements borrow this table and the loaded actor package.
static AreaResource _gShelterB1PodAccessTunnelVariant4BruteMossbackResources[2] = {
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_BRUTE_MOSSBACK, .fileNumber = 49, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor101100Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 7's Rook GOLEM file and spawn recipe, followed by AREA_PLACEMENT_END.
///
/// Entry 23 loads file 102300 and selects descriptor 0. Its single placement
/// borrows this table and the actor-slot-1 Rook package through task teardown.
static AreaResource _gShelterB1PodAccessTunnelVariant7RookResources[2] = {
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_ROOK, .fileNumber = 23, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor102300RookGolemTasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 11's Watcher and Rook GOLEM recipes, followed by AREA_PLACEMENT_END.
///
/// Entry 21 loads file 102100 in actor slot 1; entry 23 loads file 202300 in
/// slot 2. Both select descriptor 0. Three Watchers and one Rook borrow this
/// table and both loaded actor packages through their task lifetimes.
static AreaResource _gShelterB1PodAccessTunnelVariant11WatcherRookResources[3] = {
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .fileNumber = 21, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor102100WatcherTaskDesc },
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_ROOK, .fileNumber = 23, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = gActor202300RookGolemTasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 21's Watcher file and spawn recipe, followed by AREA_PLACEMENT_END.
///
/// Entry 21 loads file 102100 and selects its single descriptor at index 0.
/// Six matching placements borrow this table and the loaded actor package.
static AreaResource _gShelterB1PodAccessTunnelVariant21WatcherResources[2] = {
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .fileNumber = 21, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor102100WatcherTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 1's scripted Eve placement at the world origin.
///
/// Entry 132 uses actor 141000, with spawn variant/mode and yaw zero. Texture
/// relocation is page 0, CLUT row 2. Includes AREA_PLACEMENT_END; the actor
/// borrows this record from the loaded room for its lifetime.
static AreaPlacement _gShelterB1PodAccessTunnelVariant1EvePlacements[2] = {
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_EVE, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's four elevated Watchers, in two pairs along the tunnel.
///
/// First pair has stationary beam-0 weapons; the second patrols in 40-tick
/// legs with beam 0 and a quarter-turn roll. All use entry 21 and sight row 0
/// (3000-unit acquisition range), page offset 0 and CLUT-row offset 2.
/// World positions and yaw in 4096-unit turns can be overridden by saved poses. Includes
/// AREA_PLACEMENT_END; spawned actors borrow records while the room is loaded.
static AreaPlacement _gShelterB1PodAccessTunnelVariant2WatcherPlacements[5] = {
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .variant = SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_STATIONARY_BEAM0, .mode = 0, .x = 730, .y = -2400, .z = -1050, .yaw = ONE / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .variant = SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_STATIONARY_BEAM0, .mode = 0, .x = 730, .y = -2400, .z = -2400, .yaw = ONE / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .variant = SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_PATROL_BEAM0, .mode = ONE / 4, .x = 1000, .y = -2200, .z = -4750, .yaw = ONE / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .variant = SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_PATROL_BEAM0, .mode = ONE / 4, .x = 2750, .y = -2200, .z = -4750, .yaw = 3 * ONE / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 3's two floor-level Mossbacks near the tunnel bend and R47 door.
///
/// Both use entry 11, variant/mode (1, 0), actor row 0 and texture offsets
/// (page 0, CLUT row 2). Positions are world units, yaw is 4096 units per turn;
/// saved poses can override them. Includes AREA_PLACEMENT_END; records remain
/// borrowed by spawned actors while the room overlay is loaded.
static AreaPlacement _gShelterB1PodAccessTunnelVariant3MossbackPlacements[3] = {
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_MOSSBACK, .variant = 1, .mode = 0, .x = 1750, .y = 0, .z = -1900, .yaw = ONE / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_MOSSBACK, .variant = 1, .mode = 0, .x = 6950, .y = 0, .z = -2450, .yaw = 3800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 4's two floor-level Brute Mossbacks near the tunnel bend.
///
/// Both use larger-model entry 49, variant/mode (1, 0), actor row 0 and texture
/// offsets (page 0, CLUT row 2). Positions are world units, yaw is 4096 units
/// per turn; saved poses can override them. Includes AREA_PLACEMENT_END;
/// records remain borrowed by spawned actors while the room overlay is loaded.
static AreaPlacement _gShelterB1PodAccessTunnelVariant4BruteMossbackPlacements[3] = {
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_BRUTE_MOSSBACK, .variant = 1, .mode = 0, .x = 3500, .y = 0, .z = -2300, .yaw = 3750, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_BRUTE_MOSSBACK, .variant = 1, .mode = 0, .x = 1750, .y = 0, .z = -2500, .yaw = ONE / 2, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 7's single Rook GOLEM at world (1600, 0, -1800), facing a quarter-turn.
///
/// Entry 23 enables a 5000-game-unit patrol, selects actor row 0 and uses
/// texture offsets (page 0, CLUT row 2). Saved poses may replace its initial transform.
/// Includes AREA_PLACEMENT_END; the actor borrows the record while the room is loaded.
static AreaPlacement _gShelterB1PodAccessTunnelVariant7RookPlacements[2] = {
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_ROOK, .variant = 5, .mode = SHELTER_B1_POD_ACCESS_TUNNEL_ROOK_PATROLS, .x = 1600, .y = 0, .z = -1800, .yaw = ONE / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 11's three elevated Watchers and one floor-level Rook GOLEM.
///
/// Watchers use stationary beam-1 weapons, zero roll and sight row 3 (4500
/// game units), with texture offsets (0, 2). The slot-2 Rook patrols for 4000
/// game units per leg, with row 0 and texture offsets (2, 4).
/// Positions are world units and yaw is 4096 units per turn; saved poses can
/// override them. Includes AREA_PLACEMENT_END; actors borrow these loaded-room records.
static AreaPlacement _gShelterB1PodAccessTunnelVariant11WatcherRookPlacements[5] = {
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .variant = SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_STATIONARY_BEAM1, .mode = 0, .x = 1200, .y = -2400, .z = -700, .yaw = ONE / 2, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .variant = SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_STATIONARY_BEAM1, .mode = 0, .x = 2000, .y = -2400, .z = -700, .yaw = ONE / 2, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .variant = SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_STATIONARY_BEAM1, .mode = 0, .x = 2800, .y = -2400, .z = -700, .yaw = ONE / 2, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_ROOK, .variant = 4, .mode = SHELTER_B1_POD_ACCESS_TUNNEL_ROOK_PATROLS, .x = 5700, .y = 0, .z = -1700, .yaw = 3 * ONE / 4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 21's six Watchers in two three-wide rows at Y=-2400 and Y=-1800.
///
/// All use stationary gun weapons, zero roll, sight row 3 (4500 game units),
/// a half-turn yaw and texture offsets (page 0, CLUT row 2). Positions are world units; saved poses
/// can override the initial transforms. Includes AREA_PLACEMENT_END; actors
/// borrow these records while this room overlay is loaded.
static AreaPlacement _gShelterB1PodAccessTunnelVariant21WatcherPlacements[7] = {
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .variant = SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_STATIONARY_GUN, .mode = 0, .x = 1200, .y = -2400, .z = -700, .yaw = ONE / 2, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .variant = SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_STATIONARY_GUN, .mode = 0, .x = 2000, .y = -2400, .z = -700, .yaw = ONE / 2, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .variant = SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_STATIONARY_GUN, .mode = 0, .x = 2800, .y = -2400, .z = -700, .yaw = ONE / 2, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .variant = SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_STATIONARY_GUN, .mode = 0, .x = 1200, .y = -1800, .z = -700, .yaw = ONE / 2, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .variant = SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_STATIONARY_GUN, .mode = 0, .x = 2000, .y = -1800, .z = -700, .yaw = ONE / 2, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = SHELTER_B1_POD_ACCESS_TUNNEL_ENTRY_WATCHER, .variant = SHELTER_B1_POD_ACCESS_TUNNEL_WATCHER_STATIONARY_GUN, .mode = 0, .x = 2800, .y = -1800, .z = -700, .yaw = ONE / 2, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gShelterB1PodAccessTunnelAreaVariants[23] = {
    [1]  = { .placements = _gShelterB1PodAccessTunnelVariant1EvePlacements, .resources = _gShelterB1PodAccessTunnelVariant1EveResources },
    [2]  = { .placements = _gShelterB1PodAccessTunnelVariant2WatcherPlacements, .resources = _gShelterB1PodAccessTunnelVariant2WatcherResources },
    [3]  = { .placements = _gShelterB1PodAccessTunnelVariant3MossbackPlacements, .resources = _gShelterB1PodAccessTunnelVariant3MossbackResources },
    [4]  = { .placements = _gShelterB1PodAccessTunnelVariant4BruteMossbackPlacements, .resources = _gShelterB1PodAccessTunnelVariant4BruteMossbackResources },
    [7]  = { .placements = _gShelterB1PodAccessTunnelVariant7RookPlacements, .resources = _gShelterB1PodAccessTunnelVariant7RookResources },
    [11] = { .placements = _gShelterB1PodAccessTunnelVariant11WatcherRookPlacements, .resources = _gShelterB1PodAccessTunnelVariant11WatcherRookResources },
    [21] = { .placements = _gShelterB1PodAccessTunnelVariant21WatcherPlacements, .resources = _gShelterB1PodAccessTunnelVariant21WatcherResources },
};

enum {
    SHELTER_B1_POD_ACCESS_TUNNEL_FOOTSTEP_WALK_SCRIPT = 0x59,
    SHELTER_B1_POD_ACCESS_TUNNEL_FOOTSTEP_RUN_SCRIPT  = 0x5B,
};

/// Floor cue bases for walking, running and scripted jumps in the loaded type-1 bank.
///
/// Cue 2 selects scripts 89/91/89; cue 1 adds one and companion cues add 100.
/// The floor policy borrows this record while the room overlay is loaded.
static WorldCollisionFootstepSounds _gShelterB1PodAccessTunnelFloorFootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B1_POD_ACCESS_TUNNEL_FOOTSTEP_WALK_SCRIPT,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B1_POD_ACCESS_TUNNEL_FOOTSTEP_RUN_SCRIPT,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B1_POD_ACCESS_TUNNEL_FOOTSTEP_WALK_SCRIPT,
};

/// Classes 0 and 3..7: block probes, allow weapon impacts and pushback, with no footstep cues.
static WorldCollisionSurfaceProperties _gShelterB1PodAccessTunnelDefaultSurface = { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL };

/// Class 1: pass probes and suppress weapon impacts, retaining pushback and no footstep cues.
static WorldCollisionSurfaceProperties _gShelterB1PodAccessTunnelProbePassThroughSurface = { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL };

/// Class 2: the Y=0 floor, blocking probes with weapon impacts, pushback and footstep cues.
static WorldCollisionSurfaceProperties _gShelterB1PodAccessTunnelFloorSurface = { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &_gShelterB1PodAccessTunnelFloorFootstepSounds };

WorldCollisionSurfaceProperties* gShelterB1PodAccessTunnelSurfacePropertiesByClass[8] = {
    &_gShelterB1PodAccessTunnelDefaultSurface,
    &_gShelterB1PodAccessTunnelProbePassThroughSurface,
    &_gShelterB1PodAccessTunnelFloorSurface,
    &_gShelterB1PodAccessTunnelDefaultSurface,
    &_gShelterB1PodAccessTunnelDefaultSurface,
    &_gShelterB1PodAccessTunnelDefaultSurface,
    &_gShelterB1PodAccessTunnelDefaultSurface,
    &_gShelterB1PodAccessTunnelDefaultSurface,
};

RoomFadeStorage gRoomEventFade = { 0 };

/// Destination snapshot retained from event acceptance through staged scene reload.
///
/// The start handler copies all eight bytes; playback commits area, warp and
/// room to the live save. Keep the snapshot unchanged until reload tears down
/// the room. A later accepted start replaces it; no separate queue is stored.
static RoomEventMsg _gRoomEventStagedMsg = { 0 };

/// Result marker for the most recent one-time departure-event test (0 no attempt, 1 attempted).
///
/// Queries and ineligible calls clear it. An eligible execution sets it after
/// requesting a child task even if allocation failed; no reader is established.
static u8 _gShelterB1PodAccessTunnelEventStartAttempted = 0;

/// Retained, unreferenced bytes after the event-start byte; their role and grouping are unproven.
static u8 D_shelter_b1_pod_access_tunnel_80184D0D = 63;

static u8 D_shelter_b1_pod_access_tunnel_80184D0E = 253;

static u8 D_shelter_b1_pod_access_tunnel_80184D0F = 225;

RoomLatchedEvent gRoomEventLatched;

#include "../../shared/room/room_event_staged_task.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Tests a one-time departure event and latches its inputs when execution is requested.
///
/// Borrows complete eight-byte destination and twelve-byte event records for this
/// call; execution copies both into the room's single staged-event slot. A zero
/// flag ID is always eligible; otherwise its nibble must be clear and is set to 1.
/// Returns the room's HANDLED result when eligible, DEFAULT_DEPARTURE otherwise. Every call clears
/// the event-start byte, including queries; execution sets it even if spawning
/// fails. The staged task must finish before another event replaces the slot.
static inline s32 _shelterB1PodAccessTunnelStartEvent(const RoomEventMsg* destination, const RoomLatchedEvent* event)
{
    enum { EVENT_FLAG_CLEAR   = 0,
           EVENT_FLAG_LATCHED = 1,
           EVENT_NO_FLAG      = 0,
           EVENT_NOT_STARTED  = 0,
           EVENT_STARTED      = 1 };

    _gShelterB1PodAccessTunnelEventStartAttempted = EVENT_NOT_STARTED;
    if (gameFlagGetNibble(event->flagId) == EVENT_FLAG_CLEAR || event->flagId == EVENT_NO_FLAG) {
        if (destination->queryOnly == ROOM_EVENT_EXECUTE) {
            ROOM_EVENT_STAGED_DESTINATION = *destination;
            gRoomEventLatched             = *event;
            if (event->flagId != EVENT_NO_FLAG) {
                gameFlagSetNibble(event->flagId, EVENT_FLAG_LATCHED);
            }
            taskSpawnFromTable(&_gShelterB1PodAccessTunnelStagedDepartureTaskDesc, 0, 0, 0);
            _gShelterB1PodAccessTunnelEventStartAttempted = EVENT_STARTED;
        }
        return SHELTER_B1_POD_ACCESS_TUNNEL_EVENT_HANDLED;
    }
    return SHELTER_B1_POD_ACCESS_TUNNEL_EVENT_DEFAULT_DEPARTURE;
}

/// Resolves tunnel departures and substitutes the progress-dependent dialogue or travel.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with a borrowed request and writable reply;
/// they may alias. Copies the entire eight-byte record before resolving its room.
/// Queries suppress departure effects except the staged-event helper's start-byte
/// clear. Replies are BLOCKED, DEFAULT_DEPARTURE or HANDLED; task/messageId are
/// unused. Execution can retain the resolved destination in the staged-event slot.
/// The request remains mutable in the signature for the room-variant resolver's
/// interface; this handler only reads it, apart from writes through an aliased reply.
static s32 _shelterB1PodAccessTunnelResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { CAP_LOCKED_R47_DOOR           = 3,
           CAP_COMPANION_ROUTE_BLOCKED   = 4,
           CAP_STERILIZATION_DEPARTURE   = 6,
           CAP_GANTRY_RETURN_BLOCKED     = 8,
           STORY_CHAPTER_GANTRY_TRANSIT  = 6,
           COMPANION_ROUTE_STARTED       = 1,
           COMPANION_ROUTE_BLOCKED       = 2,
           COMPANION_R47_SCHEDULE        = 7,
           MAP_FLAG_BLOCKED_DOOR         = 2,
           EVENT_NO_FADE                 = 0,
           STERILIZATION_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 1) };

    RoomLatchedEvent event;

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_R47) {
        if (gameFlagGetNibble(GAME_FLAG_118) == SHELTER_B1_POD_ACCESS_TUNNEL_GANTRY_RETURN_COMPLETE) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(CAP_GANTRY_RETURN_BLOCKED);
            }
            return SHELTER_B1_POD_ACCESS_TUNNEL_EVENT_HANDLED;
        }
        if (gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER) >= STORY_CHAPTER_GANTRY_TRANSIT) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                taskSpawnFromTable(_gShelterB1PodAccessTunnelTravelTaskDescs, SHELTER_B1_POD_ACCESS_TUNNEL_TRAVEL_TO_GANTRY, 0, 0);
            }
            return SHELTER_B1_POD_ACCESS_TUNNEL_EVENT_HANDLED;
        }
        if (gameFlagGetNibble(GAME_FLAG_B1_POD_TUNNEL_R47_DOOR_UNLOCKED) == 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibbleIfPresent(request->flagId, MAP_FLAG_BLOCKED_DOOR);
                capRunCommandWithTransition(CAP_LOCKED_R47_DOOR);
            }
            return SHELTER_B1_POD_ACCESS_TUNNEL_EVENT_BLOCKED;
        }
        // Start the companion's R47 route once when that story branch is available.
        if (request->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_0D1) == 0 && gameFlagGetNibble(GAME_FLAG_083) == 0) {
            companionRestoreFullHp();
            gameFlagSetNibble(GAME_FLAG_0D1, COMPANION_ROUTE_STARTED);
            gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, COMPANION_R47_SCHEDULE);
        }
    }
    if (request->areaId == GAME_AREA_SHELTER_B1_STERILIZATION_ROOM) {
        if (gameFlagGetNibble(GAME_FLAG_118) == SHELTER_B1_POD_ACCESS_TUNNEL_GANTRY_RETURN_COMPLETE) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(CAP_GANTRY_RETURN_BLOCKED);
            }
            return SHELTER_B1_POD_ACCESS_TUNNEL_EVENT_HANDLED;
        }
        if (gameFlagGetNibble(GAME_FLAG_0D1) == COMPANION_ROUTE_BLOCKED) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(CAP_COMPANION_ROUTE_BLOCKED);
            }
            return SHELTER_B1_POD_ACCESS_TUNNEL_EVENT_HANDLED;
        }
        // The first sterilization-room departure owns the CAP/sound/warp sequence.
        event.capCmd   = CAP_STERILIZATION_DEPARTURE;
        event.stageSnd = STERILIZATION_DEPARTURE_SOUND;
        event.flagId   = GAME_FLAG_B1_POD_TUNNEL_TO_STERILIZE_SCENE;
        event.fade     = EVENT_NO_FADE;
        return _shelterB1PodAccessTunnelStartEvent(reply, &event);
    }
    return SHELTER_B1_POD_ACCESS_TUNNEL_EVENT_DEFAULT_DEPARTURE;
}

/// Room-controller states: 0 initialize, 1 idle, 2 teardown.
///
/// Copied by value and indexed by `Task::state` in 0..2, without a terminator
/// or bounds check. Initialization installs the room's borrowed message table.
static const TaskFuncTable3 _gShelterB1PodAccessTunnelRoomTaskStates = {
    .funcs = { _shelterB1PodAccessTunnelInitializeRoomTask, _shelterB1PodAccessTunnelIdleTaskState, taskKill },
};

/// Commits B2 pod arrival 3 and schedules a reload from the live save.
///
/// Call after accepting the ride and its sound ending, with actor/player control
/// still held. Preserves the saved stage and selects room 1, or room 2 when the
/// gantry-return state is complete. Stops nonambient sound scripts and selects
/// next-session sprite resource variant 1 before spawning the reload's frame
/// capture phase. Requires live save/session and reload resources; kills this
/// bodyless ride task last, so its pointer must not be used afterwards.
static inline void _shelterB1PodAccessTunnelCommitB2Ride(Task* task)
{
    enum { RIDE_ARRIVAL_WARP                = 3,
           RIDE_DEFAULT_ROOM                = 1,
           RIDE_NEXT_SESSION_SPRITE_VARIANT = 1 };
    s32 gantryReturnProgress;

    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = RIDE_ARRIVAL_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = RIDE_DEFAULT_ROOM;
    gantryReturnProgress                                        = gameFlagGetNibble(GAME_FLAG_118);
    if (gantryReturnProgress == SHELTER_B1_POD_ACCESS_TUNNEL_GANTRY_RETURN_COMPLETE) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = gantryReturnProgress;
    }
    gDisplayState.spriteVariant = RIDE_NEXT_SESSION_SPRITE_VARIANT;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    taskKill(task);
}

/// Prompts for the pod ride, waits for its sound, then reloads into B2 arrival 3.
///
/// Start as a bodyless task in state 0 with player scripted control held.
/// States 0..4 are prompt, wait CAP, interpret choice, wait sound and departure.
/// CAP key 10 accepts; key 1 declines and marks the pod, other keys only decline.
/// Declining restores actor/player control. Accepting selects B2 room 1, or room
/// 2 after the gantry return. The room overlay and CAP/sound resources must stay
/// loaded until the task ends; spawn arguments are unused.
static void _shelterB1PodAccessTunnelRideToB2Task(Task* task)
{
    enum { RIDE_PROMPT,
           RIDE_WAIT_CAP,
           RIDE_INTERPRET_CHOICE,
           RIDE_WAIT_SOUND,
           RIDE_DEPART,
           RIDE_CAP_PROMPT           = 1,
           RIDE_CAP_ALTERNATE_PROMPT = 5,
           RIDE_CHOICE_ACCEPT        = 10,
           RIDE_CHOICE_MARK_POD      = 1,
           RIDE_MAP_MARK_VISIBLE     = 2,
           RIDE_MAP_MARK_HIDDEN      = 0 };

    switch (task->state) {
        case RIDE_PROMPT:
            capRunCommandWithTransition(gameFlagGetNibble(GAME_FLAG_0FC) != 0 ? RIDE_CAP_ALTERNATE_PROMPT : RIDE_CAP_PROMPT);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state++;
            break;
        case RIDE_WAIT_CAP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case RIDE_INTERPRET_CHOICE:
            if (capGetVariantKey() != RIDE_CHOICE_ACCEPT) {
                if (capGetVariantKey() == RIDE_CHOICE_MARK_POD) {
                    gameFlagSetNibble(GAME_FLAG_MAP_MARK_POD, RIDE_MAP_MARK_VISIBLE);
                }
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                return;
            }
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_POD, RIDE_MAP_MARK_HIDDEN);
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_POD_TUNNEL_RIDE_TO_B2, 0, 0);
            task->state++;
            break;
        case RIDE_WAIT_SOUND:
            if (sndScriptHasActiveId(SOUND_SHELTER_B1_POD_TUNNEL_RIDE_TO_B2) == 0) {
                task->state++;
            }
            break;
        case RIDE_DEPART:
            // Keep actor control held while the reload replaces this room.
            _shelterB1PodAccessTunnelCommitB2Ride(task);
            break;
    }
}

/// Commits the gantry access flags, arrival and scene selection before reloading.
///
/// Requires a live departure task and held player/actor control. Unlocks the
/// B2 R48 door, hides the gantry map mark and sets live-save scene event 28,
/// gantry area, arrival 1 and room 1. Sprite variant 1 accompanies the captured
/// reload request. Kills the caller's task without waiting for reload; committed
/// flags and save fields are retained even if the spawn fails.
static inline void _shelterB1PodAccessTunnelCommitGantryTransit(Task* task)
{
    enum { GANTRY_DOOR_UNLOCKED          = 1,
           GANTRY_MAP_MARK_HIDDEN        = 0,
           GANTRY_SCENE_EVENT            = 28,
           GANTRY_ARRIVAL_WARP           = 1,
           GANTRY_ARRIVAL_ROOM           = 1,
           RELOAD_CAPTURE_SPRITE_VARIANT = 1 };

    gameFlagSetNibble(GAME_FLAG_B2_POD_TUNNEL_R48_DOOR_UNLOCKED, GANTRY_DOOR_UNLOCKED);
    gameFlagSetNibble(GAME_FLAG_MAP_MARK_POD_SERVICE_GANTRY, GANTRY_MAP_MARK_HIDDEN);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent         = GANTRY_SCENE_EVENT;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = GANTRY_ARRIVAL_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = GANTRY_ARRIVAL_ROOM;
    gDisplayState.spriteVariant                                 = RELOAD_CAPTURE_SPRITE_VARIANT;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    taskKill(task);
}

/// Prompts for the gantry transit and reloads there after the accepted travel sound.
///
/// Start as a bodyless task in state 0 with player scripted control held.
/// States 0..4 are prompt, wait CAP, interpret choice, wait sound and departure.
/// CAP key 10 accepts; every other key restores actor/player control and ends.
/// Acceptance unlocks the B2 R48 door, hides the gantry marker and selects scene
/// event 28 and gantry room/arrival 1. Keep this overlay and CAP/sound resources
/// loaded until completion; spawn arguments are unused.
static void _shelterB1PodAccessTunnelTravelToGantryTask(Task* task)
{
    enum { GANTRY_PROMPT,
           GANTRY_WAIT_CAP,
           GANTRY_INTERPRET_CHOICE,
           GANTRY_WAIT_SOUND,
           GANTRY_DEPART,
           GANTRY_CAP_PROMPT    = 9,
           GANTRY_CHOICE_ACCEPT = 10 };

    switch (task->state) {
        case GANTRY_PROMPT:
            capRunCommandWithTransition(GANTRY_CAP_PROMPT);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state++;
            break;
        case GANTRY_WAIT_CAP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case GANTRY_INTERPRET_CHOICE:
            if (capGetVariantKey() != GANTRY_CHOICE_ACCEPT) {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                return;
            }
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_POD_TUNNEL_GANTRY_TRANSIT, 0, 0);
            task->state++;
            break;
        case GANTRY_WAIT_SOUND:
            if (sndScriptHasActiveId(SOUND_SHELTER_B1_POD_TUNNEL_GANTRY_TRANSIT) == 0) {
                task->state++;
            }
            break;
        case GANTRY_DEPART:
            // The reload owns restoring control after the destination is installed.
            _shelterB1PodAccessTunnelCommitGantryTransit(task);
            break;
    }
}

/// Refuses every key-item-use request in this room, returning zero to the inventory.
///
/// `itemId` is the inventory's selected item ID. No payload or task state is changed.
static s32 _shelterB1PodAccessTunnelRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return 0;
}

/// Handles pod command 1, showing its first-use CAP or starting the B2 ride prompt.
///
/// Receives `ROOM_MESSAGE_COMMAND`; task, message ID and second argument are unused.
/// The first use records its flag before running CAP command 10. Later uses hold
/// player control for the ride task. Other commands do nothing; always returns 0.
static s32 _shelterB1PodAccessTunnelHandleCommandMessage(Task* task, s32 messageId, s32 command, s32 secondArg)
{
    enum { COMMAND_USE_POD        = 1,
           POD_FIRST_USE_RECORDED = 1,
           CAP_POD_FIRST_USE      = 10 };

    if (command == COMMAND_USE_POD) {
        if (gameFlagGetNibble(GAME_FLAG_B1_POD_ACCESS_TUNNEL_FIRST_USE) != 0) {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(_gShelterB1PodAccessTunnelTravelTaskDescs, SHELTER_B1_POD_ACCESS_TUNNEL_TRAVEL_TO_B2, 0, 0);
        } else {
            gameFlagSetNibble(GAME_FLAG_B1_POD_ACCESS_TUNNEL_FIRST_USE, POD_FIRST_USE_RECORDED);
            capRunCommandWithTransition(CAP_POD_FIRST_USE);
        }
    }
    return 0;
}

/// Ignores direction-triggered room actions and returns zero.
///
/// The borrowed `actionRequest` and second payload are never read or retained.
static s32 _shelterB1PodAccessTunnelIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* actionRequest, s32 secondArg)
{
    return 0;
}

/// Plays the confirmation cue for room sound command 6; other commands do nothing.
///
/// Returns zero for every command. The task, message ID and second payload are unused.
static s32 _shelterB1PodAccessTunnelHandleSoundMessage(Task* task, s32 messageId, s32 soundCommand, s32 secondArg)
{
    enum { SHELTER_B1_POD_ACCESS_TUNNEL_SOUND_COMMAND_CONFIRM = 6 };

    if (soundCommand == SHELTER_B1_POD_ACCESS_TUNNEL_SOUND_COMMAND_CONFIRM) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
    }
    return 0;
}

/// Registers the room controller and starts the entry sequence selected by progress.
///
/// Requires a bodyless room task in state 0 and live room/event resources.
/// A pending gantry return takes precedence: starts its dialogue, advances the
/// progress flag to 2 and selects objective 55. Otherwise the first entry starts
/// the skippable pod scene, selects objective 30 and records it as seen immediately.
/// Advances to idle even if a child spawn fails; message table and callbacks must
/// remain loaded while the registered room task is live.
static void _shelterB1PodAccessTunnelInitializeRoomTask(Task* task)
{
    enum { ROOM_IDLE                 = 1,
           ENTRY_SCENE_RECORDED      = 1,
           OBJECTIVE_FIRST_POD_ENTRY = 30,
           OBJECTIVE_AFTER_GANTRY    = 55 };

    task->msgTable = _gShelterB1PodAccessTunnelRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_118) == SHELTER_B1_POD_ACCESS_TUNNEL_GANTRY_RETURN_PENDING) {
        // Consume the gantry return before considering the one-time entry scene.
        taskSpawnFromTable(&_gShelterB1PodAccessTunnelPostGantryDialogueTaskDesc, 0, 0, 0);
        gameFlagSetNibble(GAME_FLAG_118, SHELTER_B1_POD_ACCESS_TUNNEL_GANTRY_RETURN_COMPLETE);
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, OBJECTIVE_AFTER_GANTRY);
    } else if (gameFlagGetNibble(GAME_FLAG_POD_ACCESS_TUNNEL_SCENE_SEEN) == 0) {
        evsStartScriptWithSkip(_gShelterB1PodAccessTunnelEntrySceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gShelterB1PodAccessTunnelEntrySceneSkipScript);
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, OBJECTIVE_FIRST_POD_ENTRY);
        gameFlagSetNibble(GAME_FLAG_POD_ACCESS_TUNNEL_SCENE_SEEN, ENTRY_SCENE_RECORDED);
    }
    task->state = task->state + ROOM_IDLE;
}

/// Keeps the initialized room task alive without advancing its state or changing it.
static void _shelterB1PodAccessTunnelIdleTaskState(Task* task)
{
    // Retain the target's otherwise unused 16-byte stack frame.
    char unusedStackFrame[0x10];
}

void shelterB1PodAccessTunnelRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gShelterB1PodAccessTunnelRoomTaskStates;
    states.funcs[task->state](task);
}

/// Runs the arrival dialogue after returning from the pod service gantry.
///
/// Start in state 0 with no body or work. Waits for the attachment wheel and
/// any pending display mode to close, requests weapon-bank pose 1 and starts
/// the dialogue script with HUD restoration. State 1 waits for the script to
/// end, selects Shelter music column 1 through scene event 29 and requests
/// task teardown. Requires the live player and its loaded weapon animation
/// bank; the pose request is consumed synchronously. The room overlay must
/// remain loaded until completion.
static void _shelterB1PodAccessTunnelPostGantryDialogueTask(Task* task)
{
    enum { DIALOGUE_START,
           DIALOGUE_WAIT_SCRIPT,
           DIALOGUE_MUSIC_EVENT = 29 };

    AnimationPlayRequest poseRequest;
    s32                  state;
    s32                  weaponId;
    s32                  weaponSetIndex;

    state = task->state;
    switch (state) {
        case DIALOGUE_START:
            if (gAttachmentState.mode != ATTACHMENT_MODE_WHEEL && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                // The arrival script holds a weapon pose until normal control resumes.
                weaponId                         = gPlayerStatus.weapon;
                weaponSetIndex                   = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponId);
                poseRequest.source.index         = weaponSetIndex;
                poseRequest.animationId          = PLAYER_ANIMATION_IDLE;
                poseRequest.blend                = ANIMATION_BLEND_RESET;
                poseRequest.blendFrames          = 0;
                poseRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &poseRequest, 0);
                evsStartScript(_gShelterB1PodAccessTunnelPostGantryDialogueScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                task->state = task->state + 1;
            }
            break;
        case DIALOGUE_WAIT_SCRIPT:
            if (gGameSession->eventState == 0) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = DIALOGUE_MUSIC_EVENT;
                taskRequestKill(task, 0);
            }
            break;
    }
}

/// Draws the delayed upward pod-chamber panorama with a descending image seam.
///
/// Requires state 1 and initialized owned work. Draws the upper image at
/// VRAM (832, 0), palette (0, 255), over the lower image at (576, 256), palette
/// (0, 256). Each uses 256+64 pixels of two 8-bit texture pages for a 320x240
/// screen. Consumes four SPRT and four DR_TPAGE packets per draw, all linked
/// at slot 1023; requires a 1024-tag ordering table and enough packet storage.
/// Stops on a ready view or an idle event. The signed halfword timer and
/// 16.16 offset keep advancing after the seam reaches 240; callers must end
/// the event before the timer wraps or the offset overflows. The script's
/// 240-frame rate is valid.
static void _shelterB1PodAccessTunnelDrawImageScroll(Task* task)
{
    enum { IMAGE_SCROLL_SCREEN_LEFT        = -160,
           IMAGE_SCROLL_SCREEN_TOP         = -120,
           IMAGE_SCROLL_RIGHT_STRIP_X      = 96,
           IMAGE_SCROLL_PAGE_PIXELS        = 256,
           IMAGE_SCROLL_RIGHT_STRIP_PIXELS = 64,
           IMAGE_SCROLL_UPPER_V_ORIGIN     = 239 - 256,
           IMAGE_SCROLL_UPPER_CLUT         = getClut(0, 255),
           IMAGE_SCROLL_LOWER_CLUT         = getClut(0, 256),
           IMAGE_SCROLL_UPPER_LEFT_VRAM_X  = 832,
           IMAGE_SCROLL_UPPER_RIGHT_VRAM_X = 960,
           IMAGE_SCROLL_UPPER_VRAM_Y       = 0,
           IMAGE_SCROLL_LOWER_LEFT_VRAM_X  = 576,
           IMAGE_SCROLL_LOWER_RIGHT_VRAM_X = 704,
           IMAGE_SCROLL_LOWER_VRAM_Y       = 256,
           IMAGE_SCROLL_UNMODULATED_LEVEL  = 0x80,
           IMAGE_SCROLL_ORDERING_SLOT      = 1023 };

    _ShelterB1PodAccessTunnelImageScrollWork* work;
    SPRT*                                     sprite;
    s32                                       seamY;

    /// Links one opaque strip and prepends its page at foreground slot 1023.
    ///
    /// Captures `sprite` and the image-scroll constants in this function.
    /// Arguments are evaluated once and must have no side effects; the final
    /// call's page-argument order is unspecified. Coordinates/dimensions are
    /// centred pixels; coordinates and dimensions narrow to signed halfwords,
    /// and texture V wraps to a byte. Requires space for an SPRT and
    /// the page helper's DR_TPAGE; packet links survive until GPU consumption.
    /// `palette` is an encoded CLUT ID, page X is VRAM words and page Y is rows.
    /// Expands to a braced block; invoke inside a braced block only.
#define SHELTER_B1_POD_ACCESS_TUNNEL_DRAW_IMAGE_SCROLL_STRIP(screenX, screenY, width, height, textureV, palette, pageX, pageY) \
    {                                                                                                                          \
        sprite         = gGpuPrimCursor;                                                                                       \
        gGpuPrimCursor = sprite + 1;                                                                                           \
        setSprt(sprite);                                                                                                       \
        sprite->x0 = (screenX);                                                                                                \
        sprite->y0 = (screenY);                                                                                                \
        sprite->w  = (width);                                                                                                  \
        setRGB0(sprite, IMAGE_SCROLL_UNMODULATED_LEVEL, IMAGE_SCROLL_UNMODULATED_LEVEL, IMAGE_SCROLL_UNMODULATED_LEVEL);       \
        sprite->u0   = 0;                                                                                                      \
        sprite->v0   = (textureV);                                                                                             \
        sprite->clut = (palette);                                                                                              \
        sprite->h    = (height);                                                                                               \
        addPrim(gGpuCurrentOt + IMAGE_SCROLL_ORDERING_SLOT, sprite);                                                           \
        _shelterB1PodAccessTunnelQueueImageScrollTexturePage((pageX), (pageY));                                                \
    }

    work = task->work;
    if (gGameSession->viewReady != 0 || gGameSession->eventState == 0) {
        taskKill(task);
        return;
    }
    seamY = 0;
    if (work->timer++ >= SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_DELAY_FRAMES) {
        work->offset += work->speed;
        seamY         = work->offset >> SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_FRACTION_BITS;
        if (seamY > SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_LINES) {
            seamY = SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_LINES;
        }
    }

    // The upper image starts at its last visible row; V wraps modulo 256.
    // Retain the terminal seam's 241-line upper sprite and wrapped V=255.
    SHELTER_B1_POD_ACCESS_TUNNEL_DRAW_IMAGE_SCROLL_STRIP(IMAGE_SCROLL_SCREEN_LEFT, IMAGE_SCROLL_SCREEN_TOP,
                                                         IMAGE_SCROLL_PAGE_PIXELS, seamY + 1, IMAGE_SCROLL_UPPER_V_ORIGIN - seamY, IMAGE_SCROLL_UPPER_CLUT,
                                                         IMAGE_SCROLL_UPPER_LEFT_VRAM_X, IMAGE_SCROLL_UPPER_VRAM_Y);
    SHELTER_B1_POD_ACCESS_TUNNEL_DRAW_IMAGE_SCROLL_STRIP(IMAGE_SCROLL_RIGHT_STRIP_X, IMAGE_SCROLL_SCREEN_TOP,
                                                         IMAGE_SCROLL_RIGHT_STRIP_PIXELS, seamY + 1, IMAGE_SCROLL_UPPER_V_ORIGIN - seamY, IMAGE_SCROLL_UPPER_CLUT,
                                                         IMAGE_SCROLL_UPPER_RIGHT_VRAM_X, IMAGE_SCROLL_UPPER_VRAM_Y);

    SHELTER_B1_POD_ACCESS_TUNNEL_DRAW_IMAGE_SCROLL_STRIP(IMAGE_SCROLL_SCREEN_LEFT, seamY + IMAGE_SCROLL_SCREEN_TOP,
                                                         IMAGE_SCROLL_PAGE_PIXELS, SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_LINES - seamY, 0, IMAGE_SCROLL_LOWER_CLUT,
                                                         IMAGE_SCROLL_LOWER_LEFT_VRAM_X, IMAGE_SCROLL_LOWER_VRAM_Y);
    SHELTER_B1_POD_ACCESS_TUNNEL_DRAW_IMAGE_SCROLL_STRIP(IMAGE_SCROLL_RIGHT_STRIP_X, seamY + IMAGE_SCROLL_SCREEN_TOP,
                                                         IMAGE_SCROLL_RIGHT_STRIP_PIXELS, SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_LINES - seamY, 0, IMAGE_SCROLL_LOWER_CLUT,
                                                         IMAGE_SCROLL_LOWER_RIGHT_VRAM_X, IMAGE_SCROLL_LOWER_VRAM_Y);
#undef SHELTER_B1_POD_ACCESS_TUNNEL_DRAW_IMAGE_SCROLL_STRIP
}

/// Stages audio start for the selected scene for the view loader to commit.
///
/// Called after scene selection; the scene and playback buffers must remain
/// live through commit and consumption. No selected slot leaves the previous
/// deferred request intact.
static void _shelterB1PodAccessTunnelStageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Enqueues playback of the selected scene after the opening CAP cue.
///
/// The selected scene and prepared buffers must survive playback; requires
/// space in the CD request ring. Without a scene slot, marks playback active.
static void _shelterB1PodAccessTunnelStartScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Finishes scene streaming and restores the random state saved at selection.
///
/// Called on the script's normal completion path. Requires prior successful
/// scene selection; buffer and task owners still perform their own teardown.
static void _shelterB1PodAccessTunnelFinishScene(void)
{
    streamFinishScene();
}

/// Requests CD scene cancellation and finishes streaming when the script is skipped.
///
/// Discards the deferred request and immediately restores saved random state.
/// Cancellation completes through later CD dispatches; scene buffers and tasks
/// remain their owners' responsibility. Requires prior scene selection.
static void _shelterB1PodAccessTunnelCancelScene(void)
{
    cdCmdCancelScene();
}

/// Starts the scene camera's negative-local-Y pan for `frameCount` frame updates.
///
/// The entry script supplies 150; positive durations must fit the task's signed
/// halfword move counter (1..32767); nonpositive counts perform no moves.
/// The task ends on reaching that count or
/// leaving logical view 11. The descriptor is consumed synchronously; keep this
/// overlay and the edited camera live until the task completes. Spawn failure
/// is ignored. No payload storage is retained.
static void _shelterB1PodAccessTunnelStartSceneCameraPan(s32 frameCount)
{
    enum { SCENE_CAMERA_PAN_TASK_INDEX = 0 };

    taskSpawnFromTable(_gShelterB1PodAccessTunnelSceneTaskDescs, SCENE_CAMERA_PAN_TASK_INDEX, frameCount, 0);
}

/// Moves the pod-pan camera origin along negative camera Y for a fixed duration.
///
/// `spawnArg1.value` is the frame count; `killCountdown` starts at zero and
/// counts completed moves as a signed halfword. Each move is 16 world units.
/// The script supplies 150 frames; durations must fit this signed counter.
/// Kills the task on completion or on leaving logical view 11. The camera is
/// edited in place and must remain loaded through each queued application.
static void _shelterB1PodAccessTunnelPanSceneCameraTask(Task* task)
{
    enum { CAMERA_PAN_STEP_WORLD_UNITS = 16 };

    ViewCamera* camera;
    VECTOR      cameraOffset;

    if (task->killCountdown < task->spawnArg1.value && gGameSession->location.view == SHELTER_B1_POD_ACCESS_TUNNEL_POD_PAN_VIEW) {
        camera          = viewGetMappedCamera(&gGameSession->location);
        cameraOffset.vx = 0;
        cameraOffset.vy = CAMERA_PAN_STEP_WORLD_UNITS;
        cameraOffset.vz = 0;
        // Convert camera-space translation to the stored negated world origin.
        ApplyTransposeMatrixLV(&camera->transform, &cameraOffset, &cameraOffset);
        camera->transform.t[0] += cameraOffset.vx;
        camera->transform.t[1] += cameraOffset.vy;
        camera->transform.t[2] += cameraOffset.vz;
        viewQueueCamera(camera);
        task->killCountdown++;
        return;
    }
    taskKill(task);
}

/// Starts the pod-chamber image scroll with a `frameCount`-frame nominal line rate.
///
/// Requires a positive duration; the entry script supplies 240. The task holds
/// the lower image for another 46 draws and uses a truncated 16.16 rate, so the
/// nominal duration does not guarantee exact completion. It keeps drawing until
/// a view becomes ready or the event ends. Start in logical view 11 and keep this
/// overlay, both images and current graphics buffers live; end the event before
/// the signed halfword timer or fixed-point offset overflows. The descriptor is
/// consumed synchronously, task teardown owns its work, and spawn failure is ignored.
static void _shelterB1PodAccessTunnelStartImageScroll(s32 frameCount)
{
    enum { IMAGE_SCROLL_TASK_INDEX = 1 };

    taskSpawnFromTable(_gShelterB1PodAccessTunnelSceneTaskDescs, IMAGE_SCROLL_TASK_INDEX, frameCount, 0);
}

/// Image-scroll handlers indexed by task state: initialize, draw and release.
///
/// States 0..2 have no terminator. Dispatch copies all three pointers before
/// calling the selected handler; initialization owns work until task teardown.
static const TaskFuncTable3 _gShelterB1PodAccessTunnelImageScrollStates = {
    { _shelterB1PodAccessTunnelInitImageScroll, _shelterB1PodAccessTunnelDrawImageScroll, taskKill },
};

/// Runs the delayed scroll from the lower pod-chamber image to the upper one.
///
/// Start in state 0 with a positive scroll duration in `spawnArg1.value`, in
/// frames. State 1 draws; state 2 releases the task. Default teardown owns the
/// work allocated during initialization. The room overlay, images and current
/// graphics buffers must remain live throughout the task.
static void _shelterB1PodAccessTunnelImageScrollTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gShelterB1PodAccessTunnelImageScrollStates;
    states.funcs[task->state](task);
}

/// Allocates the image scroll's owned work and derives its 16.16 line rate.
///
/// Requires state 0 and a positive frame count in `spawnArg1.value`; the
/// script supplies 240. Outside logical view 11 or on allocation failure,
/// kills the task. Otherwise attaches zeroed primary-heap work and enters
/// state 1; default task teardown releases that allocation.
static void _shelterB1PodAccessTunnelInitImageScroll(Task* task)
{
    enum { IMAGE_SCROLL_DRAW = 1 };

    _ShelterB1PodAccessTunnelImageScrollWork* work;

    if (gGameSession->location.view != SHELTER_B1_POD_ACCESS_TUNNEL_POD_PAN_VIEW) {
        taskKill(task);
        return;
    }
    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work   = work;
    work->speed  = (SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_LINES << SHELTER_B1_POD_ACCESS_TUNNEL_IMAGE_SCROLL_FRACTION_BITS) / task->spawnArg1.value;
    task->state += IMAGE_SCROLL_DRAW;
}

/// Queues the image scroll's 8-bit texture-page command before its sprites.
///
/// `vramX` is in VRAM words, rounded down to a 64-word page within 0..1023;
/// `vramY` is in scanlines (the callers use 0 or 256). Uses average blending,
/// no dithering, and permits drawing into the display area. Requires a current
/// 1024-entry ordering table and packet arena with room for one `DR_TPAGE`.
static void _shelterB1PodAccessTunnelQueueImageScrollTexturePage(s32 vramX, s16 vramY)
{
    enum { IMAGE_SCROLL_TEXTURE_8_BIT     = 1,
           IMAGE_SCROLL_PAGE_X_MASK       = 0x3C0,
           IMAGE_SCROLL_DRAW_DISPLAY_AREA = 1,
           IMAGE_SCROLL_DITHER_OFF        = 0,
           IMAGE_SCROLL_ORDERING_SLOT     = 1023 };

    DR_TPAGE* drawMode;
    s32       pageY;

    // Promote the scanline once for the SDK's texture-page bit encoding.
    pageY          = vramY;
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, IMAGE_SCROLL_DRAW_DISPLAY_AREA, IMAGE_SCROLL_DITHER_OFF,
                 getTPage(IMAGE_SCROLL_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, vramX & IMAGE_SCROLL_PAGE_X_MASK, pageY));
    addPrim(gGpuCurrentOt + IMAGE_SCROLL_ORDERING_SLOT, drawMode);
}

/// Starts the entry scene's unfolding flying model and its attached beam.
///
/// Actor package 141000 must be loaded: descriptor 0 creates the model-owning
/// controller, which unfolds, holds and follows its recorded flight path.
/// Keep that actor overlay and its model resources loaded until a ready view
/// releases the controller and its beam. The descriptor is consumed synchronously;
/// the new task owns its body and work, and spawn failure is ignored.
static void _shelterB1PodAccessTunnelStartFlightController(void)
{
    enum { FLIGHT_CONTROLLER_TASK_INDEX = 0 };

    taskSpawnFromTable(gActor141000FlightTasks, FLIGHT_CONTROLLER_TASK_INDEX, 0, 0);
}

/// Queues a gray captured-frame display task lasting `frameCount + 1` ticks.
///
/// The script supplies 12. A pending display-mode request rejects the enqueue.
/// Requires a nonnegative frame count and the room overlay to
/// remain loaded until the countdown task completes and requests mode exit.
static void _shelterB1PodAccessTunnelStartGrayCapture(s32 frameCount)
{
    enum { GRAY_CAPTURE_TASK_INDEX = 2U };

    displayQueueModeTask(taskGetDescAt(_gShelterB1PodAccessTunnelSceneTaskDescs, GRAY_CAPTURE_TASK_INDEX), frameCount, 0, STAGE_ENTRY_GRAY_CAPTURE);
}

/// Ends the gray captured-frame display mode after its countdown becomes negative.
///
/// `spawnArg1.value` is a mutable signed frame count decremented each tick.
/// A nonnegative initial value N keeps the mode for N+1 ticks. Requires a
/// live display-mode context; releases the task before requesting mode exit.
static void _shelterB1PodAccessTunnelGrayCaptureCountdownTask(Task* task)
{
    s32 remainingFrames;

    remainingFrames       = task->spawnArg1.value - 1;
    task->spawnArg1.value = remainingFrames;
    if (remainingFrames < 0) {
        taskKill(task);
        stageRequestModeTaskExit();
    }
}

/// Requests cancellation of room effects during scene-skip cleanup.
///
/// Requires the room-effect controller to remain live. It publishes the
/// cancellation on its next update; each effect then performs its own teardown.
static void _shelterB1PodAccessTunnelCancelRoomEffects(void)
{
    roomEffectRequestCancelAll();
}

/// Selects this room's flash, twin-trail and spark-burst tasks for GOLEM attacks.
///
/// Installs packed effect bank/type IDs used by subsequent `effectSpawn` calls.
/// Call after room-effect controller initialization and before spawning these
/// effects. The selections persist until reset or rebound; this room overlay
/// must remain loaded while any selected task runs.
static inline void _shelterB1PodAccessTunnelSelectCombatEffects(void)
{
    gRoomEffectFlashId      = EFFECT_SHELTER_B1_POD_ACCESS_TUNNEL_FLASH;
    gRoomEffectTwinTrailId  = EFFECT_SHELTER_B1_POD_ACCESS_TUNNEL_TWIN_TRAIL;
    gRoomEffectSparkBurstId = EFFECT_SHELTER_B1_POD_ACCESS_TUNNEL_SPARK_BURST;
}

void shelterB1PodAccessTunnelDrawGlowsTask(Task* task)
{
    enum { GLOWS_SELECT_EFFECTS,
           GLOWS_DRAW,
           GLOWS_RADIUS_WORLD_UNITS = 0x180,
           GLOWS_PACKED_WHITE       = 0x111 };

    u8 mappedView;

    if (task->state == GLOWS_SELECT_EFFECTS) {
        _shelterB1PodAccessTunnelSelectCombatEffects();
        task->state = GLOWS_DRAW;
    }

    // Each consecutive endpoint pair defines one fixed light in world space.
    mappedView = viewGetMappedIndex();
    switch (mappedView) {
        case 2: {
            const SVECTOR(*lightSegments)[2] = _gShelterB1PodAccessTunnelLongitudinalLightSegments;
            _glowDrawCapsule(lightSegments[0], GLOWS_RADIUS_WORLD_UNITS, GLOWS_PACKED_WHITE);
            _glowDrawCapsule(lightSegments[1], GLOWS_RADIUS_WORLD_UNITS, GLOWS_PACKED_WHITE);
        } break;
        case 3:
        case 7: {
            const SVECTOR(*lightSegments)[2] = _gShelterB1PodAccessTunnelTransverseLightSegments;
            _glowDrawCapsule(lightSegments[0], GLOWS_RADIUS_WORLD_UNITS, GLOWS_PACKED_WHITE);
            _glowDrawCapsule(lightSegments[1], GLOWS_RADIUS_WORLD_UNITS, GLOWS_PACKED_WHITE);
            _glowDrawCapsule(lightSegments[2], GLOWS_RADIUS_WORLD_UNITS, GLOWS_PACKED_WHITE);
            _glowDrawCapsule(lightSegments[3], GLOWS_RADIUS_WORLD_UNITS, GLOWS_PACKED_WHITE);
        } break;
        case 4: {
            const SVECTOR(*lightSegments)[2] = _gShelterB1PodAccessTunnelTransverseLightSegments;
            _glowDrawCapsule(lightSegments[0], GLOWS_RADIUS_WORLD_UNITS, GLOWS_PACKED_WHITE);
            _glowDrawCapsule(lightSegments[1], GLOWS_RADIUS_WORLD_UNITS, GLOWS_PACKED_WHITE);
        } break;
    }
}

#include "../../shared/effects/glow_draw_capsule.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelterB1PodAccessTunnelRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelterB1PodAccessTunnelRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelterB1PodAccessTunnelRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
