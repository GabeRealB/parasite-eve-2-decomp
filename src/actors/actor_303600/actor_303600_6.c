#include "common.h"

#include "gameplay/1BC.h"
#include "main/fs.h"

/// Opcode-0x0D callback in the actor's cutscene script: restores the stream
/// random-number state, then drops the pending replacement CD command through
/// `CdCmd_CancelReplaceAndActivate`.
void func_actor_303600_80162698(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}
