#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/GeomIO.h"
#include "cinder/ImageIo.h"
#include "cinder/CameraUi.h"
#include "cinder/Rand.h"
#include "cinder/Perlin.h"
#include "Exporter.h"
#include "mtUtil.h"

#include "tbb/tbb.h"

//#include <iostream>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/moment.hpp>
using namespace boost::accumulators;

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace tbb;
using namespace boost;

class cApp : public App {
 public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyDown( KeyEvent event ) override;

    int frame = 0;
    Perlin mPln;

};

void cApp::setup(){
    setWindowSize( 1080*3*0.7, 1920*0.7 );
    setWindowPos(0, 0);
    
    mPln.setSeed( 345 );
    mPln.setOctaves( 4 );
    
    
    accumulator_set<double, stats<tag::mean, tag::moment<2> > > acc;
    
    // push in some data ...
    acc(1.2);
    acc(2.3);
    acc(3.4);
    acc(4.5);
    
    // Display the results ...
    std::cout << "Mean:   " << mean(acc) << std::endl;
    std::cout << "Moment: " << accumulators::moment<2>(acc) << std::endl;

}

void cApp::update(){
    
}

float z = 3;

void cApp::draw(){
    
    gl::clear( Color(0,0,0) );
    
    glPointSize(1);
    glLineWidth(1);
    gl::enableAlphaBlending();
    
    gl::pushMatrices();
    mt::setMatricesWindow(getWindowWidth(), getWindowHeight(), true, true, -2, 6);
    {
        mt::drawCoordinate(100);
        gl::translate(vec3(0,0,z));
        gl::color(1,0,0);
        gl::drawSolidCircle(vec2(100,100), 50);
    }
    gl::popMatrices();
    
    gl::pushMatrices();
    {
        gl::color(1, 1, 1);
        gl::drawString("fps      " + to_string( getAverageFps()),   vec2(20,20) );
        gl::drawString("frame    " + to_string( frame ),   vec2(20,35) );
        gl::drawString("z    " + to_string( z ),   vec2(20,55) );
    }
    gl::popMatrices();
    
    frame+=1;
}

void cApp::keyDown( KeyEvent event ){

    switch( event.getCode() ){
        case KeyEvent::KEY_RIGHT: z++; break;
        case KeyEvent::KEY_LEFT: z--; break;
            
    }
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 ) ) )
