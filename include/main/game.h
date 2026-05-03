#ifndef GAME_H
#define GAME_H

#include "common.h"

/// Size of the game heap.
#define G_HEAP_SIZE 0xFF80

// It seems that the scratch space is used by the game as a temporary arena.
// At least for the bottom part. The last word of the scratch points to the
// current head, i.e., the arena grows downward.
#define G_SCRATCH_HEAD         PSX_SCRATCH_ADDR(0x3FC)
#define GameResetScratchHead() *(void**)G_SCRATCH_HEAD = G_SCRATCH_HEAD

/// Pointer to the start of the game heap.
extern u8* GHeap;

/// Pointer to the auxilary heap.
extern u8* GAuxHeap;

/// Length in bytes of the heap pointed to by `GAuxHeap`.
extern size_t GAuxHeapSize;

extern size_t D_80068F88;

/// Pointer to the active auxilary heap.
extern u8* GActiveAuxHeap;

extern size_t D_80068F90;

/// Length in bytes of the heap pointed to by `GActiveAuxHeap`.
extern size_t GActiveAuxHeapSize;

/// Game entry point. Called by `main`.
void GameMain(void);

#endif
