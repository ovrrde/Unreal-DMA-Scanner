#include "UIManager.h"
#include "../DMA/DMAManager.h"
#include <imgui.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <ctime>

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
    // Initialize input buffers
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
    // Add initial log message
    m_logMessages.push_back("[INFO] UI Manager initialized successfully");
    
    std::cout << "UI Manager initialized" << std::endl;
    return true;
}

void UIManager::Update(float deltaTime)
{
    // Update UI state here if needed
    // Animate progress spinner for async operations
    if (m_dmaManager && m_dmaManager->HasPendingOperations())
    {
        m_progressSpinner += deltaTime * 8.0f; // Spin speed
        if (m_progressSpinner > 2.0f * 3.14159f)
            m_progressSpinner -= 2.0f * 3.14159f;
    }
    else
    {
        m_progressSpinner = 0.0f;
    }
}

void UIManager::Render()
{
    // Get the main viewport to calculate window sizes
    ImGuiIO& io = ImGui::GetIO();
    float windowWidth = io.DisplaySize.x;
    float windowHeight = io.DisplaySize.y;
    
    // Calculate areas for our layout
    float menuBarHeight = 20.0f;
    float leftPanelWidth = windowWidth * 0.3f;
    float rightPanelWidth = windowWidth * 0.7f;
    float bottomPanelHeight = windowHeight * 0.25f;
    float topPanelHeight = windowHeight - menuBarHeight - bottomPanelHeight;
    
    // Render main menu bar
    RenderMenuBar();
    
    // Render all windows with calculated positions and sizes
    
    // Main panels are always rendered
    RenderControlPanel();
    RenderOffsetFinder();
    RenderStatusWindow();
    
    // Optional windows are rendered based on flags
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
                    // Success message is handled in LoadOffsets()
                }
            }
            
            if (ImGui::MenuItem("Save Offsets", "Ctrl+S"))
            {
                if (SaveOffsets())
                {
                    // Success message is handled in SaveOffsets()
                }
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Save As..."))
            {
                // For now, use a different filename - could add file dialog later
                std::string filename = "offsets_" + std::to_string(time(nullptr)) + ".txt";
                if (SaveOffsets(filename))
                {
                    // Success message is handled in SaveOffsets()
                }
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Exit", "Alt+F4"))
            {
                // This would trigger application shutdown
                m_logMessages.push_back("[INFO] Exit requested");
            }
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View"))
        {
            // Main panels are always visible, so show them as checked but disabled
            ImGui::MenuItem("Control Panel", nullptr, nullptr, false);
            ImGui::MenuItem("Offset Finder", nullptr, nullptr, false);
            ImGui::MenuItem("Status Window", nullptr, nullptr, false);
            
            ImGui::Separator();
            
            // Optional windows can still be toggled
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
                                    
                                    // Get main module base to calculate offsets
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
                    // The DMA manager automatically refreshes, but we can force it
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
    // Calculate position and size for left panel
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
        // Process Selection Section
        ImGui::Text("Process Selection");
        ImGui::Separator();
        
        ImGui::InputText("Process Name", m_processNameBuffer, sizeof(m_processNameBuffer));
        ImGui::SameLine();
        
        if (ImGui::Button("Browse"))
        {
            m_showProcessSelector = true;
        }
        
        ImGui::Spacing();
        
        if (ImGui::Button("Attach to Process", ImVec2(-1, 0)))
        {
            std::string processName(m_processNameBuffer);
            if (!processName.empty())
            {
                m_selectedProcess = processName;
                m_logMessages.push_back("[INFO] Attempting to attach to process: " + processName);
                
                if (m_dmaManager)
                {
                    // Use async attachment to prevent UI blocking
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
        
        // DMA Status Section
        ImGui::Text("DMA Status");
        ImGui::Separator();
        
        // Show connection status
        bool isConnected = m_dmaManager && m_dmaManager->IsConnected();
        ImGui::Text("Status: %s", isConnected ? "Connected" : "Not Connected");
        
        if (isConnected)
        {
            auto processInfo = m_dmaManager->GetCurrentProcessInfo();
            ImGui::Text("Target: %s (PID: %u)", processInfo.processName.c_str(), processInfo.processId);
        }
        
        // Show pending operations
        if (m_dmaManager && m_dmaManager->HasPendingOperations())
        {
            size_t pending = m_dmaManager->GetPendingOperationCount();
            
            // Draw spinner
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::SameLine();
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetCursorScreenPos();
            float radius = 8.0f;
            ImVec2 center = ImVec2(pos.x + radius, pos.y + radius);
            
            // Draw spinning circle
            const int segments = 8;
            for (int i = 0; i < segments; ++i)
            {
                float angle = (i / (float)segments) * 2.0f * 3.14159f + m_progressSpinner;
                float alpha = (i / (float)segments) * 0.8f + 0.2f;
                ImU32 color = ImGui::GetColorU32(ImVec4(1, 1, 0, alpha));
                
                float x = center.x + cos(angle) * radius * 0.5f;
                float y = center.y + sin(angle) * radius * 0.5f;
                draw_list->AddCircleFilled(ImVec2(x, y), 2.0f, color);
            }
            
            ImGui::Dummy(ImVec2(radius * 2, radius * 2));
            ImGui::PopStyleVar();
            
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Operations pending: %zu", pending);
        }
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Quick Actions Section
        ImGui::Text("Quick Actions");
        ImGui::Separator();
        
        bool hasOperations = m_dmaManager && m_dmaManager->HasPendingOperations();
        
        if (ImGui::Button("Scan UE Globals", ImVec2(-1, 0)))
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
                                
                                // Get main module base to calculate offsets
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
        
        if (ImGui::Button("Get Module Base", ImVec2(-1, 0)))
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
        
        if (ImGui::Button("Clear All Offsets", ImVec2(-1, 0)))
        {
            m_offsetEntries.clear();
            m_logMessages.push_back("[INFO] Cleared all offset entries");
        }
        
        // Add cancel operations button if there are pending operations
        if (hasOperations)
        {
            if (ImGui::Button("Cancel Operations", ImVec2(-1, 0)))
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
    // Calculate position and size for right panel
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
        // Add new offset section
        ImGui::Text("Add New Offset");
        ImGui::Separator();
        
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
        
        if (ImGui::Button("Add##AddOffset"))
        {
            AddOffsetEntry();
        }
        ImGui::NextColumn();
        
        ImGui::Columns(1);
        ImGui::Spacing();
        
        // Quick Save/Load buttons
        if (ImGui::Button("Save Offsets"))
        {
            SaveOffsets();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Offsets"))
        {
            LoadOffsets();
        }
        ImGui::SameLine();
        if (ImGui::Button("Export As..."))
        {
            std::string filename = "offsets_export_" + std::to_string(time(nullptr)) + ".txt";
            SaveOffsets(filename);
        }
        
        ImGui::Spacing();
        
        // Offset entries table
        ImGui::Text("Current Offsets");
        ImGui::Separator();
        
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
                ImGui::TextColored(entry.isValid ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1), 
                                 entry.isValid ? "Valid" : "Invalid");
                
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
    // For memory viewer, we'll center it or let it float since it's optional
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Memory Viewer", &m_showMemoryViewer))
    {
        ImGui::Text("Memory Viewer - Not Yet Implemented");
        ImGui::Separator();
        
        static char addressBuffer[32] = "0x00000000";
        ImGui::InputText("Address", addressBuffer, sizeof(addressBuffer));
        
        if (ImGui::Button("Read Memory"))
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
    // Center the process selector on screen
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Process Selector", &m_showProcessSelector))
    {
        ImGui::Text("Available Processes");
        ImGui::Separator();
        
        // Get actual process list from DMA manager
        static int selectedProcess = -1;
        static std::string selectedProcessName;
        
        if (m_dmaManager)
        {
            std::vector<ProcessInfo> processes = m_dmaManager->GetProcessList();
            
            // Add search filter
            static char searchBuffer[256] = "";
            ImGui::InputText("Filter", searchBuffer, sizeof(searchBuffer));
            ImGui::Separator();
            
            // Create scrollable area for process list
            ImGui::BeginChild("ProcessList", ImVec2(0, -40));
            
            std::string filterStr = std::string(searchBuffer);
            std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);
            
            int displayIndex = 0;
            for (int i = 0; i < processes.size(); ++i)
            {
                const auto& process = processes[i];
                
                // Apply filter
                if (!filterStr.empty())
                {
                    std::string processName = process.processName;
                    std::transform(processName.begin(), processName.end(), processName.begin(), ::tolower);
                    if (processName.find(filterStr) == std::string::npos)
                        continue;
                }
                
                // Format display string with PID and name
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
            
            ImGui::Text("Found %d processes", (int)processes.size());
            if (!filterStr.empty())
            {
                ImGui::SameLine();
                ImGui::Text("(%d filtered)", displayIndex);
            }
        }
        else
        {
            ImGui::Text("DMA Manager not available");
        }
        
        ImGui::Separator();
        
        if (ImGui::Button("Select Process") && selectedProcess >= 0 && !selectedProcessName.empty())
        {
            m_showProcessSelector = false;
            m_logMessages.push_back("[INFO] Selected process: " + selectedProcessName);
            
            // Try to attach to the selected process asynchronously
            if (m_dmaManager)
            {
                // Create local copy to avoid static variable capture issue
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
        
        if (ImGui::Button("Refresh"))
        {
            if (m_dmaManager)
            {
                m_logMessages.push_back("[INFO] Refreshing process list...");
                // The process list will be refreshed automatically by the DMA manager
            }
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel"))
        {
            m_showProcessSelector = false;
        }
    }
    ImGui::End();
}

void UIManager::RenderStatusWindow()
{
    // Calculate position and size for bottom panel
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
        if (ImGui::Button("Clear Logs"))
        {
            m_logMessages.clear();
        }
        
        ImGui::Separator();
        
        // Display log messages
        ImGui::BeginChild("LogArea");
        for (const auto& message : m_logMessages)
        {
            ImGui::TextWrapped("%s", message.c_str());
        }
        
        // Auto-scroll to bottom
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
    // Center the about dialog on screen
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
        
        if (ImGui::Button("Close", ImVec2(-1, 0)))
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
    
    // Parse the input address
    uint64_t inputAddress = DMAManager::ParseHexAddress(addressInput);
    if (inputAddress == 0)
    {
        m_logMessages.push_back("[ERROR] Invalid address format");
        return;
    }
    
    // Check if this looks like an absolute address (large value) or an offset (small value)
    uint64_t mainBase = 0;
    if (m_dmaManager && m_dmaManager->IsConnected())
    {
        mainBase = m_dmaManager->GetMainModuleBase();
    }
    
    std::string offsetStr;
    std::string absoluteStr;
    
    if (mainBase != 0 && inputAddress > mainBase && inputAddress < (mainBase + 0x10000000)) // Within ~256MB of main base
    {
        // Looks like an absolute address, convert to offset
        uint64_t offset = inputAddress - mainBase;
        offsetStr = DMAManager::FormatHexAddress(offset);
        absoluteStr = DMAManager::FormatHexAddress(inputAddress);
        m_logMessages.push_back("[INFO] Converted absolute address " + absoluteStr + " to offset " + offsetStr);
    }
    else
    {
        // Treat as offset
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
    
    // Clear input buffers
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

        // Write header with game/module information
        file << "# Unreal Offset Finder - Saved Offsets\n";
        file << "# File Format Version: 1.0\n";
        
        // Get current time for more detailed timestamp
        std::time_t now = std::time(nullptr);
        char timeStr[100];
        struct tm timeInfo;
        localtime_s(&timeInfo, &now);
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeInfo);
        file << "# Generated on: " << timeStr << "\n";
        file << "# Build Date: " << __DATE__ << " " << __TIME__ << "\n";
        file << "#\n";
        
        // Add game/process information
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

        // Write offset entries
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

        // Clear existing offsets (ask user first in a real implementation)
        m_offsetEntries.clear();
        
        while (std::getline(file, line))
        {
            lineNumber++;
            
            // Parse header information for game details
            if (line.find("# Process Name:") == 0)
            {
                gameInfo = line.substr(15); // Remove "# Process Name: "
                gameInfo.erase(gameInfo.find_last_not_of(" \t\r\n") + 1); // Trim
            }
            
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#')
                continue;

            // Parse CSV format: Name,Offset,AbsoluteAddress
            std::stringstream ss(line);
            std::string name, offset, absolute;
            
            if (std::getline(ss, name, ',') && 
                std::getline(ss, offset, ',') && 
                std::getline(ss, absolute))
            {
                // Trim whitespace
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
        
        // Show load success with game information
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

 