#include "common.h"

#include "actors/actor_143000.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"

void func_actor_143000_80133CF0(Task* arg0)
{
    Actor143000CaptureArgs* p = arg0->spawnArg2;
    RECT                    r;
    RECT                    r2;
    s32                     n;
    s32                     offset;
    RECT*                   rp;
    s32                     bottom;
    Task*                   killTask = arg0;

    SOFT_TOUCH_REG(killTask);
    if (gGameSession->at4.loc.view == 0xE) {
        switch (arg0->state) {
            case 0:
                arg0->killCountdown = 6;
                p->count            = 0;
                arg0->state++;
                return;
            case 1:
                if (--arg0->killCountdown > 0) {
                    return;
                }
                arg0->killCountdown = 6;
                r.x                 = p->x;
                r.w                 = p->w;
                r.y                 = p->y + p->h * p->count / p->total;
                n                   = p->count + 1;
                rp                  = &r2;
                SOFT_TOUCH_REG_USE(rp, n);
                p->count = n;
                bottom   = p->y + p->h * n / p->total;
                offset   = r.y * 640;
                r.h      = bottom - r.y;
                SOFT_USE_REG(offset);
                r2    = r;
                r2.x  = 0x1C0;
                rp->w = 0x140;
                r2.y += 0x100;
                StoreImage(rp, (u32*)((u8*)Fs_ImgBuffers + offset));
                killTask = arg0;
                if (p->count >= p->total) {
                    goto kill;
                }
                break;
        }
    } else {
        goto kill;
    }
    return;
kill:
    taskKill(killTask);
}
