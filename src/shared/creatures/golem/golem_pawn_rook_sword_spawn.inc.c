/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Attaches the Beam Sword model and arms its delayed trail effect.
///
/// `task` is the sword child of a live GOLEM body task. It attaches to body part
/// 7, borrows the body's lighting matrices, and starts a ten-tick trail delay in
/// the parent's work. It remains in the parent's teardown tree. The common
/// enemy-state callback argument `unusedEnemy` is ignored.
static void _golemPawnRookSwordSpawn(Enemy* unusedEnemy, Task* task)
{
    enum {
        GOLEM_PAWN_ROOK_SWORD_TRAIL_DELAY_FRAMES = 10,
        GOLEM_PAWN_ROOK_WEAPON_PART              = 7,
    };

    GolemPawnRookWork* work;

    work                  = _golemPawnRookAttachChildModel(task, GOLEM_PAWN_ROOK_WEAPON_PART);
    work->swordTrailDelay = GOLEM_PAWN_ROOK_SWORD_TRAIL_DELAY_FRAMES;
}

/// Mirrors body visibility and starts the attached Beam Sword's delayed trail.
///
/// sword must remain parented to a live GOLEM body. A positive swordTrailDelay
/// counts down once per call; reaching zero attempts one unrestricted twin-trail
/// spawn at body part 7. A successful trail task becomes a child of the sword
/// so sword teardown also ends it. Allocation failure is not retried. The
/// unused enemy parameter preserves the lifecycle callback signature.
static void _golemPawnRookSwordTick(Enemy* unusedEnemy, Task* sword)
{
    EffectWork*        trail;
    Task*              body;
    GolemPawnRookWork* work;
    s16                framesLeft;

    body                    = sword->parent;
    work                    = body->work;
    sword->extra.tmd->flags = body->extra.tmd->flags;
    if (work->swordTrailDelay > 0) {
        framesLeft            = work->swordTrailDelay - 1;
        work->swordTrailDelay = framesLeft;
        if (framesLeft == 0) {
            trail = effectSpawn(gRoomEffectTwinTrailId | EFFECT_SPAWN_UNLIMITED,
                                &sword->parent->extra.tmd->coords[7], 0, NULL);
            if (trail != NULL) {
                taskReparent(sword, trail->task);
            }
        }
    }
}
