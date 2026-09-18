#include "common.h"
#include "actors/actors_shared_80134318.h"
#include "actors/actors_shared_80135098.h"
#include "gameplay/3CD8.h"

extern u8 D_801153F4;

/// Death sequence for the actor. While `D_801153F4` is 1 it only refreshes the
/// actor colour, and at 2 it holds `field_C` at 0x80. Otherwise sub-state 0
/// saves the model matrix, unlinks the display nodes and starts the light
/// fade; sub-state 1 counts frames, spawning effect 0x600A5 at frame 15 and
/// shrinking the Y scale through `ActorsShared80135098`; sub-state 2 destroys
/// the enemy.
///
/// Shared by `actor_102400` and `actor_202400`.
void ActorsShared80134318(GpEnemy* arg0, Task* arg1)
{
    VECTOR                    pos;
    ActorsShared80134318Work* work;
    TmdObject*                obj;
    GsCOORDINATE2*            coord;
    GsCOORDINATE2*            cur;

    obj   = (TmdObject*)arg1->extra;
    work  = (ActorsShared80134318Work*)arg1->work;
    coord = obj->coords;
    switch (D_801153F4) {
        case 1:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2, &pos, 0, 0);
            return;
        case 2:
            obj->flags = 0x80;
            return;
        case 0:
        default:
            switch (work->field_13E) {
                case 0:
                    work->field_140 = 0;
                    work->field_12A = 0x1000;
                    work->field_100 = coord->coord;
                    arg0->field_54  = 0;
                    Gp_UnlinkNode(&arg0->node);
                    Gp_UnlinkObj(&work->obj_40);
                    Gp_UnlinkObj(&work->obj_C0);
                    Gp_SetLightMode((GpObj4C*)arg0, 1);
                    Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0x18);
                    work->field_13E = 1;
                    cur             = ((TmdObject*)arg1->extra)->coords;
                    pos.vx          = cur->workm.t[0];
                    pos.vy          = cur->workm.t[1];
                    pos.vz          = cur->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2, &pos, 0, 0);
                    if (work->field_130 != NULL) {
                        (*work->field_130)->state = 4;
                    }
                    break;
                case 1:
                    work->field_140++;
                    if ((s16)work->field_140 == 10) {
                        obj->flags = 2;
                    }
                    if ((s16)work->field_140 == 15) {
                        Gp_SpawnEff(0x600A5, coord, 3, NULL);
                    }
                    if ((s16)work->field_140 >= 60) {
                        work->field_13E = 2;
                        obj->flags      = 0x80;
                    }
                    if (work->field_12A > 0x200) {
                        work->field_12A -= 0x50;
                    }
                    ActorsShared80135098((ActorShared80135098*)arg1);
                    cur    = ((TmdObject*)arg1->extra)->coords;
                    pos.vx = cur->workm.t[0];
                    pos.vy = cur->workm.t[1];
                    pos.vz = cur->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2, &pos, 0, 0);
                    break;
                case 2:
                    Gp_DestroyEnemy(arg0, arg1);
                    break;
            }
            break;
    }
}
