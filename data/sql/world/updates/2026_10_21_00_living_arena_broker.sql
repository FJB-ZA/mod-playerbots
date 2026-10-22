-- Arena partner broker beside "Techs" Rickard Rustbolt, the Dalaran Underbelly arena battlemaster.
DELETE FROM `creature_template` WHERE `entry` = 9101001;
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`,
    `speed_walk`, `speed_run`, `unit_class`, `unit_flags`, `type`, `BaseAttackTime`, `RangeAttackTime`,
    `HealthModifier`, `RegenHealth`, `ScriptName`)
VALUES (9101001, 'Vex Ironmantle', 'Arena Partner Broker', 80, 80, 2, 35, 1, 1, 1.14286, 1, 33024, 7, 2000, 2000, 1, 1,
    'npc_living_arena_broker');

DELETE FROM `creature_template_model` WHERE `CreatureID` = 9101001;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
VALUES (9101001, 0, 26386, 1, 1);

DELETE FROM `creature` WHERE `guid` = 9101001;
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `position_x`, `position_y`,
    `position_z`, `orientation`, `spawntimesecs`, `MovementType`)
VALUES (9101001, 9101001, 571, 4395, 0, 1, 1, 5815.26, 622.95, 610.666, 3.14159, 300, 0);

DELETE FROM `npc_text` WHERE `ID` BETWEEN 9101001 AND 9101006;
INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`) VALUES
(9101001, 'Need someone to watch your back in the ring? Half the gladiators in this sewer are between teams. Tell me what you need and I''ll find someone who fits.$B$BWhoever joins you keeps their own gear. They earn honor and arena points beside you and spend them at the same quartermasters you do.', 'Need someone to watch your back in the ring? Half the gladiators in this sewer are between teams. Tell me what you need and I''ll find someone who fits.$B$BWhoever joins you keeps their own gear. They earn honor and arena points beside you and spend them at the same quartermasters you do.', 1),
(9101002, 'Which bracket are you fighting in?', 'Which bracket are you fighting in?', 1),
(9101003, 'What do you need them to do?', 'What do you need them to do?', 1),
(9101004, 'These are looking for a team right now. The ones near your own gear are at the top.', 'These are looking for a team right now. The ones near your own gear are at the top.', 1),
(9101005, 'Your partners take their cues from your raid marks: skull is the kill target, moon is the one to keep crowd-controlled. Leave the marks off and they''ll call targets themselves, as well as they know how.$B$BThey earn honor and arena points with you, and they shop for themselves: the season''s gear, gated by the rating you earn together. A strong partner in poor gear is common early in a season.$B$BWhen you want to play, ask me to bring your partners and queue at the battlemaster as a group.', 'Your partners take their cues from your raid marks: skull is the kill target, moon is the one to keep crowd-controlled. Leave the marks off and they''ll call targets themselves, as well as they know how.$B$BThey earn honor and arena points with you, and they shop for themselves: the season''s gear, gated by the rating you earn together. A strong partner in poor gear is common early in a season.$B$BWhen you want to play, ask me to bring your partners and queue at the battlemaster as a group.', 1),
(9101006, 'The arenas are closed until the realm reaches the Outland campaign. Come back then.', 'The arenas are closed until the realm reaches the Outland campaign. Come back then.', 1);
