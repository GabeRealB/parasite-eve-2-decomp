#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/rand.h>

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_night_water_tank.h"

/// The tank's wobble spring: `8017EE40` is the accumulated yaw handed to
/// `Gfx_RotMatrixY` (`>> 8`), `8017EE44` its velocity, `8017EE48` the yaw it
/// steps toward and `8017EE4C` the target that step chases.
extern s32 D_dryfield_night_water_tank_8017EE40;
extern s32 D_dryfield_night_water_tank_8017EE44;
extern s32 D_dryfield_night_water_tank_8017EE48;
extern s32 D_dryfield_night_water_tank_8017EE4C;

/// Restores the room's layout lists from their template, then offsets the six
/// `field_8` coordinates by (0, 0, -0xC8) when `arg0` is non-zero.
void func_dryfield_night_water_tank_8017D9DC(s32 arg0)
{
    DryfieldNightWaterTankLayout* dst;
    DryfieldNightWaterTankLayout* src;
    DryfieldNightWaterTankVec     d;
    s32                           i;

    dst = &D_dryfield_night_water_tank_8017F4B0;
    src = &D_dryfield_night_water_tank_8017E08C;

    for (i = 0; i < 2; i++) {
        dst->field_4[i].x = src->field_4[i].x;
        dst->field_4[i].y = src->field_4[i].y;
        dst->field_4[i].z = src->field_4[i].z;
        dst->field_C[i]   = src->field_C[i];
    }

    for (i = 0; i < 6; i++) {
        dst->field_8[i].x = src->field_8[i].x;
        dst->field_8[i].y = src->field_8[i].y;
        dst->field_8[i].z = src->field_8[i].z;
    }

    if (arg0 == 0) {
        d.x = 0;
        d.y = 0;
        d.z = 0;
    } else {
        d.x = 0;
        d.y = 0;
        d.z = -0xC8;
    }

    for (i = 0; i < 6; i++) {
        dst->field_8[i].x += d.x;
        dst->field_8[i].y += d.y;
        dst->field_8[i].z += d.z;
    }
}

/// Per-frame model update for the tank, the callback of the task entry 0 of
/// `D_dryfield_night_water_tank_8017EE28` describes (spawned by the room entry
/// task). State 0 parents the model's coordinate to `gGfxViewCoord` and places
/// it at (0xBB8, -0x34A8, -0x4D8), then advances to state 1. State 1 drives the
/// tank's slow wobble about `y`: an occasional roll re-picks the target yaw,
/// the step moves toward it 0x100 at a time, and the velocity follows 19/20 of
/// the way to that step. Every frame then sets the model's flags to 0x80 while
/// the view is 7 (0 otherwise), publishes the coordinate's `workm` translation
/// as a `VECTOR` to `func_800D7A9C`, rebuilds the coordinate's yaw matrix from
/// the accumulated angle, and clears `flg` so the world matrix is recomputed.
///
/// The coordinate's load is written through the cast expression, before the
/// object pointer is assigned: the pointer assignment has to stay a separate
/// register copy, or the overlay comes up an `addu` short.
void func_dryfield_night_water_tank_8017DB8C(Task* arg0)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord = ((TmdObject*)arg0->extra)->coords;
    obj   = (TmdObject*)arg0->extra;
    switch (arg0->state) {
        case 0:
            obj->flags        = 0;
            coord->sub        = &gGfxViewCoord;
            coord->coord.t[0] = 0xBB8;
            coord->coord.t[1] = -0x34A8;
            coord->coord.t[2] = -0x4D8;
            arg0->state++;
            break;
        case 1:
            if (((s32)(rand() * 100) >> 15) <= 0) {
                if (((s32)(rand() * 100) >> 15) < 0x50) {
                    D_dryfield_night_water_tank_8017EE4C = (s32)(rand() * 20) >> 7;
                } else {
                    D_dryfield_night_water_tank_8017EE4C = 0;
                }
            }
            if (D_dryfield_night_water_tank_8017EE48 < D_dryfield_night_water_tank_8017EE4C) {
                D_dryfield_night_water_tank_8017EE48 += 0x100;
            } else if (D_dryfield_night_water_tank_8017EE4C < D_dryfield_night_water_tank_8017EE48) {
                D_dryfield_night_water_tank_8017EE48 -= 0x100;
            }
            D_dryfield_night_water_tank_8017EE44 =
                (D_dryfield_night_water_tank_8017EE44 + D_dryfield_night_water_tank_8017EE48) * 19 / 20;
            D_dryfield_night_water_tank_8017EE40 += D_dryfield_night_water_tank_8017EE44;
            break;
    }
    if (gGameSession->at4.loc.view == 7) {
        obj->flags = 0x80;
    } else {
        obj->flags = 0;
    }
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    Gfx_RotMatrixY(&coord->coord, D_dryfield_night_water_tank_8017EE40 >> 8, 1);
    coord->flg = 0;
}

void func_dryfield_night_water_tank_8017DD8C(void)
{
}
