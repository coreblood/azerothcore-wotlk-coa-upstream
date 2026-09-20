-- Bonus Layer: Computed cache of effective values
-- Recomputed on 7 discrete events (login, gear change, sacrament, reaping, scar, ascension, rank-up)
-- Stores the final computed value after all modifiers, caps, and surplus routing applied
CREATE TABLE IF NOT EXISTS `bonus_layer_cache` (
  `character_id` int unsigned NOT NULL PRIMARY KEY,
  `field_name` varchar(64) NOT NULL,
  `effective_value` int NOT NULL,
  `last_recompute_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

  KEY `idx_character_field` (`character_id`, `field_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Cache of effective computed stat values, regenerated on 7 triggers';
