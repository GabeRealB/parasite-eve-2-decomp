#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/rand.h>

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

extern s32        D_8007107C;
extern u8         D_80071090;
extern u8         D_80115598;
extern GpMsgEntry D_neo_ark_island_80181B48[];
extern s16        D_80071076;

/// Staging save location the island commits: `field_2` / `field_4` / `field_1`
/// hold what `func_neo_ark_island_8017E968` copies out of the incoming
/// location, and `func_neo_ark_island_8017E844` moves those same three bytes
/// into `Mc_SaveData.at4.loc.area` / `warp` / `room`.
extern GpSaveLoc D_neo_ark_island_80184008;

extern TaskDesc D_neo_ark_island_80181B78;

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

void func_neo_ark_island_8017EA94(Task* arg0);
void func_neo_ark_island_8017EB08(Task* task);

/// Rotates `v` in place by `m` through the GTE, working from a stack copy so
/// the load and the store can name the same vector.
static inline void _neoArkIslandRotTrans(MATRIX* m, SVECTOR* v)
{
    SVECTOR tmp;

    tmp = *v;
    gte_SetRotMatrix(m);
    gte_ldv0(&tmp);
    gte_rtv0();
    gte_stsv(v);
}

/// Water-refraction ripple over part of the screen. Only some views of areas
/// 27, 14, 15, 13, 30 and 29 have one; each picks a row range, a split row
/// with the x at which a row's strip is cut in two, a clip mode, a wave scale
/// and an ordering-table offset, and every other view returns at once. Each
/// row's camera-space vector goes through the transposed view rotation to
/// give its ordering-table depth, and one or two `POLY_FT4` strips per row
/// sample the other display buffer, displaced vertically by a `rsin` / `rcos`
/// wave that fades in over the first 8 rows of the range and of the split.
/// The phases live in `Task::killCountdown`: seeded from `rand()` on the first
/// call, advanced by 0x20 per call while `Gp_StateF0.field_4` is clear, and, for views
/// 6 and 7 of area 13, by 0x20 while it is set.
///
/// Matching note: `wave = w` is written in both arms of the scale test and the
/// pass-1 fade starts from `v = 0x79`. jump2 merges the two copies and deletes
/// the constant set, but both change register allocation and scheduling the
/// way the retail code needs.
void func_neo_ark_island_8017D650(Task* task)
{
    s32                   buf;
    s32                   sinArg;
    s32                   cosArg;
    s32                   kind;
    s32                   scale;
    s32                   zoff;
    s32                   split;
    s32                   splitX;
    s32                   otzOff;
    s32                   xLeft0;
    s32                   xRight0;
    s32                   xLeftS;
    s32                   start;
    s32                   end;
    s32                   area;
    POLY_FT4*             prim;
    OverlayRippleScratch* scratch;
    s32                   y;
    s32                   y0;
    s32                   xl;
    s32                   xr;
    s32                   passes;
    s32                   pass;
    s32                   wave;
    s32                   sinv;
    s32                   cosv;
    s32                   w;
    s32                   d;
    s32                   z;
    s32                   otz;
    s32                   v;
    s32                   dy;
    s32                   xv;
    s32                   x;
    s32                   xe;
    s32                   xMin;
    s32                   xMax;
    s32                   fadeLen;
    s32                   one;
    DisplayState*         disp;

    kind    = 0;
    scale   = 0x1000;
    zoff    = 0x21C;
    otzOff  = 0;
    xLeft0  = -0xA0;
    xRight0 = 0xA0;
    xLeftS  = -0xA0;
    split   = 0;
    splitX  = 0;
    buf     = D_8007107C;
    area    = gGameSession->at4.loc.area;
    if (area == 27) {
        otzOff = 10;
        switch (gGameSession->at4.loc.view) {
            case 2:
                start  = 0x7F;
                end    = 0xF0;
                split  = 0x9F;
                splitX = 0x23;
                break;
            case 3:
                start  = 0x4A;
                end    = 0xF0;
                split  = 0x68;
                splitX = 0x55;
                break;
            case 4:
                start  = 1;
                end    = 0xF0;
                split  = 0x74;
                splitX = -0xD3;
                break;
            case 5:
                start  = 0x66;
                end    = 0xF0;
                split  = 0x6B;
                splitX = 0xC2;
                break;
            case 6:
                start  = 0x93;
                end    = 0xF0;
                split  = 0xA1;
                splitX = 0xBC;
                break;
            default:
                return;
        }
    } else if (area == 14) {
        switch (gGameSession->at4.loc.view) {
            case 2:
                start  = 0x77;
                end    = 0xF0;
                split  = 0xA1;
                splitX = -0x2C;
                break;
            case 3:
                start  = 0x4C;
                end    = 0xF0;
                split  = 0x68;
                splitX = -0x4E;
                break;
            case 4:
                start  = 1;
                end    = 0xF0;
                split  = 0x3E8;
                kind   = 3;
                otzOff = 10;
                scale  = 0x800;
                break;
            default:
                return;
        }
    } else if (area == 15) {
        if (gGameSession->at4.loc.view == 2) {
            split  = 0x3E8;
            start  = 0x84;
            end    = 0xF0;
            splitX = 0x4B;
            scale  = 0x800;
        } else {
            return;
        }
    } else if (area == 13) {
        otzOff = 10;
        switch (gGameSession->at4.loc.view) {
            case 2:
            case 4:
                start = 0x52;
                end   = 0xF0;
                kind  = 1;
                split = 0x3E8;
                scale = 0x800;
                break;
            case 3:
            case 5:
                start = 0x4C;
                end   = 0xF0;
                kind  = 2;
                split = 0x3E8;
                scale = 0x800;
                break;
            case 6:
                start = 0x63;
                end   = 0xF0;
                split = 0;
                scale = 0x800;
                if (Gp_StateF0.field_4 != 0) {
                    task->killCountdown += 0x20;
                }
                break;
            case 7:
                start = 0x35;
                end   = 0xF0;
                kind  = 4;
                scale = 0x800;
                if (Gp_StateF0.field_4 != 0) {
                    task->killCountdown += 0x20;
                }
                break;
            default:
                return;
        }
    } else if (area == 30) {
        scale  = 0x800;
        otzOff = -10;
        zoff   = 0x131A;
        switch (gGameSession->at4.loc.view) {
            case 2:
                xLeft0 = 0x3B;
                start  = 0xA9;
                end    = 0xE0;
                split  = -0xC7;
                splitX = -0x43;
                break;
            case 3:
                otzOff  = 10;
                xLeft0  = -0x4F;
                xRight0 = 0x4F;
                xLeftS  = -0x3A;
                start   = 1;
                end     = 0x5E;
                split   = -0x49;
                splitX  = 0x78;
                break;
            case 4:
                xRight0 = -0x3B;
                start   = 0xA9;
                end     = 0xE0;
                split   = -0xC7;
                splitX  = 0x43;
                break;
            case 5:
                start  = 0xA1;
                end    = 0xF0;
                xLeftS = -0x8A;
                splitX = 0x114;
                split  = 0xAC;
                otzOff = 0;
                break;
            default:
                return;
        }
    } else if (area == 29) {
        zoff = 0x8C;
        switch (gGameSession->at4.loc.view) {
            case 6:
                start = 0xA5;
                end   = 0xF0;
                split = 0;
                break;
            case 7:
                start = 0xA4;
                end   = 0xF0;
                split = 0;
                break;
            default:
                return;
        }
    } else {
        return;
    }

    xMin    = -0xA0;
    xMax    = 0xA0;
    fadeLen = 8;
    one     = 1;
    if (task->state == 0) {
        gGameSession->field_80 = 0;
        task->killCountdown    = rand();
        task->state++;
    }
    prim = (POLY_FT4*)D_8005C374;
    disp = &gDisplayState;
    if (disp->otBuffer != 0) {
        prim += 488;
    }
    prim--;
    if (Gp_StateF0.field_4 == 0) {
        task->killCountdown += 0x20;
    }
    sinArg = task->killCountdown * 2;
    cosArg = task->killCountdown;
    SCRATCH_PUSH(OverlayRippleScratch);
    scratch = SCRATCH_HEAD(OverlayRippleScratch);
    TransposeMatrix(&gGfxViewCoord.workm, &scratch->mtx);
    scratch->origin.vx = gGfxViewCoord.workm.t[0];
    scratch->origin.vy = gGfxViewCoord.workm.t[1];
    scratch->origin.vz = gGfxViewCoord.workm.t[2];
    _neoArkIslandRotTrans(&scratch->mtx, &scratch->origin);
    scratch->depth  = scratch->origin.vy + zoff;
    scratch->depth *= disp->screenDistance;
    scratch->row.vx = 0;
    scratch->row.vz = disp->screenDistance;
    gte_SetRotMatrix(&scratch->mtx);

    for (y = start; y < end; y++) {
        y0              = y - 0x78;
        scratch->row.vy = y0;
        gte_ldv0(&scratch->row);
        gte_rtv0();
        xl     = xLeft0;
        xr     = xRight0;
        passes = 1;
        if (split > 0) {
            if (y < split + 8) {
                xr = xMax;
                if (splitX > 0) {
                    xl = xLeftS;
                    xr = xl + splitX;
                } else {
                    xl = xr + splitX;
                }
                if (split < y) {
                    passes = 2;
                }
            }
        } else if (split < 0 && -split < y) {
            xr = xMax;
            if (splitX > 0) {
                xl = xLeftS;
                xr = xl + splitX;
            } else {
                xl = xr + splitX;
            }
        }
        sinv  = rsin(sinArg);
        cosv  = rcos(cosArg + 0x134);
        sinv += 0x2000;
        w     = cosv + sinv;
        w   >>= 9;
        if (scale != 0x1000) {
            w    = (w * scale) >> 12;
            wave = w;
        } else {
            wave = w;
        }
        w++;
        if (start != 1) {
            d = y - start;
            if (d < fadeLen) {
                w  = wave >> ((fadeLen - d) >> one);
                w += one;
            }
        }
        gte_stsv(&scratch->rowView);
        if (scratch->rowView.vy > 0) {
            otz   = scratch->depth / scratch->rowView.vy;
            otz >>= 2;
        } else {
            otz = 0x3FFF;
        }
        v    = (y0 + 0x78) + w;
        z    = otz;
        otz  = ((z << D_80071090) & 0x3FFF) >> 4;
        otz += otzOff;
        if (v >= 0xEF) {
            v = 0x1DC - v;
        }
        if (kind == 1) {
            if (y < 0x7D) {
                xl = -0xA0;
                xr = 0xA0;
            } else if (y < 0xB3) {
                passes = 2;
                xl     = -0xA0;
                xr     = -0x59;
            } else {
                xl = -0xA0;
                xr = -0x59;
            }
        } else if (kind == 2) {
            if (y < 0x83) {
                xl = -0xA0;
                xr = 0xA0;
            } else if (y < 0xB7) {
                passes = 2;
                xl     = 0x57;
                xr     = 0xA0;
            } else {
                xl = 0x57;
                xr = 0xA0;
            }
        } else if (kind == 3) {
            if (y < 0x43) {
                passes = 1;
                xl     = -0xA0;
                xr     = 0xA0;
            } else {
                passes = 2;
            }
        } else if (kind == 4) {
            passes = 1;
            xr     = 0xA0;
            xl     = -9;
            if (y >= 0x42) {
                xl = -0xA0;
                if (y < 0x4D) {
                    xl = -0x6A;
                }
            }
        }
        for (pass = 0; pass < passes; pass++) {
            dy = y - split;
            if (kind == 1) {
                if (pass != 0) {
                    xl = 0x3C;
                    xr = 0xA0;
                }
            } else if (kind == 2) {
                if (pass == 1) {
                    xl = -0xA0;
                    xr = -0x69;
                }
            } else if (kind == 3) {
                if (pass == 0) {
                    if (y < 0x43) {
                        xl = -0xA0;
                        xr = 0xA0;
                    } else {
                        xl = -0xA0;
                        xr = -0x57;
                    }
                } else {
                    if (y < 0xC1) {
                        xl = 0x5D;
                        xr = 0xA0;
                    } else {
                        xl = 0x2A;
                        xr = 0xA0;
                    }
                }
            } else if (pass == 1) {
                if (dy < fadeLen) {
                    w = wave >> ((fadeLen - dy) >> 1);
                    v = 0x79;
                    v = y0 + (v + w);
                    if (v >= 0xEF) {
                        v = 0x1DC - v;
                    }
                }
                if (splitX > 0) {
                    xv = splitX - 0x140;
                } else {
                    xv = splitX + 0x140;
                }
                xl = xMin;
                if (xv > 0) {
                    xr = xv + xl;
                } else {
                    xr = xMax;
                    xl = xv + xr;
                }
            }
            if (xr > 0) {
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y0 + 1;
                prim->y2    = y0 + 1;
                prim->tpage = getTPage(2, 0, 0x80, buf << 8);
                x           = xl;
                if (xl < 0) {
                    x = 0;
                }
                prim->x2 = x;
                prim->x0 = x;
                prim->u2 = x + 0x20;
                prim->u0 = x + 0x20;
                prim->x3 = xr;
                prim->x1 = xr;
                prim->u3 = xr + 0x20;
                prim->u1 = xr + 0x20;
                prim->v1 = v + buf * 16;
                prim->v0 = v + buf * 16;
                prim->v3 = v + buf * 16 + 1;
                prim->v2 = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
            if (xl <= 0) {
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y0 + 1;
                prim->y2    = y0 + 1;
                prim->tpage = getTPage(2, 0, 0, buf << 8);
                xe          = xr;
                if (xr > 0) {
                    xe = 0;
                }
                prim->u2 = xl - 0x60;
                prim->u0 = xl - 0x60;
                prim->u3 = (xl - 0x60) + (xe - xl);
                prim->u1 = (xl - 0x60) + (xe - xl);
                prim->x2 = xl;
                prim->x0 = xl;
                prim->x3 = xe;
                prim->x1 = xe;
                prim->v1 = v + buf * 16;
                prim->v0 = v + buf * 16;
                prim->v3 = v + buf * 16 + 1;
                prim->v2 = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
        }
        sinArg += 0x1F;
        if (z > 0x300) {
            cosArg += 0xC5 + (z - 0x300) / 4;
        } else {
            cosArg += 0xC5;
        }
    }
    SCRATCH_POP(OverlayRippleScratch);
}

/// Wavy screen-distortion band for some views of areas 12 and 30; every other
/// view returns at once. Each screen row between the view's start and end
/// rows gets one or two raw-textured `POLY_FT4` strips sampling the other
/// display buffer, displaced vertically by a `rsin` / `rcos` wave and faded
/// out over the band's last 16 rows, each row linked one depth nearer into
/// `gGpuCurrentOt`. One view of area 30 draws a second band. The phases live
/// in `Task::killCountdown`, seeded from `rand()` on the first call and
/// advanced every call while `Gp_StateF0.field_4` is clear.
///
/// Matching note: `spare` is never assigned, so `spare >> 16` is always zero;
/// it stands in for the stack slot the retail frame carries, which the
/// register allocator needs to see.
void func_neo_ark_island_8017E2A4(Task* task)
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

    if (Gp_StateF0.field_4 == 0) {
        task->killCountdown++;
    }
    sinArg = task->killCountdown << 5;
    cosArg = task->killCountdown << 4;
    SCRATCH_PUSH_BYTES(0x40);
    otz = ((0x3FFF << D_80071090) & 0x3FFF) >> 4;

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
    SCRATCH_POP_BYTES(0x40);
}

/// State handlers of the room's entry task, indexed by its state through
/// `func_neo_ark_island_8017EB10`: set-up, idle, then kill.
const TaskFuncTable3 D_neo_ark_island_8017D614 = {
    { func_neo_ark_island_8017EA94, func_neo_ark_island_8017EB08, taskKill }
};

/// Island arrival sequence, advanced one step per call: step 0 asks for the
/// caption, step 1 waits for the CAP system to go idle, step 2 clears the mode
/// flag and waits for the event key it answers with - anything but 0xA kills
/// the task and messages the player weapon - step 3 is the shared advance, and
/// step 4 raises the outgoing sound, commits the staged save location to
/// `Mc_SaveData` and spawns the task's successor.
void func_neo_ark_island_8017E844(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_SpawnIfCapIdle(1, 0);
            goto L_advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
            goto L_advance;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            Gp_StateF0.field_4 = 0;
            goto L_advance;
        case 3:
        L_advance:
            arg0->state++;
            return;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_neo_ark_island_80184008.field_2;
            Mc_SaveData.at4.loc.warp = D_neo_ark_island_80184008.field_4;
            Mc_SaveData.at4.loc.room = D_neo_ark_island_80184008.field_1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_neo_ark_island_8017E960(void)
{
    return 0;
}

/// Island message handler. Message 0x1E, while the incoming location still
/// reports no pending flag, latches the save location the outgoing message
/// carries and starts the cutscene that leads to the island's arrival. Returns
/// 1 for every other message and for a location that is already latched.
s32 func_neo_ark_island_8017E968(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179B14(src, dst);
    if (*(u16*)src == 0x1E) {
        if (src->field_5 == 0) {
            D_neo_ark_island_80184008.field_2 = dst->field_0;
            D_neo_ark_island_80184008.field_4 = dst->field_2;
            D_neo_ark_island_80184008.field_1 = dst->field_3;
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_neo_ark_island_80181B78, 0, 0, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_neo_ark_island_8017EA24(void)
{
    return 0;
}

s32 func_neo_ark_island_8017EA2C(void)
{
    return 0;
}

/// Maps a cap (cutscene) script event key to the island cue it should play:
/// key 3 plays `0x550E0003` outright, key 0x65 plays `0x550E0004` only while
/// the running cap script reports no event key. Every other key, and key 0x65
/// with a script still parked on one, is ignored. Always returns 0.
s32 func_neo_ark_island_8017EA34(s32 arg0, s32 arg1, s32 arg2)
{
    s32 id;

    switch (arg2) {
        case 3:
            id = 0x550E0003;
            goto play;
        case 0x65:
            if (Gp_GetCapEventKey() != 0) {
                break;
            }
            id = 0x550E0004;
        play:
            SndEvt_EnqueueType6(id, 0, 0);
            break;
    }
    return 0;
}

/// Room entry task tick in the family that announces the island's arrival:
/// installs the room's message table, hands the task to pointer slot 7, plays
/// the two island cues, then advances state and raises the `D_80115598` flag.
void func_neo_ark_island_8017EA94(Task* arg0)
{
    arg0->msgTable = D_neo_ark_island_80181B48;
    Game_SetPtrSlot(arg0, 7);
    SndEvt_EnqueueType6(0x550E0005, 0, 0);
    SndEvt_EnqueueType6(0x550E0006, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

void func_neo_ark_island_8017EB08(Task* task)
{
}

/// Task tick that dispatches on the task's state through the three-entry
/// handler table `D_neo_ark_island_8017D614`, copied to the stack first.
void func_neo_ark_island_8017EB10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_island_8017D614;
    sp.funcs[task->state](task);
}
