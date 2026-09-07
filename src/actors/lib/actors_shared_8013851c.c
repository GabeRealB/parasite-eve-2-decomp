#include "common.h"

#include "actors/actors_shared_8013851c.h"

#include "main/tmd.h"

void ActorsShared8013851c(Task* dst, Task* src)
{
    TmdObject* to;
    TmdObject* from;

    from         = (TmdObject*)src->extra;
    to           = (TmdObject*)dst->extra;
    to->field_24 = from->field_24;
    to->field_25 = from->field_25;
    if (to->field_18 != NULL) {
        Tmd_ProcessStream(to);
        Tmd_ProcessStream(to);
    }
}
