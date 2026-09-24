-- Account Vault: the only storage on the server.
-- There are no bags and no backpack, so everything a character acquires lands
-- here. Capacity is 1200 stacks, shared across every character on the account.
--
-- Rows reference item_instance rather than duplicating item state, and are NOT
-- instantiated as Item objects at login the way character_inventory is. They
-- become real items only on withdraw or use, so a full vault costs rows, not
-- 1200 live objects per online player.
--
-- account_id has no foreign key: accounts live in the auth database, and MySQL
-- cannot reference across schemas.
CREATE TABLE IF NOT EXISTS `account_vault` (
  `account_id` int unsigned NOT NULL,
  `slot` smallint unsigned NOT NULL COMMENT '0-1199; the addon derives tabs from this',
  `item_guid` int unsigned NOT NULL COMMENT 'item_instance.guid',
  `deposited_by` int unsigned DEFAULT NULL COMMENT 'characters.guid that deposited it',
  `deposited_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,

  PRIMARY KEY (`account_id`, `slot`),
  UNIQUE KEY `uk_item_guid` (`item_guid`) COMMENT 'an item cannot occupy two slots',
  KEY `idx_deposited_by` (`deposited_by`),

  CONSTRAINT `fk_account_vault_item` FOREIGN KEY (`item_guid`)
    REFERENCES `item_instance` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Account-wide item vault; the only storage, 1200 stacks';
