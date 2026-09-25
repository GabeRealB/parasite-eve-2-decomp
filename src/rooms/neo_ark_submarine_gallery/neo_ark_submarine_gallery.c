#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/rand.h>

#include "gameplay/1A8.h"
#include "main/display.h"
#include "gameplay/gameplay.h"
#include "gameplay/D4.h"
#include "gameplay/3FB8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/mc.h"
#include "main/sound.h"
#include "rooms/room.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

extern GpMsgEntry D_neo_ark_submarine_gallery_80181884[];
extern TaskDesc   D_neo_ark_submarine_gallery_801818BC[];
extern TaskDesc   D_neo_ark_submarine_gallery_801818AC;
extern s16        D_neo_ark_submarine_gallery_801818B8;

extern s16 D_80071076;
extern s32 D_8007107C;
extern u8  D_80071090;
extern s8  D_80115690;

/// 0x1E pair the gallery hands `Task_Spawn` for the helper it raises in state 3,
/// the same shape `D_mine_mesa_80189B38` has.
extern GpFadeWork D_neo_ark_submarine_gallery_8018591C;

/// Staging save location the gallery commits: `field_2` / `field_4` / `field_1`
/// hold what `func_neo_ark_submarine_gallery_8017EA0C` copies out of the
/// incoming location, and `func_neo_ark_submarine_gallery_8017E86C` moves those
/// same three bytes into `Mc_SaveData.at4.loc.area` / `field_8` / `field_5`.
extern GpSaveLoc D_neo_ark_submarine_gallery_80185924;

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// Rotates `v` in place by `m` through the GTE, working from a stack copy so
/// the load and the store can name the same vector.
static inline void _neoArkSubmarineGalleryRotTrans(MATRIX* m, SVECTOR* v)
{
    SVECTOR tmp;

    tmp = *v;
    gte_SetRotMatrix(m);
    gte_ldv0(&tmp);
    gte_rtv0();
    gte_stsv(v);
}

/// Water-refraction ripple over parts of the screen, drawn only in some views
/// of areas 27, 14, 15, 13, 30 and 29; every other view returns at once. The
/// view picks the row range, a split row with the x where each row's strip is
/// cut, a clip mode, a wave scale and an ordering-table offset. Each row goes
/// through the transposed view rotation to get its ordering-table depth, then
/// one or two `POLY_FT4` strips copy the other display buffer shifted
/// vertically by a `rsin` / `rcos` wave, faded in over the first 8 rows of the
/// range and of the split. The wave phases come from `Task::killCountdown`,
/// seeded with `rand()` on the first call and stepped by 0x20 per call while
/// `Gp_StateF0.field_4` is clear (views 6 and 7 of area 13 step it while it is set).
///
/// Matching note: `wave = w` is written in both arms of the scale test and the
/// pass-1 fade starts from `v = 0x79`; both shape the register allocation and
/// scheduling the retail code has.
void func_neo_ark_submarine_gallery_8017D678(Task* task)
{
    s32                buf;
    s32                sinArg;
    s32                cosArg;
    s32                kind;
    s32                scale;
    s32                zoff;
    s32                split;
    s32                splitX;
    s32                otzOff;
    s32                xLeft0;
    s32                xRight0;
    s32                xLeftS;
    s32                start;
    s32                end;
    s32                area;
    POLY_FT4*          prim;
    RoomRippleScratch* scratch;
    s32                y;
    s32                y0;
    s32                xl;
    s32                xr;
    s32                passes;
    s32                pass;
    s32                wave;
    s32                sinv;
    s32                cosv;
    s32                w;
    s32                d;
    s32                z;
    s32                otz;
    s32                v;
    s32                dy;
    s32                xv;
    s32                x;
    s32                xe;
    s32                xMin;
    s32                xMax;
    s32                fadeLen;
    s32                one;
    DisplayState*      disp;

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
    sinArg                               = task->killCountdown * 2;
    cosArg                               = task->killCountdown;
    *(RoomRippleScratch**)G_SCRATCH_HEAD = *(RoomRippleScratch**)G_SCRATCH_HEAD - 1;
    scratch                              = *(RoomRippleScratch**)G_SCRATCH_HEAD;
    TransposeMatrix(&gGfxViewCoord.workm, &scratch->mtx);
    scratch->origin.vx = gGfxViewCoord.workm.t[0];
    scratch->origin.vy = gGfxViewCoord.workm.t[1];
    scratch->origin.vz = gGfxViewCoord.workm.t[2];
    _neoArkSubmarineGalleryRotTrans(&scratch->mtx, &scratch->origin);
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
    *(RoomRippleScratch**)G_SCRATCH_HEAD += 1;
}

/// Wavy screen-distortion band, drawn only in some views of areas 12 and 30;
/// every other view returns at once. Each row between the view's start and end
/// rows gets one or two raw-textured `POLY_FT4` strips copying the other display
/// buffer shifted vertically by a `rsin` / `rcos` wave that fades out over the
/// band's last 16 rows, each row linked one ordering-table slot nearer. View 5
/// of area 30 draws a second band. The wave phases come from
/// `Task::killCountdown`, seeded with `rand()` on the first call and stepped
/// every call while `Gp_StateF0.field_4` is clear.
///
/// Matching note: `spare` is never assigned, so `spare >> 16` is always zero;
/// it stands in for the stack slot the retail frame carries, which the
/// register allocator needs to see.
void func_neo_ark_submarine_gallery_8017E2CC(Task* task)
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

void func_neo_ark_submarine_gallery_8017EB50(Task* arg0);
void func_neo_ark_submarine_gallery_8017EBC4(Task* arg0);

/// State handlers of the room's entry task, indexed by its state through
/// `func_neo_ark_submarine_gallery_8017EBCC`: set-up, idle, then kill.
const TaskFuncTable3 D_neo_ark_submarine_gallery_8017D614 = {
    { func_neo_ark_submarine_gallery_8017EB50, func_neo_ark_submarine_gallery_8017EBC4, taskKill }
};

/// Runs the gallery's save sequence once state 0 has asked for the caption.
/// State 1 waits for that caption, state 2 takes the confirm key or backs out,
/// state 3 raises the helper task 0x31 and queues the sound event, state 4
/// waits for that voice, and state 5 - the commit - copies the staged location
/// into `Mc_SaveData` and reloads. Every state advances by one except a
/// confirmed cancel and the commit itself.
void func_neo_ark_submarine_gallery_8017E86C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(9, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                Gp_StateF0.field_4 = 0;
                break;
            }
            Gp_StateF0.field_4 = 1;
            Gp_TriggerPeIfArmed();
            arg0->state++;
            break;
        case 3:
            D_neo_ark_submarine_gallery_8018591C.field_0 = 0;
            D_neo_ark_submarine_gallery_8018591C.field_1 = 0;
            D_neo_ark_submarine_gallery_8018591C.field_2 = 0x1E;
            Task_Spawn(1, 0x31, 0, (s32)&D_neo_ark_submarine_gallery_8018591C);
            SndEvt_EnqueueType6(0x551E0001, 0, 0);
            arg0->state++;
            break;
        case 4:
            if (SndVoice_HasActiveId(0x551E0001) == 0) {
                arg0->state++;
            }
            break;
        case 5:
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_neo_ark_submarine_gallery_80185924.field_2;
            Mc_SaveData.at4.loc.warp = D_neo_ark_submarine_gallery_80185924.field_4;
            Mc_SaveData.at4.loc.room = D_neo_ark_submarine_gallery_80185924.field_1;
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_neo_ark_submarine_gallery_8017EA04(void)
{
    return 0;
}

/// Gallery message handler. Message 0xE, while the incoming location still
/// reports no pending flag, latches the save location the outgoing message
/// carries and starts the cutscene the gallery leads out of. Returns 0 for that
/// message and 1 for every other one.
s32 func_neo_ark_submarine_gallery_8017EA0C(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179B14(src, dst);
    if (*(u16*)src == 0xE) {
        if (src->field_5 == 0) {
            D_neo_ark_submarine_gallery_80185924.field_2 = dst->field_0;
            D_neo_ark_submarine_gallery_80185924.field_4 = dst->field_2;
            D_neo_ark_submarine_gallery_80185924.field_1 = dst->field_3;
            Task_SpawnFromTable(&D_neo_ark_submarine_gallery_801818AC, 0, 0, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_neo_ark_submarine_gallery_8017EABC(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 2:
            if (D_neo_ark_submarine_gallery_801818B8 == 1) {
                Gp_SpawnIfCapIdle(2, 0);
            } else {
                Gp_SpawnIfCapIdle(4, 0);
            }
            break;
        case 3:
            if (gGameSession->at4.loc.place == 4) {
                Gp_SpawnIfCapIdle(5, 0);
            } else if (Gp_StateF0.field_0 == 1) {
                Gp_SpawnIfCapIdle(3, 0);
            } else {
                Gp_SpawnIfCapIdle(6, 0);
            }
            break;
    }
    return 0;
}

s32 func_neo_ark_submarine_gallery_8017EB48(void)
{
    return 0;
}

void func_neo_ark_submarine_gallery_8017EB50(Task* arg0)
{
    arg0->msgTable = D_neo_ark_submarine_gallery_80181884;
    Game_SetPtrSlot(arg0, 7);
    if (gGameSession->at4.loc.place == 4) {
        Task_SpawnFromTable(D_neo_ark_submarine_gallery_801818BC, 0, 0, 0);
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_neo_ark_submarine_gallery_8017EBC4(Task* arg0)
{
}

/// Entry task tick: dispatches on the task's state through
/// `D_neo_ark_submarine_gallery_8017D614`, copied to the stack first.
void func_neo_ark_submarine_gallery_8017EBCC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_submarine_gallery_8017D614;
    sp.funcs[task->state](task);
}

/// Sweeps a 32-wedge red disc of radius `arg0` through the view matrix: each
/// step projects the fan's centre and the two rim points 0x80 apart and, when
/// the projection passes, queues one semi-transparent `POLY_G3` plus its
/// drawing-mode packet (tpage 0x2A) into `gGpuCurrentOt[(otz >> 4) + 0x18]`.
/// The disc sits at view-space height 0x14B4.
s32 func_neo_ark_submarine_gallery_8017EC24(u16 arg0, s32 arg1)
{
    SVECTOR  p0;
    SVECTOR  p1;
    SVECTOR  p2;
    s32      sxy0;
    s32      sxy1;
    s32      sxy2;
    s32      p;
    s32      flag;
    POLY_G3* prim;
    DR_MODE* dr;
    s32      otz;
    s32      ang;
    s16      i;
    s16      y;

    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    y = 0x14B4;
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    for (i = 0; i < 0x20; i++) {
        p0.vx = 0;
        p0.vy = y;
        p0.vz = 0;
        ang   = (i << 16) >> 9;
        p1.vx = (rsin(ang) * arg0) >> 12;
        p1.vy = y;
        p1.vz = (rcos(ang) * arg0) >> 12;
        ang   = ang + 0x80;
        p2.vx = (rsin(ang) * arg0) >> 12;
        p2.vy = y;
        p2.vz = (rcos(ang) * arg0) >> 12;
        otz   = RotTransPers3(&p0, &p1, &p2, &sxy0, &sxy1, &sxy2, &p, &flag);
        if (flag >= 0) {
            prim           = (POLY_G3*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG3(prim);
            setRGB0(prim, 0xFF, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, 0, 0);
            setSemiTrans(prim, 1);
            *(s32*)&prim->x0 = sxy0;
            *(s32*)&prim->x1 = sxy1;
            *(s32*)&prim->x2 = sxy2;
            addPrim(&gGpuCurrentOt[(otz >> 4) + 0x18], prim);
            dr             = (DR_MODE*)gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setDrawTPage(dr, 0, 0, 0x2A);
            addPrim(&gGpuCurrentOt[(otz >> 4) + 0x18], dr);
        }
    }
}

void func_neo_ark_submarine_gallery_8017EED8(Task* arg0)
{
    if (gGameSession->at4.loc.place != 4) {
        arg0->killCountdown = 0;
    } else {
        arg0->killCountdown = 0x780;
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_neo_ark_submarine_gallery_8017EF14(Task* arg0)
{
    s32 mode;
    if (Gp_ActorSlots[0] != NULL) {
        mode = 4;
        if (gGameSession->at4.loc.place != mode && gGameSession->field_126 != 0) {
            gGameSession->at4.loc.place = mode;
        }
        if (arg0->killCountdown < 0x780) {
            arg0->killCountdown = (s16)((u16)arg0->killCountdown + 0x10);
        }
        func_neo_ark_submarine_gallery_8017EC24((u16)arg0->killCountdown, mode);
    }
}

void func_neo_ark_submarine_gallery_8017EF8C(Task* arg0)
{
}

/// State handlers of the disc task, indexed by its state through
/// `func_neo_ark_submarine_gallery_8017EF94`: set-up, the per-frame disc sweep,
/// then an idle state.
const TaskFuncTable3 D_neo_ark_submarine_gallery_8017D63C = {
    { func_neo_ark_submarine_gallery_8017EED8, func_neo_ark_submarine_gallery_8017EF14,
      func_neo_ark_submarine_gallery_8017EF8C }
};

/// Disc task tick: dispatches on the task's state through
/// `D_neo_ark_submarine_gallery_8017D63C`, copied to the stack first.
void func_neo_ark_submarine_gallery_8017EF94(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_submarine_gallery_8017D63C;
    sp.funcs[task->state](task);
}
