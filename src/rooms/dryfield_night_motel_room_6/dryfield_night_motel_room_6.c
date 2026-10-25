#include "rooms/dryfield_night_motel_room_6.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "gameplay/companion_load.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_cutscene.h"
#include "../../shared/room/room_variants.h"

static void _roomCutsceneTask(Task* task);

static s32 _roomVariantMotelBalconyMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

/// Singleton playback configuration for motel room 6's command-22 cutscene.
///
/// Selects view 12 and CAP slot 1. Story chapter and stage select the CAP file
/// and texture-page X; texture-page Y retains its zero-initialized value.
/// The scene runner borrows this writable record through completion. Keep the
/// overlay loaded and do not reconfigure it while that runner is using it.
static RoomCutsceneRec _gMotelRoom6CutsceneRec;

/// Selects the nighttime motel room 6 glow export.
///
/// Keep this binding through all motel room 6 implementation fragments.
#define DRYFIELD_TIME DRYFIELD_NIGHT
#include "../../shared/motel_room_6.h"

static s32 _motelRoom6CutsceneMessage(Task* task, s32 messageId, s32 command, s32 secondArg);

static s32 _dryfieldNightMotelRoom6HandleCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);

static void _roomCutsceneSoundTask(Task* task);

/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType` (ally present), read through its own symbol.

/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view` as it was when the cutscene started, restored
/// when it ends.

/// Area-record patch list applied when the cutscene advances the story flags.

/// The sound task the cutscene task spawned, killed when the player skips the
/// scene.
extern Task* gRoomCutsceneSoundTask;

#include "../../shared/room/telephone.h"

/// Defines the reflection scale at the shared implementation's include position.
///
/// 1 lets `planar_reflection.inc.c` include `planar_reflection_rodata.inc.c`.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 1
#include "../../shared/room/planar_reflection.h"

static void _dryfieldNightMotelRoom6InitRoomTask(Task* task);
static void _dryfieldNightMotelRoom6IdleTask(Task* unusedTask);

static WorldCollisionGrid    _gDryfieldNightMotelRoom6CollisionGrid;
static WorldCollisionTrigger _gDryfieldNightMotelRoom6ViewBoundaryTriggers[10];
static WorldCollisionTrigger _gDryfieldNightMotelRoom6ActionTriggers[15];
static WorldCoordRoomLights  _gDryfieldNightMotelRoom6Lights;
static s32                   _dryfieldNightMotelRoom6RejectKeyItemMessage(Task* receiver, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32                   _dryfieldNightMotelRoom6IgnoreActionMessage(Task* receiver, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32                   _dryfieldNightMotelRoom6SoundMessage(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);
static void                  _dryfieldNightMotelRoom6RestTask(Task* task);

#include "../../shared/room/telephone_data.inc.c"

#include "../../shared/room/planar_reflection_data.inc.c"

TaskDesc gDryfieldNightMotelRoom6ReflectionTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelRoom6PlayerReflectionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_EFFECT } }, _planarReflectionAttachmentTask, { .value = 0 } },
};

/// Borrows this overlay's two reflection task descriptors.
///
/// Slot 0 spawns the player reflection; slot 1 spawns an attachment or equipment
/// reflection. There is no terminator. The table and its callbacks remain valid
/// while the overlay is loaded; the caller neither owns nor copies the table.
static inline TaskDesc* _planarReflectionGetTaskTable(void)
{
    return gDryfieldNightMotelRoom6ReflectionTaskDescs;
}

/// This carrier's bodyless scene runner and timed sound task, followed by END.
///
/// Entry SCENE borrows a RoomCutsceneRec in spawnArg2 and takes the follow-up
/// CAP command in spawnArg1; SOUND takes a packed sound request in spawnArg2.
/// Both run at priority 32. Keep callback code and the scene record loaded
/// until completion; spawning copies the recipe and retains no table pointer.
static TaskDesc _gRoomCutsceneTaskDescs[ROOM_CUTSCENE_TASK_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Synchronous message handlers borrowed by the room controller until it dies.
///
/// Resolves departures, refuses key items, ignores room actions, selects
/// command/cutscene playback and routes sound cues. The final reserved ID
/// makes unsupported messages return zero; callback payload types follow each ID.
static const TaskMessageEntry _gDryfieldNightMotelRoom6RoomMessages[6] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantMotelBalconyMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightMotelRoom6RejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightMotelRoom6IgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, _motelRoom6CutsceneMessage },
    { ROOM_MESSAGE_SOUND, _dryfieldNightMotelRoom6SoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless priority-32 recipe for the refueling-dependent rest interaction.
///
/// Command 6 spawns this single entry at slot zero. Spawn payloads are ignored;
/// keep the room callback loaded until the rest task completes.
static TaskDesc _gDryfieldNightMotelRoom6RestTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_SCENE } },
    .callback = _dryfieldNightMotelRoom6RestTask,
    .data     = { .value = 0 },
};

/// Unreferenced bodyless priority-192 recipe for the same rest task.
///
/// No current caller selects this retained descriptor; its intended dispatch
/// path is unproven. Payloads would be ignored by the callback.
static TaskDesc _gDryfieldNightMotelRoom6RestPriority192TaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    .callback = _dryfieldNightMotelRoom6RestTask,
    .data     = { .value = 0 },
};

/// Fixed world-space centre of motel room 6's cyan glow, in game-coordinate units.
///
/// The glow drawers borrow this point for one frame and do not modify it.
static const SVECTOR _gMotelRoom6GlowPosition SECTION(".data") = { 550, -850, 5170, 0 };

WorldCoordRoomLighting gDryfieldNightMotelRoom6RoomLighting[1] = {
    { .lights = &_gDryfieldNightMotelRoom6Lights, .ambientTable = NULL },
};

WorldCollisionRoomResources gDryfieldNightMotelRoom6RoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldNightMotelRoom6CollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightMotelRoom6ViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightMotelRoom6ActionTriggers,
        .occluders            = NULL,
    },
};

u8* gDryfieldNightMotelRoom6ViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldNightMotelRoom6ViewCounts[1] = { ARRAY_SIZE(gDryfieldNightMotelRoom6ViewCameras) };

/// Map nibble marked on balcony arrival, and current/destination warp-slot pairs.
enum {
    DRYFIELD_NIGHT_MOTEL_ROOM_6_BALCONY_MAP_FLAG       = 465,
    DRYFIELD_NIGHT_MOTEL_ROOM_6_BALCONY_WARP_SLOTS     = (1 << 4) | 3,
    DRYFIELD_NIGHT_MOTEL_ROOM_6_WATER_TOWER_WARP_SLOTS = (2 << 4) | 4,
};

DirectionWarpEntry gDryfieldNightMotelRoom6WarpEntries[2] = {
    {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 4350, 0, 1500 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 4350, 0, 1500 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_MOTEL_ROOM_6_BALCONY_MAP_FLAG,
    },
    {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 715, 0, 6640 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 715, 0, 6640 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6, 3),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 5,
        .flags          = DIRECTION_WARP_FLAG_FADE_DEPARTURE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Room-space face normals for the motel room 6 grid, with 4096 per unit.
static SVECTOR _gDryfieldNightMotelRoom6CollisionGridNormals[24] = {
#include "assets/dryfield_night_motel_room_6_collision_063C4_normals.inc"
};

/// Motel room 6 grid vertices in signed room-coordinate units.
static SVECTOR _gDryfieldNightMotelRoom6CollisionGridVerts[158] = {
#include "assets/dryfield_night_motel_room_6_collision_063C4_verts.inc"
};

/// Indexed grid faces carrying room-local collision surface classes 0..3.
static WorldCollisionGridFace _gDryfieldNightMotelRoom6CollisionGridFaces[56] = {
#include "assets/dryfield_night_motel_room_6_collision_063C4_faces.inc"
};

/// Serialized face-index lists for the grid cells, each ending with -1.
static s16 _gDryfieldNightMotelRoom6CollisionGridCells[198] = {
#include "assets/dryfield_night_motel_room_6_collision_063C4_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightMotelRoom6CollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..197 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_MOTEL_ROOM_6_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightMotelRoom6CollisionGridCells[(halfwordOffset)])
/// X-major directory of six face lists, indexed as x * 3 + z.
static s16* _gDryfieldNightMotelRoom6CollisionGridTable[6] = {
#include "assets/dryfield_night_motel_room_6_collision_063C4_table.inc"
};
#undef DRYFIELD_NIGHT_MOTEL_ROOM_6_CELL_FACE_LIST_AT_OFFSET

/// Motel room 6's collision mesh and its 2-by-3 XZ cell directory.
///
/// Cells are 4000 room-coordinate units wide and start at XZ (0, 0).
/// Activation binds the view coordinate; the descriptor and mutable geometry
/// remain owned by this overlay while collision queries borrow them.
static WorldCollisionGrid _gDryfieldNightMotelRoom6CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightMotelRoom6CollisionGridNormals,
    .vertices    = _gDryfieldNightMotelRoom6CollisionGridVerts,
    .faces       = _gDryfieldNightMotelRoom6CollisionGridFaces,
    .cellFaceIds = _gDryfieldNightMotelRoom6CollisionGridTable,
    .xBias       = 0,
    .zBias       = 0,
    .cellCountX  = 2,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightMotelRoom6CollisionGridFaces),
};

ViewCamera gDryfieldNightMotelRoom6ViewCameras[12] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -2500, 0x3656, -4000 } }, 289 },
    { { { { -3958, 0, -1051 }, { -55, 4090, 209 }, { 1049, 216, -3953 } }, { -1150, 1360, -5850 } }, 221 },
    { { { { 3963, 0, -1033 }, { -53, 4090, -206 }, { 1032, 213, 3957 } }, { -1200, 1360, -200 } }, 216 },
    { { { { 3992, 0, -914 }, { -253, 3935, -1106 }, { 879, 1134, 3836 } }, { -1000, 1960, -2700 } }, 246 },
    { { { { -567, 0, -4056 }, { -3575, 1934, 500 }, { 1916, 3610, -268 } }, { 750, 4410, -7350 } }, 235 },
    { { { { 358, 0, -4080 }, { -98, 4094, -8 }, { 4079, 98, 358 } }, { -200, 1060, -5350 } }, 263 },
    { { { { -4088, 0, -255 }, { -145, 3367, 2327 }, { 210, 2331, -3360 } }, { -3350, 2540, -7800 } }, 257 },
    { { { { 4092, 0, -163 }, { -99, 3249, -2491 }, { 129, 2493, 3246 } }, { -3350, 2390, -5000 } }, 246 },
    { { { { -3961, 0, -1041 }, { -524, 3537, 1997 }, { 899, 2065, -3421 } }, { -2523, 1553, -1908 } }, 282 },
    { { { { 1777, 0, 3690 }, { -2798, 2670, 1347 }, { -2405, -3105, 1158 } }, { -1947, 0x2BC9, 3461 } }, 257 },
    { { { { 1823, 0, 3667 }, { 2, ONE, -1 }, { -3667, 2, 1823 } }, { -7945, 4655, 5950 } }, 230 },
    { { { { 887, 0, 3998 }, { 2647, 3069, -587 }, { -2996, 2711, 665 } }, { -1401, 1658, -4928 } }, 680 },
};

/// GPU texture-format selector and neutral modulation for this room's sprites.
enum {
    DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT = 1,
    DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB  = 128,
};

/// Empty sprite batches for mapped view 1 of nighttime motel room 6.
///
/// The zero first count retains decoded background strips. The following
/// SPRITE_BATCH_END record ends the list without reading any source sprites;
/// the source slot retains this list's address. Storage belongs to this overlay.
static SpriteBatch _gDryfieldNightMotelRoom6View1SpriteBatches[2] = {
    { 0, 0, false, false, { 0, 0 } },
    { SPRITE_BATCH_END, 0, false, false, { 0, 0 } },
};

/// Textured foreground rectangles for mapped view 2 of nighttime motel room 6.
///
/// The view's batches select source-element ranges. XY and dimensions are
/// pixels relative to the view centre; depth is an unscaled ordering value.
/// Texture pages are 8-bit and RGB 128 is neutral modulation. Packet creation
/// snapshots geometry and texture state; cached linking rereads depth. The
/// overlay owns this writable array while its sprite resources are used.
static SpriteSource _gDryfieldNightMotelRoom6View2SpriteSources[56] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -128, 64, 0, { .fields = { 96, 56 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -144, 72, 0, { .fields = { 48, 88 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -136, 56, 0, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -120, -48, 0, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -136, -48, 0, { .fields = { 48, 80 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -128, 32, 487, { .fields = { 24, 136 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -136, -40, 474, { .fields = { 112, 80 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -160, 32, 443, { .fields = { 0, 208 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -160, 56, 490, { .fields = { 40, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -160, 72, 471, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -144, 80, 460, { .fields = { 48, 40 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -160, 0, 497, { .fields = { 8, 72 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, -160, -48, 406, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, -64, 0, 0, { .fields = { 104, 128 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -56, 16, 0, { .fields = { 120, 168 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -48, 112, 0, { .fields = { 64, 152 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 128, 32 } }, -16, 0, 0, { .fields = { 40, 40 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 120, 0, 0, { .fields = { 0, 24 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 128, 16, 0, { .fields = { 32, 168 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 144, 0, 0, { .fields = { 0, 144 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 152, 48, 0, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -24, 0, 926, { .fields = { 16, 232 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -24, 24, 929, { .fields = { 88, 96 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -40, 24, 722, { .fields = { 8, 104 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -48, 24, 686, { .fields = { 24, 104 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -56, 24, 627, { .fields = { 104, 112 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -64, 16, 627, { .fields = { 32, 72 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -64, 40, 614, { .fields = { 104, 216 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 112, 0, 784, { .fields = { 56, 176 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 120, 16, 680, { .fields = { 112, 64 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 144, 8, 485, { .fields = { 48, 136 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 128, 24, 605, { .fields = { 8, 176 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 64 } }, 128, 48, 531, { .fields = { 104, 152 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, 128, 112, 500, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 88, 32, 594, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, 88, 72, 496, { .fields = { 64, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 48, 32, 649, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, 48, 72, 523, { .fields = { 64, 208 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 16, 32, 708, { .fields = { 24, 176 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, 16, 64, 548, { .fields = { 64, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 32, 112, 536, { .fields = { 0, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -16, 32, 728, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -24, 40, 602, { .fields = { 40, 72 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -40, 48, 555, { .fields = { 16, 104 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -48, 56, 567, { .fields = { 16, 128 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -64, 72, 634, { .fields = { 24, 232 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -64, 96, 634, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, -8, 64, 559, { .fields = { 80, 112 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -8, 32, 718, { .fields = { 32, 208 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -16, 56, 569, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -24, 64, 578, { .fields = { 72, 96 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -40, 72, 583, { .fields = { 112, 216 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -56, 64, 601, { .fields = { 40, 136 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -48, 80, 588, { .fields = { 40, 40 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -32, 16, 819, { .fields = { 16, 152 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -32, 40, 613, { .fields = { 56, 208 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped view 2 of nighttime motel room 6.
///
/// The zero first count retains decoded background strips; subsequent ranges
/// cover every element of `_gDryfieldNightMotelRoom6View2SpriteSources` before SPRITE_BATCH_END.
/// Serialized trailing bytes are retained with their roles unproven. The room
/// owns these writable records while its sprite resources are used.
static SpriteBatch _gDryfieldNightMotelRoom6View2SpriteBatches[4] = {
    { 0, 0, false, false, { 0, 0 } },
    { 0, 13, false, false, { 1, 0 } },
    { 13, ARRAY_SIZE(_gDryfieldNightMotelRoom6View2SpriteSources) - 13, false, false, { 0, 0 } },
    { SPRITE_BATCH_END, 0, false, false, { 0, 0 } },
};

/// Textured foreground rectangles for mapped view 3 of nighttime motel room 6.
///
/// The view's batches select source-element ranges. XY and dimensions are
/// pixels relative to the view centre; depth is an unscaled ordering value.
/// Texture pages are 8-bit and RGB 128 is neutral modulation. Packet creation
/// snapshots geometry and texture state; cached linking rereads depth. The
/// overlay owns this writable array while its sprite resources are used.
static SpriteSource _gDryfieldNightMotelRoom6View3SpriteSources[47] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 184, 40 } }, -120, -40, 0, { .fields = { 72, 208 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 176, 8 } }, -112, 0, 0, { .fields = { 64, 248 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, 24, 8, 0, { .fields = { 0, 128 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 32, 16, 0, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 120, 8 } }, -56, 112, 0, { .fields = { 56, 200 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -8, 104, 0, { .fields = { 8, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 128, 24 } }, -112, 8, 0, { .fields = { 48, 56 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -80, 32, 0, { .fields = { 8, 96 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 16, 8, 905, { .fields = { 32, 32 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 48, 16, 601, { .fields = { 32, 184 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, 32, 589, { .fields = { 40, 112 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, 56, 593, { .fields = { 40, 32 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 56, 80, 596, { .fields = { 80, 64 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 48, 32, 619, { .fields = { 104, 168 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 40, 24, 704, { .fields = { 112, 168 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 32, 24, 739, { .fields = { 48, 184 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 24, 16, 839, { .fields = { 96, 168 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 16, 24, 888, { .fields = { 56, 192 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -120, 0, 700, { .fields = { 88, 128 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, -40, 371, { .fields = { 88, 88 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, -160, 0, 0xAF28, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, 48, 373, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, -160, 88, 371, { .fields = { 48, 32 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 32 } }, -120, 88, 490, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -120, 48, 516, { .fields = { 96, 128 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, -120, 32, 642, { .fields = { 120, 16 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -56, 32, 700, { .fields = { 0, 80 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, -24, 32, 700, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 48, 64, 579, { .fields = { 80, 168 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 40, 56, 566, { .fields = { 72, 64 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 32, 48, 578, { .fields = { 72, 88 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 24, 48, 572, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 16, 40, 613, { .fields = { 80, 120 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, -24, 48, 591, { .fields = { 120, 32 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -56, 48, 594, { .fields = { 8, 112 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -80, 40, 636, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -88, 48, 522, { .fields = { 120, 168 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, -80, 64, 517, { .fields = { 32, 88 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -40, 64, 537, { .fields = { 32, 136 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -8, 64, 559, { .fields = { 24, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, 24, 88, 563, { .fields = { 48, 112 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 24, 80, 564, { .fields = { 96, 200 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 24, 72, 567, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -8, 88, 561, { .fields = { 64, 192 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -32, 88, 536, { .fields = { 56, 168 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -56, 88, 516, { .fields = { 48, 64 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -64, 88, 509, { .fields = { 88, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped view 3 of nighttime motel room 6.
///
/// The zero first count retains decoded background strips; subsequent ranges
/// cover every element of `_gDryfieldNightMotelRoom6View3SpriteSources` before SPRITE_BATCH_END.
/// Serialized trailing bytes are retained with their roles unproven. The room
/// owns these writable records while its sprite resources are used.
static SpriteBatch _gDryfieldNightMotelRoom6View3SpriteBatches[3] = {
    { 0, 0, false, false, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightMotelRoom6View3SpriteSources), false, false, { 0, 0 } },
    { SPRITE_BATCH_END, 0, false, false, { 0, 0 } },
};

/// Textured foreground rectangles for mapped view 4 of nighttime motel room 6.
///
/// The view's batches select source-element ranges. XY and dimensions are
/// pixels relative to the view centre; depth is an unscaled ordering value.
/// Texture pages are 8-bit and RGB 128 is neutral modulation. Packet creation
/// snapshots geometry and texture state; cached linking rereads depth. The
/// overlay owns this writable array while its sprite resources are used.
static SpriteSource _gDryfieldNightMotelRoom6View4SpriteSources[43] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 16 } }, -144, 16, 809, { .fields = { 64, 208 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 16 } }, -144, 32, 693, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -144, 48, 621, { .fields = { 32, 40 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -144, 56, 622, { .fields = { 32, 16 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -144, 64, 618, { .fields = { 32, 32 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -144, 72, 618, { .fields = { 32, 24 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -96, 80, 600, { .fields = { 32, 200 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 40 } }, -144, 80, 375, { .fields = { 8, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 160 } }, -120, -88, 1250, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -56, -88, 825, { .fields = { 88, 232 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -56, -64, 825, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -56, -48, 825, { .fields = { 48, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -56, -32, 834, { .fields = { 24, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -56, -16, 837, { .fields = { 104, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -56, 0, 900, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -56, 16, 900, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -56, 32, 904, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -56, 48, 900, { .fields = { 0, 224 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, -88, 48, 680, { .fields = { 88, 176 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -88, 64, 680, { .fields = { 64, 56 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -88, 72, 679, { .fields = { 64, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, -88, 80, 715, { .fields = { 88, 224 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 232 } }, 128, -120, 475, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 232 } }, 72, -120, 693, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 144 } }, 80, -120, 612, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 144 } }, 88, -120, 587, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 144 } }, 96, -120, 587, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 144 } }, 104, -120, 582, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 144 } }, 112, -120, 583, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 144 } }, 120, -120, 583, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, 96, 24, 587, { .fields = { 80, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 112, 88, 425, { .fields = { 8, 200 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 104, 88, 662, { .fields = { 120, 232 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 104, 64, 662, { .fields = { 112, 232 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 104, 24, 590, { .fields = { 64, 200 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 112, 24, 592, { .fields = { 56, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 120, 24, 593, { .fields = { 56, 200 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 120, 64, 425, { .fields = { 0, 144 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 112, 64, 662, { .fields = { 0, 168 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 80, 24, 615, { .fields = { 24, 200 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 80, 64, 631, { .fields = { 72, 208 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 88, 24, 615, { .fields = { 64, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 88, 64, 631, { .fields = { 72, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped view 4 of nighttime motel room 6.
///
/// The zero first count retains decoded background strips; subsequent ranges
/// cover every element of `_gDryfieldNightMotelRoom6View4SpriteSources` before SPRITE_BATCH_END.
/// Serialized trailing bytes are retained with their roles unproven. The room
/// owns these writable records while its sprite resources are used.
static SpriteBatch _gDryfieldNightMotelRoom6View4SpriteBatches[6] = {
    { 0, 0, false, false, { 0, 0 } },
    { 0, 8, false, false, { 3, 0 } },
    { 8, 10, false, false, { 0, 0 } },
    { 18, 4, false, false, { 2, 0 } },
    { 22, ARRAY_SIZE(_gDryfieldNightMotelRoom6View4SpriteSources) - 22, false, false, { 1, 0 } },
    { SPRITE_BATCH_END, 0, false, false, { 0, 0 } },
};

/// Textured foreground rectangles for mapped view 5 of nighttime motel room 6.
///
/// The view's batches select source-element ranges. XY and dimensions are
/// pixels relative to the view centre; depth is an unscaled ordering value.
/// Texture pages are 8-bit and RGB 128 is neutral modulation. Packet creation
/// snapshots geometry and texture state; cached linking rereads depth. The
/// overlay owns this writable array while its sprite resources are used.
static SpriteSource _gDryfieldNightMotelRoom6View5SpriteSources[29] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 72, -120, 0, { .fields = { 80, 72 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 80, -80, 0, { .fields = { 56, 24 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 88, -56, 0, { .fields = { 64, 96 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 128 } }, 24, -112, 0, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 104 } }, 32, -88, 0, { .fields = { 120, 128 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, 40, -56, 0, { .fields = { 112, 72 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 48, -32, 0, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, -8, 0, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, 64, -8, 500, { .fields = { 48, 232 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 24 } }, 56, -32, 500, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, 48, -40, 500, { .fields = { 16, 176 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, 48, -56, 500, { .fields = { 32, 128 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, 40, -80, 500, { .fields = { 40, 72 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, 40, -88, 500, { .fields = { 40, 168 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, 24, -120, 500, { .fields = { 24, 184 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, 32, -112, 500, { .fields = { 88, 232 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 72 } }, -48, -64, 1500, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -16, -24, 1500, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -16, -40, 930, { .fields = { 80, 56 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -16, -64, 930, { .fields = { 80, 32 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 72, 48, 0, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -56, 32, 450, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 32, 48, 500, { .fields = { 72, 112 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -8, 48, 500, { .fields = { 72, 152 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 40 } }, -56, 48, 500, { .fields = { 56, 192 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -32, 32, 0, { .fields = { 24, 64 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -32, 40, 450, { .fields = { 24, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, 16, 32, 450, { .fields = { 32, 112 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 56, 32, 450, { .fields = { 40, 144 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped view 5 of nighttime motel room 6.
///
/// The zero first count retains decoded background strips; subsequent ranges
/// cover every element of `_gDryfieldNightMotelRoom6View5SpriteSources` before SPRITE_BATCH_END.
/// Serialized trailing bytes are retained with their roles unproven. The room
/// owns these writable records while its sprite resources are used.
static SpriteBatch _gDryfieldNightMotelRoom6View5SpriteBatches[5] = {
    { 0, 0, false, false, { 0, 0 } },
    { 0, 16, false, false, { 1, 0 } },
    { 16, 4, false, false, { 2, 0 } },
    { 20, ARRAY_SIZE(_gDryfieldNightMotelRoom6View5SpriteSources) - 20, false, false, { 0, 0 } },
    { SPRITE_BATCH_END, 0, false, false, { 0, 0 } },
};

/// Empty sprite batches for mapped view 6 of nighttime motel room 6.
///
/// The zero first count retains decoded background strips. The following
/// SPRITE_BATCH_END record ends the list without reading any source sprites;
/// the source slot retains this list's address. Storage belongs to this overlay.
static SpriteBatch _gDryfieldNightMotelRoom6View6SpriteBatches[2] = {
    { 0, 0, false, false, { 0, 0 } },
    { SPRITE_BATCH_END, 0, false, false, { 0, 0 } },
};

/// Textured foreground rectangles for mapped view 7 of nighttime motel room 6.
///
/// The view's batches select source-element ranges. XY and dimensions are
/// pixels relative to the view centre; depth is an unscaled ordering value.
/// Texture pages are 8-bit and RGB 128 is neutral modulation. Packet creation
/// snapshots geometry and texture state; cached linking rereads depth. The
/// overlay owns this writable array while its sprite resources are used.
static SpriteSource _gDryfieldNightMotelRoom6View7SpriteSources[24] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 184 } }, 112, -64, 300, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 184 } }, 80, -64, 287, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 56, 88 } }, 24, -64, 0x30D4, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 56, 8 } }, 24, 24, 512, { .fields = { 88, 184 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 24, 32, 553, { .fields = { 8, 248 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 56, 8 } }, 24, 40, 560, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 24, 48, 542, { .fields = { 104, 248 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 56, 8 } }, 24, 56, 539, { .fields = { 72, 88 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 56, 8 } }, 24, 64, 537, { .fields = { 88, 232 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 56, 8 } }, 24, 72, 534, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 56, 8 } }, 24, 80, 532, { .fields = { 88, 216 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 56, 8 } }, 24, 88, 366, { .fields = { 88, 224 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 56, 8 } }, 24, 96, 325, { .fields = { 72, 96 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 16 } }, 24, 104, 312, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 40, 8 } }, -80, 24, 1250, { .fields = { 96, 248 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 40, 8 } }, -80, 40, 710, { .fields = { 104, 208 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -80, 48, 705, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -80, 56, 709, { .fields = { 64, 248 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -80, 64, 706, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -80, 72, 1250, { .fields = { 32, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 64 } }, -128, 24, 1250, { .fields = { 16, 184 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 40, 8 } }, -80, 32, 710, { .fields = { 104, 200 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 240 } }, 88, -120, 310, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 120, 240 } }, 40, -120, 200, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Sprite ranges for mapped view 7 of nighttime motel room 6.
///
/// The zero first count retains decoded background strips; subsequent ranges
/// cover every element of `_gDryfieldNightMotelRoom6View7SpriteSources` before SPRITE_BATCH_END.
/// Serialized trailing bytes are retained with their roles unproven. The room
/// owns these writable records while its sprite resources are used.
static SpriteBatch _gDryfieldNightMotelRoom6View7SpriteBatches[6] = {
    { 0, 0, false, false, { 0, 0 } },
    { 0, 14, false, false, { 3, 0 } },
    { 14, 8, false, false, { 0, 0 } },
    { 22, 1, false, false, { 2, 0 } },
    { 23, ARRAY_SIZE(_gDryfieldNightMotelRoom6View7SpriteSources) - 23, false, false, { 1, 0 } },
    { SPRITE_BATCH_END, 0, false, false, { 0, 0 } },
};

/// Textured foreground rectangles for mapped view 8 of nighttime motel room 6.
///
/// The view's batches select source-element ranges. XY and dimensions are
/// pixels relative to the view centre; depth is an unscaled ordering value.
/// Texture pages are 8-bit and RGB 128 is neutral modulation. Packet creation
/// snapshots geometry and texture state; cached linking rereads depth. The
/// overlay owns this writable array while its sprite resources are used.
static SpriteSource _gDryfieldNightMotelRoom6View8SpriteSources[147] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 40, 8 } }, -136, 0, 0, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -96, 0, 489, { .fields = { 104, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -96, 40, 0, { .fields = { 0, 80 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -88, 24, 0, { .fields = { 24, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -104, 56, 0, { .fields = { 8, 176 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, -136, 8, 360, { .fields = { 32, 184 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, -136, 24, 368, { .fields = { 48, 120 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, -136, 40, 367, { .fields = { 48, 200 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -136, 56, 358, { .fields = { 8, 40 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 48 } }, -64, -112, 0, { .fields = { 0, 96 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, -56, -64, 0, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 24 } }, -48, -16, 0, { .fields = { 88, 24 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -24, 40, 0, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, -64, 104, 0, { .fields = { 48, 64 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 40, 8 } }, -56, 96, 0, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 96 } }, -160, 24, 376, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 72 } }, -160, -48, 303, { .fields = { 56, 176 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 72 } }, -160, -120, 308, { .fields = { 48, 72 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 24 } }, -24, -16, 0, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 32 } }, -48, 8, 1250, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, -48, 40, 1250, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 96 } }, -120, 24, 406, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -120, -80, 412, { .fields = { 24, 144 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 40 } }, -120, -120, 429, { .fields = { 120, 40 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 48, 8 } }, -64, -120, 0, { .fields = { 104, 232 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -88, -120, 625, { .fields = { 0, 144 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -88, -80, 663, { .fields = { 64, 24 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 24 } }, -88, 32, 1250, { .fields = { 72, 216 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 24 } }, -88, 56, 1250, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, -88, 80, 1250, { .fields = { 32, 168 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 8 } }, -88, 96, 1250, { .fields = { 112, 16 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -88, 104, 556, { .fields = { 64, 104 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, -80, -8, 606, { .fields = { 80, 184 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -72, -8, 827, { .fields = { 0, 24 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -72, 0, 807, { .fields = { 0, 208 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -72, 8, 834, { .fields = { 0, 216 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -88, 8, 483, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -72, 16, 1250, { .fields = { 48, 152 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -80, -24, 811, { .fields = { 120, 88 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -80, -32, 768, { .fields = { 120, 104 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, -88, -32, 612, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -104, -32, 447, { .fields = { 0, 152 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 24 } }, -96, -24, 472, { .fields = { 120, 136 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, -120, 8, 353, { .fields = { 88, 104 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -104, 16, 432, { .fields = { 0, 32 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 24 } }, -112, 0, 0, { .fields = { 120, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -120, -32, 0, { .fields = { 32, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -112, -32, 0, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, -88, -16, 609, { .fields = { 72, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -96, 0, 497, { .fields = { 0, 56 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -104, 8, 0, { .fields = { 0, 80 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -104, -24, 0, { .fields = { 120, 80 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -72, -40, 781, { .fields = { 8, 8 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -72, -48, 818, { .fields = { 16, 88 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -72, -56, 743, { .fields = { 16, 152 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -72, -64, 733, { .fields = { 16, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -88, -64, 702, { .fields = { 24, 24 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -88, -56, 692, { .fields = { 16, 16 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -88, -48, 732, { .fields = { 16, 192 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -88, -40, 711, { .fields = { 16, 200 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -80, -16, 777, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -64, -16, 817, { .fields = { 16, 168 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -112, 0, 0, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -112, 80, 0, { .fields = { 112, 216 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 40, 8 } }, -56, 96, 0, { .fields = { 120, 184 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 48, 8 } }, -64, 0, 0, { .fields = { 96, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 8, 569, { .fields = { 104, 32 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 16, 584, { .fields = { 104, 56 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 24, 564, { .fields = { 104, 64 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 32, 545, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 40, 528, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 48, 512, { .fields = { 104, 24 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 56, 498, { .fields = { 104, 216 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 64, 484, { .fields = { 96, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 24 } }, -40, 72, 451, { .fields = { 96, 56 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 8 } }, -72, 16, 584, { .fields = { 96, 208 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 8 } }, -72, 24, 573, { .fields = { 96, 72 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, -72, 72, 431, { .fields = { 96, 112 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 40, 8 } }, -96, 96, 454, { .fields = { 88, 80 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 24 } }, -96, 72, 428, { .fields = { 80, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 16, 8 } }, -112, 72, 406, { .fields = { 112, 152 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 8 } }, -112, 64, 400, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 8 } }, -112, 56, 405, { .fields = { 104, 8 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 40, 8 } }, -112, 32, 423, { .fields = { 96, 40 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -96, 24, 465, { .fields = { 120, 168 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -96, 16, 483, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -64, 8, 534, { .fields = { 112, 136 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -96, 0, 507, { .fields = { 32, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -72, 0, 515, { .fields = { 72, 56 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 16, 8 } }, -56, 56, 511, { .fields = { 120, 144 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -80, 56, 468, { .fields = { 112, 176 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 16, 8 } }, -80, 64, 452, { .fields = { 120, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, -64, 64, 452, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 48, 8 } }, -112, 48, 419, { .fields = { 88, 112 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -64, 48, 510, { .fields = { 24, 208 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -64, 40, 541, { .fields = { 120, 32 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -112, 40, 433, { .fields = { 24, 232 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -72, 32, 511, { .fields = { 64, 248 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -56, 32, 550, { .fields = { 8, 248 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 120, 96, 0, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, 120, 104, 375, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 24 } }, 72, 96, 375, { .fields = { 72, 136 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 48 } }, -32, -40, 0, { .fields = { 40, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 48 } }, -104, 8, 0, { .fields = { 24, 192 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 32 } }, -104, -24, 0, { .fields = { 0, 216 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 40 } }, -104, -64, 0, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 32 } }, -104, -120, 429, { .fields = { 88, 184 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 24 } }, -104, -88, 410, { .fields = { 56, 80 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 80 } }, -32, -120, 0, { .fields = { 88, 96 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 56 } }, -40, -120, 0, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 16 } }, -80, -8, 549, { .fields = { 56, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -72, 16, 1250, { .fields = { 0, 64 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -72, 24, 1250, { .fields = { 0, 72 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -72, 32, 1250, { .fields = { 8, 248 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -72, 40, 1250, { .fields = { 8, 96 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -72, 48, 1250, { .fields = { 8, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 8 } }, -72, 8, 530, { .fields = { 32, 160 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 8 } }, -56, 8, 574, { .fields = { 32, 200 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 8 } }, -40, 8, 530, { .fields = { 32, 152 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 56, 8 } }, -88, -64, 463, { .fields = { 120, 120 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 56, 8 } }, -88, -56, 488, { .fields = { 120, 128 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, -88, -48, 482, { .fields = { 8, 136 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, -88, -40, 493, { .fields = { 8, 144 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, -88, -32, 472, { .fields = { 8, 112 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -80, -24, 469, { .fields = { 16, 32 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -80, -16, 529, { .fields = { 16, 104 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 16 } }, -144, 8, 0, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 8 } }, -96, 16, 0, { .fields = { 96, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 8 } }, -120, 16, 388, { .fields = { 32, 88 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 8 } }, -120, 8, 407, { .fields = { 16, 80 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -144, -8, 0, { .fields = { 16, 80 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -128, 0, 374, { .fields = { 8, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 8 } }, -128, -8, 390, { .fields = { 16, 56 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 72 } }, -88, -72, 0, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 40 } }, -96, -72, 0, { .fields = { 24, 40 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 16 } }, -104, -72, 0, { .fields = { 72, 176 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -136, -16, 376, { .fields = { 40, 40 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -144, -32, 0, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 48, 8 } }, -136, -24, 377, { .fields = { 80, 248 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -136, -32, 377, { .fields = { 24, 224 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -144, -40, 351, { .fields = { 24, 216 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 8 } }, -136, -48, 365, { .fields = { 24, 8 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 8 } }, -128, -56, 380, { .fields = { 32, 16 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 8 } }, -128, -64, 366, { .fields = { 40, 24 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 8 } }, -128, -72, 373, { .fields = { 96, 104 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 24 } }, -136, -72, 0, { .fields = { 112, 32 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -144, -72, 0, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_ROOM_6_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Sprite ranges for mapped view 8 of nighttime motel room 6.
///
/// The zero first count retains decoded background strips; subsequent ranges
/// cover every element of `_gDryfieldNightMotelRoom6View8SpriteSources` before SPRITE_BATCH_END.
/// Serialized trailing bytes are retained with their roles unproven. The room
/// owns these writable records while its sprite resources are used.
static SpriteBatch _gDryfieldNightMotelRoom6View8SpriteBatches[8] = {
    { 0, 0, false, false, { 0, 0 } },
    { 0, 9, false, false, { 3, 0 } },
    { 9, 53, false, false, { 0, 0 } },
    { 62, 37, false, false, { 5, 0 } },
    { 99, 3, false, false, { 1, 0 } },
    { 102, 24, false, false, { 4, 0 } },
    { 126, ARRAY_SIZE(_gDryfieldNightMotelRoom6View8SpriteSources) - 126, false, false, { 2, 0 } },
    { SPRITE_BATCH_END, 0, false, false, { 0, 0 } },
};

/// Empty sprite list for mapped view 9, selecting decoded background strips.
///
/// The zero-count selector precedes `SPRITE_BATCH_END`. The source slot retains
/// this list's address, but no `SpriteSource` is read; visibility bytes remain mutable.
static SpriteBatch _gDryfieldNightMotelRoom6View9SpriteBatches[2] = {
    { 0, 0, false, false, { 0, 0 } },
    { SPRITE_BATCH_END, 0, false, false, { 0, 0 } },
};

/// Empty sprite list for mapped view 10, selecting decoded background strips.
///
/// Retains its own source-slot address; the zero-count selector and terminal
/// record supply no source elements. Keep it loaded while the view is used.
static SpriteBatch _gDryfieldNightMotelRoom6View10SpriteBatches[2] = {
    { 0, 0, false, false, { 0, 0 } },
    { SPRITE_BATCH_END, 0, false, false, { 0, 0 } },
};

/// Empty sprite list for mapped view 11, selecting decoded background strips.
///
/// Retains its own source-slot address; the zero-count selector and terminal
/// record supply no source elements. Keep it loaded while the view is used.
static SpriteBatch _gDryfieldNightMotelRoom6View11SpriteBatches[2] = {
    { 0, 0, false, false, { 0, 0 } },
    { SPRITE_BATCH_END, 0, false, false, { 0, 0 } },
};

/// Empty sprite list for mapped view 12, selecting decoded background strips.
///
/// Retains its own source-slot address; the zero-count selector and terminal
/// record supply no source elements. Keep it loaded while the view is used.
static SpriteBatch _gDryfieldNightMotelRoom6View12SpriteBatches[2] = {
    { 0, 0, false, false, { 0, 0 } },
    { SPRITE_BATCH_END, 0, false, false, { 0, 0 } },
};

SpriteView gDryfieldNightMotelRoom6SpriteViews[12] = {
    { { .empty = _gDryfieldNightMotelRoom6View1SpriteBatches }, _gDryfieldNightMotelRoom6View1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom6View2SpriteSources }, _gDryfieldNightMotelRoom6View2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom6View3SpriteSources }, _gDryfieldNightMotelRoom6View3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom6View4SpriteSources }, _gDryfieldNightMotelRoom6View4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom6View5SpriteSources }, _gDryfieldNightMotelRoom6View5SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelRoom6View6SpriteBatches }, _gDryfieldNightMotelRoom6View6SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom6View7SpriteSources }, _gDryfieldNightMotelRoom6View7SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom6View8SpriteSources }, _gDryfieldNightMotelRoom6View8SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelRoom6View9SpriteBatches }, _gDryfieldNightMotelRoom6View9SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelRoom6View10SpriteBatches }, _gDryfieldNightMotelRoom6View10SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelRoom6View11SpriteBatches }, _gDryfieldNightMotelRoom6View11SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelRoom6View12SpriteBatches }, _gDryfieldNightMotelRoom6View12SpriteBatches, NULL },
};

/// Point lights for nighttime motel room 6 in every logical view.
///
/// Positions and inner/outer falloff radii use integer room-coordinate units;
/// RGB intensities have 12 fractional bits. Coordinate updates bind the view
/// parent and compose transforms, and lighting queries overwrite attenuation.
/// The third source retains zero RGB and contributes no light. The room owns
/// this mutable array for as long as its lighting collection is borrowed.
static WorldCoordPointLight _gDryfieldNightMotelRoom6PointLights[5] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -882, -3651, 10890 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 528, 586, 603 },
        },
        .inner = 13000,
        .outer = 18000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2452, -1396, 2755 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3173, 3151, 3128 },
        },
        .inner = 2120,
        .outer = 4223,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4822, -1215, 4142 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 0, 0, 0 },
        },
        .inner = 0,
        .outer = 478,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 429, -1248, 4815 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 4094, 3618, 3115 },
        },
        .inner = 1800,
        .outer = 2981,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3295, -1382, 6280 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3340, 3277, 3227 },
        },
        .inner = 652,
        .outer = 2681,
    },
};

/// Room light collection with five point sources and no directional or cone lights.
///
/// The lighting descriptor borrows this collection independently of the view.
/// Its sources remain mutable for coordinate composition and attenuation queries
/// and must stay loaded with this overlay.
static WorldCoordRoomLights _gDryfieldNightMotelRoom6Lights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightMotelRoom6PointLights),
    .pointLights           = _gDryfieldNightMotelRoom6PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Directed boundaries between view pairs 2/3, 3/4, 4/5, 4/7 and 7/8.
///
/// Each pair has both directions; the final live record carries `WORLD_COLLISION_TRIGGER_LAST`. Binding
/// supplies the view coordinate and links/enables these mutable triggers.
/// Origins, vertex offsets and radii use room units; normals use `ONE` per unit.
/// Unlink or clear their runtime list before unloading the room overlay.
static WorldCollisionTrigger _gDryfieldNightMotelRoom6ViewBoundaryTriggers[10] = {
    { NULL, NULL, NULL, { 3590, -1168, 3040, 0 }, { { -1702, -2192, 0, 0 }, { 1690, -2192, 0, 0 }, { -1702, 2192, 0, 0 }, { 1716, 2192, 0, 0 } }, { 0, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 2780, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3743, -1104, 2911, 0 }, { { 1727, -2128, 51, 0 }, { -1726, -2128, -50, 0 }, { 1727, 2128, 51, 0 }, { -1726, 2128, -50, 0 } }, { -120, 0, 4098, 0 }, { 0, 0, 4096, 0 }, 2733, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1215, -1088, 4559, 0 }, { { 1311, -2112, 99, 0 }, { -1310, -2112, -98, 0 }, { 1311, 2112, 99, 0 }, { -1310, 2112, -98, 0 } }, { -309, 0, 4084, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1247, -1200, 4735, 0 }, { { -1304, -2224, -163, 0 }, { 1304, -2224, 163, 0 }, { -1304, 2224, -163, 0 }, { 1304, 2224, 163, 0 } }, { 508, 0, -4070, 0 }, { 0, 0, 4096, 0 }, 2572, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1439, -1248, 6239, 0 }, { { -1267, -2272, -353, 0 }, { 1266, -2272, 352, 0 }, { -1267, 2272, -353, 0 }, { 1266, 2272, 352, 0 } }, { 1101, 0, -3960, 0 }, { 0, 0, 4096, 0 }, 2623, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1343, -1296, 6079, 0 }, { { 1266, -2320, 352, 0 }, { -1266, -2320, -352, 0 }, { 1266, 2320, 352, 0 }, { -1266, 2320, -352, 0 } }, { -1099, 0, 3949, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2654, -1296, 5439, 0 }, { { -1, -2320, 639, 0 }, { 1, -2320, -639, 0 }, { -1, 2320, 639, 0 }, { 1, 2320, -639, 0 } }, { -4111, 0, -9, 0 }, { 0, 0, 4096, 0 }, 2401, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2527, -1376, 5407, 0 }, { { 33, -2400, -638, 0 }, { -32, -2400, 639, 0 }, { 33, 2400, -638, 0 }, { -32, 2400, 639, 0 } }, { 4103, 0, 208, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3455, -1248, 6365, 0 }, { { -627, -2272, 124, 0 }, { 627, -2272, -123, 0 }, { -627, 2272, 124, 0 }, { 627, 2272, -123, 0 } }, { -797, 0, -4032, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3392, -1312, 6237, 0 }, { { 627, -2336, -123, 0 }, { -627, -2336, 124, 0 }, { 627, 2336, -123, 0 }, { -627, 2336, 124, 0 } }, { 791, 0, 4028, 0 }, { 0, 0, 4096, 0 }, 2415, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Fifteen mutable room interactions, including balcony and water-tower departures.
///
/// CAP selectors 6, 22, 11 and 13 go through room messages; the others select
/// CAP directly. One automatic room action sends ID zero, which this room
/// ignores. The final live quad carries `WORLD_COLLISION_TRIGGER_LAST`. Binding supplies
/// coordinates and list links; positions/radii use room units and normals use `ONE` per unit.
/// Unlink or clear their runtime list before unloading the room overlay.
static WorldCollisionTrigger _gDryfieldNightMotelRoom6ActionTriggers[15] = {
    { NULL, NULL, NULL, { 4512, -48, 1328, 0 }, { { -320, 0, -784, 0 }, { 320, 0, -784, 0 }, { -320, 0, 784, 0 }, { 320, 0, 784, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 846, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY, DRYFIELD_NIGHT_MOTEL_ROOM_6_BALCONY_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 896, -64, 1199, 0 }, { { -320, 0, -1088, 0 }, { 320, 0, -1088, 0 }, { -320, 0, 1089, 0 }, { 320, 0, 1089, 0 } }, { 0, 4111, 0, 0 }, { 4096, 0, 0, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1304, -64, 7248, 0 }, { { -504, 0, -1088, 0 }, { -216, 0, -1088, 0 }, { -504, 0, 1088, 0 }, { 1224, 0, 1088, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1634, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 0, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 3328, -64, 4384, 0 }, { { -704, 0, -400, 0 }, { 704, 0, -400, 0 }, { -704, 0, 400, 0 }, { 704, 0, 400, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, -4096, 0 }, 809, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1264, -64, 3024, 0 }, { { -1776, 0, -1280, 0 }, { 1776, 0, -1280, 0 }, { -1776, 0, 1280, 0 }, { 1776, 0, 1280, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 2187, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 352, -128, 5568, 0 }, { { -240, 0, -1024, 0 }, { 1392, 0, -1024, 0 }, { -240, 0, 448, 0 }, { 1392, 0, 448, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, -4096, 0 }, 1726, WORLD_COLLISION_TRIGGER_ACTION_CAP, 22, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 640, -48, 6656, 0 }, { { -320, 0, -432, 0 }, { 320, 0, -432, 0 }, { -320, 0, 432, 0 }, { 320, 0, 432, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 535, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER, DRYFIELD_NIGHT_MOTEL_ROOM_6_WATER_TOWER_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2920, -64, 208, 0 }, { { -1480, 0, -160, 0 }, { 1432, 0, -160, 0 }, { -936, 0, 896, 0 }, { 984, 0, 896, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 1487, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1728, -64, 576, 0 }, { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 561, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4256, -64, 608, 0 }, { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 561, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3936, -64, 4000, 0 }, { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 561, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1663, -64, 7415, 0 }, { { 669, 0, -1319, 0 }, { 650, 0, 404, 0 }, { -713, 0, -355, 0 }, { -604, 0, 1272, 0 } }, { 0, 4095, 0, 0 }, { -2896, 0, -2896, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4544, -64, 7104, 0 }, { { -400, 0, -512, 0 }, { 400, 0, -512, 0 }, { -400, 0, 512, 0 }, { 400, 0, 512, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 649, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, 5568, 0 }, { { -1072, 0, -576, 0 }, { 400, 0, -576, 0 }, { -1072, 0, 1184, 0 }, { 400, 0, 1184, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 1593, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2976, -64, 6528, 0 }, { { -176, 0, -1024, 0 }, { 816, 0, -1024, 0 }, { -176, 0, 960, 0 }, { 816, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1305, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaVariant gDryfieldNightMotelRoom6AreaVariants[11] = { 0 };

/// Cue-2 entry numbers in the loaded type-1 footstep bank; cue 1 adds one.
enum {
    DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_1_WALK_ENTRY = 0x2D,
    DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_1_RUN_ENTRY  = 0x2F,
    DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_2_WALK_ENTRY = 0x3D,
    DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_2_RUN_ENTRY  = 0x3F,
    DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_3_WALK_ENTRY = 0x01,
    DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_3_RUN_ENTRY  = 0x03,
};

/// Footstep cue bases for motel room 6 surface class 1.
///
/// Walking and scripted stair climbing share a pair; running uses a separate pair.
/// Cue 1 adds one, and companion playback adds 100. These read-only requests
/// select the loaded type-1 bank and remain owned by this overlay.
static const WorldCollisionFootstepSounds _gDryfieldNightMotelRoom6Class1FootstepSounds SECTION(".data") = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_1_WALK_ENTRY,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_1_RUN_ENTRY,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_1_WALK_ENTRY,
};

/// Footstep cue bases for motel room 6 surface class 2.
///
/// Walking and scripted stair climbing share a pair; running uses a separate pair.
/// Cue 1 adds one, and companion playback adds 100. These read-only requests
/// select the loaded type-1 bank and remain owned by this overlay.
static const WorldCollisionFootstepSounds _gDryfieldNightMotelRoom6Class2FootstepSounds SECTION(".data") = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_2_WALK_ENTRY,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_2_RUN_ENTRY,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_2_WALK_ENTRY,
};

/// Footstep cue bases for motel room 6 surface class 3.
///
/// Walking and scripted stair climbing share a pair; running uses a separate pair.
/// Cue 1 adds one, and companion playback adds 100. These read-only requests
/// select the loaded type-1 bank and remain owned by this overlay.
static const WorldCollisionFootstepSounds _gDryfieldNightMotelRoom6Class3FootstepSounds SECTION(".data") = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_3_WALK_ENTRY,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_3_RUN_ENTRY,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_ROOM_6_CLASS_3_WALK_ENTRY,
};

/// Blocking, impact-enabled, pushback-applying surface shared by classes 0 and 4..7.
///
/// No footstep cues are selected. The class directory borrows this single
/// overlay-owned record; the stored field_0 byte is zero and its role unproven.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom6SilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking, impact-enabled, pushback-applying surface for motel room 6 class 1.
///
/// Borrows the class's footstep requests while this overlay is loaded.
/// The stored field_0 byte is zero and its role unproven.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom6Class1SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelRoom6Class1FootstepSounds,
};

/// Blocking, impact-enabled, pushback-applying surface for motel room 6 class 2.
///
/// Borrows the class's footstep requests while this overlay is loaded.
/// The stored field_0 byte is zero and its role unproven.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom6Class2SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelRoom6Class2FootstepSounds,
};

/// Blocking, impact-enabled, pushback-applying surface for motel room 6 class 3.
///
/// Borrows the class's footstep requests while this overlay is loaded.
/// The stored field_0 byte is zero and its role unproven.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom6Class3SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelRoom6Class3FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightMotelRoom6SurfacePropertiesByClass[8] = {
    &_gDryfieldNightMotelRoom6SilentSurfaceProperties,
    &_gDryfieldNightMotelRoom6Class1SurfaceProperties,
    &_gDryfieldNightMotelRoom6Class2SurfaceProperties,
    &_gDryfieldNightMotelRoom6Class3SurfaceProperties,
    &_gDryfieldNightMotelRoom6SilentSurfaceProperties,
    &_gDryfieldNightMotelRoom6SilentSurfaceProperties,
    &_gDryfieldNightMotelRoom6SilentSurfaceProperties,
    &_gDryfieldNightMotelRoom6SilentSurfaceProperties,
};

/// Low-nibble policy that sets a saved area's map mark when applying a rest update.
enum { DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_MAP_MARK_SET = 1 };

/// Saved placement-layout and map-mark updates committed after an accepted rest.
///
/// Fifteen night-stage areas are updated in every save mode and lose saved
/// enemy poses. Main street and garage clear their marks; the other areas set
/// them. `AREA_APPLY_END` terminates the list. Application borrows it synchronously.
static const AreaApplyRec _gDryfieldNightMotelRoom6PostRestAreaUpdates[16] SECTION(".data") = {
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, 2, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE, 3, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 3, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE, 3, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1, 2, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2, 2, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3, 2, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4, 2, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT, 4, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R, 3, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GARAGE, 3, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, 2, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5, 2, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY, 2, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT, 4, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_MAP_MARK_SET },
    { AREA_APPLY_END, 0, 0, 0 },
};

/// Empty additional rest-update list selected after Gray Stalker has been defeated.
///
/// Contains only `AREA_APPLY_END`, so the conditional application changes no areas.
/// Retains separate overlay-owned storage, borrowed synchronously during rest completion.
static const AreaApplyRec _gDryfieldNightMotelRoom6PostRestGrayStalkerAreaUpdates[1] SECTION(".data") = {
    { AREA_APPLY_END, 0, 0, 0 },
};

Task* gRoomCutsceneSoundTask = NULL;

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\xDF\xDC"
#include "../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

void dryfieldNightMotelRoom6TelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/room/telephone_panels.inc.c"

#include "../../shared/room/planar_reflection.inc.c"

void dryfieldNightMotelRoom6PlayerReflectionTask(Task* reflectionTask)
{
    _planarReflectionPlayerTask(reflectionTask);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

#include "../../shared/room/room_cutscene_task.inc.c"

/// Room-controller states: 0 registers its message endpoint, 1 idles, 2 kills it.
///
/// Dispatch copies all three callbacks to the stack and indexes them directly
/// by `Task.state`, without a terminator. The callbacks require the room overlay loaded.
static const TaskFuncTable3 _gDryfieldNightMotelRoom6RoomStates = {
    {
        _dryfieldNightMotelRoom6InitRoomTask,
        _dryfieldNightMotelRoom6IdleTask,
        taskKill,
    },
};

/// Selects the nighttime CAP/rest handler for motel room 6 commands other than 22.
///
/// The replacement names a previously declared `s32 (Task*, s32, s32, s32)`
/// function taking the receiver, message ID, command and second payload.
/// `motel_room_6_cutscene_msg.inc.c` calls it once with those arguments unchanged
/// and discards its reply. Define this alias only around that fragment inclusion
/// and undefine it afterwards.
#define MOTEL_ROOM_6_HANDLE_OTHER_COMMAND _dryfieldNightMotelRoom6HandleCommand
#include "../../shared/motel_room_6_cutscene_msg.inc.c"
#undef MOTEL_ROOM_6_HANDLE_OTHER_COMMAND

/// Heals the player, commits the rest's story updates and reloads the saved arrival.
///
/// Requires the completed rest CAP, a live player and room-owned area updates.
/// Reload allocation failure still commits those updates and kills the task.
static inline void _dryfieldNightMotelRoom6FinishRest(Task* task)
{
    enum { DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_SALOON_PROGRESS = 2,
           DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_ARRIVAL_WARP    = 1,
           DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_ARRIVAL_ROOM    = 1,
           DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_SPRITE_VARIANT  = 1 };
    // Commit the rest and destination before handing presentation to reload.
    playerStateRestoreFullHpMp();
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    areaApplySavedUpdates(_gDryfieldNightMotelRoom6PostRestAreaUpdates);
    if (gameFlagGetNibble(GAME_FLAG_GRAY_STALKER_DEFEATED) != 0) {
        areaApplySavedUpdates(_gDryfieldNightMotelRoom6PostRestGrayStalkerAreaUpdates);
    }
    gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_CUTSCENE_SEEN, 1);
    gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_TALK_PROGRESS, DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_SALOON_PROGRESS);
    gameFlagSetNibble(GAME_FLAG_030, 0);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_DRYFIELD_NIGHT_R08;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_ARRIVAL_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_ARRIVAL_ROOM;
    gDisplayState.spriteVariant                                 = DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_SPRITE_VARIANT;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    taskKill(task);
}

/// Offers the nighttime rest, then heals the player and reloads the next story area.
///
/// States 0..2 hold scripted control and run CAP command 16, leaving one tick
/// before checking key 11 for refusal. That path kills the task, resumes control
/// and still increments its state. States 3..5 mark rest taken, wait for CAP,
/// refill HP/MP, commit saved-area/story changes and reload area 8 at warp/room 1.
/// Requires a live player, night room resources and valid CAP selection. Owns
/// no work; spawn arguments are ignored. Failed reload allocation still commits
/// the story and kills the task.
static void _dryfieldNightMotelRoom6RestTask(Task* task)
{
    enum { DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_START           = 0,
           DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_SELECTION_DELAY = 1,
           DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_CHECK_SELECTION = 2,
           DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_COMMIT          = 3,
           DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_WAIT_CAP        = 4,
           DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_RELOAD          = 5,
           DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_CAP_COMMAND     = 16,
           DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_DECLINED_KEY    = 11,
           DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_TAKEN           = 2 };

    switch (task->state) {
        case DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommandWithTransition(DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_CAP_COMMAND);
            task->state++;
            break;
        case DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_SELECTION_DELAY:
            task->state++;
            break;
        case DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_CHECK_SELECTION:
            if (capGetVariantKey() == DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_DECLINED_KEY) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            }
            task->state++;
            break;
        case DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_COMMIT:
            gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_ROOM_6_REST_TAKEN, DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_TAKEN);
            task->state++;
            break;
        case DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_WAIT_CAP:
            if (capIsBusy() != 0) {
                return;
            }
            task->state++;
            break;
        case DRYFIELD_NIGHT_MOTEL_ROOM_6_REST_RELOAD:
            _dryfieldNightMotelRoom6FinishRest(task);
            break;
    }
}

#include "../../shared/room/room_cutscene_sound_task.inc.c"

/// Selects nighttime motel room 6 dialogue or its refueling-dependent rest task.
///
/// Handles non-cutscene `ROOM_MESSAGE_COMMAND` requests; only the command word
/// is used. Commands 6, 13 and 11 select CAP, with alternatives 20, 19 and 21
/// after the balcony scene. Before that scene, command 6 spawns rest when garage
/// progress is positive and rest has not been taken. Other commands do nothing.
/// Returns zero; the shared dispatcher ignores it. Keep room/scene resources
/// loaded through the selected CAP sequence or child task.
static s32 _dryfieldNightMotelRoom6HandleCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum { ROOM_COMMAND_REST      = 6,
           ROOM_COMMAND_13        = 13,
           ROOM_COMMAND_11        = 11,
           BALCONY_REST_CAP       = 20,
           BALCONY_COMMAND_13_CAP = 19,
           BALCONY_COMMAND_11_CAP = 21,
           REST_TAKEN             = 2,
           REST_TASK_SLOT         = 0,
           REST_TASK_ARGUMENT     = 17 };
    if (command == ROOM_COMMAND_REST) {
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) != 0) {
            capRunCommandWithTransition(BALCONY_REST_CAP);
        } else if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) > 0 && gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_ROOM_6_REST_TAKEN) < REST_TAKEN) {
            taskSpawnFromTable(&_gDryfieldNightMotelRoom6RestTaskDesc, REST_TASK_SLOT, REST_TASK_ARGUMENT, 0);
        } else {
            capRunCommandWithTransition(command);
        }
    }
    if (command == ROOM_COMMAND_13) {
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) != 0) {
            capRunCommandWithTransition(BALCONY_COMMAND_13_CAP);
        } else {
            capRunCommandWithTransition(ROOM_COMMAND_13);
        }
    }
    if (command == ROOM_COMMAND_11) {
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) != 0) {
            capRunCommandWithTransition(BALCONY_COMMAND_11_CAP);
        } else {
            capRunCommandWithTransition(ROOM_COMMAND_11);
        }
    }
    return 0;
}

/// Refuses every key-item use request in this room without consuming the item.
///
/// `itemId` is the collected inventory item's ID. All arguments are ignored;
/// returns 0 so the inventory menu reports that the item cannot be used here.
static s32 _dryfieldNightMotelRoom6RejectKeyItemMessage(Task* receiver, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

#include "../../shared/room/room_variants_motel_balcony.inc.c"

/// Ignores trigger requests for room-specific actions and returns zero.
///
/// `request` is borrowed for synchronous dispatch, but no argument is read or
/// retained. No room action is started and the request is left untouched.
static s32 _dryfieldNightMotelRoom6IgnoreActionMessage(Task* receiver, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Queues the room's sound script 12 for CAP sound cue 99.
///
/// Handles `ROOM_MESSAGE_SOUND` with an integer cue key. Other cues do nothing;
/// returns zero and ignores the other callback arguments. Requires the room's
/// sound bank to remain loaded for the queued script.
static s32 _dryfieldNightMotelRoom6SoundMessage(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_ROOM_6_SOUND_CUE    = 99,
        DRYFIELD_NIGHT_MOTEL_ROOM_6_SOUND_SCRIPT = 12
    };

    if (cueKey == DRYFIELD_NIGHT_MOTEL_ROOM_6_SOUND_CUE) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6, DRYFIELD_NIGHT_MOTEL_ROOM_6_SOUND_SCRIPT), 0, 0);
    }
    return 0;
}

/// Registers the room task and its message handlers, then enters the idle state.
///
/// Runs once in state 0 on a bodyless task. The task borrows this overlay's
/// message table for its lifetime; the overlay must remain loaded.
static void _dryfieldNightMotelRoom6InitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldNightMotelRoom6RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state += 1;
}

/// Keeps the room task alive to receive messages after initialization.
///
/// State 1's idle callback leaves the task, its state and its message table intact.
static void _dryfieldNightMotelRoom6IdleTask(Task* unusedTask)
{
}

void dryfieldNightMotelRoom6RoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gDryfieldNightMotelRoom6RoomStates;
    states.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_wide_diamond.inc.c"

#include "../../shared/effects/glow_draw_pulsing_disc.inc.c"

#include "../../shared/motel_room_6_draw_glow.inc.c"
