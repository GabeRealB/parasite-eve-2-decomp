#include "common.h"

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

#include "actors/actors_shared_8014a1c4.h"
#include "actors/actors_shared_8014df20.h"

extern u32 Gp_LcgState;
extern u8  D_801153F2[2];

void ActorsShared8014a1c4(Task* arg0)
{
    ActorShared8014df20Work* work;
    GsCOORDINATE2*           obj;
    s32                      snd;
    s16                      mode;
    s32                      id;
    GpEnemy*                 ctx;

    work                   = (ActorShared8014df20Work*)arg0->work;
    *(u8**)G_SCRATCH_HEAD -= 8;
    obj                    = ((TmdObject*)arg0->extra)->coords;
    if (Gp_CountRec18Hi(work->field_16C, 0x10000) != 0 || Gp_CountRec18Hi(work->field_134, 0x10000) != 0) {
        D_801153F2[1]   = 1;
        work->field_2AA = 1;
        work->field_28C = 2;
        if (work->field_2A6 != 0 && work->field_2A4 == 0x12) {
            if (work->field_2AC != 0) {
                ctx = arg0->spawnArg2;
                id  = 0x40480007;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            } else {
                ctx = arg0->spawnArg2;
                id  = 0x402E0006;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            }
            work->field_290 = 0;
            work->field_2A6 = 0;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_2A8 = ((Gp_LcgState >> 16) & 0x1F) + 0x12;
        }
    }
    if (work->field_2AA != 0) {
        if (work->field_2AC != 0) {
            ctx = arg0->spawnArg2;
            id  = 0x40480008;
            snd = ((ctx->placeKey >> 12) << 8) | id;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
        } else {
            ctx = arg0->spawnArg2;
            id  = 0x402E0007;
            snd = ((ctx->placeKey >> 12) << 8) | id;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
        }
        work->field_14C.flags &= 0x7FFF;
        work->field_FC.flags  &= 0x7FFF;
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(work->field_16C);
    mode = work->field_28C;
    if (mode == 1) {
        work->field_29A = 1;
        work->field_292 = 0;
        if (work->field_290 > work->field_2A8) {
            if (work->field_2A6 != 0 && work->field_2A4 == 0x12) {
                if (work->field_2AC != 0) {
                    ctx = arg0->spawnArg2;
                    id  = 0x40480007;
                    snd = ((ctx->placeKey >> 12) << 8) | id;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
                } else {
                    ctx = arg0->spawnArg2;
                    id  = 0x402E0006;
                    snd = ((ctx->placeKey >> 12) << 8) | id;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
                }
                work->field_290 = 0;
                work->field_2A6 = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_2A8 = ((Gp_LcgState >> 16) & 0x1F) + 0x12;
            } else if (*(s32*)&work->field_2A4 == 0) {
                work->field_290 = 0;
                work->field_2A6 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_2A8 = ((Gp_LcgState >> 16) & 0x3F) + 0x64;
            }
        }
    } else if (mode == 2) {
        work->field_2AA = 0;
        if (work->field_290 >= 0x28) {
            if (work->field_2AC != 0) {
                ctx = arg0->spawnArg2;
                id  = 0x40480008;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            } else {
                ctx = arg0->spawnArg2;
                id  = 0x402E0007;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            }
            work->field_290 = 0;
        }
    }
    *(u8**)G_SCRATCH_HEAD += 8;
}
