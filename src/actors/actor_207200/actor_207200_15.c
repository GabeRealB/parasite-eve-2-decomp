#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_207200.h"

/// Copies the texture page and CLUT from `src`'s model onto `dst`'s and, when
/// `dst` has a stream buffer, processes it twice so both halves pick the new
/// pair up. The enemy calls it with a freshly spawned effect as `dst` and
/// itself as `src`.
void func_actor_207200_8014DAF8(Task* dst, Task* src)
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
