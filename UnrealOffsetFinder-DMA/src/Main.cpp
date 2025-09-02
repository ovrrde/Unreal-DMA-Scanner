#include "Application.h"
#include <exception>
#include <iostream>

namespace ExitCode
{
    constexpr int SUCCESS = 0;
    constexpr int RUNTIME_ERROR = -1;
    constexpr int UNKNOWN_ERROR = -2;
}

/**
 * @brief Application entry point
 * @param argc Command line argument count
 * @param argv Command line argument values
 * @return Exit code (0 for success, negative for errors)
 */
int main(int argc, char* argv[])
{
    try 
    {
        Application app;
        return app.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return ExitCode::RUNTIME_ERROR;
    }
    catch (...)
    {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return ExitCode::UNKNOWN_ERROR;
    }
} 