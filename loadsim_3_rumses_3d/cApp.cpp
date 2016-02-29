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
#include "SoundWriter.h"
#include "Dft.h"
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
    
    vector<vector<Ramses>> rms;
    bool bStart = false;
    bool bOrtho = false;
    int frame = 100;
    
    bool bZsort = false;
    
    params::InterfaceGlRef gui;
    
    vector<vector<CameraPersp>> cams;
    vector<vector<CameraUi>> camUis;
    
    VboSet line;
};

void cApp::setup(){
    setWindowPos( 0, 0 );

    float w = 1080*3;
    float h = 1920;
    
    setWindowSize( w*0.6, h*0.6 );
    mExp.setup( w, h, 0, 1000, GL_RGB, mt::getRenderPath(), 0 );
    
    cams.assign(5, vector<CameraPersp>());
    camUis.assign(5, vector<CameraUi>() );

    for( int s=0; s<5; s++){

        cams[s].assign(6, CameraPersp());
        camUis[s].assign(6, CameraUi());
        
        for( int i=0; i<6; i++){
            cams[s][i] = CameraPersp(w, h, 55.0f, 1, 100000 );
            cams[s][i].lookAt( vec3(0,0,800), vec3(0,0,0) );
            cams[s][i].setLensShift( -1.0+0.3333*0.5+0.333*i, s*0.4 -0.8);
            camUis[s][i].setCamera( &cams[s][i] );
        }
    }
    
    mPln.setSeed(123);
    mPln.setOctaves(4);

    for( int s=0; s<5; s++){
        rms.push_back( vector<Ramses>() );
        
        for( int i=0; i<6; i++){
            rms[s].push_back( Ramses(s,i) );
        }
    }

    makeGui();
    
    
#ifdef RENDER
    mExp.startRender();
#endif
    
}


void cApp::update(){
    if( bStart ){
        for( int s=0; s<5; s++){
            for( int i=0; i<rms[s].size(); i++){
                if( rms[s][i].bShow ){
                    bool loadok = rms[s][i].loadSimData( frame );
                    if( loadok ){
                        rms[s][i].updateVbo();
                    }
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

        for( int s=0; s<5; s++){
            for( int i=0; i<6; i++){
                gl::pushMatrices();
                gl::setMatrices( cams[s][i] );
                
                if( !mExp.bRender && !mExp.bSnap ){
                    mt::drawCoordinate(10);
                }
                
                rms[s][i].draw();
                
                gl::popMatrices();
            }
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
    for( int s=0; s<5; s++ ){
        for( int i=0; i<6; i++ ){
            camUis[s][i].mouseDown( event.getPos() );
        }
    }
}

void cApp::mouseDrag( MouseEvent event ){
    for( int s=0; s<5; s++ ){
        for( int i=0; i<6; i++ ){
            camUis[s][i].mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
        }
    }
}

void cApp::resize(){
    for( int s=0; s<5; s++ ){
        for( int i=0; i<6; i++ ){
            CameraPersp & cam = const_cast<CameraPersp&>(camUis[s][i].getCamera());
            cam.setAspectRatio( getWindowAspectRatio() );
            camUis[s][i].setCamera( &cam );
        }
    }
}


void cApp::makeGui(){
    gui = params::InterfaceGl::create( getWindow(), "Ramses", vec2(300, getWindowHeight()) );
    gui->setOptions( "", "position=`0 0` valueswidth=100" );
    
    function<void(void)> update = [&](){
        for( int s=0; s<rms.size(); s++){
            for( int i=0; i<rms[s].size(); i++){
                rms[s][i].updateVbo();
            }
        }
    };
    
    
    function<void(void)> sx = [this](){
        saveXml();
    };
    
    function<void(void)> ld = [this](){
        loadXml();
        for( int s=0; s<rms.size(); s++){
            for( int i=0; i<rms[s].size(); i++){
                rms[s][i].eSimType = s;
                rms[s][i].loadSimData(frame);
                rms[s][i].updateVbo();
            }
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
    
    for( int eSimType=0; eSimType<5; eSimType++ ){        
        for( int i=0; i<6; i++){
            string p = to_string(eSimType) + "_"+  Ramses::prm[i];

            function<void(void)> up = [i, eSimType, this](){
                rms[eSimType][i].updateVbo();
            };
            
            function<void(void)> up2 = [i, eSimType, this](){
                rms[eSimType][i].loadSimData(this->frame);
                rms[eSimType][i].updateVbo();
            };
            
            gui->addParam(p+" show", &rms[eSimType][i].bShow ).group(p).updateFn(up2);
            //gui->addParam(p+" polar coordinate", &rms[i].bPolar ).group(p).updateFn(up2);
            
            gui->addParam(p+" Auto Min Max", &rms[eSimType][i].bAutoMinMax ).group(p).updateFn(up);
            gui->addParam(p+" in min", &rms[eSimType][i].in_min).step(0.05f).group(p).updateFn(up);
            gui->addParam(p+" in max", &rms[eSimType][i].in_max).step(0.05f).group(p).updateFn(up);
            
            gui->addParam(p+" z extrude", &rms[eSimType][i].extrude).step(1.0f).group(p).updateFn(up);
            //gui->addParam(p+" x offset", &rms[i].xoffset).step(1.0f).group(p).updateFn(up);
            //gui->addParam(p+" y offset", &rms[i].yoffset).step(1.0f).group(p).updateFn(up);
            //gui->addParam(p+" z offset", &rms[i].zoffset).step(1.0f).group(p).updateFn(up);
            
            gui->addParam(p+" xy scale", &rms[eSimType][i].scale).step(1.0f).group(p).updateFn(up);
            //gui->addParam(p+" visible thresh", &rms[i].visible_thresh).step(0.005f).min(0.0f).max(1.0f).group(p).updateFn(up);
            gui->addParam(p+" log", &rms[eSimType][i].eStretch).step(1).min(0).max(1).group(p).updateFn(up2);
            
            // read only
            //gui->addParam(p+" r(x) resolution", &rms[eSimType][i].boxelx, true );
            //gui->addParam(p+" theta(y) resolution", &rms[eSimType][i].boxely, true );

            //gui->addParam(p+" visible rate(%)", &rms[i].visible_rate, true ).group(p);
            //gui->addParam(p+" num particle", &rms[i].nParticle, true).group(p);
            
            gui->addSeparator();
        }
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
        
        for( int eSimType=0; eSimType<5; eSimType++ ){
            
            XmlTree sim = xml.getChild( "gui_setting/simType_" + to_string(eSimType) );
            
            for( int i=0; i<rms[eSimType].size(); i++){
                Ramses & r = rms[eSimType][i];
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

    for( int eSimType=0; eSimType<5; eSimType++ ){

        XmlTree sim;
        sim.setTag("simType_"+to_string(eSimType));
        
        for( int i=0; i<rms[eSimType].size(); i++){
            Ramses & r = rms[eSimType][i];
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
    }
    
    DataTargetRef file = DataTargetPath::createRef( "gui.xml" );
    xml.write( file );
    
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 )) )
