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
    void makeVbo();
    
    bool bStart = false;
    bool bScanDir;
    bool bRotate = true;
    bool bDrawScan = true;

    int idump;  // =300; //24 - 50 -12;
    int idumpMax = 524;
    int boxelx, boxely, boxelz;
    int scanY = 0;
    int scanZ = 0;
    int speed =  16; // 25 ystep
    int speedZ = 20; // 20 zstep
    int frame = 0;

    // rotation settings
    float angle1 = 90;
    float angle2 = 0;
    float angleSpd1 = 0;
    float angleSpd2 = -0.5f;
    vec3 axis1 = vec3(0.5, 0.7, -0.21);
    vec3 axis2 = vec3(0.22, -0.16, 1.0);

    // camera
    vec3 eye;
    float fov = 55;

    vector<double> rho;

    params::InterfaceGlRef gui;
    CameraUi camUi;
    CameraPersp cam;
    Perlin mPln;
    
    Exporter mExp;
    Exporter mExpScan;
    
    VboSet vbo_ptcl;
    VboSet vScanLine;
};

void cApp::setup(){
    setWindowPos( 0, 0 );
    
    if(1){
        setWindowSize( 1920, 1080 );
        mExp.setup( 1920, 1080, 0, 1000, GL_RGB, mt::getRenderPath(), 0, "3d/3d_");
        mExpScan.setup( 1920, 1080, 0, 1000, GL_RGB, mt::getRenderPath(), 0, "scan/scan_");

        cam = CameraPersp(1920, 1080, fov, 1, 100000 );
        eye = vec3(0,0,800);
        cam.lookAt( eye, vec3(0,0,0) );
        camUi.setCamera( &cam );
    }else{
        setWindowSize( 1080*3*0.5, 1920*0.5 );
        mExp.setup( 1080*3, 1920, 0, 1000, GL_RGB, mt::getRenderPath(), 0, "3d/3d_");
        if(bDrawScan)mExpScan.setup( 1080*3, 1920, 0, 1000, GL_RGB, mt::getRenderPath(), 0, "scan/scan_");
        cam = CameraPersp(1080*3, 1920, fov, 1, 100000 );
        eye = vec3(0,0,200);
        cam.lookAt( eye, vec3(0,0,0) );
        camUi.setCamera( &cam );
    }
    
    setupGui();
    
    mPln.setSeed(123);
    mPln.setOctaves(4);
    
    boxelx = boxely = boxelz = 400;
    
    //loadSimulationData( 24 );
    
    
#ifdef RENDER
    bStart = true;
    mExp.startRender();
    if(bDrawScan)mExpScan.startRender();
    
#endif
}

void cApp::setupGui(){

    gui = params::InterfaceGl::create( getWindow(), "settings", vec2(300,400) );
    
    gui->addText("main");
    gui->addParam("frame", &frame);
    gui->addParam("idunmp", &idump);

    gui->addParam("start", &bStart);

    gui->addParam("scanZ", &scanZ, true);
    gui->addParam("scanY", &scanY, true);
    
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
    
    scanZ = (frame*speed)/400.1f;
    scanZ *= speedZ;
    scanZ += speedZ;
    
    bScanDir = (scanZ/speedZ)%2==1;
    
    if (bScanDir) {
        scanY = (frame*speed)%401;
    }else{
        scanY = 400 - ((frame*speed)%401);
    }
    
    idump = frame+25;
    angle2 = angleSpd2*(frame+25);
    
    if( bStart ){
        
        vbo_ptcl.resetPos();
        vbo_ptcl.resetCol();
        vbo_ptcl.resetVbo();
        
        vScanLine.resetPos();
        vScanLine.resetCol();
        vScanLine.resetVbo();
        
        loadSimulationData( idump );
        makeVbo();
    }
}

void cApp::loadSimulationData( int idump){
    
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
    
    //rho.clear(); // need this?
    rho.assign(arraySize, double(0));
    is.read(reinterpret_cast<char*>(&rho[0]), fileSize);
    is.close();
}

void cApp::makeVbo(){
    double in_min = 4.255660e-26;
    double in_max = 1.584844e-18;
    cout << "in_min ~ in_max : " << in_min << " ~ " << in_max << endl;
    
    
    concurrent_vector<Particle> tp;
    concurrent_vector<Particle> tp_s;

    std::function<void(int i)> func = [&](int i){
        float x = i % boxelx;
        float y = (i / boxelx) % boxely;
        float z = i / (boxelx*boxely);

        float tz = 399-z;
        
        if( scanZ < tz ){
            return;
        }else if(scanZ-speedZ<=tz && tz<=scanZ){
            if( bScanDir ){
                if(scanY < y) return;
            }else{
                if(scanY > y) return;
            }
        }
        
        double rho_raw = rho[i];
        float rhof;

        double logLevel = 10.0;
        double rho_map = lmap(rho_raw, in_min, in_max, 1.0, pow(10.0,logLevel));
        rhof = log10(rho_map)/logLevel;
        
        float threshold = 0.65f;
        if( threshold<rhof){
            vec3 noise = mPln.dfBm(x, y, z)*0.3f;
            vec3 v = vec3(x, y, z) + vec3(-200,-200,-200) + noise;
            
            vec3 vr = v;
            if(bRotate){
                vr = glm::rotate( vr, toRadians(angle2), axis2 );
                vr = glm::rotate( vr, toRadians(angle1), axis1 );
            }
            
            float intst = lmap(rhof, threshold, 1.0f, 0.1f, 0.9f);
            
            ColorAf c;
            if( tz>=scanZ-speedZ ){
                c = ColorAf(0.1+intst,0,0,0.1f+intst);
            }else{
                c = ColorAf(intst, intst, intst, 0.1f+intst);
            }
            
            Particle p;
            p.pos = vr;
            p.col =  c;
            p.dist = glm::distance(eye, vr);
            tp.push_back( p );
            
            //
            if( bDrawScan && tz==scanZ ){
                c = ColorAf(intst, intst, intst, 0.1f+intst);
                Particle ps;
                ps.pos = v;
                ps.col =  c;
                ps.dist = glm::distance(eye, vr);
                tp_s.push_back(ps);
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

            Particle & p = tp[i];
            vbo_ptcl.addPos( p.pos );
            vbo_ptcl.addCol( p.col );
        }

        vbo_ptcl.init(GL_POINTS);

        if(bDrawScan){
            for( int i=0; i<tp_s.size(); i++){
                Particle & p = tp_s[i];
                vScanLine.addPos( p.pos );
                vScanLine.addCol( p.col );
            }
            
            vScanLine.init(GL_POINTS);
        }
        
        int totalPoints = vbo_ptcl.getPos().size();
        
        cout << "Particle     : " << totalPoints << "/" << rho.size() << "      "
             << "Visible Rate : " << (float)totalPoints/rho.size()*100.0f << endl;
    }
}

void cApp::draw(){
    
    angle2 = angleSpd2*idump;
    
    if(1){
        mExp.begin( camUi.getCamera() );
        
    }else{
        
        mExp.beginOrtho( true );
        gl::scale(1.5,1.5,1.5);
        float rad = atan(sqrt(2.0)/1.0);
        gl::rotate(rad, 1, 0, 0);
        gl::rotate(toRadians(45.0f), 0, 0, 1);
    }
    
    {
        gl::clear( ColorAf(0,0,0,1) );

        gl::enableAlphaBlending();
        gl::enableDepthRead();
        gl::enableDepthWrite();
    
        glLineWidth( 1 );

        glPointSize( 1 );
        vbo_ptcl.draw();
        
        {
            if(bRotate){
                gl::rotate( toRadians(angle1), axis1 );
                gl::rotate( toRadians(angle2), axis2 );
            }
            
            if( !mExp.bSnap && !mExp.bRender ){
                mt::drawCoordinate( -200 );
            }

            float w = 0.75f;
            gl::color(w,w,w);
            gl::drawStrokedCube( vec3(0,0,0), vec3(400,400,400) );

            if( frame>0){
                // draw scan line
                float z = 200.0f-scanZ-1;
                float y = -200.0f+scanY;
                gl::translate(0,0, z);
                gl::color(1, 0, 0, 0.8f);
                gl::drawLine(vec2(-200, y), vec2(200, y));

                gl::color(1, 0, 0, 0.5f);
                

                if(0){
                    float sy = (bScanDir==true?-200:200);
                    gl::drawStrokedRect( Rectf(vec2(-200,sy), vec2(200,y)));
                    gl::translate(0,0,speedZ);
                    gl::drawStrokedRect( Rectf(vec2(-200,sy), vec2(200,y)));
                }else{

                    float d = 400.0f;
                    float w = bScanDir==true?scanY:400-scanY;
                    float h = speedZ;
                    float y = bScanDir==true?w/2-200:-w/2+200;
                    gl::drawStrokedCube( vec3(0, y, speedZ/2), vec3(d,w,h) );

                }
            }
        }
        
        gl::disableDepthWrite();
        gl::disableDepthRead();
        
        
    }mExp.end();
    
    if(bDrawScan){
        mExpScan.beginOrtho(true, false);{
            gl::clear();
            gl::rotate(toRadians(-90.0f));
            gl::rotate(toRadians(180.0f), 1, 0, 0);
            //gl::scale(2.0,2.0,1);
            //mt::drawCoordinate( -200 );
            vScanLine.draw();
            gl::translate(0.5, 0.5, 0);
            vScanLine.draw();
        }
        mExpScan.end();
    }
    
    mExp.draw();
    gui->draw();
    
    if(bStart)
        frame++;
}

void cApp::keyDown( KeyEvent event ) {
    char key = event.getChar();
    switch (key) {
        case 's':{
            mExp.snapShot("idump_"+toString(idump)+"_3d.png");
            mExpScan.snapShot("idump_"+toString(idump)+"_scanFace.png");
            break;
        }
        case ' ': bStart = !bStart; break;
    }
}

void cApp::mouseDown( MouseEvent event ){
   camUi.mouseDown( event.getPos() );
}

void cApp::mouseDrag( MouseEvent event ){
    camUi.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
}

void cApp::resize(){
    CameraPersp & cam = const_cast<CameraPersp&>(camUi.getCamera());
    cam.setAspectRatio( mExp.mFbo->getAspectRatio());
    camUi.setCamera( &cam );
}

CINDER_APP( cApp, RendererGl(RendererGl::Options().msaa( 0 )) )
