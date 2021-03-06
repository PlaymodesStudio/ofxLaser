#pragma once

#include "ofMain.h"
#include "ofxLaserManager.h"
#include "ofxLaserDacEtherdream.h"
#include "ofxGui.h"
#include "ofxSvg.h"

class ofApp : public ofBaseApp{
	
public:
	void setup();
	void update();
	void draw();
	void exit();
	
	void keyPressed  (int key);
		
    ofParameter<int> currentSVG;
    ofParameter<float> scale;
    
    vector<ofxSVG> svgs;
    vector<string> fileNames; 
	
	ofxLaser::Manager laser;
    ofxLaser::DacEtherdream dac;
	
	int laserWidth;
	int laserHeight;
    
};

