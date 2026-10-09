/* Shop inventory, purchase dialogs, previews and refill animation.
 *
 * Every function is static. The including overlay defines its own session
 * descriptor pointing to _shopSessionTask and declares that descriptor in its
 * overlay header. The descriptor's storage wrapper, when present, stays there.
 * Refill work belongs to its existing overlay TU; another TU imports it through
 * the overlay's private header. Refill-state bindings select the carrier-owned lvalues.
 * SHOP_CHARGE_TITLE_BYTES preserves all eight title bytes, including the byte
 * after the terminator. Visible text and behaviour are shared.
 *
 * Include this header in the prologue, shop_data.inc.c and shop_panels.inc.c
 * at their data positions, and shop.inc.c at the function run.
 */

#ifndef SRC_SHARED_SHOP_H
#define SRC_SHARED_SHOP_H

#include "main/task_types.h"

/// Special row ids in stock lists; the terminator is never offered as a row.
enum {
    SHOP_ROW_RECHARGE_SERVICE = 0xFFFE, // Batteries/Fuel service, outside the item catalogue.
    SHOP_ROW_END              = 0xFFFF  // End of the readable stock list.
};

/// Writable s32 lvalue holding the refill panel's displayed quantity, with eight fractional bits.
///
/// Defaults to TU-private storage. Mist Parking, night trailer coach, night
/// garage and Shelter B1 armory bind their externally linked room-local copy
/// before this header; keep the binding through shop.inc.c and undefine it after.
#ifndef SHOP_REFILL_DISPLAY_QUANTITY
#define SHOP_REFILL_DISPLAY_QUANTITY _gShopRefillDisplayQuantityFixed
#endif

/// Writable const EquipmentWeaponSupply* lvalue borrowing the currently displayed catalogue row.
///
/// Defaults to TU-private storage, selected before refill drawing and retained
/// through panel closure. The same four split-TU carriers bind their room-local
/// externally linked copy before this header and undefine it after shop.inc.c.
/// The gameplay catalogue must remain loaded throughout the shop session.
#ifndef SHOP_REFILL_SUPPLY
#define SHOP_REFILL_SUPPLY _gShopRefillSupply
#endif

/// Empty preview selection; readable by data initializers and preview consumers.
enum { SHOP_PREVIEW_NONE = -1 };

/* Used by each overlay's own session descriptor. */
static void _shopSessionTask(Task* task);

#endif /* SRC_SHARED_SHOP_H */
