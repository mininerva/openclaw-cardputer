/**
 * @file audio_streamer.cpp
 * @brief Audio streamer implementation with real I2S capture and VAD
 */

#include "audio_streamer.h"
#include <cmath>
#include <cstring>

namespace OpenClaw {

// Static instance pointer
AudioStreamer* AudioStreamer::instance_ = nullptr;

// I2S pin configuration for M5Stack Cardputer ADV
static constexpr gpio_num_t I2S_BCLK = GPIO_NUM_6;
static constexpr gpio_num_t I2S_WS = GPIO_NUM_7;
static constexpr gpio_num_t I2S_DIN = GPIO_NUM_8;

AudioStreamer::AudioStreamer()
    : config_(),
      state_(AudioStreamState::IDLE),
      vad_state_(VADState::SILENCE),
      event_callback_(nullptr),
      i2s_port_(I2S_NUM_0),
      i2s_initialized_(false),
      capture_task_(nullptr),
      raw_queue_(nullptr),
      encoded_queue_(nullptr),
      state_mutex_(nullptr),
      frame_buffer_pos_(0),
      voice_start_time_(0),
      silence_start_time_(0),
      voice_frame_count_(0),
      total_frame_count_(0),
      current_rms_(0.0f),
      frames_captured_(0),
      frames_streamed_(0),
      voice_events_(0),
      errors_(0),
      opus_encoder_(nullptr),
      opus_initialized_(false) {
    instance_ = this;
    last_error_[0] = '\0';
}

AudioStreamer::~AudioStreamer() {
    end();
    instance_ = nullptr;
}

bool AudioStreamer::begin(const AudioStreamerConfig& config) {
    config_ = config;
    
    if (!createQueues()) {
        strcpy(last_error_, "Failed to create queues");
        return false;
    }
    
    if (!createMutexes()) {
        strcpy(last_error_, "Failed to create mutexes");
        return false;
    }
    
    // Allocate buffers
    size_t i2s_buffer_samples = config_.dma_buf_len * config_.dma_buf_count;
    i2s_buffer_.reset(new int16_t[i2s_buffer_samples]);
    
    size_t frame_samples = (config_.sample_rate * config_.frame_duration_ms) / 1000;
    frame_buffer_.reset(new int16_t[frame_samples]);
    frame_buffer_pos_ = 0;
    
    if (!i2s_buffer_ || !frame_buffer_) {
        strcpy(last_error_, "Failed to allocate buffers");
        return false;
    }
    
    state_ = AudioStreamState::IDLE;
    return true;
}

void AudioStreamer::end() {
    stop();
    teardownI2S();
    teardownOpus();
    destroyQueues();
    destroyMutexes();
    i2s_buffer_.reset();
    frame_buffer_.reset();
    state_ = AudioStreamState::IDLE;
}

bool AudioStreamer::start() {
    if (state_ == AudioStreamState::STREAMING) {
        return true;
    }
    
    if (!setupI2S()) {
        return false;
    }
    
    if (!startCaptureTask()) {
        teardownI2S();
        return false;
    }
    
    state_ = AudioStreamState::STREAMING;
    
    if (event_callback_) {
        event_callback_(AudioEvent::STREAM_STARTED, nullptr);
    }
    
    return true;
}

void AudioStreamer::stop() {
    if (state_ == AudioStreamState::IDLE) {
        return;
    }
    
    state_ = AudioStreamState::IDLE;
    stopCaptureTask();
    teardownI2S();
    
    if (event_callback_) {
        event_callback_(AudioEvent::STREAM_STOPPED, nullptr);
    }
}

void AudioStreamer::pause() {
    if (state_ == AudioStreamState::STREAMING) {
        state_ = AudioStreamState::PAUSED;
    }
}

void AudioStreamer::resume() {
    if (state_ == AudioStreamState::PAUSED) {
        state_ = AudioStreamState::STREAMING;
    }
}

void AudioStreamer::update() {
    // Process any pending events or queue maintenance
    // Most work happens in capture task
}

bool AudioStreamer::readFrame(AudioFrame& frame) {
    if (!raw_queue_) return false;
    
    // Create temporary frame structure for queue
    struct QueueFrame {
        int16_t samples[640];  // Max 40ms at 16kHz
        size_t num_samples;
        uint32_t timestamp;
        VADState vad_state;
        float rms_level;
    } qframe;
    
    if (xQueueReceive(raw_queue_, &qframe, pdMS_TO_TICKS(10)) == pdTRUE) {
        frame = AudioFrame(qframe.num_samples);
        memcpy(frame.samples.get(), qframe.samples, qframe.num_samples * sizeof(int16_t));
        frame.num_samples = qframe.num_samples;
        frame.timestamp = qframe.timestamp;
        frame.vad_state = qframe.vad_state;
        frame.rms_level = qframe.rms_level;
        return true;
    }
    return false;
}

bool AudioStreamer::readEncodedPacket(EncodedAudioPacket& packet) {
    if (!encoded_queue_) return false;
    
    struct QueuePacket {
        uint8_t data[2048];
        size_t length;
        uint32_t timestamp;
        bool is_final;
        AudioCodec codec;
    } qpacket;
    
    if (xQueueReceive(encoded_queue_, &qpacket, pdMS_TO_TICKS(10)) == pdTRUE) {
        packet = EncodedAudioPacket(qpacket.length);
        memcpy(packet.data.get(), qpacket.data, qpacket.length);
        packet.length = qpacket.length;
        packet.timestamp = qpacket.timestamp;
        packet.is_final = qpacket.is_final;
        packet.codec = qpacket.codec;
        return true;
    }
    return false;
}

void AudioStreamer::onEvent(AudioEventCallback callback) {
    event_callback_ = callback;
}

float AudioStreamer::getAudioLevel() const {
    return current_rms_;
}

void AudioStreamer::setGain(uint8_t gain) {
    config_.mic_gain = gain;
}

void AudioStreamer::setVADThreshold(int16_t threshold) {
    config_.vad_threshold = threshold;
}

bool AudioStreamer::setupI2S() {
    if (i2s_initialized_) {
        return true;
    }
    
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = config_.sample_rate,
        .bits_per_sample = config_.bits_per_sample,
        .channel_format = config_.channel_format,
        .communication_format = config_.communication_format,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = config_.dma_buf_count,
        .dma_buf_len = config_.dma_buf_len,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DIN
    };
    
    esp_err_t err = i2s_driver_install(i2s_port_, &i2s_config, 0, nullptr);
    if (err != ESP_OK) {
        snprintf(last_error_, sizeof(last_error_), "I2S driver install failed: %d", err);
        return false;
    }
    
    err = i2s_set_pin(i2s_port_, &pin_config);
    if (err != ESP_OK) {
        i2s_driver_uninstall(i2s_port_);
        snprintf(last_error_, sizeof(last_error_), "I2S pin config failed: %d", err);
        return false;
    }
    
    i2s_initialized_ = true;
    return true;
}

void AudioStreamer::teardownI2S() {
    if (i2s_initialized_) {
        i2s_stop(i2s_port_);
        i2s_driver_uninstall(i2s_port_);
        i2s_initialized_ = false;
    }
}

bool AudioStreamer::createQueues() {
    raw_queue_ = xQueueCreate(config_.stream_queue_size, 2048);  // Fixed size for QueueFrame
    encoded_queue_ = xQueueCreate(config_.stream_queue_size, 2056);  // Fixed size for QueuePacket
    return raw_queue_ != nullptr && encoded_queue_ != nullptr;
}

void AudioStreamer::destroyQueues() {
    if (raw_queue_) {
        vQueueDelete(raw_queue_);
        raw_queue_ = nullptr;
    }
    if (encoded_queue_) {
        vQueueDelete(encoded_queue_);
        encoded_queue_ = nullptr;
    }
}

bool AudioStreamer::createMutexes() {
    state_mutex_ = xSemaphoreCreateMutex();
    return state_mutex_ != nullptr;
}

void AudioStreamer::destroyMutexes() {
    if (state_mutex_) {
        vSemaphoreDelete(state_mutex_);
        state_mutex_ = nullptr;
    }
}

bool AudioStreamer::startCaptureTask() {
    if (capture_task_) {
        return true;
    }
    
    BaseType_t result = xTaskCreatePinnedToCore(
        captureTaskWrapper,
        "AudioCapture",
        8192,
        this,
        1,
        &capture_task_,
        1  // Run on APP CPU (core 1)
    );
    
    return result == pdPASS;
}

void AudioStreamer::stopCaptureTask() {
    if (capture_task_) {
        vTaskDelete(capture_task_);
        capture_task_ = nullptr;
    }
}

bool AudioStreamer::initOpus() {
    // Opus encoder initialization would go here
    // For now, using PCM fallback
    opus_initialized_ = false;
    return true;
}

void AudioStreamer::teardownOpus() {
    opus_initialized_ = false;
}

void AudioStreamer::captureTaskWrapper(void* param) {
    AudioStreamer* self = static_cast<AudioStreamer*>(param);
    self->captureLoop();
    vTaskDelete(nullptr);
}

void AudioStreamer::captureLoop() {
    size_t bytes_read;
    size_t samples_per_frame = (config_.sample_rate * config_.frame_duration_ms) / 1000;
    size_t bytes_per_frame = samples_per_frame * sizeof(int16_t);
    
    std::unique_ptr<int16_t[]> temp_buffer(new int16_t[samples_per_frame]);
    
    while (state_ == AudioStreamState::STREAMING || state_ == AudioStreamState::PAUSED) {
        if (state_ == AudioStreamState::PAUSED) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        
        // Read from I2S
        esp_err_t err = i2s_read(
            i2s_port_,
            temp_buffer.get(),
            bytes_per_frame,
            &bytes_read,
            pdMS_TO_TICKS(100)
        );
        
        if (err != ESP_OK || bytes_read != bytes_per_frame) {
            errors_++;
            continue;
        }
        
        // Apply gain
        applyGain(temp_buffer.get(), samples_per_frame);
        
        // Process frame (VAD)
        processFrame(temp_buffer.get(), samples_per_frame);
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void AudioStreamer::processFrame(const int16_t* samples, size_t count) {
    // Calculate RMS
    float rms = calculateRMS(samples, count);
    current_rms_ = rms;
    total_frame_count_++;
    
    // Update VAD
    VADState new_vad_state = updateVAD(rms);
    
    // Copy to frame buffer
    if (frame_buffer_pos_ + count <= (config_.sample_rate * config_.frame_duration_ms) / 1000) {
        memcpy(frame_buffer_.get() + frame_buffer_pos_, samples, count * sizeof(int16_t));
        frame_buffer_pos_ += count;
    }
    
    // Check if we have a complete frame
    size_t frame_samples = (config_.sample_rate * config_.frame_duration_ms) / 1000;
    if (frame_buffer_pos_ >= frame_samples) {
        // Send to queue if voice detected
        if (vad_state_ == VADState::VOICE_ACTIVE || vad_state_ == VADState::VOICE_END) {
            struct QueueFrame {
                int16_t samples[640];
                size_t num_samples;
                uint32_t timestamp;
                VADState vad_state;
                float rms_level;
            } qframe;
            
            qframe.num_samples = frame_samples;
            memcpy(qframe.samples, frame_buffer_.get(), frame_samples * sizeof(int16_t));
            qframe.timestamp = millis();
            qframe.vad_state = vad_state_;
            qframe.rms_level = current_rms_;
            
            if (xQueueSend(raw_queue_, &qframe, pdMS_TO_TICKS(10)) == pdTRUE) {
                frames_captured_++;
                
                // Also encode and queue
                encodeAndQueue(frame_buffer_.get(), frame_samples, vad_state_ == VADState::VOICE_END);
            }
        }
        
        frame_buffer_pos_ = 0;
    }
}

void AudioStreamer::encodeAndQueue(const int16_t* samples, size_t count, bool is_final) {
    // For now, just copy PCM data (no Opus encoding)
    struct QueuePacket {
        uint8_t data[2048];
        size_t length;
        uint32_t timestamp;
        bool is_final;
        AudioCodec codec;
    } qpacket;
    
    size_t pcm_bytes = count * sizeof(int16_t);
    if (pcm_bytes > 2048) pcm_bytes = 2048;
    
    memcpy(qpacket.data, samples, pcm_bytes);
    qpacket.length = pcm_bytes;
    qpacket.timestamp = millis();
    qpacket.is_final = is_final;
    qpacket.codec = AudioCodec::PCM_S16LE;  // PCM fallback
    
    if (xQueueSend(encoded_queue_, &qpacket, pdMS_TO_TICKS(10)) == pdTRUE) {
        frames_streamed_++;
        if (event_callback_) {
            event_callback_(AudioEvent::ENCODED_PACKET_READY, nullptr);
        }
    }
}

float AudioStreamer::calculateRMS(const int16_t* samples, size_t count) {
    if (count == 0) return 0.0f;
    
    int64_t sum = 0;
    for (size_t i = 0; i < count; i++) {
        int64_t sample = samples[i];
        sum += sample * sample;
    }
    
    float mean = static_cast<float>(sum) / count;
    return std::sqrt(mean);
}

void AudioStreamer::applyGain(int16_t* samples, size_t count) {
    if (config_.mic_gain == 64) return;  // 64 is unity gain (0.5 * 128)
    
    float gain = config_.mic_gain / 64.0f;
    for (size_t i = 0; i < count; i++) {
        int32_t sample = static_cast<int32_t>(samples[i] * gain);
        // Clip to int16 range
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        samples[i] = static_cast<int16_t>(sample);
    }
}

VADState AudioStreamer::updateVAD(float rms) {
    uint32_t now = millis();
    bool is_speech = rms > config_.vad_threshold;
    
    switch (vad_state_) {
        case VADState::SILENCE:
            if (is_speech) {
                vad_state_ = VADState::VOICE_START;
                voice_start_time_ = now;
                voice_frame_count_ = 0;
            }
            break;
            
        case VADState::VOICE_START:
            if (is_speech) {
                if (now - voice_start_time_ >= config_.vad_min_duration_ms) {
                    vad_state_ = VADState::VOICE_ACTIVE;
                    voice_events_++;
                    if (event_callback_) {
                        event_callback_(AudioEvent::VOICE_DETECTED, nullptr);
                    }
                }
            } else {
                vad_state_ = VADState::SILENCE;
            }
            break;
            
        case VADState::VOICE_ACTIVE:
            if (!is_speech) {
                vad_state_ = VADState::VOICE_END;
                silence_start_time_ = now;
            } else {
                voice_frame_count_++;
            }
            break;
            
        case VADState::VOICE_END:
            if (is_speech) {
                vad_state_ = VADState::VOICE_ACTIVE;
            } else if (now - silence_start_time_ >= config_.vad_silence_ms) {
                vad_state_ = VADState::SILENCE;
                if (event_callback_) {
                    event_callback_(AudioEvent::VOICE_LOST, nullptr);
                }
                voice_frame_count_ = 0;
            }
            break;
    }
    
    return vad_state_;
}

const char* audioStreamStateToString(AudioStreamState state) {
    switch (state) {
        case AudioStreamState::IDLE: return "IDLE";
        case AudioStreamState::INITIALIZING: return "INITIALIZING";
        case AudioStreamState::STREAMING: return "STREAMING";
        case AudioStreamState::PAUSED: return "PAUSED";
        case AudioStreamState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* vadStateToString(VADState state) {
    switch (state) {
        case VADState::SILENCE: return "SILENCE";
        case VADState::VOICE_START: return "VOICE_START";
        case VADState::VOICE_ACTIVE: return "VOICE_ACTIVE";
        case VADState::VOICE_END: return "VOICE_END";
        default: return "UNKNOWN";
    }
}

const char* audioEventToString(AudioEvent event) {
    switch (event) {
        case AudioEvent::STREAM_STARTED: return "STREAM_STARTED";
        case AudioEvent::STREAM_STOPPED: return "STREAM_STOPPED";
        case AudioEvent::FRAME_CAPTURED: return "FRAME_CAPTURED";
        case AudioEvent::VOICE_DETECTED: return "VOICE_DETECTED";
        case AudioEvent::VOICE_LOST: return "VOICE_LOST";
        case AudioEvent::ENCODED_PACKET_READY: return "ENCODED_PACKET_READY";
        case AudioEvent::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // namespace OpenClaw
