#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "actors/actors_shared_80149ed0.h"
#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// Scratchpad block the ripple borrows from `G_SCRATCH_HEAD` for one call:
/// the transposed view rotation, the camera-space row vector fed to the GTE
/// (`row`) and its rotated result (`rowView`), the rotated view translation
/// (`origin`) and the numerator of the per-row depth division (`depth`). The
/// block is 0x4C bytes; nothing reads the tail.
typedef struct {
    MATRIX  mtx;
    SVECTOR row;
    SVECTOR rowView;
    SVECTOR origin;
    s32     depth;
    u8      _pad[0x10];
} _NeoArkSubmarineTunnelRippleScratch;

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

extern s16 D_800691CA;
extern s32 D_8007107C;
extern u8  D_80071090;

/// The area-record id the event handler publishes, and the cutscene script
/// blobs `func_800E8634` / `func_800E8614` are handed as `(s32)&blob`.
extern s8  D_8007272D;
extern s32 D_80135220;
extern s32 D_80135FD0;
extern s32 D_80136108;

/// Spawn table of the screen-wave task, and the context it is spawned with.
/// The context's mode word is written through its own symbol, which is how the
/// original reached it.
extern TaskDesc     D_neo_ark_submarine_tunnel_80181A34;
extern ActorWaveCtx D_neo_ark_submarine_tunnel_80187A20;
extern s16          D_neo_ark_submarine_tunnel_80187A24;

/// Current displacement of the screen wave, recomputed every frame from the
/// context's ramp.
extern s32 D_neo_ark_submarine_tunnel_80181A4C;

/// The ramp and tint the wave task was spawned with.
extern ActorWaveCtx* D_neo_ark_submarine_tunnel_8018790C;

/// Phase records of the wave's 11 column edges and 30 row edges.
extern ActorWaveRec6 D_neo_ark_submarine_tunnel_80187910[11];
extern ActorWaveRec6 D_neo_ark_submarine_tunnel_80187960[30];

/// Message handlers this room's task answers, installed into pointer slot 7.
extern GpMsgEntry D_neo_ark_submarine_tunnel_80181A50[];

/// The tunnel's own script blob and the byte recording which of its scenes has
/// already been staged.
extern s32 D_neo_ark_submarine_tunnel_80181AF0;
extern u8  D_neo_ark_submarine_tunnel_80181DF0;

void func_neo_ark_submarine_tunnel_8017F3BC(Task* arg0);
void func_neo_ark_submarine_tunnel_8017F414(Task* task);

/// Rotates `v` in place by `m` through the GTE, working from a stack copy so
/// the load and the store can name the same vector.
static inline void _neoArkSubmarineTunnelRotTrans(MATRIX* m, SVECTOR* v)
{
    SVECTOR tmp;

    tmp = *v;
    gte_SetRotMatrix(m);
    gte_ldv0(&tmp);
    gte_rtv0();
    gte_stsv(v);
}

/// Draws a water-refraction ripple for some views of areas 27, 14, 15, 13, 30
/// and 29 and returns at once for every other view. The view sets the row
/// range, a split row and the x at which each row's strip is cut in two, a
/// clip mode, a wave scale and an ordering-table offset. Each row's vector is
/// rotated through the transposed view matrix to find its ordering-table
/// depth, and one or two `POLY_FT4` strips per row sample the other display
/// buffer shifted vertically by a `rsin` / `rcos` wave that fades in over the
/// first 8 rows of the range and of the split. The wave phases derive from
/// `Task::killCountdown`, seeded from `rand()` on the first call and advanced
/// by 0x20 per call while `Gp_StateF0.field_4` is clear; views 6 and 7 of area 13
/// advance them by 0x20 while it is set instead.
///
/// Matching note: `wave = w` is written in both arms of the scale test and the
/// pass-1 fade starts from `v = 0x79`. jump2 merges the two copies and deletes
/// the constant set, but both change register allocation and scheduling the
/// way the retail code needs.
void func_neo_ark_submarine_tunnel_8017D634(Task* task)
{
    s32                                  buf;
    s32                                  sinArg;
    s32                                  cosArg;
    s32                                  kind;
    s32                                  scale;
    s32                                  zoff;
    s32                                  split;
    s32                                  splitX;
    s32                                  otzOff;
    s32                                  xLeft0;
    s32                                  xRight0;
    s32                                  xLeftS;
    s32                                  start;
    s32                                  end;
    s32                                  area;
    POLY_FT4*                            prim;
    _NeoArkSubmarineTunnelRippleScratch* scratch;
    s32                                  y;
    s32                                  y0;
    s32                                  xl;
    s32                                  xr;
    s32                                  passes;
    s32                                  pass;
    s32                                  wave;
    s32                                  sinv;
    s32                                  cosv;
    s32                                  w;
    s32                                  d;
    s32                                  z;
    s32                                  otz;
    s32                                  v;
    s32                                  dy;
    s32                                  xv;
    s32                                  x;
    s32                                  xe;
    s32                                  xMin;
    s32                                  xMax;
    s32                                  fadeLen;
    s32                                  one;
    DisplayState*                        disp;

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
    sinArg                                                 = task->killCountdown * 2;
    cosArg                                                 = task->killCountdown;
    *(_NeoArkSubmarineTunnelRippleScratch**)G_SCRATCH_HEAD = *(_NeoArkSubmarineTunnelRippleScratch**)G_SCRATCH_HEAD - 1;
    scratch                                                = *(_NeoArkSubmarineTunnelRippleScratch**)G_SCRATCH_HEAD;
    TransposeMatrix(&gGfxViewCoord.workm, &scratch->mtx);
    scratch->origin.vx = gGfxViewCoord.workm.t[0];
    scratch->origin.vy = gGfxViewCoord.workm.t[1];
    scratch->origin.vz = gGfxViewCoord.workm.t[2];
    _neoArkSubmarineTunnelRotTrans(&scratch->mtx, &scratch->origin);
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
    *(_NeoArkSubmarineTunnelRippleScratch**)G_SCRATCH_HEAD += 1;
}

/// Draws a wavy screen-distortion band for some views of areas 12 and 30 and
/// returns at once for every other view. Each screen row between the view's
/// start and end rows gets one or two raw-textured `POLY_FT4` strips that
/// sample the other display buffer shifted vertically by a `rsin` / `rcos`
/// wave, faded out over the band's last 16 rows; the strips are linked into
/// `gGpuCurrentOt` one depth nearer per row. One view of area 30 draws a second
/// band. The wave phases derive from `Task::killCountdown`, seeded from
/// `rand()` on the first call and advanced every call while `Gp_StateF0.field_4` is
/// clear.
///
/// Matching note: `spare` is never assigned, so `spare >> 16` is always zero;
/// it stands in for the stack slot the retail frame carries, which the
/// register allocator needs to see.
void func_neo_ark_submarine_tunnel_8017E288(Task* task)
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

/// State handlers of the room task `func_neo_ark_submarine_tunnel_8017F434`
/// runs: `func_neo_ark_submarine_tunnel_8017F3BC` sets it up,
/// `func_neo_ark_submarine_tunnel_8017F414` runs every later tick, and
/// `taskKill` ends it.
const TaskFuncTable3 D_neo_ark_submarine_tunnel_8017D614 = {
    { func_neo_ark_submarine_tunnel_8017F3BC, func_neo_ark_submarine_tunnel_8017F414, taskKill }
};

/// Task that ripples the whole screen: it redraws the frame just rendered as a
/// 10 by 30 grid of textured quads whose corners are pushed around by sine
/// waves. The first frame gives every column and row edge a random phase
/// offset and speed, takes its context from `spawnArg2` and passes -8 to
/// `Display_ClampField126`. Afterwards the context's mode ramps the strength
/// up to its limit (mode 0), back down to zero and on to mode 2 (mode 1), or
/// ends the task and passes 0 back (mode 2); the displacement is the ramp's
/// share of the context's peak. A non-zero tint flag shades the quads with the
/// context's colour instead of drawing them unlit. The grid is bracketed by
/// draw-mode packets that switch mask-bit setting on at the back of the order
/// table and off again at the front. The state is read through a plain word
/// load at its offset, which keeps it ordered after the store to `D_800691CA`.
void func_neo_ark_submarine_tunnel_8017E828(Task* arg0)
{
    ActorWaveCtx* ctx;
    POLY_FT4*     p;
    DR_STP*       stp;
    s32           i, j, k;
    s32           drawY;
    s32           tpage0, tpage1;
    s32           u0, u1, v0, v1;
    s32           waveX0, waveY0, waveX1, waveY1;
    s32           waveX2, waveY2, waveX3, waveY3;

    D_800691CA = 2;
    switch (*(s32*)((u8*)arg0 + 0x30)) {
        case 0:
            for (i = 0; i < 11; i++) {
                D_neo_ark_submarine_tunnel_80187910[i].phase  = 0;
                D_neo_ark_submarine_tunnel_80187910[i].offset = (u32)rand() >> 3;
                D_neo_ark_submarine_tunnel_80187910[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                D_neo_ark_submarine_tunnel_80187960[i].phase  = 0;
                D_neo_ark_submarine_tunnel_80187960[i].offset = (u32)rand() >> 3;
                D_neo_ark_submarine_tunnel_80187960[i].speed  = (rand() * 100 + 20) >> 15;
            }
            D_neo_ark_submarine_tunnel_80181A4C          = 0;
            D_neo_ark_submarine_tunnel_8018790C          = arg0->spawnArg2;
            D_neo_ark_submarine_tunnel_8018790C->field_6 = 0;
            D_neo_ark_submarine_tunnel_8018790C->field_4 = 0;
            Display_ClampField126(-8);
            arg0->state++;
            break;
        case 1:
            ctx = D_neo_ark_submarine_tunnel_8018790C;
            switch (ctx->field_4) {
                case 0:
                    if (ctx->field_6 < ctx->field_0) {
                        ctx->field_6++;
                    }
                    break;
                case 1:
                    if (ctx->field_6 > 0) {
                        ctx->field_6--;
                    } else {
                        ctx->field_4 = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    Display_ClampField126(0);
                    break;
            }
            D_neo_ark_submarine_tunnel_80181A4C = D_neo_ark_submarine_tunnel_8018790C->field_6 * D_neo_ark_submarine_tunnel_8018790C->field_2 / D_neo_ark_submarine_tunnel_8018790C->field_0;
            for (i = 0; i < 11; i++) {
                D_neo_ark_submarine_tunnel_80187910[i].phase += D_neo_ark_submarine_tunnel_80187910[i].speed;
            }
            for (i = 0; i < 30; i++) {
                D_neo_ark_submarine_tunnel_80187960[i].phase += D_neo_ark_submarine_tunnel_80187960[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)(p + 1);
                    setPolyFT4(p);
                    if (D_neo_ark_submarine_tunnel_8018790C->field_8 == 0) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = D_neo_ark_submarine_tunnel_8018790C->field_9;
                        p->g0 = D_neo_ark_submarine_tunnel_8018790C->field_A;
                        p->b0 = D_neo_ark_submarine_tunnel_8018790C->field_B;
                    }
                    u0 = k * 32;
                    u1 = (k + 1) * 32;
                    if (u1 == 320)
                        u1 = 319;
                    if (u0 < 128) {
                        p->tpage = tpage0;
                    } else {
                        p->tpage = tpage1;
                        u0      -= 128;
                        u1      -= 128;
                    }
                    if (j != -1) {
                        v1     = (j + 1) * 8 + gDisplayState.drawBuffer * 16;
                        v0     = j * 8 + gDisplayState.drawBuffer * 16;
                        waveX0 = D_neo_ark_submarine_tunnel_80181A4C * (rsin((j << 9) + D_neo_ark_submarine_tunnel_80187910[k].phase + D_neo_ark_submarine_tunnel_80187910[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_neo_ark_submarine_tunnel_80181A4C * (rsin((k << 10) + D_neo_ark_submarine_tunnel_80187960[j].phase + D_neo_ark_submarine_tunnel_80187960[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_neo_ark_submarine_tunnel_80181A4C * (rsin((j << 9) + D_neo_ark_submarine_tunnel_80187910[k + 1].phase + D_neo_ark_submarine_tunnel_80187910[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_neo_ark_submarine_tunnel_80181A4C * (rsin(((k + 1) << 10) + D_neo_ark_submarine_tunnel_80187960[j].phase + D_neo_ark_submarine_tunnel_80187960[j].offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        drawY = gDisplayState.drawBuffer * 16;
                        p->x0 = k * 32 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 32 - 160;
                        p->y1 = -112;
                        v0    = drawY + 8;
                        v1    = drawY;
                    }
                    {

                        waveX2 = D_neo_ark_submarine_tunnel_80181A4C * (rsin(((j + 1) << 9) + D_neo_ark_submarine_tunnel_80187910[k].phase + D_neo_ark_submarine_tunnel_80187910[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = D_neo_ark_submarine_tunnel_80181A4C * (rsin((k << 10) + D_neo_ark_submarine_tunnel_80187960[j + 1].phase + D_neo_ark_submarine_tunnel_80187960[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = D_neo_ark_submarine_tunnel_80181A4C * (rsin(((j + 1) << 9) + D_neo_ark_submarine_tunnel_80187910[k + 1].phase + D_neo_ark_submarine_tunnel_80187910[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = D_neo_ark_submarine_tunnel_80181A4C * (rsin(((k + 1) << 10) + D_neo_ark_submarine_tunnel_80187960[j + 1].phase + D_neo_ark_submarine_tunnel_80187960[j + 1].offset) << 3);
                        p->y3  = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    p->u0 = u0;
                    p->v0 = v0;
                    p->u1 = u1;
                    p->v1 = v0;
                    p->u2 = u0;
                    p->v2 = v1;
                    p->u3 = u1;
                    p->v3 = v1;
                    addPrim(&gGpuCurrentOt[3], p);
                }
            }
            break;
    }
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
}

s32 func_neo_ark_submarine_tunnel_8017F064(s32 arg0, s32 arg1, RoomEventMsg* arg2)
{
    u8 temp_s0;
    u8 temp_s0_2;
    u8 temp_s0_3;
    u8 temp_s0_4;

    temp_s0 = arg2->field_2;
    if ((temp_s0 == 1) && (GameFlag_GetNibble(0xFF) == temp_s0) && (gGameSession->at4.loc.place == 3)) {
        func_800E3FAC(0xA2, 0x35);
        GameFlag_SetNibble(0xFF, 2);
        GameFlag_SetNibble(0x11F, 1);
        D_8007272D = 0x1A;
        func_800E8634((s32)&D_80135220, 0, (s32)&D_80135FD0);
    }
    if ((arg2->field_2 == 2) && (GameFlag_GetNibble(0xBC) == 0)) {
        temp_s0_2 = gGameSession->at4.loc.place;
        if (temp_s0_2 == 1) {
            func_800E8614((s32)&D_neo_ark_submarine_tunnel_80181AF0, 0);
            D_neo_ark_submarine_tunnel_80181DF0 = temp_s0_2;
        }
    }
    temp_s0_3 = arg2->field_2;
    if ((temp_s0_3 == 3) && (D_neo_ark_submarine_tunnel_80181DF0 == 0) && (gGameSession->at4.loc.warp == 2) && (GameFlag_GetNibble(0xFF) == 0) && (gGameSession->at4.loc.place == temp_s0_3)) {
        GameFlag_SetNibble(0xFF, 1);
        func_800E8614((s32)&D_80136108, 0);
        D_neo_ark_submarine_tunnel_80181DF0 = 1;
    }
    if ((arg2->field_2 == 2) && (D_neo_ark_submarine_tunnel_80181DF0 == 0)) {
        temp_s0_4 = gGameSession->at4.loc.warp;
        if (temp_s0_4 == 1) {
            Gp_MsgPlayerWeapon(1);
            D_neo_ark_submarine_tunnel_80181DF0 = temp_s0_4;
        }
    }
    if ((arg2->field_2 == 3) && (D_neo_ark_submarine_tunnel_80181DF0 == 0) && (gGameSession->at4.loc.warp == 2)) {
        Gp_MsgPlayerWeapon(1);
        D_neo_ark_submarine_tunnel_80181DF0 = 1;
    }
    return 0;
}

/// Answers 0 unconditionally.
s32 func_neo_ark_submarine_tunnel_8017F27C(void)
{
    return 0;
}

/// Save-location message handler: copies the incoming `GpSaveLoc` onto the
/// outgoing one, forwards both to `func_80179B14` and answers 1.
s32 func_neo_ark_submarine_tunnel_8017F284(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}

/// Message 0x13F0 handler: for an `arg2` of 4 or 5, and only while the
/// session's place is 1, passes it to `Gp_SpawnIfCapIdle`. Answers 0.
s32 func_neo_ark_submarine_tunnel_8017F2C8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 < 6) {
        if (arg2 >= 4) {
            if (gGameSession->at4.loc.place == 1) {
                Gp_SpawnIfCapIdle(arg2, 0);
            }
        }
    }
    return 0;
}

/// Starts or steers the screen wave. A non-positive `arg0` sets the CD
/// queue's `field_22A` to 2, fills the wave context (ramp length 1, peak 0x60,
/// tinted 0x40/0x80/0x80) and spawns the wave task with it; a positive one is
/// written to the context's mode, where 1 ramps the running wave back down.
void func_neo_ark_submarine_tunnel_8017F318(s32 arg0)
{
    CdCmdQueue* queue = &CdCmd_Queue;

    if (arg0 <= 0) {
        queue->field_22A                            = 2;
        D_neo_ark_submarine_tunnel_80187A20.field_0 = 1;
        D_neo_ark_submarine_tunnel_80187A20.field_2 = 0x60;
        D_neo_ark_submarine_tunnel_80187A20.field_9 = 0x40;
        D_neo_ark_submarine_tunnel_80187A20.field_8 = 1;
        D_neo_ark_submarine_tunnel_80187A20.field_A = 0x80;
        D_neo_ark_submarine_tunnel_80187A20.field_B = 0x80;
        Task_SpawnFromTable(&D_neo_ark_submarine_tunnel_80181A34, 0, 0, (s32)&D_neo_ark_submarine_tunnel_80187A20);
        return;
    }
    D_neo_ark_submarine_tunnel_80187A24 = arg0;
}

void func_neo_ark_submarine_tunnel_8017F398(s32 arg0)
{
    GameFlag_SetNibble(0xBC, arg0);
}

/// First state of the room task: installs the room's message table, publishes
/// the task in pointer slot 7, plays sound event 0x550C0003 and advances.
void func_neo_ark_submarine_tunnel_8017F3BC(Task* arg0)
{
    arg0->msgTable = D_neo_ark_submarine_tunnel_80181A50;
    Game_SetPtrSlot(arg0, 7);
    SndEvt_EnqueueType6(0x550C0003, 0, 0);
    arg0->state = arg0->state + 1;
}

/// Later states of the room task: reads pointer slot 3 and discards it.
void func_neo_ark_submarine_tunnel_8017F414(Task* task)
{
    gameGetPtrSlot(3);
}

/// Room task tick: copies the three-entry state table
/// `D_neo_ark_submarine_tunnel_8017D614` to the stack and calls the entry for
/// the task's state.
void func_neo_ark_submarine_tunnel_8017F434(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_submarine_tunnel_8017D614;
    sp.funcs[task->state](task);
}
