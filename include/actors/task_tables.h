#ifndef INCLUDE_ACTORS_TASK_TABLES_H
#define INCLUDE_ACTORS_TASK_TABLES_H

#include "main/task_types.h"

// Room placement records address these tables in the currently loaded actor
// slots. Different packages occupy those slots over the lifetime of a stage.
/// Standing Sucklerceph TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor04600SucklercephTaskDesc;

/// Skull Stalker TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor04600SkullStalkerTaskDesc;

/// Black Beetle TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor03800BlackBeetleTaskDesc;

/// Bat TMD spawn descriptor; priority 96, with automatic model buffers disabled.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor03700BatTaskDesc;

/// Amoeba spawn descriptors: 0 TMD body, 1 coordinate-body fireball.
///
/// Spawn indices are 0..1, without a terminator. Keep the callbacks and
/// borrowed model geometry loaded while their tasks are in use.
extern TaskDesc gActor02400Tasks[2];

/// Beam Sword Rook GOLEM spawn descriptors: 0 body, 1 sword, 2 shield; all are TMD bodies.
///
/// Spawn indices are 0..2, without a terminator. Keep the callbacks and
/// borrowed model geometry loaded while their tasks are in use.
extern TaskDesc gActor02300RookGolemTasks[3];

/// Watcher TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor02100WatcherTaskDesc;

/// Mossback spawn descriptors: 0 body, 1 coordinate-body spit glob, 2 coordinate-body spit cloud.
///
/// Spawn indices are 0..2, without a terminator. Keep the callbacks and
/// borrowed model geometry loaded while their tasks are in use.
extern TaskDesc gActor01100Tasks[3];

/// Desert Chaser TMD spawn descriptor; priority 96, with automatic model buffers disabled.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor00100DesertChaserTaskDesc;

/// Laboratory terminal task table: slot 0 ends a placement task, slot 1 runs login/menu/quiz control.
///
/// Slot 1 borrows a persistent `ScreenFade` as its second spawn argument and
/// requests its return on initialization; the actor overlay must remain loaded.
extern TaskDesc gActor143000TerminalTasks[2];

/// Slot export of `gActor02100WatcherTaskDesc` from actor_102100, with the same descriptor contract.
extern TaskDesc gActor102100WatcherTaskDesc;

extern TaskDesc D_actor_142600_80135E24;

/// Slot export of `gActor02400Tasks` from actor_102400, with the same descriptor contract.
extern TaskDesc gActor102400Tasks[2];

extern TaskDesc D_actor_123200_80137234;

extern TaskDesc D_actor_142900_80137600[];

/// Slot export of `gActor04600SucklercephTaskDesc` from actor_104600, with the same descriptor contract.
extern TaskDesc gActor104600SucklercephTaskDesc;

/// Slot export of `gActor02500Tasks` from actor_102500, with the same descriptor contract.
extern TaskDesc gActor102500Tasks[2];

/// Bodyless observatory scene tasks: 0 immediate stop, 1 companion puffs, 2 player head aim.
///
/// Loaded with the actor_450200 package through the observatory's area resources.
/// Direct spawns use entries 1 and 2; entry 0 kills its task on first dispatch.
/// The table has no terminator, and the task callbacks require the scene's actors
/// and room resources to remain loaded while they run.
extern TaskDesc gActor450200SceneTaskDescs[3];

/// Slot export of `gActor03800BlackBeetleTaskDesc` from actor_103800, with the same descriptor contract.
extern TaskDesc gActor103800BlackBeetleTaskDesc;

/// Slot export of `gActor04600SkullStalkerTaskDesc` from actor_104600, with the same descriptor contract.
extern TaskDesc gActor104600SkullStalkerTaskDesc;

extern TaskDesc Actor00700_D06E60;

extern TaskDesc D_actor_101200_80138E98[];

extern TaskDesc Actor00700_D075A8;

extern TaskDesc D_actor_511000_80139924[];

/// Slot export of `gActor03700BatTaskDesc` from actor_103700, with the same descriptor contract.
extern TaskDesc gActor103700BatTaskDesc;

extern TaskDesc D_actor_107000_80139E60[];

extern TaskDesc gActor110800ViewFigureTasks[];

extern TaskDesc D_actor_461800_80139F8C[];

extern TaskDesc gActor110300ViewFigureTasks[];

extern TaskDesc D_actor_111800_8013A468;

extern TaskDesc D_actor_135400_8013A4AC[];

extern TaskDesc gMaggotCaterpillarBodyTask;

extern TaskDesc D_actor_105500_8013A8DC[];

extern TaskDesc D_actor_113000_8013ABB4;

extern TaskDesc D_actor_135600_8013B0C4[];

/// Mind Suckler TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor01500MindSucklerTaskDesc;

extern TaskDesc D_actor_110700_8013BF94;

extern TaskDesc D_actor_150400_8013C8F4[];

/// Descriptor indices shared by the Beta and Proto Generator spawn tables.
///
/// Both tables contain exactly two recipes and have no terminator.
enum {
    GENERATOR_BODY_TASK_INDEX         = 0,
    GENERATOR_LIFE_SUPPORT_TASK_INDEX = 1,
    GENERATOR_TASK_DESC_COUNT         = 2
};

/// Proto Generator body and Life Support spawn recipes exported by actor_105400.
///
/// Entry `GENERATOR_BODY_TASK_INDEX` creates the TMD body; entry
/// `GENERATOR_LIFE_SUPPORT_TASK_INDEX` creates its coordinate-only child.
/// Both execute at priority 96 and receive a live `Enemy` in spawn argument 2
/// through `enemySpawnFromTable`. The child requires its body task as parent.
/// The actor overlay and the body's borrowed model data must remain loaded.
extern TaskDesc gActor105400GeneratorTasks[GENERATOR_TASK_DESC_COUNT];

extern TaskDesc D_actor_151000_8013D2E0;

extern TaskDesc D_actor_121300_8013D390[];

/// Beta Generator body and Life Support spawn recipes exported by actor_105300.
///
/// Entry `GENERATOR_BODY_TASK_INDEX` creates the TMD body; entry
/// `GENERATOR_LIFE_SUPPORT_TASK_INDEX` creates its coordinate-only child.
/// Both execute at priority 96 and receive a live `Enemy` in spawn argument 2
/// through `enemySpawnFromTable`. The child requires its body task as parent.
/// The actor overlay and the body's borrowed model data must remain loaded.
extern TaskDesc gActor105300GeneratorTasks[GENERATOR_TASK_DESC_COUNT];

extern TaskDesc D_actor_141000_8013D77C;

extern TaskDesc D_actor_535700_8013DADC;

/// Twenty-part model task for the sterilization room's placed scene actor.
///
/// Creates a TMD body at priority 96 and runs the paced walker's spawn/frame
/// states. Scene placement supplies a live owning `Enemy` through `spawnArg2.pointer`.
/// The `actor_160600` model, clips and callbacks must remain loaded until teardown.
extern TaskDesc gActor160600PlacedActorTask;

/// Blood Suckler TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor04000BloodSucklerTaskDesc;

extern TaskDesc D_actor_120400_8013E748[];

extern TaskDesc D_actor_420700_8013EF68[];

extern TaskDesc D_actor_107000_8013F5F0[];

extern TaskDesc D_actor_451100_8013F734;

extern TaskDesc D_actor_135400_8013F8D8;

extern TaskDesc D_actor_136100_80140744[];

extern TaskDesc D_actor_143900_801413EC;

extern TaskDesc D_actor_105100_80141464[];

extern TaskDesc D_actor_146300_801427C8[];

extern TaskDesc D_actor_461800_801437EC;

extern TaskDesc D_actor_113100_80144308[];

/// Slot export of `gActor01600ScavengerTaskDesc` from actor_101600, with the same descriptor contract.
extern TaskDesc gActor101600ScavengerTaskDesc;

extern TaskDesc gPairWalkTasks[];

extern TaskDesc D_actor_511000_801472E8[];

/// Slot export of `gActor01100Tasks` from actor_101100, with the same descriptor contract.
extern TaskDesc gActor101100Tasks[3];

/// Slot export of `gActor02300RookGolemTasks` from actor_102300, with the same descriptor contract.
extern TaskDesc gActor102300RookGolemTasks[3];

extern TaskDesc Actor02000_D15FD0[];

extern TaskDesc D_actor_100300_80148110[];

extern TaskDesc gActor460200StrideWalkTasks[];

/// Slot export of `gActor05600PawnGolemTasks` from actor_105600, with the same descriptor contract.
extern TaskDesc gActor105600PawnGolemTasks[3];

/// Boss Stranger task recipe: nineteen-part TMD body, priority 96 and actor-managed buffers.
///
/// The callback and model data belong to actor_110600 and must stay loaded
/// while its tasks live. Area resources select this descriptor at task index 0.
extern TaskDesc gActor110600TaskDesc;

extern TaskDesc D_actor_101900_80149120[];

/// Launcher Pawn GOLEM spawn descriptors: 0 body, 1 launcher, 2 grenade; all are TMD bodies.
///
/// Indices select live descriptors without a terminator. Keep this package
/// loaded while its callbacks and borrowed model geometry are in use.
extern TaskDesc gActor05600PawnGolemTasks[3];

extern TaskDesc D_actor_143900_80149664[];

extern TaskDesc D_actor_450800_8014AC88[];

extern TaskDesc D_actor_205200_8014CA60[];

/// Slot export of `gActor02100WatcherTaskDesc` from actor_202100, with the same descriptor contract.
extern TaskDesc gActor202100WatcherTaskDesc;

/// Slot export of `gActor02400Tasks` from actor_202400, with the same descriptor contract.
extern TaskDesc gActor202400Tasks[2];

extern TaskDesc D_actor_451100_8014E6E4[];

extern TaskDesc D_actor_207200_8014E7A4;

/// Slot export of `gActor04600SucklercephTaskDesc` from actor_204600, with the same descriptor contract.
extern TaskDesc gActor204600SucklercephTaskDesc;

/// Scorpion spawn descriptors: 0 TMD body, 1 coordinate-body corpse poison.
///
/// Indices select live descriptors without a terminator. Keep this package
/// loaded while its callbacks and borrowed model geometry are in use.
extern TaskDesc gActor02500Tasks[2];

/// Slot export of `gActor03800BlackBeetleTaskDesc` from actor_203800, with the same descriptor contract.
extern TaskDesc gActor203800BlackBeetleTaskDesc;

/// Slot export of `gActor04600SkullStalkerTaskDesc` from actor_204600, with the same descriptor contract.
extern TaskDesc gActor204600SkullStalkerTaskDesc;

extern TaskDesc D_actor_223600_80150B48;

extern TaskDesc D_actor_200700_80150C80;

extern TaskDesc D_actor_201200_80150E98[];

extern TaskDesc D_actor_421600_80151254;

extern TaskDesc Actor00700_D075A8;

extern TaskDesc D_actor_405800_801514CC;

/// Spawn recipe for the Zebra Stalker body task, running at priority 96.
///
/// Requires this actor overlay's model, animation records and code to remain
/// loaded until the body and its arm tasks are released. The callback takes
/// ownership of the Enemy in spawnArg2.pointer. The high nibble of spawnArg1's
/// low byte selects 0 ordinary, 1 water, 2 group drop, 3 Junk Yard, 4 floor drop
/// or 5 delayed group entrance; the low nibble is ignored.
extern TaskDesc gActor400600ZebraStalkerTaskDesc;

/// Slot export of `gActor03700BatTaskDesc` from actor_203700, with the same descriptor contract.
extern TaskDesc gActor203700BatTaskDesc;

extern TaskDesc D_actor_213100_801521A8[];

extern TaskDesc D_actor_202600_801528D4[];

extern TaskDesc D_actor_205500_801528DC[];

extern TaskDesc gPairWalkTasks[];

/// Gray Stalker body-task spawn recipe exported to Dryfield underpass resources.
///
/// Creates a TMD body at execution priority 96. The `actor_400500` package and
/// its geometry must remain loaded throughout the body and child-arm lifetimes.
extern TaskDesc gActor400500TaskDesc;

/// Creeping Stranger TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor207200CreepingStrangerTaskDesc;

extern TaskDesc D_actor_403900_801540E0;

/// Knight GOLEM model-task descriptor exported to room resource tables.
///
/// Attaches the nineteen-part GOLEM body at task priority 96; the second spawn
/// argument borrows the live Enemy through spawn, living frames and corpse frames.
extern TaskDesc gActor402200GolemTaskDesc;

extern TaskDesc D_actor_260400_80154C18[];

/// Odd Stranger spawn recipe exported by actor_401000 to room resources.
///
/// Uses a TMD body at priority 96 and defers primitive-buffer allocation
/// until the chosen opening state needs drawing. Borrows a live `Enemy` as
/// spawn argument 2; the actor overlay and its model data must remain loaded.
extern TaskDesc gActor401000StrangerTaskDesc;

extern TaskDesc D_actor_511000_80155070[];

extern TaskDesc D_actor_403100_8015560C[];

/// Odd Stranger TMD spawn descriptor; priority 96, with automatic model buffers disabled.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor401800OddStrangerTaskDesc;

/// Slot export of `gActor04000BloodSucklerTaskDesc` from actor_204000, with the same descriptor contract.
extern TaskDesc gActor204000BloodSucklerTaskDesc;

extern TaskDesc D_actor_205200_801567C4;

extern TaskDesc D_actor_202900_80156E24[];

extern TaskDesc D_actor_213000_80157DE0[];

extern TaskDesc D_actor_210700_801585CC;

/// Spawn recipes for Pierce in shelter room 47 and his attached held model.
///
/// Entry 0 creates the twenty-part body without an automatic primitive buffer;
/// its second spawn argument must point to the live owning `Enemy`. Entry 1
/// creates the held model: first argument is the parent part index (Pierce uses
/// 4), second argument is the live parent `Task*`. Both run at priority 192.
/// The table has two entries and no terminator. Keep `actor_443500` loaded for
/// the tasks' callbacks and borrowed model/animation resources.
extern TaskDesc gActor443500PierceTaskDescs[2];

extern TaskDesc D_actor_401300_80158A18;

extern TaskDesc D_actor_206100_80158B0C[];

extern TaskDesc D_actor_403000_80158D58;

extern TaskDesc D_actor_260500_80159DB0;

extern TaskDesc D_actor_210600_8015A4EC;

/// Scavenger TMD spawn descriptor; priority 96, with automatic model buffers disabled.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor01600ScavengerTaskDesc;

extern TaskDesc D_actor_215100_8015E5D0[];

/// Slot export of `gActor01100Tasks` from actor_201100, with the same descriptor contract.
extern TaskDesc gActor201100Tasks[3];

extern TaskDesc D_actor_521100_8015F6E4[];

extern TaskDesc D_actor_403200_8015F8D0;

/// Slot export of `gActor02300RookGolemTasks` from actor_202300, with the same descriptor contract.
extern TaskDesc gActor202300RookGolemTasks[3];

extern TaskDesc Actor02000_D15FD0[];

extern TaskDesc D_actor_200300_80160110[];

/// Slot export of `gActor05600PawnGolemTasks` from actor_205600, with the same descriptor contract.
extern TaskDesc gActor205600PawnGolemTasks[3];

extern TaskDesc D_actor_205700_801611F8[];

/// TMD host recipe for the garbage-incinerator Glutton, at execution priority 96.
///
/// The room borrows this descriptor from the loaded actor_444000 overlay.
/// Its model source and callback must remain loaded for the host's lifetime.
extern TaskDesc gActor444000GluttonHostTaskDesc;

extern TaskDesc D_actor_341300_80165208[];

/// Moth TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor300700MothTaskDesc;

extern TaskDesc D_actor_510900_80167A18[];

extern TaskDesc D_actor_301200_80168E98[];

extern TaskDesc D_actor_350500_80168EA4;

extern TaskDesc D_actor_311500_80169338;

extern TaskDesc D_actor_300700_801693AC;

extern TaskDesc D_actor_350700_80169D10;

extern TaskDesc D_actor_312200_80169F7C;

extern TaskDesc D_actor_521100_8016A388[];

extern TaskDesc gMaggotCaterpillarBodyTask;

extern TaskDesc D_actor_361100_8016BAE4;

extern TaskDesc D_actor_317000_8016CF44;

/// Slot export of `gActor04000BloodSucklerTaskDesc` from actor_304000, with the same descriptor contract.
extern TaskDesc gActor304000BloodSucklerTaskDesc;

extern TaskDesc D_actor_503500_8016E924[];

/// Gary Douglas's body, attached head-and-hat and gun spawn recipes (indices 0..2).
///
/// The night motel balcony resource selects body 0. Body initialization spawns
/// head 1 on part 4 and gun 2 on part 8, borrowing the body's lighting and
/// lifetime. All run at priority 192; only the body skips automatic model buffers.
/// The actor package's code and model data must remain loaded for all three tasks.
extern TaskDesc gActor335800GaryDouglasTasks[3];

extern TaskDesc D_actor_311900_8016EC00;

extern TaskDesc D_actor_311900_8016EC0C;

extern TaskDesc D_actor_350700_801708DC[];

extern TaskDesc D_actor_323400_8017120C;

extern TaskDesc D_actor_361100_80171BAC;

extern TaskDesc D_actor_323300_8017255C[];

/// Spawn recipe for the night motel balcony's independently placed Flint model.
///
/// The room resource addresses this single nineteen-part TMD recipe directly.
/// It runs at priority 192 and skips automatic model buffers; actor_335800's
/// code and geometry must stay loaded until Flint's task is released.
extern TaskDesc gActor335800FlintTaskDesc;

extern TaskDesc D_actor_356100_80173294;

extern TaskDesc D_actor_323000_80173A08;

extern TaskDesc D_actor_342400_80173A54[];

/// Slot export of `gActor01600ScavengerTaskDesc` from actor_301600, with the same descriptor contract.
extern TaskDesc gActor301600ScavengerTaskDesc;

extern TaskDesc D_actor_341700_80176354;

/// Scripted tentacle used by Shelter R48 resource slot 45.
///
/// Creates the nineteen-part actor_361100 model with explicit primitive-buffer
/// control, then handles placement, animation, translation and a burning collapse.
extern TaskDesc gActor503500TentacleTaskDesc;

/// Slot export of `gActor01100Tasks` from actor_301100, with the same descriptor contract.
extern TaskDesc gActor301100Tasks[3];

extern TaskDesc D_actor_302000_80177DF0[];

extern TaskDesc D_actor_301900_80179120[];

extern TaskDesc D_actor_310600_801796A4[];

extern TaskDesc gActor05700GolemPawnRookTasks[];

extern TaskDesc gActor02600MaggotCaterpillarBodyTask;

extern TaskDesc gActor450800PairWalkTasks[];

extern TaskDesc gActor535700PairWalkTasks[];

#endif // INCLUDE_ACTORS_TASK_TABLES_H
