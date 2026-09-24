#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/task.h"
#include "rooms/shelter_b3_garbage_incinerator.h"

extern TaskDesc D_shelter_b3_garbage_incinerator_8018719C;
extern s16      D_shelter_b3_garbage_incinerator_80187180;
extern s16      D_shelter_b3_garbage_incinerator_80187182;

/// Starts caption script `arg0` at line key `arg1` and spawns the caption
/// display task that shows it for `arg2` frames.
void func_shelter_b3_garbage_incinerator_80181038(s16 arg0, s16 arg1, s16 arg2)
{
    func_shelter_b3_garbage_incinerator_8017FD64(arg0, arg1, 0xD0);
    Display_InitModeObj(&D_shelter_b3_garbage_incinerator_8018719C, arg2, 0, 0);
}

/// Sets up the caption system: `arg0`, `arg1` are the VRAM position of the
/// caption glyph texture page, and the caption file relocated is the
/// `arg2`-th resident file slot of type 3.
void func_shelter_b3_garbage_incinerator_8018108C(s16 arg0, s16 arg1, s16 arg2)
{
    s32 count;
    s32 i;

    count                                     = 0;
    D_shelter_b3_garbage_incinerator_80187180 = arg0;
    D_shelter_b3_garbage_incinerator_80187182 = arg1;
    for (i = 0; i < 0x32; i++) {
        if (D_8006C338[i].field_0 == 3) {
            if (count == arg2) {
                func_shelter_b3_garbage_incinerator_8017FC5C((GpCapFile*)D_8006C338[i].field_4);
                break;
            }
            count++;
        }
    }
}
