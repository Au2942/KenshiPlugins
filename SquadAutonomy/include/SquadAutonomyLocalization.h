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
		static void init(unsigned int platform, const std::string& version, uintptr_t baseAddr);
		static std::string gettextMain(const char* id);
		static std::string gettextMain(const boost::locale::message& msg);
		static std::string formatMain(const boost::locale::format& fmt);
		static std::string gettext(const char* id);
		static void initLine();


		static std::string lineAIPackageList;
		static std::string lineTownList;
		static std::string lineItemCategory;
		static std::string lineInvalidSquad;
		static std::string lineSquadAutonomy;
		static std::string lineSquadAutonomyTitle;
		static std::string lineSquadAutonomyPopup;
	private:
	};
}