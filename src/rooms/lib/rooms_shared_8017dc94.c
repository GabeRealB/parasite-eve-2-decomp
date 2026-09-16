#include "common.h"

#include "gameplay/3CD8.h"

#include "rooms/rooms_shared_8017dc94.h"

s32 RoomsShared8017dc94(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 8:
            Gp_EnqueueStageSnd6(0x52190008, 0, 0);
            break;
        case 10:
            Gp_EnqueueStageSnd6(0x5219000A, 0, 0);
            break;
    }
    return 0;
}
