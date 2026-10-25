#include "common.h"

#include "main/areas.h"
#include "main/task_types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
/// Defeat/departure phase (0 waiting, 1 defeat scene running, 2 departure started).
///
/// Initialization resets it; the defeat message sets 1. In placement variant
/// 1, an idle event interpreter permits departure and changes it to 2, preventing
/// another start. Preserve signed-word storage and this first BSS declaration.
static s32 _gShelterB6TrainingRoomDefeatDeparturePhase;

/// Borrowed player head-aim controller task, or NULL when no controller is tracked.
///
/// Starting publishes the new task even if it replaces an existing handle.
/// Room initialization clears the handle without releasing a task; commands
/// fade the tracked controller or release it and clear the handle. Keep the
/// tracked task live until control releases it.
static Task* _gShelterB6TrainingRoomPlayerHeadAimTask;

#include "rooms/shelter_b6_training_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "shelter_b6_training_room_private.h"

#include "actors/actor_105100.h"
#include "actors/actor_205200.h"
#include "actors/actor_350700.h"

#include "gameplay/companion_load.h"
#include "gameplay/area.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/sound_ids.h"

#include "mapui/map_neo_ark.h"

static void _shelterB6TrainingRoomClearPlayerHeadAimTask(s32 unusedArg);
static void _shelterB6TrainingRoomInitializeRoom(Task* task);
static void _shelterB6TrainingRoomUpdateRoom(Task* unusedTask);

// The defeat notification is latched until its scene ends and departure starts.
enum {
    SHELTER_B6_TRAINING_ROOM_WAITING_FOR_DEFEAT   = 0,
    SHELTER_B6_TRAINING_ROOM_DEFEAT_SCENE_RUNNING = 1,
    SHELTER_B6_TRAINING_ROOM_DEPARTURE_STARTED    = 2
};

/// Sets the GPU mask bit on every RGB16 pixel in the complete decoded image workspace.
///
/// Requires exclusive writable access to all 320x240 decoded pixels; no decode
/// or VLC-table build may overlap this pass. The u16 pointer is the pixel view
/// of the word-packed workspace. RGB bits remain intact and storage is borrowed.
static inline void _shelterB6TrainingRoomMaskBackdrop(void)
{
    u16* imagePixel;
    s32  pixelIndex;

    // The decoder stores two RGB16 pixels per word; mask both halfwords.
    imagePixel = (u16*)gFsImageWorkspacePtr;
    pixelIndex = 0;
    do {
        *imagePixel |= FILE_SYSTEM_IMAGE_PIXEL_MASK;
        pixelIndex  += 1;
        imagePixel  += 1;
    } while (pixelIndex <= (s32)(sizeof(*gFsImageWorkspacePtr) / sizeof(*imagePixel)) - 1);
}

// Texture placement and source-element splits for the destroyed-part overlays.
enum {
    SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT         = getClut(0, 255),
    SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR     = 128,
    SHELTER_B6_TRAINING_ROOM_VIEW3_PART1_SPRITES      = 12,
    SHELTER_B6_TRAINING_ROOM_VIEW7_PART0_SPRITES      = 14
};

u8* gShelterB6TrainingRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB6TrainingRoomRoomViewCounts[2] = { SHELTER_B6_TRAINING_ROOM_VIEW_COUNT, 0 };

DirectionWarpEntry gShelterB6TrainingRoomWarpEntries[2] = {
    {
        .player         = { { .word = 1024 }, 500, 0, 1500 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 1024 }, 500, 0, 1500 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_TRAINING_ROOM, 5),
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { { .word = 3072 }, 4500, 0, 9390 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 3072 }, 4500, 0, 9390 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 4,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Q12 room-space normals borrowed by the training-room collision grid.
static SVECTOR _gShelterB6TrainingRoomCollisionGridNormals[] = {
#include "assets/shelter_b6_training_room_collision_07174_normals.inc"
};

/// Room-space collision vertices, including the raised platform, in game units.
static SVECTOR _gShelterB6TrainingRoomCollisionGridVerts[] = {
#include "assets/shelter_b6_training_room_collision_07174_verts.inc"
};

/// Wall, floor and platform faces indexing this grid's vertex and normal pools.
static WorldCollisionGridFace _gShelterB6TrainingRoomCollisionGridFaces[] = {
#include "assets/shelter_b6_training_room_collision_07174_faces.inc"
};

/// Six signed face-index lists, each terminated by WORLD_COLLISION_GRID_CELL_END.
static s16 _gShelterB6TrainingRoomCollisionGridCells[] = {
#include "assets/shelter_b6_training_room_collision_07174_cells.inc"
};

/// Address of a cell's terminated face list at a halfword offset in this grid.
///
/// Captures the overlay-lifetime signed-halfword pool. The pure integer offset
/// is evaluated once and must select a list start within that pool; the generated
/// table supplies 0, 14, 32, 50, 60 and 70. It is neither a byte offset nor a
/// cell-directory index. The collisionSource manifest's cell_ref_macro selects
/// this binding for the next generated table, after which it is undefined.
#define SHELTER_B6_TRAINING_ROOM_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB6TrainingRoomCollisionGridCells[(halfwordOffset)])
/// X-major pointers to the grid's two-by-three cell lists.
static s16* _gShelterB6TrainingRoomCollisionGridTable[] = {
#include "assets/shelter_b6_training_room_collision_07174_table.inc"
};
#undef SHELTER_B6_TRAINING_ROOM_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB6TrainingRoomCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB6TrainingRoomCollisionGridNormals,
    .vertices    = _gShelterB6TrainingRoomCollisionGridVerts,
    .faces       = _gShelterB6TrainingRoomCollisionGridFaces,
    .cellFaceIds = _gShelterB6TrainingRoomCollisionGridTable,
    .xBias       = 0,
    .zBias       = -750,
    .cellCountX  = 2,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB6TrainingRoomCollisionGridFaces),
};

ViewCamera gShelterB6TrainingRoomViewCameras[SHELTER_B6_TRAINING_ROOM_VIEW_COUNT] = {
    { .transform = { .m = { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, .t = { -2500, 0x61A8, -5250 } }, .screenDistance = 380 },
    { .transform = { .m = { { -4058, 0, 553 }, { 56, 4074, 413 }, { -550, 417, -4037 } }, .t = { -3120, 1960, -5900 } }, .screenDistance = 257 },
    { .transform = { .m = { { 4069, 0, 468 }, { -98, 4004, 855 }, { -457, -861, 3978 } }, .t = { -3190, 870, 2460 } }, .screenDistance = 257 },
    { .transform = { .m = { { 4064, 0, 507 }, { -119, 3980, 959 }, { -492, -967, 3949 } }, .t = { -3240, 820, -590 } }, .screenDistance = 257 },
    { .transform = { .m = { { 4078, 0, 377 }, { -7, 4095, 76 }, { -376, -77, 4077 } }, .t = { -3110, 1500, -4690 } }, .screenDistance = 257 },
    { .transform = { .m = { { 831, 0, 4010 }, { 365, 4078, -75 }, { -3994, 373, 828 } }, .t = { -4510, 1400, -8940 } }, .screenDistance = 289 },
    { .transform = { .m = { { -4087, 0, 257 }, { 5, 4095, 80 }, { -257, 80, -4087 } }, .t = { -2730, 1400, -0x2846 } }, .screenDistance = 329 },
    { .transform = { .m = { { 4085, 0, -286 }, { -138, 3587, -1972 }, { 251, 1977, 3578 } }, .t = { -2490, 2380, -6300 } }, .screenDistance = 329 },
};

/// Empty mapped-view-1 list retaining decoded backdrop strips and its terminator.
static SpriteBatch _gShelterB6TrainingRoomView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty mapped-view-2 list retaining decoded backdrop strips and its terminator.
static SpriteBatch _gShelterB6TrainingRoomView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Destroyed-part rectangles for mapped view 3, in pixels and unscaled sorting depth.
///
/// Elements 0..11 show part slot 1, elements 12..27 show part slot 0.
static SpriteSource _gShelterB6TrainingRoomView3SpriteSources[] = {
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 16, 24 } }, 112, 64, 1565, { .fields = { 96, 120 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 16, 16 } }, 112, 88, 1359, { .fields = { 96, 240 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 16, 24 } }, 104, -24, 1567, { .fields = { 96, 96 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 16, 32 } }, 104, 0, 1707, { .fields = { 112, 80 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 16, 32 } }, 104, 32, 1697, { .fields = { 112, 144 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, 104, 64, 1684, { .fields = { 120, 112 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, 96, 64, 1688, { .fields = { 120, 176 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, 96, 32, 1731, { .fields = { 120, 208 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, 96, 0, 1775, { .fields = { 104, 0 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, 96, -32, 1693, { .fields = { 112, 176 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, -24, 1821, { .fields = { 96, 192 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 16 } }, 104, -40, 1515, { .fields = { 112, 240 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 16, 40 } }, -112, -32, 1632, { .fields = { 112, 40 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 16, 40 } }, -112, 8, 1746, { .fields = { 112, 0 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 16, 24 } }, -112, 48, 1711, { .fields = { 96, 144 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 16, 24 } }, -112, 72, 1577, { .fields = { 96, 168 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 24 } }, -120, 48, 1430, { .fields = { 104, 192 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 24 } }, -120, 72, 1405, { .fields = { 96, 0 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, -96, -32, 1744, { .fields = { 104, 64 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, -96, 0, 1906, { .fields = { 104, 32 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, -96, 32, 1867, { .fields = { 112, 112 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, -96, 64, 1834, { .fields = { 112, 208 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 24 } }, -88, -24, 1866, { .fields = { 96, 48 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 24 } }, -88, 0, 1934, { .fields = { 96, 72 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 24 } }, -88, 24, 1895, { .fields = { 96, 24 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 24 } }, -88, 48, 1859, { .fields = { 104, 216 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 16 } }, -88, 72, 1821, { .fields = { 96, 208 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, -16, 1951, { .fields = { 120, 240 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped-view-3 backdrop selector, mutable destroyed-part ranges and terminator.
///
/// Batch 1 selects part slot 1; batch 2 selects part slot 0. The leading
/// zero-count batch retains the decoded backdrop. Both ranges fit all 28 sources.
static SpriteBatch _gShelterB6TrainingRoomView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, SHELTER_B6_TRAINING_ROOM_VIEW3_PART1_SPRITES, 0, 0, { 1, 0 } },
    { SHELTER_B6_TRAINING_ROOM_VIEW3_PART1_SPRITES, ARRAY_SIZE(_gShelterB6TrainingRoomView3SpriteSources) - SHELTER_B6_TRAINING_ROOM_VIEW3_PART1_SPRITES, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty mapped-view-4 list retaining decoded backdrop strips and its terminator.
static SpriteBatch _gShelterB6TrainingRoomView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty mapped-view-5 list retaining decoded backdrop strips and its terminator.
static SpriteBatch _gShelterB6TrainingRoomView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty mapped-view-6 list retaining decoded backdrop strips and its terminator.
static SpriteBatch _gShelterB6TrainingRoomView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Destroyed-part rectangles for mapped view 7, in pixels and unscaled sorting depth.
///
/// Elements 0..13 show part slot 0, elements 14..27 show part slot 1.
static SpriteSource _gShelterB6TrainingRoomView7SpriteSources[] = {
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 16, 40 } }, 128, -80, 1589, { .fields = { 104, 200 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 16, 40 } }, 128, -40, 1643, { .fields = { 96, 0 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 16, 40 } }, 128, 0, 1643, { .fields = { 96, 40 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 16, 40 } }, 128, 40, 1514, { .fields = { 104, 160 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, 120, -72, 1711, { .fields = { 88, 0 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 40 } }, 120, -40, 1762, { .fields = { 96, 80 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 40 } }, 120, 0, 1758, { .fields = { 96, 120 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, 120, 40, 1650, { .fields = { 96, 224 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, 112, -72, 1808, { .fields = { 88, 64 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 40 } }, 112, -40, 1832, { .fields = { 112, 120 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 40 } }, 112, 0, 1836, { .fields = { 120, 128 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, 112, 40, 1680, { .fields = { 88, 32 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 24 } }, 104, 40, 1784, { .fields = { 88, 96 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 16 } }, 104, -72, 1806, { .fields = { 88, 120 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 40 } }, -160, -88, 1507, { .fields = { 112, 80 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 40 } }, -160, -48, 1606, { .fields = { 120, 208 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 40 } }, -160, -8, 1619, { .fields = { 112, 0 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 48 } }, -160, 32, 1589, { .fields = { 120, 0 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, -152, -80, 1616, { .fields = { 96, 192 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 40 } }, -152, -48, 1708, { .fields = { 120, 168 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 40 } }, -152, -8, 1708, { .fields = { 104, 120 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 40 } }, -152, 32, 1689, { .fields = { 104, 80 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 32 } }, -144, -80, 1680, { .fields = { 96, 160 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, -48, 1743, { .fields = { 112, 40 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, -8, 1747, { .fields = { 120, 88 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, 32, 1752, { .fields = { 120, 48 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 16 } }, -136, 48, 1722, { .fields = { 104, 240 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B6_TRAINING_ROOM_PART_SPRITE_TEXTURE_PAGE, SHELTER_B6_TRAINING_ROOM_PART_SPRITE_CLUT, { .fields = { 8, 16 } }, -136, -72, 1742, { .fields = { 112, 240 } }, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_TRAINING_ROOM_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped-view-7 backdrop selector, mutable destroyed-part ranges and terminator.
///
/// Batch 1 selects part slot 0; batch 2 selects part slot 1. The leading
/// zero-count batch retains the decoded backdrop. Both ranges fit all 28 sources.
static SpriteBatch _gShelterB6TrainingRoomView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, SHELTER_B6_TRAINING_ROOM_VIEW7_PART0_SPRITES, 0, 0, { 1, 0 } },
    { SHELTER_B6_TRAINING_ROOM_VIEW7_PART0_SPRITES, ARRAY_SIZE(_gShelterB6TrainingRoomView7SpriteSources) - SHELTER_B6_TRAINING_ROOM_VIEW7_PART0_SPRITES, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty mapped-view-8 list retaining decoded backdrop strips and its terminator.
static SpriteBatch _gShelterB6TrainingRoomView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB6TrainingRoomSpriteViews[SHELTER_B6_TRAINING_ROOM_VIEW_COUNT] = {
    { { .empty = _gShelterB6TrainingRoomView1SpriteBatches }, _gShelterB6TrainingRoomView1SpriteBatches, NULL },
    { { .empty = _gShelterB6TrainingRoomView2SpriteBatches }, _gShelterB6TrainingRoomView2SpriteBatches, NULL },
    { { .elements = _gShelterB6TrainingRoomView3SpriteSources }, _gShelterB6TrainingRoomView3SpriteBatches, NULL },
    { { .empty = _gShelterB6TrainingRoomView4SpriteBatches }, _gShelterB6TrainingRoomView4SpriteBatches, NULL },
    { { .empty = _gShelterB6TrainingRoomView5SpriteBatches }, _gShelterB6TrainingRoomView5SpriteBatches, NULL },
    { { .empty = _gShelterB6TrainingRoomView6SpriteBatches }, _gShelterB6TrainingRoomView6SpriteBatches, NULL },
    { { .elements = _gShelterB6TrainingRoomView7SpriteSources }, _gShelterB6TrainingRoomView7SpriteBatches, NULL },
    { { .empty = _gShelterB6TrainingRoomView8SpriteBatches }, _gShelterB6TrainingRoomView8SpriteBatches, NULL },
};

/// Twelve room-space point lights, writable for transform composition and attenuation.
static WorldCoordPointLight _gShelterB6TrainingRoomPointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2500, -2750, 7500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 1638, 1392 }, { 0, 0 } }, 3000, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2500, -2750, 3000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 1638, 1392 }, { 0, 0 } }, 3000, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 250, -2000, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 2457, 1228 }, { 0, 0 } }, 750, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4750, -2000, 9500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 2457, 1228 }, { 0, 0 } }, 750, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2500, -2750, 3000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1310, 1228, 983 }, { 0, 0 } }, 1000, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2500, -2750, 7500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1310, 1228, 983 }, { 0, 0 } }, 1000, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 250, -2250, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 2457, 1228 }, { 0, 0 } }, 750, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4750, -2250, 9500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 2457, 1228 }, { 0, 0 } }, 750, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1030, -1417, 0x2710 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 245, 1638, 1474 }, { 0, 0 } }, 100, 700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2070, -1417, 0x2742 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 245, 1638, 1474 }, { 0, 0 } }, 100, 600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 550, -1417, 0x2710 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 245, 1638, 1474 }, { 0, 0 } }, 100, 600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1785, -1417, 0x27A6 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1474, 245, 0 }, { 0, 0 } }, 100, 400 },
};

/// Twelve room-space cone lights with Q12 axes and 375-unit full openings (4096 per turn).
static WorldCoordSpotLight _gShelterB6TrainingRoomConeLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 200, -5750, 740 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 4014, 3686 }, { 0, 0 } }, { 564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4800, -5750, 6240 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 4014, 3686 }, { 0, 0 } }, { -564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4800, -5750, 7740 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 4014, 3686 }, { 0, 0 } }, { -564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4800, -5750, 9740 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 4014, 3686 }, { 0, 0 } }, { -564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 200, -5750, 2740 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 4014, 3686 }, { 0, 0 } }, { 564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 200, -5750, 4240 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 4014, 3686 }, { 0, 0 } }, { 564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4800, -5750, 740 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 4014, 3686 }, { 0, 0 } }, { -564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4800, -5750, 2740 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 4014, 3686 }, { 0, 0 } }, { -564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4800, -5750, 4240 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 4014, 3686 }, { 0, 0 } }, { -564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 200, -5750, 6240 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 4014, 3686 }, { 0, 0 } }, { 564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 200, -5750, 7740 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 4014, 3686 }, { 0, 0 } }, { 564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 200, -5750, 9740 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 4014, 3686 }, { 0, 0 } }, { 564, 4056, 7, 0 }, 5000, 6500, 375 },
};

WorldCoordRoomLights gShelterB6TrainingRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB6TrainingRoomPointLights),
    .pointLights           = _gShelterB6TrainingRoomPointLights,
    .coneLightCount        = ARRAY_SIZE(_gShelterB6TrainingRoomConeLights),
    .coneLights            = _gShelterB6TrainingRoomConeLights,
};

WorldCollisionTrigger gShelterB6TrainingRoomViewBoundaryTriggers[7] = {
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2497, -2912, 4352, 0 }, .vertices = { { 3360, -3312, 0, 0 }, { -3360, -3312, 0, 0 }, { 3360, 3312, 0, 0 }, { -3360, 3312, 0, 0 } }, .normal = { 0, 0, 4106, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 4692, .control = 0, .parameter0 = 4, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2480, -2929, 4480, 0 }, .vertices = { { -3248, -3328, 0, 0 }, { 3248, -3328, 0, 0 }, { -3248, 3328, 0, 0 }, { 3248, 3328, 0, 0 } }, .normal = { 0, 0, -4101, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 4636, .control = 0, .parameter0 = 3, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 4753, -2401, 8944, 0 }, .vertices = { { -2390, -3328, -575, 0 }, { 2384, -3328, 569, 0 }, { -2390, 3328, -575, 0 }, { 2384, 3328, 569, 0 } }, .normal = { 955, 0, -3989, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 4127, .control = 0, .parameter0 = 4, .parameter1 = 5, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 4800, -2240, 8785, 0 }, .vertices = { { 2446, -3312, 610, 0 }, { -2448, -3312, -612, 0 }, { 2446, 3312, 610, 0 }, { -2448, 3312, -612, 0 } }, .normal = { -995, 0, 3980, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 4159, .control = 0, .parameter0 = 5, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 173, -2304, 8607, 0 }, .vertices = { { 2351, -3312, -432, 0 }, { -2352, -3312, 432, 0 }, { 2351, 3312, -432, 0 }, { -2352, 3312, 432, 0 } }, .normal = { 740, 0, 4029, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 4079, .control = 0, .parameter0 = 5, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 186, -2273, 8811, 0 }, .vertices = { { -2336, -3328, 448, 0 }, { 2335, -3328, -449, 0 }, { -2336, 3328, 448, 0 }, { 2335, 3328, -449, 0 } }, .normal = { -774, 0, -4030, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 4087, .control = 0, .parameter0 = 4, .parameter1 = 5, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2479, -2144, 2047, 0 }, .vertices = { { -4031, -3312, -3, 0 }, { 4031, -3312, 3, 0 }, { -4031, 3312, -3, 0 }, { 4031, 3312, 3, 0 } }, .normal = { 2, 0, -4097, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 5196, .control = 0, .parameter0 = 2, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, .hit = 0 },
};

/// Training-room variant-1 recipes for Masked Eve, Kyle, Stinger and Eve's controller/body.
///
/// Keys 131/140 load 650700/350700 and select Masked Eve/Kyle descriptor zero;
/// keys 51/52/60 load 105100/205200/206000 and select their body/controller
/// recipes at index zero. The base-60 file requires key 131's base placement.
/// `AREA_PLACEMENT_END` ends the list. Keep room recipes and map placements
/// loaded during loading, spawning and model-policy updates; retain each
/// actor overlay throughout its tasks' lifetime.
static AreaResource _gShelterB6TrainingRoomVariant1Resources[6] = {
    { .entryId = 131, .fileNumber = 507, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_60, .taskTable = &gActor350700MaskedEveReverseWalkTaskDesc },
    { .entryId = 140, .fileNumber = 507, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskTable = gActor350700KyleMadiganTaskDescriptors },
    { .entryId = 51, .fileNumber = 51, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskTable = gActor105100TaskDescriptors },
    { .entryId = 52, .fileNumber = 52, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskTable = gActor205200ControllerPartTasks },
    { .entryId = 60, .fileNumber = 60, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskTable = &gActor205200BodyTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gShelterB6TrainingRoomAreaVariants[SHELTER_B6_TRAINING_ROOM_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapNeoArkTrainingRoomDefaultPlacements, .resources = _gShelterB6TrainingRoomVariant1Resources },
};

/// Action payloads: packed departure/arrival slots and room-local selectors.
enum {
    SHELTER_B6_TRAINING_ROOM_TRIGGER_WARP_TO_B6_CORRIDOR = (1 << 4) | 2,
    SHELTER_B6_TRAINING_ROOM_TRIGGER_WARP_TO_B6_NURSERY  = (2 << 4) | 1,
    SHELTER_B6_TRAINING_ROOM_TRIGGER_CAP_COMMAND_1       = 1,
    SHELTER_B6_TRAINING_ROOM_TRIGGER_COMMAND_PART_0      = 5,
    SHELTER_B6_TRAINING_ROOM_TRIGGER_COMMAND_PART_1      = 6,
};

WorldCollisionTrigger gShelterB6TrainingRoomActionTriggers[5] = {
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { -1056, -48, 2416, 0 }, .vertices = { { -352, 0, -592, 0 }, { 352, 0, -592, 0 }, { -352, 0, 592, 0 }, { 352, 0, 592, 0 } }, .normal = { 0, 4107, 0, 0 }, .facingNormal = { 4096, 0, 0, 0 }, .radius = 686, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP, .parameter0 = GAME_AREA_SHELTER_B6_CORRIDOR, .parameter1 = SHELTER_B6_TRAINING_ROOM_TRIGGER_WARP_TO_B6_CORRIDOR, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 4608, -48, 9600, 0 }, .vertices = { { -352, 0, -592, 0 }, { 352, 0, -592, 0 }, { -352, 0, 592, 0 }, { 352, 0, 592, 0 } }, .normal = { 0, 4107, 0, 0 }, .facingNormal = { -4096, 0, 0, 0 }, .radius = 686, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP, .parameter0 = GAME_AREA_SHELTER_B6_NURSERY, .parameter1 = SHELTER_B6_TRAINING_ROOM_TRIGGER_WARP_TO_B6_NURSERY, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 223, -64, 1825, 0 }, .vertices = { { -352, 0, -592, 0 }, { 352, 0, -592, 0 }, { -352, 0, 592, 0 }, { 352, 0, 592, 0 } }, .normal = { 0, 4107, 0, 0 }, .facingNormal = { 4096, 0, 0, 0 }, .radius = 686, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = SHELTER_B6_TRAINING_ROOM_TRIGGER_CAP_COMMAND_1, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 224, -64, 4368, 0 }, .vertices = { { -352, 0, -1088, 0 }, { 352, 0, -1088, 0 }, { -352, 0, 1088, 0 }, { 352, 0, 1088, 0 } }, .normal = { 0, 4103, 0, 0 }, .facingNormal = { 4096, 0, 0, 0 }, .radius = 1137, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = SHELTER_B6_TRAINING_ROOM_TRIGGER_COMMAND_PART_0, .parameter1 = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 4671, -64, 4352, 0 }, .vertices = { { -352, 0, -1088, 0 }, { 352, 0, -1088, 0 }, { -352, 0, 1088, 0 }, { 352, 0, 1088, 0 } }, .normal = { 0, 4103, 0, 0 }, .facingNormal = { -4091, 0, 201, 0 }, .radius = 1137, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = SHELTER_B6_TRAINING_ROOM_TRIGGER_COMMAND_PART_1, .parameter1 = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, .hit = 0 },
};

WorldCoordRoomAmbientEntry gShelterB6TrainingRoomAmbientByView[SHELTER_B6_TRAINING_ROOM_VIEW_COUNT + 1] = {
    { .viewCount = ARRAY_SIZE(gShelterB6TrainingRoomAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 457, 493, 457, 475 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Type-1 footstep cue bases for surface class 1's outer floor faces.
///
/// Walk and scripted jump select entries 89/90; run selects 91/92.
/// The second entry is animation cue 1; companion playback adds 100.
static WorldCollisionFootstepSounds _gShelterB6TrainingRoomSurface1FootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | 89,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | 91,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | 89,
};

/// Type-1 footstep cue bases for surface class 2's two narrow floor strips.
///
/// Walk and scripted jump select entries 93/94; run selects 95/96.
/// The second entry is animation cue 1; companion playback adds 100.
static WorldCollisionFootstepSounds _gShelterB6TrainingRoomSurface2FootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | 93,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | 95,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | 93,
};

/// Blocking, impact-enabled surface with pushback and no footsteps (classes 0, 3..7).
static WorldCollisionSurfaceProperties _gShelterB6TrainingRoomDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface class 1's blocking, weapon-impact, pushback and footstep policies.
static WorldCollisionSurfaceProperties _gShelterB6TrainingRoomSurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB6TrainingRoomSurface1FootstepSounds,
};

/// Surface class 2's blocking, weapon-impact, pushback and footstep policies.
static WorldCollisionSurfaceProperties _gShelterB6TrainingRoomSurface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB6TrainingRoomSurface2FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB6TrainingRoomSurfaceProperties[8] = {
    &_gShelterB6TrainingRoomDefaultSurfaceProperties,
    &_gShelterB6TrainingRoomSurface1Properties,
    &_gShelterB6TrainingRoomSurface2Properties,
    &_gShelterB6TrainingRoomDefaultSurfaceProperties,
    &_gShelterB6TrainingRoomDefaultSurfaceProperties,
    &_gShelterB6TrainingRoomDefaultSurfaceProperties,
    &_gShelterB6TrainingRoomDefaultSurfaceProperties,
    &_gShelterB6TrainingRoomDefaultSurfaceProperties,
};

s8 gShelterB6TrainingRoomRingTexturePhases[SHELTER_B6_TRAINING_ROOM_BAND_COUNT][SHELTER_B6_TRAINING_ROOM_RING_PHASE_ROW_STRIDE];

const GfxCoord* gShelterB6TrainingRoomSummonBeamOrigin;

const GfxCoord* gShelterB6TrainingRoomEnergyArcAnchor;

u16 gShelterB6TrainingRoomHitFlashGeneration;

s32 shelterB6TrainingRoomRefuseKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 shelterB6TrainingRoomResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_B6_TRAINING_ROOM_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return SHELTER_B6_TRAINING_ROOM_TRANSITION_ALLOWED;
}

s32 shelterB6TrainingRoomHandlePartCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_COMMAND_PART_0        = 5,
        SHELTER_B6_TRAINING_ROOM_COMMAND_PART_1        = 6,
        SHELTER_B6_TRAINING_ROOM_COMMAND_CORRIDOR_PART = 4,
        SHELTER_B6_TRAINING_ROOM_CAP_PART_0_BATTLE     = 5,
        SHELTER_B6_TRAINING_ROOM_CAP_PART_1_BATTLE     = 6,
        SHELTER_B6_TRAINING_ROOM_CAP_PART_0_INACTIVE   = 7,
        SHELTER_B6_TRAINING_ROOM_CAP_PART_1_INACTIVE   = 8,
        SHELTER_B6_TRAINING_ROOM_CAP_CORRIDOR_BATTLE   = 4,
        SHELTER_B6_TRAINING_ROOM_CAP_CORRIDOR_DOWN     = 7,
        SHELTER_B6_TRAINING_ROOM_CAP_CORRIDOR_IDLE     = 10
    };

    switch (command) {
        case SHELTER_B6_TRAINING_ROOM_COMMAND_PART_0:
            if (gameFlagGetNibble(GAME_FLAG_153) != 0) {
                capRunCommandWithTransition(SHELTER_B6_TRAINING_ROOM_CAP_PART_0_INACTIVE);
            } else if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                capRunCommandWithTransition(SHELTER_B6_TRAINING_ROOM_CAP_PART_0_BATTLE);
            } else {
                capRunCommandWithTransition(SHELTER_B6_TRAINING_ROOM_CAP_PART_0_INACTIVE);
            }
            break;
        case SHELTER_B6_TRAINING_ROOM_COMMAND_PART_1:
            if (gameFlagGetNibble(GAME_FLAG_154) != 0) {
                capRunCommandWithTransition(SHELTER_B6_TRAINING_ROOM_CAP_PART_1_INACTIVE);
            } else if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                capRunCommandWithTransition(SHELTER_B6_TRAINING_ROOM_CAP_PART_1_BATTLE);
            } else {
                capRunCommandWithTransition(SHELTER_B6_TRAINING_ROOM_CAP_PART_1_INACTIVE);
            }
            break;
        case SHELTER_B6_TRAINING_ROOM_COMMAND_CORRIDOR_PART:
            if (gameFlagGetNibble(GAME_FLAG_B6_CORRIDOR_EVE_PART_2_DOWN) != 0) {
                capRunCommandWithTransition(SHELTER_B6_TRAINING_ROOM_CAP_CORRIDOR_DOWN);
            } else if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
                capRunCommandWithTransition(SHELTER_B6_TRAINING_ROOM_CAP_CORRIDOR_IDLE);
            } else {
                capRunCommandWithTransition(SHELTER_B6_TRAINING_ROOM_CAP_CORRIDOR_BATTLE);
            }
            break;
    }
    return 0;
}

s32 shelterB6TrainingRoomIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    return 0;
}

s32 shelterB6TrainingRoomStartDefeatScene(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    enum { SHELTER_B6_TRAINING_ROOM_DEFEAT_SCENE_ACTOR_INDEX = 3 };

    // Keep the normal and skipped scene paths paired before commanding the actor.
    gGameSession->flowFlags |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
    evsStartScriptWithSkip(gShelterB6TrainingRoomDefeatScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gShelterB6TrainingRoomDefeatSkipScript);
    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(SHELTER_B6_TRAINING_ROOM_DEFEAT_SCENE_ACTOR_INDEX), ACTOR_COMMAND_MESSAGE_APPLY, &gShelterB6TrainingRoomControllerStopCommand, 0);
    _gShelterB6TrainingRoomDefeatDeparturePhase = SHELTER_B6_TRAINING_ROOM_DEFEAT_SCENE_RUNNING;
    return 0;
}

/// Initializes the room controller and prepares its backdrop for masked RGB16 decoding.
///
/// Requires a complete decoded image workspace. Sets every pixel's GPU mask bit,
/// selects the companion schedule and scene music entry, clears the head-aim
/// handle without releasing a task, and advances from state 0 to state 1.
static void _shelterB6TrainingRoomInitializeRoom(Task* task)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_COMPANION_SCHEDULE = 1,
        SHELTER_B6_TRAINING_ROOM_SCENE_MUSIC_ENTRY  = 10
    };
    task->msgTable = gShelterB6TrainingRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _shelterB6TrainingRoomMaskBackdrop();
    gameFlagSetNibble(GAME_FLAG_COMPANION_3_SCHEDULE, SHELTER_B6_TRAINING_ROOM_COMPANION_SCHEDULE);
    gStageCountdownMusicEntry = SHELTER_B6_TRAINING_ROOM_SCENE_MUSIC_ENTRY;
    _shelterB6TrainingRoomClearPlayerHeadAimTask(0);
    task->state++;
    _gShelterB6TrainingRoomDefeatDeparturePhase = SHELTER_B6_TRAINING_ROOM_WAITING_FOR_DEFEAT;
}

/// Keeps RGB16 mask-bit decoding selected and starts departure after the defeat scene.
///
/// In placement variant 1, a latched defeat starts departure once the event
/// interpreter is idle. The latch then prevents another start. `unusedTask`
/// is retained for the room controller's state-callback signature.
static void _shelterB6TrainingRoomUpdateRoom(Task* unusedTask)
{
    enum { SHELTER_B6_TRAINING_ROOM_DEFAULT_VARIANT = 1 };
    u8 variant;

    gCdCmdQueue.imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
    variant                   = gGameSession->location.variant;
    // In the default variant, the variant byte also equals the running-scene latch.
    if (variant == SHELTER_B6_TRAINING_ROOM_DEFAULT_VARIANT && gGameSession->eventState == 0 && _gShelterB6TrainingRoomDefeatDeparturePhase == variant) {
        evsStartScript(gShelterB6TrainingRoomDepartureScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        _gShelterB6TrainingRoomDefeatDeparturePhase = SHELTER_B6_TRAINING_ROOM_DEPARTURE_STARTED;
    }
}

/// Room-controller callbacks: 0 initialize, 1 update departure progress, 2 release.
///
/// Dispatch copies all three pointers; state must be 0..2. There is no
/// terminator or bounds check; keep the room's callback code loaded.
static const TaskFuncTable3 _gShelterB6TrainingRoomRoomTaskStates = {
    { _shelterB6TrainingRoomInitializeRoom, _shelterB6TrainingRoomUpdateRoom, taskKill },
};

void shelterB6TrainingRoomRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gShelterB6TrainingRoomRoomTaskStates;
    states.funcs[task->state](task);
}

void shelterB6TrainingRoomStartPlayerHeadAim(void)
{
    _gShelterB6TrainingRoomPlayerHeadAimTask = taskSpawnFromTable(&gShelterB6TrainingRoomPlayerHeadAimTaskDesc, 0, 0, 0);
}

void shelterB6TrainingRoomControlPlayerHeadAim(s32 command)
{
    Task* headAimTask = _gShelterB6TrainingRoomPlayerHeadAimTask;

    if (headAimTask == NULL) {
        return;
    }
    switch (command) {
        case SHELTER_B6_TRAINING_ROOM_HEAD_AIM_FADE_OUT:
        case SHELTER_B6_TRAINING_ROOM_HEAD_AIM_FADE_IN:
            headAimTask->spawnArg1.value = command;
            break;
        default: {
            Task** headAimHandle = &_gShelterB6TrainingRoomPlayerHeadAimTask;

            taskKill(*headAimHandle);
            *headAimHandle = NULL;
            break;
        }
    }
}

void shelterB6TrainingRoomPlayerHeadAimTask(Task* task)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_HEAD_AIM_ACTIVE       = 0,
        SHELTER_B6_TRAINING_ROOM_HEAD_AIM_WEIGHT_STEP  = ONE / 16,
        SHELTER_B6_TRAINING_ROOM_HEAD_AIM_MAX_YAW      = ONE / 8,
        SHELTER_B6_TRAINING_ROOM_HEAD_AIM_MAX_PITCH    = ONE / 16,
        SHELTER_B6_TRAINING_ROOM_HEAD_AIM_TARGET_INDEX = 1
    };
    Enemy* targetEnemy;
    s16    blendWeight;

    if (gEvsPlaybackPaused == 0) {
        if (task->state == SHELTER_B6_TRAINING_ROOM_HEAD_AIM_ACTIVE) {
            // Preserve the wrapping halfword step before the signed endpoint tests.
            if (task->spawnArg1.value != 0) {
                blendWeight         = task->killCountdown + SHELTER_B6_TRAINING_ROOM_HEAD_AIM_WEIGHT_STEP;
                task->killCountdown = blendWeight;
                if (blendWeight >= ONE + 1) {
                    task->killCountdown = ONE;
                }
            } else {
                blendWeight         = task->killCountdown - SHELTER_B6_TRAINING_ROOM_HEAD_AIM_WEIGHT_STEP;
                task->killCountdown = blendWeight;
                if (blendWeight < 0) {
                    task->killCountdown = 0;
                }
            }
            targetEnemy = sceneFindEnemyByPlaceKey(gGameSession->location.area | ((gGameSession->location.stage << ENEMY_PLACE_STAGE_SHIFT) | (SHELTER_B6_TRAINING_ROOM_HEAD_AIM_TARGET_INDEX << ENEMY_PLACE_INDEX_SHIFT)));
            animationAimHeadAtTask(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), targetEnemy->task, SHELTER_B6_TRAINING_ROOM_HEAD_AIM_MAX_YAW, SHELTER_B6_TRAINING_ROOM_HEAD_AIM_MAX_PITCH, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}

void shelterB6TrainingRoomStartMovie(void)
{
    enum { SHELTER_B6_TRAINING_ROOM_MOVIE_LAUNCH_TASK = 0 };

    taskSpawnFromTable(gShelterB6TrainingRoomMovieTasks, SHELTER_B6_TRAINING_ROOM_MOVIE_LAUNCH_TASK, 0, 0);
}

void shelterB6TrainingRoomPrepareDefeatScene(s32 endDelayFrames)
{
    gGameSession->flowFlags |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
    if (endDelayFrames != 0) {
        gSceneCombatState.signals.bytes.endDelayFrames = endDelayFrames;
    }
}

void shelterB6TrainingRoomReloadNursery(void)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_NURSERY_ARRIVAL = 1,
        SHELTER_B6_TRAINING_ROOM_NURSERY_VARIANT = 1
    };

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_SHELTER_B6_NURSERY;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = SHELTER_B6_TRAINING_ROOM_NURSERY_ARRIVAL;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = SHELTER_B6_TRAINING_ROOM_NURSERY_VARIANT;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
}

void shelterB6TrainingRoomStopBattlePresentation(void)
{
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    roomEffectRequestCancelAll();
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
}

/// Clears the borrowed player head-aim task handle without destroying a task.
///
/// Used before any head-aim task is spawned during room initialization.
/// `unusedArg` has no effect.
static void _shelterB6TrainingRoomClearPlayerHeadAimTask(s32 unusedArg)
{
    _gShelterB6TrainingRoomPlayerHeadAimTask = NULL;
}
