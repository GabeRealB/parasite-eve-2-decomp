#include "common.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

/* `D_800678F0` selects the model stream the next `Gp_SpawnEff` copies into
 * its effect's `TmdObject`. It is declared as a one-element array for the
 * same reason as in `actor_400500`: as a bare scalar, GCC 2.8.1 decides the
 * store cannot alias the `TmdObject` loads and sinks it past them. */
extern void* D_800678F0[1];

/* Model streams in the overlay's own `.data`, selected through `D_800678F0`. */
extern u8 D_actor_341700_8016DE70[];
extern u8 D_actor_341700_8016E514[];
extern u8 D_actor_341700_8016EA84[];

void func_actor_341700_801624F8(Task* arg0)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* src;
    TmdObject* src2;

    D_800678F0[0] = D_actor_341700_8016DE70;
    eff           = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[6], 0x200, NULL);
    if (eff != NULL) {
        src        = (TmdObject*)arg0->extra;
        dst        = (TmdObject*)eff->task->extra;
        dst->tpage = src->tpage;
        dst->clut  = src->clut;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if ((Gp_LcgState >> 16) & 1) {
        D_800678F0[0] = D_actor_341700_8016E514;
        eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[8], 0x200, NULL);
    } else {
        D_800678F0[0] = D_actor_341700_8016EA84;
        eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[2], 0x200, NULL);
    }
    if (eff2 != NULL) {
        src2        = (TmdObject*)arg0->extra;
        dst2        = (TmdObject*)eff2->task->extra;
        dst2->tpage = src2->tpage;
        dst2->clut  = src2->clut;
        if (dst2->buffer != NULL) {
            tmdProcessStream(dst2);
            tmdProcessStream(dst2);
        }
    }
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[3], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[4], 0x200, NULL);
}
INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161E24);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161E3C);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161E64);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161E90);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161E9C);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161EA8);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161EBC);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161ED0);
