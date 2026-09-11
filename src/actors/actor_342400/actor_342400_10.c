#include "common.h"
#include "main/task.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_80168d3c.h"

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_10", func_actor_342400_801694A8);

/// While `field_41E` is 1, consumes the pending request in `field_448`:
/// requests 1..5 jump the state machine to states 6, 7, 8, 7 and 9 at
/// sub-state 0, anything else is just cleared. Returns 1 when `field_41E` is 1
/// and 0 otherwise. Each case reloads the work block through its own local;
/// one shared local lands in `$a0` instead of `$v1`.
s32 func_actor_342400_80169518(Task* arg0)
{
    ActorsShared80168d3cWork* work = (ActorsShared80168d3cWork*)arg0->idMap;

    if (work->field_41E == 1) {
        switch ((s16)(work->field_448 - 1)) {
            case 0: {
                ActorsShared80168d3cWork* w = (ActorsShared80168d3cWork*)arg0->idMap;
                w->field_420                = 6;
                w->field_422                = 0;
                break;
            }
            case 1: {
                ActorsShared80168d3cWork* w = (ActorsShared80168d3cWork*)arg0->idMap;
                w->field_420                = 7;
                w->field_422                = 0;
                break;
            }
            case 2: {
                ActorsShared80168d3cWork* w = (ActorsShared80168d3cWork*)arg0->idMap;
                w->field_420                = 8;
                w->field_422                = 0;
                break;
            }
            case 3: {
                ActorsShared80168d3cWork* w = (ActorsShared80168d3cWork*)arg0->idMap;
                w->field_420                = 7;
                w->field_422                = 0;
                break;
            }
            case 4: {
                ActorsShared80168d3cWork* w = (ActorsShared80168d3cWork*)arg0->idMap;
                w->field_420                = 9;
                w->field_422                = 0;
                break;
            }
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}

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
