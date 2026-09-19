#include "common.h"
#include "actors/actor_202600.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/sound.h"
#include "main/tmd.h"

void func_actor_202600_8014B950(Actor202600* arg0)
{
    Actor202600Obj2C* obj;
    Actor202600Work*  work;
    GsCOORDINATE2*    coord;
    s16(*motion)[2];
    SVECTOR*        scratchEnd;
    SVECTOR*        velocity;
    s32             state;
    s32             one;
    s32             pan1;
    s32             pan2;
    s32             pan3;
    s16             timer;
    Actor202600Ctx* ctx;
    s32             indexOrSound;
    u32             randomY;
    u32             randomZ;
    u32             randomX;
    u32             randomDelay;
    u32             randomRise;

    obj        = arg0->field_2C;
    work       = arg0->field_1C;
    ctx        = arg0->field_20;
    scratchEnd = *(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    velocity   = (*(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC) = scratchEnd - 1);
    state      = work->field_39C;
    coord      = obj->field_8;
    one        = 1;
    switch (state) {
        case 0:
            work->field_294.flags &= 0x7FFF;
            work->field_214.flags &= 0xBFFF;
            obj->field_C           = (u16)obj->field_C | 0x84;
            ctx->field_14          = one;
            if (D_8011540E == one) {
                if (work->field_3C2 == 0) {
                    work->field_39E = D_actor_202600_801527C8[work->field_3C4];
                } else {
                    work->field_39E = D_actor_202600_801527F8[work->field_3C4];
                }
                work->field_39C = 1;
            }
            break;
        case 1:
            timer           = (u16)work->field_39E - 1;
            work->field_39E = timer;
            if (timer <= 0) {
                work->field_39E = 0;
                work->field_39C = 2;
            }
            break;
        case 2:
            Tmd_AllocBuffers((TmdObject*)obj);
            obj->field_C = (u16)obj->field_C & 0xFFFB;
            indexOrSound = 0;
            if (work->field_3C2 == 0) {
                work->field_39C = 3;
                work->field_392 = 4;
                work->field_398 = 0;
                work->field_3A6 = 0;
                work->field_3A2 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
            } else {
                work->field_392        = 7;
                work->field_39A        = state;
                work->field_39C        = 1;
                work->field_3A8        = D_actor_202600_80152800[work->field_3C4];
                work->field_294.flags |= 0x8000;
                work->field_214.flags |= 0x4000;
                do {
                    velocity->vx = 0;
                    velocity->vz = 0;
                    randomRise   = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomRise;
                    velocity->vy = ((randomRise >> 0x10) & 0x1FF) + 0x2EE;
                    Gp_SpawnEff(0x6017C, coord, 0, velocity);
                    indexOrSound++;
                } while (indexOrSound < 5);
                indexOrSound = (((u16)((Actor202600Ctx*)arg0->field_20)->field_8 >> 0xC) << 8) | 0x510D0012;
                pan1         = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(indexOrSound, pan1, (s8)Gp_GetObjDepth(coord));
            }
            break;
        case 3:
            if ((s16)work->field_396 == 0x1E) {
                indexOrSound = (((u16)((Actor202600Ctx*)arg0->field_20)->field_8 >> 0xC) << 8) | 0x51090007;
                pan2         = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(indexOrSound, pan2, (s8)Gp_GetObjDepth(coord));
            }
            indexOrSound = 0;
            if ((s16)work->field_396 == 0x27) {
                do {
                    randomX      = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomX;
                    velocity->vx = -((coord->coord.m[0][2] * (s32)(((randomX >> 16) & 0x3F) + 0xAF)) >> 12);
                    randomY      = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomY;
                    velocity->vy = ((randomY >> 16) & 0x1FF) - 0x6D6;
                    randomZ      = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomZ;
                    velocity->vz = -((coord->coord.m[2][2] * (s32)(((randomZ >> 16) & 0x3F) + 0xAF)) >> 12);
                    Gp_SpawnEff(0x60051, coord, 0, velocity);
                    indexOrSound++;
                } while (indexOrSound < 3);
                indexOrSound = 0;
            }
            motion = D_actor_202600_80152850;
            do {
                indexOrSound++;
                if ((s16)work->field_396 <= ((*motion)[0] + D_actor_202600_80152838)) {
                    coord->coord.t[0] += ((*motion)[1] * rsin(work->field_3A2)) >> 12;
                    coord->coord.t[2] += ((*motion)[1] * rcos(work->field_3A2)) >> 12;
                    break;
                }
                motion++;
            } while (indexOrSound < 9);
            if ((s16)work->field_396 == 0x28) {
                indexOrSound = (((u16)((Actor202600Ctx*)arg0->field_20)->field_8 >> 0xC) << 8) | 0x401A0002;
                pan3         = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(indexOrSound, pan3, (s8)Gp_GetObjDepth(coord));
                work->field_3A8        = 0x80;
                work->field_294.flags |= 0x8000;
                work->field_214.flags |= 0x4000;
            }
            if ((s16)work->field_396 >= (D_actor_202600_80152838 + 0x46)) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                randomDelay     = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = D_actor_202600_80152798[((Actor202600Ctx*)arg0->field_20)->field_3C->rowIndex] + ((randomDelay >> 0x10) & 0xF);
                Gp_LcgState     = randomDelay;
                Gp_ArmStateF0(1);
            }
            break;
    }
    *(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC) = *(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}
