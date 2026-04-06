#include "UniPlanRuntime.h"

#include <exception>
#include <iostream>

int main(int InArgc, char* InArgv[])
{
    try
    {
        return UniPlan::RunMain(InArgc, InArgv);
    }
    catch (const std::exception& InException)
    {
        std::cerr << "[Error] " << InException.what() << "\n";
        return 1;
    }
}
