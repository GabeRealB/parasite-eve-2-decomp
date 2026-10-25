#include "rooms/shelter_r49.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor_143900.h"

#include "gameplay/companion_load.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_types.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"

enum {
    SHELTER_R49_ARRIVAL_VIEW                = 2,
    SHELTER_R49_ARRIVAL_YAW                 = 1024, // Quarter turn; 4096 units per turn
    SHELTER_R49_GRID_CELL_COUNT_X           = 1,
    SHELTER_R49_GRID_CELL_COUNT_Z           = 1,
    SHELTER_R49_GRID_CELL_SIZE              = 30000,                  // Whole world-coordinate units
    SHELTER_R49_SCENE_ACTOR_ENTRY_ID        = 111,
    SHELTER_R49_SCENE_ACTOR_FILE_NUMBER     = 439,                    // Base-10 file family yields actor file 143900
    SHELTER_R49_SCENE_ACTOR_CLUT_ROW_OFFSET = 2,
    SHELTER_R49_VIEW2_TEXTURE_PAGE          = getTPage(1, 0, 960, 0), // 8-bit texture, blend mode 0
    SHELTER_R49_VIEW2_CLUT                  = getClut(0, 255),
    SHELTER_R49_SPRITE_COLOR_NEUTRAL        = 128,
    SHELTER_R49_FOOTSTEP_WALK_SCRIPT        = 45,
    SHELTER_R49_FOOTSTEP_RUN_SCRIPT         = 47,
};

static WorldCollisionGrid   _gShelterR49CollisionGrid;
static WorldCoordRoomLights _gShelterR49RoomLights;
static void                 _shelterR49PlayMovieTask(Task* movieTask);
static void                 _shelterR49EnterObservatoryTask(Task* transitionTask);

TaskDesc gShelterR49ObservatoryTransitionTasks[SHELTER_R49_OBSERVATORY_TASK_COUNT] = {
    [SHELTER_R49_OBSERVATORY_TRANSITION_TASK_INDEX] = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _shelterR49EnterObservatoryTask, { .value = 0 } },
    [SHELTER_R49_OBSERVATORY_MOVIE_TASK_INDEX]      = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _shelterR49PlayMovieTask, { .value = 0 } },
};

WorldCollisionRoomResources gShelterR49RoomCollisionResources[SHELTER_R49_ROOM_COUNT] = {
    { &_gShelterR49CollisionGrid, NULL, NULL, NULL },
};

u8* gShelterR49RoomViewMaps[SHELTER_R49_ROOM_COUNT] = {
    gViewIdentityMap,
};

ViewCount gShelterR49RoomViewCounts[SHELTER_R49_ROOM_COUNT] = { ARRAY_SIZE(gShelterR49ViewCameras) };

WorldCoordRoomLighting gShelterR49RoomLighting[SHELTER_R49_ROOM_COUNT] = {
    { &_gShelterR49RoomLights, NULL },
};

DirectionWarpEntry gShelterR49WarpEntries[1] = {
    { { { .word = SHELTER_R49_ARRIVAL_YAW }, 0, -10000, 0 }, { 0, 0, 0, 0 }, { { .word = SHELTER_R49_ARRIVAL_YAW }, 0, 0, 0 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, SHELTER_R49_ARRIVAL_VIEW, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterR49CollisionGridNormals[1] = {
#include "assets/shelter_r49_collision_004EC_normals.inc"
};

static SVECTOR _gShelterR49CollisionGridVerts[4] = {
#include "assets/shelter_r49_collision_004EC_verts.inc"
};

static WorldCollisionGridFace _gShelterR49CollisionGridFaces[1] = {
#include "assets/shelter_r49_collision_004EC_faces.inc"
};

static s16 _gShelterR49CollisionGridCells[2] = {
#include "assets/shelter_r49_collision_004EC_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterR49CollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..1
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_R49_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterR49CollisionGridCells[(halfwordOffset)])
static s16* _gShelterR49CollisionGridTable[SHELTER_R49_GRID_CELL_COUNT_X * SHELTER_R49_GRID_CELL_COUNT_Z] = {
#include "assets/shelter_r49_collision_004EC_table.inc"
};
#undef SHELTER_R49_CELL_FACE_LIST_AT_OFFSET

/// Floor quad at Y=0 spanning X/Z -4000..4000, with surface class 1.
///
/// One XZ cell selects face 0 through a signed list ending in -1. The pools
/// and this descriptor remain writable; activation binds the view coordinate.
static WorldCollisionGrid _gShelterR49CollisionGrid = {
    NULL,
    _gShelterR49CollisionGridNormals,
    _gShelterR49CollisionGridVerts,
    _gShelterR49CollisionGridFaces,
    _gShelterR49CollisionGridTable,
    4000,
    4000,
    SHELTER_R49_GRID_CELL_COUNT_X,
    SHELTER_R49_GRID_CELL_COUNT_Z,
    SHELTER_R49_GRID_CELL_SIZE,
    ARRAY_SIZE(_gShelterR49CollisionGridFaces),
};

ViewCamera gShelterR49ViewCameras[SHELTER_R49_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 13000, 0 } }, 257 },
    { { { { 3982, 0, -955 }, { 108, 4069, 451 }, { 949, -463, 3957 } }, { 1470, 1070, 2380 } }, 257 },
    { { { { 4057, 0, -561 }, { 28, 4090, 208 }, { 560, -210, 4051 } }, { 770, 1400, -190 } }, 541 },
};

/// View 1's empty sprite list, keeping decoded-image strips as the background.
static SpriteBatch _gShelterR49View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's fifteen textured rectangles, positioned in pixels and sorted by source depth.
///
/// Uses the 8-bit page at (960, 0) and CLUT at (0, 255). The first seven
/// rectangles have depth 0; the other eight sort with scene models at 500..1375.
static SpriteSource _gShelterR49View2SpriteSources[15] = {
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 24, 8 } }, -40, 24, 0, { .fields = { 80, 72 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 8, 16 } }, -40, 32, 0, { .fields = { 120, 224 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 24, 8 } }, 16, 32, 0, { .fields = { 72, 0 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 88, 8 } }, 8, 24, 0, { .fields = { 16, 64 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 48, 8 } }, 48, 32, 0, { .fields = { 56, 56 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 16, 8 } }, 80, 40, 0, { .fields = { 112, 240 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 16, 8 } }, 80, 96, 0, { .fields = { 112, 248 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 24, 80 } }, -16, 24, 500, { .fields = { 104, 0 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 16, 72 } }, -32, 32, 961, { .fields = { 112, 80 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 8, 56 } }, -40, 48, 1131, { .fields = { 96, 0 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 8, 72 } }, 8, 32, 1050, { .fields = { 120, 152 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 24, 64 } }, 16, 40, 1150, { .fields = { 88, 80 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 8, 72 } }, 40, 32, 1375, { .fields = { 112, 152 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 32, 64 } }, 48, 40, 1375, { .fields = { 80, 144 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
    { SHELTER_R49_VIEW2_TEXTURE_PAGE, SHELTER_R49_VIEW2_CLUT, { .fields = { 16, 48 } }, 80, 48, 1250, { .fields = { 96, 208 } }, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, SHELTER_R49_SPRITE_COLOR_NEUTRAL, 0 },
};

/// View 2's decoded-image background followed by the complete source-sprite range.
static SpriteBatch _gShelterR49View2SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterR49View2SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's empty sprite list, keeping decoded-image strips as the background.
static SpriteBatch _gShelterR49View3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterR49SpriteViews[SHELTER_R49_VIEW_COUNT] = {
    { { .empty = _gShelterR49View1SpriteBatches }, _gShelterR49View1SpriteBatches, NULL },
    { { .elements = _gShelterR49View2SpriteSources }, _gShelterR49View2SpriteBatches, NULL },
    { { .empty = _gShelterR49View3SpriteBatches }, _gShelterR49View3SpriteBatches, NULL },
};

/// One white point light at (0, -2500, 3000), contributing in every room view.
///
/// RGB is Q12 (2867 per channel); full strength extends to 6000 world units
/// and fades to zero at 7000. Room-light setup and queries update its transform and attenuation.
static WorldCoordPointLight _gShelterR49PointLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2500, 3000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 6000, 7000 },
};

/// The single room's mutable point-light collection, with no directional or cone lights.
static WorldCoordRoomLights _gShelterR49RoomLights = {
    0,
    NULL,
    ARRAY_SIZE(_gShelterR49PointLights),
    _gShelterR49PointLights,
    0,
    NULL,
};

/// Default-layout resource 111: actor file 143900 and its first scripted-walker descriptor.
///
/// The entry ID joins the placement table; `AREA_PLACEMENT_END` terminates the list.
static AreaResource _gShelterR49DefaultAreaResources[2] = {
    { SHELTER_R49_SCENE_ACTOR_ENTRY_ID, SHELTER_R49_SCENE_ACTOR_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor143900ScriptedWalkerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Default-layout walker at world (0, 0, 3210), facing yaw 0, with CLUT row offset +2.
///
/// Borrows the base actor file with no additional-file selector or texture-page
/// offset. The spawned actor may retain the placement; `AREA_PLACEMENT_END` ends the list.
static AreaPlacement _gShelterR49DefaultAreaPlacements[2] = {
    { SHELTER_R49_SCENE_ACTOR_ENTRY_ID, 0, 0, 0, 0, 3210, 0, 0, 0, SHELTER_R49_SCENE_ACTOR_CLUT_ROW_OFFSET, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterR49AreaVariants[SHELTER_R49_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { _gShelterR49DefaultAreaPlacements, _gShelterR49DefaultAreaResources },
};

/// Floor's type-1 cue-2 script requests: walk/jump entry 45 and run entry 47.
///
/// Cue 1 adds one; companion playback adds 100. Playback requires a loaded
/// type-1 bank; these requests do not load one. Surface records borrow this storage.
static WorldCollisionFootstepSounds _gShelterR49FootstepSounds = {
    SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_R49_FOOTSTEP_WALK_SCRIPT,
    SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_R49_FOOTSTEP_RUN_SCRIPT,
    SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_R49_FOOTSTEP_WALK_SCRIPT,
};

/// Blocking solid-surface policies without footstep cues, shared by classes other than 1.
static WorldCollisionSurfaceProperties _gShelterR49SilentSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Floor class 1's blocking solid-surface policies and borrowed footstep cues.
static WorldCollisionSurfaceProperties _gShelterR49FloorSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gShelterR49FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterR49SurfacePropertiesByClass[8] = {
    &_gShelterR49SilentSurfaceProperties,
    &_gShelterR49FloorSurfaceProperties,
    &_gShelterR49SilentSurfaceProperties,
    &_gShelterR49SilentSurfaceProperties,
    &_gShelterR49SilentSurfaceProperties,
    &_gShelterR49SilentSurfaceProperties,
    &_gShelterR49SilentSurfaceProperties,
    &_gShelterR49SilentSurfaceProperties,
};

/// Queues movie 100 for the current room, borrowing the loaded stream descriptors.
///
/// Requires a live session, prepared movie workspace, a loaded descriptor for
/// the current room/sub-ID 0 and room in the CD ring. Lookup must yield slot
/// 0..14; failure is unchecked. Enqueue copies all four argument bytes
/// synchronously, but playback reads only the initialized slot byte. The other
/// three bytes retain their stack contents. The resident zero-key convention
/// supplies the ignored file key. Movie resources must stay live through playback.
static inline void _shelterR49QueueRoomMovie(void)
{
    enum { SHELTER_R49_MOVIE_STREAM_ID = 100 };
    u8              commandArgs[sizeof(gCdCmdQueue.entries[0].args)];
    GameLocationKey movieLocation;

    movieLocation      = gGameSession->location;
    movieLocation.view = SHELTER_R49_MOVIE_STREAM_ID;
    commandArgs[0]     = streamFindMovieSlot(&movieLocation, 0, 0);
    cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, commandArgs);
}

/// Plays room movie 100 after a blank-display delay, then restores game presentation.
///
/// Start this bodyless controller in state 0 with exclusive movie/CD workspace
/// use. Waits 31 further task ticks before saving VRAM and preparing the decoder.
/// The current room's movie lookup must succeed with slot 0..14; failure is
/// unchecked. Start cancels playback. Both completion and cancellation drain
/// the CD queue before restoring image memory, model buffers and sprite images.
/// The overlay, session and saved VRAM regions must stay live through restore.
/// No task work is allocated; teardown releases this task and resumes the game loop.
static void _shelterR49PlayMovieTask(Task* movieTask)
{
    enum {
        SHELTER_R49_MOVIE_BLANK_DISPLAY = 0,
        SHELTER_R49_MOVIE_DELAY         = 1,
        SHELTER_R49_MOVIE_QUEUE         = 2,
        SHELTER_R49_MOVIE_WAIT_READY    = 3,
        SHELTER_R49_MOVIE_PLAYING       = 4,
        SHELTER_R49_MOVIE_WAIT_IDLE     = 5,
        SHELTER_R49_MOVIE_RESTORE       = 6,
        SHELTER_R49_MOVIE_DELAY_TICKS   = 31,
    };
    CdCmdQueue* cdQueue;

    cdQueue = &gCdCmdQueue;
    switch (movieTask->state) {
        case SHELTER_R49_MOVIE_BLANK_DISPLAY:
            SetDispMask(false);
            movieTask->killCountdown = 0;
            movieTask->state++;
            break;
        case SHELTER_R49_MOVIE_DELAY:
            movieTask->killCountdown++;
            if (movieTask->killCountdown < SHELTER_R49_MOVIE_DELAY_TICKS) {
                break;
            }
            streamPrepareMovieWorkspace(true);
            movieTask->state++;
            break;
        case SHELTER_R49_MOVIE_QUEUE:
            _shelterR49QueueRoomMovie();
            movieTask->state++;
            break;
        case SHELTER_R49_MOVIE_WAIT_READY:
            if (cdQueue->movieReady == 0) {
                break;
            }
            SetDispMask(true);
            movieTask->state++;
            break;
        case SHELTER_R49_MOVIE_PLAYING:
            if (cdCmdIsIdle()) {
                SetDispMask(false);
                movieTask->state++;
                break;
            }
            if (padIsStartPressed() == 0) {
                break;
            }
            SetDispMask(false);
            cdCmdRequestCancel();
            movieTask->state++;
            break;
        // Decoder use must end before its workspace and saved VRAM are restored.
        case SHELTER_R49_MOVIE_WAIT_IDLE:
            if (cdCmdIsIdle() == 0) {
                break;
            }
            streamResetGameRestore();
            movieTask->state++;
            break;
        case SHELTER_R49_MOVIE_RESTORE:
            if (streamPollGameRestore(false, true) == 0) {
                break;
            }
            taskKill(movieTask);
            displayResumeGameLoop();
            break;
    }
}

/// Hands presentation to room movie 100 and queues the current camera and packets.
///
/// Requires game-loop display ownership and loaded current-view resources.
/// Spawn failures are ignored; the movie task returns presentation after playback.
static inline void _shelterR49StartObservatoryMovie(void)
{
    displaySpawnTaskFromTable(gShelterR49ObservatoryTransitionTasks, SHELTER_R49_OBSERVATORY_MOVIE_TASK_INDEX, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
}

/// Selects the observatory's first room and arrival warp for a live-save reload.
///
/// Requires live save/display state and the loaded gameplay reload task bank.
/// Retains the saved view and placement variant; selects sprite-resource variant 1.
/// The queued reload reads these selectors later when rebuilding the session.
static inline void _shelterR49RequestObservatoryReload(void)
{
    enum {
        SHELTER_R49_OBSERVATORY_ARRIVAL_WARP   = 1,
        SHELTER_R49_OBSERVATORY_ROOM           = 1,
        SHELTER_R49_OBSERVATORY_SPRITE_VARIANT = 1,
    };

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_SHELTER_NEO_ARK;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_NEO_ARK_OBSERVATORY;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = SHELTER_R49_OBSERVATORY_ARRIVAL_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = SHELTER_R49_OBSERVATORY_ROOM;
    gDisplayState.spriteVariant                                 = SHELTER_R49_OBSERVATORY_SPRITE_VARIANT;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
}

/// Plays the room movie, then reloads the live save into Neo Ark's observatory.
///
/// Descriptor slot 0 starts this bodyless default-list task in state 0;
/// states 0..3 start playback, wait two ticks, then request reload.
/// Requires game-loop display ownership, live save/session state and loaded
/// current-view/movie resources. The current room's view must be 1..3 and
/// the movie-100 lookup must succeed. The movie's display loop suspends this list;
/// after presentation returns, two task ticks pass before the reload request.
/// No work or spawn payload is used. Spawn failures are unchecked; the task
/// releases itself after requesting reload. Keep this overlay loaded until then.
static void _shelterR49EnterObservatoryTask(Task* transitionTask)
{
    enum {
        SHELTER_R49_OBSERVATORY_START_MOVIE      = 0,
        SHELTER_R49_OBSERVATORY_WAIT_FIRST_TICK  = 1,
        SHELTER_R49_OBSERVATORY_WAIT_SECOND_TICK = 2,
        SHELTER_R49_OBSERVATORY_RELOAD           = 3,
    };

    switch (transitionTask->state) {
        case SHELTER_R49_OBSERVATORY_START_MOVIE:
            _shelterR49StartObservatoryMovie();
            transitionTask->state++;
            break;
        case SHELTER_R49_OBSERVATORY_WAIT_FIRST_TICK:
        case SHELTER_R49_OBSERVATORY_WAIT_SECOND_TICK:
            transitionTask->state++;
            break;
        // The reload consumes saved selectors after the movie has restored resources.
        case SHELTER_R49_OBSERVATORY_RELOAD:
            SetDispMask(true);
            _shelterR49RequestObservatoryReload();
            taskKill(transitionTask);
            break;
    }
}

void shelterR49EffectNoopTask(Task* unusedTask)
{
}
