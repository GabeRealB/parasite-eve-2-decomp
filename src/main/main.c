#include "common.h"

#include <psyq/libapi.h>

#include "main/game.h"

// For some reason, the program starts by modifying its stack pointer and
// calling the actual entry function of the game. This address does not
// seem to be inserted by the linker.
u32 GStackBase = 0x801fff00;

// Apparently, it seems that the heap is not located on the heap, but rather
// after the main heap defined by the entry point.
u8* GHeap = (u8*)0x80083800;

u8*        GAuxHeap;
size_t     GAuxHeapSize;
size_t     D_80068F88;
u8*        GActiveAuxHeap;
size_t     D_80068F90;
size_t     GActiveAuxHeapSize;
int        D_80068F98;
static int MainPadding;
GStruct3   D_80068FA0;
u8*        D_800691F4;
size_t     D_800691F8;

int main(void)
{
    // Modify the stack pointer.
    SetSp(GStackBase);

    // Call the entry function.
    GameMain();
}
