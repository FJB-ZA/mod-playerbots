-- Dalaran Sewers and Ring of Valor are Wrath maps: keep them out of the TBC 65-70 skirmish and rated pick, which era-locked sessions cannot enter
UPDATE `battleground_template` SET `MinLvl` = 71 WHERE `ID` IN (10, 11);
