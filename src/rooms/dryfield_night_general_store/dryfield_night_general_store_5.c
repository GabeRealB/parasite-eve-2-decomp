#include "common.h"

#include <psyq/libgte.h>

#include "main/session.h"
#include "rooms/room_common.h"

#include "rooms/dryfield_night_general_store.h"

extern SVECTOR D_dryfield_night_general_store_8017E7EC[];
extern SVECTOR D_dryfield_night_general_store_8017E7FC;
extern SVECTOR D_dryfield_night_general_store_8017E80C;
extern SVECTOR D_dryfield_night_general_store_8017E81C;

/// The shop's four lit wedges, one 8-byte `SVECTOR` pair apiece, laid out along
/// the room's depth from `E7EC`: the pair `func_dryfield_night_general_store_8017DEE0` draws is the one whose
/// ends the view index `gGameSession->at4.loc.view` selects. `E7FC`, `E80C` and
/// `E81C` are named separately because their views reach them by name (each
/// emits its own `lui`), where views 4 and 8 reach the second half of `E7EC` as
/// `E7EC[2]` - indexing emits the base plus 0x10. Both of those views write the
/// same two draws out again rather than sharing one case label, which the jump
/// table shows as its two distinct entries.
void func_dryfield_night_general_store_8017E6C8(void)
{
    switch (gGameSession->at4.loc.view) {
        case 2:
            func_dryfield_night_general_store_8017DEE0(&D_dryfield_night_general_store_8017E7EC[0], 0x100);
            break;
        case 3:
            func_dryfield_night_general_store_8017DEE0(&D_dryfield_night_general_store_8017E7FC, 0x100);
            break;
        case 4:
            func_dryfield_night_general_store_8017DEE0(&D_dryfield_night_general_store_8017E7EC[0], 0x100);
            func_dryfield_night_general_store_8017DEE0(&D_dryfield_night_general_store_8017E7EC[2], 0x100);
            break;
        case 8:
            func_dryfield_night_general_store_8017DEE0(&D_dryfield_night_general_store_8017E7EC[0], 0x100);
            func_dryfield_night_general_store_8017DEE0(&D_dryfield_night_general_store_8017E7EC[2], 0x100);
            break;
        case 12:
            func_dryfield_night_general_store_8017DEE0(&D_dryfield_night_general_store_8017E80C, 0x100);
            break;
        case 13:
            func_dryfield_night_general_store_8017DEE0(&D_dryfield_night_general_store_8017E81C, 0x100);
            break;
    }
}
