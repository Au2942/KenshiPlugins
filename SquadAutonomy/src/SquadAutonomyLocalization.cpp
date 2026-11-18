#include "SquadAutonomyLocalization.h"
#include <kenshi/Globals.h>

namespace SquadAutonomy
{
	// from KEP translation.cpp
	std::string(boost::locale::message::* messageStr)() const = nullptr;
	std::string(boost::locale::format::* formatStr)(const std::locale&) const = nullptr;

	std::string Localization::lineAIPackageList = "";
	std::string Localization::lineTownList = "";
	std::string Localization::lineItemCategory = "";
	std::string Localization::lineInvalidSquad = "";
	std::string Localization::lineSquadAutonomy = "";
	std::string Localization::lineSquadAutonomyTitle = "";
	void Localization::init(unsigned int platform, const std::string& version, uintptr_t baseAddr)
	{
		if (version == "1.0.65")
		{
			*(uintptr_t*)&messageStr = baseAddr + 0xa9580;
			*(uintptr_t*)&formatStr = baseAddr + 0xa9730;
		}

		Ogre::ConfigFile config;
		config.load("settings.cfg");
		std::string language = config.getSetting("language");

		boost::locale::generator gen;
		//SquadAutonomy.mo
		gen.add_messages_path((converter.to_bytes(modPath) + '/' + "locale"));
		gen.add_messages_domain("SquadAutonomy");
		std::locale::global(gen.generate(std::locale(), language + ".UTF-8"));

		initLine();
	}

	void Localization::initLine()
	{
		lineAIPackageList = Localization::gettext("AI Package List");
		lineTownList = Localization::gettext("Town");
		lineItemCategory = Localization::gettext("Item Category");
		lineInvalidSquad = Localization::gettext("No squad selected/invalid squad");
		lineSquadAutonomy = Localization::gettext("Squad Autonomy");
		lineSquadAutonomyTitle = Localization::gettext("Squad Autonomy:");
	}

	std::string Localization::gettextMain(const char* id)
	{
		return (boost::locale::translate(id).*messageStr)();
	}

	std::string Localization::gettextMain(const boost::locale::message& msg)
	{
		return (msg.*messageStr)();
	}

	std::string Localization::formatMain(const boost::locale::format& fmt)
	{
		return (fmt.*formatStr)(*globalLocale);
	}

	std::string Localization::gettext(const char* id)
	{
		return boost::locale::gettext(id);
	}
}