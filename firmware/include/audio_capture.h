/**
 * @file audio_capture.h
 * @brief I2S microphone audio capture for OpenClaw Cardputer
 * 
 * Handles continuous audio streaming from the built-in microphone
 * with support for voice activity detection and audio compression.
 */

#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <Arduino.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "config_manager.h"

namespace OpenClaw {

// Audio constants
constexpr size_t AUDIO_BUFFER_SIZE = 1024;
constexpr size_t AUDIO_QUEUE_LENGTH = 10;
constexpr i2s_port_t I2S_PORT = I2S_NUM_0;

/**
 * @brief Audio frame structure for queue transmission
 */
struct AudioFrame {
    int16_t samples[AUDIO_BUFFER_SIZE];
    size_t num_samples;
    uint32_t timestamp;
    bool voice_detected;
    
    AudioFrame() : num_samples(0), timestamp(0), voice_detected(false) {}
};

/**
 * @brief Audio capture state
 */
enum class AudioState {
    IDLE,           // Not capturing
    INITIALIZING,   // Setting up I2S
    CAPTURING,      // Active capture
    PAUSED,         // Temporarily paused
    ERROR           // Error state
};

/**
 * @brief Voice activity detection configuration
 */
struct VADConfig {
    int16_t threshold = 500;        // Amplitude threshold
    uint16_t min_duration_ms = 200;  // Minimum voice duration
    uint16_t silence_ms = 500;       // Silence timeout
    bool enabled = true;
};

/**
 * @brief Audio capture callback interface
 */
class AudioCaptureCallback {
public:
    virtual ~AudioCaptureCallback() = default;
    
    /**
     * @brief Called when audio frame is ready
     * @param frame Audio frame data
     */
    virtual void onAudioFrame(const AudioFrame& frame) = 0;
    
    /**
     * @brief Called when voice activity detected
     * @param detected true if voice detected
     */
    virtual void onVoiceActivity(bool detected) {}
    
    /**
     * @brief Called on capture error
     * @param error Error code
     */
    virtual void onAudioError(int error) {}
};

/**
 * @brief Audio capture manager class
 * 
 * Manages I2S microphone input with voice activity detection
 * and efficient buffering for streaming.
 */
class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();
    
    // Disable copy
    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;
    
    /**
     * @brief Initialize audio capture
     * @param config Audio configuration
     * @return true if initialization successful
     */
    bool begin(const AudioSettings& config);
    
    /**
     * @brief Start audio capture
     * @return true if started successfully
     */
    bool start();
    
    /**
     * @brief Stop audio capture
     */
    void stop();
    
    /**
     * @brief Pause capture (keep I2S active)
     */
    void pause();
    
    /**
     * @brief Resume capture
     */
    void resume();
    
    /**
     * @brief Deinitialize audio capture
     */
    void end();
    
    /**
     * @brief Set callback for audio events
     */
    void setCallback(AudioCaptureCallback* callback);
    
    /**
     * @brief Read audio frame from queue (blocking)
     * @param frame Output frame
     * @param timeout_ms Timeout in milliseconds
     * @return true if frame received
     */
    bool readFrame(AudioFrame& frame, uint32_t timeout_ms = portMAX_DELAY);
    
    /**
     * @brief Read audio frame from queue (non-blocking)
     * @param frame Output frame
     * @return true if frame available
     */
    bool readFrameNonBlocking(AudioFrame& frame);
    
    /**
     * @brief Get current capture state
     */
    AudioState getState() const { return state_; }
    
    /**
     * @brief Check if currently capturing
     */
    bool isCapturing() const { return state_ == AudioState::CAPTURING; }
    
    /**
     * @brief Get current audio level (RMS)
     * @return RMS value (0-32767)
     */
    int16_t getAudioLevel() const { return current_level_; }
    
    /**
     * @brief Check if voice is currently detected
     */
    bool isVoiceDetected() const { return voice_active_; }
    
    /**
     * @brief Set microphone gain
     * @param gain Gain value (0-100)
     */
    void setGain(uint8_t gain);
    
    /**
     * @brief Get microphone gain
     */
    uint8_t getGain() const { return gain_; }
    
    /**
     * @brief Configure voice activity detection
     */
    void setVADConfig(const VADConfig& config);
    
    /**
     * @brief Get VAD configuration
     */
    const VADConfig& getVADConfig() const { return vad_config_; }
    
    /**
     * @brief Get sample rate
     */
    uint16_t getSampleRate() const { return sample_rate_; }
    
    /**
     * @brief Get samples per frame
     */
    size_t getSamplesPerFrame() const { return samples_per_frame_; }
    
    /**
     * @brief Get the last error message
     */
    const char* getLastError() const { return last_error_; }
    
    /**
     * @brief Get total frames captured
     */
    uint32_t getFrameCount() const { return frame_count_; }

private:
    // I2S configuration
    i2s_config_t i2s_config_;
    i2s_pin_config_t pin_config_;
    
    // State
    AudioState state_ = AudioState::IDLE;
    AudioSettings audio_config_;
    VADConfig vad_config_;
    AudioCaptureCallback* callback_ = nullptr;
    
    // FreeRTOS
    QueueHandle_t audio_queue_ = nullptr;
    TaskHandle_t capture_task_ = nullptr;
    SemaphoreHandle_t state_mutex_ = nullptr;
    
    // Audio parameters
    uint16_t sample_rate_ = 16000;
    size_t samples_per_frame_ = 960;  // 60ms at 16kHz
    uint8_t gain_ = 64;
    
    // Runtime state
    int16_t current_level_ = 0;
    bool voice_active_ = false;
    uint32_t voice_start_time_ = 0;
    uint32_t silence_start_time_ = 0;
    uint32_t frame_count_ = 0;
    char last_error_[128];
    
    // Task stack size
    static constexpr uint32_t TASK_STACK_SIZE = 4096;
    static constexpr UBaseType_t TASK_PRIORITY = 5;
    
    // Private methods
    bool setupI2S();
    void teardownI2S();
    bool createQueue();
    void destroyQueue();
    bool startTask();
    void stopTask();
    
    // Static task wrapper
    static void captureTask(void* param);
    void captureLoop();
    
    // Audio processing
    void processFrame(AudioFrame& frame);
    int16_t calculateRMS(const int16_t* samples, size_t count);
    void applyGain(int16_t* samples, size_t count);
    bool detectVoiceActivity(const AudioFrame& frame);
};

} // namespace OpenClaw

#endif // AUDIO_CAPTURE_H
