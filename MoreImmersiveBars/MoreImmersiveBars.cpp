#include <Debug.h>

#include <ogre/OgreStringConverter.h>

#include <kenshi/Kenshi.h>
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/GameData.h>
#include <kenshi/Dialogue.h>
#include <kenshi/Character.h>
#include <kenshi/RaceData.h>
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
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

#include <core/Functions.h>

#include "lektorExtension.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


std::map<hand, float>* rentedBeds = nullptr;
typedef Ogre::StringConverter stringConverter;
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
                        if (b) lektorEx::push_back_unique(beds, b);
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
        UseableStuff* optimalBed = nullptr;
        Building* building = nullptr;
        if (!character) return nullptr;

        float maxScore = -1; // std::numeric_limits<float>::lowest();
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
        if (optimalBed) return &optimalBed->handle;
        else return nullptr;
    }



    hand* FindOptimalCurrentBarBed(Character* character)
    {
        UseableStuff* optimalBed = nullptr;
        Building* building = nullptr;
        if (!character) return nullptr;
        AI* ai = character->ai;
        if (!ai) return nullptr;
        RaceData* race = character->getRace();
        hand buildingHand = character->isInsideBuilding;
        if (buildingHand) building = buildingHand.getBuilding();
        if (building && building->designation == BD_BAR)
        {
            float maxScore = std::numeric_limits<float>::lowest();
            lektor<Building*> barBeds;
            BuildingFunction bf = BF_BED;
            if (race && race->robot)
            {
                bf = BF_SKELETON_BED;
            }
            building->findAllFurnitureWithFunction(barBeds, bf);
            for (uint32_t i = 0; i < barBeds.size(); ++i)
            {
                bool isRented = false;
                if (!barBeds[i]) continue;
                UseableStuff* b = barBeds[i]->getUseableStuff();
                if (!b) continue;
                //check if rented by player
                //DebugLog("rentedBeds : " + Ogre::StringConverter::toString((*rentedBeds).size()));
                if (rentedBeds)
                {
                    for (auto it = (*rentedBeds).begin(); it != (*rentedBeds).end(); ++it)
                    {
                        UseableStuff* rentedBed = nullptr;
                        Building* bedBuilding = nullptr;
                        if (it->first) bedBuilding = it->first.getBuilding();
                        if (bedBuilding) rentedBed = bedBuilding->getUseableStuff();
                        if (rentedBed)
                        {
                            //TODO: check time as well
                            //DebugLog("Rented bed cost: " + Ogre::StringConverter::toString(rentedCost));
                            if (rentedBed == b)
                            {
                                //TODO: check time as well
                                //DebugLog("Bed already rented!");
                                float currentTime = ou->getTimeStamp_inGameHours().getTotalHours();
                                //DebugLog("Time now: " + Ogre::StringConverter::toString(currentTime) + " rented time: " + Ogre::StringConverter::toString(it->second));
                                if (currentTime - it->second < 24.0)
                                {
                                    isRented = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (isRented) continue;
                //try to use the same bed again - prevent beds hopping
                if (b->getOccupant())
                {
                    if (b->getOccupant() == character->getHandle())
                    {
                        optimalBed = b;
                        break;
                    }
                    else continue;
                } 
                if (b->getCostToUse(character) >= 0)
                {
                    float distanceScore = character->ai->scoreDistanceTo(b, false);
                    if (maxScore < distanceScore)
                    {
                        maxScore = distanceScore;
                        optimalBed = b;
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
        if (thisptr && (thisptr->specialFunction == BF_BED || thisptr->specialFunction == BF_SKELETON_BED))
        {
            Blackboard* bb = nullptr; 
            if (who) bb = who->getBlackboard();
            std::string aiPackageName = "";
            if(bb) aiPackageName = bb->getCurrentAIPackageName();
            if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering" || aiPackageName == "town thugs night patrol + day bar")
            {
                Building* building = nullptr;
                if (thisptr->isFurnitureOrDoor())
                {
                    if (thisptr->isDoor())
                    {
                        //DebugLog("Is a door of " + building->doorParentBuilding()->displayName);
                        building = thisptr->doorParentBuilding();
                    }
                    else if (thisptr->isFurniture())
                    {
                        //DebugLog("Is a furniture of " + building->furnitureParentBuilding()->displayName);
                        building = thisptr->furnitureParentBuilding();
                    }
                }
                if (building && building->getBuildingDesignation() == BD_BAR)
                {
                    if (who && who->getFaction() && who->getFaction()->notARealFaction)
                    {
                        return 0;
                    }
                }
            }
        }
        return _NV_getCostToUse_orig(thisptr, who);
    }

    const float relaxDurationMin = 1.0;
    const float relaxDurationFuzz = 4.0;
    const bool relaxDurationBased = false;

    //do this in setup parameter for current goal instead?
    void (*_NV_setCurrentGoal_orig)(AITaskSytem* thisptr, Tasker* t, float score, taskPriority pri);
    void _NV_setCurrentGoal_hook(AITaskSytem* thisptr, Tasker* t, float score, taskPriority pri)
    {

        Blackboard* bb = nullptr;
        Character* character = nullptr;
        TaskData* data = nullptr;
        float min = 0;
        float fuzz = 0;
        bool isDurationBased = false;
        bool resetTaskData = false;
        if (thisptr) character = thisptr->character;
        if (character) bb = character->getBlackboard();
        if (bb)
        {
            if (t && t->key() == RELAX_IN_TOWN_PACKAGE)
            {
                std::string aiPackageName = bb->getCurrentAIPackageName();
                if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering" 
                    || aiPackageName == "town thugs night patrol + day bar")
                {
                    data = t->taskData;
                    if (data)
                    {
                        min = data->durationMin;
                        fuzz = data->durationFuzz;
                        isDurationBased = data->isDurationBased;
                        resetTaskData = true;
                        //DebugLog("Relax Original: " + stringConverter::toString(min) + ' ' + stringConverter::toString(fuzz) + ' ' + stringConverter::toString(isDurationBased));
                        data->setDurationBased(relaxDurationMin, relaxDurationFuzz, relaxDurationBased);
                    }
                }
            }
        }
        /*========Actual Function=======*/
        _NV_setCurrentGoal_orig(thisptr, t, score, pri);
        /*==============================*/
        if (resetTaskData)
        {
            if (data)
            {
                data->setDurationBased(min, fuzz, isDurationBased);
            }
        }
    }

    void (*setTaskExpiryTimer_orig)(AITaskSytem* thisptr);
    void setTaskExpiryTimer_hook(AITaskSytem* thisptr)
    {
        Blackboard* bb = nullptr;
        Character* character = nullptr;
        TaskData* data = nullptr;
        Tasker* currentDurationTT = nullptr;
        float min = 0;
        float fuzz = 0;
        bool isDurationBased = false;
        bool resetTaskData = false;
        if (thisptr)
        {
            character = thisptr->character;
            currentDurationTT = thisptr->getCurrentDurationTimedTask();
        }

        if (character) bb = character->getBlackboard();
        if (bb)
        {
            if (currentDurationTT && currentDurationTT->key() == RELAX_IN_TOWN_PACKAGE)
            {
                std::string aiPackageName = bb->getCurrentAIPackageName();
                if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering"
                    || aiPackageName == "town thugs night patrol + day bar")
                {
                    data = currentDurationTT->taskData;
                    if (data)
                    {
                        min = data->durationMin;
                        fuzz = data->durationFuzz;
                        isDurationBased = data->isDurationBased;
                        resetTaskData = true;
                        //DebugLog("Relax Original: " + stringConverter::toString(min) + ' ' + stringConverter::toString(fuzz) + ' ' + stringConverter::toString(isDurationBased));
                        data->setDurationBased(relaxDurationMin, relaxDurationFuzz, relaxDurationBased);
                    }
                }
            }
        }
        /*========Actual Function=======*/
        setTaskExpiryTimer_orig(thisptr);
        /*==============================*/
        if (resetTaskData)
        {
            if (data)
            {
                data->setDurationBased(min, fuzz, isDurationBased);
            }
        }
    }

    // Hook the method (call interception) — the primary mod mechanism
    void (*update4Frame_orig)(AITaskSytem* thisptr, Ogre::Vector3 position, float time);
    void update4Frame_hook(AITaskSytem* thisptr, Ogre::Vector3 position, float time)
    {
        Blackboard* bb = nullptr;
        Character* character = nullptr;
        TaskData* data = nullptr;
        Tasker* currentDurationTT = nullptr;
        float min = 0;
        float fuzz = 0;
        bool isDurationBased = false;
        bool resetTaskData = false;
        if (thisptr)
        {
            character = thisptr->character;
            currentDurationTT = thisptr->getCurrentDurationTimedTask();
        }

        if (character) bb = character->getBlackboard();
        if (bb)
        {
            if (currentDurationTT && currentDurationTT->key() == RELAX_IN_TOWN_PACKAGE)
            {
                std::string aiPackageName = bb->getCurrentAIPackageName();
                if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering"
                    || aiPackageName == "town thugs night patrol + day bar")
                {
                    data = currentDurationTT->taskData;
                    if (data)
                    {
                        min = data->durationMin;
                        fuzz = data->durationFuzz;
                        isDurationBased = data->isDurationBased;
                        resetTaskData = true;
                        //DebugLog("Relax Original: " + stringConverter::toString(min) + ' ' + stringConverter::toString(fuzz) + ' ' + stringConverter::toString(isDurationBased));
                        data->setDurationBased(relaxDurationMin, relaxDurationFuzz, relaxDurationBased);
                    }
                }
            }
        }
        /*========Actual Function=======*/
        update4Frame_orig(thisptr, position, time);
        /*==============================*/
        if (resetTaskData)
        {
            if (data)
            {
                data->setDurationBased(min, fuzz, isDurationBased);
            }
        }
    }
    // install (in startPlugin):

    /*float (*runTargetFind_orig)(TaskData* thisptr, AI* ai, const hand& _target, hand& out, bool justAsking);
    float runTargetFind_hook(TaskData* thisptr, AI* ai, const hand& _target, hand& out, bool justAsking)
    {
        float score = runTargetFind_orig(thisptr, ai, _target, out, justAsking);
        if (thisptr && thisptr->key == GO_HOME_AND_GO_TO_BED)
        {
            if (!ai) return score;
            if (!(ai->getBlackboard())) return score;
            std::string aiPackageName = ai->getBlackboard()->getCurrentAIPackageName();
            if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering" || 
                aiPackageName == "town thugs night patrol + day bar" || aiPackageName == "Shop-24hr" || aiPackageName == "Civilian")
            {
                Character* ch = ai->getCharacter();
                if (ch)
                {
                    hand* bed = MoreImmersiveBars::FindOptimalCurrentBarBed(ch);
                    if (bed)
                    {
                        out = *bed;
                        return 1.0f;
                    }
                    out = nullptr;
                    return 0.0f;
                }
            }
        }
        return score;
    }*/
    float (*runTargetFinder_orig)(AI::AIResultsCacher* thisptr, float (AI::* func)(const hand&, hand&, bool), const TaskMatch& key, hand& out);
    float runTargetFinder_hook(AI::AIResultsCacher* thisptr, float (AI::* func)(const hand&, hand&, bool), const TaskMatch& key, hand& out)
    {
        AI* ai = nullptr;
        Blackboard* bb = nullptr;
        if (thisptr) ai = thisptr->ai;
        if (ai)
        {
            bb = ai->getBlackboard();
            if (bb && key && key.key() == GO_HOME_AND_GO_TO_BED)
            {
                std::string aiPackageName = bb->getCurrentAIPackageName();
                if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering" ||
                    aiPackageName == "town thugs night patrol + day bar" || aiPackageName == "Shop-24hr" || aiPackageName == "Civilian")
                {
                    Character* ch = ai->getCharacter();
                    if (ch)
                    {
                        hand* bed = MoreImmersiveBars::FindOptimalCurrentBarBed(ch);
                        if (bed)
                        {
                            out = *bed;
                            return 1.0f;
                        }
                        out = nullptr;
                        return 0.0f;
                    }
                }
            }
        }
        return runTargetFinder_orig(thisptr, func, key, out);
    }

    float (*score_orig)(Tasker* thisptr, AI* ai);
    float score_hook(Tasker* thisptr, AI* ai)
    {
        float score = score_orig(thisptr, ai);

        Blackboard* bb = nullptr;
        Character* character = nullptr;
        Platoon* platoon = nullptr;
        if (ai)
        {
            bb = ai->getBlackboard();
            character = ai->getCharacter();
            platoon = ai->getPlatoon();
        }
        if (thisptr && thisptr->key() == GO_HOME_AND_GO_TO_BED)
        {
            if (bb && character && platoon)
            {
                std::string aiPackageName = bb->getCurrentAIPackageName();
                StateBroadcastData* state = character->getStateBroadcast();
                if (aiPackageName == "Shop-24hr" && platoon->squadType != SQUAD_LEADER)
                {

                    //DebugLog(thisptr->character->displayName + " is trying to sleep!");
                    //DebugLog("Squad leader is " + thisptr->character->getSquadLeader()->displayName);
                    int maxSlackers = (bb->characterCount) / 2;
                    if (maxSlackers < 1) maxSlackers = 1;
                    int currentSlackers = bb->howManyGuysDoingThisGoal(thisptr, character);
                        
                    //DebugLog(" max slackers: " + Ogre::StringConverter::toString(maxSlackers) + " current: " + Ogre::StringConverter::toString(currentSlackers));
                    if (currentSlackers > maxSlackers)
                    {
                        return 0.0;
                    }
                }
                if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering" 
                    || aiPackageName == "town thugs night patrol + day bar" || aiPackageName == "Shop-24hr")
                {
                    if (state)
                    {
                        if (state->lastSlept < 180.0) return score * 0.1;
                        if (state->lastSlept < 360.0) return score * 0.3;
                        if (state->lastSlept < 640.0) return score * 0.5;
                        if (state->lastSlept > 1080.0) return score * 10.0;
                    }
                }
            }

        }
        return score;
    }

    bool (*checkTags_orig)(DialogLineData* thisptr, Character* me, Character* target);
    bool checkTags_hook(DialogLineData* thisptr, Character* me, Character* target)
    {
        Blackboard* bb = nullptr;
        if (target) bb = me->getBlackboard();
        if (bb) 
        {
            std::string aiPackageName = bb->getCurrentAIPackageName();
            if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering" 
                || aiPackageName == "town thugs night patrol + day bar" || aiPackageName == "Shop-24hr")
            {
                StateBroadcastData* statebroadcast = me->getStateBroadcast();
                if (statebroadcast && statebroadcast->isSleeping)
                {
                    return false;
                }
            }
        }
        return checkTags_orig(thisptr, me, target);
    }


}



__declspec(dllexport) void startPlugin()
{
    auto versionInfo = KenshiLib::GetKenshiVersion();
    auto platform = versionInfo.GetPlatform();
    auto version = versionInfo.GetVersion();
    auto baseAddr = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));

    if (version == "1.0.65")
    {
        if (platform == 1)
        {
            *(uintptr_t*)&rentedBeds = baseAddr + 0x212db18;
        }
        else if (platform == 0)
        {
            *(uintptr_t*)&rentedBeds = baseAddr + 0x212BA58;
        }
    }
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&UseableStuff::_NV_getCostToUse), &MoreImmersiveBars::_NV_getCostToUse_hook, &MoreImmersiveBars::_NV_getCostToUse_orig))
        ErrorLog("Could not add UseableStuff::_NV_getCostToUse hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::_NV_setCurrentGoal), &MoreImmersiveBars::_NV_setCurrentGoal_hook, &MoreImmersiveBars::_NV_setCurrentGoal_orig))
        ErrorLog("Could not add AITaskSytem::_NV_setCurrentGoal hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::setTaskExpiryTimer), &MoreImmersiveBars::setTaskExpiryTimer_hook, &MoreImmersiveBars::setTaskExpiryTimer_orig))
        ErrorLog("Could not add AITaskSytem::setTaskExpiryTimer hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::update4Frame), &MoreImmersiveBars::update4Frame_hook, &MoreImmersiveBars::update4Frame_orig))
        ErrorLog("Could not add AITaskSytem::update4Frame hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&TaskData::runTargetFind), &MoreImmersiveBars::runTargetFind_hook, &MoreImmersiveBars::runTargetFind_orig))
        ErrorLog("Could not add TaskData::runTargetFind hook!");*/

    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::AIResultsCacher::runTargetFinder), &MoreImmersiveBars::runTargetFinder_hook, &MoreImmersiveBars::runTargetFinder_orig))
        ErrorLog("Could not add AI::AIResultsCacher::runTargetFinder hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&Tasker::score), &MoreImmersiveBars::score_hook, &MoreImmersiveBars::score_orig))
        ErrorLog("Could not add Tasker::score hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&DialogLineData::checkTags), &MoreImmersiveBars::checkTags_hook, &MoreImmersiveBars::checkTags_orig))
        ErrorLog("Could not add DialogLineData::checkTags hook!");


    KenshiLib::ApplyQueuedHooks();
    DebugLog("Mod started");
}



