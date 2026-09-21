#include "common.h"

#include "actors/actor_302600.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/wipsys.h"

INCLUDE_ASM("actors/nonmatchings/actor_302600/actor_302600", func_actor_302600_80162574);

INCLUDE_ASM("actors/nonmatchings/actor_302600/actor_302600", func_actor_302600_80162734);

INCLUDE_ASM("actors/nonmatchings/actor_302600/actor_302600", func_actor_302600_801628B4);

void func_actor_302600_80162DC0(Actor302600* arg0)
{
    Actor302600Work*       work;
    GsCOORDINATE2*         coord;
    s32                    state;
    s16                    timer;
    s16                    timer2;
    s32                    distance;
    s32                    sound;
    s32                    dx;
    s32                    dz;
    s32                    pan;
    u32                    random;
    u32                    random2;
    Actor302600RotScratch* delta;
    Actor302600RotScratch* scratchEnd;

    scratchEnd                                        = *(Actor302600RotScratch**)PSX_SCRATCH_ADDR(0x3FC);
    delta                                             = scratchEnd - 1;
    *(Actor302600RotScratch**)PSX_SCRATCH_ADDR(0x3FC) = delta;
    work                                              = arg0->field_1C;
    state                                             = work->field_39C;
    coord                                             = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            work->field_3C8 = 0;
            work->field_398 = 0;
            work->field_3A6 = 0;
            timer           = (u16)work->field_39E - 1;
            work->field_39E = timer;
            if (timer <= 0) {
                work->field_39C = 1;
                work->field_392 = 2;
                random          = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = D_actor_302600_8016A7A8[arg0->field_20->field_3C->rowIndex] + ((random >> 0x10) & 0x3FF);
                Gp_LcgState     = random;
                return;
            }
            return;
        case 1:
            scratchEnd[-1].vec.vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
            delta->vec.vy         = 0;
            delta->vec.vz         = (s32)(Player_Status.coordMtx->t[2] - coord->coord.t[2]);
            work->field_3A4       = ratan2((s32)(s16)scratchEnd[-1].vec.vx, (s32)(s16)delta->vec.vz) & 0xFFF;
            work->field_3A6       = 0x12;
            if ((s16)work->field_396 >= 0xB) {
                work->field_398 = 0x17;
            }
            timer2          = (u16)work->field_39E - (u16)work->field_398;
            work->field_39E = timer2;
            if (timer2 <= 0) {
                work->field_39C = 0;
                work->field_392 = state;
                random2         = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = D_actor_302600_8016A798[arg0->field_20->field_3C->rowIndex] + ((random2 >> 0x10) & 0xF);
                Gp_LcgState     = random2;
                return;
            }
            if ((s16)work->field_396 == 0xC) {
                sound = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x401A0001;
                pan   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_396 >= 0x29) {
                work->field_396 = 0xB;
            }
            if (work->field_3A4 == work->field_3A2) {
                scratchEnd[-1].vec.vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
                delta->vec.vy         = 0;
                dz                    = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                delta->vec.vz         = dz;
                dx                    = scratchEnd[-1].vec.vx;
                distance              = SquareRoot0((dx * dx) + (dz * dz));
                if ((work->field_3C0 == 0) && (distance < 0x578) && (work->field_3B0 == 0) && !(Player_Status.peStateFlags & 1)) {
                    work->field_39A = 4;
                    work->field_39C = 0;
                    work->field_392 = 3;
                    work->field_3AC = 0;
                } else if (distance < 0x9C4) {
                    work->field_39A = 5;
                    work->field_39C = 0;
                    work->field_392 = 4;
                }
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_302600/actor_302600", func_actor_302600_80163108);

INCLUDE_ASM("actors/nonmatchings/actor_302600/actor_302600", func_actor_302600_8016325C);

INCLUDE_RODATA("actors/nonmatchings/actor_302600/actor_302600", D_actor_302600_80161E4C);
INCLUDE_RODATA("actors/nonmatchings/actor_302600/actor_302600", D_actor_302600_80161E58);
