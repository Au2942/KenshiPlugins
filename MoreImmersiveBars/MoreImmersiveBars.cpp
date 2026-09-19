#include "MoreImmersiveBars.h"
#include "ModOptions.h"
#include <Debug.h>

#include <ogre/OgreStringConverter.h>
#include <boost/scoped_ptr.hpp>
#include <ogre/OgrePrerequisites.h>
#include <kenshi/util/OgreUnordered.h>

#include <kenshi/gui/OptionsWindow.h>
#include <kenshi/gui/ToolTip.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>
#include <kenshi/gui/ForgottenGUI.h>

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


#include <core/Functions.h>

#include "lektorExtension.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>



typedef Ogre::StringConverter stringConverter;
namespace MoreImmersiveBars
{
    ogre_unordered_map<TaskType, TaskData*>::type* taskTypetaskData = nullptr;
    const TaskData* (*getTaskDataConst)(TaskType key) = nullptr;
    std::map<hand, float>* rentedBeds = nullptr;
    std::string* _MainColorCode = nullptr;

    std::string cfgPath = "";
    bool noSleepTalk = true;

    class OriginalTaskDataDuration
    {
    public:
        float durationMin;
        float durationFuzz;
        bool isDurationBased;
        bool endsAfterTime;
        OriginalTaskDataDuration(float min, float fuzz, bool based, bool ends)
        {
            durationMin = min;
            durationFuzz = fuzz;
            isDurationBased = based;
            endsAfterTime = ends;
        }
    };
    std::unordered_map<TaskType, OriginalTaskDataDuration*> taskTypeOrigDataDuration;

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

        float minScore = std::numeric_limits<float>::max();
        lektor<UseableStuff*> barBeds = GetCurrentTownBarsBeds(character);
        for (uint32_t i = 0; i < barBeds.size(); ++i)
        {
            UseableStuff* b = barBeds[i]->getUseableStuff();
            if (b->isPublic())
            {
                if (!b->getOccupant())
                {
                    float distanceScore = character->ai->scoreDistanceTo(b, false);
                    if (minScore > distanceScore)
                    {
                        minScore = distanceScore;
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
            float minScore = std::numeric_limits<float>::max();
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
                    if (minScore > distanceScore)
                    {
                        minScore = distanceScore;
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


    void RevertTaskDuration(TaskType type)
    {
        TaskData* data = taskTypetaskData->find(type)->second;
        if (taskTypeOrigDataDuration.find(type) != taskTypeOrigDataDuration.end())
        {
            //DebugLog("Reverting Duration!");
            auto orig = taskTypeOrigDataDuration.find(type)->second;
            data->durationMin = orig->durationMin;
            data->durationFuzz = orig->durationFuzz;
            data->isDurationBased = orig->isDurationBased;
            data->endsAfterTime = orig->endsAfterTime;
        }
    }

    // Hook the method (call interception) — the primary mod mechanism
    void (*periodicUpdate_orig)(AITaskSytem* thisptr, float time);
    void periodicUpdate_hook(AITaskSytem* thisptr, float time)
    {
        Blackboard* bb = nullptr;
        Character* character = nullptr;
        std::string aiPackageName = "";
        if (thisptr) character = thisptr->character;
        if (character) bb = character->getBlackboard();
        if (bb)
        {
            aiPackageName = bb->getCurrentAIPackageName();
        }
        if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering"
            || aiPackageName == "town thugs night patrol + day bar")
        {
            //DebugLog("Periodic Update!");
            TaskData* data = taskTypetaskData->find(RELAX_IN_TOWN_PACKAGE)->second;
            data->setDurationBased(relaxDurationMin, relaxDurationFuzz, relaxDurationBased);
        }
        periodicUpdate_orig(thisptr, time);
        if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering"
            || aiPackageName == "town thugs night patrol + day bar")
        {
            RevertTaskDuration(RELAX_IN_TOWN_PACKAGE);
        }
    }

    // Hook the method (call interception) — the primary mod mechanism
    void (*update4Frame_orig)(AITaskSytem* thisptr, Ogre::Vector3 position, float time);
    void update4Frame_hook(AITaskSytem* thisptr, Ogre::Vector3 position, float time)
    {
        Blackboard* bb = nullptr;
        Character* character = nullptr;
        std::string aiPackageName = "";
        if (thisptr) character = thisptr->character;
        if (character) bb = character->getBlackboard();
        if (bb)
        {
            aiPackageName = bb->getCurrentAIPackageName();
        }
        if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering"
            || aiPackageName == "town thugs night patrol + day bar")
        {
            //DebugLog("Update4Frame!");
            TaskData* data = taskTypetaskData->find(RELAX_IN_TOWN_PACKAGE)->second;
            data->setDurationBased(relaxDurationMin, relaxDurationFuzz, relaxDurationBased);
        }
        /*========Actual Function=======*/
        update4Frame_orig(thisptr, position, time);
        /*==============================*/
        if (aiPackageName == "hang out in a bar" || aiPackageName == "hang out in a bar with slave gathering"
            || aiPackageName == "town thugs night patrol + day bar")
        {
            RevertTaskDuration(RELAX_IN_TOWN_PACKAGE);
        }
    }
    
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
                if (aiPackageName == "Shop-24hr" && platoon->getSquadLeader() != character)
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



    void (*ForgottenGUI_changeFontSize_orig)();
    void ForgottenGUI_changeFontSize_hook()
    {
        ForgottenGUI_changeFontSize_orig();
        if (ModOptions::initialized)
            ModOptions::getSingletonPtr()->create();
        if (ModOptions::initialized)
            ModOptions::getSingletonPtr()->create();

    }

    void ShowModOptions(MyGUI::Widget* sender)
    {
        auto modOptions = ModOptions::getSingletonPtr();
        if (modOptions && modOptions->initialized)
        {
            if (modOptions->isVisible())
            {
                modOptions->hide();
            }
            else
            {
                modOptions->show();
            }
        }
    }

    void (*OptionsWindow_create_orig)(OptionsWindow* thisptr);
    void OptionsWindow_create_hook(OptionsWindow* thisptr)
    {
        OptionsWindow_create_orig(thisptr);
        ModOptions* modOptions = ModOptions::getSingletonPtr();
        if (!modOptions) return;
        //DebugLog("mod options initialized!");
        auto tabCount = thisptr->tabs->getItemCount();
        std::vector<int> catList(tabCount);
        int maxCat = 0;
        int cat = 0x0;
        DataPanelLine* modLine = nullptr;
        DatapanelGUI* settingsPanel = nullptr;
        for (size_t i = 0; i < tabCount; i++)
        {
            auto panel = *(thisptr->tabs->getItemDataAt<DatapanelGUI*>(i, false));
            //From KEP: mod category = 0x0
            if (panel && panel->getCurrentCategory() == cat)
            {
                //cat = panel->getCurrentCategory();
                settingsPanel = panel;
                break;
            }
        }
        if (settingsPanel)
        {
            //DebugLog(Ogre::StringConverter::toString(settingsPanel->getNumLines(cat)));
            std::string identifier = "MoreImmersiveBars";
            for (int i = 0; i < settingsPanel->getNumLines(cat); ++i)
            {
                auto line = settingsPanel->getLineByNum(cat, i);
                if (line)
                {
                    std::string key = line->keyValue;
                    int slash = -1;
                    std::string modName = "";
                    key.erase(0, key.find_first_not_of(" \t"));
                    slash = key.find('-');
                    if (slash == std::string::npos)
                    {
                        continue;
                    }

                    modName = key.substr(slash + 1);
                    modName.erase(0, modName.find_first_not_of(" \t"));
                    //DebugLog(modName);
                    if (modName == identifier)
                    {
                        modLine = line;
                        break;
                    }
                }
            }
            if (modLine)
            {
                modOptions->setOptionsWindow(thisptr);
                //DebugLog(Ogre::StringConverter::toString(modLine->getNumWidgets()));
                //DebugLog(modLine->w1->getCaption().asUTF8());
                //DebugLog(modLine->w2->getCaption().asUTF8());
                float right = static_cast<float>(modLine->w2->getRight() - modLine->w2->getTextSize().width) / settingsPanel->getWidget()->getSize().width;
                float left = right - 0.1;
                float top = static_cast<float>(modLine->w2->getTop()) / settingsPanel->getWidget()->getSize().height;
                float height = static_cast<float>(modLine->w2->getHeight()) / settingsPanel->getWidget()->getSize().height;
                //DebugLog("left: " + Ogre::StringConverter::toString(left) + " top: " + Ogre::StringConverter::toString(top) + " height " + Ogre::StringConverter::toString(height));
                auto btn = settingsPanel->getWidget()->createWidgetReal<MyGUI::Button>("Kenshi_Button1", left, top, 0.1, height, MyGUI::Align::Top | MyGUI::Align::Left, "SquadAutonomySettingsBtn");
                btn->setCaption("Settings");
                btn->eventMouseButtonClick += MyGUI::newDelegate(ShowModOptions);
            }
        }
    }

    std::string GetCurrentDLLDirectory() {
        char path[MAX_PATH];
        HMODULE hModule = NULL;

        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&GetCurrentDLLDirectory, &hModule)) {

            GetModuleFileNameA(hModule, path, MAX_PATH);
            std::string fullPath(path);

            size_t lastSlash = fullPath.find_last_of("\\/");
            if (std::string::npos != lastSlash) {
                return fullPath.substr(0, lastSlash + 1);
            }
            return fullPath;
        }
        return "";
    }

    void init()
    {
        cfgPath = GetCurrentDLLDirectory() + "MoreImmersiveBars.cfg";
    }

    void (*saveOptions_orig)(OptionsWindow* thisptr);
    void saveOptions_hook(OptionsWindow* thisptr)
    {
        saveOptions_orig(thisptr);
        ModOptions::getSingletonPtr()->saveOptionsSettings();
    }

    bool (*checkTags_orig)(DialogLineData* thisptr, Character* me, Character* target);
    bool checkTags_hook(DialogLineData* thisptr, Character* me, Character* target)
    {
        Blackboard* bb = nullptr;
        if (target) bb = me->getBlackboard();
        if (noSleepTalk && bb)
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

    bool (*initialisation_orig)(GameWorld* thisptr);
    bool initialisation_hook(GameWorld* thisptr)
    {
        bool result = initialisation_orig(thisptr);
        if (result)
        {
            //taskData are initialised here
            auto relaxData = getTaskDataConst(RELAX_IN_TOWN_PACKAGE);
            OriginalTaskDataDuration* relaxOrig = new OriginalTaskDataDuration(relaxData->durationMin, relaxData->durationFuzz, relaxData->isDurationBased, relaxData->endsAfterTime);
            /*DebugLog("relax Task : " + Ogre::StringConverter::toString(relaxData->durationMin) + ", " + Ogre::StringConverter::toString(relaxData->durationFuzz)
                + ", " + Ogre::StringConverter::toString(relaxData->isDurationBased) + ", " + Ogre::StringConverter::toString(relaxData->endsAfterTime));*/
            taskTypeOrigDataDuration[RELAX_IN_TOWN_PACKAGE] = relaxOrig;
        }
        return result;
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
            *(uintptr_t*)&MoreImmersiveBars::rentedBeds = baseAddr + 0x212db18;
            *(uintptr_t*)&MoreImmersiveBars::_MainColorCode = baseAddr + 0x01f48238;
            *(uintptr_t*)&MoreImmersiveBars::getTaskDataConst = baseAddr + 0x283F40;
            *(uintptr_t*)&MoreImmersiveBars::taskTypetaskData = baseAddr + 0x1ce80f0;

        }
        else if (platform == 0)
        {
            *(uintptr_t*)&MoreImmersiveBars::rentedBeds = baseAddr + 0x212BA58;
            *(uintptr_t*)&MoreImmersiveBars::_MainColorCode = baseAddr + 0x01f46248;
            *(uintptr_t*)&MoreImmersiveBars::getTaskDataConst = baseAddr + 0x283AD0;
            *(uintptr_t*)&MoreImmersiveBars::taskTypetaskData = baseAddr + 0x1ce60F0;

        }
    }
    MoreImmersiveBars::init();
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&ForgottenGUI::changeFontSize), &MoreImmersiveBars::ForgottenGUI_changeFontSize_hook, &MoreImmersiveBars::ForgottenGUI_changeFontSize_orig))
        ErrorLog("Could not add ForgottenGUI::changeFontSize hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&UseableStuff::_NV_getCostToUse), &MoreImmersiveBars::_NV_getCostToUse_hook, &MoreImmersiveBars::_NV_getCostToUse_orig))
        ErrorLog("Could not add UseableStuff::_NV_getCostToUse hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::periodicUpdate), &MoreImmersiveBars::periodicUpdate_hook, &MoreImmersiveBars::periodicUpdate_orig))
        ErrorLog("Could not add AITaskSytem::update4Frame hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::update4Frame), &MoreImmersiveBars::update4Frame_hook, &MoreImmersiveBars::update4Frame_orig))
        ErrorLog("Could not add AITaskSytem::update4Frame hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&GameWorld::initialisation), &MoreImmersiveBars::initialisation_hook, &MoreImmersiveBars::initialisation_orig))
        ErrorLog("Could not add GameWorld::initialisation constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::AIResultsCacher::runTargetFinder), &MoreImmersiveBars::runTargetFinder_hook, &MoreImmersiveBars::runTargetFinder_orig))
        ErrorLog("Could not add AI::AIResultsCacher::runTargetFinder hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&Tasker::score), &MoreImmersiveBars::score_hook, &MoreImmersiveBars::score_orig))
        ErrorLog("Could not add Tasker::score hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&OptionsWindow::create), &MoreImmersiveBars::OptionsWindow_create_hook, &MoreImmersiveBars::OptionsWindow_create_orig))
        ErrorLog("Could not add OptionsWindow::create constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&OptionsWindow::saveOptions), &MoreImmersiveBars::saveOptions_hook, &MoreImmersiveBars::saveOptions_orig))
        ErrorLog("Could not add OptionsWindow::saveOptions constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&DialogLineData::checkTags), &MoreImmersiveBars::checkTags_hook, &MoreImmersiveBars::checkTags_orig))
        ErrorLog("Could not add DialogLineData::checkTags hook!");
    KenshiLib::ApplyQueuedHooks();
    DebugLog("Mod started");
}



