#pragma once
#include "SquadAutonomySettings.h"

#include <kenshi/Character.h>
#include <kenshi/AI/AI.h>
#include <kenshi/Inventory.h>
#include <kenshi/Platoon.h>
#include <kenshi/Town.h>
#include <kenshi/Building/Building.h>
#include <kenshi/TradeCulture.h>

namespace SquadAutonomy
{
	//void updateTradeItemsCount(AI* ai, Character* character);
	int getTradeItemPrice(Item* tradeItem, Town* tradeTown, Character* me, Character* trader, Platoon* traderSquad, TradeCulture* shopCulture, bool selling, bool& isGoodDeal);
	int countBuyingItemInInventory(ItemFunction itemFunc, ShoppingItemInfo shoppingItem, Character* character);
	int countSellingItemInInventory(ItemFunction itemFunc, ShoppingItemInfo shoppingItem, Character* character, std::vector<Item*>& out);
	void getItemToSellToShop(SquadSettingsInfo* settings, Character* character, Building* shop, std::vector<Item*>& items, bool getJustFirst = false);
	void getItemToBuyFromShop(SquadSettingsInfo* settings, Character* character, Building* shop, std::vector<Item*>& items, bool sortItem = false, bool getJustFirst = false);
	bool checkValidBuyItem(Character* character, Item* itemToBuy, ShoppingItemInfo shoppingItem);
	bool sellItemToShop(Item* item, int price, Character* trader, Building* shop);
	bool buyItemFromShop(Item* item, int price, Character* buyer);
	bool doTradeItem(Item* item, Character* character, Building* shop, bool selling);
	namespace Shopping
	{
		void hook();
	}
}