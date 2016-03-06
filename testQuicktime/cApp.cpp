#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/Surface.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/Rand.h"
#include "cinder/qtime/QuickTimeGl.h"

//#define USE_MovieGlRef

using namespace ci;
using namespace ci::app;
using namespace std;

class QTimeAdvApp : public App {
  public:
	void setup();
	void keyDown( KeyEvent event );

	void update();
	void draw();

	void loadMovieFile( const fs::path &path );

#ifdef USE_MovieGlRef
	qtime::MovieGlRef mov;
#else
    qtime::MovieSurfaceRef mov;
    SurfaceRef mSurface;
#endif
    
};


void QTimeAdvApp::setup(){

    setWindowPos(0, 0);
    setWindowSize(1920, 1080);
    setFrameRate(25);
    
    fs::path moviePath = getOpenFilePath();
    if( ! moviePath.empty() ){
        
#ifdef USE_MovieGlRef
        mov = qtime::MovieGl::create( moviePath );
#else
        mov = qtime::MovieSurface::create( moviePath );
#endif
        console() << "Dimensions:" << mov->getWidth() << " x " << mov->getHeight() << std::endl;
        console() << "Duration:  " << mov->getDuration() << " seconds" << std::endl;
        console() << "Frames:    " << mov->getNumFrames() << std::endl;
        console() << "Framerate: " << mov->getFramerate() << std::endl;
        mov->setLoop( true, true );
        mov->seekToStart();
        mov->play();
    }
}

void QTimeAdvApp::keyDown( KeyEvent event ){
    //mov->seekToFrame( 200 );
    mov->seekToTime( 8 );
}

void QTimeAdvApp::update()
{
    
    if( mov ){
#ifdef USE_MovieGlRef
        
#else
        mSurface = mov->getSurface();
#endif
        mov->seekToFrame( getElapsedFrames());
        //mov->stepForward();
    }
}

void QTimeAdvApp::draw()
{
    gl::clear( Color( 0, 0, 0 ) );
 
#ifdef USE_MovieGlRef
    int totalWidth = 0;
    totalWidth += mov->getWidth();
    
    int drawOffsetX = 0;
    float relativeWidth = mov->getWidth() / (float)totalWidth;
    gl::TextureRef texture = mov->getTexture();
    if( texture ) {
        float drawWidth = getWindowWidth() * relativeWidth;
        float drawHeight = ( getWindowWidth() * relativeWidth ) / mov->getAspectRatio();
        float x = drawOffsetX;
        float y = ( getWindowHeight() - drawHeight ) / 2.0f;
        
        gl::color( Color::white() );
        gl::draw( texture, Rectf( x, y, x + drawWidth, y + drawHeight ) );
    }
    drawOffsetX += getWindowWidth() * relativeWidth;
#else
    if( ( ! mov ) || ( ! mSurface ) )
        return;
  	gl::draw( gl::Texture::create( *mSurface ) );
#endif
    
    gl::color(1, 1, 1);
    gl::drawString("fps: " + to_string(getAverageFps()), vec2(10,10) );
    
}

CINDER_APP( QTimeAdvApp, RendererGl )