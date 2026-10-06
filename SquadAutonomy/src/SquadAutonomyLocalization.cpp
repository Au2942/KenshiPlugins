#include "SquadAutonomyLocalization.h"

namespace SquadAutonomy
{
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
	}

	std::string Localization::gettext(const char* id)
	{
		return boost::locale::gettext(id);
	}
}