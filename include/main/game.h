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

// Type forwards declarations.
struct _GStruct0;

/// Callback function in a `_GStruct0`.
typedef void (*GFunc0)(struct _GStruct0*);

/// Intrusive linked list node for a `GStruct0`.
///
/// The head node is not an element in the linked list and points to the first
/// and the last elements.
typedef struct _GStruct0Node {
    struct _GStruct0*     next;
    struct _GStruct0Node* prev;
} GStruct0Node;

typedef struct _GStruct0 {
    GStruct0Node      node;
    struct _GStruct0* field_8;
    struct _GStruct0* field_c;
    struct _GStruct0* field_10;
    GFunc0            field_14;
    GFunc0            field_18;
    byte              unknown1[0xc];
    u8                field_28;
    byte              unknown2[0x1c];
} GStruct0;
STATIC_ASSERT_SIZEOF(GStruct0, 0x48);

typedef struct _GStruct1 {
    byte unknown_0[0x10b];
    u8   field_10b;
    byte unknown_10c[0x18];
    u16  field_124;
    byte unknown_12a[0x12];
} GStruct1;
STATIC_ASSERT_SIZEOF(GStruct1, 0x138);

typedef struct _GStruct2 {
    u16    field_0;
    byte   unknown_2[0x2];
    GFunc0 field_4;
    byte   unknown_8[0x4];
} GStruct2;
STATIC_ASSERT_SIZEOF(GStruct2, 0xc);

typedef struct _GStruct3 {
    byte unknown_0[0x254];
} GStruct3;
STATIC_ASSERT_SIZEOF(GStruct3, 0x254);

typedef struct _GStruct4 {
    u8 field_0;
    u8 field_1;
    u8 field_2;
    u8 field_3;
    u8 field_4;
    u8 field_5;
} GStruct4;
STATIC_ASSERT_SIZEOF(GStruct4, 0x6);

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

extern int      D_80068F98;
extern int      MainPadding;
extern GStruct3 D_80068FA0;
extern u8*      D_800691F4;
extern size_t   D_800691F8;

/// Game entry point. Called by `main`.
void GameMain(void);

#endif
