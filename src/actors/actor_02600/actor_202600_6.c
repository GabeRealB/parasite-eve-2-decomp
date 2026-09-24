#include "common.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"

#include "actors/actor_202600.h"

/* `D_80067704` is the third word of a `D_800676A8` record: it selects the model
 * stream the next `Gp_SpawnEff` uses for the effect's own `TmdObject`. Declared
 * as a one-element array so GCC 2.8.1 cannot treat the store as non-aliasing
 * with the struct traffic that follows and sink it past the loads. */
extern void* D_80067704[1];

/* Model stream in this overlay's own data. */
extern u8 Actor02600_D05F10[];

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Ground shadow under the actor's coordinate. With `field_39A` at 2 the
/// position is cast down to the ground by `func_800EA1A8` and the shade comes
/// from `func_800EA318`; otherwise the coordinate's own world translation is
/// used at full shade.
void Actor02600_Fn03AC0(Actor202600* arg0)
{
    Actor202600Work* work;
    GsCOORDINATE2*   coord;
    VECTOR3          vec;
    s16              hit;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    if (work->field_39A == 2) {
        hit = func_800EA1A8((VECTOR3*)coord->workm.t, &vec);
        if (hit != 0) {
            Gp_DrawEffGroundQuad(&vec, 0x200, func_800EA318(0x200, 0x80, hit));
        }
    } else {
        vec.vx = coord->workm.t[0];
        vec.vy = coord->workm.t[1];
        vec.vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(&vec, 0x200, 0x80);
    }
}
