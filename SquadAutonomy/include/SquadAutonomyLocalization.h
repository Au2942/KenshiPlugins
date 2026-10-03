#pragma once
#include "SquadAutonomy.h"
#include <kenshi/LocaleInfo.h>
#include <boost/locale.hpp>
#include <boost/filesystem.hpp>
#include <boost/locale.hpp>

#include <Debug.h>
#include <core/Functions.h>

#include <ogre/OgreConfigFile.h>

namespace SquadAutonomy 
{
	// localization code below taken and adapted from KEP and Jim's unreleased unarmed mod
	class Localization
	{
	public:
		static void init()
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

		static std::string gettext(const char* id)
		{
			return boost::locale::gettext(id);
		}
	private:

	};
}