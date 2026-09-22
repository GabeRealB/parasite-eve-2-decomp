#include "common.h"

#include "actors/actors_shared_80132d00.h"

#include "main/session.h"

void ActorsShared80132d00(s32 bits)
{
    gGameSession->flowFlags |= bits;
}
