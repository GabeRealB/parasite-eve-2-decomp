#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/task.h"

#include <psyq/rand.h>

void func_dryfield_night_gas_station_80180D1C(void);
void func_dryfield_night_gas_station_80180DC8(s16 arg0);

extern TaskFuncTable3 D_dryfield_night_gas_station_8017D644;
extern SVECTOR        D_dryfield_night_gas_station_8017D658;

/// Gates the room's two sprite records on nibble 0x8D, then dispatches the task
/// through the room's own three-state table, copied onto the stack first.
void func_dryfield_night_gas_station_8017FB70(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_gas_station_8017D644;
    func_dryfield_night_gas_station_80180D1C();
    sp.funcs[arg0->state](arg0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_3", func_dryfield_night_gas_station_8017FBD4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_3", func_dryfield_night_gas_station_8017FD80);

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
    coord              = ((TmdObject*)((Task*)Game_GetPtrSlot(0xA))->extra)->field_8;
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
        line           = (LINE_G2*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)((u8*)line + 0x14);
        setLineG2(line);
        setSemiTrans(line, 1);
        line->x0 = x0;
        line->y0 = y0;
        line->x1 = x1;
        line->y1 = y1;
        setRGB0(line, rand() % 100 - 0x7E, 0, 0);
        setRGB1(line, 0, 0, 0);
        addPrim((u_long*)(((((u32)0xA << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), line);
        dr             = Gpu_PrimCursor;
        Gpu_PrimCursor = dr + 1;
        setDrawTPage(dr, 1, 0, 0x25);
        addPrim((u_long*)(((((u32)0xA << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), dr);
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
        coord = ((TmdObject*)((Task*)work->field_0)->extra)->field_8;
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
