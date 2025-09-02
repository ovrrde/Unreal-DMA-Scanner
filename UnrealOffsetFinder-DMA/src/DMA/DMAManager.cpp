#include "DMAManager.h"
#include <leechcore.h>
#include <vmmdll.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <condition_variable>

DMAManager::DMAManager()
    : m_hLeechCore(nullptr)
    , m_hVMM(nullptr)
    , m_isInitialized(false)
    , m_isConnected(false)
    , m_currentProcessId(0)
    , m_processRefreshTimer(0.0f)
    , m_shouldStop(false)
    , m_pendingOperations(0)
{
}

DMAManager::~DMAManager()
{
    Shutdown();
}

bool DMAManager::Initialize()
{
    try
    {
        std::cout << "Initializing DMA Manager..." << std::endl;
        
        // Initialize LeechCore device
        if (!InitializeDevice())
        {
            std::cerr << "Failed to initialize LeechCore device" << std::endl;
            return false;
        }
        
        m_isInitialized = true;
        
        // Start worker thread
        m_shouldStop = false;
        m_workerThread = std::thread(&DMAManager::WorkerThread, this);
        
        // Initial process list refresh
        RefreshProcessList();
        
        std::cout << "DMA Manager initialized successfully with async support" << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception during DMA initialization: " << e.what() << std::endl;
        return false;
    }
}

void DMAManager::Update(float deltaTime)
{
    if (!m_isInitialized)
        return;
    
    // Update process refresh timer
    m_processRefreshTimer += deltaTime;
    
    // Auto-refresh process list periodically
    if (m_processRefreshTimer >= PROCESS_REFRESH_INTERVAL)
    {
        RefreshProcessList();
        m_processRefreshTimer = 0.0f;
    }
    
    // Process completed async tasks on main thread
    ProcessCompletedTasks();
}

void DMAManager::Shutdown()
{
    // Stop worker thread
    if (m_workerThread.joinable())
    {
        {
            std::lock_guard<std::mutex> lock(m_taskQueueMutex);
            m_shouldStop = true;
        }
        m_taskCondition.notify_all();
        m_workerThread.join();
    }
    
    if (m_isConnected)
    {
        DetachFromProcess();
    }
    
    CleanupDevice();
    
    m_isInitialized = false;
    m_processList.clear();
    
    // Clear any remaining callbacks
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_completedCallbacks.clear();
    }
    
    std::cout << "DMA Manager shutdown complete" << std::endl;
}

bool DMAManager::IsConnected() const
{
    return m_isInitialized && m_isConnected;
}

bool DMAManager::AttachToProcess(const std::string& processName)
{
    if (!m_isInitialized)
    {
        std::cerr << "DMA Manager not initialized" << std::endl;
        return false;
    }
    
    try
    {
        std::cout << "Attempting to attach to process: " << processName << std::endl;
        
        // Find process in the list
        auto it = std::find_if(m_processList.begin(), m_processList.end(),
            [&processName](const ProcessInfo& info) {
                return info.processName == processName;
            });
        
        if (it == m_processList.end())
        {
            std::cerr << "Process not found: " << processName << std::endl;
            RefreshProcessList(); // Try refreshing the list
            return false;
        }
        
        return AttachToProcess(it->processId);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception during process attachment: " << e.what() << std::endl;
        return false;
    }
}

bool DMAManager::AttachToProcess(uint32_t processId)
{
    if (!m_isInitialized)
    {
        std::cerr << "DMA Manager not initialized" << std::endl;
        return false;
    }
    
    try
    {
        std::cout << "Attempting to attach to PID: " << processId << std::endl;
        
        // Find process info
        auto it = std::find_if(m_processList.begin(), m_processList.end(),
            [processId](const ProcessInfo& info) {
                return info.processId == processId;
            });
        
        if (it == m_processList.end())
        {
            std::cerr << "Process ID not found: " << processId << std::endl;
            return false;
        }
        
        m_currentProcess = *it;
        m_currentProcessId = processId;
        m_isConnected = true;
        
        // Get the real main module information now that we're attached
        std::cout << "Getting main module information for attached process..." << std::endl;
        uint64_t realMainBase = GetMainModuleBase(); // This will update m_currentProcess with correct info
        if (realMainBase != 0)
        {
            std::cout << "Updated process info with real main module data" << std::endl;
        }
        
        std::cout << "Successfully attached to process: " << m_currentProcess.processName 
                  << " (PID: " << m_currentProcess.processId << ")" << std::endl;
        
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception during process attachment: " << e.what() << std::endl;
        return false;
    }
}

void DMAManager::DetachFromProcess()
{
    if (m_isConnected)
    {
        std::cout << "Detaching from process: " << m_currentProcess.processName << std::endl;
        
        m_isConnected = false;
        m_currentProcess = ProcessInfo();
        m_currentProcessId = 0;
        
        std::cout << "Process detached successfully" << std::endl;
    }
}

ProcessInfo DMAManager::GetCurrentProcessInfo() const
{
    return m_currentProcess;
}

std::vector<ProcessInfo> DMAManager::GetProcessList() const
{
    return m_processList;
}

size_t DMAManager::ReadMemory(uint64_t address, void* buffer, size_t size) const
{
    if (!IsConnected())
    {
        std::cerr << "Not connected to any process" << std::endl;
        return 0;
    }
    
    if (!buffer || size == 0)
    {
        return 0;
    }
    
    try
    {
        if (!m_hVMM)
        {
            // Mock read when VMM is not available
            std::cout << "Mock reading " << size << " bytes from address 0x" 
                      << std::hex << address << std::dec << std::endl;
            
            uint8_t* buf = static_cast<uint8_t*>(buffer);
            for (size_t i = 0; i < size; ++i)
            {
                buf[i] = static_cast<uint8_t>((address + i) & 0xFF);
            }
            return size;
        }
        
        // Real memory read using VMMDLL
        DWORD cbRead = 0;
        BOOL success = VMMDLL_MemReadEx(
            static_cast<VMM_HANDLE>(m_hVMM),
            m_currentProcessId,
            address,
            static_cast<PBYTE>(buffer),
            static_cast<DWORD>(size),
            &cbRead,
            0  // flags
        );
        
        if (success)
        {
            return static_cast<size_t>(cbRead);
        }
        else
        {
            std::cerr << "VMMDLL_MemRead failed for address 0x" << std::hex << address << std::dec << std::endl;
            return 0;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception during memory read: " << e.what() << std::endl;
        return 0;
    }
}

size_t DMAManager::WriteMemory(uint64_t address, const void* buffer, size_t size) const
{
    if (!IsConnected())
    {
        std::cerr << "Not connected to any process" << std::endl;
        return 0;
    }
    
    if (!buffer || size == 0)
    {
        return 0;
    }
    
    try
    {
        if (!m_hVMM)
        {
            // Mock write when VMM is not available
            std::cout << "Mock writing " << size << " bytes to address 0x" 
                      << std::hex << address << std::dec << std::endl;
            return size;
        }
        
        // Real memory write using VMMDLL
        BOOL success = VMMDLL_MemWrite(
            static_cast<VMM_HANDLE>(m_hVMM),
            m_currentProcessId,
            address,
            static_cast<PBYTE>(const_cast<void*>(buffer)),
            static_cast<DWORD>(size)
        );
        
        if (success)
        {
            return size;
        }
        else
        {
            std::cerr << "VMMDLL_MemWrite failed for address 0x" << std::hex << address << std::dec << std::endl;
            return 0;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception during memory write: " << e.what() << std::endl;
        return 0;
    }
}

std::string DMAManager::ReadString(uint64_t address, size_t maxLength) const
{
    if (!IsConnected())
    {
        return "";
    }
    
    try
    {
        std::vector<char> buffer(maxLength + 1);
        size_t bytesRead = ReadMemory(address, buffer.data(), maxLength);
        
        if (bytesRead == 0)
        {
            return "";
        }
        
        // Ensure null termination
        buffer[bytesRead] = '\0';
        
        return std::string(buffer.data());
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception during string read: " << e.what() << std::endl;
        return "";
    }
}

uint64_t DMAManager::ParseHexAddress(const std::string& hexString)
{
    if (hexString.empty())
    {
        return 0;
    }
    
    try
    {
        std::string cleanHex = hexString;
        
        // Remove 0x prefix if present
        if (cleanHex.length() >= 2 && cleanHex.substr(0, 2) == "0x")
        {
            cleanHex = cleanHex.substr(2);
        }
        else if (cleanHex.length() >= 2 && cleanHex.substr(0, 2) == "0X")
        {
            cleanHex = cleanHex.substr(2);
        }
        
        // Parse as hex
        return std::stoull(cleanHex, nullptr, 16);
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

std::string DMAManager::FormatHexAddress(uint64_t address, bool uppercase)
{
    std::stringstream ss;
    ss << "0x" << std::hex;
    
    if (uppercase)
    {
        ss << std::uppercase;
    }
    
    ss << address;
    return ss.str();
}

bool DMAManager::InitializeDevice()
{
    try
    {
        std::cout << "Initializing DMA device..." << std::endl;
        
        // First try with memory map, then fallback without
        bool success = false;
        
        // Try with memory map first
        if (CheckMemoryMapExists())
        {
            std::cout << "Found memory map, attempting initialization with mmap.txt..." << std::endl;
            success = InitializeWithArgs(true);
        }
        
        // Fallback without memory map if first attempt failed
        if (!success)
        {
            std::cout << "Attempting initialization without memory map..." << std::endl;
            success = InitializeWithArgs(false);
        }
        
        if (success)
        {
            std::cout << "DMA device initialized successfully!" << std::endl;
            return true;
        }
        else
        {
            std::cerr << "Failed to initialize DMA device" << std::endl;
            return false;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception during device initialization: " << e.what() << std::endl;
        return false;
    }
}

void DMAManager::CleanupDevice()
{
    if (m_hVMM)
    {
        std::cout << "Closing VMM handle..." << std::endl;
        VMMDLL_Close(static_cast<VMM_HANDLE>(m_hVMM));
        m_hVMM = nullptr;
    }
    
    if (m_hLeechCore)
    {
        // LeechCore is typically closed automatically when VMM is closed
        m_hLeechCore = nullptr;
    }
    
    std::cout << "DMA device cleanup complete" << std::endl;
}

void DMAManager::RefreshProcessList()
{
    try
    {
        m_processList.clear();
        
        if (!m_hVMM)
        {
            m_processList.emplace_back(1234, "There is no current process list.", 0x140000000, 0x10000000);
            std::cout << "Error loading process list. Found " << m_processList.size() << " processes." << std::endl;
            return;
        }
        
        // Get actual process list using VMMDLL
        SIZE_T cPIDs = 0;
        
        // First call to get the number of processes
        if (!VMMDLL_PidList(static_cast<VMM_HANDLE>(m_hVMM), nullptr, &cPIDs))
        {
            std::cerr << "Failed to get process count" << std::endl;
            return;
        }
        
        if (cPIDs == 0)
        {
            std::cout << "No processes found" << std::endl;
            return;
        }
        
        // Allocate buffer for PIDs
        std::vector<DWORD> pids(cPIDs);
        
        // Second call to get actual PIDs
        if (!VMMDLL_PidList(static_cast<VMM_HANDLE>(m_hVMM), pids.data(), &cPIDs))
        {
            std::cerr << "Failed to enumerate processes" << std::endl;
            return;
        }
        
        // Get information for each process
        for (size_t i = 0; i < cPIDs; ++i)
        {
            DWORD pid = pids[i];
            
            // Get process information
            SIZE_T cbProcessInfo = sizeof(VMMDLL_PROCESS_INFORMATION);
            VMMDLL_PROCESS_INFORMATION processInfo = { 0 };
            processInfo.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
            processInfo.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;
            
            if (VMMDLL_ProcessGetInformation(static_cast<VMM_HANDLE>(m_hVMM), pid, &processInfo, &cbProcessInfo))
            {
                // Calculate approximate size (this is a rough estimate)
                uint64_t baseSize = 0x1000000; // Default 16MB
                
                // Add process to list
                m_processList.emplace_back(
                    pid,
                    processInfo.szName,
                    processInfo.win.vaPEB ? processInfo.win.vaPEB : 0x140000000,
                    baseSize
                );
            }
        }
        
        std::cout << "Process list refreshed. Found " << m_processList.size() << " processes." << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception during process list refresh: " << e.what() << std::endl;
    }
}

bool DMAManager::CheckMemoryMapExists()
{
    std::ifstream file("mmap.txt");
    return file.good();
}

bool DMAManager::InitializeWithArgs(bool useMemoryMap)
{
    // Clean up any existing VMM handle
    if (m_hVMM)
    {
        std::cout << "Closing existing VMM handle..." << std::endl;
        VMMDLL_Close(static_cast<VMM_HANDLE>(m_hVMM));
        m_hVMM = nullptr;
    }

    // Build initialization arguments
    std::vector<LPCSTR> initArgs;
    
    // Device connection string for FPGA
    initArgs.push_back("-device");
    initArgs.push_back("fpga");
    
    // Essential arguments
    initArgs.push_back("-waitinitialize");    // Wait for full initialization
    initArgs.push_back("-norefresh");         // Disable background refreshes
    initArgs.push_back("-disable-python");   // Disable Python plugin system
    initArgs.push_back("-disable-symbolserver"); // Disable symbol server
    initArgs.push_back("-disable-symbols");  // Disable symbol lookups
    initArgs.push_back("-disable-infodb");   // Disable infodb
    
    // Add memory map if requested and available
    if (useMemoryMap && CheckMemoryMapExists())
    {
        initArgs.push_back("-memmap");
        initArgs.push_back("mmap.txt");
    }

    // Convert to array for VMMDLL_Initialize
    std::vector<LPCSTR> argsArray(initArgs.begin(), initArgs.end());
    
    std::cout << "Initializing DMA device with " << argsArray.size() << " arguments..." << std::endl;
    
    // Initialize VMM
    VMM_HANDLE hVMM = VMMDLL_Initialize(static_cast<DWORD>(argsArray.size()), argsArray.data());
    
    if (!hVMM)
    {
        std::cerr << "VMMDLL_Initialize failed" << std::endl;
        return false;
    }

    m_hVMM = hVMM;
    std::cout << "VMM initialized successfully" << std::endl;
    
    return true;
}

uint64_t DMAManager::GetModuleBase(const std::string& moduleName) const
{
    if (!IsConnected() || !m_hVMM)
    {
        return 0;
    }

    try
    {
        // Use VMMDLL_ProcessGetModuleBaseU for direct module base lookup
        ULONG64 moduleBase = VMMDLL_ProcessGetModuleBaseU(
            static_cast<VMM_HANDLE>(m_hVMM), 
            m_currentProcessId, 
            moduleName.c_str()
        );
        
        return static_cast<uint64_t>(moduleBase);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception in GetModuleBase: " << e.what() << std::endl;
        return 0;
    }
}

uint64_t DMAManager::GetMainModuleBase() const
{
    if (!IsConnected() || !m_hVMM)
    {
        return 0;
    }

    try
    {
        // Get the main executable module using VMMDLL
        PVMMDLL_MAP_MODULE pModuleMap = NULL;
        
        if (!VMMDLL_Map_GetModuleU(static_cast<VMM_HANDLE>(m_hVMM), m_currentProcessId, &pModuleMap, VMMDLL_MODULE_FLAG_NORMAL))
        {
            std::cerr << "Failed to get module map for main module detection" << std::endl;
            return m_currentProcess.baseAddress; // Fallback to process base
        }

        if (!pModuleMap || pModuleMap->cMap == 0)
        {
            if (pModuleMap)
                VMMDLL_MemFree(pModuleMap);
            return m_currentProcess.baseAddress; // Fallback to process base
        }

        // Find the main executable module (usually the first one or the one matching process name)
        uint64_t mainModuleBase = 0;
        size_t mainModuleSize = 0;
        
        // First, try to find module with same name as process
        std::string processBaseName = m_currentProcess.processName;
        
        for (DWORD i = 0; i < pModuleMap->cMap; ++i)
        {
            const auto& moduleEntry = pModuleMap->pMap[i];
            std::string moduleName = moduleEntry.uszText;
            
            // Check if this module matches our process name
            if (_stricmp(moduleName.c_str(), processBaseName.c_str()) == 0)
            {
                mainModuleBase = moduleEntry.vaBase;
                mainModuleSize = moduleEntry.cbImageSize;
                std::cout << "Found main module by name: " << moduleName 
                          << " (Base: " << FormatHexAddress(mainModuleBase) 
                          << ", Size: " << FormatHexAddress(mainModuleSize) << ")" << std::endl;
                break;
            }
        }
        
        // If not found by name, use the first module (usually the main executable)
        if (mainModuleBase == 0 && pModuleMap->cMap > 0)
        {
            const auto& moduleEntry = pModuleMap->pMap[0];
            mainModuleBase = moduleEntry.vaBase;
            mainModuleSize = moduleEntry.cbImageSize;
            std::cout << "Using first module as main: " << moduleEntry.uszText 
                      << " (Base: " << FormatHexAddress(mainModuleBase) 
                      << ", Size: " << FormatHexAddress(mainModuleSize) << ")" << std::endl;
        }

        VMMDLL_MemFree(pModuleMap);
        
        // Update the current process info with correct module size
        if (mainModuleBase != 0 && mainModuleSize != 0)
        {
            const_cast<DMAManager*>(this)->m_currentProcess.baseAddress = mainModuleBase;
            const_cast<DMAManager*>(this)->m_currentProcess.imageSize = mainModuleSize;
        }
        
        return mainModuleBase != 0 ? mainModuleBase : m_currentProcess.baseAddress;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception in GetMainModuleBase: " << e.what() << std::endl;
        return m_currentProcess.baseAddress;
    }
}

std::vector<ProcessInfo> DMAManager::GetModuleList() const
{
    std::vector<ProcessInfo> moduleList;

    if (!IsConnected() || !m_hVMM)
    {
        return moduleList;
    }

    try
    {
        PVMMDLL_MAP_MODULE pModuleMap = NULL;
        
        // Get module map using VMMDLL_Map_GetModuleU
        if (!VMMDLL_Map_GetModuleU(static_cast<VMM_HANDLE>(m_hVMM), m_currentProcessId, &pModuleMap, VMMDLL_MODULE_FLAG_NORMAL))
        {
            return moduleList;
        }

        if (!pModuleMap || pModuleMap->cMap == 0)
        {
            if (pModuleMap)
            {
                VMMDLL_MemFree(pModuleMap);
            }
            return moduleList;
        }

        // Convert to ProcessInfo structures
        for (DWORD i = 0; i < pModuleMap->cMap; ++i)
        {
            const auto& moduleEntry = pModuleMap->pMap[i];
            
            moduleList.emplace_back(
                0, // No PID for modules
                std::string(moduleEntry.uszText),
                moduleEntry.vaBase,
                moduleEntry.cbImageSize
            );
        }

        VMMDLL_MemFree(pModuleMap);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception in GetModuleList: " << e.what() << std::endl;
    }

    return moduleList;
}

uint64_t DMAManager::ScanSignature(const std::string& pattern, uint64_t startAddress, size_t scanSize) const
{
    if (!IsConnected())
    {
        std::cerr << "Not connected to any process" << std::endl;
        return 0;
    }

    // Parse the pattern
    std::vector<uint8_t> patternBytes;
    std::string mask;
    
    if (!ParseSignaturePattern(pattern, patternBytes, mask))
    {
        std::cerr << "Invalid signature pattern: " << pattern << std::endl;
        return 0;
    }

    // If no start address specified, use main module base
    if (startAddress == 0)
    {
        startAddress = GetMainModuleBase();
        if (startAddress == 0)
        {
            std::cerr << "Failed to get main module base address" << std::endl;
            return 0;
        }
    }

    // If no scan size specified, use main module size
    if (scanSize == 0)
    {
        scanSize = m_currentProcess.imageSize;
        if (scanSize == 0)
        {
            scanSize = 0x1000000; // Default 16MB
        }
    }

    std::cout << "Scanning for pattern: " << pattern << std::endl;
    std::cout << "Start: 0x" << std::hex << startAddress << ", Size: 0x" << scanSize << std::dec << std::endl;

    // Scan in chunks to avoid memory issues
    const size_t CHUNK_SIZE = 0x10000; // 64KB chunks
    std::vector<uint8_t> buffer(CHUNK_SIZE);

    for (size_t offset = 0; offset < scanSize; offset += CHUNK_SIZE - patternBytes.size())
    {
        size_t currentChunkSize = min(CHUNK_SIZE, scanSize - offset);
        uint64_t currentAddress = startAddress + offset;

        size_t bytesRead = ReadMemory(currentAddress, buffer.data(), currentChunkSize);
        if (bytesRead == 0)
        {
            continue; // Skip unreadable memory regions
        }

        // Search for pattern in this chunk
        size_t patternOffset = FindPatternInBuffer(buffer.data(), bytesRead, patternBytes, mask);
        if (patternOffset != SIZE_MAX)
        {
            uint64_t foundAddress = currentAddress + patternOffset;
            std::cout << "Pattern found at: 0x" << std::hex << foundAddress << std::dec << std::endl;
            return foundAddress;
        }
    }

    std::cout << "Pattern not found" << std::endl;
    return 0;
}

uint64_t DMAManager::ScanSignatureInModule(const std::string& pattern, const std::string& moduleName) const
{
    if (!IsConnected())
    {
        return 0;
    }

    // Get the module information
    uint64_t moduleBase = GetModuleBase(moduleName);
    if (moduleBase == 0)
    {
        std::cerr << "Module not found: " << moduleName << std::endl;
        return 0;
    }

    // Get module list to find the size
    auto modules = GetModuleList();
    size_t moduleSize = 0;
    
    for (const auto& module : modules)
    {
        if (_stricmp(module.processName.c_str(), moduleName.c_str()) == 0)
        {
            moduleSize = module.imageSize;
            break;
        }
    }

    if (moduleSize == 0)
    {
        std::cerr << "Failed to get module size for: " << moduleName << std::endl;
        return 0;
    }

    std::cout << "Scanning in module: " << moduleName << " (Base: 0x" << std::hex << moduleBase 
              << ", Size: 0x" << moduleSize << ")" << std::dec << std::endl;

    return ScanSignature(pattern, moduleBase, moduleSize);
}

uint64_t DMAManager::ReadMultiLevelPointer(uint64_t baseAddress, const std::vector<uint64_t>& offsets) const
{
    if (!IsConnected() || offsets.empty())
    {
        return 0;
    }

    uint64_t currentAddress = baseAddress;

    // Follow each offset except the last one
    for (size_t i = 0; i < offsets.size() - 1; ++i)
    {
        currentAddress += offsets[i];
        
        // Read the pointer at this address
        uint64_t nextAddress = ReadPointer(currentAddress);
        if (nextAddress == 0)
        {
            std::cerr << "Failed to read pointer at offset " << i << " (0x" << std::hex << currentAddress << ")" << std::dec << std::endl;
            return 0;
        }
        
        currentAddress = nextAddress;
    }

    // Add the final offset
    currentAddress += offsets.back();
    
    return currentAddress;
}

uint64_t DMAManager::ReadPointer(uint64_t address) const
{
    uint64_t pointer = 0;
    if (ReadValue(address, pointer))
    {
        return pointer;
    }
    return 0;
}

size_t DMAManager::ReadMemoryEx(uint64_t address, void* buffer, size_t size, int retries) const
{
    if (!IsConnected() || !buffer || size == 0)
    {
        return 0;
    }

    size_t totalBytesRead = 0;
    uint8_t* buf = static_cast<uint8_t*>(buffer);

    for (int attempt = 0; attempt <= retries; ++attempt)
    {
        size_t bytesRead = ReadMemory(address + totalBytesRead, buf + totalBytesRead, size - totalBytesRead);
        totalBytesRead += bytesRead;

        if (totalBytesRead >= size)
        {
            break; // Successfully read all requested bytes
        }

        if (bytesRead == 0 && attempt < retries)
        {
            // Small delay before retry
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    return totalBytesRead;
}

bool DMAManager::ParseSignaturePattern(const std::string& pattern, std::vector<uint8_t>& bytes, std::string& mask) const
{
    bytes.clear();
    mask.clear();

    std::istringstream iss(pattern);
    std::string token;

    while (iss >> token)
    {
        if (token == "??" || token == "?")
        {
            // Wildcard byte
            bytes.push_back(0x00);
            mask += '?';
        }
        else
        {
            // Regular hex byte
            try
            {
                unsigned long value = std::stoul(token, nullptr, 16);
                if (value > 0xFF)
                {
                    return false; // Invalid byte value
                }
                bytes.push_back(static_cast<uint8_t>(value));
                mask += 'x';
            }
            catch (const std::exception&)
            {
                return false; // Invalid hex string
            }
        }
    }

    return !bytes.empty();
}

size_t DMAManager::FindPatternInBuffer(const uint8_t* buffer, size_t bufferSize, 
                                      const std::vector<uint8_t>& pattern, const std::string& mask) const
{
    if (!buffer || bufferSize == 0 || pattern.empty() || pattern.size() != mask.size())
    {
        return SIZE_MAX;
    }

    if (pattern.size() > bufferSize)
    {
        return SIZE_MAX;
    }

    for (size_t i = 0; i <= bufferSize - pattern.size(); ++i)
    {
        bool found = true;
        
        for (size_t j = 0; j < pattern.size(); ++j)
        {
            if (mask[j] == 'x' && buffer[i + j] != pattern[j])
            {
                found = false;
                break;
            }
        }
        
        if (found)
        {
            return i;
        }
    }

    return SIZE_MAX;
}

std::vector<UnrealSignature> DMAManager::GetUnrealSignatures() const
{
    std::vector<UnrealSignature> signatures;

    // GWorld signatures
    signatures.emplace_back("GWorld (Variant 1)",
        std::vector<uint8_t>{0x48, 0x89, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x00, 0x00, 0x00, 0xF6, 0x86, 0x3B, 0x01, 0x00, 0x00, 0x40},
        "xxx?????x???xxxxxxx", "GWorld");

    signatures.emplace_back("GWorld (Variant 2)",
        std::vector<uint8_t>{0x48, 0x89, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x00, 0x00, 0xF6, 0x86, 0x3B, 0x01, 0x00, 0x00, 0x40},
        "xxx?????x??xxxxxxx", "GWorld");

    signatures.emplace_back("GWorld (Variant 3)",
        std::vector<uint8_t>{0x48, 0x89, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF6, 0x86, 0x00, 0x01, 0x00, 0x00, 0x40},
        "xxx?????x?????xx?xxxx", "GWorld");

    signatures.emplace_back("GWorld (Variant 4)",
        std::vector<uint8_t>{0x00, 0x8B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x89, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        "?x???xx?xxx?????x???xx?????x?", "GWorld");

    signatures.emplace_back("GWorld (Variant 5)",
        std::vector<uint8_t>{0x48, 0x89, 0x05, 0x00, 0x00, 0x00, 0x02, 0x48, 0x8B, 0x8F, 0xA0, 0x00, 0x00, 0x00},
        "xxx???xxxxx???", "GWorld");

    signatures.emplace_back("GWorld (Variant 6)",
        std::vector<uint8_t>{0x48, 0x89, 0x05, 0x00, 0x00, 0x00, 0x00, 0x49, 0x8B, 0x00, 0x78, 0xF6, 0x00, 0x3B, 0x01, 0x00, 0x00, 0x40},
        "xxx????xx?xx?xx??x", "GWorld");

    signatures.emplace_back("GWorld (Variant 7)",
        std::vector<uint8_t>{0xE8, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x8B, 0x00, 0x78, 0x48, 0x89, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x00, 0x78},
        "x???x?x?xxxx?????x?x", "GWorld");

    signatures.emplace_back("GWorld (Variant 8)",
        std::vector<uint8_t>{0x48, 0x89, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x00, 0x88, 0x00, 0x00, 0x00, 0xF6, 0x00, 0x0B, 0x01, 0x00, 0x00, 0x40, 0x75, 0x00},
        "xxx?????x?x???x?xx??xx?", "GWorld");

    signatures.emplace_back("GWorld (Variant 9)",
        std::vector<uint8_t>{0x48, 0x8B, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x5C, 0x24, 0x00, 0x48, 0x8B, 0xC7},
        "xxx????xxxx?xxx", "GWorld");

    // GNames signatures
    signatures.emplace_back("GNames (Variant 1)",
        std::vector<uint8_t>{0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0xFE, 0xFF, 0x4C, 0x8B, 0xC0, 0xC6, 0x05, 0x00, 0x00, 0x00, 0x00, 0x01},
        "xxx????x??xxxxxxx????x", "GNames");

    signatures.emplace_back("GNames (Variant 2)",
        std::vector<uint8_t>{0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x03, 0xE8, 0x00, 0x00, 0xFF, 0xFF, 0x4C, 0x00, 0xC0},
        "xxx???xx??xxx?x", "GNames");

    signatures.emplace_back("GNames (Variant 3)",
        std::vector<uint8_t>{0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0xFF, 0xFF, 0x48, 0x8B, 0xD0, 0xC6, 0x05, 0x00, 0x00, 0x00, 0x00, 0x01},
        "xxx????x??xxxxxxx????x", "GNames");

    signatures.emplace_back("GNames (Variant 4)",
        std::vector<uint8_t>{0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x02, 0x48, 0x85, 0xC0, 0x75, 0x5F, 0xB9, 0x08, 0x08, 0x00},
        "xxx???xxxxxxxxx?", "GNames");

    // GObjects signatures
    signatures.emplace_back("GObjects (Variant 1)",
        std::vector<uint8_t>{0x4C, 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x99, 0x0F, 0xB7, 0xD2},
        "xxx????xxxx", "GObjects");

    signatures.emplace_back("GObjects (Variant 2)",
        std::vector<uint8_t>{0x4C, 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x41, 0x3B, 0xC0, 0x7D, 0x17},
        "xxx????xxxxx", "GObjects");

    signatures.emplace_back("GObjects (Variant 3)",
        std::vector<uint8_t>{0x4C, 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x04, 0x90, 0x0F, 0xB7, 0xC6, 0x8B, 0xD6},
        "xxx???xxxxxxx", "GObjects");

    signatures.emplace_back("GObjects (Variant 4)",
        std::vector<uint8_t>{0x4C, 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x04, 0x90, 0x0F, 0xB7, 0xC6, 0x8B, 0xD6},
        "xxx???xxxxxxx", "GObjects");

    signatures.emplace_back("GObjects (Variant 5)",
        std::vector<uint8_t>{0x4C, 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x8B, 0xD0, 0xC1, 0xEA, 0x10},
        "xxx????xxxxx", "GObjects");

    return signatures;
}

UnrealGlobals DMAManager::ScanUnrealGlobals() const
{
    UnrealGlobals globals;
    
    if (!IsConnected())
    {
        std::cerr << "Not connected to any process" << std::endl;
        return globals;
    }

    std::cout << "Scanning for Unreal Engine globals..." << std::endl;
    std::cout << "Current process: " << m_currentProcess.processName 
              << " (PID: " << m_currentProcess.processId << ")" << std::endl;
    std::cout << "Process base: " << FormatHexAddress(m_currentProcess.baseAddress) 
              << ", size: " << FormatHexAddress(m_currentProcess.imageSize) << std::endl;

    // Scan for each global type
    globals.GWorld = ScanUnrealGlobal("GWorld");
    globals.GNames = ScanUnrealGlobal("GNames");
    globals.GObjects = ScanUnrealGlobal("GObjects");

    // Log results
    std::cout << "Unreal Engine globals scan results:" << std::endl;
    std::cout << "  GWorld:   " << (globals.GWorld ? FormatHexAddress(globals.GWorld) : "Not found") << std::endl;
    std::cout << "  GNames:   " << (globals.GNames ? FormatHexAddress(globals.GNames) : "Not found") << std::endl;
    std::cout << "  GObjects: " << (globals.GObjects ? FormatHexAddress(globals.GObjects) : "Not found") << std::endl;

    return globals;
}

uint64_t DMAManager::ScanUnrealGlobal(const std::string& groupName) const
{
    if (!IsConnected())
    {
        return 0;
    }

    uint64_t mainModuleBase = GetMainModuleBase();
    if (mainModuleBase == 0)
    {
        std::cerr << "Failed to get main module base address" << std::endl;
        return 0;
    }

    size_t moduleSize = m_currentProcess.imageSize;
    if (moduleSize == 0)
    {
        moduleSize = 0x1000000; // Default 16MB
    }

    std::cout << "Scanning for " << groupName << " in main module (Base: " << FormatHexAddress(mainModuleBase) 
              << ", Size: " << FormatHexAddress(moduleSize) << ")..." << std::endl;

    // Try to read the entire module at once (like GSpots does)
    std::vector<uint8_t> moduleBuffer(moduleSize);
    size_t totalBytesRead = ReadMemoryEx(mainModuleBase, moduleBuffer.data(), moduleSize, 1);
    
    if (totalBytesRead == 0)
    {
        std::cout << "Failed to read module memory, trying chunked approach..." << std::endl;
        return ScanUnrealGlobalChunked(groupName, mainModuleBase, moduleSize);
    }

    std::cout << "Read " << totalBytesRead << " bytes from module memory (requested: " << moduleSize << ")" << std::endl;
    
    // Add debug output to show first few bytes of the module
    std::cout << "First 32 bytes of module: ";
    for (size_t i = 0; i < min(32ULL, totalBytesRead); ++i)
    {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)moduleBuffer[i] << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Validate PE header
    if (totalBytesRead >= 2)
    {
        uint16_t dosSignature = *reinterpret_cast<const uint16_t*>(moduleBuffer.data());
        if (dosSignature == 0x5A4D) // "MZ"
        {
            std::cout << "✓ Valid PE header detected (MZ signature found)" << std::endl;
        }
        else
        {
            std::cout << "✗ Invalid PE header - Expected 'MZ' (0x5A4D), got 0x" 
                      << std::hex << dosSignature << std::dec << std::endl;
            std::cout << "This indicates we're reading from wrong memory address!" << std::endl;
        }
    }

    // Get signatures for this group
    auto signatures = GetUnrealSignatures();
    
    // Try each signature for this group
    for (const auto& sig : signatures)
    {
        if (sig.group != groupName)
            continue;

        std::cout << "Trying " << sig.name << "..." << std::endl;

        size_t patternOffset = FindPatternInBuffer(moduleBuffer.data(), totalBytesRead, sig.pattern, sig.mask);
        if (patternOffset != SIZE_MAX)
        {
            std::cout << "Found " << sig.name << " at file offset: 0x" << std::hex << patternOffset << std::dec << std::endl;
            
            // Adjust offset for group-specific prefixes (like GSpots does)
            size_t adjustedOffset = AdjustFoundOffsetForGroup(moduleBuffer.data(), totalBytesRead, patternOffset, groupName);
            
            if (adjustedOffset + 7 > totalBytesRead)
            {
                std::cout << "Adjusted offset out of bounds, skipping..." << std::endl;
                continue;
            }

            /*
             * Find the correct "offset" because an absolute value tells us fuck-all.
             */
            
            // Calculate address like GSpots memory scanning does
            int32_t displacement = *reinterpret_cast<const int32_t*>(&moduleBuffer[adjustedOffset + 3]);
            size_t nextInstructionOffset = adjustedOffset + 7;
            
            // For memory scanning, calculate absolute address
            uint64_t nextInstructionAddress = mainModuleBase + nextInstructionOffset;
            uint64_t targetAddress = nextInstructionAddress + displacement;
            
            std::cout << "Instruction at offset: 0x" << std::hex << adjustedOffset << std::dec << std::endl;
            std::cout << "Displacement: 0x" << std::hex << displacement << std::dec << std::endl;
            std::cout << "Next instruction VA: " << FormatHexAddress(nextInstructionAddress) << std::endl;
            std::cout << "Calculated " << groupName << " address: " << FormatHexAddress(targetAddress) << std::endl;
            
            // Validate the address is reasonable
            if (targetAddress > 0x10000 && targetAddress < 0x7FFFFFFFFFFF)
            {
                return targetAddress;
            }
            else
            {
                std::cout << "Invalid target address, continuing search..." << std::endl;
            }
        }
    }

    std::cout << groupName << " not found" << std::endl;
    return 0;
}

size_t DMAManager::AdjustFoundOffsetForGroup(const uint8_t* buffer, size_t bufferSize, size_t foundOffset, const std::string& group) const
{
    std::vector<std::vector<uint8_t>> prefixes;
    
    if (group == "GWorld")
    {
        prefixes.push_back({0x48, 0x89, 0x05}); // mov [rip+disp], rax
    }
    else if (group == "GNames")
    {
        prefixes.push_back({0x48, 0x8D, 0x0D}); // lea rcx, [rip+disp] (UE <= 4.27)
        prefixes.push_back({0x48, 0x8B, 0x05}); // mov rax, [rip+disp] (UE > 4.27)
    }
    else if (group == "GObjects")
    {
        prefixes.push_back({0x4C, 0x8B, 0x0D}); // mov r9, [rip+disp]
    }
    else
    {
        return foundOffset;
    }

    // Search within 30 bytes around the found offset
    size_t searchLimit = 30;
    size_t startSearch = (foundOffset > searchLimit) ? foundOffset - searchLimit : 0;
    size_t endSearch = foundOffset + searchLimit;
    if (endSearch > bufferSize)
        endSearch = bufferSize;

    for (size_t i = startSearch; i <= endSearch; ++i)
    {
        for (const auto& prefix : prefixes)
        {
            if (i + prefix.size() > bufferSize)
                continue;

            bool match = true;
            for (size_t j = 0; j < prefix.size(); ++j)
            {
                if (buffer[i + j] != prefix[j])
                {
                    match = false;
                    break;
                }
            }

            if (match)
            {
                return i;
            }
        }
    }

    return foundOffset;
}

uint64_t DMAManager::CalculateRVAFromInstruction(const uint8_t* buffer, size_t bufferSize, size_t instructionOffset, uint64_t baseAddress) const
{
    // Ensure we have enough bytes for the instruction (3 bytes prefix + 4 bytes displacement)
    if (instructionOffset + 7 > bufferSize)
    {
        return 0;
    }

    // Read the 32-bit displacement from the instruction (offset +3 from instruction start)
    int32_t displacement = *reinterpret_cast<const int32_t*>(&buffer[instructionOffset + 3]);
    
    // Calculate the address relative to the next instruction (instruction + 7 bytes)
    uint64_t nextInstructionAddress = baseAddress + instructionOffset + 7;
    uint64_t targetAddress = nextInstructionAddress + displacement;

    // Validate the address is reasonable (not null, within expected range)
    if (targetAddress < 0x10000 || targetAddress > 0x7FFFFFFFFFFF)
    {
        return 0;
    }

    return targetAddress;
}

uint64_t DMAManager::ScanUnrealGlobalChunked(const std::string& groupName, uint64_t moduleBase, size_t moduleSize) const
{
    std::cout << "Using chunked memory scanning for " << groupName << "..." << std::endl;
    
    // Get signatures for this group
    auto signatures = GetUnrealSignatures();
    
    // Scan in chunks
    const size_t CHUNK_SIZE = 0x10000; // 64KB chunks
    std::vector<uint8_t> buffer(CHUNK_SIZE);

    for (size_t offset = 0; offset < moduleSize; offset += CHUNK_SIZE - 64) // Leave overlap for patterns
    {
        uint64_t currentAddress = moduleBase + offset;
        size_t currentChunkSize = min(CHUNK_SIZE, moduleSize - offset);

        size_t bytesRead = ReadMemory(currentAddress, buffer.data(), currentChunkSize);
        if (bytesRead == 0)
        {
            continue; // Skip unreadable memory regions
        }

        // Try each signature for this group
        for (const auto& sig : signatures)
        {
            if (sig.group != groupName)
                continue;

            size_t patternOffset = FindPatternInBuffer(buffer.data(), bytesRead, sig.pattern, sig.mask);
            if (patternOffset != SIZE_MAX)
            {
                std::cout << "Found " << sig.name << " at chunk offset: 0x" << std::hex << (offset + patternOffset) << std::dec << std::endl;
                
                // Adjust offset for group-specific prefixes
                size_t adjustedOffset = AdjustFoundOffsetForGroup(buffer.data(), bytesRead, patternOffset, groupName);
                
                if (adjustedOffset + 7 > bytesRead)
                {
                    continue; // Not enough bytes in this chunk
                }

                // Calculate address like GSpots memory scanning does
                int32_t displacement = *reinterpret_cast<const int32_t*>(&buffer[adjustedOffset + 3]);
                size_t nextInstructionOffset = offset + adjustedOffset + 7;
                
                // For memory scanning, calculate absolute address
                uint64_t nextInstructionAddress = moduleBase + nextInstructionOffset;
                uint64_t targetAddress = nextInstructionAddress + displacement;
                
                std::cout << "Chunked - Calculated " << groupName << " address: " << FormatHexAddress(targetAddress) << std::endl;
                
                // Validate the address is reasonable
                if (targetAddress > 0x10000 && targetAddress < 0x7FFFFFFFFFFF)
                {
                    return targetAddress;
                }
            }
        }
    }

    return 0; // Not found
}

void DMAManager::WorkerThread()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(m_taskQueueMutex);
        m_taskCondition.wait(lock, [this]() { return !m_taskQueue.empty() || m_shouldStop; });

        if (m_shouldStop)
        {
            break;
        }

        if (!m_taskQueue.empty())
        {
            auto task = m_taskQueue.front();
            m_taskQueue.pop();
            lock.unlock();

            try
            {
                task.task();
                m_pendingOperations--;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Exception in worker thread: " << e.what() << std::endl;
                m_pendingOperations--;
            }
        }
    }
}

void DMAManager::ProcessCompletedTasks()
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_completedCallbacks)
    {
        callback();
    }
    m_completedCallbacks.clear();
}

void DMAManager::AddAsyncTask(const AsyncTask& task)
{
    {
        std::lock_guard<std::mutex> lock(m_taskQueueMutex);
        m_taskQueue.push(task);
        m_pendingOperations++;
    }
    m_taskCondition.notify_one();
}

bool DMAManager::HasPendingOperations() const
{
    return m_pendingOperations > 0;
}

size_t DMAManager::GetPendingOperationCount() const
{
    return m_pendingOperations;
}

void DMAManager::CancelAllOperations()
{
    std::lock_guard<std::mutex> lock(m_taskQueueMutex);
    while (!m_taskQueue.empty())
    {
        m_taskQueue.pop();
        m_pendingOperations--;
    }
}

void DMAManager::AttachToProcessAsync(const std::string& processName, std::function<void(const AsyncResult<bool>&)> callback)
{
    std::string taskId = "attach_" + processName;
    std::cout << "Queuing async process attachment: " << processName << std::endl;
    
    AsyncTask task(AsyncTaskType::AttachToProcess, "Attaching to process: " + processName,
        [this, processName, callback]() {
            AsyncResult<bool> result;
            try
            {
                bool success = AttachToProcess(processName);
                result = AsyncResult<bool>(success, success, 
                    success ? "Successfully attached to " + processName : "",
                    success ? "" : "Failed to attach to " + processName);
            }
            catch (const std::exception& e)
            {
                result = AsyncResult<bool>(false, false, "", e.what());
            }
            
            if (callback)
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                m_completedCallbacks.push_back([callback, result]() { callback(result); });
            }
        }, taskId);
    
    AddAsyncTask(task);
}

void DMAManager::ScanUnrealGlobalsAsync(std::function<void(const AsyncResult<UnrealGlobals>&)> callback)
{
    std::cout << "Queuing async Unreal globals scan..." << std::endl;
    
    AsyncTask task(AsyncTaskType::ScanUnrealGlobals, "Scanning for Unreal Engine globals",
        [this, callback]() {
            AsyncResult<UnrealGlobals> result;
            try
            {
                UnrealGlobals globals = ScanUnrealGlobals();
                bool success = globals.IsValid();
                
                std::string logMsg;
                if (success)
                {
                    logMsg = "Unreal globals found - ";
                    if (globals.GWorld != 0) logMsg += "GWorld: " + FormatHexAddress(globals.GWorld) + " ";
                    if (globals.GNames != 0) logMsg += "GNames: " + FormatHexAddress(globals.GNames) + " ";
                    if (globals.GObjects != 0) logMsg += "GObjects: " + FormatHexAddress(globals.GObjects) + " ";
                }
                else
                {
                    logMsg = "No Unreal Engine globals found";
                }
                
                result = AsyncResult<UnrealGlobals>(globals, success, logMsg, 
                    success ? "" : "Failed to find any Unreal Engine globals");
            }
            catch (const std::exception& e)
            {
                result = AsyncResult<UnrealGlobals>(UnrealGlobals{}, false, "", e.what());
            }
            
            if (callback)
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                m_completedCallbacks.push_back([callback, result]() { callback(result); });
            }
        });
    
    AddAsyncTask(task);
}

void DMAManager::GetMainModuleBaseAsync(std::function<void(const AsyncResult<uint64_t>&)> callback)
{
    std::cout << "Queuing async main module base lookup..." << std::endl;
    
    AsyncTask task(AsyncTaskType::GetMainModuleBase, "Getting main module base address",
        [this, callback]() {
            AsyncResult<uint64_t> result;
            try
            {
                uint64_t baseAddress = GetMainModuleBase();
                bool success = baseAddress != 0;
                
                result = AsyncResult<uint64_t>(baseAddress, success,
                    success ? "Main module base: " + FormatHexAddress(baseAddress) : "",
                    success ? "" : "Failed to get main module base address");
            }
            catch (const std::exception& e)
            {
                result = AsyncResult<uint64_t>(0, false, "", e.what());
            }
            
            if (callback)
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                m_completedCallbacks.push_back([callback, result]() { callback(result); });
            }
        });
    
    AddAsyncTask(task);
}

void DMAManager::ScanSignatureAsync(const std::string& pattern, std::function<void(const AsyncResult<uint64_t>&)> callback)
{
    std::cout << "Queuing async signature scan: " << pattern << std::endl;
    
    AsyncTask task(AsyncTaskType::ScanSignature, "Scanning signature: " + pattern,
        [this, pattern, callback]() {
            AsyncResult<uint64_t> result;
            try
            {
                uint64_t address = ScanSignature(pattern);
                bool success = address != 0;
                
                result = AsyncResult<uint64_t>(address, success,
                    success ? "Signature found at: " + FormatHexAddress(address) : "",
                    success ? "" : "Signature not found: " + pattern);
            }
            catch (const std::exception& e)
            {
                result = AsyncResult<uint64_t>(0, false, "", e.what());
            }
            
            if (callback)
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                m_completedCallbacks.push_back([callback, result]() { callback(result); });
            }
        });
    
    AddAsyncTask(task);
} 