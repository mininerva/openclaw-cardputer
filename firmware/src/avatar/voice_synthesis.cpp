/**
 * @file voice_synthesis.cpp
 * @brief On-device TTS implementation
 */

#include "avatar/voice_synthesis.h"
#include "avatar/ancient_ritual.h"
#include <cstring>

namespace Avatar {

// Global instance
VoiceSynthesis g_voice;

VoiceSynthesis::VoiceSynthesis() 
    : initialized_(false),
      is_speaking_(false),
      current_phoneme_(Phoneme::SILENCE),
      beak_openness_(0.0f) {
}

VoiceSynthesis::~VoiceSynthesis() {
}

bool VoiceSynthesis::begin() {
    // Initialize audio output
    // M5Cardputer.Speaker.begin();
    
    // Initialize TTS engine (placeholder for ESP32-TTS)
    // ESP32-TTSService::begin();
    
    initialized_ = true;
    return true;
}

void VoiceSynthesis::setConfig(const TTSConfig& config) {
    config_ = config;
    
    // Apply settings to TTS engine
    // ESP32-TTSService::setVolume(config.volume);
    // ESP32-TTSService::setSampleRate(config.sample_rate);
    // ESP32-TTSService::setSpeed(config.quality);
    
    // Apply persona settings
    if (config.persona == VoicePersona::PERSONA_ANCIENT) {
        config_.echo_enabled = true;
        config_.echo_delay = 50;
    }
}

void VoiceSynthesis::setPersona(VoicePersona persona) {
    config_.persona = persona;
    
    // Adjust settings per persona
    switch (persona) {
        case VoicePersona::PERSONA_DEFAULT:
            config_.volume = 128;
            config_.echo_enabled = false;
            break;
        case VoicePersona::PERSONA_ANCIENT:
            config_.volume = 140;
            config_.echo_enabled = true;
            config_.echo_delay = 100;
            break;
        case VoicePersona::PERSONA_WHISPER:
            config_.volume = 80;
            config_.quality = 3;  // Slower, more intimate
            break;
        case VoicePersona::PERSONA_BROADCAST:
            config_.volume = 200;
            config_.quality = 1;  // Faster, more energetic
            break;
        case VoicePersona::PERSONA_SILENT:
            // TTS disabled - use gateway only
            return;
    }
    
    // Apply to TTS engine
    // ESP32-TTSService::setVolume(config_.volume);
}

bool VoiceSynthesis::speak(const char* text) {
    if (!initialized_ || !text) return false;
    
    // Stop current speech
    stop();
    
    // Parse text to phonemes
    if (!parseTextToPhonemes(text)) {
        return false;
    }
    
    // Start speaking
    is_speaking_ = true;
    current_index_ = 0;
    phoneme_start_time_ = millis();
    
    // Trigger avatar speaking animation
    if (g_avatar.isReady()) {
        g_avatar.speak(text);
    }
    
    return true;
}

bool VoiceSynthesis::speakAncient(const char* text) {
    // Convert to Ancient dialect first
    String ancientText = AncientDialect::toAncientSpeak(text);
    return speak(ancientText.c_str());
}

void VoiceSynthesis::stop() {
    is_speaking_ = false;
    current_text_.clear();
    phoneme_queue_.clear();
    beak_openness_ = 0.0f;
    
    // Stop TTS engine
    // ESP32-TTSService::stop();
    
    // Stop audio output
    // M5Cardputer.Speaker.stop();
}

void VoiceSynthesis::update() {
    if (!is_speaking_) {
        beak_openness_ = 0.0f;
        return;
    }
    
    uint32_t now = millis();
    uint32_t elapsed = now - phoneme_start_time_;
    
    // Check if current phoneme complete
    if (elapsed >= current_duration_) {
        advancePhoneme();
        phoneme_start_time_ = now;
    }
    
    // Smooth beak openness to target
    float approach_speed = 0.1f;  // 10% per frame
    beak_openness_ += (target_beak_openness_ - beak_openness_) * approach_speed;
    
    // Clamp
    beak_openness_ = std::max(0.0f, std::min(1.0f, beak_openness_));
    
    // Update TTS engine
    // ESP32-TTSService::update();
}

String VoiceSynthesis::textToPhonemes(const char* text) {
    // Simple phonetic transcription (for display/logging)
    // Full TTS would do this internally
    return String(text);  // Placeholder
}

// =============================================================================
// Private Methods
// =============================================================================

bool VoiceSynthesis::parseTextToPhonemes(const char* text) {
    if (!text) return false;
    
    current_text_ = text;
    current_index_ = 0;
    phoneme_queue_.clear();
    
    // Simple text-to-phoneme rules
    // This is a very basic implementation
    // Real TTS would use complex linguistic rules
    
    for (size_t i = 0; i < current_text_.length(); i++) {
        char c = tolower(current_text_[i]);
        Phoneme phoneme = Phoneme::SILENCE;
        uint16_t duration = 80;  // Default duration
        uint8_t beak_open = 2;  // 20% open
        
        // Map character to phoneme
        switch (c) {
            // Vowels (longer, more open)
            case 'a': 
            case 'e':
            case 'i':
            case 'o':
            case 'u':
                if (c == 'a') phoneme = Phoneme::A;
                if (c == 'e') phoneme = Phoneme::E;
                if (c == 'i') phoneme = Phoneme::I;
                if (c == 'o') phoneme = Phoneme::O;
                if (c == 'u') phoneme = Phoneme::U;
                duration = 120;
                beak_open = 80;
                break;
            
            // Consonants (shorter, more closed)
            case 'b': phoneme = Phoneme::B; break;
            case 'd': phoneme = Phoneme::D; break;
            case 'f': phoneme = Phoneme::F; break;
            case 'g': phoneme = Phoneme::G; break;
            case 'h': phoneme = Phoneme::H; break;
            case 'j': phoneme = Phoneme::J; break;
            case 'k': phoneme = Phoneme::K; break;
            case 'l': phoneme = Phoneme::L; break;
            case 'm': phoneme = Phoneme::M; break;
            case 'n': phoneme = Phoneme::N; break;
            case 'p': phoneme = Phoneme::P; break;
            case 'r': phoneme = Phoneme::R; break;
            case 's': phoneme = Phoneme::S; break;
            case 't': phoneme = Phoneme::T; break;
            case 'v': phoneme = Phoneme::V; break;
            case 'w': phoneme = Phoneme::W; break;
            case 'y': phoneme = Phoneme::Y; break;
            case 'z': phoneme = Phoneme::Z; break;
            
            // Punctuation
            case '.': 
            case '!': 
            case '?':
                phoneme = Phoneme::END;
                duration = 200;
                beak_open = 20;
                break;
            case ',':
            case ';':
                phoneme = Phoneme::PAUSE;
                duration = 150;
                beak_open = 10;
                break;
            case ' ':
                phoneme = Phoneme::PAUSE;
                duration = 50;
                break;
            
            default:
                continue;  // Skip unknown
        }
        
        // Queue phoneme
        if (phoneme != Phoneme::SILENCE) {
            queuePhoneme(phoneme, duration);
        }
    }
    
    return !phoneme_queue_.empty();
}

void VoiceSynthesis::generateAudioForPhoneme(Phoneme phoneme) {
    // This would interface with actual TTS engine
    // ESP32-TTSService::speakPhoneme(phoneme);
}

void VoiceSynthesis::advancePhoneme() {
    if (current_index_ >= phoneme_queue_.size()) {
        // End of speech
        stop();
        return;
    }
    
    // Get next phoneme
    const PhonemeTiming& timing = phoneme_queue_[current_index_];
    current_phoneme_ = timing.type;
    current_duration_ = timing.duration_ms;
    
    // Calculate target beak openness
    target_beak_openness_ = timing.beak_openness / 100.0f;
    
    // Generate audio
    generateAudioForPhoneme(timing.type);
    
    // Callback for lip-sync
    if (phoneme_callback_) {
        phoneme_callback_(timing.type, timing.duration_ms / 1000.0f);
    }
    
    current_index_++;
}

float VoiceSynthesis::getBeakOpennessForPhoneme(Phoneme phoneme) {
    switch (phoneme) {
        // Vowels - wide open
        case Phoneme::A:
        case Phoneme::E:
        case Phoneme::I:
        case Phoneme::O:
        case Phoneme::U:
            return 0.7f;
        
        // Consonants - mostly closed
        case Phoneme::B:
        case Phoneme::D:
        case Phoneme::F:
        case Phoneme::G:
        case Phoneme::K:
        case Phoneme::P:
        case Phoneme::T:
            return 0.2f;
        
        // Semi-open consonants
        case Phoneme::M:
        case Phoneme::N:
        case Phoneme::L:
        case Phoneme::R:
        case Phoneme::W:
            return 0.4f;
        
        // Sibilants - slight opening
        case Phoneme::S:
        case Phoneme::Z:
        case Phoneme::SH:
            return 0.3f;
        
        // H - breath
        case Phoneme::H:
        case Phoneme::DH:
            return 0.5f;
        
        // V/Y - semi-vowels
        case Phoneme::V:
        case Phoneme::Y:
            return 0.5f;
        
        // Digraphs
        case Phoneme::TH:
        case Phoneme::CH:
        case Phoneme::NG:
            return 0.4f;
        
        // Punctuation
        case Phoneme::PAUSE:
            return 0.05f;
        case Phoneme::END:
            return 0.0f;
        
        default:
            return 0.2f;
    }
}

void VoiceSynthesis::queuePhoneme(Phoneme phoneme, uint16_t duration) {
    PhonemeTiming timing;
    timing.type = phoneme;
    timing.duration_ms = duration;
    timing.beak_openness = (uint8_t)(getBeakOpennessForPhoneme(phoneme) * 100);
    
    // Adjust timing for persona
    switch (config_.persona) {
        case VoicePersona::PERSONA_ANCIENT:
            timing.duration_ms = duration * 2;  // Slower
            timing.pitch_mult = 80;
            break;
        case VoicePersona::PERSONA_WHISPER:
            timing.duration_ms = duration * 1.5f;  // Slower
            timing.pitch_mult = 120;  // Higher pitch
            break;
        case VoicePersona::PERSONA_BROADCAST:
            timing.duration_ms = duration * 0.8f;  // Faster
            timing.pitch_mult = 90;
            break;
        default:
            timing.pitch_mult = 100;
            break;
    }
    
    phoneme_queue_.push_back(timing);
}

} // namespace Avatar
