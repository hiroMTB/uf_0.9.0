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

#include "mtUtil.h"
#include "Exporter.h"
#include "ContourMap.h"

//#define RENDER

using namespace ci;
using namespace ci::app;
using namespace std;

class cApp : public App {
    
public:
    void setup();
    void update();
    void draw();
    void drawScan();
    void drawAll();
    
    void keyDown( KeyEvent event );
    void mouseDown( MouseEvent event );
    void mouseDrag( MouseEvent event );
    
    int mWin_w = 1920;
    int mWin_h = 1080;
    int n_threshold;

    params::InterfaceGlRef mParams;
    Perlin mPln;
    vector<ContourMap> mCMaps;
    CameraPersp cam;
    CameraUi mCamUi;
    Exporter mExp;
    vector<fs::path> imgPaths;
    bool bStart = true;
    int frame = 0;
};

void cApp::setup(){
    
    mPln.setSeed( 123 );
    mPln.setOctaves( 3 );
    
    n_threshold = 0;
    setWindowPos( 0, 0 );
    setWindowSize( mWin_w, mWin_h );
    mExp.setup( mWin_w, mWin_h, 0, 3001, GL_RGB, mt::getRenderPath(), 0 );

    fs::path assetPath = mt::getAssetPath();

    // load image
    //imgPaths.push_back( assetPath/"img"/"01"/"vela_orient_red_pacs160_signal_full.tiff");
    imgPaths.push_back( assetPath/"img"/"01"/"BLAST-vela-250um_half.tiff");
    //imgPaths.push_back( assetPath/"img"/"01"/"vela_orient_red_pacs160_signal.tiff");
    //imgPaths.push_back( assetPath/"img"/"01"/"vela_scana_spire250_signal.tiff");
    //imgPaths.push_back( assetPath/"img"/"01"/"vela_scana_spire350_signal.tiff");
    //imgPaths.push_back( assetPath/"img"/"01"/"vela_scana_spire500_signal.tiff");

    for ( auto & path : imgPaths ) {
        Surface32f sur = Surface32f( loadImage(path) );
        ContourMap cm;
        cm.setImage( sur, true, cv::Size(1,1) );
        mCMaps.push_back( cm );
    }
    
    for( int i=0; i<20; i++)
        mCMaps[0].addContour( 0.25f+i*0.02f, randInt(0,3) );
    
    /*
    mCMaps[0].addContour( 0.05, randInt(0,3) );
    mCMaps[0].addContour( 0.08, randInt(0,3) );
    mCMaps[0].addContour( 0.10, randInt(0,3) );
    mCMaps[0].addContour( 0.13, randInt(0,3) );
    mCMaps[0].addContour( 0.17, randInt(0,3) );
    
    mCMaps[1].addContour( 0.1 , randInt(0,3));
    mCMaps[1].addContour( 0.14, randInt(0,3) );
    mCMaps[1].addContour( 0.18, randInt(0,3) );
    mCMaps[1].addContour( 0.21, randInt(0,3) );
    mCMaps[1].addContour( 0.25, randInt(0,3) );

    mCMaps[2].addContour( 0.1 , randInt(0,3) );
    mCMaps[2].addContour( 0.12, randInt(0,3) );
    mCMaps[2].addContour( 0.14, randInt(0,3) );
    mCMaps[2].addContour( 0.16, randInt(0,3) );
    mCMaps[2].addContour( 0.19, randInt(0,3) );

    mCMaps[3].addContour( 0.12, randInt(0,3) );
    mCMaps[3].addContour( 0.13, randInt(0,3) );
    mCMaps[3].addContour( 0.14, randInt(0,3) );
    mCMaps[3].addContour( 0.16, randInt(0,3) );
    mCMaps[3].addContour( 0.18, randInt(0,3) );
    
    mCMaps[4].addContour( 0.10, randInt(0,3) );
    mCMaps[4].addContour( 0.15, randInt(0,3) );
    mCMaps[4].addContour( 0.20, randInt(0,3) );
    mCMaps[4].addContour( 0.23, randInt(0,3) );
    mCMaps[4].addContour( 0.30, randInt(0,3) );
    mCMaps[4].addContour( 0.33, randInt(0,3) );
    mCMaps[4].addContour( 0.40, randInt(0,3) );
    */
     
    // Camera
    cam.setPerspective(60, mWin_w/(float)mWin_h, 1, 10000);
   	cam.lookAt( vec3( 0, 0, 1700 ), vec3(0,0,0) );
    mCamUi.setCamera( &cam );
    
#ifdef RENDER
    mExp.startRender();
#endif
}

void cApp::update(){
}

void cApp::draw(){
    gl::enableAlphaBlending();
    glPointSize(1);
    glLineWidth(1);
    
    mExp.begin( mCamUi.getCamera() ); {
        gl::clear();
        gl::translate(0, 300);
        //drawAll();
        drawScan();
    }mExp.end();

    mExp.draw();
    mt::drawScreenGuide();

    if(bStart) frame++;
}


void cApp::drawAll(){
    for(int j=0; j<mCMaps.size(); j++){
        gl::pushMatrices();
        gl::translate( 2000*(j-2), 0, 0 );
        for( int i=0; i<mCMaps[j].mCMapData.size(); i++ ){
            gl::color(j*0.2, 0.5+i*0.02, i*0.05);
            mCMaps[j].drawContourGroup(i);
        }
        gl::popMatrices();
    }
}

void cApp::drawScan(){
    
    float scanSpeed = 40;
    
    int mapId = -1;
    
    for( auto & map : mCMaps ){
        mapId++;
        gl::translate( 1480, 0, 0 );
        for( int i=0; i<map.mCMapData.size(); i++ ){
            
            gl::translate(0,0,50);
            
            ContourMap::ContourGroup &cg = map.mCMapData[i];
            int nVertex = 0;
            int totalVerts = 0;
            vec2 scanPoint;
            bool scanFinish = false;
            for( int j=0; j<cg.size(); j++ ){
                totalVerts += cg[j].size();
            }
            
            for( int j=0; j<cg.size(); j++ ){
                
                ContourMap::Contour & c = cg[j];

                gl::enableAlphaBlending();
                
                switch (j) {
                    case 0:
                    case 1:
                        glDisable( GL_BLEND );
                        break;
                        
                    case 2:
                        glEnable( GL_BLEND );
                        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
                        break;
                    case 3:
                        glEnable( GL_BLEND );
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

                        break;

                    case 4:
                    case 5:
                        gl::enableAlphaBlending();
                        break;

                    case 6:
                        glEnable( GL_BLEND );
                        glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
                        break;
                    
                    case 7:
                        glEnable( GL_BLEND );
                        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
                        break;

                    case 8:
                        glEnable( GL_BLEND );
                        glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR);
                        break;

                    default:
                        //gl::enableAlphaBlending();
                        break;
                }
                
                glPointSize(1);
                gl::begin( GL_POINTS );
                for( int k=0; k<c.size(); k++ ){
                    scanFinish = (j==cg.size()-1) && (k==c.size()-1);
                    
                    if( ++nVertex < frame*scanSpeed){
                        cv::Point2f & p = c[k];
                        gl::color( 0.2+i*0.05+mPln.noise(j)*0.3, 0.8-i*0.1+mapId*0.01, i*0.1+j*0.01+mPln.noise(i)*0.02, 0.1+MAX(k*0.005f,0.4f) );
                        gl::vertex( fromOcv(p) );
                        if( !scanFinish ){
                            scanPoint = fromOcv(p);
                        }
                        
                        if( frame > 600 ){
                            p.x += mPln.fBm( mapId*0.5, i*0.5, frame*0.005 )*7.0;
                            p.y += mPln.fBm( mapId*0.5, j*0.5, frame*0.005 )*5.0;
                        }
                    }else{
                        break;
                    }
                }
                gl::end();
            }
            
            if( scanFinish ){
                int scanId = (int)(frame*scanSpeed) % totalVerts;
                //scanPoint = fromOcv( *cg[0][scanId] );
            }
            
            //if( !scanFinish ){
            glLineWidth( 1 );
            gl::begin( GL_LINES );
            gl::vertex( scanPoint );
            scanPoint.y = -10000;
            gl::vertex( scanPoint );
            gl::end();
            //}
        }
    }
}

void cApp::keyDown( KeyEvent event ) {
    char key = event.getChar();
    switch (key) {
        case 'S':
            mExp.startRender();
            break;
        case 'T':
            mExp.stopRender();
            break;
            
        case 't':
            n_threshold++;
            n_threshold %= 8;
            break;
         
        case ' ':
            bStart = !bStart;
            break;
            
        case 'e':
            string epsPath = "../out/eps/";
            fs::create_directories( epsPath );
            for( int i=0; i<mCMaps.size(); i++){
                mCMaps[i].exportContour( epsPath+imgPaths[i].filename().string(), "eps" );
            }
            break;
        }
}

void cApp::mouseDown( MouseEvent event ){
    mCamUi.mouseDown( event.getPos() );
}

void cApp::mouseDrag( MouseEvent event ){
    mCamUi.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
}

CINDER_APP( cApp, RendererGl )
