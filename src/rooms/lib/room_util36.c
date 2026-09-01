#include "common.h"

/// One byte of gameplay state, written by room script tables and read back with
/// `lb` by the field text actor (`actor_101600_text`), so it is signed.
extern s8 D_8011540C;

/// Room script callback: latch this room's script argument into `D_8011540C`.
void Room_Util36(s32 arg0)
{
    D_8011540C = arg0;
}
