#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/acropolis_square.h"

extern u8                      D_8007216C;
extern s8                      D_80072310;
extern TaskDesc                RoomsShared80181228Desc;
extern s32                     D_acropolis_square_8018382C;
extern AcropolisSquareCutscene D_acropolis_square_801888AC;
extern s32                     D_acropolis_square_80183834;
extern s32                     D_acropolis_square_801838DC;
extern s32                     D_acropolis_square_80188898;
extern s32                     D_acropolis_square_801888A0;
extern s32                     D_acropolis_square_801888A4;
extern GsCOORDINATE2           D_acropolis_square_801888CC;

INCLUDE_RODATA("rooms/nonmatchings/acropolis_square/acropolis_square_6", RoomsShared8017d878Table);

s32 func_acropolis_square_80181794(Task* task, s32 msgId, RoomEventMsg* arg2, RoomEventMsg* arg3)
{
    GpAreaKey key; // filled in but never used: the Gp_SetAreaObjId call the
                   // sibling rooms make with it is absent here
    u16 temp_s1;

    key.field_3 = 1;
    key.field_2 = 4;
    *arg3       = *arg2;
    if (arg2->msgId == 9) {
        if ((D_acropolis_square_8018382C != 0) && (arg2->field_5 == 0)) {
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 2);
        }
        if (arg2->msgId == 9) {
            if (GameFlag_GetNibble(9) & 1) {
                arg3->field_3 = 2;
            }
        }
        return 1;
    }
    if (arg2->msgId == 2) {
        if ((D_acropolis_square_8018382C != 0) && (arg2->field_5 == 0)) {
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 2);
        }
        if (GameFlag_GetNibble(0) < 2) {
            return 1;
        }
        if ((GameFlag_GetNibble(0) == 2) || (GameFlag_GetNibble(0) >= 3)) {
            if (arg2->field_5 == 0) {
                do {
                    Gp_SetNibbleIf(arg2->field_6, 2);
                    Gp_RunCapCmd1(1);
                } while (0);
            }
            return 0;
        }
    }
    if (arg2->msgId == 0x11) {
        if (GameFlag_GetNibble(0) < 2) {
            return 1;
        }
        if ((GameFlag_GetNibble(0) == 2) || (GameFlag_GetNibble(0) >= 3)) {
            if (arg2->field_5 == 0) {
                do {
                    Gp_SetNibbleIf(arg2->field_6, 2);
                    Gp_RunCapCmd1(1);
                } while (0);
            }
            return 0;
        }
    }
    temp_s1 = arg2->msgId;
    if (temp_s1 == 3) {
        if (arg2->field_5 == 0) {
            if (GameFlag_GetNibble(0) < 2) {
                if (GameFlag_GetNibble(0x21) < 2) {
                    arg3->field_3 = 1;
                } else {
                    arg3->field_3 = 2;
                }
            } else {
                arg3->field_3 = temp_s1;
            }
        }
    }
    return 1;
}
s32 func_acropolis_square_801819BC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 var_a0;

    if (arg2 == 2) {
        if (Mc_SaveData.field_8 == 7) {
            Mc_SaveData.field_8 = 1;
        }
        D_acropolis_square_801888AC.camera   = 9;
        D_acropolis_square_801888AC.field_1  = 1;
        D_acropolis_square_801888AC.capFile  = 1;
        D_acropolis_square_801888AC.sndStart = 0x51010001;
        D_acropolis_square_801888AC.snd8     = 0x51010007;
        D_acropolis_square_801888AC.snd10    = 0x51010006;
        D_acropolis_square_801888AC.sndC     = 0x5101000B;
        D_acropolis_square_801888AC.field_2  = D_acropolis_square_8018382C;
        D_acropolis_square_8018382C          = 0;
        Task_SpawnFromTable(&RoomsShared80181228Desc, 0, 2, (s32)&D_acropolis_square_801888AC);
    }
    if ((arg2 == 0xE) && (GameFlag_GetNibble(0x124) == 0)) {
        GameFlag_SetNibble(0x124, 1);
        Gp_SpawnIfCapIdle(0xE, 1);
    }
    if ((arg2 == 0x10) && (GameFlag_GetNibble(0x156) == 0)) {
        GameFlag_SetNibble(0x156, 1);
        var_a0 = 0x11;
        if (D_80072310 != 1) {
            var_a0 = 0x10;
        }
        Gp_SpawnIfCapIdle(var_a0, 1);
    }
    return 0;
}
/// Siren task for the square. States 0-2 arm the scene and tick, 3 fires the
/// first siren blast, 4 repeats it every 0x79 frames until the player answers,
/// and 5 waits for the scripted phase to advance before handing the scene off
/// to the slot-5 task and killing itself.
void func_acropolis_square_80181AEC(Task* task)
{
    s32 pan;
    s32 pan2;
    s32 pan3;
    s32 count;
    s32 count2;
    u32 state;

    state = task->state;
    switch (state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            D_acropolis_square_8018382C = 1;
            D_acropolis_square_80188898 = 0;
            func_800E8634((s32)&D_acropolis_square_80183834, 0, (s32)&D_acropolis_square_801838DC);
            goto advance;

        case 3:
            D_acropolis_square_801888CC.coord.t[0] = 0x19AA;
            D_acropolis_square_801888CC.coord.t[1] = -0xF96;
            D_acropolis_square_801888CC.coord.t[2] = 0x8DE;
            D_acropolis_square_801888CC.sub        = &Gfx_ViewCoord;
            Gp_UpdateCoord(&D_acropolis_square_801888CC);
            pan = Gp_GetObjPan((GpObj38*)&D_acropolis_square_801888CC);
            SndEvt_EnqueueType6(
                0x51010009, (s8)pan, (s8)Gp_GetObjDepth((GpObj38*)&D_acropolis_square_801888CC));
            goto advance;

        case 4:
            count                       = D_acropolis_square_80188898 + 1;
            D_acropolis_square_80188898 = count;
            if (count >= 0x79) {
                D_acropolis_square_801888CC.coord.t[0] = 0x19AA;
                D_acropolis_square_801888CC.coord.t[1] = -0xF96;
                D_acropolis_square_801888CC.coord.t[2] = 0x8DE;
                D_acropolis_square_80188898            = 0;
                D_acropolis_square_801888CC.sub        = &Gfx_ViewCoord;
                Gp_UpdateCoord(&D_acropolis_square_801888CC);
                pan2 = Gp_GetObjPan((GpObj38*)&D_acropolis_square_801888CC);
                SndEvt_EnqueueType6(0x51010009, (s8)pan2,
                                    (s8)Gp_GetObjDepth((GpObj38*)&D_acropolis_square_801888CC));
            }
            if (Game_Session->field_1 != 0) {
                return;
            }
            Gp_MsgPlayerWeapon(1);
            /* fallthrough */

        case 1:
        case 2:
        advance:
            task->state += 1;
            return;

        case 5:
            if ((u32)(D_8007216C - 5) >= 3U) {
                if (D_8007216C == 9) {
                    goto checkArmed;
                }
                goto handOff;
            }
        checkArmed:
            if (D_acropolis_square_8018382C == 0) {
            handOff:
                if (D_acropolis_square_8018382C != 0) {
                    GameFlag_SetNibble(3, 0);
                    GameFlag_SetNibble(0x155, 2);
                    D_acropolis_square_8018382C = 0;
                }
                Gp_DispatchMsg((Task*)Game_GetPtrSlot(5), 0xC1F, 0, 0);
                SndEvt_EnqueueType7(0x51010009, 1);
                Task_Kill(task);
                return;
            }
            count2                      = D_acropolis_square_80188898 + 1;
            D_acropolis_square_80188898 = count2;
            if (count2 >= 0x79) {
                D_acropolis_square_801888CC.coord.t[0] = 0x19AA;
                D_acropolis_square_801888CC.coord.t[1] = -0xF96;
                D_acropolis_square_801888CC.coord.t[2] = 0x8DE;
                D_acropolis_square_80188898            = 0;
                D_acropolis_square_801888CC.sub        = &Gfx_ViewCoord;
                Gp_UpdateCoord(&D_acropolis_square_801888CC);
                pan3 = Gp_GetObjPan((GpObj38*)&D_acropolis_square_801888CC);
                SndEvt_EnqueueType6(0x51010009, (s8)pan3,
                                    (s8)Gp_GetObjDepth((GpObj38*)&D_acropolis_square_801888CC));
            }
            break;
    }
}
/// Scrolling backdrop task: three 256x240 sprite strips (the last one half
/// width) tiled across the screen from `D_acropolis_square_801888A0`, each with
/// its own texture page. States 0-3 slide the strip in and hold it for a while,
/// state 4 kills the task; every state still draws.
void func_acropolis_square_80181DD0(Task* task)
{
    SPRT*     p;
    DR_TPAGE* dr;
    s32       x;
    s32       i;
    s32       tpageX;
    s32       count;
    s32       count2;
    s32       pos;

    switch (task->state) {
        case 0:
            D_acropolis_square_801888A0 = -0x140;
            D_acropolis_square_801888A4 = 0;
            task->state                += 1;
            break;

        case 1:
            count                       = D_acropolis_square_801888A4 + 1;
            D_acropolis_square_801888A4 = count;
            if (count >= 0x2E) {
                task->state += 1;
            }
            break;

        case 2:
            pos                         = D_acropolis_square_801888A0 + 1;
            D_acropolis_square_801888A0 = pos;
            if (pos >= 0) {
                D_acropolis_square_801888A4 = 0;
                task->state                += 1;
            }
            break;

        case 3:
            count2                      = D_acropolis_square_801888A4 + 1;
            D_acropolis_square_801888A4 = count2;
            if (count2 >= 0x1F) {
                task->state += 1;
            }
            break;

        case 4:
            D_8007216C = 0xD;
            Task_Kill(task);
            break;
    }

    x = D_acropolis_square_801888A0;
    for (i = 0; i < 3; i++) {
        tpageX         = 0x1C0 + i * 0x80;
        p              = (SPRT*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
        setlen(p, 4);
        setcode(p, 0x65);
        p->x0 = x - 0xA0;
        p->y0 = -0x78;
        p->u0 = 0;
        p->v0 = 0;
        if (i == 2) {
            p->w = 0x80;
            p->h = 0xF0;
        } else {
            p->w = 0x100;
            p->h = 0xF0;
        }
        p->clut = GetClut(0, 0xFF);
        addPrim(&Gpu_CurrentOt[4], p);

        dr             = Gpu_PrimCursor;
        Gpu_PrimCursor = dr + 1;
        setDrawTPage(dr, 0, 1, GetTPage(1, 0, tpageX, 0x100));
        addPrim(&Gpu_CurrentOt[4], dr);

        x += 0x100;
    }
}
INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_6", func_acropolis_square_80182048);

s32 func_acropolis_square_801820D8(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 0) {
        Gp_SpawnIfCapIdle(5, 0);
    }
    return 0;
}
/* The retail overlay has a zero word at 0x8017D6F0, right after this TU's
 * second jump table: aspsx rounded each object's `.rdata` up to the 8-byte
 * alignment GCC gives a jump table, and modern gas does not. Ask for the pad
 * explicitly, or every rodata symbol from 0x8017D6F4 on - and with it the whole
 * `.text` - shifts down by 4. */
#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 3\n"
        ".section .text\n");
#endif
