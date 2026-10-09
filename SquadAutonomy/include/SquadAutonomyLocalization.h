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
		static void init();
		static std::string gettext(const char* id);
		static void initLine();

		static std::string lineAIPackage;
		static std::string lineInvalidSquad;
		static std::string lineSquadAutonomy;
		static std::string lineSquadAutonomyTitle;
	private:
	};
}