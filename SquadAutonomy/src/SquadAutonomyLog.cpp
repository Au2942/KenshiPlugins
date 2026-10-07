#include "SquadAutonomyLog.h"
#include "SquadAutonomyModSettings.h"
#include "SquadAutonomyLocalization.h"
#include <Debug.h>


namespace SquadAutonomy
{
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
			prefix = Localization::gettext("INF:") + ' ';
			break;
		case Warning:
			prefix = Localization::gettext("WRN:") + ' ';
			break;
		case Error:
			prefix = Localization::gettext("ERR:") + ' ';
			break;
		case Debug:
			prefix = Localization::gettext("DBG:") + ' ';
			break;
		default:
			break;
		}
		DebugLog(prefix + log);
		return true;
	}
}

