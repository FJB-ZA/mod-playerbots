CREATE TABLE IF NOT EXISTS `playerbots_living_arena_season` (
    `id` TINYINT UNSIGNED NOT NULL,
    `season` TINYINT UNSIGNED NOT NULL,
    `started` INT UNSIGNED NOT NULL,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `playerbots_living_arena_bots` (
    `guid` INT UNSIGNED NOT NULL,
    `gear_score` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
