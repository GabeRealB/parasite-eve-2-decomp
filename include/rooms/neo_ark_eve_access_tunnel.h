#ifndef ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_H
#define ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_H

#include "common.h"

/// Sets the tunnel's view flags: `arg0` picks the run to write (0 the single
/// view `field_1C` carries, 1 the `field_28` / `field_34` pair) and `arg1`
/// picks the value -- 0 sets each flag of that run to 1, 1 clears it to 0 --
/// while any other pair of arguments writes nothing.
/// `func_neo_ark_eve_access_tunnel_8017DFC0` calls it with (0, 0) and (1, 0)
/// together while the session is still below state 4, so both runs are set.
void func_neo_ark_eve_access_tunnel_8017E090(s32 arg0, s32 arg1);

#endif // ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_H
