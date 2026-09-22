#include "common.h"

#include "actors/actors_shared_801343e8.h"

extern s8  D_8011540A;
extern u32 Gp_LcgState;

/// Two-phase wait: phase 0 holds until `D_8011540A` reaches the kind's
/// `field_2 - 9`, then phase 1 counts `field_256` down from 5 and rolls a new
/// target position `field_23C` around the attach coordinate from `Gp_LcgState`
/// (mode 8 below a threshold of 10, mode 9 above it, with a larger Y lift),
/// rearms a random countdown and calls `Tmd_AllocBuffers`.
///
/// Carried by two actor slots - `actor_103700` and `actor_203700`; the shared
/// span is in `configs/USA/overlays.toml`.
void ActorsShared801343e8(Task* task)
{
    TmdObject*                obj;
    TmdObject*                ext;
    ActorShared801343e8Work*  work;
    ActorShared801343e8Spawn* spawn;
    GsCOORDINATE2*            coord;
    s32                       diff;

    ext              = (TmdObject*)task->extra;
    work             = (ActorShared801343e8Work*)task->work;
    coord            = ext->coords;
    spawn            = (ActorShared801343e8Spawn*)task->spawnArg2;
    obj              = ext;
    work->field_1C2 &= 0x3FFF;
    obj->flags      |= 0x84;
    spawn->field_14  = 1;

    switch (work->field_250) {
        case 0:
            diff = spawn->field_3C->mode - 9;
            if (D_8011540A >= diff) {
                work->field_250 = 1;
                work->field_256 = 5;
            }
            break;
        case 1:
            diff = spawn->field_3C->mode - 9;
            if (--work->field_256 <= 0) {
                work->field_1C2 |= 0xC000;
                if (diff < 10) {
                    work->field_24E    = 8;
                    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                    work->field_23C.vx = (u16)coord->coord.t[0] + ((Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                    work->field_23C.vy = (u16)coord->coord.t[1] - (((Gp_LcgState >> 16) & 0x1FF) + 0x352);
                    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                    work->field_23C.vz = (u16)coord->coord.t[2] + ((Gp_LcgState >> 16) & 0xFF);
                } else {
                    work->field_24E    = 9;
                    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                    work->field_23C.vx = (u16)coord->coord.t[0] + ((Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                    work->field_23C.vy = (u16)coord->coord.t[1] - (((Gp_LcgState >> 16) & 0x1FF) + 0x73A);
                    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                    work->field_23C.vz = (u16)coord->coord.t[2] + ((Gp_LcgState >> 16) & 0xFF);
                }
                work->field_250 = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_256 = (Gp_LcgState >> 16) & 0x1F;
                Tmd_AllocBuffers(obj);
                obj->flags &= ~4;
            }
            break;
    }
}
