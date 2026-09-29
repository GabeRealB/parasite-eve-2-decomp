/* Shop inventory, purchase dialogs, previews and refill animation.
 *
 * Every function is static. The including overlay defines its own session
 * descriptor pointing to Shop_SessionTask and declares that descriptor in its
 * overlay header. The descriptor's storage wrapper, when present, stays there.
 * Refill work belongs to its existing overlay TU; another TU imports it through
 * the overlay's private header. No linkage or symbol-name configuration is used.
 * SHOP_CHARGE_TITLE_BYTES preserves all eight title bytes, including the byte
 * after the terminator. Visible text and behaviour are shared.
 *
 * Include this header in the prologue, shop_data.inc.c and shop_panels.inc.c
 * at their data positions, and shop.inc.c at the function run.
 */

#ifndef SRC_SHARED_SHOP_H
#define SRC_SHARED_SHOP_H

#include "main/task_types.h"

/* Used by each overlay's own session descriptor. */
static void Shop_SessionTask(Task* task);

#endif /* SRC_SHARED_SHOP_H */
