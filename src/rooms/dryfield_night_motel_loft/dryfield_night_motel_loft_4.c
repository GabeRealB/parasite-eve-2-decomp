#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "rooms/dryfield_night_motel_loft.h"
#include "rooms/room_common.h"

extern s8 D_8007272D;

extern s32 Gp_LcgState;

extern u8      D_dryfield_night_motel_loft_8017EB78[];
extern GpObj4A D_dryfield_night_motel_loft_80180440;

/// The loft's points, one 8-byte `SVECTOR` per prop. The room draws them by
/// index - 0 and 5 for views 2 and 9, 1, 2 and 4 for 3 and 10, 2 for 4, 3 for
/// 6, 4 and 5 for 7 and 11 - and the two effect bursts write the seventh
/// (`[6]`, the one splat never had to name) as the offset they spawn at.
/// splat's `8017ED80` … `8017EDA0` are just the addresses this code resolves,
/// so they are elements 1, 2, 3, 4 and 5 of this one array.
extern SVECTOR D_dryfield_night_motel_loft_8017ED78[];

/// The room's grid params: `8017ED54` is the template this function copies
/// from, `8017F120` the live copy it writes and shifts.
extern GpGridParams D_dryfield_night_motel_loft_8017ED54;
extern GpGridParams D_dryfield_night_motel_loft_8017F120;

void func_dryfield_night_motel_loft_8017D9BC(s32 arg0);

/// Restores the live grid's first face normal, its face record and its four
/// face corners from the template, then raises the corners by 0xBB8 in Y when
/// `arg0` is set.
void func_dryfield_night_motel_loft_8017D9BC(s32 arg0)
{
    GpGridParams* dst;
    GpGridParams* src;
    SVECTOR       d;
    s32           i;

    dst = &D_dryfield_night_motel_loft_8017F120;
    src = &D_dryfield_night_motel_loft_8017ED54;

    for (i = 0; i < 1; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 4; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
    } else {
        d.vx = 0;
        d.vy = 0xBB8;
    }
    d.vz = 0;

    for (i = 0; i < 4; i++) {
        dst->field_8[i].vx += d.vx;
        dst->field_8[i].vy += d.vy;
        dst->field_8[i].vz += d.vz;
    }
}

/// Per-view room draw. Views 2 and 9, 4, 6 and 7 and 11 each queue one or two
/// of the room's points, and views 3, 10 and 8 additionally run a burst of
/// effect 0x601B0 at the room's seventh point: 0x20 steps from state 1 (to
/// state 2) and 0x30 from state 0 (to state 1). Every step rolls the room LCG
/// (`Gp_LcgState`) four times and builds the offset vector from the top bits of
/// each draw, the last draw's low six bits biased by 0x10 riding along as the
/// spawn argument.
void func_dryfield_night_motel_loft_8017DB64(Task* arg0)
{
    s32 i;

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
        case 9:
            func_dryfield_night_motel_loft_8017DE14(&D_dryfield_night_motel_loft_8017ED78[0], 0, 0x300);
            func_dryfield_night_motel_loft_8017DE14(&D_dryfield_night_motel_loft_8017ED78[5], 0, 0x300);
            break;
        case 3:
        case 10: {
            SVECTOR* p = &D_dryfield_night_motel_loft_8017ED78[1];

            func_dryfield_night_motel_loft_8017DE14(&p[0], 0, 0x300);
            func_dryfield_night_motel_loft_8017DE14(&p[1], 0, 0x300);
            func_dryfield_night_motel_loft_8017DE14(&p[3], 0, 0x300);
            if (arg0->state == 1) {
                SVECTOR* pos;

                i   = 0;
                pos = &p[-1];
                do {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    pos[6].vx   = 0xFB8 - (((u32)Gp_LcgState >> 16) & 0x3FF);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    pos[6].vy   = (((u32)Gp_LcgState >> 16) & 0x1FF) - 0xD5C;
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    pos[6].vz   = 0x400 - (((u32)Gp_LcgState >> 16) & 0x7FF);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x601B0, NULL, (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10, &pos[6]);
                    i++;
                } while (i < 0x20);
                arg0->state = 2;
            }
            break;
        }
        case 4:
            func_dryfield_night_motel_loft_8017DE14(&D_dryfield_night_motel_loft_8017ED78[2], 0, 0x300);
            break;
        case 6:
            func_dryfield_night_motel_loft_8017DE14(&D_dryfield_night_motel_loft_8017ED78[3], 0, 0x300);
            break;
        case 7:
        case 11:
            func_dryfield_night_motel_loft_8017DE14(&D_dryfield_night_motel_loft_8017ED78[4], 0, 0x300);
            func_dryfield_night_motel_loft_8017DE14(&D_dryfield_night_motel_loft_8017ED78[5], 0, 0x300);
            break;
        case 8:
            if (arg0->state == 0) {
                SVECTOR* pos;

                i   = 0;
                pos = &D_dryfield_night_motel_loft_8017ED78[0];
                do {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    pos[6].vx   = 0xFB8 - (((u32)Gp_LcgState >> 16) & 0x3FF);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    pos[6].vy   = (((u32)Gp_LcgState >> 16) & 0x1FF) - 0xD5C;
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    pos[6].vz   = 0x400 - (((u32)Gp_LcgState >> 16) & 0x7FF);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x601B0, NULL, (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10, &pos[6]);
                    i++;
                } while (i < 0x30);
                arg0->state = 1;
            }
            break;
    }
}
