#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/neo_ark_woodland_path.h"

#include <psyq/libgs.h>

/* The room calls the dispatcher with only the task, leaving a1-a3 holding
   whatever the caller had, so the declaration must stay unprototyped. */
s32 Gp_DispatchMsg();

extern Task* D_neo_ark_woodland_path_80181680;

/// Gameplay's effect ids for the two effects this room scatters, set by
/// `func_neo_ark_woodland_path_8017EA08` on its first run and passed to
/// `Gp_SpawnEff`. Gameplay clears both when its effect state is reset.
extern s32 D_80115738;
extern s32 D_8011574C;

/// Gameplay's LCG state, stepped as `state * 5 + 0x71357911`.
extern u32 Gp_LcgState;

/// The distance between `a` and `b`, spelled as a conditional subtraction.
#define ABS_DIFF(a, b) ((a) - (b) >= 0 ? (a) - (b) : (b) - (a))

/// Scatters effects around the slot-3 task's model while its root coordinate
/// is at a y of 0x12C or more (y grows downward) and no event is running.
/// Once per frame, the spawn chance is set from how far model parts 15 and 18
/// moved since the previous frame. Two effects are rolled at the model's x and
/// z with y fixed at 0xC8: effect `D_8011574C` against that chance, then effect
/// `D_80115738` against the chance less 0x20. The same function also sets the
/// room effect mode to 2 while the root y is below 0x11. On its first run it
/// stores the two effect ids and the starting part positions.
void func_neo_ark_woodland_path_8017EA08(Task* task)
{
    NeoArkWoodlandPathTrailObj* obj;
    Task*                       owner;
    GsCOORDINATE2*              root;
    GsCOORDINATE2*              part;
    GsCOORDINATE2               coord;
    s32                         i;

    obj   = task->spawnArg2;
    owner = gameGetPtrSlot(3);
    root  = ((TmdObject*)owner->extra)->coords;
    if (task->state == 0) {
        D_8011574C  = 0x60058;
        D_80115738  = 0x60187;
        task->state = 1;
        for (i = 0; i < 2; i++) {
            part                                   = &((TmdObject*)owner->extra)->coords[i * 3 + 15];
            D_neo_ark_woodland_path_80181684[i].vx = part->workm.t[0];
            D_neo_ark_woodland_path_80181684[i].vy = part->workm.t[1];
            D_neo_ark_woodland_path_80181684[i].vz = part->workm.t[2];
        }
    }
    Gp_State1C->roomEffectMode = (root->coord.t[1] < 0x11) * 2;
    if (Gp_State1C->eventState == 0 && root->coord.t[1] >= 0x12C) {
        for (i = 0; i < 2; i++) {
            part             = &((TmdObject*)owner->extra)->coords[i * 3 + 15];
            obj->chance      = ABS_DIFF(D_neo_ark_woodland_path_80181684[i].vx, part->workm.t[0]) + ABS_DIFF(D_neo_ark_woodland_path_80181684[i].vy, part->workm.t[1]) + ABS_DIFF(D_neo_ark_woodland_path_80181684[i].vz, part->workm.t[2]) + 0x20;
            coord.sub        = root->sub;
            coord.coord      = root->coord;
            coord.coord.t[0] = root->coord.t[0];
            coord.coord.t[1] = 0xC8;
            coord.coord.t[2] = root->coord.t[2];
            coord.flg        = 0;
            Gp_UpdateCoord(&coord);
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((s32)((Gp_LcgState >> 16) & 0x1FF) < obj->chance) {
                Gp_SpawnEff(D_8011574C, &coord, 0x40, 0);
            }
            obj->chance -= 0x20;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            if ((s32)((Gp_LcgState >> 16) & 0x1FF) < obj->chance) {
                Gp_SpawnEff(D_80115738, &coord, 0x1202180, 0);
            }
            D_neo_ark_woodland_path_80181684[i].vx = part->workm.t[0];
            D_neo_ark_woodland_path_80181684[i].vy = part->workm.t[1];
            D_neo_ark_woodland_path_80181684[i].vz = part->workm.t[2];
        }
    }
}
