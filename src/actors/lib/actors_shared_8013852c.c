#include "common.h"

#include "actors/actors_shared_8013852c.h"

/// Message 0x7D5 handler: switches the enemy's model and display nodes between
/// hidden and shown. `flags ^ 1` is the requested mode, latched in `field_BA0`
/// so only a change acts. Mode 1 hides the model and releases the enemy's link
/// node slot, saving its `field_4` first, and clears the 0xC000 pair off all
/// four display nodes; mode 0 puts the saved `field_4` back, lifts the hidden
/// bit, and sets those bits on the first and last display node.
///
/// The value register is pinned to `$v0` on purpose: local-alloc sorts a block
/// of exactly three quantities with a hand-rolled sort that undoes its own first
/// exchange, so the quantities are placed in birth order and the address would
/// take `$v0` ahead of the value it feeds. With `$v0` live over the value the
/// address lands in `$v1` instead, as the original does.
s32 ActorsShared8013852c(Task* task, s32 arg1, s32 flags)
{
    ActorShared8013852cWork* work;
    GpEnemy*                 enemy;
    TmdObject*               model;
    GpObj*                   obj;
    s32                      i;
    s32                      mode;

    mode  = flags ^ 1;
    work  = (ActorShared8013852cWork*)task->work;
    model = (TmdObject*)task->extra;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work->field_BA0 != mode) {
        work->field_BA0 = mode;
        if ((mode << 0x18) == 0) {
            model->flags        = (u16)(model->flags & 0xFF7F);
            enemy->node.field_4 = work->field_BA1;
            obj                 = &work->field_9A8[0];
            obj->flags          = (u16)(obj->flags | 0xC000);
            obj                 = &work->field_9A8[3];
            obj->flags          = (u16)(obj->flags | 0xC000);
        } else {
            register s32 value asm("v0");

            model->flags        = (u16)(model->flags | 0x80);
            work->field_BA1     = enemy->node.field_4;
            enemy->node.field_4 = 1;
            for (i = 0; i < 4; i++) {
                obj        = (GpObj*)((u8*)work + (OFFSET_OF(ActorShared8013852cWork, field_9A8) + i * 0x20));
                value      = obj->flags;
                value     &= 0x3FFF;
                obj->flags = (u16)value;
            }
        }
    }
    return 0;
}
