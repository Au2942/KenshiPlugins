#pragma once
#include <cstdint>
#include <string>

namespace SquadAutonomy
{
	class Logger
	{
	public:
		enum Severity
		{
			None,
			Error,
			Warning,
			Info,
			Debug
		};
		static const Logger::Severity severities[5];

		enum Category : uint32_t
		{
			Any = 0,
			AI_SCORING = 1 << 0,
			AI_TARGETFINDING = 1 << 1,
			LABOUR = 1 << 2,
			GUARD = 1 << 3,
			SHOPPING = 1 << 4,
		};
		static const Logger::Category categories[6];

		static bool log(const std::string& log, Severity severity = Debug, bool spam = false);
		static bool logDebug(const std::string& log, Category category = Any, bool spam = false);
		static bool logDebug(const std::string& log, uint32_t categories = 0, bool spam = false);
	private:
	};
}