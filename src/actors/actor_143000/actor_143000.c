#include "common.h"

#include "actors/actor_143000.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "psyq/strings.h"
#include "rooms/room_common.h"
extern TaskDesc D_actor_143000_801350C8;

extern s32              D_80070F6C;
extern s8               D_8007218B;
extern u8               D_801153F4;
extern GpAreaApplyRec   D_80186488;
extern GpAreaApplyRec   D_8018649C;
extern char             D_actor_143000_80131EB0[];
extern char             D_actor_143000_80131EBC[];
extern TaskDesc         D_actor_143000_80134558;
extern Actor143000Rect  D_actor_143000_80134580[];
extern s32              D_actor_143000_801351B0;
extern s32              D_actor_143000_80135870;
extern s32              D_actor_143000_80135A20;
extern s32              D_actor_143000_80135AE0;
extern Actor143000Spawn D_actor_143000_80135C08;
extern u8               D_actor_143000_80135C0C;
extern s32              D_actor_143000_80135C14;
extern s32              D_actor_143000_80135C18;
extern s32              D_actor_143000_80135C1C;
extern char             D_actor_143000_80135C20[];
extern u8               D_actor_143000_80135C38[];

INCLUDE_RODATA("actors/nonmatchings/actor_143000/actor_143000", D_actor_143000_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", ActorsShared8013845cSub1);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_801323E0);

void func_actor_143000_801324C8(Task* arg0)
{
    Actor143000Work* work;
    Actor143000Rect* p;
    u8               temp_a0;

    p    = D_actor_143000_80134580;
    work = Mem_Calloc(0x1CU, false);
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    arg0->spawnArg2         = Task_SpawnFromTable(&D_actor_143000_80134558, 0, 1, 0);
    arg0->idMap             = (TaskIdMap*)work;
    temp_a0                 = Mc_SaveData.field_4;
    Mc_SaveData.field_4     = 0xB;
    D_actor_143000_80135C0C = temp_a0;
    arg0->state            += 1;
    work->field_4           = 0;
    Display_AcquireRef();
    if (p->field_8 != -1) {
        do {
            p->field_B = 0;
            p++;
        } while (p->field_8 != -1);
    }
    work->field_7          = 0;
    work->field_12         = 1;
    work->field_13         = 0;
    work->field_16         = 0xA00;
    work->field_C          = 0;
    work->field_10         = 0;
    work->field_18         = 0x10;
    work->field_1A         = 0;
    Game_Session->field_66 = 1;
    Game_Session->field_68 = 1;
    Gp_MsgPlayerWeapon(0);
    Gp_MsgPlayer3F3(0);
}

void func_actor_143000_801325F0(Actor143000* arg0)
{
    Actor143000Work*  work;
    u8                u;
    Actor143000Rect*  p;
    POLY_FT4*         prim;
    RoomActionPrompt* prompt;
    s16               dx;
    s16               dy;
    s16               x;
    s16               y;
    s16               w;
    s16               h;
    u8                v;
    u8                uw;
    u8                vh;

    work                   = arg0->field_1C;
    Game_Session->field_68 = 1;
    Game_Session->field_1  = 1;
    p                      = D_actor_143000_80134580;
    D_801153F4             = 2;
    prompt                 = &D_80114D28;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (D_8007218B == 9) {
        func_actor_143000_80133C2C();
    }
    work->field_2 = 0;
    if (func_actor_143000_80133AE8(p, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            for (; p->field_8 != -1; p++) {
                if (p->field_B != 0) {
                    if (work->field_7 != 0 && p->field_8 == 5) {
                        SndEvt_EnqueueType6(0x541F0013, 0, 0);
                        prompt->mode     = 0;
                        prompt->targetId = 0;
                        work->field_8    = prompt->screen.xy.x;
                        work->field_A    = prompt->screen.xy.y;
                        arg0->field_30   = 8;
                        return;
                    }
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    work->field_2    = p->field_8;
                    work->field_6    = p->field_A;
                    arg0->field_30   = 3;
                    return;
                }
            }
        }
        for (p = D_actor_143000_80134580; p->field_8 != -1; p++) {
            if (p->field_B != 0) {
                if (p->field_8 != 3) {
                    if (p->field_8 == 5) {
                        prim           = (POLY_FT4*)Gpu_PrimCursor;
                        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                        SetPolyFT4(prim);
                        setShadeTex(prim, 1);
                        x  = (s16)(prompt->screen.xy.x - p->x) / 16 * 16;
                        y  = (s16)(prompt->screen.xy.y - p->y) / 16 * 16;
                        dx = p->x;
                        dy = p->y;
                        u  = x;
                        v  = y + 0x70;
                        x += dx;
                        y += dy;
                        setXYWH(prim, x, y, 16, 16);
                        setUVWH(prim, u, v, 16, 16);
                        prim->tpage = 0x16;
                        prim->clut  = 0x3DC1;
                        addPrim(&Gpu_CurrentOt[0x3FE], prim);
                    }
                } else {
                    prim           = (POLY_FT4*)Gpu_PrimCursor;
                    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                    SetPolyFT4(prim);
                    setShadeTex(prim, 1);
                    u  = 0x30;
                    x  = p->x;
                    w  = p->w;
                    y  = p->y;
                    h  = p->h;
                    uw = p->w;
                    vh = p->h;
                    setXY4(prim, x, y, x + w, y, x, y + h, x + w, y + h);
                    setUV4(prim, u, 0xB8, uw + 0x30, 0xB8, u, vh - 0x48, uw + 0x30, vh - 0x48);
                    prim->tpage = 0x16;
                    prim->clut  = 0x3DC1;
                    addPrim(&Gpu_CurrentOt[0x3FE], prim);
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        arg0->field_30 = 5;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_143000/actor_143000", D_actor_143000_80131E54);

INCLUDE_RODATA("actors/nonmatchings/actor_143000/actor_143000", D_actor_143000_80131E84);

INCLUDE_RODATA("actors/nonmatchings/actor_143000/actor_143000", D_actor_143000_80131EB0);

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl D_actor_143000_80131EBC\n"
        "D_actor_143000_80131EBC:\n"
        "\t.asciz \"YSD\"\n"
        "\t.align 2\n"
        "\t.asciz \"\"\n"
        "\t.align 2\n"
        ".section .text\n");
#endif

void func_actor_143000_80132A04(Actor143000* arg0)
{
    s32              var_s2;
    Actor143000Work* temp_s0;

    COMPILER_BARRIER();
    temp_s0 = arg0->field_1C;
    var_s2  = 0;
    if (arg0->field_2A == 0) {
        if ((strcmp(D_actor_143000_80135C20, D_actor_143000_80131EB0) == 0) || ((strcmp(D_actor_143000_80135C20, D_actor_143000_80131EBC) == 0) && (D_8007218B != 0))) {
            var_s2 = 1;
        }
        temp_s0->field_C = var_s2;
    }
    if (temp_s0->field_C != 0) {
        switch (arg0->field_2A) {
            case 0:
                temp_s0->field_12 = 2;
                break;
            case 0x3C:
                temp_s0->field_12 = 3;
                temp_s0->field_14 = 0x7C;
                break;
            case 0x46:
                temp_s0->field_14 = 0x83;
                break;
            case 0x50:
                temp_s0->field_14 = 0x8A;
                break;
            case 0x5A:
                temp_s0->field_14 = 0;
                break;
            case 0x78:
                SndEvt_EnqueueType6(0x541F0011, 0, 0);
                temp_s0->field_13 = 1;
                break;
            case 0x96:
                temp_s0->field_12 = 4;
                temp_s0->field_13 = 0;
                break;
            case 0xF0:
                temp_s0->field_12 = 5;
                break;
            case 0x14A:
                arg0->field_30                  = 0xA;
                D_actor_143000_80135C08.field_0 = 0;
                D_actor_143000_80135C08.field_1 = 0;
                D_actor_143000_80135C08.field_2 = 0xF;
                arg0->field_2A                  = 0xF;
                Task_Spawn(1, 0x31, 0, (s32)&D_actor_143000_80135C08);
                break;
        }
    } else {
        switch (arg0->field_2A) {
            case 0:
                temp_s0->field_12 = 2;
                break;
            case 0x3C:
                temp_s0->field_12 = 3;
                temp_s0->field_14 = 0x7C;
                break;
            case 0x46:
                temp_s0->field_14 = 0x83;
                break;
            case 0x50:
                temp_s0->field_14 = 0x8A;
                break;
            case 0x5A:
                temp_s0->field_14 = 0;
                break;
            case 0x78:
                SndEvt_EnqueueType6(0x541F0012, 0, 0);
                temp_s0->field_13 = 2;
                break;
            case 0x96:
                temp_s0->field_12 = 6;
                temp_s0->field_14 = 0x7C;
                temp_s0->field_13 = 0;
                break;
            case 0xA0:
                temp_s0->field_14 = 0x83;
                break;
            case 0xAA:
                temp_s0->field_14 = 0x8A;
                break;
            case 0xB4:
                temp_s0->field_14 = 0;
                break;
            case 0xD2:
                temp_s0->field_13 = 2;
                break;
            case 0xF0:
                temp_s0->field_12 = 7;
                temp_s0->field_13 = 0;
                break;
            case 0x14A:
                temp_s0->field_12 = 1;
                temp_s0->field_10 = 0;
                arg0->field_30    = 2;
                break;
        }
    }
    arg0->field_2A = (s16)((u16)arg0->field_2A + 1);
}

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80132D10);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80133334);
