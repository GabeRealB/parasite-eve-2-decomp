#include "common.h"

#include "rooms/rooms_shared_8017f438.h"

void RoomsShared8017f438(s16 arg0)
{
    if (arg0 == 0) {
        RoomsShared8017f438Sel = RoomsShared8017f438Bank0;
        return;
    }
    RoomsShared8017f438Sel = RoomsShared8017f438Bank1;
}
