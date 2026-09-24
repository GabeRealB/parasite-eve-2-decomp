#include "common.h"

#include "main/display.h"
#include "main/task.h"
#include "rooms/mist_r18.h"

extern TaskFuncTable3 D_mist_r18_8017D5C4;

/// Redraw the room's two backdrop halves as semi-transparent `SPRT`s in OT
/// slot 8, tinting both with `shade`, then append each half's tpage.
void func_mist_r18_8017E144(s16 shade)
{
    SPRT* p;

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = -0xA0;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0xC0;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    func_mist_r18_8017E994(0x340, 0);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = 0x20;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0x80;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    func_mist_r18_8017E994(0x280, 0x100);
}

/// Per-frame entry point of the attached-model task: run the handler its state
/// selects from `D_mist_r18_8017D5C4` (attach to the parent's part, an empty
/// idle state, then `taskKill`), copied onto the stack each frame.
void func_mist_r18_8017E2C8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_r18_8017D5C4;
    sp.funcs[task->state](task);
}
