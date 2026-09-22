#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// This overlay's id, the `u16` every package opens with.

/// PROVISIONAL: written before `Task` was processed, so the statements
/// about `Task` fields rest on unverified names. Rewrite once `Task` is done.
/// Draws the expanding flash the ofuda produces, over three states.
///
/// Spawned with a `GpEffWork` block in `Task::spawnArg2` holding the effect's
/// brightness, ring radius and per-frame step, and a coordinate reached through
/// `Task::extra`. `Task::spawnArg1` counts the frames of the growing phase down
/// to zero.
///
/// State 0 arms a 30-frame growth and starts the sound cue panned to where the
/// object is. State 1 brightens and widens the ring each frame, drawing it at
/// two radii plus two arcs, and on the last frame snaps to full brightness and
/// hands over to state 2. State 2 shrinks and dims until the brightness falls
/// below 9, then releases the block.
///
/// A cancelled or interrupted cast stops the cue and releases immediately.
void ofudaEffectTask(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpStateC08*    state;
    s32            pan;
    u8             rgb[3];

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if ((state->field_3 == -2) || (Gp_State1C->fadeState != 0)) {
        SndEvt_EnqueueType7(0xE03D0001, 1);
        goto kill;
    }

    mem->age = (u16)mem->age + 1;
    switch (arg0->state) {
        case 0:
            arg0->spawnArg1 = 0x1E;
            mem->scale      = 0;
            mem->angle      = 0x100;
            mem->step       = 0x100 / arg0->spawnArg1;
            arg0->state     = 1;
            pan             = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(0xE03D0001, pan, (s8)gpGetObjDepth(coord));
            return;
        case 1: {
            s32 cur;
            s32 addend;
            s32 copy;

            cur        = (u16)mem->scale;
            addend     = (u16)mem->step;
            mem->scale = cur + addend;
            copy       = addend;
            COPY_REG_EC(copy, addend);
            mem->angle = (u16)mem->angle + (copy << 3);
        }
            arg0->spawnArg1 = arg0->spawnArg1 - 1;
            rgb[0]          = *(u8*)&mem->scale;
            rgb[1]          = (u16)mem->scale >> 2;
            rgb[2]          = (u16)mem->scale >> 1;
            Gp_DrawRing(coord, mem->angle, rgb);
            Gp_DrawRing(coord, (s16)((u16)mem->angle * 2), rgb);
            Gp_DrawArc(coord, (s16)(((u16)arg0->spawnArg1 << 4) + 0x800), 0x100, rgb);
            {
                GsCOORDINATE2* c;
                s32            span;
                unsigned int   r;
                unsigned int   g;
                unsigned int   b;

                c = coord;
                COPY_REG_EC(c, coord);
                span = 0xC0;
                TOUCH_REG(span);
                r      = rgb[0];
                b      = rgb[2];
                rgb[0] = r >> 1;
                SOFT_COMPILER_BARRIER();
                g      = rgb[1];
                rgb[2] = b >> 1;
                rgb[1] = g >> 1;
                Gp_DrawArc(c, (s16)(((u16)arg0->spawnArg1 << 5) + 0xC00), span, rgb);
            }
            if (arg0->spawnArg1 == 0) {
                mem->scale      = 0xFF;
                arg0->state     = 2;
                mem->period     = 0x600;
                mem->step       = 0;
                state->field_6 |= 8;
            }
            return;
        case 2:
            if (mem->scale < 9) {
                goto kill;
            }
            rgb[0] = *(u8*)&mem->scale;
            rgb[1] = (u16)mem->scale >> 2;
            rgb[2] = (u16)mem->scale >> 1;
            Gp_DrawRing(coord, mem->angle, rgb);
            Gp_DrawRing(coord, (s16)((u16)mem->angle * 2), rgb);
            mem->scale = (u16)mem->scale - 8;
            mem->angle = (u16)mem->angle - 0x30;
            Gp_DrawFadeQuad(rgb, 1);
            Gp_DrawFadeQuad(rgb, 1);
            return;
    }
    return;
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}
