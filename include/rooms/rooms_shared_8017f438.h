#ifndef ROOMS_SHARED_8017F438_H
#define ROOMS_SHARED_8017F438_H

#include "common.h"

/// The room's active data bank pointer, and the two banks it chooses between.
/// Each carrying room holds all three at its own address, named there by the
/// family's symbol maps, so the shared object owns no data.
extern void* RoomsShared8017f438Sel;
extern u32   RoomsShared8017f438Bank0[];
extern u32   RoomsShared8017f438Bank1[];

/// Publishes one of the room's two data banks as the active one: bank 0 for a
/// zero argument, bank 1 otherwise. Two rooms carry this body.
void RoomsShared8017f438(s16 arg0);

#endif // ROOMS_SHARED_8017F438_H
