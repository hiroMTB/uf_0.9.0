#include "cinder/app/App.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Camera.h"
#include "cinder/gl/Texture.h"
#include "CinderOpenCv.h"
#include "cinder/cairo/Cairo.h"
#include "cinder/params/Params.h"
#include "mtUtil.h"
#include "VboSet.h"

using namespace ci;
using namespace ci::app;
using namespace std;

/*

Contour retrieval modes
enum
{
    CV_RETR_EXTERNAL=0,
    CV_RETR_LIST=1,
    CV_RETR_CCOMP=2,
    CV_RETR_TREE=3,
    CV_RETR_FLOODFILL=4
};

Contour approximation methods
enum
{
    CV_CHAIN_CODE=0,
    CV_CHAIN_APPROX_NONE=1,
    CV_CHAIN_APPROX_SIMPLE=2,
    CV_CHAIN_APPROX_TC89_L1=3,
    CV_CHAIN_APPROX_TC89_KCOS=4,
    CV_LINK_RUNS=5
};
*/

class cApp : public App {
  public:
	void setup();
    void update();
    void draw();

    void open();
    void make();
    void save();
    
    void keyDown( KeyEvent event );
    void mouseMove( MouseEvent event );
    void mouseDown( MouseEvent event );
    void mouseDrag( MouseEvent event );

    
   int imgW, imgH;
    fs::path assetPath = mt::getAssetPath();
    //string filename = "vela_orient_red_pacs160_signal_full.tiff";
    
    fs::path path;
    
    VboSet vbo;

    CameraPersp cam;
    CameraUi camUi;

    typedef vector<cv::Point> Contour;
    typedef vector<Contour> ContourGroup;

    ContourGroup cts;
    vector<cv::Vec4i> hierarchy;
    
    cv::Mat src;
    cv::Mat proc;
    cv::RNG rng = cv::RNG(12345);

    params::InterfaceGlRef gui;

    bool bShowBlackBg = true;
    bool bShowSrc = true;
    bool bShowProc = false;
    bool bShowContour = true;
    bool bRemoveHalf = false;

    // cv::blur
    bool bBlur = true;
    int blur_size = 2;

    // cv::Eq histo
    bool bEqHist = false;
    
    // cv::canny
    bool bCanny = false;
    double th1 = 10;
    double th2 = 20;
    bool bDoubleThresh = true;
    int apertureSize = 3;
    bool L2gradient = false;
    
    // cv::threshold
    bool bThreshold = true;
    double thresh = 60;

    // cv::contour
    int cnt_retrieval_mode = 3;
    int approx_method = 2;
};

void cApp::open(){
    try {
        path = getOpenFilePath( "", ImageIo::getLoadExtensions() );
    }
    catch( Exception &exc ) {
        cout << "failed to load image" << endl;;
    }
    make();
}

void cApp::setup(){
    
   
    setWindowPos( 0, 0 );
    setWindowSize( 1920, 1080 );
    
    cam.lookAt(vec3(0,0,1500), vec3(0,0,0) );
    cam.setPerspective( 60.0f, getWindowAspectRatio(), 1.0f, 10000.0f );
    camUi.setCamera( &cam );

    function<void()> op1 = [&](){ open(); };
    function<void()> up1 = [&](){ make(); };
    function<void()> sv1 = [&](){ make(); save(); };

    gui = params::InterfaceGl::create("contour control", vec2(300, 600) );
    gui->setPosition(ivec2(10,10));
    
    gui->addText("show...");
    gui->addParam( "show black bg", &bShowBlackBg);
    gui->addParam( "show soure img", &bShowSrc);
    gui->addParam( "show result img", &bShowProc);
    gui->addParam( "show contor line", &bShowContour);
    gui->addParam("Remove Half contour", &bRemoveHalf).updateFn(up1);
    gui->addSeparator();
    
    gui->addText( "cv::blur" );
    gui->addParam( "Blur", &bBlur).updateFn(up1);
    gui->addParam( "Blur size", &blur_size).updateFn(up1).min(0);
    gui->addSeparator();
    
    gui->addText( "cv::equlizeHisto" );
    gui->addParam( "equlize histogram", &bEqHist).updateFn(up1);
    gui->addSeparator();

    
    gui->addText("cv::threshold");
    gui->addParam( "apply threshold", &bThreshold).updateFn(up1);
    gui->addParam( "thresh", &thresh ).updateFn(up1).min(0.0).max(255);
    gui->addSeparator();
    
    gui->addText( "cv::Canny" );
    gui->addParam( "Canny", &bCanny).updateFn(up1);
    gui->addParam( "threshold1", &th1 ).updateFn(up1).min(0).step(0.1);
    gui->addParam( "threshold2", &th2 ).updateFn(up1).min(0).step(0.1);
    gui->addParam( "th2 = th1 x 2", &bDoubleThresh ).updateFn(up1);
    
    gui->addParam( "apertureSize", &apertureSize ).updateFn(up1).min(3).max(7).step(2); // must be odd val
    gui->addParam( "L2gradient", &L2gradient ).updateFn(up1);
    gui->addSeparator();
    
    gui->addText("cv::findContour param");
    gui->addParam( "Contour retrieval modes", &cnt_retrieval_mode ).updateFn(up1).min(0).max(4);
    gui->addParam( "Contour approximation methods", &approx_method ).updateFn(up1).min(0).max(5);
    gui->addSeparator();
    
    //gui->addButton( "update", up1 );
    gui->addButton( "open", op1 );
    gui->addButton( "save", sv1 );

    open();
}

void cApp::make(){
    
    if(bDoubleThresh) th2 = th1*2.0;
    
    //Surface8u sur = Surface8u( loadImage( assetPath/"img"/"01"/filename) );
    Surface8u sur = Surface8u( loadImage(path) );
    src = toOcv( sur );
    imgW = sur.getWidth();
    imgH = sur.getHeight();
    
    proc = src.clone();

    // GRAY
    cv::cvtColor( proc, proc, CV_RGB2GRAY );
    
    // EQ
    if( bEqHist ) cv::equalizeHist( proc, proc );
    
    // BLUR
    if( bBlur ) cv::blur( proc, proc, cv::Size(blur_size, blur_size) );
    
    // threshold
    if( bThreshold ) cv::threshold( proc, proc, thresh, 255, cv::THRESH_BINARY );

    // CANY
    if( bCanny ){
        apertureSize += apertureSize%2==0 ? 1 : 0;
        cv::Canny( proc, proc, th1, th2, apertureSize, L2gradient);
    }
    
    cv::findContours( proc.clone(), cts, hierarchy, cnt_retrieval_mode, approx_method, cv::Point(0, 0) );
    
    // vbo
    vbo.resetPos();
    vbo.resetCol();
    vbo.resetVbo();
    
    for( int i=0; i<cts.size(); i++ ){
        
        if(bRemoveHalf && i%2==0 ) continue;
        
        Contour & c = cts[i];
        Colorf col(1,0,0);
        if(0){
            if( hierarchy[i][2] == -1 ){
                col = Colorf(0,1,0);
            }else if( hierarchy[i][3] == -1 ){
                col = Colorf(0,0,1);
            }
        }
        
        for( int j=0; j<c.size(); j++){

            vec2 v,v2;
            if( j != c.size()-1){
                v = fromOcv(c[j]);
                v2 = fromOcv(c[j+1]);
            }else{
                v = fromOcv(c[j]);
                v2 = fromOcv(c[0]);
            }
            vbo.addPos( vec3(v.x, v.y, 1) );
            vbo.addPos( vec3(v2.x, v2.y, 1) );
            vbo.addCol( col );
            vbo.addCol( col );
        }
    }
    
    vbo.init( GL_LINES );
}

void cApp::save(){

    fs::path outdir( mt::getRenderPath() );
    fs::create_directories( outdir );
    cairo::Context ctx;
    //fs::path filepath = outdir/(filename+".svg");
    fs::path filepath = path;
    
    ctx = cairo::Context( cairo::SurfaceSvg( filepath, imgW, imgH ) );
    cout << filepath.string() << endl;
    ctx.setLineWidth( 1 );
    ctx.setSource( ColorAf(1,1,1,1) );
    ctx.paint();
    ctx.stroke();
    
    ctx.setSource( ColorAf(0,0,0,1) );
    ctx.setLineWidth(1);
    
    for( int i=0; i<cts.size(); i++ ){
        if( i%2==1 )  continue;
        
//        bool haveParent = hierarchy[i][2] < 0;
//        bool haveChild = hierarchy[i][3] < 0;
//        if( haveParent && haveChild ){
//            ctx.setSource( Colorf(0,0,0) );
//        }else if( haveParent && !haveChild ){
//            ctx.setSource( Colorf(1,0,0) );
//            //continue;
//        }else if( !haveParent && haveChild ){
//            ctx.setSource( Colorf(0,1,0) );
//            //continue;
//        }else if( !haveParent && !haveChild ){
//            ctx.setSource( Colorf(0,0,1) );
//        }else {
//            ctx.setSource( Colorf(1,0,1) );
//        }
        
        ctx.newPath();
        if( cts.size() >=5 ){
            for( auto & p : cts[i] ){
                ctx.lineTo( p.x, p.y );
            }
        }
        ctx.closePath();
        ctx.stroke();
    }
}


void cApp::update(){
}

void cApp::draw(){

    if(bShowBlackBg)
        gl::clear( Color(0,0,0) );
    else
        gl::clear( Color(1,1,1) );
    
    gl::pushMatrices();
    gl::setMatrices( camUi.getCamera() );
    
    gl::translate(-imgW/2, -imgH/2);
    
    if(bShowSrc){
        gl::color(1,1,1);
        Surface s = fromOcv(src);
        gl::TextureRef tex = gl::Texture::create(s);
        gl::draw( tex );
    }

    if(bShowProc){
        gl::color(1,1,1);
        Surface s = fromOcv(proc);
        gl::TextureRef tex = gl::Texture::create(s);
        gl::draw( tex );
    }

    if( bShowContour ){
        if( 0 ){
            gl::color(1, 0, 0);
            cv::Mat drawing = cv::Mat::zeros( proc.size(), CV_8UC3 );
            for( int j=0; j<cts.size(); j++ ){
                cv::Scalar color = cv::Scalar( rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255) );
                cv::drawContours( drawing, cts, j, color, 1, 8, hierarchy, 0, cv::Point() );
            }
        }else{
            vbo.draw();
        }
    }

    gl::popMatrices();

    gui->draw();
}

void cApp::keyDown( KeyEvent event ){
    
    char k = event.getChar();
    switch (k) {
        case ' ':
            make();
            break;
    }
    
}

void cApp::mouseDown( MouseEvent event ){
    camUi.mouseDown( event.getPos() );
}

void cApp::mouseMove( MouseEvent event ){
}

void cApp::mouseDrag( MouseEvent event ){
    camUi.mouseDrag( event.getPos(), false/*event.isLeftDown()*/, event.isMiddleDown(), event.isRightDown() );
}


CINDER_APP( cApp, RendererGl )
