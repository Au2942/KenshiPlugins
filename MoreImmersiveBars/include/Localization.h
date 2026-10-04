#pragma once
#include "MoreImmersiveBars.h"

#include <kenshi/LocaleInfo.h>
#include <boost/locale.hpp>
#include <boost/filesystem.hpp>
#include <boost/locale.hpp>
#include <ogre/OgreConfigFile.h>

using namespace MoreImmersiveBars;

class Localization
{
public:
    static void init()
    {
        Ogre::ConfigFile config;
        config.load("settings.cfg");
        std::string language = config.getSetting("language");

        boost::locale::generator gen;
        //MoreImmersiveBars.mo
        gen.add_messages_path((converter.to_bytes(modPath) + '/' + "locale"));
        gen.add_messages_domain("MoreImmersiveBars");
        std::locale::global(gen.generate(std::locale(), language + ".UTF-8"));
    }

    static std::string gettext(const char* id)
    {
        return boost::locale::gettext(id);
    }
private:

};