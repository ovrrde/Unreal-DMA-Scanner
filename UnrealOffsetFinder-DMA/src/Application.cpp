#include "Application.h"
#include "UI/UIManager.h"
#include "DMA/DMAManager.h"
#include <imgui-SFML.h>
#include <imgui.h>
#include <fstream>
#include <iostream>

Application::Application()
    : m_isRunning(false)
{
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
    
    while (m_isRunning && m_window->isOpen())
    {
        sf::Time deltaTime = m_clock.restart();
        float dt = deltaTime.asSeconds();

        HandleEvents();

        Update(dt);

        Render();
    }

    return 0;
}

bool Application::Initialize()
{
    try
    {
        m_window = std::make_unique<sf::RenderWindow>(
            sf::VideoMode(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT),
            WINDOW_TITLE,
            sf::Style::Default
        );

        if (!m_window)
        {
            std::cerr << "Failed to create SFML window" << std::endl;
            return false;
        }

        m_window->setFramerateLimit(TARGET_FPS);
        m_window->setVerticalSyncEnabled(true);

        if (!ImGui::SFML::Init(*m_window))
        {
            std::cerr << "Failed to initialize ImGui-SFML" << std::endl;
            return false;
        }

        InitializeFonts();

        InitializeStyle();

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
    ImGui::SFML::Update(*m_window, sf::seconds(deltaTime));

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
    m_window->clear(sf::Color(28, 38, 43)); 

    if (m_uiManager)
    {
        m_uiManager->Render();
    }

    ImGui::SFML::Render(*m_window);

    m_window->display();
}

void Application::Shutdown()
{
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

    ImGui::SFML::Shutdown();

    if (m_window && m_window->isOpen())
    {
        m_window->close();
    }

    std::cout << "Application shutdown complete." << std::endl;
}

void Application::InitializeStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.90f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.90f, 0.35f);
    
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);

    style.WindowPadding                     = ImVec2(8.00f, 8.00f);
    style.FramePadding                      = ImVec2(5.00f, 2.00f);
    style.CellPadding                       = ImVec2(6.00f, 6.00f);
    style.ItemSpacing                       = ImVec2(6.00f, 6.00f);
    style.ItemInnerSpacing                  = ImVec2(6.00f, 6.00f);
    style.TouchExtraPadding                 = ImVec2(0.00f, 0.00f);
    style.IndentSpacing                     = 25;
    style.ScrollbarSize                     = 15;
    style.GrabMinSize                       = 10;
    style.WindowBorderSize                  = 1;
    style.ChildBorderSize                   = 1;
    style.PopupBorderSize                   = 1;
    style.FrameBorderSize                   = 1;
    style.TabBorderSize                     = 1;
    style.WindowRounding                    = 7;
    style.ChildRounding                     = 4;
    style.FrameRounding                     = 3;
    style.PopupRounding                     = 4;
    style.ScrollbarRounding                 = 9;
    style.GrabRounding                      = 3;
    style.LogSliderDeadzone                 = 4;
    style.TabRounding                       = 4;
}

void Application::InitializeFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    
    io.Fonts->Clear();
    
    ImFont* defaultFont = nullptr;
    
    const char* fontPaths[] = {
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\calibri.ttf"
    };
    
    bool fontLoaded = false;
    for (const char* fontPath : fontPaths)
    {
        std::ifstream fontFile(fontPath);
        if (fontFile.good())
        {
            fontFile.close();
            
            defaultFont = io.Fonts->AddFontFromFileTTF(fontPath, 16.0f);
            if (defaultFont != nullptr)
            {
                io.Fonts->AddFontFromFileTTF(fontPath, 18.0f);
                fontLoaded = true;
                std::cout << "Loaded font: " << fontPath << std::endl;
                break;
            }
        }
    }
    
    if (!fontLoaded)
    {
        ImFontConfig fontConfig;
        fontConfig.SizePixels = 16.0f;
        defaultFont = io.Fonts->AddFontDefault(&fontConfig);
        std::cout << "Using default ImGui font" << std::endl;
    }
    
    io.Fonts->Build();
    
    ImGui::SFML::UpdateFontTexture();
} 