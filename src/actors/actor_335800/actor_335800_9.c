#include "common.h"

#include "main/fs.h"

/// Script callback: cancels the pending overlay replacement and activates the
/// loaded one.
void func_actor_335800_801620A0(void)
{
    CdCmd_CancelReplaceAndActivate();
}
