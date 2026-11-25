#include "SquadAutonomy.h"
#include "SquadAutonomyShopping.h"
#include "SquadAutonomyLocalization.h"
#include "SquadAutonomyLog.h"

#include <util/lektorExtension.h>

#include <kenshi/gui/ForgottenGUI.h>

#include <kenshi/util/hand.h>
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/GameData.h>
#include <kenshi/RootObject.h>
#include <kenshi/RootObjectFactory.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Character.h>
#include <kenshi/RaceData.h>
#include <kenshi/AI/AI.h>
#include <kenshi/AI/AIPackage.h>
#include <kenshi/Faction.h>
#include <kenshi/Inventory.h>
#include <kenshi/FactionRelations.h>
#include <kenshi/Platoon.h>
#include <kenshi/Town.h>
#include <kenshi/Building/Building.h>
#include <kenshi/Research.h>
#include <kenshi/Gear.h>
#include <kenshi/GunClass.h>
#include <kenshi/TradeCulture.h>


namespace SquadAutonomy
{
    bool (*_NV_giveItem_orig)(Character* thisptr, Item* item, bool dropOnFail, bool destroyOnFail);
    bool _NV_giveItem_hook(Character* thisptr, Item* item, bool dropOnFail, bool destroyOnFail)
    {
        Platoon* squad = nullptr;

        bool result = _NV_giveItem_orig(thisptr, item, dropOnFail, destroyOnFail);
        if (thisptr && thisptr->getPlatoon())
        {
            squad = thisptr->getPlatoon()->me;
        }
        SquadSettingsInfo* settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {
            Logger::logDebug(thisptr->displayName + " give " + item->displayName + " " + sc::toString(result), Logger::SHOPPING);
        }
        return result;
    }

    float (*scoreShopping_orig)(AI* thisptr, const hand& subject, const Ogre::Vector3& v);
    float scoreShopping_hook(AI* thisptr, const hand& subject, const Ogre::Vector3& v)
    {
        Character* character = thisptr->getCharacter();
        Platoon* squad = nullptr;
        if (character && character->getPlatoon()) squad = character->getPlatoon()->me;
        SquadSettingsInfo* settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {
            if (!settings->getDoShopping()) return 0.0;
        }
        return scoreShopping_orig(thisptr, subject, v);
    }

    float (*findShop_orig)(AI* thisptr, const hand& in, hand& out, bool justAsking);
    float findShop_hook(AI* thisptr, const hand& in, hand& out, bool justAsking)
    {
        float score = findShop_orig(thisptr, in, out, justAsking);

        Character* character = thisptr->getCharacter();
        Platoon* squad = nullptr;
        //TradingInfo* tradingInfo = thisptr->tradingInfo;
        if (character && character->getPlatoon()) squad = character->getPlatoon()->me;
        SquadSettingsInfo* settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled() && settings->getDoShopping())
        {
            lektor<Building*>* shopCandidates = new lektor<Building*>();
            TownBase* currentTown = character->getCurrentTownLocation();
            if (currentTown) shopCandidates = currentTown->findAllBuildingsWithFunction(BF_SHOP, character);
            else if (out && out.getBuilding())
            {
                Building* outBuilding = out.getBuilding();
                if (outBuilding->getSpecialFunction() != BF_SHOP || !outBuilding->getInventory() || (outBuilding->getFaction() == squad->getFaction()))
                {
                    out.setNull();
                    return 0.0;
                }
                else lektorEx::push_back(*shopCandidates, outBuilding);
            }

            for (int i = 0; i < shopCandidates->size(); ++i)
            {
                Building* shopBuilding = shopCandidates->at(i);
                if (!shopBuilding->isPublic() || shopBuilding->getSpecialFunction() != BF_SHOP || (shopBuilding->getFaction() == squad->getFaction()))
                {
                    continue;
                }
                else
                {
                    std::vector<Item*> tradeItems;
                    getItemToSellToShop(settings, character, shopBuilding, tradeItems, true);
                    if (tradeItems.size() > 0)
                    {
                        Logger::logDebug(character->displayName + ": has sellable item", SHOPPING);
                        out = shopBuilding->getHandle();
                        return 1.0;
                    }

                    getItemToBuyFromShop(settings, character, shopBuilding, tradeItems, false, true);
                    if (tradeItems.size() > 0)
                    {
                        Logger::logDebug(character->displayName + ": has buyable item", SHOPPING);
                        out = shopBuilding->getHandle();
                        return 1.0;
                    }

                }
            }
            out.setNull();
            return 0.0;
        }

        return score;
    }

    bool (*boughtShitComplete_orig)(AI* thisptr, const hand& subject, const Ogre::Vector3& _a2);
    bool boughtShitComplete_hook(AI* thisptr, const hand& subject, const Ogre::Vector3& _a2)
    {
        Character* character = thisptr->getCharacter();
        Platoon* squad = nullptr;
        //TradingInfo* tradingInfo = thisptr->tradingInfo;
        if (character && character->getPlatoon()) squad = character->getPlatoon()->me;
        SquadSettingsInfo* settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {
            if (!settings->getDoShopping()) return true;
            //updateTradeItemsCount(thisptr, character);

            auto sellItems = settings->getSellItems();

            Inventory* inventory = character->getInventory();
            ContainerItem* backpack = character->hasABackpackOn();
            Inventory* backpackInventory = backpack ? backpack->getInventory() : nullptr;
            for (auto iter = sellItems.begin(); iter != sellItems.end(); ++iter)
            {
                std::vector<Item*> out;
                int amount = countSellingItemInInventory(iter->first, iter->second, character, out);
                // has less or equal to keep amount
                if (amount > iter->second.quantity) return false;
            }

            auto buyItems = settings->getBuyItems();
            for (auto iter = buyItems.begin(); iter != buyItems.end(); ++iter)
            {
                int itemCount = countBuyingItemInInventory(iter->first, iter->second, character);
                if (itemCount < iter->second.quantity) return false;
            }
            Logger::logDebug("No item to buy/sell", SHOPPING);
            return true;
        }
        return boughtShitComplete_orig(thisptr, subject, _a2);
    }

    bool (*buySomething_orig)(AI* thisptr, Building* shop);
    bool buySomething_hook(AI* thisptr, Building* shop)
    {
        Character* character = thisptr->getCharacter();
        Platoon* squad = nullptr;
        if (character && character->getPlatoon()) squad = character->getPlatoon()->me;
        SquadSettingsInfo* settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {

            // grabbing necessary values
            Inventory* inventory = character->getInventory();
            ContainerItem* backpack = character->hasABackpackOn();
            Inventory* backpackInventory = backpack ? backpack->getInventory() : nullptr;
            Inventory* shopInventory = shop->getInventory();
            Building* shopBuilding = shop->furnitureParentBuilding();
            if (!inventory || !shopInventory || !shopBuilding) return false;

            Town* tradingTown = shop->getRealTown();
            Character* trader = shop->getResidentSquadLeader();
            Platoon* traderPlatoon = nullptr;
            TradeCulture* tradeCulture = nullptr;
            if (!trader && shopBuilding)
                trader = shopBuilding->getResidentSquadLeader();
            if (trader && trader->getPlatoon())
                traderPlatoon = trader->getPlatoon()->me;
            if (traderPlatoon && traderPlatoon->getFaction())
                tradeCulture = &traderPlatoon->getFaction()->tradeCulture;


            std::vector<Item*> shopItems;
            // selling item
            getItemToSellToShop(settings, character, shop, shopItems);
            for (int i = 0; i < shopItems.size(); ++i)
            {
                Item* originalItem = shopItems[i];
                if (originalItem)
                {
                    if (doTradeItem(originalItem, character, shop, true)) return true;
                    else continue;
                }
            }

            // buying item
            getItemToBuyFromShop(settings, character, shop, shopItems, true);
            for (int i = 0; i < shopItems.size(); ++i)
            {
                Item* originalItem = shopItems[i];
                if (originalItem)
                {
                    if (doTradeItem(originalItem, character, shop, false)) return true;
                    else continue;
                }
            }
            return false;
        }
        return buySomething_orig(thisptr, shop);
    }

    int getTradeItemPrice(Item* tradeItem, Town* tradeTown, Character* me, Character* trader, Platoon* traderSquad, TradeCulture* shopCulture, bool selling, bool& isGoodDeal)
    {
        if (selling && tradeItem->isStolen(false) && !traderSquad->iBuyStolenGoods(tradeItem) && trader->getFencingSuccessChance(tradeItem, me) < 1.0)
        {
            Logger::logDebug(tradeItem->displayName + ": might get caught selling stolen item", SHOPPING);
            return -1;
        }
        int price = tradeItem->getValueSingle(selling);
        float traderSquadMult = 1.0;
        if (traderSquad)
        {
            traderSquadMult = traderSquad->priceMultWhenITrade;
            if (traderSquad) price *= traderSquadMult;
        }
        GameData* itemData = tradeItem->getGameData();
        if (!itemData) return -1;

        // check if tradeculture allow it
        if (tradeTown)
        {
            if (selling && tradeTown->isIllegal(tradeItem) && !traderSquad->iBuyIllegalGoods())
            {
                Logger::logDebug(tradeItem->displayName + ": is illegal", SHOPPING);
                return -1;
            }
            float localMult = tradeTown->getLocalTradePriceMult(itemData);
            float factionMult = tradeTown->getFactionTradeCultureMult(itemData);
            Logger::logDebug(tradeItem->displayName + ": " + sc::toString(price) +
                " local mult x" + sc::toString(localMult) +
                " culture mult x" + sc::toString(factionMult), SHOPPING);
            price *= localMult;
            price *= factionMult;
            isGoodDeal = (traderSquadMult * factionMult) < 1.0;
            Logger::logDebug("GoodDeal Mult: " + sc::toString(traderSquadMult * factionMult));
            if (selling) isGoodDeal = !isGoodDeal;
        }
        else if (shopCulture)
        {
            if (selling && shopCulture->isItemIllegal(itemData) && !traderSquad->iBuyIllegalGoods())
            {
                Logger::logDebug(tradeItem->displayName + ": is illegal", SHOPPING);
                return -1;
            }
            if (shopCulture->hasTradePriceMultiplier(itemData))
            {
                float shopMult = shopCulture->getTradePriceMultiplier(itemData);
                Logger::logDebug(tradeItem->displayName + ": " + sc::toString(price) + " x" + sc::toString(shopMult), SHOPPING);
                price *= shopMult;
                isGoodDeal = (traderSquadMult * shopMult) < 1.0;
                Logger::logDebug("GoodDeal Mult: " + sc::toString(traderSquadMult * shopMult));
                if (selling) isGoodDeal = !isGoodDeal;
            }
        }
        return price;
    }


    bool checkValidBuyItem(Character* character, Item* itemToBuy, ShoppingItemInfo shoppingItem)
    {
        GameData* itemData = itemToBuy->getGameData();
        if (!itemData) return false;
        if (shoppingItem.shopItemDatas.size() > 0)
        {
            if (itemToBuy->getGameData() && shoppingItem.shopItemDatas.find(itemToBuy->getGameData()) != shoppingItem.shopItemDatas.end()) return true;
            else false;
        }
        if (character->getInventory())
        {
            Inventory* inventory = character->getInventory();

            if (!inventory->hasRoomForItem(itemData))
            {
                ContainerItem* backpack = character->hasABackpackOn();
                Inventory* backpackInventory = backpack ? backpack->getInventory() : nullptr;
                if (!backpackInventory || !backpackInventory->hasRoomForItem(itemData))
                {
                    Logger::logDebug("No room for: " + itemToBuy->displayName, SHOPPING);
                    return false;
                }

            }
            if (itemToBuy->itemFunction == ITEM_FOOD || itemToBuy->itemFunction == ITEM_FOOD_RESTRICTED)
            {
                if (!character->getRace()->canEat(itemToBuy, character->isAnimal()))
                {
                    Logger::logDebug("Inedible: " + itemToBuy->displayName, SHOPPING);
                    return false;
                }
            }
            else if (itemToBuy->itemFunction == ITEM_CLOTHING)
            {
                lektor<InventorySection*> sections = inventory->getAllSections();
                bool allSlotsEquipped = true;
                bool mySlotExist = false;
                for (int i = 0; i < sections.size(); ++i)
                {
                    //DebugLog("Sections" + sc::toString(i) + " " + sections[i]->name);
                    if (!sections[i]->isAnEquippedItemSection) continue;
                    //DebugLog("Is an equipped item section");
                    if (!sections[i]->getEnabled()) continue;
                    //DebugLog("Is enabled");
                    if (!sections[i]->isEmpty()) continue;
                    //DebugLog("Is empty");
                    allSlotsEquipped = false;
                    //DebugLog("Found empty armor slot");
                    if (sections[i]->isLimitedSlotCompatible(itemToBuy))
                    {
                        //DebugLog("Found valid slot");
                        mySlotExist = true;
                        break;
                    }
                }
                if (!allSlotsEquipped && !mySlotExist)
                {
                    return false;
                }
            }
            else if (itemToBuy->itemFunction == ITEM_AMMO)
            {
                lektor<Item*> equipped;
                inventory->getEquippedWeapons(equipped);
                bool validAmmoType = false;
                for (int i = 0; i < equipped.size(); ++i)
                {
                    Crossbow* crossbow = equipped[i]->isCrossbow();
                    GameData* ammoType = nullptr;

                    if (crossbow && crossbow->gunClass) ammoType = crossbow->gunClass->ammoType;
                    //DebugLog(crossbow->displayName + " ammo type: " + ammoType->name + " : " + itemToBuy->displayName);
                    if (ammoType && ammoType == itemData)
                    {
                        //DebugLog("Valid ammo type!");
                        validAmmoType = true;
                        break;
                    }
                }
                if (!validAmmoType)
                {
                    return false;
                }
            }

        }
        return true;
    }


    int countBuyingItemInInventory(ItemFunction itemFunc, ShoppingItemInfo shoppingItem, Character* character)
    {
        int amount = 0;
        Inventory* inventory = character->getInventory();
        ContainerItem* backpack = character->hasABackpackOn();
        Inventory* backpackInventory = backpack ? backpack->getInventory() : nullptr;
        lektor<Item*> items;
        inventory->getAllItemsWithFunction(items, itemFunc);
        if (backpackInventory)
        {
            backpackInventory->getAllItemsWithFunction(items, itemFunc);
        }

        if (itemFunc == ITEM_FOOD)
        {
            inventory->getAllItemsWithFunction(items, ITEM_FOOD_RESTRICTED);
            if (backpackInventory)
            {
                backpackInventory->getAllItemsWithFunction(items, ITEM_FOOD_RESTRICTED);
            }
        }

        if (itemFunc == ITEM_WEAPON)
        {
            inventory->getEquippedWeapons(items);
        }
        else if (itemFunc == ITEM_CLOTHING)
        {
            inventory->getEquippedArmour(items);
        }


        for (int i = 0; i < items.size(); ++i)
        {
            Item* item = items[i];
            if (shoppingItem.shopItemDatas.size() > 0)
            {
                if (!item->getGameData() || shoppingItem.shopItemDatas.find(item->getGameData()) == shoppingItem.shopItemDatas.end()) continue;
            }
            if (itemFunc == ITEM_FOOD_RESTRICTED)
            {
                if (!character->getRace() || !character->getRace()->canEat(item, character->isAnimal())) continue;
            }
            amount += item->quantity;
        }
        return amount;
    }

    int countSellingItemInInventory(ItemFunction itemFunc, ShoppingItemInfo shoppingItem, Character* character, std::vector<Item*>& out)
    {
        int amount = 0;
        Inventory* inventory = character->getInventory();
        ContainerItem* backpack = character->hasABackpackOn();
        Inventory* backpackInventory = backpack ? backpack->getInventory() : nullptr;
        lektor<Item*> items;
        inventory->getAllItemsWithFunction(items, itemFunc);
        if (backpackInventory)
        {
            backpackInventory->getAllItemsWithFunction(items, itemFunc);
        }

        if (itemFunc == ITEM_FOOD)
        {
            inventory->getAllItemsWithFunction(items, ITEM_FOOD_RESTRICTED);
            if (backpackInventory)
            {
                backpackInventory->getAllItemsWithFunction(items, ITEM_FOOD_RESTRICTED);
            }
        }
        for (int i = 0; i < items.size(); ++i)
        {
            Item* item = items[i];
            if (shoppingItem.shopItemDatas.size() > 0)
            {
                if (!item->getGameData() || shoppingItem.shopItemDatas.find(item->getGameData()) == shoppingItem.shopItemDatas.end())
                {
                    Logger::logDebug(item->displayName + " is not set as shop items", SHOPPING);
                    continue;
                }
            }
            if (shoppingItem.onlyStolenItems && !item->isStolen(true))
            {
                Logger::logDebug(item->displayName + " is not stolen but onlySellStolen is on", SHOPPING);
                continue;
            }
            out.push_back(item);
            amount += item->quantity;
        }
        return amount;
    }


    void getItemToSellToShop(SquadSettingsInfo* settings, Character* character, Building* shop, std::vector<Item*>& items, bool getJustFirst)
    {
        items.clear();
        if (!shop) return;
        Building* shopBuilding = shop->furnitureParentBuilding();
        if (!shopBuilding) return;
        Logger::logDebug(character->displayName + " check sellable item: " + shopBuilding->displayName, SHOPPING);
        auto buyItems = settings->getBuyItems();
        Town* tradingTown = shop->getRealTown();
        TradeCulture* tradeCulture = nullptr;
        Inventory* inventory = character->getInventory();
        ContainerItem* backpack = character->hasABackpackOn();
        Inventory* backpackInventory = backpack ? backpack->getInventory() : nullptr;
        Inventory* shopInventory = shop->getInventory();
        if (!inventory || !shopInventory) return;
        Character* trader = shop->getResidentSquadLeader();
        Platoon* traderPlatoon = nullptr;
        if (!trader) trader = shopBuilding->getResidentSquadLeader();
        if (!trader) return;
        if (trader->getPlatoon()) traderPlatoon = trader->getPlatoon()->me;
        if (traderPlatoon && traderPlatoon->getFaction()) tradeCulture = &traderPlatoon->getFaction()->tradeCulture;
        auto sellItems = settings->getSellItems();
        for (auto iter = sellItems.begin(); iter != sellItems.end(); iter++)
        {
            if (iter->second.shopTowns.size() > 0)
            {
                TownBase* shopTown = shop->getTown();
                bool hasTown = shopTown ? iter->second.hasShopTown(shopTown) : false;
                if (iter->second.excludeTowns)
                {
                    if (hasTown) continue;
                }
                else if (!shopTown || !hasTown) continue;
            }

            std::vector<Item*> itemsToSell;
            int amount = countSellingItemInInventory(iter->first, iter->second, character, itemsToSell);
            if (amount <= iter->second.quantity) continue;
            
            // tried using inventoryGUI::RClickAutoTrade but that requires the inventoryGUI to be created which can only be done safely on UI thread
            // based the following code from buySomething and buyItem code for buying stuffs
            for (int i = static_cast<int>(itemsToSell.size()) - 1; i >= 0; --i)
            {
                // ignore equipped section
                Item* item = itemsToSell[i];
                
                if (!shopInventory->hasRoomForItem(item->getGameData()))
                {
                    Logger::logDebug(item->displayName + " has no room", SHOPPING);
                    continue;
                }
                // can move this outside the item transfer block
                // use price < 0 as a skip flag
                bool goodDeal = false;
                int price = getTradeItemPrice(item, tradingTown, character, trader, traderPlatoon, tradeCulture, true, goodDeal);
                // if onlyGoodDeal then check if sell price is lower/equal to base price
                if (iter->second.onlyGoodDeal && !goodDeal)
                {
                    Logger::logDebug(item->displayName + " not a good deal", SHOPPING);
                    continue;
                }
                if (price >= 0 && price <= trader->getMoney())
                {
                    amount += item->quantity;
                    items.push_back(item);
                    // if has more sellable items than amount to keep then finish the function
                    if (getJustFirst) return;
                }
            }
        }
    }

    void getItemToBuyFromShop(SquadSettingsInfo* settings, Character* character, Building* shop, std::vector<Item*>& items, bool sortItem, bool getJustFirst)
    {
        items.clear();
        if (!shop) return;
        Building* shopBuilding = shop->furnitureParentBuilding();
        if (!shopBuilding) return;
        Logger::logDebug(character->displayName + " check buyable item: " + shopBuilding->displayName, SHOPPING);
        Town* tradingTown = shop->getRealTown();
        TradeCulture* tradeCulture = nullptr;
        Inventory* inventory = character->getInventory();
        ContainerItem* backpack = character->hasABackpackOn();
        Inventory* backpackInventory = backpack ? backpack->getInventory() : nullptr;
        Inventory* shopInventory = shop->getInventory();
        if (!inventory || !shopInventory) return;
        Character* trader = shop->getResidentSquadLeader();
        Platoon* traderPlatoon = nullptr;
        if (!trader) trader = shopBuilding->getResidentSquadLeader();
        if (!trader) return;
        if (trader && trader->getPlatoon()) traderPlatoon = trader->getPlatoon()->me;
        if (traderPlatoon && traderPlatoon->getFaction()) tradeCulture = &traderPlatoon->getFaction()->tradeCulture;
        auto buyItems = settings->getBuyItems();
        for (auto iter = buyItems.begin(); iter != buyItems.end(); iter++)
        {
            if (iter->second.shopTowns.size() > 0)
            {
                TownBase* shopTown = shop->getTown();
                bool hasTown = shopTown ? iter->second.hasShopTown(shopTown) : false;
                if (iter->second.excludeTowns)
                {
                    if (hasTown) continue;
                }
                else if (!shopTown || !hasTown) continue;
            }

            int itemCount = countBuyingItemInInventory(iter->first, iter->second, character);
            if (itemCount >= iter->second.quantity) continue;


            lektor<Item*> itemsToBuy;
            shopInventory->getAllItemsWithFunction(itemsToBuy, iter->first);

            if (iter->first == ITEM_FOOD)
            {
                shopInventory->getAllItemsWithFunction(itemsToBuy, ITEM_FOOD_RESTRICTED);
            }

            lektor<Building*> storages;
            shopBuilding->findAllFurnitureWithFunction(storages, BF_GENERAL_STORAGE);
            shopBuilding->findAllFurnitureWithFunction(storages, BF_RESOURCE_STORAGE);
            for (int i = 0; i < storages.size(); ++i)
            {
                Inventory* storageInventory = storages[i]->getInventory();
                if (!storageInventory) continue;
                storageInventory->getAllItemsWithFunction(itemsToBuy, iter->first);
                if (iter->first == ITEM_FOOD)
                {
                    storageInventory->getAllItemsWithFunction(itemsToBuy, ITEM_FOOD_RESTRICTED);
                }
            }
            if (sortItem)
            {
                // sort food by quality
                if (iter->first == ITEM_FOOD)
                {
                    std::sort(itemsToBuy.begin(), itemsToBuy.end(), [](Item* a, Item* b)
                    {
                        return a->originalFullChargeAmount > b->originalFullChargeAmount;
                    });
                }
                else
                {
                    //sort everything else by price?
                    std::sort(itemsToBuy.begin(), itemsToBuy.end(), [](Item* a, Item* b)
                    {
                        return a->getValueSingle(false) > b->getValueSingle(false);
                    });
                }
            }
            // tried using inventoryGUI::RClickAutoTrade but that requires the inventoryGUI to be created which can only be done safely on UI thread
            // based the following code from buySomething and buyItem code for buying stuffs
            for (int i = 0; i < itemsToBuy.size(); ++i)
            {
                // ignore equipped section
                Item* item = itemsToBuy[i];
                if (iter->second.shopItemDatas.size() > 0)
                {
                    if (!item->getGameData() || iter->second.shopItemDatas.find(item->getGameData()) == iter->second.shopItemDatas.end())
                    {
                        Logger::logDebug(item->displayName + " is not set as shop items", SHOPPING);
                        continue;
                    }
                }
                if (!checkValidBuyItem(character, item, iter->second))
                {
                    Logger::logDebug(item->displayName + " is not valid", SHOPPING);
                    continue;
                }
                // can move this outside the item transfer block
                bool goodDeal = false;
                int price = getTradeItemPrice(item, tradingTown, character, trader, traderPlatoon, tradeCulture, false, goodDeal);
                // if onlyGoodDeal then check if buy price is higher/equal to base price
                if (iter->second.onlyGoodDeal && !goodDeal)
                {
                    Logger::logDebug(item->displayName + " not a good deal", SHOPPING);
                    continue;
                }
                if (price >= 0 && price <= character->getMoney())
                {
                    items.push_back(item);
                    if (getJustFirst) return;
                }
            }
        }
    }

    bool sellItemToShop(Item* item, int price, Character* trader, Building* shop)
    {
        if (price < 0) return false;

        if (price > trader->getMoney())
        {
            Logger::logDebug(trader->displayName + ": shop money - " + sc::toString(trader->getMoney()), SHOPPING);
            return false;
        }
        if (!shop->giveItem(item, false, false))
        {
            Logger::logDebug(trader->displayName + ": no room for - " + item->displayName, SHOPPING);
            return false;
        }
        return true;
    }

    bool buyItemFromShop(Item* item, int price, Character* buyer)
    {
        if (price < 0) return false;

        if (price > buyer->getMoney())
        {
            Logger::logDebug(buyer->displayName + ": money - " + sc::toString(buyer->getMoney()), SHOPPING);
            return false;
        }
        if (!buyer->giveItem(item, false, false))
        {
            ContainerItem* backpack = buyer->hasABackpackOn();
            if (backpack)
            {
                if (backpack->giveItem(item, false, false)) return true;
            }
            Logger::logDebug(buyer->displayName + ": no room for - " + item->displayName, SHOPPING);
            return false;
        }
        return true;
    }

    bool doTradeItem(Item* item, Character* character, Building* shop, bool selling)
    {
        if (!shop) return false;
        Building* shopBuilding = shop->furnitureParentBuilding();
        if (!shopBuilding) return false;
        Town* tradingTown = shop->getRealTown();
        TradeCulture* tradeCulture = nullptr;
        Inventory* shopInventory = shop->getInventory();
        if (!shopInventory) return false;
        Character* trader = shop->getResidentSquadLeader();
        Platoon* traderPlatoon = nullptr;
        if (!trader) trader = shopBuilding->getResidentSquadLeader();
        if (!trader) return false;
        if (trader->getPlatoon()) traderPlatoon = trader->getPlatoon()->me;
        if (traderPlatoon && traderPlatoon->getFaction()) tradeCulture = &traderPlatoon->getFaction()->tradeCulture;
        if (item)
        {
            bool _goodDeal = false;
            int price = getTradeItemPrice(item, tradingTown, character, trader, traderPlatoon, tradeCulture, selling, _goodDeal);
            std::string sectionName = item->inventorySection;
            Inventory* itemInventory = item->getInventoryWeAreIn().getRootObject()->getInventory();
            InventorySection* section = itemInventory->getSection(sectionName);
            if (section)
            {
                if (section->isAnEquippedItemSection) return false;
                Item* tradeItem;
                if (item->quantity < 2)
                {
                    if (section->removeItem(item))
                    {
                        tradeItem = item;
                    }
                    else return false;
                }
                else
                {
                    tradeItem = ou->theFactory->copyItem(item);
                    tradeItem->quantity = 1;
                    item->quantity -= 1;
                }
                bool tradeSuccess = selling ? sellItemToShop(tradeItem, price, trader, shop) : buyItemFromShop(tradeItem, price, character);
                if (tradeSuccess)
                {
                    int moneyChange = price;
                    if (selling) moneyChange = -moneyChange;
                    trader->takeMoney(-moneyChange);
                    character->takeMoney(moneyChange);
                    if (shop->getInventory()->getInventoryGUI()) shop->getInventory()->getInventoryGUI()->refreshSection();
                    if (itemInventory->getInventoryGUI()) itemInventory->getInventoryGUI()->refreshSection();

                    std::string priceText = Localization::formatMain(boost::locale::format("{1,num}") % price);
                    std::string prefix = selling ? "+" : "";
                    MyGUI::Colour priceColor = selling ? MyGUI::Colour::Colour(0.6, 1.0, 0.6, 0.9) : MyGUI::Colour::Colour(1.0, 1.0, 0.6, 0.9);
                    ScreenLabel* priceLabel = gui->createScreenLabel(prefix + "c." + priceText, priceColor, ScreenLabel::LS_LARGE, ScreenLabel::RS_SLOW);
                    priceLabel->setTracking(character, Ogre::Vector3(0.0, 15.0, 0.0));
                    character->audioEvent("Sell_Item", static_cast<SoundRange>(40000));
                    Logger::log(character->displayName + (selling ? " sold " : " bought ") + tradeItem->displayName + " for " + priceText, Logger::Info);

                    return true;
                }
                else
                {
                    if (!item)
                    {
                        itemInventory->addItem(tradeItem, 1, false, false);
                    }
                    else
                    {
                        ou->destroy(tradeItem, false, "SquadAutonomy:SellItem");
                        item->quantity += 1;
                    }
                }
            }
        }
        return false;
    }



    void Shopping::hook()
    {
        if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::buySomething), &SquadAutonomy::buySomething_hook, &SquadAutonomy::buySomething_orig))
            ErrorLog("Could not add AI::buySomething hook!");
        if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::boughtShitComplete), &SquadAutonomy::boughtShitComplete_hook, &SquadAutonomy::boughtShitComplete_orig))
            ErrorLog("Could not add AI::boughtShitComplete hook!");
        if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::findShop), &SquadAutonomy::findShop_hook, &SquadAutonomy::findShop_orig))
            ErrorLog("Could not add AI::findShop hook!");
        if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::scoreShopping), &SquadAutonomy::scoreShopping_hook, &SquadAutonomy::scoreShopping_orig))
            ErrorLog("Could not add AI::scoreShopping hook!");
        if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&Character::_NV_giveItem), &SquadAutonomy::_NV_giveItem_hook, &SquadAutonomy::_NV_giveItem_orig))
            ErrorLog("Could not add RootObject::Character hook!");

        /*
        if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::_createTradingInfo), &SquadAutonomy::_createTradingInfo_hook, &SquadAutonomy::_createTradingInfo_orig))
            ErrorLog("Could not add AI::_createTradingInfo hook!");
        if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&Inventory::buyItem), &SquadAutonomy::buyItem_hook, &SquadAutonomy::buyItem_orig))
            ErrorLog("Could not add Inventory::buyItem hook!");
        */
    }

}