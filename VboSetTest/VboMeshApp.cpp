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

#include "Exporter.h"

using namespace ci;
using namespace ci::app;

using std::vector;

class VboMeshApp : public App {
 public:
	void	setup() override;
	void	update() override;
	void	draw() override;

	void	keyDown( KeyEvent event ) override;

  private:
	gl::VboMeshRef	mVboMesh;
	gl::TextureRef	mTexture;
	
	CameraPersp		mCamera;
	CameraUi		mCamUi;
    Exporter mExp;
};

void VboMeshApp::setup()
{
    
    mExp.setup(1080*3, 1920, 0, 100-1, GL_RGB, mt::getRenderPath(), 0, "test" );
    setWindowSize( 1080*3/2, 1920/2 );
    setWindowPos(0, 0);
    
	auto plane = geom::Plane().size( vec2( 20, 20 ) ).subdivisions( ivec2( 200, 50 ) );

	vector<gl::VboMesh::Layout> bufferLayout = {
		gl::VboMesh::Layout().usage( GL_DYNAMIC_DRAW ).attrib( geom::Attrib::POSITION, 3 ),
		gl::VboMesh::Layout().usage( GL_STATIC_DRAW ).attrib( geom::Attrib::TEX_COORD_0, 2 )
	};

	mVboMesh = gl::VboMesh::create( plane, bufferLayout );
	mTexture = gl::Texture::create( loadImage( loadFile( "../cinder_logo.png" ) ), gl::Texture::Format().loadTopDown() );
    mCamUi = CameraUi( &mCamera, getWindow() );

    auto mappedPosAttrib = mVboMesh->mapAttrib3f( geom::Attrib::POSITION, false );
    for( int i = 0; i < mVboMesh->getNumVertices(); i++ ) {
        vec3 &pos = *mappedPosAttrib;
        mappedPosAttrib->x *= 100;
        mappedPosAttrib->y *= 100.0;
        mappedPosAttrib->z *= 100.0;
        ++mappedPosAttrib;
    }
}

void VboMeshApp::update()
{
	float offset = getElapsedSeconds() * 4.0f;

	auto mappedPosAttrib = mVboMesh->mapAttrib3f( geom::Attrib::POSITION, false );
	for( int i = 0; i < mVboMesh->getNumVertices(); i++ ) {
		vec3 &pos = *mappedPosAttrib;
		mappedPosAttrib->y = sinf( pos.x*0.005 * 1.1467f + offset ) * 0.323f + cosf( pos.z*0.005 * 0.7325f + offset ) * 0.431f;
        mappedPosAttrib->y *= 100.0;
		++mappedPosAttrib;
	}
	mappedPosAttrib.unmap();
}

void VboMeshApp::draw()
{
    mExp.begin( mCamera );
    
    gl::clear( Color(1,1,1) );
    gl::color(1,0,0);

    //gl::translate(1080*3/2, 1920/2);
    //gl::drawSolidCircle( vec2(0,0), 100);

    gl::scale(0.01, 0.01, 0.01);
    {
        gl::ScopedGlslProg glslScope( gl::getStockShader( gl::ShaderDef().texture() ) );
        gl::ScopedTextureBind texScope( mTexture );
        gl::draw( mVboMesh );
    }

    mExp.end();
    mExp.draw();
}


void VboMeshApp::keyDown( KeyEvent event )
{
    if( event.getChar() == 'w' ){
        
        gl::setWireframeEnabled( ! gl::isWireframeEnabled() );
        
    }else if( event.getChar() == 's' ){
        mExp.snapShot();
        writeImage(mt::getRenderPath()/"test.png", copyWindowSurface() );
    }
}


CINDER_APP( VboMeshApp, RendererGl( RendererGl::Options().msaa( 0 ) ) )
