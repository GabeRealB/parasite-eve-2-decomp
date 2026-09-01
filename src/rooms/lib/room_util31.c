#include "common.h"

/// One byte of gameplay state, written by room script tables and read back with
/// `lb` by the field actors (`actor_103700` / `actor_203700`), so it is signed.
extern s8 D_8011540A;

/// Room script callback: latch this room's script argument into `D_8011540A`.
void Room_Util31(s32 arg0)
{
    D_8011540A = arg0;
}
