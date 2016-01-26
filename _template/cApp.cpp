#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/GeomIO.h"
#include "cinder/ImageIo.h"
#include "cinder/CameraUi.h"
#include "cinder/Rand.h"
#include "cinder/Perlin.h"
#include "Exporter.h"
#include "tbb/tbb.h"

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

    int frame = 0;
    
    Perlin mPln;
    Exporter mExp;
};

void cApp::setup(){
    mExp.setup(1080*3, 1920, 0, 100-1, GL_RGB, mt::getRenderPath(), 0, "test" );
    setWindowSize( 1080*3*0.7, 1920*0.7 );
    setWindowPos(0, 0);
    
    mPln.setSeed( 345 );
    mPln.setOctaves( 4 );
}

void cApp::update(){
    
    function<void(int i)> func = [&](int i){
    };
    
    int n = 100;
    if(1){
        parallel_for( blocked_range<size_t>(0, n), [&](const blocked_range<size_t>r){
            for( int i=r.begin(); i!=r.end(); i++){ func(i); }
        });
    }else{
        for( int i=0; i<n; i++){ func(i); }
    }
}

void cApp::draw(){
    glPointSize(1);
    glLineWidth(1);
    gl::enableAlphaBlending();
    
    mExp.beginPersp();
    {
        gl::clear( Color(0,0,0) );
        gl::color(1,0,0);
        gl::drawSolidCircle(vec2(100,100), 50);
    }
    mExp.end();
    
    mExp.draw();
    
    gl::pushMatrices();
    {
        gl::color(1, 1, 1);
        gl::drawString("fps      " + to_string( getAverageFps()),   vec2(20,20) );
        gl::drawString("frame    " + to_string( frame ),   vec2(20,35) );
    }
    gl::popMatrices();
    
    frame+=1;
}

void cApp::keyDown( KeyEvent event ){
    switch( event.getChar() ){
        case 'S': mExp.startRender(); break;
        case 's': mExp.snapShot(); break;
    }
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 ) ) )
