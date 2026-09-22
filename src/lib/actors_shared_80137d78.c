#include "common.h"

#include "actors/actors_shared_80137d78.h"

void ActorsShared80137d78(ActorShared80137d78* arg0)
{
    ActorShared80137d78Work* work;
    s16                      state;
    s32                      next;

    work  = arg0->field_1C;
    state = work->field_6CE;
    switch (state) {
        case 0:
            next = work->field_6F0;
            if (next == 1) {
                work->field_6C0 = 0xE;
                work->field_6CE = next;
            } else {
                work->field_6C0 = 0x12;
                work->field_6CE = 2;
            }
            work->field_6DA = 1;
            work->field_6DC = 0xA;
            work->field_6DE = 5;
            break;
        case 1:
            if (work->field_6C4 >= 0x10) {
                arg0->field_30  = 2;
                work->field_6CE = 0;
            }
            break;
        case 2:
            if (work->field_6C4 >= 0x16) {
                arg0->field_30  = state;
                work->field_6CE = 0;
            }
            break;
    }
}
