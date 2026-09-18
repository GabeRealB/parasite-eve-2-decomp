#include "common.h"

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/sound.h"

#include <psyq/libgpu.h>

/// `Gp_StateF0.field_4` (0x801153F4), the byte the room's cutscene driver sets on
/// enter and clears on exit. Declared as a one-element array so the store keeps the
/// in-struct memory attribute a real struct-member store would have: that is what makes
/// the store alias the `task->state` load for the scheduler, which leaves the state
/// load where the source put it and lets jump.c cross-jump case 0's `task->state++`
/// into the shared tail. A plain `u8` declaration assembles to the same bytes but is
/// scheduled one statement early.
extern u8 D_801153F4[1];
extern u8 D_80115690;
extern u8 D_8007216C;

void func_neo_ark_pyramid_8017DAC0(s32 arg0);

/// The `0x0..0x14` header (overlay id + `RoomsShared8017d878Table`) stays in assembly as
/// `neo_ark_pyramid_hdr.rodata.s` (`rodata_head` in the manifest); the jump table below is
/// compiler-generated and has to start this unit's `.rodata`, which is what the cut does.
void func_neo_ark_pyramid_8017D600(Task* task)
{
    u16 count;

    switch (task->state) {
        case 0:
            D_8007216C             = 8;
            gGameSession->field_68 = 1;
            gGameSession->field_1  = 1;
            D_801153F4[0]          = 2;
            task->state++;
            break;
        case 1:
            Gp_RunCapCmd(1, 0);
            task->state++;
            break;
        case 2:
            D_80115690 = 1;
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 3:
            if (Gp_GetCapEventKey() == 0xC) {
                task->state = 0xA;
                break;
            }
            SndEvt_EnqueueType6(0x55200003, 0, 0);
            task->killCountdown = 0;
            task->state++;
            break;
        case 4:
            count               = task->killCountdown + 4;
            task->killCountdown = count;
            if ((s16)count >= 0x156) {
                GameFlag_SetNibble(0xEC, GameFlag_GetNibble(0xEC) + 1);
                func_neo_ark_pyramid_8017DAC0(0);
                if (GameFlag_GetNibble(0xEC) >= 4) {
                    SndEvt_EnqueueType6(0x55200005, 0, 0);
                    Gp_RunCapCmd(2, 0);
                    task->state++;
                } else {
                    SndEvt_EnqueueType6(0x55200004, 0, 0);
                    task->state = 1;
                }
            } else {
                func_neo_ark_pyramid_8017DAC0((s16)count);
            }
            break;
        case 5:
            D_80115690 = 1;
            if (Gp_CapBusy() == 0) {
                task->state = 0xA;
            }
            break;
        case 10:
            D_8007216C             = 3;
            gGameSession->field_68 = 0;
            gGameSession->field_1  = 0;
            D_801153F4[0]          = 0;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            Task_Kill(task);
            break;
    }
}

void func_neo_ark_pyramid_8017D7F4(s32 arg0)
{
    POLY_FT4* prim;
    s16       src[4][2];
    s16       dst[4][2];
    s32       i;

    src[0][0] = -0x57;
    src[0][1] = -0x57;
    src[1][0] = 0x57;
    src[1][1] = -0x57;
    src[2][0] = -0x57;
    src[2][1] = 0x57;
    src[3][0] = 0x57;
    src[3][1] = 0x57;
    for (i = 0; i < 4; i++) {
        dst[i][0] = (src[i][0] * rcos(arg0) - src[i][1] * rsin(arg0)) >> 12;
        dst[i][1] = (src[i][0] * rsin(arg0) + src[i][1] * rcos(arg0)) >> 12;
    }
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2D);
    prim->x0    = dst[0][0];
    prim->y0    = dst[0][1];
    prim->x1    = dst[1][0];
    prim->y1    = dst[1][1];
    prim->x2    = dst[2][0];
    prim->y2    = dst[2][1];
    prim->x3    = dst[3][0];
    prim->y3    = dst[3][1];
    prim->u0    = 1;
    prim->v0    = 1;
    prim->u1    = 0xAF;
    prim->v1    = 1;
    prim->u2    = 1;
    prim->v2    = 0xAF;
    prim->u3    = 0xAF;
    prim->v3    = 0xAF;
    prim->clut  = 0x3FC0;
    prim->tpage = 0x8E;
    addPrim(Gpu_CurrentOt + 0xC, prim);
}

s32 func_neo_ark_pyramid_8017D9F0(void)
{
    return 0;
}
