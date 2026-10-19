/* Shop inventory, purchase dialogs, previews and refill animation.
 *
 * Every function is static. The including overlay defines its own session
 * descriptor pointing to _shopSessionTask and declares that descriptor in its
 * overlay header. The descriptor's storage wrapper, when present, stays there.
 * Refill work belongs to its existing overlay TU; another TU imports it through
 * the overlay's private header. Refill-state bindings select the carrier-owned lvalues.
 * Each carrier binds SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES only around
 * shop.inc.c. It supplies a one-byte string literal retained after the common
 * refill title's NUL terminator; label drawing does not read that byte.
 *
 * Include this header in the prologue, shop_data.inc.c and shop_data.inc.c
 * at their data positions, and shop.inc.c at the function run.
 */

#ifndef SRC_SHARED_SHOP_H
#define SRC_SHARED_SHOP_H

#include "main/task_types.h"

/// Special row ids in stock lists; the terminator is never offered as a row.
enum {
    SHOP_ROW_RECHARGE_SERVICE = 0xFFFE, // Batteries/Fuel service, outside the item catalog.
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

#ifndef SHOP_REFILL_SUPPLY
/// Selects the borrowed weapon supply catalog row displayed by the refill panel.
///
/// Expand to a single identifier naming a writable `const EquipmentWeaponSupply*`
/// object. Repeated assignments and reads must select the same storage without
/// side effects. The callback replaces the pointer and only reads the row's
/// weapon item id, supply item id and primary/secondary load selector.
///
/// The panel must start with at least one carried rechargeable weapon: the first
/// selection initializes the pointer before drawing, with no `NULL` check. Each
/// subsequent selection replaces it; exhaustion retains the last row so drawing
/// continues through panel closure. Keep the pointer storage and gameplay
/// catalog alive until closure finishes; the panel never copies or frees a row.
/// Pair the storage with `SHOP_REFILL_DISPLAY_QUANTITY` in the same carrier and
/// allow only one refill panel to use that pair at a time.
///
/// The default names TU-private storage declared by the carrier. Mist Parking,
/// night trailer coach, night garage and Shelter B1 armory instead bind storage
/// defined in another room TU and declared in their private header. Bind before
/// this header, retain through `shop.inc.c`, then undefine it.
#define SHOP_REFILL_SUPPLY _gShopRefillSupply
#endif

/// Empty preview selection; readable by data initializers and preview consumers.
enum { SHOP_PREVIEW_NONE = -1 };

/* Used by each overlay's own session descriptor. */
static void _shopSessionTask(Task* task);

#endif /* SRC_SHARED_SHOP_H */
