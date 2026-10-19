#include "SquadAutonomyLog.h"
#include "SquadAutonomyModSettings.h"
#include "SquadAutonomyLocalization.h"
#include <Debug.h>


namespace SquadAutonomy
{
	const Logger::Severity Logger::severities[5] =
	{
		Logger::None,
		Logger::Error,
		Logger::Warning,
		Logger::Info,
		Logger::Debug
	};
	const Logger::Category Logger::categories[6] =
	{
		Logger::Any,
		Logger::AI_SCORING,
		Logger::AI_TARGETFINDING,
		Logger::LABOUR,
		Logger::GUARD,
		Logger::SHOPPING
	};

	bool Logger::log(const std::string& log, Severity severity, bool spam)
	{
		ModSettings::SettingsValues settings = ModSettings::getSingleton().getValues();
		if (!settings.enableLogging) return false;
		if (spam && !settings.logSpam) return false;
		if (severity > settings.logSeverity) return false;
		std::string prefix = "";
		switch (severity)
		{
		case Info:
			prefix = "INF: ";
			break;
		case Warning:
			prefix = "WRN: ";
			break;
		case Error:
			prefix = "ERR: ";
			break;
		case Debug:
			prefix = "DBG: ";
			break;
		default:
			break;
		}
		DebugLog(prefix + log);
		return true;
	}
	bool Logger::logDebug(const std::string& log, Category category, bool spam)
	{
		ModSettings::SettingsValues settings = ModSettings::getSingleton().getValues();
		if (category != Any && category != settings.logCategory) return false;
		return Logger::log(log, Logger::Debug, spam);
	}
	bool Logger::logDebug(const std::string& log, uint32_t categories, bool spam)
	{
		ModSettings::SettingsValues settings = ModSettings::getSingleton().getValues();
		if (categories != 0 && !(settings.logCategory & categories)) return false;
		return Logger::log(log, Logger::Debug, spam);
	}
}

