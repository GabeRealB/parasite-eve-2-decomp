#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/inferno.h"

extern s8 D_80114C0B;

void func_inferno_8012F3EC(s16 arg0);

/// Runs one frame of the inferno cast: a state machine driven by
/// `Task::state`, with the chain it takes chosen in state 0 from
/// `Gp_StateC08.field_0 % 10 - 1` (the combo counter), which also picks the
/// roar from `D_inferno_801304F0` and lands the task on state 1, 5 or 9.
/// State 1 spawns the two ignition effects, state 5 fans six flames around a
/// 0x400 step, state 9 the ground burst; states 10 and 11 fade the effect
/// brightness `GpEffWork::field_26` down and back up and each fire one ring of
/// flames on their own tick, and state 12 fades out and releases. Every state
/// updates the effect coordinate first, and any state releases immediately if
/// the player is dying (`D_80114C0B`) or the room is fading (`Gp_State1C`).
void func_inferno_8012EF88(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            i;
    s32            pan;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((D_80114C0B == -2) || (Gp_State1C->field_E >= 4)) {
        goto release;
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            mem->field_24 = 0x200;
            mem->field_26 = 0xFF;
            pan           = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(D_inferno_801304F0[(u16)(Gp_StateC08.field_0 % 10) - 1], pan,
                                (s8)Gp_GetObjDepth((GpObj38*)coord));
            arg0->state = ((u16)(Gp_StateC08.field_0 % 10) - 1) * 4 + 1;
            return;
        case 1:
            Gp_SpawnEff(0x800600DA, coord, 3, NULL);
            Gp_SpawnEff(0x800600DA, coord, 5, NULL);
            Gp_StateC08.field_6 |= 8;
            Gp_SpawnPadLerp(0x10, 0xFF, 8);
            arg0->state = 0xC;
            return;
        case 5:
            i = 0x200;
            func_inferno_8012F3EC(mem->field_26);
            Gp_SpawnEff(0x800600DA, coord, 3, NULL);
            mem->field_24 = 0x600;
            do {
                mem->field_10 = (rsin(i) * mem->field_24) >> 12;
                mem->field_14 = (rcos(i) * mem->field_24) >> 12;
                i            += 0x400;
                Gp_SpawnEff(0x800600DA, coord, 4, (SVECTOR*)&mem->field_10);
            } while (i < 0x1200);
            Gp_StateC08.field_6 |= 8;
            Gp_SpawnPadLerp(0x14, 0xFF, 8);
            arg0->state = 0xC;
            return;
        case 9:
            Gp_SpawnEff(0x800600DA, coord, 0, NULL);
            Gp_SpawnPadLerp(0xC, 0xFF, 8);
            arg0->state = 0xA;
            return;
        case 10:
            func_inferno_8012F3EC(mem->field_26);
            mem->field_26 = (u16)mem->field_26 - 0x10;
            if (mem->field_22 != 0xC) {
                return;
            }
            mem->field_24 = 0x600;
            i             = 0x155;
            do {
                mem->field_10 = (rsin(i) * mem->field_24) >> 12;
                mem->field_14 = (rcos(i) * mem->field_24) >> 12;
                i            += 0x2AA;
                Gp_SpawnEff(0x800600DA, coord, 1, (SVECTOR*)&mem->field_10);
            } while (i < 0x1151);
            Gp_SpawnPadLerp(0xC, 0xFF, 8);
            arg0->state = 0xB;
            return;
        case 11:
            func_inferno_8012F3EC(mem->field_26);
            if (mem->field_26 < 0xF0) {
                mem->field_26 = (u16)mem->field_26 + 0x10;
            }
            if (mem->field_22 != 0x18) {
                return;
            }
            mem->field_24 = 0x900;
            i             = 0;
            do {
                mem->field_10 = (rsin(i) * mem->field_24) >> 12;
                mem->field_14 = (rcos(i) * mem->field_24) >> 12;
                i            += 0x2AA;
                Gp_SpawnEff(0x800600DA, coord, 2, (SVECTOR*)&mem->field_10);
            } while (i < 0xFFC);
            Gp_StateC08.field_6 |= 8;
            Gp_SpawnPadLerp(0x18, 0xFF, 8);
            arg0->state   = 0xC;
            mem->field_26 = 0xFF;
            return;
        case 12:
            func_inferno_8012F3EC(mem->field_26);
            if (mem->field_26 >= 9) {
                mem->field_26 = (u16)mem->field_26 - 8;
                return;
            }
            break;
        default:
            return;
    }
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("pe/nonmatchings/inferno/inferno", func_inferno_8012F3EC);

INCLUDE_RODATA("pe/nonmatchings/inferno/inferno", D_inferno_8012EF68);

INCLUDE_ASM("pe/nonmatchings/inferno/inferno", func_inferno_8012F530);

INCLUDE_ASM("pe/nonmatchings/inferno/inferno", func_inferno_8012F978);

INCLUDE_ASM("pe/nonmatchings/inferno/inferno", func_inferno_8012FF34);
