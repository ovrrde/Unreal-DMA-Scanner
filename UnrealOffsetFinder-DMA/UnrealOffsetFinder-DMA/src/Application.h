#pragma once

#include <SFML/Graphics.hpp>
#include <memory>

// Forward declarations
class UIManager;
class DMAManager;

/**
 * @class Application
 * @brief Main application class that manages the entire program lifecycle
 * 
 * This class handles window management, main loop execution, and coordinates
 * between different subsystems like UI and DMA operations.
 */
class Application
{
public:
    /**
     * @brief Constructor - initializes the application
     */
    Application();

    /**
     * @brief Destructor - cleans up resources
     */
    ~Application();

    /**
     * @brief Main application run loop
     * @return Exit code (0 for success)
     */
    int Run();

private:
    /**
     * @brief Initialize all application subsystems
     * @return true if initialization successful, false otherwise
     */
    bool Initialize();

    /**
     * @brief Handle SFML events (window close, input, etc.)
     */
    void HandleEvents();

    /**
     * @brief Update application state
     * @param deltaTime Time since last update in seconds
     */
    void Update(float deltaTime);

    /**
     * @brief Render the application
     */
    void Render();

    /**
     * @brief Clean up all resources
     */
    void Shutdown();

private:
    // Core SFML components
    std::unique_ptr<sf::RenderWindow> m_window;     ///< Main application window
    sf::Clock m_clock;                              ///< Clock for delta time calculation
    
    // Application subsystems
    std::unique_ptr<UIManager> m_uiManager;         ///< UI management system
    std::unique_ptr<DMAManager> m_dmaManager;       ///< DMA operations manager
    
    // Application state
    bool m_isRunning;                               ///< Application running state
    
    // Window settings
    static constexpr unsigned int WINDOW_WIDTH = 1200;   ///< Default window width
    static constexpr unsigned int WINDOW_HEIGHT = 800;   ///< Default window height
    static constexpr const char* WINDOW_TITLE = "Unreal Offset Finder - DMA"; ///< Window title
}; 