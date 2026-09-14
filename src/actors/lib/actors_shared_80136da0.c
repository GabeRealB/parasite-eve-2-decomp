#include "common.h"

#include "actors/actors_shared_80136da0.h"

void ActorsShared80136da0(Task* task)
{
    ActorsShared80136da0Work* work;
    s32                       sel;
    s16                       state;

    work  = (ActorsShared80136da0Work*)task->idMap;
    state = work->field_6A8;
    switch (state) {
        case 0:
            /* The 32-bit local is load-bearing: an s16 one makes combine fold
             * the sign-extension into a second `lh` of field_6B8. */
            sel = work->field_6B8;
            if (sel == 1) {
                work->field_694 = 0x17;
                work->field_6A8 = sel;
                return;
            }
            work->field_694 = 0x1B;
            work->field_6A8 = 2;
            return;
        case 1:
            if (work->field_698 >= 0x10) {
                task->state     = 2;
                work->field_6A8 = 0;
            }
            return;
        case 2:
            if (work->field_698 >= 0x16) {
                task->state     = state;
                work->field_6A8 = 0;
            }
            return;
    }
}
