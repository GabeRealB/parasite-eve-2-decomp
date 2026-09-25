#ifndef ACTORS_SHARED_801344AC_H
#define ACTORS_SHARED_801344AC_H

#include "common.h"

#include "actors/actor.h"
#include "main/task.h"

/// The spelling the rooms that carry a copy of the actors' fade task use for
/// `ActorFadeWork`.
typedef ActorFadeWork ActorShared801344acWork;

void ActorsShared801344ac(Task* arg0);

#endif
