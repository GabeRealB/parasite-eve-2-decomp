#include "common.h"

#include "gameplay/3FB8.h"

/// Room script callback: latch this room's script argument into `D_80115768`.
/// Same body as the field actors' `func_actor_161500_80132294` /
/// `func_actor_503500_80132EE8`.
void Room_Util28(u8 arg0)
{
    D_80115768 = arg0;
}
