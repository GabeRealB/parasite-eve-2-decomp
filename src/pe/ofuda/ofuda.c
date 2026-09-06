#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// This overlay's id. Every package opens with one: a u16 in a u32
/// slot, distinct across all 448, with the families in contiguous blocks.
const u32 D_ofuda_8012EF30 = 60;

/// Ofuda PE flash. `Task::spawnArg2` is the `GpEffWork` block (`field_24`
/// brightness, `field_26` ring radius, `field_2A` per-frame step);
/// `Task::extra` reaches the coordinate. Cancel (`Gp_StateC08.field_3 == -2`
/// or a non-zero `Gp_State1C->field_E`) stops the 0xE03D0001 cue and
/// releases the pool block.
///
/// State 0 seeds a 30-frame grow, a 0x100 ring, and the step `0x100 /
/// spawnArg1`, then plays the cue at the object's pan/depth. State 1 grows
/// the ring and draws it twice plus two arcs; at frame 0 it snaps
/// brightness to 0xFF, sets `field_6` bit 3, and goes to state 2. State 2
/// shrinks until brightness drops below 9, then releases.
void func_ofuda_8012EF34(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpStateC08*    state;
    s32            pan;
    u8             rgb[3];

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((state->field_3 == -2) || (Gp_State1C->field_E != 0)) {
        SndEvt_EnqueueType7(0xE03D0001, 1);
        goto kill;
    }

    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            arg0->spawnArg1 = 0x1E;
            mem->field_24   = 0;
            mem->field_26   = 0x100;
            mem->field_2A   = 0x100 / arg0->spawnArg1;
            arg0->state     = 1;
            pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(0xE03D0001, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            return;
        case 1: {
            s32 cur;
            s32 addend;
            s32 copy;

            cur           = (u16)mem->field_24;
            addend        = (u16)mem->field_2A;
            mem->field_24 = cur + addend;
            copy          = addend;
            COPY_REG_EC(copy, addend);
            mem->field_26 = (u16)mem->field_26 + (copy << 3);
        }
            arg0->spawnArg1 = arg0->spawnArg1 - 1;
            rgb[0]          = *(u8*)&mem->field_24;
            rgb[1]          = (u16)mem->field_24 >> 2;
            rgb[2]          = (u16)mem->field_24 >> 1;
            Gp_DrawRing(coord, mem->field_26, rgb);
            Gp_DrawRing(coord, (s16)((u16)mem->field_26 * 2), rgb);
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
                mem->field_24   = 0xFF;
                arg0->state     = 2;
                mem->field_28   = 0x600;
                mem->field_2A   = 0;
                state->field_6 |= 8;
            }
            return;
        case 2:
            if (mem->field_24 < 9) {
                goto kill;
            }
            rgb[0] = *(u8*)&mem->field_24;
            rgb[1] = (u16)mem->field_24 >> 2;
            rgb[2] = (u16)mem->field_24 >> 1;
            Gp_DrawRing(coord, mem->field_26, rgb);
            Gp_DrawRing(coord, (s16)((u16)mem->field_26 * 2), rgb);
            mem->field_24 = (u16)mem->field_24 - 8;
            mem->field_26 = (u16)mem->field_26 - 0x30;
            Gp_DrawFadeQuad(rgb, 1);
            Gp_DrawFadeQuad(rgb, 1);
            return;
    }
    return;
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}
