#include "common.h"
#include "actors/actor_102400_fn0208c.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/sound.h"
#include "main/wipsys.h"

extern s32 D_80115734;
extern u32 Gp_LcgState;

/// Idle breathing driven by `field_13E`: state 0 turns to face the player,
/// state 1 rests with a lower Y-wobble ceiling, and state 2 pulses the scale
/// until Y passes 0x1C00. While `field_14C` is set the work block is reset and
/// the `D_80115734` effect is spawned with a random roll and a sound.
///
/// Shared by `actor_102400` and `actor_202400`.
void ActorsShared801333b0(Actor02400Scale* arg0)
{
    Actor02400ScaleWork*     work;
    GsCOORDINATE2*           coord;
    s16                      limit;
    s32                      diff;
    s32                      step;
    s32                      sound;
    s32                      pan;
    Task**                   eff;
    Actor02400FacingScratch* scratch;
    Actor02400FacingScratch* scratchEnd;

    scratchEnd                                          = *(Actor02400FacingScratch**)PSX_SCRATCH_ADDR(0x3FC);
    *(Actor02400FacingScratch**)PSX_SCRATCH_ADDR(0x3FC) = scratchEnd - 1;
    scratch                                             = scratchEnd - 1;
    work                                                = arg0->field_1C;
    coord                                               = arg0->field_2C->field_8;
    limit                                               = 0x1000;
    switch (work->field_13E) {
        case 0:
            scratchEnd[-1].delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            scratch->delta.vy       = 0;
            scratch->delta.vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_146         = ratan2((s16)scratchEnd[-1].delta.vx, (s16)scratch->delta.vz) & 0xFFF;
            work->field_13A         = 0x19;
            work->field_148         = 0xA;
            work->field_140--;
            if (work->field_140 < 0) {
                work->field_13E = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_140 = ((Gp_LcgState >> 16) & 0x3F) + 0x1E;
            }
            break;
        case 1:
            work->field_13A = 0;
            work->field_148 = 0x14;
            work->field_140--;
            limit = 0x600;
            if (work->field_140 < 0) {
                work->field_13E = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_140 = ((Gp_LcgState >> 16) & 0x1F) + 0x1E;
            }
            break;
        case 2:
            work->field_13A = 0;
            if (work->field_142 == 0) {
                work->field_140 += 0x40;
                if (work->field_140 >= 0x100) {
                    work->field_142 = 1;
                }
            } else {
                work->field_140 -= 0x40;
                if (work->field_140 < -0xFF) {
                    work->field_142 = 0;
                }
            }
            work->field_128 += (u16)work->field_140 + 0x80;
            work->field_12A += (u16)work->field_140 + 0x80;
            work->field_12C += (u16)work->field_140 + 0x80;
            if (work->field_12A > 0x1C00) {
                work->field_13E = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_140 = ((Gp_LcgState >> 16) & 0x1F) + 0x1E;
            }
            break;
    }
    if (work->field_13E < 2) {
        work->field_128 -= 0x80;
        if (work->field_128 <= 0x1000) {
            work->field_128 = 0x1000;
        }
        if (work->field_142 == 0) {
            work->field_12A += 0x80;
            if (work->field_12A > limit + 0x100) {
                work->field_142 = 1;
            }
        } else {
            work->field_12A -= 0x80;
            if (work->field_12A < limit - 0x100) {
                work->field_142 = 0;
            }
        }
        work->field_12C -= 0x80;
        if (work->field_12C <= 0x1000) {
            work->field_12C = 0x1000;
        }
    }
    diff = work->field_148 - work->field_138;
    step = -3;
    if (diff > 0) {
        step = 2;
    }
    if ((diff >= 0 ? diff : -diff) < (step >= 0 ? step : -step)) {
        work->field_138 = work->field_148;
    } else {
        work->field_138 += step;
    }
    if (work->field_14C != 0) {
        work->field_13C = 2;
        work->field_13E = 0;
        work->field_140 = 0;
        work->field_13A = 0;
        work->field_138 = 0;
        work->field_128 = 0x1000;
        work->field_12A = 0x1000;
        work->field_12C = 0x1000;
        scratch->rot.vy = 0;
        scratch->rot.vz = 0;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        scratch->rot.vx = ((Gp_LcgState >> 16) & 0xFF) + 0x100;
        RotMatrix(&scratch->rot, &arg0->field_2C->field_8[2].coord);
        sound = ((arg0->field_20->placeKey >> 12) << 8) | 0x40180002;
        pan   = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(coord));
        eff             = (Task**)Gp_SpawnEff(D_80115734, coord, work->field_14E, NULL);
        work->field_130 = eff;
        if (eff != NULL) {
            Task_Reparent((Task*)arg0, *eff);
        }
    }
    *(Actor02400FacingScratch**)PSX_SCRATCH_ADDR(0x3FC) += 1;
}
