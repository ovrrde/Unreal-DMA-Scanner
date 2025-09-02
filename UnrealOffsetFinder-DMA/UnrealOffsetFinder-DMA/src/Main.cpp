#include "Application.h"
#include <iostream>
#include <exception>

int main(int argc, char* argv[])
{
    try 
    {
        // fuckin go time baby. run the 1s.
        Application app;
        return app.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
    catch (...)
    {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return -2;
    }
} 