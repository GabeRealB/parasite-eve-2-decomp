#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"

#include "rooms/neo_ark_power_plant_1.h"

/// Sprite-suppression switch for the room's two per-view objects: 0 draws the
/// sprites of both views (both skip-OT-link bytes cleared), 1 leaves both out
/// of the ordering table, and any other value is ignored. The two views hang
/// off the room's sprite-table record at 0x40 and 0x4C.
void func_neo_ark_power_plant_1_8017E524(s32 arg0)
{
    GpAreaKey*                 sess;
    NeoArkPowerPlant1SprtRec*  rec;
    NeoArkPowerPlant1SprtView* view;
    s32                        v;

    sess = &gGameSession->at4.loc;
    rec  = (NeoArkPowerPlant1SprtRec*)Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    v    = arg0 & 0xFF;

    if (v == 0) {
        view          = rec->field_40;
        view->field_C = 0;
        view          = rec->field_4C;
        view->field_C = 0;
        return;
    }
    if (v == 1) {
        view          = rec->field_40;
        view->field_C = v;
        view          = rec->field_4C;
        view->field_C = v;
    }
}
