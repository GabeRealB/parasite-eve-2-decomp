#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"

#include <psyq/libgpu.h>
#include <psyq/rand.h>

s32 rcos(s32);
s32 rsin(s32);

extern s32 D_8007107C;
extern u8  D_80071090;
extern u8  D_801153F4;

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path", func_neo_ark_woodland_path_8017D694);

/// Draws a rippling screen-distortion band for certain views of areas 12 and 30
/// (other views return at once): one semi-transparent textured quad per screen
/// row between the view's start and end rows, sampling the other display buffer with a vertical
/// offset from a `rsin` / `rcos` wave (faded over the last 16 rows) and linked
/// into `gGpuCurrentOt` at a depth that decreases row by row. The phases advance
/// from `Task::killCountdown`, which is seeded from `rand()` on the first call.
///
/// Matching note: `spare` is never assigned and `spare >> 16` is always zero.
/// It exists only in the register allocator's view, as the stack slot the
/// retail frame carries (the same construct as `func_actor_361100_80161FF8`).
void func_neo_ark_woodland_path_8017E2E8(Task* task)
{
    s32        xLeft  = -0xA0;
    s32        xRight = 0xA0;
    s32        buf    = D_8007107C;
    s32        passes = 1;
    GpAreaKey* loc    = &gGameSession->at4.loc;
    s32        area   = loc->area;
    s32        start;
    s32        end;
    POLY_FT4*  prim;
    u8*        base;
    s32        size;
    s32        sinArg;
    s32        cosArg;
    s32        otz;
    s32        pass;
    s32        y;
    s32        y0;
    s32        wave;
    s32        sinv;
    s32        cosv;
    s32        v;
    s32        x0;
    s32        x1;
    u16        spare;

    if (area == 12) {
        switch (gGameSession->at4.loc.view) {
            case 2:
                start = 1;
                end   = 0x3F;
                break;
            case 3:
                start = 1;
                end   = 0x49;
                break;
            case 4:
                start = 1;
                end   = 0x49;
                break;
            case 5:
                start = 1;
                end   = 0x3F;
                break;
            case 8:
                start = 1;
                end   = 0x72;
                break;
            case 9:
                start = 1;
                end   = 0x45;
                break;
            default:
                return;
        }
    } else if (area == 30) {
        switch (gGameSession->at4.loc.view) {
            case 2:
                start  = 1;
                end    = 0x40;
                xRight = -0x50;
                break;
            case 4:
                start  = 0x3B;
                end    = 0x55;
                xRight = -0x67;
                break;
            case 5:
                start  = 0x22;
                end    = 0x4A;
                xRight = -0x4E;
                passes = 2;
                break;
            default:
                return;
        }
    } else {
        return;
    }

    if (task->state == 0) {
        gGameSession->field_80 = 0;
        task->killCountdown    = rand();
        task->state++;
    }

    if (area == 12 && loc->place == 3) {
        size  = 0x30000 - D_8006D860;
        size &= ~7;
        base  = (u8*)D_8005C36C - (size - 0x30000);
        if (size < sizeof(POLY_FT4) * 976) {
            return;
        }
        if (D_8007107C != 0) {
            base += size >> 1;
        }
        prim = (POLY_FT4*)base - 1;
    } else {
        prim = (POLY_FT4*)((u8*)D_8005C374 + 0x9880);
        if (D_8007107C != 0) {
            prim += 488;
        }
        prim--;
    }

    if (D_801153F4 == 0) {
        task->killCountdown++;
    }
    sinArg             = task->killCountdown << 5;
    cosArg             = task->killCountdown << 4;
    *(s32*)0x1F8003FC -= 0x40;
    otz                = ((0x3FFF << D_80071090) & 0x3FFF) >> 4;

    for (pass = 0; pass < passes; pass++) {
        if (pass == 1) {
            start  = 0x2E;
            end    = 0x54;
            xLeft  = 1;
            xRight = 0x55;
        }
        for (y = start; y < end; y++) {
            y0     = y - 0x78;
            sinv   = rsin(sinArg);
            cosv   = rcos(cosArg + 0x134);
            sinv  += 0x2000;
            wave   = cosv + sinv;
            wave >>= 10;
            if (end - 0x10 < y) {
                wave = (wave * (end - y)) >> 4;
            }
            cosv = wave + 0x79;
            v    = y0 + cosv;
            otz--;
            if (v >= 0xEF) {
                v = 0x1DC - v;
            }
            if (v < 0) {
                v = -v;
            }
            if (xRight > 0) {
                x0 = xLeft < 0 ? 0 : xLeft;
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y - 0x77;
                prim->y2    = y - 0x77;
                prim->tpage = getTPage(2, 0, 0x80, buf << 8);
                prim->x2    = x0;
                prim->x0    = x0;
                prim->u2    = x0 + 0x20;
                prim->u0    = x0 + 0x20;
                prim->x3    = xRight;
                prim->x1    = xRight;
                prim->u3    = xRight + 0x20;
                prim->u1    = xRight + 0x20;
                prim->v1    = v + buf * 16;
                prim->v0    = v + buf * 16;
                prim->v3    = v + buf * 16 + 1;
                prim->v2    = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
            if (xLeft < 0) {
                x1 = xRight;
                if (x1 > 0) {
                    x1 = 0;
                }
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y - 0x77;
                prim->y2    = y - 0x77;
                prim->tpage = getTPage(2, 0, 0, buf << 8);
                prim->u2    = xLeft - 0x60;
                prim->u0    = xLeft - 0x60;
                prim->u3    = x1 - 0x60;
                prim->u1    = x1 - 0x60;
                prim->x2    = xLeft;
                prim->x0    = xLeft;
                prim->x3    = x1;
                prim->x1    = x1;
                prim->v1    = v + buf * 16;
                prim->v0    = v + buf * 16;
                prim->v3    = v + buf * 16 + 1;
                prim->v2    = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
            sinArg += 0x1F + (spare >> 16);
            cosArg += 0xC5;
        }
    }
    *(s32*)0x1F8003FC += 0x40;
}

s32 func_neo_ark_woodland_path_8017E888(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path", RoomsShared8017d878Table);
