#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/task.h"

#include <psyq/rand.h>

#include "rooms/dryfield_night_gas_station.h"

void func_8004BFF8(s16 angle, MATRIX* matrix);
void func_dryfield_night_gas_station_80180D1C(void);
void func_dryfield_night_gas_station_80180DC8(s16 arg0);

extern TaskFuncTable3 D_dryfield_night_gas_station_8017D644;
extern SVECTOR        D_dryfield_night_gas_station_8017D650;
extern SVECTOR        D_dryfield_night_gas_station_8017D658;
extern SVECTOR        D_dryfield_night_gas_station_80188580[];

/// The layout template and the live copy the reset below restores from it.
extern DryfieldNightGasStationLayout D_dryfield_night_gas_station_80184374;
extern DryfieldNightGasStationLayout D_dryfield_night_gas_station_8018ABBC;

/// Gates the room's two sprite records on nibble 0x8D, then dispatches the task
/// through the room's own three-state table, copied onto the stack first.
void func_dryfield_night_gas_station_8017FB70(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_gas_station_8017D644;
    func_dryfield_night_gas_station_80180D1C();
    sp.funcs[arg0->state](arg0);
}

/// Resets the live layout lists from the template: the four-entry vector list
/// and its 12-byte records, then the eight-entry list, which is afterwards
/// raised by 0xBB8 on y when `arg0` is nonzero.
void func_dryfield_night_gas_station_8017FBD4(s32 arg0)
{
    DryfieldNightGasStationLayout* dst;
    DryfieldNightGasStationLayout* src;
    DryfieldNightGasStationVec     d;
    s32                            i;

    dst = &D_dryfield_night_gas_station_8018ABBC;
    src = &D_dryfield_night_gas_station_80184374;

    for (i = 0; i < 4; i++) {
        dst->field_4[i].x = src->field_4[i].x;
        dst->field_4[i].y = src->field_4[i].y;
        dst->field_4[i].z = src->field_4[i].z;
        dst->field_C[i]   = src->field_C[i];
    }

    for (i = 0; i < 8; i++) {
        dst->field_8[i].x = src->field_8[i].x;
        dst->field_8[i].y = src->field_8[i].y;
        dst->field_8[i].z = src->field_8[i].z;
    }

    if (arg0 == 0) {
        d.x = 0;
        d.y = 0;
    } else {
        d.x = 0;
        d.y = 0xBB8;
    }
    d.z = 0;

    for (i = 0; i < 8; i++) {
        dst->field_8[i].x += d.x;
        dst->field_8[i].y += d.y;
        dst->field_8[i].z += d.z;
    }
}

/// Draws a marker for entry `arg0` of `D_dryfield_night_gas_station_80188580`:
/// the vector is turned by a fixed -0x262 yaw, offset by
/// `D_dryfield_night_gas_station_8017D650` and projected through
/// `Gfx_ViewWorldMtx`. When the projection passes, a semi-transparent 3x3 dark
/// red `TILE` with its draw-mode `DR_TPAGE` and a bright red `TILE_1` mark the
/// point. The point is then projected again and once more displaced by
/// (-0x3E8, +0x1F4, +0x1F4), and a semi-transparent `LINE_G2` runs from the
/// displaced point's x, 10 pixels below the point, back to the point, shading
/// from black to a red that flickers with `rand()`. Everything is linked into
/// OT slot 0xA.
void func_dryfield_night_gas_station_8017FD80(s32 arg0)
{
    SVECTOR                        off;
    DryfieldNightGasStationMatrix  mtx;
    SVECTOR                        pos;
    s32                            sxy;
    s32                            p;
    s32                            flag;
    u16                            x0;
    u16                            y0;
    u16                            x1;
    u16                            y1;
    u16                            x2;
    SVECTOR*                       vec;
    s32                            val;
    s32                            one;
    DryfieldNightGasStationMatrix* m;
    TILE*                          tile;
    TILE_1*                        tile1;
    LINE_G2*                       line;
    DR_TPAGE*                      dr;

    off               = D_dryfield_night_gas_station_8017D650;
    one               = ONE;
    m                 = &mtx;
    mtx.ident.m00_m01 = one;
    mtx.ident.m02_m10 = 0;
    m->ident.m11_m12  = one;
    mtx.ident.m20_m21 = 0;
    m->ident.m22      = one;
    func_8004BFF8(-0x262, &mtx.mat);
    vec = &D_dryfield_night_gas_station_80188580[arg0];
    ApplyMatrixSV(&mtx.mat, vec, &pos);
    SetRotMatrix(&Gfx_ViewWorldMtx);
    SetTransMatrix(&Gfx_ViewWorldMtx);
    pos.vx += off.vx;
    pos.vy += off.vy;
    pos.vz += off.vz;
    RotTransPers(&pos, &sxy, &p, &flag);
    if (flag >= 0) {
        val            = 0xA;
        x0             = sxy;
        y0             = sxy >> 16;
        tile           = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)tile + 0x10;
        setTile(tile);
        setSemiTrans(tile, 1);
        tile->x0 = x0 - 1;
        tile->y0 = y0 - 1;
        setRGB0(tile, 0x80, 0, 0);
        tile->w = 3;
        tile->h = 3;
        addPrim((u_long*)(((((u32)val << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), tile);
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 1, 0, 0x25);
        addPrim((u_long*)(((((u32)val << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), dr);
        tile1          = (TILE_1*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)tile1 + 0xC;
        setTile1(tile1);
        setSemiTrans(tile1, 1);
        tile1->x0 = x0;
        tile1->y0 = y0;
        setRGB0(tile1, 0xFF, 0, 0);
        addPrim((u_long*)(((((u32)val << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), tile1);

        mtx.ident.m00_m01 = one;
        mtx.ident.m02_m10 = 0;
        m->ident.m11_m12  = one;
        mtx.ident.m20_m21 = 0;
        m->ident.m22      = one;
        val               = -0x262;
        func_8004BFF8(val, &mtx.mat);
        ApplyMatrixSV(&mtx.mat, vec, &pos);
        SetRotMatrix(&Gfx_ViewWorldMtx);
        SetTransMatrix(&Gfx_ViewWorldMtx);
        pos.vx += off.vx;
        pos.vy += off.vy;
        pos.vz += off.vz;
        RotTransPers(&pos, &sxy, &p, &flag);
        x1      = sxy;
        y1      = sxy >> 16;
        pos.vx -= 0x3E8;
        pos.vy += 0x1F4;
        pos.vz += 0x1F4;
        RotTransPers(&pos, &sxy, &p, &flag);
        x2             = sxy;
        line           = (LINE_G2*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)line + 0x14;
        setLineG2(line);
        setSemiTrans(line, 1);
        line->x0 = x2;
        line->y0 = y1 + 0xA;
        line->x1 = x1;
        line->y1 = y1;
        setRGB1(line, rand() % 60 + 0x50, 0, 0);
        val = 0xA;
        setRGB0(line, 0, 0, 0);
        addPrim((u_long*)(((((u32)val << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), line);
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 1, 0, 0x25);
        addPrim((u_long*)(((((u32)val << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), dr);
    }
}

/// Draws the room's lamp beam. The matrix comes from the slot-0xA model's
/// per-part coordinate array, 0x280 (part 8) in, composed into an identity
/// rotation on the stack; the beacon vector `D_dryfield_night_gas_station_8017D658`
/// is rotated by it twice - as-is and 0x190 further up - and both points are
/// added to that world position and projected through `Gfx_ViewWorldMtx`. Once
/// both `RotTransPers` FLAG words pass, a semi-transparent `LINE_G2` between
/// them, whose red channel flickers with `rand() % 100 - 0x7E`, and its
/// draw-mode `DR_TPAGE` are linked into OT slot 0xA.
void func_dryfield_night_gas_station_801802EC(void)
{
    MATRIX         mtx;
    SVECTOR        pos;
    SVECTOR        p0;
    SVECTOR        p1;
    SVECTOR        off;
    s32            sxy;
    s32            p;
    s32            flag0;
    s32            flag1;
    u16            x0;
    u16            y0;
    u16            x1;
    u16            y1;
    s32            one;
    MATRIX*        m;
    GsCOORDINATE2* coord;
    LINE_G2*       line;
    DR_TPAGE*      dr;

    off                = D_dryfield_night_gas_station_8017D658;
    one                = ONE;
    m                  = &mtx;
    *(s32*)&mtx        = one;
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[1][1] = one;
    *(s32*)&m->m[2][0] = 0;
    m->m[2][2]         = one;
    coord              = ((TmdObject*)(gameGetPtrSlot(0xA))->extra)->coords;
    Gp_ComposeParentWorld((GsCOORDINATE2*)((u8*)coord + 0x280), m, &pos);
    ApplyMatrixSV(&mtx, &off, &p0);
    p0.vx  += pos.vx;
    p0.vy  += pos.vy;
    p0.vz  += pos.vz;
    off.vy += 0x190;
    ApplyMatrixSV(&mtx, &off, &p1);
    p1.vx += pos.vx;
    p1.vy += pos.vy;
    p1.vz += pos.vz;
    SetRotMatrix(&Gfx_ViewWorldMtx);
    SetTransMatrix(&Gfx_ViewWorldMtx);
    RotTransPers(&p0, &sxy, &p, &flag0);
    x0 = sxy;
    y0 = sxy >> 16;
    RotTransPers(&p1, &sxy, &p, &flag1);
    x1 = sxy;
    y1 = sxy >> 16;
    if (flag0 >= 0 && flag1 >= 0) {
        line           = (LINE_G2*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)line + 0x14;
        setLineG2(line);
        setSemiTrans(line, 1);
        line->x0 = x0;
        line->y0 = y0;
        line->x1 = x1;
        line->y1 = y1;
        setRGB0(line, rand() % 100 - 0x7E, 0, 0);
        setRGB1(line, 0, 0, 0);
        addPrim((u_long*)(((((u32)0xA << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), line);
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 1, 0, 0x25);
        addPrim((u_long*)(((((u32)0xA << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), dr);
    }
}

/// Bursts the room's lamp effect: `arg0` picks one of three spawn-argument
/// triples and the effect's `arg2`, the spark is spawned at the lamp task's own
/// coordinate, and the lamp is then told to light up. Any other `arg0` only
/// switches the lamp back to dark.
void func_dryfield_night_gas_station_80180604(s32 arg0)
{
    GpWorkObj*     work;
    GsCOORDINATE2* coord;
    SVECTOR        offset;

    work = Gp_FindWorkById(gGameSession->at4.loc.area | ((gGameSession->at4.loc.stage << 8) | 0x2000));
    if (work != NULL) {
        coord = ((TmdObject*)((Task*)work->field_0)->extra)->coords;
        switch (arg0) {
            case 0:
                offset.vx = 0;
                offset.vy = -0x64;
                offset.vz = -0x12C;
                Gp_SpawnEff(0x600E0, coord, 0x300, &offset);
                func_dryfield_night_gas_station_80180DC8(1);
                break;

            case 1:
                offset.vx = 0xC8;
                offset.vy = -0x64;
                offset.vz = -0xC8;
                Gp_SpawnEff(0x600E0, coord, 0x200, &offset);
                func_dryfield_night_gas_station_80180DC8(1);
                break;

            case 2:
                offset.vx = -0x64;
                offset.vy = -0x64;
                offset.vz = -0xC8;
                Gp_SpawnEff(0x600E0, coord, 0x200, &offset);
                func_dryfield_night_gas_station_80180DC8(1);
                break;

            default:
                func_dryfield_night_gas_station_80180DC8(0);
                break;
        }
    }
}
