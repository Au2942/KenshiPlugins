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
                if (shopBuilding->getSpecialFunction() != BF_SHOP || !shopBuilding->getInventory() || (shopBuilding->getFaction() == squad->getFaction()))
                {
                    continue;
                }
                else
                {
                    Inventory* shopInventory = shopBuilding->getInventory();

                    if (hasSellableItem(settings, character, shopBuilding))
                    {
                        Logger::logDebug(character->displayName + ": has sellable item", SHOPPING);
                        out = shopBuilding->getHandle();
                        return 1.0;
                    }
                    else if (hasBuyableItem(settings, character, shopBuilding))
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
/*
    Item* (*buyItem_orig)(Inventory* thisptr, Item* itemToBuy, RootObject* sendingTo);
    Item* buyItem_hook(Inventory* thisptr, Item* itemToBuy, RootObject* sendingTo)
    {
        Character* character = sendingTo->getHandle().getCharacter();
        Platoon* squad = nullptr;
        Faction* inventoryFaction = nullptr;
        bool inventoryNotPlayer = false;
        AI* ai = character->getAI();

        if (character && character->getPlatoon())
        {
            squad = character->getPlatoon()->me;
        }

        SquadSettingsInfo* settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {

            if (!checkValidBuyItem(character, itemToBuy)) return nullptr;

            if (thisptr->getOwner()) inventoryFaction = thisptr->getOwner()->getFaction();
            if (inventoryFaction && !inventoryFaction->isPlayer)
            {
                // there's a random 20% chance to not get the item if the shop inventory is not of player faction
                inventoryNotPlayer = true;
                inventoryFaction->isPlayer = settings->getPlayerInterface();
            }
        }

        Item* item = buyItem_orig(thisptr, itemToBuy, sendingTo);

        if (settings && settings->isEnabled() && inventoryFaction && inventoryNotPlayer)
        {
            inventoryFaction->isPlayer = nullptr;
            if (item)
            {
                Logger::log(sendingTo->displayName + " bought " + item->displayName, Logger::Info);
            }
            updateTradeItemsCount(ai, character);
        }


        return item;
    }
*/

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
                lektor<Item*> items;
                inventory->getAllItemsWithFunction(items, iter->first);
                if (backpackInventory)
                {
                    backpackInventory->getAllItemsWithFunction(items, iter->first);
                }
                int amountToKeep = iter->second.quantity;
                int amountOwned = 0;
                for (int i = 0; i < items.size(); ++i)
                {
                    amountOwned += items[i]->quantity;
                }
                // has less or equal to keep amount
                if (amountOwned > amountToKeep) return false;
            }

            auto buyItems = settings->getBuyItems();
            for (auto iter = buyItems.begin(); iter != buyItems.end(); ++iter)
            {
                int itemCount = 0;
                Inventory* inventory = character->getInventory();
                if (inventory)
                {
                    itemCount += tradeItemsOfTypeInInventoryCount(iter->first, character, inventory);
                }
                if (backpackInventory)
                {
                    itemCount += tradeItemsOfTypeInInventoryCount(iter->first, character, backpackInventory);
                }
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

            // selling item
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
                lektor<Item*> itemsToSell;
                inventory->getAllItemsWithFunction(itemsToSell, iter->first);
                if (backpackInventory)
                {
                    backpackInventory->getAllItemsWithFunction(itemsToSell, iter->first);
                }
                int amountToKeep = iter->second.quantity;
                int amountOwned = 0;
                for (int i = 0; i < itemsToSell.size(); ++i)
                {
                    amountOwned += itemsToSell[i]->quantity;
                }
                // has less or equal to keep amount
                if (amountOwned <= amountToKeep) continue;

                // tried using inventoryGUI::RClickAutoTrade but that requires the inventoryGUI to be created which can only be done safely on UI thread
                // based the following code from buySomething and buyItem code for buying stuffs
                for (int i = static_cast<int>(itemsToSell.size()) - 1; i >= 0; --i)
                {
                    // ignore equipped section
                    Item* originalItem = itemsToSell[i];
                    std::string sectionName = originalItem->inventorySection;
                    Inventory* itemInventory = originalItem->getInventoryWeAreIn().getRootObject()->getInventory();
                    InventorySection* section = itemInventory->getSection(sectionName);
                    if (iter->second.onlyStolenItems && !originalItem->isStolen(false)) continue;
                    if (section)
                    {
                        if (section->isAnEquippedItemSection) continue;
                        Item* sellItem;
                        if (originalItem->quantity < 2)
                        {
                            if (section->removeItem(originalItem))
                            {
                                sellItem = originalItem;
                            }
                            else continue;
                        }
                        else
                        {
                            sellItem = ou->theFactory->copyItem(originalItem);
                            sellItem->quantity = 1;
                            originalItem->quantity -= 1;
                        }
                        // can move this outside the item transfer block
                        // use price < 0 as a skip flag
                        int price = getTradeItemPrice(sellItem, tradingTown, character, trader, traderPlatoon, tradeCulture, true);
                        // if onlyGoodDeal then check if sell price is lower/equal to base price
                        if (iter->second.onlyGoodDeal && (price <= sellItem->getValueSingle(true))) price = -1;
                        if (price >= 0 && sellItemToShop(sellItem, price, trader, shop))
                        {
                            trader->takeMoney(price);
                            character->takeMoney(-price);
                            if (shop->getInventory()->getInventoryGUI()) shop->getInventory()->getInventoryGUI()->refreshSection();
                            if (itemInventory->getInventoryGUI()) itemInventory->getInventoryGUI()->refreshSection();

                            std::string priceText = Localization::formatMain(boost::locale::format("{1,num}") % price);
                            ScreenLabel* priceLabel = gui->createScreenLabel("+c." + priceText, MyGUI::Colour::Colour(0.6, 1.0, 0.6, 0.9), ScreenLabel::LS_LARGE, ScreenLabel::RS_SLOW);
                            priceLabel->setTracking(character, Ogre::Vector3(0.0, 15.0, 0.0));
                            character->audioEvent("Sell_Item", static_cast<SoundRange>(40000));
                            Logger::log(character->displayName + " sold " + sellItem->displayName + " for " + priceText, Logger::Info);
                            return true;
                        }
                        else
                        {
                            if (!originalItem)
                            {
                                itemInventory->addItem(sellItem, 1, false, false);
                            }
                            else
                            {
                                ou->destroy(sellItem, false, "SquadAutonomy:SellItem");
                                originalItem->quantity += 1;
                            }
                        }
                    }
                }

            }

            // buying item
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

                int itemCount = 0;
                if (inventory)
                {
                    itemCount += tradeItemsOfTypeInInventoryCount(iter->first, character, inventory);
                }
                if (backpackInventory)
                {
                    itemCount += tradeItemsOfTypeInInventoryCount(iter->first, character, backpackInventory);
                }
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
                        return a->getValueSingle(true) > b->getValueSingle(true);
                    });
                }
                // tried using inventoryGUI::RClickAutoTrade but that requires the inventoryGUI to be created which can only be done safely on UI thread
                // based the following code from buySomething and buyItem code for buying stuffs
                for (int i = 0; i < itemsToBuy.size(); ++i)
                {
                    // ignore equipped section
                    Item* originalItem = itemsToBuy[i];
                    std::string sectionName = originalItem->inventorySection;
                    Inventory* itemInventory = originalItem->getInventoryWeAreIn().getRootObject()->getInventory();
                    InventorySection* section = itemInventory->getSection(sectionName);
                    if (section)
                    {
                        Item* buyItem;
                        if (originalItem->quantity < 2)
                        {
                            if (section->removeItem(originalItem))
                            {
                                buyItem = originalItem;
                            }
                            else continue;
                        }
                        else
                        {
                            buyItem = ou->theFactory->copyItem(originalItem);
                            buyItem->quantity = 1;
                            originalItem->quantity -= 1;
                        }
                        // can move this outside the item transfer block
                        // use price < 0 as a skip flag
                        int price = getTradeItemPrice(buyItem, tradingTown, character, trader, traderPlatoon, tradeCulture, false);
                        // if onlyGoodDeal then check if buy price is higher/equal to base price
                        if (iter->second.onlyGoodDeal && (price <= buyItem->getValueSingle(true))) price = -1;
                        if (price >= 0 && buyItemFromShop(buyItem, price, character))
                        {
                            character->takeMoney(price);
                            trader->takeMoney(-price);
                            if (itemInventory->getInventoryGUI()) itemInventory->getInventoryGUI()->refreshSection();
                            if (shop->getInventory()->getInventoryGUI()) shop->getInventory()->getInventoryGUI()->refreshSection();

                            std::string priceText = Localization::formatMain(boost::locale::format("{1,num}") % price);
                            ScreenLabel* priceLabel = gui->createScreenLabel("c." + priceText, MyGUI::Colour::Colour(1.0, 1.0, 0.6, 0.9), ScreenLabel::LS_LARGE, ScreenLabel::RS_SLOW);
                            priceLabel->setTracking(character, Ogre::Vector3(0.0, 15.0, 0.0));
                            character->audioEvent("Sell_Item", static_cast<SoundRange>(40000));
                            Logger::log(character->displayName + " bought " + buyItem->displayName + " for " + priceText, Logger::Info);
                            return true;
                        }
                        else
                        {
                            if (!originalItem)
                            {
                                itemInventory->addItem(buyItem, 1, false, false);
                            }
                            else
                            {
                                ou->destroy(buyItem, false, "SquadAutonomy:BuyItem");
                                originalItem->quantity += 1;
                            }
                        }
                    }
                }
            }
            return false;
        }

        return buySomething_orig(thisptr, shop);

    }
/*
    void (*_createTradingInfo_orig)(AI* thisptr);
    void _createTradingInfo_hook(AI* thisptr)
    {
        _createTradingInfo_orig(thisptr);
        Character* character = thisptr->getCharacter();
        Platoon* squad = nullptr;
        TradingInfo* tradingInfo = thisptr->tradingInfo;
        if (character && character->getPlatoon()) squad = character->getPlatoon()->me;
        SquadSettingsInfo* settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {
            thisptr->tradingInfo->tradeItems.clear();
            if (settings->getDoShopping())
            {
                auto buyItems = settings->getBuyItems();
                for (auto iter = buyItems.begin(); iter != buyItems.end(); iter++)
                {

                    TradingInfo::TradeItem tradeItem;

                    tradeItem.itemFunction = iter->first;
                    tradeItem.quantity = iter->second;
                    tradeItem.count = 0;
                    thisptr->tradingInfo->tradeItems.push_back(tradeItem);
                    if (iter->first == ITEM_FOOD)
                    {
                        tradeItem.itemFunction = ITEM_FOOD_RESTRICTED;
                        thisptr->tradingInfo->tradeItems.push_back(tradeItem);
                    }
                }
                updateTradeItemsCount(thisptr, character);

                return;
            }
        }

    }
*/

    int tradeItemsOfTypeInInventoryCount(ItemFunction itemFunction, Character* character, Inventory* inventory)
    {
        int itemCount = 0;
        if (inventory)
        {
            lektor<Item*> itemsOfType;
            if (itemFunction == ITEM_FOOD || itemFunction == ITEM_FOOD_RESTRICTED)
            {
                inventory->getAllItemsWithFunction(itemsOfType, ITEM_FOOD);
                inventory->getAllItemsWithFunction(itemsOfType, ITEM_FOOD_RESTRICTED);
                for (int i = 0; i < itemsOfType.size(); ++i)
                {
                    itemCount += itemsOfType[i]->quantity;
                }
            }
            else
            {
                inventory->getAllItemsWithFunction(itemsOfType, itemFunction);
                for (int i = 0; i < itemsOfType.size(); ++i)
                {
                    itemCount += itemsOfType[i]->quantity;
                }
                itemsOfType.clear();
                if (itemFunction == ITEM_WEAPON)
                {
                    inventory->getEquippedWeapons(itemsOfType);
                    for (int i = 0; i < itemsOfType.size(); ++i)
                    {
                        itemCount += itemsOfType[i]->quantity;
                    }
                }
                else if (itemFunction == ITEM_CLOTHING)
                {
                    lektor<Item*> equippedArmour;
                    inventory->getEquippedArmour(itemsOfType);
                    for (int i = 0; i < itemsOfType.size(); ++i)
                    {
                        itemCount += itemsOfType[i]->quantity;
                    }
                }
            }
        }
        return itemCount;
    }

    /*
    void updateTradeItemsCount(AI* ai, Character* character)
    {
        for (int i = 0; i < ai->tradingInfo->tradeItems.size(); ++i)
        {
            auto tradeItem = &(ai->tradingInfo->tradeItems[i]);
            int itemCount = 0;
            Inventory* inventory = character->getInventory();
            if (inventory)
            {
                itemCount += tradeItemsOfTypeInInventoryCount(tradeItem->itemFunction, character, inventory);
            }
            ContainerItem* backpack = character->hasABackpackOn();
            if (backpack)
            {
                itemCount += tradeItemsOfTypeInInventoryCount(tradeItem->itemFunction, character, backpack->getInventory());
            }
            tradeItem->count = itemCount;
        }
    }
    */

    int getTradeItemPrice(Item* tradeItem, Town* tradeTown, Character* me, Character* trader, Platoon* traderSquad, TradeCulture* shopCulture, bool selling)
    {
        if (selling && tradeItem->isStolen(false) && !traderSquad->iBuyStolenGoods(tradeItem) && trader->getFencingSuccessChance(tradeItem, me) < 1.0)
        {
            Logger::logDebug(tradeItem->displayName + ": might get caught selling stolen item", SHOPPING);
            return -1;
        }
        int price = tradeItem->getValueSingle(selling);
        if (traderSquad) price *= traderSquad->priceMultWhenITrade;
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
            Logger::logDebug(tradeItem->displayName + ": " + sc::toString(price) +
                " local mult x" + sc::toString(tradeTown->getLocalTradePriceMult(itemData)) +
                " culture mult x" + sc::toString(tradeTown->getFactionTradeCultureMult(itemData)), SHOPPING);
            price *= tradeTown->getLocalTradePriceMult(itemData);
            price *= tradeTown->getFactionTradeCultureMult(itemData);
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
                Logger::logDebug(tradeItem->displayName + ": " + sc::toString(price) + " x" + sc::toString(shopCulture->hasTradePriceMultiplier(itemData)), SHOPPING);
                price *= price * shopCulture->getTradePriceMultiplier(itemData);
            }
        }
        return price;
    }

    bool hasSellableItem(SquadSettingsInfo* settings, Character* character, Building* shop)
    {
        if (!shop) return false;
        Building* shopBuilding = shop->furnitureParentBuilding();
        if (!shopBuilding) return false;
        Logger::logDebug(character->displayName + " check sellable item: " + shopBuilding->displayName, SHOPPING);
        auto sellItems = settings->getSellItems();
        Town* tradingTown = shop->getRealTown();
        TradeCulture* tradeCulture = nullptr;
        Inventory* inventory = character->getInventory();
        ContainerItem* backpack = character->hasABackpackOn();
        Inventory* backpackInventory = backpack ? backpack->getInventory() : nullptr;
        Inventory* shopInventory = shop->getInventory();
        if (!inventory || !shopInventory) return false;
        Character* trader = shop->getResidentSquadLeader();
        Platoon* traderPlatoon = nullptr;
        if (!trader) trader = shopBuilding->getResidentSquadLeader();
        if (!trader) return false;
        if (trader->getPlatoon()) traderPlatoon = trader->getPlatoon()->me;
        if (traderPlatoon && traderPlatoon->getFaction()) tradeCulture = &traderPlatoon->getFaction()->tradeCulture;
        for (auto iter = sellItems.begin(); iter != sellItems.end(); ++iter)
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

            lektor<Item*> items;
            inventory->getAllItemsWithFunction(items, iter->first);
            if (backpackInventory)
            {
                backpackInventory->getAllItemsWithFunction(items, iter->first);
            }
            int amountToKeep = iter->second.quantity;
            int amountOwned = 0;
            for (int i = 0; i < items.size(); ++i)
            {
                amountOwned += items[i]->quantity;
            }
            // has less or equal to keep amount
            if (amountOwned <= amountToKeep)
            {
                Logger::logDebug("Itemfunc: " + sc::toString(static_cast<int>(iter->first)) + " owned: " + sc::toString(amountOwned) + " amount: " + sc::toString(amountToKeep));
                continue;
            }

            for (int i = 0; i < items.size(); ++i)
            {
                auto item = items[i];
                if (iter->second.onlyStolenItems && !item->isStolen(false)) continue;
                if (!shopInventory->hasRoomForItem(item->getGameData())) continue;

                int price = getTradeItemPrice(item, tradingTown, character, trader, traderPlatoon, tradeCulture, true);
                if (price < 0) continue;
                // if onlyGoodDeal then check if sell price is lower/equal to base price
                if (iter->second.onlyGoodDeal && (price <= item->getValueSingle(true))) continue;

                // check if can afford it
                Logger::logDebug("Shop money: " + sc::toString(trader->getMoney()));
                if (price <= trader->getMoney())
                {
                    Logger::logDebug(item->displayName + " is sellable", SHOPPING);
                    return true;
                }

            }
        }
        return false;
    }


    bool checkValidBuyItem(Character* character, Item* itemToBuy)
    {
        GameData* itemData = itemToBuy->getGameData();
        if (!itemData) return false;
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

    bool hasBuyableItem(SquadSettingsInfo* settings, Character* character, Building* shop)
    {
        if (!shop) return false;
        Building* shopBuilding = shop->furnitureParentBuilding();
        if (!shopBuilding) return false;
        Logger::logDebug(character->displayName + " check buyable item: " + shopBuilding->displayName, SHOPPING);
        auto buyItems = settings->getBuyItems();
        Town* tradingTown = shop->getRealTown();
        TradeCulture* tradeCulture = nullptr;
        Inventory* inventory = character->getInventory();
        ContainerItem* backpack = character->hasABackpackOn();
        Inventory* backpackInventory = backpack ? backpack->getInventory() : nullptr;
        Inventory* shopInventory = shop->getInventory();
        if (!inventory || !shopInventory) return false;
        Character* trader = shop->getResidentSquadLeader();
        Platoon* traderPlatoon = nullptr;
        if (!trader) trader = shopBuilding->getResidentSquadLeader();
        if (trader && trader->getPlatoon()) traderPlatoon = trader->getPlatoon()->me;
        if (traderPlatoon && traderPlatoon->getFaction()) tradeCulture = &traderPlatoon->getFaction()->tradeCulture;
        for (auto iter = buyItems.begin(); iter != buyItems.end(); ++iter)
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

            int itemCount = 0;
            if (inventory)
            {
                itemCount += tradeItemsOfTypeInInventoryCount(iter->first, character, inventory);
            }
            if (backpackInventory)
            {
                itemCount += tradeItemsOfTypeInInventoryCount(iter->first, character, backpackInventory);
            }
            if (itemCount >= iter->second.quantity)
            {
                Logger::logDebug("Itemfunc: " + sc::toString(static_cast<int>(iter->first)) + " owned: " + sc::toString(itemCount) + " amount: " + sc::toString(iter->second.quantity));
                continue;
            }


            lektor<Item*> items;
            shopInventory->getAllItemsWithFunction(items, iter->first);
            if (iter->first == ITEM_FOOD)
            {
                shopInventory->getAllItemsWithFunction(items, ITEM_FOOD_RESTRICTED);
            }
            lektor<Building*> storages;
            shopBuilding->findAllFurnitureWithFunction(storages, BF_GENERAL_STORAGE);
            shopBuilding->findAllFurnitureWithFunction(storages, BF_RESOURCE_STORAGE);
            for (int i = 0; i < storages.size(); ++i)
            {
                Inventory* storageInventory = storages[i]->getInventory();
                if (!storageInventory) continue;
                storageInventory->getAllItemsWithFunction(items, iter->first);
                if (iter->first == ITEM_FOOD)
                {
                    storageInventory->getAllItemsWithFunction(items, ITEM_FOOD_RESTRICTED);
                }
            }
            for (int i = 0; i < items.size(); ++i)
            {
                auto item = items[i];
                if (!checkValidBuyItem(character, item))
                {
                    Logger::logDebug(item->displayName + " not a valid buy item", SHOPPING);
                    continue;
                }
                int price = getTradeItemPrice(item, tradingTown, character, trader, traderPlatoon, tradeCulture, false);
                if (price < 0) continue;
                // if onlyGoodDeal then check if buy price is higher/equal to base price
                if (iter->second.onlyGoodDeal && (price >= item->getValueSingle(false)))
                {
                    Logger::logDebug(item->displayName + " not a good deal", SHOPPING);
                    continue;
                }
                // check if can afford it
                Logger::logDebug("My money: " + sc::toString(character->getMoney()));
                if (price <= character->getMoney())
                {
                    Logger::logDebug(item->displayName + " is buyable", SHOPPING);
                    return true;
                }
            }
        }
        return false;
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