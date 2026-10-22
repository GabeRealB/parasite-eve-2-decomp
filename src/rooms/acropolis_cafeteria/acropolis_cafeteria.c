#include "rooms/acropolis_cafeteria.h"
#include "../../stages/akropolis/event_flags.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "acropolis_cafeteria_private.h"

#include "actors/actor_202900.h"

#include "actors/actor_210600.h"

#include "actors/actor_310600.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/companion_load.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

/// Shares the cafeteria's twin-trail endpoint table with its effect-task TU.
///
/// Empty presence flag: keep it defined through the shared header and the
/// single trail-data inclusion here; acropolis_cafeteria_2.c reads the table.
#define ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS
#include "../../shared/room/room_visual_effects.h"

#define ROOM_PART declarations_acropolis_cafeteria
#include "assets/rooms/akropolis/cafeteria/declarations.inc"

extern void func_807245E4(void*);
extern void func_80724608(void*, s32, s32, void*);

static void       _acropolisCafeteriaTickPlayerDebug(Task* task);
static void       _acropolisCafeteriaInitRoom(Task* task);
static inline s32 _acropolisCafeteriaResolvePatioRoom(const RoomEventMsg* request, RoomEventMsg* reply);

enum {
    ACROPOLIS_CAFETERIA_PROGRESS_AFTER_SCENE    = 2,
    ACROPOLIS_CAFETERIA_PLACED_STRANGER         = 0,
    ACROPOLIS_CAFETERIA_SCENE_NOT_STARTED       = 0,
    ACROPOLIS_CAFETERIA_SCENE_ACTIVE            = 1,
    ACROPOLIS_CAFETERIA_PLAYER_ANIMATION        = 1,
    ACROPOLIS_CAFETERIA_STRANGER_COMMAND_START  = 1,
    ACROPOLIS_CAFETERIA_STRANGER_COMMAND_RECOIL = 4,
    ACROPOLIS_CAFETERIA_STRANGER_COMMAND_SETTLE = 7,
    ACROPOLIS_CAFETERIA_VIEW_COUNT              = 24,
    ACROPOLIS_CAFETERIA_COLLISION_CELLS_X       = 3,
    ACROPOLIS_CAFETERIA_COLLISION_CELLS_Z       = 3,
    ACROPOLIS_CAFETERIA_COLLISION_CELL_SIZE     = 4000,
    ACROPOLIS_CAFETERIA_UNIT_NORMAL             = ONE,
    ACROPOLIS_CAFETERIA_WARP1_MAP_FLAG          = 499,
    ACROPOLIS_CAFETERIA_WARP2_MAP_FLAG          = 501,
    ACROPOLIS_CAFETERIA_WARP3_MAP_FLAG          = 500,
    ACROPOLIS_CAFETERIA_TRIGGER_FOLLOW_UP       = 0,
    ACROPOLIS_CAFETERIA_TRIGGER_CAP_COMMAND     = 2,
    ACROPOLIS_CAFETERIA_TRIGGER_START_BATTLE    = 3,
    ACROPOLIS_CAFETERIA_CAP_COMMAND_7           = 7,
    ACROPOLIS_CAFETERIA_CAP_COMMAND_8           = 8,
    ACROPOLIS_CAFETERIA_CAP_COMMAND_10          = 10,
    ACROPOLIS_CAFETERIA_CAP_COMMAND_20          = 20,
    ACROPOLIS_CAFETERIA_CAP_COMMAND_21          = 21,
    ACROPOLIS_CAFETERIA_CAP_COMMAND_22          = 22,
    ACROPOLIS_CAFETERIA_CAP_COMMAND_23          = 23,
    ACROPOLIS_CAFETERIA_CAP_COMMAND_24          = 24,
    ACROPOLIS_CAFETERIA_CAP_COMMAND_25          = 25,
};

// Current-area warp slot in the high nibble; destination arrival slot in the low.
enum {
    ACROPOLIS_CAFETERIA_WARP_TO_PATIO     = (2 << 4) | 3,
    ACROPOLIS_CAFETERIA_WARP_TO_HALLWAY_3 = (1 << 4) | 3,
    ACROPOLIS_CAFETERIA_WARP_TO_HALLWAY_4 = (3 << 4) | 4,
};

/// Cafeteria room-task states: initialize, tick the player debug hook, and exit.
///
/// Indexed by `Task::state` in 0..2 with no bound check or terminator. Dispatch
/// copies all three callback pointers; their code must remain loaded, and the
/// exit handler releases the task.
static const TaskFuncTable3 _gAcropolisCafeteriaRoomStateHandlers = {
    { _acropolisCafeteriaInitRoom, _acropolisCafeteriaTickPlayerDebug, taskKill },
};

/// Player label borrowed by the injected player-debug hook.
///
/// The null-terminated text remains read-only for the loaded overlay lifetime.
/// The external hook's untyped interface does not establish permission to write it.
static const char _gAcropolisCafeteriaPlayerDebugLabel[12] = "Player";

static AnimationSet _gAkropolisCafeteriaAcropolisCafeteriaFollowUpPlayerAnim01Clip;
static s32          _acropolisCafeteriaResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32          _acropolisCafeteriaRefuseKeyItemMsg(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32          _acropolisCafeteriaHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg);
static s32          _acropolisCafeteriaHandleActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unusedArg);
static s32          _acropolisCafeteriaPlaySoundMsg(Task* task, s32 messageId, s32 soundCue, s32 unusedArg);
static void         _acropolisCafeteriaBattleSceneTask(Task* task);
static void         _acropolisCafeteriaFollowUpSceneTask(Task* task);
static void         _acropolisCafeteriaSettleStrangerTask(Task* task);
static void         _acropolisCafeteriaSetPlayerSurface(s32 surfaceClass);
static void         _acropolisCafeteriaCancelPeEffects(void);
static void         _acropolisCafeteriaCopyPostBattleVram(void);
static void         _acropolisCafeteriaPreparePostBattleScene(void);

enum {
    ACROPOLIS_CAFETERIA_SCENE_TASK_PRIORITY           = 32,
    ACROPOLIS_CAFETERIA_POST_BATTLE_PLAYER_CLIP       = 8,
    ACROPOLIS_CAFETERIA_POST_BATTLE_PLAYER_INTRO_CLIP = 9,
    ACROPOLIS_CAFETERIA_OPENING_WOMAN_CLIP            = 1,
    ACROPOLIS_CAFETERIA_OPENING_PLAYER_CLIP           = ANIMATION_BANK_BASE_SET_COUNT,
    ACROPOLIS_CAFETERIA_FOLLOW_UP_PLAYER_CLIP_47      = ANIMATION_BANK_BASE_SET_COUNT,
    ACROPOLIS_CAFETERIA_FOLLOW_UP_PLAYER_CLIP_48      = ANIMATION_BANK_BASE_SET_COUNT + 1,
    ACROPOLIS_CAFETERIA_FOLLOW_UP_PLAYER_CLIP_49      = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACROPOLIS_CAFETERIA_FOLLOW_UP_PLAYER_CLIP_50      = ANIMATION_BANK_BASE_SET_COUNT + 3,
    ACROPOLIS_CAFETERIA_POST_BATTLE_STRANGER_COMMAND  = 2,
};

// Receiver selectors and scene-resource keys used by the cafeteria scripts.
enum {
    ACROPOLIS_CAFETERIA_STRANGER_COMMAND_SLOT2      = 3,
    ACROPOLIS_CAFETERIA_STRANGER_COMMAND_SLOT0      = 5,
    ACROPOLIS_CAFETERIA_STRANGER_COMMAND_BURN       = 8,
    ACROPOLIS_CAFETERIA_STRANGER_COMMAND_CLIP17     = 9,
    ACROPOLIS_CAFETERIA_SCENE_STRANGER              = 0,
    ACROPOLIS_CAFETERIA_SCENE_WOMAN                 = 1,
    ACROPOLIS_CAFETERIA_SCENE_GRINNING_STRANGER     = 1,
    ACROPOLIS_CAFETERIA_SCENE_RUPERT                = 2,
    ACROPOLIS_CAFETERIA_SCENE_DRAW_HIDE             = 0,
    ACROPOLIS_CAFETERIA_SCENE_DRAW_SHOW             = 1,
    ACROPOLIS_CAFETERIA_SCENE_DRAW_SKIP_AUTO_BUFFER = 2,
    ACROPOLIS_CAFETERIA_RUPERT_ANIMATION_BANK       = 0,
    ACROPOLIS_CAFETERIA_RUPERT_SHOOTING_CLIP        = 1,
    ACROPOLIS_CAFETERIA_RUPERT_ARRIVAL_CLIP         = 2,
    ACROPOLIS_CAFETERIA_RUPERT_RELOAD_CLIP          = 3,
    ACROPOLIS_CAFETERIA_RUPERT_DEPARTURE_CLIP       = 4,
    ACROPOLIS_CAFETERIA_PLAYER_FOLLOW_UP_WALK_CLIP  = 19,
    ACROPOLIS_CAFETERIA_SCENE_STREAM_GROUP          = 1,
    ACROPOLIS_CAFETERIA_FOLLOW_UP_STREAM_ID         = 6,
    ACROPOLIS_CAFETERIA_FOLLOW_UP_STREAM_SUB_ID     = 11,
    ACROPOLIS_CAFETERIA_CAP_OPENING                 = 1,
    ACROPOLIS_CAFETERIA_CAP_FOLLOW_UP               = 2,
    ACROPOLIS_CAFETERIA_CAP_POST_BATTLE             = 11,
    ACROPOLIS_CAFETERIA_CAP_BATTLE_INTRO            = 12,
    ACROPOLIS_CAFETERIA_CAP_SHAKE                   = 13,
    ACROPOLIS_CAFETERIA_EVENT_HANDOFF               = 2,
    ACROPOLIS_CAFETERIA_OPENING_SURFACE_CLASS       = 2,
    ACROPOLIS_CAFETERIA_OPENING_MUSIC_EVENT         = 6,
    ACROPOLIS_CAFETERIA_OPENING_VOLUME_INDEX        = 76,
    ACROPOLIS_CAFETERIA_FOLLOW_UP_LIGHT_SCALE       = 128, // Half intensity in 1/256 units.
    ACROPOLIS_CAFETERIA_POST_BATTLE_SOUND           = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA, 12),
    ACROPOLIS_CAFETERIA_STRANGER_MESSAGE_ALERT      = 2007,
    ACROPOLIS_CAFETERIA_MOVIE_TASK_PRIORITY         = 192,
};

/// Message dispatch for cafeteria transitions, actions, commands, items and sound.
///
/// The room task borrows all six entries while this overlay is loaded. The
/// final marker makes unsupported IDs return zero; handlers borrow their
/// receiver-specific payloads only through synchronous dispatch.
static TaskMessageEntry _gAcropolisCafeteriaRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisCafeteriaResolveRoomTransition },
    { DIRECTION_MESSAGE_ROOM_ACTION, _acropolisCafeteriaHandleActionMessage },
    { ROOM_MESSAGE_COMMAND, _acropolisCafeteriaHandleCommandMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _acropolisCafeteriaRefuseKeyItemMsg },
    { ROOM_MESSAGE_SOUND, _acropolisCafeteriaPlaySoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gAcropolisCafeteriaSceneTasks[4] = {
    { .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = ACROPOLIS_CAFETERIA_SCENE_TASK_PRIORITY } },
      .callback = _acropolisCafeteriaBattleSceneTask,
      .data     = { .value = 0 } },
    { .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = ACROPOLIS_CAFETERIA_SCENE_TASK_PRIORITY } },
      .callback = _acropolisCafeteriaFollowUpSceneTask,
      .data     = { .value = 0 } },
    { .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = ACROPOLIS_CAFETERIA_SCENE_TASK_PRIORITY } },
      .callback = _acropolisCafeteriaSettleStrangerTask,
      .data     = { .value = 0 } },
    { .header = { .fields = { .flags = TASK_DESC_END, .priority = 0 } }, .callback = NULL, .data = { .model = NULL } },
};

/// Player clip-1 request shared by the cafeteria event scenes.
///
/// The event interpreter copies the request and resolves its bank from the equipped
/// weapon before dispatch. Resets playback without blending and disables world
/// collision; the copied request is consumed synchronously.
static AnimationPlayRequest _gAcropolisCafeteriaPlayerAnimation1 = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_CAFETERIA_PLAYER_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced bank-1, clip-1 request; its recipient and scene role are unproven.
static AnimationPlayRequest D_acropolis_cafeteria_80182B1C = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 8 at the end of the post-battle scene, including its skip path.
///
/// The interpreter resolves the weapon bank in a request copy. Dispatch resets
/// playback without blending and requests world collision disablement.
static AnimationPlayRequest _gAcropolisCafeteriaPlayerPostBattleAnimation8 = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_CAFETERIA_POST_BATTLE_PLAYER_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player clip 9 at the start of the post-battle scene.
///
/// The interpreter resolves the weapon bank in a request copy. Dispatch resets
/// playback without blending and requests world collision disablement.
static AnimationPlayRequest _gAcropolisCafeteriaPlayerPostBattleAnimation9 = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_CAFETERIA_POST_BATTLE_PLAYER_INTRO_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the cafeteria woman's clip 1 during the opening scene.
///
/// Placement 1 in the opening layout reads only animationId and reseeds its
/// driven tracks synchronously; the other request fields are ignored.
static AnimationPlayRequest _gAcropolisCafeteriaOpeningWomanAnimation1 = {
    .source               = { .index = 0 },
    .animationId          = ACROPOLIS_CAFETERIA_OPENING_WOMAN_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player placement used with the opening scene's extended animation.
///
/// XYZ uses integer root-parent units; Euler angles use 4096 units per turn.
/// The complete transform is borrowed through synchronous dispatch.
static ActorTransform _gAcropolisCafeteriaPlayerOpeningScenePlacement = { .pos = { 0, 128, 0, 0 }, .rot = { 0, 0, 0, 0 } };

/// Player placement restored by both exits from the opening scene.
///
/// XYZ uses room game units; yaw is a half turn. The player consumes the complete
/// transform synchronously from this overlay-owned record.
static ActorTransform _gAcropolisCafeteriaPlayerOpeningResumePlacement = { .pos = { -3759, -300, 18, 0 }, .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

/// Player placement at the beginning of the accepted follow-up scene.
///
/// XYZ uses room game units; yaw is a half turn. Borrowed synchronously before
/// the first follow-up animation; the overlay owns the record.
static ActorTransform _gAcropolisCafeteriaPlayerFollowUpStartPlacement = { .pos = { -3748, -300, -885, 0 }, .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

/// Player placement before Rupert is shown in the follow-up scene.
///
/// XYZ uses room game units; yaw is a half turn. The player copies the complete
/// transform during synchronous dispatch; no pointer is retained.
static ActorTransform _gAcropolisCafeteriaPlayerBeforeRupertArrivalPlacement = { .pos = { -3689, -300, -2934, 0 }, .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

/// Player placement as the battle introduction hands control to combat.
///
/// XYZ uses room game units; yaw uses 4096 units per turn. The complete transform
/// is borrowed through synchronous dispatch while the overlay is loaded.
static ActorTransform _gAcropolisCafeteriaPlayerBattleStartPlacement = { .pos = { -3892, -300, -1836, 0 }, .rot = { 0, 1800, 0, 0 } };

/// Player placement before the scripted walk in the follow-up scene.
///
/// XYZ uses room game units; yaw is a half turn. The player consumes the complete
/// transform during synchronous dispatch.
static ActorTransform _gAcropolisCafeteriaPlayerFollowUpWalkStartPlacement = { .pos = { -3000, -300, 1000, 0 }, .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

/// Player walk destination and subsequent facing target in the follow-up scene.
///
/// The move message copies XYZ in room game units; the later turn copies only
/// yaw, in 4096 units per turn. Neither retains this overlay-owned pointer.
static ActorTransform _gAcropolisCafeteriaPlayerFollowUpWalkTarget = { .pos = { -3000, -300, -600, 0 }, .rot = { 0, 2304, 0, 0 } };

/// Unreferenced placement; its actor and coordinate frame are unproven.
static ActorTransform D_acropolis_cafeteria_80182C14 = {
    .pos = { -2800, -300, -892, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 },
};

/// Unreferenced bank-0, clip-0 request; its recipient and scene role are unproven.
static AnimationPlayRequest D_acropolis_cafeteria_80182C2C = {
    .source               = { .index = 0 },
    .animationId          = 0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// One borrowed player clip installed at bank slot 47 for the opening scene.
///
/// The copy spans exactly this pointer array. The woman's animation overlay
/// must remain loaded while the player's bank or playback retains its clip.
static AnimationSet* _gAcropolisCafeteriaOpeningPlayerAnimationSets[1] = {
    &gAkropolisCafeteriaPlayerCafeteriaOpeningClip,
};

/// Installs the opening scene's one borrowed clip in the equipped player bank.
///
/// Copies one 32-bit pointer to slot 47 synchronously without starting playback.
/// The request and source table live with this overlay; clip data stays borrowed.
static AnimationBankCopyRequest _gAcropolisCafeteriaOpeningPlayerBankCopy = {
    .sets  = _gAcropolisCafeteriaOpeningPlayerAnimationSets,
    .count = ARRAY_SIZE(_gAcropolisCafeteriaOpeningPlayerAnimationSets),
};

/// Plays the opening scene's installed player clip at bank slot 47.
///
/// Requires the opening bank copy and loaded woman clip data. The interpreter
/// resolves the weapon bank in a copy; playback resets without blending and
/// requests world collision disablement.
static AnimationPlayRequest _gAcropolisCafeteriaPlayerOpeningAnimation47 = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_CAFETERIA_OPENING_PLAYER_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Four borrowed player clips installed at bank slots 47..50 for the follow-up.
///
/// Slots borrow Rupert's clip, a room clip and two Grinning Stranger clips in
/// that order. The copy spans exactly four pointers; all three overlays must
/// remain loaded while the bank or playback retains the corresponding clips.
static AnimationSet* _gAcropolisCafeteriaFollowUpPlayerAnimationSets[4] = {
    &gAkropolisCafeteriaPlayerCafeteriaFollowUpStartClip,
    &_gAkropolisCafeteriaAcropolisCafeteriaFollowUpPlayerAnim01Clip,
    &gAkropolisCafeteriaPlayerCafeteriaRupertTalkClip,
    &gAkropolisCafeteriaPlayerCafeteriaFollowUpEndClip,
};

/// Installs the follow-up scene's four borrowed clips in the equipped player bank.
///
/// Copies four 32-bit pointers to slots 47..50 synchronously without starting
/// playback. The source table lives with this overlay; clip data stays borrowed.
static AnimationBankCopyRequest _gAcropolisCafeteriaFollowUpPlayerBankCopy = {
    .sets  = _gAcropolisCafeteriaFollowUpPlayerAnimationSets,
    .count = ARRAY_SIZE(_gAcropolisCafeteriaFollowUpPlayerAnimationSets),
};

/// Plays the follow-up's Rupert-supplied player clip at bank slot 47.
///
/// Requires the follow-up bank copy and loaded Rupert clip. The interpreter
/// resolves the weapon bank in a copy; playback resets without blending and
/// requests world collision disablement.
static AnimationPlayRequest _gAcropolisCafeteriaPlayerFollowUpAnimation47 = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_CAFETERIA_FOLLOW_UP_PLAYER_CLIP_47,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays the follow-up's room-supplied player clip at bank slot 48.
///
/// Requires the follow-up bank copy and loaded cafeteria clip. The interpreter
/// resolves the weapon bank in a copy; playback resets without blending and
/// requests world collision disablement.
static AnimationPlayRequest _gAcropolisCafeteriaPlayerFollowUpAnimation48 = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_CAFETERIA_FOLLOW_UP_PLAYER_CLIP_48,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays the follow-up's first Grinning Stranger-supplied player clip at slot 49.
///
/// Requires the follow-up bank copy and loaded Grinning Stranger clip data.
/// The interpreter resolves the weapon bank in a copy; playback resets without
/// blending and requests world collision disablement.
static AnimationPlayRequest _gAcropolisCafeteriaPlayerFollowUpAnimation49 = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_CAFETERIA_FOLLOW_UP_PLAYER_CLIP_49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays the follow-up's second Grinning Stranger-supplied player clip at slot 50.
///
/// Requires the follow-up bank copy and loaded Grinning Stranger clip data.
/// The interpreter resolves the weapon bank in a copy; playback resets without
/// blending and requests world collision disablement.
static AnimationPlayRequest _gAcropolisCafeteriaPlayerFollowUpAnimation50 = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_CAFETERIA_FOLLOW_UP_PLAYER_CLIP_50,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Stranger placement at the beginning of the battle introduction.
///
/// XYZ uses room game units; Euler angles use 4096 units per turn. The actor
/// consumes the complete transform during synchronous placement dispatch.
static ActorTransform _gAcropolisCafeteriaStrangerBattleIntroPlacement = { .pos = { -3106, -300, -2056, 0 }, .rot = { 0, 0, 0, 0 } };

/// Stranger placement shared by the post-battle scene and its skip path.
///
/// XYZ uses room game units; Euler angles use 4096 units per turn. The actor
/// borrows the complete transform only through synchronous dispatch.
static ActorTransform _gAcropolisCafeteriaStrangerPostBattlePlacement = { .pos = { -3860, -299, -1365, 0 }, .rot = { 0, 0, 0, 0 } };

/// Stranger placement at the start of the accepted follow-up scene.
///
/// XYZ uses room game units; Euler angles use 4096 units per turn. The actor
/// consumes the complete transform synchronously from this overlay-owned record.
static ActorTransform _gAcropolisCafeteriaStrangerFollowUpStartPlacement = { .pos = { -3890, -299, -1555, 0 }, .rot = { 0, 0, 0, 0 } };

/// Stranger placement paired with player clip 48 in the follow-up scene.
///
/// XYZ uses room game units; Euler angles use 4096 units per turn. The actor
/// consumes the complete transform during synchronous placement dispatch.
static ActorTransform _gAcropolisCafeteriaStrangerFollowUpAnimation48Placement = { .pos = { -3490, -299, -4500, 0 }, .rot = { 0, 0, 0, 0 } };

/// Stranger placement shared by the recoil scene and its settling controller.
///
/// Position is in room game units; Euler rotations use 4096 units per turn.
/// Borrowed synchronously by placement messages while the overlay is loaded.
static ActorTransform _gAcropolisCafeteriaStrangerRecoilPlacement = { { -3860, -299, -1253, 0 }, { 0, 200, 0, 0 } };

/// Stranger placement before its scripted slot-0 clip in the follow-up scene.
///
/// XYZ uses room game units; Euler angles use 4096 units per turn. The actor
/// borrows the complete transform only through synchronous dispatch.
static ActorTransform _gAcropolisCafeteriaStrangerFollowUpSlot0Placement = { .pos = { -3290, -299, -3500, 0 }, .rot = { 0, 0, 0, 0 } };

/// Stranger placement before the follow-up scene's death-burn command.
///
/// XYZ uses room game units; yaw uses 4096 units per turn. The actor consumes
/// the transform synchronously; the following command selects its state.
static ActorTransform _gAcropolisCafeteriaStrangerFollowUpBurnPlacement = { .pos = { -3650, -299, -961, 0 }, .rot = { 0, 200, 0, 0 } };

/// Stranger placement as the battle introduction hands control to combat.
///
/// XYZ uses room game units; yaw uses 4096 units per turn. Borrowed through
/// synchronous dispatch before the script releases player control.
static ActorTransform _gAcropolisCafeteriaStrangerBattleStartPlacement = { .pos = { -3026, -300, -4126, 0 }, .rot = { 0, -200, 0, 0 } };

/// Grinning Stranger placement shared by its follow-up appearance scripts.
///
/// Targets placement 1 in the battle layout. XYZ uses room game units; yaw
/// uses 4096 units per turn. The actor consumes the transform synchronously.
static ActorTransform _gAcropolisCafeteriaGrinningStrangerScenePlacement = { .pos = { -470, -1400, 701, 0 }, .rot = { 0, -800, 0, 0 } };

/// Unreferenced cafeteria-context command zero; its recipient and effect are unproven.
static ActorCommand D_acropolis_cafeteria_80182DA0 = {
    .context = { .loc = { GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA } },
    .command = 0,
};

/// Starts a cafeteria Stranger in its receiver-specific command-1 state.
///
/// The Grinning Stranger resumes animation 1; the combat Stranger enters alert
/// rewind. The context is Acropolis/cafeteria and dispatch borrows the four bytes.
static ActorCommand _gAcropolisCafeteriaStrangerStartCommand = { { .loc = { GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA } }, ACROPOLIS_CAFETERIA_STRANGER_COMMAND_START };

/// Selects the combat Stranger's scripted slot-1 clip in the post-battle scene.
///
/// Command 2 binds the clip in the Acropolis/cafeteria namespace and resets
/// scripted state entry. The actor borrows all four bytes through dispatch.
static ActorCommand _gAcropolisCafeteriaStrangerPostBattleSlot1Command = {
    .context = { .loc = { GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA } },
    .command = ACROPOLIS_CAFETERIA_POST_BATTLE_STRANGER_COMMAND,
};

/// Binds the combat Stranger's scripted slot 2 to its borrowed follow-up clip.
///
/// Requires the loaded Grinning Stranger clip and Acropolis/cafeteria receiver.
/// Consumes all four bytes synchronously, selects scripted playback and resets
/// state entry; the borrowed clip remains live through playback.
static ActorCommand _gAcropolisCafeteriaStrangerFollowUpSlot2Command = {
    .context = { .loc = { GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA } },
    .command = ACROPOLIS_CAFETERIA_STRANGER_COMMAND_SLOT2,
};

/// Selects the cafeteria Stranger's scripted slot-3 recoil animation.
///
/// Command 4 resets state entry and borrows the recoil clip for playback.
static ActorCommand _gAcropolisCafeteriaStrangerRecoilCommand = { { .loc = { GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA } }, ACROPOLIS_CAFETERIA_STRANGER_COMMAND_RECOIL };

/// Binds the combat Stranger's scripted slot 0 for the follow-up scene.
///
/// Requires its borrowed Grinning Stranger clip to remain loaded through
/// playback. Synchronous dispatch reads the complete four-byte command and
/// resets scripted state entry in the Acropolis/cafeteria namespace.
static ActorCommand _gAcropolisCafeteriaStrangerFollowUpSlot0Command = {
    .context = { .loc = { GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA } },
    .command = ACROPOLIS_CAFETERIA_STRANGER_COMMAND_SLOT0,
};

/// Selects the Stranger's recoil animation before room-driven root settling.
///
/// Command 7 is the slot-3 recoil alias in the Acropolis/cafeteria context.
/// The complete four-byte command is borrowed through synchronous dispatch.
static ActorCommand _gAcropolisCafeteriaStrangerSettleCommand = { { .loc = { GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA } }, ACROPOLIS_CAFETERIA_STRANGER_COMMAND_SETTLE };

/// Starts the combat Stranger's death-burn state during the follow-up scene.
///
/// Reads the Acropolis/cafeteria context and command synchronously from all
/// four bytes, then clears the previous-state latch so death-burn initializes.
static ActorCommand _gAcropolisCafeteriaStrangerFollowUpBurnCommand = {
    .context = { .loc = { GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA } },
    .command = ACROPOLIS_CAFETERIA_STRANGER_COMMAND_BURN,
};

/// Plays the combat Stranger's native clip 17 in the follow-up scene.
///
/// The Acropolis/cafeteria receiver consumes all four bytes synchronously,
/// selects scripted playback and resets state entry; native clips stay loaded.
static ActorCommand _gAcropolisCafeteriaStrangerFollowUpClip17Command = {
    .context = { .loc = { GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA } },
    .command = ACROPOLIS_CAFETERIA_STRANGER_COMMAND_CLIP17,
};

/// Rupert placement when he is revealed in the accepted follow-up scene.
///
/// Position uses room game units and yaw is a half turn. The scene's placement
/// 2 consumes the complete transform during synchronous dispatch.
static ActorTransform _gAcropolisCafeteriaRupertArrivalPlacement = {
    .pos = { -3403, -250, 800, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 },
};

/// Rupert's conversation placement, also used to position the scripted player.
///
/// Room initialization and the follow-up scene place Rupert here; the player
/// shares it for clip 49. XYZ uses room game units and yaw is a half turn.
/// Both receivers consume the complete transform during synchronous dispatch.
static ActorTransform _gAcropolisCafeteriaRupertConversationPlacement = { .pos = { -3850, -299, -900, 0 }, .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

/// Shared Rupert/player placement for the follow-up scene's final paired clips.
///
/// XYZ uses room game units and yaw is a half turn. Both receivers consume the
/// complete transform synchronously while the cafeteria overlay remains loaded.
static ActorTransform _gAcropolisCafeteriaRupertDeparturePlacement = {
    .pos = { -4400, -299, -1400, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 },
};

/// Rupert's bank-0 shooting clip during the accepted follow-up scene.
///
/// Dispatch reads the request synchronously; the loaded bank and clip remain
/// borrowed through playback. Reset blending is requested, but a repeated
/// bank/clip pair does not restart. Rupert ignores the world-collision word.
static AnimationPlayRequest _gAcropolisCafeteriaRupertShootingAnimation1 = {
    .source               = { .index = ACROPOLIS_CAFETERIA_RUPERT_ANIMATION_BANK },
    .animationId          = ACROPOLIS_CAFETERIA_RUPERT_SHOOTING_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Rupert's bank-0 clip 2 when he is revealed in the follow-up scene.
///
/// Read synchronously after arrival placement. Playback borrows the loaded
/// clip, resets only when the bank or clip changes, and ignores world collision.
static AnimationPlayRequest _gAcropolisCafeteriaRupertArrivalAnimation2 = {
    .source               = { .index = ACROPOLIS_CAFETERIA_RUPERT_ANIMATION_BANK },
    .animationId          = ACROPOLIS_CAFETERIA_RUPERT_ARRIVAL_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Rupert's bank-0 reload clip paired with player clip 49 in the follow-up.
///
/// Consumed synchronously; loaded animation data stays borrowed for playback.
/// Reset blending is requested, repeated bank/clip pairs are retained, and the
/// receiver ignores the request's world-collision word.
static AnimationPlayRequest _gAcropolisCafeteriaRupertReloadAnimation3 = {
    .source               = { .index = ACROPOLIS_CAFETERIA_RUPERT_ANIMATION_BANK },
    .animationId          = ACROPOLIS_CAFETERIA_RUPERT_RELOAD_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Rupert's bank-0 clip 4 paired with player clip 50 before scene departure.
///
/// The request is read synchronously; clip data must remain loaded for playback.
/// Reset blending is requested only on a changed bank/clip pair. Rupert ignores
/// world collision; the request does not supply movement speed.
static AnimationPlayRequest _gAcropolisCafeteriaRupertDepartureAnimation4 = {
    .source               = { .index = ACROPOLIS_CAFETERIA_RUPERT_ANIMATION_BANK },
    .animationId          = ACROPOLIS_CAFETERIA_RUPERT_DEPARTURE_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced current-folder stream key (4,11); its scene role is unproven.
static EvsSceneKey D_acropolis_cafeteria_80182E5C = {
    .group    = ACROPOLIS_CAFETERIA_SCENE_STREAM_GROUP,
    .streamId = 4,
    .subId    = 11,
};

/// Current-folder scene/audio stream for the accepted cafeteria follow-up.
///
/// Selects stream 6, first qualifier 11 and implicit second qualifier zero.
/// The interpreter retains this key until scene replacement or script end;
/// its storage and selected stream resources must remain loaded.
static EvsSceneKey _gAcropolisCafeteriaFollowUpSceneKey = {
    .group    = ACROPOLIS_CAFETERIA_SCENE_STREAM_GROUP,
    .streamId = ACROPOLIS_CAFETERIA_FOLLOW_UP_STREAM_ID,
    .subId    = ACROPOLIS_CAFETERIA_FOLLOW_UP_STREAM_SUB_ID,
};

/// Player approach clip 19 and arrival clip 1 for the follow-up scripted walk.
///
/// Both IDs address the equipped player bank, independently of its scene
/// extension at slots 47..50. MOVE_TO reads both words synchronously, retaining
/// their low halfwords in actor work; only the selected clip data stays borrowed.
static GameActorMoveAnim _gAcropolisCafeteriaPlayerFollowUpWalkAnimations = {
    .approachAnimId = ACROPOLIS_CAFETERIA_PLAYER_FOLLOW_UP_WALK_CLIP,
    .arrivalAnimId  = ACROPOLIS_CAFETERIA_PLAYER_ANIMATION,
};

/// Opening cafeteria scene before the woman is replaced by the battle layout.
///
/// Pairs with `_gAcropolisCafeteriaOpeningSceneSkipScript`. Requires layout 1,
/// the woman/player tasks and the woman's borrowed clip through extended player
/// playback. Restores native clip 1, weapons and the shared resume placement.
/// The interpreter borrows all 35 commands through END and their payloads;
/// waits count interpreter updates, and CAP cues synchronize the scene.
static EvsCommand _gAcropolisCafeteriaOpeningSceneScript[35] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisCafeteriaCancelPeEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gAcropolisCafeteriaOpeningPlayerBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FADE_VOLUME, { .value = ACROPOLIS_CAFETERIA_OPENING_VOLUME_INDEX }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_HIDE_HUD }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACROPOLIS_CAFETERIA_CAP_OPENING }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_WOMAN }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_CAFETERIA_SCENE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_WOMAN }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gAcropolisCafeteriaOpeningWomanAnimation1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerOpeningAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_RUN_MOVEMENT }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaPlayerOpeningScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisCafeteriaSetPlayerSurface }, { .value = ACROPOLIS_CAFETERIA_OPENING_SURFACE_CLASS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 69 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = ACROPOLIS_CAFETERIA_OPENING_MUSIC_EVENT }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_MUSIC_LOAD, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = ACROPOLIS_CAFETERIA_EVENT_HANDOFF }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaPlayerOpeningResumePlacement } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip exit from the opening scene to its shared player resume placement.
///
/// Selects the same scene music and native player clip as the full path, restores
/// weapons, cleans up CAP control and returns the primary fade. Borrows 14
/// commands through END; player, CAP and room resources stay loaded through it.
static EvsCommand _gAcropolisCafeteriaOpeningSceneSkipScript[14] = {
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = ACROPOLIS_CAFETERIA_OPENING_MUSIC_EVENT }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_MUSIC_LOAD, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = ACROPOLIS_CAFETERIA_EVENT_HANDOFF }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaPlayerOpeningResumePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skippable aftermath of the cafeteria Stranger battle.
///
/// Pairs with `_gAcropolisCafeteriaPostBattleSceneSkipScript`. Requires layout 2,
/// live player/Stranger/CAP tasks, the loaded borrowed Stranger clip and the
/// post-battle image regions. Plays player clips 9 then 8 around the Stranger
/// action and VRAM change. Borrows 19 commands through END and all payloads;
/// animation data stays loaded through playback, including after script end.
static EvsCommand _gAcropolisCafeteriaPostBattleSceneScript[19] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisCafeteriaPreparePostBattleScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerPostBattleAnimation9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACROPOLIS_CAFETERIA_CAP_POST_BATTLE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisCafeteriaCopyPostBattleVram }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaStrangerPostBattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisCafeteriaStrangerPostBattleSlot1Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACROPOLIS_CAFETERIA_POST_BATTLE_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 320 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerPostBattleAnimation8 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip exit from the aftermath, restoring the view and player clip 8.
///
/// Stops the scene's sound, selects Stranger recoil, applies the same VRAM copy
/// and placement, cleans up CAP and returns the fade. Requires layout 2 and
/// its loaded actor clips. Borrows 15 commands through END and their payloads.
static EvsCommand _gAcropolisCafeteriaPostBattleSceneSkipScript[15] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACROPOLIS_CAFETERIA_POST_BATTLE_SOUND }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisCafeteriaStrangerRecoilCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisCafeteriaCopyPostBattleVram }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaStrangerPostBattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerPostBattleAnimation8 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Accepted follow-up with Rupert, both Stranger forms and the scripted player.
///
/// Pairs with `_gAcropolisCafeteriaFollowUpSceneSkipScript`. Requires layout 2,
/// its three actor overlays, the current-folder scene stream and player/CAP
/// tasks. Installs borrowed player clips 47..50, sequences combat Stranger
/// commands and Rupert clips, then hands control back for the patio reload.
/// Borrows 80 commands through END, the selected scene key and payloads;
/// installed clip data stays loaded for every playback that uses it.
static EvsCommand _gAcropolisCafeteriaFollowUpSceneScript[80] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gAcropolisCafeteriaFollowUpSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 70 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_LIGHT_SCALE, { .value = ACROPOLIS_CAFETERIA_FOLLOW_UP_LIGHT_SCALE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_CAFETERIA_SCENE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_GRINNING_STRANGER }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_CAFETERIA_SCENE_DRAW_SKIP_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_RUPERT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_CAFETERIA_SCENE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaStrangerFollowUpStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gAcropolisCafeteriaFollowUpPlayerBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaPlayerFollowUpStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerFollowUpAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACROPOLIS_CAFETERIA_CAP_FOLLOW_UP }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisCafeteriaStrangerFollowUpClip17Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisCafeteriaStrangerRecoilCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaPlayerBeforeRupertArrivalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaStrangerFollowUpSlot0Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisCafeteriaStrangerFollowUpSlot0Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_RUPERT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_CAFETERIA_SCENE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_RUPERT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaRupertArrivalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gAcropolisCafeteriaRupertArrivalAnimation2 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaStrangerFollowUpAnimation48Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisCafeteriaStrangerFollowUpSlot2Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerFollowUpAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaStrangerRecoilPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_RUPERT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaRupertConversationPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisCafeteriaStrangerFollowUpClip17Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gAcropolisCafeteriaRupertShootingAnimation1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaPlayerFollowUpWalkStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 76 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gAcropolisCafeteriaPlayerFollowUpWalkTarget } }, { .message = { .moveAnimation = &_gAcropolisCafeteriaPlayerFollowUpWalkAnimations } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 57 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gAcropolisCafeteriaPlayerFollowUpWalkTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaStrangerFollowUpBurnPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisCafeteriaStrangerFollowUpBurnCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_RUPERT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaRupertConversationPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gAcropolisCafeteriaRupertReloadAnimation3 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaRupertConversationPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerFollowUpAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 686 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_CAFETERIA_SCENE_DRAW_SKIP_AUTO_BUFFER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_GRINNING_STRANGER }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_CAFETERIA_SCENE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_GRINNING_STRANGER }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaGrinningStrangerScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_GRINNING_STRANGER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisCafeteriaStrangerStartCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_GRINNING_STRANGER }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_CAFETERIA_SCENE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_RUPERT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaRupertDeparturePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gAcropolisCafeteriaRupertDepartureAnimation4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaRupertDeparturePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerFollowUpAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 252 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = ACROPOLIS_CAFETERIA_EVENT_HANDOFF }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_LIGHT_SCALE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip exit that releases follow-up presentation before the patio handoff.
///
/// Cleans up CAP, drops the light override, restores player drawing and ends
/// scripted control. Requires the same loaded room/player resources as the
/// full path. The interpreter borrows all 10 commands through END.
static EvsCommand _gAcropolisCafeteriaFollowUpSceneSkipScript[10] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_LIGHT_SCALE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = ACROPOLIS_CAFETERIA_EVENT_HANDOFF }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Unreferenced scene stream that shakes the screen and starts CAP slot 13.
///
/// Restores player clip 1, waits 250 interpreter updates and signals handoff.
/// Its trigger and HUD mode are unproven. If run, all six commands through END
/// require loaded player/CAP resources and the equipped animation bank.
static EvsCommand _gAcropolisCafeteriaCap13ShakeScript[6] = {
    { EVENT_SCRIPT_OPCODE_SHAKE_SCREEN, { .value = 4 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACROPOLIS_CAFETERIA_CAP_SHAKE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 250 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = ACROPOLIS_CAFETERIA_EVENT_HANDOFF }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Stranger appearance and combat engagement after the cafeteria layout swap.
///
/// Requires layout 2, live player/Stranger/CAP tasks and native animation banks.
/// Shows and places the Stranger, engages battle, restores the combat placements
/// and resumes player aim control without transferring the animated root offset.
/// Started without a skip stream; borrows 19 commands through END and payloads.
static EvsCommand _gAcropolisCafeteriaBattleIntroScript[19] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisCafeteriaPlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_CAFETERIA_SCENE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACROPOLIS_CAFETERIA_CAP_BATTLE_INTRO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaStrangerBattleIntroPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisCafeteriaStrangerStartCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaPlayerBattleStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaStrangerBattleStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_STRANGER }, { .value = ACROPOLIS_CAFETERIA_STRANGER_MESSAGE_ALERT }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Unreferenced appearance stream for placement 1's Grinning Stranger.
///
/// Shows and places the actor, then starts its clip-1 playback. Requires layout
/// 2 with the Grinning Stranger overlay loaded. The trigger is unproven; if run,
/// the interpreter borrows all four commands through END and their payloads.
static EvsCommand _gAcropolisCafeteriaGrinningStrangerAppearanceScript[4] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_GRINNING_STRANGER }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_CAFETERIA_SCENE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_GRINNING_STRANGER }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisCafeteriaGrinningStrangerScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_CAFETERIA_SCENE_GRINNING_STRANGER }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gAcropolisCafeteriaStrangerStartCommand } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Live cafeteria scene progress (0 not started, 1 active, 2 battle released).
///
/// Independent of saved story progress, this overlay-local latch gates actions,
/// commands and departures. It starts at zero whenever the overlay is loaded.
static s32 _gAcropolisCafeteriaSceneProgress = ACROPOLIS_CAFETERIA_SCENE_NOT_STARTED;

/// Source image rectangle for the post-battle VRAM copy to (384,256).
///
/// X and width count 16-bit VRAM words; Y and height count rows.
/// The 64-by-256 region's image format and pictured content are unproven.
static RECT _gAcropolisCafeteriaPostBattleImageSource = { .x = 704, .y = 0, .w = 64, .h = 256 };

/// Source row rectangle for the post-battle VRAM copy to (0,247).
///
/// X and width count 16-bit VRAM words; Y and height count rows.
/// The 256-word row's content is unproven.
static RECT _gAcropolisCafeteriaPostBattleRowSource = { .x = 0, .y = 271, .w = 256, .h = 1 };

TaskDesc gAcropolisCafeteriaMovieTasks[ACROPOLIS_CAFETERIA_MOVIE_TASK_COUNT] = {
    { .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = ACROPOLIS_CAFETERIA_MOVIE_TASK_PRIORITY } },
      .callback = acropolisCafeteriaStartMovieTask,
      .data     = { .value = 0 } },
    { .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = ACROPOLIS_CAFETERIA_MOVIE_TASK_PRIORITY } },
      .callback = acropolisCafeteriaBlackoutTask,
      .data     = { .value = 0 } },
    { .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = ACROPOLIS_CAFETERIA_MOVIE_TASK_PRIORITY } },
      .callback = acropolisCafeteriaPlayMovieTask,
      .data     = { .value = 0 } },
};

#include "assets/rooms/akropolis/cafeteria/clips.inc"

TaskMessageEntry gAcropolisCafeteriaRoomEffectMessages[2] = {
    { ACROPOLIS_CAFETERIA_MESSAGE_SET_PUFF_ENABLED, acropolisCafeteriaSetPuffEnabled },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s32 gAcropolisCafeteriaPuffEnabled = 0;

#include "assets/rooms/akropolis/cafeteria/acropolis_cafeteria_model_077D8.inc"

#include "../../shared/room/room_visual_effects_trail_data.inc.c"
#undef ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS

static TmdBone _gAcropolisCafeteriaModel07CA8Skeleton[1] = {
#include "assets/acropolis_cafeteria_model_07CA8_skeleton.inc"
};

static u32 _gAcropolisCafeteriaModel07CA8PartVerts[1] = {
#include "assets/acropolis_cafeteria_model_07CA8_partVerts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel07CA8Verts[46] = {
#include "assets/acropolis_cafeteria_model_07CA8_verts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel07CA8Normals[72] = {
#include "assets/acropolis_cafeteria_model_07CA8_normals.inc"
};

static u32 _gAcropolisCafeteriaModel07CA8Stream[407] = {
#include "assets/acropolis_cafeteria_model_07CA8_stream.inc"
};

TmdSource gAcropolisCafeteriaModel07CA8 = {
    0,
    2952,
    0,
    1,
    _gAcropolisCafeteriaModel07CA8PartVerts,
    _gAcropolisCafeteriaModel07CA8Verts,
    _gAcropolisCafeteriaModel07CA8Normals,
    _gAcropolisCafeteriaModel07CA8Skeleton,
    _gAcropolisCafeteriaModel07CA8Stream,
};

static TmdBone _gAcropolisCafeteriaModel08638Skeleton[1] = {
#include "assets/acropolis_cafeteria_model_08638_skeleton.inc"
};

static u32 _gAcropolisCafeteriaModel08638PartVerts[1] = {
#include "assets/acropolis_cafeteria_model_08638_partVerts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel08638Verts[31] = {
#include "assets/acropolis_cafeteria_model_08638_verts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel08638Normals[62] = {
#include "assets/acropolis_cafeteria_model_08638_normals.inc"
};

static u32 _gAcropolisCafeteriaModel08638Stream[409] = {
#include "assets/acropolis_cafeteria_model_08638_stream.inc"
};

TmdSource gAcropolisCafeteriaModel08638 = {
    0,
    2880,
    0,
    1,
    _gAcropolisCafeteriaModel08638PartVerts,
    _gAcropolisCafeteriaModel08638Verts,
    _gAcropolisCafeteriaModel08638Normals,
    _gAcropolisCafeteriaModel08638Skeleton,
    _gAcropolisCafeteriaModel08638Stream,
};

static TmdBone _gAcropolisCafeteriaModel09090Skeleton[1] = {
#include "assets/acropolis_cafeteria_model_09090_skeleton.inc"
};

static u32 _gAcropolisCafeteriaModel09090PartVerts[1] = {
#include "assets/acropolis_cafeteria_model_09090_partVerts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel09090Verts[46] = {
#include "assets/acropolis_cafeteria_model_09090_verts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel09090Normals[71] = {
#include "assets/acropolis_cafeteria_model_09090_normals.inc"
};

static u32 _gAcropolisCafeteriaModel09090Stream[407] = {
#include "assets/acropolis_cafeteria_model_09090_stream.inc"
};

TmdSource gAcropolisCafeteriaModel09090 = {
    0,
    2952,
    0,
    1,
    _gAcropolisCafeteriaModel09090PartVerts,
    _gAcropolisCafeteriaModel09090Verts,
    _gAcropolisCafeteriaModel09090Normals,
    _gAcropolisCafeteriaModel09090Skeleton,
    _gAcropolisCafeteriaModel09090Stream,
};

static TmdBone _gAcropolisCafeteriaModel09A20Skeleton[1] = {
#include "assets/acropolis_cafeteria_model_09A20_skeleton.inc"
};

static u32 _gAcropolisCafeteriaModel09A20PartVerts[1] = {
#include "assets/acropolis_cafeteria_model_09A20_partVerts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel09A20Verts[37] = {
#include "assets/acropolis_cafeteria_model_09A20_verts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel09A20Normals[56] = {
#include "assets/acropolis_cafeteria_model_09A20_normals.inc"
};

static u32 _gAcropolisCafeteriaModel09A20Stream[334] = {
#include "assets/acropolis_cafeteria_model_09A20_stream.inc"
};

TmdSource gAcropolisCafeteriaModel09A20 = {
    0,
    2296,
    0,
    1,
    _gAcropolisCafeteriaModel09A20PartVerts,
    _gAcropolisCafeteriaModel09A20Verts,
    _gAcropolisCafeteriaModel09A20Normals,
    _gAcropolisCafeteriaModel09A20Skeleton,
    _gAcropolisCafeteriaModel09A20Stream,
};
#include "assets/rooms/akropolis/cafeteria/header.inc"
#include "assets/rooms/akropolis/cafeteria/geometry.inc"
#include "assets/rooms/akropolis/cafeteria/view_boundaries.inc"
#include "assets/rooms/akropolis/cafeteria/action_triggers.inc"
#include "assets/rooms/akropolis/cafeteria/occluders.inc"
#include "assets/rooms/akropolis/cafeteria/placements.inc"
#define ROOM_PART gAcropolisCafeteriaPointLights
#include "assets/rooms/akropolis/cafeteria/lights.inc"

STATIC_ASSERT(ARRAY_SIZE(_gAcropolisCafeteriaCollisionGridTable) == ACROPOLIS_CAFETERIA_COLLISION_CELLS_X * ACROPOLIS_CAFETERIA_COLLISION_CELLS_Z, cafeteriaCollisionCellTableSize);

enum {
    ACROPOLIS_CAFETERIA_RESOURCE_STRANGER      = 10,
    ACROPOLIS_CAFETERIA_RESOURCE_WOMAN         = 29,
    ACROPOLIS_CAFETERIA_RESOURCE_SCRIPTED_FORM = 19,
    ACROPOLIS_CAFETERIA_RESOURCE_RUPERT        = 102,
};

/// Runs the player debug hooks while the display's debug mode is nonzero.
///
/// Requires a live player task; the room task argument is unused. The debug
/// label borrows static storage for the lifetime of the cafeteria overlay.
static void _acropolisCafeteriaTickPlayerDebug(Task* task)
{
    if (gDisplayState.debugMode != 0) {
        func_80724608(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), -0x8C, -0x32, (void*)_gAcropolisCafeteriaPlayerDebugLabel);
        func_807245E4(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER));
    }
}

/// Selects the patio room for an executed departure from the cafeteria.
///
/// Borrows a readable request and an initialized writable reply, which may
/// alias. Queries and other destination areas leave the reply unchanged.
/// Before the cafeteria scene, opening progress selects patio room 1 or 2;
/// afterwards it selects room 3. Returns 1 to allow the transition.
static inline s32 _acropolisCafeteriaResolvePatioRoom(const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_CAFETERIA_OPENING_VARIANT_THRESHOLD = 2,
        ACROPOLIS_CAFETERIA_PATIO_ROOM_EARLY          = 1,
        ACROPOLIS_CAFETERIA_PATIO_ROOM_OPENING_PASSED = 2,
    };
    s32 destinationArea = request->areaId;

    if (destinationArea == GAME_AREA_AKROPOLIS_PATIO && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < ACROPOLIS_CAFETERIA_PROGRESS_AFTER_SCENE) {
            if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) < ACROPOLIS_CAFETERIA_OPENING_VARIANT_THRESHOLD) {
                reply->roomState = ACROPOLIS_CAFETERIA_PATIO_ROOM_EARLY;
            } else {
                reply->roomState = ACROPOLIS_CAFETERIA_PATIO_ROOM_OPENING_PASSED;
            }
        } else {
            reply->roomState = destinationArea;
        }
    }
    return 1;
}

/// Resolves cafeteria departures, gating the hallway and patio on scene progress.
///
/// Copies complete borrowed request/reply records, which may alias. Returns
/// 1 to allow departure or 0 to hold it. Execution of a blocked hallway warp-4
/// exit starts CAP sequence 5; a completed early battle gates patio departure
/// through sequence 6. Queries suppress those effects. Allowed patio exits
/// select their destination room from story progress. Receiver and ID are unused.
static s32 _acropolisCafeteriaResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_CAFETERIA_HALLWAY_EXIT_WARP     = 4,
        ACROPOLIS_CAFETERIA_HALLWAY_EXIT_PROGRESS = 3,
        ACROPOLIS_CAFETERIA_SCENE_NOT_STARTED     = 0,
        ACROPOLIS_CAFETERIA_SCENE_RELEASED        = 2,
    };

    enum {
        ACROPOLIS_CAFETERIA_HALLWAY_EXIT_SEQUENCE = 5,
        ACROPOLIS_CAFETERIA_PATIO_EXIT_SEQUENCE   = 6,
    };

    s32 destinationArea;

    *reply = *request;
    if (request->areaId == GAME_AREA_AKROPOLIS_HALLWAY && request->warp == ACROPOLIS_CAFETERIA_HALLWAY_EXIT_WARP) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) >= ACROPOLIS_CAFETERIA_HALLWAY_EXIT_PROGRESS) {
            return 1;
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            gameFlagSetNibbleIfPresent(request->flagId, 2);
            capStartSequenceSlot(ACROPOLIS_CAFETERIA_HALLWAY_EXIT_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
        }
        return 0;
    }
    destinationArea = request->areaId;
    if (destinationArea == GAME_AREA_AKROPOLIS_PATIO) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < ACROPOLIS_CAFETERIA_PROGRESS_AFTER_SCENE) {
            if (_gAcropolisCafeteriaSceneProgress == ACROPOLIS_CAFETERIA_SCENE_NOT_STARTED) {
                return _acropolisCafeteriaResolvePatioRoom(request, reply);
            }
            if (_gAcropolisCafeteriaSceneProgress == ACROPOLIS_CAFETERIA_SCENE_RELEASED) {
                if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                    capStartSequenceSlot(ACROPOLIS_CAFETERIA_PATIO_EXIT_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
                }
            }
            return 0;
        }
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_BOSS_REPORT) == destinationArea && request->queryOnly == ROOM_EVENT_EXECUTE) {
            gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_BOSS_REPORT, 2);
        }
        return _acropolisCafeteriaResolvePatioRoom(request, reply);
    }
    return 1;
}

/// Covers the centered 320-by-256 frame while the battle layout is replaced.
///
/// Requires a word-aligned frame primitive cursor with room for one `TILE`
/// and an ordering table with index 10. The opaque black packet is borrowed
/// until GPU drawing completes; screen coordinates are centered pixels.
static inline void _acropolisCafeteriaDrawBattleBlackout(void)
{
    enum { ACROPOLIS_CAFETERIA_BLACKOUT_OT_INDEX = 10 };
    TILE* tile;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    SetTile(tile);
    tile->x0 = -160;
    tile->y0 = -128;
    tile->w  = 320;
    tile->h  = 256;
    tile->r0 = 0;
    tile->g0 = 0;
    tile->b0 = 0;
    addPrim(&gGpuCurrentOt[ACROPOLIS_CAFETERIA_BLACKOUT_OT_INDEX], tile);
}

/// Runs the cafeteria introduction, hidden layout swap, battle and aftermath.
///
/// Starts bodyless in state 0 with loaded room/gameplay resources. Keeps the
/// frame black while rebuilding room 2, waits for the enemy to leave while the
/// player is alive and outside the wheel/display transition, then releases battle
/// rewards and player control. States 9..13 have no handler; normal sequencing
/// jumps from 8 to 14. The final state waits for the player's animation to end
/// before killing the task. CD queue admission and script-spawn results are ignored.
static void _acropolisCafeteriaBattleSceneTask(Task* task)
{
    enum {
        ACROPOLIS_CAFETERIA_BATTLE_START_INTRO              = 0,
        ACROPOLIS_CAFETERIA_BATTLE_WAIT_INTRO               = 1,
        ACROPOLIS_CAFETERIA_BATTLE_SELECT_BATTLE_ROOM       = 2,
        ACROPOLIS_CAFETERIA_BATTLE_EXIT_PLACED_ACTORS       = 3,
        ACROPOLIS_CAFETERIA_BATTLE_WAIT_ACTOR_EXIT          = 4,
        ACROPOLIS_CAFETERIA_BATTLE_RESTORE_VIEW             = 5,
        ACROPOLIS_CAFETERIA_BATTLE_LOAD_ACTOR_FILE          = 6,
        ACROPOLIS_CAFETERIA_BATTLE_SPAWN_BATTLE_LAYOUT      = 7,
        ACROPOLIS_CAFETERIA_BATTLE_START_BATTLE_CONTROLLER  = 8,
        ACROPOLIS_CAFETERIA_BATTLE_WAIT_BATTLE_SETUP        = 14,
        ACROPOLIS_CAFETERIA_BATTLE_START_BATTLE_SCRIPT      = 15,
        ACROPOLIS_CAFETERIA_BATTLE_WAIT_BATTLE_SCRIPT       = 16,
        ACROPOLIS_CAFETERIA_BATTLE_DELAY_BATTLE_CHECK       = 17,
        ACROPOLIS_CAFETERIA_BATTLE_WAIT_ENEMY_DEFEAT        = 18,
        ACROPOLIS_CAFETERIA_BATTLE_START_POST_BATTLE_SCRIPT = 19,
        ACROPOLIS_CAFETERIA_BATTLE_WAIT_POST_BATTLE_SCRIPT  = 20,
        ACROPOLIS_CAFETERIA_BATTLE_RELEASE_BATTLE           = 21,
        ACROPOLIS_CAFETERIA_BATTLE_DELAY_1                  = 22,
        ACROPOLIS_CAFETERIA_BATTLE_DELAY_2                  = 23,
        ACROPOLIS_CAFETERIA_BATTLE_DELAY_3                  = 24,
        ACROPOLIS_CAFETERIA_BATTLE_DELAY_4                  = 25,
        ACROPOLIS_CAFETERIA_BATTLE_DELAY_5                  = 26,
        ACROPOLIS_CAFETERIA_BATTLE_WAIT_PLAYER_ANIMATION    = 27,
    };

    u8 fileKeyBytes[4];
    u8 loadArgsBytes[4];
    u8 drawBlackout;

    drawBlackout = 0;
    switch (task->state) {
        case ACROPOLIS_CAFETERIA_BATTLE_START_INTRO:
            gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
            evsStartScriptWithSkip(_gAcropolisCafeteriaOpeningSceneScript, EVENT_SCRIPT_HUD_KEEP, _gAcropolisCafeteriaOpeningSceneSkipScript);
            task->state += 1;
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_WAIT_INTRO:
            if (gGameSession->eventState != 1) {
                task->state += 1;
            }
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_SELECT_BATTLE_ROOM:
            drawBlackout                     = 1;
            gGameSession->location.roomState = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = 2;
            gGameSession->roomObjsDirty                                                                    = 1;
            task->state                                                                                   += 1;
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_EXIT_PLACED_ACTORS:
            drawBlackout = 1;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_EXIT_PLACED_ACTORS, 0, 0);
            task->state += 1;
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_WAIT_ACTOR_EXIT:
            drawBlackout = 1;
            task->state += 1;
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_RESTORE_VIEW:
            drawBlackout = 1;
            loadingRequestViewGraphicsRestore();
            task->state += 1;
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_LOAD_ACTOR_FILE:
            drawBlackout = 1;
            // Stage-zero file 210600; byte 1 of the key is ignored by the CD API.
            fileKeyBytes[2]  = 0x15;
            fileKeyBytes[3]  = 0;
            fileKeyBytes[0]  = 0;
            loadArgsBytes[0] = 6;
            loadArgsBytes[1] = CD_COMMAND_LOAD_DEFAULT;
            loadArgsBytes[2] = 4;
            loadArgsBytes[3] = 6;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKeyBytes, loadArgsBytes);
            task->state += 1;
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_SPAWN_BATTLE_LAYOUT:
            drawBlackout = 1;
            if (cdCmdIsIdle()) {
                areaSetPlacementVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, 2, AREA_VARIANT_RESET_ALWAYS);
                areaSyncLocationVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location);
                areaSpawnPlacements(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location);
                gCapControlFlags &= ~CAP_CONTROL_EVENT_SCRIPT_WAITING_FOR_CUE;
                task->state      += 1;
            }
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_START_BATTLE_CONTROLLER:
            drawBlackout = 1;
            if (gDisplayState.debugMode != -1) {
                taskSpawnFromTable(gAcropolisCafeteriaMovieTasks, ACROPOLIS_CAFETERIA_MOVIE_TASK_LAUNCH, 0, 0);
            }
            task->state = ACROPOLIS_CAFETERIA_BATTLE_WAIT_BATTLE_SETUP;
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_WAIT_BATTLE_SETUP:
            drawBlackout = 1;
            if (gGameSession->eventState == 0) {
                task->state += 1;
            }
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_START_BATTLE_SCRIPT:
            evsStartScript(_gAcropolisCafeteriaBattleIntroScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            task->state += 1;
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_WAIT_BATTLE_SCRIPT:
        case ACROPOLIS_CAFETERIA_BATTLE_WAIT_POST_BATTLE_SCRIPT:
            if (gGameSession->eventState == 0) {
                task->state += 1;
            }
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_WAIT_ENEMY_DEFEAT:
            if (taskMessageDispatch(sceneFindPlacedActor(ACROPOLIS_CAFETERIA_PLACED_STRANGER), ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0 && gPlayerStatus.hp > 0 && gAttachmentState.mode != ATTACHMENT_MODE_WHEEL &&
                gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                task->state += 1;
            }
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_START_POST_BATTLE_SCRIPT:
            displaySetShakeY(0);
            evsStartScriptWithSkip(_gAcropolisCafeteriaPostBattleSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisCafeteriaPostBattleSceneSkipScript);
            task->state += 1;
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_RELEASE_BATTLE:
            // Release combat before returning control and the next objective.
            sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(ACROPOLIS_CAFETERIA_PLACED_STRANGER), 0xA);
            gGameSession->flowFlags                       |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
            gSceneCombatState.signals.bytes.endDelayFrames = 3;
            _gAcropolisCafeteriaSceneProgress              = ACROPOLIS_CAFETERIA_PROGRESS_AFTER_SCENE;
            task->state                                   += 1;
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_DELAY_BATTLE_CHECK:
        case ACROPOLIS_CAFETERIA_BATTLE_DELAY_1:
        case ACROPOLIS_CAFETERIA_BATTLE_DELAY_2:
        case ACROPOLIS_CAFETERIA_BATTLE_DELAY_3:
        case ACROPOLIS_CAFETERIA_BATTLE_DELAY_4:
        case ACROPOLIS_CAFETERIA_BATTLE_DELAY_5:
            task->state += 1;
            break;
        case ACROPOLIS_CAFETERIA_BATTLE_WAIT_PLAYER_ANIMATION:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 3);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            }
            break;
    }
    if (drawBlackout != 0) {
        _acropolisCafeteriaDrawBattleBlackout();
    }
}

/// Commits the cafeteria follow-up scene and hands its patio destination to reload.
///
/// Requires the live save, loaded area updates and a live bodyless controller.
/// Applies progress and placement changes before stopping sound and starting
/// reload. Releases the controller last; reload owns the subsequent handoff.
static inline void _acropolisCafeteriaCommitFollowUpDeparture(Task* task)
{
    enum {
        ACROPOLIS_CAFETERIA_FOLLOW_UP_SCENE_EVENT    = 4,
        ACROPOLIS_CAFETERIA_FOLLOW_UP_OBJECTIVE      = 4,
        ACROPOLIS_CAFETERIA_FOLLOW_UP_PATIO_WARP     = 3,
        ACROPOLIS_CAFETERIA_FOLLOW_UP_PATIO_ROOM     = 3,
        ACROPOLIS_CAFETERIA_FOLLOW_UP_SPRITE_VARIANT = 1,
    };
    // Commit the completed scene before handing the saved destination to the reload task.
    gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS, ACROPOLIS_CAFETERIA_PROGRESS_AFTER_SCENE);
    gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
    gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_NEW_SPECIES);
    gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_BOSS_REPORT, 1);
    areaApplySavedUpdates(gAcropolisCafeteriaFollowUpAreaUpdates);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACROPOLIS_CAFETERIA_FOLLOW_UP_SCENE_EVENT;
    gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACROPOLIS_CAFETERIA_FOLLOW_UP_OBJECTIVE);
    func_800ABFF8();
    func_800AC000();
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_AKROPOLIS_PATIO;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = ACROPOLIS_CAFETERIA_FOLLOW_UP_PATIO_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACROPOLIS_CAFETERIA_FOLLOW_UP_PATIO_ROOM;
    gDisplayState.spriteVariant                                 = ACROPOLIS_CAFETERIA_FOLLOW_UP_SPRITE_VARIANT;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    taskKill(task);
}

/// Runs the post-battle interaction and its accepted follow-up scene before returning to the patio.
///
/// Starts bodyless in state 0 after the cafeteria battle. Placement 3 state 1
/// permits CAP command 3 between the player's presentation clips. If that
/// placement remains at 1, resumes play and ends; otherwise runs a skippable
/// scene, commits story and area updates, and reloads patio room/warp 3.
/// Requires a live player and loaded room scripts through the reload handoff.
static void _acropolisCafeteriaFollowUpSceneTask(Task* task)
{
    enum {
        ACROPOLIS_CAFETERIA_FOLLOW_UP_CHECK             = 0,
        ACROPOLIS_CAFETERIA_FOLLOW_UP_WAIT_PRESENTATION = 1,
        ACROPOLIS_CAFETERIA_FOLLOW_UP_WAIT_CAP          = 2,
        ACROPOLIS_CAFETERIA_FOLLOW_UP_WAIT_RESUME       = 3,
        ACROPOLIS_CAFETERIA_FOLLOW_UP_START_SCENE       = 6,
        ACROPOLIS_CAFETERIA_FOLLOW_UP_WAIT_SCENE        = 7,
        ACROPOLIS_CAFETERIA_FOLLOW_UP_DEPART            = 8,
        ACROPOLIS_CAFETERIA_FOLLOW_UP_PLACEMENT_ID      = 3,
        ACROPOLIS_CAFETERIA_FOLLOW_UP_CAP_COMMAND       = 3,
        ACROPOLIS_CAFETERIA_PRESENTATION_ENTER          = 0,
        ACROPOLIS_CAFETERIA_PRESENTATION_LEAVE          = 1,
        ACROPOLIS_CAFETERIA_EVENT_RUNNING               = 1
    };
    // Unused automatic storage retains the scene task's original stack frame.
    char unusedStack[8];

    switch (task->state) {
        case ACROPOLIS_CAFETERIA_FOLLOW_UP_CHECK:
            if (areaGetCurrentObjectState(ACROPOLIS_CAFETERIA_FOLLOW_UP_PLACEMENT_ID) == 1) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION, ACROPOLIS_CAFETERIA_PRESENTATION_ENTER, 0);
                task->state = task->state + 1;
            } else {
                taskKill(task);
            }
            break;

        case ACROPOLIS_CAFETERIA_FOLLOW_UP_WAIT_PRESENTATION:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                capRunCommandWithTransition(ACROPOLIS_CAFETERIA_FOLLOW_UP_CAP_COMMAND);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_CAFETERIA_FOLLOW_UP_WAIT_CAP:
            if (capIsBusy() == 0) {
                if (areaGetCurrentObjectState(ACROPOLIS_CAFETERIA_FOLLOW_UP_PLACEMENT_ID) == 1) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION, ACROPOLIS_CAFETERIA_PRESENTATION_LEAVE, 0);
                    task->state = task->state + 1;
                } else {
                    task->state = ACROPOLIS_CAFETERIA_FOLLOW_UP_START_SCENE;
                }
            }
            break;

        case ACROPOLIS_CAFETERIA_FOLLOW_UP_WAIT_RESUME:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            }
            break;

        case ACROPOLIS_CAFETERIA_FOLLOW_UP_START_SCENE:
            evsStartScriptWithSkip(_gAcropolisCafeteriaFollowUpSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisCafeteriaFollowUpSceneSkipScript);
            task->state = task->state + 1;
            break;

        case ACROPOLIS_CAFETERIA_FOLLOW_UP_WAIT_SCENE:
            if (gGameSession->eventState != ACROPOLIS_CAFETERIA_EVENT_RUNNING) {
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_CAFETERIA_FOLLOW_UP_DEPART:
            _acropolisCafeteriaCommitFollowUpDeparture(task);
            break;
    }
}

/// Integrates one recoil tick and releases its controller when the root settles.
///
/// Velocities use room units per callback tick; gravity uses units per tick
/// squared. Signed division damps X/Z toward zero, including negative values.
/// Borrows the live model root and bodyless controller; only one controller may
/// use the singleton velocities at a time. Positive Y points down. Clamps to
/// floor Y before damping, then kills only the controller once X/Z are zero
/// and the root is on the floor. Marks the root's composition cache stale.
static inline void _acropolisCafeteriaStepStrangerRecoil(Task* task, GfxCoord* root)
{
    enum {
        ACROPOLIS_CAFETERIA_STRANGER_FLOOR_Y        = -300,
        ACROPOLIS_CAFETERIA_STRANGER_GRAVITY        = 5,
        ACROPOLIS_CAFETERIA_STRANGER_MAX_FALL_SPEED = 20,
    };

    root->coord.t[0] += gAcropolisCafeteriaStrangerRecoilVelocityX;
    root->coord.t[1] += gAcropolisCafeteriaStrangerRecoilVelocityY;
    if (root->coord.t[1] > ACROPOLIS_CAFETERIA_STRANGER_FLOOR_Y) {
        root->coord.t[1] = ACROPOLIS_CAFETERIA_STRANGER_FLOOR_Y;
    }
    root->coord.t[2]                           += gAcropolisCafeteriaStrangerRecoilVelocityZ;
    root->composeStamp                          = GRAPHICS_COORD_DIRTY;
    gAcropolisCafeteriaStrangerRecoilVelocityX  = gAcropolisCafeteriaStrangerRecoilVelocityX / 2;
    gAcropolisCafeteriaStrangerRecoilVelocityY += ACROPOLIS_CAFETERIA_STRANGER_GRAVITY;
    if (gAcropolisCafeteriaStrangerRecoilVelocityY > ACROPOLIS_CAFETERIA_STRANGER_MAX_FALL_SPEED) {
        gAcropolisCafeteriaStrangerRecoilVelocityY = ACROPOLIS_CAFETERIA_STRANGER_MAX_FALL_SPEED;
    }
    gAcropolisCafeteriaStrangerRecoilVelocityZ = gAcropolisCafeteriaStrangerRecoilVelocityZ / 2;
    if (gAcropolisCafeteriaStrangerRecoilVelocityX == 0 && gAcropolisCafeteriaStrangerRecoilVelocityZ == 0 &&
        root->coord.t[1] == ACROPOLIS_CAFETERIA_STRANGER_FLOOR_Y) {
        taskKill(task);
    }
}

/// Settles the cafeteria Stranger after the final scripted shot.
///
/// Requires placed actor 0's live TMD root and the cafeteria overlay to remain
/// loaded. State 0 places the actor, selects its recoil animation and initializes
/// singleton X/Y/Z velocities; state 1 steps them until X/Z stop and Y reaches
/// the floor at -300 room units, then kills only this bodyless controller.
/// Only one such controller may use the shared velocities at a time.
static void _acropolisCafeteriaSettleStrangerTask(Task* task)
{
    enum {
        ACROPOLIS_CAFETERIA_SETTLE_INITIALIZE       = 0,
        ACROPOLIS_CAFETERIA_SETTLE_MOVE             = 1,
        ACROPOLIS_CAFETERIA_STRANGER_INITIAL_Y_STEP = -20,
        ACROPOLIS_CAFETERIA_STRANGER_INITIAL_Z_STEP = -20,
    };
    GfxCoord* root;

    root = sceneFindPlacedActor(ACROPOLIS_CAFETERIA_PLACED_STRANGER)->extra.tmd->coords;
    switch (task->state) {
        case ACROPOLIS_CAFETERIA_SETTLE_INITIALIZE:
            // Start the recoil clip before taking over the root's translation.
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(ACROPOLIS_CAFETERIA_PLACED_STRANGER), ACTOR_MESSAGE_PLACE, &_gAcropolisCafeteriaStrangerRecoilPlacement, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(ACROPOLIS_CAFETERIA_PLACED_STRANGER), ACTOR_COMMAND_MESSAGE_APPLY, &_gAcropolisCafeteriaStrangerSettleCommand, 0);
            gAcropolisCafeteriaStrangerRecoilVelocityX = 0;
            gAcropolisCafeteriaStrangerRecoilVelocityY = ACROPOLIS_CAFETERIA_STRANGER_INITIAL_Y_STEP;
            gAcropolisCafeteriaStrangerRecoilVelocityZ = ACROPOLIS_CAFETERIA_STRANGER_INITIAL_Z_STEP;
            task->state                                = task->state + 1;
            break;

        case ACROPOLIS_CAFETERIA_SETTLE_MOVE:
            _acropolisCafeteriaStepStrangerRecoil(task, root);
            break;
    }
}

/// Refuses every key-item use requested at the cafeteria room task.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are unused. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED`, so the menu presents its refusal notice.
static s32 _acropolisCafeteriaRefuseKeyItemMsg(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Starts CAP sequence 7 for room command 7 after the cafeteria scene is ready.
///
/// Requires saved or live scene progress at least 2 and placement 4 in state
/// 0 or 1. Other commands do nothing. Remaining arguments are unused; returns 0.
static s32 _acropolisCafeteriaHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg)
{
    enum {
        ACROPOLIS_CAFETERIA_COMMAND_SEQUENCE_7 = 7,
        ACROPOLIS_CAFETERIA_CAP_PLACEMENT_ID   = 4
    };
    if (commandId == ACROPOLIS_CAFETERIA_COMMAND_SEQUENCE_7) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) >= ACROPOLIS_CAFETERIA_PROGRESS_AFTER_SCENE || _gAcropolisCafeteriaSceneProgress >= ACROPOLIS_CAFETERIA_PROGRESS_AFTER_SCENE) {
            if (areaGetCurrentObjectState(ACROPOLIS_CAFETERIA_CAP_PLACEMENT_ID) == 1 || areaGetCurrentObjectState(ACROPOLIS_CAFETERIA_CAP_PLACEMENT_ID) == 0) {
                capStartSequenceSlot(ACROPOLIS_CAFETERIA_COMMAND_SEQUENCE_7, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
            }
        }
    }
    return 0;
}
/// Routes directed cafeteria actions to the battle, follow-up scene or CAP command 9.
///
/// Borrows a four-byte request through synchronous dispatch. Action 0 starts
/// the follow-up task once saved or live scene progress reaches 2; action 2
/// runs CAP command 9; action 3 starts the battle once while saved progress is
/// 1 and the live scene latch is zero. No payload is retained. Other arguments
/// are unused; returns 0. Requires the room task descriptors and scripts loaded.
static s32 _acropolisCafeteriaHandleActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unusedArg)
{
    enum {
        ACROPOLIS_CAFETERIA_ACTION_FOLLOW_UP      = 0,
        ACROPOLIS_CAFETERIA_ACTION_CAP_COMMAND    = 2,
        ACROPOLIS_CAFETERIA_ACTION_START_BATTLE   = 3,
        ACROPOLIS_CAFETERIA_TASK_BATTLE           = 0,
        ACROPOLIS_CAFETERIA_TASK_FOLLOW_UP        = 1,
        ACROPOLIS_CAFETERIA_ACTION_CAP_COMMAND_ID = 9,
        ACROPOLIS_CAFETERIA_PROGRESS_BEFORE_SCENE = 1
    };

    if (actionRequest->actionId == ACROPOLIS_CAFETERIA_ACTION_FOLLOW_UP) {
        if (_gAcropolisCafeteriaSceneProgress >= ACROPOLIS_CAFETERIA_PROGRESS_AFTER_SCENE || gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) >= ACROPOLIS_CAFETERIA_PROGRESS_AFTER_SCENE) {
            taskSpawnFromTable(gAcropolisCafeteriaSceneTasks, ACROPOLIS_CAFETERIA_TASK_FOLLOW_UP, 0, 0);
            return 0;
        }
    }
    if (actionRequest->actionId == ACROPOLIS_CAFETERIA_ACTION_CAP_COMMAND) {
        capRunCommandWithTransition(ACROPOLIS_CAFETERIA_ACTION_CAP_COMMAND_ID);
    } else if (actionRequest->actionId == ACROPOLIS_CAFETERIA_ACTION_START_BATTLE) {
        if (_gAcropolisCafeteriaSceneProgress == ACROPOLIS_CAFETERIA_SCENE_NOT_STARTED && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == ACROPOLIS_CAFETERIA_PROGRESS_BEFORE_SCENE) {
            _gAcropolisCafeteriaSceneProgress = ACROPOLIS_CAFETERIA_SCENE_ACTIVE;
            taskSpawnFromTable(gAcropolisCafeteriaSceneTasks, ACROPOLIS_CAFETERIA_TASK_BATTLE, 0, 0);
        }
    }
    return 0;
}
/// Maps cafeteria room sound cues 10 and 11 to the same entries in its area bank.
///
/// Handles `ROOM_MESSAGE_SOUND`; other cues do nothing. Task, messageId and
/// unusedArg are ignored. Requests use zero pan offset and attenuation, and
/// return 0 regardless of sound-queue admission.
static s32 _acropolisCafeteriaPlaySoundMsg(Task* task, s32 messageId, s32 soundCue, s32 unusedArg)
{
    enum {
        ACROPOLIS_CAFETERIA_SOUND_CUE_10 = 10,
        ACROPOLIS_CAFETERIA_SOUND_CUE_11 = 11,
    };

    switch (soundCue) {
        case ACROPOLIS_CAFETERIA_SOUND_CUE_10:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA, ACROPOLIS_CAFETERIA_SOUND_CUE_10), 0, 0);
            break;
        case ACROPOLIS_CAFETERIA_SOUND_CUE_11:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA, ACROPOLIS_CAFETERIA_SOUND_CUE_11), 0, 0);
            break;
    }
    return 0;
}
/// Overrides the live player's room surface index for the scripted footsteps.
///
/// The event callback passes a complete signed word, stored without narrowing.
/// Requires live `GameActor` work and a surface index in 0..7 for the cafeteria's
/// surface table; the opening script supplies 2. Does not retain a pointer.
static void _acropolisCafeteriaSetPlayerSurface(s32 surfaceClass)
{
    GameActor* player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;

    player->surfaceClass = surfaceClass;
}

/// Requests parasite-energy effect cancellation before the opening room scene.
///
/// Requires live room-effect state. Cancellation is deferred until its next
/// update; the event callback ignores its argument register.
static void _acropolisCafeteriaCancelPeEffects(void)
{
    roomEffectRequestCancelPe();
}

/// Copies the two authored VRAM regions used by the post-battle scene.
///
/// Requires initialized GPU transfer state and intact source VRAM. Copies a
/// 64-word by 256-row image and then a 256-word row to their scene destinations;
/// X coordinates and widths count 16-bit VRAM words, Y coordinates count rows.
/// Queues both transfers without waiting for completion. The event callback
/// ignores its argument register.
static void _acropolisCafeteriaCopyPostBattleVram(void)
{
    enum {
        ACROPOLIS_CAFETERIA_POST_BATTLE_IMAGE_X = 384,
        ACROPOLIS_CAFETERIA_POST_BATTLE_IMAGE_Y = 256,
        ACROPOLIS_CAFETERIA_POST_BATTLE_ROW_X   = 0,
        ACROPOLIS_CAFETERIA_POST_BATTLE_ROW_Y   = 247,
    };

    MoveImage(&_gAcropolisCafeteriaPostBattleImageSource, ACROPOLIS_CAFETERIA_POST_BATTLE_IMAGE_X, ACROPOLIS_CAFETERIA_POST_BATTLE_IMAGE_Y);
    MoveImage(&_gAcropolisCafeteriaPostBattleRowSource, ACROPOLIS_CAFETERIA_POST_BATTLE_ROW_X, ACROPOLIS_CAFETERIA_POST_BATTLE_ROW_Y);
}

/// Requests effect cancellation and locks attachment activation for the post-battle scene.
///
/// Requires live room-effect state. Cancellation is deferred to its next
/// update; the attachment event lock is set immediately and left for scene
/// cleanup to clear. The event callback ignores its argument register.
static void _acropolisCafeteriaPreparePostBattleScene(void)
{
    roomEffectRequestCancelAll();
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Registers the cafeteria room task and restores its progress-dependent actors.
///
/// Requires the live scene task and current room resources. At story progress 1,
/// hides the boss and shows the scripted form; at 2, shows the boss and Rupert,
/// suspends the scripted form, disables the CAP 20 trigger and places Rupert.
/// Other progress values leave actors and triggers unchanged. Installs the
/// borrowed message table, publishes the task in the room slot and advances
/// to state 1.
static void _acropolisCafeteriaInitRoom(Task* task)
{
    enum {
        ACROPOLIS_CAFETERIA_PROGRESS_BEFORE_SCENE = 1,
        ACROPOLIS_CAFETERIA_PLACED_SCRIPTED_FORM  = 1,
        ACROPOLIS_CAFETERIA_PLACED_RUPERT         = 2,
        ACROPOLIS_CAFETERIA_DRAW_HIDE             = 0,
        ACROPOLIS_CAFETERIA_DRAW_SHOW             = 1,
        ACROPOLIS_CAFETERIA_DRAW_SUSPEND          = 2,
        ACROPOLIS_CAFETERIA_CAP_20_TRIGGER_INDEX  = 9,
    };
    WorldCollisionTrigger* disabledTrigger;

    task->msgTable = _gAcropolisCafeteriaRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == ACROPOLIS_CAFETERIA_PROGRESS_BEFORE_SCENE) {
        sceneSetPlacedActorDrawMode(ACROPOLIS_CAFETERIA_PLACED_STRANGER, ACROPOLIS_CAFETERIA_DRAW_HIDE);
        sceneSetPlacedActorDrawMode(ACROPOLIS_CAFETERIA_PLACED_SCRIPTED_FORM, ACROPOLIS_CAFETERIA_DRAW_SHOW);
    } else if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == ACROPOLIS_CAFETERIA_PROGRESS_AFTER_SCENE) {
        sceneSetPlacedActorDrawMode(ACROPOLIS_CAFETERIA_PLACED_STRANGER, ACROPOLIS_CAFETERIA_DRAW_SHOW);
        sceneSetPlacedActorDrawMode(ACROPOLIS_CAFETERIA_PLACED_SCRIPTED_FORM, ACROPOLIS_CAFETERIA_DRAW_SUSPEND);
        sceneSetPlacedActorDrawMode(ACROPOLIS_CAFETERIA_PLACED_RUPERT, ACROPOLIS_CAFETERIA_DRAW_SHOW);
        disabledTrigger         = &_gAcropolisCafeteriaRoom1ActionTriggers[ACROPOLIS_CAFETERIA_CAP_20_TRIGGER_INDEX];
        disabledTrigger->flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(ACROPOLIS_CAFETERIA_PLACED_RUPERT), ACTOR_MESSAGE_PLACE, &_gAcropolisCafeteriaRupertConversationPlacement, 0);
    }
    task->state = task->state + 1;
}

void acropolisCafeteriaRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gAcropolisCafeteriaRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}
