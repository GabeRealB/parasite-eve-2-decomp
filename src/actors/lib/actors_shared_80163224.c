#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_80163224.h"

void ActorsShared80163224(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)arg0->extra;
    switch (arg2) {
        case 0:
            extra->field_C = (extra->field_C | 0x80) & 0xFFFB;
            return;
        case 1:
            extra->field_C = extra->field_C & 0xFF7B;
            return;
        case 2:
            extra->field_C = extra->field_C | 0x84;
            return;
    }
}
