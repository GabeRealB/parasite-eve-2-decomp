/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Inlined copy of `golemKnightBishopTickAnim`: reseeds animation slots
/// 1..0x12 when the animation id changes, otherwise ticks them a frame.
static inline void golemKnightBishopTickAnimInline(Task* arg0)
{
    GolemKnightBishopWork* work;
    s32                    i;
    s32                    value;

    work = arg0->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        value             = gGolemKnightBishopAnimBlend[work->anim];
        for (i = 1; i < 0x13; i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->anim, 0, value);
        }
    } else {
        work->animFrame++;
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// Inlined copy of `golemKnightBishopDrawShadow`: draws the ground shadow quad.
static inline void golemKnightBishopDrawShadowInline(Task* arg0)
{
    GolemKnightBishopWork* work;
    GfxCoord*              coord;
    GfxCoord*              sub;
    VECTOR3                vec;

    work  = arg0->work;
    coord = &arg0->extra.tmd->coords[0];
    sub   = &arg0->extra.tmd->coords[3];
    if (work->shadowShade == 0) {
        work->shadowShade = -1;
    }
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    effectDrawGroundShadow(&vec, 0x300, work->shadowShade);
}
