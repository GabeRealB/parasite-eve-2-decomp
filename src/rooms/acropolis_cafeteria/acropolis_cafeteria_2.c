#include "rooms/acropolis_cafeteria.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "acropolis_cafeteria_private.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_flags.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
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
#include "main/random.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
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

/// Selects the cafeteria's twin-trail endpoint table defined in acropolis_cafeteria.c.
///
/// Empty presence flag needed only while including the shared header, which
/// declares the external table of two SVECTOR entries and binds the task's accesses.
#define ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS
#include "../../shared/room/room_visual_effects.h"
#undef ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS
#include "../../shared/actor/actor_contacts.h"
#include "../../shared/room/streamed_scene.h"

#define ROOM_PART declarations_acropolis_cafeteria_2
#include "assets/rooms/akropolis/cafeteria/declarations.inc"

/// Placed pickup state after collection, shared by this room's model hooks.
enum { ACROPOLIS_CAFETERIA_PICKUP_COLLECTED = 2 };

/// Phases of the loose-prop task, held in `_AcropolisCafeteriaLoosePropWork::phase`.
enum {
    ACROPOLIS_CAFETERIA_LOOSE_PROP_RESTING,  // Waiting for anything to touch the body
    ACROPOLIS_CAFETERIA_LOOSE_PROP_HOPPING,  // Knocked upwards: the lift wears off while the prop spins and slides
    ACROPOLIS_CAFETERIA_LOOSE_PROP_SLIDING,  // Horizontal speed decaying to a stop
    ACROPOLIS_CAFETERIA_LOOSE_PROP_SETTLING, // Pitch and roll decaying to level, then resting again
};

/// Work block of the cafeteria's loose-prop task, kept at `Task::work`.
///
/// The prop is a small model set down above and ahead of the player. It sinks
/// at a constant rate every frame, and whenever its collision sphere touches
/// another body it is knocked away along the player's facing with a hop and a
/// tumble, slides to a stop and levels out before it can be knocked again.
/// The block owns the body and the contact table the body borrows, so the
/// task's exit unlinks the body before the block is freed.
typedef struct {
    WorldCollisionBody    body;          // Sphere linked into the world's collision lists
    WorldCollisionContact contacts[6];   // Contact table the body borrows; emptied after every update
    s32                   kickStrength;  // Random 0x3000..0x3FFF at set-up, one less every frame; scales the launch speed and the spin
    VECTOR                velocity;      // World units added to the prop's position per frame; `vy` is the hop on top of the constant sink
    SVECTOR               rotation;      // Euler angles (4096 per turn) the model's rotation is rebuilt from every frame
    SVECTOR               kickDirection; // Player's forward axis at the contact, normalised to 4096
    u16                   phase;         // An `ACROPOLIS_CAFETERIA_LOOSE_PROP_*` phase
} _AcropolisCafeteriaLoosePropWork;
STATIC_ASSERT_SIZEOF(_AcropolisCafeteriaLoosePropWork, 0xD8);

extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

static void _acropolisCafeteriaLoosePropExit(Task* task);

static u32     _gAkropolisCafeteriaAcropolisCafeteriaModel0FDFCPartVerts[1];
static SVECTOR _gAkropolisCafeteriaAcropolisCafeteriaModel0FDFCVerts[22];
static SVECTOR _gAkropolisCafeteriaAcropolisCafeteriaModel0FDFCNormals[18];
static TmdBone _gAkropolisCafeteriaAcropolisCafeteriaModel0FDFCSkeleton[1];
static u32     _gAkropolisCafeteriaAcropolisCafeteriaModel0FDFCStream[112];

/// Neutral colour modulation and an empty GPU sprite-code flag mask.
enum {
    ACROPOLIS_CAFETERIA_SPRITE_NEUTRAL_RGB = 128,
    ACROPOLIS_CAFETERIA_SPRITE_OPAQUE      = 0,
};

/// Placement layout and low-policy-nibble map mark committed by the follow-up scene.
enum {
    ACROPOLIS_CAFETERIA_FOLLOW_UP_PLACEMENT_VARIANT = 2,
    ACROPOLIS_CAFETERIA_FOLLOW_UP_SET_MAP_MARK      = 1,
};

/// Paired cue-2 bases in the loaded type-1 sound bank; cue 1 uses base + 1.
enum {
    ACROPOLIS_CAFETERIA_SURFACE1_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x2D,
    ACROPOLIS_CAFETERIA_SURFACE1_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x2F,
    ACROPOLIS_CAFETERIA_SURFACE2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x11,
    ACROPOLIS_CAFETERIA_SURFACE2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x13,
    ACROPOLIS_CAFETERIA_SURFACE3_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x01,
    ACROPOLIS_CAFETERIA_SURFACE3_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x03,
    ACROPOLIS_CAFETERIA_SURFACE3_JUMP_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x05,
};

#define ROOM_PART gAcropolisCafeteriaRoomLights
#include "assets/rooms/akropolis/cafeteria/lights.inc"
#include "assets/rooms/akropolis/cafeteria/sprites.inc"
#include "assets/rooms/akropolis/cafeteria/cameras.inc"
#include "assets/rooms/akropolis/cafeteria/ambient.inc"
#include "assets/rooms/akropolis/cafeteria/updates.inc"
#include "assets/rooms/akropolis/cafeteria/surfaces.inc"

static TmdBone _gAcropolisCafeteriaModel0F7A4Skeleton[1] = {
#include "assets/acropolis_cafeteria_model_0F7A4_skeleton.inc"
};

static u32 _gAcropolisCafeteriaModel0F7A4PartVerts[1] = {
#include "assets/acropolis_cafeteria_model_0F7A4_partVerts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel0F7A4Verts[41] = {
#include "assets/acropolis_cafeteria_model_0F7A4_verts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel0F7A4Normals[53] = {
#include "assets/acropolis_cafeteria_model_0F7A4_normals.inc"
};

static u32 _gAcropolisCafeteriaModel0F7A4Stream[307] = {
#include "assets/acropolis_cafeteria_model_0F7A4_stream.inc"
};

TmdSource gAcropolisCafeteriaModel0F7A4 = {
    0,
    2168,
    0,
    1,
    _gAcropolisCafeteriaModel0F7A4PartVerts,
    _gAcropolisCafeteriaModel0F7A4Verts,
    _gAcropolisCafeteriaModel0F7A4Normals,
    _gAcropolisCafeteriaModel0F7A4Skeleton,
    _gAcropolisCafeteriaModel0F7A4Stream,
};

#include "assets/rooms/akropolis/cafeteria/acropolis_cafeteria_model_0FDFC.inc"

/// Fixed light colours and ambient base for the Mendel pickup model.
///
/// All light-colour coefficients are one half in Q12; translation supplies the
/// GTE background RGB at 128 byte levels (2048 raw units). The model borrows
/// this SDK matrix while the cafeteria overlay is loaded.
static MATRIX _gAcropolisCafeteriaMendelPickupColorMatrix = {
    .m = { { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 } },
    .t = { ONE / 2, ONE / 2, ONE / 2 },
};

/// Fixed world-space light-direction rows for the Mendel pickup model.
///
/// Coefficients use Q12 (4096 for one); rows have equal XYZ values of 1/32,
/// -1/2 and -1/2. TMD drawing combines them with view and part rotation, without
/// using translation. The model borrows this matrix while the overlay is loaded.
static MATRIX _gAcropolisCafeteriaMendelPickupLightMatrix = {
    .m = { { ONE / 32, ONE / 32, ONE / 32 }, { -ONE / 2, -ONE / 2, -ONE / 2 }, { -ONE / 2, -ONE / 2, -ONE / 2 } },
    .t = { ONE / 2, ONE / 2, ONE / 2 },
};

/// Fixed light colours and ambient base for the stim pickup models.
///
/// Light-colour coefficients are one half in Q12; translation supplies the GTE
/// background RGB at 256 byte levels (4096 raw units). Placements using the stim
/// draw hook borrow this separate matrix while the cafeteria overlay is loaded.
static MATRIX _gAcropolisCafeteriaStimPickupColorMatrix = {
    .m = { { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 } },
    .t = { ONE, ONE, ONE },
};

/// Fixed world-space light-direction rows for the stim pickup models.
///
/// Q12 rows have equal XYZ values of 1/32, -1/2 and -1/2. TMD drawing uses the
/// rotation coefficients only. Placements using the stim draw hook borrow this
/// separate matrix while the cafeteria overlay is loaded.
static MATRIX _gAcropolisCafeteriaStimPickupLightMatrix = {
    .m = { { ONE / 32, ONE / 32, ONE / 32 }, { -ONE / 2, -ONE / 2, -ONE / 2 }, { -ONE / 2, -ONE / 2, -ONE / 2 } },
    .t = { ONE / 2, ONE / 2, ONE / 2 },
};

/// Fixed light colours and ambient base for the retained object-flag-10 draw hook.
///
/// Light-colour coefficients are one half in Q12; background RGB is 4096 raw
/// GTE units (256 byte levels). The hook's model identity is unproven. Its model
/// borrows this separate SDK matrix while the cafeteria overlay is loaded.
static MATRIX _gAcropolisCafeteriaObject10ColorMatrix = {
    .m = { { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 } },
    .t = { ONE, ONE, ONE },
};

/// Fixed light-direction rows for the retained object-flag-10 draw hook.
///
/// World-space Q12 rows have equal XYZ values of 1/32, -1/2 and -1/2;
/// translation is unused by TMD lighting. The hook's model identity is unproven.
/// Its model borrows this separate matrix while the cafeteria overlay is loaded.
static MATRIX _gAcropolisCafeteriaObject10LightMatrix = {
    .m = { { ONE / 32, ONE / 32, ONE / 32 }, { -ONE / 2, -ONE / 2, -ONE / 2 }, { -ONE / 2, -ONE / 2, -ONE / 2 } },
    .t = { ONE / 2, ONE / 2, ONE / 2 },
};

/// Fixed light colours and ambient base for the retained object-flag-11 draw hook.
///
/// Light-colour coefficients are one half in Q12; background RGB is 4096 raw
/// GTE units (256 byte levels). The hook's model identity is unproven. Its model
/// borrows this separate SDK matrix while the cafeteria overlay is loaded.
static MATRIX _gAcropolisCafeteriaObject11ColorMatrix = {
    .m = { { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 }, { ONE / 2, ONE / 2, ONE / 2 } },
    .t = { ONE, ONE, ONE },
};

/// Fixed light-direction rows for the retained object-flag-11 draw hook.
///
/// World-space Q12 rows have equal XYZ values of 1/32, -1/2 and -1/2;
/// translation is unused by TMD lighting. The hook's model identity is unproven.
/// Its model borrows this separate matrix while the cafeteria overlay is loaded.
static MATRIX _gAcropolisCafeteriaObject11LightMatrix = {
    .m = { { ONE / 32, ONE / 32, ONE / 32 }, { -ONE / 2, -ONE / 2, -ONE / 2 }, { -ONE / 2, -ONE / 2, -ONE / 2 } },
    .t = { ONE / 2, ONE / 2, ONE / 2 },
};

s32 gAcropolisCafeteriaStrangerRecoilVelocityX = 0;

s32 gAcropolisCafeteriaStrangerRecoilVelocityY = 0;

s32 gAcropolisCafeteriaStrangerRecoilVelocityZ = 0;

SVECTOR ActorContact_ScratchPosition = { 0 };

static void _acropolisCafeteriaLoosePropInit(Task* task);
static void _acropolisCafeteriaLoosePropUpdate(Task* task);
static void _acropolisCafeteriaLoosePropRequestExit(Task* task);
static void _acropolisCafeteriaObject10DrawTask(Task* task);
static void _acropolisCafeteriaObject11DrawTask(Task* task);

/// Chooses a puff's local XYZ offset using three successive shared LCG draws.
///
/// Borrows writable effect work and preserves `move.pad`. Writes integer
/// coordinates in the emitter's local frame: X 560..3179, five Y levels
/// -1900..-300, Z 2816..3839. Advances the shared LCG exactly three times.
static inline void _acropolisCafeteriaChoosePuffSpawnOffset(EffectWork* work)
{
    enum { PUFF_OFFSET_X_MIN         = 560,
           PUFF_OFFSET_X_SPAN        = 2620,
           PUFF_OFFSET_Y_TOP         = -300,
           PUFF_OFFSET_Y_LEVELS      = 5,
           PUFF_OFFSET_Y_STEP        = 400,
           PUFF_OFFSET_Z_MIN         = 2816,
           PUFF_OFFSET_Z_RANDOM_MASK = 0x3FF };
    u16 randomWord;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    randomWord      = gRandomLcgState >> 16;
    work->move.vx   = (u32)randomWord % PUFF_OFFSET_X_SPAN + PUFF_OFFSET_X_MIN;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    randomWord      = gRandomLcgState >> 16;
    work->move.vy   = PUFF_OFFSET_Y_TOP - (u16)((u32)randomWord % PUFF_OFFSET_Y_LEVELS) * PUFF_OFFSET_Y_STEP;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    randomWord      = gRandomLcgState >> 16;
    work->move.vz   = (randomWord & PUFF_OFFSET_Z_RANDOM_MASK) + PUFF_OFFSET_Z_MIN;
}

void acropolisCafeteriaPlayMovieTask(Task* movieTask)
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

void acropolisCafeteriaBlackoutTask(Task* task)
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

void acropolisCafeteriaStartMovieTask(Task* task)
{
    displaySpawnTaskFromTable(gAcropolisCafeteriaMovieTasks, ACROPOLIS_CAFETERIA_MOVIE_TASK_PLAYBACK, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(task);
}

void acropolisCafeteriaInitRoomEffectsTask(Task* task)
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
    task->msgTable = gAcropolisCafeteriaRoomEffectMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
    spawnOffset                    = &work->move;
    gAcropolisCafeteriaPuffEnabled = 0;
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
    gRoomEffectFlashId      = EFFECT_ACROPOLIS_CAFETERIA_FLASH;
    gRoomEffectTwinTrailId  = EFFECT_ACROPOLIS_CAFETERIA_TWIN_TRAIL;
    gRoomEffectSparkBurstId = EFFECT_ACROPOLIS_CAFETERIA_SPARK_BURST;
}
void acropolisCafeteriaPuffEmitterTask(Task* task)
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
    if (gAcropolisCafeteriaPuffEnabled == 0) {
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
            _acropolisCafeteriaChoosePuffSpawnOffset(work);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomWord      = gRandomLcgState >> 16;
            spawnSizeBase   = spawnFlags + PUFF_EMITTER_SIZE_MIN;
            effectSpawn(EFFECT_ACROPOLIS_CAFETERIA_PUFF, coord, (randomWord & PUFF_EMITTER_SIZE_RANDOM_MASK) + spawnSizeBase, &work->move);
        }
    }
    work->scale = gGameSession->location.view;
}

/// Sets the puff quad's UVs to its current 48 x 48 texel animation cell.
///
/// `work->age` is elapsed active ticks and `work->step` is ticks per cell.
/// The period must be positive, with 0 <= age < 10 * period. Cells 0..9 run
/// left to right across five columns, then continue on the second row.
/// UV endpoints include the last texel (origin + 47), so U stays in 0..239
/// and V in 0..95. The quad and work are distinct borrowed live objects;
/// only the eight UV bytes are written, without advancing the animation.
static inline void _acropolisCafeteriaSetPuffCellUvs(POLY_FT4* quad, const EffectWork* work)
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

void acropolisCafeteriaPuffTask(Task* task)
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
    if (gAcropolisCafeteriaPuffEnabled != 0) {
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
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->scale     = (gRandomLcgState >> 16) & PUFF_ROTATION_MASK;
                work->angle     = task->spawnArg1.halves.low & PUFF_SPAWN_SIZE_MASK;
                work->move.vx   = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = ((gRandomLcgState >> 16) & 0xF) + 4;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = -((gRandomLcgState >> 16) & 0xF) - 4;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->step      = ((gRandomLcgState >> 16) & 3) + 3;
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
            _acropolisCafeteriaSetPuffCellUvs(quad, work);
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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

void acropolisCafeteriaModelWanderTask(Task* task)
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
            work->period    = WANDER_AMBIENT_EXIT_X;
            work->angle     = 0;
            work->index     = WANDER_AMBIENT_TURN;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = (gRandomLcgState >> 16) & 0xF00;
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
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = 0;
            work->scale    -= ((gRandomLcgState >> 16) & 0xFF) - 0x80;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % 30) == 0) {
                work->index = WANDER_TIMED_MOVE;
            }
            if (work->age >= WANDER_DEPART_AGE) {
                work->index = WANDER_DEPART;
            }
            break;
        case WANDER_TIMED_MOVE:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                previousYaw = work->scale;
                if (previousYaw > WANDER_EXIT_YAW) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    nextYaw         = previousYaw - 0x10;
                    nextYaw        -= (gRandomLcgState >> 16) & 0x3F;
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    nextYaw         = previousYaw + 0x10;
                    nextYaw        += (gRandomLcgState >> 16) & 0x3F;
                }
                work->scale = nextYaw;
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = WANDER_MOVE_SPEED_Q4;
            if ((u16)((gRandomLcgState >> 16) % 30) == 0) {
                work->index = WANDER_TIMED_TURN;
            }
            if (work->age >= WANDER_DEPART_AGE) {
                work->index = WANDER_DEPART;
            }
            break;
        case WANDER_AMBIENT_TURN:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale    -= ((gRandomLcgState >> 16) & 0xFF) - 0x80;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % 240) == 0) {
                work->scale = WANDER_EXIT_YAW;
                work->angle = WANDER_MOVE_SPEED_Q4;
                work->index = WANDER_AMBIENT_MOVE;
            }
            break;
        case WANDER_AMBIENT_MOVE:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 7) == 0) {
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
                departureYaw   -= (gRandomLcgState >> 16) & 0x3F;
            } else {
                randomIncrement = RANDOM_LCG_INCREMENT;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + randomIncrement;
                departureYaw   += 0x10;
                departureYaw   += (gRandomLcgState >> 16) & 0x3F;
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

s32 acropolisCafeteriaSetPuffEnabled(Task* task, s32 messageId, s32 enabled, s32 unused)
{
    GfxCoord* coord;

    coord                          = task->extra.coordBody->coord;
    gAcropolisCafeteriaPuffEnabled = enabled;
    if (enabled != 0) {
        effectSpawn(EFFECT_ACROPOLIS_CAFETERIA_PUFF_EMITTER, coord, 0, NULL);
    }
    return 0;
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void acropolisCafeteriaRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void acropolisCafeteriaRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void acropolisCafeteriaRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"

    /// Places a loose model above the player with a positive-Z offset and links its sphere.
    ///
    /// Requires a fresh state-zero TMD task and a live player. Owns one primary-heap
    /// work block with six contacts; allocation failure kills the uninitialized task.
    /// The exit callback unlinks the sphere before task teardown frees the work.
    static void _acropolisCafeteriaLoosePropInit(Task * task)
    {
        TmdObject*                        model;
        GfxCoord*                         coord;
        _AcropolisCafeteriaLoosePropWork* work;
        GfxCoord*                         playerCoord;

        model = task->extra.tmd;
        coord = model->coords;
        work  = memCalloc(sizeof(*work), false);
        if (work == NULL) {
            taskKill(task);
            return;
        }
        task->work         = work;
        task->exitCallback = _acropolisCafeteriaLoosePropExit;
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
    ///
    /// Requires initialized task work in the hopping or sliding phase. X/Z velocity
    /// is in integer world units per tick: retain 6/7, truncating toward zero, then
    /// snap magnitudes below 9 to zero. Both axes stopping increments the current
    /// phase once, including during a hop; vertical velocity is preserved.
    static inline void _acropolisCafeteriaDampLoosePropSlide(_AcropolisCafeteriaLoosePropWork * work)
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
    ///
    /// Requires initialized work and a linked sphere. Position increments are integer
    /// world units per tick; Euler angles and the normalized kick direction use 4096
    /// units per turn and per unit vector respectively. Every update clears contacts.
    static void _acropolisCafeteriaLoosePropUpdate(Task * task)
    {
        MATRIX*                           scratchEnd;
        _AcropolisCafeteriaLoosePropWork* work;
        GfxCoord*                         coord;
        SVECTOR*                          kickDirection;
        s32                               launchMagnitude;

        scratchEnd                   = SCRATCH_STACK_CURSOR(MATRIX);
        SCRATCH_STACK_CURSOR(MATRIX) = scratchEnd - 1;
        work                         = task->work;
        coord                        = task->extra.tmd->coords;
        work->kickStrength--;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[1]  += 0x80;
        actorRenderComposeCoord(coord);
        switch (work->phase) {
            case ACROPOLIS_CAFETERIA_LOOSE_PROP_RESTING:
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
            case ACROPOLIS_CAFETERIA_LOOSE_PROP_HOPPING:
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
            case ACROPOLIS_CAFETERIA_LOOSE_PROP_SLIDING:
                _acropolisCafeteriaDampLoosePropSlide(work);
                coord->coord.t[0] += work->velocity.vx;
                coord->coord.t[1] += work->velocity.vy;
                coord->coord.t[2] += work->velocity.vz;
                break;
            case ACROPOLIS_CAFETERIA_LOOSE_PROP_SETTLING:
                work->rotation.vx = (work->rotation.vx * 2) / 3;
                if (ABS(work->rotation.vx) < 9) {
                    work->rotation.vx = 0;
                }
                work->rotation.vz = (work->rotation.vz * 2) / 3;
                if (ABS(work->rotation.vz) < 9) {
                    work->rotation.vz = 0;
                }
                if (((u16)work->rotation.vx | (u16)work->rotation.vz) == 0) {
                    work->phase = ACROPOLIS_CAFETERIA_LOOSE_PROP_RESTING;
                }
                break;
        }
        RotMatrix(&work->rotation, &coord->coord);
        worldCollisionClearContacts(work->contacts);
        SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
    }

    /// Selects the loose-prop exit handler for the next task dispatch.
    static void _acropolisCafeteriaLoosePropRequestExit(Task * task)
    {
        enum { LOOSE_PROP_TASK_EXIT = 3 };
        task->state = LOOSE_PROP_TASK_EXIT;
    }

    /// Unlinks the initialized loose prop's sphere before teardown frees its work and model.
    static void _acropolisCafeteriaLoosePropExit(Task * task)
    {
        _AcropolisCafeteriaLoosePropWork* work;

        work = task->work;
        worldCollisionUnlinkBody(&work->body);
        taskKill(task);
    }

    /// Task-state dispatch for the cafeteria's loose prop.
    ///
    /// Slots 0..3 initialize, update motion, request exit and unlink/kill the task.
    /// No terminator; the dispatcher copies all four callback pointers to the stack
    /// before indexing by `Task::state`. The initialized work and model stay live
    /// through slot 3; slot 0 may kill the task if allocation fails.
    static const TaskFuncTable4 _gAcropolisCafeteriaLoosePropStates = { {
        _acropolisCafeteriaLoosePropInit,
        _acropolisCafeteriaLoosePropUpdate,
        _acropolisCafeteriaLoosePropRequestExit,
        _acropolisCafeteriaLoosePropExit,
    } };

    void acropolisCafeteriaLoosePropTask(Task * task)
    {
        TaskFuncTable4 states;

        states = _gAcropolisCafeteriaLoosePropStates;
        states.funcs[task->state](task);
    }

#include "../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../shared/actor/actor_contacts_push.inc.c"

    void acropolisCafeteriaMendelPickupTask(Task * task)
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
        if (areaGetCurrentObjectState((u8)placement->placeKey) != ACROPOLIS_CAFETERIA_PICKUP_COLLECTED) {
            model->lightMtx = &_gAcropolisCafeteriaMendelPickupLightMatrix;
            model->colorMtx = &_gAcropolisCafeteriaMendelPickupColorMatrix;
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
    void acropolisCafeteriaStimPickupTask(Task * task)
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
        model->lightMtx = &_gAcropolisCafeteriaStimPickupLightMatrix;
        model->colorMtx = &_gAcropolisCafeteriaStimPickupColorMatrix;
        // Retire collected placements only in the active view; buffer allocation retries otherwise.
        if (placeState == ACROPOLIS_CAFETERIA_PICKUP_COLLECTED) {
            model->flags &= (u16)~TMD_OBJECT_FLAGGED_PASS;
            taskCallExit(task);
        } else {
            model->flags    = TMD_OBJECT_FLAGGED_PASS;
            model->otOffset = 0;
            tmdAllocPrimitiveBuffer(model);
        }
    }
    /// Draws an unreferenced model hook in view 9 until placed-object flag 10 is collected.
    ///
    /// Requires a TMD body, a live auxiliary heap and cafeteria lighting storage.
    /// Replaces X rotation with a quarter turn even when collection hides the model.
    /// No spawn row or call references this retained hook; its model identity is unproven.
    static void _acropolisCafeteriaObject10DrawTask(Task * task)
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
        model->lightMtx = &_gAcropolisCafeteriaObject10LightMatrix;
        model->colorMtx = &_gAcropolisCafeteriaObject10ColorMatrix;
        if (areaGetCurrentObjectState(OBJECT_FLAG_INDEX) == ACROPOLIS_CAFETERIA_PICKUP_COLLECTED) {
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            model->flags    = TMD_OBJECT_FLAGGED_PASS;
            model->otOffset = 0;
            tmdAllocPrimitiveBuffer(model);
        }
        gfxRotMatrixX(&task->extra.tmd->coords->coord, OBJECT_QUARTER_TURN, GRAPHICS_ROTATION_REPLACE);
    }
    /// Draws an unreferenced model hook in views 6, 7 and 10 until object flag 11 is collected.
    ///
    /// Requires a TMD body, a live auxiliary heap and cafeteria lighting storage.
    /// No spawn row or call references this retained hook; its model identity is unproven.
    static void _acropolisCafeteriaObject11DrawTask(Task * task)
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
                model->lightMtx = &_gAcropolisCafeteriaObject11LightMatrix;
                model->colorMtx = &_gAcropolisCafeteriaObject11ColorMatrix;
                break;
            default:
                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                return;
        }
        if (areaGetCurrentObjectState(OBJECT_FLAG_INDEX) == ACROPOLIS_CAFETERIA_PICKUP_COLLECTED) {
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            model->flags    = TMD_OBJECT_FLAGGED_PASS;
            model->otOffset = 0;
            tmdAllocPrimitiveBuffer(model);
        }
    }
