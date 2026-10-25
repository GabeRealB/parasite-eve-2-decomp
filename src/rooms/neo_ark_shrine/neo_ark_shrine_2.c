#include "rooms/neo_ark_shrine.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "neo_ark_shrine_private.h"

#include "actors/actor_02000.h"
#include "actors/actor_02500.h"
#include "actors/actor_03800.h"
#include "actors/actor_402200.h"
#include "actors/actor_403900.h"

#include "gameplay/display.h"
#include "gameplay/action_prompt.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/item_menu.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/areas.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/room/action_prompt.h"

/// Work block of the shrine's two falling-prop tasks.
///
/// Allocated zeroed when a prop spawns and kept in `Task::work`. The two
/// matrices are the storage the prop's `TmdObject::colorMtx` and `lightMtx`
/// point at; the rest is the drop that brings the prop down from above the
/// room onto the floor, which starts from rest and whose acceleration itself
/// grows by a fixed step every frame.
typedef struct {
    MATRIX color;            // The model's colour matrix
    MATRIX light;            // The model's light matrix
    s16    fallAcceleration; // Added to `fallVelocity` every frame, in world units per frame squared; grows by the prop's own step each frame
    s16    fallVelocity;     // Added to the prop's height every frame, in world units per frame (positive is down)
    s16    fallFrames;       // Frames the drop has lasted; times its sound and variable-motor vibration ramp
} _NeoArkShrineFallingPropWork;
STATIC_ASSERT_SIZEOF(_NeoArkShrineFallingPropWork, 0x48);

/// Puzzle state destinations, room selectors and transition timing used below.
enum {
    NEO_ARK_SHRINE_PUZZLE_STATE_IDLE           = 2,
    NEO_ARK_SHRINE_PUZZLE_STATE_COMMAND_RESULT = 4,
    NEO_ARK_SHRINE_LAYOUT_ACTIVE               = 2,
    NEO_ARK_SHRINE_LAYOUT_ACTIVE_AFTER_REVEAL  = 5,
    NEO_ARK_SHRINE_LAYOUT_ENEMIES_RELEASED     = 6,
    NEO_ARK_SHRINE_ROOM_VIEW                   = 10,
    NEO_ARK_SHRINE_LAYOUT_WAIT_FRAMES          = 30,
    NEO_ARK_SHRINE_LAYOUT_RUMBLE_FRAMES        = 18,
    NEO_ARK_SHRINE_LAYOUT_RUMBLE_START         = 48,
    NEO_ARK_SHRINE_LAYOUT_RUMBLE_END           = 144,
};

/// Starts a puzzle layout transition and its motor ramp with a fresh frame timer.
///
/// `task` must be a stable, live `Task*` with owned `NeoArkShrinePuzzleWork`;
/// it is evaluated three times. `active` is evaluated once and must be 0 or 1.
/// Borrows port 0's action prompt and requires the puzzle drawing resources.
/// Expands as a block with locals `prompt` and `work`; arguments must
/// not name either local. Increments the caller's task state after clearing
/// the timer; no task or work ownership transfers. Invoke in a braced caller context.
#define NEO_ARK_SHRINE_BEGIN_LAYOUT(task, active)                                                              \
    {                                                                                                          \
        ActionPrompt*           prompt = gActionPromptSlots;                                                   \
        NeoArkShrinePuzzleWork* work   = (task)->work;                                                         \
                                                                                                               \
        padScriptSpawnVariableMotorRamp(NEO_ARK_SHRINE_LAYOUT_RUMBLE_FRAMES,                                   \
                                        NEO_ARK_SHRINE_LAYOUT_RUMBLE_START, NEO_ARK_SHRINE_LAYOUT_RUMBLE_END); \
        gNeoArkShrinePuzzleLayoutActive = (active);                                                            \
        prompt->mode                    = ACTION_PROMPT_MODE_HIDDEN;                                           \
        prompt->cursorSpeed             = ACTION_PROMPT_SPEED_STOPPED;                                         \
        neoArkShrineDrawPuzzleFrame((task));                                                                   \
        work->timer = 0;                                                                                       \
        (task)->state++;                                                                                       \
    }

/// Waits thirty puzzle frames, publishes the selected layout and returns to idle.
///
/// `task` must be a stable, live `Task*` with owned `NeoArkShrinePuzzleWork`;
/// it is evaluated twice, or three times when returning to idle. Its u16 timer
/// counts puzzle frames with wraparound. The two layout arguments must be
/// stable byte-valued room selectors; only the selected one is evaluated, twice.
/// Borrows port 0's action prompt and requires the puzzle drawing resources.
/// Expands as a block with locals `prompt` and `work`; arguments must
/// not name either local. Publishes both saved and live room selectors before
/// requesting the object rebuild; no task or work ownership transfers.
/// Invoke in a braced caller context.
#define NEO_ARK_SHRINE_WAIT_LAYOUT(task, beforeRevealLayout, afterRevealLayout)                     \
    {                                                                                               \
        ActionPrompt*           prompt = gActionPromptSlots;                                        \
        NeoArkShrinePuzzleWork* work   = (task)->work;                                              \
                                                                                                    \
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;                                            \
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;                                          \
        work->timer         = work->timer + 1;                                                      \
        neoArkShrineDrawPuzzleFrame((task));                                                        \
        if (work->timer >= NEO_ARK_SHRINE_LAYOUT_WAIT_FRAMES) {                                     \
            /* Update both locations in each branch before requesting the object rebuild. */        \
            if (gameFlagGetNibble(GAME_FLAG_0E9) == 0) {                                            \
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = (beforeRevealLayout); \
                gGameSession->location.roomState                            = (beforeRevealLayout); \
            } else {                                                                                \
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = (afterRevealLayout);  \
                gGameSession->location.roomState                            = (afterRevealLayout);  \
            }                                                                                       \
            gGameSession->roomObjsDirty = true;                                                     \
            (task)->state               = NEO_ARK_SHRINE_PUZZLE_STATE_IDLE;                         \
        }                                                                                           \
    }

static void _actionPromptResetDefault(Task* task);
static void _neoArkShrineUpdateFallingPropLighting(Task* task);
static void _neoArkShrineDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);

/// Offset of the beam's near end from the effect's parent coordinate. The far
/// end's offset follows it directly; the beam's set-up state reaches that one
/// as element 1.

/// Offset of the beam's far end from the effect's parent coordinate.

static void _neoArkShrineInitializePuzzle(Task* task);
static void _neoArkShrinePreparePuzzleCursor(Task* task);
static void _neoArkShrineOpenPuzzleCommands(Task* task);
static void _neoArkShrineResolvePuzzleExamine(Task* task);
static void _neoArkShrineClosePuzzle(Task* task);
static void _neoArkShrineBeginPuzzleLayoutActivation(Task* task);
static void _neoArkShrineWaitPuzzleLayoutActivation(Task* task);
static void _neoArkShrineBeginPuzzleEnemyRelease(Task* task);
static void _neoArkShrineWaitToSpawnFirstFallingProp(Task* task);
static void _neoArkShrineWaitToSpawnSecondFallingProp(Task* task);
static void _neoArkShrineWaitPuzzleEnemyReveal(Task* task);
static void _neoArkShrineFinishPuzzleEnemyRelease(Task* task);
static void _neoArkShrineBeginPuzzleLayoutRestoration(Task* task);
static void _neoArkShrineWaitPuzzleLayoutRestoration(Task* task);
static void _neoArkShrineInitializeFirstFallingProp(Task* task);
static void _neoArkShrineDropFirstProp(Task* task);
static void _neoArkShrineWaitFirstPropLayoutRestore(Task* task);
static void _neoArkShrineInitializeSecondFallingProp(Task* task);
static void _neoArkShrineDropSecondProp(Task* task);

/// State handlers of the shrine's sliding-tile puzzle and its room outcomes.
///
/// Indexed by `Task::state` in 0..15 without a bounds check. States 0..6
/// initialize the session, handle input/commands, close it and slide tiles;
/// 7..8 activate the layout, 9..13 release enemies, and 14..15 restore the
/// base layout. The dispatcher copies this complete 0x40-byte table by value.
static const TaskFuncTable16 _gNeoArkShrinePuzzleStates = {
    {
        _neoArkShrineInitializePuzzle,
        _neoArkShrinePreparePuzzleCursor,
        neoArkShrinePuzzleIdle,
        _neoArkShrineOpenPuzzleCommands,
        _neoArkShrineResolvePuzzleExamine,
        _neoArkShrineClosePuzzle,
        neoArkShrineSlidePuzzleTile,
        _neoArkShrineBeginPuzzleLayoutActivation,
        _neoArkShrineWaitPuzzleLayoutActivation,
        _neoArkShrineBeginPuzzleEnemyRelease,
        _neoArkShrineWaitToSpawnFirstFallingProp,
        _neoArkShrineWaitToSpawnSecondFallingProp,
        _neoArkShrineWaitPuzzleEnemyReveal,
        _neoArkShrineFinishPuzzleEnemyRelease,
        _neoArkShrineBeginPuzzleLayoutRestoration,
        _neoArkShrineWaitPuzzleLayoutRestoration,
    },
};
/// First falling prop's initialize, drop, restore-wait and kill state handlers.
///
/// The dispatcher copies the complete table and indexes `Task::state` in
/// 0..3 without checking the bound. The landed prop stays live until the
/// puzzle consumes its pending base-layout restore request.
static const TaskFuncTable4 _gNeoArkShrineFirstPropStates = {
    { _neoArkShrineInitializeFirstFallingProp, _neoArkShrineDropFirstProp, _neoArkShrineWaitFirstPropLayoutRestore, taskKill },
};
/// Second falling prop's initialize, drop and kill state handlers.
///
/// The dispatcher copies the complete table and indexes `Task::state` in
/// 0..2 without checking the bound. This prop has no restore-wait state.
static const TaskFuncTable3 _gNeoArkShrineSecondPropStates = {
    { _neoArkShrineInitializeSecondFallingProp, _neoArkShrineDropSecondProp, taskKill },
};

static WorldCollisionGrid     _gNeoArkShrineBaseCollisionGrid;
static WorldCollisionGrid     _gNeoArkShrineActiveCollisionGrid;
static WorldCollisionGrid     _gNeoArkShrineEnemyReleaseCollisionGrid;
static WorldCollisionOccluder _gNeoArkShrineSightOccluders[4];
static WorldCollisionTrigger  _gNeoArkShrineViewBoundaryTriggers[14];
static WorldCollisionTrigger  _gNeoArkShrineBaseActionTriggers[9];
static WorldCollisionTrigger  _gNeoArkShrineActiveActionTriggers[8];
static WorldCollisionTrigger  _gNeoArkShrineEnemyReleaseActionTriggers[8];
static WorldCoordRoomLights   _gNeoArkShrineRoomLights;

/// Seventeen fixed shrine flare centres in signed world-coordinate units.
///
/// Mapped views select overlapping subsets of this single array. Entries
/// 0..14 use flare texture 1; entries 15..16 use texture 0 in views 10 and 17.
/// Storage remains in the loaded room's data segment throughout drawing.
static SVECTOR _gNeoArkShrineFlarePositions[] = {
    { 3560, -1200, 6310, 0 },
    { 4990, -1200, 6280, 0 },
    { 3560, -1200, 3690, 0 },
    { 4920, -1200, 3690, 0 },
    { 5770, -1200, 2980, 0 },
    { 5790, -1200, 1740, 0 },
    { 5780, -1200, 440, 0 },
    { 8320, -1200, 5070, 0 },
    { 8340, -1200, 3610, 0 },
    { 9060, -1200, 2750, 0 },
    { 10430, -1200, 2880, 0 },
    { 11710, -1200, 2750, 0 },
    { 9060, -1200, 260, 0 },
    { 10410, -1200, 260, 0 },
    { 11710, -1200, 260, 0 },
    { 6300, -1870, -4490, 0 },
    { 7760, -1880, -4490, 0 },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources gNeoArkShrineCollisionRooms[NEO_ARK_SHRINE_ROOM_COUNT] = {
    { &_gNeoArkShrineBaseCollisionGrid, _gNeoArkShrineViewBoundaryTriggers, _gNeoArkShrineBaseActionTriggers, _gNeoArkShrineSightOccluders },
    { &_gNeoArkShrineActiveCollisionGrid, _gNeoArkShrineViewBoundaryTriggers, _gNeoArkShrineActiveActionTriggers, _gNeoArkShrineSightOccluders },
    { &_gNeoArkShrineEnemyReleaseCollisionGrid, _gNeoArkShrineViewBoundaryTriggers, _gNeoArkShrineEnemyReleaseActionTriggers, _gNeoArkShrineSightOccluders },
    { &_gNeoArkShrineBaseCollisionGrid, _gNeoArkShrineViewBoundaryTriggers, _gNeoArkShrineBaseActionTriggers, _gNeoArkShrineSightOccluders },
    { &_gNeoArkShrineActiveCollisionGrid, _gNeoArkShrineViewBoundaryTriggers, _gNeoArkShrineActiveActionTriggers, _gNeoArkShrineSightOccluders },
    { &_gNeoArkShrineEnemyReleaseCollisionGrid, _gNeoArkShrineViewBoundaryTriggers, _gNeoArkShrineEnemyReleaseActionTriggers, _gNeoArkShrineSightOccluders },
};

WorldCoordRoomLighting gNeoArkShrineRoomLighting[6] = {
    { &_gNeoArkShrineRoomLights, NULL },
    { &_gNeoArkShrineRoomLights, NULL },
    { &_gNeoArkShrineRoomLights, NULL },
    { &_gNeoArkShrineRoomLights, NULL },
    { &_gNeoArkShrineRoomLights, NULL },
    { &_gNeoArkShrineRoomLights, NULL },
};

/// Room 2's logical-view map, substituting mapped views 16 and 18 for views 4 and 5.
///
/// The first eighteen bytes map logical view - 1 to a 1-based resource index.
/// The two retained trailing zeros are outside the lookup and reverse-search
/// domain; they are not a terminator.
static u8 _gNeoArkShrineRoom2ViewMap[] = {
    1,
    2,
    3,
    16,
    18,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    0,
    0,
};

/// Logical-view map shared by rooms 4 and 6, exchanging mapped views 10 and 17.
///
/// Eighteen logical entries followed by two retained, unsearched zero bytes.
static u8 _gNeoArkShrineRoom4ViewMap[] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    17,
    11,
    12,
    13,
    14,
    15,
    16,
    10,
    18,
    0,
    0,
};

/// Room 5's logical-view map, combining the room-2 substitutions and room-4 exchange.
///
/// Eighteen logical entries followed by two retained, unsearched zero bytes.
static u8 _gNeoArkShrineRoom5ViewMap[] = {
    1,
    2,
    3,
    16,
    18,
    6,
    7,
    8,
    9,
    17,
    11,
    12,
    13,
    14,
    15,
    16,
    10,
    18,
    0,
    0,
};

u8* gNeoArkShrineViewMaps[6] = {
    gViewIdentityMap,
    _gNeoArkShrineRoom2ViewMap,
    gViewIdentityMap,
    _gNeoArkShrineRoom4ViewMap,
    _gNeoArkShrineRoom5ViewMap,
    _gNeoArkShrineRoom4ViewMap,
};

ViewCount gNeoArkShrineViewCounts[NEO_ARK_SHRINE_ROOM_COUNT] = {
    NEO_ARK_SHRINE_MAPPED_VIEW_COUNT,
    NEO_ARK_SHRINE_MAPPED_VIEW_COUNT,
    NEO_ARK_SHRINE_MAPPED_VIEW_COUNT,
    NEO_ARK_SHRINE_MAPPED_VIEW_COUNT,
    NEO_ARK_SHRINE_MAPPED_VIEW_COUNT,
    NEO_ARK_SHRINE_MAPPED_VIEW_COUNT,
};

/// Door sound events in the shrine's loaded area bank.
enum {
    NEO_ARK_SHRINE_POWER_PLANT_1_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 4),
    NEO_ARK_SHRINE_POWER_PLANT_1_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 3),
    NEO_ARK_SHRINE_POWER_PLANT_1_BLOCKED_SOUND   = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 5),
    NEO_ARK_SHRINE_SAVANNA_ZONE_ARRIVAL_SOUND    = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 2),
    NEO_ARK_SHRINE_SAVANNA_ZONE_DEPARTURE_SOUND  = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 1),
};

DirectionWarpEntry gNeoArkShrineWarpEntries[NEO_ARK_SHRINE_WARP_COUNT] = {
    {
        .player         = { { .word = 1024 }, 525, 0, 1200 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 1024 }, 525, 0, 1200 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = NEO_ARK_SHRINE_POWER_PLANT_1_ARRIVAL_SOUND,
        .departureSound = NEO_ARK_SHRINE_POWER_PLANT_1_DEPARTURE_SOUND,
        .blockedSound   = NEO_ARK_SHRINE_POWER_PLANT_1_BLOCKED_SOUND,
        .initialView    = 9,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = GAME_FLAG_MAP_MARK_SHRINE,
    },
    {
        .player         = { { .word = 3072 }, 13000, 0, 1530 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 3072 }, 13000, 0, 1530 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = NEO_ARK_SHRINE_SAVANNA_ZONE_ARRIVAL_SOUND,
        .departureSound = NEO_ARK_SHRINE_SAVANNA_ZONE_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { { .word = 2048 }, 6950, 0, -3700 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 2048 }, 6950, 0, -3700 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = NEO_ARK_SHRINE_ROOM_VIEW,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Cell dimensions of the base-layout collision grid's X-major directory.
enum {
    NEO_ARK_SHRINE_BASE_COLLISION_CELLS_X = 4,
    NEO_ARK_SHRINE_BASE_COLLISION_CELLS_Z = 3,
};

/// Base-layout collision normals in room axes, with ONE for unit length.
static SVECTOR _gNeoArkShrineBaseCollisionGridNormals[] = {
#include "assets/neo_ark_shrine_collision_0576C_normals.inc"
};

/// Base-layout collision vertices in signed room-coordinate units.
static SVECTOR _gNeoArkShrineBaseCollisionGridVerts[] = {
#include "assets/neo_ark_shrine_collision_0576C_verts.inc"
};

/// Base-layout indexed quads, selecting the shrine's surface classes 0 and 2.
static WorldCollisionGridFace _gNeoArkShrineBaseCollisionGridFaces[] = {
#include "assets/neo_ark_shrine_collision_0576C_faces.inc"
};

/// Packed face-index lists for the base layout, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gNeoArkShrineBaseCollisionGridCells[] = {
#include "assets/neo_ark_shrine_collision_0576C_cells.inc"
};

/// Binds a generated base-grid table entry to its packed face-list start.
///
/// halfwordOffset is an element offset into the captured s16 pool, evaluated
/// once; it must lie inside that pool at a sentinel-terminated list start.
/// Configure collisionSource.cell_face_list_macro to this identifier and
/// keep the binding through the table include, then undefine it.
#define NEO_ARK_SHRINE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkShrineBaseCollisionGridCells[(halfwordOffset)])
/// X-major directory of base-layout face lists, indexed as x * 3 + z.
static s16* _gNeoArkShrineBaseCollisionGridTable[NEO_ARK_SHRINE_BASE_COLLISION_CELLS_X * NEO_ARK_SHRINE_BASE_COLLISION_CELLS_Z] = {
#include "assets/neo_ark_shrine_collision_0576C_table.inc"
};
#undef NEO_ARK_SHRINE_CELL_FACE_LIST_AT_OFFSET

/// Mutable collision mesh for the shrine's base layout, used by room selectors 1 and 4.
///
/// Its twelve 4000-unit cells begin at X = 0, Z = -5000. Activation borrows
/// the room-owned pools and binds viewCoord to the active view coordinate.
static WorldCollisionGrid _gNeoArkShrineBaseCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkShrineBaseCollisionGridNormals,
    .vertices    = _gNeoArkShrineBaseCollisionGridVerts,
    .faces       = _gNeoArkShrineBaseCollisionGridFaces,
    .cellFaceIds = _gNeoArkShrineBaseCollisionGridTable,
    .xBias       = 0,
    .zBias       = 5000,
    .cellCountX  = NEO_ARK_SHRINE_BASE_COLLISION_CELLS_X,
    .cellCountZ  = NEO_ARK_SHRINE_BASE_COLLISION_CELLS_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gNeoArkShrineBaseCollisionGridFaces),
};

/// Cell dimensions of the active-layout collision grid's X-major directory.
enum {
    NEO_ARK_SHRINE_ACTIVE_COLLISION_CELLS_X = 4,
    NEO_ARK_SHRINE_ACTIVE_COLLISION_CELLS_Z = 3,
};

/// Active-layout collision normals in room axes, with ONE for unit length.
static SVECTOR _gNeoArkShrineActiveCollisionGridNormals[] = {
#include "assets/neo_ark_shrine_collision_05C18_normals.inc"
};

/// Active-layout collision vertices in signed room-coordinate units.
static SVECTOR _gNeoArkShrineActiveCollisionGridVerts[] = {
#include "assets/neo_ark_shrine_collision_05C18_verts.inc"
};

/// Active-layout indexed quads, selecting the shrine's surface classes 1 and 2.
static WorldCollisionGridFace _gNeoArkShrineActiveCollisionGridFaces[] = {
#include "assets/neo_ark_shrine_collision_05C18_faces.inc"
};

/// Packed active-layout face lists, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gNeoArkShrineActiveCollisionGridCells[] = {
#include "assets/neo_ark_shrine_collision_05C18_cells.inc"
};

/// Binds a generated active-grid entry to its packed face-list start.
///
/// halfwordOffset is an in-bounds s16 element offset, evaluated once, into
/// the captured active-grid pool at a sentinel-terminated list start. The
/// collisionSource binding name and the table include must agree; undefine
/// after the include.
#define NEO_ARK_SHRINE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkShrineActiveCollisionGridCells[(halfwordOffset)])
/// X-major directory of active-layout face lists, indexed as x * 3 + z.
static s16* _gNeoArkShrineActiveCollisionGridTable[NEO_ARK_SHRINE_ACTIVE_COLLISION_CELLS_X * NEO_ARK_SHRINE_ACTIVE_COLLISION_CELLS_Z] = {
#include "assets/neo_ark_shrine_collision_05C18_table.inc"
};
#undef NEO_ARK_SHRINE_CELL_FACE_LIST_AT_OFFSET

/// Mutable collision mesh for the puzzle's active layout, used by room selectors 2 and 5.
///
/// Twelve 4000-unit cells begin at X = 0, Z = -5000. Activation borrows
/// the room-owned pools and binds viewCoord to the active view coordinate.
static WorldCollisionGrid _gNeoArkShrineActiveCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkShrineActiveCollisionGridNormals,
    .vertices    = _gNeoArkShrineActiveCollisionGridVerts,
    .faces       = _gNeoArkShrineActiveCollisionGridFaces,
    .cellFaceIds = _gNeoArkShrineActiveCollisionGridTable,
    .xBias       = 0,
    .zBias       = 5000,
    .cellCountX  = NEO_ARK_SHRINE_ACTIVE_COLLISION_CELLS_X,
    .cellCountZ  = NEO_ARK_SHRINE_ACTIVE_COLLISION_CELLS_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gNeoArkShrineActiveCollisionGridFaces),
};

/// Cell dimensions of the enemy-release collision grid's X-major directory.
enum {
    NEO_ARK_SHRINE_ENEMY_RELEASE_COLLISION_CELLS_X = 4,
    NEO_ARK_SHRINE_ENEMY_RELEASE_COLLISION_CELLS_Z = 3,
};

/// Enemy-release collision normals in room axes, with ONE for unit length.
static SVECTOR _gNeoArkShrineEnemyReleaseCollisionGridNormals[] = {
#include "assets/neo_ark_shrine_collision_060D8_normals.inc"
};

/// Enemy-release collision vertices in signed room-coordinate units.
static SVECTOR _gNeoArkShrineEnemyReleaseCollisionGridVerts[] = {
#include "assets/neo_ark_shrine_collision_060D8_verts.inc"
};

/// Enemy-release indexed quads, selecting the shrine's surface classes 1 and 2.
static WorldCollisionGridFace _gNeoArkShrineEnemyReleaseCollisionGridFaces[] = {
#include "assets/neo_ark_shrine_collision_060D8_faces.inc"
};

/// Packed enemy-release face lists, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gNeoArkShrineEnemyReleaseCollisionGridCells[] = {
#include "assets/neo_ark_shrine_collision_060D8_cells.inc"
};

/// Binds a generated enemy-release-grid entry to its packed face-list start.
///
/// halfwordOffset is an in-bounds s16 element offset, evaluated once, into
/// the captured enemy-release pool at a sentinel-terminated list start. The
/// collisionSource binding name and the table include must agree; undefine
/// after the include.
#define NEO_ARK_SHRINE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkShrineEnemyReleaseCollisionGridCells[(halfwordOffset)])
/// X-major directory of enemy-release face lists, indexed as x * 3 + z.
static s16* _gNeoArkShrineEnemyReleaseCollisionGridTable[NEO_ARK_SHRINE_ENEMY_RELEASE_COLLISION_CELLS_X * NEO_ARK_SHRINE_ENEMY_RELEASE_COLLISION_CELLS_Z] = {
#include "assets/neo_ark_shrine_collision_060D8_table.inc"
};
#undef NEO_ARK_SHRINE_CELL_FACE_LIST_AT_OFFSET

/// Mutable collision mesh for the enemy-release layout, used by room selectors 3 and 6.
///
/// The puzzle publishes selector 6 after releasing enemies; the directory
/// also retains selector 3. Twelve 4000-unit cells begin at X = 0, Z = -5000.
/// Pools remain room-owned; activation binds viewCoord to the current view.
static WorldCollisionGrid _gNeoArkShrineEnemyReleaseCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkShrineEnemyReleaseCollisionGridNormals,
    .vertices    = _gNeoArkShrineEnemyReleaseCollisionGridVerts,
    .faces       = _gNeoArkShrineEnemyReleaseCollisionGridFaces,
    .cellFaceIds = _gNeoArkShrineEnemyReleaseCollisionGridTable,
    .xBias       = 0,
    .zBias       = 5000,
    .cellCountX  = NEO_ARK_SHRINE_ENEMY_RELEASE_COLLISION_CELLS_X,
    .cellCountZ  = NEO_ARK_SHRINE_ENEMY_RELEASE_COLLISION_CELLS_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gNeoArkShrineEnemyReleaseCollisionGridFaces),
};

ViewCamera gNeoArkShrineViewCameras[NEO_ARK_SHRINE_MAPPED_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -7000, 0x5A28, -1050 } }, 329 },
    { { { { 642, 0, -4045 }, { -628, 4046, -99 }, { 3996, 636, 634 } }, { -7100, 1660, -950 } }, 329 },
    { { { { 617, 0, 4049 }, { 342, 4081, -52 }, { -4034, 346, 615 } }, { -0x32C8, 1390, -860 } }, 275 },
    { { { { 4049, 0, -613 }, { -103, 4037, -681 }, { 604, 689, 3991 } }, { -6330, 1660, 4750 } }, 257 },
    { { { { 4082, 0, -331 }, { -43, 4061, -529 }, { 329, 531, 4047 } }, { -6720, 1475, 5 } }, 329 },
    { { { { -574, 0, -4055 }, { -1, ONE, 0 }, { 4055, 1, -574 } }, { 1295, 970, -5540 } }, 329 },
    { { { { -802, 0, 4016 }, { 90, 4094, 18 }, { -4015, 92, -801 } }, { -6180, 1120, -5590 } }, 329 },
    { { { { 4022, 0, -772 }, { -101, 4060, -528 }, { 765, 537, 3987 } }, { -365, 1325, 425 } }, 282 },
    { { { { -4086, 0, -277 }, { -137, 3557, 2026 }, { 240, 2030, -3548 } }, { -610, 2540, -4060 } }, 282 },
    { { { { -4063, 0, -515 }, { -123, 3977, 970 }, { 500, 978, -3945 } }, { -6730, 1660, -480 } }, 225 },
    { { { { -ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, -ONE } }, { -7000, 1554, 3630 } }, 541 },
    { { { { 4085, 0, -297 }, { -133, 3662, -1828 }, { 265, 1832, 3653 } }, { -6700, 2300, -1490 } }, 348 },
    { { { { -3829, 0, -1452 }, { 701, 3587, -1848 }, { 1272, -1977, -3353 } }, { -7890, 1790, 2780 } }, 312 },
    { { { { 3986, 0, -938 }, { 71, 4084, 303 }, { 936, -312, 3975 } }, { -6140, 980, 4450 } }, 269 },
    { { { { 407, 0, -4075 }, { -498, 4065, -49 }, { 4045, 500, 404 } }, { -900, 1100, -770 } }, 380 },
    { { { { 4049, 0, -613 }, { -103, 4037, -681 }, { 604, 689, 3991 } }, { -6330, 1660, 4750 } }, 257 },
    { { { { -4063, 0, -515 }, { -123, 3977, 970 }, { 500, 978, -3945 } }, { -6730, 1660, -480 } }, 225 },
    { { { { 4082, 0, -331 }, { -43, 4061, -529 }, { 329, 531, 4047 } }, { -6720, 1475, 5 } }, 329 },
};

/// Decoded-background selection and an empty sprite list for mapped shrine view 1.
static SpriteBatch _gNeoArkShrineView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Decoded-background selection and an empty sprite list for mapped shrine view 2.
static SpriteBatch _gNeoArkShrineView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored texture rectangles and sorting depths for mapped shrine view 3.
static SpriteSource _gNeoArkShrineView3SpriteSources[] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, -72, 1175, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 48 } }, -136, -120, 750, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 48 } }, -136, -72, 750, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -136, -24, 750, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -136, 24, 750, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -136, 72, 750, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, -24, 1062, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, -72, 1062, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, -120, 1062, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -72, -120, 1153, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, 24, 1062, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, 72, 1106, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -72, 40, 1153, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -72, 72, 1153, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, -120, 1187, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 1150, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 64, 72, 1162, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1150, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1150, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1150, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1150, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 64, -120, 1162, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 64, -72, 1162, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -24, 1162, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 24, 1162, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 40, 24, 1187, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, 40, 72, 1187, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, 56, -8, 1187, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 40, -120, 1187, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 48, -72, 1187, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 72, -56, 1950, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 72, 24, 1950, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -24, -8, 1950, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 24, -8, 1950, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -24, -56, 1950, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 24, -120, 1950, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 24, -104, 1950, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, -56, 1950, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, -104, 1950, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, -120, 1950, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 104, -120, 1950, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -56, 1950, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -8, 1950, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -88, -56, 1950, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -88, -8, 1950, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 462, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 462, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -160, -24, 462, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -160, -72, 462, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -160, -120, 462, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 48, 812, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -136, 56, 812, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, -88, 56, 812, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, 8, 812, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, -40, 812, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, -88, 812, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -136, -104, 812, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
};

/// Five sprite ranges covering mapped shrine view 3's sources over its decoded background.
///
/// The zero-count first record selects the background; `SPRITE_BATCH_END`
/// terminates the list. The final two bytes of each record retain authored
/// metadata whose meaning is unproven.
static SpriteBatch _gNeoArkShrineView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { 14, 16, 0, 0, { 3, 0 } },
    { 30, 15, 0, 0, { 2, 0 } },
    { 45, 5, 0, 0, { 4, 0 } },
    { 50, ARRAY_SIZE(_gNeoArkShrineView3SpriteSources) - 50, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored texture rectangles and sorting depths for mapped shrine view 4.
static SpriteSource _gNeoArkShrineView4SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -120, 1200, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -72, 1200, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1087, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1087, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1087, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1087, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -120, 1125, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -72, 1125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -24, 1125, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 24, 1125, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -120, 1187, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -72, 1187, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -24, 1187, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 48, 24, 1187, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 24, 1200, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, -120, 1200, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 1062, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 987, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 987, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 987, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 987, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -160, 72, 987, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -120, 1000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -72, 1000, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -24, 1000, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, 24, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -112, 72, 1000, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -72, -120, 1075, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -72, -72, 1062, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -72, -24, 1062, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -72, 24, 1062, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -56, 0, 1075, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, 24, 1075, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -80, -32, 2125, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -80, -80, 2125, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -72, -120, 2125, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
};

/// Three sprite ranges covering mapped shrine view 4's sources over its decoded background.
///
/// The first record selects background strips; the last ends the list.
/// The final two bytes of each record retain metadata of unproven meaning.
static SpriteBatch _gNeoArkShrineView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 1, 0 } },
    { 16, 17, 0, 0, { 2, 0 } },
    { 33, ARRAY_SIZE(_gNeoArkShrineView4SpriteSources) - 33, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored texture rectangles and sorting depths for mapped shrine view 5.
static SpriteSource _gNeoArkShrineView5SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 862, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 862, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 862, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 862, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -160, 72, 862, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -112, 72, 1000, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 24, 1000, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 0, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, -120, 1000, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
};

/// One range covering mapped shrine view 5's sources over its decoded background.
static SpriteBatch _gNeoArkShrineView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkShrineView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored texture rectangles and sorting depths for mapped shrine view 6.
static SpriteSource _gNeoArkShrineView6SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, 24, 987, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, -32, 987, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, -88, 987, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 32 } }, 104, -120, 987, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 64, 24, 987, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 64, -32, 987, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 64, -88, 987, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 64, -120, 987, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, 32, -120, 1025, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, 0, -120, 1025, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 40, 950, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, 40, 950, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -8, 950, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -56, 950, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -104, 950, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -160, -120, 950, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -112, -8, 950, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -56, 950, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -104, 950, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -112, -120, 950, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -72, -120, 950, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -32, -120, 950, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -56, 1950, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 40, 1950, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, 40, 1950, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, -8, 1950, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, -56, 1950, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 24, -104, 1950, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 24, -120, 1950, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -8, -120, 1950, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 48 } }, -8, -104, 1950, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, -120, 1950, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, -104, 1950, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -40, -64, 3125, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -40, -16, 3125, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 8, -16, 3125, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 8, -64, 3125, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 40, 8, 1750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 48, -40, 1750, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, 48, -72, 1750, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -112, 600, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -104, 600, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -160, -120, 600, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, -112, -104, 600, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -144, 40, 550, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -144, -8, 550, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -144, -56, 550, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -128, -72, 550, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, -24, 550, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -96, 40, 550, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -96, 24, 550, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -8, 550, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -48, 48, 550, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, 72, 625, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -136, 24, 625, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -136, -24, 625, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, -136, -48, 625, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -88, -8, 625, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -88, 24, 625, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 40 } }, -88, 72, 625, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 40, 625, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
};

/// Eight sprite ranges covering mapped shrine view 6's sources over its decoded background.
///
/// The first record selects background strips; the last ends the list.
/// The final two bytes of each record retain metadata of unproven meaning.
static SpriteBatch _gNeoArkShrineView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 6, 0 } },
    { 10, 12, 0, 0, { 1, 0 } },
    { 22, 11, 0, 0, { 4, 0 } },
    { 33, 4, 0, 0, { 0, 0 } },
    { 37, 3, 0, 0, { 5, 0 } },
    { 40, 4, 0, 0, { 3, 0 } },
    { 44, 9, 0, 0, { 7, 0 } },
    { 53, ARRAY_SIZE(_gNeoArkShrineView6SpriteSources) - 53, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Decoded-background selection and an empty sprite list for mapped shrine view 7.
static SpriteBatch _gNeoArkShrineView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored texture rectangles and sorting depths for mapped shrine view 8.
static SpriteSource _gNeoArkShrineView8SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 925, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 925, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 925, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 925, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 24, 925, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -24, 925, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -72, 925, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -120, 925, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, 24, 1125, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 24, 1137, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -24, 1125, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -72, 1125, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -120, 1125, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 24, -120, 1125, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 24, -72, 1125, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -24, 1125, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
};

/// One range covering mapped shrine view 8's sources over its decoded background.
static SpriteBatch _gNeoArkShrineView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkShrineView8SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored texture rectangles and sorting depths for mapped shrine view 9.
static SpriteSource _gNeoArkShrineView9SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 937, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 937, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 937, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 937, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 937, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -120, 1125, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -72, 1125, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -24, 1125, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, 24, 1125, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 72, 1125, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -120, 1275, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -72, 1275, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -24, 1275, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, 24, 1275, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, -96, 1425, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -72, 1425, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -24, 1425, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 24, 1425, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

/// One range covering mapped shrine view 9's sources over its decoded background.
static SpriteBatch _gNeoArkShrineView9SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkShrineView9SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Authored texture rectangles and sorting depths for mapped shrine view 10.
static SpriteSource _gNeoArkShrineView10SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 450, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 104, 450, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 32, 450, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 0, 450, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -40, 450, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -80, 450, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, -104, 450, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 450, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 32, 450, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 0, 450, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -32, 450, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -72, 450, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -104, 450, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 104, 450, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
};

/// Two sprite ranges covering mapped shrine view 10's sources over its decoded background.
///
/// The first record selects background strips; the last ends the list.
/// The final two bytes of each record retain metadata of unproven meaning.
static SpriteBatch _gNeoArkShrineView10SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, ARRAY_SIZE(_gNeoArkShrineView10SpriteSources) - 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Decoded-background selection and an empty sprite list for mapped shrine view 11.
static SpriteBatch _gNeoArkShrineView11SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Decoded-background selection and an empty sprite list for mapped shrine view 12.
static SpriteBatch _gNeoArkShrineView12SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Encoded 8-bit texture pages, row-255 palette and neutral opaque sprite modulation.
enum {
    NEO_ARK_SHRINE_SPRITE_PAGE_832          = 141,
    NEO_ARK_SHRINE_SPRITE_PAGE_896          = 142,
    NEO_ARK_SHRINE_SPRITE_PAGE_960          = 143,
    NEO_ARK_SHRINE_SPRITE_PALETTE           = 0x3FC0,
    NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB       = 128,
    NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION = 0,
};

/// Foreground rectangles for mapped shrine view 13, with centre-relative pixel positions and OT depths.
///
/// Texture coordinates select 8-bit page (960, 0) and CLUT row 255. Cached
/// drawing snapshots rectangle geometry while retaining mutable sorting depths.
static SpriteSource _gNeoArkShrineView13SpriteSources[] = {
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 8, 8 } }, -112, -48, 575, { .fields = { 48, 248 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 32 } }, -160, -120, 475, { .fields = { 80, 192 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 32 } }, -112, -120, 475, { .fields = { 80, 160 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 32 } }, -64, -120, 475, { .fields = { 56, 64 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 32 } }, -16, -120, 475, { .fields = { 80, 224 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 32 } }, 32, -120, 475, { .fields = { 80, 128 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 40, 24 } }, 80, -120, 475, { .fields = { 40, 120 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 40, 16 } }, 120, -120, 475, { .fields = { 32, 48 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 24 } }, -160, -88, 525, { .fields = { 32, 144 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 24 } }, -112, -88, 525, { .fields = { 32, 192 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 24 } }, -64, -88, 525, { .fields = { 32, 168 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 24 } }, -16, -88, 525, { .fields = { 32, 24 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 8 } }, 32, -88, 525, { .fields = { 48, 112 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 16 } }, -160, -64, 575, { .fields = { 32, 216 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 16 } }, -112, -64, 575, { .fields = { 16, 96 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 24, 8 } }, -64, -64, 575, { .fields = { 56, 248 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 16 } }, -160, -48, 575, { .fields = { 8, 80 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 32 } }, 88, -120, 475, { .fields = { 80, 32 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 32, 32 } }, 56, -120, 475, { .fields = { 96, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 32 } }, 88, -88, 475, { .fields = { 80, 96 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 24, 32 } }, 64, -88, 475, { .fields = { 104, 64 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 24 } }, 88, -56, 525, { .fields = { 48, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 24 } }, 72, -56, 525, { .fields = { 64, 96 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 16 } }, 88, -32, 550, { .fields = { 32, 232 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 8, 16 } }, 80, -32, 550, { .fields = { 72, 48 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 16 } }, 88, -16, 562, { .fields = { 8, 64 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 8, 16 } }, 80, -16, 562, { .fields = { 40, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 8 } }, 88, 0, 575, { .fields = { 80, 24 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
};

/// Two sprite ranges covering mapped shrine view 13 over its decoded background.
///
/// Ranges split after source 16; the terminal record contributes no sprites.
/// The final two serialized bytes of each record retain an unproven role.
static SpriteBatch _gNeoArkShrineView13SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 1, 0 } },
    { 17, ARRAY_SIZE(_gNeoArkShrineView13SpriteSources) - 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Decoded-background selection and an empty sprite list for mapped shrine view 14.
static SpriteBatch _gNeoArkShrineView14SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Decoded-background selection and an empty sprite list for mapped shrine view 15.
static SpriteBatch _gNeoArkShrineView15SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground rectangles for mapped shrine view 16, with centre-relative pixel positions and OT depths.
///
/// Texture coordinates select 8-bit pages (896, 0) and (960, 0), with CLUT
/// row 255. Rooms 2 and 5 also map logical view 4 to this background.
static SpriteSource _gNeoArkShrineView16SpriteSources[] = {
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, -160, 32, 962, { .fields = { 24, 144 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, -160, -16, 962, { .fields = { 72, 192 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, -112, 32, 962, { .fields = { 80, 48 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, -112, -16, 962, { .fields = { 80, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, -160, -64, 962, { .fields = { 32, 48 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, -160, -112, 962, { .fields = { 32, 96 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 8 } }, -160, -120, 962, { .fields = { 64, 248 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, -112, -64, 962, { .fields = { 72, 144 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, -112, -112, 962, { .fields = { 80, 96 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 8 } }, -112, -120, 962, { .fields = { 64, 240 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 40 } }, -64, 32, 962, { .fields = { 72, 48 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 48 } }, -64, -16, 962, { .fields = { 64, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 8, 48 } }, -64, -64, 962, { .fields = { 120, 192 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 8, 24 } }, -64, -88, 962, { .fields = { 80, 224 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, 112, 16, 1000, { .fields = { 24, 192 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, 112, -32, 1000, { .fields = { 16, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, 112, -80, 1000, { .fields = { 104, 192 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 40 } }, 112, -120, 1000, { .fields = { 64, 144 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 40, 48 } }, 72, 16, 1000, { .fields = { 120, 96 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 40, 48 } }, 72, -32, 1000, { .fields = { 112, 144 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 40, 48 } }, 72, -80, 1000, { .fields = { 120, 48 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 40, 40 } }, 72, -120, 1000, { .fields = { 64, 184 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 48 } }, 56, 16, 1112, { .fields = { 0, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 40 } }, 40, 16, 1250, { .fields = { 88, 96 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 48 } }, 56, -32, 1112, { .fields = { 112, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 8, 48 } }, 48, -32, 1250, { .fields = { 120, 144 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 48 } }, 56, -80, 1112, { .fields = { 104, 96 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 16 } }, 40, -80, 1250, { .fields = { 112, 240 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 40 } }, 56, -120, 1125, { .fields = { 56, 40 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 40 } }, 40, -120, 1250, { .fields = { 64, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 32 } }, 24, -120, 1250, { .fields = { 88, 224 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 32, 48 } }, -80, -32, 1950, { .fields = { 80, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 32, 48 } }, -80, -80, 1950, { .fields = { 88, 48 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_896, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 24, 40 } }, -72, -120, 1950, { .fields = { 64, 88 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
};

/// Three sprite ranges covering mapped shrine view 16 over its decoded background.
///
/// Ranges split at source indices 14 and 31; the terminal record contributes
/// no sprites. The final two serialized bytes retain an unproven role.
static SpriteBatch _gNeoArkShrineView16SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 1, 0 } },
    { 14, 17, 0, 0, { 2, 0 } },
    { 31, ARRAY_SIZE(_gNeoArkShrineView16SpriteSources) - 31, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground rectangles for mapped shrine view 17, with centre-relative pixel positions and OT depths.
///
/// Texture coordinates select 8-bit page (832, 0) and CLUT row 255. Rooms
/// 4..6 map logical view 10 to this background after the first enemy reveal.
static SpriteSource _gNeoArkShrineView17SpriteSources[] = {
    { NEO_ARK_SHRINE_SPRITE_PAGE_832, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, -160, 72, 475, { .fields = { 0, 176 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_832, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 40, 40 } }, -160, 32, 475, { .fields = { 0, 136 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_832, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 32, 32 } }, -160, 0, 475, { .fields = { 0, 104 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_832, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 24, 32 } }, -160, -32, 475, { .fields = { 0, 72 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_832, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 40 } }, -160, -72, 475, { .fields = { 0, 32 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_832, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 8, 32 } }, -160, -104, 475, { .fields = { 0, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_832, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 8, 16 } }, -112, 104, 485, { .fields = { 48, 208 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_832, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, 112, 72, 475, { .fields = { 8, 176 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_832, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 40, 40 } }, 120, 32, 475, { .fields = { 16, 136 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_832, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 32, 32 } }, 128, 0, 475, { .fields = { 24, 104 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_832, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 24, 32 } }, 136, -32, 475, { .fields = { 32, 72 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_832, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 8, 32 } }, 152, -104, 475, { .fields = { 48, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_832, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 40 } }, 144, -72, 475, { .fields = { 40, 32 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_832, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 8, 16 } }, 104, 104, 475, { .fields = { 0, 208 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
};

/// Two sprite ranges covering mapped shrine view 17 over its decoded background.
///
/// The ranges split at source index 7; the terminal record contributes no
/// sprites. The final two serialized bytes retain an unproven role.
static SpriteBatch _gNeoArkShrineView17SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, ARRAY_SIZE(_gNeoArkShrineView17SpriteSources) - 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground rectangles for mapped shrine view 18, with centre-relative pixel positions and OT depths.
///
/// Texture coordinates select 8-bit page (960, 0) and CLUT row 255. Rooms
/// 2 and 5 also map logical view 5 to this background.
static SpriteSource _gNeoArkShrineView18SpriteSources[] = {
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, -160, 72, 875, { .fields = { 80, 48 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 24, 48 } }, -112, 72, 875, { .fields = { 104, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 8, 16 } }, -88, 72, 1000, { .fields = { 120, 240 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, -160, 24, 875, { .fields = { 80, 96 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 16, 48 } }, -112, 24, 875, { .fields = { 88, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 8, 24 } }, -112, 0, 875, { .fields = { 72, 48 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, -160, -24, 875, { .fields = { 80, 192 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, -160, -72, 875, { .fields = { 80, 144 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 48, 48 } }, -160, -120, 875, { .fields = { 40, 0 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
    { NEO_ARK_SHRINE_SPRITE_PAGE_960, NEO_ARK_SHRINE_SPRITE_PALETTE, { .fields = { 24, 24 } }, -112, -120, 875, { .fields = { 56, 72 } }, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_NEUTRAL_RGB, NEO_ARK_SHRINE_SPRITE_OPAQUE_MODULATION },
};

/// One sprite range covering mapped shrine view 18 over its decoded background.
static SpriteBatch _gNeoArkShrineView18SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkShrineView18SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gNeoArkShrineSpriteViews[NEO_ARK_SHRINE_MAPPED_VIEW_COUNT] = {
    { { .empty = _gNeoArkShrineView1SpriteBatches }, _gNeoArkShrineView1SpriteBatches, NULL },
    { { .empty = _gNeoArkShrineView2SpriteBatches }, _gNeoArkShrineView2SpriteBatches, NULL },
    { { .elements = _gNeoArkShrineView3SpriteSources }, _gNeoArkShrineView3SpriteBatches, NULL },
    { { .elements = _gNeoArkShrineView4SpriteSources }, _gNeoArkShrineView4SpriteBatches, NULL },
    { { .elements = _gNeoArkShrineView5SpriteSources }, _gNeoArkShrineView5SpriteBatches, NULL },
    { { .elements = _gNeoArkShrineView6SpriteSources }, _gNeoArkShrineView6SpriteBatches, NULL },
    { { .empty = _gNeoArkShrineView7SpriteBatches }, _gNeoArkShrineView7SpriteBatches, NULL },
    { { .elements = _gNeoArkShrineView8SpriteSources }, _gNeoArkShrineView8SpriteBatches, NULL },
    { { .elements = _gNeoArkShrineView9SpriteSources }, _gNeoArkShrineView9SpriteBatches, NULL },
    { { .elements = _gNeoArkShrineView10SpriteSources }, _gNeoArkShrineView10SpriteBatches, NULL },
    { { .empty = _gNeoArkShrineView11SpriteBatches }, _gNeoArkShrineView11SpriteBatches, NULL },
    { { .empty = _gNeoArkShrineView12SpriteBatches }, _gNeoArkShrineView12SpriteBatches, NULL },
    { { .elements = _gNeoArkShrineView13SpriteSources }, _gNeoArkShrineView13SpriteBatches, NULL },
    { { .empty = _gNeoArkShrineView14SpriteBatches }, _gNeoArkShrineView14SpriteBatches, NULL },
    { { .empty = _gNeoArkShrineView15SpriteBatches }, _gNeoArkShrineView15SpriteBatches, NULL },
    { { .elements = _gNeoArkShrineView16SpriteSources }, _gNeoArkShrineView16SpriteBatches, NULL },
    { { .elements = _gNeoArkShrineView17SpriteSources }, _gNeoArkShrineView17SpriteBatches, NULL },
    { { .elements = _gNeoArkShrineView18SpriteSources }, _gNeoArkShrineView18SpriteBatches, NULL },
};

/// Two mutable directional lights contributing in every shrine view and room selector.
static WorldCoordLight _gNeoArkShrineDirectionalLights[] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -332, -462, -388 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 750, 745, 740 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 267, -272, 272 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 348, 286, 245 }, { 0, 0 } },
};

/// Seventeen mutable point lights shared by every shrine view and room selector.
///
/// Positions and falloff radii use world units; RGB intensities use twelve
/// fractional bits. Coordinate caches and attenuation remain writable.
static WorldCoordPointLight _gNeoArkShrinePointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1198, -2188, 3758 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4100, 4038, 3936 }, { 0, 0 } }, 832, 4161 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6232, -1290, -505 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1845, 1765, 1703 }, { 0, 0 } }, 1685, 3529 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6055, -1410, 3087 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1475, 1311, 1232 }, { 0, 0 } }, 992, 2101 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3560, -1430, 3820 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1843, 1515, 1187 }, { 0, 0 } }, 600, 1400 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4910, -1430, 3820 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2170, 1843 }, { 0, 0 } }, 900, 1180 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4579, -1430, 5537 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1621, 1496, 1375 }, { 0, 0 } }, 900, 1902 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3560, -1430, 6155 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1293, 1272, 1232 }, { 0, 0 } }, 363, 941 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7688, -1235, 4382 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1478, 1434, 1372 }, { 0, 0 } }, 1201, 1931 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2CEB, -1360, 713 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2460, 2170, 1844 }, { 0, 0 } }, 881, 1663 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2E28, -1320, 2500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2461, 2174, 1847 }, { 0, 0 } }, 1159, 1985 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x27BB, -1360, 2967 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0, 0, 0 }, { 0, 0 } }, 5, 357 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9402, -1360, 2037 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2458, 2171, 1844 }, { 0, 0 } }, 1182, 2084 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2869, -1360, 285 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 309, 226, 184 }, { 0, 0 } }, 150, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9060, -1360, 445 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2170, 1843 }, { 0, 0 } }, 900, 1603 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7005, -2049, -3922 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2376, 2338, 2276 }, { 0, 0 } }, 850, 3850 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -10, -2789, 1661 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4567, 4550, 4508 }, { 0, 0 } }, 2520, 6612 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -25, -2065, 1050 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1802, 1863, 1884 }, { 0, 0 } }, 200, 2200 },
};

/// Authored light collection shared by all six shrine room selectors, with no cone lights.
static WorldCoordRoomLights _gNeoArkShrineRoomLights = {
    .directionalLightCount = ARRAY_SIZE(_gNeoArkShrineDirectionalLights),
    .directionalLights     = _gNeoArkShrineDirectionalLights,
    .pointLightCount       = ARRAY_SIZE(_gNeoArkShrinePointLights),
    .pointLights           = _gNeoArkShrinePointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Directed view boundaries shared by every shrine layout.
///
/// Seven reciprocal pairs connect logical views 2/3, 3/4, 4/5, 4/10, 5/6,
/// 6/8 and 8/9. Positions/radii use room units and normals use ONE for unit
/// length. Setup binds, links and enables all fourteen mutable records,
/// including the final record marked WORLD_COLLISION_TRIGGER_LAST.
static WorldCollisionTrigger _gNeoArkShrineViewBoundaryTriggers[] = {
    { NULL, NULL, NULL, { 7039, -2432, -2210, 0 }, { { 2687, -2832, -10, 0 }, { -2698, -2832, 1, 0 }, { 2687, 2832, -10, 0 }, { -2698, 2832, 1, 0 } }, { 8, 0, 4099, 0 }, { 0, 0, ONE, 0 }, 3907, 0, 4, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7087, -2401, -2096, 0 }, { { -2713, -2800, 14, 0 }, { 2705, -2800, -23, 0 }, { -2713, 2800, 14, 0 }, { 2705, 2800, -23, 0 } }, { -29, 0, -4099, 0 }, { 0, 0, ONE, 0 }, 3890, 0, 10, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8224, -2369, 1312, 0 }, { { 0, -2832, 1984, 0 }, { 0, -2832, -1984, 0 }, { 0, 2832, 1984, 0 }, { 0, 2832, -1984, 0 } }, { -4112, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3453, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8079, -2481, 1391, 0 }, { { 4, -2784, -1833, 0 }, { -3, -2784, 1834, 0 }, { 4, 2784, -1833, 0 }, { -3, 2784, 1834, 0 } }, { 4111, 0, 7, 0 }, { 0, 0, ONE, 0 }, 3328, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x28D1, -2416, 1408, 0 }, { { 208, -2880, -2032, 0 }, { -208, -2880, 2032, 0 }, { 208, 2880, -2032, 0 }, { -208, 2880, 2032, 0 } }, { 4088, 0, 418, 0 }, { 0, 0, ONE, 0 }, 3528, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2960, -2416, 1504, 0 }, { { -224, -2816, 2144, 0 }, { 224, -2816, -2144, 0 }, { -224, 2816, 2144, 0 }, { 224, 2816, -2144, 0 } }, { -4074, 0, -426, 0 }, { 0, 0, ONE, 0 }, 3537, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6767, -2545, 2671, 0 }, { { 1946, -2848, 68, 0 }, { -1945, -2848, -67, 0 }, { 1946, 2849, 68, 0 }, { -1945, 2849, -67, 0 } }, { -143, 0, 4108, 0 }, { 0, 0, ONE, 0 }, 3444, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6751, -2496, 2751, 0 }, { { -1810, -2800, -67, 0 }, { 1810, -2800, 68, 0 }, { -1810, 2800, -67, 0 }, { 1810, 2800, 68, 0 } }, { 152, 0, -4106, 0 }, { 0, 0, ONE, 0 }, 3328, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1535, -2496, 5119, 0 }, { { 408, -2864, -2184, 0 }, { -418, -2864, 2174, 0 }, { 408, 2864, -2184, 0 }, { -418, 2864, 2174, 0 } }, { 4033, 0, 764, 0 }, { 0, 0, ONE, 0 }, 3620, 0, 6, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1630, -2480, 5215, 0 }, { { -415, -2848, 2178, 0 }, { 411, -2848, -2181, 0 }, { -415, 2848, 2178, 0 }, { 411, 2848, -2181, 0 } }, { -4028, 0, -764, 0 }, { 0, 0, ONE, 0 }, 3602, 0, 8, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1054, -2400, 2080, 0 }, { { -2215, -2928, 128, 0 }, { 2215, -2928, -129, 0 }, { -2215, 2928, 128, 0 }, { 2215, 2928, -129, 0 } }, { -238, 0, -4097, 0 }, { 0, 0, ONE, 0 }, 3665, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1086, -2384, 1984, 0 }, { { 2215, -2848, -129, 0 }, { -2215, -2848, 128, 0 }, { 2215, 2848, -129, 0 }, { -2215, 2848, 128, 0 } }, { 237, 0, 4092, 0 }, { 0, 0, ONE, 0 }, 3602, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5631, -2368, 5472, 0 }, { { -71, -2848, 1942, 0 }, { 64, -2848, -1948, 0 }, { -71, 2849, 1942, 0 }, { 64, 2849, -1948, 0 } }, { -4109, 0, -143, 0 }, { 0, 0, ONE, 0 }, 3444, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5535, -2464, 5472, 0 }, { { 65, -2848, -1947, 0 }, { -70, -2848, 1943, 0 }, { 65, 2849, -1947, 0 }, { -70, 2849, 1943, 0 } }, { 4107, 0, 141, 0 }, { 0, 0, ONE, 0 }, 3444, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Shrine-specific action and command IDs and packed door endpoint selectors.
///
/// Warp selectors pack the shrine departure slot in the high nibble and the
/// destination arrival slot in the low nibble. CAP command numbers address
/// the loaded shrine command table; room-message command IDs use that
/// handler's independent domain even where their numeric values agree.
enum {
    NEO_ARK_SHRINE_WARP_TO_POWER_PLANT_1 = (1 << 4) | 1,
    NEO_ARK_SHRINE_WARP_TO_SAVANNA_ZONE  = (2 << 4) | 1,
    NEO_ARK_SHRINE_ACTION_OPEN_PUZZLE    = 1,
    NEO_ARK_SHRINE_COMMAND_POWER_PLANT_1 = 5,
    NEO_ARK_SHRINE_COMMAND_OBJECT_EVENT  = 7,
    NEO_ARK_SHRINE_CAP_COMMAND_6         = 6,
    NEO_ARK_SHRINE_CAP_COMMAND_8         = 8,
    NEO_ARK_SHRINE_CAP_COMMAND_10        = 10,
    NEO_ARK_SHRINE_CAP_COMMAND_11        = 11,
};

/// Facing-gated interaction quads for base-layout room selectors 1 and 4.
///
/// Includes both door warps, the puzzle action and six CAP interactions.
/// Positions/radii are room units, normals use ONE, and setup owns the
/// temporary links, coordinate bindings and hit latches. The ninth record
/// carries WORLD_COLLISION_TRIGGER_LAST; storage stays borrowed until unlink.
static WorldCollisionTrigger _gNeoArkShrineBaseActionTriggers[] = {
    { NULL, NULL, NULL, { 400, -48, 1088, 0 }, { { -496, 0, -768, 0 }, { 496, 0, -768, 0 }, { -496, 0, 768, 0 }, { 496, 0, 768, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 914, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_POWER_PLANT_1, NEO_ARK_SHRINE_WARP_TO_POWER_PLANT_1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3100, -48, 1504, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { -ONE, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_SAVANNA_ZONE, NEO_ARK_SHRINE_WARP_TO_SAVANNA_ZONE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7008, -64, -4224, 0 }, { { -400, 0, -320, 0 }, { 400, 0, -320, 0 }, { -400, 0, 320, 0 }, { 400, 0, 320, 0 } }, { 0, 4095, 0, 0 }, { 151, 0, 4093, 0 }, 512, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, NEO_ARK_SHRINE_ACTION_OPEN_PUZZLE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1728, -64, 976, 0 }, { { -496, 0, -656, 0 }, { 496, 0, -656, 0 }, { -496, 0, 656, 0 }, { 496, 0, 656, 0 } }, { 0, 4098, 0, 0 }, { -ONE, 0, 0, 0 }, 822, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_COMMAND_POWER_PLANT_1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6992, -64, 5616, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -ONE, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_CAP_COMMAND_8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5216, -64, -3840, 0 }, { { -496, 0, -1184, 0 }, { 496, 0, -1184, 0 }, { -496, 0, 1184, 0 }, { 496, 0, 1184, 0 } }, { 0, 4099, 0, 0 }, { ONE, 0, 0, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_CAP_COMMAND_10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8864, -64, -3840, 0 }, { { -496, 0, -1184, 0 }, { 496, 0, -1184, 0 }, { -496, 0, 1184, 0 }, { 496, 0, 1184, 0 } }, { 0, 4099, 0, 0 }, { -ONE, 0, 0, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_CAP_COMMAND_10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5599, -64, -2273, 0 }, { { -954, 0, -858, 0 }, { -57, 0, -1282, 0 }, { 58, 0, 1283, 0 }, { 955, 0, 859, 0 } }, { 0, 4099, 0, 0 }, { 3513, 0, -2106, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_CAP_COMMAND_11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8479, -64, -2048, 0 }, { { 368, 0, -1230, 0 }, { 1135, 0, -600, 0 }, { -1135, 0, 600, 0 }, { -368, 0, 1230, 0 } }, { 0, 4099, 0, 0 }, { -3290, 0, -2440, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_CAP_COMMAND_11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Facing-gated interaction quads for active-layout room selectors 2 and 5.
///
/// Includes both door warps, the puzzle action and five CAP interactions;
/// the central interaction routes the object-event command to the room.
/// Positions/radii are room units and normals use ONE. Setup mutates links,
/// coordinates and hits; the eighth record carries WORLD_COLLISION_TRIGGER_LAST.
static WorldCollisionTrigger _gNeoArkShrineActiveActionTriggers[] = {
    { NULL, NULL, NULL, { 400, -48, 1344, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_POWER_PLANT_1, NEO_ARK_SHRINE_WARP_TO_POWER_PLANT_1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3100, -48, 1504, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { -ONE, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_SAVANNA_ZONE, NEO_ARK_SHRINE_WARP_TO_SAVANNA_ZONE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7024, -64, -4048, 0 }, { { -1152, 0, -560, 0 }, { 1152, 0, -560, 0 }, { -1152, 0, 560, 0 }, { 1152, 0, 560, 0 } }, { 0, 4114, 0, 0 }, { 601, 0, 4052, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, NEO_ARK_SHRINE_ACTION_OPEN_PUZZLE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7008, -64, 3680, 0 }, { { -1152, 0, -560, 0 }, { 1152, 0, -560, 0 }, { -1152, 0, 560, 0 }, { 1152, 0, 560, 0 } }, { 0, 4114, 0, 0 }, { -601, 0, -4052, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_COMMAND_OBJECT_EVENT, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5184, -64, -3696, 0 }, { { -496, 0, -1264, 0 }, { 496, 0, -1264, 0 }, { -496, 0, 1264, 0 }, { 496, 0, 1264, 0 } }, { 0, 4105, 0, 0 }, { ONE, 0, 0, 0 }, 1354, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_CAP_COMMAND_10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8864, -64, -3744, 0 }, { { -496, 0, -1248, 0 }, { 496, 0, -1248, 0 }, { -496, 0, 1248, 0 }, { 496, 0, 1248, 0 } }, { 0, 4097, 0, 0 }, { -ONE, 0, 0, 0 }, 1342, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_CAP_COMMAND_10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8511, -64, -2153, 0 }, { { 249, 0, -1143, 0 }, { 1015, 0, -514, 0 }, { -1046, 0, 531, 0 }, { -216, 0, 1128, 0 } }, { 0, 4095, 0, 0 }, { -3035, 0, -2751, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_CAP_COMMAND_11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5599, -64, -2209, 0 }, { { -941, 0, -913, 0 }, { -25, 0, -1293, 0 }, { 26, 0, 1422, 0 }, { 942, 0, 786, 0 } }, { 0, 4099, 0, 0 }, { 3166, 0, -2598, 0 }, 1419, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_CAP_COMMAND_11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Facing-gated interaction quads for enemy-release room selectors 3 and 6.
///
/// Includes both door warps, the puzzle action and five CAP interactions;
/// the central interaction now starts CAP command 6 directly. Positions/radii
/// are room units and normals use ONE. Setup mutates links, coordinates and
/// hits; the eighth record carries WORLD_COLLISION_TRIGGER_LAST.
static WorldCollisionTrigger _gNeoArkShrineEnemyReleaseActionTriggers[] = {
    { NULL, NULL, NULL, { 400, -48, 1344, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_POWER_PLANT_1, NEO_ARK_SHRINE_WARP_TO_POWER_PLANT_1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3100, -48, 1504, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { -ONE, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_SAVANNA_ZONE, NEO_ARK_SHRINE_WARP_TO_SAVANNA_ZONE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7024, -64, -4016, 0 }, { { -1152, 0, -560, 0 }, { 1152, 0, -560, 0 }, { -1152, 0, 560, 0 }, { 1152, 0, 560, 0 } }, { 0, 4114, 0, 0 }, { 601, 0, 4052, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, NEO_ARK_SHRINE_ACTION_OPEN_PUZZLE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7008, -64, -1248, 0 }, { { -1152, 0, -560, 0 }, { 1152, 0, -560, 0 }, { -1152, 0, 560, 0 }, { 1152, 0, 560, 0 } }, { 0, 4114, 0, 0 }, { 201, 0, -4092, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_CAP_COMMAND_6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5184, -64, -3856, 0 }, { { -496, 0, -1232, 0 }, { 496, 0, -1232, 0 }, { -496, 0, 1232, 0 }, { 496, 0, 1232, 0 } }, { 0, 4112, 0, 0 }, { ONE, 0, 0, 0 }, 1324, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_CAP_COMMAND_10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8960, -64, -3776, 0 }, { { -496, 0, -1280, 0 }, { 496, 0, -1280, 0 }, { -496, 0, 1280, 0 }, { 496, 0, 1280, 0 } }, { 0, 4113, 0, 0 }, { -ONE, 0, 0, 0 }, 1372, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_CAP_COMMAND_10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5631, -64, -2321, 0 }, { { -911, 0, -789, 0 }, { -14, 0, -1213, 0 }, { 15, 0, 1214, 0 }, { 912, 0, 790, 0 } }, { 0, 4113, 0, 0 }, { 3784, 0, -1567, 0 }, 1207, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_CAP_COMMAND_11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8399, -64, -2161, 0 }, { { 317, 0, -1041, 0 }, { 1052, 0, -375, 0 }, { -1051, 0, 376, 0 }, { -316, 0, 1042, 0 } }, { 0, 4098, 0, 0 }, { -3290, 0, -2440, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SHRINE_CAP_COMMAND_11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Placement-entry keys for the shrine's enemy resources, independent of actor RAM slot.
enum {
    NEO_ARK_SHRINE_RESOURCE_BLACK_BEETLE = 38,
    NEO_ARK_SHRINE_RESOURCE_SCORPION     = 25,
    NEO_ARK_SHRINE_RESOURCE_PAWN_GOLEM   = 20,
    NEO_ARK_SHRINE_RESOURCE_KNIGHT_GOLEM = 22,
    NEO_ARK_SHRINE_RESOURCE_BISHOP_GOLEM = 39,
};

/// Black Beetle files and spawn recipe for placement variant 1, ending with AREA_PLACEMENT_END.
static AreaResource _gNeoArkShrineVariant1Resources[] = {
    { .entryId = NEO_ARK_SHRINE_RESOURCE_BLACK_BEETLE, .fileNumber = 38, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor103800BlackBeetleTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Actor-slot-1 Scorpion files and spawn recipe for placement variant 2, ending with AREA_PLACEMENT_END.
static AreaResource _gNeoArkShrineVariant2Resources[] = {
    { .entryId = NEO_ARK_SHRINE_RESOURCE_SCORPION, .fileNumber = 25, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor102500Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Actor-slot-1 Scorpion files and spawn recipe for placement variant 3, ending with AREA_PLACEMENT_END.
static AreaResource _gNeoArkShrineVariant3Resources[] = {
    { .entryId = NEO_ARK_SHRINE_RESOURCE_SCORPION, .fileNumber = 25, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor102500Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Black Beetle and actor-slot-2 Scorpion files and spawn recipes for placement variant 4.
///
/// Entry IDs 38 and 25 bind the matching placements; AREA_PLACEMENT_END ends
/// the list. Each live entry selects its descriptor table's index 0.
static AreaResource _gNeoArkShrineVariant4Resources[] = {
    { .entryId = NEO_ARK_SHRINE_RESOURCE_BLACK_BEETLE, .fileNumber = 38, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor103800BlackBeetleTaskDesc },
    { .entryId = NEO_ARK_SHRINE_RESOURCE_SCORPION, .fileNumber = 25, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = gActor02500Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Pawn GOLEM files and spawn recipe for placement variant 5, ending with AREA_PLACEMENT_END.
static AreaResource _gNeoArkShrineVariant5Resources[] = {
    { .entryId = NEO_ARK_SHRINE_RESOURCE_PAWN_GOLEM, .fileNumber = 20, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor102000PawnGolemTasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Knight GOLEM files and spawn recipe for placement variant 7, ending with AREA_PLACEMENT_END.
static AreaResource _gNeoArkShrineVariant7Resources[] = {
    { .entryId = NEO_ARK_SHRINE_RESOURCE_KNIGHT_GOLEM, .fileNumber = 22, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor402200GolemTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Bishop GOLEM files and spawn recipe for placement variant 11, ending with AREA_PLACEMENT_END.
static AreaResource _gNeoArkShrineVariant11Resources[] = {
    { .entryId = NEO_ARK_SHRINE_RESOURCE_BISHOP_GOLEM, .fileNumber = 39, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor403900GolemTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gNeoArkShrineAreaVariants[NEO_ARK_SHRINE_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapNeoArkShrineVariant1Placements, .resources = _gNeoArkShrineVariant1Resources },
    [2]                    = { .placements = gMapNeoArkShrineVariant2Placements, .resources = _gNeoArkShrineVariant2Resources },
    [3]                    = { .placements = gMapNeoArkShrineVariant3Placements, .resources = _gNeoArkShrineVariant3Resources },
    [4]                    = { .placements = gMapNeoArkShrineVariant4Placements, .resources = _gNeoArkShrineVariant4Resources },
    [5]                    = { .placements = gMapNeoArkShrineVariant5Placements, .resources = _gNeoArkShrineVariant5Resources },
    [7]                    = { .placements = gMapNeoArkShrineVariant7Placements, .resources = _gNeoArkShrineVariant7Resources },
    [11]                   = { .placements = gMapNeoArkShrineVariant11Placements, .resources = _gNeoArkShrineVariant11Resources },
};

/// Four line-of-sight blocking quads shared by every shrine layout.
///
/// Origins and corner offsets use signed room units; normals use ONE for
/// unit length. Setup links and enables every mutable record, including
/// the fourth marked WORLD_COLLISION_OCCLUDER_LAST. The authored low flag
/// bit remains 1; its role is unproven. Storage stays borrowed until unlink.
static WorldCollisionOccluder _gNeoArkShrineSightOccluders[] = {
    { NULL, NULL, { 2736, -2352, 2159, 0 }, { { -784, 3376, 1841, 0 }, { 784, 3376, -1840, 0 }, { -784, -3376, 1841, 0 }, { 784, -3376, -1840, 0 } }, { 3777, 0, 1608, 0 }, 3924, 1, 0 },
    { NULL, NULL, { 3119, -2080, -289, 0 }, { { 2728, 3376, 4275, 0 }, { -2727, 3376, -4275, 0 }, { 2728, -3376, 4275, 0 }, { -2727, -3376, -4275, 0 } }, { 3469, 0, -2214, 0 }, 6079, 1, 0 },
    { NULL, NULL, { 0x2800, -1888, -2288, 0 }, { { -1840, 3376, 2657, 0 }, { 1840, 3376, -2657, 0 }, { -1840, -3376, 2657, 0 }, { 1840, -3376, -2657, 0 } }, { 3371, 0, 2334, 0 }, 4664, 1, 0 },
    { NULL, NULL, { 0x2840, -1984, 4256, 0 }, { { 1904, 3376, 1617, 0 }, { -1904, 3376, -1616, 0 }, { 1904, -3376, 1617, 0 }, { -1904, -3376, -1616, 0 } }, { 2664, 0, -3139, 0 }, 4190, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Cue-2 script bases in the loaded shrine sound bank; cue 1 uses the following entry.
enum {
    NEO_ARK_SHRINE_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3D,
    NEO_ARK_SHRINE_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3F,
};

/// Paired footstep cues for shrine surfaces 0 and 2..7, with walking cues for scripted jumps.
///
/// Cue 1 adds one to these bases; companion playback adds another hundred.
static WorldCollisionFootstepSounds _gNeoArkShrineFootstepSounds = {
    .walk         = NEO_ARK_SHRINE_FOOTSTEP_WALK,
    .run          = NEO_ARK_SHRINE_FOOTSTEP_RUN,
    .scriptedJump = NEO_ARK_SHRINE_FOOTSTEP_WALK,
};

/// Classes 0 and 3..7: probe blocking, weapon impacts, pushback and shrine footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkShrineSurface0Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkShrineFootstepSounds,
};

/// Class 1: probe blocking, weapon impacts and pushback without surface footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkShrineSurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 2's distinct record for probe blocking, weapon impacts, pushback and shrine footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkShrineSurface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkShrineFootstepSounds,
};

WorldCollisionSurfaceProperties* gNeoArkShrineSurfaceProperties[8] = {
    &_gNeoArkShrineSurface0Properties,
    &_gNeoArkShrineSurface1Properties,
    &_gNeoArkShrineSurface2Properties,
    &_gNeoArkShrineSurface0Properties,
    &_gNeoArkShrineSurface0Properties,
    &_gNeoArkShrineSurface0Properties,
    &_gNeoArkShrineSurface0Properties,
    &_gNeoArkShrineSurface0Properties,
};

Task* gNeoArkShrinePuzzleControllerTask = NULL;

s16 gNeoArkShrinePuzzleLayoutActive = false;

s16 gNeoArkShrinePuzzleCellTiles[NEO_ARK_SHRINE_PUZZLE_CELL_COUNT] = { 0 };

NeoArkShrineTileOrigin gNeoArkShrinePuzzleDrawnOrigins[NEO_ARK_SHRINE_PUZZLE_TILE_COUNT] = { 0 };

NeoArkShrineTileOrigin gNeoArkShrinePuzzleTargetOrigins[NEO_ARK_SHRINE_PUZZLE_TILE_COUNT] = { 0 };

#include "../../shared/room/action_prompt_move_cursors.inc.c"

#include "../../shared/room/action_prompt_draw_cursor.inc.c"

void neoArkShrinePuzzleCursorTask(Task* task)
{
    TaskFunc states[2] = { _actionPromptResetDefault, _actionPromptMoveCursorsDefault };

    states[task->state](task);
}

void neoArkShrineDrawPuzzleFrame()
{
    neoArkShrineAnimateAndDrawPuzzle();
}

void neoArkShrinePuzzleTask(Task* task)
{
    TaskFuncTable16 states;

    states = _gNeoArkShrinePuzzleStates;
    states.funcs[task->state](task);
}

void neoArkShrineFirstFallingPropTask(Task* task)
{
    TaskFuncTable4 states;

    states = _gNeoArkShrineFirstPropStates;
    states.funcs[task->state](task);
}

void neoArkShrineSecondFallingPropTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gNeoArkShrineSecondPropStates;
    states.funcs[task->state](task);
}

#include "../../shared/room/action_prompt_hit_test.inc.c"

/// Binds the puzzle's work and cursor, selects its view and acquires presentation.
///
/// Requires controller state 0 and a fresh, zeroed primary-heap work block.
/// The controller adopts the allocation and enters cursor setup. The independent
/// port-0 cursor is retained in spawnArg2 for explicit teardown, without task
/// reparenting; its spawn failure is unchecked. The room records and work stay
/// live through puzzle closing, which releases the acquired display hold.
static inline void _neoArkShrineBeginPuzzleSession(Task* task, NeoArkShrinePuzzleWork* work)
{
    enum { NEO_ARK_SHRINE_PUZZLE_VIEW          = 11,
           NEO_ARK_SHRINE_PUZZLE_CURSOR_ENTRY  = 0,
           NEO_ARK_SHRINE_PUZZLE_CURSOR_PORT_0 = 1 };

    task->spawnArg2.pointer                                = taskSpawnFromTable(gNeoArkShrinePuzzleCursorTaskDescs, NEO_ARK_SHRINE_PUZZLE_CURSOR_ENTRY, NEO_ARK_SHRINE_PUZZLE_CURSOR_PORT_0, NULL);
    task->work                                             = work;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = NEO_ARK_SHRINE_PUZZLE_VIEW;
    task->state++;
    displayAcquireMenuHold();
}

/// Initializes the shrine's sliding-tile puzzle controller and cursor session.
///
/// Entry is state 0. Owns zeroed puzzle work until task teardown and retains
/// a separate cursor child until closing. Work-allocation failure kills the
/// controller. Success selects saved view 11, clears all live hotspot hits,
/// holds player/menu presentation and hides the HUD, then enters cursor setup.
/// Requires the loaded cursor descriptor and hotspot run through its end marker.
static void _neoArkShrineInitializePuzzle(Task* task)
{
    NeoArkShrinePuzzleWork* work;
    ActionPromptHotspot*    hotspot;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    _neoArkShrineBeginPuzzleSession(task, work);
    for (hotspot = gNeoArkShrinePuzzleHotspots; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
        hotspot->hit = 0;
    }
    gGameSession->cutsceneHold = true;
    gGameSession->hideHud      = true;
    gGameSession->eventState   = 1;
}

/// Centers and enables the puzzle cursor, then enters the idle state.
///
/// State 1 borrows port 0's action prompt; positions are display-centred pixels.
/// The cursor task has already reset both ports and needs no puzzle work here.
static void _neoArkShrinePreparePuzzleCursor(Task* task)
{
    ActionPrompt* prompt = gActionPromptSlots;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_NORMAL;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Opens the confirmed hotspot's commands and waits for their result.
///
/// State 3 requires the owned puzzle work with a latched prompt kind. Draws one
/// puzzle frame before hiding and stopping port 0's cursor; the command menu
/// opens at that cursor's display-centred pixel position.
static void _neoArkShrineOpenPuzzleCommands(Task* task)
{
    ActionPrompt*           prompt = gActionPromptSlots;
    NeoArkShrinePuzzleWork* work   = task->work;

    neoArkShrineDrawPuzzleFrame(task);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = NEO_ARK_SHRINE_PUZZLE_STATE_COMMAND_RESULT;
}

/// Applies the puzzle's Examine command and returns to cursor input.
///
/// State 4 requires owned puzzle work with the confirmed hotspot latched.
/// Hides and stops port 0's cursor, then draws one frame. On acceptance runs
/// sequence 2 off the board, or latches board examination and runs sequence 1
/// on a tile. Every path enters idle state 2; CAP playback gates further input.
static void _neoArkShrineResolvePuzzleExamine(Task* task)
{
    enum { NEO_ARK_SHRINE_EXAMINE_BOARD_SEQUENCE     = 1,
           NEO_ARK_SHRINE_EXAMINE_OFF_BOARD_SEQUENCE = 2 };

    ActionPrompt*           prompt = gActionPromptSlots;
    NeoArkShrinePuzzleWork* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    neoArkShrineDrawPuzzleFrame(task);
    if (itemMenuIsHotspotActionConfirmed() == 0) {
        task->state = NEO_ARK_SHRINE_PUZZLE_STATE_IDLE;
        return;
    }
    if (work->selection == NEO_ARK_SHRINE_HOTSPOT_OFF_BOARD) {
        capStartSequenceSlot(NEO_ARK_SHRINE_EXAMINE_OFF_BOARD_SEQUENCE, CAP_PLAYBACK_IN_PLACE, 0);
        task->state = NEO_ARK_SHRINE_PUZZLE_STATE_IDLE;
        return;
    }
    work->boardExamined = 1;
    capStartSequenceSlot(NEO_ARK_SHRINE_EXAMINE_BOARD_SEQUENCE, CAP_PLAYBACK_IN_PLACE, 0);
    task->state = NEO_ARK_SHRINE_PUZZLE_STATE_IDLE;
}

/// Restores player control, HUD, event gates and the room view after the puzzle.
///
/// Requires a live player task and its attachments and one acquired menu hold.
/// Resumes player control and automatic model drawing before releasing the
/// hold, then clears the event, HUD and cutscene gates. Restores saved view
/// selector 10; task completion and cursor teardown are left to the caller.
static inline void _neoArkShrineResumeRoomFromPuzzle(void)
{
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    displayReleaseMenuHold();
    gGameSession->eventState                               = 0;
    gGameSession->hideHud                                  = false;
    gGameSession->cutsceneHold                             = false;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = NEO_ARK_SHRINE_ROOM_VIEW;
}

/// Closes the puzzle, restores room control and reports a zero completion result.
///
/// State 5 requires a live cursor task in `spawnArg2.pointer`. That task is
/// killed immediately; this puzzle task and its owned work stay live until
/// the room's waiting task polls the stop request. Manual direction actions
/// are inhibited for ten eligible input ticks after closing.
static void _neoArkShrineClosePuzzle(Task* task)
{
    enum { NEO_ARK_SHRINE_EXIT_ACTION_COOLDOWN_TICKS = 10 };

    gDirectionInteractionRearmUpdates = NEO_ARK_SHRINE_EXIT_ACTION_COOLDOWN_TICKS;
    _neoArkShrineResumeRoomFromPuzzle();
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, 0);
}

/// Starts the active-layout transition with controller vibration.
///
/// State 7 latches the active layout, hides and stops the cursor, draws one
/// puzzle frame, clears the owned work's frame timer and enters state 8.
/// The 18-frame motor ramp starts at intensity 48 and targets 144.
static void _neoArkShrineBeginPuzzleLayoutActivation(Task* task)
{
    NEO_ARK_SHRINE_BEGIN_LAYOUT(task, true);
}

/// Draws the puzzle until the active-layout transition reaches thirty frames.
///
/// State 8 requires the owned puzzle work, with its timer cleared on entry.
/// Keeps the cursor hidden and stopped; at the threshold selects layout 2, or
/// 5 after the first enemy reveal, in both the saved and live location.
/// Requests an object rebuild and returns to puzzle idle.
static void _neoArkShrineWaitPuzzleLayoutActivation(Task* task)
{
    NEO_ARK_SHRINE_WAIT_LAYOUT(task, NEO_ARK_SHRINE_LAYOUT_ACTIVE, NEO_ARK_SHRINE_LAYOUT_ACTIVE_AFTER_REVEAL);
}

/// Begins the enemy-release sequence and retires the puzzle cursor.
///
/// State 9 requires owned puzzle work and a live cursor task in
/// `spawnArg2.pointer`. Sets the pending base-layout restore request, draws
/// one puzzle frame, kills the cursor, clears the timer and enters the
/// first falling-prop delay. The restore request is consumed by a later tile move.
static void _neoArkShrineBeginPuzzleEnemyRelease(Task* task)
{
    NeoArkShrinePuzzleWork* work;

    work                                    = task->work;
    gNeoArkShrinePuzzleLayoutRestorePending = true;
    neoArkShrineDrawPuzzleFrame();
    taskKill(task->spawnArg2.pointer);
    work->timer = 0;
    task->state++;
}

/// Draws the puzzle during the thirty-frame delay before the first falling prop.
///
/// State 10 requires owned puzzle work with its u16 timer reset on entry.
/// At the threshold spawns the first model prop, selects saved view 14,
/// clears the timer and advances to the second-prop delay. The prop runs
/// independently; puzzle and prop resources must remain loaded.
static void _neoArkShrineWaitToSpawnFirstFallingProp(Task* task)
{
    enum { NEO_ARK_SHRINE_FIRST_PROP_DELAY_FRAMES  = 30,
           NEO_ARK_SHRINE_FIRST_FALLING_PROP_ENTRY = 1,
           NEO_ARK_SHRINE_FIRST_PROP_VIEW          = 14 };

    NeoArkShrinePuzzleWork* work;
    u16                     elapsedFrames;

    work = task->work;
    neoArkShrineDrawPuzzleFrame();
    elapsedFrames = work->timer + 1;
    work->timer   = elapsedFrames;
    if (elapsedFrames >= (u32)NEO_ARK_SHRINE_FIRST_PROP_DELAY_FRAMES) {
        taskSpawnFromTable(gNeoArkShrinePuzzleTaskDescs, NEO_ARK_SHRINE_FIRST_FALLING_PROP_ENTRY, 0, 0);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = NEO_ARK_SHRINE_FIRST_PROP_VIEW;
        // Reset the following delay before advancing to its state.
        work->timer = 0;
        task->state++;
    }
}

/// Waits ninety frames and selects the first or repeated enemy-release path.
///
/// State 11 requires owned puzzle work with its u16 timer reset on entry.
/// At the threshold clears the timer. On the first release spawns the second
/// falling prop, selects saved view 13, sets the reveal latch and enters
/// state 12. Later releases skip that prop and reveal delay for state 13.
/// The independently running props and room resources must remain loaded.
static void _neoArkShrineWaitToSpawnSecondFallingProp(Task* task)
{
    enum { NEO_ARK_SHRINE_SECOND_PROP_DELAY_FRAMES  = 90,
           NEO_ARK_SHRINE_SECOND_FALLING_PROP_ENTRY = 2,
           NEO_ARK_SHRINE_SECOND_PROP_VIEW          = 13 };

    NeoArkShrinePuzzleWork* work;
    u16                     elapsedFrames;
    s32                     nextState;

    work          = task->work;
    elapsedFrames = work->timer + 1;
    work->timer   = elapsedFrames;
    if (elapsedFrames >= (u32)NEO_ARK_SHRINE_SECOND_PROP_DELAY_FRAMES) {
        work->timer = 0;
        // The second prop and enemy reveal occur only on the first release.
        if (gameFlagGetNibble(GAME_FLAG_0E9) == 0) {
            taskSpawnFromTable(gNeoArkShrinePuzzleTaskDescs, NEO_ARK_SHRINE_SECOND_FALLING_PROP_ENTRY, 0, 0);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = NEO_ARK_SHRINE_SECOND_PROP_VIEW;
            gameFlagSetNibble(GAME_FLAG_0E9, 1);
            nextState = task->state + 1;
        } else {
            nextState = task->state + 2;
        }
        task->state = nextState;
    }
}

/// Signals the first enemy reveal at frame thirty and leaves at frame sixty.
///
/// State 12 requires owned puzzle work with its timer reset by the preceding
/// state. The u16 timer increments with wraparound. Reveal makes the shrine
/// enemies visible and repeatedly resets their startup delay until state 13
/// switches the signal to released.
static void _neoArkShrineWaitPuzzleEnemyReveal(Task* task)
{
    enum { NEO_ARK_SHRINE_ENEMY_REVEAL_FRAME        = 30,
           NEO_ARK_SHRINE_ENEMY_RELEASE_READY_FRAME = 60 };

    NeoArkShrinePuzzleWork* work;
    u16                     elapsedFrames;

    work          = task->work;
    elapsedFrames = work->timer + 1;
    work->timer   = elapsedFrames;
    if (elapsedFrames == NEO_ARK_SHRINE_ENEMY_REVEAL_FRAME) {
        gSceneCombatState.shrineEnemyPhase = SCENE_COMBAT_SHRINE_REVEALED;
    }
    if (work->timer >= (u32)NEO_ARK_SHRINE_ENEMY_RELEASE_READY_FRAME) {
        task->state++;
    }
}

/// Releases the shrine enemies, restores room control and reports completion.
///
/// State 13 selects room layout 6 in both locations and requests an object
/// rebuild. The cursor has already been killed by state 9. Stops this puzzle
/// task with result zero; the waiting room task later polls it and frees the
/// owned work. Enemy startup delays now count down toward normal behavior.
static void _neoArkShrineFinishPuzzleEnemyRelease(Task* task)
{
    gSceneCombatState.shrineEnemyPhase                          = SCENE_COMBAT_SHRINE_RELEASED;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = NEO_ARK_SHRINE_LAYOUT_ENEMIES_RELEASED;
    gGameSession->location.roomState                            = NEO_ARK_SHRINE_LAYOUT_ENEMIES_RELEASED;
    gGameSession->roomObjsDirty                                 = true;
    _neoArkShrineResumeRoomFromPuzzle();
    taskRequestKill(task, 0);
}

/// Starts the base-layout transition with controller vibration.
///
/// State 14 clears the active-layout latch, hides and stops the cursor, draws
/// one puzzle frame, clears the owned work's frame timer and enters state 15.
/// The 18-frame motor ramp starts at intensity 48 and targets 144.
static void _neoArkShrineBeginPuzzleLayoutRestoration(Task* task)
{
    NEO_ARK_SHRINE_BEGIN_LAYOUT(task, false);
}

#undef NEO_ARK_SHRINE_BEGIN_LAYOUT

/// Draws the puzzle until the base-layout transition reaches thirty frames.
///
/// State 15 requires the owned puzzle work, with its timer cleared on entry.
/// Keeps the cursor hidden and stopped; at the threshold selects layout 1, or
/// 4 after the first enemy reveal, in both the saved and live location.
/// Requests an object rebuild and returns to puzzle idle.
static void _neoArkShrineWaitPuzzleLayoutRestoration(Task* task)
{
    NEO_ARK_SHRINE_WAIT_LAYOUT(task, NEO_ARK_SHRINE_LAYOUT_BASE, NEO_ARK_SHRINE_LAYOUT_BASE_AFTER_REVEAL);
}

#undef NEO_ARK_SHRINE_WAIT_LAYOUT

void neoArkShrineResetPuzzle(void)
{
    s32 tileIndex;
    s32 cellIndex;

    gNeoArkShrinePuzzleLayoutRestorePending = false;
    gNeoArkShrinePuzzleLayoutActive         = false;
    // Restore per-tile drawn positions before reseeding the cell-to-tile board.
    for (tileIndex = 0; tileIndex < (s32)ARRAY_SIZE(gNeoArkShrinePuzzleDrawnOrigins); tileIndex++) {
        gNeoArkShrinePuzzleDrawnOrigins[tileIndex].x = gNeoArkShrinePuzzleInitialTileOrigins[tileIndex].x;
        gNeoArkShrinePuzzleDrawnOrigins[tileIndex].y = gNeoArkShrinePuzzleInitialTileOrigins[tileIndex].y;
    }
    for (cellIndex = 0; cellIndex < (s32)ARRAY_SIZE(gNeoArkShrinePuzzleCellTiles); cellIndex++) {
        gNeoArkShrinePuzzleCellTiles[cellIndex] = gNeoArkShrinePuzzleInitialCellTiles[cellIndex];
    }
}

/// Creates a falling prop's owned lighting work and places its model above the floor.
///
/// `task` has a TMD body and starts with null work. X and Z are world-coordinate
/// positions; Y starts at -3000, with positive Y downward. Allocates zeroed
/// primary-heap work, so acceleration, velocity and drop frame count start at
/// zero. Work is published even on allocation failure, when the task is killed.
/// Success parents the model to the view coordinate, exposes the owned matrices
/// to the model, allows normal drawing, refreshes lighting and enters the drop.
static inline void _neoArkShrineInitializeFallingProp(Task* task, s32 startX, s32 startZ)
{
    enum { NEO_ARK_SHRINE_PROP_START_Y = -3000 };

    TmdObject*                    model;
    GfxCoord*                     coord;
    _NeoArkShrineFallingPropWork* work;

    model      = task->extra.tmd;
    coord      = model->coords;
    work       = memCalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    // The model borrows these matrices until task teardown frees the work.
    model->lightMtx   = &work->light;
    model->flags      = 0;
    model->colorMtx   = &work->color;
    coord->parent     = &gGfxViewCoord;
    coord->coord.t[0] = startX;
    coord->coord.t[1] = NEO_ARK_SHRINE_PROP_START_Y;
    coord->coord.t[2] = startZ;
    _neoArkShrineUpdateFallingPropLighting(task);
    task->state++;
}

/// Initializes the first falling prop at world position (7000, -3000, -1000).
///
/// State 0 requires a TMD body and null work. Success starts the drop from
/// rest; its zeroed work and lighting matrices remain owned by the task.
/// Allocation failure kills the task without advancing the state.
static void _neoArkShrineInitializeFirstFallingProp(Task* task)
{
    enum { NEO_ARK_SHRINE_FIRST_PROP_START_X = 7000,
           NEO_ARK_SHRINE_FIRST_PROP_START_Z = -1000 };

    _neoArkShrineInitializeFallingProp(task, NEO_ARK_SHRINE_FIRST_PROP_START_X, NEO_ARK_SHRINE_FIRST_PROP_START_Z);
}

/// Advances a prop's accelerating drop and enters its next state below the floor.
///
/// Requires the task's owned drop work and model root. `accelerationStep` is
/// world units per frame cubed. Acceleration and velocity narrow to signed
/// halfwords after each addition; height is a signed 32-bit world Y coordinate.
/// Equality at floor Y stays in the drop state until a later positive step.
static inline void _neoArkShrineIntegratePropDrop(Task* task, _NeoArkShrineFallingPropWork* work,
                                                  GfxCoord* coord, s32 accelerationStep)
{
    enum { NEO_ARK_SHRINE_PROP_FLOOR_Y = 0 };

    work->fallAcceleration += accelerationStep;
    work->fallVelocity     += work->fallAcceleration;
    coord->coord.t[1]      += work->fallVelocity;
    if (coord->coord.t[1] > NEO_ARK_SHRINE_PROP_FLOOR_Y) {
        coord->coord.t[1] = NEO_ARK_SHRINE_PROP_FLOOR_Y;
        task->state++;
    }
}

/// Drops the first prop and starts its sound and motor ramp on drop frame four.
///
/// State 1 requires the TMD body and owned zero-initialized drop work. Positive
/// world Y is downward. Acceleration grows by one world unit per frame squared
/// each frame; acceleration, velocity and frame count narrow to signed halfwords.
/// Crossing Y = 0 clamps the model to the floor and enters the restore wait.
/// Refreshes composed coordinates and lighting even on the landing frame.
static void _neoArkShrineDropFirstProp(Task* task)
{
    enum { NEO_ARK_SHRINE_FIRST_PROP_EFFECT_FRAME  = 4,
           NEO_ARK_SHRINE_FIRST_PROP_RUMBLE_FRAMES = 24,
           NEO_ARK_SHRINE_FIRST_PROP_RUMBLE_START  = 64,
           NEO_ARK_SHRINE_FIRST_PROP_RUMBLE_END    = 255,
           NEO_ARK_SHRINE_FIRST_PROP_ACCEL_STEP    = 1 };

    _NeoArkShrineFallingPropWork* work;
    GfxCoord*                     coord;

    work  = task->work;
    coord = task->extra.tmd->coords;
    work->fallFrames++;
    if (work->fallFrames == NEO_ARK_SHRINE_FIRST_PROP_EFFECT_FRAME) {
        padScriptSpawnVariableMotorRamp(NEO_ARK_SHRINE_FIRST_PROP_RUMBLE_FRAMES,
                                        NEO_ARK_SHRINE_FIRST_PROP_RUMBLE_START, NEO_ARK_SHRINE_FIRST_PROP_RUMBLE_END);
        sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_PROP_1_FALL, 0, 0);
    }
    _neoArkShrineIntegratePropDrop(task, work, coord, NEO_ARK_SHRINE_FIRST_PROP_ACCEL_STEP);
    _neoArkShrineUpdateFallingPropLighting(task);
}

/// Keeps the landed first prop lit until the puzzle consumes its restore request.
///
/// State 2 requires the TMD body and owned lighting matrices. The request
/// clears when a later tile move restores the base layout, or the board resets.
/// Once clear, advances to the task's kill state without freeing anything here.
static void _neoArkShrineWaitFirstPropLayoutRestore(Task* task)
{
    _neoArkShrineUpdateFallingPropLighting(task);
    if (gNeoArkShrinePuzzleLayoutRestorePending == false) {
        task->state++;
    }
}

/// Initializes the second falling prop at world position (8750, -3000, -4550).
///
/// State 0 requires a TMD body and null work. Success starts the drop from
/// rest; its zeroed work and lighting matrices remain owned by the task.
/// Allocation failure kills the task without advancing the state.
static void _neoArkShrineInitializeSecondFallingProp(Task* task)
{
    enum { NEO_ARK_SHRINE_SECOND_PROP_START_X = 8750,
           NEO_ARK_SHRINE_SECOND_PROP_START_Z = -4550 };

    _neoArkShrineInitializeFallingProp(task, NEO_ARK_SHRINE_SECOND_PROP_START_X, NEO_ARK_SHRINE_SECOND_PROP_START_Z);
}

/// Drops the second prop with sound on frame two and vibration on frame eighteen.
///
/// State 1 requires the TMD body and owned zero-initialized drop work. Positive
/// world Y is downward. Acceleration grows by two world units per frame squared
/// each frame; acceleration, velocity and frame count narrow to signed halfwords.
/// Crossing Y = 0 clamps the model to the floor and enters the kill state.
/// Refreshes composed coordinates and lighting even on the landing frame.
static void _neoArkShrineDropSecondProp(Task* task)
{
    enum { NEO_ARK_SHRINE_SECOND_PROP_SOUND_FRAME   = 2,
           NEO_ARK_SHRINE_SECOND_PROP_RUMBLE_FRAME  = 18,
           NEO_ARK_SHRINE_SECOND_PROP_RUMBLE_FRAMES = 10,
           NEO_ARK_SHRINE_SECOND_PROP_RUMBLE_START  = 160,
           NEO_ARK_SHRINE_SECOND_PROP_RUMBLE_END    = 255,
           NEO_ARK_SHRINE_SECOND_PROP_ACCEL_STEP    = 2 };

    _NeoArkShrineFallingPropWork* work;
    GfxCoord*                     coord;

    work  = task->work;
    coord = task->extra.tmd->coords;
    work->fallFrames++;
    if (work->fallFrames == NEO_ARK_SHRINE_SECOND_PROP_SOUND_FRAME) {
        sndEvtRequestScriptStart(SOUND_NEO_ARK_SHRINE_PROP_2_FALL, 0, 0);
    }
    if (work->fallFrames == NEO_ARK_SHRINE_SECOND_PROP_RUMBLE_FRAME) {
        padScriptSpawnVariableMotorRamp(NEO_ARK_SHRINE_SECOND_PROP_RUMBLE_FRAMES,
                                        NEO_ARK_SHRINE_SECOND_PROP_RUMBLE_START, NEO_ARK_SHRINE_SECOND_PROP_RUMBLE_END);
    }
    _neoArkShrineIntegratePropDrop(task, work, coord, NEO_ARK_SHRINE_SECOND_PROP_ACCEL_STEP);
    _neoArkShrineUpdateFallingPropLighting(task);
}

#include "../../shared/room/action_prompt_reset.inc.c"

/// Refreshes a falling model's composed transform and its lighting matrices.
///
/// Requires a TMD body with writable light and colour matrices. The lighting
/// sample retains the composed translation with Y reduced by 800 coordinate
/// units; for these view-parented models that translation is in view space.
static void _neoArkShrineUpdateFallingPropLighting(Task* task)
{
    enum { NEO_ARK_SHRINE_PROP_LIGHT_SAMPLE_Y_OFFSET = -800,
           NEO_ARK_SHRINE_PROP_LIGHT_COUNT           = 3 };

    TmdObject* model;
    GfxCoord*  coord;
    VECTOR     samplePosition;

    model               = task->extra.tmd;
    coord               = model->coords;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    // Retain the original sample in the composed coordinate's space.
    samplePosition.vx = coord->workm.t[0];
    samplePosition.vy = coord->workm.t[1] + NEO_ARK_SHRINE_PROP_LIGHT_SAMPLE_Y_OFFSET;
    samplePosition.vz = coord->workm.t[2];
    worldCoordSetModelLighting(model, &samplePosition, 0, NEO_ARK_SHRINE_PROP_LIGHT_COUNT);
}

void neoArkShrineFlareTask(Task* task)
{
    enum { NEO_ARK_SHRINE_FLARES_INITIALIZE,
           NEO_ARK_SHRINE_FLARES_DRAW,
           NEO_ARK_SHRINE_FLARE_TEXTURE_0    = 0,
           NEO_ARK_SHRINE_FLARE_TEXTURE_1    = 1,
           NEO_ARK_SHRINE_FLARE_RADIUS_SCALE = 0x300 };

    if (task->state == NEO_ARK_SHRINE_FLARES_INITIALIZE) {
        gRoomEffectFlashId      = EFFECT_NEO_ARK_SHRINE_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_NEO_ARK_SHRINE_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_NEO_ARK_SHRINE_SPARK_BURST;
        task->state             = NEO_ARK_SHRINE_FLARES_DRAW;
    }

    switch (viewGetMappedIndex() & 0xFF) {
        case 2: {
            const SVECTOR* positions = &_gNeoArkShrineFlarePositions[9];
            _neoArkShrineDrawFlare(&positions[0], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[1], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[2], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[3], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[4], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[5], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            break;
        }
        case 3: {
            const SVECTOR* positions = &_gNeoArkShrineFlarePositions[4];
            _neoArkShrineDrawFlare(&positions[0], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[1], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[2], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[5], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[6], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[8], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[9], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[10], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            break;
        }
        case 4: {
            const SVECTOR* positions = &_gNeoArkShrineFlarePositions[4];
            _neoArkShrineDrawFlare(&positions[0], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[3], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[4], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[5], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            break;
        }
        case 5:
        case 18: {
            const SVECTOR* positions = &_gNeoArkShrineFlarePositions[4];
            _neoArkShrineDrawFlare(&positions[0], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[3], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[4], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            break;
        }
        case 6: {
            const SVECTOR* positions = &_gNeoArkShrineFlarePositions[2];
            _neoArkShrineDrawFlare(&positions[0], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[1], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[5], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[6], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            break;
        }
        case 7: {
            const SVECTOR* positions = _gNeoArkShrineFlarePositions;
            _neoArkShrineDrawFlare(&positions[0], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[2], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            break;
        }
        case 12: {
            const SVECTOR* positions = &_gNeoArkShrineFlarePositions[1];
            _neoArkShrineDrawFlare(&positions[0], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[3], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[6], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            break;
        }
        case 14: {
            const SVECTOR* positions = &_gNeoArkShrineFlarePositions[7];
            _neoArkShrineDrawFlare(&positions[0], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[1], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[2], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            break;
        }
        case 16: {
            const SVECTOR* positions = &_gNeoArkShrineFlarePositions[4];
            _neoArkShrineDrawFlare(&positions[0], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[3], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[4], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[5], NEO_ARK_SHRINE_FLARE_TEXTURE_1, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            break;
        }
        case 10:
        case 17: {
            const SVECTOR* positions = &_gNeoArkShrineFlarePositions[15];
            _neoArkShrineDrawFlare(&positions[0], NEO_ARK_SHRINE_FLARE_TEXTURE_0, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            _neoArkShrineDrawFlare(&positions[1], NEO_ARK_SHRINE_FLARE_TEXTURE_0, NEO_ARK_SHRINE_FLARE_RADIUS_SCALE);
            break;
        }
    }
}

/// Sets a shrine flare quad's square around its projected light centre.
///
/// Borrows the writable `flare` packet and read-only `projection` for this call.
/// Requires initialized `sx`, `sy` and `radius`, all in pixels; only the low
/// unsigned 16 bits of `radius` supply the square's half-extent. Vertices
/// 0, 1, 2 and 3 become top-left, top-right, bottom-left and bottom-right,
/// respectively, with each coordinate wrapping to signed 16 bits.
/// The caller supplies the packet header, texture, colour and linkage.
static inline void _neoArkShrineSetFlareBounds(POLY_FT4* flare, const GlowCentreScratch* projection)
{
    flare->x0 = flare->x2 = projection->sx - (u16)projection->radius;
    flare->x1 = flare->x3 = projection->sx + (u16)projection->radius;
    flare->y0 = flare->y1 = projection->sy - (u16)projection->radius;
    flare->y2 = flare->y3 = projection->sy + (u16)projection->radius;
}

/// Draws a flickering textured flare at a shrine light's world position.
///
/// Borrows `worldPoint` for the call. The signed low halfword of `textureIndex`
/// selects a 40-texel column and its low six bits select the palette offset;
/// shrine callers select columns 0 and 1. The signed low halfword of
/// `radiusScale` gives the pixel half-extent `radiusScale * 39 / depth`, with
/// depth equal to camera Z / 4 and required to be nonzero after projection.
/// Negative GTE flags suppress the packet. Accepted points queue one
/// semitransparent quad with RGB intensity 32 or 48 on alternating frames.
/// Requires a composed view, a current packet arena and depth ordering table,
/// and 16 free scratch-stack bytes, released before return on either path.
static void _neoArkShrineDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale)
{
    GlowCentreScratch* projection;
    POLY_FT4*          flare;
    DisplayState*      display;
    s32                columnIndex;
    s32                leftU;
    s32                rightU;
    s32                signedRadiusScale;
    s32                intensity;

    projection = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreScratch);

    // Reject projection errors before reserving a GPU packet.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    display   = &gDisplayState;
    intensity = (((u8)display->animFrame & 1) * (1 << GLOW_BRIGHT_FLICKER_SHIFT)) + GLOW_FLICKER_BASE_INTENSITY;
    gte_stsxy(&projection->sx);
    gte_stflg(&projection->flag);
    if (projection->flag >= 0) {
        gte_stszotz(&projection->otz);
        flare          = gGpuPrimCursor;
        gGpuPrimCursor = flare + 1;
        setPolyFT4(flare);
        columnIndex       = (s16)textureIndex;
        flare->tpage      = GLOW_FLARE_TEXTURE_PAGE;
        flare->clut       = (columnIndex & GLOW_FLARE_PALETTE_OFFSET_MASK) | GLOW_FLARE_PALETTE_BASE;
        leftU             = columnIndex * GLOW_FLARE_CELL_STRIDE;
        rightU            = leftU + GLOW_FLARE_CELL_LAST_TEXEL;
        signedRadiusScale = (s16)radiusScale;
        setRGB0(flare, intensity, intensity, intensity);
        flare->u0 = leftU;
        flare->v0 = 0;
        flare->u1 = rightU;
        flare->v1 = 0;
        flare->u2 = leftU;
        flare->v2 = GLOW_FLARE_CELL_LAST_TEXEL;
        flare->u3 = rightU;
        flare->v3 = GLOW_FLARE_CELL_LAST_TEXEL;
        setSemiTrans(flare, true);

        // Size the square in screen pixels and sort it at the projected depth.
        projection->radius = (signedRadiusScale * GLOW_FLARE_CELL_LAST_TEXEL) / projection->otz;
        _neoArkShrineSetFlareBounds(flare, projection);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH_SCALED(projection->otz, display->otDepthShift),
                flare);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreScratch);
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void neoArkShrineRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void neoArkShrineRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void neoArkShrineRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
