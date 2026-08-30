#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/sound.h"

/// Sound emitter positions for the cavern's four ambient loops, indexed by the
/// emitter id `func_mine_cavern_801825C8` and its siblings are called with.
extern SVECTOR D_mine_cavern_8018E39C[4];

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80181730);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80181864);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80181CAC);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80181D80);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80182184);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80182454);

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern_2", D_mine_cavern_8017D65C);

void func_mine_cavern_801825C8(s16 arg0)
{
    GsCOORDINATE2 coord;
    s32           view;

    view             = Gp_GetViewIndex() & 0xFF;
    coord.sub        = &Gfx_ViewCoord;
    coord.coord.t[0] = D_mine_cavern_8018E39C[arg0].vx;
    coord.coord.t[1] = D_mine_cavern_8018E39C[arg0].vy;
    coord.coord.t[2] = D_mine_cavern_8018E39C[arg0].vz;
    coord.flg        = 0;
    Gp_UpdateCoord(&coord);

    switch (arg0) {
        case 0:
            switch (view) {
                case 2:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 20:
                    SndEvt_EnqueueType7(0x5402000F, 1);
                    break;
                case 3:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    break;
                case 4:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    break;
                case 5:
                case 25:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    break;
                case 18:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    break;
                case 19:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0xD);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0xD);
                    break;
                case 21:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    break;
                case 22:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    break;
                case 23:
                case 24:
                    SndEvt_EnqueueType6(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x5402000F, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    break;
            }
            break;
        case 1:
            switch (view) {
                case 2:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    break;
                case 3:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    break;
                case 4:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    break;
                case 20:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    break;
                case 22:
                    SndEvt_EnqueueType6(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x5402000E, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    break;
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 18:
                case 19:
                case 21:
                case 23:
                case 24:
                case 25:
                    SndEvt_EnqueueType7(0x5402000E, 1);
                    break;
            }
            break;
        case 2:
            switch (view) {
                case 5:
                case 25:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    break;
                case 6:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    break;
                case 7:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x46);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x46);
                    break;
                case 14:
                case 15:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    break;
                case 16:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x46);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x46);
                    break;
                case 17:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    break;
                case 8:
                case 21:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x59);
                    break;
                case 23:
                case 24:
                    SndEvt_EnqueueType6(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x54020010, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    break;
                case 2:
                case 3:
                case 4:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 18:
                case 19:
                case 20:
                case 22:
                default:
                    SndEvt_EnqueueType7(0x54020010, 1);
                    break;
            }
            break;
        case 3:
            switch (view) {
                case 2:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x40);
                    break;
                case 7:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x33);
                    break;
                case 8:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x20);
                    break;
                case 6:
                case 20:
                    SndEvt_EnqueueType6(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x46);
                    SndEvt_EnqueueTypeA(0x54020011, (s8)Gp_GetObjPan((GpObj38*)&coord), 0x46);
                    break;
                case 3:
                case 4:
                case 5:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 18:
                case 19:
                case 21:
                case 22:
                case 23:
                case 24:
                case 25:
                default:
                    SndEvt_EnqueueType7(0x54020011, 1);
                    break;
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80182CEC);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80182DA8);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80182DC8);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80182E34);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_801830F0);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_801836D0);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80183860);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80183890);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_801838F4);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80183A68);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80183AD4);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_80183C10);

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern_2", jtbl_mine_cavern_8017D818);
