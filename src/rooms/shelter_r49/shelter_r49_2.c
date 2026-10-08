#include "rooms/shelter_r49.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/task_tables.h"

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
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"

extern WorldCollisionGrid   D_shelter_r49_8017DAAC[1];
extern WorldCoordRoomLights D_shelter_r49_8017DD24[1];
static void                 _shelterR49PlayMovieTask(Task* movieTask);
void                        func_shelter_r49_8017D8D8(Task*);

TaskDesc D_shelter_r49_8017DA00[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_r49_8017D8D8, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _shelterR49PlayMovieTask, { .value = 0 } },
};

WorldCollisionRoomResources D_shelter_r49_8017DA18[1] = {
    { D_shelter_r49_8017DAAC, NULL, NULL, NULL },
};

u8* D_shelter_r49_8017DA28[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_r49_8017DA2C[1] = { 3 };

WorldCoordRoomLighting D_shelter_r49_8017DA30[1] = {
    { D_shelter_r49_8017DD24, NULL },
};

DirectionWarpEntry D_shelter_r49_8017DA38[1] = {
    { { { .word = 1024 }, 0, -0x2710, 0 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 0, 0, 0 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterR49Collision004ECNormals[1] = {
#include "assets/shelter_r49_collision_004EC_normals.inc"
};

static SVECTOR _gShelterR49Collision004ECVerts[4] = {
#include "assets/shelter_r49_collision_004EC_verts.inc"
};

static WorldCollisionGridFace _gShelterR49Collision004ECFaces[1] = {
#include "assets/shelter_r49_collision_004EC_faces.inc"
};

static s16 _gShelterR49Collision004ECCells[2] = {
#include "assets/shelter_r49_collision_004EC_cells.inc"
};

#define GRID_CELL(i) (&_gShelterR49Collision004ECCells[i])
static s16* _gShelterR49Collision004ECTable[1] = {
#include "assets/shelter_r49_collision_004EC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_r49_8017DAAC[1] = {
    { NULL, _gShelterR49Collision004ECNormals, _gShelterR49Collision004ECVerts, _gShelterR49Collision004ECFaces, _gShelterR49Collision004ECTable, 4000, 4000, 1, 1, 0x7530, 1 },
};

ViewCamera D_shelter_r49_8017DAD0[3] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x32C8, 0 } }, 257 },
    { { { { 3982, 0, -955 }, { 108, 4069, 451 }, { 949, -463, 3957 } }, { 1470, 1070, 2380 } }, 257 },
    { { { { 4057, 0, -561 }, { 28, 4090, 208 }, { 560, -210, 4051 } }, { 770, 1400, -190 } }, 541 },
};

SpriteBatch D_shelter_r49_8017DB3C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_r49_8017DB4C[15] = {
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 24, 0, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 32, 0, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 32, 0, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, 8, 24, 0, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 48, 32, 0, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 40, 0, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 96, 0, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -16, 24, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -32, 32, 961, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, 48, 1131, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 8, 32, 1050, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 16, 40, 1150, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 40, 32, 1375, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 48, 40, 1375, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, 48, 1250, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_r49_8017DC78[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_r49_8017DC90[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_r49_8017DCA0[3] = {
    { { .empty = D_shelter_r49_8017DB3C }, D_shelter_r49_8017DB3C, NULL },
    { { .elements = D_shelter_r49_8017DB4C }, D_shelter_r49_8017DC78, NULL },
    { { .empty = D_shelter_r49_8017DC90 }, D_shelter_r49_8017DC90, NULL },
};

WorldCoordPointLight D_shelter_r49_8017DCC4[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2500, 3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 6000, 7000 },
};

WorldCoordRoomLights D_shelter_r49_8017DD24[1] = {
    { 0, NULL, ARRAY_SIZE(D_shelter_r49_8017DCC4), D_shelter_r49_8017DCC4, 0, NULL },
};

AreaResource D_shelter_r49_8017DD3C[2] = {
    { 111, 439, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_143900_801413EC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_r49_8017DD54[2] = {
    { 111, 0, 0, 0, 0, 3210, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_r49_8017DD74[13] = {
    { NULL, NULL },
    { D_shelter_r49_8017DD54, D_shelter_r49_8017DD3C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionFootstepSounds D_shelter_r49_8017DDDC = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

WorldCollisionSurfaceProperties D_shelter_r49_8017DDE8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_r49_8017DDF0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_r49_8017DDDC },
};

WorldCollisionSurfaceProperties* D_shelter_r49_8017DDF8[8] = {
    D_shelter_r49_8017DDE8,
    D_shelter_r49_8017DDF0,
    D_shelter_r49_8017DDE8,
    D_shelter_r49_8017DDE8,
    D_shelter_r49_8017DDE8,
    D_shelter_r49_8017DDE8,
    D_shelter_r49_8017DDE8,
    D_shelter_r49_8017DDE8,
};

/// Queues movie 100 for the current room, borrowing the loaded stream descriptors.
///
/// Requires a matching slot in 0..14 and prepared movie workspace. The CD queue
/// copies the four-byte argument block synchronously and consumes its slot byte.
static inline void _shelterR49QueueRoomMovie(void)
{
    enum { SHELTER_R49_MOVIE_STREAM_ID = 100 };
    u8      commandArgs[sizeof(gCdCmdQueue.entries[0].args)];
    GameLoc movieLocation;

    movieLocation          = gGameSession->location;
    movieLocation.loc.view = SHELTER_R49_MOVIE_STREAM_ID;
    commandArgs[0]         = streamFindMovieSlot(&movieLocation.loc, 0, 0);
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

void func_shelter_r49_8017D8D8(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            displaySpawnTaskFromTable(D_shelter_r49_8017DA00, 1, 0, 0);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
            viewQueueCurrentCameraAndPackets();
            arg0->state = arg0->state + 1;
            break;
        case 1:
        case 2:
            arg0->state = arg0->state + 1;
            break;
        case 3:
            SetDispMask(1);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_SHELTER_NEO_ARK;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_NEO_ARK_OBSERVATORY;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
            gDisplayState.spriteVariant                                 = 1;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            taskKill(arg0);
            break;
    }
}

void shelterR49EffectNoopTask(Task* unusedTask)
{
}
