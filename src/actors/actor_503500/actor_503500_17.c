#include "common.h"

#include "main/session.h"

/// Record handler (opcode 0x0D) of the actor's script data, taking the
/// record's argument word: ORs it into `GameSession::flowFlags` (the script
/// passes 1 and 2).
void func_actor_503500_80132D00(s32 bits)
{
    gGameSession->flowFlags |= bits;
}
