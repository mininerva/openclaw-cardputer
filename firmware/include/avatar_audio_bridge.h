/**
 * @file avatar_audio_bridge.h
 * @brief Bridge between audio capture and avatar lip-sync
 * 
 * Connects audio streamer voice activity to avatar beak animation.
 */

#ifndef AVATAR_AUDIO_BRIDGE_H
#define AVATAR_AUDIO_BRIDGE_H

#include "audio_streamer.h"
#include "avatar/procedural_avatar.h"

namespace OpenClaw {

/**
 * @brief Bridge audio events to avatar lip-sync
 * 
 * Usage:
 *   AvatarAudioBridge bridge;
 *   bridge.begin(&audioStreamer, &avatar);
 */
class AvatarAudioBridge {
public:
    AvatarAudioBridge();
    
    /**
     * @brief Initialize bridge
     * @param audio Audio streamer instance
     * @param avatar Avatar instance
     */
    void begin(AudioStreamer* audio, Avatar::ProceduralAvatar* avatar);
    
    /**
     * @brief Update bridge (call in main loop)
     */
    void update();
    
    /**
     * @brief Check if currently "speaking" (voice detected)
     */
    bool isSpeaking() const { return is_speaking_; }

private:
    AudioStreamer* audio_;
    Avatar::ProceduralAvatar* avatar_;
    bool is_speaking_;
    uint32_t last_voice_time_;
    
    static void onAudioEvent(AudioEvent event, const void* data);
    static AvatarAudioBridge* instance_;
};

} // namespace OpenClaw

#endif // AVATAR_AUDIO_BRIDGE_H
