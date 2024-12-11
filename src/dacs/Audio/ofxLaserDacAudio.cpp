#include "ofxLaserDacAudio.h"

using namespace ofxLaser;

void laserPointsToInterleavedAudio(const vector<ofxLaser::Point> &points, size_t point_count, vector<float> &audio, size_t audio_channels) {
    // The index for each data element within the interleaved audio buffer.
    size_t x = 0;
    size_t y = 1;
    size_t r = 2;
    size_t g = 3;
    size_t b = 4;
    size_t a = 5;

    // Check the audio buffer size matches the point buffer size.
    assert(audio.size() % audio_channels == 0);
    assert(point_count == (audio.size() / audio_channels));
    assert(points.size() >= point_count);

    // Write the point data to the audio buffer.
    for (size_t p_ix = 0; p_ix < point_count; p_ix++) {
        size_t a_ix = p_ix * audio_channels;
        const ofxLaser::Point& point = points[p_ix];
        if (x < audio_channels) {
            audio[a_ix + x] = ofMap(point.x, 0, 800, -1.0, 1.0);
        }
        if (y < audio_channels) {
            audio[a_ix + y] = ofMap(point.y, 0, 800, -1.0, 1.0);
        }
        if (r < audio_channels) {
            audio[a_ix + r] = point.r / 255.0f;
        }
        if (g < audio_channels) {
            audio[a_ix + g] = point.g / 255.0f;
        }
        if (b < audio_channels) {
            audio[a_ix + b] = point.b / 255.0f;
        }
        if (a < audio_channels) {
            // TODO: ofxLaser should allow arbitrary point types and only
            // require that they can be interpolated. Or at least just add a
            // 4th colour channel.
            audio[a_ix + a] = 0.0;
        }
    }
}

size_t sampleCountToMinPoints(size_t samples, size_t channels) {
    float points = (float)samples / (float)channels;
    return (size_t)ceil(points);
}

ofSoundStreamSettings DacAudio::defaultSettings(ofSoundDevice device) {
    ofSoundStreamSettings settings;
    settings.setOutDevice(device);
    settings.numOutputChannels = MIN(DacAudio::DEFAULT_CHANNELS, device.outputChannels);
    settings.setOutListener(this);
    // TODO: We may want to get the closest to some specific sample rate here.
    // For now, we just use the highest supported rate.
    if (!device.sampleRates.empty()) {
        settings.sampleRate = device.sampleRates[device.sampleRates.size() - 1];
    }
    
    settings.sampleRate = 48000;
    
    return settings;
}

ofSoundStreamSettings DacAudio::currentSettings() {
    ofSoundStreamSettings settings = defaultSettings(device);
    settings.numOutputChannels = stream.getNumOutputChannels();
    settings.bufferSize = stream.getBufferSize();
    settings.sampleRate = stream.getSampleRate();
    return settings;
}

const vector<ofAbstractParameter*>& DacAudio :: getDisplayData() {
    return displayData;
}

bool DacAudio::setup(ofSoundStreamSettings& settings) {
    mutex.lock();
    frameMode = false;
    buffered.clear();
    frame.clear();
    samples.clear();
    device = *settings.getOutDevice();
    bool res = stream.setup(settings);
    mutex.unlock();
    return res;
}

void DacAudio::audioOut(ofSoundBuffer& buffer) {
    // Silence the buffer first. This is important for the case where we don't
    // have enough samples. this can happen if we either were not in
    // `frameMode` or we didn't have any `frame` points yet. In this case, we
    // can only write silence to the rest of the buffer.
    buffer.set(0.0);

    mutex.lock();

    // Write any leftover samples.
    size_t written = 0;
    size_t to_write = MIN(samples.size(), buffer.size());
    for (size_t i = 0; i < to_write; i++) {
        buffer[i] = samples[i];
        written++;
    }

    // Erase the written samples.
    samples.erase(samples.begin(), samples.begin() + written);

    // If we've filled the buffer, we're done.
    if (written == buffer.size()) {
        mutex.unlock();
        return;
    }

    // The number of samples yet to be written.
    size_t remaining_samples = buffer.size() - written;
    size_t channels = buffer.getNumChannels();
    size_t required_points = sampleCountToMinPoints(remaining_samples, channels);

    // If in frameMode, make sure we have enough buffered points.
    if (frameMode && !frame.empty()) {
        while (buffered.size() < required_points) {
            for (auto p : frame) {
                buffered.push_back(p);
            }
        }
    }

    // Write the points to the `samples` buffer.
    size_t points_to_write = MIN(required_points, buffered.size());
//    size_t points_to_write = MAX(required_points, frame.size());
    samples.resize(points_to_write * channels);
    laserPointsToInterleavedAudio(buffered, points_to_write, samples, channels);
    buffered.erase(buffered.begin(), buffered.begin() + points_to_write);

    // Fill the remaining samples in the output `buffer`.
    size_t samples_to_write = MIN(samples.size(), remaining_samples);
    for (size_t i = 0; i < samples_to_write; i++) {
        buffer[written + i] = samples[i];
    }
    samples.erase(samples.begin(), samples.begin() + samples_to_write);

    // We're done!
    mutex.unlock();
}

bool DacAudio::sendFrame(const vector<Point>& points) {
    mutex.lock();
    frameMode = true;
    frame.clear();
    for (auto point : points) {
        frame.push_back(point);
    }
    mutex.unlock();
    return true;
}

//bool DacAudio::sendPoints(const vector<Point>& points) {
//    mutex.lock();
//    frameMode = false;
//    for (auto point : points) {
//        buffered.push_back(point);
//    }
//    mutex.unlock();
//    return true;
//}

bool DacAudio::setPointsPerSecond(uint32_t pps) {
    auto settings = currentSettings();
    uint32_t targetSampleRate = pps;// * settings.numOutputChannels;

    // Find the closest valid sample rate to the target...
    // TODO: If there is no exact match we should indicate this to the user somehow?
    uint32_t distance = abs((int32_t)targetSampleRate - (int32_t)settings.sampleRate);
    uint32_t closestRate = settings.sampleRate;
    for (auto sr : device.sampleRates) {
        uint32_t d = abs((int32_t)targetSampleRate - (int32_t)sr);
        if (d < distance) {
            distance = d;
            closestRate = sr;
        }
    }

    settings.sampleRate = closestRate;
    close();
    bool res = setup(settings);
    return res;
}

string DacAudio::getId() {
    return "Audio " + device.name;
}

int DacAudio::getStatus() {
    // TODO: `ofSoundStream` doesn't seem to offer any way to detect
    // disconnected device or any kind of errors soooo...
    return OFXLASER_DACSTATUS_GOOD;
}

void DacAudio::reset() {
    auto settings = currentSettings();
    close();
    setup(settings);
}

void DacAudio::close() {
    stream.close();
}
