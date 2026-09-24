#include "common.h"

/// Record handler (opcode 0x0D) of the actor's script data: cancels the queued
/// CD command and restarts the CD queue.
void func_actor_503500_80132BD8(void)
{
    CdCmd_CancelReplaceAndActivate();
}
