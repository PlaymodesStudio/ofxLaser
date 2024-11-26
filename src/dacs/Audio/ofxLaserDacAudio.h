#pragma once

#include "ofMain.h"
#include "ofxLaserDacBase.h"

namespace ofxLaser {

// A DAC implementation for LASER DACs that appear as audio DACs.
class DacAudio : public DacBase {
    public:
    DacAudio() {};

    ofSoundStreamSettings defaultSettings(ofSoundDevice device);
    ofSoundStreamSettings currentSettings();
    bool setup(ofSoundStreamSettings& settings);
    void audioOut(ofSoundBuffer& buffer);
    
    const vector<ofAbstractParameter*>& getDisplayData() override;

    bool sendFrame(const vector<Point>& points) override;
//    bool sendPoints(const vector<Point>& points) override;
    bool setPointsPerSecond(uint32_t pps) override;
    bool setColourShift(float shiftseconds) override { return true; }; // TODO implement here in DAC
    string getId() override;
    int getStatus() override;
    void reset() override;
    void setArmed(bool armed) override {};
    void close() override;

    // TODO: These are arbitrarily decided for a downstream use-case with AVB.
    // Ideally there would be a way for users to configure these...
    // x, y, r, g, b, amber.
    static const size_t DEFAULT_CHANNELS = 6;

    private:
    ofSoundStream stream;
    ofSoundDevice device;

    // A single mutex for synchronising access to the `buffered`, `frame`,
    // `samples,` buffers and the `stream`.
    std::mutex mutex;
    // A queue where the newest points are pushed to the back.
    vector<Point> buffered;
    // Stores the most recently received frame of points.
    // This is used in the case that we are in `frameMode` and run out of
    // `buffered` points.
    vector<Point> frame;
    // It is possible for the `audioOut` method to request a number of samples
    // that ends halfway through a point. We use this buffer to store any
    // left-over samples. We assume that the size of this buffer is always less
    // than the size of the `audioOut` buffer.
    vector<float> samples;
    // If true, indicates that there are points available in `frame` that
    // can be used in the case that we run out of `buffered` points.
    bool frameMode;
};
}
