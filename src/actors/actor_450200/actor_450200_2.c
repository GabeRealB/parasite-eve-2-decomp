#include "common.h"

#include "actors/actor_450200.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
extern TaskDesc D_actor_450200_8013FB40;

extern u8 D_8007216D;

extern void func_8017FA98(s32);
extern void func_80180DAC(s32);

extern s32      D_actor_450200_80138870;
extern s32      D_actor_450200_80138A68;
extern s32      D_actor_450200_80138C60;
extern s32      D_actor_450200_80138E88;
extern s32      D_actor_450200_80139098;
extern TaskDesc D_actor_450200_80137A60;
extern Task*    D_actor_450200_801401E0;

void func_actor_450200_8013217C(s32 arg0)
{
    if (D_actor_450200_801401E0 != NULL) {
        D_actor_450200_801401E0->spawnArg1 = arg0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200_2", func_actor_450200_8013219C);

void func_actor_450200_80132220(void)
{
    switch (GameFlag_GetNibble(0x101)) {
        case 0:
            func_800E8614((s32)&D_actor_450200_80138870, 0);
            GameFlag_SetNibble(0x101, 1);
            break;
        case 1:
            func_800E8614((s32)&D_actor_450200_80138A68, 0);
            GameFlag_SetNibble(0x101, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_450200_80138C60, 0);
            GameFlag_SetNibble(0x101, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_450200_80138E88, 0);
            break;
    }
}

void func_actor_450200_801322F8(void)
{
    if (GameFlag_GetNibble(0xD7) != 0) {
        func_800E8614((s32)&D_actor_450200_80139098, 1);
    } else {
        func_8017FA98(0);
    }
    if (gameGetPtrSlot(0xA) != NULL) {
        D_actor_450200_801401E0 = Task_SpawnFromTable(&D_actor_450200_80137A60, 2, 0, 0);
    }
}

void func_actor_450200_80132368(s32 x, s32 tpageX, s32 clutY, s32 semiTrans, s32 rgb, s32 shadeTex)
{
    SPRT*    p;
    DR_MODE* dr;
    s32      i;

    for (i = 0; i < 2; i++) {
        p              = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
        setSprt(p);
        setShadeTex(p, shadeTex);
        setSemiTrans(p, semiTrans);
        p->x0   = x - 0xA0;
        p->y0   = -0x78;
        p->w    = 0x100;
        p->u0   = 0;
        p->v0   = 0;
        p->h    = 0xF0;
        p->r0   = rgb;
        p->g0   = rgb;
        p->b0   = rgb;
        p->clut = GetClut(0, clutY);
        addPrim(&gGpuCurrentOt[0x3FE], p);

        dr             = (DR_MODE*)gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 0, 1, getTPage(1, 1, tpageX, 0x100));
        addPrim(&gGpuCurrentOt[0x3FE], dr);

        tpageX += 0x80;
        x      += 0x100;
    }
}

void func_actor_450200_80132538(Task* task)
{
    RECT    rect0;
    RECT    rect1;
    RECT*   rp;
    RECT*   ap;
    s32     i;
    s32     j;
    u16*    src;
    u16*    dst;
    u_long* buf;
    s32     scale;
    s32     state;
    s32     level;
    u32     r;
    u32     g;
    u32     b;
    u32     col;

    if (gGameSession->at4.loc.view == 8) {
        taskKill(task);
        return;
    }

    state = task->state;
    switch (state) {
        case 0:
            task->killCountdown = 0x80;
            task->state        += 1;
            setRECT(&rect0, 0, 0xF7, 0x100, 1);
            StoreImage(&rect0, (u_long*)D_actor_450200_801401E8);
            rect0.y = 0xF8;
            StoreImage(&rect0, (u_long*)D_actor_450200_801403E8);
            break;

        case 1:
            if (task->killCountdown >= 0) {
                func_actor_450200_80132368(-0x40, 0x1C0, 0xFA, 1, 0x80, state);
                func_actor_450200_80132368(0, 0x140, 0xF9, 0, 0x80, state);

                rp    = &rect1;
                src   = D_actor_450200_801401E8;
                dst   = D_actor_450200_801405E8;
                scale = task->killCountdown;
                for (i = 0; i < 0x100; i++) {
                    r      = ((src[i] >> 10) & 0x1F) * scale;
                    g      = ((src[i] >> 5) & 0x1F) * scale;
                    b      = (*(u8*)&src[i] & 0x1F) * scale;
                    col    = r >> 7;
                    g    >>= 7;
                    col   &= 0xFF;
                    col  <<= 10;
                    col   |= ~0x7FFF;
                    g     &= 0xFF;
                    g    <<= 5;
                    col   |= g;
                    r      = b >> 7;
                    r     &= 0xFF;
                    r     |= col;
                    dst[i] = r;
                }
                rect1.x = 0;
                rect1.y = 0xF9;
                rp->w   = 0x100;
                rp->h   = 1;
                ap      = &rect1;
                LoadImage(ap, (u_long*)dst);
                SOFT_DEF_REG(ap);
                rp = &rect1;

                src   = D_actor_450200_801403E8;
                dst   = D_actor_450200_801407E8;
                scale = 0x80 - task->killCountdown;
                buf   = (u_long*)dst;
                for (j = 0; j < 0x100; j++) {
                    r      = ((src[j] >> 10) & 0x1F) * scale;
                    g      = ((src[j] >> 5) & 0x1F) * scale;
                    b      = (*(u8*)&src[j] & 0x1F) * scale;
                    col    = r >> 7;
                    g    >>= 7;
                    col   &= 0xFF;
                    col  <<= 10;
                    col   |= ~0x7FFF;
                    g     &= 0xFF;
                    g    <<= 5;
                    col   |= g;
                    r      = b >> 7;
                    r     &= 0xFF;
                    r     |= col;
                    dst[j] = r;
                }
                rect1.x = 0;
                rect1.y = 0xFA;
                rp->w   = 0x100;
                rp->h   = 1;
                LoadImage(&rect1, buf);
            } else {
                func_actor_450200_80132368(-0x40, 0x1C0, 0xFA, 0, 0x80, state);
            }

            level = 0xA0 - (u16)task->killCountdown;
            if ((u32)(level & 0xFFFF) >= 0xA0U) {
                level = 0xA0;
            }
            func_80180DAC(level & 0xFFFF);
            task->killCountdown = (u16)task->killCountdown - 4;
            break;
    }
}

void func_actor_450200_80132848(s32 arg0)
{
    if (arg0 == 1) {
        Task_SpawnFromTable(&D_actor_450200_8013FB40, 0, 0, 0);
    }
}

void func_actor_450200_80132880(s32 arg0)
{
    func_80180DAC(arg0 & 0xFFFF);
}

void func_actor_450200_801328A0(u8 arg0)
{
    gGameSession->at4.loc.room = arg0;
    D_8007216D                 = arg0;
}
