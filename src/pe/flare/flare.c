#include "pe/flare.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/sprite_quad.h"

/// Gives a spark the player's local rotation while preserving its spawn translation.
///
/// Both matrices must be live and word-aligned, with `sparkMatrix` writable and
/// `playerMatrix`'s nine signed, 12-fractional-bit coefficients initialized.
/// Writes exactly the 18 coefficient bytes, preserving the two alignment bytes
/// and all translation values. The caller must invalidate the spark coordinate's
/// composition cache before using it. Retains no pointers and changes no GTE state.
static inline void _flareCopySparkRotation(MATRIX* sparkMatrix, const MATRIX* playerMatrix)
{
    GfxRotationWords*       sparkRotation  = (GfxRotationWords*)sparkMatrix;
    const GfxRotationWords* playerRotation = (const GfxRotationWords*)playerMatrix;

    sparkRotation->m00M01 = playerRotation->m00M01;
    sparkRotation->m02M10 = playerRotation->m02M10;
    sparkRotation->m11M12 = playerRotation->m11M12;
    sparkRotation->m20M21 = playerRotation->m20M21;
    sparkRotation->m22    = playerRotation->m22;
}

/// This overlay's id, the `u16` every package opens with.

/// PROVISIONAL: written before `Task` was processed, so the statements
/// about `Task` fields rest on unverified names. Rewrite once `Task` is done.
/// Emits the flare's shower of sparks.
///
/// Starts the sound cue panned to the object, then spawns one spark a frame for
/// the first 20 frames, each in a random direction, reparenting itself to the
/// last one spawned. Releases at frame 36, by which time the sparks it created
/// are running on their own.
///
/// A cancelled or interrupted cast stops the cue and releases immediately.
void flareEffectTask(Task* arg0)
{
    EffectWork*      mem;
    GfxCoord*        coord;
    AttachmentState* state;
    s32              pan;
    s16              tick;
    EffectWork*      spawned;
    s32              rng;

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((state->effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING)) {
        sndEvtRequestScriptStop(SOUND_FLARE_USE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        effectKillTask(mem, arg0);
        return;
    }
    mem->age = mem->age + 1;
    if (arg0->state == 0) {
        pan = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(SOUND_FLARE_USE, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        arg0->state = 1;
    }
    tick = mem->age;
    if (tick < 0x14) {
        if (tick == 8) {
            state->flags |= ATTACHMENT_FLAG_APPLY_STATS;
        }
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        spawned         = Gp_SpawnEff(EFFECT_FLARE_SPARK, coord, (((u32)rng >> 16) & 0x1FF) + 0x680, 0);
        if (spawned != NULL) {
            taskReparent(arg0, spawned->task);
        }
        return;
    }
    if (tick == 0x24) {
        effectKillTask(mem, arg0);
    }
}

void flareSparkTask(Task* task)
{
    enum {
        FLARE_SPARK_STATE_INITIALIZE    = 0,
        FLARE_SPARK_STATE_FLYING        = 1,
        FLARE_SPARK_SIZE_MASK           = 0xFFF,
        FLARE_SPARK_ANGLE_TURN          = 0x1000,
        FLARE_SPARK_SIZE_TO_SPEED_SHIFT = 5,
        FLARE_SPARK_TRIG_FRACTION_BITS  = 12,
        FLARE_SPARK_FORWARD_SPEED       = 0x100,
        FLARE_SPARK_TEXTURE_FRAME_COUNT = 8
    };
    EffectWork*     work;
    GfxCoord*       sparkCoord;
    const GfxCoord* playerCoord;
    u32             directionRng;
    s32             verticalProduct;

    work       = task->spawnArg2.pointer;
    sparkCoord = task->extra.coordBody->coord;
    work->age  = work->age + 1;
    if (task->state == FLARE_SPARK_STATE_INITIALIZE) {
        // Keep the spawn translation and orient the spark in the player's local frame.
        playerCoord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
        _flareCopySparkRotation(&sparkCoord->coord, &playerCoord->coord);
        sparkCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(sparkCoord);

        // Reuse the effect parameters for sprite size, bearing and transverse speed.
        directionRng    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->period    = task->spawnArg1.value & FLARE_SPARK_SIZE_MASK;
        work->scale     = (directionRng >> 16) & (FLARE_SPARK_ANGLE_TURN - 1);
        gRandomLcgState = directionRng;
        work->angle     = work->period >> FLARE_SPARK_SIZE_TO_SPEED_SHIFT;
        work->move.vx   = (rsin(work->scale) * work->angle) >> FLARE_SPARK_TRIG_FRACTION_BITS;
        verticalProduct = rcos(work->scale) * work->angle;
        work->move.vz   = FLARE_SPARK_FORWARD_SPEED;
        work->move.vy   = verticalProduct >> FLARE_SPARK_TRIG_FRACTION_BITS;
        gte_SetRotMatrix(&playerCoord->coord);
        gte_ldv0(&work->move);
        gte_rtv0();
        gte_stsv(&work->move);
        task->state = FLARE_SPARK_STATE_FLYING;
    }

    // Move even on initialization; refresh the cached translation before drawing.
    sparkCoord->coord.t[0]  += work->move.vx;
    sparkCoord->coord.t[1]  += work->move.vy;
    sparkCoord->coord.t[2]  += work->move.vz;
    sparkCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(sparkCoord);
    if (!(work->age & 1)) {
        work->index = work->index + 1;
    }
    if (work->index < FLARE_SPARK_TEXTURE_FRAME_COUNT) {
        spriteQuadDraw(sparkCoord, work->index, work->period, work->scale);
        return;
    }
    effectKillTask(work, task);
}

/// Spark palette: VRAM X=272 words, Y=268 scanlines.
#define SPRITE_QUAD_CLUT getClut(272, 268)
/// Texel width and horizontal stride of each of the spark's eight texture frames.
#define SPRITE_QUAD_CELL_WIDTH 32
/// Inclusive top texel row of the spark strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x18
#define SPRITE_QUAD_V1    0x37
/// Perspective-sizing multiplier for the flare spark.
///
/// Uses the cell's inclusive 31-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"
