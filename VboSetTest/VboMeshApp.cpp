//
// This sample demonstrates basic usage of the VboMesh class by creating a simple Plane mesh
// with a texture mapped onto it. The mesh has static indices and texture coordinates, but
// its vertex positions are dynamic.
//

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/GeomIO.h"
#include "cinder/ImageIo.h"
#include "cinder/CameraUi.h"
#include "cinder/Rand.h"

#include "Exporter.h"
#include "VboSet.h"

using namespace ci;
using namespace ci::app;

using std::vector;

class VboMeshApp : public App {
 public:
	void	setup() override;
	void	update() override;
	void	draw() override;
	void	keyDown( KeyEvent event ) override;

    bool bShader = true;
	gl::VboMeshRef	mVboMesh;
	CameraPersp		mCamera;
	CameraUi		mCamUi;
    Exporter mExp;
    VboSet vbo;
};

void VboMeshApp::setup()
{
    mExp.setup(1080*3, 1920, 0, 100-1, GL_RGB, mt::getRenderPath(), 0, "test" );
    setWindowSize( 1080*3/2, 1920/2 );
    setWindowPos(0, 0);
    
	auto plane = geom::Plane().size( vec2( 20, 20 ) ).subdivisions( ivec2( 2000, 1000 ) );

    mCamUi = CameraUi( &mCamera, getWindow() );

    float w = 6000;
    float h = 6000;
    
    for( int i=0;i<w; i++){
        for( int j=0;j<h; j++){
            vec3 v(i/w-0.5f, j/h-0.5f, randFloat()-0.5 );
            vbo.addPos(v*5.0f);
            vbo.addCol(ColorAf(randFloat(),randFloat(),randFloat(),1));
        }
    }
    
    vbo.init( GL_POINTS );
}

void VboMeshApp::update()
{
}
 void VboMeshApp::draw()
{
    mExp.begin( mCamera );
    gl::clear( Color(0.5,0.5,0.5) );
    gl::color(1,0,0);
    bShader ? vbo.drawShader() : vbo.draw();
    mExp.end();
    mExp.draw();
    
    gl::color(1, 1, 1);
    gl::drawString("fps " + to_string(getAverageFps()), vec2(20,20) );
}


void VboMeshApp::keyDown( KeyEvent event )
{

    switch( event.getChar() ){
        case 'w':
            gl::setWireframeEnabled( ! gl::isWireframeEnabled() );
            break;
            
        case 's':
            mExp.snapShot();
            writeImage(mt::getRenderPath()/"test.png", copyWindowSurface() );
            break;
            
        case 't':
            bShader = !bShader;
            break;
    }
}


CINDER_APP( VboMeshApp, RendererGl( RendererGl::Options().msaa( 0 ) ) )
