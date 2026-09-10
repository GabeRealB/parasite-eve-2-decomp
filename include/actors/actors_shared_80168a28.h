#ifndef ACTORS_SHARED_80168A28_H
#define ACTORS_SHARED_80168A28_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Death handler: flags the work block, plays sound 0x402C0003 (bank from the
/// enemy's `field_8` high nibble) panned and attenuated from the model root
/// while the enemy (`Task::spawnArg2`) is not below zero HP, releases the
/// enemy's `Gp_StateF0` claims and list links, puts the task in state 5,
/// posts message 0x13F4 to slot 4 and sets bit 0x80 of the model's `field_C`.
/// Shared by `actor_341700` and `actor_342400`, whose work blocks are the
/// same `ActorsShared80168d3cWork`.
void ActorsShared80168a28(Task* arg0);

#endif
