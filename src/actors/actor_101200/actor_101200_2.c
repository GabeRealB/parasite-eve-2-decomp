#include "common.h"

#include "actors/actor_101200.h"

INCLUDE_ASM("actors/nonmatchings/actor_101200/actor_101200_2", func_actor_101200_80135B78);

/// Restarts the actor once its work block reports ready: clears the caller's
/// flag and the display object's visibility, asks for animation 4 in motion
/// state 1, and flips the record flags the run needs. Otherwise it advances
/// the shared per-frame routine and, once bit 0 of `field_58` is set, moves
/// the actor to state 2.
void func_actor_101200_80135BE0(Actor101200Ctx* arg0, Actor101200* arg1)
{
    Actor101200Work*  work;
    Actor101200Obj2C* obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj              = arg1->field_2C;
        arg0->field_14   = 0;
        obj->field_C     = 0;
        work->field_174  = 4;
        work->field_170  = 1;
        work->field_178  = 0;
        work->field_2E6 |= 0x8000;
        work->field_31E &= 0x7FFF;
        work->field_356 &= 0x7FFF;
        work->field_24E |= 0x4000;
        func_actor_101200_80132640(arg1);
    } else {
        func_actor_101200_80132640(arg1);
        if (work->field_58 & 1) {
            work->field_0 = 2;
        }
    }
}

/// Restarts the actor once its work block reports ready: clears the caller's
/// flag and the display object's visibility, asks for animation 6 in motion
/// state 1, and flips the record flags the run needs. Otherwise it advances
/// the shared per-frame routine and, once bit 0 of `field_58` is set, moves
/// the actor to state 7.
void func_actor_101200_80135C98(Actor101200Ctx* arg0, Actor101200* arg1)
{
    Actor101200Work*  work;
    Actor101200Obj2C* obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj              = arg1->field_2C;
        arg0->field_14   = 0;
        obj->field_C     = 0;
        work->field_174  = 6;
        work->field_170  = 1;
        work->field_178  = 0;
        work->field_2E6 |= 0x8000;
        work->field_31E &= 0x7FFF;
        work->field_356 &= 0x7FFF;
        work->field_24E |= 0x4000;
        func_actor_101200_80132640(arg1);
    } else {
        func_actor_101200_80132640(arg1);
        if (work->field_58 & 1) {
            work->field_0 = 7;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_101200/actor_101200_2", func_actor_101200_80135D50);
