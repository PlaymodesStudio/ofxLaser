#include "ofxLaserDacManagerAudio.h"

using namespace ofxLaser;

vector<DacData> DacManagerAudio::updateDacList() {
    vector<ofSoundDevice> devices = ofSoundStreamListDevices();
    vector<DacData> dacs = {};
    for (auto device: devices) {
        DacData data(getType(), device.name);
        dacs.push_back(data);
    }
    return dacs;
}

std::shared_ptr<DacBase> DacManagerAudio::getAndConnectToDac(const string& id) {
    std::shared_ptr<DacBase> dac = getDacById(id);
    // If we've already setup a stream for this ID, return a pointer to the existing dac.
    if (dac) {
        return dac;
    }
    // Loop through the DACs until we find a matching ID.
    vector<ofSoundDevice> devices = ofSoundStreamListDevices();
    for (auto device: devices) {
        if (id.find(device.name) != std::string::npos) {
            DacAudio* adac = new DacAudio();
            auto settings = adac->defaultSettings(device);
            if (!adac->setup(settings)) {
                ofLogError("Failed to setup audio DAC " + id + ". Continuing search...");
                adac = nullptr;
                continue;
            }
            dacsById.emplace(std::make_pair(id, adac));
            return dacsById[id];
        }
    }
    return nullptr;
}

//bool DacManagerAudio::disconnectAndDeleteDac(const string& id) {
//    if (dacsById.count(id) == 0) {
//        ofLogError("DacManagerAudio::disconnectAndDeleteDac("+id+") - dac not found");
//        return false;
//    }
//    DacAudio* dac = (DacAudio*)dacsById.at(id);
//    dac->close();
//    dacsById.erase(id);
//    delete dac;
//    return true;
//}
