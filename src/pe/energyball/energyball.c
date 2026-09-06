#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/energyball.h"

extern s32 D_80115724;
extern s32 Gp_LcgState;

/// Fires the energy ball: on the first frame it picks the charge level from the
/// combo counter, plays the matching loop sound, refills the surface-jitter
/// table and spawns one ball per charge level, fanning them out by 0x555 of
/// yaw each while `D_80115724` (the number of balls already in flight) allows
/// it. Every later frame just releases the work block.
void func_energyball_8012EF48(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            i;
    s32            level;
    s32            rng;

    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            mem->field_20 = Gp_StateC08.field_0 % 10 - 1;
            level         = mem->field_20;
            mem->field_26 = (level << 8) + 0x300;
            if (D_80115724 < 0) {
                D_80115724 = 0;
            }
            if (D_80115724 == 0) {
                SndEvt_EnqueueType6(D_energyball_8013117C[mem->field_20], 0, 0);
            }
            for (i = 0; i < 0x10; i++) {
                rng                      = Gp_LcgState * 5 + 0x71357911;
                D_energyball_801311A0[i] = ((u32)rng >> 16) & 0xFF;
                Gp_LcgState              = rng;
            }
            for (i = 0; i < mem->field_20 + 1; i++) {
                if (D_80115724 + i >= 3) {
                    break;
                }
                mem->field_24 = i * 0x555 - (s16)(u16)mem->field_20 * 0x2AA;
                mem->field_10 = (mem->field_26 * rsin(mem->field_24)) >> 12;
                mem->field_14 = (mem->field_26 * rcos(mem->field_24)) >> 12;
                Gp_SpawnEff(0x800600F8, coord, i, (SVECTOR*)&mem->field_10);
            }
            arg0->state = 1;
            return;
        case 1:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}

INCLUDE_RODATA("pe/nonmatchings/energyball/energyball", D_energyball_8012EF30);

INCLUDE_ASM("pe/nonmatchings/energyball/energyball", func_energyball_8012F180);

INCLUDE_ASM("pe/nonmatchings/energyball/energyball", func_energyball_8012FFD0);

INCLUDE_ASM("pe/nonmatchings/energyball/energyball", func_energyball_8013035C);

INCLUDE_ASM("pe/nonmatchings/energyball/energyball", func_energyball_801307D4);

INCLUDE_ASM("pe/nonmatchings/energyball/energyball", func_energyball_80130B54);

void func_energyball_8013107C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    u8             rgb[3];
    s32            scale;
    s32            angle;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_E;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1 & 0xFFF, 0);
        coord->flg    = 0;
        mem->field_24 = 0x80;
        mem->field_26 = 0x100;
        arg0->state   = 1;
    }

    Gp_UpdateCoord(coord);
    rgb[0] = (u16)mem->field_24 >> 1;
    rgb[1] = *(u8*)&mem->field_24;
    rgb[2] = (u16)mem->field_24 >> 1;
    Gp_DrawBandEx(coord, mem->field_26, 0x180, rgb);

    angle         = (u16)mem->field_26;
    scale         = (u16)mem->field_24;
    angle        += 0x80;
    scale        -= 8;
    mem->field_24 = scale;
    mem->field_26 = angle;
    if ((s16)scale < 9) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}
