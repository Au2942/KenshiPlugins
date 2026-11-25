#pragma once

#include <string>
#include <extern/AreaBiomeGroup.h>
#include <kenshi/Town.h>


namespace KenshiModUtil
{

    static std::string getTownBiomeName(TownBase* town)
    {
        std::string biomeName = "";
        if (town->biome && town->biome->data) biomeName = town->biome->data->name;
        std::string townName = town->getKnownName();
        if (!biomeName.empty() && biomeName != "NONE") townName += ", " + biomeName;
        return townName;
    }
}