/* The item catalog: every item's catalog text, then gItemDescriptors and gItemKeyDescriptors, written from
 * inventory/catalog (tools/peassets/shape/item.toml).
 *
 * An object of its own in the original build, and data only: its .rodata is the texts and its .data the two
 * tables. It looks generated: its .rodata ends in a junk byte past the last text, a different value in every
 * release (0x97 US, 0xDF Europe, 0xA6 Japan) where every other gap is zero - the object's own, since the link
 * starts each object's section on a word - which the include writes last (the catalog's `junk`). */
#include "types.h"

#include "gameplay/inventory.h"
#include "gameplay/items.h"

#include "assets/inventory/catalog.inc"
