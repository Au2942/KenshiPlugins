#pragma once
#include <string>
#include <codecvt>

namespace MoreImmersiveBars
{
	extern std::wstring cfgPath;
	extern std::wstring modPath;
	extern bool noSleepTalk;
	extern std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
}