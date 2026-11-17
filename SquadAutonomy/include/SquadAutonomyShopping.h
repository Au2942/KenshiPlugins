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
	int tradeItemsOfTypeInInventoryCount(ItemFunction itemFunction, Character* character, Inventory* inventory);
	void updateTradeItemsCount(AI* ai, Character* character);
	int getTradeItemPrice(Item* tradeItem, Town* tradeTown, Platoon* traderSquad, TradeCulture* shopCulture, bool selling);
	bool hasSellableItem(SquadSettingsInfo* settings, Character* character, Building* shop);
	bool checkValidBuyItem(Character* character, Item* itemToBuy);
	bool hasBuyableItem(SquadSettingsInfo* settings, Character* character, Building* shop);
	bool sellItemToShop(Item* item, int price, Character* trader, Building* shop);
	bool buyItemFromShop(Item* item, int price, Character* buyer);
	namespace Shopping
	{
		void hook();
	}
}