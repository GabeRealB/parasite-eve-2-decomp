#include "common.h"
#include "main/task.h"
#include "gameplay/3A34.h"
#include "actors/actors_shared_80166b20.h"
#include "main/tmd.h"
#include "actors/actors_shared_80166c68.h"
#include "actors/actors_shared_8016945c.h"
#include "actors/actors_shared_80166dd4.h"
#include "actors/actors_shared_80166f54.h"

void ActorsShared80166b20(Task* arg0)
{
    ActorsShared80168d3cWork* work = (ActorsShared80168d3cWork*)arg0->work;
    ActorsShared80168d3cWork* state;
    ActorsShared80168d3cWork* state2;
    s32                       angle;
    s32                       cur;
    s32                       aim;

    if ((s16)++work->field_412 > work->field_446) {
        state            = (ActorsShared80168d3cWork*)arg0->work;
        state->field_420 = 2;
        state->field_422 = 0;
        return;
    }
    if (work->field_446 - 0x30 < (s16)work->field_412) {
        cur             = (u16)work->field_424;
        work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
        return;
    }
    if (work->field_43A < 0xDAC && (aim = (u16)work->field_444, (aim < 0x3C0 || aim > 0xC40))) {
        angle           = (u16)work->field_424;
        work->field_424 = angle + ((s16)((aim - angle) * 16) >> 6);
        if (++work->field_42C >= 0x10) {
            Gp_ArmStateF0(1);
            state2            = (ActorsShared80168d3cWork*)arg0->work;
            state2->field_420 = 3;
            state2->field_422 = 0;
        }
    } else {
        // Both arms are spelled out: the cross-jumped tail leaves each its own
        // load of `field_424`, which a single update after an if/else lacks.
        if (!(((u16)work->field_442 >> 6) & 1)) {
            work->field_424 = (u16)work->field_424 + ((s16)(0x3800 - (u16)work->field_424 * 16) >> 9);
        } else {
            work->field_424 = (u16)work->field_424 + ((s16)(-0x3800 - (u16)work->field_424 * 16) >> 9);
        }
    }
}

void ActorsShared80166c68(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    s32                       cond;
    s16                       angle;
    s16                       speed;
    s32                       scale;

    work = (ActorsShared80168d3cWork*)arg0->work;
    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        scale                                          = 0x1E;
        angle                                          = work->field_7A + 0x400;
        speed                                          = (((ActorsShared80168d3cWork*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->flg         = 0;
    }
    work2 = (ActorsShared80168d3cWork*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_438 = 0;
        ActorsShared_SetTaskState(arg0, 3);
        ActorsShared_SetWorkState(arg0, 3);
    }
}

void ActorsShared80166dd4(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    s32                       cond;
    s16                       angle;
    s16                       speed;
    s32                       scale;

    work = (ActorsShared80168d3cWork*)arg0->work;
    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        scale                                          = 0x1E;
        angle                                          = work->field_7A + 0x400;
        speed                                          = (((ActorsShared80168d3cWork*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->flg         = 0;
    }
    work2 = (ActorsShared80168d3cWork*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_438  = 0;
        work2            = (ActorsShared80168d3cWork*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 3;
        work2->field_414 = 1;
        work->field_412  = 0;
        work->field_422++;
    }
}

void ActorsShared80166f54(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    ActorsShared80168d3cWork* next;
    s32                       cond;
    s16                       angle;
    s16                       speed;
    s32                       scale;

    work = (ActorsShared80168d3cWork*)arg0->work;
    if ((ActorsShared8016945c(arg0) << 0x10) == 0) {
        if ((u16)(work->field_412++ - 0x17) < 0xD) {
            scale                                          = -0x1E;
            angle                                          = work->field_7A + 0x400;
            speed                                          = (((ActorsShared80168d3cWork*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
            ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
            ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
            ((TmdObject*)arg0->extra)->coords->flg         = 0;
        }
        work2 = (ActorsShared80168d3cWork*)arg0->work;
        if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_438 = 0;
            next            = (ActorsShared80168d3cWork*)arg0->work;
            next->field_420 = 0;
            next->field_422 = 0;
        }
    }
}
