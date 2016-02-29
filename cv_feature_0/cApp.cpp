#define RENDER

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
    void mouseDown( MouseEvent event ) override;
    void mouseDrag( MouseEvent event ) override;
    void resize() override;
        
    void open();
    void openDir();
    
    void makeFeature();
    
    int frame = 0;
    fs::path dir;
    
    Perlin mPln;
    Exporter mExp;
    
    Surface8u sur;
    vector<cv::KeyPoint> key;
    VboSet vbo;
    VboSet nline;
    
    CameraUi camUi;
    CameraPersp pcam;

};

void cApp::setup(){
    
    mPln.setSeed( 345 );
    mPln.setOctaves( 4 );
    
    openDir();
    
    fs::path path = dir/("f_00000.png");
    sur = Surface8u( loadImage( path) );
    int w = sur.getWidth();
    int h = sur.getHeight();
    
    pcam = CameraPersp(w, h, 50, 1, 10000);
    camUi.setCamera( &pcam );
    mExp.setup( w, h, 0, 3000-1, GL_RGB, mt::getRenderPath(), 0 );
    setWindowSize( w*0.5, h*0.5 );
    setWindowPos(0, 0);
    
    
#ifdef RENDER
    mExp.startRender();
#endif
}

void cApp::open(){
    //path = getOpenFilePath( "", ImageIo::getLoadExtensions() );
    //makeFeature();
}

void cApp::openDir(){
    dir = getFolderPath();
}

void cApp::makeFeature(){
    
    stringstream fst;
    fst << setfill('0') << setw(5) << frame;
    fs::path path = dir/("f_" + fst.str() + ".png");
    sur = Surface8u( loadImage( path) );
    
    cv::Mat input( toOcv(sur) );
    //cv::blur(input, input, cv::Size(3,3) );
    
    cv::cvtColor( input, input, CV_RGB2GRAY );
    cv::equalizeHist(input, input);
    cv::Canny( input, input, 10, 10, 3, false);
    //sur = fromOcv( input );
    
    //auto detector = cv::BRISK::create(10, 1, 1.f);
    auto detector = cv::ORB::create(100, 1.2f, 16, 0, 0, 4);
    //auto detector = cv::MSER::create( 5, 60, 14400);
    //auto detector = cv::FastFeatureDetector::create( 30, true, cv::FastFeatureDetector::TYPE_9_16 );
    detector->detect( input, key );
    
    {
        vbo.resetPos();
        vbo.resetCol();
        vbo.resetVbo();
        
        for( int i=0; i<key.size(); i++ ){
            cv::Point2f & p = key[i].pt;
            vbo.addPos( vec3(p.x, p.y, 0) );
            
            const ColorAf & c = sur.getPixel(vec2(p.x, p.y) );
            vbo.addCol( ColorAf( 0.8f-c.r*1.2f,0.8f-c.g*1.2f,0.8f-c.b*1.2, 0.3));
        }
        vbo.init( GL_POINTS );

        if(0){
            nline.resetPos();
            nline.resetCol();
            nline.resetVbo();
            
            const vector<vec3> & inpos = vbo.getPos();
            const vector<ColorAf> & incol = vbo.getCol();
            
            for( int i=0; i<5000; i++){
                
                int id1 = randInt(0,inpos.size());
                int id2 = randInt(0,inpos.size());
                const vec3 &p1 = inpos[id1];
                const vec3 &p2 = inpos[id2];
                
                float dist = glm::distance(p1, p2);
                if( 10<dist && dist<1300){
                    const ColorAf &c1 = incol[id1];
                    const ColorAf &c2 = incol[id2];
                    nline.addPos(p1);
                    nline.addPos(p2);
                    nline.addCol(c1);
                    nline.addCol(c2);
                }
            }
            
            nline.init(GL_LINES);
        }
        
        if(1){
            int num_line = 3;
            int num_dupl = 1;
            int size = key.size();
            float nlimit = 10;
            float flimit = 1000/2;
            const vector<vec3> & inpos = vbo.getPos();
            const vector<ColorAf> & incol = vbo.getCol();
            
            vector<vec3> outpos( size*(num_line*num_dupl)*2 );
            vector<ColorAf> outcol( size*(num_line*num_dupl)*2 );
            TbbNpFinder np;
            np.findNearestPoints(&inpos[0], &outpos[0], &incol[0], &outcol[0], size, num_line, num_dupl, nlimit, flimit);
            
            nline.resetPos();
            nline.resetCol();
            nline.resetVbo();
            nline.addPos(outpos);
            nline.addCol(outcol);
            nline.init(GL_LINES);
        }
    }
    
}

void cApp::update(){
    makeFeature();
}

void cApp::draw(){
    
    //gl::setMatrices( camUi.getCamera() );
    mExp.begin( camUi.getCamera() );
    {
        gl::clear( Color(0,0,0) );
        gl::pointSize(1);
        gl::lineWidth(1);
        gl::enableAlphaBlending();

        gl::color(1,1,1);
        gl::TextureRef tex = gl::Texture::create( sur );
        gl::draw( tex );
        
        // feature key point
        //gl::enableAdditiveBlending();
        nline.draw();
        vbo.draw();
    }
    mExp.end();
    
    mExp.draw();
    
    frame+=1;
}

void cApp::keyDown( KeyEvent event ){
    switch( event.getChar() ){
        case 'S': mExp.startRender(); break;
        case 's': mExp.snapShot(); break;
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
    cam.setAspectRatio( getWindowAspectRatio() );
    camUi.setCamera( &cam );
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 ) ) )
