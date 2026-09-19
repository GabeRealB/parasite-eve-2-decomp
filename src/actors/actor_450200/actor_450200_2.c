#include "common.h"

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
        p              = (SPRT*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
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

        dr             = (DR_MODE*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(dr + 1);
        setDrawTPage(dr, 0, 1, getTPage(1, 1, tpageX, 0x100));
        addPrim(&gGpuCurrentOt[0x3FE], dr);

        tpageX += 0x80;
        x      += 0x100;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_450200/actor_450200_2", func_actor_450200_80132538);

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
