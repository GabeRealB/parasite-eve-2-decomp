#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/healing.h"

/// This overlay's id. Every package opens with one: a u16 in a u32
/// slot, distinct across all 448, with the families in contiguous blocks.
const u32 D_healing_8012EF30 = 55;

/// Per-level tuning for the healing aura: rows are PE levels 1-3, selected by
/// `field_20`. `field_2` is the brightness ceiling, `field_4` the per-tick
/// spin, `field_6` the radius the ring grows to before the effect ends.
HealingScale D_healing_8012FC1C[] = {
    { 0x0008, 0x0080, 0x0040, 0x0400 },
    { 0x000C, 0x00B0, 0x0048, 0x0500 },
    { 0x0010, 0x00E0, 0x0050, 0x0600 },
};

/// The `SndEvt_EnqueueType6` id for each `D_healing_8012FC1C` row.
s32 D_healing_8012FC34[] = { 0xE0200001, 0xE0230001, 0xE0260001 };

extern s32 Gp_LcgState;

/// Healing PE ring. Cancel (`Gp_StateC08.field_3 == -2` or
/// `Gp_State1C->field_E >= 4`) releases the work block, and if the effect has
/// not started yet also sets `field_6` bit 3. State 0 parents the coordinate
/// to the player, plays the combo-indexed cue from `D_healing_8012FC34`, and
/// falls into state 1, which grows brightness / radius, randomizes a spawn
/// offset and parents a `0x60017` spark. State 2 shrinks brightness. Both
/// draw two rings plus one or two arcs. State 3 holds for 0x1F frames then
/// releases.
void func_healing_8012EF34(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpStateC08*    state;
    GpMtxWords*    rot;
    GpEffWork*     spawned;
    s32            pan;
    s32            bright;
    s16            ang;
    s32            rng;
    s32            temp_lo;
    u8             rgb[3];

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((state->field_3 == -2) || (Gp_State1C->field_E >= 4)) {
        if (arg0->state == 0) {
            state->field_6 |= 8;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }

    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            rot               = (GpMtxWords*)&coord->coord;
            coord->sub        = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
            rot->w0           = 0x1000;
            rot->w1           = 0;
            rot->w2           = 0x1000;
            rot->w3           = 0;
            rot->h4           = 0x1000;
            coord->coord.t[0] = 0;
            coord->coord.t[1] = -0x400;
            coord->coord.t[2] = 0;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            arg0->state     = 1;
            mem->field_20   = (Gp_StateC08.field_0 % 10) - 1;
            mem->field_26   = 0x80;
            state->field_6 |= 8;
            pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(D_healing_8012FC34[mem->field_20], pan,
                                (s8)Gp_GetObjDepth((GpObj38*)coord));
            /* fallthrough */
        case 1:
            bright = mem->field_24;
            if (bright < D_healing_8012FC1C[mem->field_20].field_2) {
                bright += 0x10;
            }
            mem->field_24 = bright;
            mem->field_26 = (u16)mem->field_26 + (u16)D_healing_8012FC1C[mem->field_20].field_4;
            Gfx_RotMatrixY(&coord->coord, -(D_healing_8012FC1C[mem->field_20].field_4 * 2), 0);
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            rng           = Gp_LcgState * 5 + 0x71357911;
            ang           = ((u32)rng >> 16) & 0xFFF;
            Gp_LcgState   = rng;
            mem->field_2A = ang;
            mem->field_10 = (rcos(ang) * ((s16)mem->field_26 * 3 / 2)) >> 12;
            temp_lo       = rsin(mem->field_2A) * ((s16)mem->field_26 * 3 / 2);
            rng           = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState   = rng;
            mem->field_12 = temp_lo >> 12;
            mem->field_14 = (rsin(((u32)rng >> 16) & 0xFFF) * mem->field_10) >> 12;
            spawned       = Gp_SpawnEff(0x60017, coord, D_healing_8012FC1C[mem->field_20].field_6,
                                        (SVECTOR*)&mem->field_10);
            if (spawned != NULL) {
                Task_Reparent(arg0, spawned->field_0);
            }
            if (mem->field_26 >= D_healing_8012FC1C[mem->field_20].field_6) {
                arg0->state = 2;
            }
            goto draw;
        case 2:
            Gfx_RotMatrixY(&coord->coord, -(D_healing_8012FC1C[mem->field_20].field_4 * 2), 0);
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            mem->field_24 = (u16)mem->field_24 - 0x10;
            mem->field_26 = (u16)mem->field_26 + (u16)D_healing_8012FC1C[mem->field_20].field_4;
            if (mem->field_24 < 0x11) {
                arg0->state = 3;
            }
        draw:
            rgb[0] = (u16)mem->field_24 >> 2;
            rgb[1] = (u16)mem->field_24 >> 1;
            rgb[2] = *(u8*)&mem->field_24;
            Gp_DrawRing(coord, (s32)((u16)mem->field_26 << 16) >> 17, rgb);
            Gp_DrawRing(coord, (s32)((u16)mem->field_26 << 16) >> 17, rgb);
            {
                GsCOORDINATE2* c;
                s32            span;
                unsigned int   r;
                unsigned int   g;
                unsigned int   b;

                c = coord;
                COPY_REG_EC(c, coord);
                span = 0x80;
                TOUCH_REG(span);
                r      = rgb[0];
                b      = rgb[2];
                rgb[0] = r >> 1;
                SOFT_COMPILER_BARRIER();
                g      = rgb[1];
                rgb[2] = b >> 1;
                rgb[1] = g >> 1;
                Gp_DrawArc(c, mem->field_26, span, rgb);
            }
            if ((u16)mem->field_22 & 1) {
                Gp_DrawArc(coord, 0x80, mem->field_26, rgb);
            }
            if (mem->field_20 != 0) {
                GsCOORDINATE2* c;
                s32            span;
                unsigned int   r;
                unsigned int   g;
                unsigned int   b;

                c = coord;
                COPY_REG_EC(c, coord);
                span = 0x80;
                TOUCH_REG(span);
                r      = rgb[0];
                b      = rgb[2];
                rgb[0] = r >> 1;
                SOFT_COMPILER_BARRIER();
                g      = rgb[1];
                rgb[2] = b >> 1;
                rgb[1] = g >> 1;
                Gp_DrawArc(c, (s16)((u16)mem->field_26 + 0x200), span, rgb);
            }
            return;
        case 3:
            Gfx_RotMatrixY(&coord->coord, -(D_healing_8012FC1C[mem->field_20].field_4 * 2), 0);
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            mem->field_28 = (u16)mem->field_28 + 1;
            if ((s16)mem->field_28 < 0x1F) {
                return;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        default:
            return;
    }
}
