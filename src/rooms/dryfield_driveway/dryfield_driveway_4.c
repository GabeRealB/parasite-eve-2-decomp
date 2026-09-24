#include "common.h"

#include "main/session.h"

/// Script callback: stores its argument in the session's `viewDirty` flag.
void func_dryfield_driveway_8017DC54(s16 arg0)
{
    gGameSession->viewDirty = arg0;
}
