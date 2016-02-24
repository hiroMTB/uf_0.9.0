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
#include "mtUtil.h"
#include "ConsoleColor.h"
#include "Exporter.h"
#include "VboSet.h"
#include "cinder/Xml.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class cApp : public App {
    
    typedef tuple<
                    string,             // name
                    vector<float>,      // data
                    float,              // max
                    float,              // min
                    float,              // in
                    float,              // out
                    VboSet,
                    bool,               // log on/off
                    float,              // logLeve
                    int                 // id
    > Gdata;

public:
    void setup();
    void update();
    void draw();
    void mouseDown( MouseEvent event );
    void mouseDrag( MouseEvent event );
    void keyDown( KeyEvent event );

    void prepare();
    void loadXml( fs::path path );
    void loadBin( fs::path path, vector<float> & array );
    void resize();
    
    void makeVbo( Gdata & gd);
    void updateVbo();
    void printCamera();
    
    fs::path assetDir;
    CameraUi camUi;
    CameraPersp cam;
    Perlin mPln;
    Exporter mExp;
    
    float fmax = numeric_limits<float>::max();
    float fmin = numeric_limits<float>::min();
    
    vector<bool> bShowPrm = { false, true, false, false, false, false};
    
    vector<Gdata> v_gd{
        Gdata( "pos",        vector<float>(), fmax, fmin, 0.0f,     1.00f, VboSet(),   false,  0,   0),
        Gdata( "vel_length", vector<float>(), fmax, fmin, 0.41f,    0.60f, VboSet(),   true,  10,  1),
        Gdata( "rho",        vector<float>(), fmax, fmin, 0.05f,    0.90f, VboSet(),   true,  10,  2),
        Gdata( "N",          vector<float>(), fmax, fmin, 0.00f,    1.00f, VboSet(),   false,  0,   3),
        Gdata( "mass",       vector<float>(), fmax, fmin, 0.1f,     0.95f, VboSet(),   true,  10,  4),
        Gdata( "K",          vector<float>(), fmax, fmin, 0.8f,     0.88f, VboSet(),   true,  10,  5)
    };
    
    bool bStart = false;
    bool bShowStats = false;
    int frame = 0;
    
    vector<VboSet> stats;
    
    //string frameName = "rdr_00468_l17.hydro";
    //string frameName = "rdr_01027_l17.hydro";
    string frameName = "rdr_01398_l17.hydro";
    
    
};

void cApp::setup(){
    setWindowPos( 0, 0 );
    setWindowSize( 1080*3*0.5, 1920*0.5 );
    mExp.setup( 1080*3, 1920, 0, 1000, GL_RGB, mt::getRenderPath(), 0);
    
    cam.setPerspective(54.5f, 1080.0f*3/1920.0f, 1.0f, 100000.0f);
    cam.lookAt( vec3(0, 0, -4000), vec3(0,0,0), vec3(1,0,0) );

    if(1){
        cam.setNearClip(1.000000);
        cam.setFarClip(100000.000000);
        cam.setAspectRatio(1.687500);
        cam.setFov(54.500000);
        cam.setEyePoint(vec3(-1764.669800,1200.267578,-2621.229248));
        cam.setWorldUp(vec3(1.000000,0.000000,0.000000));
        cam.setLensShift(vec2(0.000000,0.000000));
        cam.setViewDirection(vec3(0.581035,-0.324420,0.746425));
        cam.lookAt(vec3(-1764.669800,1200.267578,-2621.229248)+vec3(0.581035,-0.324420,0.746425));
    }
    
    camUi.setCamera( &cam );
    
    mPln.setSeed(123);
    mPln.setOctaves(4);
    
    assetDir = mt::getAssetPath();
    
    prepare();
    
#ifdef RENDER
    mExp.startRender();
#endif

}

void cApp::printCamera(){
    
    const CameraPersp & c = camUi.getCamera();

    float n = c.getNearClip();
    float f = c.getFarClip();
    float a = c.getAspectRatio();
    float fov = c.getFov();
    vec3 e = c.getEyePoint();
    vec3 u = c.getWorldUp();
    vec2 s = c.getLensShift();
    vec3 d = c.getViewDirection();
    float dis = c.getPivotDistance();
    
    printf("\n");
    printf( "cam.setNearClip(%f);\n", n);
    printf( "cam.setFarClip(%f);\n", f);
    printf( "cam.setAspectRatio(%f);\n", a);
    printf( "cam.setFov(%f);\n", fov);
    printf( "cam.setEyePoint(vec3(%f,%f,%f));\n", e.x,e.y,e.z);
    printf( "cam.setWorldUp(vec3(%f,%f,%f));\n", u.x,u.y,u.z);
    printf( "cam.setLensShift(vec2(%f,%f));\n", s.x,s.y);
    printf( "cam.setViewDirection(vec3(%f,%f,%f));\n", d.x, d.y, d.z);
    //printf( "cam.setPivotDistance(%f);\n", dis);
    printf( "cam.lookAt(vec3(%f,%f,%f)+vec3(%f,%f,%f));\n)", e.x,e.y,e.z, d.x,d.y,d.z );
}
void cApp::prepare(){
    

    /*
            XML
     */
    if(1){
        printf(")\nstart loading XML file\n");
        loadXml(assetDir/"sim"/"garaxy"/"bin"/"_settings"/(frameName+".settings.xml") );
    }
    
    /*
            Bin
     */
    printf("\nstart loading binary file\n");
    for( auto & gd : v_gd ){
        string & prmName = std::get<0>(gd);
        vector<float> & data = std::get<1>(gd);
        loadBin(assetDir/"sim"/"garaxy"/"bin"/prmName/(frameName+"_"+ prmName + ".bin"), data );
        
        // min & max
        if(0){
            float min = numeric_limits<float>::max();
            float max = numeric_limits<float>::min();
            for( int i=0; i<data.size(); i++ ) {
                min = MIN(min, data[i]);
                max = MAX(max, data[i]);
                std::get<2>(gd) = min;
                std::get<3>(gd) = max;
            }
            printf("%-10s  : %e - %e\n", prmName.c_str(), min, max );
        }
    }
    
    
    stats.assign(6, VboSet());
    
    /*
            Vbo
     */
    printf("\nstart making Vbo\n");
    makeVbo( v_gd[1] ); // 1 : vel_length
    makeVbo( v_gd[2] ); // 2 : rho
    makeVbo( v_gd[4] ); // 4 : mass
    //makeVbo( v_gd[5] ); // 5 : K
    
}

void cApp::loadXml( fs::path path ){
    
    printf("%s\n", path.filename().string().c_str() );
    XmlTree settings( loadFile(path) );

    for( auto & gd : v_gd ){
        string & prmName = std::get<0>(gd);

        if( prmName != "pos" && prmName != "vel"){        
            float & min = std::get<2>(gd) = std::stod( settings.getChild("settings/"+ prmName+"/min").getValue() );
            float & max = std::get<3>(gd) = std::stod( settings.getChild("settings/"+ prmName+"/max").getValue() );
            printf("%-10s  : %e - %e\n", prmName.c_str(), min, max );
        }
    }
}

void cApp::loadBin( fs::path path, vector<float> & array ){
    
    printf("%-34s", path.filename().string().c_str() );
    std::ifstream is( path.string(), std::ios::binary );
    if( is ){   printf(" - DONE, "); }
    else{       printf(" - ERROR\nquit()"); quit(); }

    is.seekg (0, is.end);
    int fileSize = is.tellg();
    printf("%d byte, ", fileSize);
    is.seekg (0, is.beg);
    
    int arraySize = fileSize / sizeof(float);
    printf("%d float numbers\n", arraySize);
    
    array.assign(arraySize, float(0) );
    is.read(reinterpret_cast<char*>(&array[0]), fileSize);
    is.close();

}

void cApp::makeVbo( Gdata & gd ){
    vector<float> & posdata = std::get<1>( v_gd[0] );
    string prmName = std::get<0>(gd);
    vector<float> & data = std::get<1>(gd);
    float min = std::get<2>(gd);
    float max = std::get<3>(gd);
    float in  = std::get<4>(gd);
    float out = std::get<5>(gd);
    VboSet & vs = std::get<6>(gd);
    //bool logalizm = std::get<7>(gd);
    float logLevel = std::get<8>(gd);
    //int id = std::get<9>(gd);
    printf("\nprmName   : %s\nmin-max   : %e - %e\nin-out(log): %0.4f - %0.4f\n", prmName.c_str(), min, max, in, out );
    
    vector<float> result;

    int Nmin = std::get<2>(v_gd[3]);
    int Nmax = std::get<3>(v_gd[3]);
    
    for( int i=0; i<data.size(); i++ ) {

        //
        //  N
        //  AMR(adaptive mesh refinement) level of the cell,
        //  200/N kpc
        //
        float N = std::get<1>(v_gd[3])[i];
        int res = 1 + N-Nmin; //1+(N-8)*2;

        if( i%res!=0) continue;
        if( N-Nmin <= 2 ) continue;

        float d = data[i];

        float map = lmap( d, min, max, 1.0f, pow(10.0f, logLevel) );
        float log = log10(map) / logLevel;
        float val = log;
        val = MIN(val, out);
        
        if( in<=val ){
            //if( val <= out){
                vec3 p( posdata[i*3+0], posdata[i*3+1], posdata[i*3+2] );
                //vec3 n = mPln.dfBm( p*0.5 ) * N * 0.1;
                //p += n;
                //glm::rotateZ(p, toRadians(90.0f));
                vs.addPos( p * 100.0f );
                float remap = lmap(val, in, out, 0.3f, 1.0f);
                
                ColorAf c( remap, remap, remap, remap );
                //float w = remap;
                //ColorAf c( w, w, w, 0.5f+w*0.5f );
                vs.addCol( c );
            //}
        }
    }
    
    vs.init( GL_POINTS );

    int nVerts = vs.getPos().size();
    printf("add %d vertices, %0.4f %% visible\n", nVerts, (float)nVerts/data.size()*100.0f);

}

void cApp::update(){
    if( bStart ){
    }
}

void cApp::draw(){
    
    mExp.begin( camUi.getCamera() );{
        gl::clear( ColorA(0,0,0,1) );
        gl::enableAlphaBlending();
        gl::enableDepthRead();
        gl::enableDepthWrite();
        gl::lineWidth( 1 );
        glPointSize( 1 );
        
        if(!mExp.bRender && !mExp.bSnap)
            mt::drawCoordinate(100);
        
        for( int i=0; i<v_gd.size(); i++ ){
            if( bShowPrm[i]){
                VboSet & vs = std::get<6>( v_gd[i] );
                vs.draw();
            }
        }
    }mExp.end();
    
    mExp.draw();
    
    gl::disableDepthRead();
    gl::disableDepthWrite();
    
    gl::lineWidth(2);
    gl::color(1, 1, 1);
    mt::drawScreenGuide();
}

void cApp::keyDown( KeyEvent event ) {
    switch ( event.getChar() ) {
        case 'S': mExp.startRender();       break;
        case 's': mExp.snapShot();          break;
        case 't': bShowStats = !bShowStats; break;
        case '0': bShowPrm[0]=!bShowPrm[0]; break;  // pos, no vbo
        case '1': bShowPrm[1]=!bShowPrm[1]; break;  // vel
        case '2': bShowPrm[2]=!bShowPrm[2]; break;  // rho
        case '3': bShowPrm[3]=!bShowPrm[3]; break;  // N,   no vbo
        case '4': bShowPrm[4]=!bShowPrm[4]; break;  // mass
        case '5': bShowPrm[5]=!bShowPrm[5]; break;  // K
        case 'c': printCamera(); break;
    }
}

void cApp::mouseDown( MouseEvent event ){
    camUi.mouseDown( event.getPos() );
}

void cApp::mouseDrag( MouseEvent event ){
    camUi.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
}

void cApp::resize(){
    cam.setAspectRatio( getWindowAspectRatio() );
    camUi.setCamera( &cam );
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 ) ) )
