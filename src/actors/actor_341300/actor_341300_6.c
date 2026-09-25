#include "common.h"

#include "main/fs.h"

/// Record handler (opcode 0x0D) of the actor's script data: cancels the queued
/// CD command and restarts the CD queue.
void func_actor_341300_801623FC(void)
{
    CdCmd_CancelReplaceAndActivate();
}
