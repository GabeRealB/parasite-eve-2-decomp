#include "common.h"

#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"
#include "actors/actors_shared_801673f8.h"

/// Moves the task's state machine to `state`, sub-state 0. Each call site
/// reloads `work`, and cross-jumping merges the identical stores, which is
/// what leaves one `lw` per arm in front of a shared tail.
static __inline__ void set_state_s16(Task* arg0, s16 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    w->field_420 = state;
    w->field_422 = 0;
}

/// Answers message 0x2C00 with low nibble 1 (latched in `field_44C`): shows
/// the model and arms its hit bodies, places the root at the spawn point
/// bits 8..11 pick from the current map's table (0x427 or 0x428, playing
/// sound 6 on 0x427), requests animation 7 with an upward launch, and starts
/// state 1, 4 or 7 by bits 4..7.
void func_actor_341700_80166114(Task* arg0)
{
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    TmdObject*       obj   = arg0->extra;
    GpEnemy*         enemy = arg0->spawnArg2;
    GsCOORDINATE2*   coord = obj->coords;
    Actor341700Work* w2;
    s32              id;
    s32              pan;
    u32              map;

    if ((work->field_44C & 0xF) == 1) {
        work->field_451      = 1;
        work->obj_2AC.flags |= 0x8000;
        work->obj_2CC.flags &= 0xBFFF;
        obj->flags          &= 0xFF7F;
        if ((arg0->spawnArg1 & 0xF) != 2) {
            Tmd_AllocBuffers(obj);
            obj->flags &= 0xFFFB;
        }
        enemy->node.flags = 0;
        map               = *(u32*)&gGameSession->at4.loc & 0xFFFF0000;
        if (map == 0x4270000) {
            // The 7C store follows 7A here; written first, it schedules
            // ahead of the heading load.
            work->field_78    = 0;
            work->field_7A    = (D_8018B74C[(work->field_44C >> 8) & 0xF].heading + 0x800) & 0xFFF;
            work->field_7C    = 0;
            coord->coord.t[0] = D_8018B74C[(work->field_44C >> 8) & 0xF].x;
            coord->coord.t[1] = D_8018B74C[(work->field_44C >> 8) & 0xF].y;
            coord->coord.t[2] = D_8018B74C[(work->field_44C >> 8) & 0xF].z;
            id                = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x54270006;
            pan               = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
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
        w2              = (Actor341700Work*)arg0->work;
        w2->field_41C   = 0x10;
        w2->field_418   = 7;
        w2->field_414   = 2;
        switch ((work->field_44C >> 4) & 0xF) {
            case 0:
                set_state_s16(arg0, 1);
                break;
            case 1:
                set_state_s16(arg0, 4);
                break;
            default:
                set_state_s16(arg0, 7);
                break;
        }
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work->field_44C = 0;
    }
}

/// Flies backwards off the heading, pitching up toward 0x800, under the
/// accelerating drop `field_42A`; on landing turns around, requests
/// animation 0x11, launches again and advances the state.
void func_actor_341700_801663F0(Task* arg0)
{
    Actor341700Work* work;
    s16              angle;
    GsCOORDINATE2*   coord;
    Actor341700Work* anim;
    s32              speed;
    s32              dx;

    work                                           = (Actor341700Work*)arg0->work;
    angle                                          = work->field_7A;
    coord                                          = ((TmdObject*)arg0->extra)->coords;
    dx                                             = rsin(angle) << 4;
    speed                                          = -0x8C;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += (dx * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    work->field_78                                += (0x800 - work->field_78) >> 3;
    coord->coord.t[1]                             += work->field_42A;
    work->field_428                               += 2;
    work->field_42A                               += work->field_428;
    if (coord->coord.t[1] > 0) {
        work->field_451   = 0;
        work->field_412   = 0;
        coord->coord.t[1] = -0x3C;
        work->field_78    = 0;
        work->field_7C    = 0;
        work->field_7A   += 0x800;
        anim              = (Actor341700Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0x11;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x6E;
        work->field_420++;
    }
}

/// Plays sound 9 on the first frame and hops forward 0x50 units a frame
/// under the accelerating drop; on landing advances the state.
void func_actor_341700_80166568(Task* arg0)
{
    Actor341700Work* work;
    GsCOORDINATE2*   coord;
    s32              soundId;
    s32              pan;
    s16              angle;
    s16              speed;

    work  = (Actor341700Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    speed                                          = 0x50;
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    coord->coord.t[1]                             += work->field_42A;
    work->field_428                               += 4;
    work->field_42A                               += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_412   = 0;
        work->field_420++;
    }
}
