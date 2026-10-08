/* Part of the Pawn/Rook GOLEM library; see golem_pawn_rook.h. */

/// Two-vector scratch block holding the composed offset and Q12 normalized aim.
typedef struct {
    VECTOR playerDirection; // player offset, then Q12 aim in room axes
    VECTOR normalizedAim;   // Q12 direction in composed-coordinate space
} _GolemPawnRookBurstAimScratch;
STATIC_ASSERT_SIZEOF(_GolemPawnRookBurstAimScratch, 0x20);

/// Backs a launcher-equipped GOLEM away, aims, and requests grenade bursts and reloads.
///
/// actor must have live body work, the launcher child and room/player coordinates.
/// fireRequest asks the child for one grenade; shotsSinceReload counts six shots
/// per reload, while burstShots ends at the carrier's burst limit. Aim uses the
/// player's part 2: normalize its offset in composed space, then rotate into
/// room axes. The horizontal cutoff is a Q12 direction magnitude, not distance.
/// Interrupt damage of at least 41 HP breaks the burst. The two VECTORs are
/// borrowed from the scratch stack for the call and released before returning.
static void _golemPawnRookGrenadeBurstState(Task* actor)
{

    enum {
        GOLEM_PAWN_ROOK_BURST_OPENING             = 0,
        GOLEM_PAWN_ROOK_BURST_BACKOFF             = 1,
        GOLEM_PAWN_ROOK_BURST_CHECK_AIM           = 2,
        GOLEM_PAWN_ROOK_BURST_FIRE                = 3,
        GOLEM_PAWN_ROOK_BURST_RELOAD              = 4,
        GOLEM_PAWN_ROOK_BURST_FINISH              = 5,
        GOLEM_PAWN_ROOK_BURST_CANCEL              = 6,
        GOLEM_PAWN_ROOK_BURST_OPENING_FRAMES      = 20,
        GOLEM_PAWN_ROOK_BURST_BACKOFF_SPEED       = -22,
        GOLEM_PAWN_ROOK_BURST_BACKOFF_TURN        = 30,
        GOLEM_PAWN_ROOK_BURST_FIRE_TURN           = 7,
        GOLEM_PAWN_ROOK_BURST_MIN_HORIZONTAL_AIM  = 2000,
        GOLEM_PAWN_ROOK_BURST_CANCEL_YAW          = 257,
        GOLEM_PAWN_ROOK_BURST_FIRE_LOCK_FRAMES    = 3,
        GOLEM_PAWN_ROOK_BURST_SHOTS_PER_RELOAD    = 6,
        GOLEM_PAWN_ROOK_BURST_INTERRUPT_HP        = 41,
        GOLEM_PAWN_ROOK_BURST_REPEAT_BLEND_FRAMES = 3,
        GOLEM_PAWN_ROOK_BURST_END_FRAMES          = 22,
        GOLEM_PAWN_ROOK_BURST_RELOAD_EFFECT_FRAME = 26,
        GOLEM_PAWN_ROOK_BURST_RELOAD_FRAMES       = 135,
        GOLEM_PAWN_ROOK_BURST_CANCEL_FRAMES       = 25,
        GOLEM_PAWN_ROOK_BURST_FIRE_ANIM           = 13,
        GOLEM_PAWN_ROOK_BURST_CANCEL_ANIM         = 14,
        GOLEM_PAWN_ROOK_BURST_RELOAD_ANIM         = 15,
        GOLEM_PAWN_ROOK_BURST_BACKOFF_ANIM        = 30,
        GOLEM_PAWN_ROOK_BURST_RELOAD_ARGUMENT     = (6 << 16) | 12, // six emitted frames, weapon-offset row 12

    };

    s16                            yawDelta;
    s32                            yawMagnitude;
    s16                            yawError;
    s32                            directionX;
    s32                            directionZ;
    _GolemPawnRookBurstAimScratch* scratch;
    GolemPawnRookWork*             work;
    GfxCoord*                      playerPart;
    GfxCoord*                      root;

/// Samples a composed player offset and converts its normalized aim to room axes.
///
/// root and aim are side-effect-free pointers; playerPart is a GfxCoord* local
/// lvalue. Inputs must have composed matrices in the same frame, and aim owns
/// two live VECTORs. Repeats the pointer arguments, changes GTE state through
/// normalization/rotation, and captures the live player slot and grid view.
/// Expands to a statement sequence; use only at a standalone braced call site.
#define GOLEM_PAWN_ROOK_BUILD_BURST_AIM(root, aim, playerPart)                                 \
    (playerPart)              = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[2]; \
    (aim)->playerDirection.vx = (playerPart)->workm.t[0] - (root)->workm.t[0];                 \
    (aim)->playerDirection.vy = (playerPart)->workm.t[1] - (root)->workm.t[1];                 \
    (aim)->playerDirection.vz = (playerPart)->workm.t[2] - (root)->workm.t[2];                 \
    VectorNormal(&(aim)->playerDirection, &(aim)->normalizedAim);                              \
    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &(aim)->normalizedAim, &(aim)->playerDirection);

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GolemPawnRookBurstAimScratch);
    work    = actor->work;
    root    = actor->extra.tmd->coords;
    switch (work->step) {
        case GOLEM_PAWN_ROOK_BURST_OPENING:
            if (work->animFrame >= GOLEM_PAWN_ROOK_BURST_OPENING_FRAMES) {
                work->step  = GOLEM_PAWN_ROOK_BURST_BACKOFF;
                work->anim  = GOLEM_PAWN_ROOK_BURST_BACKOFF_ANIM;
                work->timer = 0;
            }
            break;
        case GOLEM_PAWN_ROOK_BURST_BACKOFF:
            work->forwardSpeed          = GOLEM_PAWN_ROOK_BURST_BACKOFF_SPEED;
            work->turnRate              = GOLEM_PAWN_ROOK_BURST_BACKOFF_TURN;
            work->shieldRaised          = work->shieldHp > 0;
            scratch->playerDirection.vx = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
            scratch->playerDirection.vz = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
            work->targetYaw             = ratan2((s16)scratch->playerDirection.vx, (s16)scratch->playerDirection.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            _golemPawnRookAimLaserSight(actor);
            work->timer++;
            if (work->timer >= GOLEM_PAWN_ROOK_BACKOFF_FRAMES) {
                work->step             = GOLEM_PAWN_ROOK_BURST_CHECK_AIM;
                work->timer            = 0;
                work->laserBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            break;
        case GOLEM_PAWN_ROOK_BURST_CHECK_AIM:
            work->shieldRaised = work->shieldHp > 0;
            GOLEM_PAWN_ROOK_BUILD_BURST_AIM(root, scratch, playerPart);
            directionX = scratch->playerDirection.vx;
            directionZ = scratch->playerDirection.vz;
            if (SquareRoot0((directionX * directionX) + (directionZ * directionZ)) < GOLEM_PAWN_ROOK_BURST_MIN_HORIZONTAL_AIM) {
                work->behavior     = GOLEM_PAWN_ROOK_BEHAVIOR_LAUNCHER_STRIKE;
                work->step         = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                work->anim         = GOLEM_PAWN_ROOK_LUNGE_ANIM;
                work->shieldRaised = 0;
                break;
            }
            yawDelta     = (ratan2((s16)scratch->playerDirection.vx, (s16)scratch->playerDirection.vz) & ACTOR_TRANSFORM_ANGLE_MASK) - work->yaw;
            yawMagnitude = __builtin_abs(yawDelta);
            if (yawMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                yawError = yawMagnitude;
            } else if (yawDelta > 0) {
                yawError = ACTOR_TRANSFORM_ANGLE_TURN - yawDelta;
            } else {
                yawError = yawDelta + ACTOR_TRANSFORM_ANGLE_TURN;
            }
            if (yawError >= GOLEM_PAWN_ROOK_BURST_CANCEL_YAW) {
                work->step         = GOLEM_PAWN_ROOK_BURST_CANCEL;
                work->anim         = GOLEM_PAWN_ROOK_BURST_CANCEL_ANIM;
                work->shieldRaised = 0;
            } else {
                work->step            = GOLEM_PAWN_ROOK_BURST_FIRE;
                work->anim            = GOLEM_PAWN_ROOK_BURST_FIRE_ANIM;
                work->attackActive    = 1;
                work->fireRequest     = 1;
                work->interruptDamage = 0;
                work->shotsSinceReload++;
                work->burstShots++;
            }
            break;
        case GOLEM_PAWN_ROOK_BURST_FIRE:
            work->forwardSpeed = 0;
            work->shieldRaised = work->shieldHp > 0;
            if (work->animFrame < GOLEM_PAWN_ROOK_BURST_FIRE_LOCK_FRAMES) {
                work->turnRate = 0;
            } else {
                GOLEM_PAWN_ROOK_BUILD_BURST_AIM(root, scratch, playerPart);
                work->targetYaw = ratan2((s16)scratch->playerDirection.vx, (s16)scratch->playerDirection.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                work->turnRate  = GOLEM_PAWN_ROOK_BURST_FIRE_TURN;
            }
            if (work->burstShots != 0 && work->animFrame == gGolemPawnRookAnimBlendFrames[GOLEM_PAWN_ROOK_BURST_FIRE_ANIM] - 1) {
                work->fireRequest = 1;
                work->shotsSinceReload++;
                work->burstShots++;
            }
            if (work->interruptDamage >= GOLEM_PAWN_ROOK_BURST_INTERRUPT_HP) {
                work->behavior          = GOLEM_PAWN_ROOK_BEHAVIOR_STAGGER;
                work->step              = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                work->forwardSpeed      = 0;
                work->turnRate          = 0;
                work->attackActive      = 0;
                work->shieldRaised      = 0;
                work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else if (work->shotsSinceReload >= GOLEM_PAWN_ROOK_BURST_SHOTS_PER_RELOAD) {
                if (work->animFrame >= gGolemPawnRookAnimBlendFrames[GOLEM_PAWN_ROOK_BURST_FIRE_ANIM] + GOLEM_PAWN_ROOK_BURST_END_FRAMES) {
                    work->step                                                     = GOLEM_PAWN_ROOK_BURST_RELOAD;
                    work->anim                                                     = GOLEM_PAWN_ROOK_BURST_RELOAD_ANIM;
                    work->shotsSinceReload                                         = 0;
                    work->burstShots                                               = 0;
                    gGolemPawnRookAnimBlendFrames[GOLEM_PAWN_ROOK_BURST_FIRE_ANIM] = 0;
                    work->attackActive                                             = 0;
                }
            } else if (work->burstShots < GOLEM_PAWN_ROOK_STRIKE_LIMIT) {
                if (work->animFrame >= gGolemPawnRookAnimBlendFrames[GOLEM_PAWN_ROOK_BURST_FIRE_ANIM] + GOLEM_PAWN_ROOK_BURST_REPEAT_BLEND_FRAMES) {
                    gGolemPawnRookAnimBlendFrames[GOLEM_PAWN_ROOK_BURST_FIRE_ANIM] = GOLEM_PAWN_ROOK_BURST_REPEAT_BLEND_FRAMES;
                    work->anim                                                     = GOLEM_PAWN_ROOK_BURST_FIRE_ANIM;
                    work->playingAnim                                              = GOLEM_PAWN_ROOK_BURST_BACKOFF_ANIM;
                }
            } else if (work->animFrame >= gGolemPawnRookAnimBlendFrames[GOLEM_PAWN_ROOK_BURST_FIRE_ANIM] + GOLEM_PAWN_ROOK_BURST_END_FRAMES) {
                work->step                                                     = GOLEM_PAWN_ROOK_BURST_BACKOFF;
                work->burstShots                                               = 0;
                work->anim                                                     = GOLEM_PAWN_ROOK_BURST_BACKOFF_ANIM;
                gGolemPawnRookAnimBlendFrames[GOLEM_PAWN_ROOK_BURST_FIRE_ANIM] = 0;
                work->attackActive                                             = 0;
            }
            break;
        case GOLEM_PAWN_ROOK_BURST_RELOAD:
            if (work->animFrame == GOLEM_PAWN_ROOK_BURST_RELOAD_EFFECT_FRAME) {
                effectSpawn(EFFECT_RELOAD_EMITTER, &actor->extra.tmd->coords[7], GOLEM_PAWN_ROOK_BURST_RELOAD_ARGUMENT, NULL);
            }
            work->shieldRaised = 0;
            if (work->animFrame >= GOLEM_PAWN_ROOK_BURST_RELOAD_FRAMES) {
                work->step = GOLEM_PAWN_ROOK_BURST_FINISH;
            }
            break;
        case GOLEM_PAWN_ROOK_BURST_FINISH:
            work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
            work->step     = GOLEM_PAWN_ROOK_ENGAGE_LISTEN_STEP;
            work->anim     = GOLEM_PAWN_ROOK_ANIM_LISTEN;
            break;
        case GOLEM_PAWN_ROOK_BURST_CANCEL:
            if (work->animFrame >= GOLEM_PAWN_ROOK_BURST_CANCEL_FRAMES) {
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                work->anim     = GOLEM_PAWN_ROOK_ANIM_WALK;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemPawnRookBurstAimScratch);
}

#undef GOLEM_PAWN_ROOK_BUILD_BURST_AIM
