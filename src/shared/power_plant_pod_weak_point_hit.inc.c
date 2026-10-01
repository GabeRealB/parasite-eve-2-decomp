/* Part of the power plant pod library; see power_plant_pod.h. */

/// Per-frame hit handling of the weak point: distance-scaled damage with
/// critical rolls and hit effects/sounds. On death it marks the body's
/// field_336, spawns the burst effects and plays the break sound.
void podWeakPointHit(Enemy* arg0, Task* arg1)
{
    VECTOR*         vec;
    Actor05300Part* part;
    GfxCoord*       coord;
    s32             damage;
    s32             snd;
    s32             hitTime;

    coord = arg1->extra.tmd->coords;
    part  = (Actor05300Part*)arg1->work;
    switch (Gp_StateF0.field_4) {
        case 1:
            return;
        case 0:
            arg0->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
            break;
        case 2:
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    vec = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    if (part->field_40 != 0) {
        part->field_40--;
        if (part->field_40 <= 0) {
            part->field_40 = 0;
        }
    }
    if (part->field_44 != 0) {
        part->field_44--;
    }
    if (part->field_40 == 0 && (part->rec18[0].key.value & 0xFFFF0000) == 0x20000) {
        if (part->rec18[0].key.value & 0x8000) {
            func_800DA6E8(&arg0->node, 0, 0);
        } else {
            vec->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vec->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
            vec->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            damage  = Gp_ComputeDamage(part->rec18[0].key.value, SquareRoot0(vec->vx * vec->vx + vec->vy * vec->vy + vec->vz * vec->vz), 0, 0);
            if (Gp_RollEnemyChance(arg0, part->rec18[0].key.value, 0) != 0) {
                damage *= 4;
                Gp_SpawnEff(0x6009C, coord, 0, NULL);
            }
            func_800DA6E8(&arg0->node, damage, 0);
            arg0->hp -= damage;
            if (arg0->hp <= 0) {
                arg1->state                                      = 2;
                part->field_42                                   = 0;
                ((Actor05300Work*)arg1->parent->work)->field_336 = 1;
                Gp_SpawnEff(0x6005C, coord, 0x10002400, NULL);
                Gp_SpawnEff(0x60070, coord, 0x32FF1400, NULL);
                snd  = gPodSoundIds[1];
                snd |= (arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)gpGetObjDepth(coord));
            } else if (damage > 0) {
                if (part->field_44 == 0) {
                    if ((Gp_GetIdParam0(part->rec18[0].key.value) & 0xFFFF) == 7) {
                        func_800FDB18(3, coord, NULL, &part->field_38);
                    }
                    func_800FDB18(7, coord, NULL, &part->field_38);
                    part->field_44 = 10;
                }
                hitTime = Gp_GetIdParam2(part->rec18[0].key.value);
                if (hitTime > 0) {
                    part->field_40 = hitTime;
                }
                snd  = gPodSoundIds[0];
                snd |= (arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)gpGetObjDepth(coord));
            }
        }
    }
    Gp_ClearRec18Occupied(part->rec18);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}
