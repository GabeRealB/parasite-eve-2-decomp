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

/// A drifting mote. The first tick unpacks the spawn argument: a mote with
/// either low bit set starts at full brightness and moves at its given
/// vertical speed (upwards when bit 1 is set) in state 2; otherwise it starts
/// dim, rises at its speed plus a random 0..0x3F and brightens as it goes, in
/// state 1. Every tick moves it, every other tick advances its drawing phase
/// and draws it, and within eight ticks of its lifetime it fades out, releasing
/// its work block once dark. It pauses while the room's event state is set
/// and releases the block when that state reaches 4.
static inline void RoomFx_MoteTask(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    s32         lifetime;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                // Unpack the spawn argument and choose the motion.
                if (task->spawnArg1.value & (ROOM_VISUAL_EFFECTS_MOTE_STEADY | ROOM_VISUAL_EFFECTS_MOTE_STEADY_UP)) {
                    work->scale   = 0x80;
                    work->angle   = ((_RoomFxMoteArg*)&task->spawnArg1)->extentPalette & ROOM_VISUAL_EFFECTS_MOTE_EXTENT;
                    work->period  = ((_RoomFxMoteArg*)&task->spawnArg1)->extentPalette & ROOM_VISUAL_EFFECTS_MOTE_PALETTE;
                    lifetime      = ((_RoomFxMoteArg*)&task->spawnArg1)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = ((_RoomFxMoteArg*)&task->spawnArg1)->verticalSpeed;
                    work->move.vz = 0;
                    if (task->spawnArg1.value & ROOM_VISUAL_EFFECTS_MOTE_STEADY_UP) {
                        work->move.vy = -work->move.vy;
                    }
                    task->state = 2;
                } else {
                    work->scale   = 0x20;
                    work->angle   = ((_RoomFxMoteArg*)&task->spawnArg1)->extentPalette & ROOM_VISUAL_EFFECTS_MOTE_EXTENT;
                    work->period  = ((_RoomFxMoteArg*)&task->spawnArg1)->extentPalette & ROOM_VISUAL_EFFECTS_MOTE_PALETTE;
                    lifetime      = ((_RoomFxMoteArg*)&task->spawnArg1)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = -((_RoomFxMoteArg*)&task->spawnArg1)->verticalSpeed - (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x3F);
                    work->move.vz = 0;
                    task->state   = (task->spawnArg1.value & ROOM_VISUAL_EFFECTS_MOTE_STEADY) + 1;
                }
                break;
            case 1:
                coord->coord.t[1]  += work->move.vy;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    _roomVisualEffectsDrawMote(coord, work->index, work->angle | ROOM_VISUAL_EFFECTS_MOTE_TEXTURE_ROW_1, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    } else if (work->scale < 0x80) {
                        work->scale += 0x20;
                    }
                } else {
                    effectKillTask(work, task);
                }
                break;
            case 2:
                coord->coord.t[1]  += work->move.vy;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    _roomVisualEffectsDrawMote(coord, work->index, work->angle, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    }
                } else {
                    effectKillTask(work, task);
                }
                break;
        }
    }
}
