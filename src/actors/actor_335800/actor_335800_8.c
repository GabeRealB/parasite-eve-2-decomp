#include "common.h"

#include "gameplay/1BC.h"

/// Script callback: restores the stream random state.
void func_actor_335800_80162080(void)
{
    Gp_RestoreStreamRng();
}
