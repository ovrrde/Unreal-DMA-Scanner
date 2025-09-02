#pragma once

#include <string>
#include <vector>
#include <memory>

// Forward declarations
class DMAManager;

/**
 * @struct OffsetEntry
 * @brief Represents a single offset entry in the finder
 */
struct OffsetEntry
{
    std::string name;           ///< Name/description of the offset
    std::string address;        ///< Memory address (hex string)
    std::string value;          ///< Current value at address
    bool isValid;               ///< Whether this offset is currently valid
    
    OffsetEntry(const std::string& n = "", const std::string& addr = "", const std::string& val = "")
        : name(n), address(addr), value(val), isValid(false) {}
};

/**
 * @class UIManager
 * @brief Manages all user interface components and windows
 * 
 * This class handles the creation and management of all ImGUI windows,
 * including the main interface, offset tables, and configuration dialogs.
 */
class UIManager
{
public:
    /**
     * @brief Constructor
     */
    UIManager();

    /**
     * @brief Destructor
     */
    ~UIManager();

    /**
     * @brief Initialize the UI Manager
     * @return true if successful, false otherwise
     */
    bool Initialize();

    /**
     * @brief Update UI state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render all UI components
     */
    void Render();

    /**
     * @brief Shutdown and cleanup UI resources
     */
    void Shutdown();

    /**
     * @brief Set reference to DMA manager for UI callbacks
     * @param dmaManager Pointer to the DMA manager
     */
    void SetDMAManager(DMAManager* dmaManager);

    /**
     * @brief Remove an offset entry from the list
     * @param index Index of the entry to remove
     */
    void RemoveOffsetEntry(size_t index);

    /**
     * @brief Save current offsets to a text file
     * @param filename Filename to save to (default: "offsets.txt")
     * @return true if successful, false otherwise
     */
    bool SaveOffsets(const std::string& filename = "offsets.txt");

    /**
     * @brief Load offsets from a text file
     * @param filename Filename to load from (default: "offsets.txt")
     * @return true if successful, false otherwise
     */
    bool LoadOffsets(const std::string& filename = "offsets.txt");

private:
    /**
     * @brief Render the main menu bar
     */
    void RenderMenuBar();

    /**
     * @brief Render the main control panel
     */
    void RenderControlPanel();

    /**
     * @brief Render the offset finder window
     */
    void RenderOffsetFinder();

    /**
     * @brief Render the memory viewer window
     */
    void RenderMemoryViewer();

    /**
     * @brief Render the process selection window
     */
    void RenderProcessSelector();

    /**
     * @brief Render the status/log window
     */
    void RenderStatusWindow();

    /**
     * @brief Render the about dialog
     */
    void RenderAboutDialog();

    /**
     * @brief Add a new offset entry to the list
     */
    void AddOffsetEntry();

private:
    // Window visibility flags
    bool m_showControlPanel;        ///< Show main control panel
    bool m_showOffsetFinder;        ///< Show offset finder window
    bool m_showMemoryViewer;        ///< Show memory viewer window
    bool m_showProcessSelector;     ///< Show process selector window
    bool m_showStatusWindow;        ///< Show status/log window
    bool m_showAboutDialog;         ///< Show about dialog

    // UI State
    std::vector<OffsetEntry> m_offsetEntries;   ///< List of offset entries
    std::vector<std::string> m_logMessages;     ///< Log messages for status window
    std::string m_selectedProcess;              ///< Currently selected process
    float m_progressSpinner;                    ///< Spinner animation for async operations
    
    // Input buffers
    char m_processNameBuffer[256];              ///< Buffer for process name input
    char m_offsetNameBuffer[128];               ///< Buffer for offset name input
    char m_offsetAddressBuffer[32];             ///< Buffer for offset address input
    
    // References
    DMAManager* m_dmaManager;                   ///< Reference to DMA manager

    // UI Configuration
    static constexpr float PANEL_WIDTH = 400.0f;     ///< Default panel width
    static constexpr float PANEL_HEIGHT = 600.0f;    ///< Default panel height
}; 