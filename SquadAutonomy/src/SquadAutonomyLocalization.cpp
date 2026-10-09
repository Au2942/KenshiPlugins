#include "SquadAutonomyLocalization.h"

namespace SquadAutonomy
{
	std::string Localization::lineAIPackage = "";
	std::string Localization::lineInvalidSquad = "";
	std::string Localization::lineSquadAutonomy = "";
	std::string Localization::lineSquadAutonomyTitle = "";
	void Localization::init()
	{
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
		lineAIPackage = Localization::gettext("AI package");
		lineInvalidSquad = Localization::gettext("No squad selected/invalid squad");
		lineSquadAutonomy = Localization::gettext("Squad Autonomy");
		lineSquadAutonomyTitle = Localization::gettext("Squad Autonomy:");
	}

	std::string Localization::gettext(const char* id)
	{
		return boost::locale::gettext(id);
	}
}