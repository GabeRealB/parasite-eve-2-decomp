#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_night_factory.h"

extern void     Room_Util20(Task* task);
extern void     func_8004BFF8(s32 angle, MATRIX* matrix);
extern void     Room_Script16(Task* task);
extern TaskDesc D_dryfield_night_factory_80186DE0[];
extern TaskDesc D_dryfield_night_factory_80186E28[];

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_12", func_dryfield_night_factory_8017F1DC);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_12", D_dryfield_night_factory_8017D5C4);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_12", D_dryfield_night_factory_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_12", D_dryfield_night_factory_8017D5DC);
