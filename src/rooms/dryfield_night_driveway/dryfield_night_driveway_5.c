#include "common.h"

#include "main/session.h"

/// Script callback: stores its argument into the session's `viewDirty`.
void func_dryfield_night_driveway_8017DC78(s16 arg0)
{
    gGameSession->viewDirty = arg0;
}
