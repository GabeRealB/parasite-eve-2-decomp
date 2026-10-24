/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Runs the Grenade Launcher's close strike and its brief forward approach.
///
/// Frame marks are relative to the requested clip's blend length. Attack 4 is live
/// at marks 28..39; marks 28..30 allow 100 parent-coordinate units of forward motion
/// per frame while the player is at least 1000 units away in X/Z. At mark 122,
/// returns to engagement's listening step. Requires live body work and a player
/// matrix in the root's parent frame; borrows one VECTOR block and leaves Y unused.
static void _golemPawnRookLauncherStrikeState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_ATTACK            = 4,
        GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_START_FRAME       = 28,
        GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_END_FRAME         = 40,
        GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_ADVANCE_END_FRAME = 30,
        GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_STOP_RANGE        = 1000,
        GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_SPEED             = 100,
        GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_FRAMES            = 122,
    };
    GolemPawnRookWork* work;
    GfxCoord*          root;
    VECTOR*            toPlayer;
    s16                blendFrames;
    s32                playerOffsetX;
    s32                playerOffsetZ;
    s32                playerDistance;

    toPlayer    = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    work        = actor->work;
    blendFrames = _gGolemPawnRookAnimBlendFrames[work->anim];
    root        = actor->extra.tmd->coords;
    if (work->animFrame == blendFrames + GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_START_FRAME) {
        work->strikeBody.key    = damagePackAttackKey(_gGolemPawnRookAttacks, GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_ATTACK);
        work->strikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else if (work->animFrame == blendFrames + GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_END_FRAME) {
        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    blendFrames = _gGolemPawnRookAnimBlendFrames[work->anim];
    if ((work->animFrame >= blendFrames + GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_START_FRAME) && (blendFrames + GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_ADVANCE_END_FRAME >= work->animFrame)) {
        playerOffsetX  = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
        toPlayer->vx   = playerOffsetX;
        playerOffsetZ  = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
        toPlayer->vz   = playerOffsetZ;
        playerDistance = SquareRoot0((toPlayer->vx * toPlayer->vx) + (toPlayer->vz * toPlayer->vz));
        if (playerDistance < GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_STOP_RANGE) {
            work->forwardSpeed = 0;
        } else {
            work->forwardSpeed = GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_SPEED;
        }
    } else {
        work->forwardSpeed = 0;
    }
    if (work->animFrame >= _gGolemPawnRookAnimBlendFrames[work->anim] + GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_FRAMES) {
        work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
        work->step     = GOLEM_PAWN_ROOK_ENGAGE_LISTEN_STEP;
        work->anim     = GOLEM_PAWN_ROOK_ANIM_LISTEN;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Scratch-stack block the laser sight is aimed in.
///
/// Reserved for the length of the call. The sight runs from the launcher's
/// muzzle along a fixed aim; the two short vectors follow its two ends through
/// the spaces the steps work in.
typedef struct {
    MATRIX  matrix; // part 7's transform relative to the root, which carries the muzzle into root space; then the rotation built from the aim angles
    VECTOR  offset; // result of each GTE rotation; also the first laser contact's position less the muzzle's, whose length is the beam's reach
    SVECTOR aim;    // the aim's Euler angles (0x1000 to the turn); then the sight's far end, before and after the aim rotation, in root space
    SVECTOR muzzle; // the sight's near end: its offset along part 7's axes, then its composed position for measuring the contact, then its root-space position for the beam
} _GolemPawnRookLaserSightScratch;
STATIC_ASSERT_SIZEOF(_GolemPawnRookLaserSightScratch, 0x40);

/// Updates the launcher's root-space laser probe and draws its contact-limited sight.
///
/// actor is a live Grenade Launcher GOLEM body task. Part 7 supplies the muzzle;
/// the probe's far end uses a fixed aim with angles in 4096 units per turn.
/// A zero burst timer arms the probe without drawing or consuming contacts.
/// Otherwise the previous probe contact limits the drawn reach, with 1000 game
/// units of overshoot for a player or companion body, and its contact is cleared.
/// Borrows scratch storage for the call and leaves the collision body enabled.
static void _golemPawnRookAimLaserSight(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_LASER_MUZZLE_PART      = 7,
        GOLEM_PAWN_ROOK_LASER_MUZZLE_Y         = 100,
        GOLEM_PAWN_ROOK_LASER_MUZZLE_Z         = -100,
        GOLEM_PAWN_ROOK_LASER_AIM_PITCH        = -5,
        GOLEM_PAWN_ROOK_LASER_AIM_YAW          = -5,
        GOLEM_PAWN_ROOK_LASER_FAR_Y            = -1300,
        GOLEM_PAWN_ROOK_LASER_REACH            = 10000,
        GOLEM_PAWN_ROOK_LASER_PLAYER_OVERSHOOT = 1000,
    };
    _GolemPawnRookLaserSightScratch* scratch;
    GolemPawnRookWork*               work;
    GfxCoord*                        bodyCoords;

    scratch                                                    = SCRATCH_STACK_RESERVE_BLOCK(_GolemPawnRookLaserSightScratch);
    bodyCoords                                                 = actor->extra.tmd->coords;
    work                                                       = actor->work;
    bodyCoords[0].composeStamp                                 = GRAPHICS_COORD_DIRTY;
    bodyCoords[GOLEM_PAWN_ROOK_LASER_MUZZLE_PART].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&bodyCoords[GOLEM_PAWN_ROOK_LASER_MUZZLE_PART]);
    // Carry the animated launcher muzzle from part 7 into body-root space.
    gfxMakeRelativeTransform(&bodyCoords->workm, &bodyCoords[GOLEM_PAWN_ROOK_LASER_MUZZLE_PART].workm, &scratch->matrix);
    scratch->muzzle.vy = GOLEM_PAWN_ROOK_LASER_MUZZLE_Y;
    scratch->muzzle.vx = 0;
    scratch->muzzle.vz = GOLEM_PAWN_ROOK_LASER_MUZZLE_Z;
    gte_ApplyMatrix(&scratch->matrix, &scratch->muzzle, &scratch->offset);
    work->laserCapsule.ends[1].vx = scratch->matrix.t[0] + scratch->offset.vx;
    work->laserCapsule.ends[1].vy = scratch->matrix.t[1] + scratch->offset.vy;
    work->laserCapsule.ends[1].vz = scratch->matrix.t[2] + scratch->offset.vz;
    scratch->aim.vx               = GOLEM_PAWN_ROOK_LASER_AIM_PITCH;
    scratch->aim.vy               = GOLEM_PAWN_ROOK_LASER_AIM_YAW;
    scratch->aim.vz               = 0;
    RotMatrix(&scratch->aim, &scratch->matrix);
    scratch->aim.vx = 0;
    scratch->aim.vy = GOLEM_PAWN_ROOK_LASER_FAR_Y;
    scratch->aim.vz = GOLEM_PAWN_ROOK_LASER_REACH;
    gte_ApplyMatrix(&scratch->matrix, &scratch->aim, &scratch->offset);
    work->laserCapsule.ends[0].vx = scratch->offset.vx;
    work->laserCapsule.ends[0].vy = scratch->offset.vy;
    work->laserCapsule.ends[0].vz = scratch->offset.vz;
    work->laserBody.flags        |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    if (work->timer == 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_GolemPawnRookLaserSightScratch);
        return;
    }
    // Measure the previous probe contact in the root matrix's composition frame.
    gte_SetRotMatrix(&bodyCoords->workm);
    scratch->muzzle.vx = work->laserCapsule.ends[1].vx;
    scratch->muzzle.vy = work->laserCapsule.ends[1].vy;
    scratch->muzzle.vz = work->laserCapsule.ends[1].vz;
    gte_ldv0(&scratch->muzzle);
    gte_rtv0();
    gte_stlvnl(&scratch->offset);
    scratch->muzzle.vx = scratch->offset.vx + bodyCoords->workm.t[0];
    scratch->muzzle.vy = scratch->offset.vy + bodyCoords->workm.t[1];
    scratch->muzzle.vz = scratch->offset.vz + bodyCoords->workm.t[2];
    scratch->aim.vx    = work->laserCapsule.ends[0].vx;
    scratch->aim.vy    = work->laserCapsule.ends[0].vy;
    if (worldCollisionFindContactIndex(work->laserContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        scratch->offset.vx = work->laserContacts[0].point.vx - scratch->muzzle.vx;
        scratch->offset.vy = work->laserContacts[0].point.vy - scratch->muzzle.vy;
        scratch->offset.vz = work->laserContacts[0].point.vz - scratch->muzzle.vz;
        scratch->aim.vz    = SquareRoot0(scratch->offset.vx * scratch->offset.vx + scratch->offset.vy * scratch->offset.vy +
                                         scratch->offset.vz * scratch->offset.vz);
        if ((work->laserContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            scratch->aim.vz += GOLEM_PAWN_ROOK_LASER_PLAYER_OVERSHOOT;
        }
    } else {
        scratch->aim.vz = GOLEM_PAWN_ROOK_LASER_REACH;
    }
    // Retain the probe's rotated X/Y, replace Z with reach, then rotate again for drawing.
    worldCollisionClearContacts(work->laserContacts);
    gte_ApplyMatrix(&scratch->matrix, &scratch->aim, &scratch->offset);
    scratch->aim.vx    = scratch->offset.vx;
    scratch->aim.vy    = scratch->offset.vy;
    scratch->aim.vz    = scratch->offset.vz;
    scratch->muzzle.vx = work->laserCapsule.ends[1].vx;
    scratch->muzzle.vy = work->laserCapsule.ends[1].vy;
    scratch->muzzle.vz = work->laserCapsule.ends[1].vz;
    _golemPawnRookDrawLaserBeam(actor, &scratch->aim, &scratch->muzzle);
    SCRATCH_STACK_RELEASE_BLOCK(_GolemPawnRookLaserSightScratch);
}

/// Scratch-stack block the laser beam is drawn from.
///
/// The beam is walked in eight steps from its near end, each point projected
/// in turn, and a segment is drawn between each point and the one before it.
/// A segment has three pairs of vertices: its start and end on the centre
/// line, and the same two points set off to either side by a width that
/// shrinks with the segment's depth. Reserved for the length of the call.
typedef struct {
    VECTOR  span;       // screen vector from the segment's start to its end; z is zero
    SVECTOR point;      // the beam point being projected, in root space; then `span` normalised (0x1000 for 1), with y negated, whose components, swapped, give the offsets across the beam
    SVECTOR step;       // an eighth of the beam, in root space
    s32     prevScreen; // screen position of the segment's start, x in the low halfword and y in the high
    s32     screen;     // screen position of the segment's end, packed the same way
    s32     prevDepth;  // ordering depth of the segment's start, a quarter of its screen z
    s32     depth;      // ordering depth of the segment's end
    s16     vertexX[6]; // screen x of the segment's vertices: [0], [1] its start and end on the centre line; [2], [3] and [4], [5] the same points on either edge
    s16     vertexY[6]; // screen y of the same vertices
} _GolemPawnRookLaserBeamScratch;
STATIC_ASSERT_SIZEOF(_GolemPawnRookLaserBeamScratch, 0x48);

/// Draws a red additive laser sight between two borrowed body-root-space endpoints.
///
/// nearEnd starts the beam; farEnd defines its span. The body's root workm must
/// already be composed for the active view. Seven segments cover seven eighths
/// of the span, with integer division truncating each step. Segments whose mean
/// ordering depth is below 30 are skipped. Each visible segment takes two shaded
/// quads, a centre line and a draw-mode packet from the current primitive arena;
/// the caller must leave room for up to seven such groups. Scratch is call-owned.
static void _golemPawnRookDrawLaserBeam(Task* actor, const SVECTOR* farEnd, const SVECTOR* nearEnd)
{
    enum {
        GOLEM_PAWN_ROOK_LASER_SUBDIVISIONS         = 8,
        GOLEM_PAWN_ROOK_LASER_MIN_DEPTH            = 30,
        GOLEM_PAWN_ROOK_LASER_WIDTH                = 0x900,
        GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS = 12,
        GOLEM_PAWN_ROOK_LASER_DRAW_MODE            = 0xE1000620, // Draw-to-display, dithering and additive blend.
    };
    _GolemPawnRookLaserBeamScratch* scratch;
    GfxCoord*                       root;
    POLY_G4*                        ribbon;
    LINE_F2*                        line;
    DR_TPAGE*                       drawMode;
    s32                             pointIndex;
    s32                             ribbonIndex;
    s32                             segmentDepth;

    /// Projects a root-space laser point to packed screen XY and quarter-Z depth.
    ///
    /// root's workm is composed into the active view; point borrows a word-aligned
    /// eight-byte SVECTOR. screen/depth are word-aligned writable s32 lvalues.
    /// screen packs signed X/Y into its low/high halfwords; depth receives GTE
    /// screen Z divided by four. Requires the current GTE projection settings;
    /// replaces rotation, translation and projection results without testing
    /// clipping flags. The caller filters depth after projecting both endpoints.
    /// Arguments must be side-effect-free, including through the SDK macros;
    /// root occurs repeatedly. Captures no locals; use the compound statement
    /// only as a standalone invocation. It is undefined below this handler.
#define GOLEM_PAWN_ROOK_PROJECT_LASER_POINT(root, point, screen, depth) \
    {                                                                   \
        gte_SetRotMatrix(&(root)->workm);                               \
        gte_SetTransMatrix(&(root)->workm);                             \
        gte_ldv0((point));                                              \
        gte_rtps();                                                     \
        gte_stsxy(&(screen));                                           \
        gte_stszotz(&(depth));                                          \
    }

    scratch          = SCRATCH_STACK_RESERVE_BLOCK(_GolemPawnRookLaserBeamScratch);
    root             = actor->extra.tmd->coords;
    scratch->step.vx = (farEnd->vx - nearEnd->vx) / GOLEM_PAWN_ROOK_LASER_SUBDIVISIONS;
    scratch->step.vy = (farEnd->vy - nearEnd->vy) / GOLEM_PAWN_ROOK_LASER_SUBDIVISIONS;
    scratch->step.vz = (farEnd->vz - nearEnd->vz) / GOLEM_PAWN_ROOK_LASER_SUBDIVISIONS;
    GOLEM_PAWN_ROOK_PROJECT_LASER_POINT(root, nearEnd, scratch->prevScreen, scratch->prevDepth);
    // Project seven adjacent segments; the final eighth remains undrawn.
    for (pointIndex = 1; pointIndex < GOLEM_PAWN_ROOK_LASER_SUBDIVISIONS; pointIndex++) {
        scratch->point.vx = nearEnd->vx + scratch->step.vx * pointIndex;
        scratch->point.vy = nearEnd->vy + scratch->step.vy * pointIndex;
        scratch->point.vz = nearEnd->vz + scratch->step.vz * pointIndex;
        GOLEM_PAWN_ROOK_PROJECT_LASER_POINT(root, &scratch->point, scratch->screen, scratch->depth);
        segmentDepth = (scratch->prevDepth + scratch->depth) / 2;
        if (segmentDepth < GOLEM_PAWN_ROOK_LASER_MIN_DEPTH) {
            scratch->prevScreen = scratch->screen;
            scratch->prevDepth  = scratch->depth;
            continue;
        }
        // Build two fading ribbons around the projected centre line.
        scratch->span.vz    = 0;
        scratch->vertexX[0] = scratch->prevScreen;
        scratch->vertexY[0] = scratch->prevScreen >> 16;
        scratch->vertexX[1] = scratch->screen;
        scratch->vertexY[1] = scratch->screen >> 16;
        scratch->span.vx    = scratch->vertexX[1] - scratch->vertexX[0];
        scratch->span.vy    = scratch->vertexY[1] - scratch->vertexY[0];
        VectorNormalS(&scratch->span, &scratch->point);
        scratch->point.vy  *= -1;
        scratch->vertexX[2] = scratch->vertexX[0] + (-(scratch->point.vy * GOLEM_PAWN_ROOK_LASER_WIDTH) >> GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS) / segmentDepth;
        scratch->vertexX[3] = scratch->vertexX[1] + (-(scratch->point.vy * GOLEM_PAWN_ROOK_LASER_WIDTH) >> GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS) / segmentDepth;
        scratch->vertexX[4] = scratch->vertexX[0] + ((scratch->point.vy * (GOLEM_PAWN_ROOK_LASER_WIDTH >> 8)) >> (GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS - 8)) / segmentDepth;
        scratch->vertexY[2] = scratch->vertexY[0] + (-(scratch->point.vx * GOLEM_PAWN_ROOK_LASER_WIDTH) >> GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS) / segmentDepth;
        scratch->vertexY[3] = scratch->vertexY[1] + (-(scratch->point.vx * GOLEM_PAWN_ROOK_LASER_WIDTH) >> GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS) / segmentDepth;
        scratch->vertexY[4] = scratch->vertexY[0] + ((scratch->point.vx * (GOLEM_PAWN_ROOK_LASER_WIDTH >> 8)) >> (GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS - 8)) / segmentDepth;
        scratch->vertexY[5] = scratch->vertexY[1] + ((scratch->point.vx * (GOLEM_PAWN_ROOK_LASER_WIDTH >> 8)) >> (GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS - 8)) / segmentDepth;
        scratch->vertexX[5] = scratch->vertexX[1] + ((scratch->point.vy * (GOLEM_PAWN_ROOK_LASER_WIDTH >> 8)) >> (GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS - 8)) / segmentDepth;
        for (ribbonIndex = 0; ribbonIndex < ARRAY_SIZE(_gGolemPawnRookBeamRibbonCorners); ribbonIndex++) {
            ribbon         = gGpuPrimCursor;
            gGpuPrimCursor = ribbon + 1;
            setPolyG4(ribbon);
            setSemiTrans(ribbon, 1);
            ribbon->x0 = scratch->vertexX[_gGolemPawnRookBeamRibbonCorners[ribbonIndex][0]];
            ribbon->y0 = scratch->vertexY[_gGolemPawnRookBeamRibbonCorners[ribbonIndex][0]];
            ribbon->x1 = scratch->vertexX[_gGolemPawnRookBeamRibbonCorners[ribbonIndex][1]];
            ribbon->y1 = scratch->vertexY[_gGolemPawnRookBeamRibbonCorners[ribbonIndex][1]];
            ribbon->x2 = scratch->vertexX[_gGolemPawnRookBeamRibbonCorners[ribbonIndex][2]];
            ribbon->y2 = scratch->vertexY[_gGolemPawnRookBeamRibbonCorners[ribbonIndex][2]];
            ribbon->x3 = scratch->vertexX[_gGolemPawnRookBeamRibbonCorners[ribbonIndex][3]];
            ribbon->y3 = scratch->vertexY[_gGolemPawnRookBeamRibbonCorners[ribbonIndex][3]];
            setRGB0(ribbon, 0xFF, 0, 0);
            setRGB1(ribbon, 0xFF, 0, 0);
            setRGB2(ribbon, 0, 0, 0);
            setRGB3(ribbon, 0, 0, 0);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(segmentDepth), ribbon);
        }
        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setSemiTrans(line, 1);
        line->x0 = scratch->prevScreen;
        line->y0 = scratch->prevScreen >> 16;
        line->x1 = scratch->screen;
        line->y1 = scratch->screen >> 16;
        setRGB0(line, 0xFF, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(segmentDepth), line);
        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setlen(drawMode, 1);
        drawMode->code[0] = GOLEM_PAWN_ROOK_LASER_DRAW_MODE;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(segmentDepth), drawMode);
        scratch->prevScreen = scratch->screen;
        scratch->prevDepth  = scratch->depth;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemPawnRookLaserBeamScratch);

#undef GOLEM_PAWN_ROOK_PROJECT_LASER_POINT
}

enum {
    GOLEM_PAWN_ROOK_GRENADE_LAUNCH_Y     = 500,
    GOLEM_PAWN_ROOK_GRENADE_LAUNCH_Z     = 100,
    GOLEM_PAWN_ROOK_GRENADE_LAUNCH_PITCH = 128,
    GOLEM_PAWN_ROOK_GRENADE_LAUNCH_ROLL  = 16,
};

/// Detaches a grenade root from the launcher with its launch offset and rotation.
///
/// Both roots must be live, distinct coordinates; placement is caller-owned
/// scratch. Carries the launcher's composed pose into gGfxViewCoord's local
/// frame and parents the grenade there. The offset is (0, 500, 100) along the
/// launcher's axes; the additional pitch 128 and roll 16 use 4096 units per turn.
/// Preserves the translated origin while applying that local rotation. The
/// spawn handler composes the grenade and manages its task-tree detachment.
static inline void _golemPawnRookPlaceGrenade(GfxCoord* grenadeRoot, GfxCoord* launcherRoot,
                                              ActorChildPlaceScratch* placement)
{
    // Place the launch offset and Euler turn relative to the view coordinate.
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    launcherRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(launcherRoot);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &launcherRoot->workm, &grenadeRoot->coord);

    placement->operand.vx = 0;
    placement->operand.vy = GOLEM_PAWN_ROOK_GRENADE_LAUNCH_Y;
    placement->operand.vz = GOLEM_PAWN_ROOK_GRENADE_LAUNCH_Z;
    gte_SetRotMatrix(&grenadeRoot->coord);
    gte_ldv0(&placement->operand);
    gte_rtv0();
    gte_stlvnl(&placement->offset);
    grenadeRoot->parent      = &gGfxViewCoord;
    grenadeRoot->coord.t[0] += placement->offset.vx;
    grenadeRoot->coord.t[1] += placement->offset.vy;
    grenadeRoot->coord.t[2] += placement->offset.vz;

    placement->operand.vx = GOLEM_PAWN_ROOK_GRENADE_LAUNCH_PITCH;
    placement->operand.vy = 0;
    placement->operand.vz = GOLEM_PAWN_ROOK_GRENADE_LAUNCH_ROLL;
    RotMatrix(&placement->operand, &placement->rotation);
    // The GTE retains the original basis while its in-place product columns are stored.
    gte_MulMatrix0(&grenadeRoot->coord, &placement->rotation, &grenadeRoot->coord);
}

/// Launches a grenade from the attached launcher and starts its independent flight.
///
/// `enemy` is the grenade's enemy record and `grenade` is its TMD child task,
/// initially parented to the launcher. Allocates grenade-owned work and lighting
/// matrices, places the root in view-coordinate space, and links player, enemy
/// and wall collision bodies. The grenade detaches before flying; allocation
/// failure destroys it. Placement scratch is released before returning.
static void _golemPawnRookGrenadeSpawn(Enemy* enemy, Task* grenade)
{
    enum {
        GOLEM_PAWN_ROOK_GRENADE_RADIUS            = 100,
        GOLEM_PAWN_ROOK_GRENADE_ENEMY_ATTACK_KEY  = 0x22B2B, // Category 2, weapon row 43 and distance-scale row 43.
        GOLEM_PAWN_ROOK_GRENADE_ATTACK            = 3,
        GOLEM_PAWN_ROOK_GRENADE_REACTION_DARKNESS = 1,
        GOLEM_PAWN_ROOK_GRENADE_WALL_TRAIL_LENGTH = 500,
    };

    GolemPawnRookGrenadeWork* work;
    ActorChildPlaceScratch*   placement;
    Enemy*                    spawnEnemy;
    GfxCoord*                 root;
    GfxCoord*                 launcherRoot;
    TmdObject*                model;
    Task*                     launcher;
    s32                       soundId;
    s32                       audioPan;

    model        = grenade->extra.tmd;
    root         = model->coords;
    launcher     = grenade->parent;
    launcherRoot = launcher->extra.tmd->coords;
    work         = memCalloc(sizeof(GolemPawnRookGrenadeWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, grenade);
        return;
    }
    grenade->work   = work;
    model->flags    = 0;
    placement       = SCRATCH_STACK_RESERVE_BLOCK(ActorChildPlaceScratch);
    model->lightMtx = &work->lightMtx;
    model->colorMtx = &work->colorMtx;

    _golemPawnRookPlaceGrenade(root, launcherRoot, placement);

    // Two strike spheres share a contact; the trailing capsule tests the room grid.
    work->burstStyle = (_gGolemPawnRookAttacks[GOLEM_PAWN_ROOK_GRENADE_ATTACK].reaction != GOLEM_PAWN_ROOK_GRENADE_REACTION_DARKNESS);

    work->playerStrikeBody.coord            = root;
    work->playerStrikeBody.context.contacts = work->strikeContacts;
    work->playerStrikeBody.pos.vx           = 0;
    work->playerStrikeBody.pos.vy           = 0;
    work->playerStrikeBody.pos.vz           = 0;
    work->playerStrikeBody.key              = damagePackAttackKey(_gGolemPawnRookAttacks, GOLEM_PAWN_ROOK_GRENADE_ATTACK);
    work->playerStrikeBody.radius           = GOLEM_PAWN_ROOK_GRENADE_RADIUS;
    work->playerStrikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->playerStrikeBody);
    worldCollisionInitContacts(work->strikeContacts, ARRAY_SIZE(work->strikeContacts), 0);
    work->playerStrikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->enemyStrikeBody.coord            = root;
    work->enemyStrikeBody.context.contacts = work->strikeContacts;
    work->enemyStrikeBody.pos.vx           = 0;
    work->enemyStrikeBody.pos.vy           = 0;
    work->enemyStrikeBody.pos.vz           = 0;
    work->enemyStrikeBody.key              = GOLEM_PAWN_ROOK_GRENADE_ENEMY_ATTACK_KEY;
    work->enemyStrikeBody.radius           = GOLEM_PAWN_ROOK_GRENADE_RADIUS;
    work->enemyStrikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &work->enemyStrikeBody);
    work->enemyStrikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->wallCapsule.ends[0].vx   = 0;
    work->wallCapsule.ends[0].vy   = 0;
    work->wallCapsule.ends[0].vz   = 0;
    work->wallCapsule.ends[1].vx   = 0;
    work->wallCapsule.ends[1].vy   = -GOLEM_PAWN_ROOK_GRENADE_WALL_TRAIL_LENGTH;
    work->wallCapsule.ends[1].vz   = 0;
    work->wallCapsule.end0Radius   = 1;
    work->wallCapsule.end1Radius   = 1;
    work->wallCapsule.contacts     = work->wallContacts;
    work->wallBody.context.capsule = &work->wallCapsule;
    work->wallBody.coord           = root;
    work->wallBody.pos.vx          = 0;
    work->wallBody.pos.vy          = 0;
    work->wallBody.pos.vz          = 0;
    work->wallBody.key             = 0;
    work->wallBody.radius          = 0;
    work->wallBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->wallBody);
    worldCollisionInitContacts(work->wallContacts, ARRAY_SIZE(work->wallContacts), 0);
    work->wallBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);

    // The projectile owns its work and no longer follows launcher teardown.
    grenade->state = GOLEM_PAWN_ROOK_TASK_RUNNING;
    taskDetachFromParent(grenade);

    root->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(root);

    spawnEnemy = grenade->spawnArg2.pointer;
    soundId    = _gGolemPawnRookGrenadeLaunchCue | ((spawnEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT);
    audioPan   = (s8)worldCoordGetOriginAudioPan(root);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(root));

    SCRATCH_STACK_RELEASE_BLOCK(ActorChildPlaceScratch);
}

/// Advances an independently flying launcher grenade and bursts it on impact.
///
/// grenade owns live grenade work and linked collision bodies. The Q12 local
/// Y axis advances it 150 game units per running frame using 75 >> 11 scaling.
/// Paused frames only sample colour; hidden frames skip drawing. A strike,
/// probe-blocking wall, or 90 running frames starts hidden teardown and the room
/// burst effect; a player-body strike also rumbles. The unused enemy parameter
/// preserves the lifecycle callback signature. Flight scratch is released here.
static void _golemPawnRookGrenadeFly(Enemy* unusedEnemy, Task* grenade)
{
    /// Flight's 40-byte reservation; only the puff's local offset is identified.
    typedef struct {
        SVECTOR puffOffset;       // smoke spawn offset along the grenade's local axes
        u8      unknown_08[0x20]; // reserved bytes with no recovered access; role and layout unproven
    } _GolemPawnRookGrenadeFlightScratch;
    STATIC_ASSERT_SIZEOF(_GolemPawnRookGrenadeFlightScratch, 0x28);
    enum {
        GOLEM_PAWN_ROOK_GRENADE_MOVE_NUMERATOR       = 75,
        GOLEM_PAWN_ROOK_GRENADE_MOVE_SHIFT           = 11,
        GOLEM_PAWN_ROOK_GRENADE_PUFF_FRAMES          = 4,
        GOLEM_PAWN_ROOK_GRENADE_PUFF_OFFSET_Y        = 100,
        GOLEM_PAWN_ROOK_GRENADE_SMOKE_ARGUMENT       = 0x01001600, // size 1536, period 1, drifting mode 1
        GOLEM_PAWN_ROOK_GRENADE_FLIGHT_FRAMES        = 90,
        GOLEM_PAWN_ROOK_GRENADE_COMPANION_BIT        = 0x80,
        GOLEM_PAWN_ROOK_GRENADE_RUMBLE_FRAMES        = 10,
        GOLEM_PAWN_ROOK_GRENADE_RUMBLE_POWER         = 255,
        GOLEM_PAWN_ROOK_GRENADE_RUMBLE_END_INTENSITY = 8,
    };

    GolemPawnRookGrenadeWork*           work;
    GfxCoord*                           root;
    TmdObject*                          model;
    _GolemPawnRookGrenadeFlightScratch* flightScratch;
    Enemy*                              spawnEnemy;
    s32                                 blockedSurface;
    s32                                 surfaceClass;
    s32                                 soundId;
    s32                                 audioPan;
    VECTOR                              worldPos;

    model          = grenade->extra.tmd;
    root           = model->coords;
    work           = grenade->work;
    blockedSurface = 0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            model->flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            worldPos.vx = root->workm.t[0];
            worldPos.vy = root->workm.t[1];
            worldPos.vz = root->workm.t[2];
            worldCoordUpdateActorColor(grenade->spawnArg2.pointer, &worldPos, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    root->composeStamp = GRAPHICS_COORD_DIRTY;
    root->coord.t[0]  += (root->coord.m[0][1] * GOLEM_PAWN_ROOK_GRENADE_MOVE_NUMERATOR) >> GOLEM_PAWN_ROOK_GRENADE_MOVE_SHIFT;
    root->coord.t[1]  += (root->coord.m[1][1] * GOLEM_PAWN_ROOK_GRENADE_MOVE_NUMERATOR) >> GOLEM_PAWN_ROOK_GRENADE_MOVE_SHIFT;
    root->coord.t[2]  += (root->coord.m[2][1] * GOLEM_PAWN_ROOK_GRENADE_MOVE_NUMERATOR) >> GOLEM_PAWN_ROOK_GRENADE_MOVE_SHIFT;

    flightScratch = SCRATCH_STACK_RESERVE_BLOCK(_GolemPawnRookGrenadeFlightScratch);
    if (++work->timer >= GOLEM_PAWN_ROOK_GRENADE_PUFF_FRAMES) {
        flightScratch->puffOffset.vx = 0;
        flightScratch->puffOffset.vy = GOLEM_PAWN_ROOK_GRENADE_PUFF_OFFSET_Y;
        flightScratch->puffOffset.vz = 0;
        effectSpawn(EFFECT_SMOKE_PUFF, root, GOLEM_PAWN_ROOK_GRENADE_SMOKE_ARGUMENT, &flightScratch->puffOffset);
        work->timer = 0;
    }
    worldPos.vx = root->workm.t[0];
    worldPos.vy = root->workm.t[1];
    worldPos.vz = root->workm.t[2];
    worldCoordUpdateActorColor(grenade->spawnArg2.pointer, &worldPos, 0, 0);

    if (work->wallContacts[0].key.value != 0) {
        surfaceClass = worldCollisionSurfaceClassFromKey(work->wallContacts[0].key.value);
        if (gWorldCollisionSurfacePropertiesTables[gGameSession->location.stage - 1][gGameSession->location.area - 1][surfaceClass]->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
            blockedSurface = 1;
        }
        worldCollisionClearContacts(work->wallContacts);
    }
    if (work->strikeContacts[0].key.value != 0 || blockedSurface || ++work->flightFrames >= GOLEM_PAWN_ROOK_GRENADE_FLIGHT_FRAMES) {
        effectSpawn(gRoomEffectSparkBurstId, root, (s32)work->burstStyle, NULL);
        grenade->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        spawnEnemy                = grenade->spawnArg2.pointer;
        soundId                   = gGolemPawnRookImpactSound | (((u16)spawnEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT);
        audioPan                  = (s8)worldCoordGetOriginAudioPan(root);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
        grenade->state = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
        if ((work->strikeContacts[0].key.value & (WORLD_COLLISION_CONTACT_KIND_MASK | GOLEM_PAWN_ROOK_GRENADE_COMPANION_BIT)) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            padScriptSpawnVariableMotorRamp(GOLEM_PAWN_ROOK_GRENADE_RUMBLE_FRAMES, GOLEM_PAWN_ROOK_GRENADE_RUMBLE_POWER, GOLEM_PAWN_ROOK_GRENADE_RUMBLE_END_INTENSITY);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemPawnRookGrenadeFlightScratch);
}
