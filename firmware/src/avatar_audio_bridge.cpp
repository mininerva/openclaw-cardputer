/**
 * @file avatar_audio_bridge.cpp
 * @brief Audio-to-avatar bridge implementation
 */

#include "avatar_audio_bridge.h"

namespace OpenClaw {

AvatarAudioBridge* AvatarAudioBridge::instance_ = nullptr;

AvatarAudioBridge::AvatarAudioBridge()
    : audio_(nullptr),
      avatar_(nullptr),
      is_speaking_(false),
      last_voice_time_(0) {
    instance_ = this;
}

void AvatarAudioBridge::begin(AudioStreamer* audio, Avatar::ProceduralAvatar* avatar) {
    audio_ = audio;
    avatar_ = avatar;
    is_speaking_ = false;
    
    // Register for audio events
    if (audio_) {
        audio_->onEvent(onAudioEvent);
    }
}

void AvatarAudioBridge::update() {
    if (!avatar_) return;
    
    // If voice detected recently, animate beak
    if (is_speaking_) {
        uint32_t now = millis();
        
        // Simple pulsing animation while speaking
        float pulse = (sinf(now * 0.02f) + 1.0f) * 0.5f;  // 0-1 sine wave
        float openness = 0.3f + (pulse * 0.4f);  // 0.3 to 0.7 range
        
        // Update avatar beak
        avatar_->speak("~");  // Tilde triggers speaking mode without actual TTS
        
        // Auto-stop if no voice for 500ms
        if (now - last_voice_time_ > 500) {
            is_speaking_ = false;
            avatar_->stopSpeaking();
        }
    }
}

void AvatarAudioBridge::onAudioEvent(AudioEvent event, const void* data) {
    if (!instance_) return;
    
    switch (event) {
        case AudioEvent::VOICE_DETECTED:
            instance_->is_speaking_ = true;
            instance_->last_voice_time_ = millis();
            break;
            
        case AudioEvent::VOICE_LOST:
            // Don't immediately stop - let update() handle timeout
            // for smoother animation
            break;
            
        case AudioEvent::FRAME_CAPTURED:
            // Update timing while voice continues
            if (instance_->is_speaking_) {
                instance_->last_voice_time_ = millis();
            }
            break;
            
        default:
            break;
    }
}

} // namespace OpenClaw
