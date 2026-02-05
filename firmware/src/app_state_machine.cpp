/**
 * @file app_state_machine.cpp
 * @brief Application state machine implementation (stub)
 */

#include "app_state_machine.h"

namespace OpenClaw {

// State class implementation
State::State(AppState id, const char* name)
    : id_(id),
      name_(name),
      entry_action_(nullptr),
      exit_action_(nullptr),
      update_action_(nullptr),
      timeout_ms_(0),
      timeout_state_(AppState::BOOT),
      entry_time_(0),
      last_activity_time_(0) {
}

void State::addTransition(const Transition& transition) {
    transitions_.push_back(transition);
}

void State::addTransition(AppEvent event, AppState target, 
                          GuardCondition guard, StateAction action) {
    transitions_.emplace_back(event, target, guard, action);
}

void State::setTimeout(uint32_t timeout_ms, AppState timeout_state) {
    timeout_ms_ = timeout_ms;
    timeout_state_ = timeout_state;
}

void State::onEntry() {
    entry_time_ = millis();
    last_activity_time_ = entry_time_;
    if (entry_action_) {
        entry_action_();
    }
}

void State::onExit() {
    if (exit_action_) {
        exit_action_();
    }
}

void State::onUpdate() {
    if (update_action_) {
        update_action_();
    }
}

bool State::handleEvent(const StateMachineEvent& event, AppState& out_target) {
    for (auto& t : transitions_) {
        if (t.event == event.type) {
            if (!t.guard || t.guard()) {
                if (t.action) {
                    t.action();
                }
                out_target = t.target_state;
                return true;
            }
        }
    }
    return false;
}

bool State::checkTimeout(uint32_t current_time, AppState& out_target) {
    if (timeout_ms_ > 0 && (current_time - entry_time_) >= timeout_ms_) {
        out_target = timeout_state_;
        return true;
    }
    return false;
}

void State::resetTimeout() {
    entry_time_ = millis();
}

uint32_t State::getTimeInState() const {
    return millis() - entry_time_;
}

// AppStateMachine implementation
AppStateMachine::AppStateMachine()
    : current_state_id_(AppState::BOOT),
      previous_state_id_(AppState::BOOT),
      current_state_(nullptr),
      transitioning_(false) {
}

AppStateMachine::~AppStateMachine() = default;

bool AppStateMachine::begin() {
    current_state_ = findState(current_state_id_);
    if (current_state_) {
        current_state_->onEntry();
    }
    return true;
}

void AppStateMachine::end() {
    if (current_state_) {
        current_state_->onExit();
    }
}

void AppStateMachine::update() {
    if (!current_state_ || transitioning_) return;
    
    current_state_->onUpdate();
    
    // Check timeout
    AppState timeout_target;
    if (current_state_->checkTimeout(millis(), timeout_target)) {
        transitionTo(timeout_target);
    }
    
    processEvents();
}

void AppStateMachine::addState(std::unique_ptr<State> state) {
    if (state) {
        states_[state->getId()] = std::move(state);
    }
}

State* AppStateMachine::getState(AppState id) {
    return findState(id);
}

bool AppStateMachine::transitionTo(AppState target) {
    if (transitioning_) return false;
    if (target == current_state_id_) return true;
    
    State* target_state = findState(target);
    if (!target_state) return false;
    
    transitioning_ = true;
    
    if (current_state_) {
        current_state_->onExit();
    }
    
    previous_state_id_ = current_state_id_;
    current_state_id_ = target;
    current_state_ = target_state;
    
    current_state_->onEntry();
    
    if (on_state_change_) {
        on_state_change_(previous_state_id_, current_state_id_);
    }
    
    transitioning_ = false;
    return true;
}

void AppStateMachine::postEvent(const StateMachineEvent& event) {
    event_queue_.push_back(event);
}

void AppStateMachine::postEvent(AppEvent type, void* data, size_t data_size) {
    event_queue_.emplace_back(type, data, data_size);
}

const char* AppStateMachine::getCurrentStateName() const {
    return current_state_ ? current_state_->getName() : "UNKNOWN";
}

uint32_t AppStateMachine::getTimeInCurrentState() const {
    return current_state_ ? current_state_->getTimeInState() : 0;
}

void AppStateMachine::forceTransition(AppState target) {
    transitionTo(target);
}

void AppStateMachine::processEvents() {
    while (!event_queue_.empty()) {
        StateMachineEvent event = event_queue_.front();
        event_queue_.erase(event_queue_.begin());
        
        AppState target;
        if (current_state_ && current_state_->handleEvent(event, target)) {
            transitionTo(target);
        }
    }
}

State* AppStateMachine::findState(AppState id) {
    auto it = states_.find(id);
    return (it != states_.end()) ? it->second.get() : nullptr;
}

const char* appStateToString(AppState state) {
    switch (state) {
        case AppState::BOOT: return "BOOT";
        case AppState::CONFIG_LOADING: return "CONFIG_LOADING";
        case AppState::CONFIG_ERROR_STATE: return "CONFIG_ERROR_STATE";
        case AppState::WIFI_CONNECTING: return "WIFI_CONNECTING";
        case AppState::WIFI_ERROR_STATE: return "WIFI_ERROR_STATE";
        case AppState::GATEWAY_CONNECTING: return "GATEWAY_CONNECTING";
        case AppState::GATEWAY_ERROR_STATE: return "GATEWAY_ERROR_STATE";
        case AppState::AUTHENTICATING: return "AUTHENTICATING";
        case AppState::READY: return "READY";
        case AppState::VOICE_INPUT: return "VOICE_INPUT";
        case AppState::TEXT_INPUT_STATE: return "TEXT_INPUT_STATE";
        case AppState::AI_PROCESSING: return "AI_PROCESSING";
        case AppState::AI_RESPONDING: return "AI_RESPONDING";
        case AppState::ANCIENT_MODE: return "ANCIENT_MODE";
        case AppState::ERROR_STATE: return "ERROR_STATE";
        case AppState::SHUTTING_DOWN: return "SHUTTING_DOWN";
        default: return "UNKNOWN";
    }
}

const char* appEventToString(AppEvent event) {
    switch (event) {
        case AppEvent::NONE: return "NONE";
        case AppEvent::BOOT_COMPLETE: return "BOOT_COMPLETE";
        case AppEvent::CONFIG_LOADED: return "CONFIG_LOADED";
        case AppEvent::CONFIG_ERROR: return "CONFIG_ERROR";
        case AppEvent::WIFI_CONNECTED: return "WIFI_CONNECTED";
        case AppEvent::WIFI_DISCONNECTED: return "WIFI_DISCONNECTED";
        case AppEvent::WIFI_ERROR: return "WIFI_ERROR";
        case AppEvent::GATEWAY_CONNECTED: return "GATEWAY_CONNECTED";
        case AppEvent::GATEWAY_DISCONNECTED: return "GATEWAY_DISCONNECTED";
        case AppEvent::GATEWAY_ERROR: return "GATEWAY_ERROR";
        case AppEvent::AUTHENTICATED: return "AUTHENTICATED";
        case AppEvent::AUTH_FAILED: return "AUTH_FAILED";
        case AppEvent::KEY_PRESSED: return "KEY_PRESSED";
        case AppEvent::TEXT_INPUT: return "TEXT_INPUT";
        case AppEvent::TEXT_SUBMITTED: return "TEXT_SUBMITTED";
        case AppEvent::VOICE_KEY_PRESSED: return "VOICE_KEY_PRESSED";
        case AppEvent::VOICE_STARTED: return "VOICE_STARTED";
        case AppEvent::VOICE_STOPPED: return "VOICE_STOPPED";
        case AppEvent::VOICE_DETECTED: return "VOICE_DETECTED";
        case AppEvent::VOICE_LOST: return "VOICE_LOST";
        case AppEvent::AI_THINKING: return "AI_THINKING";
        case AppEvent::AI_RESPONSE_CHUNK: return "AI_RESPONSE_CHUNK";
        case AppEvent::AI_RESPONSE_COMPLETE: return "AI_RESPONSE_COMPLETE";
        case AppEvent::AI_ERROR: return "AI_ERROR";
        case AppEvent::AUDIO_STARTED: return "AUDIO_STARTED";
        case AppEvent::AUDIO_STOPPED: return "AUDIO_STOPPED";
        case AppEvent::AUDIO_ERROR: return "AUDIO_ERROR";
        case AppEvent::ANCIENT_MODE_TRIGGER: return "ANCIENT_MODE_TRIGGER";
        case AppEvent::ERROR_RECOVERED: return "ERROR_RECOVERED";
        case AppEvent::TIMEOUT: return "TIMEOUT";
        case AppEvent::USER_ACTIVITY: return "USER_ACTIVITY";
        case AppEvent::FORCE_RECONNECT: return "FORCE_RECONNECT";
        case AppEvent::SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

} // namespace OpenClaw
