-- Bonus Layer: Immutable entry log for character stat bonuses
-- Each entry represents a write event (positive value) or revoke (negative value)
-- Sources: gear, sacrament, bloodline, legendary_rank, class_set, reaping, scar, origin_path
CREATE TABLE IF NOT EXISTS `bonus_layer_ledger` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT PRIMARY KEY,
  `character_id` int unsigned NOT NULL,
  `field_name` varchar(64) NOT NULL COMMENT 'e.g., strength, agility, armor, dodge_rating',
  `source` varchar(64) NOT NULL COMMENT 'e.g., gear, sacrament, bloodline',
  `value` int NOT NULL COMMENT 'Positive for write, negative for revoke',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `context` varchar(255) COMMENT 'Optional context: slot name, item GUID, etc.',

  KEY `idx_character_id` (`character_id`),
  KEY `idx_field_source` (`field_name`, `source`),
  KEY `idx_created_at` (`created_at`),

  CONSTRAINT `fk_bonus_layer_ledger_character` FOREIGN KEY (`character_id`)
    REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Immutable ledger of all bonus layer write/revoke events per character';
