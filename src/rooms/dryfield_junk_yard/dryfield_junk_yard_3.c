#include "common.h"

#include "main/session.h"
#include "rooms/dryfield_junk_yard.h"

extern s16 D_8007107A;

void func_dryfield_junk_yard_8017DC60(void)
{
    if ((D_8007107A != 0) && (gameGetPtrSlot(0xA) != 0)) {
        func_80724608(gameGetPtrSlot(0xA), -0x8C, 0xA, &D_dryfield_junk_yard_8017D5D0);
    }
}
