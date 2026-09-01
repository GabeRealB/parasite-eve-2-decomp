#include "common.h"

/// One byte of gameplay state, read back with `lb` by the field actors
/// (`actor_102600` / `actor_202600` / `actor_302600`), so it is signed.
extern s8 D_8011540E;

/// Room script callback: raise this room's `D_8011540E` flag.
void Room_Util32(void)
{
    D_8011540E = 1;
}
