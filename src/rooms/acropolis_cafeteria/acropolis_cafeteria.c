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

#define ROOM_PART _gAcropolisCafeteriaPlayerAnimation1
#include "assets/rooms/akropolis/cafeteria/events.inc"
/// One borrowed player clip installed at bank slot 47 for the opening scene.
///
/// The copy spans exactly this pointer array. The woman's animation overlay
/// must remain loaded while the player's bank or playback retains its clip.
static AnimationSet* _gAcropolisCafeteriaOpeningPlayerAnimationSets[1] = {
    &gAkropolisCafeteriaPlayerCafeteriaOpeningClip,
};

#define ROOM_PART _gAcropolisCafeteriaOpeningPlayerBankCopy
#include "assets/rooms/akropolis/cafeteria/events.inc"
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

#define ROOM_PART _gAcropolisCafeteriaFollowUpPlayerBankCopy
#include "assets/rooms/akropolis/cafeteria/events.inc"
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
