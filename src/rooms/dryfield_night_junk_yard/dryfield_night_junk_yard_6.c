#include "common.h"

#include "main/session.h"

extern u8 D_8007216D;

/// Stores `arg0` as the session's current room number and mirrors it into the
/// main-executable byte `D_8007216D`.
void func_dryfield_night_junk_yard_8017D894(u8 arg0)
{
    gGameSession->at4.loc.room = arg0;
    D_8007216D                 = arg0;
}
