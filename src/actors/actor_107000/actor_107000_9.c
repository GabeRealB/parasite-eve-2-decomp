#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actors_shared_801673f8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// The enemy's four main-body handlers, dispatched through by state.
extern GpEnemyTaskFuncTable4 D_actor_107000_80131E5C;

/// Offset the reveal arm spawns the 0x60080 effect at, `{ 0, -0x12C, 0 }`.
extern SVECTOR D_actor_107000_8013F5D8;

/// Message 0x7DB handler of the second form's table (`D_actor_107000_8013F5E0`,
/// parked in `Task::msgTable` by `func_actor_107000_80136E88`). The payload's
/// halfword at 0x2 is a command word.
///
/// 4 and 5 are the collapse arms: both spawn the 0x60080 effect on the model's
/// root coordinate, clear the spawn countdown `field_390` and arm the reaction
/// sub-state `field_36A` to 4; 5 clears the spawn counter `field_392` as well.
///
/// Low byte 1 is the reveal. Unless the task already runs one of the two live
/// states, the model is placed at the spawn point bits 8..11 select from the
/// current map's table - `D_8018B74C` on map 0x27, where the appearance sound
/// is cued through `SndEvt_EnqueueType6` as well, `D_801874C4` on 0x28 - the
/// buffers are re-armed, the 0x80 and 4 bits are cleared from the model's flag
/// word, the pose flag `field_14` is zeroed, both render nodes are revealed, and
/// the model is turned to the spawn point's heading. Low byte 3 is the hide:
/// the two bits and the pose flag go the other way, both nodes are hidden, the
/// model's translation and rotation are zeroed, and the task moves to state 4.
s32 func_actor_107000_801378D8(Task* arg0, s32 arg1, Actor107000Msg* arg2)
{
    Actor107000Spawn2Work* work;
    Actor107000Ctx*        ctx;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    SVECTOR                rot;
    u16                    word;
    s32                    mode;
    s32                    sound;
    s32                    pan;

    word  = arg2->field_2;
    obj   = (TmdObject*)arg0->extra;
    ctx   = arg0->spawnArg2;
    work  = (Actor107000Spawn2Work*)arg0->work;
    mode  = word & 0xFFFF;
    coord = obj->coords;
    if (mode == 4) {
        Gp_SpawnEff(0x60080, coord, 0x400, &D_actor_107000_8013F5D8);
        work->field_390 = 0;
        work->field_36A = 4;
        return 0;
    }
    if (mode == 5) {
        Gp_SpawnEff(0x60080, coord, 0x400, &D_actor_107000_8013F5D8);
        work->field_390 = 0;
        work->field_392 = 0;
        work->field_36A = 4;
        return 0;
    }
    if ((word & 0xFF) == 1) {
        if ((u32)(arg0->state - 1) >= 2U) {
            if (gGameSession->at4.loc.area == 0x27) {
                rot.vx            = 0;
                rot.vy            = D_8018B74C[arg2->field_2 >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_8018B74C[arg2->field_2 >> 8].x;
                coord->coord.t[1] = D_8018B74C[arg2->field_2 >> 8].y;
                coord->coord.t[2] = D_8018B74C[arg2->field_2 >> 8].z;
                sound             = (((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54270006);
                pan               = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            } else if (gGameSession->at4.loc.area == 0x28) {
                rot.vx            = 0;
                rot.vy            = D_801874C4[arg2->field_2 >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_801874C4[arg2->field_2 >> 8].x;
                coord->coord.t[1] = D_801874C4[arg2->field_2 >> 8].y;
                coord->coord.t[2] = D_801874C4[arg2->field_2 >> 8].z;
            }
            Tmd_AllocBuffers((TmdObject*)arg0->extra);
            ((TmdObject*)arg0->extra)->flags &= 0xFF7F;
            ((TmdObject*)arg0->extra)->flags &= 0xFFFB;
            ctx->field_14                     = 0;
            work->obj1.flags                 |= 0x8000;
            work->obj2.flags                 |= 0xC200;
            RotMatrix(&rot, &coord->coord);
            work->field_378                        = 0xC8;
            work->field_396                        = 1;
            work->field_398                        = 0x64;
            work->field_39A                        = 0;
            ((TmdObject*)arg0->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        }
        return 0;
    }
    if ((word & 0xFF) == 3) {
        ((TmdObject*)arg0->extra)->flags |= 0x80;
        ((TmdObject*)arg0->extra)->flags |= 4;
        ctx->field_14                     = 1;
        work->obj1.flags                 &= 0x7FFF;
        work->obj2.flags                 &= 0x3DFF;
        rot.vz                            = 0;
        rot.vy                            = 0;
        rot.vx                            = 0;
        RotMatrix(&rot, &coord->coord);
        coord->coord.t[2]                      = 0;
        coord->coord.t[1]                      = 0;
        coord->coord.t[0]                      = 0;
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        arg0->state     = 4;
        work->field_396 = 0;
    }
    return 0;
}

// actor_207000 carries the same body at 0x8014F8D8, but it cannot be shared:
// the effect offset the two collapse arms spawn with is this overlay's own
// vector (`D_actor_207000_801575D8` at the twin's address) and the only
// overlay-local name the body has. `overlay_dup_index.py promote` refuses it
// for that reason; matching it once needs the vector shared first.

void func_actor_107000_80137C8C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = D_actor_107000_80131E5C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
