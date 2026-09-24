#include "common.h"

#include "actors/actor_107000.h"
#include "main/task.h"
#include "main/tmd.h"

/// Gives `dst`'s model the texture page and CLUT of `src`'s, re-streaming it
/// twice when it has a buffer.
void Actor07000_Fn066FC(Task* dst, Task* src)
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
