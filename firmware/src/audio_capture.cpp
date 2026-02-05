/**
 * @file audio_capture.cpp
 * @brief Audio capture implementation
 */

#include "audio_capture.h"
#include <cmath>

namespace OpenClaw {

AudioCapture::AudioCapture() {
    memset(last_error_, 0, sizeof(last_error_));
}

AudioCapture::~AudioCapture() {
    end();
}

bool AudioCapture::begin(const AudioSettings& config) {
    if (state_ != AudioState::IDLE) {
        strncpy(last_error_, "AudioCapture already initialized", sizeof(last_error_) - 1);
        return false;
    }
    
    audio_config_ = config;
    sample_rate_ = config.sample_rate;
    samples_per_frame_ = (sample_rate_ * config.frame_duration_ms) / 1000;
    gain_ = config.mic_gain;
    
    if (!createQueue()) {
        return false;
    }
    
    if (!setupI2S()) {
        destroyQueue();
        return false;
    }
    
    state_ = AudioState::INITIALIZING;
    return true;
}

bool AudioCapture::start() {
    if (state_ != AudioState::INITIALIZING && state_ != AudioState::PAUSED) {
        strncpy(last_error_, "AudioCapture not initialized", sizeof(last_error_) - 1);
        return false;
    }
    
    if (!startTask()) {
        return false;
    }
    
    state_ = AudioState::CAPTURING;
    return true;
}

void AudioCapture::stop() {
    if (state_ != AudioState::CAPTURING && state_ != AudioState::PAUSED) {
        return;
    }
    
    stopTask();
    state_ = AudioState::INITIALIZING;
}

void AudioCapture::pause() {
    if (state_ == AudioState::CAPTURING) {
        state_ = AudioState::PAUSED;
    }
}

void AudioCapture::resume() {
    if (state_ == AudioState::PAUSED) {
        state_ = AudioState::CAPTURING;
    }
}

void AudioCapture::end() {
    stop();
    teardownI2S();
    destroyQueue();
    state_ = AudioState::IDLE;
}

void AudioCapture::setCallback(AudioCaptureCallback* callback) {
    callback_ = callback;
}

bool AudioCapture::readFrame(AudioFrame& frame, uint32_t timeout_ms) {
    if (!audio_queue_) {
        return false;
    }
    
    return xQueueReceive(audio_queue_, &frame, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

bool AudioCapture::readFrameNonBlocking(AudioFrame& frame) {
    return readFrame(frame, 0);
}

void AudioCapture::setGain(uint8_t gain) {
    if (gain > 100) gain = 100;
    gain_ = gain;
}

void AudioCapture::setVADConfig(const VADConfig& config) {
    vad_config_ = config;
}

bool AudioCapture::setupI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate = sample_rate_,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = GPIO_NUM_42,    // PDM clock on Cardputer
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = GPIO_NUM_41   // PDM data on Cardputer
    };
    
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, nullptr);
    if (err != ESP_OK) {
        snprintf(last_error_, sizeof(last_error_), "I2S driver install failed: %d", err);
        return false;
    }
    
    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        snprintf(last_error_, sizeof(last_error_), "I2S set pin failed: %d", err);
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }
    
    return true;
}

void AudioCapture::teardownI2S() {
    i2s_driver_uninstall(I2S_PORT);
}

bool AudioCapture::createQueue() {
    audio_queue_ = xQueueCreate(AUDIO_QUEUE_LENGTH, sizeof(AudioFrame));
    if (!audio_queue_) {
        strncpy(last_error_, "Failed to create audio queue", sizeof(last_error_) - 1);
        return false;
    }
    
    state_mutex_ = xSemaphoreCreateMutex();
    if (!state_mutex_) {
        vQueueDelete(audio_queue_);
        audio_queue_ = nullptr;
        strncpy(last_error_, "Failed to create state mutex", sizeof(last_error_) - 1);
        return false;
    }
    
    return true;
}

void AudioCapture::destroyQueue() {
    if (audio_queue_) {
        vQueueDelete(audio_queue_);
        audio_queue_ = nullptr;
    }
    
    if (state_mutex_) {
        vSemaphoreDelete(state_mutex_);
        state_mutex_ = nullptr;
    }
}

bool AudioCapture::startTask() {
    BaseType_t result = xTaskCreatePinnedToCore(
        captureTask,
        "AudioCapture",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &capture_task_,
        1  // Run on Core 1 (Arduino runs on Core 1)
    );
    
    if (result != pdPASS) {
        strncpy(last_error_, "Failed to create capture task", sizeof(last_error_) - 1);
        return false;
    }
    
    return true;
}

void AudioCapture::stopTask() {
    if (capture_task_) {
        // Signal task to stop by changing state
        AudioState prev_state = state_;
        state_ = AudioState::IDLE;
        
        // Wait for task to complete
        vTaskDelay(pdMS_TO_TICKS(100));
        
        capture_task_ = nullptr;
        state_ = prev_state;
    }
}

void AudioCapture::captureTask(void* param) {
    AudioCapture* capture = static_cast<AudioCapture*>(param);
    capture->captureLoop();
    vTaskDelete(nullptr);
}

void AudioCapture::captureLoop() {
    AudioFrame frame;
    size_t bytes_read = 0;
    
    while (state_ == AudioState::CAPTURING || state_ == AudioState::PAUSED) {
        if (state_ == AudioState::PAUSED) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        
        // Read audio data from I2S
        esp_err_t err = i2s_read(I2S_PORT, 
                                  frame.samples, 
                                  samples_per_frame_ * sizeof(int16_t),
                                  &bytes_read, 
                                  pdMS_TO_TICKS(100));
        
        if (err != ESP_OK) {
            if (callback_) {
                callback_->onAudioError(err);
            }
            continue;
        }
        
        if (bytes_read == 0) {
            continue;
        }
        
        frame.num_samples = bytes_read / sizeof(int16_t);
        frame.timestamp = millis();
        
        // Process the frame (apply gain, VAD, etc.)
        processFrame(frame);
        
        // Queue the frame
        if (xQueueSend(audio_queue_, &frame, 0) != pdTRUE) {
            // Queue full, drop oldest frame
            AudioFrame dropped;
            xQueueReceive(audio_queue_, &dropped, 0);
            xQueueSend(audio_queue_, &frame, 0);
        }
        
        // Notify callback
        if (callback_) {
            callback_->onAudioFrame(frame);
        }
        
        frame_count_++;
    }
}

void AudioCapture::processFrame(AudioFrame& frame) {
    // Apply gain
    applyGain(frame.samples, frame.num_samples);
    
    // Calculate RMS level
    current_level_ = calculateRMS(frame.samples, frame.num_samples);
    
    // Voice activity detection
    if (vad_config_.enabled) {
        frame.voice_detected = detectVoiceActivity(frame);
    } else {
        frame.voice_detected = true;
    }
}

int16_t AudioCapture::calculateRMS(const int16_t* samples, size_t count) {
    if (count == 0) return 0;
    
    int64_t sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += (int32_t)samples[i] * samples[i];
    }
    
    return (int16_t)sqrt(sum / count);
}

void AudioCapture::applyGain(int16_t* samples, size_t count) {
    if (gain_ == 64) return;  // 64 is neutral (1.0 gain)
    
    float gain_factor = gain_ / 64.0f;
    
    for (size_t i = 0; i < count; i++) {
        int32_t amplified = (int32_t)(samples[i] * gain_factor);
        // Clip to prevent overflow
        if (amplified > 32767) amplified = 32767;
        if (amplified < -32768) amplified = -32768;
        samples[i] = (int16_t)amplified;
    }
}

bool AudioCapture::detectVoiceActivity(const AudioFrame& frame) {
    uint32_t now = millis();
    bool above_threshold = current_level_ > vad_config_.threshold;
    
    if (above_threshold) {
        if (!voice_active_) {
            // Voice start
            voice_start_time_ = now;
        }
        silence_start_time_ = now;
        voice_active_ = true;
    } else {
        // Check for silence timeout
        if (voice_active_ && (now - silence_start_time_ > vad_config_.silence_ms)) {
            voice_active_ = false;
        }
    }
    
    // Notify callback on state change
    static bool last_voice_state = false;
    if (voice_active_ != last_voice_state && callback_) {
        callback_->onVoiceActivity(voice_active_);
        last_voice_state = voice_active_;
    }
    
    return voice_active_;
}

} // namespace OpenClaw
