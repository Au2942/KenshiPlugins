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
			Info,
			Warning,
			Error,
			Debug
		};
		static bool log(std::string log, Severity severity = None, bool spam = false);
	private:
	};
}