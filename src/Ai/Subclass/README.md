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

A path without a tree of its own, like the Ranger (a hunter without a pet), skips steps 3 and 4: `AiFactory` gives every
bot of the class its strategy, which casts only spells a plain bot never learns. The factory checks the core's
`SubclassPaths::ActivePath` for anything else, as it does to keep a Ranger without a pet or a Beast Mastery build.

Nothing else in the engine changes. The core decides which bots follow the path (`subclass_path.bot_share`).

A path can have both: the Necromancer's bots play the class AI on its trainer mirrors (Plague Strike for Immolate) and,
on its Blight tree, add `blight` beside Affliction's `affli` for the tree's own spells. `SubclassBotMgr` maps every
path's trainer mirrors; a replacement learned from a tree (Gladiator Stance) is left to the path's strategy.

The Ranger does the same: on its Windrunner tree its bots run Marksmanship's `mm` with `windrunner` beside it, which
builds and spends Advantage on the tree's finishers; a dip into Windrunner from another tree adds `windrunner` too.

The Witch Doctor's Hexcraft tree (`WitchDoctor/`) is the shaman's fourth page, `SHAMAN_TAB_HEXCRAFT`. Its bots run
Elemental's `ele` and totems with `hexcraft` beside it: Shade Puppet, Loa's Spite and Witchfire on cooldown, Echo Ward
before the Lightning Bolts it echoes, Choking Jinx beside Wind Shear, and Spirit Shroud, Loa's Ride, Mojo Golem and
Marionette Dance as cooldowns against bosses, elites, players and training dummies. Elemental's own tab checks (the
boost and AoE strategies, weapon imbue, stat weights, enchants, glyphs) take the Hexcraft tab as Elemental, since any
shaman tab past Restoration would otherwise fall into Enhancement's branch. A dip into Hexcraft adds `hexcraft` too.
The Witch Doctor's other spells are the shaman's under new names, which `SubclassBotMgr`'s aliases resolve.
