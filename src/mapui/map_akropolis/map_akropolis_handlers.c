#include "common.h"

/// The key-item panel body: a switch whose jump table sits at 0x4 and whose
/// cases run to 0x300, all converging on a shared tail that returns. It links
/// with the rodata rather than the text (`linker_section_order`), so the table
/// and the code have to stay in one unit - splitting them left the jtbl
/// undefined.
///
/// Order matters: the object's `.rodata` is laid out in include order, and the
/// jump table rides in the function's own `.s`. The header word, then the
/// function, then the "Key Item" string is what reproduces
/// `[header][jtbl][string]`; putting both rodata blocks first swaps the last
/// two.
INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis_handlers", D_map_akropolis_80179950);

INCLUDE_ASM("mapui/nonmatchings/map_akropolis/map_akropolis_handlers", func_map_akropolis_80179988);

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis_handlers", D_map_akropolis_8017997C);
