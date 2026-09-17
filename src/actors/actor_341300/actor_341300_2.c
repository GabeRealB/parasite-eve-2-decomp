#include "common.h"

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern Task* D_actor_341300_80165A2C;

extern TaskDesc D_actor_341300_80165A68;

extern TaskDesc D_actor_341300_80165208;

extern Task* D_actor_341300_80165AA4;

extern s8 D_8007272D;

void func_actor_341300_80161E84(void);

void func_actor_341300_8016398C(s32 arg0);

void func_actor_341300_801639CC(s32 arg0);

void func_actor_341300_8016241C(void)
{
    D_actor_341300_80165AA4 = Task_SpawnFromTable(&D_actor_341300_80165208, 0, 0, 0);
}

void func_actor_341300_80162450(void)
{
    if (D_actor_341300_80165AA4 != NULL) {
        D_actor_341300_80165AA4->state         = -1;
        D_actor_341300_80165AA4->killCountdown = 0;
        D_actor_341300_80165AA4                = NULL;
    }
}

void func_actor_341300_80162478(Task* arg0)
{
    s16 next;
    s16 count;

    switch (arg0->state) {
        case 0:
            next                = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = next;
            if (next != 0x3D) {
                func_actor_341300_80161E84();
                return;
            }
            return;
        case 1:
            count = arg0->killCountdown;
            if (count < 0x1E) {
                if ((count != 0xA) && (count != 0x14)) {
                    func_actor_341300_80161E84();
                }
                arg0->killCountdown = (u16)arg0->killCountdown + 1;
                return;
            }
        default:
            Task_Kill(arg0);
            break;
    }
}

void func_actor_341300_80162530(void)
{
    D_actor_341300_80165AA4 = Task_SpawnFromTable(&D_actor_341300_80165208, 1, 0, 0);
}

void func_actor_341300_80162564(s16 arg0)
{
    func_actor_341300_8016398C(arg0);
}

void func_actor_341300_80162588(s16 arg0)
{
    func_actor_341300_801639CC(arg0);
}

/// Offset vector the four `Gp_SpawnEff` calls below spawn on. splat migrates
/// it into `func_actor_341300_801625AC`'s own `.s`, so there is no standalone
/// rodata file to `INCLUDE_RODATA`; defining it here emits it where the
/// function sits, ahead of the `jtbl_actor_341300_80161E6C` include below.
const SVECTOR D_actor_341300_80161E64 = { 100, -200, -100, 0 };

void func_actor_341300_801625AC(void)
{
    SVECTOR        vec   = D_actor_341300_80161E64;
    GsCOORDINATE2* coord = &((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8[2];

    Gp_SpawnEff(0x60055, coord, 0x10013300, &vec);
    Gp_SpawnEff(0x60055, coord, 0x10112280, &vec);
    Gp_SpawnEff(0x60055, coord, 0x10112280, &vec);
    Gp_SpawnEff(0x60055, coord, 0x10112280, &vec);
}

void func_actor_341300_80162680(s8 arg0)
{
    D_8007272D = arg0;
}

void func_actor_341300_8016268C(void)
{
    D_actor_341300_80165AA4 = 0;
}

void func_actor_341300_80162698(Task* arg0)
{
    s16 i;
    s16 next;
    u16 count;

    switch (arg0->state) {
        case 0:
            i = 0;
            do {
                Task_SpawnFromTable(&D_actor_341300_80165A68, 1, 0, (s32)arg0);
                next = i + 1;
                i    = next;
            } while (next < 0xA);
            goto done;
        case 1:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            i                   = 0;
            if ((s16)count >= 0x1F) {
                do {
                    Task_SpawnFromTable(&D_actor_341300_80165A68, 1, 0, (s32)arg0);
                    next = i + 1;
                    i    = next;
                } while (next < 0xA);
                goto done;
            }
            break;
        case 2:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            i                   = 0;
            if ((s16)count >= 0x10) {
                do {
                    Task_SpawnFromTable(&D_actor_341300_80165A68, 1, 1, (s32)arg0);
                    next = i + 1;
                    i    = next;
                } while (next < 0xA);
                goto done;
            }
            break;
        case 3:
        case 4:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            i                   = 0;
            if ((s16)count >= 0x10) {
                do {
                    Task_SpawnFromTable(&D_actor_341300_80165A68, 1, 3, (s32)arg0);
                    Task_SpawnFromTable(&D_actor_341300_80165A68, 1, 1, (s32)arg0);
                    next = i + 1;
                    i    = next;
                } while (next < 0xA);
            done:
                arg0->killCountdown = 0;
                arg0->state         = arg0->state + 1;
            }
            break;
        case 5:
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_80162878);

void func_actor_341300_80163028(Task* arg0)
{
    u16 count;

    switch (arg0->state) {
        case 0:
            arg0->killCountdown = 0;
            arg0->state         = arg0->state + 1;
            break;
        case 1:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            if ((s16)count % 3 == 0) {
                Task_SpawnFromTable(&D_actor_341300_80165A68, 3, 0, (s32)arg0);
                Task_SpawnFromTable(&D_actor_341300_80165A68, 3, 1, (s32)arg0);
                Task_SpawnFromTable(&D_actor_341300_80165A68, 3, 1, (s32)arg0);
                arg0->state = arg0->state + 1;
            }
            break;
        case 2:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            if ((s16)count % 3 == 0) {
                Task_SpawnFromTable(&D_actor_341300_80165A68, 3, 0, (s32)arg0);
                Task_SpawnFromTable(&D_actor_341300_80165A68, 3, 0, (s32)arg0);
                Task_SpawnFromTable(&D_actor_341300_80165A68, 3, 1, (s32)arg0);
                arg0->state = arg0->state - 1;
            }
            break;
    }
    if (arg0->killCountdown >= 0x1F) {
        arg0->state = 3;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_801631D4);

void func_actor_341300_8016398C(s32 arg0)
{
    if ((arg0 << 0x10) == 0) {
        D_actor_341300_80165A2C = Task_SpawnFromTable(&D_actor_341300_80165A68, 0, 0, 0);
    }
}

void func_actor_341300_801639CC(s32 arg0)
{
    if (((arg0 << 0x10) == 0) && (D_actor_341300_80165A2C != NULL)) {
        Task_Kill(D_actor_341300_80165A2C);
        D_actor_341300_80165A2C = NULL;
    }
}

void func_actor_341300_80163A10(Task* arg0)
{
    s16 i;
    s16 next;
    s32 tmp;
    s32 state;
    u16 count;

    if (arg0->state < 3) {
        if (arg0->state <= 0) {
            if (arg0->state == 0) {
                tmp = arg0->state;
                SOFT_TOUCH_REG(tmp);
                state               = tmp + 1;
                arg0->killCountdown = 0;
                goto store;
            }
        } else {
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            i                   = 0;
            if ((s16)count >= 0x10) {
                do {
                    Task_SpawnFromTable(&D_actor_341300_80165A68, 1, 0, (s32)arg0);
                    Task_SpawnFromTable(&D_actor_341300_80165A68, 1, 1, (s32)arg0);
                    next = i + 1;
                    i    = next;
                } while (next < 0xA);
                arg0->killCountdown = 0;
                state               = arg0->state + 1;
            store:
                arg0->state = state;
            }
        }
    }
}
