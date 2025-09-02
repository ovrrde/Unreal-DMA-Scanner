#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <future>

/**
 * @struct UnrealSignature
 * @brief Structure for Unreal Engine signature definitions
 */
struct UnrealSignature
{
    std::string name;               ///< Name/description of the signature
    std::vector<uint8_t> pattern;  ///< Byte pattern to search for
    std::string mask;               ///< Pattern mask ('x' = exact match, '?' = wildcard)
    std::string group;              ///< Group category (GWorld, GNames, GObjects)
    
    UnrealSignature(const std::string& n, const std::vector<uint8_t>& p, const std::string& m, const std::string& g)
        : name(n), pattern(p), mask(m), group(g) {}
};

/**
 * @struct UnrealGlobals
 * @brief Structure to hold found Unreal Engine global addresses
 */
struct UnrealGlobals
{
    uint64_t GWorld = 0;    ///< GWorld global address
    uint64_t GNames = 0;    ///< GNames global address  
    uint64_t GObjects = 0;  ///< GObjects global address
    
    bool IsValid() const { return GWorld != 0 || GNames != 0 || GObjects != 0; }
};

/**
 * @struct ProcessInfo
 * @brief Contains information about a target process
 */
struct ProcessInfo
{
    uint32_t processId;         ///< Process ID
    std::string processName;    ///< Process executable name
    uint64_t baseAddress;       ///< Base address of the process
    uint64_t imageSize;         ///< Size of the process image
    
    ProcessInfo(uint32_t pid = 0, const std::string& name = "", 
                uint64_t base = 0, uint64_t size = 0)
        : processId(pid), processName(name), baseAddress(base), imageSize(size) {}
};

/**
 * @struct AsyncResult
 * @brief Result container for async DMA operations
 */
template<typename T>
struct AsyncResult
{
    bool isComplete = false;
    bool isSuccess = false;
    T result;
    std::string errorMessage;
    std::string logMessage;
    
    AsyncResult() = default;
    AsyncResult(const T& res, bool success = true, const std::string& log = "", const std::string& error = "")
        : result(res), isSuccess(success), logMessage(log), errorMessage(error), isComplete(true) {}
};

/**
 * @enum AsyncTaskType
 * @brief Types of async DMA tasks
 */
enum class AsyncTaskType
{
    ScanUnrealGlobals,
    GetMainModuleBase,
    ScanSignature,
    AttachToProcess,
    DetachFromProcess
};

/**
 * @struct AsyncTask
 * @brief Container for async DMA task information
 */
struct AsyncTask
{
    AsyncTaskType type;
    std::string description;
    std::function<void()> task;
    std::string taskId;
    
    AsyncTask(AsyncTaskType t, const std::string& desc, std::function<void()> taskFunc, const std::string& id = "")
        : type(t), description(desc), task(taskFunc), taskId(id) {}
};

/**
 * @class DMAManager
 * @brief Manages Direct Memory Access operations using LeechCore
 * 
 * This class provides an interface for DMA operations including process
 * attachment, memory reading/writing, and process enumeration.
 * Now includes async operation support to prevent UI blocking.
 */
class DMAManager
{
public:
    /**
     * @brief Constructor
     */
    DMAManager();

    /**
     * @brief Destructor
     */
    ~DMAManager();

    /**
     * @brief Initialize the DMA Manager and LeechCore
     * @return true if initialization successful, false otherwise
     */
    bool Initialize();

    /**
     * @brief Update DMA state (called each frame)
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Shutdown and cleanup DMA resources
     */
    void Shutdown();

    /**
     * @brief Check if DMA is currently connected and operational
     * @return true if connected, false otherwise
     */
    bool IsConnected() const;

    /**
     * @brief Attach to a target process by name
     * @param processName Name of the process executable
     * @return true if successfully attached, false otherwise
     */
    bool AttachToProcess(const std::string& processName);

    /**
     * @brief Attach to a target process by PID
     * @param processId Process ID to attach to
     * @return true if successfully attached, false otherwise
     */
    bool AttachToProcess(uint32_t processId);

    /**
     * @brief Detach from the current target process
     */
    void DetachFromProcess();

    /**
     * @brief Get information about the currently attached process
     * @return ProcessInfo structure with current process details
     */
    ProcessInfo GetCurrentProcessInfo() const;

    /**
     * @brief Get a list of all available processes
     * @return Vector of ProcessInfo structures
     */
    std::vector<ProcessInfo> GetProcessList() const;

    /**
     * @brief Read memory from the target process
     * @param address Virtual address to read from
     * @param buffer Buffer to store the read data
     * @param size Number of bytes to read
     * @return Number of bytes actually read, 0 on failure
     */
    size_t ReadMemory(uint64_t address, void* buffer, size_t size) const;

    /**
     * @brief Write memory to the target process
     * @param address Virtual address to write to
     * @param buffer Buffer containing data to write
     * @param size Number of bytes to write
     * @return Number of bytes actually written, 0 on failure
     */
    size_t WriteMemory(uint64_t address, const void* buffer, size_t size) const;

    /**
     * @brief Read a value of specific type from memory
     * @tparam T Type of value to read
     * @param address Virtual address to read from
     * @param value Reference to store the read value
     * @return true if successful, false otherwise
     */
    template<typename T>
    bool ReadValue(uint64_t address, T& value) const
    {
        return ReadMemory(address, &value, sizeof(T)) == sizeof(T);
    }

    /**
     * @brief Write a value of specific type to memory
     * @tparam T Type of value to write
     * @param address Virtual address to write to
     * @param value Value to write
     * @return true if successful, false otherwise
     */
    template<typename T>
    bool WriteValue(uint64_t address, const T& value) const
    {
        return WriteMemory(address, &value, sizeof(T)) == sizeof(T);
    }

    /**
     * @brief Read a null-terminated string from memory
     * @param address Virtual address to read from
     * @param maxLength Maximum length of string to read
     * @return String read from memory, empty if failed
     */
    std::string ReadString(uint64_t address, size_t maxLength = 256) const;

    /**
     * @brief Parse hex string to address
     * @param hexString Hex string (e.g., "0x12345678" or "12345678")
     * @return Parsed address, 0 if invalid
     */
    static uint64_t ParseHexAddress(const std::string& hexString);

    /**
     * @brief Format address as hex string
     * @param address Address to format
     * @param uppercase Whether to use uppercase hex digits
     * @return Formatted hex string
     */
    static std::string FormatHexAddress(uint64_t address, bool uppercase = true);

    /**
     * @brief Get the base address of a specific module
     * @param moduleName Name of the module (e.g., "ntdll.dll", "kernel32.dll")
     * @return Base address of the module, 0 if not found
     */
    uint64_t GetModuleBase(const std::string& moduleName) const;

    /**
     * @brief Get the main module (executable) base address
     * @return Base address of the main executable module
     */
    uint64_t GetMainModuleBase() const;

    /**
     * @brief Get list of all loaded modules in the target process
     * @return Vector of module information structures
     */
    std::vector<ProcessInfo> GetModuleList() const;

    /**
     * @brief Scan for a byte pattern (signature) in process memory
     * @param pattern Byte pattern to search for (e.g., "48 8B 05 ?? ?? ?? ??")
     * @param startAddress Starting address for the scan
     * @param scanSize Size of memory region to scan
     * @return Address where pattern was found, 0 if not found
     */
    uint64_t ScanSignature(const std::string& pattern, uint64_t startAddress = 0, size_t scanSize = 0) const;

    /**
     * @brief Scan for a byte pattern within a specific module
     * @param pattern Byte pattern to search for
     * @param moduleName Name of the module to scan within
     * @return Address where pattern was found, 0 if not found
     */
    uint64_t ScanSignatureInModule(const std::string& pattern, const std::string& moduleName) const;

    /**
     * @brief Follow a multi-level pointer chain
     * @param baseAddress Starting address
     * @param offsets Vector of offsets to follow
     * @return Final address after following all offsets, 0 if any read fails
     */
    uint64_t ReadMultiLevelPointer(uint64_t baseAddress, const std::vector<uint64_t>& offsets) const;

    /**
     * @brief Read a pointer (64-bit address) from memory
     * @param address Address to read pointer from
     * @return Pointer value, 0 if read fails
     */
    uint64_t ReadPointer(uint64_t address) const;

    /**
     * @brief Read memory with automatic retry on partial reads
     * @param address Virtual address to read from
     * @param buffer Buffer to store the read data
     * @param size Number of bytes to read
     * @param retries Number of retry attempts
     * @return Number of bytes actually read, 0 on complete failure
     */
    size_t ReadMemoryEx(uint64_t address, void* buffer, size_t size, int retries = 3) const;

    /**
     * @brief Scan for Unreal Engine globals (GWorld, GNames, GObjects)
     * @return UnrealGlobals structure with found addresses
     */
    UnrealGlobals ScanUnrealGlobals() const;

    /**
     * @brief Scan for a specific Unreal Engine global by group name
     * @param groupName Group to scan for ("GWorld", "GNames", "GObjects")
     * @return Address of the global, 0 if not found
     */
    uint64_t ScanUnrealGlobal(const std::string& groupName) const;

    /**
     * @brief Scan for Unreal Engine global using chunked memory reading (fallback method)
     * @param groupName Group to scan for ("GWorld", "GNames", "GObjects")
     * @param moduleBase Base address of the module
     * @param moduleSize Size of the module
     * @return Address of the global, 0 if not found
     */
    uint64_t ScanUnrealGlobalChunked(const std::string& groupName, uint64_t moduleBase, size_t moduleSize) const;

    /**
     * @brief Get all predefined Unreal Engine signatures
     * @return Vector of UnrealSignature structures
     */
    std::vector<UnrealSignature> GetUnrealSignatures() const;

    // Async methods (new)
    /**
     * @brief Attach to a process asynchronously
     * @param processName Name of the process
     * @param callback Callback function for result
     */
    void AttachToProcessAsync(const std::string& processName, std::function<void(const AsyncResult<bool>&)> callback = nullptr);

    /**
     * @brief Scan for Unreal Engine globals asynchronously
     * @param callback Callback function for result
     */
    void ScanUnrealGlobalsAsync(std::function<void(const AsyncResult<UnrealGlobals>&)> callback = nullptr);

    /**
     * @brief Get main module base asynchronously
     * @param callback Callback function for result
     */
    void GetMainModuleBaseAsync(std::function<void(const AsyncResult<uint64_t>&)> callback = nullptr);

    /**
     * @brief Scan signature asynchronously
     * @param pattern Signature pattern to scan for
     * @param callback Callback function for result
     */
    void ScanSignatureAsync(const std::string& pattern, std::function<void(const AsyncResult<uint64_t>&)> callback = nullptr);

    /**
     * @brief Check if there are any pending async operations
     * @return true if operations are pending
     */
    bool HasPendingOperations() const;

    /**
     * @brief Get the number of pending operations
     * @return Number of pending operations
     */
    size_t GetPendingOperationCount() const;

    /**
     * @brief Cancel all pending operations
     */
    void CancelAllOperations();

private:
    // Threading methods
    /**
     * @brief Worker thread function
     */
    void WorkerThread();

    /**
     * @brief Add a task to the async queue
     * @param task Task to add
     */
    void AddAsyncTask(const AsyncTask& task);

    /**
     * @brief Process completed async tasks and call callbacks
     */
    void ProcessCompletedTasks();

    /**
     * @brief Initialize LeechCore device
     * @return true if successful, false otherwise
     */
    bool InitializeDevice();

    /**
     * @brief Cleanup LeechCore resources
     */
    void CleanupDevice();

    /**
     * @brief Refresh the process list
     */
    void RefreshProcessList();

    /**
     * @brief Check if memory map file exists
     * @return true if mmap.txt exists, false otherwise
     */
    bool CheckMemoryMapExists();

    /**
     * @brief Initialize VMMDLL with specified arguments
     * @param useMemoryMap Whether to use memory map
     * @return true if successful, false otherwise
     */
    bool InitializeWithArgs(bool useMemoryMap);

    /**
     * @brief Parse a signature pattern string into bytes and wildcards
     * @param pattern Pattern string (e.g., "48 8B 05 ?? ?? ?? ??")
     * @param bytes Output vector for pattern bytes
     * @param mask Output string for pattern mask ('x' = match, '?' = wildcard)
     * @return true if pattern is valid, false otherwise
     */
    bool ParseSignaturePattern(const std::string& pattern, std::vector<uint8_t>& bytes, std::string& mask) const;

    /**
     * @brief Search for a pattern in a memory buffer
     * @param buffer Memory buffer to search in
     * @param bufferSize Size of the buffer
     * @param pattern Pattern bytes to search for
     * @param mask Pattern mask ('x' = match, '?' = wildcard)
     * @return Offset within buffer where pattern was found, SIZE_MAX if not found
     */
    size_t FindPatternInBuffer(const uint8_t* buffer, size_t bufferSize, 
                              const std::vector<uint8_t>& pattern, const std::string& mask) const;

    /**
     * @brief Adjust found offset based on group-specific instruction prefixes
     * @param buffer Memory buffer containing the found pattern
     * @param foundOffset Offset where pattern was found
     * @param group Unreal Engine group name (GWorld, GNames, GObjects)
     * @return Adjusted offset pointing to the correct instruction
     */
    size_t AdjustFoundOffsetForGroup(const uint8_t* buffer, size_t bufferSize, size_t foundOffset, const std::string& group) const;

    /**
     * @brief Calculate RVA from instruction offset
     * @param buffer Memory buffer
     * @param instructionOffset Offset of the instruction (e.g., mov, lea)
     * @param baseAddress Base address of the module
     * @return Calculated RVA address, 0 if calculation fails
     */
    uint64_t CalculateRVAFromInstruction(const uint8_t* buffer, size_t bufferSize, size_t instructionOffset, uint64_t baseAddress) const;

private:
    // LeechCore handles (using void* to avoid including headers here)
    void* m_hLeechCore;             ///< LeechCore device handle
    void* m_hVMM;                   ///< VMM handle
    
    // Current state
    std::atomic<bool> m_isInitialized;      ///< Whether DMA is initialized
    std::atomic<bool> m_isConnected;        ///< Whether DMA is connected to target
    ProcessInfo m_currentProcess;           ///< Currently attached process info
    std::atomic<uint32_t> m_currentProcessId;   ///< Currently attached process ID
    
    // Process management
    std::vector<ProcessInfo> m_processList;  ///< Cached process list
    float m_processRefreshTimer;             ///< Timer for auto-refreshing process list
    
    // Threading
    std::thread m_workerThread;                 ///< Background worker thread
    std::atomic<bool> m_shouldStop;             ///< Flag to stop worker thread
    std::mutex m_taskQueueMutex;                ///< Mutex for task queue
    std::condition_variable m_taskCondition;    ///< Condition variable for task queue
    std::queue<AsyncTask> m_taskQueue;          ///< Queue of pending async tasks
    std::atomic<size_t> m_pendingOperations;    ///< Number of pending operations
    
    // Callback storage
    std::mutex m_callbackMutex;                 ///< Mutex for callback access
    std::vector<std::function<void()>> m_completedCallbacks;  ///< Callbacks to execute on main thread
    
    // Configuration
    static constexpr float PROCESS_REFRESH_INTERVAL = 5.0f;  ///< Process list refresh interval (seconds)
}; 