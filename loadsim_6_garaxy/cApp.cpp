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
    
    typedef tuple<string, vector<float>, float, float, float, float, VboSet, bool, int> Gdata;

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
    
    fs::path assetDir;
    CameraUi camUi;
    CameraPersp cam;
    Perlin mPln;
    Exporter mExp;
    
    float fmax = numeric_limits<float>::max();
    float fmin = numeric_limits<float>::min();
    
    vector<bool> bShowPrm = { false, true, false, false, false, false};
    
    /*
         0 : prmNam
         1 : data
         2 : data min
         3 : data max
         4 : range in
         5 : range out
     */
    vector<Gdata> v_gd{
        Gdata( "pos",        vector<float>(), fmax, fmin, 0.0f, 1.0f, VboSet(),   false,  0),
        Gdata( "vel_length", vector<float>(), fmax, fmin, 0.51f, 1.0f, VboSet(),  true,   1),
        Gdata( "rho",        vector<float>(), fmax, fmin, 0.3f, 1.0f, VboSet(),true,  2),
        Gdata( "N",          vector<float>(), fmax, fmin, 0.0f, 1.0f, VboSet(),   false,  3),
        Gdata( "mass",       vector<float>(), fmax, fmin, 0.5f, 1.0f, VboSet(),   true,   4),
        Gdata( "K",          vector<float>(), fmax, fmin, 0.8f, 1.0f, VboSet(),   true,   5)
    };
    
    bool bStart = false;
    bool bShowStats = false;
    int frame = 0;
    
    vector<VboSet> stats;
};

void cApp::setup(){
    setWindowPos( 0, 0 );
    setWindowSize( 1080*3*0.5, 1920*0.5 );
    mExp.setup( 1080*3, 1920, 0, 1000, GL_RGB, mt::getRenderPath(), 0);
    
    cam = camUi.getCamera();
    cam.setPerspective(54.5f, 1080.0f*3/1920.0f, 1.0f, 100000.0f);
    cam.lookAt( vec3(0,0,-2000), vec3(0,0,0) );
    camUi.setCamera( &cam );
    
    mPln.setSeed(123);
    mPln.setOctaves(4);
    
    assetDir = mt::getAssetPath();
    
    prepare();
    
#ifdef RENDER
    mExp.startRender();
#endif

}

void cApp::prepare(){
    
    //string frameName = "rdr_00468_l17.hydro";
    //string frameName = "rdr_01027_l17.hydro";
    string frameName = "rdr_01398_l17.hydro";
    
    
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
    makeVbo( v_gd[5] ); // 5 : K
    
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
    bool logalizm = std::get<7>(gd);
    int id = std::get<8>(gd);
    printf("\nprmName   : %s\nmin-max   : %e - %e\nin-out(log): %0.4f - %0.4f\n", prmName.c_str(), min, max, in, out );
    
    vector<float> result;

    for( int i=0; i<data.size(); i++ ) {

        //
        //  N
        //  AMR(adaptive mesh refinement) level of the cell,
        //  200/N kpc
        //
        float N = std::get<1>(v_gd[3])[i];

        int res = 1+(N-10)/2;

        if( 10 <= N && i%res!=0) continue;
        
        N = lmap(N, std::get<2>(v_gd[3]), std::get<3>(v_gd[3]), 0.0f, 1.0f);
        N = 1.0f - N;
        if( 0.8 <= N) continue;

        float val;
        float d = data[i];

        if( logalizm){
            float p = 10.0f;
            float map;
            
            if( id==5 ){
                map = lmap( d, 0.0f, max*0.8f, 10.0f, pow(10.0f, p) );
            }else{
                map = lmap( d, min, max, 10.0f, pow(10.0f, p) );
            }
            
            float log = log10(map) / p;
            val = log;
        }else{
            float map = lmap( d, min, max, 0.0f, 1.0f);
            val = map;
        }
 
        if( in<=val && val<=out ){

            vec3 p( posdata[i*3+0], posdata[i*3+1], posdata[i*3+2] );
            //vec3 n = mPln.dfBm( p*0.5 ) * N * 0.1;
            //p += n;
            glm::rotateZ(p, toRadians(90.0f));
            vs.addPos( p * 1000.0f );
            
            float remap = lmap(val, in, out, 0.3f, 0.7f);
            result.push_back( remap );
            
            ColorAf c;
            //ColorAf c( CM_HSV, 0.4+remap*0.5, 0.8f, 0.8f, 0.5f );
            if(id!=2){
                remap *= 0.7f;
                c = ColorAf( remap, remap, remap, remap);
            }
            else{
                remap *= 1.5f;
                c = ColorAf( remap, remap, remap, remap );
            }
            
            vs.addCol( c );
        }
    }
    
    vs.init( GL_POINTS );

    int nVerts = vs.getPos().size();
    printf("add %d vertices, %0.4f %% visible\n", nVerts, (float)nVerts/data.size()*100.0f);
    
    
    if( result.size() !=0 ){
        std::sort(result.begin(), result.end() );
        int size = result.size();
        float min = result[0];
        float max = result[size-1];
        float median = result[size/2];
        printf("RESULT %-10s  : %e - %e, median %e\n", prmName.c_str(), min, max, median );
        
        for( int i=0; i<result.size(); i++){
            stats[id].addPos( vec3( i, result[i], 0 ));
            stats[id].addCol(ColorAf(0,0,1,1));
        }
        
        stats[id].init(GL_POINTS);
    }
}

void cApp::update(){
    if( bStart ){
    }
}

void cApp::draw(){
    
    
    if( !bShowStats ){
        mExp.begin( camUi.getCamera() );{
            gl::clear( ColorA(0,0,0,1) );
            gl::enableAlphaBlending();
            //gl::enableAdditiveBlending();
            gl::enableDepthRead();
            gl::enableDepthWrite();
            gl::lineWidth( 1 );
            glPointSize( 1 );
            
            if(!mExp.bRender && !mExp.bSnap)
                mt::drawCoordinate(1000);
            
            for( int i=0; i<v_gd.size(); i++ ){
                if( bShowPrm[i]){
                    VboSet & vs = std::get<6>( v_gd[i] );
                    
                    if( vs.vbo )
                        gl::draw( vs.vbo );
                }
            }
        }mExp.end();
        
        mExp.draw();

    }else{
        
        gl::pushMatrices();
        mt::setMatricesWindow(getWindowWidth(), getWindowHeight(), false);
        gl::translate(10, 10, 0);
        for( int i=0; i<stats.size(); i++){
            stats[i].draw();
        }
        gl::popMatrices();

    }
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
    }
}

void cApp::mouseDown( MouseEvent event ){
    camUi.mouseDown( event.getPos() );
}

void cApp::mouseDrag( MouseEvent event ){
    camUi.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
}

void cApp::resize(){
    CameraPersp cam = camUi.getCamera();
    cam.setAspectRatio( getWindowAspectRatio() );
    camUi.setCamera( &cam );
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 ) ) )
