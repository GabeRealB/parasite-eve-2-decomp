/* Part of the cloaked stalker library; see cloaked_stalker.h. */

/// Inlined copy of `stalkerTickAnim`: reseeds animation slots
/// 1..0x12 when the animation id changes, otherwise ticks them a frame.
static inline void stalkerTickAnimInline(Task* arg0)
{
    Actor402200Work* work;
    s32              i;
    s32              value;

    work = arg0->work;
    if (work->field_6C0 != work->field_6C2) {
        work->field_6C2 = work->field_6C0;
        work->field_6C4 = 0;
        value           = gStalkerAnimBlend[work->field_6C0];
        for (i = 1; i < 0x13; i++) {
            func_800B4114(&work->rig.anim, i, work->field_6C0, 0, value);
        }
    } else {
        work->field_6C4++;
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
}

/// Inlined copy of `stalkerDrawShadow`: draws the ground shadow quad.
static inline void stalkerDrawShadowInline(Task* arg0)
{
    Actor402200Work* work;
    GfxCoord*        coord;
    GfxCoord*        sub;
    VECTOR3          vec;

    work  = arg0->work;
    coord = &arg0->extra.tmd->coords[0];
    sub   = &arg0->extra.tmd->coords[3];
    if (work->field_6E2 == 0) {
        work->field_6E2 = -1;
    }
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, work->field_6E2);
}
