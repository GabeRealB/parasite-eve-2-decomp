#ifndef INCLUDE_DEBUG_NMC_NAMES_H
#define INCLUDE_DEBUG_NMC_NAMES_H

/// Enemy-name slot count, including the empty first entry.
enum { NMC_NAMES_ENEMY_NAME_COUNT = 63 };

/// NUL-terminated Shift-JIS enemy labels for text ids 0x500..0x53E.
///
/// Slot `id - 0x500` is in 0..62; slot 0 is empty. Both the pointer slots
/// and their strings are read-only. The table and borrowed labels remain
/// readable only while the `nmc_names` overlay (file 20600) is loaded.
/// The lookup performs no bounds or load-state check.
extern const char* const gNmcNamesEnemyNameTable[NMC_NAMES_ENEMY_NAME_COUNT];

#endif // INCLUDE_DEBUG_NMC_NAMES_H
