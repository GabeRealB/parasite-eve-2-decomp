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
        SndEvt_EnqueueType7(SOUND_FLARE_USE, 1);
        effectKillTask(mem, arg0);
        return;
    }
    mem->age = mem->age + 1;
    if (arg0->state == 0) {
        pan = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(SOUND_FLARE_USE, pan, (s8)worldCoordGetOriginAudioDepth(coord));
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

/// PROVISIONAL: written before `Task` was processed, so the statements
/// about `Task` fields rest on unverified names. Rewrite once `Task` is done.
/// Flies one spark away from the player and draws it.
///
/// On the first frame it starts from the player's position, picks a random
/// heading and pitch, and turns those into a velocity in the player's frame of
/// reference. Every frame after that it advances by that velocity and draws the
/// next sprite frame, stepping the frame on every second tick. Releases once
/// all eight frames have been drawn.
void flareSparkTask(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    GfxCoord*         player;
    GfxRotationWords* destinationRotation;
    GfxRotationWords* sourceRotation;
    u32               rng;
    s32               temp_lo;

    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    mem->age = mem->age + 1;
    if (arg0->state == 0) {
        player                      = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
        destinationRotation         = (GfxRotationWords*)&coord->coord;
        sourceRotation              = (GfxRotationWords*)&player->coord;
        destinationRotation->m00M01 = sourceRotation->m00M01;
        destinationRotation->m02M10 = sourceRotation->m02M10;
        destinationRotation->m11M12 = sourceRotation->m11M12;
        destinationRotation->m20M21 = sourceRotation->m20M21;
        destinationRotation->m22    = sourceRotation->m22;
        coord->composeStamp         = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->period     = arg0->spawnArg1.value & 0xFFF;
        mem->scale      = (rng >> 16) & 0xFFF;
        gRandomLcgState = rng;
        mem->angle      = mem->period >> 5;
        mem->move.vx    = (rsin(mem->scale) * mem->angle) >> 12;
        temp_lo         = rcos(mem->scale) * mem->angle;
        mem->move.vz    = 0x100;
        mem->move.vy    = temp_lo >> 12;
        gte_SetRotMatrix(&player->coord);
        gte_ldv0(&mem->move);
        gte_rtv0();
        gte_stsv(&mem->move);
        arg0->state = 1;
    }
    coord->coord.t[0]  += mem->move.vx;
    coord->coord.t[1]  += mem->move.vy;
    coord->coord.t[2]  += mem->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    if (!(mem->age & 1)) {
        mem->index = mem->index + 1;
    }
    if (mem->index < 8) {
        spriteQuadDraw(coord, mem->index, mem->period, mem->scale);
        return;
    }
    effectKillTask(mem, arg0);
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
