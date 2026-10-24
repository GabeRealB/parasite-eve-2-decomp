#include "rooms/mine_refuge.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
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
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_ids.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "actors/actor_548100.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_cutscene.h"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

/// Logical camera view held across the refuge's cutscene confirmation prompt.
///
/// Snapshots the live saved-location view before the prompt selects view 6.
/// Refusal restores it; acceptance gives it to the cutscene for restoration
/// on completion. Only one prompt may own this snapshot at a time. The three
/// following stored bytes have no proven role or relationship to the snapshot.
typedef struct {
    u8 restoreView;  // Logical view restored on refusal or passed to the accepted cutscene
    u8 unknown_1[3]; // Retained bytes; role and object boundary unproven
} _MineRefugePromptViewStorage;
STATIC_ASSERT_SIZEOF(_MineRefugePromptViewStorage, 4);

/// The save's `companionType` byte under a symbol of its own; the cutscene's
/// end reads it through this name rather than through `gMcSaveData`.

/// View saved when the cutscene starts and restored when it ends.

/// The cutscene's sound task, killed when the scene is skipped.
extern Task* gRoomCutsceneSoundTask;

/// Mutable singleton script, view and sound record for the refuge cutscene.
///
/// The scene runner borrows this record through completion; a second scene
/// must not overwrite it. A negative view restores the prompt's saved view,
/// while view 6 snapshots the current view. Unwritten texture-page fields
/// retain their zero defaults for the loaded room's lifetime.
static RoomCutsceneRec _gMineRefugeCutscene;

#include "../../shared/room/telephone.h"

static void _mineRefugeStartCutscene(s32 restoreView);
static void _mineRefugeInitRoomTask(Task* task);
static void _mineRefugeIdleRoomTask(Task* task);

/// Camera view used by the refuge cutscene and its confirmation prompt.
enum { MINE_REFUGE_CUTSCENE_VIEW = 6 };

enum {
    MINE_REFUGE_COLLISION_GRID_CELL_COUNT_X  = 1,
    MINE_REFUGE_COLLISION_GRID_CELL_COUNT_Z  = 2,
    MINE_REFUGE_COLLISION_GRID_CELL_SIZE     = 4000,
    MINE_REFUGE_EXIT_WARP_SLOT               = 1,
    MINE_REFUGE_GORGE_ARRIVAL_SLOT           = 2,
    MINE_REFUGE_TRIGGER_CIRCUIT_PANEL_ACTION = 1,
    MINE_REFUGE_TRIGGER_CUTSCENE_COMMAND     = 1,
    MINE_REFUGE_TRIGGER_CAP_3                = 3,
    MINE_REFUGE_TRIGGER_CAP_2                = 2,
    MINE_REFUGE_TRIGGER_CAP_11               = 11,
    MINE_REFUGE_FOOTSTEP_WALK_SCRIPT         = 65,
    MINE_REFUGE_FOOTSTEP_RUN_SCRIPT          = 67,
};

static WorldCollisionGrid         _gMineRefugeCollisionGrid;
static WorldCollisionTrigger      _gMineRefugeViewBoundaryTriggers[2];
static WorldCollisionTrigger      _gMineRefugeActionTriggers[6];
static WorldCoordRoomAmbientEntry _gMineRefugeAmbientByView[MINE_REFUGE_VIEW_COUNT + 1];
static WorldCoordRoomLights       _gMineRefugeRoomLights;
static s32                        _mineRefugeUseKeyItemMsg(Task* task, s32 messageId, s32 itemId, s32 secondArg);
static s32                        _mineRefugeResolveRoomEventMsg(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32                        _mineRefugeRoomCommandMsg(Task* task, s32 messageId, s32 commandIndex, s32 secondArg);
static s32                        _mineRefugeCircuitPanelActionMsg(Task* task, s32 messageId, const DirectionActionRequest* actionRequest, s32 secondArg);
static s32                        _mineRefugeSoundMsg(Task* task, s32 messageId, s32 cueId, s32 secondArg);
static void                       _mineRefugeCircuitPanelTask(Task* task);
static void                       _mineRefugeCutscenePromptTask(Task* task);

#include "../../shared/room/telephone_data.inc.c"

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

/// Refuge transition, key-item, circuit-panel, cutscene-command and sound handlers, then END.
///
/// Six ordered records are borrowed by the live room receiver. Payloads follow
/// each message's contract; the final TASK_MESSAGE_TABLE_END returns zero for
/// unsupported IDs. Keep this overlay and its callbacks loaded until teardown.
static const TaskMessageEntry _gMineRefugeRoomMessages[6] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _mineRefugeResolveRoomEventMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _mineRefugeUseKeyItemMsg },
    { DIRECTION_MESSAGE_ROOM_ACTION, _mineRefugeCircuitPanelActionMsg },
    { ROOM_MESSAGE_COMMAND, _mineRefugeRoomCommandMsg },
    { ROOM_MESSAGE_SOUND, _mineRefugeSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Circuit-panel and confirmation-prompt recipes, followed by TASK_DESC_END.
///
/// Live entries 0/1 are bodyless at priorities 32/31 and take zero payloads.
/// Direct spawns must exclude entry 2. Callback code and the prompt's saved
/// view remain room-owned until those tasks finish.
static TaskDesc _gMineRefugeSceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _mineRefugeCircuitPanelTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 31 } }, _mineRefugeCutscenePromptTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// World-space positions of the view-2 flare and the cyan glow in views 2 and 6.
static struct {
    SVECTOR flare; // Textured flare in mapped view 2, in world-coordinate units.
    SVECTOR cyan;  // Pulsing star in view 2 and burst in view 6, in world-coordinate units.
} _gMineRefugeGlowAnchors = {
    { 1910, -1850, 990, 0 },
    { 769, -798, 2640, 0 },
};

/// World-space position of the powered circuit-panel glow in mapped views 3..5.
static SVECTOR _gMineRefugePoweredPanelGlowPosition = { 2731, -1261, 4325, 0 };

WorldCoordRoomLighting gMineRefugeRoomLighting[1] = {
    { &_gMineRefugeRoomLights, _gMineRefugeAmbientByView },
};

WorldCollisionRoomResources gMineRefugeRoomCollisionResources[1] = {
    {
        .grid                 = &_gMineRefugeCollisionGrid,
        .viewBoundaryTriggers = _gMineRefugeViewBoundaryTriggers,
        .actionTriggers       = _gMineRefugeActionTriggers,
        .occluders            = NULL,
    },
};

u8* gMineRefugeRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gMineRefugeRoomViewCounts[1] = { MINE_REFUGE_VIEW_COUNT };

DirectionWarpEntry gMineRefugeWarpEntries[1] = {
    {
        .player         = { .yaw = { .word = 0 }, .x = 1472, .y = 0, .z = 288 },
        .unknown_10     = { 0 },
        .companion      = { .yaw = { .word = 0 }, .x = 1472, .y = 0, .z = 288 },
        .unknown_24     = { 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Room-space face normals of the refuge collision grid, with ONE per unit.
static SVECTOR _gMineRefugeCollisionGridNormals[12] = {
#include "assets/mine_refuge_collision_045E4_normals.inc"
};

/// Vertices of the refuge collision grid, in signed room-coordinate units.
static SVECTOR _gMineRefugeCollisionGridVerts[34] = {
#include "assets/mine_refuge_collision_045E4_verts.inc"
};

/// Indexed collision quads: one class-1 floor and fourteen class-0 wall faces.
static WorldCollisionGridFace _gMineRefugeCollisionGridFaces[15] = {
#include "assets/mine_refuge_collision_045E4_faces.inc"
};

/// Storage for the grid's two face-index lists, each ending at -1.
///
/// The lists start at elements 0 and 16; unused stored words are retained.
static s16 _gMineRefugeCollisionGridCells[24] = {
#include "assets/mine_refuge_collision_045E4_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gMineRefugeCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define MINE_REFUGE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gMineRefugeCollisionGridCells[(halfwordOffset)])
/// X-major cell directory borrowing the two terminated face-index lists.
static s16* _gMineRefugeCollisionGridTable[MINE_REFUGE_COLLISION_GRID_CELL_COUNT_X * MINE_REFUGE_COLLISION_GRID_CELL_COUNT_Z] = {
#include "assets/mine_refuge_collision_045E4_table.inc"
};
#undef MINE_REFUGE_CELL_FACE_LIST_AT_OFFSET

/// Room collision mesh indexed by a 1-by-2 XZ grid with origin (-200, -200).
///
/// Room setup borrows this descriptor and its writable asset pools, binding its
/// transform to the current view. They stay live while the refuge is loaded.
static WorldCollisionGrid _gMineRefugeCollisionGrid = {
    NULL,
    _gMineRefugeCollisionGridNormals,
    _gMineRefugeCollisionGridVerts,
    _gMineRefugeCollisionGridFaces,
    _gMineRefugeCollisionGridTable,
    200,
    200,
    MINE_REFUGE_COLLISION_GRID_CELL_COUNT_X,
    MINE_REFUGE_COLLISION_GRID_CELL_COUNT_Z,
    MINE_REFUGE_COLLISION_GRID_CELL_SIZE,
    ARRAY_SIZE(_gMineRefugeCollisionGridFaces),
};

ViewCamera gMineRefugeViewCameras[MINE_REFUGE_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -1500, 8300, -2500 } }, 289 },
    { { { { -3873, 0, -1331 }, { -400, 3906, 1165 }, { 1269, 1232, -3693 } }, { -550, 1610, -4900 } }, 269 },
    { { { { 3915, 0, -1201 }, { -777, 3121, -2535 }, { 915, 2651, 2984 } }, { -1005, 2661, -1831 } }, 246 },
    { { { { 0, 0, -ONE }, { -891, 3997, 0 }, { 3997, 891, 0 } }, { -725, 1640, -3952 } }, 680 },
    { { { { 0, 0, -ONE }, { -891, 3997, 0 }, { 3997, 891, 0 } }, { -725, 1640, -3952 } }, 680 },
    { { { { -1156, 0, 3929 }, { 2912, 2750, 856 }, { -2638, 3035, -776 } }, { -1291, 1474, -2830 } }, 629 },
    { { { { 2270, 0, 3408 }, { 2132, 3195, -1420 }, { -2659, 2562, 1771 } }, { -2075, 1908, -3789 } }, 598 },
};

/// Empty mapped-view-1 sprite list retaining decoded background strips.
static SpriteBatch _gMineRefugeView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 2's 79 source sprites.
static SpriteSource _gMineRefugeView2SpriteSources[79] = {
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 40, 32, 487, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, 48, 524, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 72, 48, 503, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 40, 88, 549, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 72, 88, 527, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 40, 24, 541, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 24, 541, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 40, 16, 582, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 16, 582, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 8, 620, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 8, 630, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 0, 675, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 0, 686, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, -16, 834, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -16, 834, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, -8, 753, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -8, 753, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, -24, 891, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -24, 891, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -16, 703, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 24, 719, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -144, -16, 675, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -16, 675, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 0, 626, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, 0, 653, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, 24, 751, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, 48, 688, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 72, 696, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 40, -32, 754, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 32, 0, 771, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 48, 761, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 16, 80, 484, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, 88, 467, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 88, 442, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, 104, 426, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 96, 425, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 96, 449, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 112, 420, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 112, 420, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, 8, 593, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -144, 56, 618, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 0, 554, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, 56, 564, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, -40, 760, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -32, 809, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -24, 971, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -24, 965, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -8, 812, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 0, 1005, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 8, 988, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 24, 875, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 32, 803, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, -104, 669, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -136, -104, 661, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, -88, 693, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, -64, 716, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -152, -32, 894, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, -56, 696, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, -24, 711, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -104, 705, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, -96, 783, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -96, 880, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, -64, 911, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, -88, 916, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -64, 945, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -32, 761, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -40, 997, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -40, 844, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, -40, 786, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, -32, 941, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, 8, 984, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -112, -16, 900, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, -8, 998, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 0, 786, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -8, 812, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 16, 916, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 24, 838, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 16, 815, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 8, 861, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
};

/// Six source ranges covering mapped view 2's sprites, followed by END.
///
/// The initial zero-count batch retains decoded background strips. The ranges
/// partition the source array, and the final range reaches its full extent.
static SpriteBatch _gMineRefugeView2SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 9, 0, 0, { 0, 0 } },
    { 28, 3, 0, 0, { 5, 0 } },
    { 31, 8, 0, 0, { 1, 0 } },
    { 39, 4, 0, 0, { 4, 0 } },
    { 43, ARRAY_SIZE(_gMineRefugeView2SpriteSources) - 43, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 3's 33 source sprites.
static SpriteSource _gMineRefugeView3SpriteSources[33] = {
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 104, 482, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 80, 513, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, 88, 495, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -128, 96, 489, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 104, 447, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -80, 753, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, -16, 896, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 0, 849, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -104, 0, 850, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -104, 16, 773, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, 16, 777, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 32, 724, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 48, 675, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 64, 641, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 80, 600, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 88, 600, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 88, 660, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 32, 725, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 48, 684, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 64, 642, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 80, 608, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -8, 883, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -88, -32, 833, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, -24, 817, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, -24, 798, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -16, 766, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, -16, 795, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -16, 865, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -8, 839, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, -8, 772, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -8, 812, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 0, 810, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 0, 807, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
};

/// Four source ranges covering mapped view 3's sprites, then an empty range and END.
///
/// The initial zero-count batch retains decoded background strips. The empty
/// range starts one past the sources and consumes no sprites or cached packets.
static SpriteBatch _gMineRefugeView3SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { 5, 1, 0, 0, { 3, 0 } },
    { 6, 15, 0, 0, { 2, 0 } },
    { 21, ARRAY_SIZE(_gMineRefugeView3SpriteSources) - 21, 0, 0, { 4, 0 } },
    { ARRAY_SIZE(_gMineRefugeView3SpriteSources), 0, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty mapped-view-4 sprite list retaining decoded background strips.
static SpriteBatch _gMineRefugeView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty mapped-view-5 sprite list retaining decoded background strips.
static SpriteBatch _gMineRefugeView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty mapped-view-6 sprite list retaining decoded background strips.
static SpriteBatch _gMineRefugeView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty mapped-view-7 sprite list retaining decoded background strips.
static SpriteBatch _gMineRefugeView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gMineRefugeSpriteViews[MINE_REFUGE_VIEW_COUNT] = {
    { { .empty = _gMineRefugeView1SpriteBatches }, _gMineRefugeView1SpriteBatches, NULL },
    { { .elements = _gMineRefugeView2SpriteSources }, _gMineRefugeView2SpriteBatches, NULL },
    { { .elements = _gMineRefugeView3SpriteSources }, _gMineRefugeView3SpriteBatches, NULL },
    { { .empty = _gMineRefugeView4SpriteBatches }, _gMineRefugeView4SpriteBatches, NULL },
    { { .empty = _gMineRefugeView5SpriteBatches }, _gMineRefugeView5SpriteBatches, NULL },
    { { .empty = _gMineRefugeView6SpriteBatches }, _gMineRefugeView6SpriteBatches, NULL },
    { { .empty = _gMineRefugeView7SpriteBatches }, _gMineRefugeView7SpriteBatches, NULL },
};

/// Two mutable point lights contributing in every refuge view.
///
/// The first has warm Q12 RGB intensity and a 2200-unit falloff radius; the
/// second is white with a 3000-unit radius. Both begin falloff at their origins.
/// Room setup parents their transforms to the view and shading updates attenuation.
static WorldCoordPointLight _gMineRefugePointLights[2] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2128, -1869, 1212 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3342, 2588 }, { 0, 0 } }, 0, 2200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1562, -2170, 3664 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 0, 3000 },
};

/// Refuge light collection borrowing two point lights and no directional or cone lights.
static WorldCoordRoomLights _gMineRefugeRoomLights = {
    0,
    NULL,
    ARRAY_SIZE(_gMineRefugePointLights),
    _gMineRefugePointLights,
    0,
    NULL,
};

/// Opposite-facing boundaries switching logical views 2 and 3.
///
/// Room setup binds and links both mutable records, including the LAST-marked
/// second boundary. Their geometry uses room units and ONE-scaled normals.
static WorldCollisionTrigger _gMineRefugeViewBoundaryTriggers[2] = {
    { NULL, NULL, NULL, { 2048, -1600, 2790, 0 }, { { -1888, -1904, 176, 0 }, { 1888, -1904, -176, 0 }, { -1888, 1904, 176, 0 }, { 1888, 1904, -176, 0 } }, { -382, 0, -4086, 0 }, { 0, 0, ONE, 0 }, 2684, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2017, -1568, 2734, 0 }, { { 1792, -1904, -176, 0 }, { -1792, -1904, 176, 0 }, { 1792, 1904, -176, 0 }, { -1792, 1904, 176, 0 } }, { 399, 0, 4075, 0 }, { 0, 0, ONE, 0 }, 2610, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Refuge interactions: gorge exit, circuit panel, CAP 3, cutscene prompt, CAP 2 and CAP 11.
///
/// The exit uses departure slot 1 and gorge arrival slot 2. Every record requires
/// the player's facing test; the LAST-marked CAP-11 record is also live.
/// Room setup binds their coordinates and links them until room resources are cleared.
static WorldCollisionTrigger _gMineRefugeActionTriggers[6] = {
    { NULL, NULL, NULL, { 1504, -48, 208, 0 }, { { -576, 0, -400, 0 }, { 576, 0, -400, 0 }, { -576, 0, 400, 0 }, { 576, 0, 400, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, ONE, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_MINE_GORGE, (MINE_REFUGE_EXIT_WARP_SLOT << 4) | MINE_REFUGE_GORGE_ARRIVAL_SLOT, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2368, -64, 4257, 0 }, { { -448, 0, -880, 0 }, { 448, 0, -880, 0 }, { -448, 0, 880, 0 }, { 448, 0, 880, 0 } }, { 0, 4105, 0, 0 }, { -4091, 0, -201, 0 }, 987, WORLD_COLLISION_TRIGGER_ACTION_ROOM, MINE_REFUGE_TRIGGER_CIRCUIT_PANEL_ACTION, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1024, -64, 4544, 0 }, { { -576, 0, -400, 0 }, { 576, 0, -400, 0 }, { -576, 0, 400, 0 }, { 576, 0, 400, 0 } }, { 0, 4102, 0, 0 }, { 4091, 0, -201, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_CAP, MINE_REFUGE_TRIGGER_CAP_3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1024, -64, 2416, 0 }, { { -576, 0, -576, 0 }, { 576, 0, -576, 0 }, { -576, 0, 576, 0 }, { 576, 0, 576, 0 } }, { 0, 4105, 0, 0 }, { 4095, 0, 0, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_CAP, MINE_REFUGE_TRIGGER_CUTSCENE_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2592, -64, 1808, 0 }, { { -576, 0, -704, 0 }, { 576, 0, -704, 0 }, { -576, 0, 704, 0 }, { 576, 0, 704, 0 } }, { 0, 4097, 0, 0 }, { -4095, 0, 0, 0 }, 909, WORLD_COLLISION_TRIGGER_ACTION_CAP, MINE_REFUGE_TRIGGER_CAP_2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2464, -64, 2944, 0 }, { { -576, 0, -400, 0 }, { 576, 0, -400, 0 }, { -576, 0, 400, 0 }, { 576, 0, 400, 0 } }, { 0, 4102, 0, 0 }, { -ONE, 0, 0, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_CAP, MINE_REFUGE_TRIGGER_CAP_11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Placement-entry keys and decimal file numbers for this room's resource recipes.
enum {
    MINE_REFUGE_RESOURCE_CIRCUIT_PANEL = 101,
    MINE_REFUGE_FILE_CIRCUIT_PANEL     = 481,
};

/// Circuit Panel resource recipes for placement variant 1.
///
/// Loads the circuit-panel overlay even though the paired default placements
/// contain only AREA_PLACEMENT_END; this layout spawns no actors. The room
/// starts the panel task separately after its descriptor overlay is loaded.
static AreaResource _gMineRefugeVariant1Resources[2] = {
    { .entryId = MINE_REFUGE_RESOURCE_CIRCUIT_PANEL, .fileNumber = MINE_REFUGE_FILE_CIRCUIT_PANEL, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_50, .taskIndex = 0, .taskTable = &gActor548100PanelTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Terminator-only default layout: no actors are spawned from placement records.
///
/// The separate resource table still loads the circuit-panel task's overlay.
static AreaPlacement _gMineRefugeDefaultPlacements[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gMineRefugeAreaVariants[MINE_REFUGE_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = _gMineRefugeDefaultPlacements, .resources = _gMineRefugeVariant1Resources },
};

/// Logical-view ambient minima: 520 in view 3 and 16 in the other six views.
///
/// Levels have four fractional bits. Entry zero counts the following views;
/// each stored luma equals its equal RGB components.
static WorldCoordRoomAmbientEntry _gMineRefugeAmbientByView[MINE_REFUGE_VIEW_COUNT + 1] = {
    { .viewCount = ARRAY_SIZE(_gMineRefugeAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 520, 520, 520, 520 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Paired loaded-bank footstep requests for the class-1 floor.
///
/// Walk and scripted jump use entry 65, running uses 67; cue 1 adds one and
/// companion playback adds 100. The surface record borrows this room storage.
static WorldCollisionFootstepSounds _gMineRefugeFootstepSounds = {
    SOUND_SCRIPT_REQUEST_TYPE_1 | MINE_REFUGE_FOOTSTEP_WALK_SCRIPT,
    SOUND_SCRIPT_REQUEST_TYPE_1 | MINE_REFUGE_FOOTSTEP_RUN_SCRIPT,
    SOUND_SCRIPT_REQUEST_TYPE_1 | MINE_REFUGE_FOOTSTEP_WALK_SCRIPT,
};

/// Blocking, weapon-impact and pushback properties without footsteps for classes 0 and 2..7.
static WorldCollisionSurfaceProperties _gMineRefugeSilentSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class-1 floor properties: blocks probes, permits weapon impacts, applies pushback and footsteps.
static WorldCollisionSurfaceProperties _gMineRefugeSurfaceClass1Properties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gMineRefugeFootstepSounds,
};

WorldCollisionSurfaceProperties* gMineRefugeSurfacePropertiesByClass[8] = {
    &_gMineRefugeSilentSurfaceProperties,
    &_gMineRefugeSurfaceClass1Properties,
    &_gMineRefugeSilentSurfaceProperties,
    &_gMineRefugeSilentSurfaceProperties,
    &_gMineRefugeSilentSurfaceProperties,
    &_gMineRefugeSilentSurfaceProperties,
    &_gMineRefugeSilentSurfaceProperties,
    &_gMineRefugeSilentSurfaceProperties,
};

Task* gRoomCutsceneSoundTask = NULL;

/// Borrowed live circuit-panel task receiving battery-item use messages.
///
/// NULL outside a panel session. The room's scene controller publishes the
/// spawned actor-548100 task and clears this handle immediately after exit
/// polling releases it; room initialization also clears it.
static Task* _gMineRefugeCircuitPanelTask = NULL;

/// Original logical view held while the refuge confirmation prompt is active.
///
/// Replaced before each prompt; refusal restores it and acceptance passes it
/// to the scene runner. At most one prompt may use this snapshot at a time.
/// Preserve the three following nonzero bytes, whose meaning is unproven.
static _MineRefugePromptViewStorage _gMineRefugePromptViewStorage = {
    .restoreView = 0,
    .unknown_1   = { 47, 36, 50 },
};

static void _mineRefugeDrawPulsingCyanStar(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _mineRefugeDrawPulsingCyanBurst(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _mineRefugeDrawLayeredGlow(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\x1A\x1C"
#include "../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);

void mineRefugeTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/room/telephone_panels.inc.c"

#include "../../shared/room/room_cutscene_task.inc.c"

/// Refuge room callbacks: 0 register/music/CAP sound setup, 1 idle, 2 release.
///
/// Dispatch copies all three callbacks and requires `Task::state` in 0..2.
/// No terminator or bounds check; keep the room overlay loaded through exit.
static const TaskFuncTable3 _gMineRefugeRoomTaskStates = {
    {
        _mineRefugeInitRoomTask,
        _mineRefugeIdleRoomTask,
        taskKill,
    },
};

/// Runs the circuit-panel session, publishing its live task for battery-item use.
///
/// States 0..5 advance through the pending follow-up caption, one opening-delay
/// tick, panel spawn, exit polling, one closing-delay tick and the close sound.
/// Clears the published handle immediately after polling its exit. Requires the
/// panel overlay and CAP commands loaded; the panel spawn must succeed.
static void _mineRefugeCircuitPanelTask(Task* task)
{
    enum {
        MINE_REFUGE_PANEL_INITIALIZE    = 0,
        MINE_REFUGE_PANEL_OPEN_DELAY    = 1,
        MINE_REFUGE_PANEL_OPEN          = 2,
        MINE_REFUGE_PANEL_WAIT_EXIT     = 3,
        MINE_REFUGE_PANEL_CLOSE_DELAY   = 4,
        MINE_REFUGE_PANEL_CLOSE         = 5,
        MINE_REFUGE_PANEL_SCENE_PENDING = 1,
        MINE_REFUGE_PANEL_SCENE_SEEN    = 2,
        MINE_REFUGE_CAP_PANEL_FOLLOWUP  = 0xF,
    };
    s32 panelResult;

    switch (task->state) {
        case MINE_REFUGE_PANEL_INITIALIZE:
            if (gameFlagGetNibble(GAME_FLAG_MINE_REFUGE_SCENE_STATE) == MINE_REFUGE_PANEL_SCENE_PENDING) {
                gameFlagSetNibble(GAME_FLAG_MINE_REFUGE_SCENE_STATE, MINE_REFUGE_PANEL_SCENE_SEEN);
                capRunCommandWithTransition(MINE_REFUGE_CAP_PANEL_FOLLOWUP);
            }
            task->state = task->state + 1;
            return;
        case MINE_REFUGE_PANEL_OPEN:
            sndEvtRequestScriptStart(SOUND_MINE_REFUGE_CIRCUIT_PANEL_OPEN, 0, 0);
            _gMineRefugeCircuitPanelTask = taskSpawnFromTable(&gActor548100PanelTaskDesc, 0, 0, 0);
            task->state                  = task->state + 1;
            return;
        case MINE_REFUGE_PANEL_WAIT_EXIT:
            if (taskPollKill(_gMineRefugeCircuitPanelTask, &panelResult) != 0) {
                // Item-use messages must stop reaching the panel after its exit.
                _gMineRefugeCircuitPanelTask = NULL;
                task->state                  = task->state + 1;
            }
            return;
        case MINE_REFUGE_PANEL_OPEN_DELAY:
        case MINE_REFUGE_PANEL_CLOSE_DELAY:
            task->state = task->state + 1;
            return;
        case MINE_REFUGE_PANEL_CLOSE:
            sndEvtRequestScriptStart(SOUND_MINE_REFUGE_CIRCUIT_PANEL_CLOSE, 0, 0);
            taskKill(task);
            break;
    }
}

#include "../../shared/room/room_cutscene_sound_task.inc.c"

/// Relays collected key-item use to the active circuit-panel task.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`, forwarding the item ID, second word
/// and panel's item-menu reply unchanged. Refuses the use while no panel task
/// is published. The scene owns that task and clears its handle after teardown.
static s32 _mineRefugeUseKeyItemMsg(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    s32 itemUseReply;

    if (_gMineRefugeCircuitPanelTask == NULL) {
        itemUseReply = ROOM_KEY_ITEM_USE_REFUSED;
    } else {
        itemUseReply = taskMessageDispatch(_gMineRefugeCircuitPanelTask, messageId, itemId, secondArg);
    }
    return itemUseReply;
}

/// Accepts a room transition after resolving its Mine/Shelter destination variant.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; borrows a complete request and writable
/// reply through dispatch, allowing both pointers to address the same record.
/// Copies all eight bytes before resolving the reply's room. Queries preserve
/// the copied destination. Requires the loaded map overlay and always returns 1.
static s32 _mineRefugeResolveRoomEventMsg(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return 1;
}

/// Holds the player in the scene view while the first cutscene prompt runs.
///
/// Saves the live view for the prompt task before starting CAP command 13.
/// The room permits only one outstanding prompt; its task restores play or
/// hands the saved view to the cutscene runner after CAP completes.
/// Requires loaded CAP, sound and task records and a live player. Failed prompt
/// allocation leaves the view changed and the player held and hidden.
static inline void _mineRefugeStartCutscenePrompt(void)
{
    enum {
        MINE_REFUGE_CAP_PROMPT         = 0xD,
        MINE_REFUGE_TASK_PROMPT        = 1,
        MINE_REFUGE_PROMPT_START_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 3),
    };
    u8 savedView;

    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
    savedView                                              = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = MINE_REFUGE_CUTSCENE_VIEW;
    _gMineRefugePromptViewStorage.restoreView              = savedView;
    sndEvtRequestScriptStart(MINE_REFUGE_PROMPT_START_SOUND, 0, 0);
    capRunCommand(MINE_REFUGE_CAP_PROMPT, CAP_PLAYBACK_IN_PLACE);
    taskSpawnFromTable(_gMineRefugeSceneTaskDescs, MINE_REFUGE_TASK_PROMPT, 0, 0);
}

/// Starts the refuge cutscene, asking for confirmation until it has been accepted.
///
/// Handles `ROOM_MESSAGE_COMMAND` index 1; other indices and the second word
/// are ignored. The first prompt holds and hides the player in view 6 until its
/// task restores the saved view or hands it to the cutscene runner. Returns zero.
/// Requires loaded room CAP resources and at most one prompt or cutscene active.
static s32 _mineRefugeRoomCommandMsg(Task* task, s32 messageId, s32 commandIndex, s32 secondArg)
{
    enum { MINE_REFUGE_COMMAND_CUTSCENE = 1 };

    if (commandIndex == MINE_REFUGE_COMMAND_CUTSCENE) {
        if (gameFlagGetNibble(GAME_FLAG_MINE_REFUGE_PROMPT_ACCEPTED) != 0) {
            _mineRefugeStartCutscene(0U);
        } else {
            _mineRefugeStartCutscenePrompt();
        }
    }
    return 0;
}

/// Opens the circuit panel unless the secret passage has already been opened.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION` action 1, borrowing the four-byte
/// request only during dispatch. Its control and argument fields and the second
/// word are ignored. An opened passage instead runs CAP command 10; other
/// actions do nothing. Returns zero. Requires the panel overlay and room CAP.
static s32 _mineRefugeCircuitPanelActionMsg(Task* task, s32 messageId, const DirectionActionRequest* actionRequest, s32 secondArg)
{
    enum {
        MINE_REFUGE_ACTION_CIRCUIT_PANEL  = 1,
        MINE_REFUGE_CAP_PANEL_UNAVAILABLE = 0xA,
        MINE_REFUGE_TASK_CIRCUIT_PANEL    = 0,
    };
    u8 actionId = actionRequest->actionId;

    if (actionId == MINE_REFUGE_ACTION_CIRCUIT_PANEL) {
        // Action 1 also equals the passage flag's opened state.
        if (gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE) != actionId) {
            gameFlagSetNibble(GAME_FLAG_0C4, 0);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(_gMineRefugeSceneTaskDescs, MINE_REFUGE_TASK_CIRCUIT_PANEL, 0, 0);
        } else {
            capRunCommandWithTransition(MINE_REFUGE_CAP_PANEL_UNAVAILABLE);
        }
    }
    return 0;
}

/// Maps room sound cues 12, 99 and 103 to Mine Refuge scripts 12, 15 and 13.
///
/// Handles `ROOM_MESSAGE_SOUND`; other cues and the second payload are ignored.
/// Queues each script with no pan offset or attenuation and returns zero.
static s32 _mineRefugeSoundMsg(Task* task, s32 messageId, s32 cueId, s32 secondArg)
{
    enum {
        MINE_REFUGE_SOUND_CUE_SCRIPT_0C = 0x0C,
        MINE_REFUGE_SOUND_CUE_SCRIPT_0F = 0x63,
        MINE_REFUGE_SOUND_CUE_SCRIPT_0D = 0x67,
    };

    switch (cueId) {
        case MINE_REFUGE_SOUND_CUE_SCRIPT_0C:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 0x0C), 0, 0);
            break;
        case MINE_REFUGE_SOUND_CUE_SCRIPT_0F:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 0x0F), 0, 0);
            break;
        case MINE_REFUGE_SOUND_CUE_SCRIPT_0D:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 0x0D), 0, 0);
            break;
    }
    return 0;
}

/// Resolves the cutscene prompt, restoring play or handing its saved view to the scene.
///
/// State 0 waits for CAP completion; choice key 5 advances to acceptance on the
/// next tick, while other keys restore the camera and player control and drawing.
/// State 1 records acceptance and starts the cutscene. Releases this task after
/// either outcome; the saved view must remain unchanged while the prompt waits.
static void _mineRefugeCutscenePromptTask(Task* task)
{
    enum {
        MINE_REFUGE_PROMPT_WAIT_CAP = 0,
        MINE_REFUGE_PROMPT_ACCEPT   = 1,
        MINE_REFUGE_CAP_KEY_ACCEPT  = 5,
    };
    switch (task->state) {
        case MINE_REFUGE_PROMPT_WAIT_CAP:
            if (capIsBusy() != 0) {
                return;
            }
            if (capGetVariantKey() == MINE_REFUGE_CAP_KEY_ACCEPT) {
                task->state = task->state + 1;
                return;
            }
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = _gMineRefugePromptViewStorage.restoreView;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
            break;
        case MINE_REFUGE_PROMPT_ACCEPT:
            gameFlagSetNibble(GAME_FLAG_MINE_REFUGE_PROMPT_ACCEPTED, 1);
            _mineRefugeStartCutscene(_gMineRefugePromptViewStorage.restoreView);
            break;
        default:
            return;
    }
    taskKill(task);
}

/// Starts the refuge cutscene with its room-owned script and sound sequence.
///
/// `restoreView` is 0 to save the live view and force view 6 with the opening
/// sound, or a saved room view 1..7 to restore after the prompt without repeating
/// that sound or changing the current view. Final dialogue index 15 uses CAP
/// slot 14 in the current file and follow-up command 1; otherwise slot/file 1
/// and follow-up command 5. The record is borrowed until the runner finishes:
/// no second cutscene may overwrite it while active. Requires loaded CAP resources.
static void _mineRefugeStartCutscene(s32 restoreView)
{
    enum {
        MINE_REFUGE_DIALOGUE_FINAL     = 0xF,
        MINE_REFUGE_CAP_SLOT_FINAL     = 0xE,
        MINE_REFUGE_CAP_SLOT_STORY     = 1,
        MINE_REFUGE_CAP_FILE_CURRENT   = 0,
        MINE_REFUGE_CAP_FILE_STORY     = 1,
        MINE_REFUGE_CAP_FOLLOWUP_FINAL = 1,
        MINE_REFUGE_CAP_FOLLOWUP_STORY = 5,
        MINE_REFUGE_TASK_CUTSCENE      = 0,
    };
    s32 followupCommandIndex;

    if (restoreView != 0) {
        _gMineRefugeCutscene.startSound = 0;
        _gMineRefugeCutscene.view       = -restoreView;
    } else {
        _gMineRefugeCutscene.view       = MINE_REFUGE_CUTSCENE_VIEW;
        _gMineRefugeCutscene.startSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 3);
    }
    if (gameFlagGetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX) == MINE_REFUGE_DIALOGUE_FINAL) {
        followupCommandIndex         = MINE_REFUGE_CAP_FOLLOWUP_FINAL;
        _gMineRefugeCutscene.capSlot = MINE_REFUGE_CAP_SLOT_FINAL;
        _gMineRefugeCutscene.capFile = MINE_REFUGE_CAP_FILE_CURRENT;
    } else {
        followupCommandIndex         = MINE_REFUGE_CAP_FOLLOWUP_STORY;
        _gMineRefugeCutscene.capSlot = MINE_REFUGE_CAP_SLOT_STORY;
        _gMineRefugeCutscene.capFile = MINE_REFUGE_CAP_FILE_STORY;
    }
    // Keep the room-owned record live while the runner and its sound task use it.
    _gMineRefugeCutscene.skipScene       = 0;
    _gMineRefugeCutscene.endSound        = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 6);
    _gMineRefugeCutscene.sceneSound      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 4);
    _gMineRefugeCutscene.afterSceneSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 5);
    taskSpawnFromTable(_gRoomCutsceneTaskDescs, MINE_REFUGE_TASK_CUTSCENE, followupCommandIndex, &_gMineRefugeCutscene);
}

/// Installs the room handlers and enables CAP completion sound cues.
///
/// Runs in state 0, registers this task in the room slot, clears the panel
/// handle, selects music entry 1 and advances to the idle state.
static void _mineRefugeInitRoomTask(Task* task)
{
    enum {
        MINE_REFUGE_SCENE_MUSIC_ENTRY             = 1,
        MINE_REFUGE_CAP_COMPLETION_SOUNDS_ENABLED = 1,
    };

    task->msgTable = _gMineRefugeRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _gMineRefugeCircuitPanelTask    = NULL;
    gStageCountdownMusicEntry       = MINE_REFUGE_SCENE_MUSIC_ENTRY;
    task->state                     = task->state + 1;
    gCapRoomCompletionSoundsEnabled = MINE_REFUGE_CAP_COMPLETION_SOUNDS_ENABLED;
}

/// Keeps the room message task idle while its installed handlers remain available.
static void _mineRefugeIdleRoomTask(Task* task)
{
    // Retain the idle callback's 16-byte stack frame.
    char stackFrame[0x10];
}

void mineRefugeRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gMineRefugeRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_flare.inc.c"

/// Initializes the packet header and centre-to-rim colors of a Gouraud glow quad.
///
/// Vertex 2 receives the low byte of each intensity without clamping; vertices
/// 0, 1 and 3 are black. The untextured polygon command starts opaque, clearing
/// any previous semitransparency setting.
///
/// Borrows one word-aligned, writable `POLY_G4` without allocating it. Coordinates
/// and the DMA link remain untouched. The caller places vertex 2 at the centre
/// and the other vertices on the rim, links the packet and selects blending
/// before drawing; queued storage must remain live until GPU drawing completes.
static inline void _mineRefugeInitGlowQuad(POLY_G4* quad, s32 redIntensity, s32 greenIntensity, s32 blueIntensity)
{
    setPolyG4(quad);
    setRGB0(quad, 0, 0, 0);
    setRGB1(quad, 0, 0, 0);
    setRGB2(quad, redIntensity, greenIntensity, blueIntensity);
    setRGB3(quad, 0, 0, 0);
}

/// Draws a pulsing cyan diamond and two crossing diagonals at a world point.
///
/// The signed low halfword of `pulseRate` is in 4096 angle units per animation
/// frame. Intensity is `rsin(animFrame * pulseRate) / 34 + 120` (0..240).
/// The signed low halfword of `radiusScale` gives a pixel half-extent of
/// `radiusScale * 32 / depth`, where depth is camera Z / 4. The second diagonal
/// extends twice as far as the diamond. Rejects negative GTE flags and requires
/// nonzero depth. Borrows the point during this call, reserves and releases one
/// `EffectCentreScratch`, and queues two additive quads and two three-point
/// lines plus their blend commands in the current frame.
static void _mineRefugeDrawPulsingCyanStar(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale)
{
    EffectCentreScratch* scratchEnd;
    EffectCentreScratch* scratchBlock;
    POLY_G4*             quad;
    LINE_G3*             diagonal;
    s32                  pulseSine;
    s32                  cyanIntensity;
    s32                  screenRadius;
    s32                  partIndex;
    s32                  xRadiusMultiple;
    s32                  yRadiusMultiple;
    s32                  verticalSide;
    u16                  screenX;
    u16                  screenY;

    scratchEnd   = *SCRATCH_STACK_CURSOR_SLOT;
    scratchBlock = (*SCRATCH_STACK_CURSOR_SLOT = scratchEnd - 1);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (scratchBlock->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        pulseSine                  = rsin(gDisplayState.animFrame * (s16)pulseRate);
        screenRadius               = ((s16)radiusScale * GLOW_DIAMOND_RADIUS_SCALE) / scratchBlock->depth;
        partIndex                  = 0;
        cyanIntensity              = pulseSine / GLOW_PULSE_DIVISOR + GLOW_PULSE_BASE_INTENSITY;
        scratchBlock->screenExtent = screenRadius;
        // Fill the diamond, then add diagonals; the second diagonal has twice the extent.
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _mineRefugeInitGlowQuad(quad, 0, cyanIntensity, cyanIntensity);
            quad->x0     = scratchBlock->screenX - scratchBlock->screenExtent;
            screenX      = scratchBlock->screenX;
            quad->x2     = screenX;
            quad->x1     = screenX;
            quad->x3     = scratchBlock->screenX + scratchBlock->screenExtent;
            screenY      = scratchBlock->screenY;
            quad->y3     = screenY;
            quad->y2     = screenY;
            quad->y0     = screenY;
            verticalSide = partIndex * 2;
            quad->y1     = (scratchBlock->screenY - scratchBlock->screenExtent) + (scratchBlock->screenExtent * verticalSide);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratchBlock->depth),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratchBlock->depth);
            partIndex++;
        } while (partIndex < 2);

        partIndex = 0;
        do {
            diagonal       = gGpuPrimCursor;
            gGpuPrimCursor = diagonal + 1;
            setLineG3(diagonal);
            setRGB0(diagonal, 0, 0, 0);
            setRGB1(diagonal, 0, cyanIntensity, cyanIntensity);
            setRGB2(diagonal, 0, 0, 0);
            xRadiusMultiple = partIndex * 3 - 1;
            yRadiusMultiple = partIndex + 1;
            diagonal->x0    = scratchBlock->screenX + (scratchBlock->screenExtent * xRadiusMultiple);
            diagonal->y0    = scratchBlock->screenY - (scratchBlock->screenExtent * yRadiusMultiple);
            diagonal->x1    = scratchBlock->screenX;
            diagonal->y1    = scratchBlock->screenY;
            diagonal->x2    = scratchBlock->screenX - (scratchBlock->screenExtent * xRadiusMultiple);
            diagonal->y2    = scratchBlock->screenY + (scratchBlock->screenExtent * yRadiusMultiple);
            addPrim((&gGpuCurrentOt[((u32)scratchBlock->depth << gDisplayState.otDepthShift) >> 4 & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK >> 2)]),
                    diagonal);
            gpuSetPrimitiveBlendMode(diagonal, GPU_BLEND_ADD, scratchBlock->depth);
            partIndex = yRadiusMultiple;
        } while (partIndex < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Draws a pulsing cyan disc with four extended blades at a world point.
///
/// The signed low halfword of `pulseRate` is in 4096 angle units per animation
/// frame. Intensity is `rsin(animFrame * pulseRate) / 34 + 120` (0..240).
/// The signed low halfword of `radiusScale` gives outer and inner pixel radii
/// of `radiusScale * 64 / depth` and `radiusScale * 8 / depth`, where depth is
/// camera Z / 4. Eight half-bright outer wedges have full-bright copies at
/// half radius; four half-bright blades alternate tips at one and two outer
/// radii. Rejects negative GTE flags and requires nonzero depth. Borrows the
/// point during this call, reserves and releases one `EffectShapeScratch`,
/// and queues twenty additive quads plus blend commands in the current frame.
static void _mineRefugeDrawPulsingCyanBurst(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale)
{
    void**              scratchCursor;
    EffectShapeScratch* scratchEnd;
    EffectShapeScratch* scratchBlock;
    POLY_G4*            quad;
    s32                 pulseSine;
    s32                 cyanIntensity;
    s32                 halfIntensity;
    s32                 radiusNumerator;
    s32                 angle;
    s32                 middleAngle;
    s32                 endAngle;
    s32                 rimAngle;

    scratchCursor = SCRATCH_STACK_CURSOR_SLOT;
    scratchEnd    = *scratchCursor;
    scratchBlock  = (*scratchCursor = scratchEnd - 1);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (scratchBlock->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        pulseSine                        = rsin(gDisplayState.animFrame * (s16)pulseRate);
        angle                            = 0;
        radiusNumerator                  = (s16)radiusScale;
        scratchBlock->extent.burst.outer = (radiusNumerator * GLOW_RADIUS_SCALE) / scratchBlock->depth;
        scratchBlock->extent.burst.inner = (radiusNumerator * GLOW_INNER_RADIUS_SCALE) / scratchBlock->depth;
        cyanIntensity                    = pulseSine / GLOW_PULSE_DIVISOR + GLOW_PULSE_BASE_INTENSITY;
        // Pair eight dim outer wedges with brighter wedges at half the radius.
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            halfIntensity  = (s16)cyanIntensity >> 1;
            _mineRefugeInitGlowQuad(quad, 0, halfIntensity, halfIntensity);
            quad->x0    = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(angle)) >> GLOW_TRIG_SHIFT);
            middleAngle = angle + GLOW_SIXTEENTH_TURN;
            quad->y0    = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(angle)) >> GLOW_TRIG_SHIFT);
            quad->x1    = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(middleAngle)) >> GLOW_TRIG_SHIFT);
            quad->y1    = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(middleAngle)) >> GLOW_TRIG_SHIFT);
            endAngle    = angle + GLOW_EIGHTH_TURN;
            quad->x2    = scratchBlock->screenX;
            quad->y2    = scratchBlock->screenY;
            quad->x3    = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(endAngle)) >> GLOW_TRIG_SHIFT);
            quad->y3    = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(endAngle)) >> GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratchBlock->depth),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratchBlock->depth);

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _mineRefugeInitGlowQuad(quad, 0, cyanIntensity, cyanIntensity);
            quad->x0 = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(angle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->y0 = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(angle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->x1 = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(middleAngle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->y1 = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(middleAngle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->x2 = scratchBlock->screenX;
            quad->y2 = scratchBlock->screenY;
            quad->x3 = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(endAngle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->y3 = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(endAngle)) >> (GLOW_TRIG_SHIFT + 1));
            angle    = endAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratchBlock->depth),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratchBlock->depth);
        } while (angle < GLOW_FULL_TURN);

        // Add four blades, alternating tips at one and two outer radii.
        cyanIntensity = halfIntensity;
        angle         = GLOW_EIGHTH_TURN;
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _mineRefugeInitGlowQuad(quad, 0, cyanIntensity, cyanIntensity);
            rimAngle = angle - GLOW_QUARTER_TURN;
            quad->x0 = scratchBlock->screenX + ((scratchBlock->extent.burst.inner * rsin(rimAngle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->y0 = scratchBlock->screenY + ((scratchBlock->extent.burst.inner * rcos(rimAngle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->x1 = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(angle)) >> GLOW_TRIG_SHIFT);
            quad->y1 = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(angle)) >> GLOW_TRIG_SHIFT);
            rimAngle = angle + GLOW_QUARTER_TURN;
            quad->x2 = scratchBlock->screenX;
            quad->y2 = scratchBlock->screenY;
            quad->x3 = scratchBlock->screenX + ((scratchBlock->extent.burst.inner * rsin(rimAngle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->y3 = scratchBlock->screenY + ((scratchBlock->extent.burst.inner * rcos(rimAngle)) >> (GLOW_TRIG_SHIFT + 1));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratchBlock->depth),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratchBlock->depth);

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _mineRefugeInitGlowQuad(quad, 0, cyanIntensity, cyanIntensity);
            quad->x0 = scratchBlock->screenX + ((scratchBlock->extent.burst.inner * rsin(angle)) >> GLOW_TRIG_SHIFT);
            quad->y0 = scratchBlock->screenY + ((scratchBlock->extent.burst.inner * rcos(angle)) >> GLOW_TRIG_SHIFT);
            quad->x1 = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(rimAngle)) >> (GLOW_TRIG_SHIFT - 1));
            quad->y1 = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(rimAngle)) >> (GLOW_TRIG_SHIFT - 1));
            rimAngle = angle + GLOW_HALF_TURN;
            quad->x2 = scratchBlock->screenX;
            quad->y2 = scratchBlock->screenY;
            quad->x3 = scratchBlock->screenX + ((scratchBlock->extent.burst.inner * rsin(rimAngle)) >> GLOW_TRIG_SHIFT);
            quad->y3 = scratchBlock->screenY + ((scratchBlock->extent.burst.inner * rcos(rimAngle)) >> GLOW_TRIG_SHIFT);
            angle    = rimAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratchBlock->depth),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratchBlock->depth);
        } while (angle < GLOW_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Draws three overlapping, flickering discs with a fading centre at a world point.
///
/// The signed low halfword of `radiusScale` gives the first pixel radius as
/// `radiusScale * 64 / depth`, where depth is camera Z / 4. Later discs double
/// that radius and halve each centre color byte; each disc has eight wedges.
/// `packedColor` is RGB444 (0..0xFFF); each nibble is scaled by 16 and odd
/// animation frames add 32 before byte narrowing. Rims are black.
///
/// Rejects negative GTE flags and requires nonzero depth. Borrows the point
/// during this call and queues twenty-four additive quads plus blend commands.
/// Reserves one `GlowCentreScratch` even for a rejected projection and leaves
/// that reservation active until the enclosing scratch-stack reset.
static void _mineRefugeDrawLayeredGlow(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor)
{
    enum {
        MINE_REFUGE_GLOW_DISC_COUNT         = 3,
        MINE_REFUGE_GLOW_FLICKER_SHIFT      = 5,
        MINE_REFUGE_GLOW_COLOR_NIBBLE_SHIFT = 4,
        MINE_REFUGE_GLOW_COLOR_NIBBLE_MASK  = 0xF,
        MINE_REFUGE_GLOW_COLOR_BYTE_MASK    = 0xF0,
    };

    void**             scratchCursor;
    GlowCentreScratch* scratchEnd;
    GlowCentreScratch* scratchBlock;
    POLY_G4*           quad;
    s32                screenRadius;
    s32                discIndex;
    s32                angle;
    s32                middleAngle;
    s32                endAngle;
    s32                shiftedColor;
    s32                flickerIntensity;
    s32                redIntensity;
    s32                greenIntensity;
    s32                blueIntensity;

    scratchCursor = SCRATCH_STACK_CURSOR_SLOT;
    scratchEnd    = *scratchCursor;
    scratchBlock  = (*scratchCursor = scratchEnd - 1);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->sx);
    gte_stflg(&(scratchEnd - 1)->flag);
    if (scratchBlock->flag >= 0) {
        gte_stszotz(&scratchBlock->otz);
        screenRadius         = ((s16)radiusScale * GLOW_RADIUS_SCALE) / (scratchEnd - 1)->otz;
        discIndex            = 0;
        flickerIntensity     = ((u8)gDisplayState.animFrame & 1) << MINE_REFUGE_GLOW_FLICKER_SHIFT;
        shiftedColor         = packedColor << 16;
        redIntensity         = flickerIntensity + ((shiftedColor >> (16 + MINE_REFUGE_GLOW_COLOR_NIBBLE_SHIFT)) & MINE_REFUGE_GLOW_COLOR_BYTE_MASK);
        greenIntensity       = flickerIntensity + ((shiftedColor >> 16) & MINE_REFUGE_GLOW_COLOR_BYTE_MASK);
        blueIntensity        = flickerIntensity + ((packedColor & MINE_REFUGE_GLOW_COLOR_NIBBLE_MASK) << MINE_REFUGE_GLOW_COLOR_NIBBLE_SHIFT);
        scratchBlock->radius = screenRadius;
        // Each filled disc doubles the radius and halves the centre color.
        do {
            angle = 0;
            do {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                _mineRefugeInitGlowQuad(quad, redIntensity, greenIntensity, blueIntensity);
                quad->x0    = scratchBlock->sx + ((scratchBlock->radius * rsin(angle)) >> GLOW_TRIG_SHIFT);
                middleAngle = angle + GLOW_SIXTEENTH_TURN;
                quad->y0    = scratchBlock->sy + ((scratchBlock->radius * rcos(angle)) >> GLOW_TRIG_SHIFT);
                quad->x1    = scratchBlock->sx + ((scratchBlock->radius * rsin(middleAngle)) >> GLOW_TRIG_SHIFT);
                quad->y1    = scratchBlock->sy + ((scratchBlock->radius * rcos(middleAngle)) >> GLOW_TRIG_SHIFT);
                endAngle    = angle + GLOW_EIGHTH_TURN;
                quad->x2    = scratchBlock->sx;
                quad->y2    = scratchBlock->sy;
                quad->x3    = scratchBlock->sx + ((scratchBlock->radius * rsin(endAngle)) >> GLOW_TRIG_SHIFT);
                quad->y3    = scratchBlock->sy + ((scratchBlock->radius * rcos(endAngle)) >> GLOW_TRIG_SHIFT);
                angle       = endAngle;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratchBlock->otz),
                        quad);
                gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratchBlock->otz);
            } while (angle < GLOW_FULL_TURN);
            redIntensity          = (u8)redIntensity >> 1;
            greenIntensity        = (u8)greenIntensity >> 1;
            blueIntensity         = (u8)blueIntensity >> 1;
            scratchBlock->radius *= 2;
            discIndex++;
        } while (discIndex < MINE_REFUGE_GLOW_DISC_COUNT);
    }
}

void mineRefugeDrawGlowsTask(Task* task)
{
    enum { MINE_REFUGE_POWER_PANEL_ON  = 1,
           MINE_REFUGE_CYAN_PULSE_RATE = 0x60 };
    u8 viewIndex;

    viewIndex = viewGetMappedIndex();
    switch (viewIndex) {
        case 2:
            _glowDrawFlare(&_gMineRefugeGlowAnchors.flare, 1, 0x300);
            _mineRefugeDrawPulsingCyanStar(&_gMineRefugeGlowAnchors.cyan, MINE_REFUGE_CYAN_PULSE_RATE, 0x40);
            break;
        case 3:
            if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) == MINE_REFUGE_POWER_PANEL_ON) {
                _mineRefugeDrawLayeredGlow(&_gMineRefugePoweredPanelGlowPosition, 0x30, 0xF0);
            }
            break;
        case 4:
        case 5:
            if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) == MINE_REFUGE_POWER_PANEL_ON) {
                _mineRefugeDrawLayeredGlow(&_gMineRefugePoweredPanelGlowPosition, 0x60, 0xD0);
            }
            break;
        case 6:
            _mineRefugeDrawPulsingCyanBurst(&_gMineRefugeGlowAnchors.cyan, MINE_REFUGE_CYAN_PULSE_RATE, 0x80);
            break;
    }
}
