#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_100400.h"
#include "main/gfx.h"

/// Carries `pos` from `coord`'s space along the `sub` links, applying each
/// coordinate's matrix in turn, until the walk reaches the view coordinate
/// `gGfxViewCoord`; the result is written back to `pos` and 1 returned. A
/// chain that ends before reaching the view returns 0 and leaves `pos` as it
/// was.
s32 Actor00400_Fn0A08C(GsCOORDINATE2* coord, SVECTOR* pos)
{
    SVECTOR        local;
    VECTOR         result;
    s32            flag;
    GsCOORDINATE2* current;

    current  = coord;
    local.vx = pos->vx;
    local.vy = pos->vy;
    local.vz = pos->vz;
    while (1) {
        if (current->sub == NULL) {
            return 0;
        }
        if (current == &gGfxViewCoord) {
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            return 1;
        }
        gte_SetTransMatrix(&current->coord);
        gte_SetRotMatrix(&current->coord);
        gte_ldv0(&local);
        gte_rtv0tr();
        gte_stlvnl(&result);
        gte_stflg(&flag);
        local.vx = result.vx;
        local.vy = result.vy;
        local.vz = result.vz;
        current  = current->sub;
    }
}
