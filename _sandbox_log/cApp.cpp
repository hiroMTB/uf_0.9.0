#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/GeomIO.h"
#include "cinder/ImageIo.h"
#include "cinder/CameraUi.h"
#include "cinder/Rand.h"
#include "cinder/Perlin.h"
#include "Exporter.h"
#include "VboSet.h"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/moment.hpp>
#include <boost/accumulators/statistics/tail_quantile.hpp>
#include <boost/ref.hpp>
#include <boost/bind.hpp>

#include "mtUtil.h"
#include "VboSet.h"

using namespace boost::accumulators;

using namespace ci;
using namespace ci::app;
using namespace std;

class cApp : public App {
 public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyDown( KeyEvent event ) override;

    VboSet vOrig;
    VboSet vMap;
    VboSet vLog;
    
    
    vector<float> dOrig;
    vector<float> dMap;
    vector<float> dLog;
    
    float rMin;
    float rMax;
    
    int dataSize = 10000;
    float scale = 500.0f;
};

void cApp::setup(){
    setWindowSize( 1920, 1080);
    setWindowPos(0, 0);

    // model data
    for( int i=0; i<dataSize; i++){
        float d = randFloat(223.0, 3400);
        dOrig.push_back(d);
        
        float x = (float)i/dataSize;
        vOrig.addPos(vec3(x,d/3400.0f,0)*scale);
        vOrig.addCol(ColorAf(0,0,1,1));
    }
    vOrig.init(GL_POINTS);
    
    std::sort( dOrig.begin(), dOrig.end());
    
    accumulator_set<float, stats<tag::min, tag::max, tag::mean, tag::median, tag::sum> > acc;
    std::for_each( dOrig.begin(), dOrig.end(), bind<void>( boost::ref(acc), _1) );
    
    // calc statics
    rMin = extract::min(acc);
    rMax = extract::max(acc);

    // linear map
    if( 1 ){
        for( int i=0; i<dOrig.size(); i++){
            float f = dOrig[i];
            f = lmap( f, rMin, rMax, 0.0f, 1.0f);
            dMap.push_back(f);
        }
        
        for(int i=0; i<dMap.size(); i++){
            float x = (float)i/dataSize;
            vMap.addPos(vec3(x, dMap[i], 0) * scale );
            vMap.addCol(ColorAf(0,0,0,1));
        }
        
        vMap.init(GL_POINTS);
    }
    

}


void cApp::update(){
    // logmap
    if( 1 ){
        
        vLog.resetPos();
        vLog.resetCol();
        vLog.resetVbo();
        dLog.clear();
        
        for( int i=0; i<dOrig.size(); i++){
            float f = dOrig[i];
            float p = (float)getElapsedFrames()*0.02f + 1.0f;
            f = lmap( f, rMin, rMax, 10.0f, pow(10.0f, p));
            f = log10(f) / p;
            dLog.push_back(f);
        }
        
        for( int i=0; i<dLog.size(); i++){
            float x = (float)i/dataSize;
            vLog.addPos(vec3( x, dLog[i], 0) * scale);
            vLog.addCol(ColorAf(1,0,0,1));
        }
        vLog.init(GL_POINTS);
    }
    
}

void cApp::draw(){
    gl::pointSize(1);
    gl::lineWidth(1);
    gl::clear( Color(1,1,1) );

    mt::setMatricesWindow(getWindowWidth(), getWindowHeight(), true, false);
    
    gl::translate(-scale/2, -scale/2);
    mt::drawCoordinate(scale);
    vMap.draw();
    vOrig.draw();
    vLog.draw();
}

void cApp::keyDown( KeyEvent event ){
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 ) ) )
