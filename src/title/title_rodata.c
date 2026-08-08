#include "common.h"

#include "main/title.h"

/* Package header / demo strings for the title overlay (load @ 0x80093800).
 *
 * Kept in its own TU so switch jtables in title.c are the first symbols in
 * that object’s .rodata — GCC’s `.align 3` is then a no-op at offset 0, and
 * tables link at 0x5C without a ninja sed rewrite.
 *
 * Title_Padding is the retail halfword after DemoCardRestore’s NUL (not zero).
 */
const u32            Title_Header     = 5;
const TaskFuncTable5 Title_PhaseTable = {
    .funcs = {
        Title_InitTask,
        Title_FlagAdvanceTask,
        Title_MenuTask,
        Title_MenuTask,
        Task_Kill,
    },
};
const char Title_DemoStartMsg[]       = "##########DEMO START\n";
const char Title_DemoCardRestoreMsg[] = "####DEMO_CARD_RESTORE STAGE %d, SCENE %d\n";
const u16  Title_Padding              = 0xE122;
