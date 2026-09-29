#ifndef ACTOR_215100_PRIVATE_H
#define ACTOR_215100_PRIVATE_H

#include "main/task_types.h"

void func_actor_215100_80149F2C(Task* task);

// Retain the zero tail after the accessed value. Whether it was spare
// fields or alignment storage remains unresolved.
typedef struct {
    s32 value;
    u8 retained[4];
} Actor215100StorageE670;
STATIC_ASSERT_SIZEOF(Actor215100StorageE670, 8);

extern Actor215100StorageE670 D_actor_215100_8015E670;

// Callbacks referenced by the overlay's shared data tables.
void func_actor_215100_8014A5C0(Task *);
void func_actor_215100_8014A7C4(Task *);

#endif
