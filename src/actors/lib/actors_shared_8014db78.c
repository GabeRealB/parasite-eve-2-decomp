#include "common.h"

#include "actors/actor_101200.h"

/// Teardown state of the actor's state table: when the work block's `field_4`
/// latch is set it raises the caller's flag, sets the display object's
/// visibility bit and clears the gate bits on four records of the
/// 0x38-byte-stride flag table - the mirror of what the start-up half of the
/// state handlers sets there. Nothing happens while the latch is clear.
///
/// Shared: the three actor slots (`actor_101200` / `201200` / `301200`) carry
/// the same body byte for byte, so one object serves every overlay that lists
/// this unit in `configs/USA/overlays.toml`.
void ActorsShared8014db78(Actor101200Ctx* arg0, Actor101200* arg1)
{
    Actor101200Work*  work;
    Actor101200Obj2C* obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj             = arg1->field_2C;
        arg0->field_14  = 1;
        obj->field_C    = (u16)(obj->field_C | 0x80);
        work->field_2E6 = (u16)(work->field_2E6 & 0x7FFF);
        work->field_31E = (u16)(work->field_31E & 0x7FFF);
        work->field_356 = (u16)(work->field_356 & 0x7FFF);
        work->field_24E = (u16)(work->field_24E & 0xBFFF);
    }
}
