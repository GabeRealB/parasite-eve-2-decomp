#include "common.h"

#include "actors/actor_103700.h"
#include "gameplay/3A34.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Modes 8 and 9, the approach after the drop-in: counts `field_256` down,
/// then moves the root coordinate towards the target position `field_23C` by
/// 75/2048 of the unit direction per frame until it is within 150 on Y, then
/// counts 30 frames and hands over to mode 3, arming `Gp_StateF0`.
void Actor03700_Fn027DC(Task* task)
{
    Actor103700SteerScratch* s;
    Actor103700Work*         work;
    GsCOORDINATE2*           coord;
    s32                      d;

    s     = (Actor103700SteerScratch*)(SCRATCH_SP -= sizeof(Actor103700SteerScratch));
    work  = (Actor103700Work*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    switch (work->field_250) {
        case 0:
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 1;
            }
            break;
        case 1:
            s->delta.vx = work->field_23C.vx - coord->coord.t[0];
            s->delta.vy = work->field_23C.vy - coord->coord.t[1];
            s->delta.vz = work->field_23C.vz - coord->coord.t[2];
            VectorNormalS(&s->delta, &s->normal);
            coord->coord.t[0] += (s->normal.vx * 75) >> 11;
            coord->coord.t[1] += (s->normal.vy * 75) >> 11;
            coord->coord.t[2] += (s->normal.vz * 75) >> 11;
            d                  = (s32)work->field_23C.vy - coord->coord.t[1];
            if ((d < 0 ? -d : d) < 150) {
                work->field_250 = 2;
                work->field_256 = 30;
            }
            break;
        case 2:
            if ((s16)--work->field_256 <= 0) {
                work->field_24E = 3;
                work->field_250 = 0;
                work->field_256 = 0;
                Gp_ArmStateF0(1);
            }
            break;
    }
    SCRATCH_SP += sizeof(Actor103700SteerScratch);
}
