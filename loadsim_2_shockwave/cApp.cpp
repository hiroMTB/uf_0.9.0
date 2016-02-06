#define RENDER

#include "cinder/app/App.h"
#include "cinder/Rand.h"
#include "cinder/Utilities.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/Texture.h"
#include "cinder/Camera.h"
#include "cinder/CameraUi.h"
#include "cinder/Perlin.h"
#include "cinder/params/Params.h"
#include "CinderOpenCv.h"

#include "tbb/tbb.h"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/moment.hpp>
#include <boost/accumulators/statistics/tail_quantile.hpp>
#include <boost/ref.hpp>
#include <boost/bind.hpp>

#include "mtUtil.h"
#include "ConsoleColor.h"
#include "Exporter.h"
#include "DataGroup.h"
#include "VboSet.h"

#include <iostream>
#include <fstream>

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace tbb;

class Particle{
public:
    vec3 pos;
    ColorAf col;
    float dist;
};


class cApp : public App {
    
public:
    void setup();
    void update();
    void draw();
    void mouseDown( MouseEvent event );
    void mouseDrag( MouseEvent event );
    void keyDown( KeyEvent event );
    void resize();
    void loadSimulationData( int idump );
    void setupGui();
    
    int boxelx, boxely, boxelz;
    
    CameraUi camUi;
    CameraPersp cam;
    Perlin mPln;
    
    Exporter mExp;
    VboSet vbo_ptcl;
    
    gl::VboMeshRef bridge;
    int idump = 24 - 50;
    
    bool bStart = false;

    float angle1 = 90;
    float angle2 = 0;
    float angleSpd1 = 0;
    float angleSpd2 = -0.5f;
    vec3 axis1 = vec3(0.5, 0.7, -0.21);
    vec3 axis2 = vec3(0.22, -0.16, 1.0);
    
    int rotateStartFrame = 25 + 50;
    
    params::InterfaceGlRef gui;
    
    vector< tuple<float, float, ColorAf> > thresholds = {
        { 0.000645,   0.00066},
        { 0.00067,  0.00072},
        { 0.00075,  0.0008 },
        { 0.01,     0.0102 },
        { 0.02,     0.022 },
        { 0.1,      0.24 },
        { 0.24,     1 }
    };
    
    vec3 eye = vec3(0,0,1350);
    float fov = 50;
    
};

void cApp::setup(){
    setWindowPos( 0, 0 );
    setWindowSize( 1920, 1080 );
    mExp.setup( 1920, 1080, 0, 1000, GL_RGB, mt::getRenderPath(), 0);
    
    cam = CameraPersp(1920, 1080, fov, 1, 100000 );
    cam.lookAt( eye, vec3(0,0,0) );
    camUi.setCamera( &cam );
    
    setupGui();
    
    mPln.setSeed(123);
    mPln.setOctaves(4);
    
    boxelx = boxely = boxelz = 400;
    
    //loadSimulationData( idump );
    
#ifdef RENDER
    bStart = true;
    mExp.startRender();
#endif
}

void cApp::setupGui(){

    gui = params::InterfaceGl::create( getWindow(), "settings", vec2(300,400) );
    
    gui->addText("main");
    gui->addParam("idum", &idump);
    gui->addParam("start", &bStart);

    gui->addText("Rotation1");
    gui->addParam("axis1", &axis1);
    gui->addParam("angle1", &angle1);
    gui->addParam("angleSpeed1", &angleSpd1);
    
    gui->addText("Rotation2");
    gui->addParam("axis2", &axis2);
    gui->addParam("angle2", &angle2);
    gui->addParam("angleSpeed2", &angleSpd2);

}

void cApp::update(){
   
    if( bStart ){
        idump++;
        if( 25<=idump ){
            vbo_ptcl.resetPos();
            vbo_ptcl.resetCol();
            vbo_ptcl.resetVbo();
            loadSimulationData( idump );
        }
    }
}

void cApp::loadSimulationData( int idump){
    
    angle2 = angleSpd2*idump;

    stringstream fileName;
    fileName << "sim/Heracles/simu_mach4_split/rho/rho_" << setw(3) << setfill('0') << idump << + ".bin";
    
    fs::path assetPath = mt::getAssetPath();
    string path = ( assetPath/fileName.str() ).string();
    cout << "loading binary file : " << path << endl;
    std::ifstream is( path, std::ios::binary );
    if(is){
        ccout::b("load OK binary file");
    }else{
        ccout::r("load ERROR bin file");
        quit();
    }
    
    // get length of file:
    is.seekg (0, is.end);
    int fileSize = is.tellg();
    is.seekg (0, is.beg);
    
    int arraySize = arraySize = fileSize / sizeof(double);      // 400*400*400 = 64,000,000
    
    vector<double> rho;
    rho.assign(arraySize, double(0));
    is.read(reinterpret_cast<char*>(&rho[0]), fileSize);
    is.close();
    
    /*
    double in_min = std::numeric_limits<double>::max();
    double in_max = std::numeric_limits<double>::min();
    
    for( auto &r : rho ){
        in_min = MIN( in_min, r);
        in_max = MAX( in_max, r);
    }
     */
    
    double in_min = 4.255660e-26;
    double in_max = 1.584844e-18;
    cout << "in_min ~ in_max : " << in_min << " ~ " << in_max << endl;
    
    float alpha = 0.3f;
    
    concurrent_vector<Particle> tp;

    std::function<void(int i)> func = [&](int i){
        double rho_raw = rho[i];
        float rhof;
        float x = i % boxelx;
        float y = (i / boxelx) % boxely;
        float z = i / (boxelx*boxely);
        
        bool logarizm = true;
        if( logarizm ){
            double rho_map = lmap(rho_raw, in_min, in_max, 1.0, 10.0);
            rhof = log10(rho_map);
        }else{
            double rho_map = lmap(rho_raw, in_min, in_max, 0.0, 1.0);
            rhof = rho_map;
        }

        if( 0 ){
            for( int t=0; t<thresholds.size(); t++ ){
                
                float low = std::get<0>(thresholds[t]);
                float high = std::get<1>(thresholds[t]);
                
                if( low<=rhof && rhof<high ){
                    vec3 noise = mPln.dfBm(x, y, z)*0.3f;
                    vec3 v = vec3(x, y, z) + vec3(-200,-200,-200)  + noise;
                    
                    v = glm::rotate( v, toRadians(angle2), axis2 );
                    v = glm::rotate( v, toRadians(angle1), axis1 );
                    
                    ColorAf c;
                    float hue = lmap(rhof, 0.0f, 1.0f, 0.04f, 0.9f);
                    c.set(CM_HSV, vec4(hue, 1.0f, 1.0f, alpha) );
                    
                    Particle p;
                    p.pos = v;
                    p.col = c;
                    p.dist = glm::distance(eye, v);
                    tp.push_back( p );
                    
                    break;
                }
            }
        }else{
            
            float threshold = 0.0101f;
            if( threshold<rhof){
                vec3 noise = mPln.dfBm(x, y, z)*0.3f;
                vec3 v = vec3(x, y, z) + vec3(-200,-200,-200) + noise;
                
                v = glm::rotate( v, toRadians(angle2), axis2 );
                v = glm::rotate( v, toRadians(angle1), axis1 );
                
                ColorAf c;
                float hue = lmap(rhof, threshold, 1.0f, 0.04f, 0.9f);
                c.set(CM_HSV, vec4(hue, 1.0f, 1.0f, alpha) );
                
                Particle p;
                p.pos = v;
                p.col = c;
                p.dist = glm::distance(eye, v);
                tp.push_back( p );
            }
        }
    };
    
    if( 1 ){
        parallel_for( blocked_range<size_t>(0,rho.size()), [&](const blocked_range<size_t> r){
            for( int i=r.begin(); i!=r.end(); i++){
                func(i);
            }
        } );
    }else{
        for( int i=0; i<rho.size(); i++){
            func(i);
        }
    }


    //
    //  we need sort depends on position between eye and particle pos.
    //  otherwise point is rendered with dark color from behind.
    //
    sort(
         tp.begin(),
         tp.end(),
         []( const Particle& l, const Particle& r){
             return l.dist > r.dist;
         }
    );
    
    //
    //  make vbo
    //
    {
        for( int i=0; i<tp.size(); i++){
            vbo_ptcl.addPos( tp[i].pos );
            vbo_ptcl.addCol( tp[i].col );
        }
        
        vbo_ptcl.init(GL_POINTS);
        
        int totalPoints = vbo_ptcl.getPos().size();
        
        cout << "Particle Visible/Total : " << totalPoints << "/" << arraySize << endl;
        cout << "Visible Rate           : " << (float)totalPoints/arraySize*100.0f << endl;
    }
}

void cApp::draw(){
    
    angle2 = angleSpd2*idump;
    
    mExp.begin( camUi.getCamera() );{
        
        gl::clear( ColorAf(0,0,0,1) );

        gl::enableAlphaBlending();
        gl::enableDepthRead();
        gl::enableDepthWrite();
    
//        if(bridge){
//            glLineWidth( 1 );
//            gl::draw( bridge );
//        }

        glLineWidth( 1 );
//        for( int i=0; i<mDataGroup.size(); i++){
//            if(mDataGroup[i].mLine) gl::draw( mDataGroup[i].mLine );
//        }

        glPointSize( 1 );
        vbo_ptcl.draw();
        
        {
            gl::rotate( toRadians(angle1), axis1 );
            gl::rotate( toRadians(angle2), axis2 );
            
            if( !mExp.bSnap && !mExp.bRender ){
                mt::drawCoordinate( -200 );
            }

            gl::color(1, 1, 1);
            gl::drawStrokedCube( vec3(0,0,0), vec3(400,400,400) );
        }
        
        gl::disableDepthWrite();
        gl::disableDepthRead();
        
    }mExp.end();
    mExp.draw();
    
    gui->draw();
    

}

void cApp::keyDown( KeyEvent event ) {
    char key = event.getChar();
    switch (key) {
        case 's': mExp.snapShot(); break;
        case ' ': bStart = !bStart; break;
    }
}

void cApp::mouseDown( MouseEvent event ){
   // camUi.mouseDown( event.getPos() );
}

void cApp::mouseDrag( MouseEvent event ){
    //camUi.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
}

void cApp::resize(){
    CameraPersp & cam = const_cast<CameraPersp&>(camUi.getCamera());
    cam.setAspectRatio( mExp.mFbo->getAspectRatio());
    camUi.setCamera( &cam );
}

CINDER_APP( cApp, RendererGl(RendererGl::Options().msaa( 0 )) )
