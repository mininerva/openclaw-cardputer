/**
 * @file audio_streamer.h
 * @brief Audio capture and streaming for OpenClaw Cardputer
 * 
 * Features:
 * - I2S microphone input with continuous streaming
 * - Voice Activity Detection (VAD)
 * - Opus codec encoding
 * - Real-time streaming to gateway
 * - Configurable sample rates and frame sizes
 */

#ifndef OPENCLAW_AUDIO_STREAMER_H
#define OPENCLAW_AUDIO_STREAMER_H

#include <Arduino.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <memory>
#include <functional>
#include "protocol.h"

namespace OpenClaw {

// Forward declaration
class AudioStreamer;

// Audio stream state
enum class AudioStreamState {
    IDLE,
    INITIALIZING,
    STREAMING,
    PAUSED,
    ERROR
};

// VAD state
enum class VADState {
    SILENCE,
    VOICE_START,
    VOICE_ACTIVE,
    VOICE_END
};

// Audio configuration
struct AudioStreamerConfig {
    // I2S configuration
    uint16_t sample_rate;
    i2s_bits_per_sample_t bits_per_sample;
    i2s_channel_fmt_t channel_format;
    i2s_comm_format_t communication_format;
    int dma_buf_count;
    int dma_buf_len;
    
    // Audio processing
    AudioCodec codec;
    uint16_t frame_duration_ms;
    uint8_t mic_gain;           // 0-100
    
    // VAD configuration
    bool vad_enabled;
    int16_t vad_threshold;      // Amplitude threshold (0-32767)
    uint16_t vad_min_duration_ms;
    uint16_t vad_silence_ms;
    float vad_ratio;            // Voice/silence ratio threshold
    
    // Streaming
    bool auto_stream;
    size_t stream_queue_size;
    
    AudioStreamerConfig()
        : sample_rate(16000),
          bits_per_sample(I2S_BITS_PER_SAMPLE_16BIT),
          channel_format(I2S_CHANNEL_FMT_ONLY_LEFT),
          communication_format(I2S_COMM_FORMAT_STAND_I2S),
          dma_buf_count(8),
          dma_buf_len(1024),
          codec(AudioCodec::OPUS),
          frame_duration_ms(60),
          mic_gain(64),
          vad_enabled(true),
          vad_threshold(500),
          vad_min_duration_ms(200),
          vad_silence_ms(500),
          vad_ratio(0.3f),
          auto_stream(true),
          stream_queue_size(10) {}
};

// Audio frame structure
struct AudioFrame {
    std::unique_ptr<int16_t[]> samples;
    size_t num_samples;
    uint32_t timestamp;
    VADState vad_state;
    float rms_level;
    
    AudioFrame() : num_samples(0), timestamp(0), vad_state(VADState::SILENCE), rms_level(0.0f) {}
    
    AudioFrame(size_t sample_count)
        : samples(new int16_t[sample_count]),
          num_samples(sample_count), timestamp(0),
          vad_state(VADState::SILENCE), rms_level(0.0f) {}
    
    // Move constructor
    AudioFrame(AudioFrame&& other) noexcept
        : samples(std::move(other.samples)),
          num_samples(other.num_samples),
          timestamp(other.timestamp),
          vad_state(other.vad_state),
          rms_level(other.rms_level) {
        other.num_samples = 0;
    }
    
    // Move assignment
    AudioFrame& operator=(AudioFrame&& other) noexcept {
        if (this != &other) {
            samples = std::move(other.samples);
            num_samples = other.num_samples;
            timestamp = other.timestamp;
            vad_state = other.vad_state;
            rms_level = other.rms_level;
            other.num_samples = 0;
        }
        return *this;
    }
    
    // Disable copy
    AudioFrame(const AudioFrame&) = delete;
    AudioFrame& operator=(const AudioFrame&) = delete;
};

// Encoded audio packet
struct EncodedAudioPacket {
    std::unique_ptr<uint8_t[]> data;
    size_t length;
    uint32_t timestamp;
    bool is_final;
    AudioCodec codec;
    
    EncodedAudioPacket() : length(0), timestamp(0), is_final(false), codec(AudioCodec::OPUS) {}
    
    EncodedAudioPacket(size_t max_size)
        : data(new uint8_t[max_size]),
          length(0), timestamp(0), is_final(false), codec(AudioCodec::OPUS) {}
    
    // Move constructor
    EncodedAudioPacket(EncodedAudioPacket&& other) noexcept
        : data(std::move(other.data)),
          length(other.length),
          timestamp(other.timestamp),
          is_final(other.is_final),
          codec(other.codec) {
        other.length = 0;
    }
    
    // Move assignment
    EncodedAudioPacket& operator=(EncodedAudioPacket&& other) noexcept {
        if (this != &other) {
            data = std::move(other.data);
            length = other.length;
            timestamp = other.timestamp;
            is_final = other.is_final;
            codec = other.codec;
            other.length = 0;
        }
        return *this;
    }
    
    // Disable copy
    EncodedAudioPacket(const EncodedAudioPacket&) = delete;
    EncodedAudioPacket& operator=(const EncodedAudioPacket&) = delete;
};

// Audio event types
enum class AudioEvent {
    STREAM_STARTED,
    STREAM_STOPPED,
    FRAME_CAPTURED,
    VOICE_DETECTED,
    VOICE_LOST,
    ENCODED_PACKET_READY,
    ERROR
};

// Audio event callback
using AudioEventCallback = std::function<void(AudioEvent event, const void* data)>;

// Audio streamer class
class AudioStreamer {
public:
    AudioStreamer();
    ~AudioStreamer();
    
    // Disable copy
    AudioStreamer(const AudioStreamer&) = delete;
    AudioStreamer& operator=(const AudioStreamer&) = delete;
    
    // Initialize with configuration
    bool begin(const AudioStreamerConfig& config);
    
    // Start streaming
    bool start();
    
    // Stop streaming
    void stop();
    
    // Pause/Resume
    void pause();
    void resume();
    
    // Deinitialize
    void end();
    
    // Update (call in main loop)
    void update();
    
    // Read raw audio frame (non-blocking)
    bool readFrame(AudioFrame& frame);
    
    // Read encoded packet (non-blocking)
    bool readEncodedPacket(EncodedAudioPacket& packet);
    
    // Set event callback
    void onEvent(AudioEventCallback callback);
    
    // Get current state
    AudioStreamState getState() const { return state_; }
    VADState getVADState() const { return vad_state_; }
    
    // Get audio level (RMS, 0.0 - 1.0)
    float getAudioLevel() const;
    
    // Check if voice is detected
    bool isVoiceDetected() const { return vad_state_ == VADState::VOICE_ACTIVE; }
    
    // Set microphone gain
    void setGain(uint8_t gain);
    uint8_t getGain() const { return config_.mic_gain; }
    
    // Set VAD threshold
    void setVADThreshold(int16_t threshold);
    int16_t getVADThreshold() const { return config_.vad_threshold; }
    
    // Get configuration
    const AudioStreamerConfig& getConfig() const { return config_; }
    
    // Get last error
    const char* getLastError() const { return last_error_; }
    
    // Get statistics
    uint32_t getFramesCaptured() const { return frames_captured_; }
    uint32_t getFramesStreamed() const { return frames_streamed_; }
    uint32_t getVoiceEvents() const { return voice_events_; }

private:
    // Configuration
    AudioStreamerConfig config_;
    
    // State
    AudioStreamState state_;
    VADState vad_state_;
    AudioEventCallback event_callback_;
    
    // I2S
    i2s_port_t i2s_port_;
    bool i2s_initialized_;
    
    // FreeRTOS
    TaskHandle_t capture_task_;
    QueueHandle_t raw_queue_;
    QueueHandle_t encoded_queue_;
    SemaphoreHandle_t state_mutex_;
    
    // Audio buffers
    std::unique_ptr<int16_t[]> i2s_buffer_;
    std::unique_ptr<int16_t[]> frame_buffer_;
    size_t frame_buffer_pos_;
    
    // VAD state
    uint32_t voice_start_time_;
    uint32_t silence_start_time_;
    uint32_t voice_frame_count_;
    uint32_t total_frame_count_;
    float current_rms_;
    
    // Statistics
    uint32_t frames_captured_;
    uint32_t frames_streamed_;
    uint32_t voice_events_;
    uint32_t errors_;
    
    // Error buffer
    char last_error_[128];
    
    // Opus encoder (if available)
    void* opus_encoder_;
    bool opus_initialized_;
    
    // Static instance for task
    static AudioStreamer* instance_;
    
    // Private methods
    bool setupI2S();
    void teardownI2S();
    bool createQueues();
    void destroyQueues();
    bool createMutexes();
    void destroyMutexes();
    bool startCaptureTask();
    void stopCaptureTask();
    bool initOpus();
    void teardownOpus();
    
    void captureLoop();
    void processFrame(const int16_t* samples, size_t count);
    void encodeAndQueue(const int16_t* samples, size_t count, bool is_final);
    
    float calculateRMS(const int16_t* samples, size_t count);
    void applyGain(int16_t* samples, size_t count);
    VADState updateVAD(float rms);
    
    // Static task wrapper
    static void captureTaskWrapper(void* param);
};

// Utility functions
const char* audioStreamStateToString(AudioStreamState state);
const char* vadStateToString(VADState state);
const char* audioEventToString(AudioEvent event);

} // namespace OpenClaw

#endif // OPENCLAW_AUDIO_STREAMER_H
