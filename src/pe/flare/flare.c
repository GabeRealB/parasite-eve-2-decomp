#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32 Gp_LcgState;

void func_flare_8012EF34(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpStateC08*    state;
    s32            pan;
    s16            tick;
    GpEffWork*     spawned;
    s32            rng;

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((state->field_3 == -2) || (Gp_State1C->field_E != 0)) {
        SndEvt_EnqueueType7(0xE03E0001, 1);
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->field_22 = (u16)mem->field_22 + 1;
    if (arg0->state == 0) {
        pan = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(0xE03E0001, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        arg0->state = 1;
    }
    tick = mem->field_22;
    if (tick < 0x14) {
        if (tick == 8) {
            state->field_6 |= 8;
        }
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        spawned     = Gp_SpawnEff(0x6019E, coord, (((u32)rng >> 16) & 0x1FF) + 0x680, 0);
        if (spawned != NULL) {
            Task_Reparent(arg0, spawned->field_0);
        }
        return;
    }
    if (tick == 0x24) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

INCLUDE_ASM("pe/nonmatchings/flare/flare", func_flare_8012F0B8);

INCLUDE_ASM("pe/nonmatchings/flare/flare", func_flare_8012F304);

INCLUDE_RODATA("pe/nonmatchings/flare/flare", D_flare_8012EF30);
