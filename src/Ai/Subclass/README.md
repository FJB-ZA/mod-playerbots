# Sub-class paths

Each solo-wow sub-class path has its own folder here, the way each dungeon has one under `Ai/Dungeon`.
A path (for example Gladiator, a warrior path) follows its class's rotation code and adds its own spells
and its fourth talent tree.

## Adding a path

1. Make `Ai/Subclass/<Path>/` with:
   - `<Path>Actions.h/.cpp`: the path's spells, as actions;
   - `<Path>Triggers.h/.cpp`: when to cast them;
   - `<Path>ActionContext.h`, `<Path>TriggerContext.h`: their names;
   - `<Path>Strategy.h/.cpp`: the combat strategy, usually built on the class's generic strategy.
2. Register the two contexts in `SubclassContexts.h` and the strategy in `SubclassStrategyContext.h`.
3. In `AiFactory`, give the class's fourth page (`<CLASS>_TAB_<PATH>`) the strategy, as `WARRIOR_TAB_GLADIATOR`
   does.
4. In `conf/playerbots.conf.dist`, add the premade spec: its name, glyphs, links with the path's tree as the
   fourth `-` group, and `RandomClassSpecProb` 0. A bot takes the spec only while its slot follows the path.

Nothing else in the engine changes. The core decides which bots follow the path (`subclass_path.bot_share`).
