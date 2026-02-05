/**
 * @file voice_synthesis.h
 * @brief On-device TTS with multiple voice personas
 * 
 * Lightweight text-to-speech for ESP32 with lip-sync support.
 */

#ifndef VOICE_SYNTHESIS_H
#define VOICE_SYNTHESIS_H

#include <Arduino.h>
#include "avatar/procedural_avatar.h"

namespace Avatar {

// =============================================================================
// Voice Personas
// =============================================================================

enum class VoicePersona {
    PERSONA_DEFAULT,      // Crisp, slightly condescending
    PERSONA_ANCIENT,       // Reverberating, archaic diction
    PERSONA_WHISPER,       // Sub-1kHz intimate mode
    PERSONA_BROADCAST,      // Loud, confident
    PERSONA_SILENT          // TTS disabled (gateway only)
};

// =============================================================================
// Phoneme/Timing for Lip-Sync
// =============================================================================

enum class Phoneme {
    SILENCE,
    A, E, I, O, U,        // Vowels
    B, D, F, G, H, J, K, L, M, N,
    P, R, S, T, V, W, Y, Z, // Consonants
    TH, DH, NG, CH, SH,    // Digraphs
    PAUSE,                 // Comma/pause
    END                      // Sentence end
};

struct PhonemeTiming {
    Phoneme type;
    uint16_t duration_ms;  // How long to hold
    uint8_t beak_openness;  // 0-1 beak opening
    uint8_t pitch_mult;     // 0-1 pitch multiplier
};

// =============================================================================
// TTS Configuration
// =============================================================================

struct TTSConfig {
    VoicePersona persona;
    uint8_t volume;         // 0-255
    uint16_t sample_rate;    // 8000, 16000, 22050
    uint8_t quality;        // 1-3 (quality vs speed)
    bool lip_sync;         // Enable beak animation
    bool echo_enabled;      // For Ancient persona
    uint8_t echo_delay;     // Echo delay in samples
    
    TTSConfig() 
        : persona(VoicePersona::PERSONA_DEFAULT),
          volume(128),
          sample_rate(16000),
          quality(2),
          lip_sync(true),
          echo_enabled(false),
          echo_delay(0) {}
};

// =============================================================================
// Voice Synthesis Manager
// =============================================================================

class VoiceSynthesis {
public:
    VoiceSynthesis();
    ~VoiceSynthesis();
    
    /**
     * @brief Initialize TTS system
     * @return true if initialized successfully
     */
    bool begin();
    
    /**
     * @brief Set TTS configuration
     */
    void setConfig(const TTSConfig& config);
    
    /**
     * @brief Get current configuration
     */
    const TTSConfig& getConfig() const { return config_; }
    
    /**
     * @brief Set voice persona
     */
    void setPersona(VoicePersona persona);
    
    /**
     * @brief Get current persona
     */
    VoicePersona getPersona() const { return config_.persona; }
    
    /**
     * @brief Speak text
     * @param text Text to speak
     * @return true if queued successfully
     */
    bool speak(const char* text);
    
    /**
     * @brief Speak text with Ancient dialect conversion
     */
    bool speakAncient(const char* text);
    
    /**
     * @brief Stop current speech
     */
    void stop();
    
    /**
     * @brief Check if currently speaking
     */
    bool isSpeaking() const { return is_speaking_; }
    
    /**
     * @brief Update TTS and lip-sync (call in loop)
     */
    void update();
    
    /**
     * @brief Get current phoneme for lip-sync
     */
    Phoneme getCurrentPhoneme() const { return current_phoneme_; }
    
    /**
     * @brief Get current beak openness (0-1)
     */
    float getBeakOpenness() const { return beak_openness_; }
    
    /**
     * @brief Set callback for phoneme events (for lip-sync)
     */
    void setPhonemeCallback(void (*callback)(Phoneme, float duration));
    
    /**
     * @brief Process text into phonemes for preview
     */
    static String textToPhonemes(const char* text);

private:
    TTSConfig config_;
    bool initialized_ = false;
    bool is_speaking_ = false;
    
    // Phoneme/lip-sync state
    Phoneme current_phoneme_ = Phoneme::SILENCE;
    float beak_openness_ = 0.0f;
    float target_beak_openness_ = 0.0f;
    uint32_t phoneme_start_time_ = 0;
    uint16_t current_duration_ = 0;
    
    // Text to phonemes
    String current_text_;
    size_t current_index_ = 0;
    std::vector<PhonemeTiming> phoneme_queue_;
    
    // Audio buffer
    static constexpr size_t AUDIO_BUFFER_SIZE = 4096;
    uint8_t audio_buffer_[AUDIO_BUFFER_SIZE];
    size_t audio_buffer_pos_ = 0;
    
    // Callback
    void (*phoneme_callback_)(Phoneme, float duration) = nullptr;
    
    // Private methods
    bool parseTextToPhonemes(const char* text);
    void generateAudioForPhoneme(Phoneme phoneme);
    void advancePhoneme();
    float getBeakOpennessForPhoneme(Phoneme phoneme);
    void queuePhoneme(Phoneme phoneme, uint16_t duration);
};

// Global instance
extern VoiceSynthesis g_voice;

} // namespace Avatar

#endif // VOICE_SYNTHESIS_H
