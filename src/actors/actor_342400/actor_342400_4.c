#include "common.h"
#include "main/task.h"
#include "actors/actor_342400.h"

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_4", func_actor_342400_801694A8);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_4", func_actor_342400_80169518);

/// Message handler: on message 0x2C00 whose low nibble is 1..5, store the
/// message halfword in `field_44C`. The five identical case bodies are
/// cross-jumped into one, but only separate bodies keep the jump table; a
/// single `case 1 ... 5` becomes a range test.
void func_actor_342400_801695C0(Task* arg0, s32 arg1, Actor342400Msg* arg2)
{
    Actor342400Work* work = (Actor342400Work*)arg0->idMap;

    if (arg2->field_0 == 0x2C00) {
        switch (arg2->field_2 & 0xF) {
            case 1:
                work->field_44C = arg2->field_2;
                break;
            case 2:
                work->field_44C = arg2->field_2;
                break;
            case 3:
                work->field_44C = arg2->field_2;
                break;
            case 4:
                work->field_44C = arg2->field_2;
                break;
            case 5:
                work->field_44C = arg2->field_2;
                break;
        }
    }
}
