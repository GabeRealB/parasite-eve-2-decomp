#include "common.h"

/// One byte of gameplay state, shared with the field actors. Read back with
/// `lb` (`acropolis_fire_escape_5`), so it is signed.
extern s8 D_8007272D;

/// Room script callback: latch this room's script argument into `D_8007272D`.
void Room_Util35(s8 arg0)
{
    D_8007272D = arg0;
}
