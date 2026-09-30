/* Part of the burster library; see burster.h. */

/// Contact handler of the first enemy, with 0x4C bytes of scratch. The
/// `func_800E0C10` push-back from its contact table moves the root (response
/// 1) or restores the position the last step started from (response 2), and
/// the hit cooldown ticks down. Coming within 0x320 of the player moves a live
/// enemy to its dying stage. Each of the four contacts is then handled by
/// class: 0x10000 does the same, 0x20000 (outside the cooldown) either kills
/// the enemy outright on a critical roll or applies the damage, the id's side
/// effect, the hit effect and the id's cooldown, and 0x30000 pushes the root
/// out of the wall along the contact normal while the enemy walks. The table
/// is released, and a flagged hit on the third body's record clears that
/// body's 0x8000 bit.
void bursterContacts(Task* arg0)
{
    Enemy*                 enemy;
    WorldCollisionContact* effectRec;
    s32                    effect;
    s32                    pushY;
    s32                    movement;
    s32                    dx;
    s32                    dz;
    s32                    wallDx;
    s32                    wallDz;
    s16                    hitCooldown;
    s32                    distance;
    u32                    damage;
    Actor104600Work*       work;
    GfxCoord*              coord;
    ActorContactFrame*     scratch;
    s32                    i;

    work     = (Actor104600Work*)arg0->work;
    scratch  = (ActorContactFrame*)SCRATCH_STACK_RESERVE_BYTES(0x4C);
    coord    = arg0->extra.tmd->coords;
    enemy    = arg0->spawnArg2.pointer;
    movement = func_800E0C10(work->rec154, &scratch->delta, 4, &scratch->result);
    switch (movement) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += scratch->delta.vx.h.hi;
            coord->coord.t[1] += scratch->delta.vy.h.hi;
            coord->coord.t[2] += scratch->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_274.vx;
            coord->coord.t[1] = work->field_274.vy;
            coord->coord.t[2] = work->field_274.vz;
            break;
    }
    if (work->field_2CE != 0) {
        work->field_2CE--;
        if (work->field_2CE <= 0) {
            work->field_2CE = 0;
        }
    }
    dx                  = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    scratch->delta.vx.w = dx;
    scratch->delta.vy.w = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    dz                  = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    scratch->delta.vz.w = dz;
    distance            = SquareRoot0((dx * dx) + (dz * dz));
    if (distance < 0x320 && work->field_2C8 == 1) {
        work->field_2D2 = 1;
        work->field_2C8 = 2;
    }
    for (i = 0; i < 4; i++) {
        switch (work->rec154[i].key.value & 0xFFFF0000) {
            case 0x10000:
                if (work->field_2C8 == 1) {
                    work->field_2D2 = 1;
                    work->field_2C8 = 2;
                }
                break;
            case 0x20000:
                if (work->field_2CE == 0) {
                    damage = Gp_ComputeDamage(work->rec154[i].key.value, distance, 0, 0);
                    if (Gp_RollEnemyChance(arg0->spawnArg2.pointer, work->rec154[i].key.value, 0) != 0) {
                        bursterKill(arg0, 1);
                        arg0->killCountdown = 5;
                        arg0->state         = 2;
                        work->field_2B4     = 0;
                        enemy->hp           = -1;
                    } else {
                        func_800E2C78(enemy, work->rec154[i].key.value, damage, 0);
                        bursterTakeDamage(arg0, damage);
                        effect = Gp_GetIdParam0(work->rec154[i].key.value) & 0xFFFF;
                        switch (effect) {
                            case 1:
                                work->field_2D2 = 1;
                                work->field_2C8 = 2;
                                break;
                            case 3:
                                Gp_SetObjFlag4(enemy, work->rec154[i].key.value, 0);
                                break;
                            case 2:
                            case 9:
                                Gp_SetObjFlag2(enemy, work->rec154[i].key.value, 0);
                                break;
                        }
                        if (enemy->hp > 0) {
                            func_800FDB18(Gp_GetIdParam1(work->rec154[i].key.value) & 0xFFFF, arg0->extra.tmd->coords + 1, NULL, &work->field_284);
                        }
                        hitCooldown = Gp_GetIdParam2(work->rec154[i].key.value);
                        if (hitCooldown > 0) {
                            work->field_2CE = hitCooldown;
                        }
                    }
                }
                break;
            case 0x30000:
                wallDx              = coord->workm.t[0] - work->rec154[i].point.vx;
                scratch->delta.vy.w = 0;
                scratch->delta.vx.w = wallDx;
                wallDz              = coord->workm.t[2] - work->rec154[i].point.vz;
                scratch->delta.vz.w = wallDz;
                distance            = SquareRoot0((wallDx * wallDx) + (wallDz * wallDz));
                distance            = work->rec154[i].distance - distance;
                distance            = (distance <= 0) ? 0 : distance;
                scratch->delta.vx.w = coord->workm.t[0] - work->rec154[i].point.vx;
                scratch->delta.vy.w = coord->workm.t[1] - work->rec154[i].point.vy;
                scratch->delta.vz.w = coord->workm.t[2] - work->rec154[i].point.vz;
                VectorNormal((VECTOR*)&scratch->delta, &scratch->normal);
                ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &scratch->normal, (VECTOR*)&scratch->delta);
                if (work->field_2B8 == 1 || work->field_2B8 == 2) {
                    coord->coord.t[0] += (distance * scratch->delta.vx.w) >> 12;
                    pushY              = distance * scratch->delta.vy.w;
                    if (pushY < 0) {
                        coord->coord.t[1] += pushY >> 12;
                    }
                    coord->coord.t[2] += (distance * scratch->delta.vz.w) >> 12;
                }
                break;
        }
    }
    Gp_ClearRec18Occupied(work->rec154);
    effectRec = &work->rec1D4;
    if ((work->field_2C8 != 0) && (Gp_FindRec18(effectRec, 0) != 0)) {
        work->obj1B4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(effectRec);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x4C);
}
