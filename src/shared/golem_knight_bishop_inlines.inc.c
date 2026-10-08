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

/// Draws a ground shadow below part 3 at the root's composed world height.
///
/// `task` owns a live GOLEM model/work block with current composed coordinates.
/// The half-side is 768 world units. Zero shade is changed to the negative
/// no-shadow sentinel before drawing; this also updates the work block.
static inline void _golemKnightBishopDrawShadowInline(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_SHADOW_HALF_SIZE = 768,
        GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN    = -1,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    GfxCoord*              shadowPart;
    VECTOR3                shadowCentre;

    work       = task->work;
    root       = &task->extra.tmd->coords[0];
    shadowPart = &task->extra.tmd->coords[3];
    if (work->shadowShade == 0) {
        work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
    }
    shadowCentre.vx = shadowPart->workm.t[0];
    shadowCentre.vy = root->workm.t[1];
    shadowCentre.vz = shadowPart->workm.t[2];
    effectDrawGroundShadow(&shadowCentre, GOLEM_KNIGHT_BISHOP_SHADOW_HALF_SIZE, work->shadowShade);
}

/// Samples room lighting at the root and applies a pending ambient-colour tint.
///
/// Requires a live GOLEM model/work block, Enemy spawn argument and current
/// composed root position. Dim blue sets ambient RGB to (0, 0, 1024); white
/// sets it to (4095, 4095, 4095), in Q12 channel units. Recognized requests are
/// cleared after application; other values leave the room lighting in effect.
static __inline__ void _golemKnightBishopUpdateTintInline(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_TINT_NONE        = 0,
        GOLEM_KNIGHT_BISHOP_TINT_DIM_BLUE    = 1,
        GOLEM_KNIGHT_BISHOP_TINT_BLUE_LEVEL  = 1024,
        GOLEM_KNIGHT_BISHOP_TINT_WHITE_LEVEL = 4095,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    VECTOR                 worldPosition;
    s16                    red;
    s16                    green;
    s16                    blue;

    root             = task->extra.tmd->coords;
    work             = task->work;
    worldPosition.vx = root->workm.t[0];
    worldPosition.vy = root->workm.t[1];
    worldPosition.vz = root->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &worldPosition, 0, 0);
    switch (work->tintRequest) {
        case GOLEM_KNIGHT_BISHOP_TINT_DIM_BLUE:
            red   = 0;
            green = 0;
            blue  = GOLEM_KNIGHT_BISHOP_TINT_BLUE_LEVEL;
            worldCoordSetModelAmbientColor(task->extra.tmd, red, green, blue);
            work->tintRequest = GOLEM_KNIGHT_BISHOP_TINT_NONE;
            break;
        case GOLEM_KNIGHT_BISHOP_TINT_WHITE:
            red   = GOLEM_KNIGHT_BISHOP_TINT_WHITE_LEVEL;
            green = GOLEM_KNIGHT_BISHOP_TINT_WHITE_LEVEL;
            blue  = GOLEM_KNIGHT_BISHOP_TINT_WHITE_LEVEL;
            worldCoordSetModelAmbientColor(task->extra.tmd, red, green, blue);
            work->tintRequest = GOLEM_KNIGHT_BISHOP_TINT_NONE;
            break;
        case GOLEM_KNIGHT_BISHOP_TINT_NONE:
        default:
            return;
    }
}
