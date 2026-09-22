#include "common.h"
#include "actors/actor_201200.h"

void Actor01200_Fn00820(Actor201200* arg0);

/// State 1: on entry (`field_4` set) clear the actor and model flags, set
/// `field_174` to 4, and set or clear the high bits of the four sub-object
/// flags; afterwards run `Actor01200_Fn00820` and move to state 2 once bit 0
/// of `field_58` is set.
void Actor01200_Fn03DC0(Actor201200Ctx* arg0, Actor201200* arg1)
{
    Actor201200Work* work;
    TmdObject*       obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                 = arg1->field_2C;
        arg0->field_14      = 0;
        obj->flags          = 0;
        work->field_174     = 4;
        work->field_170     = 1;
        work->field_178     = 0;
        work->obj2C8.flags |= 0x8000;
        work->obj300.flags &= 0x7FFF;
        work->obj338.flags &= 0x7FFF;
        work->obj230.flags |= 0x4000;
        Actor01200_Fn00820(arg1);
        return;
    }
    Actor01200_Fn00820(arg1);
    if (work->field_58 & 1) {
        work->field_0 = 2;
    }
}

/// State 3: on entry (`field_4` set) clear the actor and model flags, set
/// `field_174` to 6, and set or clear the high bits of the four sub-object
/// flags; afterwards run `Actor01200_Fn00820` and move to state 7 once bit 0
/// of `field_58` is set.
void Actor01200_Fn03E78(Actor201200Ctx* arg0, Actor201200* arg1)
{
    Actor201200Work* work;
    TmdObject*       obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                 = arg1->field_2C;
        arg0->field_14      = 0;
        obj->flags          = 0;
        work->field_174     = 6;
        work->field_170     = 1;
        work->field_178     = 0;
        work->obj2C8.flags |= 0x8000;
        work->obj300.flags &= 0x7FFF;
        work->obj338.flags &= 0x7FFF;
        work->obj230.flags |= 0x4000;
        Actor01200_Fn00820(arg1);
        return;
    }
    Actor01200_Fn00820(arg1);
    if (work->field_58 & 1) {
        work->field_0 = 7;
    }
}

/// On entry (`field_4` set) clear the actor and model flags, set `field_174`
/// to 2, and set or clear the high bits of the four sub-object flags; then run
/// `Actor01200_Fn00820` and clear the model's coordinate flag every frame.
void Actor01200_Fn03F30(Actor201200Ctx* arg0, Actor201200* arg1)
{
    Actor201200Work* work;
    TmdObject*       obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                 = arg1->field_2C;
        arg0->field_14      = 0;
        obj->flags          = 0;
        work->field_174     = 2;
        work->field_170     = 1;
        work->field_178     = 0;
        work->obj2C8.flags |= 0x8000;
        work->obj300.flags &= 0x7FFF;
        work->obj338.flags &= 0x7FFF;
        work->obj230.flags |= 0x4000;
    }
    Actor01200_Fn00820(arg1);
    arg1->field_2C->coords->flg = 0;
}
