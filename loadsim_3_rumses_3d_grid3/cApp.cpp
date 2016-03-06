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
#include "cinder/Xml.h"

#include "ConsoleColor.h"
#include "Exporter.h"
#include "Ramses.h"
#include "mtUtil.h"
#include "VboSet.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class cApp : public App {
    
public:
    void setup();
    void update();
    void draw();
    void mouseDown( MouseEvent event );
    void mouseDrag( MouseEvent event );
    void keyDown( KeyEvent event );
    void resize();
    
    void makeGui();
    void saveXml();
    void loadXml();
    
    Perlin mPln;
    Exporter mExp;
    
    vector<Ramses> rms;
    bool bStart = false;
    bool bOrtho = false;
    int frame = 100;
    
    bool bZsort = false;
    
    params::InterfaceGlRef gui;
    
    vector<CameraPersp> cams;
    vector<CameraUi> camUis;
    
    VboSet line;
    
    int simType = 4;
    vec3 eye = vec3(900,0,0);
};

void cApp::setup(){
    setWindowPos( 0, 0 );

    float w = 1920;
    float h = 1080;
    
    setWindowSize( w*0.6, h*0.6 );
    mExp.setup( w, h, 0, 1000, GL_RGB, mt::getRenderPath(), 0 );
    
    cams.assign(6, CameraPersp());
    camUis.assign(6, CameraUi());
    
    for( int i=0; i<6; i++){
        cams[i] = CameraPersp(w, h, 55.0f, 1, 100000 );
        cams[i].lookAt( eye, vec3(0,0,0) );
        
        int col = i%3;
        int row = i/3;
        
        cams[i].setLensShift( -0.666+0.666*col, -0.5+row*1.0);
        camUis[i].setCamera( &cams[i] );
    }
    
    mPln.setSeed(123);
    mPln.setOctaves(4);
    
    for( int i=0; i<6; i++){
        rms.push_back( Ramses(simType,i) );
    }
    makeGui();
    
    
#ifdef RENDER
    mExp.startRender();
#endif
    
}


void cApp::update(){
    if( bStart ){
        for( int i=0; i<rms.size(); i++){
            if( rms[i].bShow ){
                bool loadok = rms[i].loadSimData( frame );
                if( loadok ){
                    rms[i].updateVbo( eye );
                }
            }
        }
    }
}

void cApp::draw(){
    
    gl::enableAlphaBlending();
    glPointSize(1);
    glLineWidth(1);
    
    //bOrtho ? mExp.beginOrtho( true ) : mExp.begin( camUi.getCamera() ); {
    {
        mExp.bind();
        gl::clear();
        gl::enableDepthRead();
        gl::enableDepthWrite();

        for( int i=0; i<6; i++){
            gl::pushMatrices();
            gl::setMatrices( cams[i] );
            
            if( !mExp.bRender && !mExp.bSnap ){
                mt::drawCoordinate(160);
            }
            
            rms[i].draw();
            
            gl::popMatrices();
        }
    }mExp.end();

    mExp.draw();

    if(gui) gui->draw();

    if( bStart)frame++;
}

void cApp::keyDown( KeyEvent event ) {
    char key = event.getChar();
    switch (key) {
        case 'S': mExp.startRender();  break;
        case 'T': mExp.stopRender();  break;
        case 's': mExp.snapShot();  break;
        case ' ': bStart = !bStart; break;
    }
}

void cApp::mouseDown( MouseEvent event ){
    for( int i=0; i<6; i++ ){
        camUis[i].mouseDown( event.getPos() );
    }
}

void cApp::mouseDrag( MouseEvent event ){
    for( int i=0; i<6; i++ ){
        camUis[i].mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
    }
}

void cApp::resize(){
    for( int i=0; i<6; i++ ){
        CameraPersp & cam = const_cast<CameraPersp&>(camUis[i].getCamera());
        cam.setAspectRatio( getWindowAspectRatio() );
        camUis[i].setCamera( &cam );
    }
}


void cApp::makeGui(){
    gui = params::InterfaceGl::create( getWindow(), "Ramses", vec2(300, getWindowHeight()) );
    gui->setOptions( "", "position=`0 0` valueswidth=100" );
    
    function<void(void)> update = [&](){
        for( int i=0; i<rms.size(); i++){
            rms[i].updateVbo(eye);
        }
    };
    
    
    function<void(void)> sx = [this](){
        saveXml();
    };
    
    function<void(void)> ld = [this](){
        loadXml();
        for( int i=0; i<rms.size(); i++){
            rms[i].eSimType = simType;
            rms[i].loadSimData(frame);
            rms[i].updateVbo(eye);
        }
    };

    function<void(void)> ren = [this](){
        bStart = true;
        mExp.startRender();
    };
    
    gui->addText( "main" );
    gui->addParam("start", &bStart );
    gui->addParam("frame", &frame ).updateFn(update);
    gui->addParam("ortho", &bOrtho );
    gui->addParam("xyz global scale", &Ramses::globalScale ).step(0.01).updateFn(update);
    //gui->addParam("r(x) resolution", &Ramses::boxelx, true );
    //gui->addParam("theta(y) resolution", &Ramses::boxely, true );
    gui->addButton("save XML", sx );
    gui->addButton("load XML", ld );
    gui->addButton("start Render", ren );

    gui->addSeparator();
    
    for( int i=0; i<6; i++){
        string p = to_string(simType) + "_"+  Ramses::prm[i];
        
        function<void(void)> up = [i, this](){
            rms[i].updateVbo(eye);
        };
        
        function<void(void)> up2 = [i, this](){
            rms[i].loadSimData(this->frame);
            rms[i].updateVbo(eye);
        };
        
        gui->addParam(p+" show", &rms[i].bShow ).group(p).updateFn(up2);
        //gui->addParam(p+" polar coordinate", &rms[i].bPolar ).group(p).updateFn(up2);
        
        gui->addParam(p+" Auto Min Max", &rms[i].bAutoMinMax ).group(p).updateFn(up);
        gui->addParam(p+" in min", &rms[i].in_min).step(0.05f).group(p).updateFn(up);
        gui->addParam(p+" in max", &rms[i].in_max).step(0.05f).group(p).updateFn(up);
        
        gui->addParam(p+" z extrude", &rms[i].extrude).step(1.0f).group(p).updateFn(up);
        //gui->addParam(p+" x offset", &rms[i].xoffset).step(1.0f).group(p).updateFn(up);
        //gui->addParam(p+" y offset", &rms[i].yoffset).step(1.0f).group(p).updateFn(up);
        //gui->addParam(p+" z offset", &rms[i].zoffset).step(1.0f).group(p).updateFn(up);
        
        gui->addParam(p+" xy scale", &rms[i].scale).step(1.0f).group(p).updateFn(up);
        //gui->addParam(p+" visible thresh", &rms[i].visible_thresh).step(0.005f).min(0.0f).max(1.0f).group(p).updateFn(up);
        gui->addParam(p+" log", &rms[i].eStretch).step(1).min(0).max(1).group(p).updateFn(up2);
        
        // read only
        //gui->addParam(p+" r(x) resolution", &rms[eSimType][i].boxelx, true );
        //gui->addParam(p+" theta(y) resolution", &rms[eSimType][i].boxely, true );
        
        //gui->addParam(p+" visible rate(%)", &rms[i].visible_rate, true ).group(p);
        //gui->addParam(p+" num particle", &rms[i].nParticle, true).group(p);
        
        gui->addSeparator();
        
    }
}

void cApp::loadXml(){
    
    fs::path p = "gui.xml";
    if( !fs::is_empty( p ) ){
        XmlTree xml( loadFile( p ) );
        XmlTree mn = xml.getChild("gui_setting/main");
        frame =  (mn/"frame").getValue<int>();
        bOrtho =  (mn/"ortho").getValue<bool>();
        Ramses::globalScale =  (mn/"xyz_global_scale").getValue<float>();
        //Ramses::boxelx =  (mn/"r_resolution").getValue<float>();
        //Ramses::boxely =  (mn/"theta_resolution").getValue<float>();
        
        XmlTree sim = xml.getChild( "gui_setting/simType_" + to_string(simType) );
        
        for( int i=0; i<rms.size(); i++){
            Ramses & r = rms[i];
            string name = Ramses::prm[i];
            XmlTree prm = sim.getChild(name);
            r.bShow = (prm/"show").getValue<bool>();
            r.bPolar = (prm/"polar").getValue<bool>(true);
            r.bAutoMinMax = (prm/"Auto_Min_Max").getValue<bool>();
            r.in_min = (prm/"in_min").getValue<float>();
            r.in_max = (prm/"in_max").getValue<float>();
            r.extrude = (prm/"z_extrude").getValue<float>();
            r.xoffset = (prm/"x_offset").getValue<float>();
            r.yoffset = (prm/"y_offset").getValue<float>();
            r.zoffset = (prm/"z_offset").getValue<float>();
            r.scale = (prm/"xy_scale").getValue<float>();
            r.eStretch = (prm/"log").getValue<float>();
        }
    }
}

void cApp::saveXml(){
    
    XmlTree xml, mn;
    xml.setTag("gui_setting");
    mn.setTag("main");
    
    mn.push_back( XmlTree("frame", to_string(frame) ));
    mn.push_back( XmlTree("ortho", to_string(bOrtho) ));
    mn.push_back( XmlTree("xyz_global_scale", to_string(Ramses::globalScale) ));
    //mn.push_back( XmlTree("r_resolution", to_string(Ramses::boxelx) ));
    //mn.push_back( XmlTree("theta_resolution", to_string(Ramses::boxely) ));
    
    xml.push_back( mn );
    
    XmlTree sim;
    sim.setTag("simType_"+to_string(simType));
    
    for( int i=0; i<rms.size(); i++){
        Ramses & r = rms[i];
        string name = Ramses::prm[i];
        XmlTree prm;
        prm.setTag(name);
        prm.push_back( XmlTree("show", to_string( r.bShow) ));
        prm.push_back( XmlTree("polar", to_string( r.bPolar )));
        prm.push_back( XmlTree("Auto_Min_Max", to_string( r.bAutoMinMax) ));
        prm.push_back( XmlTree("in_min", to_string(r.in_min) ));
        prm.push_back( XmlTree("in_max",to_string(r.in_max)));
        prm.push_back( XmlTree("z_extrude", to_string(r.extrude)));
        prm.push_back( XmlTree("x_offset", to_string(r.xoffset)));
        prm.push_back( XmlTree("y_offset", to_string(r.yoffset)));
        prm.push_back( XmlTree("z_offset", to_string(r.zoffset)));
        prm.push_back( XmlTree("xy_scale", to_string( r.scale )));
        prm.push_back( XmlTree("log", to_string( r.eStretch )));
        sim.push_back( prm );
    }
    xml.push_back(sim);

    DataTargetRef file = DataTargetPath::createRef( "gui.xml" );
    xml.write( file );

}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 )) )
