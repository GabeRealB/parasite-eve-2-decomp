#include "common.h"

#include "actors/actors_shared_8013851c.h"

#include "main/tmd.h"

void ActorsShared8013851c(Task* dst, Task* src)
{
    TmdObject* to;
    TmdObject* from;

    from      = (TmdObject*)src->extra;
    to        = (TmdObject*)dst->extra;
    to->tpage = from->tpage;
    to->clut  = from->clut;
    if (to->buffer != NULL) {
        tmdProcessStream(to);
        tmdProcessStream(to);
    }
}
