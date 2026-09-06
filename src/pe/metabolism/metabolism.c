#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32 Gp_LcgState;

INCLUDE_ASM("pe/nonmatchings/metabolism/metabolism", func_metabolism_8012EF34);

/// Metabolism billboard. State 0 seeds the spin from the spawn argument and
/// picks the draw path: the plain additive quad (state 1), or, one roll in
/// three when the level's difficulty band allows it, the alternate
/// `func_800EB6E8` quad that fades its colour by 0x18 a frame (state 2).
/// Both states lift the frame and draw on odd ticks until it runs out.
void func_metabolism_8012F5A0(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            y;
    s16            step;
    u16            kind;
    u16            roll;

    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            mem->field_10 = 0;
            mem->field_12 = 8;
            mem->field_14 = 0;
            mem->field_26 = arg0->spawnArg1 & 0xFFF;
            kind          = Gp_StateC08.field_0 % 10U;
            if (kind - 1 < 2 ||
                (Gp_LcgState = Gp_LcgState * 5 + 0x71357911,
                 roll        = ((u32)Gp_LcgState >> 16) % 3U, roll != 0)) {
                arg0->state   = 1;
                mem->field_28 = 0x1000;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_24 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            } else {
                arg0->state   = 2;
                mem->field_24 = 0xC0;
                mem->field_28 = 0x3000;
            }
            return;
        case 1:
            step              = mem->field_12;
            y                 = coord->coord.t[1] + step;
            coord->flg        = 0;
            coord->coord.t[1] = y;
            Gp_UpdateCoord(coord);
            if (!((u16)mem->field_22 & 1)) {
                mem->field_20 = (u16)mem->field_20 + 1;
            }
            if (mem->field_20 < 8) {
                if ((u16)mem->field_22 & 1) {
                    Gp_DrawFxQuad(coord, (u16)mem->field_20, mem->field_26,
                                  (u16)mem->field_24 | (u16)mem->field_28);
                    return;
                }
            } else {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            break;
        case 2:
            step              = mem->field_12;
            y                 = coord->coord.t[1] + step;
            coord->flg        = 0;
            coord->coord.t[1] = y;
            Gp_UpdateCoord(coord);
            if (!((u16)mem->field_22 & 1)) {
                mem->field_20 = (u16)mem->field_20 + 1;
            }
            if (mem->field_20 < 8) {
                if ((u16)mem->field_22 & 1) {
                    func_800EB6E8(coord, (u16)mem->field_20, (u16)mem->field_26,
                                  (u16)mem->field_24 | (u16)mem->field_28);
                    mem->field_24 = (u16)mem->field_24 - 0x18;
                    return;
                }
            } else {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            break;
    }
}

INCLUDE_ASM("pe/nonmatchings/metabolism/metabolism", func_metabolism_8012F840);

INCLUDE_RODATA("pe/nonmatchings/metabolism/metabolism", D_metabolism_8012EF30);
