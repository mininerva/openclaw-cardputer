/**
 * @file app_state_machine.h
 * @brief Application state machine for OpenClaw Cardputer
 * 
 * Features:
 * - Hierarchical state machine
 * - Event-driven transitions
 * - State entry/exit actions
 * - Timeout handling
 * - History state support
 */

#ifndef OPENCLAW_APP_STATE_MACHINE_H
#define OPENCLAW_APP_STATE_MACHINE_H

#include <Arduino.h>
#include <functional>
#include <vector>
#include <map>
#include <memory>
#include <string>

namespace OpenClaw {

// Forward declarations
class AppStateMachine;
class State;

// Event types
enum class AppEvent : uint8_t {
    // System events
    NONE = 0,
    BOOT_COMPLETE,
    CONFIG_LOADED,
    CONFIG_ERROR,
    
    // Connection events
    WIFI_CONNECTED,
    WIFI_DISCONNECTED,
    WIFI_ERROR,
    GATEWAY_CONNECTED,
    GATEWAY_DISCONNECTED,
    GATEWAY_ERROR,
    AUTHENTICATED,
    AUTH_FAILED,
    
    // Input events
    KEY_PRESSED,
    TEXT_INPUT,
    TEXT_SUBMITTED,
    VOICE_KEY_PRESSED,
    VOICE_STARTED,
    VOICE_STOPPED,
    VOICE_DETECTED,
    VOICE_LOST,
    
    // AI events
    AI_THINKING,
    AI_RESPONSE_CHUNK,
    AI_RESPONSE_COMPLETE,
    AI_ERROR,
    
    // Audio events
    AUDIO_STARTED,
    AUDIO_STOPPED,
    AUDIO_ERROR,
    
    // Special events
    ANCIENT_MODE_TRIGGER,
    ERROR_RECOVERED,
    TIMEOUT,
    USER_ACTIVITY,
    
    // Control events
    FORCE_RECONNECT,
    SHUTDOWN
};

// Application states
enum class AppState : uint8_t {
    BOOT,
    CONFIG_LOADING,
    CONFIG_ERROR_STATE,
    WIFI_CONNECTING,
    WIFI_ERROR_STATE,
    GATEWAY_CONNECTING,
    GATEWAY_ERROR_STATE,
    AUTHENTICATING,
    READY,
    VOICE_INPUT,
    TEXT_INPUT_STATE,
    AI_PROCESSING,
    AI_RESPONDING,
    ANCIENT_MODE,
    ERROR_STATE,
    SHUTTING_DOWN
};

// Event structure
struct StateMachineEvent {
    AppEvent type;
    void* data;
    size_t data_size;
    uint32_t timestamp;
    
    StateMachineEvent()
        : type(AppEvent::NONE), data(nullptr), data_size(0), timestamp(0) {}
    
    StateMachineEvent(AppEvent t, void* d = nullptr, size_t ds = 0)
        : type(t), data(d), data_size(ds), timestamp(millis()) {}
};

// State action callbacks
using StateAction = std::function<void()>;
using StateEventHandler = std::function<bool(const StateMachineEvent& event)>;
using GuardCondition = std::function<bool()>;

// Transition definition
struct Transition {
    AppEvent event;
    AppState target_state;
    GuardCondition guard;
    StateAction action;
    
    Transition(AppEvent e, AppState target, GuardCondition g = nullptr, StateAction a = nullptr)
        : event(e), target_state(target), guard(g), action(a) {}
};

// State definition
class State {
public:
    State(AppState id, const char* name);
    
    // Set actions
    void setEntryAction(StateAction action) { entry_action_ = action; }
    void setExitAction(StateAction action) { exit_action_ = action; }
    void setUpdateAction(StateAction action) { update_action_ = action; }
    
    // Add transition
    void addTransition(const Transition& transition);
    void addTransition(AppEvent event, AppState target, 
                       GuardCondition guard = nullptr, StateAction action = nullptr);
    
    // Set timeout
    void setTimeout(uint32_t timeout_ms, AppState timeout_state);
    void setRetryDelay(uint32_t delay_ms);
    
    // Getters
    AppState getId() const { return id_; }
    const char* getName() const { return name_; }
    
    // Execute actions
    void onEntry();
    void onExit();
    void onUpdate();
    
    // Handle event
    bool handleEvent(const StateMachineEvent& event, AppState& out_target);
    
    // Check timeout
    bool checkTimeout(uint32_t current_time, AppState& out_target);
    void resetTimeout();
    
    // Get time in state
    uint32_t getTimeInState() const;

private:
    AppState id_;
    const char* name_;
    
    StateAction entry_action_;
    StateAction exit_action_;
    StateAction update_action_;
    
    std::vector<Transition> transitions_;
    
    uint32_t timeout_ms_;
    AppState timeout_state_;
    uint32_t retry_delay_ms_;
    uint32_t entry_time_;
    uint32_t last_activity_time_;
};

// State machine class
class AppStateMachine {
public:
    AppStateMachine();
    ~AppStateMachine();
    
    // Disable copy
    AppStateMachine(const AppStateMachine&) = delete;
    AppStateMachine& operator=(const AppStateMachine&) = delete;
    
    // Initialize
    bool begin();
    
    // Update (call in main loop)
    void update();
    
    // Deinitialize
    void end();
    
    // Add state
    void addState(std::unique_ptr<State> state);
    State* getState(AppState id);
    
    // Transition to state
    bool transitionTo(AppState target);
    
    // Post event
    void postEvent(const StateMachineEvent& event);
    void postEvent(AppEvent type, void* data = nullptr, size_t data_size = 0);
    
    // Get current state
    AppState getCurrentState() const { return current_state_id_; }
    const char* getCurrentStateName() const;
    uint32_t getTimeInCurrentState() const;
    
    // Check if in state
    bool isInState(AppState state) const { return current_state_id_ == state; }
    
    // Set global actions
    void setOnStateChange(std::function<void(AppState from, AppState to)> callback) {
        on_state_change_ = callback;
    }
    
    // Get state history
    AppState getPreviousState() const { return previous_state_id_; }
    
    // Force transition (bypass guards)
    void forceTransition(AppState target);

private:
    std::map<AppState, std::unique_ptr<State>> states_;
    AppState current_state_id_;
    AppState previous_state_id_;
    State* current_state_;
    
    std::vector<StateMachineEvent> event_queue_;
    
    std::function<void(AppState from, AppState to)> on_state_change_;
    
    bool transitioning_;
    
    // Private methods
    void processEvents();
    void executeTransition(State* target, const StateMachineEvent* trigger_event);
    State* findState(AppState id);
};

// Application context
struct AppContext {
    // Configuration
    struct {
        char wifi_ssid[64];
        char wifi_password[64];
        char gateway_url[128];
        char device_id[32];
        char device_name[32];
        char api_key[64];
    } config;
    
    // State
    struct {
        bool wifi_connected;
        bool gateway_connected;
        bool authenticated;
        AppState current_state;
    } state;
    
    // Statistics
    struct {
        uint32_t messages_sent;
        uint32_t messages_received;
        uint32_t uptime_seconds;
        uint32_t reconnect_count;
    } stats;
    
    // Input
    struct {
        char text_buffer[256];
        bool voice_active;
        float audio_level;
    } input;
    
    AppContext() {
        memset(&config, 0, sizeof(config));
        memset(&state, 0, sizeof(state));
        memset(&stats, 0, sizeof(stats));
        memset(&input, 0, sizeof(input));
    }
};

// Utility functions
const char* appStateToString(AppState state);
const char* appEventToString(AppEvent event);

} // namespace OpenClaw

#endif // OPENCLAW_APP_STATE_MACHINE_H
