#include "rooms/shelter_b6_nursery.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_450800.h"
#include "actors/actor_450800.h"

#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/player_state.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/areas.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
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
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_cutscene.h"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

/// Cues the room's cutscenes leave for its view-effect task.
///
/// The task clears both when it starts, and again once it has fired the spark
/// shower, so the shower plays once per cue and takes the pulse cue with it.
typedef struct {
    u16 fastGlintPulse;   // non-zero: the glints of views 3, 6, 8 and 10 pulse at four times their idle rate
    u16 sparkShowerScale; // non-zero: view 13 throws sixteen spark-shower shards, their spread and size multiplied by this
} _ShelterB6NurseryEffectCues;
STATIC_ASSERT_SIZEOF(_ShelterB6NurseryEffectCues, 0x4);

/// Glint position and particle-placement inputs for the nursery's view effects.
///
/// XYZ are signed whole game units; the fourth halfwords are not consumed.
/// The glint is projected from world space. Spawn origins are transformed by
/// the current GsWSMATRIX, with XYZ copied before the next spawn changes them.
/// Only `sparkShowerOrigin` is rewritten. The room owns all seven vectors.
typedef struct {
    SVECTOR glint;                     // Glint centre for views 3, 6, 8, 10 and 17
    SVECTOR debrisOrigin;              // Three model chunks spawned on the first visit to view 12
    SVECTOR latchedParticleOrigins[2]; // Both emitters stay active after visiting view 13
    SVECTOR view17And18ParticleOrigin; // Particle emitter shared by views 17 and 18
    SVECTOR view15ParticleOrigin;      // Particle emitter active in view 15
    SVECTOR sparkShowerOrigin;         // Mutable origin rebuilt for each of the sixteen cued shards
} _ShelterB6NurseryEffectAnchors;
STATIC_ASSERT_SIZEOF(_ShelterB6NurseryEffectAnchors, 0x38);

/// Scratch-stack block one spark-shower shard is drawn from.
///
/// Each corner is staged in `corners` as a point of the shard's own plane and
/// replaced in place by its world position, narrowed to signed 16-bit
/// coordinate units. One RTPT then projects the three together; the screen
/// positions go straight into the packet, so the block keeps none of them.
///
/// Reserve the whole block and release it before the drawer returns.
typedef struct {
    SVECTOR corners[3];      // Local corner workspace, then the world positions supplied to the projection
    s32     otz;             // SZ3 / 4 of the projection, a quarter of the last corner's depth; ordering-table and blend depth
    s32     projectionFlags; // GTE FLAG word of the projection; bit 31 set drops the triangle
} _ShelterB6NurseryTriScratch;
STATIC_ASSERT_SIZEOF(_ShelterB6NurseryTriScratch, 0x20);

s32 rsin(s32);
s32 rcos(s32);

/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType` (ally present). A distinct symbol so the restore
/// path does not share the `gMcSaveData` address with case 0.

/// View saved when the cutscene starts and restored when it ends.

/// The cutscene's running sound task.
extern Task* gRoomCutsceneSoundTask;

/// Nursery ambience start/stop request latch (0 stop, 1 start requested).
///
/// Set before task allocation and retained on failure; it does not prove that
/// a task exists. The running task observes a clear request in its wait state.
s32 _gShelterB6NurseryAmbienceRequested;

/// Persistent view-6 scene configuration shared by nursery command-10 scenes.
///
/// The runner borrows `rec` until completion. Its sounds and scene selectors
/// are filled before spawning; the retained eight-byte unknown span is unused.
RoomCutsceneRecStorage _gShelterB6NurseryCutsceneRec;

/// Persistent coordinate for the nursery ambience source at room (6000, 0, -830).
///
/// Reparented to the current view and composed each tick for pan and depth
/// queries. Only translation is sampled; its zero-initialized basis is retained.
GfxCoord _gShelterB6NurseryAmbienceOrigin;

/// Pending glint-pulse and spark-shower cues from nursery scene scripts.
///
/// The setter retains each argument's low 16 bits. Initialization and a cued
/// view-13 shower clear both; leaving view 13 alone does not consume a cue.
_ShelterB6NurseryEffectCues _gShelterB6NurseryEffectCues;

#include "../../shared/room/telephone.h"
#include "../../shared/effects/sprite_quad.h"

static void _shelterB6NurseryInitializeRoomTask(Task* task);
static void _shelterB6NurseryMessageIdle(Task* task);
static void _shelterB6NurseryDrawParticleFrame(const GfxCoord* coord, u16 animationFrame, s16 halfDiagonal, s16 rotation);
static void _shelterB6NurseryDrawSparkShowerShard(const GfxCoord* coord, s16 radius, s16 shade);

static void _shelterB6NurseryAmbienceTask(Task* task);

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

/// Single spawn recipe for nursery ambience, with no body or resource payload.
///
/// Only index zero is valid; this is not a terminated descriptor list. Spawning
/// copies the recipe. The nursery callback must remain loaded until teardown.
static TaskDesc _gShelterB6NurseryAmbienceTaskDesc = {
    { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB6NurseryAmbienceTask, { .value = 0 }
};

/// Values shared by nursery entry and command-driven story progression.
enum {
    SHELTER_B6_NURSERY_PROGRESS_NOT_ENTERED = 0,
    SHELTER_B6_NURSERY_PROGRESS_ENTERED     = 1,
    SHELTER_B6_NURSERY_PROGRESS_ADVANCED    = 2
};

static s32 _shelterB6NurseryHandleRoomCommand(Task* unusedTask, s32 messageId, s32 command, s32 unusedSecondArg);
static s32 _shelterB6NurseryRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32 _shelterB6NurseryResolveRoomTransition(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB6NurseryHandleRoomAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Nursery room messages: departures, refused key items, actions and commands.
///
/// The four callbacks receive two room-event pointers, an item ID and zero,
/// an action-request pointer and zero, or a command and zero, respectively.
/// They all return zero. The registered room task borrows this terminated
/// table and its callbacks for the loaded nursery overlay's lifetime.
static TaskMessageEntry _gShelterB6NurseryRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB6NurseryResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB6NurseryRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB6NurseryHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB6NurseryHandleRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Nursery ambience choices shared by the request latch and depth table.
enum {
    SHELTER_B6_NURSERY_AMBIENCE_STOP_REQUEST   = 0,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH = -1
};

/// Signed ambience depth overrides indexed directly by the logical view ID.
///
/// Active views are 1..19: views 2/4 use 25, view 3 uses 0 and view 5 uses
/// 64. `SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH` keeps spatial attenuation.
/// Values are signed sound-event depth units; computed depth advances one
/// unit per 256 game-coordinate units from the camera's projection plane.
/// Preserve all 24 serialized entries, including index zero and slots 20..23.
static s8 _gShelterB6NurseryAmbienceDepthOverrides[24] = {
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    25,
    0,
    25,
    64,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH,
    0,
    0,
    0,
};

/// Persistent glint, debris and particle origins, with mutable shower workspace.
static _ShelterB6NurseryEffectAnchors _gShelterB6NurseryEffectAnchors = {
    .glint                     = { 6170, -80, -860, 0 },
    .debrisOrigin              = { 3000, -1000, 0, 0 },
    .latchedParticleOrigins    = { { 5500, 0, 2500, 0 }, { 6000, 0, 0, 0 } },
    .view17And18ParticleOrigin = { 4000, 0, -1000, 0 },
    .view15ParticleOrigin      = { 2000, 0, 0, 0 },
    .sparkShowerOrigin         = { 7000, -1500, 2000, 0 },
};

static TmdBone _gShelterB6NurseryModel07BACSkeleton[1] = {
#include "assets/shelter_b6_nursery_model_07BAC_skeleton.inc"
};

static u32 _gShelterB6NurseryModel07BACPartVerts[1] = {
#include "assets/shelter_b6_nursery_model_07BAC_partVerts.inc"
};

static SVECTOR _gShelterB6NurseryModel07BACVerts[12] = {
#include "assets/shelter_b6_nursery_model_07BAC_verts.inc"
};

static SVECTOR _gShelterB6NurseryModel07BACNormals[12] = {
#include "assets/shelter_b6_nursery_model_07BAC_normals.inc"
};

static u32 _gShelterB6NurseryModel07BACStream[89] = {
#include "assets/shelter_b6_nursery_model_07BAC_stream.inc"
};

TmdSource gShelterB6NurseryModel07BAC = {
    0,
    576,
    0,
    1,
    _gShelterB6NurseryModel07BACPartVerts,
    _gShelterB6NurseryModel07BACVerts,
    _gShelterB6NurseryModel07BACNormals,
    _gShelterB6NurseryModel07BACSkeleton,
    _gShelterB6NurseryModel07BACStream,
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

u8* gShelterB6NurseryViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB6NurseryViewCounts[1] = { ARRAY_SIZE(gShelterB6NurseryViewCameras) };

DirectionWarpEntry gShelterB6NurseryWarpEntries[2] = {
    {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 680, 0, 0 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 680, 0, 0 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 19700, 0, 3600 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 19700, 0, 3600 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 6,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Q12 room-space normals referenced by the nursery collision mesh.
static SVECTOR _gShelterB6NurseryCollisionGridNormals[14] = {
#include "assets/shelter_b6_nursery_collision_082E0_normals.inc"
};

/// Room-space vertices in signed game-coordinate units for the nursery mesh.
static SVECTOR _gShelterB6NurseryCollisionGridVerts[54] = {
#include "assets/shelter_b6_nursery_collision_082E0_verts.inc"
};

/// Nursery collision triangles/quads, with room-local surface classes 0 and 1.
static WorldCollisionGridFace _gShelterB6NurseryCollisionGridFaces[43] = {
#include "assets/shelter_b6_nursery_collision_082E0_faces.inc"
};

/// Four terminated face-index lists and the final retained serialized halfword.
static s16 _gShelterB6NurseryCollisionGridCells[120] = {
#include "assets/shelter_b6_nursery_collision_082E0_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterB6NurseryCollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..119
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_B6_NURSERY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB6NurseryCollisionGridCells[(halfwordOffset)])
/// X-major pointers into the nursery grid's four face-index lists.
static s16* _gShelterB6NurseryCollisionGridTable[4] = {
#include "assets/shelter_b6_nursery_collision_082E0_table.inc"
};
#undef SHELTER_B6_NURSERY_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB6NurseryCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB6NurseryCollisionGridNormals,
    .vertices    = _gShelterB6NurseryCollisionGridVerts,
    .faces       = _gShelterB6NurseryCollisionGridFaces,
    .cellFaceIds = _gShelterB6NurseryCollisionGridTable,
    .xBias       = -50,
    .zBias       = 1450,
    .cellCountX  = 2,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB6NurseryCollisionGridFaces),
};

ViewCamera gShelterB6NurseryViewCameras[19] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -3500, 0x61A8, -2500 } }, 447 },
    { { { { 540, 0, 4060 }, { -792, 4017, 105 }, { -3982, -799, 530 } }, { -6040, 500, 440 } }, 230 },
    { { { { 668, 0, -4041 }, { -249, 4088, -41 }, { 4033, 252, 667 } }, { -940, 1450, 570 } }, 257 },
    { { { { 3940, 0, 1118 }, { 170, 4048, -601 }, { -1105, 624, 3894 } }, { -6140, 1650, 90 } }, 230 },
    { { { { 4042, 0, 659 }, { 298, 3654, -1825 }, { -588, 1850, 3606 } }, { -6090, 2850, -2510 } }, 230 },
    { { { { -364, 0, -4079 }, { -2502, 3234, 223 }, { 3222, 2512, -287 } }, { -5469, 711, 782 } }, 680 },
    { { { { 3615, 0, -1925 }, { 0, ONE, 0 }, { 1925, 0, 3615 } }, { -4740, 870, -450 } }, 329 },
    { { { { -3171, 0, -2592 }, { -979, 3792, 1197 }, { 2400, 1547, -2936 } }, { -5280, 1300, -2310 } }, 329 },
    { { { { 3044, 0, -2740 }, { -2083, 2660, -2314 }, { 1779, 3114, 1977 } }, { -6030, 1220, -1570 } }, 447 },
    { { { { 4020, 0, -781 }, { 149, 4019, 771 }, { 766, -785, 3946 } }, { -6050, 100, 1490 } }, 329 },
    { { { { 1075, 0, 3952 }, { -297, 4084, 80 }, { -3941, -308, 1072 } }, { -7400, 780, 60 } }, 289 },
    { { { { 3641, 0, 1875 }, { -8, 4095, 16 }, { -1875, -18, 3641 } }, { -5890, 1100, 1170 } }, 289 },
    { { { { 4074, 0, 422 }, { -44, 4072, 433 }, { -419, -435, 4051 } }, { -6360, 760, 1430 } }, 257 },
    { { { { 4073, 0, -430 }, { -20, 4091, -198 }, { 429, 199, 4068 } }, { -5560, 1280, 1550 } }, 230 },
    { { { { -182, 0, 4091 }, { 676, 4039, 30 }, { -4035, 677, -179 } }, { -3100, 1350, -80 } }, 257 },
    { { { { 3990, 0, 924 }, { 25, 4094, -109 }, { -923, 112, 3988 } }, { -6560, 1210, 780 } }, 257 },
    { { { { -3964, 0, 1029 }, { 55, 4090, 211 }, { -1027, 219, -3958 } }, { -5890, 1130, -2310 } }, 257 },
    { { { { -3641, 0, 1875 }, { 0, ONE, 0 }, { -1875, 0, -3641 } }, { -5860, 1460, -980 } }, 329 },
    { { { { 2356, 0, 3349 }, { -350, 4073, 246 }, { -3331, -428, 2344 } }, { -6700, 780, 110 } }, 289 },
};

/// Empty view-1 sprite list; the zero first count keeps decoded background strips.
static SpriteBatch _gShelterB6NurseryView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-2 sprite list; the zero first count keeps decoded background strips.
static SpriteBatch _gShelterB6NurseryView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture and presentation values shared by the nursery's source sprites.
enum {
    SHELTER_B6_NURSERY_SPRITE_TPAGE         = getTPage(1, 0, 960, 0),
    SHELTER_B6_NURSERY_SPRITE_CLUT          = getClut(0, 255),
    SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR = 128,
};

/// View-3 textured rectangles in screen pixels with unscaled sorting depths.
static SpriteSource _gShelterB6NurseryView3SpriteSources[24] = {
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, -136, -120, 760, { .fields = { 104, 184 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 40 } }, -136, -88, 758, { .fields = { 104, 40 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, -136, -48, 750, { .fields = { 104, 120 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 40 } }, -128, -16, 775, { .fields = { 112, 120 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 40 } }, -136, -16, 762, { .fields = { 112, 80 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 40 } }, -128, 24, 775, { .fields = { 112, 40 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 40 } }, -136, 24, 757, { .fields = { 104, 80 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 40 } }, -128, 64, 787, { .fields = { 120, 80 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, -136, 64, 758, { .fields = { 96, 64 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, -136, 96, 759, { .fields = { 120, 240 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, -144, -120, 717, { .fields = { 96, 32 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, -88, 721, { .fields = { 120, 40 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, -144, -48, 720, { .fields = { 96, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, -16, 721, { .fields = { 120, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, 24, 723, { .fields = { 120, 120 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, -144, 64, 724, { .fields = { 96, 184 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, -144, 96, 726, { .fields = { 88, 32 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 32 } }, -160, -120, 673, { .fields = { 96, 152 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, -88, 693, { .fields = { 104, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 32 } }, -160, -48, 676, { .fields = { 96, 216 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, -16, 677, { .fields = { 112, 200 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, 24, 679, { .fields = { 112, 160 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 32 } }, -160, 64, 680, { .fields = { 88, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, -160, 96, 682, { .fields = { 88, 128 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View-3 sprite ranges: keep background strips, draw every source, then end.
static SpriteBatch _gShelterB6NurseryView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB6NurseryView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-4 textured rectangles partitioned between two drawing batches.
static SpriteSource _gShelterB6NurseryView4SpriteSources[44] = {
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 40, 16, 836, { .fields = { 88, 144 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 56, 16, 787, { .fields = { 88, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, 16, 806, { .fields = { 88, 48 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, 16, 793, { .fields = { 88, 64 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 8 } }, 64, 8, 806, { .fields = { 80, 184 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, 16, 777, { .fields = { 88, 80 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 8 } }, 32, 40, 800, { .fields = { 88, 16 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 8 } }, 32, 48, 688, { .fields = { 80, 160 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 8 } }, 32, 56, 656, { .fields = { 80, 192 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 8 } }, 24, 64, 637, { .fields = { 80, 40 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 8 } }, 40, 64, 637, { .fields = { 80, 32 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 8 } }, 24, 72, 547, { .fields = { 80, 224 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 8 } }, 40, 72, 554, { .fields = { 80, 240 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 8 } }, 24, 80, 535, { .fields = { 80, 232 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 8 } }, 40, 80, 510, { .fields = { 88, 216 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 8 } }, 16, 88, 476, { .fields = { 88, 208 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 8 } }, 32, 88, 428, { .fields = { 72, 24 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 8 } }, 16, 96, 434, { .fields = { 72, 8 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 16 } }, 16, 104, 438, { .fields = { 80, 128 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 8 } }, 40, 32, 800, { .fields = { 80, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 16 } }, 56, 32, 800, { .fields = { 104, 176 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 16 } }, 56, 48, 675, { .fields = { 104, 160 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 16 } }, 56, 64, 514, { .fields = { 104, 208 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 16 } }, 56, 80, 493, { .fields = { 104, 192 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 8 } }, 40, 96, 424, { .fields = { 88, 200 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 40, 104, 427, { .fields = { 112, 112 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 56, 96, 415, { .fields = { 104, 48 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 16 } }, 80, 32, 803, { .fields = { 104, 144 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 16 } }, 80, 48, 664, { .fields = { 104, 128 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 16 } }, 80, 64, 491, { .fields = { 88, 112 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 16 } }, 80, 80, 492, { .fields = { 96, 240 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 80, 96, 403, { .fields = { 104, 72 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, 32, 785, { .fields = { 96, 224 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 120, 24, 773, { .fields = { 120, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 32, 24 } }, 104, 48, 642, { .fields = { 96, 24 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, -24, 849, { .fields = { 112, 16 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 80, -48, 917, { .fields = { 104, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 96, -48, 913, { .fields = { 104, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 8 } }, 80, -32, 915, { .fields = { 88, 176 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 8 } }, 96, -32, 846, { .fields = { 88, 168 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, -24, 884, { .fields = { 112, 224 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, -8, 897, { .fields = { 120, 240 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, 8, 792, { .fields = { 96, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, 24, 799, { .fields = { 120, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Number of source sprites selected by view 4's first nonempty batch.
enum { SHELTER_B6_NURSERY_VIEW4_BATCH1_SPRITE_COUNT = 35 };

/// View-4 ranges: background strips, 35 sprites, the remaining nine, then end.
///
/// The first nonempty range retains an uninterpreted 1 in its serialized bytes.
static SpriteBatch _gShelterB6NurseryView4SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, SHELTER_B6_NURSERY_VIEW4_BATCH1_SPRITE_COUNT, 0, 0, { 1, 0 } },
    { SHELTER_B6_NURSERY_VIEW4_BATCH1_SPRITE_COUNT, ARRAY_SIZE(_gShelterB6NurseryView4SpriteSources) - SHELTER_B6_NURSERY_VIEW4_BATCH1_SPRITE_COUNT, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-5 textured rectangles in screen pixels with unscaled sorting depths.
static SpriteSource _gShelterB6NurseryView5SpriteSources[17] = {
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 8 } }, 72, 48, 381, { .fields = { 112, 24 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 8 } }, 144, 88, 326, { .fields = { 120, 248 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 112, 475, { .fields = { 120, 240 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 80, 48, 396, { .fields = { 112, 160 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 80, 64, 399, { .fields = { 112, 144 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 72, 72, 432, { .fields = { 120, 128 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 80, 80, 404, { .fields = { 112, 176 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 96, 56, 374, { .fields = { 112, 224 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 96, 72, 377, { .fields = { 112, 208 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 96, 88, 392, { .fields = { 112, 192 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 96, 104, 400, { .fields = { 112, 64 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, 96, 476, { .fields = { 120, 48 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 112, 64, 354, { .fields = { 112, 80 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 112, 80, 360, { .fields = { 112, 112 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 112, 96, 363, { .fields = { 112, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 128, 80, 332, { .fields = { 112, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 128, 96, 346, { .fields = { 120, 24 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View-5 sprite ranges: keep background strips, draw every source, then end.
static SpriteBatch _gShelterB6NurseryView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB6NurseryView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-6 sprite list; the zero first count keeps decoded background strips.
static SpriteBatch _gShelterB6NurseryView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-7 sprite list; the zero first count keeps decoded background strips.
static SpriteBatch _gShelterB6NurseryView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-8 sprite list; the zero first count keeps decoded background strips.
static SpriteBatch _gShelterB6NurseryView8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-9 sprite list; the zero first count keeps decoded background strips.
static SpriteBatch _gShelterB6NurseryView9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-10 textured rectangles partitioned between two drawing batches.
static SpriteSource _gShelterB6NurseryView10SpriteSources[38] = {
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 32, 129, { .fields = { 80, 152 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 8 } }, 8, 56, 154, { .fields = { 80, 120 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 80, 142, { .fields = { 72, 120 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 56, 24, 135, { .fields = { 56, 64 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, 24, 132, { .fields = { 56, 80 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 48, 40, 150, { .fields = { 104, 232 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 72, 40, 122, { .fields = { 56, 176 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 96, 40, 150, { .fields = { 56, 200 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 32, 48, 151, { .fields = { 56, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 16, 48, 153, { .fields = { 56, 16 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, -16, 64, 150, { .fields = { 72, 128 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 8, 64, 150, { .fields = { 72, 48 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 32, 64, 150, { .fields = { 56, 224 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 56, 64, 150, { .fields = { 56, 152 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 80, 64, 121, { .fields = { 64, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 104, 64, 139, { .fields = { 72, 72 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 32, 32 } }, -24, 88, 150, { .fields = { 96, 32 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 32, 32 } }, 8, 88, 150, { .fields = { 96, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 32, 32 } }, 40, 88, 137, { .fields = { 96, 64 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 32, 32 } }, 72, 88, 122, { .fields = { 96, 128 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 40, 32 } }, 104, 88, 128, { .fields = { 88, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 8 } }, -88, 0, 739, { .fields = { 88, 152 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, -80, -8, 745, { .fields = { 56, 120 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, -64, -8, 752, { .fields = { 40, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, -48, -8, 759, { .fields = { 48, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, -32, -8, 766, { .fields = { 56, 136 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, -96, 8, 732, { .fields = { 104, 184 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, -72, 8, 742, { .fields = { 72, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, -48, 8, 752, { .fields = { 72, 24 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, -24, 8, 763, { .fields = { 80, 232 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, -16, -8, 774, { .fields = { 56, 32 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, -96, 32, 722, { .fields = { 80, 184 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, -96, 56, 713, { .fields = { 80, 208 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, -72, 32, 732, { .fields = { 104, 208 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, -48, 32, 742, { .fields = { 80, 160 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, -24, 32, 752, { .fields = { 104, 160 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 16 } }, -72, 56, 700, { .fields = { 48, 48 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 8 } }, -48, 56, 1163, { .fields = { 56, 248 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Number of source sprites selected by view 10's first nonempty batch.
enum { SHELTER_B6_NURSERY_VIEW10_BATCH1_SPRITE_COUNT = 21 };

/// View-10 ranges: background strips, 21 sprites, the remaining 17, then end.
///
/// The first nonempty range retains an uninterpreted 1 in its serialized bytes.
static SpriteBatch _gShelterB6NurseryView10SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, SHELTER_B6_NURSERY_VIEW10_BATCH1_SPRITE_COUNT, 0, 0, { 1, 0 } },
    { SHELTER_B6_NURSERY_VIEW10_BATCH1_SPRITE_COUNT, ARRAY_SIZE(_gShelterB6NurseryView10SpriteSources) - SHELTER_B6_NURSERY_VIEW10_BATCH1_SPRITE_COUNT, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-11 sprite list; the zero first count keeps decoded background strips.
static SpriteBatch _gShelterB6NurseryView11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-12 sprite list; the zero first count keeps decoded background strips.
static SpriteBatch _gShelterB6NurseryView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-13 textured rectangles in screen pixels with unscaled sorting depths.
static SpriteSource _gShelterB6NurseryView13SpriteSources[12] = {
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, -16, 511, { .fields = { 120, 216 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, -120, 536, { .fields = { 112, 120 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 80, -96, 681, { .fields = { 104, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 80, -72, 861, { .fields = { 104, 144 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 80, -48, 1000, { .fields = { 104, 72 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 80, -24, 1000, { .fields = { 104, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 72, -80, 766, { .fields = { 120, 184 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 64, -64, 905, { .fields = { 112, 200 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 56, -48, 1123, { .fields = { 104, 48 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 56, -24, 1130, { .fields = { 104, 24 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 16 } }, 56, 0, 977, { .fields = { 104, 168 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 8 } }, 80, 0, 696, { .fields = { 112, 224 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View-13 sprite ranges; the source range can be hidden by room scripts.
///
/// The zero-count first record keeps background strips; the next covers every
/// source, followed by `SPRITE_BATCH_END`. Hidden state is mutable at runtime.
static SpriteBatch _gShelterB6NurseryView13SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB6NurseryView13SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-14 sprite list; its zero first count keeps decoded background strips.
static SpriteBatch _gShelterB6NurseryView14SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Alternate texture page used by view 15's rectangles in the adjacent VRAM page.
enum { SHELTER_B6_NURSERY_VIEW15_ALTERNATE_SPRITE_TPAGE = getTPage(1, 0, 896, 0) };

/// View-15 textured rectangles in screen pixels with unscaled sorting depths.
///
/// Sources span two adjacent texture pages and share the room's CLUT. The
/// drawing batch covers all 51 elements; cached packets snapshot their geometry.
static SpriteSource _gShelterB6NurseryView15SpriteSources[51] = {
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -120, 664, { .fields = { 80, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -72, 667, { .fields = { 80, 48 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -24, 668, { .fields = { 80, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, 24, 670, { .fields = { 80, 144 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 48, 48 } }, -112, -120, 739, { .fields = { 80, 192 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 48, 48 } }, -112, -72, 762, { .fields = { 32, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 48, 48 } }, -112, -24, 785, { .fields = { 32, 48 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 48, 48 } }, -112, 24, 809, { .fields = { 32, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 48 } }, -64, -120, 737, { .fields = { 64, 144 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 48 } }, -64, -72, 758, { .fields = { 64, 192 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 48 } }, -64, -24, 784, { .fields = { 48, 144 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 48 } }, -64, 24, 809, { .fields = { 48, 192 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, -120, 731, { .fields = { 32, 144 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, -96, 740, { .fields = { 32, 168 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, -72, 750, { .fields = { 32, 192 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, -48, 762, { .fields = { 32, 216 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, -24, 773, { .fields = { 16, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, 0, 786, { .fields = { 16, 24 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, 24, 798, { .fields = { 16, 48 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, 48, 812, { .fields = { 16, 72 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_VIEW15_ALTERNATE_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, -32, -16, 787, { .fields = { 112, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, -32, 0, 789, { .fields = { 24, 120 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, -32, 24, 812, { .fields = { 24, 144 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, -32, 48, 804, { .fields = { 24, 168 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_VIEW15_ALTERNATE_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, -32, -120, 724, { .fields = { 112, 176 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_VIEW15_ALTERNATE_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, -32, -104, 726, { .fields = { 112, 160 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_VIEW15_ALTERNATE_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, -16, -120, 722, { .fields = { 112, 144 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_VIEW15_ALTERNATE_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, -16, -104, 724, { .fields = { 120, 240 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 0, -120, 720, { .fields = { 8, 240 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 0, -104, 723, { .fields = { 24, 240 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 16, -120, 718, { .fields = { 40, 240 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 16, -104, 721, { .fields = { 56, 240 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, -24, 56, 810, { .fields = { 72, 240 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, -8, 56, 808, { .fields = { 88, 240 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 16 } }, 8, 56, 805, { .fields = { 104, 240 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_VIEW15_ALTERNATE_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 32, -120, 718, { .fields = { 120, 216 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_VIEW15_ALTERNATE_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 32, -96, 741, { .fields = { 112, 48 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_VIEW15_ALTERNATE_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 32, -72, 752, { .fields = { 112, 72 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_VIEW15_ALTERNATE_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 32, -48, 763, { .fields = { 112, 120 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_VIEW15_ALTERNATE_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 32, -24, 775, { .fields = { 120, 192 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_VIEW15_ALTERNATE_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 32, 0, 787, { .fields = { 120, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 32, 24, 800, { .fields = { 0, 72 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 32, 48, 794, { .fields = { 0, 48 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_VIEW15_ALTERNATE_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 48, -120, 717, { .fields = { 120, 24 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_VIEW15_ALTERNATE_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 48, -96, 729, { .fields = { 120, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 48, -72, 737, { .fields = { 0, 168 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 48, -48, 750, { .fields = { 0, 144 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 48, -24, 761, { .fields = { 0, 120 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 48, 0, 773, { .fields = { 8, 216 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 48, 24, 784, { .fields = { 8, 192 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 24, 24 } }, 48, 48, 799, { .fields = { 8, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View-15 drawing list: background strips, every source sprite, then end.
static SpriteBatch _gShelterB6NurseryView15SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB6NurseryView15SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-16 textured rectangles in screen pixels with unscaled sorting depths.
///
/// All 56 elements use the room's primary texture page and CLUT. The drawing
/// batch selects the full array; cached packets snapshot their geometry.
static SpriteSource _gShelterB6NurseryView16SpriteSources[56] = {
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 136, -88, 318, { .fields = { 88, 232 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 136, -72, 319, { .fields = { 80, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 136, -56, 320, { .fields = { 80, 16 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 136, -40, 319, { .fields = { 88, 216 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 136, -24, 320, { .fields = { 88, 144 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 128, 0, 490, { .fields = { 88, 160 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 120, 0, 525, { .fields = { 88, 176 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 136, -8, 320, { .fields = { 104, 168 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 144, -96, 307, { .fields = { 104, 192 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 144, -72, 308, { .fields = { 96, 48 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 144, -48, 309, { .fields = { 96, 24 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 144, -24, 310, { .fields = { 96, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 144, 0, 318, { .fields = { 96, 72 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, 152, -120, 269, { .fields = { 120, 64 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, 152, -88, 268, { .fields = { 120, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, 152, -56, 268, { .fields = { 120, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, 152, -24, 271, { .fields = { 120, 32 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, 152, 8, 269, { .fields = { 120, 128 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, 152, 40, 268, { .fields = { 120, 224 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 152, 72, 268, { .fields = { 80, 152 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, 144, 24, 299, { .fields = { 112, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 144, 56, 295, { .fields = { 96, 144 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 136, 16, 360, { .fields = { 96, 168 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, 136, 40, 327, { .fields = { 120, 160 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 128, 16, 400, { .fields = { 88, 24 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 128, 40, 358, { .fields = { 112, 152 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 120, 16, 475, { .fields = { 112, 128 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 120, 40, 475, { .fields = { 80, 168 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 144, -120, 307, { .fields = { 112, 176 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 32 } }, 136, -120, 479, { .fields = { 120, 192 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 128, -120, 436, { .fields = { 80, 184 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 128, -104, 517, { .fields = { 80, 136 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 120, -120, 454, { .fields = { 112, 104 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 120, -96, 573, { .fields = { 80, 32 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 112, -120, 462, { .fields = { 104, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 112, -96, 606, { .fields = { 104, 72 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 112, -72, 645, { .fields = { 104, 144 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 112, -48, 658, { .fields = { 104, 120 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 112, -24, 651, { .fields = { 104, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 112, 0, 500, { .fields = { 80, 72 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 16 } }, 112, 16, 475, { .fields = { 80, 120 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 112, 32, 475, { .fields = { 104, 24 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 104, -120, 543, { .fields = { 104, 48 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 104, -96, 597, { .fields = { 112, 224 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 104, -72, 725, { .fields = { 112, 80 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 104, -48, 722, { .fields = { 112, 32 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 104, -24, 720, { .fields = { 112, 56 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 104, 0, 550, { .fields = { 112, 200 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 8, 24 } }, 104, 24, 539, { .fields = { 88, 72 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, -120, 725, { .fields = { 88, 0 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, -96, 729, { .fields = { 96, 216 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, -72, 873, { .fields = { 88, 120 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, -48, 962, { .fields = { 80, 48 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, -24, 957, { .fields = { 80, 96 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, 0, 720, { .fields = { 88, 192 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_NURSERY_SPRITE_TPAGE, SHELTER_B6_NURSERY_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, 24, 684, { .fields = { 96, 240 } }, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, SHELTER_B6_NURSERY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View-16 drawing list: background strips, every source sprite, then end.
static SpriteBatch _gShelterB6NurseryView16SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB6NurseryView16SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-17 sprite list; its zero first count keeps decoded background strips.
static SpriteBatch _gShelterB6NurseryView17SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-18 sprite list; its zero first count keeps decoded background strips.
static SpriteBatch _gShelterB6NurseryView18SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-19 sprite list; its zero first count keeps decoded background strips.
static SpriteBatch _gShelterB6NurseryView19SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB6NurserySpriteViews[19] = {
    { { .empty = _gShelterB6NurseryView1SpriteBatches }, _gShelterB6NurseryView1SpriteBatches, NULL },
    { { .empty = _gShelterB6NurseryView2SpriteBatches }, _gShelterB6NurseryView2SpriteBatches, NULL },
    { { .elements = _gShelterB6NurseryView3SpriteSources }, _gShelterB6NurseryView3SpriteBatches, NULL },
    { { .elements = _gShelterB6NurseryView4SpriteSources }, _gShelterB6NurseryView4SpriteBatches, NULL },
    { { .elements = _gShelterB6NurseryView5SpriteSources }, _gShelterB6NurseryView5SpriteBatches, NULL },
    { { .empty = _gShelterB6NurseryView6SpriteBatches }, _gShelterB6NurseryView6SpriteBatches, NULL },
    { { .empty = _gShelterB6NurseryView7SpriteBatches }, _gShelterB6NurseryView7SpriteBatches, NULL },
    { { .empty = _gShelterB6NurseryView8SpriteBatches }, _gShelterB6NurseryView8SpriteBatches, NULL },
    { { .empty = _gShelterB6NurseryView9SpriteBatches }, _gShelterB6NurseryView9SpriteBatches, NULL },
    { { .elements = _gShelterB6NurseryView10SpriteSources }, _gShelterB6NurseryView10SpriteBatches, NULL },
    { { .empty = _gShelterB6NurseryView11SpriteBatches }, _gShelterB6NurseryView11SpriteBatches, NULL },
    { { .empty = _gShelterB6NurseryView12SpriteBatches }, _gShelterB6NurseryView12SpriteBatches, NULL },
    { { .elements = _gShelterB6NurseryView13SpriteSources }, _gShelterB6NurseryView13SpriteBatches, NULL },
    { { .empty = _gShelterB6NurseryView14SpriteBatches }, _gShelterB6NurseryView14SpriteBatches, NULL },
    { { .elements = _gShelterB6NurseryView15SpriteSources }, _gShelterB6NurseryView15SpriteBatches, NULL },
    { { .elements = _gShelterB6NurseryView16SpriteSources }, _gShelterB6NurseryView16SpriteBatches, NULL },
    { { .empty = _gShelterB6NurseryView17SpriteBatches }, _gShelterB6NurseryView17SpriteBatches, NULL },
    { { .empty = _gShelterB6NurseryView18SpriteBatches }, _gShelterB6NurseryView18SpriteBatches, NULL },
    { { .empty = _gShelterB6NurseryView19SpriteBatches }, _gShelterB6NurseryView19SpriteBatches, NULL },
};

/// Five warm point lights with 2000-unit full-strength and 2750-unit outer radii.
///
/// Positions use room-coordinate units and RGB uses Q12 intensity. Every light
/// applies in every view; transform caches, parents and attenuation are mutable.
static WorldCoordPointLight _gShelterB6NurseryPointLights[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5491, -2000, 5000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3031, 2949 }, { 0, 0 } }, 2000, 2750 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5491, -2000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3031, 2949 }, { 0, 0 } }, 2000, 2750 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5491, -2000, 2500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3031, 2949 }, { 0, 0 } }, 2000, 2750 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3491, -2000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3031, 2949 }, { 0, 0 } }, 2000, 2750 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1491, -2000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3031, 2949 }, { 0, 0 } }, 2000, 2750 },
};

WorldCoordRoomLights gShelterB6NurseryRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB6NurseryPointLights),
    .pointLights           = _gShelterB6NurseryPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB6NurseryViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { 5616, -1504, 4352, 0 }, { { 2032, -1904, 0, 0 }, { -2032, -1904, 0, 0 }, { 2032, 1904, 0, 0 }, { -2032, 1904, 0, 0 } }, { 0, 0, 4102, 0 }, { 0, 0, ONE, 0 }, 2780, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5488, -1505, 4416, 0 }, { { -2000, -1904, 0, 0 }, { 2000, -1904, 0, 0 }, { -2000, 1904, 0, 0 }, { 2000, 1904, 0, 0 } }, { 0, 0, -4095, 0 }, { 0, 0, ONE, 0 }, 2757, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3936, -1441, 96, 0 }, { { 0, -1904, 2144, 0 }, { 0, -1904, -2144, 0 }, { 0, 1904, 2144, 0 }, { 0, 1904, -2144, 0 } }, { -4100, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2862, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3808, -1601, 128, 0 }, { { 0, -1904, -2256, 0 }, { 0, -1904, 2256, 0 }, { 0, 1904, -2256, 0 }, { 0, 1904, 2256, 0 } }, { 4098, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2941, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6193, -1440, 2032, 0 }, { { 2959, -1904, 437, 0 }, { -2961, -1904, -440, 0 }, { 2959, 1904, 437, 0 }, { -2961, 1904, -440, 0 } }, { -601, 0, 4052, 0 }, { 0, 0, ONE, 0 }, 3537, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6017, -1504, 2081, 0 }, { { -2694, -1904, -404, 0 }, { 2686, -1904, 395, 0 }, { -2694, 1904, -404, 0 }, { 2686, 1904, 395, 0 } }, { 602, 0, -4063, 0 }, { 0, 0, ONE, 0 }, 3318, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Nursery variant-1 recipes for Kyle, Pawn GOLEMs and the second scene body.
///
/// Keys 101 and 140 select descriptors 0 and 3 from the nursery actor table;
/// key 20 selects the Pawn GOLEM body recipe. Files are 450800, 535800 and
/// 650800, respectively. The base-60 load requires key 140's base placement.
/// `AREA_PLACEMENT_END` ends the list. Keep room recipes and map placements
/// loaded through loading, spawning and model-policy updates; actor overlays
/// must remain loaded while their tasks run. Buffer-policy restoration uses
/// descriptor 0 even for key 140, whose spawned descriptor 3 suppresses
/// automatic buffers; the reason for this distinction is unproven.
static AreaResource _gShelterB6NurseryVariant1Resources[4] = {
    { .entryId = 101, .fileNumber = 508, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = ACTOR_450800_NURSERY_ACTOR_TASK_KYLE, .taskTable = gActor450800NurseryActorTasks },
    { .entryId = 20, .fileNumber = 358, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_50, .taskIndex = PAIR_WALK_TASK_BODY, .taskTable = gActor450800PawnGolemTasks },
    { .entryId = 140, .fileNumber = 508, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_60, .taskIndex = ACTOR_450800_NURSERY_ACTOR_TASK_SECOND_BODY, .taskTable = gActor450800NurseryActorTasks },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gShelterB6NurseryAreaVariants[SHELTER_B6_NURSERY_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapNeoArkNurseryDefaultPlacements, .resources = _gShelterB6NurseryVariant1Resources },
};

/// Room action IDs and the scene command shared by triggers and message handlers.
enum {
    SHELTER_B6_NURSERY_ACTION_INTERACT           = 1,
    SHELTER_B6_NURSERY_ACTION_COMPANION_DIALOGUE = 2,
    SHELTER_B6_NURSERY_ACTION_LATER_SCENE        = 3,
    SHELTER_B6_NURSERY_COMMAND_SCENE             = 10
};

/// Departure slot 1 and training-room arrival slot 2 in the packed warp byte.
enum { SHELTER_B6_NURSERY_TRAINING_ROOM_WARP_KEY = 0x12 };

WorldCollisionTrigger gShelterB6NurseryActionTriggers[12] = {
    { NULL, NULL, NULL, { 5968, -64, 2416, 0 }, { { -1232, 0, -720, 0 }, { 368, 0, -720, 0 }, { -1232, 0, 688, 0 }, { -240, 0, 1040, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, ONE, 0 }, 1425, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B6_NURSERY_ACTION_INTERACT, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6816, -64, 320, 0 }, { { -1024, 0, -864, 0 }, { 288, 0, -864, 0 }, { -1024, 0, 896, 0 }, { 288, 0, 896, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, ONE, 0 }, 1360, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B6_NURSERY_ACTION_COMPANION_DIALOGUE, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6597, -64, -1143, 0 }, { { -1382, 0, -1023, 0 }, { 1103, 0, 1080, 0 }, { -1359, 0, 584, 0 }, { -730, 0, 1151, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, ONE, 0 }, 1717, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B6_NURSERY_COMMAND_SCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 512, -48, 16, 0 }, { { -352, 0, -880, 0 }, { 352, 0, -880, 0 }, { -352, 0, 880, 0 }, { 352, 0, 880, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 947, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B6_TRAINING_ROOM, SHELTER_B6_NURSERY_TRAINING_ROOM_WARP_KEY, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5392, -64, 1488, 0 }, { { -1872, 0, -528, 0 }, { 1872, 0, -16, 0 }, { -1872, 0, 16, 0 }, { 1872, 0, 528, 0 } }, { 0, ONE, 0, 0 }, { 0, 0, ONE, 0 }, 1941, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, SHELTER_B6_NURSERY_ACTION_LATER_SCENE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6287, -64, 4127, 0 }, { { -64, 0, -1188, 0 }, { 1154, 0, 296, 0 }, { -1153, 0, -295, 0 }, { 65, 0, 1189, 0 } }, { 0, 4101, 0, 0 }, { -3513, 0, 2106, 0 }, 1187, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4224, -64, -1153, 0 }, { { -1584, 0, -352, 0 }, { 1584, 0, -352, 0 }, { -1584, 0, 352, 0 }, { 1584, 0, 352, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, ONE, 0 }, 1619, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3888, -64, -1632, 0 }, { { -864, 0, 48, 0 }, { 928, 0, 48, 0 }, { -864, 0, 976, 0 }, { 928, 0, 976, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, ONE, 0 }, 1342, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6592, -64, 1728, 0 }, { { -1472, 0, -832, 0 }, { 448, 0, -832, 0 }, { -1472, 0, 256, 0 }, { 448, 0, 256, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1688, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6335, -64, 5535, 0 }, { { -991, 0, 349, 0 }, { 493, 0, -869, 0 }, { -492, 0, 870, 0 }, { 992, 0, -348, 0 } }, { 0, 4112, 0, 0 }, { -2598, 0, -3166, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4512, -64, 5728, 0 }, { { -960, 0, -1152, 0 }, { 864, 0, -1152, 0 }, { -960, 0, 544, 0 }, { 1792, 0, 544, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, ONE, 0 }, 1872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5536, -64, 5440, 0 }, { { -1584, 0, -704, 0 }, { 1584, 0, -704, 0 }, { -1584, 0, 704, 0 }, { 1584, 0, 704, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, ONE, 0 }, 1731, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry gShelterB6NurseryAmbientByView[20] = {
    { .viewCount = ARRAY_SIZE(gShelterB6NurseryAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1098, 1116, 1256, 1126 } },
    { .color = { 1237, 1258, 1320, 1257 } },
    { .color = { 1601, 1619, 1653, 1616 } },
    { .color = { 1175, 1200, 1260, 1198 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Cue-2 script entries in the loaded type-1 bank; cue 1 uses the next entry.
enum {
    SHELTER_B6_NURSERY_FOOTSTEP_WALK_SCRIPT = 0x2D,
    SHELTER_B6_NURSERY_FOOTSTEP_RUN_SCRIPT  = 0x2F,
};

/// Class-1 paired footstep requests, with walking cues reused for scripted jumps.
static WorldCollisionFootstepSounds _gShelterB6NurseryFootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B6_NURSERY_FOOTSTEP_WALK_SCRIPT,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B6_NURSERY_FOOTSTEP_RUN_SCRIPT,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B6_NURSERY_FOOTSTEP_WALK_SCRIPT,
};

/// Blocking, impact-enabled surface with pushback and no footstep cues.
static WorldCollisionSurfaceProperties _gShelterB6NurserySilentBlockingSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Blocking, impact-enabled class-1 surface with pushback and paired footsteps.
static WorldCollisionSurfaceProperties _gShelterB6NurseryFootstepBlockingSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gShelterB6NurseryFootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB6NurserySurfaceProperties[8] = {
    &_gShelterB6NurserySilentBlockingSurface,
    &_gShelterB6NurseryFootstepBlockingSurface,
    &_gShelterB6NurserySilentBlockingSurface,
    &_gShelterB6NurserySilentBlockingSurface,
    &_gShelterB6NurserySilentBlockingSurface,
    &_gShelterB6NurserySilentBlockingSurface,
    &_gShelterB6NurserySilentBlockingSurface,
    &_gShelterB6NurserySilentBlockingSurface,
};

Task* gRoomCutsceneSoundTask;

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\x1F\x51"
#include "../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

static void _glowDrawDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _glowDrawPulsingDisc(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);

void shelterB6NurseryTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/room/telephone_panels.inc.c"

#include "../../shared/room/room_cutscene_task.inc.c"

/// Nursery room task states 0 initialize, 1 wait for messages, 2 release.
///
/// Dispatch copies all three callback addresses before indexing by task state.
static const TaskFuncTable3 _gShelterB6NurseryRoomStateHandlers = {
    {
        _shelterB6NurseryInitializeRoomTask,
        _shelterB6NurseryMessageIdle,
        taskKill,
    },
};

/// Starts a nursery cutscene with the current CAP file and the selected follow-up.
///
/// Requires preconfigured sound fields and loaded view-6/CAP resources. Borrows
/// the persistent record unchanged through completion. Slot and skip values
/// narrow to signed bytes; skipScene zero plays the scene, nonzero skips its CAP playback.
/// capFile zero retains the selected file; followUpCommand runs after dialogue.
static inline void _shelterB6NurseryStartCommandScene(RoomCutsceneRec* scene, s32 capSlot, s32 skipScene, s32 followUpCommand)
{
    enum {
        SHELTER_B6_NURSERY_SCENE_VIEW            = 6,
        SHELTER_B6_NURSERY_CAP_FILE_KEEP_CURRENT = 0,
        SHELTER_B6_NURSERY_COMMAND_SCENE_TASK    = 0
    };

    scene->view      = SHELTER_B6_NURSERY_SCENE_VIEW;
    scene->capSlot   = capSlot;
    scene->capFile   = SHELTER_B6_NURSERY_CAP_FILE_KEEP_CURRENT;
    scene->skipScene = skipScene;
    taskSpawnFromTable(_gRoomCutsceneTaskDescs, SHELTER_B6_NURSERY_COMMAND_SCENE_TASK, followUpCommand, scene);
}

/// Handles command 10's nursery progress, CAP playback and cutscene selection.
///
/// The first payload is an integer command; other commands leave state intact.
/// Command 10 requests ambience stop and fills persistent scene sounds. At
/// progress 1 it advances the objective and progress, starts a cutscene task
/// with movie playback skipped, prepares the actor and clears effect cues.
/// Otherwise the first request latches a CAP event even if playback is busy;
/// later requests start the repeat cutscene. The cutscene borrows the persistent
/// record, which must remain loaded and unchanged through playback. Ignores
/// receiver, message ID and second payload; always returns zero.
static s32 _shelterB6NurseryHandleRoomCommand(Task* unusedTask, s32 messageId, s32 command, s32 unusedSecondArg)
{
    enum {
        SHELTER_B6_NURSERY_SCENE_START_SOUND         = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_NURSERY, 2),
        SHELTER_B6_NURSERY_SCENE_PLAYBACK_SOUND      = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_NURSERY, 3),
        SHELTER_B6_NURSERY_SCENE_AFTER_SOUND         = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_NURSERY, 4),
        SHELTER_B6_NURSERY_SCENE_END_SOUND           = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_NURSERY, 5),
        SHELTER_B6_NURSERY_PROGRESS_OBJECT_ID        = 0x22,
        SHELTER_B6_NURSERY_PROGRESS_OBJECT_STATE     = 1,
        SHELTER_B6_NURSERY_OBJECTIVE_AFTER_COMMAND   = 0x31,
        SHELTER_B6_NURSERY_INITIAL_SCENE_CAP_SLOT    = 11,
        SHELTER_B6_NURSERY_INITIAL_FOLLOW_UP_COMMAND = 25,
        SHELTER_B6_NURSERY_REPEAT_SCENE_CAP_SLOT     = 22,
        SHELTER_B6_NURSERY_FIRST_REPEAT_CAP_COMMAND  = 23,
        SHELTER_B6_NURSERY_REPEAT_FOLLOW_UP_COMMAND  = 10,
        SHELTER_B6_NURSERY_REPEAT_EVENT_UNSEEN       = 0,
        SHELTER_B6_NURSERY_REPEAT_EVENT_SEEN         = 1,
        SHELTER_B6_NURSERY_SCENE_PLAY_MOVIE          = 0
    };
    s32 nurseryProgress;

    if (command == SHELTER_B6_NURSERY_COMMAND_SCENE) {
        _gShelterB6NurseryAmbienceRequested               = SHELTER_B6_NURSERY_AMBIENCE_STOP_REQUEST;
        _gShelterB6NurseryCutsceneRec.rec.startSound      = SHELTER_B6_NURSERY_SCENE_START_SOUND;
        _gShelterB6NurseryCutsceneRec.rec.endSound        = SHELTER_B6_NURSERY_SCENE_END_SOUND;
        _gShelterB6NurseryCutsceneRec.rec.sceneSound      = SHELTER_B6_NURSERY_SCENE_PLAYBACK_SOUND;
        _gShelterB6NurseryCutsceneRec.rec.afterSceneSound = SHELTER_B6_NURSERY_SCENE_AFTER_SOUND;
        nurseryProgress                                   = gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS);
        if (nurseryProgress == SHELTER_B6_NURSERY_PROGRESS_ENTERED) {
            // Commit the first command's progress before starting its scene.
            if (gameFlagGetNibble(GAME_FLAG_083) != 0) {
                areaSetObjectState(SHELTER_B6_NURSERY_PROGRESS_OBJECT_ID, SHELTER_B6_NURSERY_PROGRESS_OBJECT_STATE, GAME_STAGE_MINE_SHELTER);
            }
            gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, SHELTER_B6_NURSERY_OBJECTIVE_AFTER_COMMAND);
            gameFlagSetNibble(GAME_FLAG_B6_NURSERY_PROGRESS, SHELTER_B6_NURSERY_PROGRESS_ADVANCED);
            _shelterB6NurseryStartCommandScene(&_gShelterB6NurseryCutsceneRec.rec,
                                               SHELTER_B6_NURSERY_INITIAL_SCENE_CAP_SLOT,
                                               nurseryProgress, SHELTER_B6_NURSERY_INITIAL_FOLLOW_UP_COMMAND);
            actor450800PrepareNurseryKyleMadigan();
            shelterB6NurserySetEffectCues(0, 0);
            return 0;
        }
        if (gameFlagGetNibble(GAME_FLAG_NURSERY_SCENE_SEEN) == SHELTER_B6_NURSERY_REPEAT_EVENT_UNSEEN) {
            capSpawnEventIfIdle(SHELTER_B6_NURSERY_FIRST_REPEAT_CAP_COMMAND, CAP_EVENT_NO_FLAGS);
            gameFlagSetNibble(GAME_FLAG_NURSERY_SCENE_SEEN, SHELTER_B6_NURSERY_REPEAT_EVENT_SEEN);
            return 0;
        }
        _shelterB6NurseryStartCommandScene(&_gShelterB6NurseryCutsceneRec.rec,
                                           SHELTER_B6_NURSERY_REPEAT_SCENE_CAP_SLOT,
                                           SHELTER_B6_NURSERY_SCENE_PLAY_MOVIE, SHELTER_B6_NURSERY_REPEAT_FOLLOW_UP_COMMAND);
    }
    return 0;
}

/// Refreshes the nursery's fixed sound origin through the current view.
///
/// Replaces the persistent origin's translation with room-coordinate units
/// (6000, 0, -830), attaches it to the live view and refreshes its composed
/// matrix for the spatial pan and depth queries. Leaves its stored basis
/// unchanged; the current view and composition pass must be initialized.
static inline void _shelterB6NurseryComposeAmbienceOrigin(void)
{
    _gShelterB6NurseryAmbienceOrigin.coord.t[0]   = 6000;
    _gShelterB6NurseryAmbienceOrigin.coord.t[1]   = 0;
    _gShelterB6NurseryAmbienceOrigin.coord.t[2]   = -830;
    _gShelterB6NurseryAmbienceOrigin.parent       = &gGfxViewCoord;
    _gShelterB6NurseryAmbienceOrigin.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&_gShelterB6NurseryAmbienceOrigin);
}

/// Plays positional nursery ambience and retunes it after a view change.
///
/// Starts once, then waits for saved and active view IDs to diverge. Three
/// settling states precede the mix update for the active nursery view (1..19).
/// A depth override of -1 keeps the computed attenuation; pan/depth transport
/// keeps their signed low bytes. The room's ambience latch requests stopping
/// in the wait state; the task owns that sound's stop and its own teardown.
static void _shelterB6NurseryAmbienceTask(Task* task)
{
    enum {
        SHELTER_B6_NURSERY_AMBIENCE_START     = 0,
        SHELTER_B6_NURSERY_AMBIENCE_WAIT_VIEW = 1,
        SHELTER_B6_NURSERY_AMBIENCE_SETTLE_1  = 2,
        SHELTER_B6_NURSERY_AMBIENCE_SETTLE_2  = 3,
        SHELTER_B6_NURSERY_AMBIENCE_SETTLE_3  = 4,
        SHELTER_B6_NURSERY_AMBIENCE_RETUNE    = 5
    };
    s32 pan;
    s32 depth;
    s32 viewDepth;

    // Compose the fixed room-space source through the current camera every frame.
    _shelterB6NurseryComposeAmbienceOrigin();
    pan   = worldCoordGetOriginAudioPan(&_gShelterB6NurseryAmbienceOrigin);
    depth = worldCoordGetOriginAudioDepth(&_gShelterB6NurseryAmbienceOrigin);
    switch (task->state) {
        case SHELTER_B6_NURSERY_AMBIENCE_START:
            sndEvtRequestScriptStart(SOUND_SHELTER_B6_NURSERY_AMBIENCE, (s8)pan, (s8)depth);
            task->state++;
            break;
        case SHELTER_B6_NURSERY_AMBIENCE_WAIT_VIEW:
            if (_gShelterB6NurseryAmbienceRequested == SHELTER_B6_NURSERY_AMBIENCE_STOP_REQUEST) {
                sndEvtRequestScriptStop(SOUND_SHELTER_B6_NURSERY_AMBIENCE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                taskKill(task);
                return;
            }
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view != gGameSession->location.view) {
                task->state++;
            }
            break;
        case SHELTER_B6_NURSERY_AMBIENCE_SETTLE_1:
        case SHELTER_B6_NURSERY_AMBIENCE_SETTLE_2:
        case SHELTER_B6_NURSERY_AMBIENCE_SETTLE_3:
            task->state++;
            break;
        case SHELTER_B6_NURSERY_AMBIENCE_RETUNE:
            viewDepth = _gShelterB6NurseryAmbienceDepthOverrides[gGameSession->location.view];
            if (viewDepth != SHELTER_B6_NURSERY_AMBIENCE_COMPUTED_DEPTH) {
                depth = viewDepth;
            }
            sndEvtRequestScriptMix(SOUND_SHELTER_B6_NURSERY_AMBIENCE, (s8)pan, (s8)depth);
            task->state = SHELTER_B6_NURSERY_AMBIENCE_WAIT_VIEW;
            break;
    }
}

#include "../../shared/room/room_cutscene_sound_task.inc.c"

/// Refuses every key-item-use request to this room, returning 0 without consuming the item.
static s32 _shelterB6NurseryRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves nursery departures and requests their CAP transition on execution.
///
/// Borrows complete eight-byte request/reply records, which may alias. Copies
/// the request and resolves its Neo Ark room variant before testing query mode;
/// queries start no playback. Requires map_neo_ark to remain loaded for the
/// synchronous resolver. Retains no payload, ignores receiver/message ID and
/// always returns zero to keep departure under CAP control.
static s32 _shelterB6NurseryResolveRoomTransition(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_B6_NURSERY_DEPARTURE_CAP_COMMAND = 12 };
    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
        capRunCommandWithTransition(SHELTER_B6_NURSERY_DEPARTURE_CAP_COMMAND);
    }
    return 0;
}

/// Dispatches nursery interaction, companion dialogue and the enabled later scene.
///
/// Borrows a four-byte `DirectionActionRequest`. Actions 1 and 2 request the
/// actor's interaction and companion dialogue; action 3 starts a skippable
/// scene only after the nursery scene count is nonzero. The request's argument
/// byte, receiver, message ID and zero second payload are ignored. Retains no
/// payload and returns zero for every action, including unsupported IDs.
static s32 _shelterB6NurseryHandleRoomAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    if (request->actionId == SHELTER_B6_NURSERY_ACTION_INTERACT) {
        actor450800StartNurseryInteraction();
    }
    if (request->actionId == SHELTER_B6_NURSERY_ACTION_COMPANION_DIALOGUE) {
        actor450800StartNurseryCompanionDialogue();
    }
    if (request->actionId == SHELTER_B6_NURSERY_ACTION_LATER_SCENE && gameFlagGetNibble(GAME_FLAG_B6_NURSERY_SCENE_COUNT) != 0) {
        evsStartScriptWithSkip(gActor450800NurseryShootingScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor450800NurseryShootingSkipScript);
    }
    return 0;
}

/// Registers nursery messages, restores companion HP and selects the entry scene.
///
/// Requires a live state-0 room task and loaded actor_450800 scripts. Publishes
/// the borrowed message table and room task slot. First entry commits progress
/// 1, clears the follow-up state and updates the objective; later entries choose
/// the intermediate or advanced script without changing progress. Advances to
/// idle state 1 after starting the selected script.
static void _shelterB6NurseryInitializeRoomTask(Task* task)
{
    enum {
        SHELTER_B6_NURSERY_OBJECTIVE_ON_ENTRY = 0x30
    };
    task->msgTable = _gShelterB6NurseryRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    companionRestoreFullHp();
    if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) == SHELTER_B6_NURSERY_PROGRESS_NOT_ENTERED) {
        gameFlagSetNibble(GAME_FLAG_B6_NURSERY_PROGRESS, SHELTER_B6_NURSERY_PROGRESS_ENTERED);
        evsStartScriptWithSkip(gActor450800NurseryIntroScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor450800NurseryIntroSkipScript);
        gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, SHELTER_B6_NURSERY_OBJECTIVE_ON_ENTRY);
    } else if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) == SHELTER_B6_NURSERY_PROGRESS_ENTERED) {
        evsStartScript(gActor450800NurseryReentryScript, EVENT_SCRIPT_HUD_KEEP);
    } else {
        evsStartScript(gActor450800NurseryAdvancedReentryScript, EVENT_SCRIPT_HUD_KEEP);
    }
    task->state++;
}

/// Keeps the room message task alive while it waits for messages.
static void _shelterB6NurseryMessageIdle(Task* task)
{
    // Retain the original idle state's otherwise unused 16-byte stack frame.
    char unusedStackFrame[0x10];
}

void shelterB6NurseryRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gShelterB6NurseryRoomStateHandlers;
    states.funcs[task->state](task);
}

void shelterB6NurseryStartAmbience(void)
{
    enum {
        SHELTER_B6_NURSERY_AMBIENCE_STOPPED = 0,
        SHELTER_B6_NURSERY_AMBIENCE_STARTED = 1
    };

    if (_gShelterB6NurseryAmbienceRequested == SHELTER_B6_NURSERY_AMBIENCE_STOPPED) {
        _gShelterB6NurseryAmbienceRequested = SHELTER_B6_NURSERY_AMBIENCE_STARTED;
        taskSpawnFromTable(&_gShelterB6NurseryAmbienceTaskDesc, 0, 0, 0);
    }
}

void shelterB6NurserySetView13SpriteHidden(u8 hidden)
{
    enum { VIEW_INDEX     = 12,
           BATCH_INDEX    = 1,
           SPRITE_VISIBLE = 0,
           SPRITE_HIDDEN  = 1 };

    const GameLocationKey* location = &gGameSession->location;
    SpriteBatch*           view13Batches;

    view13Batches = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1][VIEW_INDEX].batches;
    if (hidden == SPRITE_VISIBLE) {
        view13Batches[BATCH_INDEX].hidden = SPRITE_VISIBLE;
    } else if (hidden == SPRITE_HIDDEN) {
        view13Batches[BATCH_INDEX].hidden = SPRITE_HIDDEN;
    }
}

void shelterB6NurseryViewEffectsTask(Task* task)
{
    enum { VIEW_EFFECT_INITIALIZE,
           VIEW_EFFECT_WAIT_DEBRIS,
           VIEW_EFFECT_DEBRIS_SPAWNED,
           VIEW_EFFECT_SHOWER_ANCHORS_ACTIVE,
           GLINT_PULSE_IDLE         = 0x60,
           GLINT_PULSE_CUED         = 0x180,
           GLINT_RADIUS_SCALE       = 0x80,
           SPARK_SHOWER_SHARD_COUNT = 16,
           SPARK_SHOWER_Q12_SHIFT   = 12,
           // Scatter, speed 48, period 3..4, half-diagonal 768..1279.
           AMBIENT_PARTICLE_SPAWN_BASE  = 0x02303300,
           AMBIENT_PARTICLE_RANDOM_MASK = 0x11FF };

    SVECTOR* glintPoint;
    u32      angleRandom;
    u32      spreadRandom;
    s32      angle;
    s32      spread;
    s32      shardIndex;

    if (task->state == VIEW_EFFECT_INITIALIZE) {
        gRoomEffectFlashId                            = EFFECT_SHELTER_B6_NURSERY_FLASH;
        gRoomEffectTwinTrailId                        = EFFECT_SHELTER_B6_NURSERY_TWIN_TRAIL;
        gRoomEffectSparkBurstId                       = EFFECT_SHELTER_B6_NURSERY_SPARK_BURST;
        _gShelterB6NurseryEffectCues.fastGlintPulse   = 0;
        _gShelterB6NurseryEffectCues.sparkShowerScale = 0;
        task->state                                   = VIEW_EFFECT_WAIT_DEBRIS;
    }
    switch (viewGetMappedIndex() & 0xFF) {
        case 3:
        case 8:
            if (_gShelterB6NurseryEffectCues.fastGlintPulse != 0) {
                _glowDrawDiamond(&_gShelterB6NurseryEffectAnchors.glint, GLINT_PULSE_CUED, GLINT_RADIUS_SCALE);
            } else {
                _glowDrawDiamond(&_gShelterB6NurseryEffectAnchors.glint, GLINT_PULSE_IDLE, GLINT_RADIUS_SCALE);
            }
            break;
        case 6:
        case 10:
            if (_gShelterB6NurseryEffectCues.fastGlintPulse != 0) {
                _glowDrawPulsingDisc(&_gShelterB6NurseryEffectAnchors.glint, GLINT_PULSE_CUED, GLINT_RADIUS_SCALE);
            } else {
                _glowDrawPulsingDisc(&_gShelterB6NurseryEffectAnchors.glint, GLINT_PULSE_IDLE, GLINT_RADIUS_SCALE);
            }
            break;
        case 12:
            // Spawn the three independent model chunks once before the shower phase.
            if (task->state == VIEW_EFFECT_WAIT_DEBRIS) {
                effectSpawn(EFFECT_SHELTER_B6_NURSERY_DEBRIS_CHUNK, NULL, task->spawnArg1.value, &_gShelterB6NurseryEffectAnchors.debrisOrigin);
                effectSpawn(EFFECT_SHELTER_B6_NURSERY_DEBRIS_CHUNK, NULL, task->spawnArg1.value, &_gShelterB6NurseryEffectAnchors.debrisOrigin);
                effectSpawn(EFFECT_SHELTER_B6_NURSERY_DEBRIS_CHUNK, NULL, task->spawnArg1.value, &_gShelterB6NurseryEffectAnchors.debrisOrigin);
                task->state = VIEW_EFFECT_DEBRIS_SPAWNED;
            }
            break;
        case 13:
            task->state = VIEW_EFFECT_SHOWER_ANCHORS_ACTIVE;
            if (_gShelterB6NurseryEffectCues.sparkShowerScale != 0) {
                // Consume one cue as sixteen scaled shards; retain the identical Y/Z sine offsets.
                for (shardIndex = 0; shardIndex < SPARK_SHOWER_SHARD_COUNT; shardIndex++) {
                    angleRandom                                          = RANDOM_LCG_NEXT(gRandomLcgState);
                    spreadRandom                                         = RANDOM_LCG_NEXT(angleRandom);
                    angle                                                = (angleRandom >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
                    gRandomLcgState                                      = spreadRandom;
                    spread                                               = ((spreadRandom >> 16) & 0xFF) * _gShelterB6NurseryEffectCues.sparkShowerScale;
                    _gShelterB6NurseryEffectAnchors.sparkShowerOrigin.vx = 0x1C20;
                    _gShelterB6NurseryEffectAnchors.sparkShowerOrigin.vy = ((spread * rsin(angle)) >> SPARK_SHOWER_Q12_SHIFT) - 0x6D6;
                    _gShelterB6NurseryEffectAnchors.sparkShowerOrigin.vz = ((spread * rsin(angle)) >> SPARK_SHOWER_Q12_SHIFT) + 0x7D0;
                    RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_SHELTER_B6_NURSERY_SPARK_SHOWER, NULL,
                                ((RANDOM_LCG_DRAW() & 0x1F) + 8) * _gShelterB6NurseryEffectCues.sparkShowerScale,
                                &_gShelterB6NurseryEffectAnchors.sparkShowerOrigin);
                }
                _gShelterB6NurseryEffectCues.fastGlintPulse   = 0;
                _gShelterB6NurseryEffectCues.sparkShowerScale = 0;
            }
            break;
        case 15:
            if (!(gDisplayState.animFrame & 1)) {
                RANDOM_LCG_ADVANCE();
                effectSpawn(EFFECT_1A4, NULL, (RANDOM_LCG_DRAW() & AMBIENT_PARTICLE_RANDOM_MASK) + AMBIENT_PARTICLE_SPAWN_BASE, &_gShelterB6NurseryEffectAnchors.view15ParticleOrigin);
            }
            break;
        case 17:
            glintPoint = &_gShelterB6NurseryEffectAnchors.glint;
            _glowDrawDiamond(glintPoint, GLINT_PULSE_IDLE, GLINT_RADIUS_SCALE);
            if (!(gDisplayState.animFrame & 1)) {
                RANDOM_LCG_ADVANCE();
                effectSpawn(EFFECT_1A4, NULL, (RANDOM_LCG_DRAW() & AMBIENT_PARTICLE_RANDOM_MASK) + AMBIENT_PARTICLE_SPAWN_BASE, &_gShelterB6NurseryEffectAnchors.view17And18ParticleOrigin);
            }
            break;
        case 18:
            if (!(gDisplayState.animFrame & 1)) {
                RANDOM_LCG_ADVANCE();
                effectSpawn(EFFECT_1A4, NULL, (RANDOM_LCG_DRAW() & AMBIENT_PARTICLE_RANDOM_MASK) + AMBIENT_PARTICLE_SPAWN_BASE, &_gShelterB6NurseryEffectAnchors.view17And18ParticleOrigin);
            }
            break;
    }
    // Visiting view 13 latches these two emitters for the rest of the task.
    if (task->state == VIEW_EFFECT_SHOWER_ANCHORS_ACTIVE && !(gDisplayState.animFrame & 1)) {
        RANDOM_LCG_ADVANCE();
        effectSpawn(EFFECT_1A4, NULL, (RANDOM_LCG_DRAW() & AMBIENT_PARTICLE_RANDOM_MASK) + AMBIENT_PARTICLE_SPAWN_BASE, &_gShelterB6NurseryEffectAnchors.latchedParticleOrigins[0]);
        RANDOM_LCG_ADVANCE();
        effectSpawn(EFFECT_1A4, NULL, (RANDOM_LCG_DRAW() & AMBIENT_PARTICLE_RANDOM_MASK) + AMBIENT_PARTICLE_SPAWN_BASE, &_gShelterB6NurseryEffectAnchors.latchedParticleOrigins[1]);
    }
}

#include "../../shared/effects/glow_draw_diamond.inc.c"

#include "../../shared/effects/glow_draw_pulsing_disc.inc.c"

void shelterB6NurseryDebrisChunkTask(Task* task)
{
    /// Adds one speed-scaled Q12 direction step to the debris's local translation.
    ///
    /// coordNode, effectWork and displacementOut are stable GfxCoord*, EffectWork*
    /// and writable SVECTOR* arguments with distinct storage. Evaluated repeatedly.
    /// scale is whole parent-coordinate units per tick; move is a signed Q12
    /// direction. GPF12 narrows/saturates their product to signed-halfword XYZ,
    /// writes the displacement for collision rollback, then adds it to the root's
    /// full-width XYZ. Clobbers GTE vector/product state; leaves pad and composition
    /// stamp unchanged. The caller marks composition dirty before probing/retrying.
#define SHELTER_B6_NURSERY_INTEGRATE_DEBRIS_MOTION(coordNode, effectWork, displacementOut) \
    {                                                                                      \
        gte_lddp((effectWork)->scale);                                                     \
        gte_ldsv(&(effectWork)->move);                                                     \
        gte_gpf12();                                                                       \
        gte_stsv(displacementOut);                                                         \
        (coordNode)->coord.t[0] += (displacementOut)->vx;                                  \
        (coordNode)->coord.t[1] += (displacementOut)->vy;                                  \
        (coordNode)->coord.t[2] += (displacementOut)->vz;                                  \
    }

    enum { DEBRIS_INITIALIZE     = 0,
           DEBRIS_VIEW           = 12,
           DEBRIS_GRAVITY_Q12    = 0x180,
           DEBRIS_LATE_TRAIL_AGE = 0x40,
           // Small strip, scatter, speed 16, period 1..2, half-diagonal 768.
           DEBRIS_EARLY_PARTICLE_SPAWN_BASE  = 0x82101300,
           DEBRIS_EARLY_PARTICLE_RANDOM_MASK = 0x1000,
           // Large strip, scatter, speed 24, period 3..4, half-diagonal 768..1023.
           DEBRIS_LATE_PARTICLE_SPAWN_BASE  = 0x02183300,
           DEBRIS_LATE_PARTICLE_RANDOM_MASK = 0x10FF };

    SVECTOR     displacement;
    SVECTOR     probeEnd;
    SVECTOR     probeStartOrNormal;
    TmdObject*  model;
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;

    model = task->extra.tmd;
    work  = task->spawnArg2.pointer;
    coord = model->coords;
    if ((viewGetMappedIndex() & 0xFF) != DEBRIS_VIEW) {
        effectKillTask(work, task);
        return;
    }
    effectControl = gRoomEffectState->effectControl;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
    } else {
        actorRenderComposeCoord(coord);
        if (task->state == DEBRIS_INITIALIZE) {
            model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            RANDOM_LCG_ADVANCE();
            work->move.vx = (RANDOM_LCG_DRAW() & 0x3F) + 0x60;
            work->move.vy = 0;
            RANDOM_LCG_ADVANCE();
            work->move.vz = (RANDOM_LCG_DRAW() & 0x3F) + 0x20;
            RANDOM_LCG_ADVANCE();
            work->scale = (RANDOM_LCG_DRAW() & 0x3F) + 0x40;
            VectorNormalSS(&work->move, &work->move);
            work->pos.vy = 0;
            RANDOM_LCG_ADVANCE();
            work->pos.vx = -(RANDOM_LCG_DRAW() & 0x3F);
            RANDOM_LCG_ADVANCE();
            work->pos.vz        = 0x40 - (RANDOM_LCG_DRAW() & 0x7F);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state++;
            return;
        }
        // Tumble the model, then integrate its speed-scaled Q12 direction.
        gfxRotMatrixXYZ(&coord->coord, &work->pos, GRAPHICS_ROTATION_COMPOSE);
        MatrixNormal(&coord->coord, &coord->coord);
        SHELTER_B6_NURSERY_INTEGRATE_DEBRIS_MOTION(coord, work, &displacement);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        // Probe the candidate segment in view space; reuse the start as a room-space hit normal.
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&displacement);
        gte_rtv0();
        gte_stsv(&probeEnd);
        probeStartOrNormal.vx = coord->workm.t[0];
        probeStartOrNormal.vy = coord->workm.t[1];
        probeStartOrNormal.vz = coord->workm.t[2];
        probeEnd.vx          += probeStartOrNormal.vx;
        probeEnd.vy          += probeStartOrNormal.vy;
        probeEnd.vz          += probeStartOrNormal.vz;
        if (worldCollisionProbeGridSegment(&probeEnd, &probeStartOrNormal, &probeEnd, &probeStartOrNormal) == 1) {
            // Undo the blocked step and mix the normal into the direction before retrying.
            coord->coord.t[0] -= displacement.vx;
            coord->coord.t[1] -= displacement.vy;
            coord->coord.t[2] -= displacement.vz;
            work->move.vx      = (probeStartOrNormal.vx >> 1) + (work->move.vx >> 1);
            work->move.vy      = probeStartOrNormal.vy + (work->move.vy >> 1);
            work->move.vz      = (probeStartOrNormal.vz >> 1) + (work->move.vz >> 1);
            VectorNormalSS(&work->move, &work->move);
            work->scale = work->scale * 2 / 3;
            SHELTER_B6_NURSERY_INTEGRATE_DEBRIS_MOTION(coord, work, &displacement);
        } else {
            work->move.vy += DEBRIS_GRAVITY_Q12;
        }
        // Alternate active frames emit a particle; age selects its strip and launch parameters.
        if (work->age & 1) {
            if (work->age > DEBRIS_LATE_TRAIL_AGE) {
                RANDOM_LCG_ADVANCE();
                effectSpawn(EFFECT_1A4, coord, (RANDOM_LCG_DRAW() & DEBRIS_LATE_PARTICLE_RANDOM_MASK) + DEBRIS_LATE_PARTICLE_SPAWN_BASE, NULL);
            } else {
                RANDOM_LCG_ADVANCE();
                effectSpawn(EFFECT_1A4, coord, (RANDOM_LCG_DRAW() & DEBRIS_EARLY_PARTICLE_RANDOM_MASK) + DEBRIS_EARLY_PARTICLE_SPAWN_BASE, NULL);
            }
        }
        work->age++;
    }
}

#undef SHELTER_B6_NURSERY_INTEGRATE_DEBRIS_MOTION

/// Advances a moving particle in its parent's coordinate frame and updates its Y velocity.
///
/// A zero `work->step` disables both motion and acceleration. `work->move`
/// holds signed coordinate units per frame; movement precedes the velocity
/// update and invalidates the composed matrix. Motion mode 7 adds signed
/// `work->age / 10` to Y velocity; other modes subtract
/// `negativeYAcceleration` (2 for the ten-frame strip, 1 for the eight-frame
/// strip), in coordinate units per frame squared. Velocity updates narrow to
/// s16. All pointers borrow live objects for this call; the task is read-only.
static inline void _shelterB6NurseryAdvanceParticle(const Task* task, EffectWork* work, GfxCoord* coord, s32 negativeYAcceleration)
{
    enum { PARTICLE_MOTION_PARENT_JET      = 7,
           PARTICLE_MOTION_SHIFT           = 24,
           PARTICLE_MOTION_MASK            = 0xF,
           PARTICLE_PARENT_JET_AGE_DIVISOR = 10 };

    if (work->step != 0) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        if (((task->spawnArg1.value >> PARTICLE_MOTION_SHIFT) & PARTICLE_MOTION_MASK) == PARTICLE_MOTION_PARENT_JET) {
            work->move.vy += work->age / PARTICLE_PARENT_JET_AGE_DIVISOR;
        } else {
            work->move.vy -= negativeYAcceleration;
        }
    }
}

void shelterB6NurseryAnimatedParticleTask(Task* task)
{
    enum { PARTICLE_INITIALIZE,
           PARTICLE_LARGE_STRIP,
           PARTICLE_SMALL_STRIP,
           PARTICLE_LARGE_FRAME_COUNT     = 10,
           PARTICLE_SMALL_FRAME_COUNT     = 8,
           PARTICLE_FRAME_PERIOD_DEFAULT  = 1,
           PARTICLE_SPEED_DEFAULT         = 0x40,
           PARTICLE_MOTION_STATIONARY     = 0,
           PARTICLE_MOTION_NEGATIVE_Y_FAN = 1,
           PARTICLE_MOTION_SCATTER        = 2,
           PARTICLE_MOTION_NEGATIVE_Y_JET = 3,
           PARTICLE_MOTION_SPAWN_VECTOR   = 5,
           PARTICLE_MOTION_XZ_SCATTER     = 6,
           PARTICLE_MOTION_PARENT_JET     = 7,
           PARTICLE_HALF_DIAGONAL_MASK    = 0xFFF,
           PARTICLE_PERIOD_SELECT_MASK    = 0xF000,
           PARTICLE_PERIOD_SHIFT          = 12,
           PARTICLE_PERIOD_MASK           = 7,
           PARTICLE_SPEED_SELECT_MASK     = 0xFF0000,
           PARTICLE_SPEED_SHIFT           = 16,
           PARTICLE_SPEED_MASK            = 0xFF,
           PARTICLE_MOTION_SHIFT          = 24,
           PARTICLE_MOTION_MASK           = 0xF };

    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    velocity;
    s32         framePeriod;
    s32         initialSpeed;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    work->age++;
    switch (task->state) {
        case PARTICLE_INITIALIZE:
            // Decode the two texture strips, frame cadence and initial motion from the spawn word.
            work->scale = task->spawnArg1.value & PARTICLE_HALF_DIAGONAL_MASK;
            RANDOM_LCG_ADVANCE();
            work->angle = RANDOM_LCG_DRAW() & ACTOR_TRANSFORM_ANGLE_MASK;
            if (task->spawnArg1.value & PARTICLE_PERIOD_SELECT_MASK) {
                framePeriod = (task->spawnArg1.value >> PARTICLE_PERIOD_SHIFT) & PARTICLE_PERIOD_MASK;
            } else {
                framePeriod = PARTICLE_FRAME_PERIOD_DEFAULT;
            }
            work->period = framePeriod;
            work->age    = 0;
            task->state  = task->spawnArg1.value < 0 ? PARTICLE_SMALL_STRIP : PARTICLE_LARGE_STRIP;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & PARTICLE_SPEED_SELECT_MASK) {
                    initialSpeed = (task->spawnArg1.value >> PARTICLE_SPEED_SHIFT) & PARTICLE_SPEED_MASK;
                } else {
                    initialSpeed = PARTICLE_SPEED_DEFAULT;
                }
                work->step = initialSpeed;
                switch ((task->spawnArg1.value >> PARTICLE_MOTION_SHIFT) & PARTICLE_MOTION_MASK) {
                    case PARTICLE_MOTION_STATIONARY:
                        work->step = 0;
                        break;
                    case PARTICLE_MOTION_NEGATIVE_Y_FAN:
                        RANDOM_LCG_ADVANCE();
                        work->move.vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                        RANDOM_LCG_ADVANCE();
                        work->move.vy = 0xFFC0 - (RANDOM_LCG_DRAW() & 0x7F);
                        RANDOM_LCG_ADVANCE();
                        work->move.vz = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                        break;
                    case PARTICLE_MOTION_SCATTER:
                        RANDOM_LCG_ADVANCE();
                        work->move.vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                        RANDOM_LCG_ADVANCE();
                        work->move.vy = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                        RANDOM_LCG_ADVANCE();
                        work->move.vz = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                        break;
                    case PARTICLE_MOTION_NEGATIVE_Y_JET:
                        RANDOM_LCG_ADVANCE();
                        work->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                        RANDOM_LCG_ADVANCE();
                        work->move.vy = -(RANDOM_LCG_DRAW() & 0xFF);
                        RANDOM_LCG_ADVANCE();
                        work->move.vz = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                        break;
                    case PARTICLE_MOTION_SPAWN_VECTOR:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                    case PARTICLE_MOTION_XZ_SCATTER:
                        work->move.vy = 0;
                        RANDOM_LCG_ADVANCE();
                        work->move.vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                        RANDOM_LCG_ADVANCE();
                        work->move.vz = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                        break;
                    case PARTICLE_MOTION_PARENT_JET:
                        RANDOM_LCG_ADVANCE();
                        work->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                        RANDOM_LCG_ADVANCE();
                        work->move.vy = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                        RANDOM_LCG_ADVANCE();
                        work->move.vz = RANDOM_LCG_DRAW() & 0xFF;
                        gte_SetRotMatrix(&work->parent->coord);
                        gte_ldv0(&work->move);
                        gte_rtv0();
                        gte_stsv(&work->move);
                        break;
                }
                velocity = &work->move;
                VectorNormalSS(velocity, velocity);
                gte_lddp(work->step);
                gte_ldsv(velocity);
                gte_gpf12();
                gte_stsv(velocity);
            } else {
                work->step = PARTICLE_SPEED_DEFAULT;
            }
            break;
        case PARTICLE_LARGE_STRIP:
            _shelterB6NurseryDrawParticleFrame(coord, work->index, work->scale, work->angle);
            _shelterB6NurseryAdvanceParticle(task, work, coord, 2);
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= PARTICLE_LARGE_FRAME_COUNT) {
                    effectKillTask(work, task);
                }
            }
            break;
        case PARTICLE_SMALL_STRIP:
            spriteQuadDraw(coord, work->index, work->scale, work->angle);
            _shelterB6NurseryAdvanceParticle(task, work, coord, 1);
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= PARTICLE_SMALL_FRAME_COUNT) {
                    effectKillTask(work, task);
                }
            }
            break;
    }
}

/// Writes one rotated pixel offset from the ten-frame particle's projected centre.
///
/// `projection` borrows a live scratch block with `depth` initialized to SZ3 / 4;
/// its caller admits depths of at least 65. Only `extent.corner` is replaced.
/// The signed `sizeFactor * 47 / depth` is the screen-space half-diagonal in
/// pixels, truncated toward zero before rotation; 47 is the cell's UV span.
/// `cornerAngle` uses 4096 units per turn, with zero pointing up and a quarter
/// turn pointing right. Signed Q12 products shift to pixels, rounding negative
/// values down. The caller applies the Y offset with the opposite sign for
/// screen coordinates. No scratch pointer is retained.
static inline void _shelterB6NurserySetParticleCornerOffset(EffectShapeScratch* projection, s16 sizeFactor, s32 cornerAngle)
{
    enum { PARTICLE_PROJECTION_SCALE   = 47,
           PARTICLE_TRIG_FRACTION_BITS = 12 };

    s32 halfDiagonalPixels;
    s32 trigSample;

    trigSample                  = rsin(cornerAngle);
    halfDiagonalPixels          = (sizeFactor * PARTICLE_PROJECTION_SCALE) / projection->depth;
    projection->extent.corner.x = (halfDiagonalPixels * trigSample) >> PARTICLE_TRIG_FRACTION_BITS;
    trigSample                  = rcos(cornerAngle);
    halfDiagonalPixels          = (sizeFactor * PARTICLE_PROJECTION_SCALE) / projection->depth;
    projection->extent.corner.y = (halfDiagonalPixels * trigSample) >> PARTICLE_TRIG_FRACTION_BITS;
}

/// Draws one raw additive frame of the nursery's ten-cell particle animation.
///
/// `animationFrame` is 0..9. The cached centre must be current; `halfDiagonal`
/// is scaled by 47 / (SZ3 / 4) to pixels, then rotated by `rotation` in
/// 4096 units per turn. Uses 28 scratch-stack bytes and allocates a textured
/// quad before rejecting negative projection flags or depths below 65.
/// Packets borrow the frame arena until GPU drawing completes.
static void _shelterB6NurseryDrawParticleFrame(const GfxCoord* coord, u16 animationFrame, s16 halfDiagonal, s16 rotation)
{
    enum { PARTICLE_CELLS_PER_ROW = 5,
           PARTICLE_CELL_TEXELS   = 48,
           PARTICLE_TOP_V         = 0x28,
           PARTICLE_MIN_DEPTH     = 0x41,
           PARTICLE_QUARTER_TURN  = ACTOR_TRANSFORM_ANGLE_TURN / 4 };

    EffectShapeScratch* projection;
    POLY_FT4*           quad;
    s32                 u0;
    s32                 v0;
    s32                 cornerAngle;
    s32                 nextCornerAngle;
    u16                 textureColumn;
    u16                 textureRow;

    // Stage the cached centre's low 16 bits, preserving packet allocation before culling.
    projection                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    projection->worldPoint.vx = (u16)coord->workm.t[0];
    projection->worldPoint.vy = (u16)coord->workm.t[1];
    projection->worldPoint.vz = (u16)coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        if (projection->depth >= PARTICLE_MIN_DEPTH) {
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
            quad->clut  = getClut(64, 270);
            setSemiTrans(quad, 1);
            setShadeTex(quad, 1);
            textureColumn = animationFrame % PARTICLE_CELLS_PER_ROW;
            textureRow    = animationFrame / PARTICLE_CELLS_PER_ROW;
            u0            = textureColumn * PARTICLE_CELL_TEXELS;
            v0            = textureRow * PARTICLE_CELL_TEXELS;
            setUV4(quad, u0, v0 + PARTICLE_TOP_V, u0 + PARTICLE_CELL_TEXELS - 1, v0 + PARTICLE_TOP_V, u0, v0 + PARTICLE_TOP_V + PARTICLE_CELL_TEXELS - 1, u0 + PARTICLE_CELL_TEXELS - 1, v0 + PARTICLE_TOP_V + PARTICLE_CELL_TEXELS - 1);
            cornerAngle = rotation;
            _shelterB6NurserySetParticleCornerOffset(projection, halfDiagonal, cornerAngle);
            quad->x0        = projection->screenX + (u16)projection->extent.corner.x;
            quad->x3        = projection->screenX - (u16)projection->extent.corner.x;
            quad->y0        = projection->screenY - (u16)projection->extent.corner.y;
            nextCornerAngle = cornerAngle + PARTICLE_QUARTER_TURN;
            quad->y3        = projection->screenY + (u16)projection->extent.corner.y;
            _shelterB6NurserySetParticleCornerOffset(projection, halfDiagonal, nextCornerAngle);
            quad->x1 = projection->screenX + (u16)projection->extent.corner.x;
            quad->x2 = projection->screenX - (u16)projection->extent.corner.x;
            quad->y1 = projection->screenY - (u16)projection->extent.corner.y;
            quad->y2 = projection->screenY + (u16)projection->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Packed additive particle texture page: 4-bit indexed texels at VRAM X=704 words, Y=0 scanlines.
#define SPRITE_QUAD_TEXTURE_PAGE getTPage(0, GPU_BLEND_ADD, 704, 0)
/// Eight-frame particle palette: VRAM X=80 words, Y=270 scanlines.
#define SPRITE_QUAD_CLUT getClut(80, 270)
/// Texel width and horizontal stride of each cell in the eight-frame particle strip.
#define SPRITE_QUAD_CELL_WIDTH 32
/// Low three frame bits selecting the small particle strip's eight columns.
///
/// The particle frame index selects U=0..224 in 32-texel steps, with
/// V=0x88..0xA7. The task ends the animation after frame 7; this mask only
/// selects its column. The next drawer inclusion undefines this signed integer
/// constant; see `sprite_quad.h` for the column-mask contract.
#define SPRITE_QUAD_COLUMN_MASK 7
/// Inclusive top texel row of the particle strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x88
/// Inclusive bottom texel row of the eight-frame particle strip's square cells, relative to its texture page.
#define SPRITE_QUAD_BOTTOM_V (SPRITE_QUAD_TOP_V + SPRITE_QUAD_CELL_WIDTH - 1)
/// Perspective-sizing multiplier for the nursery particle sprite.
///
/// Uses the cell's inclusive 31-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
/// Keeps the eight-frame particle's projected depth unchanged for rejection, sizing and sorting.
///
/// Zero disables the unit increment, so the minimum of 65 tests SZ3 / 4
/// directly. The next sprite-quad fragment undefines this preprocessor switch.
#define SPRITE_QUAD_INCREMENT_DEPTH 0
/// Minimum accepted camera Z / 4 depth for the nursery particle sprite.
///
/// Positive signed integer constant, tested after the optional depth increment
/// and before perspective division. This instance disables the increment, so 65
/// is the projected depth itself. Undefined omits the depth test. The next
/// sprite-quad fragment clears the binding.
#define SPRITE_QUAD_MIN_OTZ 0x41
#include "../../shared/effects/sprite_quad_draw.inc.c"

void shelterB6NurserySparkShowerShardTask(Task* task)
{
    /// Spins one shard and advances it by its scaled Q12 direction.
    ///
    /// Arguments must be side-effect-free pointers, evaluated repeatedly; the
    /// output vector receives parent-coordinate units after multiplying the
    /// Q12 movement vector by scale's speed. Spin uses 4096 units per
    /// turn. Requires distinct live coordinate, work and output storage.
    /// Marks the composed cache dirty without refreshing it; drawing stays
    /// with the caller. Captures no locals and is undefined after this callback.
#define SHELTER_B6_NURSERY_ADVANCE_SHARD(coordNode, effectWork, displacementOut)             \
    do {                                                                                     \
        gfxRotMatrixXYZ(&(coordNode)->coord, &(effectWork)->pos, GRAPHICS_ROTATION_COMPOSE); \
        MatrixNormal(&(coordNode)->coord, &(coordNode)->coord);                              \
        gte_lddp((effectWork)->scale);                                                       \
        gte_ldsv(&(effectWork)->move);                                                       \
        gte_gpf12();                                                                         \
        gte_stsv(displacementOut);                                                           \
        (coordNode)->coord.t[0]  += (displacementOut)->vx;                                   \
        (coordNode)->coord.t[1]  += (displacementOut)->vy;                                   \
        (coordNode)->coord.t[2]  += (displacementOut)->vz;                                   \
        (coordNode)->composeStamp = GRAPHICS_COORD_DIRTY;                                    \
    } while (0)

    enum { SHARD_INITIALIZE  = 0,
           SHARD_RADIUS_MASK = 0xFFF,
           SHARD_GRAVITY_Q12 = 0x180 };

    SVECTOR     displacement;
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
    } else {
        if (task->state == SHARD_INITIALIZE) {
            RANDOM_LCG_ADVANCE();
            work->move.vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
            RANDOM_LCG_ADVANCE();
            work->move.vy = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
            RANDOM_LCG_ADVANCE();
            work->move.vz = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
            RANDOM_LCG_ADVANCE();
            work->scale = (RANDOM_LCG_DRAW() & 0x3F) + 0x40;
            work->angle = task->spawnArg1.value & SHARD_RADIUS_MASK;
            RANDOM_LCG_ADVANCE();
            work->period = (RANDOM_LCG_DRAW() & 0x7F) + 0x40;
            VectorNormalSS(&work->move, &work->move);
            RANDOM_LCG_ADVANCE();
            work->pos.vx = 0x100 - (RANDOM_LCG_DRAW() & 0x1FF);
            RANDOM_LCG_ADVANCE();
            work->pos.vy = 0x100 - (RANDOM_LCG_DRAW() & 0x1FF);
            RANDOM_LCG_ADVANCE();
            work->pos.vz        = 0x100 - (RANDOM_LCG_DRAW() & 0x1FF);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state++;
            return;
        }
        SHELTER_B6_NURSERY_ADVANCE_SHARD(coord, work, &displacement);
        _shelterB6NurseryDrawSparkShowerShard(coord, work->angle, work->period);
        // The release plane is Y=0 in the coordinate's parent frame.
        if (coord->coord.t[1] > 0) {
            effectKillTask(work, task);
        } else {
            work->move.vy += SHARD_GRAVITY_Q12;
        }
    }
}

#undef SHELTER_B6_NURSERY_ADVANCE_SHARD

/// Scales a shard's Q12 local corner and transforms it in place for projection.
///
/// `corner` is a word-aligned SVECTOR; its XYZ components start as a Q12 unit
/// direction and finish as signed coordinate units in `composedCoord->workm`'s
/// destination frame (view space for the shard task). `radius` is a signed
/// local-coordinate distance (0..4095 in that task). The coordinate is borrowed
/// read-only; its cached Q12 rotation and integer translation are used as
/// supplied, without recomposition. The corner must not overlap it.
/// GTE scaling and rotation each saturate through signed IR
/// results before the translation is added modulo 65536; the vector's fourth
/// halfword is untouched. GTE rotation and result registers change, while its
/// translation registers are preserved. Neither pointer is retained.
static inline void _shelterB6NurseryTransformShardCorner(SVECTOR* corner, const GfxCoord* composedCoord, s16 radius)
{
    // Keep scaling and rotation as separate GTE operations with s16 results.
    gte_lddp(radius);
    gte_ldsv(corner);
    gte_gpf12();
    gte_stsv(corner);
    gte_SetRotMatrix(&composedCoord->workm);
    gte_ldv0(corner);
    gte_rtv0();
    gte_stsv(corner);
    // The final translation preserves only the low 16 bits for the next RTPT.
    corner->vx = (u16)corner->vx + (u16)composedCoord->workm.t[0];
    corner->vy = (u16)corner->vy + (u16)composedCoord->workm.t[1];
    corner->vz = (u16)corner->vz + (u16)composedCoord->workm.t[2];
}

/// Draws a rotating spark-shower shard as a grey triangle in the coordinate's YZ plane.
///
/// `radius` is in local coordinate units; `shade` supplies the low byte of each
/// RGB channel. Requires a current cached matrix. Transforms three corners
/// through it, narrows their positions to s16, and projects with `GsWSMATRIX`.
/// Reserves 32 scratch-stack bytes and one `POLY_F3` before culling; visible
/// triangles also consume a blend packet, selecting average or additive blending
/// at random. Packets remain in the frame arena until GPU drawing completes.
static void _shelterB6NurseryDrawSparkShowerShard(const GfxCoord* coord, s16 radius, s16 shade)
{
    enum { SHARD_CORNER_ANGLE_STEP = 0x555 }; // One third of a 4096-unit turn, truncated.

    _ShelterB6NurseryTriScratch* projection;
    SVECTOR*                     corner;
    POLY_F3*                     triangle;
    s32                          cornerIndex;

    projection = SCRATCH_STACK_RESERVE_BLOCK(_ShelterB6NurseryTriScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Scale the local corners, then transform and narrow them for one RTPT.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(projection->corners); cornerIndex++) {
        corner     = &projection->corners[cornerIndex];
        corner->vx = 0;
        corner->vy = rsin(cornerIndex * SHARD_CORNER_ANGLE_STEP);
        corner->vz = rcos(cornerIndex * SHARD_CORNER_ANGLE_STEP);
        _shelterB6NurseryTransformShardCorner(corner, coord, radius);
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv3(&projection->corners[0], &projection->corners[1], &projection->corners[2]);
    gte_rtpt();
    triangle       = gGpuPrimCursor;
    gGpuPrimCursor = triangle + 1;
    setPolyF3(triangle);
    gte_stsxy3(&triangle->x0, &triangle->x1, &triangle->x2);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->otz);
        setRGB0(triangle, shade, shade, shade);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->otz),
                triangle);
        RANDOM_LCG_ADVANCE();
        gpuSetPrimitiveBlendMode(triangle, RANDOM_LCG_DRAW() & 1, projection->otz);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_ShelterB6NurseryTriScratch);
}

void shelterB6NurserySetEffectCues(s32 fastGlintPulse, s32 sparkShowerScale)
{
    _gShelterB6NurseryEffectCues.fastGlintPulse   = fastGlintPulse;
    _gShelterB6NurseryEffectCues.sparkShowerScale = sparkShowerScale;
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelterB6NurseryRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelterB6NurseryRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelterB6NurseryRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
