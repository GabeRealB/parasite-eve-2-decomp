#include "common.h"

#include "actors/actors_shared_80169dbc.h"

extern s32 Gp_LcgState;

void ActorsShared80169dbc(Task* arg0)
{
    ActorsShared80169dbcWork* work;

    work            = (ActorsShared80169dbcWork*)arg0->idMap;
    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 1;
    work->field_414 = 1;
    /* Rolling the LCG through the global rather than an m2c temporary is what
     * hoists its `lw` above the field stores; see DECOMPILATION_LEARNINGS.md. */
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_446 = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
    work->field_412 = 0;
    work->field_422 = work->field_422 + 1;
}
