#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

void Room_Util07(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}
