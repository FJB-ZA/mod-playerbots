-- Bots a player chose to avoid (the SoloWowBonds addon): they stop whispering, greeting and inviting that player,
-- never pick them for role-play (duels, trades), and the Dungeon Finder keeps them out of the player's groups.
CREATE TABLE IF NOT EXISTS `playerbots_bond_avoids` (
    `player` INT UNSIGNED NOT NULL,
    `bot` INT UNSIGNED NOT NULL,
    `set_at` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`player`, `bot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
