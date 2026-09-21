#include "common.h"
#include "actors/actor_104000.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include <psyq/inline_c.h>

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern s32 D_actor_104000_80138EB4;
extern s32 D_actor_104000_80138EC0;

/// Turns `coord` to face along its own Z axis in the XZ plane and scales the
/// rotation uniformly by `s`, working on a scratch block.
static __inline__ void Actor104000_FaceScale(GsCOORDINATE2* coord, s16 s)
{
    Actor104000FaceScratch* head;
    Actor104000FaceScratch* sc;

    head                                      = *(Actor104000FaceScratch**)G_SCRATCH_HEAD;
    sc                                        = head - 1;
    *(Actor104000FaceScratch**)G_SCRATCH_HEAD = sc;
    sc->angle                                 = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&sc->m, sc->angle, 1);
    sc->scale.vx = sc->scale.vy = sc->scale.vz = s;
    ScaleMatrix(&sc->m, &head[-1].scale);
    coord->coord.m[0][0]                       = head[-1].m.m[0][0];
    coord->coord.m[0][1]                       = sc->m.m[0][1];
    coord->coord.m[0][2]                       = sc->m.m[0][2];
    coord->coord.m[1][0]                       = sc->m.m[1][0];
    coord->coord.m[1][1]                       = sc->m.m[1][1];
    coord->coord.m[1][2]                       = sc->m.m[1][2];
    coord->coord.m[2][0]                       = sc->m.m[2][0];
    coord->coord.m[2][1]                       = sc->m.m[2][1];
    coord->coord.m[2][2]                       = sc->m.m[2][2];
    coord->flg                                 = 0;
    *(Actor104000FaceScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_80132ED8);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_801334AC);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_80133C3C);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_8013451C);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_80134710);

/// Frames 0x28 onward of the collapse: steps the effects keyed on `field_6`,
/// then fades the colour matrix out and grows the model over frames 0x2A-0x32.
void func_actor_104000_80134D68(Actor104000Ctx* arg0, Actor104000* arg1)
{
    VECTOR            scale;
    Actor104000Work*  work;
    Actor104000Obj2C* obj;
    s16               s;
    s32               pan;
    s32               id;

    work = arg1->field_1C;
    obj  = arg1->field_2C;
    if (work->field_4 != 0) {
        arg0->field_14      = 1;
        obj->field_C        = 0;
        work->obj350.flags |= 0x8000;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj388.key    = Gp_PackObjPair((GpObj50*)arg0, 1);
        work->obj3C0.key    = 0x22222;
        work->field_6       = 0;
        work->obj270.flags |= 0x4000;
        work->savedColorMtx = work->colorMtx;
        work->field_178     = 0;
        func_actor_104000_80132C8C(arg1);
        work->obj3C0.pos.vx = arg1->field_2C->field_8->coord.t[0];
        work->obj3C0.pos.vy = arg1->field_2C->field_8->coord.t[1] - 0x1F4;
        work->obj3C0.pos.vz = arg1->field_2C->field_8->coord.t[2];
        work->obj388.pos.vx = arg1->field_2C->field_8->coord.t[0];
        work->obj388.pos.vy = arg1->field_2C->field_8->coord.t[1];
        work->obj388.pos.vz = arg1->field_2C->field_8->coord.t[2];
        if (work->field_194 == 0x1003) {
            D_actor_104000_8013E538[arg0->field_8 >> 12] = 0;
        }
        return;
    }
    func_actor_104000_80132C8C(arg1);
    switch ((s16)(work->field_6 - 0x28)) {
        case 0:
            work->obj350.flags &= 0x7FFF;
            break;
        case 1:
            if (work->field_496 == 1) {
                if (((GameActor*)(gameGetPtrSlot(3))->work)->field_954 == 2) {
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
                }
                work->field_496 = 0;
            }
            arg1->field_2C->field_C = 2;
            break;
        case 2:
            Gp_SpawnScript18Ex((s32)&D_actor_104000_80138EB4, (s32)&D_actor_104000_80138EC0,
                               (s16)gpGetObjDepth((GsCOORDINATE2*)arg1->field_2C->field_8));
            work->obj388.radius = 0x3E8;
            work->obj3C0.radius = 0xFA;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, 0, 0x7DE);
            work->obj388.flags |= 0x8000;
            work->obj3C0.flags |= 0x8000;
            Gp_SpawnEff(0x6009C, &arg1->field_2C->field_8[2], 1, NULL);
            break;
        case 3:
            work->obj3C0.radius = 0x1F4;
            break;
        case 4:
            work->obj3C0.radius = 0x3E8;
            break;
        case 6:
            Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0xC);
            work->obj3C0.flags &= 0x7FFF;
            break;
        case 8:
            if ((s8)work->field_479 == 0) {
                Gp_SpawnEff(0x6009E, arg1->field_2C->field_8, 0, NULL);
            }
            id  = ((arg0->field_8 >> 12) << 8) | 0x40280004;
            pan = (s8)Gp_GetObjPan((GsCOORDINATE2*)arg1->field_2C->field_8);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth((GsCOORDINATE2*)arg1->field_2C->field_8));
            break;
        case 10:
            work->obj388.flags &= 0x7FFF;
            obj->field_C        = 0x80;
            break;
        case 12:
            obj->field_C |= 4;
            break;
        case 29:
            work->field_0  = 0;
            arg0->field_40 = 0;
            break;
        default:
            work->colorMtx = work->savedColorMtx;
            break;
    }
    work->colorMtx = work->savedColorMtx;
    if ((u16)(work->field_6 - 0x17) < 0x12) {
        work->colorMtx.t[0] += ((s16)work->field_6 - 0x16) * 0x60;
    }
    if ((u16)(work->field_6 - 0x2A) < 9) {
        s = 0xBB8 - ((s16)work->field_6 - 0x2A) * 600;
        if (s < 0x4B0) {
            scale.vx = scale.vy = scale.vz = 0;
            Actor104000_FaceScale(arg1->field_2C->field_8, 0x1000);
            ScaleMatrix(&work->colorMtx, &scale);
            work->colorMtx.t[0] = work->colorMtx.t[1] = work->colorMtx.t[2] = 0;
            Actor104000_FaceScale(arg1->field_2C->field_8, 0x1000);
        } else {
            scale.vx = scale.vy = scale.vz = s;
            work->colorMtx                 = work->savedColorMtx;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(s);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12_real();
            gte_stlvl(work->colorMtx.t);
            s = ((s16)work->field_6 - 0x28) * 0x400 + 0x1000;
            if (s > 0x2000) {
                s = 0x2000;
            }
            Actor104000_FaceScale(arg1->field_2C->field_8, s);
        }
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_801355B8);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_80135B50);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_80135DD4);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_8013614C);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_801367E0);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_80136DC4);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_8013704C);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_801373E8);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_80137908);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_80137D2C);

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_801381A0);

INCLUDE_RODATA("actors/nonmatchings/actor_104000/actor_104000_2", ActorsShared80135df4Table);

INCLUDE_RODATA("actors/nonmatchings/actor_104000/actor_104000_2", func_actor_104000_8013206C);
