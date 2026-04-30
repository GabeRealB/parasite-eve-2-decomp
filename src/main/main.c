#include "common.h"

#include <psyq/libapi.h>

#include "main/game.h"

// For some reason, the program starts by modifying its stack pointer and
// calling the actual entry function of the game. This address does not
// seem to be inserted by the linker.
static u32 GStackBase = 0x801fff00;

int main(void)
{
    // Modify the stack pointer.
    SetSp(GStackBase);

    // Call the entry function.
    GameMain();
}
