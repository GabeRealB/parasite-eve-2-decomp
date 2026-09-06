#include "common.h"
#include "main/display.h"
#include "main/task.h"

INCLUDE_ASM("aya/nonmatchings/aya_20900/aya_20900", func_aya_20900_8011578C);

void func_aya_20900_80115948(void)
{
    POLY_FT4* p;
    s16       x;
    s16       y;
    s16       w;
    s16       h;

    x              = -0x3C;
    p              = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setlen(p, 9);
    setcode(p, 0x2D);
    y = -0xC;
    w = 0x78;
    h = 0x18;
    setXYWH(p, x, y, w, h);
    setUVWH(p, 0, 0, w, h);
    p->clut  = 0x3FC0;
    p->tpage = 0x2F;
    addPrim(Gpu_CurrentOt, p);
}

INCLUDE_RODATA("aya/nonmatchings/aya_20900/aya_20900", D_aya_20900_80115770);

INCLUDE_ASM("aya/nonmatchings/aya_20900/aya_20900", func_aya_20900_80115A14);

s32 func_aya_20900_80115A14(Task*); /* extern */

void func_aya_20900_80115CFC(Task* arg0)
{
    TaskIdMap* temp_v0;
    s32        temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            temp_v0     = Mem_Malloc(8U, false);
            arg0->idMap = temp_v0;
            if (temp_v0 == NULL) {
                Task_Kill(arg0);
                return;
            }
            Mem_Set(temp_v0, 0U, 8U);
            arg0->state += 1;
        case 1:
            if ((func_aya_20900_80115A14(arg0) << 0x10) != 0) {
                Task_RequestKill(arg0, 0);
            }
            return;
    }
}
