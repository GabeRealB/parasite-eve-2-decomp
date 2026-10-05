#include "room_visual_effects.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "gameplay/actor_render.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/room_effects.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/room_common.h"

/// A mote's spawn argument word, read through its packed bytes.
///
/// The low two bits choose the motion, and the low twelve also give the
/// sprite's half-extent, so the two overlap. A steady mote keeps full
/// brightness and moves at `verticalSpeed`; any other rises at that speed plus
/// a random 0..0x3F while it brightens.
typedef struct {
    u16 extentPalette; // Bits 0..11 sprite half-extent, bits 12..15 palette (0 the default)
    u8  verticalSpeed; // World units moved per tick
    s8  lifetime;      // Ticks of age at which the mote has faded; the fade starts 8 ticks earlier
} _RoomFxMoteArg;
STATIC_ASSERT_SIZEOF(_RoomFxMoteArg, 0x4);

/// Bit of the mote argument: steady, moving down.
#define ROOM_VISUAL_EFFECTS_MOTE_STEADY 0x1
/// Bit of the mote argument: steady, moving up.
#define ROOM_VISUAL_EFFECTS_MOTE_STEADY_UP 0x2
/// `_RoomFxMoteArg::extentPalette` bits giving the sprite's half-extent.
#define ROOM_VISUAL_EFFECTS_MOTE_EXTENT 0x0FFF
/// `_RoomFxMoteArg::extentPalette` bits giving the sprite's palette.
#define ROOM_VISUAL_EFFECTS_MOTE_PALETTE 0xF000

/// Moves a mote each active tick and queues its next sprite frame on odd ages.
///
/// Borrowed `work` and `coord`, including the coordinate's ancestors, must be
/// live and writable. The caller advances `work->age` before this call and
/// changes brightness afterward. `work->move.vy` is a signed displacement in
/// parent-space coordinate units; the composed transform is refreshed every call.
/// On odd ages, `work->index` advances before drawing; its low two bits select
/// one of four frames. `work->angle` holds the half-extent (0..4095 coordinate
/// units), `work->scale` the brightness (0..128), and `work->period` the palette
/// selector already packed into bits 12..15. `packedTextureStrip` is 0 for the
/// first strip or `ROOM_VISUAL_EFFECTS_MOTE_TEXTURE_ROW_1` for the second;
/// its bits 12..15 are combined with the half-extent, not the palette.
static inline void _roomVisualEffectsMoveAndDrawMote(EffectWork* work, GfxCoord* coord, u16 packedTextureStrip)
{
    enum { MOTE_ANIMATION_INTERVAL_TICKS = 2 };

    // Refresh the composed position even on ticks that queue no sprite.
    coord->coord.t[1]  += work->move.vy;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    if (work->age & (MOTE_ANIMATION_INTERVAL_TICKS - 1)) {
        work->index++;
        _roomVisualEffectsDrawMote(coord, work->index, work->angle | packedTextureStrip, work->scale | work->period);
    }
}

/// Updates a vertically drifting, animated mote until its brightness has faded.
///
/// `task` must own a coordinate body and an `EffectWork` with zero age and
/// animation index in `spawnArg2.pointer`, as supplied by `Gp_SpawnEff`.
/// State 0 initializes it; state 1 rises and brightens using texture strip 1,
/// and state 2 starts at full brightness and moves using strip 0.
/// The first tick neither moves nor draws.
/// `spawnArg1` is a packed word: bits 0..11 are the world-unit half-extent
/// (including the overlapping motion bits 0..1), bits 12..15 select a palette
/// (0 default), bits 16..23 are an unsigned speed in coordinate units per
/// active tick, and bits 24..31 are a signed lifetime in active ticks.
/// Either motion bit selects steady motion; bit 1 makes that motion upward.
/// Otherwise upward speed adds a random 0..63. Age advances only while room
/// effect control is running; later ticks move along local Y and draw every
/// other tick. Brightness fades by 16 after age exceeds lifetime minus eight,
/// and the following tick releases the task's work once brightness is zero.
/// Nonzero room effect control pauses it; control four or above cancels it.
static inline void _roomVisualEffectsMoteTask(Task* task)
{
    enum { MOTE_INITIALIZE,
           MOTE_RISE,
           MOTE_STEADY,
           MOTE_INITIAL_BRIGHTNESS = 0x20,
           MOTE_FULL_BRIGHTNESS    = 0x80,
           MOTE_BRIGHTEN_STEP      = 0x20,
           MOTE_FADE_STEP          = 0x10,
           MOTE_FADE_LEAD_TICKS    = 8,
           MOTE_RANDOM_SPEED_MASK  = 0x3F };

    EffectWork* work;
    GfxCoord*   coord;
    s32         lifetime;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    // scale = brightness, angle = half-extent, period = packed palette, step = lifetime.
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case MOTE_INITIALIZE:
                // The byte view keeps speed unsigned and lifetime signed.
                if (task->spawnArg1.value & (ROOM_VISUAL_EFFECTS_MOTE_STEADY | ROOM_VISUAL_EFFECTS_MOTE_STEADY_UP)) {
                    work->scale   = MOTE_FULL_BRIGHTNESS;
                    work->angle   = ((const _RoomFxMoteArg*)&task->spawnArg1)->extentPalette & ROOM_VISUAL_EFFECTS_MOTE_EXTENT;
                    work->period  = ((const _RoomFxMoteArg*)&task->spawnArg1)->extentPalette & ROOM_VISUAL_EFFECTS_MOTE_PALETTE;
                    lifetime      = ((const _RoomFxMoteArg*)&task->spawnArg1)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = ((const _RoomFxMoteArg*)&task->spawnArg1)->verticalSpeed;
                    work->move.vz = 0;
                    if (task->spawnArg1.value & ROOM_VISUAL_EFFECTS_MOTE_STEADY_UP) {
                        work->move.vy = -work->move.vy;
                    }
                    task->state = MOTE_STEADY;
                } else {
                    work->scale   = MOTE_INITIAL_BRIGHTNESS;
                    work->angle   = ((const _RoomFxMoteArg*)&task->spawnArg1)->extentPalette & ROOM_VISUAL_EFFECTS_MOTE_EXTENT;
                    work->period  = ((const _RoomFxMoteArg*)&task->spawnArg1)->extentPalette & ROOM_VISUAL_EFFECTS_MOTE_PALETTE;
                    lifetime      = ((const _RoomFxMoteArg*)&task->spawnArg1)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = -((const _RoomFxMoteArg*)&task->spawnArg1)->verticalSpeed - (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & MOTE_RANDOM_SPEED_MASK);
                    work->move.vz = 0;
                    task->state   = (task->spawnArg1.value & ROOM_VISUAL_EFFECTS_MOTE_STEADY) + MOTE_RISE;
                }
                break;
            case MOTE_RISE:
                _roomVisualEffectsMoveAndDrawMote(work, coord, ROOM_VISUAL_EFFECTS_MOTE_TEXTURE_ROW_1);
                // Draw before changing brightness; release on the tick after it reaches zero.
                if (work->scale > 0) {
                    if (work->step - MOTE_FADE_LEAD_TICKS < work->age) {
                        work->scale -= MOTE_FADE_STEP;
                    } else if (work->scale < MOTE_FULL_BRIGHTNESS) {
                        work->scale += MOTE_BRIGHTEN_STEP;
                    }
                } else {
                    effectKillTask(work, task);
                }
                break;
            case MOTE_STEADY:
                _roomVisualEffectsMoveAndDrawMote(work, coord, 0);
                if (work->scale > 0) {
                    if (work->step - MOTE_FADE_LEAD_TICKS < work->age) {
                        work->scale -= MOTE_FADE_STEP;
                    }
                } else {
                    effectKillTask(work, task);
                }
                break;
        }
    }
}
