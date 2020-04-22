START_SCHEMA( GC, cbase.h )

// --------------------------------------------------------
// WARNING!  All new tables need to be added to the end of the file
// if you expect to deploy the GC without deploying new clients.
// --------------------------------------------------------

//-----------------------------------------------------------------------------
// ItemName
//
//-----------------------------------------------------------------------------
START_TABLE( k_ESchemaCatalogMain, ItemName, TABLE_PROP_NORMAL )
MEM_FIELD_BIN( unDefIndex,	DefIndex,	uint16 )	// Item definition index
MEM_FIELD_VAR_CHAR_LEN( VarCharName,	Name,	32 )	// Item Name
PRIMARY_KEY_CLUSTERED( 100, unDefIndex )
UNIQUE_FIELD( ItemNameIndex, VarCharName )
WIPE_TABLE_BETWEEN_TESTS( k_EWipePolicyPreserveAlways )
ALLOW_WIPE_TABLE_IN_PRODUCTION( true )
END_TABLE


//-----------------------------------------------------------------------------
// Item
//
//-----------------------------------------------------------------------------
START_TABLE( k_ESchemaCatalogMain, Item, TABLE_PROP_NORMAL )
MEM_FIELD_BIN( ulID,		ID,			uint64 )	// Item ID
MEM_FIELD_BIN( unAccountID, AccountID,	uint32 )	// Item Owner
MEM_FIELD_BIN( unDefIndex,	DefIndex,	uint16 )	// Item definition index
MEM_FIELD_BIN( unLevel,		Level,		uint8 )	// Item Level
MEM_FIELD_BIN( nQuality,	EQuality,	uint8 )		// Item quality (rarity)
MEM_FIELD_BIN( unInventory,	Inventory,	uint32 )	// App managed int representing inventory placement
MEM_FIELD_BIN( unQuantity,	Quantity,	uint32 )	// Consumable stack count (ammo, money, etc)
MEM_FIELD_VAR_CHAR_LEN( VarCharCustomName,	Name,	MAX_ITEM_CUSTOM_NAME_LENGTH+1 )	// User-crafted custom name
PRIMARY_KEY_CLUSTERED( 100, ulID )
INDEX_FIELD( UserAppLookup, unAccountID )
WIPE_TABLE_BETWEEN_TESTS( k_EWipePolicyWipeForAllTests )
ALLOW_WIPE_TABLE_IN_PRODUCTION( false )
END_TABLE


//-----------------------------------------------------------------------------
// AttributeName
//
//-----------------------------------------------------------------------------
START_TABLE( k_ESchemaCatalogMain, AttributeName, TABLE_PROP_NORMAL )
MEM_FIELD_BIN( unAttrDefIndex,AttrDefIndex,uint32 )	// Attribute definition index
MEM_FIELD_VAR_CHAR_LEN( VarCharName,	Name,	32 )	// Attribute Name
PRIMARY_KEY_CLUSTERED( 100, unAttrDefIndex )
UNIQUE_FIELD( AttributeNameIndex, VarCharName )
WIPE_TABLE_BETWEEN_TESTS( k_EWipePolicyPreserveAlways )
ALLOW_WIPE_TABLE_IN_PRODUCTION( true )
END_TABLE


//-----------------------------------------------------------------------------
// ItemAttribute
//
//-----------------------------------------------------------------------------
START_TABLE( k_ESchemaCatalogMain, ItemAttribute, TABLE_PROP_NORMAL )
MEM_FIELD_BIN( ulItemID,	ItemID,		uint64 )	// Item ID
MEM_FIELD_BIN( unAttrDefIndex,AttrDefIndex,uint16 )	// Attribute definition index
MEM_FIELD_BIN( flValue,		Value,		float )		// Attribute Value
PRIMARY_KEYS_CLUSTERED( 80, ulItemID, unAttrDefIndex )
FOREIGN_KEY_CONSTRAINT( FKItemID, ItemID, Item, ID, GCSDK::k_EForeignKeyActionCascade, GCSDK::k_EForeignKeyActionCascade )
WIPE_TABLE_BETWEEN_TESTS( k_EWipePolicyWipeForAllTests )
ALLOW_WIPE_TABLE_IN_PRODUCTION( false )
END_TABLE


//-----------------------------------------------------------------------------
// ItemAudit
//
//-----------------------------------------------------------------------------
START_TABLE( k_ESchemaCatalogMain, ItemAudit, TABLE_PROP_NORMAL )
MEM_FIELD_BIN( ulItemID,	ItemID,		uint64 )	// Item ID
MEM_FIELD_BIN( RTime32Stamp,TimeStamp,	RTime32 )	// Time
MEM_FIELD_BIN( eAction,		Action,		uint8 ) // What Happened
MEM_FIELD_BIN( unOwnerID,	OwnerID,	uint32 )	// Player who owns the item
MEM_FIELD_BIN( unServerIP,	ServerIP,	uint32 )	// IP of the server this happened on
MEM_FIELD_BIN( usServerPort,ServerPort,	uint16 )	// Port of the server this happened on
MEM_FIELD_BIN( unData,		Data,		uint32 )	// Additional data for the operation
PRIMARY_KEYS_CLUSTERED( 80, ulItemID, RTime32Stamp, eAction )
INDEX_FIELDS( AccountLookupIndex, unOwnerID, eAction )
WIPE_TABLE_BETWEEN_TESTS( k_EWipePolicyWipeForAllTests )
ALLOW_WIPE_TABLE_IN_PRODUCTION( false )
END_TABLE


//-----------------------------------------------------------------------------
// Recipe
//
//-----------------------------------------------------------------------------
START_TABLE( k_ESchemaCatalogMain, Recipe, TABLE_PROP_NORMAL )
MEM_FIELD_BIN( unAccountID, AccountID,	uint32 )	// Recipe Owner
MEM_FIELD_BIN( unDefIndex,	DefIndex,	uint16 )	// Recipe definition index
PRIMARY_KEYS_CLUSTERED( 100, unDefIndex, unAccountID )
INDEX_FIELD( UserAppLookup, unAccountID )
WIPE_TABLE_BETWEEN_TESTS( k_EWipePolicyWipeForAllTests )
ALLOW_WIPE_TABLE_IN_PRODUCTION( false )
END_TABLE


//-----------------------------------------------------------------------------
// WarDeaths
//
//-----------------------------------------------------------------------------
START_TABLE( k_ESchemaCatalogMain, WarDeaths, TABLE_PROP_NORMAL )
MEM_FIELD_BIN( unVictimID,	VictimID,	uint32 )	// Player who was killed by the opposite side
MEM_FIELD_BIN( RTime32Stamp,TimeStamp,	RTime32 )	// Time this player's session ended
PRIMARY_KEYS_CLUSTERED( 80, unVictimID, RTime32Stamp )
MEM_FIELD_BIN( unSessionDuration, SessionDuration, uint32)// Length of this player's session in seconds
MEM_FIELD_BIN( unSoldierDeaths,	SoldierDeaths,	uint32 )	// Number of times this player died as a solder to a demoman
MEM_FIELD_BIN( unDemomanDeaths,	DemomanDeaths,	uint32 )	// Number of times this player died as a demoman to a solder 
WIPE_TABLE_BETWEEN_TESTS( k_EWipePolicyWipeForAllTests )
ALLOW_WIPE_TABLE_IN_PRODUCTION( false )
END_TABLE

//-----------------------------------------------------------------------------
// GameAccountClient
//
//-----------------------------------------------------------------------------
START_TABLE( k_ESchemaCatalogMain, GameAccountClient, TABLE_PROP_NORMAL )
MEM_FIELD_BIN( unAccountID, AccountID,	uint32 )	// Item Owner
PRIMARY_KEY_CLUSTERED( 80, unAccountID )
MEM_FIELD_BIN( unSoldierKills,	SoldierKills,	uint32 )	// Number of times this player killed soldiers during the War!
MEM_FIELD_BIN( unDemomanKills,	DemomanKills,	uint32 )	// Number of times this player killed demomen during the War!
WIPE_TABLE_BETWEEN_TESTS( k_EWipePolicyWipeForAllTests )
ALLOW_WIPE_TABLE_IN_PRODUCTION( false )
END_TABLE


//-----------------------------------------------------------------------------
// GameAccount
//
//-----------------------------------------------------------------------------
START_TABLE( k_ESchemaCatalogMain, GameAccount, TABLE_PROP_NORMAL )
MEM_FIELD_BIN( unAccountID, AccountID,	uint32 )	// Account ID of the user
MEM_FIELD_BIN( unRewardPoints, RewardPoints, uint32 ) // number of timed reward points (coplayed minutes) for this user
PRIMARY_KEY_CLUSTERED( 100, unAccountID )
WIPE_TABLE_BETWEEN_TESTS( k_EWipePolicyWipeForAllTests )
ALLOW_WIPE_TABLE_IN_PRODUCTION( false )
END_TABLE


// NEED A CARRIAGE RETURN HERE!
//-------------------------