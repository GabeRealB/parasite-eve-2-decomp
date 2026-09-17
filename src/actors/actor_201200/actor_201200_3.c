#include "common.h"
#include "actors/actor_201200.h"
#include "actors/actors_shared_8014a7b0.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "psyq/inline_c.h"

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern MATRIX* D_80073B8C;
extern u32     Gp_LcgState;

void Gp_ArmStateF0(s32 active);
void func_actor_201200_8014A640(Actor201200* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void func_actor_201200_8014C738(Actor201200Ctx* arg0, Actor201200* arg1);
void ActorsShared8014db78(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014DBE0(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014AE60(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014DC98(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014B054(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014B5FC(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014BDFC(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014CA08(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014D0B4(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014DD50(Actor201200Ctx* arg0, Actor201200* arg1);

extern u8  D_801153F4;
extern s32 D_actor_201200_8014DE64;
extern s32 D_actor_201200_8014DE70;

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_3", func_actor_201200_8014C738);

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_3", func_actor_201200_8014CA08);

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_3", func_actor_201200_8014D0B4);

const Actor201200StateTable D_actor_201200_80149F04 = {
    {
        ActorsShared8014db78,
        func_actor_201200_8014DBE0,
        func_actor_201200_8014AE60,
        func_actor_201200_8014DC98,
        func_actor_201200_8014B054,
        func_actor_201200_8014B5FC,
        func_actor_201200_8014BDFC,
        func_actor_201200_8014CA08,
        func_actor_201200_8014D0B4,
        func_actor_201200_8014DD50,
    }
};

/// Per-frame tick: refreshes the coordinate and color, handles the render
/// mode in `D_801153F4`, dispatches the substate handler and plays its sound.
void func_actor_201200_8014D4D0(Actor201200Ctx* arg0, Actor201200* arg1)
{
    VECTOR                pos;
    Actor201200StateTable table;
    Actor201200Work*      work;
    s32                   snd;
    s32                   pan;
    s32                   id;

    work                         = arg1->field_1C;
    table                        = D_actor_201200_80149F04;
    arg1->field_2C->field_8->flg = 0;
    Gp_UpdateCoord(arg1->field_2C->field_8);
    pos.vx = arg1->field_2C->field_8->workm.t[0];
    pos.vy = arg1->field_2C->field_8->workm.t[1];
    pos.vz = arg1->field_2C->field_8->workm.t[2];
    Gp_UpdateActorColor((struct _GpEnemy*)arg0, &pos, 0, 0);
    switch (D_801153F4) {
        case 0:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 5) {
                arg1->field_2C->field_C = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->field_2C->field_8->workm.t, 0x180, Gp_State1C->field_8);
            }
            break;
        case 1:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 5) {
                arg1->field_2C->field_C = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->field_2C->field_8->workm.t, 0x180, Gp_State1C->field_8);
            }
            Gp_ClearRec18Occupied(&work->rec1B8);
            Gp_ClearRec18Occupied(&work->rec250);
            Gp_ClearRec18Occupied(&work->rec2E8);
            return;
        case 2:
            arg1->field_2C->field_C = 0x80;
            Gp_ClearRec18Occupied(&work->rec1B8);
            Gp_ClearRec18Occupied(&work->rec250);
            Gp_ClearRec18Occupied(&work->rec2E8);
            return;
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    table.fn[work->field_0](arg0, arg1);
    if (arg0->field_40 > 0) {
        func_actor_201200_8014C738(arg0, arg1);
        if (arg0->field_40 <= 0) {
            work->field_0 = 6;
        }
    }
    Gp_ClearRec18Occupied(&work->rec1B8);
    Gp_ClearRec18Occupied(&work->rec250);
    Gp_ClearRec18Occupied(&work->rec2E8);
    id = ActorsShared8014a7b0((ActorsShared8014a7b0Work*)work);
    if (id != 0) {
        snd = id | ((arg0->field_8 >> 12) << 8);
        pan = (s8)Gp_GetObjPan((GpObj38*)arg1->field_2C->field_8);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)arg1->field_2C->field_8));
    }
    if (work->field_3D8 != 0) {
        func_800D7A9C(arg1->field_2C, (VECTOR*)arg1->field_2C->field_8->workm.t, 0, 3);
    }
    if (Game_Session->field_4D != 0) {
        arg1->field_2C->field_8->flg = 0;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_201200/actor_201200_3", ActorsShared80135df4Table);
