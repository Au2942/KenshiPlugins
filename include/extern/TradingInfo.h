// taken from Lucius's KEP https://github.com/Lucius64/KenshiExtensionPlugin
#pragma once
#include <ogre/OgrePrerequisites.h>

class GameData;
class Town;
enum ItemFunction;

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
	Ogre::vector<TradeItem>::type tradeItems;
	int day;
	Town* town;
};