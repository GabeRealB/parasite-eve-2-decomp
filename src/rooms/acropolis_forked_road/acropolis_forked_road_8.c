#include "common.h"

/// One byte of gameplay state that field actors read back with `lb`.
extern s8 D_8011540E;

/// Room script callback: sets `D_8011540E` to 1.
void func_acropolis_forked_road_8017E288(void)
{
    D_8011540E = 1;
}
