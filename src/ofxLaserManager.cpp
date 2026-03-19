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
        createDefaultCanvasZone();
        std::shared_ptr<InputZone> zone = canvasTarget->getInputZoneForZoneIndex(0);
        addZoneToLaser(zone->zoneId, 0);
    }
    
    selectedLaserIndex = 0;
    
    dacSettingsTimeSlice.set("Magnification", 0.5, 0.1, 20);
        
    params.add(customParams);
}

Manager::~Manager() {
    
}


void Manager :: resetAllLasersToDefault() {
    ManagerBase::resetAllLasersToDefault();
    setSelectedLaserIndex(0);
    createDefaultCanvasZone();
}

void Manager :: createAndAddLaser()  {
    int laserindex = lasers.size();
    ManagerBase:: createAndAddLaser();
    setSelectedLaserIndex(laserindex);
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
    
    params.add(globalLatency.set("Latency (ms)", 150,30,400));

    params.add(showCustomParametersWindow.set("showCustomParametersWindow", true));
    params.add(showLaserOverviewWindow.set("showLaserManagementWindow", true));
    params.add(showLaserOutputSettingsWindow.set("showLaserOutputSettingsWindow", true));

    loadSettings();
    
    //updates zone settings and global latency on all
    updateLatencyToLasers();

    ofAddListener(params.parameterChangedE(), this, &Manager::paramChanged);
    
    copyParams.add(copyZonePositions.set("Copy output zone positions", false));
    copyParams.add(copyScannerSettings.set("Copy scanner / speed settings", false));
    copyParams.add(copyColourSettings.set("Copy colour settings", false));
    copyParams.add(copyAdvancedSettings.set("Copy advanced settings", false));

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
    
    selectedLaserIndex = 0;

    return success;
    
}


void Manager :: paramChanged(ofAbstractParameter& e) {
    updateLatencyToLasers();

    scheduleSaveSettings();
}
void Manager :: updateLatencyToLasers() {
    for(std::shared_ptr<Laser>& laser : lasers) {
        laser->maxLatencyMS = globalLatency;
    }
    
}


void Manager :: update() {
 
    ManagerBase :: update();
    
    if(laserToDelete!=nullptr) {
        deleteLaser(laserToDelete);
        laserToDelete = nullptr; 
    }
}

bool Manager :: deleteLaser(std::shared_ptr<Laser>& laser) {
    
    int laserindex = getLaserIndex(laser);
    
    bool success = ManagerBase::deleteLaser(laser);
    if(success) {
        if(selectedLaserIndex>=getNumLasers()) selectedLaserIndex = getNumLasers()-1;
        
        return true;
    } else {
        return false;
    }
    
}

void Manager::selectNextLaser() {
    if(lasers.size()>1) {
        int next = selectedLaserIndex+1;
        if(next>=(int)lasers.size()) next=0;
        
        setSelectedLaserIndex(next);
    }
}

void Manager::selectPreviousLaser() {
    int prev = selectedLaserIndex-1;
    if(prev<0) prev=(int)lasers.size()-1;
    setSelectedLaserIndex(prev);
    
}

bool Manager::setSelectedLaserIndex(int i){
    if((selectedLaserIndex!=i) && (i<getNumLasers())) {
        selectedLaserIndex = i;
        return true;
    } else {
        return false;
    }
}

bool Manager::selectAndShowLaser(int i) {
    bool changed = setSelectedLaserIndex(i);
    return changed;
    
}

int Manager::getSelectedLaserIndex(){
    return selectedLaserIndex;
}
std::shared_ptr<Laser> Manager::getSelectedLaser() {
    
    if((selectedLaserIndex>=0) && (selectedLaserIndex<lasers.size())) {
        return lasers.at(selectedLaserIndex);
    } else {
        return nullptr;
    }
}
    
int Manager::getLaserIndex(std::shared_ptr<Laser>& laser) {
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


bool Manager :: deleteCanvasZone(std::shared_ptr<InputZone> inputZone) {
    ZoneId zoneid = inputZone->zoneId;
    if(ManagerBase::deleteCanvasZone(inputZone) ) {
        for(std::shared_ptr<Laser>& laser : lasers) {
            laser->removeZone(zoneid);
            
        }
        scheduleSaveSettings();
        return true;
    } else {
        return false;
    }
    
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


//
//void Manager :: guiZoneSettings() {
//
//    int sourceindex = -1;
//    int targetindex = -1;
//    bool autosort = false;
//    bool labelchanged = false;
//    
//    static char newZoneLabel[255];
//    if(showBeamZoneSortWindow) {
//        if(UI::startWindow("Re-order beam zones", ImVec2(800+guiSpacing, guiSpacing+menuBarHeight), ImVec2(380,500), ImGuiWindowFlags_None, false, &showBeamZoneSortWindow)) {
//            
//            
//           
//            ImGui::Text("Drag and drop to re-order beam zones");
//            
//            //        ImGui::Columns(2);
//            //        ImGui::SetColumnWidth(0, 330);
//            //vector<int>& customOrder = zoneChaseSettings.customOrder;
//            
//            //vector<int>& activeZoneNumbers = selectedClip->getActiveZoneNumbers();
//            int numzones = beamZoneContainer.getNumZoneIds();
//            
//            for (int n = 0; n < numzones; n++) {
//                
//                bool orphan = false;
//                
//                ImGui::PushID(n);
//                int item = n;// customOrder[n];
//                ZoneId& zoneid = beamZoneContainer.getBeamZoneAtIndex(n)->zoneId;
//                int laserindexforzone = getLaserIndexForBeamZoneId(zoneid);
//                
//                if(laserindexforzone <0) {
//                    ofLogError("Zone not allocated to laser!");
////                    ImGui::PopID();
////                    continue;
//                    orphan = true;
//                }
//                
//                //int zonenumber = activeZoneNumbers[item];
//                string label =  "Laser " + ofToString(laserindexforzone+1)+" "+ ofToString(ICON_FK_ARROW_RIGHT) +" "+ zoneid.getLabel();
//                if(orphan) ImGui::PushStyleColor(ImGuiCol_ChildBg, {255,0,0,255});
//                
//                ImGui::Text("%d", n+1); ImGui::SameLine();
//                
//                ImGui::SetCursorPosX(24);
//                if(orphan) {
//                    ImGui::Text("ORPHAN"); ImGui::SameLine();
//                    
//                    ImGui::PopStyleColor();
//                }
//                if(ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_None, ImVec2(130,19))) {
//                    
//                }
//                if(ImGui::IsItemClicked()) {
//                    ofLogNotice("CLICKED");
//                    setSelectedLaserIndex(laserindexforzone);
//                    viewMode = OFXLASER_VIEW_OUTPUT;
//                }
//                
//                // make it a drag source
//                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
//                {
//                    ImGui::SetDragDropPayload("MOVE_BEAM_ZONE", &n, sizeof(int));    // Set payload to carry the index of our item (could be anything)
//                    
//                    ImGui::Text("Move %s", label.c_str());
////                    setSelectedLaserIndex(laserindexforzone);
////                    viewMode = OFXLASER_VIEW_OUTPUT;
//                    
//                    ImGui::EndDragDropSource();
//                }
//                
//                if (ImGui::BeginDragDropTarget())
//                {
//                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MOVE_BEAM_ZONE"))
//                    {
//                        IM_ASSERT(payload->DataSize == sizeof(int));
//                        int payload_n = *(const int*)payload->Data;
//                        
//                        // DO THE SWAP
//                        ofLogNotice("moving : ") << payload_n << " to " << n << " ";
//                        sourceindex = payload_n;
//                        targetindex = n;
//                    }
//                    ImGui::EndDragDropTarget();
//                }
//                               
//                ImGui::PopID();
//                
//                
//                
//            }
//        }
//        if(ImGui::Button("Autosort by laser")) {
//
//            autosort = true;
//            setSelectedLaserIndex(0);
//        }
//   
//        UI::endWindow();
//    }
//    
//    if(autosort) {
//        int numzones = beamZoneContainer.getNumZoneIds();
//        
//        for(int i = 1; i<numzones; i++) {
//            int targetslot = i;
//            int laserindex1 = getLaserIndexForBeamZoneId(beamZoneContainer.getObjectAtIndex(i)->zoneId);
//            
//            for(int j = i-1; j>=0; j--) {
//                int laserindex2 = getLaserIndexForBeamZoneId(beamZoneContainer.getObjectAtIndex(j)->zoneId);
//                if(laserindex2>laserindex1) targetslot = j;
//                // if value at j> value at i, then target becomes j
//            }
//            
//            moveBeamZoneToIndex(i, targetslot);
//            
//        }
//        saveSettings();
//        
//    }
//    if(sourceindex>-1) {
//        moveBeamZoneToIndex(sourceindex, targetindex);
//        saveSettings();
//    }
//    
//    if(labelchanged) {
//        updateZoneLabels();
//        saveSettings();
//    }
//        
//}







//
//void Manager::ShowExampleMenuFile() {
//
//    ImGui::MenuItem("(dummy menu)", NULL, false, false);
//    if (ImGui::MenuItem("New")) {}
//    if (ImGui::MenuItem("Open", "Ctrl+O")) {}
//    if (ImGui::BeginMenu("Open Recent"))
//    {
//        ImGui::MenuItem("fish_hat.c");
//        ImGui::MenuItem("fish_hat.inl");
//        ImGui::MenuItem("fish_hat.h");
//        if (ImGui::BeginMenu("More.."))
//        {
//            ImGui::MenuItem("Hello");
//            ImGui::MenuItem("Sailor");
//            if (ImGui::BeginMenu("Recurse.."))
//            {
//                ShowExampleMenuFile();
//                ImGui::EndMenu();
//            }
//            ImGui::EndMenu();
//        }
//        ImGui::EndMenu();
//    }
//    if (ImGui::MenuItem("Save", "Ctrl+S")) {}
//    if (ImGui::MenuItem("Save As..")) {}
//
//    ImGui::Separator();
//    if (ImGui::BeginMenu("Options"))
//    {
//        static bool enabled = true;
//        ImGui::MenuItem("Enabled", "", &enabled);
//        ImGui::BeginChild("child", ImVec2(0, 60), true);
//        for (int i = 0; i < 10; i++)
//            ImGui::Text("Scrolling Text %d", i);
//        ImGui::EndChild();
//        static float f = 0.5f;
//        static int n = 0;
//        ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
//        ImGui::InputFloat("Input", &f, 0.1f);
//        ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
//        ImGui::EndMenu();
//    }
//
//    if (ImGui::BeginMenu("Colors"))
//    {
//        float sz = ImGui::GetTextLineHeight();
//        for (int i = 0; i < ImGuiCol_COUNT; i++)
//        {
//            const char* name = ImGui::GetStyleColorName((ImGuiCol)i);
//            ImVec2 p = ImGui::GetCursorScreenPos();
//            ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x+sz, p.y+sz), ImGui::GetColorU32((ImGuiCol)i));
//            ImGui::Dummy(ImVec2(sz, sz));
//            ImGui::SameLine();
//            ImGui::MenuItem(name);
//        }
//        ImGui::EndMenu();
//    }
//
//    // Here we demonstrate appending again to the "Options" menu (which we already created above)
//    // Of course in this demo it is a little bit silly that this function calls BeginMenu("Options") twice.
//    // In a real code-base using it would make senses to use this feature from very different code locations.
//    if (ImGui::BeginMenu("Options")) // <-- Append!
//    {
//        static bool b = true;
//        ImGui::Checkbox("SomeOption", &b);
//        ImGui::EndMenu();
//    }
//
//    if (ImGui::BeginMenu("Disabled", false)) // Disabled
//    {
//        IM_ASSERT(0);
//    }
//    if (ImGui::MenuItem("Checked", NULL, true)) {}
//    if (ImGui::MenuItem("Quit", "Alt+F4")) {
//        ofExit();
//    }
//
//}
