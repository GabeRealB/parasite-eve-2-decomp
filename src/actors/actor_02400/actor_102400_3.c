#include "common.h"
#include "actors/actor_102400_fn0208c.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/wipsys.h"

/// Declared as an array: a scalar extern lets sched1 hoist the `field_130`
/// load above the store (`fixed_scalar_and_varying_struct_p`).
extern s8       D_8011540B[1];
extern u32      Gp_LcgState;
extern TaskDesc Actor02400_D0465C[];
/// Frames state 1 waits before spawning, indexed by `field_14E`.
extern s16 Actor02400_D045D4[];

/// Grow-and-spawn sequence driven by `field_13E`: state 0 pulses the scale up
/// until Y passes 0x1C00 (task state 2), state 1 faces the player while Y
/// wobbles around 0x1C00, state 2 spawns the table entry and plays the sound,
/// and state 3 shrinks the scale back to its floor before re-arming the
/// 30..61 frame idle wait.
void ActorsShared80134cfc_Fn339B0(Actor02400Scale* arg0)
{
    Actor02400ScaleWork* work;
    GsCOORDINATE2*       coord;
    s32                  flags;
    s32                  sound;
    s32                  pan;
    Task**               task;
    VECTOR*              delta;
    VECTOR*              scratchEnd;

    scratchEnd                         = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    delta                              = scratchEnd - 1;
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = delta;
    work                               = arg0->field_1C;
    coord                              = arg0->field_2C->field_8;
    flags                              = 0;
    switch (work->field_13E) {
        case 0:
            work->field_13A = 0;
            work->field_138 = 0;
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
            work->field_128 += (u16)work->field_140 + 0x40;
            work->field_12A += (u16)work->field_140 + 0x40;
            work->field_12C += (u16)work->field_140 + 0x40;
            if (work->field_12A > 0x1C00) {
                work->field_13E = 1;
                work->field_140 = 0;
                if (work->field_130 != NULL) {
                    (*work->field_130)->state = 2;
                }
            }
            break;
        case 1:
            work->field_13A   = 0x19;
            scratchEnd[-1].vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            delta->vy         = 0;
            delta->vz         = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_146   = ratan2((s16)scratchEnd[-1].vx, (s16)delta->vz) & 0xFFF;
            if (work->field_142 == 0) {
                work->field_12A += 0x80;
                if (work->field_12A > 0x1D00) {
                    work->field_142 = 1;
                }
            } else {
                work->field_12A -= 0x80;
                if (work->field_12A < 0x1B00) {
                    work->field_142 = 0;
                }
            }
            work->field_140++;
            if (work->field_140 > Actor02400_D045D4[work->field_14E]) {
                work->field_13E = 2;
                work->field_140 = 0;
                work->field_13A = 0;
            }
            break;
        case 2:
            Gp_SpawnEnemyFromTable(Actor02400_D0465C, 1, 0, arg0->field_20);
            D_8011540B[0]   = 1;
            work->field_13E = 3;
            if (work->field_130 != NULL) {
                (*work->field_130)->state = 3;
            }
            work->field_130 = NULL;
            sound           = ((arg0->field_20->placeKey >> 12) << 8) | 0x40180004;
            pan             = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            break;
        case 3:
            work->field_128 -= 0x80;
            if (work->field_128 <= 0x1000) {
                work->field_128 = 0x1000;
                flags           = 1;
            }
            work->field_12A -= 0x80;
            if (work->field_12A <= 0x600) {
                work->field_12A = 0x600;
                flags          |= 2;
            }
            work->field_12C -= 0x80;
            if (work->field_12C <= 0x1000) {
                work->field_12C = 0x1000;
                flags          |= 4;
            }
            if (flags == 7) {
                work->field_13C = 1;
                work->field_13E = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_140 = ((Gp_LcgState >> 16) & 0x1F) + 0x1E;
                work->field_DE |= 0x8000;
            }
            break;
    }
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) += 1;
}
