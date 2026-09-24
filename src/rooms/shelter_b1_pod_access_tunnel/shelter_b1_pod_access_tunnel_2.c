#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/task.h"
#include "main/display.h"
#include "rooms/shelter_b1_pod_access_tunnel.h"

extern u8             D_80071075;
extern s8             D_8007218A;
extern s8             D_8007272D;
extern u8             D_80073BA9;
extern s8             D_80114C12;
extern u8             D_shelter_b1_pod_access_tunnel_80181120[];
extern TaskFuncTable3 D_shelter_b1_pod_access_tunnel_8017D5D8;

/// Runs the room task through its state table, copied onto the stack first and
/// indexed by the task's state.
void func_shelter_b1_pod_access_tunnel_8017DEE8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_pod_access_tunnel_8017D5D8;
    sp.funcs[task->state](task);
}

/// Two-state task: state 0, unless blocked by `D_80114C12` or `D_80071075`,
/// sends the slot-3 task a `GpRec14` built from `D_80073BA9` (msg 0x3E8) and runs
/// `D_shelter_b1_pod_access_tunnel_80181120` through `func_800E8614`; state 1
/// sets `D_8007272D` to 0x1D and kills this task once the session is idle.
void func_shelter_b1_pod_access_tunnel_8017DF40(Task* task)
{
    GpRec14 rec;
    s32     state;
    s32     weaponId;
    s32     id;

    state = task->state;
    switch (state) {
        case 0:
            if (D_80114C12 != 1 && D_80071075 == 0) {
                weaponId     = D_80073BA9;
                id           = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                rec.field_0  = id;
                rec.field_4  = 1;
                rec.field_8  = 0;
                rec.field_C  = 0;
                rec.field_10 = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&rec, 0);
                func_800E8614((s32)D_shelter_b1_pod_access_tunnel_80181120, 0);
                task->state = task->state + 1;
            }
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                D_8007272D = 0x1D;
                Task_RequestKill(task, 0);
            }
            break;
    }
}

/// Draw state of the vertical image scroll. After a delay the seam moves down
/// the screen: the image above it slides in from the top, bottom rows first,
/// while the one below is pushed off the bottom. Each image is drawn as two
/// sprites spanning the screen width. Kills the task once `viewReady` is set
/// or no event is running.
void func_shelter_b1_pod_access_tunnel_8017E048(Task* task)
{
    ShelterB1PodAccessTunnelWork* work;
    SPRT*                         p;
    s32                           y;

    work = task->work;
    if (gGameSession->viewReady != 0 || gGameSession->eventState == 0) {
        taskKill(task);
        return;
    }
    y = 0;
    if (work->timer++ >= 0x2E) {
        work->offset += work->speed;
        y             = work->offset >> 16;
        if (y > 0xF0) {
            y = 0xF0;
        }
    }

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(p + 1);
    setSprt(p);
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = 0x100;
    setRGB0(p, 0x80, 0x80, 0x80);
    p->u0   = 0;
    p->v0   = -0x11 - y;
    p->clut = 0x3FC0;
    p->h    = y + 1;
    addPrim(gGpuCurrentOt + 1023, p);
    func_shelter_b1_pod_access_tunnel_8017E66C(0x340, 0);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(p + 1);
    setSprt(p);
    p->x0 = 0x60;
    p->y0 = -0x78;
    p->w  = 0x40;
    setRGB0(p, 0x80, 0x80, 0x80);
    p->u0   = 0;
    p->v0   = -0x11 - y;
    p->clut = 0x3FC0;
    p->h    = y + 1;
    addPrim(gGpuCurrentOt + 1023, p);
    func_shelter_b1_pod_access_tunnel_8017E66C(0x3C0, 0);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(p + 1);
    setSprt(p);
    p->x0 = -0xA0;
    p->w  = 0x100;
    setRGB0(p, 0x80, 0x80, 0x80);
    p->u0   = 0;
    p->v0   = 0;
    p->y0   = y - 0x78;
    p->clut = 0x4000;
    p->h    = 0xF0 - y;
    addPrim(gGpuCurrentOt + 1023, p);
    func_shelter_b1_pod_access_tunnel_8017E66C(0x240, 0x100);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(p + 1);
    setSprt(p);
    p->x0 = 0x60;
    p->w  = 0x40;
    setRGB0(p, 0x80, 0x80, 0x80);
    p->u0   = 0;
    p->v0   = 0;
    p->y0   = y - 0x78;
    p->clut = 0x4000;
    p->h    = 0xF0 - y;
    addPrim(gGpuCurrentOt + 1023, p);
    func_shelter_b1_pod_access_tunnel_8017E66C(0x2C0, 0x100);
}
