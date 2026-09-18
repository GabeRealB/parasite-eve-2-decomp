#include "common.h"

#include "actors/actor_356100.h"
#include "gameplay/D4.h"

extern u32 Gp_LcgState;

INCLUDE_RODATA("actors/nonmatchings/actor_356100/actor_356100_2", ActorsShared80135df4Table);

s32 func_actor_356100_8016A0B8(Actor356100* arg0, s32 arg1, Actor356100Event* arg2)
{
    Actor356100Work* work = arg0->field_1C;
    s32              code;

    work->field_B58[0] = arg2->b[0];
    work->field_B58[1] = arg2->b[1];
    work->field_B58[2] = arg2->b[2];
    if (arg2->w[0] == 0xB05) {
        code = arg2->w[1];
        switch (code) {
            case 0:
            case 2:
                work->field_0 = 0;
                return 1;
            case 1:
                work->field_0   = 0x1E;
                work->field_97E = code;
                work->field_978 = 2;
                return 1;
            default:
                return 0;
        }
    }
    return 0;
}

void func_actor_356100_8016A158(Task* task)
{
    Actor356100Work* work;
    GpEnemy*         enemy;

    work  = (Actor356100Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_B5C != NULL) {
            Task_Kill(work->field_B5C);
        }
        if (work->field_B60 != NULL) {
            Task_Kill(work->field_B60);
        }
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

void func_actor_356100_8016A1D8(Actor356100* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 1;
        obj->flags                  |= 0x80;
    }
}

void func_actor_356100_8016A21C(Actor356100* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_97A = 0;
        work->field_982 = 0x10;
        work->field_97E = 2;
        func_actor_356100_80163508(arg0);
    } else {
        arg0->field_2C->coords->flg = 0;
        func_actor_356100_80163508(arg0);
    }
}

void func_actor_356100_8016A2AC(Actor356100* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 3;
        func_actor_356100_80163508(arg0);
    } else {
        arg0->field_2C->coords->flg = 0;
        func_actor_356100_80163508(arg0);
    }
}

void func_actor_356100_8016A340(Actor356100* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 0xB;
        func_actor_356100_80163508(arg0);
    } else {
        arg0->field_2C->coords->flg = 0;
        func_actor_356100_80163508(arg0);
    }
}

void func_actor_356100_8016A3D4(Actor356100* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 0xB;
        func_actor_356100_80163508(arg0);
    } else {
        arg0->field_2C->coords->flg = 0;
        func_actor_356100_80163508(arg0);
    }
}

void func_actor_356100_8016A468(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;
    GpAnimArg*       msg;
    void*            player;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_982 = 0x10;
        work->field_97E = 6;
        work->field_978 = 2;
        msg             = &D_actor_356100_80173244;
        msg->field_4    = 2;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)msg, 0);
        player = Game_GetPtrSlot(3);
        Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 0), 0);
    }
    if (work->field_68 & 2) {
        work->field_0 = 0xE;
    }
    work->field_974 = work->field_5A & 0x3FF;
    func_actor_356100_80163508(arg0);
}

void func_actor_356100_8016A550(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags = 0;
        work->field_9BC       = 0x180;
        enemy->node.field_4   = 0;
        work->field_978       = 2;
        work->field_97E       = 8;
        work->field_990       = 0;
        work->field_98E       = 0;
        work->field_982       = work->field_984;
    }
    func_actor_356100_80163508(arg0);
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
}

void func_actor_356100_8016A5DC(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags = 0;
        work->field_9BC       = 0x180;
        enemy->node.field_4   = 0;
        work->field_978       = 2;
        work->field_97E       = 0x16;
        work->field_990       = 0;
        work->field_98E       = 0;
        work->field_982       = work->field_984;
    }
    func_actor_356100_80163508(arg0);
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
}

void func_actor_356100_8016A668(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        work->field_6 = work->field_B54 + (((u32)Gp_LcgState >> 16) & 0xF);
    }
    if ((s16)--work->field_6 < 0) {
        switch (work->field_97E) {
            case 0xB:
                work->field_0 = 0xF;
                break;
            case 0xC:
                work->field_0 = 0x10;
                break;
        }
    }
    if (enemy->field_40 <= 0) {
        work->field_0 = 0x15;
    }
}

void func_actor_356100_8016A710(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags = 0;
        work->field_9BC       = 0x180;
        enemy->node.field_4   = 0;
        work->field_978       = 1;
        work->field_97E       = 0xA;
        work->field_982       = 0x10;
        work->field_990       = 0;
        work->field_98E       = 0;
        if (enemy->field_40 < 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    func_actor_356100_80163508(arg0);
    if (work->field_68 & 1) {
        if (work->field_97E == 0xA) {
            work->field_97E = 0xB;
            work->field_978 = 2;
            func_actor_356100_80163508(arg0);
        }
        if ((work->field_68 & 1) && (work->field_97E == 0xB)) {
            if (enemy->field_40 > 0) {
                if (work->field_B3A <= 0) {
                    work->field_0 = 0x11;
                } else {
                    work->field_0 = 4;
                }
            } else {
                work->field_0 = 0x15;
            }
        }
    }
}

void func_actor_356100_8016A834(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags = 0;
        work->field_9BC       = 0x180;
        enemy->node.field_4   = 0;
        work->field_978       = 1;
        work->field_97E       = 0xC;
        work->field_982       = 0x10;
        work->field_990       = 0;
        work->field_98E       = 0;
        if (enemy->field_40 < 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    func_actor_356100_80163508(arg0);
    if (work->field_68 & 1) {
        if (enemy->field_40 > 0) {
            if (work->field_B3A <= 0) {
                work->field_0 = 0x11;
            } else {
                work->field_0 = 4;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}
