#include "common.h"
#include "actors/actors_shared_80169dbc.h"
#include "main/task.h"
#include "actors/actors_shared_8016b104.h"
#include "actors/actors_shared_8016b1c8.h"
#include "actors/actors_shared_8016b21c.h"
#include "actors/actors_shared_8016b294.h"

extern u32 Gp_LcgState;

void ActorsShared80169dbc(Task* arg0)
{
    ActorsShared80169dbcWork* work;

    work            = (ActorsShared80169dbcWork*)arg0->work;
    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 1;
    work->field_414 = 1;
    /* Rolling the LCG through the global rather than an m2c temporary is what
     * hoists its `lw` above the field stores; see DECOMPILATION_LEARNINGS.md. */
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_446 = ((Gp_LcgState >> 16) & 0x3F) + 0x60;
    work->field_412 = 0;
    work->field_422 = work->field_422 + 1;
}

void ActorsShared8016b104(Task* arg0)
{
    ActorsShared80168d3cWork* work = (ActorsShared80168d3cWork*)arg0->work;
    s16                       dist;

    if (work->field_446 < (s16)work->field_412++) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            ActorsShared80168d3cWork* w = (ActorsShared80168d3cWork*)arg0->work;

            w->field_420 = 4;
            w->field_422 = 0;
        } else {
            ActorsShared80168d3cWork* w = (ActorsShared80168d3cWork*)arg0->work;

            w->field_420 = 1;
            w->field_422 = 0;
        }
        return;
    }
    dist = work->field_43A;
    if (dist < 0xDAC) {
        ActorsShared80168d3cWork* next = (ActorsShared80168d3cWork*)arg0->work;

        next->field_420 = 3;
        next->field_422 = 0;
        return;
    }
    if (dist < 0x1388) {
        work->field_422++;
    }
}

void ActorsShared8016b1c8(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    s32                       cond;

    work = (ActorsShared80168d3cWork*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = (ActorsShared80168d3cWork*)arg0->work;
        work->field_420 = 1;
        work->field_422 = 0;
    }
}

void ActorsShared8016b21c(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    s32                       cond;

    work = (ActorsShared80168d3cWork*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (ActorsShared80168d3cWork*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xD;
        work2->field_414 = 1;
        work->field_422++;
    }
}

void ActorsShared8016b294(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    s32                       cond;

    work = (ActorsShared80168d3cWork*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (ActorsShared80168d3cWork*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 0xE;
        work2->field_414 = 1;
        Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
        work->field_412  = 0;
        work->field_42C  = 0;
        work->field_446  = ((Gp_LcgState >> 16) & 0x3F) + 0xB0;
        work->field_422++;
    }
}
