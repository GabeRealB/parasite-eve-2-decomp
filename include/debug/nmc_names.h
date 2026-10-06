#ifndef INCLUDE_DEBUG_NMC_NAMES_H
#define INCLUDE_DEBUG_NMC_NAMES_H

/// Enemy names in Shift-JIS, one per enemy kind, after an empty first entry.
/// `itemGetText` returns entry `id - 0x500` for an id of 0x500 or more, so
/// those ids name enemies while this package is loaded.
extern char* Gp_ItemTextHi[];

#endif // INCLUDE_DEBUG_NMC_NAMES_H
