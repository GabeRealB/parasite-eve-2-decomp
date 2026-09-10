#ifndef ACTORS_SHARED_8016A184_H
#define ACTORS_SHARED_8016A184_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Request animation 9 at speed 0x10, clear the frame counter and advance
/// `field_422`; while the enemy (`Task::spawnArg2`) still has HP, play sound
/// 0x402C0002 (bank from the enemy's `field_8` high nibble) panned and
/// attenuated from the model root. Shared by `actor_341700` and
/// `actor_342400`, whose work blocks are the same `ActorsShared80168d3cWork`.
void ActorsShared8016a184(Task* arg0);

#endif
