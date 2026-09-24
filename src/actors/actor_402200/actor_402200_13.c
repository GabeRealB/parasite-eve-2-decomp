#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "main/display.h"
#include "main/mem.h"

#include "actors/actor_402200.h"
#include "actors/actors_shared_80136184.h"

/// The two four-vertex index rows the trail's shaded quads take their corners
/// from, into the scratch block's six-entry x / y runs.
extern s16 D_actor_402200_80154178[2][4];

/// Draws the red trail between the two points `func_actor_402200_80135D5C`
/// projects into `field_6FC`..`field_704`: eight segments, each skipped while
/// its interpolated depth is below 0x1E, and each drawn as two shaded quads
/// offset along the screen normal, a centre line and a tpage.
void func_actor_402200_80136184(Actor402200* arg0)
{
    ActorShared80136184Scratch* sc;
    Actor402200Work*            work;
    POLY_G4*                    poly;
    LINE_F2*                    line;
    DR_TPAGE*                   tp;
    u8*                         head;
    u8*                         carve;
    s16*                        y;
    s32                         x0;
    s32                         x1;
    s32                         i;
    s32                         j;

    head  = *(u8**)G_SCRATCH_HEAD;
    carve = head - sizeof(ActorShared80136184Scratch);
    work  = arg0->field_1C;
    y     = work->field_700;
    x1    = work->field_6FC[1];
    x0    = work->field_6FC[0];
    SOFT_TOUCH_REG_USE2(carve, x0, x1);
    sc = (ActorShared80136184Scratch*)carve;
    SOFT_TOUCH_REG(sc);
    *(u8**)G_SCRATCH_HEAD                                                              = carve;
    ((ActorShared80136184Scratch*)(head - sizeof(ActorShared80136184Scratch)))->dir.vx = x1 - x0;
    sc->dir.vy                                                                         = work->field_700[1] - y[0];
    sc->dir.vz                                                                         = 0;
    VectorNormalS((VECTOR*)carve, &((ActorShared80136184Scratch*)(head - sizeof(ActorShared80136184Scratch)))->norm);
    carve        = 0;
    sc->norm.vy *= -1;
    sc->dx       = (work->field_6FC[1] - work->field_6FC[0]) / 8;
    sc->dy       = (work->field_700[1] - work->field_700[0]) / 8;
    sc->dz       = (work->field_704[1] - work->field_704[0]) / 8;
    for (i = 0; i < 8; i++) {
        sc->z = sc->dz * (i + 1) + work->field_704[0];
        if (sc->z < 0x1E) {
            continue;
        }
        sc->x[0] = work->field_6FC[0] + sc->dx * i;
        sc->x[1] = work->field_6FC[0] + sc->dx * (i + 1);
        sc->x[2] = sc->x[0] + ((-(sc->norm.vy * 0x600) >> 12) / sc->z);
        sc->x[3] = sc->x[1] + ((-(sc->norm.vy * 0x600) >> 12) / sc->z);
        sc->x[4] = sc->x[0] + (((sc->norm.vy * 3) >> 3) / sc->z);
        sc->x[5] = sc->x[1] + (((sc->norm.vy * 3) >> 3) / sc->z);
        sc->y[0] = work->field_700[0] + sc->dy * i;
        sc->y[1] = work->field_700[0] + sc->dy * (i + 1);
        sc->y[2] = sc->y[0] + ((-(sc->norm.vx * 0x600) >> 12) / sc->z);
        sc->y[3] = sc->y[1] + ((-(sc->norm.vx * 0x600) >> 12) / sc->z);
        sc->y[4] = sc->y[0] + (((sc->norm.vx * 3) >> 3) / sc->z);
        sc->y[5] = sc->y[1] + (((sc->norm.vx * 3) >> 3) / sc->z);
        for (j = 0; j < 2; j++) {
            poly           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(poly + 1);
            setlen(poly, 8);
            poly->code = 0x3A;
            poly->x0   = sc->x[D_actor_402200_80154178[j][0]];
            poly->y0   = sc->y[D_actor_402200_80154178[j][0]];
            poly->x1   = sc->x[D_actor_402200_80154178[j][1]];
            poly->y1   = sc->y[D_actor_402200_80154178[j][1]];
            poly->x2   = sc->x[D_actor_402200_80154178[j][2]];
            poly->y2   = sc->y[D_actor_402200_80154178[j][2]];
            poly->x3   = sc->x[D_actor_402200_80154178[j][3]];
            poly->y3   = sc->y[D_actor_402200_80154178[j][3]];
            setRGB0(poly, 0xFF, 0, 0);
            setRGB1(poly, 0xFF, 0, 0);
            setRGB2(poly, 0, 0, 0);
            setRGB3(poly, 0, 0, 0);
            addPrim((u32*)((((u32)(sc->z << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
        }
        line           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(line + 1);
        setlen(line, 3);
        line->code = 0x42;
        line->x0   = sc->x[0];
        line->y0   = sc->y[0];
        line->x1   = sc->x[1];
        line->y1   = sc->y[1];
        setRGB0(line, 0xFF, 0, 0);
        addPrim((u32*)((((u32)(sc->z << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), line);
        tp             = (DR_TPAGE*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(tp + 1);
        setlen(tp, 1);
        tp->code[0] = 0xE1000620;
        addPrim((u32*)((((u32)(sc->z << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), tp);
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(ActorShared80136184Scratch);
    carve                  = 0;
}
