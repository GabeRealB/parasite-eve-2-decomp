#include "common.h"

extern s8 D_8011540A;

/// Script callback: stores its argument in the gameplay byte `D_8011540A`.
void func_dryfield_driveway_8017DC48(s32 arg0)
{
    D_8011540A = arg0;
}
