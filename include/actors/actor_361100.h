#ifndef INCLUDE_ACTORS_ACTOR_361100_H
#define INCLUDE_ACTORS_ACTOR_361100_H

#include "types.h"

/// Clears the retained scene head-aim task handle before starting the pod event.
///
/// actor_361100 must be loaded. Call before spawning its head-aim task or after
/// prior tasks have been torn down: this only clears the handle and does not kill
/// a live task or free its work. unused is ignored; the room passes 0.
void actor361100ClearHeadAimTaskHandle(s32 unused);

#endif // INCLUDE_ACTORS_ACTOR_361100_H
