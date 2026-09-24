#include "common.h"

/// One byte of gameplay state, written by room script tables and read back with
/// `lb` by the field text actor (`actor_101600_text`), so it is signed.
extern s8 D_8011540C;

/// Script callback the room's script table names: stores its argument in
/// `D_8011540C`.
void func_dryfield_night_parking_lot_8017DBA4(s32 arg0)
{
    D_8011540C = arg0;
}
