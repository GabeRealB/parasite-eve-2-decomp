#include "common.h"

#include "actors/actors_shared_80136614.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "psyq/inline_c.h"

/// `base` and `vec` hold the same address on purpose: the sibling at
/// `func_actor_403100_8013B5E0` matches the identical `addiu`/`addu` pair the
/// same way, with an explicit `move`, and the head-relative spelling of the
/// vector copy and of the reloaded x keeps the scratch pointer in `head`'s
/// register. `COMPILER_BARRIER` is load-bearing: without it cse forwards the
/// reload of x from the store just above it and the block compiles to a shift
/// pair instead of the load.
///
/// Carried by three slots - `actor_107000`, `actor_207000` and `actor_207200` -
/// so the body takes the coordinate rather than any carrier's own context type;
/// the shared span is in `configs/USA/overlays.toml`.
s32 ActorsShared80136614(GsCOORDINATE2* arg0, u32* arg1)
{
    SVECTOR        local;
    GsCOORDINATE2* coord;
    s32            angle;
    s32            x;
    s16            z;
    void*          base;
    void*          head;
    void*          vec;
    void*          matrix;

    coord = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->coords;
    head  = *(void**)0x1F8003FC;
    base  = head - 0x40;
    __asm__("move %0,%1" : "=r"(vec) : "r"(base));
    *(s16*)((s8*)base + 0) = (s16)(coord->workm.t[0] - arg0->workm.t[0]);
    *(s16*)((s8*)vec + 2)  = (s16)(coord->workm.t[1] - arg0->workm.t[1]);
    *(void**)0x1F8003FC    = vec;
    *(s16*)((s8*)vec + 4)  = (s16)(coord->workm.t[2] - arg0->workm.t[2]);
    matrix                 = head - 0x20;
    TransposeMatrix(&arg0->workm, matrix);
    local = *(SVECTOR*)((s8*)head - 0x40);
    gte_SetRotMatrix(matrix);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stsv(vec);
    angle = ratan2(*(s16*)((s8*)head - 0x40), *(s16*)((s8*)vec + 4));
    if (angle >= 0x801) {
        angle -= 0x1000;
    } else if (angle < -0x800) {
        angle += 0x1000;
    }
    *(s16*)((s8*)vec + 0) = (s16)(coord->coord.t[0] - arg0->coord.t[0]);
    COMPILER_BARRIER();
    x                     = *(s16*)((s8*)vec + 0);
    z                     = coord->coord.t[2] - arg0->coord.t[2];
    *(s16*)((s8*)vec + 4) = z;
    *arg1                 = SquareRoot0((x * x) + (z * z));
    *(void**)0x1F8003FC  += 0x40;
    return angle;
}
