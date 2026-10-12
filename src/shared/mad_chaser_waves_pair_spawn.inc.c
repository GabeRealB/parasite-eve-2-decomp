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

/// Assigns one spawned Sucklerceph its encounter identity, palette and one HP.
///
/// `member` is a non-NULL Enemy* expression evaluated once, with a live TMD
/// task. Overwrites the caller's `enemy`, `enemyTask` and `model` locals and
/// reads the `OVERLAY_ENCOUNTER_PAIR_*` constants. Consumes and increments
/// `gActor342400EncounterNextPlaceIndex` modulo 65536; the u16 place key retains
/// only the counter's low four bits shifted by `ENEMY_PLACE_INDEX_SHIFT`.
/// Model texture/CLUT offsets are set before HP. Borrows the enemy and model;
/// neither is allocated or retained here. Expands to one statement.
#define OVERLAY_ENCOUNTER_PREPARE_PAIR_MEMBER(member)                                     \
    do {                                                                                  \
        enemy           = (member);                                                       \
        enemy->placeKey = gActor342400EncounterNextPlaceIndex << ENEMY_PLACE_INDEX_SHIFT; \
        gActor342400EncounterNextPlaceIndex++;                                            \
        enemyTask                = enemy->task;                                           \
        model                    = enemyTask->extra.tmd;                                  \
        model->texturePageOffset = OVERLAY_ENCOUNTER_PAIR_TEXTURE_PAGE_OFFSET;            \
        model->clutRowOffset     = OVERLAY_ENCOUNTER_PAIR_CLUT_ROW_OFFSET;                \
        enemy->hp                = OVERLAY_ENCOUNTER_PAIR_INITIAL_HP;                     \
    } while (0)

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(waveTask);
        return;
    }
    // The spawner owns its work; the two independently spawned enemies are borrowed.
    waveTask->work = work;
    work->enemy0   = enemySpawnFromTable(gActor207000SucklercephTaskDescs, OVERLAY_ENCOUNTER_PAIR_DESCRIPTOR_INDEX, OVERLAY_ENCOUNTER_PAIR_SOUND_VARIANT, 0);
    work->enemy1   = enemySpawnFromTable(gActor207000SucklercephTaskDescs, OVERLAY_ENCOUNTER_PAIR_DESCRIPTOR_INDEX, OVERLAY_ENCOUNTER_PAIR_SOUND_VARIANT, 0);
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
    _gActor342400EncounterSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_LIVE;
    waveTask->state++;
}

#undef OVERLAY_ENCOUNTER_PREPARE_PAIR_MEMBER
