#include "common.h"

#include "main/fs.h"

#include "gameplay/1BC.h"

/// Script callback: restores the stream random state, then cancels the
/// pending overlay replacement and activates the loaded one.
void func_actor_342000_801644BC(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}
