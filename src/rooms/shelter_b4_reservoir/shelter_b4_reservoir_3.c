#include "common.h"
#include <psyq/inline_c.h>
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/rooms_shared_8017e690.h"

extern void func_shelter_b4_reservoir_8017E068(void);
void        func_shelter_b4_reservoir_8017E8E4(void);
void        func_shelter_b4_reservoir_8017E8EC(Task* task);

extern s16 D_shelter_b4_reservoir_80184F80;

typedef struct {
    s16 field_0;
    s16 field_2;
    s16 field_4;
    s16 field_6;
    s32 field_8;
} _Unk80184F90;

/// Working copy of one surface's extents, carved off the scratchpad stack.
typedef struct {
    s16 y;
    s16 dx;
    s16 dy;
    s16 step;
    s16 x;
    s16 z;
} _SurfaceScratch;

extern s8           D_8007217B;
extern s16          D_shelter_b4_reservoir_80184F82;
extern _Unk80184F90 D_shelter_b4_reservoir_80184F90[];
extern s16          D_shelter_b4_reservoir_80185020;
extern u8*          D_shelter_b4_reservoir_80187630;

void func_shelter_b4_reservoir_8017EA00(Task* task);
void func_shelter_b4_reservoir_8017EE04(Task* task);
void func_shelter_b4_reservoir_8017F23C(Task* task);
void func_shelter_b4_reservoir_8017F674(Task* task);

void func_shelter_b4_reservoir_8017E8E4(void)
{
}

void func_shelter_b4_reservoir_8017E8EC(Task* task)
{
    _Unk80184F90* p = D_shelter_b4_reservoir_80184F90;

    if (D_8007217B == 0) {
        D_shelter_b4_reservoir_80187630 = (u8*)D_8005C374 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_shelter_b4_reservoir_80187630 = (u8*)D_8005C370 + gDisplayState.otBuffer * 0xC000;
    }
    if (gGameSession->at4.loc.view != 0xA) {
        p->field_6 = 0x2328 - (((D_shelter_b4_reservoir_80184F80 + 0x7D0) * 0x31) >> 5);
        func_shelter_b4_reservoir_8017EA00(task);
        func_shelter_b4_reservoir_8017EE04(task);
        func_shelter_b4_reservoir_8017F23C(task);
        return;
    }
    if (D_shelter_b4_reservoir_80184F82 < -0x708) {
        D_shelter_b4_reservoir_80184F82 = -0x708;
    } else if (D_shelter_b4_reservoir_80184F82 > 0) {
        D_shelter_b4_reservoir_80184F82 = 0;
    }
    func_shelter_b4_reservoir_8017F674(task);
}

/// Draws each surface in `D_shelter_b4_reservoir_80184F90` as a strip of 32
/// flat semi-transparent quads laid along Z, projected through the view
/// matrix, each followed by a draw-mode packet selecting blend mode 2. Quads
/// the projection flags as invalid are skipped. `task` is unused.
void func_shelter_b4_reservoir_8017EA00(Task* task)
{
    SVECTOR          v0, v1, v2, v3;
    s32              sxy0, sxy1, sxy2, sxy3;
    s32              p, flag;
    u8*              head;
    _SurfaceScratch* s;
    _Unk80184F90*    e;
    POLY_F4*         poly;
    DR_MODE*         dr;
    s32              otz;
    s32              i;

    e                 = D_shelter_b4_reservoir_80184F90;
    head              = *(u8**)0x1F8003FC;
    gGfxViewCoord.flg = 0;
    *(u8**)0x1F8003FC = head - 0xC;
    s                 = (_SurfaceScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    ((_SurfaceScratch*)(head - 0xC))->y = D_shelter_b4_reservoir_80184F80;
    for (; e->field_8 != -1; e++) {
        s->dx   = e->field_4;
        s->step = e->field_6 / 32;
        s->x    = e->field_0 + D_shelter_b4_reservoir_80185020;
        s->z    = e->field_2;
        for (i = 0; i < 32; i++) {
            v0.vx = s->x;
            v0.vy = s->y;
            v0.vz = s->z + s->step * i;
            v1.vx = s->x;
            v1.vy = s->y;
            v1.vz = s->z + s->step * (i + 1);
            s->dy = 0;
            v2.vx = s->x + s->dx;
            v2.vy = s->y + s->dy;
            v2.vz = s->z + s->step * i;
            s->dy = 0;
            v3.vx = s->x + s->dx;
            v3.vy = s->y + s->dy;
            v3.vz = s->z + s->step * (i + 1);
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                            = (POLY_F4*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(poly + 1);
                setlen(poly, 5);
                setcode(poly, 0x2A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0x80;
                poly->g0         = 0;
                poly->b0         = 0;
                addPrim((u_long*)(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt),
                        poly);
                dr                              = (DR_MODE*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_3", func_shelter_b4_reservoir_8017EE04);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_3", func_shelter_b4_reservoir_8017F23C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_3", func_shelter_b4_reservoir_8017F674);

void func_shelter_b4_reservoir_8017FADC(Task* task)
{
    TaskFunc states[2] = { RoomsShared8017e690, func_shelter_b4_reservoir_8017E8EC };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_reservoir_80184F80;
}
