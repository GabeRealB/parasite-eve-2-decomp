#include "common.h"

#include "psyq/inline_c.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actors_shared_80163354.h"

void ActorsShared80163354(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, u8 shade)
{
    ActorsShared80163354Scratch* s;
    s16                          angle;
    GsCOORDINATE2*               secondCoord;
    GsCOORDINATE2*               firstCoord;
    s32                          offset0;
    s32                          offset1;
    s32                          offset2;
    s32                          offset3;
    GsCOORDINATE2*               coords;
    POLY_FT4*                    poly;

    coords      = ((TmdObject*)task->extra)->field_8;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = (ActorsShared80163354Scratch*)(*(u8**)G_SCRATCH_HEAD -= sizeof(ActorsShared80163354Scratch));
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&Gfx_ViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        Gp_WorldToLocal(&Gfx_ViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->first.vy       = (s16)height;
        s->second.vy      = (s16)height;
        s->first.vx       = s->firstMatrix.t[0];
        s->first.vz       = s->firstMatrix.t[2];
        s->second.vx      = s->secondMatrix.t[0];
        s->second.vz      = s->secondMatrix.t[2];
        angle             = ratan2(s->second.vx - s->first.vx, s->second.vz - s->first.vz);
        s->halfX          = (s->first.vx - s->second.vx) / 2;
        s->halfZ          = (s->first.vz - s->second.vz) / 2;
        offset0           = rcos(angle) * width;
        s->corner0.vy     = (s16)height;
        s->corner0.vx     = s->halfX + (s->first.vx - (offset0 >> 0xC));
        s->corner0.vz     = s->halfZ + (s->first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1           = rcos(angle) * width;
        s->corner1.vy     = (s16)height;
        s->corner1.vx     = s->halfX + (s->first.vx + (offset1 >> 0xC));
        s->corner1.vz     = s->halfZ + (s->first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2           = rcos(angle) * width;
        s->corner2.vy     = (s16)height;
        s->corner2.vx     = (s->second.vx - (offset2 >> 0xC)) - s->halfX;
        s->corner2.vz     = (s->second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - s->halfZ;
        offset3           = rcos(angle) * width;
        s->corner3.vy     = (s16)height;
        s->corner3.vx     = (s->second.vx + (offset3 >> 0xC)) - s->halfX;
        s->corner3.vz     = (s->second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - s->halfZ;
        Gfx_ViewCoord.flg = 0;
        Gp_UpdateCoord(&Gfx_ViewCoord);
        gte_SetRotMatrix(&Gfx_ViewCoord.workm);
        gte_SetTransMatrix(&Gfx_ViewCoord.workm);
        s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                                 &s->screen2, &s->screen3, &s->perspective, &s->flags);
        if (s->flags >= 0) {
            poly           = Gpu_PrimCursor;
            Gpu_PrimCursor = (u8*)poly + 0x28;
            setlen(poly, 9);
            poly->code       = 0x2E;
            *(s32*)&poly->x0 = s->screen0;
            *(s32*)&poly->x1 = s->screen1;
            *(s32*)&poly->x2 = s->screen2;
            *(s32*)&poly->x3 = s->screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((u32*)((((u32)(s->depth << Display_State.field_128) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt), poly);
        }
        *(u8**)G_SCRATCH_HEAD += sizeof(ActorsShared80163354Scratch);
    }
}
