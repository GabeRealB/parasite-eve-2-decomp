#include "common.h"

#include "gameplay/3CD8.h"

#include "rooms/rooms_shared_8017dab4.h"

s32 RoomsShared8017dab4(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 9:
            Gp_EnqueueStageSnd6(0x520F0009, 0, 0);
            break;
        case 10:
            Gp_EnqueueStageSnd6(0x520F000A, 0, 0);
            break;
    }
    return 0;
}
