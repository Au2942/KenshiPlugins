#include <Debug.h>

#include <ogre/OgreStringConverter.h>

#include <kenshi/Globals.h>
#include <kenshi/GameData.h>
#include <kenshi/Character.h>
#include <kenshi/CharBody.h>
#include <kenshi/StateBroadcastData.h>
#include <kenshi/util/YesNoMaybe.h>
#include <kenshi/Faction.h>
#include <kenshi/AI/AI.h>
#include <kenshi/AI/AITaskSystem.h>
#include <kenshi/AI/AIPackage.h>
#include <kenshi/Tasker.h>
#include <kenshi/Platoon.h>
#include <kenshi/Town.h>
#include <kenshi/AI/Blackboard.h>
#include <kenshi/Building/UseableStuff.h>

#include <core/Functions.h>

#include "lektorExtension.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>



namespace MoreImmersiveBars
{
    lektor<UseableStuff*> GetCurrentTownBarsBeds(Character* character)
    {
        lektor<UseableStuff*> beds;
        if (character)
        {
            TownBase* currentTown = character->getCurrentTownLocation();
            if (currentTown)
            {
                //find homebase beds

                //find friendly faction beds?

                //find bar beds
                //lektor<UseableStuff*> beds;
                lektor<Building*> bars = *currentTown->findAllBuildingsOfType(BD_BAR, character);
                float maxScore = -1; // std::numeric_limits<float>::min();
                for (uint32_t i = 0; i < bars.size(); ++i)
                {
                    lektor<Building*> barBeds;
                    bars[i]->findAllFurnitureWithFunction(barBeds, BF_BED);
                    for (uint32_t j = 0; j < barBeds.size(); ++j)
                    {
                        UseableStuff* b = barBeds[j]->getUseableStuff();
                        lektorEx::push_back_unique(beds, b);
                    }
                }
            }
        }
        return beds;
    }

    lektor<UseableStuff*> GetCurrentBarBeds(Character* character)
    {
        lektor<UseableStuff*> beds;
        if (character)
        {
            hand building = character->isInsideBuilding;
            if (building && building.getBuilding()->designation == BD_BAR)
            {
                Building* bar = building.getBuilding();

                lektor<Building*> barBeds;
                bar->findAllFurnitureWithFunction(barBeds, BF_BED);
                for (uint32_t i = 0; i < barBeds.size(); ++i)
                {
                    UseableStuff* b = barBeds[i]->getUseableStuff();
                    lektorEx::push_back_unique(beds, b);
                }
            }
        }
        return beds;
    }

    hand* FindOptimalBarsBed(Character* character)
    {
        UseableStuff* optimalBed;
        if (character)
        {
            float maxScore = -1; // std::numeric_limits<float>::min();
            lektor<UseableStuff*> barBeds = GetCurrentTownBarsBeds(character);
            for (uint32_t i = 0; i < barBeds.size(); ++i)
            {
                UseableStuff* b = barBeds[i]->getUseableStuff();
                if (b->isPublic())
                {
                    if (!b->getOccupant())
                    {
                        float distanceScore = character->ai->scoreDistanceTo(b, false);
                        if (maxScore < distanceScore)
                        {
                            maxScore = distanceScore;
                            optimalBed = b;
                        }
                    }
                    else if (b->getOccupant() == character) //try to use the same bed again - prevent beds hopping
                    {
                        optimalBed = b;
                        break;
                    }
                }
            }
        }
        if (optimalBed) return &optimalBed->handle;
        else return nullptr;
    }



    hand* FindOptimalCurrentBarBed(Character* character)
    {
        UseableStuff* optimalBed;
        if (character)
        {
            hand building = character->isInsideBuilding;
            if (building && building.getBuilding()->designation == BD_BAR)
            {
                Building* bar = building.getBuilding();

                float maxScore = -1; // std::numeric_limits<float>::min();
                lektor<Building*> barBeds;
                bar->findAllFurnitureWithFunction(barBeds, BF_BED);
                for (uint32_t i = 0; i < barBeds.size(); ++i)
                {
                    UseableStuff* b = barBeds[i]->getUseableStuff();
                    if (b->isPublic())
                    {
                        if (!b->getOccupant())
                        {
                            float distanceScore = character->ai->scoreDistanceTo(b, false);
                            if (maxScore < distanceScore)
                            {
                                maxScore = distanceScore;
                                optimalBed = b;
                            }
                        }
                        else if (b->getOccupant() == character) //try to use the same bed again - prevent beds hopping
                        {
                            optimalBed = b;
                            break;
                        }
                    }
                }
            }

        }
        if (optimalBed) return &optimalBed->handle;
        else return nullptr;
    }

    int (*_NV_getCostToUse_orig)(UseableStuff* thisptr, Character* who);
    int _NV_getCostToUse_hook(UseableStuff* thisptr, Character* who)
    {
        if (thisptr->specialFunction == BF_BED)
        {
            Blackboard* bb; 
            if (who) bb = who->getBlackboard();
            std::string aiPackageName = "";
            if(bb) aiPackageName = bb->getCurrentAIPackageName();
            if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering" || aiPackageName == "town thugs night patrol + day bar")
            {
                bool isBarBed = false;
                lektor<UseableStuff*>barsbeds = GetCurrentTownBarsBeds(who);
                //DebugLog("Checking " + Ogre::StringConverter::toString(barsbeds.count) + " beds");
                for (uint32_t i = 0; i < barsbeds.size(); ++i)
                {
                    if (barsbeds[i]->getHandle().index == thisptr->getHandle().index)
                    {
                        isBarBed = true;
                        break;
                    }
                }
                if (!isBarBed)
                {
                    return -1;
                }
                if (who && who->getFaction()->notARealFaction)
                {
                    return 0;
                }
            }
        }
        return _NV_getCostToUse_orig(thisptr, who);
    }

    void (*_NV_setCurrentGoal_orig)(AITaskSytem* thisptr, Tasker* t, float score, taskPriority pri);
    void _NV_setCurrentGoal_hook(AITaskSytem* thisptr, Tasker* t, float score, taskPriority pri)
    {

        Blackboard* bb = nullptr;
        if (thisptr->character) bb = thisptr->character->getBlackboard();
        if (bb)
        {
            std::string aiPackageName = bb->getCurrentAIPackageName();
            //limit how many bar guards can go to sleep in a squad
            if (aiPackageName == "Shop-24hr")
            {
                if (thisptr->_squadMemberType != SQUAD_LEADER)
                {
                    if (t->key() == GO_HOME_AND_GO_TO_BED)
                    {
                        //DebugLog(thisptr->character->displayName + " is trying to sleep!");
                        //DebugLog("Squad leader is " + thisptr->character->getSquadLeader()->displayName);
                        int maxSlackers = (bb->characterCount - 1) / 2 ;
                        if (maxSlackers < 1) maxSlackers = 1;
                        int currentSlackers = bb->howManyGuysDoingThisGoal(t, thisptr->character);
                        Character* leader = thisptr->character->platoon->squadleader;
                        if (leader->getStateBroadcast()->isSleeping)
                        {
                            currentSlackers -= 1;
                        }
                        //DebugLog(" max slackers: " + Ogre::StringConverter::toString(maxSlackers) + " current: " + Ogre::StringConverter::toString(currentSlackers));
                        if (bb->howManyGuysDoingThisGoal(t, thisptr->character) >= maxSlackers)
                        {
                            thisptr->body->_endAction();
                            thisptr->clearCurrentGoal(true);
                            return;
                        }
                    }
                }
            }

            _NV_setCurrentGoal_orig(thisptr, t, score, pri);

            if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering" || aiPackageName == "town thugs night patrol + day bar")
            {
                if (t->key() == RELAX_IN_TOWN_PACKAGE)
                {
                    TaskData* taskData = t->taskData;
                    if (taskData)
                    {
                        std::string description = t->getDescription();
                        if (description == "Relaxing")
                        {
                            taskData->setDurationBased(0.5, 8.0, false);
                        }
                    }
                }
            }
        }
        else _NV_setCurrentGoal_orig(thisptr, t, score, pri);
    }

    float (*runTargetFind_orig)(TaskData* thisptr, AI* ai, const hand& _target, hand& out, bool justAsking);
    float runTargetFind_hook(TaskData* thisptr, AI* ai, const hand& _target, hand& out, bool justAsking)
    {
        float score = runTargetFind_orig(thisptr, ai, _target, out, justAsking);
        if (thisptr->key == GO_HOME_AND_GO_TO_BED)
        {
            if (!ai) return score;
            if (!(ai->getBlackboard())) return score;
            std::string aiPackageName = ai->getBlackboard()->getCurrentAIPackageName();
            if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering" || aiPackageName == "town thugs night patrol + day bar")
            {
                Character* ch = ai->getCharacter();
                if (ch)
                {

                    hand* bed = MoreImmersiveBars::FindOptimalCurrentBarBed(ch);
                    if (bed)
                    {
                        //DebugLog("Find Target: Found bed");
                        out = *bed;
                        return 1.0f;
                    }
                    out = nullptr;
                    return -1.0f;
                }
            }
        }
        return score;
    }
}

__declspec(dllexport) void startPlugin()
{

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&UseableStuff::_NV_getCostToUse), &MoreImmersiveBars::_NV_getCostToUse_hook, &MoreImmersiveBars::_NV_getCostToUse_orig))
        ErrorLog("More Immersive Bars: Could not add getCostToUse hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&AITaskSytem::_NV_setCurrentGoal), &MoreImmersiveBars::_NV_setCurrentGoal_hook, &MoreImmersiveBars::_NV_setCurrentGoal_orig))
        ErrorLog("More Immersive Bars: Could not add setCurrentGoal hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&TaskData::runTargetFind), &MoreImmersiveBars::runTargetFind_hook, &MoreImmersiveBars::runTargetFind_orig))
        ErrorLog("More Immersive Bars: Could not add runTargetFind hook!");
}



