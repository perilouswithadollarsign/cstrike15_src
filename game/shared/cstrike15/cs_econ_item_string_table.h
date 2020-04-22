#ifndef CS_ECON_ITEM_STRING_TABLE_H
#define CS_ECON_ITEM_STRING_TABLE_H
#ifdef _WIN32
#pragma once
#endif

class CPlayerInventory;
class INetworkStringTable;
typedef int CStrikeEconItemIndex_t;

// econ item string table
#define MAX_PLAYER_ECON_ITEMS_STRING_BITS		10
#define MAX_PLAYER_ECON_ITEMS_STRINGS			( 1 << MAX_PLAYER_ECON_ITEMS_STRING_BITS )
#define PLAYER_ECON_ITEMS_INVALID_STRING		( MAX_PLAYER_ECON_ITEMS_STRINGS - 1 )

#define DOTA_NETWORKED_LOADOUT_SLOT_COUNT		16

void CreateEconItemStringTable( void );
CStrikeEconItemIndex_t InvalidEconItemStringIndex();

#ifdef CLIENT_DLL
void OnStringTableEconItemsChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData );
void RepopulateInventory( CPlayerInventory *pInventory, uint32 iAccountID );
#endif

#ifdef GAME_DLL
CStrikeEconItemIndex_t AddEconItemToStringTable( CEconItem *pItem );
#endif

CEconItem* GetEconItemFromStringTable( itemid_t itemID );

#endif //DOTA_ECON_ITEM_STRING_TABLE_H