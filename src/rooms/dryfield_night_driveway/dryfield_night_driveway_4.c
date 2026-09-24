#include "common.h"

/// A byte of gameplay state that the field actors read back signed; the room
/// sets it from its script.
extern s8 D_8011540A;

/// Script callback: stores its argument into `D_8011540A`.
void func_dryfield_night_driveway_8017DC6C(s32 arg0)
{
    D_8011540A = arg0;
}
