#include "common.h"

#include "actors/actor_401300.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/session.h"

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801323B0);

s32 func_actor_401300_80132554(Actor401300* arg0, s32 arg1, Actor401300Event* arg2)
{
    Actor401300Work* work  = arg0->field_1C;
    GpEnemy*         enemy = arg0->field_20;

    work->field_CA8[0] = arg2->b[0];
    work->field_CA8[1] = arg2->b[1];
    work->field_CA8[2] = arg2->b[2];
    if (arg2->w[0] == 0x301) {
        if (arg2->w[1] == 1) {
            work->field_0 = 0x17;
            return 1;
        }
    } else if (arg2->w[0] == 0xB05) {
        switch (arg2->w[1]) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 0xB:
                work->field_0 = 0x23;
                work->field_2 = -1;
                return 1;
            case 0xC:
                if ((enemy->field_8 >> 12) == 0) {
                    work->field_0   = 6;
                    enemy->field_40 = D_actor_401300_80141FA4[0];
                    ((void (*)(s32))Gp_IncStateF0Ref)(0);
                }
                return 1;
        }
    } else if (arg2->w[0] == 0x1D05) {
        switch (arg2->w[1]) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 0xB:
                work->field_0 = 0x23;
                work->field_2 = -1;
                return 1;
        }
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013267C);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80132910);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80132BE4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80132C78);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80132FF4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80133254);

void func_actor_401300_80133324(Actor401300* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    s16                  weight;
    s16                  i;
    Actor401300AnimWork* work;

    work   = (Actor401300AnimWork*)arg0->field_1C;
    weight = work->field_8B0;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_8AE;
            work->slots[i].field_9      = (u8)(work->field_8A6 - 3);
            if (i >= 7) {
                if (i < 9) {
                    continue;
                }
            }
            do {
                func_800B3448(&work->anim, i, (s32)&pose, 0);
                func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
                Gp_AnimWritePoseCopy(&work->anim, i, &pose, &blendPose, weight, 0x1000 - weight);
            } while (0);
        } else {
            work->slots[i].field_9 = (u8)(work->field_8A6 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013346C);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80133834);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80133A3C);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80134454);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80134BA4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80134F90);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80135DDC);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80135FC4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80136238);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801365F8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80136CE8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801376E4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80137D78);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80138160);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80138800);

void func_actor_401300_80138B24(Actor401300* arg0)
{
    Actor401300Work* work   = arg0->field_1C;
    GpEnemy*         enemy  = arg0->field_20;
    Task*            player = Game_GetPtrSlot(3);

    if (work->field_4 != 0) {
        work->field_8A6 = 0x10;
        work->field_8A2 = 6;
        work->field_89C = 2;
        if ((s16)Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 0), 0) == 1) {
            ((GameActor*)player->idMap)->field_956 = 0xA;
        }
        work->field_CB0 = 2;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
        work->field_D22 = 0;
    }
    if (work->field_6C & 2) {
        work->field_910.field_0 = &arg0->field_2C->field_8[1];
        work->field_910.field_4 = 0x300;
        work->field_910.field_6 = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->field_2C->field_8[5], NULL, &work->field_910);
        work->field_0 = 0xE;
    }
    work->field_898 = work->field_5E & 0x3FF;
    func_actor_401300_80133A3C(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[2].coord, -0x80, 0);
    arg0->field_2C->field_8[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[3]);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[3].coord, -0x80, 0);
    arg0->field_2C->field_8[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[2]);
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80138CF8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80138FCC);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80139134);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80139520);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801397F8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80139AB0);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013A208);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013A5C0);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013AAE8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013AE48);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013B6E8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013BB30);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013CBAC);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013D2AC);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013D6C4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013DADC);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013E930);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013F628);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80140300);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8014046C);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801405DC);

void func_actor_401300_8014148C(void)
{
}

s32 func_actor_401300_80141494(Actor401300* arg0, s32 arg1, Actor401300Msg* arg2)
{
    Actor401300Work* work = arg0->field_1C;

    switch (arg2->field_4) {
        case 0:
            work->field_8A2 = 0x22;
            break;
        case 1:
            work->field_8A2 = 0x23;
            break;
        case 2:
            work->field_8A2 = 0x24;
            break;
        case 3:
            work->field_8A2 = 0x25;
            break;
        case 4:
            work->field_8A2 = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

/* Closes this unit's .rodata after the 5-entry jump table above, so
   actor_401300_3's tables start at 0x80132044. Nothing reads it. */
const u32 D_actor_401300_80132040 = 0;
