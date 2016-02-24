//#define RENDER

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/GeomIO.h"
#include "cinder/ImageIo.h"
#include "cinder/CameraUi.h"
#include "cinder/Rand.h"
#include "cinder/Perlin.h"

#include "CinderOpenCv.h"

#include "TbbNpFinder.h"

#include "tbb/tbb.h"

#include "Exporter.h"
#include "VboSet.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace tbb;

class cApp : public App {
 public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyDown( KeyEvent event ) override;
    
    void open();
    void makeHisto();
    
    Exporter mExp;
    VboSet vHisto;
    fs::path path;
    Surface16u sur;

    int w = 1920;
    int h = 1080;

//    int w = 1080*3;
//    int h = 1920;

    int xoffset = 100;
    int yoffset = 100;
};

void cApp::setup(){
    
    mExp.setup( w, h, 0, 2, GL_RGB, mt::getRenderPath(), 0 );
    setWindowSize( w, h );
    setWindowPos(0, 0);
    
    open();
    
#ifdef RENDER
    mExp.startRender();
#endif
}

void cApp::open(){
    //path = mt::getAssetPath()/"img"/"arcsinh_0.0-3.0.tiff";
    //path = mt::getAssetPath()/"img"/"arcsinh_1.1-3.0.tiff";
    path = mt::getAssetPath()/"img"/"arcsinh_2.2-3.0.tiff";
    
    
    sur = Surface16u( loadImage(path));
    
    makeHisto();
}

void cApp::makeHisto(){
    
    int histSize = 5000; //65536;
    float range[] = {0, 65536};
    const float * histRange = {range};
    int channels[] = {0};
    bool uniform = true;
    bool accumulate = true;
    
    cv::Mat src = toOcv(sur);
    cv::Mat hist;
    cv::calcHist(&src, 1, channels, cv::Mat(), hist, 1, &histSize, &histRange, uniform, accumulate);
    
    double max = 65536.0f*0.01f;
    //cv::minMaxLoc(hist, 0, &max, 0, 0);
    
    float xs = w - xoffset*2;
    float ys = h - yoffset*2;
    
    int res = 5;
    for( int i=0; i<histSize; i++){
        if(i%res!=0) continue;
        float val = hist.at<float>(i);
        float x = (float)i/histSize * xs;
        float y = (float)val/max    * ys;
        vHisto.addPos( vec3( x, 1+y, 0));
        vHisto.addPos( vec3( x, 0, 0));

        vHisto.addCol(ColorAf(0,0,0,1) );
        vHisto.addCol(ColorAf(0,0,0,1) );
    }

    vHisto.init(GL_LINES);
}

void cApp::update(){}

void cApp::draw(){
    
    gl::pointSize(1);
    gl::lineWidth(1);
    gl::enableAlphaBlending();
    
    mExp.beginOrtho(false, false);
    {
        gl::clear( Color(1,1,1) );
        gl::translate(xoffset, yoffset);
        vHisto.draw();
    }
    mExp.end();
    
    mExp.draw();
    
}

void cApp::keyDown( KeyEvent event ){
    switch( event.getChar() ){
        case 'S': mExp.startRender(); break;
        case 's': mExp.snapShot(); break;
    }
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 ) ) )
