#include "common.h"

#include "actors/actors_shared_801345fc.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Three-phase approach: counts `field_256` down, then moves the attach
/// coordinate towards the target position by 75/2048 of the unit direction per
/// call until it is within 150 on Y, then counts down again and arms
/// `Gp_ArmStateF0` with the work block in mode 3.
///
/// Carried by two actor slots - `actor_103700` and `actor_203700`; the shared
/// span is in `configs/USA/overlays.toml`.
void ActorsShared801345fc(ActorShared801345fc* arg0)
{
    ActorShared801345fcScratch* s;
    ActorShared801345fcWork*    work;
    GsCOORDINATE2*              coord;
    s32                         d;

    s     = (ActorShared801345fcScratch*)(SCRATCH_SP -= sizeof(ActorShared801345fcScratch));
    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    switch (work->field_250) {
        case 0:
            if (--work->field_256 <= 0) {
                work->field_250 = 1;
            }
            break;
        case 1:
            s->dir.vx = work->field_23C - coord->coord.t[0];
            s->dir.vy = work->field_23E - coord->coord.t[1];
            s->dir.vz = work->field_240 - coord->coord.t[2];
            VectorNormalS(&s->dir, &s->norm);
            coord->coord.t[0] += (s->norm.vx * 75) >> 11;
            coord->coord.t[1] += (s->norm.vy * 75) >> 11;
            coord->coord.t[2] += (s->norm.vz * 75) >> 11;
            d                  = (s32)work->field_23E - coord->coord.t[1];
            if ((d < 0 ? -d : d) < 150) {
                work->field_250 = 2;
                work->field_256 = 30;
            }
            break;
        case 2:
            if (--work->field_256 <= 0) {
                work->field_24E = 3;
                work->field_250 = 0;
                work->field_256 = 0;
                Gp_ArmStateF0(1);
            }
            break;
    }
    SCRATCH_SP += sizeof(ActorShared801345fcScratch);
}
