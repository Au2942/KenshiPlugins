// taken from Lucius's KEP https://github.com/Lucius64/KenshiExtensionPlugin
#pragma once
#include <ogre/OgrePrerequisites.h>
#include <kenshi/Enums.h>
class GameData;
class Town;

class TradingInfo
{
public:
	class TradeItem
	{
	public:
		ItemFunction itemFunction;
		int quantity;
		int count;
	};

	GameData* data;
	Ogre::vector<TradeItem, Ogre::STLAllocator<TradingInfo::TradeItem, Ogre::CategorisedAllocPolicy<Ogre::MEMCATEGORY_GENERAL>>>::type tradeItems;
	int day;
	Town* town;
};