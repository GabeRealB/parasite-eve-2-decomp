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

#ifndef SHOP_REFILL_DISPLAY_QUANTITY
/// Selects the signed 32-bit accumulator for the refill panel's animated supply quantity.
///
/// Expand to a single identifier naming a writable s32 object. The callback
/// accesses it repeatedly; it must select the same storage without side effects.
/// Each selected supply seeds it from the old whole-unit weapon load, then
/// converts it to 256 units per supply unit. Drawing adds 64 per callback and
/// clamps at the selected load's capacity in that scale. The number discards
/// the fractional bits; the meter uses the scaled quantity and capacity.
///
/// Storage is shared by this carrier's refill callbacks, so only one refill
/// panel may use it at a time. Keep it alive through panel closure, paired with
/// `SHOP_REFILL_SUPPLY`; the callback initializes it for each selected supply.
/// The default names TU-private storage declared by the carrier. Mist Parking,
/// night trailer coach, night garage and Shelter B1 armory override it with
/// storage defined in another room TU and declared in their private header.
/// Bind before this header, retain through shop.inc.c, then undefine it.
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
