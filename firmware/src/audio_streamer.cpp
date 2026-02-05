/**
 * @file audio_streamer.cpp
 * @brief Audio streamer implementation (stub)
 */

#include "audio_streamer.h"

namespace OpenClaw {

AudioStreamer* AudioStreamer::instance_ = nullptr;

AudioStreamer::AudioStreamer()
    : state_(AudioStreamState::IDLE),
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
    return createQueues() && createMutexes();
}

void AudioStreamer::end() {
    stop();
    destroyQueues();
    destroyMutexes();
}

bool AudioStreamer::start() {
    state_ = AudioStreamState::STREAMING;
    return true;
}

void AudioStreamer::stop() {
    state_ = AudioStreamState::IDLE;
}

void AudioStreamer::pause() {
    state_ = AudioStreamState::PAUSED;
}

void AudioStreamer::resume() {
    state_ = AudioStreamState::STREAMING;
}

void AudioStreamer::update() {
    // Stub implementation
}

bool AudioStreamer::readFrame(AudioFrame& frame) {
    return false;
}

bool AudioStreamer::readEncodedPacket(EncodedAudioPacket& packet) {
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

bool AudioStreamer::createQueues() {
    raw_queue_ = xQueueCreate(config_.stream_queue_size, sizeof(AudioFrame));
    encoded_queue_ = xQueueCreate(config_.stream_queue_size, sizeof(EncodedAudioPacket));
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

void AudioStreamer::captureTaskWrapper(void* param) {
    // Stub
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
