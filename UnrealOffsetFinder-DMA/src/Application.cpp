#include "Application.h"
#include "UI/UIManager.h"
#include "DMA/DMAManager.h"
#include <imgui-SFML.h>
#include <imgui.h>
#include <iostream>

Application::Application()
    : m_isRunning(false)
{
    // Constructor - initialization happens in Initialize()
}

Application::~Application()
{
    Shutdown();
}

int Application::Run()
{
    if (!Initialize())
    {
        std::cerr << "Failed to initialize application" << std::endl;
        return -1;
    }

    m_isRunning = true;
    
    // Main application loop
    while (m_isRunning && m_window->isOpen())
    {
        // Calculate delta time
        sf::Time deltaTime = m_clock.restart();
        float dt = deltaTime.asSeconds();

        // Process events
        HandleEvents();

        // Update application state
        Update(dt);

        // Render everything
        Render();
    }

    return 0;
}

bool Application::Initialize()
{
    try
    {
        // Create the main window
        m_window = std::make_unique<sf::RenderWindow>(
            sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT),
            WINDOW_TITLE,
            sf::Style::Default
        );

        if (!m_window)
        {
            std::cerr << "Failed to create SFML window" << std::endl;
            return false;
        }

        // Set window properties
        m_window->setFramerateLimit(60);
        m_window->setVerticalSyncEnabled(true);

        // Initialize ImGui-SFML
        if (!ImGui::SFML::Init(*m_window))
        {
            std::cerr << "Failed to initialize ImGui-SFML" << std::endl;
            return false;
        }

        // Configure ImGui style
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 5.0f;
        style.FrameRounding = 3.0f;
        style.ScrollbarRounding = 3.0f;
        style.GrabRounding = 3.0f;

        // Initialize subsystems
        m_uiManager = std::make_unique<UIManager>();
        m_dmaManager = std::make_unique<DMAManager>();

        if (!m_uiManager->Initialize())
        {
            std::cerr << "Failed to initialize UI Manager" << std::endl;
            return false;
        }

        if (!m_dmaManager->Initialize())
        {
            std::cerr << "Failed to initialize DMA Manager" << std::endl;
            return false;
        }

        // Connect UI Manager with DMA Manager
        m_uiManager->SetDMAManager(m_dmaManager.get());

        std::cout << "Application initialized successfully!" << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception during initialization: " << e.what() << std::endl;
        return false;
    }
}

void Application::HandleEvents()
{
    sf::Event event;
    while (m_window->pollEvent(event))
    {
        // Let ImGui handle the event first
        ImGui::SFML::ProcessEvent(event);

        switch (event.type)
        {
        case sf::Event::Closed:
            m_isRunning = false;
            break;

        case sf::Event::KeyPressed:
            if (event.key.code == sf::Keyboard::Escape)
            {
                m_isRunning = false;
            }
            break;

        default:
            break;
        }
    }
}

void Application::Update(float deltaTime)
{
    // Update ImGui-SFML
    ImGui::SFML::Update(*m_window, sf::seconds(deltaTime));

    // Update subsystems
    if (m_uiManager)
    {
        m_uiManager->Update(deltaTime);
    }

    if (m_dmaManager)
    {
        m_dmaManager->Update(deltaTime);
    }
}

void Application::Render()
{
    // Clear the window
    m_window->clear(sf::Color(40, 42, 54)); // Dark background

    // Render UI
    if (m_uiManager)
    {
        m_uiManager->Render();
    }

    // Render ImGui
    ImGui::SFML::Render(*m_window);

    // Display everything
    m_window->display();
}

void Application::Shutdown()
{
    // Shutdown subsystems in reverse order
    if (m_dmaManager)
    {
        m_dmaManager->Shutdown();
        m_dmaManager.reset();
    }

    if (m_uiManager)
    {
        m_uiManager->Shutdown();
        m_uiManager.reset();
    }

    // Shutdown ImGui-SFML
    ImGui::SFML::Shutdown();

    // Close window
    if (m_window && m_window->isOpen())
    {
        m_window->close();
    }

    std::cout << "Application shutdown complete." << std::endl;
} 