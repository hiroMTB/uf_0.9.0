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
#include "TbbNpFinder.h"
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
    
    CameraUi camUi;
    Perlin mPln;
    Exporter mExp;
    
    vector<Ramses> rms;
    bool bStart = false;
    bool bOrtho = false;
    bool bRender = false;
    int eSimType = 0;
    int frame = 100;

    params::InterfaceGlRef gui;
    CameraPersp cam;
    
    vector<VboSet> nlines;
};

void cApp::setup(){
    setWindowPos( 0, 0 );
    
    float w = 1080*3;
    float h = 1920;

    //float w = 1920;
    //float h = 1080;
    
    setWindowSize( w*0.5, h*0.5 );
    mExp.setup( w, h, 0, 3000, GL_RGB, mt::getRenderPath(), 0);
    
    cam = CameraPersp(w, h, 30, 0.1, 1000000 );
    cam.lookAt( vec3(0,0,-600), vec3(0,0,0) );
    cam.setLensShift( 1,0 );
    camUi.setCamera( &cam );
    
    mPln.setSeed(123);
    mPln.setOctaves(4);
    
    for( int i=0; i<6; i++){
        Ramses r(eSimType,i);
        rms.push_back( r );
        
        nlines.push_back( VboSet() );
    }

    makeGui();
    
#ifdef RENDER
    mExp.startRender();
#endif
    
}

void cApp::makeGui(){
    gui = params::InterfaceGl::create( getWindow(), Ramses::simType[eSimType], vec2(300, getWindowHeight()) );
    gui->setOptions( "", "position=`0 0` valueswidth=100" );
    
    function<void(void)> update = [&](){
        for( int i=0; i<rms.size(); i++){ rms[i].updateVbo(); }
    };
    
    function<void(void)> changeSym = [this](){
        for( int i=0; i<rms.size(); i++){
            rms[i].eSimType = eSimType;
            rms[i].loadSimData(frame);
            rms[i].updateVbo();
        }
    };
    
    
    function<void(void)> ld = [this](){
        loadXml();
        for( int i=0; i<rms.size(); i++){
            rms[i].eSimType = eSimType;
            rms[i].loadSimData(frame);
            rms[i].updateVbo();
        }
    };
    
    gui->addText( "main" );
    gui->addParam("simType", &eSimType ).min(0).max(4).updateFn( changeSym );
    gui->addParam("start", &bStart );
    gui->addParam("frame", &frame ).updateFn(update);
    gui->addParam("ortho", &bOrtho );
    gui->addParam("xyz global scale", &Ramses::globalScale ).step(0.01).updateFn(update);
    gui->addParam("r(x) resolution", &Ramses::boxelx, true );
    gui->addParam("theta(y) resolution", &Ramses::boxely, true );
    gui->addButton("save XML", [this](){ saveXml(); } );
    gui->addButton("load XML", ld );
    gui->addButton("Start and Render", [&](){ mExp.startRender(); bStart=true; } );
    
    gui->addSeparator();
    
    for( int i=0; i<rms.size(); i++){
        string p = Ramses::prm[i];
        //gui->addText( p );
        
        //function<void(void)> up = bind(&Ramses::updateVbo, &rms[i]);
        function<void(void)> up = [i, this](){
            rms[i].updateVbo();
        };
        
        function<void(void)> up2 = [i, this](){
            rms[i].loadSimData(this->frame);
            rms[i].updateVbo();
        };

        gui->addParam(p+" show", &rms[i].bShow ).group(p).updateFn(up2);
        gui->addParam(p+" Auto Min Max", &rms[i].bAutoMinMax ).group(p).updateFn(up);
        gui->addParam(p+" in min", &rms[i].in_min).step(0.05f).group(p).updateFn(up);
        gui->addParam(p+" in max", &rms[i].in_max).step(0.05f).group(p).updateFn(up);
        
        gui->addParam(p+" z extrude", &rms[i].extrude).step(1.0f).group(p).updateFn(up);
        gui->addParam(p+" x offset", &rms[i].xoffset).step(1.0f).group(p).updateFn(up);
        gui->addParam(p+" y offset", &rms[i].yoffset).step(1.0f).group(p).updateFn(up);
        gui->addParam(p+" z offset", &rms[i].zoffset).step(1.0f).group(p).updateFn(up);
        
        gui->addParam(p+" xy scale", &rms[i].scale).step(1.0f).group(p).updateFn(up);
        //gui->addParam(p+" visible thresh", &rms[i].visible_thresh).step(0.005f).min(0.0f).max(1.0f).group(p).updateFn(up);
        gui->addParam(p+" log", &rms[i].eStretch).step(1).min(0).max(1).group(p).updateFn(up2);
        
        // read only
        gui->addParam(p+" visible rate(%)", &rms[i].visible_rate, true ).group(p);
        gui->addParam(p+" num particle", &rms[i].nParticle, true).group(p);
        
        gui->addSeparator();
        
        rms[i].extrude = 100;
    }
}

void cApp::loadXml(){
    
    fs::path p = "gui.xml";
    if( !fs::is_empty( p ) ){
        XmlTree xml( loadFile( p ) );
        XmlTree mn = xml.getChild("gui_setting/main");
        eSimType =  (mn/"simType").getValue<int>();
        frame =  (mn/"frame").getValue<int>();
        bOrtho =  (mn/"ortho").getValue<bool>();
        Ramses::globalScale =  (mn/"xyz_global_scale").getValue<float>();
        Ramses::boxelx =  (mn/"r_resolution").getValue<float>();
        Ramses::boxely =  (mn/"theta_resolution").getValue<float>();
        
        for( int i=0; i<rms.size(); i++){
            Ramses & r = rms[i];
            string name = Ramses::prm[i];
            XmlTree prm;
            prm = xml.getChild("gui_setting/"+name);
            r.bShow = (prm/"show").getValue<bool>();
            r.bAutoMinMax = (prm/"Auto_Min_Max").getValue<bool>();
            r.in_min = (prm/"in_min").getValue<float>();
            r.in_max = (prm/"in_max").getValue<float>();
            r.extrude = (prm/"z_extrude").getValue<float>();
            r.xoffset = (prm/"x_offset").getValue<float>();
            r.yoffset = (prm/"y_offset").getValue<float>();
            r.zoffset = (prm/"z_offset").getValue<float>();
            r.scale = (prm/"xy_scale").getValue<float>();
            r.eStretch = (prm/"log").getValue<float>();
            r.visible_rate = (prm/"visible_rate").getValue<float>();
            r.nParticle = (prm/"num_particle").getValue<int>();
        }
    }
}

void cApp::saveXml(){

    XmlTree xml, mn;
    xml.setTag("gui_setting");
    mn.setTag("main");
    
    mn.push_back( XmlTree("simType", to_string(eSimType)) );
    mn.push_back( XmlTree("frame", to_string(frame) ));
    mn.push_back( XmlTree("ortho", to_string(bOrtho) ));
    mn.push_back( XmlTree("xyz_global_scale", to_string(Ramses::globalScale) ));
    mn.push_back( XmlTree("r_resolution", to_string(Ramses::boxelx) ));
    mn.push_back( XmlTree("theta_resolution", to_string(Ramses::boxely) ));
    
    xml.push_back( mn );

    for( int i=0; i<rms.size(); i++){
        Ramses & r = rms[i];
        string name = Ramses::prm[i];
        XmlTree prm;
        prm.setTag(name);
        prm.push_back( XmlTree("show", to_string( r.bShow) ));
        prm.push_back( XmlTree("Auto_Min_Max", to_string( r.bAutoMinMax) ));
        prm.push_back( XmlTree("in_min", to_string(r.in_min) ));
        prm.push_back( XmlTree("in_max",to_string(r.in_max)));
        prm.push_back( XmlTree("z_extrude", to_string(r.extrude)));
        prm.push_back( XmlTree("x_offset", to_string(r.xoffset)));
        prm.push_back( XmlTree("y_offset", to_string(r.yoffset)));
        prm.push_back( XmlTree("z_offset", to_string(r.zoffset)));
        prm.push_back( XmlTree("xy_scale", to_string( r.scale )));
        prm.push_back( XmlTree("log", to_string( r.eStretch )));
        prm.push_back( XmlTree("visible_rate", to_string( r.visible_rate )));
        prm.push_back( XmlTree("num_particle", to_string( r.nParticle )));
        xml.push_back( prm );
    }
    
    DataTargetRef file = DataTargetPath::createRef( "gui.xml" );
    xml.write( file );
    
}

void cApp::update(){
    if( bStart ){
        for( int i=0; i<rms.size(); i++){
            if( rms[i].bShow ){
                rms[i].loadSimData( frame );
                rms[i].updateVbo();
                
                float d  = rms[i].in_max - rms[i].in_min;
                //rms[i].in_max -= d*0.1;
                
                if( 0 ){
                    TbbNpFinder np;
                    int num_line = 1;
                    int num_dupl = 1;
                    int size = rms[i].pos.size();
                    float nlimit = 3;
                    float flimit = 10;
                    const vector<vec3> & inpos = rms[i].pos;
                    const vector<ColorAf> & incol = rms[i].col;
                    
                    vector<vec3> outpos( size*(num_line*num_dupl)*2 );
                    vector<ColorAf> outcol( size*(num_line*num_dupl)*2 );
                    np.findNearestPoints(&inpos[0], &outpos[0], &incol[0], &outcol[0], size, num_line, num_dupl, nlimit, flimit);

                    nlines[i].resetPos();
                    nlines[i].resetCol();
                    nlines[i].resetVbo();
                    nlines[i].addPos(outpos);
                    nlines[i].addCol(outcol);
                    nlines[i].init(GL_LINES);
                }
                
                if( rms[i].in_max < rms[i].in_min ){
                    quit();
                }
            }
        }
    }
}

void cApp::draw(){
    
    gl::enableAlphaBlending();
    glPointSize(1);
    glLineWidth(1);
    
    bOrtho ? mExp.beginOrtho( true ) : mExp.begin( camUi.getCamera() ); {
        
        gl::clear();
        gl::enableDepthRead();
        gl::enableDepthWrite();
        
        gl::translate( 0, -mExp.mFbo->getHeight()/2, 0);
        gl::rotate( toRadians(90.0f), vec3(0,1,0) );
        
        if( !mExp.bRender && !mExp.bSnap ){
            mt::drawCoordinate(10);
            gl::color(1, 0, 0);
            gl::drawStrokedCircle( vec2(0,0), 20);
        }
        
        gl::lineWidth(1);
        for( int i=0; i<nlines.size(); i++ ){
            nlines[i].draw();
        }
        
        gl::pointSize(1);
        for( int i=0; i<rms.size(); i++){
            rms[rms.size()-i-1].draw();
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
    camUi.mouseDown( event.getPos() );
}

void cApp::mouseDrag( MouseEvent event ){
    camUi.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
}

void cApp::resize(){
    CameraPersp & cam = const_cast<CameraPersp&>(camUi.getCamera());
    cam.setAspectRatio( mExp.mFbo->getAspectRatio() );
    camUi.setCamera( &cam );
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 )) )
