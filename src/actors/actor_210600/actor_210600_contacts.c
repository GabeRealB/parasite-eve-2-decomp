#include "../../shared/actor_contacts.h"

static SVECTOR ActorContact_ScratchPosition = { 0 };

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

/* Retained second contact run; its private scratch starts on an eight-byte boundary. */
#include "../../shared/actor_contacts.inc.c"
