#ifndef UNKNOWN_SYMS_H
#define UNKNOWN_SYMS_H

#include "common.h"

extern u32  func_8001F180(u32);
extern void func_8002043C(u32 arg0);

// TODO: These seem to be a pointer and size to a second auxilary heap.
// `Mem_SetActiveAuxHeap` switches between these and those in `GAuxHeap`.
// Could be that the game uses different heaps for each based on the kind
// of data that must be allocated.
extern u8*    D_800691F4;
extern size_t D_800691F8;

#endif // UNKNOWN_SYMS_H
