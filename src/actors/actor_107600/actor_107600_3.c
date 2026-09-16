#include "common.h"

#include "main/mem.h"
#include "main/gfx.h"
#include "main/task.h"

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"

#include "actors/actor_107600.h"

/// (0 and 1) start the state at 1, kick the +0x50 rotation trio off at 0x400,
/// roll `Gp_LcgState` into `field_16A` beside the 10 percent scale pair
/// `func_actor_107600_80134EF4` divides the model root's rotation by, rebuild
/// that rotation through `func_actor_107600_80134A50`, and put the spawned
/// object's light into mode 2 with its blend timer cleared. Variant 2 only
/// starts the state at 8 and leaves the scale pair at 100 percent.
void func_actor_107600_80134C54(Task* arg0)
{
    Actor107600Work* work = (Actor107600Work*)arg0->idMap;
    GpObj4C*         obj  = arg0->spawnArg2;

    switch (work->field_162) {
        case 0:
        case 1:
            work->field_158 = 1;
            work->field_50  = 0x400;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_16A = (Gp_LcgState >> 16) & 7;
            work->field_168 = 10;
            work->field_169 = 10;
            func_actor_107600_80134A50(arg0);
            Gp_SetLightMode(obj, 2);
            obj->field_4F = 0;
            break;
        case 2:
            work->field_158 = 8;
            work->field_168 = 100;
            work->field_169 = 100;
            break;
    }
}

void func_actor_107600_80134D10(Actor107600* arg0)
{
    func_actor_107600_80134B98(arg0, 1);
}

void func_actor_107600_80134D30(Actor107600* arg0)
{
    func_actor_107600_80134B98(arg0, 1);
}

void func_actor_107600_80134D50(Actor107600* arg0)
{
    func_actor_107600_80134B98(arg0, 1);
}

void func_actor_107600_80134D70(Actor107600* arg0)
{
    arg0->field_1C->field_16B = 3;
    func_actor_107600_80134B98(arg0, 7);
}

/// Measures the XZ offset from this model's own attach coordinate to the one on
/// the `Gp_ActorSlots[0]` actor's model, in a 0x10-byte `VECTOR` carved off
/// `G_SCRATCH_HEAD` the way `func_actor_107600_80134E5C` carves its block, and
/// leaves the distance in `Actor107600Work.field_14C`. With no slot-0 actor the
/// carve is undone and nothing is measured. The distance is only stored once the
/// scratch block has been handed back, which is the order the original compiled
/// in - moving the store up costs a nop after the reload.
void func_actor_107600_80134D9C(Task* arg0)
{
    Actor107600Work* work;
    GsCOORDINATE2*   self;
    GsCOORDINATE2*   target;
    void**           scratch;
    u8*              head;
    VECTOR*          block;
    s32              dist;

    work     = (Actor107600Work*)arg0->idMap;
    self     = ((TmdObject*)arg0->extra)->field_8;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (VECTOR*)(head - 0x10);
    *scratch = block;
    if (Gp_ActorSlots[0] == NULL) {
        *scratch = head;
        return;
    }
    target          = ((TmdObject*)Gp_ActorSlots[0]->extra)->field_8;
    block->vx       = target->coord.t[0] - self->coord.t[0];
    block->vy       = target->coord.t[1] - self->coord.t[1];
    block->vz       = target->coord.t[2] - self->coord.t[2];
    dist            = func_80103D8C(block->vx, block->vz);
    *scratch        = (u8*)*scratch + 0x10;
    work->field_14C = dist;
}

/// Rotates a fixed 0x10-byte offset by the coordinate's own `coord` matrix and
/// leaves the result in that matrix's translation row. The offset is carved off
/// `G_SCRATCH_HEAD` the way `func_actor_107600_80132B0C` carves its VECTOR, but
/// is filled with (0, -0x180, 0) and rotated in place by `ApplyMatrixLV`, which
/// also folds in the matrix's existing translation. `func_actor_107600_80132ED0`
/// calls this on the coordinate it then hands to `Gp_UpdateCoord`.
void func_actor_107600_80134E5C(GsCOORDINATE2* arg0)
{
    void**  scratch;
    u8*     head;
    VECTOR* block;

    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    *scratch  = block;
    block->vx = 0;
    block->vy = -0x180;
    block->vz = 0;
    ApplyMatrixLV(&arg0->coord, block, block);
    arg0->coord.t[0] = block->vx;
    arg0->coord.t[1] = block->vy;
    *scratch         = (u8*)*scratch + 0x10;
    arg0->coord.t[2] = block->vz;
}

/// Scales the model root's rotation by the two percent factors
/// `func_actor_107600_80134C54` rolls into the work block: the diagonal
/// `coord.m[0][0]` and `coord.m[2][1]` halves, each read as a raw 16-bit value
/// and re-signed before the divide so the scale stays signed.
void func_actor_107600_80134EF4(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->idMap;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    u16              x     = coord->coord.m[0][0];
    u16              y     = coord->coord.m[2][1];

    coord->coord.m[0][0] = (s16)x / 100 * work->field_168;
    coord->coord.m[2][1] = (s16)y / 100 * work->field_169;
}
