#pragma once
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
		static bool log(const std::string& log, Severity severity = Debug, bool spam = false);
	private:
	};
}