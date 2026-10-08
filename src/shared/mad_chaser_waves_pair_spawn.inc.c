/* Part of the scripted encounter library; see mad_chaser_waves.h. */

/// Allocates a pair spawner and creates up to two one-HP Sucklercephs.
///
/// Requires the loaded Sucklerceph descriptor and a spawn argument whose signed
/// high halfword is a valid encounter-row index (0..16 in this carrier).
/// The low halfword is retained for later reveal stages. Allocation failure
/// or two failed enemy spawns kills the spawner without marking its row live.
/// Task teardown owns the zeroed primary-heap work; it borrows any spawned
/// enemies until the later death/cull stages forget them. Successful enemies
/// receive sequential placement keys and the encounter palette offsets. The
/// placement halfword retains only the counter's low four bits in its index.
static void _overlayEncounterPairSpawn(Task* waveTask)
{
    enum {
        OVERLAY_ENCOUNTER_PAIR_DESCRIPTOR_INDEX = 1,
        OVERLAY_ENCOUNTER_PAIR_SOUND_VARIANT    = 1,
        OVERLAY_ENCOUNTER_PAIR_INITIAL_HP       = 1,
    };
    OverlayEncounterPairWork* work;
    Enemy*                    enemy;
    Task*                     enemyTask;
    TmdObject*                model;

/// Applies the encounter identity, palette and HP to one live Sucklerceph.
///
/// Used only in this spawner. Evaluates member once; captures its enemy,
/// enemyTask and model locals, gMadChaserWaveEnemyCount and the pair constants.
/// The member and its model task must be live. Expands to one compound statement.
#define OVERLAY_ENCOUNTER_PREPARE_PAIR_MEMBER(member)                          \
    {                                                                          \
        enemy           = (member);                                            \
        enemy->placeKey = gMadChaserWaveEnemyCount << ENEMY_PLACE_INDEX_SHIFT; \
        gMadChaserWaveEnemyCount++;                                            \
        enemyTask                = enemy->task;                                \
        model                    = enemyTask->extra.tmd;                       \
        model->texturePageOffset = OVERLAY_ENCOUNTER_PAIR_TEXTURE_PAGE_OFFSET; \
        model->clutRowOffset     = OVERLAY_ENCOUNTER_PAIR_CLUT_ROW_OFFSET;     \
        enemy->hp                = OVERLAY_ENCOUNTER_PAIR_INITIAL_HP;          \
    }

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(waveTask);
        return;
    }
    // The spawner owns its work; the two independently spawned enemies are borrowed.
    waveTask->work = work;
    work->enemy0   = enemySpawnFromTable(&D_actor_207000_80151E60, OVERLAY_ENCOUNTER_PAIR_DESCRIPTOR_INDEX, OVERLAY_ENCOUNTER_PAIR_SOUND_VARIANT, 0);
    work->enemy1   = enemySpawnFromTable(&D_actor_207000_80151E60, OVERLAY_ENCOUNTER_PAIR_DESCRIPTOR_INDEX, OVERLAY_ENCOUNTER_PAIR_SOUND_VARIANT, 0);
    if (work->enemy0 == NULL && work->enemy1 == NULL) {
        taskKill(waveTask);
        return;
    }
    if (work->enemy0 != NULL) {
        OVERLAY_ENCOUNTER_PREPARE_PAIR_MEMBER(work->enemy0);
    }
    if (work->enemy1 != NULL) {
        OVERLAY_ENCOUNTER_PREPARE_PAIR_MEMBER(work->enemy1);
    }
    gMadChaserWaveSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_LIVE;
    waveTask->state++;
}

#undef OVERLAY_ENCOUNTER_PREPARE_PAIR_MEMBER
