//#define RENDER

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
    float data;
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
    Exporter mExpScan;
    Exporter mExpScanSum;
    
    VboSet vbo_ptcl;
    
    gl::VboMeshRef bridge;
    int idump = 24 - 50 -12;
    int idumpMax = 524;
    
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
    
    vec3 eye = vec3(0,0,800);
    float fov = 55;
    
    float scan = 0.0f;
    
    VboSet vScanFace;
    VboSet vScanSum;
    
    vector<vector<float>> hist;
    
};

void cApp::setup(){
    setWindowPos( 0, 0 );
    setWindowSize( 1920, 1080 );
    mExp.setup( 1920, 1080, 0, 1000, GL_RGB, mt::getRenderPath(), 0, "3d_");
    mExpScan.setup( 1920, 1080, 0, 1000, GL_RGB, mt::getRenderPath(), 0, "scan_");
    mExpScanSum.setup( 1920, 1080, 0, 1000, GL_RGB, mt::getRenderPath(), 0, "scanSum_");

    cam = CameraPersp(1920, 1080, fov, 1, 100000 );
    cam.lookAt( eye, vec3(0,0,0) );
    camUi.setCamera( &cam );
    
    setupGui();
    
    mPln.setSeed(123);
    mPln.setOctaves(4);
    
    boxelx = boxely = boxelz = 400;
    
    //loadSimulationData( idump );
    
    hist.assign(400, vector<float>() );
    for(int i=0; i<400; i++){
        hist[i].assign(400, 0.0f);
    }
    
#ifdef RENDER
    bStart = true;
    mExp.startRender();
    mExpScan.startRender();
    mExpScanSum.startRender();
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
            scan = (idump-24)/(float)(idumpMax-24);
            vbo_ptcl.resetPos();
            vbo_ptcl.resetCol();
            vbo_ptcl.resetVbo();
            
            vScanFace.resetPos();
            vScanFace.resetCol();
            vScanFace.resetVbo();
            
            vScanSum.resetPos();
            vScanSum.resetCol();
            vScanSum.resetVbo();
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
    
    float alpha = 0.2f;
    
    concurrent_vector<Particle> tp;
    concurrent_vector<Particle> tp_s;

    std::function<void(int i)> func = [&](int i){
        float x = i % boxelx;
        float y = (i / boxelx) % boxely;
        float z = i / (boxelx*boxely);
        int scanLine = boxelx*(1.0f-scan);
        
        double rho_raw = rho[i];
        float rhof;

        int scanFaceType;

        if( scanLine < z){
            scanFaceType = 0;           // lower than face, visible
        }else if( scanLine == z){
            scanFaceType = 1;           // here is face
        }else if( scanLine > z){
            scanFaceType = 2;           // upper then face, invisible
            return;
        }
    
        bool logarizm = true;
        double logLevel = 10.0;
        if( logarizm ){
            double rho_map = lmap(rho_raw, in_min, in_max, 1.0, pow(10.0,logLevel));
            rhof = log10(rho_map)/logLevel;
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
                    
                    vec3 vr = glm::rotate( v, toRadians(angle2), axis2 );
                    vr = glm::rotate( vr, toRadians(angle1), axis1 );
                    
                    ColorAf c;
                    float hue = lmap(rhof, 0.0f, 1.0f, 0.04f, 0.9f);
                    c.set(CM_HSV, vec4(hue, 1.0f, 1.0f, alpha) );
                    
                    Particle p;
                    p.pos = vr;
                    p.col = c;
                    p.dist = glm::distance(eye, vr);
                    tp.push_back( p );
                }
            }
        }else{
            
            //float threshold = 0.0101f;
            float threshold = 0.7f;
            if( threshold<rhof){
                vec3 noise = mPln.dfBm(x, y, z)*0.3f;
                vec3 v = vec3(x, y, z) + vec3(-200,-200,-200) + noise;
                
                ColorAf c;
                float hue = lmap(rhof, threshold, 1.0f, 0.03f, 0.3f);
                
                switch (scanFaceType) {
                    case 0:
                        c.set(CM_HSV, vec4(hue, 1.0f, 1.0f, alpha) );
                        break;
                        
                    case 1:
                        c = ColorAf( 0.3+rhof*0.5f, 0.0f, 0.0f, 1.0f );
                        break;
                        
                    case 2:
                        c.set(CM_HSV, vec4(hue, 1.0f, 1.0f, 0.006f) );
                        break;
                }
                
                vec3 vr = glm::rotate( v, toRadians(angle2), axis2 );
                vr = glm::rotate( vr, toRadians(angle1), axis1 );
                
                Particle p;
                p.pos = v;
                p.col = c;
                p.dist = glm::distance(eye, vr);
                p.data = rhof;
                
                tp.push_back( p );
                
                if(scanFaceType == 1){
                    float w = lmap(rhof, threshold, 1.0f, 0.3f, 1.0f);
                    c = ColorAf( w, w, w, 0.8f );

                    Particle ps;
                    ps.pos = v;
                    ps.col = c;
                    ps.dist = p.dist;
                    ps.data = rhof;
                    tp_s.push_back(ps);
                }
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
            vec3 vr = glm::rotate( p.pos, toRadians(angle2), axis2 );
            vr = glm::rotate( vr, toRadians(angle1), axis1 );
            
            vbo_ptcl.addPos( vr );
            vbo_ptcl.addCol( p.col );
            
            int x = p.pos.x + 200;
            int z = p.pos.z + 200;
            float d = p.data;
            hist[z][x] += d;
        }

        vbo_ptcl.init(GL_POINTS);

        // face
        for( int i=0; i<tp_s.size(); i++){
            Particle & p = tp_s[i];
            vScanFace.addPos( p.pos );
            vScanFace.addCol( p.col );
        }
        vScanFace.init(GL_POINTS);

        // sum
        for( int i=0; i<hist.size(); i++){
            for( int j=0; j<hist[i].size(); j++){
                float x = i-200;
                float y = j-200;
                float d = hist[i][j] * 0.01f;
                vScanSum.addPos(vec3(-x,y,0) );
                //vScanSum.addCol(ColorAf(CM_HSV, d, 1.0f, 1.0f, 0.2f+d) );
                vScanSum.addCol(ColorAf(d, d, d, 0.1+d) );
            }
        }
        vScanSum.init(GL_POINTS);
        
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
    
        glLineWidth( 1 );

        glPointSize( 1 );
        vbo_ptcl.draw();
        
        {
            gl::rotate( toRadians(angle1), axis1 );
            gl::rotate( toRadians(angle2), axis2 );
            
            if( !mExp.bSnap && !mExp.bRender ){
                mt::drawCoordinate( -200 );
            }

            float w = 0.75f;
            gl::color(w,w,w);
            gl::drawStrokedCube( vec3(0,0,0), vec3(400,400,400) );

            if(scan>0){
                // draw scan face
                gl::translate(0,0,200.0f-400.0f*scan);
                gl::color(1, 1, 1);
                gl::drawStrokedRect(Rectf(vec2(-200, -200), vec2(200, 200)));
                gl::color(0.25f,0.25f, 0.25f, 0.4f);
                gl::drawSolidRect(Rectf(vec2(-200, -200), vec2(200, 200)));
            }
        }
        
        gl::disableDepthWrite();
        gl::disableDepthRead();
        
        
    }mExp.end();
    
    mExpScan.beginOrtho(true, false);{
        gl::clear();
        gl::rotate(toRadians(-90.0f));
        gl::rotate(toRadians(180.0f), 1, 0, 0);
        //gl::scale(2.0,2.0,1);
        gl::scale(1.5, 1.5, 1);
        //mt::drawCoordinate( -200 );
        vScanFace.draw();
        gl::translate(0.5, 0.5, 0);
        vScanFace.draw();
    }
    mExpScan.end();

    mExpScanSum.beginOrtho(true, false);{
        gl::clear();
        gl::pointSize(2);
        gl::scale(1.5, 1.5, 1);
        vScanSum.draw();
        gl::translate(0.3, 0, 0);
        vScanSum.draw();

        gl::translate(0, 0.3, 0);
        vScanSum.draw();

        float x = -200.0f+400.0f*scan+1;
        gl::color(0.7, 0.7,0.7,0.7);
        gl::drawLine(vec3(x, -250, 0), vec3(x, 250, 0));
    }mExpScanSum.end();
    
    mExp.draw();
    //mExpScan.draw();
    gui->draw();
}

void cApp::keyDown( KeyEvent event ) {
    char key = event.getChar();
    switch (key) {
        case 's':{
            mExp.snapShot("idump_"+toString(idump)+"_3d.png");
            mExpScan.snapShot("idump_"+toString(idump)+"_scanFace.png");
            mExpScanSum.snapShot("idump_"+toString(idump)+"_scanFaceSum.png");
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
