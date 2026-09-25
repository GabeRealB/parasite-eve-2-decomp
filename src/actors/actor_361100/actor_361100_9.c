#include "common.h"

#include "main/session.h"

/// Record handler (opcode 0x0D) of the actor's script data, taking the
/// record's argument word: ORs it into `GameSession::flowFlags` (the script
/// passes 1 and 2).
void func_actor_361100_80162AEC(s32 bits)
{
    gGameSession->flowFlags |= bits;
}
