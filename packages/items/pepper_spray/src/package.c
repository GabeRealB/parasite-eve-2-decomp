#include "pepper_spray.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include <math/gte.h>
#include <core/pe2.h>

#include <gameplay/display.h>
#include <gameplay/actor_render.h>
#include <gameplay/attachment_state.h>
#include <gameplay/attachments.h>
#include <gameplay/effects.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/world_coords.h>

#include <engine/coord.h>
#include <engine/display.h>
#include <engine/display_types.h>
#include <engine/random.h>
#include <engine/gfx.h>
#include <engine/gfx_types.h>
#include <engine/scratch.h>
#include <engine/session.h>
#include <engine/sound.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>
#include <effects/sprite_quad/sprite_quad.h>

PE2_EDITOR_LABEL("Spray")
/// The frames sparks are sprayed until, the frame the item applies on, and the frame the spray ends on.
PE2_OPTION_DEF(SPRAY_FRAMES, int, 20)
PE2_OPTION_DEF(APPLY_FRAME, int, 8)
PE2_OPTION_DEF(END_FRAME, int, 36)

PE2_EDITOR_LABEL("Sparks")
/// A spark's size, plus a random part under the jitter, a power of two.
PE2_OPTION_DEF(SPARK_SIZE, int, 0x680)
PE2_OPTION_DEF(SPARK_SIZE_JITTER, int, 0x200)
/// Its speed along the player's facing, and its speed across, as its size shifted down.
PE2_OPTION_DEF(SPARK_SPEED, int, 0x100)
PE2_OPTION_DEF(SPARK_SPREAD_SHIFT, int, 5)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(SPARK_SIZE_JITTER)), spark_size_jitter_is_a_mask);
STATIC_ASSERT(PE2_OPTION(APPLY_FRAME) < PE2_OPTION(SPRAY_FRAMES) && PE2_OPTION(SPRAY_FRAMES) <= PE2_OPTION(END_FRAME), frames_in_order);

void pepperSprayEffectTask(Task* task)
{
    enum {
        CAST_STATE_INITIALIZE = 0,
        CAST_STATE_EMITTING   = 1,
    };
    EffectWork*      work;
    GfxCoord*        coord;
    AttachmentState* attachment;
    s32              pan;
    s16              age;
    EffectWork*      spawned;
    s32              sizeRng;

    attachment = &gAttachmentState;
    work       = task->spawnArg2.pointer;
    coord      = task->extra.coordBody->coord;
    if ((attachment->effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING)) {
        sndEvtRequestScriptStop(SOUND_PEPPER_SPRAY_USE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        effectKillTask(work, task);
        return;
    }
    work->age = work->age + 1;
    if (task->state == CAST_STATE_INITIALIZE) {
        pan = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(SOUND_PEPPER_SPRAY_USE, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        task->state = CAST_STATE_EMITTING;
    }
    age = work->age;
    if (age < PE2_OPTION(SPRAY_FRAMES)) {
        if (age == PE2_OPTION(APPLY_FRAME)) {
            attachment->flags |= ATTACHMENT_FLAG_APPLY_STATS;
        }
        sizeRng         = RANDOM_LCG_NEXT(gRandomLcgState);
        gRandomLcgState = sizeRng;
        spawned         = effectSpawn(EFFECT_PEPPER_SPRAY_SPARK, coord, (((u32)sizeRng >> 16) & (PE2_OPTION(SPARK_SIZE_JITTER) - 1)) + PE2_OPTION(SPARK_SIZE), 0);
        if (spawned != NULL) {
            taskReparent(task, spawned->task);
        }
        return;
    }
    // Normal completion lets the cue finish; only cancellation stops it explicitly.
    if (age == PE2_OPTION(END_FRAME)) {
        effectKillTask(work, task);
    }
}

void pepperSpraySparkTask(Task* task)
{
    enum {
        SPARK_STATE_INITIALIZE    = 0,
        SPARK_STATE_FLYING        = 1,
        SPARK_SIZE_MASK           = 0xFFF,
        SPARK_ANGLE_TURN          = 0x1000,
        SPARK_TRIG_FRACTION_BITS  = 12,
        SPARK_TEXTURE_FRAME_COUNT = 8
    };
    EffectWork*     work;
    GfxCoord*       sparkCoord;
    const GfxCoord* playerCoord;
    u32             directionRng;
    s32             verticalProduct;

    work       = task->spawnArg2.pointer;
    sparkCoord = task->extra.coordBody->coord;
    work->age  = work->age + 1;
    if (task->state == SPARK_STATE_INITIALIZE) {
        // Keep the spawn translation and orient the spark in the player's local frame.
        playerCoord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
        gfxCopyRotationWords(&sparkCoord->coord, &playerCoord->coord);
        sparkCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(sparkCoord);

        // Reuse the effect parameters for sprite size, bearing and transverse speed.
        directionRng    = RANDOM_LCG_NEXT(gRandomLcgState);
        work->period    = task->spawnArg1.value & SPARK_SIZE_MASK;
        work->scale     = (directionRng >> 16) & (SPARK_ANGLE_TURN - 1);
        gRandomLcgState = directionRng;
        work->angle     = work->period >> PE2_OPTION(SPARK_SPREAD_SHIFT);
        work->move.vx   = (rsin(work->scale) * work->angle) >> SPARK_TRIG_FRACTION_BITS;
        verticalProduct = rcos(work->scale) * work->angle;
        work->move.vz   = PE2_OPTION(SPARK_SPEED);
        work->move.vy   = verticalProduct >> SPARK_TRIG_FRACTION_BITS;
        gte_SetRotMatrix(&playerCoord->coord);
        gte_ldv0(&work->move);
        gte_rtv0();
        gte_stsv(&work->move);
        task->state = SPARK_STATE_FLYING;
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
    if (work->index < SPARK_TEXTURE_FRAME_COUNT) {
        spriteQuadDraw(sparkCoord, work->index, work->period, work->scale);
        return;
    }
    effectKillTask(work, task);
}

/// The spark's sprite: eight 32-texel cells.
#define SPRITE_QUAD_CLUT       getClut(272, 268)
#define SPRITE_QUAD_CELL_WIDTH 32
#define SPRITE_QUAD_TOP_V      0x18
#define SPRITE_QUAD_BOTTOM_V   (SPRITE_QUAD_TOP_V + SPRITE_QUAD_CELL_WIDTH - 1)
#define SPRITE_QUAD_SCALE      (SPRITE_QUAD_CELL_WIDTH - 1)
#include <effects/sprite_quad/draw.inc.c>
