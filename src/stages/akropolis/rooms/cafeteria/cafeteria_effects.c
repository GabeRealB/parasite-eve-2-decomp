#include "stages/akropolis/rooms/cafeteria.h"
#include "cafeteria_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room_common.h"

/// The twin-trail endpoint table, defined in cafeteria.c.
#define ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS
#include "../../../../shared/room/room_visual_effects.h"
#undef ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS
#include "../../../../shared/actor/actor_contacts.h"
#include "../../../../shared/room/streamed_scene.h"

#define ROOM_PART declarations_cafeteria_effects
#include "assets/rooms/akropolis/cafeteria/declarations.inc"

/// Placed pickup state after collection, shared by this room's model hooks.
enum { PICKUP_COLLECTED = 2 };

/// Phases of the loose-prop task, held in `_LoosePropWork::phase`.
enum {
    AKROPOLIS_CAFETERIA_LOOSE_PROP_RESTING,  // Waiting for anything to touch the body
    AKROPOLIS_CAFETERIA_LOOSE_PROP_HOPPING,  // Knocked upwards: the lift wears off while the prop spins and slides
    AKROPOLIS_CAFETERIA_LOOSE_PROP_SLIDING,  // Horizontal speed decaying to a stop
    AKROPOLIS_CAFETERIA_LOOSE_PROP_SETTLING, // Pitch and roll decaying to level, then resting again
};

/// Work block of the cafeteria's loose-prop task, kept at `Task::work`.
typedef struct {
    WorldCollisionBody    body;          // Sphere linked into the world's collision lists
    WorldCollisionContact contacts[6];   // Contact table the body borrows; emptied after every update
    s32                   kickStrength;  // Random 0x3000..0x3FFF at set-up, one less every frame; scales the launch speed and the spin
    VECTOR                velocity;      // World units added to the prop's position per frame; `vy` is the hop on top of the constant sink
    SVECTOR               rotation;      // Euler angles (4096 per turn) the model's rotation is rebuilt from every frame
    SVECTOR               kickDirection; // Player's forward axis at the contact, normalised to 4096
    u16                   phase;         // An `AKROPOLIS_CAFETERIA_LOOSE_PROP_*` phase
} _LoosePropWork;
STATIC_ASSERT_SIZEOF(_LoosePropWork, 0xD8);

extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

static void _akropolisCafeteriaLoosePropExit(Task* task);

static u32     _gAkropolisCafeteriaMendelJournalModelPartVerts[1];
static SVECTOR _gAkropolisCafeteriaMendelJournalModelVerts[22];
static SVECTOR _gAkropolisCafeteriaMendelJournalModelNormals[18];
static TmdBone _gAkropolisCafeteriaMendelJournalModelSkeleton[1];
static u32     _gAkropolisCafeteriaMendelJournalModelStream[112];

#define ROOM_PART gAkropolisCafeteriaRoomLights
#include "assets/rooms/akropolis/cafeteria/lights.inc"
#include "assets/rooms/akropolis/cafeteria/sprites.inc"
#include "assets/rooms/akropolis/cafeteria/cameras.inc"
#include "assets/rooms/akropolis/cafeteria/ambient.inc"
#include "assets/rooms/akropolis/cafeteria/updates.inc"
#include "assets/rooms/akropolis/cafeteria/surfaces.inc"
#include "assets/rooms/akropolis/cafeteria/stim_pickup.inc"

#include "assets/rooms/akropolis/cafeteria/mendel_journal.inc"

/// Fixed light colors and ambient base for the Mendel pickup model.
static MATRIX _gAkropolisCafeteriaMendelPickupColorMatrix = {
    .m = { { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 } },
    .t = { ONE / 2, ONE / 2, ONE / 2 },
};

/// Fixed world-space light-direction rows for the Mendel pickup model.
static MATRIX _gAkropolisCafeteriaMendelPickupLightMatrix = {
    .m = { { ONE / 32, ONE / 32, ONE / 32 }, { -ONE / 2, -ONE / 2, -ONE / 2 }, { -ONE / 2, -ONE / 2, -ONE / 2 } },
    .t = { ONE / 2, ONE / 2, ONE / 2 },
};

/// Fixed light colors and ambient base for the stim pickup models.
static MATRIX _gAkropolisCafeteriaStimPickupColorMatrix = {
    .m = { { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 } },
    .t = { ONE, ONE, ONE },
};

/// Fixed world-space light-direction rows for the stim pickup models.
static MATRIX _gAkropolisCafeteriaStimPickupLightMatrix = {
    .m = { { ONE / 32, ONE / 32, ONE / 32 }, { -ONE / 2, -ONE / 2, -ONE / 2 }, { -ONE / 2, -ONE / 2, -ONE / 2 } },
    .t = { ONE / 2, ONE / 2, ONE / 2 },
};

/// Fixed light colors and ambient base for the retained object-flag-10 draw hook.
static MATRIX _gAkropolisCafeteriaObject10ColorMatrix = {
    .m = { { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 } },
    .t = { ONE, ONE, ONE },
};

/// Fixed light-direction rows for the retained object-flag-10 draw hook.
static MATRIX _gAkropolisCafeteriaObject10LightMatrix = {
    .m = { { ONE / 32, ONE / 32, ONE / 32 }, { -ONE / 2, -ONE / 2, -ONE / 2 }, { -ONE / 2, -ONE / 2, -ONE / 2 } },
    .t = { ONE / 2, ONE / 2, ONE / 2 },
};

/// Fixed light colors and ambient base for the retained object-flag-11 draw hook.
static MATRIX _gAkropolisCafeteriaObject11ColorMatrix = {
    .m = { { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 } },
    .t = { ONE, ONE, ONE },
};

/// Fixed light-direction rows for the retained object-flag-11 draw hook.
static MATRIX _gAkropolisCafeteriaObject11LightMatrix = {
    .m = { { ONE / 32, ONE / 32, ONE / 32 }, { -ONE / 2, -ONE / 2, -ONE / 2 }, { -ONE / 2, -ONE / 2, -ONE / 2 } },
    .t = { ONE / 2, ONE / 2, ONE / 2 },
};

s32 gAkropolisCafeteriaStrangerRecoilVelocityX = 0;

s32 gAkropolisCafeteriaStrangerRecoilVelocityY = 0;

s32 gAkropolisCafeteriaStrangerRecoilVelocityZ = 0;

SVECTOR ActorContact_ScratchPosition = { 0 };

static void _akropolisCafeteriaLoosePropInit(Task* task);
static void _akropolisCafeteriaLoosePropUpdate(Task* task);
static void _akropolisCafeteriaLoosePropRequestExit(Task* task);
static void _akropolisCafeteriaObject10DrawTask(Task* task);
static void _akropolisCafeteriaObject11DrawTask(Task* task);

/// Chooses a puff's local XYZ offset using three successive shared LCG draws.
static inline void _akropolisCafeteriaChoosePuffSpawnOffset(EffectWork* work)
{
    enum { PUFF_OFFSET_X_MIN         = 560,
           PUFF_OFFSET_X_SPAN        = 2620,
           PUFF_OFFSET_Y_TOP         = -300,
           PUFF_OFFSET_Y_LEVELS      = 5,
           PUFF_OFFSET_Y_STEP        = 400,
           PUFF_OFFSET_Z_MIN         = 2816,
           PUFF_OFFSET_Z_RANDOM_MASK = 0x3FF };
    u16 randomWord;

    RANDOM_LCG_ADVANCE();
    randomWord    = RANDOM_LCG_DRAW();
    work->move.vx = (u32)randomWord % PUFF_OFFSET_X_SPAN + PUFF_OFFSET_X_MIN;
    RANDOM_LCG_ADVANCE();
    randomWord    = RANDOM_LCG_DRAW();
    work->move.vy = PUFF_OFFSET_Y_TOP - (u16)((u32)randomWord % PUFF_OFFSET_Y_LEVELS) * PUFF_OFFSET_Y_STEP;
    RANDOM_LCG_ADVANCE();
    randomWord    = RANDOM_LCG_DRAW();
    work->move.vz = (randomWord & PUFF_OFFSET_Z_RANDOM_MASK) + PUFF_OFFSET_Z_MIN;
}

void akropolisCafeteriaPlayMovieTask(Task* movieTask)
{
    enum {
        MOVIE_MUSIC_START_TICK = 1091,
        MOVIE_MUSIC_PENDING    = 0,
        MOVIE_MUSIC_REQUESTED  = 1
    };
    u8              commandArgs[sizeof(gCdCmdQueue.entries[0].args)];
    GameLocationKey movieLocation;
    CdCmdQueue*     queue;

    queue = &gCdCmdQueue;
    switch (movieTask->state) {
        case STREAMED_SCENE_PREPARE:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            movieTask->state = movieTask->state + 1;
            break;

        case STREAMED_SCENE_QUEUE_MOVIE:
            movieLocation      = gGameSession->location;
            movieLocation.view = STREAMED_SCENE_MOVIE_ID;
            // The queue copies all four bytes; this opcode interprets only the slot byte.
            commandArgs[0] = streamFindMovieSlot(&movieLocation, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, commandArgs);
            movieTask->state = movieTask->state + 1;
            break;

        case STREAMED_SCENE_WAIT_READY:
            if (queue->movieReady == 0) {
                return;
            }
            SetDispMask(1);
            movieTask->killCountdown   = 0;
            movieTask->spawnArg1.value = MOVIE_MUSIC_PENDING;
            movieTask->state           = movieTask->state + 1;
            break;

        case STREAMED_SCENE_PLAYING:
            // Start area music at the movie cue, or defer it until restoration on an early skip.
            if (++movieTask->killCountdown == MOVIE_MUSIC_START_TICK) {
                stageMusicRequestAreaStart(0);
                movieTask->spawnArg1.value = MOVIE_MUSIC_REQUESTED;
            }
            if (cdCmdIsIdle()) {
                SetDispMask(0);
                movieTask->state = movieTask->state + 1;
                break;
            }
            if (padIsStartPressed() == 0) {
                return;
            }
            SetDispMask(0);
            cdCmdRequestCancel();
            movieTask->state = movieTask->state + 1;
            break;

        // Restore resources only after playback or cancellation has drained the queue.
        case STREAMED_SCENE_WAIT_IDLE:
            if (cdCmdIsIdle() == 0) {
                return;
            }
            streamResetGameRestore();
            movieTask->state = movieTask->state + 1;
            break;

        case STREAMED_SCENE_RESTORE_GAME:
            if (streamPollGameRestore(0, 1) == 0) {
                return;
            }
            if (movieTask->spawnArg1.value == MOVIE_MUSIC_PENDING) {
                stageMusicRequestAreaStart(0);
            }
            taskKill(movieTask);
            displayResumeGameLoop();
            break;
    }
}

void akropolisCafeteriaBlackoutTask(Task* task)
{
    enum {
        BLACKOUT_INTENSITY    = 0xFF,
        BLACKOUT_COUNTER_STEP = 4,
        BLACKOUT_COUNTER_END  = 0x100
    };
    u16 elapsedQuarterTicks;

    fadeDrawOverlay(BLACKOUT_INTENSITY, BLACKOUT_INTENSITY, BLACKOUT_INTENSITY, GPU_BLEND_SUBTRACT);
    elapsedQuarterTicks = task->killCountdown + BLACKOUT_COUNTER_STEP;
    task->killCountdown = elapsedQuarterTicks;
    if ((s16)elapsedQuarterTicks >= BLACKOUT_COUNTER_END) {
        taskKill(task);
    }
}

void akropolisCafeteriaStartMovieTask(Task* task)
{
    displaySpawnTaskFromTable(gAkropolisCafeteriaMovieTasks, AKROPOLIS_CAFETERIA_MOVIE_TASK_PLAYBACK, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(task);
}

void akropolisCafeteriaInitRoomEffectsTask(Task* task)
{
    enum { ROOM_EFFECTS_INITIALIZE = 0,
           MODEL_WANDER_TIMED      = 0,
           MODEL_WANDER_AMBIENT    = 1 };
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    spawnOffset;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (task->state != ROOM_EFFECTS_INITIALIZE) {
        return;
    }
    task->msgTable = gAkropolisCafeteriaRoomEffectMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
    spawnOffset                    = &work->move;
    gAkropolisCafeteriaPuffEnabled = 0;
    work->move.vx                  = 0x220;
    work->move.vy                  = -0x12C;
    work->move.vz                  = -0x6A0;
    // Seed three timed wanderers, then two ambient wanderers in the parent's local frame.
    effectSpawn(EFFECT_064, coord, MODEL_WANDER_TIMED, spawnOffset);
    work->move.vx = 0x400;
    work->move.vy = -0x12C;
    work->move.vz = -0x260;
    effectSpawn(EFFECT_064, coord, MODEL_WANDER_TIMED, spawnOffset);
    work->move.vx = 0x370;
    work->move.vy = -0x12C;
    work->move.vz = -0x860;
    effectSpawn(EFFECT_064, coord, MODEL_WANDER_TIMED, spawnOffset);
    task->state   = task->state + 1;
    work->move.vx = 0xBB8;
    work->move.vy = -0x834;
    work->move.vz = -0x7D0;
    effectSpawn(EFFECT_064, coord, MODEL_WANDER_AMBIENT, spawnOffset);
    work->move.vx = 0xB22;
    work->move.vy = -0x834;
    work->move.vz = -0x900;
    effectSpawn(EFFECT_064, coord, MODEL_WANDER_AMBIENT, spawnOffset);
    gRoomEffectFlashId      = EFFECT_AKROPOLIS_CAFETERIA_FLASH;
    gRoomEffectTwinTrailId  = EFFECT_AKROPOLIS_CAFETERIA_TWIN_TRAIL;
    gRoomEffectSparkBurstId = EFFECT_AKROPOLIS_CAFETERIA_SPARK_BURST;
}
void akropolisCafeteriaPuffEmitterTask(Task* task)
{
    enum {
        PUFF_EMITTER_ACTIVE_VIEW      = 9,
        PUFF_EMITTER_ENTRY_COUNT      = 40,
        PUFF_EMITTER_TICK_COUNT       = 2,
        PUFF_EMITTER_SKIP_FADE        = 0x1000,
        PUFF_EMITTER_SIZE_MIN         = 0x180,
        PUFF_EMITTER_SIZE_RANDOM_MASK = 0xFF
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         puffIndex;
    u16         puffCount;
    s32         spawnFlags;
    s32         spawnSizeBase;
    u8          view;
    u16         randomWord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gAkropolisCafeteriaPuffEnabled == 0) {
        effectKillTask(work, task);
        return;
    }
    view = gGameSession->location.view;
    if (view == PUFF_EMITTER_ACTIVE_VIEW) {
        // The cached previous view distinguishes a pre-aged entry burst from steady emission.
        puffCount = PUFF_EMITTER_ENTRY_COUNT;
        if (work->scale != view) {
            spawnFlags = PUFF_EMITTER_SKIP_FADE;
        } else {
            puffCount  = PUFF_EMITTER_TICK_COUNT;
            spawnFlags = 0;
        }
        // Consume four successive LCG draws for X, Y, Z and perspective size, even on spawn failure.
        for (puffIndex = 0; puffIndex < puffCount; puffIndex++) {
            _akropolisCafeteriaChoosePuffSpawnOffset(work);
            RANDOM_LCG_ADVANCE();
            randomWord    = RANDOM_LCG_DRAW();
            spawnSizeBase = spawnFlags + PUFF_EMITTER_SIZE_MIN;
            effectSpawn(EFFECT_AKROPOLIS_CAFETERIA_PUFF, coord, (randomWord & PUFF_EMITTER_SIZE_RANDOM_MASK) + spawnSizeBase, &work->move);
        }
    }
    work->scale = gGameSession->location.view;
}

/// Sets the puff quad's UVs to its current 48 x 48 texel animation cell.
static inline void _akropolisCafeteriaSetPuffCellUvs(POLY_FT4* quad, const EffectWork* work)
{
    enum { PUFF_SHEET_COLUMNS = 5,
           PUFF_CELL_TEXELS   = 48,
           PUFF_UV_SPAN       = PUFF_CELL_TEXELS - 1 };

    quad->u0 = ((work->age / work->step) % PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS;
    quad->v0 = ((work->age / work->step) / PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS;
    quad->u1 = ((work->age / work->step) % PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS + PUFF_UV_SPAN;
    quad->v1 = ((work->age / work->step) / PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS;
    quad->u2 = ((work->age / work->step) % PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS;
    quad->v2 = ((work->age / work->step) / PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS + PUFF_UV_SPAN;
    quad->u3 = ((work->age / work->step) % PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS + PUFF_UV_SPAN;
    quad->v3 = ((work->age / work->step) / PUFF_SHEET_COLUMNS) * PUFF_CELL_TEXELS + PUFF_UV_SPAN;
}

void akropolisCafeteriaPuffTask(Task* task)
{
    enum {
        PUFF_ACTIVE_VIEW        = 9,
        PUFF_SEED_MIN_DEPTH     = 16,
        PUFF_ROTATION_MASK      = 0xFFF,
        PUFF_SPAWN_SIZE_MASK    = 0xFFF,
        PUFF_SPAWN_SKIP_FADE    = 0x1000,
        PUFF_SKIP_FADE_AGE      = 10,
        PUFF_FADE_IN_TICKS      = 10,
        PUFF_MAX_SHADE          = 40,
        PUFF_CELL_TEXELS        = 48,
        PUFF_UV_SPAN            = PUFF_CELL_TEXELS - 1,
        PUFF_CELL_COUNT         = 10,
        PUFF_QUARTER_TURN       = 0x400,
        PUFF_TRIG_FRACTION_BITS = 12,
        PUFF_DRIFT_Z_LIMIT      = 0xB00
    };
    EffectWork*           work;
    GfxCoord*             coord;
    OverlaySpriteScratch* scratchEnd;
    OverlaySpriteScratch* scratch;
    POLY_FT4*             quad;
    u8                    view;
    u8                    shade;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gAkropolisCafeteriaPuffEnabled != 0) {
        view = gGameSession->location.view;
        if (view == PUFF_ACTIVE_VIEW) {
            // Project the centre once; the billboard's rotation is in screen space.
            actorRenderComposeCoord(coord);
            scratchEnd = SCRATCH_STACK_CURSOR(OverlaySpriteScratch);
            SCRATCH_STACK_RESERVE_BLOCK(OverlaySpriteScratch);
            scratch              = SCRATCH_STACK_CURSOR(OverlaySpriteScratch);
            scratch->worldPos.vx = coord->workm.t[0];
            scratch->worldPos.vy = coord->workm.t[1];
            scratch->worldPos.vz = coord->workm.t[2];
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_ldv0(&scratchEnd[-1].worldPos);
            gte_rtps();
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            gte_stsxy(&scratchEnd[-1].screenPos);
            gte_stszotz(&scratch->otz);
            // Depth gates seeding only; drawing and division below always proceed.
            if (scratchEnd[-1].otz > PUFF_SEED_MIN_DEPTH && work->age == 0) {
                RANDOM_LCG_ADVANCE();
                work->scale   = RANDOM_LCG_DRAW() & PUFF_ROTATION_MASK;
                work->angle   = task->spawnArg1.halves.low & PUFF_SPAWN_SIZE_MASK;
                work->move.vx = 0;
                RANDOM_LCG_ADVANCE();
                work->move.vy = (RANDOM_LCG_DRAW() & 0xF) + 4;
                RANDOM_LCG_ADVANCE();
                work->move.vz = -(RANDOM_LCG_DRAW() & 0xF) - 4;
                RANDOM_LCG_ADVANCE();
                work->step = (RANDOM_LCG_DRAW() & 3) + 3;
                if (task->spawnArg1.value & PUFF_SPAWN_SKIP_FADE) {
                    work->age = PUFF_SKIP_FADE_AGE;
                }
            }
            if (work->age < PUFF_FADE_IN_TICKS) {
                shade = work->age * 4;
                setRGB0(quad, shade, shade, shade);
            } else {
                shade = PUFF_MAX_SHADE;
                setRGB0(quad, shade, shade, shade);
            }
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
            quad->clut  = getClut(0, 270);
            setSemiTrans(quad, 1);
            _akropolisCafeteriaSetPuffCellUvs(quad, work);
            scratch->cornerDx = (((work->angle * PUFF_UV_SPAN) / scratch->otz) * rsin(work->scale)) >> PUFF_TRIG_FRACTION_BITS;
            scratch->cornerDy = (((work->angle * PUFF_UV_SPAN) / scratch->otz) * rcos(work->scale)) >> PUFF_TRIG_FRACTION_BITS;
            quad->x0          = scratch->screenPos.vx + scratch->cornerDx;
            quad->x3          = scratch->screenPos.vx - scratch->cornerDx;
            quad->y0          = scratch->screenPos.vy - scratch->cornerDy;
            quad->y3          = scratch->screenPos.vy + scratch->cornerDy;
            scratch->cornerDx = (((work->angle * PUFF_UV_SPAN) / scratch->otz) * rsin(work->scale + PUFF_QUARTER_TURN)) >> PUFF_TRIG_FRACTION_BITS;
            scratch->cornerDy = (((work->angle * PUFF_UV_SPAN) / scratch->otz) * rcos(work->scale + PUFF_QUARTER_TURN)) >> PUFF_TRIG_FRACTION_BITS;
            quad->x1          = scratch->screenPos.vx + scratch->cornerDx;
            quad->x2          = scratch->screenPos.vx - scratch->cornerDx;
            quad->y1          = scratch->screenPos.vy - scratch->cornerDy;
            quad->y2          = scratch->screenPos.vy + scratch->cornerDy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
                    quad);
            SCRATCH_STACK_RELEASE_BLOCK(OverlaySpriteScratch);
            // Move toward the Z boundary first, then descend along positive Y.
            if (coord->coord.t[2] > PUFF_DRIFT_Z_LIMIT) {
                coord->coord.t[2] += work->move.vz;
            } else {
                coord->coord.t[1] += work->move.vy;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->age++;
            if (work->age <= work->step * PUFF_CELL_COUNT - 1) {
                return;
            }
        }
    }
    effectKillTask(work, task);
}

void akropolisCafeteriaModelWanderTask(Task* task)
{
    enum {
        WANDER_INITIALIZE                    = 0,
        WANDER_TIMED_TURN                    = 0,
        WANDER_TIMED_MOVE                    = 1,
        WANDER_AMBIENT_TURN                  = 2,
        WANDER_AMBIENT_MOVE                  = 3,
        WANDER_DEPART                        = 4,
        WANDER_DEPART_AGE                    = 121,
        WANDER_TIMED_EXIT_X                  = 0xB00,
        WANDER_AMBIENT_EXIT_X                = 0xD90,
        WANDER_EXIT_YAW                      = 0x400,
        WANDER_MOVE_SPEED_Q4                 = 0x200,
        WANDER_DEPART_SPEED_Q4               = 0x300,
        WANDER_DIRECTION_SPEED_FRACTION_BITS = 16,
        WANDER_SOUND_VIEW                    = 7,
        WANDER_SOUND_SCRIPT                  = 6,
        WANDER_SOUND_PENDING                 = 0,
        WANDER_SOUND_PLAYED                  = 1
    };
    TmdObject*  model;
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s32         previousYaw;
    s32         departureYaw;
    s32         nextYaw;
    s32         randomIncrement;
    s32         audioPan;

    model         = task->extra.tmd;
    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = model->coords;
    if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        effectKillTask(work, task);
        return;
    }
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }
    actorRenderComposeCoord(coord);
    work->age++;
    // A nonzero spawn argument selects ambient wandering without the departure timer.
    if (task->state == WANDER_INITIALIZE) {
        model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        if (task->spawnArg1.value != 0) {
            work->period = WANDER_AMBIENT_EXIT_X;
            work->angle  = 0;
            work->index  = WANDER_AMBIENT_TURN;
            RANDOM_LCG_ADVANCE();
            work->scale = RANDOM_LCG_DRAW() & 0xF00;
        } else {
            work->scale  = WANDER_EXIT_YAW;
            work->angle  = 0;
            work->period = WANDER_TIMED_EXIT_X;
        }
        gfxRotMatrixY(&coord->coord, work->scale, GRAPHICS_ROTATION_COMPOSE);
        task->state++;
        return;
    }
    switch (work->index) {
        case WANDER_TIMED_TURN:
            RANDOM_LCG_ADVANCE();
            work->angle  = 0;
            work->scale -= (RANDOM_LCG_DRAW() & 0xFF) - 0x80;
            RANDOM_LCG_ADVANCE();
            if ((u16)(RANDOM_LCG_DRAW() % 30) == 0) {
                work->index = WANDER_TIMED_MOVE;
            }
            if (work->age >= WANDER_DEPART_AGE) {
                work->index = WANDER_DEPART;
            }
            break;
        case WANDER_TIMED_MOVE:
            RANDOM_LCG_ADVANCE();
            if (RANDOM_LCG_DRAW() & 1) {
                previousYaw = work->scale;
                if (previousYaw > WANDER_EXIT_YAW) {
                    RANDOM_LCG_ADVANCE();
                    nextYaw  = previousYaw - 0x10;
                    nextYaw -= RANDOM_LCG_DRAW() & 0x3F;
                } else {
                    RANDOM_LCG_ADVANCE();
                    nextYaw  = previousYaw + 0x10;
                    nextYaw += RANDOM_LCG_DRAW() & 0x3F;
                }
                work->scale = nextYaw;
            }
            RANDOM_LCG_ADVANCE();
            work->angle = WANDER_MOVE_SPEED_Q4;
            if ((u16)(RANDOM_LCG_DRAW() % 30) == 0) {
                work->index = WANDER_TIMED_TURN;
            }
            if (work->age >= WANDER_DEPART_AGE) {
                work->index = WANDER_DEPART;
            }
            break;
        case WANDER_AMBIENT_TURN:
            RANDOM_LCG_ADVANCE();
            work->scale -= (RANDOM_LCG_DRAW() & 0xFF) - 0x80;
            RANDOM_LCG_ADVANCE();
            if ((u16)(RANDOM_LCG_DRAW() % 240) == 0) {
                work->scale = WANDER_EXIT_YAW;
                work->angle = WANDER_MOVE_SPEED_Q4;
                work->index = WANDER_AMBIENT_MOVE;
            }
            break;
        case WANDER_AMBIENT_MOVE:
            RANDOM_LCG_ADVANCE();
            if ((RANDOM_LCG_DRAW() & 7) == 0) {
                work->angle = 0;
                work->index = WANDER_AMBIENT_TURN;
            }
            break;
        case WANDER_DEPART:
            departureYaw = work->scale;
            if (departureYaw > WANDER_EXIT_YAW) {
                randomIncrement = RANDOM_LCG_INCREMENT;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + randomIncrement;
                departureYaw   -= 0x10;
                departureYaw   -= RANDOM_LCG_DRAW() & 0x3F;
            } else {
                randomIncrement = RANDOM_LCG_INCREMENT;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + randomIncrement;
                departureYaw   += 0x10;
                departureYaw   += RANDOM_LCG_DRAW() & 0x3F;
            }
            work->scale = departureYaw;
            work->angle = WANDER_DEPART_SPEED_Q4;
            if ((viewGetMappedIndex() & 0xFF) == WANDER_SOUND_VIEW && work->step == WANDER_SOUND_PENDING) {
                audioPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA, WANDER_SOUND_SCRIPT), audioPan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->step = WANDER_SOUND_PLAYED;
            }
            break;
    }
    // Rebuild yaw without changing translation, then turn Q12 forward into a Q4-speed step.
    gfxSetRotIdentity(&coord->coord);
    gfxRotMatrixY(&coord->coord, work->scale, GRAPHICS_ROTATION_COMPOSE);
    gte_ReadMatrixColumn(&coord->coord, 2, &work->move);
    work->move.vx       = (work->move.vx * work->angle) >> WANDER_DIRECTION_SPEED_FRACTION_BITS;
    work->move.vy       = (work->move.vy * work->angle) >> WANDER_DIRECTION_SPEED_FRACTION_BITS;
    work->move.vz       = (work->move.vz * work->angle) >> WANDER_DIRECTION_SPEED_FRACTION_BITS;
    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->period < coord->coord.t[0]) {
        effectKillTask(work, task);
    }
}

s32 akropolisCafeteriaSetPuffEnabled(Task* task, s32 messageId, s32 enabled, s32 unused)
{
    GfxCoord* coord;

    coord                          = task->extra.coordBody->coord;
    gAkropolisCafeteriaPuffEnabled = enabled;
    if (enabled != 0) {
        effectSpawn(EFFECT_AKROPOLIS_CAFETERIA_PUFF_EMITTER, coord, 0, NULL);
    }
    return 0;
}

#include "../../../../shared/room/room_visual_effects.inc.c"

#include "../../../../shared/room/room_visual_effects_flash_task.inc.c"

void akropolisCafeteriaRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../../../shared/room/room_visual_effects_trails.inc.c"

void akropolisCafeteriaRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../../../shared/room/room_visual_effects_trail_task.inc.c"

    void akropolisCafeteriaRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../../../shared/room/room_visual_effects_glow.inc.c"

    /// Places a loose model above the player with a positive-Z offset and links its sphere.
    static void _akropolisCafeteriaLoosePropInit(Task * task)
    {
        TmdObject*      model;
        GfxCoord*       coord;
        _LoosePropWork* work;
        GfxCoord*       playerCoord;

        model = task->extra.tmd;
        coord = model->coords;
        work  = memCalloc(sizeof(*work), false);
        if (work == NULL) {
            taskKill(task);
            return;
        }
        task->work         = work;
        task->exitCallback = _akropolisCafeteriaLoosePropExit;
        task->state        = task->state + 1;
        memFillBytes(work, 0, sizeof(*work));
        coord->parent       = &gGfxViewCoord;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        model->flags        = 0;
        RotMatrix(&work->rotation, &coord->coord);
        work->kickStrength = (rand() & 0xFFF) + 0x3000;
        playerCoord        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
        coord->coord.t[0]  = playerCoord->coord.t[0];
        coord->coord.t[1]  = playerCoord->coord.t[1] - 0x800;
        coord->coord.t[2]  = playerCoord->coord.t[2] + 0x800;
        // The body borrows the contacts embedded in the work block for its whole lifetime.
        work->body.context.contacts = work->contacts;
        work->body.key              = DAMAGE_HAZARD_CATEGORY;
        work->body.radius           = 0xFA;
        work->body.coord            = coord;
        work->body.pos.vx           = 0;
        work->body.pos.vy           = 0;
        work->body.pos.vz           = 0;
        work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PROPS, &work->body);
        worldCollisionInitContacts(work->body.context.contacts, ARRAY_SIZE(work->contacts), 0);
        work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }

    /// Dampens the loose prop's horizontal velocity and advances its motion phase at rest.
    static inline void _akropolisCafeteriaDampLoosePropSlide(_LoosePropWork * work)
    {
        enum { SLIDE_STOP_SPEED = 9 }; // Exclusive magnitude threshold after damping, in world units per tick.

        work->velocity.vx = (work->velocity.vx * 6) / 7;
        if (ABS(work->velocity.vx) < SLIDE_STOP_SPEED) {
            work->velocity.vx = 0;
        }
        work->velocity.vz = (work->velocity.vz * 6) / 7;
        if (ABS(work->velocity.vz) < SLIDE_STOP_SPEED) {
            work->velocity.vz = 0;
        }
        if ((work->velocity.vx | work->velocity.vz) == 0) {
            work->phase++;
        }
    }

    /// Sinks the loose prop and handles its contact-driven hop, slide and leveling.
    static void _akropolisCafeteriaLoosePropUpdate(Task * task)
    {
        MATRIX*         scratchEnd;
        _LoosePropWork* work;
        GfxCoord*       coord;
        SVECTOR*        kickDirection;
        s32             launchMagnitude;

        scratchEnd                   = SCRATCH_STACK_CURSOR(MATRIX);
        SCRATCH_STACK_CURSOR(MATRIX) = scratchEnd - 1;
        work                         = task->work;
        coord                        = task->extra.tmd->coords;
        work->kickStrength--;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[1]  += 0x80;
        actorRenderComposeCoord(coord);
        switch (work->phase) {
            case AKROPOLIS_CAFETERIA_LOOSE_PROP_RESTING:
                if (worldCollisionFindContactIndex(work->body.context.contacts, WORLD_COLLISION_FIND_ANY_KEY)) {
                    work->phase++;
                    // The matrix snapshot is unused; the discarded random draw still advances rand().
                    scratchEnd[-1] = coord->coord;
                    kickDirection  = &work->kickDirection;
                    gfxReadMatrixZAxis(gPlayerStatus.coordMtx, kickDirection);
                    VectorNormalSS(kickDirection, kickDirection);
                    rand();
                    launchMagnitude   = work->kickStrength;
                    launchMagnitude >>= 1;
                    launchMagnitude   = (launchMagnitude * launchMagnitude) >> 6;
                    work->velocity.vy = -0x100;
                    work->velocity.vx = (work->kickDirection.vx * launchMagnitude) >> 24;
                    work->velocity.vz = (work->kickDirection.vz * launchMagnitude) >> 24;
                }
                break;
            case AKROPOLIS_CAFETERIA_LOOSE_PROP_HOPPING:
                work->velocity.vy += 0x10;
                if (work->velocity.vy > 0) {
                    work->velocity.vy = 0;
                    work->phase++;
                } else {
                    work->rotation.vx += (work->kickStrength >> 6) + (rand() & 0x7F);
                    work->rotation.vy += (work->kickStrength >> 6) + (rand() & 0x7F);
                    work->rotation.vz += (work->kickStrength >> 6) + (rand() & 0x7F);
                }
                // Fall through: the hop also loses horizontal speed and applies velocity.
            case AKROPOLIS_CAFETERIA_LOOSE_PROP_SLIDING:
                _akropolisCafeteriaDampLoosePropSlide(work);
                coord->coord.t[0] += work->velocity.vx;
                coord->coord.t[1] += work->velocity.vy;
                coord->coord.t[2] += work->velocity.vz;
                break;
            case AKROPOLIS_CAFETERIA_LOOSE_PROP_SETTLING:
                work->rotation.vx = (work->rotation.vx * 2) / 3;
                if (ABS(work->rotation.vx) < 9) {
                    work->rotation.vx = 0;
                }
                work->rotation.vz = (work->rotation.vz * 2) / 3;
                if (ABS(work->rotation.vz) < 9) {
                    work->rotation.vz = 0;
                }
                if (((u16)work->rotation.vx | (u16)work->rotation.vz) == 0) {
                    work->phase = AKROPOLIS_CAFETERIA_LOOSE_PROP_RESTING;
                }
                break;
        }
        RotMatrix(&work->rotation, &coord->coord);
        worldCollisionClearContacts(work->contacts);
        SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
    }

    /// Selects the loose-prop exit handler for the next task dispatch.
    static void _akropolisCafeteriaLoosePropRequestExit(Task * task)
    {
        enum { LOOSE_PROP_TASK_EXIT = 3 };
        task->state = LOOSE_PROP_TASK_EXIT;
    }

    /// Unlinks the initialized loose prop's sphere before teardown frees its work and model.
    static void _akropolisCafeteriaLoosePropExit(Task * task)
    {
        _LoosePropWork* work;

        work = task->work;
        worldCollisionUnlinkBody(&work->body);
        taskKill(task);
    }

    /// Task-state dispatch for the cafeteria's loose prop.
    static const TaskFuncTable4 _gAkropolisCafeteriaLoosePropStates = { {
        _akropolisCafeteriaLoosePropInit,
        _akropolisCafeteriaLoosePropUpdate,
        _akropolisCafeteriaLoosePropRequestExit,
        _akropolisCafeteriaLoosePropExit,
    } };

    void akropolisCafeteriaLoosePropTask(Task * task)
    {
        TaskFuncTable4 states;

        states = _gAkropolisCafeteriaLoosePropStates;
        states.funcs[task->state](task);
    }

#include "../../../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../../../shared/actor/actor_contacts_push.inc.c"

    void akropolisCafeteriaMendelPickupTask(Task * task)
    {
        enum { MENDEL_VIEW_12            = 12,
               MENDEL_VIEW_24            = 24,
               MENDEL_DEPTH_BIAS_VIEW_12 = 7,
               MENDEL_DEPTH_BIAS_VIEW_24 = 4,
               MENDEL_DEPTH_DEFAULT      = -2 };
        const Enemy* placement;
        TmdObject*   model;

        placement = task->spawnArg2.pointer;
        model     = task->extra.tmd;
        if (areaGetCurrentObjectState((u8)placement->placeKey) != PICKUP_COLLECTED) {
            model->lightMtx = &_gAkropolisCafeteriaMendelPickupLightMatrix;
            model->colorMtx = &_gAkropolisCafeteriaMendelPickupColorMatrix;
            model->flags    = 0;
        } else {
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
        // Bias ordering for the mapped camera even while the collected model is hidden.
        switch (viewGetMappedIndex() & 0xFF) {
            case MENDEL_VIEW_12:
                model->otOffset = MENDEL_DEPTH_BIAS_VIEW_12;
                break;
            case MENDEL_VIEW_24:
                model->otOffset = MENDEL_DEPTH_BIAS_VIEW_24;
                break;
            default:
                model->otOffset = MENDEL_DEPTH_DEFAULT;
                break;
        }
    }
    void akropolisCafeteriaStimPickupTask(Task * task)
    {
        enum { STIM_ACTIVE_VIEW  = 9,
               STIM_TILTED_PLACE = 10,
               STIM_QUARTER_TURN = 0x400 };
        const Enemy* placement;
        TmdObject*   model;
        s32          placeState;

        placement  = task->spawnArg2.pointer;
        model      = task->extra.tmd;
        placeState = areaGetCurrentObjectState((u8)placement->placeKey);
        if ((viewGetMappedIndex() & 0xFF) != STIM_ACTIVE_VIEW) {
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        }
        if ((u8)placement->placeKey == STIM_TILTED_PLACE) {
            gfxRotMatrixX(&task->extra.tmd->coords->coord, STIM_QUARTER_TURN, GRAPHICS_ROTATION_REPLACE);
        }
        model->lightMtx = &_gAkropolisCafeteriaStimPickupLightMatrix;
        model->colorMtx = &_gAkropolisCafeteriaStimPickupColorMatrix;
        // Retire collected placements only in the active view; buffer allocation retries otherwise.
        if (placeState == PICKUP_COLLECTED) {
            model->flags &= (u16)~TMD_OBJECT_FLAGGED_PASS;
            taskCallExit(task);
        } else {
            model->flags    = TMD_OBJECT_FLAGGED_PASS;
            model->otOffset = 0;
            tmdAllocPrimitiveBuffer(model);
        }
    }
    /// Draws an unreferenced model hook in view 9 until placed-object flag 10 is collected.
    static void _akropolisCafeteriaObject10DrawTask(Task * task)
    {
        enum { OBJECT_ACTIVE_VIEW  = 9,
               OBJECT_FLAG_INDEX   = 10,
               OBJECT_QUARTER_TURN = 0x400 };
        TmdObject* model;

        model = task->extra.tmd;
        if ((viewGetMappedIndex() & 0xFF) != OBJECT_ACTIVE_VIEW) {
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        }
        model->lightMtx = &_gAkropolisCafeteriaObject10LightMatrix;
        model->colorMtx = &_gAkropolisCafeteriaObject10ColorMatrix;
        if (areaGetCurrentObjectState(OBJECT_FLAG_INDEX) == PICKUP_COLLECTED) {
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            model->flags    = TMD_OBJECT_FLAGGED_PASS;
            model->otOffset = 0;
            tmdAllocPrimitiveBuffer(model);
        }
        gfxRotMatrixX(&task->extra.tmd->coords->coord, OBJECT_QUARTER_TURN, GRAPHICS_ROTATION_REPLACE);
    }
    /// Draws an unreferenced model hook in views 6, 7 and 10 until object flag 11 is collected.
    static void _akropolisCafeteriaObject11DrawTask(Task * task)
    {
        enum { OBJECT_VIEW_FIRST  = 6,
               OBJECT_VIEW_SECOND = 7,
               OBJECT_VIEW_THIRD  = 10,
               OBJECT_FLAG_INDEX  = 11 };
        TmdObject* model;

        model = task->extra.tmd;
        switch (viewGetMappedIndex() & 0xFF) {
            case OBJECT_VIEW_FIRST:
            case OBJECT_VIEW_SECOND:
            case OBJECT_VIEW_THIRD:
                model->flags    = TMD_OBJECT_FLAGGED_PASS;
                model->lightMtx = &_gAkropolisCafeteriaObject11LightMatrix;
                model->colorMtx = &_gAkropolisCafeteriaObject11ColorMatrix;
                break;
            default:
                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                return;
        }
        if (areaGetCurrentObjectState(OBJECT_FLAG_INDEX) == PICKUP_COLLECTED) {
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            model->flags    = TMD_OBJECT_FLAGGED_PASS;
            model->otOffset = 0;
            tmdAllocPrimitiveBuffer(model);
        }
    }
