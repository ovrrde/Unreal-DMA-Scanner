#include "UIManager.h"
#include "../DMA/DMAManager.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

UIManager::UIManager()
    : m_showControlPanel(true)
    , m_showOffsetFinder(true)
    , m_showMemoryViewer(false)
    , m_showProcessSelector(false)
    , m_showStatusWindow(true)
    , m_showAboutDialog(false)
    , m_selectedProcess("")
    , m_dmaManager(nullptr)
    , m_progressSpinner(0.0f)
{
    memset(m_processNameBuffer, 0, sizeof(m_processNameBuffer));
    memset(m_offsetNameBuffer, 0, sizeof(m_offsetNameBuffer));
    memset(m_offsetAddressBuffer, 0, sizeof(m_offsetAddressBuffer));
}

UIManager::~UIManager()
{
    Shutdown();
}

bool UIManager::Initialize()
{
    m_logMessages.push_back("[INFO] UI Manager initialized successfully");
    
    std::cout << "UI Manager initialized" << std::endl;
    return true;
}

void UIManager::Update(float deltaTime)
{
    m_progressSpinner += deltaTime * 6.0f; // Smooth spin speed
    if (m_progressSpinner > 2.0f * 3.14159f)
        m_progressSpinner -= 2.0f * 3.14159f;
}

void UIManager::Render()
{
    ImGuiIO& io = ImGui::GetIO();
    float windowWidth = io.DisplaySize.x;
    float windowHeight = io.DisplaySize.y;
    
    float menuBarHeight = 20.0f;
    float leftPanelWidth = windowWidth * 0.3f;
    float rightPanelWidth = windowWidth * 0.7f;
    float bottomPanelHeight = windowHeight * 0.25f;
    float topPanelHeight = windowHeight - menuBarHeight - bottomPanelHeight;
    
    RenderMenuBar();
    
    
    RenderControlPanel();
    RenderOffsetFinder();
    RenderStatusWindow();
    
    if (m_showMemoryViewer)
        RenderMemoryViewer();
        
    if (m_showProcessSelector)
        RenderProcessSelector();
        
    if (m_showAboutDialog)
        RenderAboutDialog();
}

void UIManager::Shutdown()
{
    m_offsetEntries.clear();
    m_logMessages.clear();
    m_dmaManager = nullptr;
    
    std::cout << "UI Manager shutdown complete" << std::endl;
}

void UIManager::SetDMAManager(DMAManager* dmaManager)
{
    m_dmaManager = dmaManager;
}

void UIManager::RenderMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Load Offsets", "Ctrl+O"))
            {
                if (LoadOffsets())
                {
                            }
            }
            
            if (ImGui::MenuItem("Save Offsets", "Ctrl+S"))
            {
                if (SaveOffsets())
                {
                    }
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Save As..."))
            {
                    std::string filename = "offsets_" + std::to_string(time(nullptr)) + ".txt";
                if (SaveOffsets(filename))
                {
                    }
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Exit", "Alt+F4"))
            {
                    m_logMessages.push_back("[INFO] Exit requested");
            }
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View"))
        {
                ImGui::MenuItem("Control Panel", nullptr, nullptr, false);
            ImGui::MenuItem("Offset Finder", nullptr, nullptr, false);
            ImGui::MenuItem("Status Window", nullptr, nullptr, false);
            
            ImGui::Separator();
            
                ImGui::MenuItem("Memory Viewer", nullptr, &m_showMemoryViewer);
            ImGui::MenuItem("Process Selector", nullptr, &m_showProcessSelector);
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::MenuItem("Scan Unreal Globals"))
            {
                if (m_dmaManager && m_dmaManager->IsConnected())
                {
                    if (!m_dmaManager->HasPendingOperations())
                    {
                        m_logMessages.push_back("[INFO] Starting async Unreal Engine globals scan from menu...");
                        
                        m_dmaManager->ScanUnrealGlobalsAsync(
                            [this](const AsyncResult<UnrealGlobals>& result) {
                                if (result.isSuccess)
                                {
                                    m_logMessages.push_back("[SUCCESS] " + result.logMessage);
                                    
                                    const auto& globals = result.result;
                                    
                                    uint64_t mainBase = m_dmaManager->GetMainModuleBase();
                                    
                                    if (globals.GWorld != 0)
                                    {
                                        uint64_t offset = globals.GWorld - mainBase;
                                        std::string offsetStr = m_dmaManager->FormatHexAddress(offset);
                                        std::string absoluteStr = m_dmaManager->FormatHexAddress(globals.GWorld);
                                        m_offsetEntries.emplace_back("GWorld", offsetStr, absoluteStr);
                                        m_logMessages.push_back("[INFO] GWorld offset: " + offsetStr + " (absolute: " + absoluteStr + ")");
                                    }
                                    if (globals.GNames != 0)
                                    {
                                        uint64_t offset = globals.GNames - mainBase;
                                        std::string offsetStr = m_dmaManager->FormatHexAddress(offset);
                                        std::string absoluteStr = m_dmaManager->FormatHexAddress(globals.GNames);
                                        m_offsetEntries.emplace_back("GNames", offsetStr, absoluteStr);
                                        m_logMessages.push_back("[INFO] GNames offset: " + offsetStr + " (absolute: " + absoluteStr + ")");
                                    }
                                    if (globals.GObjects != 0)
                                    {
                                        uint64_t offset = globals.GObjects - mainBase;
                                        std::string offsetStr = m_dmaManager->FormatHexAddress(offset);
                                        std::string absoluteStr = m_dmaManager->FormatHexAddress(globals.GObjects);
                                        m_offsetEntries.emplace_back("GObjects", offsetStr, absoluteStr);
                                        m_logMessages.push_back("[INFO] GObjects offset: " + offsetStr + " (absolute: " + absoluteStr + ")");
                                    }
                                }
                                else
                                {
                                    m_logMessages.push_back("[ERROR] " + result.errorMessage);
                                }
                            });
                    }
                    else
                    {
                        m_logMessages.push_back("[INFO] Please wait for current operations to complete");
                    }
                }
                else
                {
                    m_logMessages.push_back("[ERROR] Not connected to any process");
                }
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Scan Memory"))
            {
                m_logMessages.push_back("[INFO] Memory scan functionality not yet implemented");
            }
            
            if (ImGui::MenuItem("Refresh Process List"))
            {
                m_logMessages.push_back("[INFO] Refreshing process list...");
                if (m_dmaManager)
                {
                    m_logMessages.push_back("[INFO] Process list will be refreshed automatically");
                }
                else
                {
                    m_logMessages.push_back("[ERROR] DMA Manager not available");
                }
            }
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About"))
            {
                m_showAboutDialog = true;
            }
            
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
}

void UIManager::RenderControlPanel()
{
    ImGuiIO& io = ImGui::GetIO();
    float windowWidth = io.DisplaySize.x;
    float windowHeight = io.DisplaySize.y;
    float menuBarHeight = 20.0f;
    float leftPanelWidth = windowWidth * 0.3f;
    float topPanelHeight = windowHeight - menuBarHeight - (windowHeight * 0.25f);
    
    ImGui::SetNextWindowPos(ImVec2(0, menuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, topPanelHeight));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
    
    if (ImGui::Begin("Control Panel", nullptr, flags))
    {
        DrawSectionHeader("Process Selection");
        
        float availableWidth = ImGui::GetContentRegionAvail().x;
        float browseButtonWidth = 80.0f;
        float inputWidth = availableWidth - browseButtonWidth - ImGui::GetStyle().ItemSpacing.x;
        
        ImGui::PushItemWidth(inputWidth);
        ImGui::InputText("##ProcessName", m_processNameBuffer, sizeof(m_processNameBuffer));
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        if (DrawButton("Browse", ImVec2(browseButtonWidth, 0)))
        {
            m_showProcessSelector = true;
        }
        
        ImGui::Text("Process Name");
        
        ImGui::Spacing();
        
        if (DrawButton("Attach to Process", ImVec2(-1, 0)))
        {
            std::string processName(m_processNameBuffer);
            if (!processName.empty())
            {
                m_selectedProcess = processName;
                m_logMessages.push_back("[INFO] Attempting to attach to process: " + processName);
                
                if (m_dmaManager)
                {
                    m_dmaManager->AttachToProcessAsync(processName, 
                        [this, processName](const AsyncResult<bool>& result) {
                            if (result.isSuccess)
                            {
                                m_logMessages.push_back("[SUCCESS] " + result.logMessage);
                                m_selectedProcess = processName;
                            }
                            else
                            {
                                m_logMessages.push_back("[ERROR] " + result.errorMessage);
                                m_selectedProcess = "";
                            }
                        });
                }
            }
            else
            {
                m_logMessages.push_back("[ERROR] Please enter a process name");
            }
        }
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        DrawSectionHeader("Process Status");
        
        bool isConnected = m_dmaManager && m_dmaManager->IsConnected();
        
        if (isConnected)
        {
            auto processInfo = m_dmaManager->GetCurrentProcessInfo();
            uint64_t mainBase = m_dmaManager->GetMainModuleBase();
            
            DrawStatusIndicator("Connected", true);
            
            ImGui::Spacing();
            
            ImGui::Text("Process:");
            ImGui::SameLine(80);
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s", processInfo.processName.c_str());
            
            ImGui::Text("PID:");
            ImGui::SameLine(80);
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%u", processInfo.processId);
            
            if (mainBase != 0)
            {
                std::string baseStr = m_dmaManager->FormatHexAddress(mainBase);
                ImGui::Text("Base:");
                ImGui::SameLine(80);
                ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s", baseStr.c_str());
            }
            
            if (processInfo.imageSize != 0)
            {
                std::string sizeStr = m_dmaManager->FormatHexAddress(processInfo.imageSize);
                ImGui::Text("Size:");
                ImGui::SameLine(80);
                ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s", sizeStr.c_str());
            }
        }
        else
        {
            DrawStatusIndicator("Not Connected", false);
            
            if (!m_selectedProcess.empty())
            {
                ImGui::Spacing();
                ImGui::Text("Target:");
                ImGui::SameLine(80);
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", m_selectedProcess.c_str());
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(not connected)");
            }
        }
        
        if (m_dmaManager && m_dmaManager->HasPendingOperations())
        {
            size_t pending = m_dmaManager->GetPendingOperationCount();
            
            DrawSpinner(SPINNER_RADIUS, 2.5f);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.28f, 0.56f, 1.00f, 1.0f), "Operations pending: %zu", pending);
        }
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        DrawSectionHeader("Quick Actions");
        
        bool hasOperations = m_dmaManager && m_dmaManager->HasPendingOperations();
        
        if (DrawButton("Scan UE Globals", ImVec2(-1, 0), !hasOperations))
        {
            if (m_dmaManager && m_dmaManager->IsConnected())
            {
                if (!hasOperations)
                {
                    m_logMessages.push_back("[INFO] Starting async Unreal Engine globals scan...");
                    
                    m_dmaManager->ScanUnrealGlobalsAsync(
                        [this](const AsyncResult<UnrealGlobals>& result) {
                            if (result.isSuccess)
                            {
                                m_logMessages.push_back("[SUCCESS] " + result.logMessage);
                                
                                const auto& globals = result.result;
                                uint64_t mainBase = m_dmaManager->GetMainModuleBase();
                                
                                if (globals.GWorld != 0)
                                {
                                    uint64_t offset = globals.GWorld - mainBase;
                                    std::string offsetStr = m_dmaManager->FormatHexAddress(offset);
                                    std::string absoluteStr = m_dmaManager->FormatHexAddress(globals.GWorld);
                                    m_offsetEntries.emplace_back("GWorld", offsetStr, absoluteStr);
                                    m_logMessages.push_back("[INFO] GWorld offset: " + offsetStr + " (absolute: " + absoluteStr + ")");
                                }
                                if (globals.GNames != 0)
                                {
                                    uint64_t offset = globals.GNames - mainBase;
                                    std::string offsetStr = m_dmaManager->FormatHexAddress(offset);
                                    std::string absoluteStr = m_dmaManager->FormatHexAddress(globals.GNames);
                                    m_offsetEntries.emplace_back("GNames", offsetStr, absoluteStr);
                                    m_logMessages.push_back("[INFO] GNames offset: " + offsetStr + " (absolute: " + absoluteStr + ")");
                                }
                                if (globals.GObjects != 0)
                                {
                                    uint64_t offset = globals.GObjects - mainBase;
                                    std::string offsetStr = m_dmaManager->FormatHexAddress(offset);
                                    std::string absoluteStr = m_dmaManager->FormatHexAddress(globals.GObjects);
                                    m_offsetEntries.emplace_back("GObjects", offsetStr, absoluteStr);
                                    m_logMessages.push_back("[INFO] GObjects offset: " + offsetStr + " (absolute: " + absoluteStr + ")");
                                }
                            }
                            else
                            {
                                m_logMessages.push_back("[ERROR] " + result.errorMessage);
                            }
                        });
                }
                else
                {
                    m_logMessages.push_back("[INFO] Please wait for current operations to complete");
                }
            }
            else
            {
                m_logMessages.push_back("[ERROR] Not connected to any process");
            }
        }
        
        if (DrawButton("Get Module Base", ImVec2(-1, 0), !hasOperations))
        {
            if (m_dmaManager && m_dmaManager->IsConnected())
            {
                if (!hasOperations)
                {
                    m_logMessages.push_back("[INFO] Getting main module base address...");
                    
                    m_dmaManager->GetMainModuleBaseAsync(
                        [this](const AsyncResult<uint64_t>& result) {
                            if (result.isSuccess)
                            {
                                m_logMessages.push_back("[SUCCESS] " + result.logMessage);
                                std::string absoluteStr = m_dmaManager->FormatHexAddress(result.result);
                                m_offsetEntries.emplace_back("Main Module Base", "0x0", absoluteStr);
                                m_logMessages.push_back("[INFO] Main module base: " + absoluteStr + " (offset: 0x0)");
                            }
                            else
                            {
                                m_logMessages.push_back("[ERROR] " + result.errorMessage);
                            }
                        });
                }
                else
                {
                    m_logMessages.push_back("[INFO] Please wait for current operations to complete");
                }
            }
            else
            {
                m_logMessages.push_back("[ERROR] Not connected to any process");
            }
        }
        
        if (DrawButton("Clear All Offsets", ImVec2(-1, 0)))
        {
            m_offsetEntries.clear();
            m_logMessages.push_back("[INFO] Cleared all offset entries");
        }
        
        if (hasOperations)
        {
            if (DrawButton("Cancel Operations", ImVec2(-1, 0)))
            {
                m_dmaManager->CancelAllOperations();
                m_logMessages.push_back("[INFO] Cancelled all pending operations");
            }
        }
    }
    ImGui::End();
}

void UIManager::RenderOffsetFinder()
{
    ImGuiIO& io = ImGui::GetIO();
    float windowWidth = io.DisplaySize.x;
    float windowHeight = io.DisplaySize.y;
    float menuBarHeight = 20.0f;
    float leftPanelWidth = windowWidth * 0.3f;
    float rightPanelWidth = windowWidth * 0.7f;
    float topPanelHeight = windowHeight - menuBarHeight - (windowHeight * 0.25f);
    
    ImGui::SetNextWindowPos(ImVec2(leftPanelWidth, menuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, topPanelHeight));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
    
    if (ImGui::Begin("Offset Finder", nullptr, flags))
    {
        DrawSectionHeader("Add New Offset");
        
        ImGui::Columns(3, "AddOffsetColumns");
        ImGui::SetColumnWidth(0, 200);
        ImGui::SetColumnWidth(1, 150);
        ImGui::SetColumnWidth(2, 100);
        
        ImGui::Text("Name");
        ImGui::NextColumn();
        ImGui::Text("Offset (Hex)");
        ImGui::NextColumn();
        ImGui::Text("Action");
        ImGui::NextColumn();
        
        ImGui::InputText("##OffsetName", m_offsetNameBuffer, sizeof(m_offsetNameBuffer));
        ImGui::NextColumn();
        ImGui::InputText("##OffsetAddress", m_offsetAddressBuffer, sizeof(m_offsetAddressBuffer));
        ImGui::NextColumn();
        
        if (DrawButton("Add##AddOffset"))
        {
            AddOffsetEntry();
        }
        ImGui::NextColumn();
        
        ImGui::Columns(1);
        ImGui::Spacing();
        
        if (DrawButton("Save Offsets"))
        {
            SaveOffsets();
        }
        ImGui::SameLine();
        if (DrawButton("Load Offsets"))
        {
            LoadOffsets();
        }
        ImGui::SameLine();
        if (DrawButton("Export As..."))
        {
            std::string filename = "offsets_export_" + std::to_string(time(nullptr)) + ".txt";
            SaveOffsets(filename);
        }
        
        ImGui::Spacing();
        
        DrawSectionHeader("Current Offsets");
        
        if (ImGui::BeginTable("OffsetsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 200.0f);
            ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Absolute Address", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableHeadersRow();
            
            for (size_t i = 0; i < m_offsetEntries.size(); ++i)
            {
                ImGui::TableNextRow();
                OffsetEntry& entry = m_offsetEntries[i];
                
                ImGui::TableNextColumn();
                ImGui::Text("%s", entry.name.c_str());
                
                ImGui::TableNextColumn();
                ImGui::Text("%s", entry.address.c_str());
                
                ImGui::TableNextColumn();
                ImGui::Text("%s", entry.value.c_str());
                
                ImGui::TableNextColumn();
                DrawStatusIndicator(entry.isValid ? "Valid" : "Invalid", entry.isValid);
                
                ImGui::TableNextColumn();
                
                std::string readId = "Read##" + std::to_string(i);
                std::string removeId = "Remove##" + std::to_string(i);
                
                if (ImGui::SmallButton(readId.c_str()))
                {
                    m_logMessages.push_back("[INFO] Reading value at " + entry.address);
                    // TODO: Implement read functionality
                    entry.value = "0xDEADBEEF"; // Placeholder
                    entry.isValid = true;
                }
                
                ImGui::SameLine();
                
                if (ImGui::SmallButton(removeId.c_str()))
                {
                    RemoveOffsetEntry(i);
                    break; // Break to avoid iterator invalidation
                }
            }
            
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void UIManager::RenderMemoryViewer()
{
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Memory Viewer", &m_showMemoryViewer))
    {
        ImGui::Text("Memory Viewer - Not Yet Implemented");
        ImGui::Separator();
        
        static char addressBuffer[32] = "0x00000000";
        ImGui::InputText("Address", addressBuffer, sizeof(addressBuffer));
        
        if (DrawButton("Read Memory"))
        {
            m_logMessages.push_back("[INFO] Memory viewer functionality will be implemented");
        }
        
        ImGui::Spacing();
        ImGui::Text("Memory dump will be displayed here...");
    }
    ImGui::End();
}

void UIManager::RenderProcessSelector()
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    
    float maxHeight = io.DisplaySize.y * 0.8f; // Use max 80% of screen height
    float windowHeight = std::min(450.0f, maxHeight); // Cap at 450px or 80% of screen
    ImGui::SetNextWindowSize(ImVec2(650, windowHeight), ImGuiCond_Always);
    
    if (ImGui::Begin("Process Selector", &m_showProcessSelector, ImGuiWindowFlags_NoResize))
    {
        ImGui::Text("Available Processes");
        ImGui::Separator();
        
        static int selectedProcess = -1;
        static std::string selectedProcessName;
        
        if (m_dmaManager)
        {
            std::vector<ProcessInfo> processes = m_dmaManager->GetProcessList();
            
            static char searchBuffer[256] = "";
            ImGui::InputText("Filter", searchBuffer, sizeof(searchBuffer));
            ImGui::Separator();
            
            float buttonHeight = ImGui::GetFrameHeight();
            float infoTextHeight = ImGui::GetTextLineHeightWithSpacing();
            float reservedHeight = buttonHeight + infoTextHeight + ImGui::GetStyle().WindowPadding.y + ImGui::GetStyle().ItemSpacing.y * 3;
            
            ImGui::BeginChild("ProcessList", ImVec2(0, -reservedHeight));
            
            std::string filterStr = std::string(searchBuffer);
            std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);
            
            int displayIndex = 0;
            for (int i = 0; i < processes.size(); ++i)
            {
                const auto& process = processes[i];
                
                if (!filterStr.empty())
                {
                    std::string processName = process.processName;
                    std::transform(processName.begin(), processName.end(), processName.begin(), ::tolower);
                    if (processName.find(filterStr) == std::string::npos)
                        continue;
                }
                
                std::string displayText = "[" + std::to_string(process.processId) + "] " + process.processName;
                
                if (ImGui::Selectable(displayText.c_str(), selectedProcess == i))
                {
                    selectedProcess = i;
                    selectedProcessName = process.processName;
                    strcpy_s(m_processNameBuffer, process.processName.c_str());
                }
                
                displayIndex++;
            }
            
            ImGui::EndChild();
            
            if (!filterStr.empty())
            {
                ImGui::Text("Found %d processes (%d shown)", (int)processes.size(), displayIndex);
            }
            else
            {
                ImGui::Text("Found %d processes", (int)processes.size());
            }
        }
        else
        {
            ImGui::Text("DMA Manager not available");
        }
        
        ImGui::Separator();
        
        float availableWidth = ImGui::GetContentRegionAvail().x;
        float buttonWidth = (availableWidth - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f; 
        if (DrawButton("Select Process", ImVec2(buttonWidth, 0)) && selectedProcess >= 0 && !selectedProcessName.empty())
        {
            m_showProcessSelector = false;
            m_logMessages.push_back("[INFO] Selected process: " + selectedProcessName);
            
            if (m_dmaManager)
            {
                std::string processName = selectedProcessName;
                
                m_dmaManager->AttachToProcessAsync(processName,
                    [this, processName](const AsyncResult<bool>& result) {
                        if (result.isSuccess)
                        {
                            m_logMessages.push_back("[SUCCESS] " + result.logMessage);
                            m_selectedProcess = processName;
                        }
                        else
                        {
                            m_logMessages.push_back("[ERROR] " + result.errorMessage);
                            m_selectedProcess = "";
                        }
                    });
            }
            else
            {
                m_logMessages.push_back("[ERROR] DMA Manager not available");
            }
        }
        
        ImGui::SameLine();
        
        if (DrawButton("Refresh", ImVec2(buttonWidth, 0)))
        {
            if (m_dmaManager)
            {
                m_logMessages.push_back("[INFO] Refreshing process list...");
                // The process list will be refreshed automatically by the DMA manager
            }
        }
        
        ImGui::SameLine();
        
        if (DrawButton("Cancel", ImVec2(buttonWidth, 0)))
        {
            m_showProcessSelector = false;
        }
    }
    ImGui::End();
}

void UIManager::RenderStatusWindow()
{
    ImGuiIO& io = ImGui::GetIO();
    float windowWidth = io.DisplaySize.x;
    float windowHeight = io.DisplaySize.y;
    float menuBarHeight = 20.0f;
    float bottomPanelHeight = windowHeight * 0.25f;
    float topPanelHeight = windowHeight - menuBarHeight - bottomPanelHeight;
    
    ImGui::SetNextWindowPos(ImVec2(0, menuBarHeight + topPanelHeight));
    ImGui::SetNextWindowSize(ImVec2(windowWidth, bottomPanelHeight));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
    
    if (ImGui::Begin("Status & Logs", nullptr, flags))
    {
        if (DrawButton("Clear Logs"))
        {
            m_logMessages.clear();
        }
        
        ImGui::Separator();
        
        ImGui::BeginChild("LogArea");
        for (const auto& message : m_logMessages)
        {
            ImGui::TextWrapped("%s", message.c_str());
        }
        
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }
        
        ImGui::EndChild();
    }
    ImGui::End();
}

void UIManager::RenderAboutDialog()
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Always);
    
    if (ImGui::Begin("About", &m_showAboutDialog, ImGuiWindowFlags_NoResize))
    {
        ImGui::Text("Unreal Offset Finder - DMA");
        ImGui::Separator();
        
        ImGui::Text("Version: 1.0.0");
        ImGui::Text("Build Date: %s %s", __DATE__, __TIME__);
        ImGui::Text("File Format: 1.0");
        ImGui::Spacing();
        
        ImGui::Text("Created by:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Overrde");
        ImGui::Spacing();
        
        ImGui::TextWrapped("Attempts to find GWorld, GName, and GObject with pattern scanning.");
        ImGui::Spacing();
        
        if (DrawButton("Close", ImVec2(-1, 0)))
        {
            m_showAboutDialog = false;
        }
    }
    ImGui::End();
}

void UIManager::AddOffsetEntry()
{
    std::string name(m_offsetNameBuffer);
    std::string addressInput(m_offsetAddressBuffer);
    
    if (name.empty() || addressInput.empty())
    {
        m_logMessages.push_back("[ERROR] Please enter both name and offset");
        return;
    }
    
    uint64_t inputAddress = DMAManager::ParseHexAddress(addressInput);
    if (inputAddress == 0)
    {
        m_logMessages.push_back("[ERROR] Invalid address format");
        return;
    }
    
    uint64_t mainBase = 0;
    if (m_dmaManager && m_dmaManager->IsConnected())
    {
        mainBase = m_dmaManager->GetMainModuleBase();
    }
    
    std::string offsetStr;
    std::string absoluteStr;
    
    if (mainBase != 0 && inputAddress > mainBase && inputAddress < (mainBase + 0x10000000)) // Within ~256MB of main base
    {
        uint64_t offset = inputAddress - mainBase;
        offsetStr = DMAManager::FormatHexAddress(offset);
        absoluteStr = DMAManager::FormatHexAddress(inputAddress);
        m_logMessages.push_back("[INFO] Converted absolute address " + absoluteStr + " to offset " + offsetStr);
    }
    else
    {
        offsetStr = DMAManager::FormatHexAddress(inputAddress);
        if (mainBase != 0)
        {
            absoluteStr = DMAManager::FormatHexAddress(mainBase + inputAddress);
        }
        else
        {
            absoluteStr = "Module base unknown";
        }
        m_logMessages.push_back("[INFO] Added offset: " + offsetStr);
    }
    
    m_offsetEntries.emplace_back(name, offsetStr, absoluteStr);
    
    memset(m_offsetNameBuffer, 0, sizeof(m_offsetNameBuffer));
    memset(m_offsetAddressBuffer, 0, sizeof(m_offsetAddressBuffer));
}

void UIManager::RemoveOffsetEntry(size_t index)
{
    if (index < m_offsetEntries.size())
    {
        std::string name = m_offsetEntries[index].name;
        m_offsetEntries.erase(m_offsetEntries.begin() + index);
        m_logMessages.push_back("[INFO] Removed offset: " + name);
    }
}

bool UIManager::SaveOffsets(const std::string& filename)
{
    try
    {
        std::ofstream file(filename);
        if (!file.is_open())
        {
            m_logMessages.push_back("[ERROR] Failed to create file: " + filename);
            return false;
        }

        file << "# Unreal Offset Finder - Saved Offsets\n";
        file << "# File Format Version: 1.0\n";
        
        std::time_t now = std::time(nullptr);
        char timeStr[100];
        struct tm timeInfo;
        localtime_s(&timeInfo, &now);
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeInfo);
        file << "# Generated on: " << timeStr << "\n";
        file << "# Build Date: " << __DATE__ << " " << __TIME__ << "\n";
        file << "#\n";
        
        if (m_dmaManager && m_dmaManager->IsConnected())
        {
            ProcessInfo processInfo = m_dmaManager->GetCurrentProcessInfo();
            uint64_t mainBase = m_dmaManager->GetMainModuleBase();
            
            file << "# === Game Information ===\n";
            file << "# Process Name: " << processInfo.processName << "\n";
            file << "# Process ID: " << processInfo.processId << " (changes on restart)\n";
            file << "# Module Base: " << DMAManager::FormatHexAddress(mainBase) << " (changes with ASLR)\n";
            file << "# Module Size: " << DMAManager::FormatHexAddress(processInfo.imageSize) << "\n";
            if (processInfo.baseAddress != mainBase)
            {
                file << "# Process Base: " << DMAManager::FormatHexAddress(processInfo.baseAddress) << "\n";
            }
            file << "# Offset Count: " << m_offsetEntries.size() << "\n";
        }
        else
        {
            file << "# === Game Information ===\n";
            file << "# Status: Not connected to any process\n";
            file << "# Offset Count: " << m_offsetEntries.size() << "\n";
        }
        
        file << "#\n";
        file << "# === Format Information ===\n";
        file << "# Format: Name,Offset,AbsoluteAddress\n";
        file << "# Note: Offsets are relative to module base address\n";
        file << "# Note: Absolute addresses change with ASLR, use offsets!\n";
        file << "#\n";

        for (const auto& entry : m_offsetEntries)
        {
            file << entry.name << "," << entry.address << "," << entry.value << "\n";
        }

        file.close();
        
        m_logMessages.push_back("[SUCCESS] Saved " + std::to_string(m_offsetEntries.size()) + " offsets to " + filename);
        return true;
    }
    catch (const std::exception& e)
    {
        m_logMessages.push_back("[ERROR] Exception while saving: " + std::string(e.what()));
        return false;
    }
}

bool UIManager::LoadOffsets(const std::string& filename)
{
    try
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            m_logMessages.push_back("[ERROR] Failed to open file: " + filename);
            return false;
        }

        std::string line;
        size_t loadedCount = 0;
        size_t lineNumber = 0;
        std::string gameInfo = "";

        m_offsetEntries.clear();
        
        while (std::getline(file, line))
        {
            lineNumber++;
            
            if (line.find("# Process Name:") == 0)
            {
                gameInfo = line.substr(15); // Remove "# Process Name: "
                gameInfo.erase(gameInfo.find_last_not_of(" \t\r\n") + 1); // Trim
            }
            
            if (line.empty() || line[0] == '#')
                continue;

            std::stringstream ss(line);
            std::string name, offset, absolute;
            
            if (std::getline(ss, name, ',') && 
                std::getline(ss, offset, ',') && 
                std::getline(ss, absolute))
            {
                    name.erase(name.find_last_not_of(" \t\r\n") + 1);
                offset.erase(offset.find_last_not_of(" \t\r\n") + 1);
                absolute.erase(absolute.find_last_not_of(" \t\r\n") + 1);

                if (!name.empty() && !offset.empty())
                {
                    m_offsetEntries.emplace_back(name, offset, absolute);
                    loadedCount++;
                }
            }
            else
            {
                m_logMessages.push_back("[WARNING] Skipped invalid line " + std::to_string(lineNumber) + ": " + line);
            }
        }

        file.close();
        
        std::string successMsg = "[SUCCESS] Loaded " + std::to_string(loadedCount) + " offsets from " + filename;
        if (!gameInfo.empty())
        {
            successMsg += " (Game: " + gameInfo + ")";
        }
        m_logMessages.push_back(successMsg);
        
        return true;
    }
    catch (const std::exception& e)
    {
        m_logMessages.push_back("[ERROR] Exception while loading: " + std::string(e.what()));
        return false;
    }
}

bool UIManager::DrawButton(const char* label, const ImVec2& size, bool enabled)
{
    if (!enabled)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.15f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.15f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.00f));
    }
    
    bool result = enabled ? ImGui::Button(label, size) : (ImGui::Button(label, size), false);
    
    if (!enabled)
    {
        ImGui::PopStyleColor(4);
    }
    
    return result;
}

void UIManager::DrawStatusIndicator(const char* status, bool isGood)
{
    ImVec4 color = isGood ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float size = ImGui::GetTextLineHeight() * 0.6f;
    ImVec2 rectMin = ImVec2(pos.x, pos.y + (ImGui::GetTextLineHeight() - size) * 0.5f);
    ImVec2 rectMax = ImVec2(rectMin.x + size, rectMin.y + size);
    
    ImU32 colorU32 = ImGui::GetColorU32(color);
    drawList->AddRectFilled(rectMin, rectMax, colorU32, 2.0f);
    
    ImGui::Dummy(ImVec2(size + 4.0f, ImGui::GetTextLineHeight()));
    ImGui::SameLine();
    ImGui::TextColored(color, "%s", status);
}

void UIManager::DrawSectionHeader(const char* title)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->Fonts.Size > 1)
    {
        ImGui::PushFont(io.Fonts->Fonts[1]);
    }
    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.56f, 1.00f, 1.00f));
    ImGui::Text("%s", title);
    ImGui::PopStyleColor();
    
    if (io.Fonts->Fonts.Size > 1)
    {
        ImGui::PopFont();
    }
    
    ImGui::Separator();
    ImGui::Spacing();
}

void UIManager::DrawSpinner(float radius, float thickness)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 center = ImVec2(pos.x + radius, pos.y + radius);
    
    const int segments = 30;
    const float angleStep = 2.0f * 3.14159f / segments;
    
    for (int i = 0; i < segments; ++i)
    {
        float angle = i * angleStep + m_progressSpinner;
        float nextAngle = (i + 1) * angleStep + m_progressSpinner;
        
        float alpha = (float)(segments - i) / segments * 0.8f + 0.2f;
        ImU32 color = ImGui::GetColorU32(ImVec4(0.28f, 0.56f, 1.00f, alpha));
        
        ImVec2 p1 = ImVec2(
            center.x + cos(angle) * (radius - thickness),
            center.y + sin(angle) * (radius - thickness)
        );
        ImVec2 p2 = ImVec2(
            center.x + cos(angle) * radius,
            center.y + sin(angle) * radius
        );
        ImVec2 p3 = ImVec2(
            center.x + cos(nextAngle) * radius,
            center.y + sin(nextAngle) * radius
        );
        ImVec2 p4 = ImVec2(
            center.x + cos(nextAngle) * (radius - thickness),
            center.y + sin(nextAngle) * (radius - thickness)
        );
        
        drawList->AddQuadFilled(p1, p2, p3, p4, color);
    }
    
    ImGui::Dummy(ImVec2(radius * 2, radius * 2));
}

 