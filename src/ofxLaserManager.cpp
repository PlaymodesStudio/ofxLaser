//
//  ofxLaserManager.cpp
//  ofxLaser 2
//
//  Created by Seb Lee-Delisle on 06/05/2021.
//

#include "ofxLaserManager.h"

using namespace ofxLaser;

Manager * Manager :: laserManager = NULL;

Manager * Manager::instance() {
    if(laserManager == NULL) {
        laserManager = new Manager();
    }
    return laserManager;
}

Manager :: Manager(bool hidecanvas) {
    
    if(laserManager == NULL) {
        laserManager = this;
    } else {
        ofLog(OF_LOG_ERROR, "Multiple ofxLaser::Manager instances created");
    }

    initAndLoadSettings();
    
    // if no lasers are loaded make one and add a zone
    if(lasers.size()==0) {
        createAndAddLaser();
        
        //lasers[0]->addZone(0);
       
    }
    

    selectedLaserIndex = 0;

    // seems nasty - this should be set somewhere else???
//    int pixelscale = ((ofAppGLFWWindow *)(ofGetWindowPtr()))->getPixelScreenCoordScale();
//    ofRectangle canvasrect = canvasViewController.getOutputRect();
//    canvasrect.x =54*pixelscale;
//    canvasrect.y = 100*pixelscale;

    
    dacSettingsTimeSlice.set("Magnification", 0.5, 0.1, 20);
        
    params.add(customParams);

}

Manager::~Manager() {
}


void Manager :: resetAllLasersToDefault() {
    ManagerBase::resetAllLasersToDefault();
    viewMode = OFXLASER_VIEW_3D;
}
void Manager :: createAndAddLaser()  {
    int laserindex = lasers.size();
    ManagerBase:: createAndAddLaser();
}

void Manager :: initAndLoadSettings() {
    
    if(initialised) {
        ofLogError("ofxLaser::Manager::initAndLoadSettings() called twice - NB you no longer need to call this in your code, it happens automatically");
        return ;
    }
    
    interfaceParams.setName("Interface");

    params.add(interfaceParams);
    
    customParams.setName("CUSTOM PARAMETERS");
    
    // is this still used ?
    //params.add(zoneEditorShowLaserPath.set("Show path in zone editor", true));
    //params.add(zoneEditorShowLaserPoints.set("Show points in zone editor", false));
    params.add(zoneGridSnap.set("Zone snap to grid", true));
    params.add(zoneGridSize.set("Zone grid size", 16,1,64));
    params.add(zoneGridVisible.set("Zone grid visible", true));
    
    params.add(canvasGridSnap.set("Canvas snap to grid", true));
    params.add(canvasGridSize.set("Canvas grid size", 20,1,50));
    params.add(canvasGridVisible.set("Canvas grid visible", true));
    
    params.add(globalLatency.set("Latency (ms)", 150,0,400));

    params.add(showCustomParametersWindow.set("showCustomParametersWindow", true));
    params.add(showLaserOverviewWindow.set("showLaserManagementWindow", true));
    params.add(showLaserOutputSettingsWindow.set("showLaserOutputSettingsWindow", true));

    loadSettings();
    // param changed updates zone settings and global latency on all
    paramChanged(params);
    ofAddListener(params.parameterChangedE(), this, &Manager::paramChanged);
    
    copyParams.add(copyScannerSettings.set("Copy scanner / speed settings", false));
    copyParams.add(copyAdvancedSettings.set("Copy advanced settings", false));
    copyParams.add(copyColourSettings.set("Copy colour settings", false));
    copyParams.add(copyZonePositions.set("Copy output zone positions", false));

}


// I don't think this function is currently being called!
// ah except for when importing and exporting laser settings
// At the moment, loading and saving is done via the load/save functions.

// Currently uses files :
// ofxLaser/laserSettings.json - deserializes the params list, beam zones, canvas target
// It reads the numLasers value from laserSettings.json, then attempts to load lasers 
// for each one
//
// The deserialize function does the same thing, except I think it also saves all the laser
// settings within the same file. I believe the zones are saved as different files though,
// probably not great?
//
// ok so each laser also has a load/save settings function that looks for files.
// Will need to refactor that so it is serialize / deserialize. Lasers
// should not take ownership of their own load and save!
//
// How to untangle!!!
// Step one :
// Investigate Laser load save
//      laser.loadSettings only seems to ever be called by the laser manager
//      laser.saveSettings is used throughout Laser!

// Move zone load and save to Laser itself, and include in laser settings
//      Note that laser serialize / deserialize does actually include the zone
//      settings.
// At the moment zone load/save seems to have to manage whether the zone has
// changed itself - I'm guessing at some point it got slow and needed optimizing.
// This suggests that laser.save is called often. Research!
// Laser.saveSettings is called :
//      - when a zone is added
//      - when a zone/alt zone is removed
//      - on updateZones, if a zone is in the changed list.
//          - *** WHEN is this called?
//          - updateZones is called by the laserManager when a beam zone or canvas
//            zone is deleted. Clumsy!
//          - I don't fully understand this system. I think it's something to do with
//            the connection between the zones in the laser and their interface. Honestly
//            it seems like a total mess. But that might be because I have forgotten
//            how it works.
//      - on Laser::update() if any of the zones return true to their "update" function
//      - when any of the parameters are changed (unless ignoreParamChange is set - ugh)



// Investigate how and when those functions are called and offset them up a level.
//
// Create serialize / deserialize for Laser that includes all zone settings!
// Rewrite laser load / save so that it uses the serialize / deserialize function
// Get rid of laser load / save, move it up to the laser manager.
//      Rewrite laser.saveSettings calls so that they mark the laser as needing save.

// WAIT... is bundling all the laser settings into one file sensible?
// PRO  - code is more consistent
//      - only one set of code for exporting ofxlaser setting / saving
// CON  - saving could be slow if you have a complex set up
//      - if save is interupted, whole set up could be corrupted

// SOLUTION :
// A hybrid approach where the lasers themselves no longer know about saving and loading
// but they do serialize and deserialize their data
// I need to move away from separate files for each zone data though.

void Manager :: serialize(ofJson& json) {
    ManagerBase::serialize(json);
}


bool Manager :: deserialize(ofJson& json) {
    
    bool success = ManagerBase::deserialize(json);
    //cout << json.dump(3) << endl; 
    
    if(json.contains("visualiser3D")) {
//        visualiser3D.deserialize(json["visualiser3D"]);
    } else {
        success = false;
    }
    
    if(selectedLaserIndex>=lasers.size()) {
        selectedLaserIndex = lasers.size()-1;
    }
    
    return success;
    
}


void Manager :: paramChanged(ofAbstractParameter& e) {
    for(Laser* laser : lasers) {
        //laser->setGrid(zoneGridSnap, zoneGridSize);
        laser->maxLatencyMS = globalLatency;
    }
    
    //ofLogNotice() << "paramChanged " << e.getName();
    scheduleSaveSettings();
}

void Manager :: update() {
 
    
    ManagerBase :: update();

    // bit of a nasty way to update the canvas zones
    // will be better as a listener view / control system in future
   // canvasViewController.setSourceRect(ofRectangle(0,0,canvasTarget.getWidth(), canvasTarget.getHeight()));
    //canvasViewController.setOutputRect(ofRectangle(10,10,canvasTarget.getWidth(), canvasTarget.getHeight()));
    //canvasViewController.setLockedAll(false);
   
}

bool Manager :: deleteLaser(Laser* laser) {
    
    int laserindex = getLaserIndex(laser);
    
    bool success = ManagerBase::deleteLaser(laser);
    if(success) {
        if(selectedLaserIndex>=getNumLasers()) selectedLaserIndex = getNumLasers()-1;
        if(getNumLasers()==0) {
            showLaserOutputSettingsWindow = false;
        }
        return true;
    } else {
        return false;
    }
    
}

int Manager::getLaserIndex(Laser* laser) {
    for(int i = 0; i<lasers.size(); i++) {
        if(lasers[i] == laser) {
            return i;
        }
    }
    return -1; 
    
    
}

void Manager::setCanvasSize(int width, int height) {
    ManagerBase::setCanvasSize(width, height);
}

void Manager::addCustomParameter(ofAbstractParameter& param, bool loadFromSettings){
    customParams.add(param);
    if(loadFromSettings){
        
        if(!loadedJson.empty()) {
            if(loadedJson.contains("Laser")) {
                if(loadedJson["Laser"].contains("CUSTOM_PARAMETERS")) {
                    try {
                        ofDeserialize(loadedJson["Laser"]["CUSTOM_PARAMETERS"], param);
                    } catch(...) {
                        
                    }
                    
                }
            }
        }
        
    }
}

glm::vec2 Manager::screenToLaserInput(glm::vec2& pos){
    // todo make into canvas view conversion
    glm::vec2 returnpos= pos ;//
    return returnpos;
    
}


