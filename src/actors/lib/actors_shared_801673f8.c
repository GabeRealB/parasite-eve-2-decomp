#include "common.h"

#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"

#include "actors/actors_shared_801673f8.h"
#include "actors/actors_shared_80168d3c.h"

/// Moves the task's state machine to `state`, sub-state 0. Each call site
/// reloads `idMap`, and cross-jumping merges the identical stores, which is
/// what leaves one `lw` per arm in front of a shared tail.
static __inline__ void set_state(Task* arg0, s16 state)
{
    ActorsShared80168d3cWork* w = (ActorsShared80168d3cWork*)arg0->idMap;

    w->field_420 = state;
    w->field_422 = 0;
}

void ActorsShared801673f8(Task* arg0)
{
    ActorsShared80168d3cWork* work  = (ActorsShared80168d3cWork*)arg0->idMap;
    TmdObject*                obj   = arg0->extra;
    GpEnemy*                  enemy = arg0->spawnArg2;
    GsCOORDINATE2*            coord = obj->field_8;
    ActorsShared80168d3cWork* w2;
    s32                       id;
    s32                       pan;
    u32                       map;

    if ((work->field_44C & 0xF) == 1) {
        work->field_451      = 1;
        work->obj_2AC.flags |= 0x8000;
        work->obj_2CC.flags &= 0xBFFF;
        obj->field_C        &= 0xFF7F;
        if ((arg0->spawnArg1 & 0xF) != 2) {
            Tmd_AllocBuffers(obj);
            obj->field_C &= 0xFFFB;
        }
        enemy->node.field_4 = 0;
        map                 = *(u32*)&Game_Session->field_4 & 0xFFFF0000;
        if (map == 0x4270000) {
            // The 7C store follows 7A here; written first, it schedules
            // ahead of the heading load.
            work->field_78    = 0;
            work->field_7A    = (D_8018B74C[(work->field_44C >> 8) & 0xF].heading + 0x800) & 0xFFF;
            work->field_7C    = 0;
            coord->coord.t[0] = D_8018B74C[(work->field_44C >> 8) & 0xF].x;
            coord->coord.t[1] = D_8018B74C[(work->field_44C >> 8) & 0xF].y;
            coord->coord.t[2] = D_8018B74C[(work->field_44C >> 8) & 0xF].z;
            id                = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 12) << 8) | 0x54270006;
            pan               = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        } else if (map == 0x4280000) {
            work->field_78    = 0;
            work->field_7A    = (D_801874C4[(work->field_44C >> 8) & 0xF].heading + 0x800) & 0xFFF;
            work->field_7C    = 0;
            coord->coord.t[0] = D_801874C4[(work->field_44C >> 8) & 0xF].x;
            coord->coord.t[1] = D_801874C4[(work->field_44C >> 8) & 0xF].y;
            coord->coord.t[2] = D_801874C4[(work->field_44C >> 8) & 0xF].z;
        }
        work->field_428 = 0;
        work->field_42A = 100;
        w2              = (ActorsShared80168d3cWork*)arg0->idMap;
        w2->field_41C   = 0x10;
        w2->field_418   = 7;
        w2->field_414   = 2;
        switch ((work->field_44C >> 4) & 0xF) {
            case 0:
                set_state(arg0, 1);
                break;
            case 1:
                set_state(arg0, 4);
                break;
            default:
                set_state(arg0, 7);
                break;
        }
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work->field_44C = 0;
    }
}
