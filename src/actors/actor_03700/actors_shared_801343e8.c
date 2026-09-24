#include "common.h"

#include "actors/actor_103700.h"
#include "main/tmd.h"

extern s8 D_8011540A;

/// Mode 7, the drop-in: keeps the actor hidden, unlockable and out of the
/// contact passes until `D_8011540A` reaches the placement's `mode - 9`, then
/// counts `field_256` down from 5 and picks a target position `field_23C` above
/// the root coordinate from `Gp_LcgState` - a lower one and mode 8 for
/// placements below 10, a higher one and mode 9 above - before re-enabling the
/// contacts, rearming a random countdown and allocating the model buffers.
void Actor03700_Fn025C8(Task* task)
{
    TmdObject*       obj;
    TmdObject*       ext;
    Actor103700Work* work;
    GpEnemy*         spawn;
    GsCOORDINATE2*   coord;
    s32              diff;

    ext               = (TmdObject*)task->extra;
    work              = (Actor103700Work*)task->work;
    coord             = ext->coords;
    spawn             = (GpEnemy*)task->spawnArg2;
    obj               = ext;
    work->obj.flags  &= 0x3FFF;
    obj->flags       |= 0x84;
    spawn->node.flags = 1;

    switch (work->field_250) {
        case 0:
            diff = spawn->place->mode - 9;
            if (D_8011540A >= diff) {
                work->field_250 = 1;
                work->field_256 = 5;
            }
            break;
        case 1:
            diff = spawn->place->mode - 9;
            if ((s16)--work->field_256 <= 0) {
                work->obj.flags |= 0xC000;
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
