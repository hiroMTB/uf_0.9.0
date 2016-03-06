#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/GeomIO.h"
#include "cinder/ImageIo.h"
#include "cinder/Camera.h"
#include "cinder/CameraUi.h"
#include "cinder/Rand.h"
#include "cinder/Perlin.h"
#include "cinder/Surface.h"
#include "cinder/gl/Texture.h"
#include "cinder/qtime/QuickTimeGl.h"
#include "tbb/tbb.h"

#include "Exporter.h"
#include "VboSet.h"
#include "Heatmap.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace tbb;

class Particle{
    
public:
    vec3 pos;
    ColorAf col;
    float dist;
    float val;
};

class cApp : public App {
 public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyDown( KeyEvent event ) override;
    void mouseDown( MouseEvent event ) override;
    void mouseDrag( MouseEvent event ) override;

    
    bool bStart = false;
    int frame = 0;
    
    Exporter mExp;
    VboSet vbo;
    vector<Particle> parts;
    
    Perlin mPln;
    Surface8uRef sur;
    qtime::MovieSurfaceRef mov;
    
    CameraPersp cam;
    CameraUi camui;
    vec3 eye;
};

void cApp::setup(){
    mExp.setup(1920, 1080, 0, 100-1, GL_RGB, mt::getRenderPath(), 0, "test" );
    setWindowSize( 1920, 1080 );
    setWindowPos(0, 0);
    
    mPln.setSeed( 345 );
    mPln.setOctaves( 4 );
    
    cam = CameraPersp(1920, 1080, 55, 0.1, 100000);
    eye = vec3(0,0,1000);
    cam.lookAt(eye, vec3(0,0,0));
    camui.setCamera(&cam);
}

void cApp::update(){

    if( !bStart ) return;
    
    parts.clear();
    vbo.resetAll();
    
    if(0){
        if(!mov){
            fs::path path = mt::getAssetPath()/"sim"/"supernova"/"2d"/"mov"/"7.1_simu_5_c_linear_rect.mov";
            mov = qtime::MovieSurface::create( path );
            mov->seekToStart();
            mov->play();
        }
        mov->seekToFrame(frame);
        sur = mov->getSurface();
    }else{
        fs::path path = mt::getAssetPath()/"sim"/"supernova"/"2d"/"img"/"simu_1"/"c"/"polar"/"linear"/"simu_1_idump100_c_linear_polar.png";
        //fs::path path = mt::getAssetPath()/"sim"/"supernova"/"2d"/"img"/"test.png";
        sur = Surface8u::create( loadImage(path) );
    }
    
    if(sur){
        frame++;

        Surface8u::Iter itr = sur->getIter();
        while (itr.line()) {
            while (itr.pixel()) {
                
                vec2 pos = itr.getPos();
                pos.x -= itr.getWidth()/2;
                pos.y -= itr.getHeight()/2;

                float val = itr.r()/255.0f;
                float min = 0.4f;
                float max = 0.99999f;
                if( min < val && val < max ){
                    float gray = lmap(val, min, max, 0.3f, 1.0f);
                    Particle pt;
                    pt.pos = vec3(pos.x, pos.y, gray*200.0f) + mPln.dfBm(frame*0.0001f, pos.x*0.001f, pos.y*0.001f)*0.3f;
                    pt.dist = glm::distance(eye, pt.pos);
                    pt.val = val;
                    
                    //pt.col = Colorf(gray,gray,gray);
                    pt.col = mt::getHeatmap( gray );
                    parts.push_back(pt);
                    
                    if(0){
                        for( int k=0; k<round(pt.pos.z); k+=5){
                            vec3 pp = pt.pos;
                            pp.z = k;
                            Particle pt;
                            pt.pos = pp + mPln.dfBm(frame*0.0001f, pos.x*0.001f, pos.y*0.001f)*0.3f;
                            pt.dist = glm::distance(eye, pp);
                            pt.val = val;
                            //pt.col = Colorf(gray,gray,gray);
                            pt.col = mt::getHeatmap( gray );
                            pt.col.a = k*0.01;
                            parts.push_back(pt);
                        }
                    }
                }
            }
        }
        
        std::sort(parts.begin(), parts.end(), [](const Particle&lp, const Particle&rp){ return lp.dist > rp.dist; } );
        
        for( int i=0; i<parts.size(); i++){
            vbo.addPos(parts[i].pos);
            vbo.addCol(parts[i].col);
        }
        vbo.init(GL_POINTS);
    }
}

void cApp::draw(){
    glPointSize(1);
    glLineWidth(1);
    gl::enableAlphaBlending();
    
    mExp.begin(camui.getCamera());{
        gl::clear();
        mt::drawCoordinate(100);
        vbo.draw();
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
    
}

void cApp::keyDown( KeyEvent event ){
    switch( event.getChar() ){
        case 'S': mExp.startRender(); break;
        case 's': mExp.snapShot(); break;
        case ' ': bStart = !bStart; break;
    }
}

void cApp::mouseDown( MouseEvent event ){
    camui.mouseDown( event.getPos() );
}

void cApp::mouseDrag( MouseEvent event ){
    camui.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 ) ) )
