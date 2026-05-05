#pragma once

#include <cstdio>
#include <string>

namespace UniPlan
{

inline FILE *OpenReadPipe(const std::string &InCommand)
{
#ifdef _WIN32
    return _popen(InCommand.c_str(), "r");
#else
    return popen(InCommand.c_str(), "r");
#endif
}

inline int CloseReadPipe(FILE *rpPipe)
{
#ifdef _WIN32
    return _pclose(rpPipe);
#else
    return pclose(rpPipe);
#endif
}

inline const char *ShellStderrToNull()
{
#ifdef _WIN32
    return "2>nul";
#else
    return "2>/dev/null";
#endif
}

} // namespace UniPlan
