#include "common.h"

#include "rooms/mine_tunnel.h"

/// One byte of gameplay state this room sets; its role is unproven here.
extern s8 D_8011540C;

/// Stores its argument in `D_8011540C`; the room's event task calls it with 2
/// on entry to the tunnel once flag 0xA1 is set.
void func_mine_tunnel_8017D6E0(s32 arg0)
{
    D_8011540C = arg0;
}
