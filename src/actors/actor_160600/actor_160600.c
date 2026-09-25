#include "common.h"

#include <psyq/rand.h>

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_160600.h"

extern u8 D_actor_160600_8013DFEC[];

void func_actor_160600_80131E24(void)
{
    if (gameGetPtrSlot(0xA) != NULL) {
        Task_CallExit(gameGetPtrSlot(0xA));
        Game_SetPtrSlot(NULL, 0xA);
    }
}

/// The actor's per-frame body (task state 1): refreshes the root coordinate,
/// re-lights the model at the root translation raised by 800, then runs the
/// step body and draws the ground shadow. While `field_4EE` is set and the object is live with an aux buffer,
/// every other frame spawns effect 0x60070 on a randomly chosen part, with
/// two `Gp_LcgState` draws packed into the effect argument.
void func_actor_160600_80131E68(GpEnemy* enemy, Task* task)
{
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   part;
    Actor160600Work* work;
    VECTOR           pos;
    u32              low;
    u32              high;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    part  = &((TmdObject*)task->extra)->coords[D_actor_160600_8013DFEC[(rand() * 11) >> 15]];
    work  = (Actor160600Work*)task->work;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 800;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_160600_80131FFC(task);
    func_actor_160600_80132378(task);
    if (work->field_4EE != 0 && !(obj->flags & 0x80) && obj->buffer != NULL) {
        if (task->killCountdown & 1) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            low         = (Gp_LcgState >> 16) & 0x10FF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            high        = (((Gp_LcgState >> 16) & 1) << 30) + 0x800231C0;
            Gp_SpawnEff(0x60070, part, low + high, NULL);
        }
        task->killCountdown++;
    }
}
