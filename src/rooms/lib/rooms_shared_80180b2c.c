#include "common.h"

#include "main/task.h"
#include "rooms/rooms_shared_80180b2c.h"

void RoomsShared80180b2c(s16 arg0)
{
    RoomsShared80180b2cWork* work = (RoomsShared80180b2cWork*)RoomsShared80180b2cTask->idMap;

    work->field_4 = arg0;
    work->field_6 = 0;
}
