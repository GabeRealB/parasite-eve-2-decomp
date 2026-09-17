#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "main/tmd.h"

extern SVECTOR D_neo_ark_submarine_gallery_801818C8[];
extern SVECTOR D_neo_ark_submarine_gallery_801818D8[];
extern SVECTOR D_neo_ark_submarine_gallery_801818F8[];
extern SVECTOR D_neo_ark_submarine_gallery_80181928[];

extern s32 D_80115738;
extern s32 D_8011574C;

void func_neo_ark_submarine_gallery_80180254(SVECTOR* pos, s32 arg1, s32 arg2);
void func_neo_ark_submarine_gallery_80180AC8(SVECTOR* pos, s32 arg1, s32 arg2);
void func_neo_ark_submarine_gallery_80180E80(GsCOORDINATE2* coord, s32 arg1);

/// Per-view draw callback for the gallery's display cases. The first state
/// latches the two effect ids the display cases animate with; every later run
/// draws one fixed set of positions for the current camera view. Views 2 to 6
/// each cover a run of `D_neo_ark_submarine_gallery_801818*` entries, and view 2
/// also hands the task's own coordinate to `func_neo_ark_submarine_gallery_80180E80`.
void func_neo_ark_submarine_gallery_8017EFEC(Task* arg0)
{
    SVECTOR*       pos;
    GsCOORDINATE2* coord;
    s32            view;

    coord = ((TmdObject*)arg0->extra)->field_8;
    if (arg0->state == 0) {
        D_8011574C  = 0x60193;
        D_80115738  = 0x60194;
        arg0->state = 1;
    }
    view = Gp_GetViewIndex() & 0xFF;
    switch (view) {
        case 2:
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818C8[0], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818C8[2], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818C8[4], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_801818C8[16], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_801818C8[17], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_801818C8[18], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_801818C8[31], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180E80(coord, 0x20);
            break;
        case 3:
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_80181928[0], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_80181928[4], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_80181928[5], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_80181928[14], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_80181928[15], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_80181928[16], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_80181928[17], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_80181928[18], 0x200, 0x444);
            pos = &D_neo_ark_submarine_gallery_80181928[19];
            func_neo_ark_submarine_gallery_80180AC8(pos, 0x200, 0x444);
            break;
        case 4:
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818F8[0], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818F8[2], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818F8[4], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_801818F8[18], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_801818F8[19], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_801818F8[20], 0x200, 0x444);
            pos = &D_neo_ark_submarine_gallery_801818F8[21];
            func_neo_ark_submarine_gallery_80180AC8(pos, 0x200, 0x444);
            break;
        case 5:
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818D8[0], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818D8[2], 0x200, 0x444);
            pos = &D_neo_ark_submarine_gallery_801818D8[3];
            func_neo_ark_submarine_gallery_80180254(pos, 0x200, 0x444);
            break;
        case 6:
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818F8[0], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818F8[2], 0x200, 0x444);
            pos = &D_neo_ark_submarine_gallery_801818F8[4];
            func_neo_ark_submarine_gallery_80180254(pos, 0x200, 0x444);
            break;
    }
}

/// The `.align 3` pad the original object carried behind the switch table above.
/// This unit owns `.rodata` 0x88..0xA0, so the word has to be emitted here even
/// though nothing reads it.
const s32 D_neo_ark_submarine_gallery_8017D65C = 0;
