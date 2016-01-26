#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/GeomIO.h"
#include "cinder/ImageIo.h"
#include "cinder/CameraUi.h"
#include "cinder/Rand.h"
#include "cinder/Perlin.h"

#include "Exporter.h"
#include "VboSet.h"
#include "tbb/tbb.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace tbb;

class Particle{
    
public:
    
    vec3 pos;
    vec3 vel;
    vec3 force;
    vec3 dir;
    ColorAf col;
    
    float mass;
    int id;
    int life;
    
    bool operator<( const Particle & p ) const {
        return life < p.life;
    }
    
    bool operator>( const Particle & p ) const {
        return life > p.life;
    }
    
};



class cApp : public App {
 public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyDown( KeyEvent event ) override;

    void add_particle();
    void update_particle();
    void update_vbo();
    
    void expand_func();
    vec4 dfBm( float x, float y, float z, float w, float turb, int mOctaves );
    
    bool bShader = false;
    int ptclCount = 0;
    int frame = 0;
    
	gl::VboMeshRef mVboMesh;
    CameraPersp	mCamera;

    CameraUi mCamUi;
    Perlin mPln;
    
    Exporter mExp;
    VboSet vbo;
    vector<Particle> ptcl;

};

static inline float fade( float t ) { return t * t * t * (t * (t * 6 - 15) + 10); }
static inline float dfade( float t ) { return 30.0f * t * t * ( t * ( t - 2.0f ) + 1.0f ); }
inline float nlerp(float t, float a, float b) { return a + t * (b - a); }

void cApp::setup()
{
    mExp.setup(1920, 1080, 0, 100-1, GL_RGB, mt::getRenderPath(), 0, "test" );
    setWindowSize( 1920, 1080 );
    setWindowPos(0, 0);

    mCamera.setNearClip(1);
    mCamera.setFarClip(10000);
    mCamera.lookAt(vec3(0,0,-1200), vec3(0,0,0) );
    mCamUi = CameraUi( &mCamera, getWindow() );
    
    add_particle();
    
    mPln.setSeed( 345 );
    mPln.setOctaves( 4 );
}

void cApp::update(){
    
    add_particle();
    update_particle();
    update_vbo();
}

vec3 newVelocity( quat orientation, float emissionAngle, float averageVelocity, float velocityVariation){
    float phi = randFloat(0, 2*M_PI);
    float nz = randFloat(cos(emissionAngle), 1.0);
    float nx = sqrt(1-nz*nz)*cos(phi);
    float ny = sqrt(1-nz*nz)*sin(phi);
    vec3 velDir = glm::rotate(orientation, vec3(nx, ny, nz));
    float velNorm = averageVelocity*(1.0 + randFloat(-velocityVariation, velocityVariation)/100.0);
    vec3 newVel = velDir*velNorm;
    return newVel;
}

void cApp::add_particle(){
    
    int nAdd = MIN(frame, 1000+mPln.noise(frame*0.01)*100.0f);
    for( int i=0; i<nAdd; i++){
        
        Particle pt;
        pt.life = randInt(1000, 2000);
        pt.id = ptclCount++;
        pt.mass = randFloat(0.5, 1.1);
        vec3 ax(0.2, 1, 0.2);
        pt.force = newVelocity( angleAxis( mPln.noise(frame*0.01f), ax), 0, 5, 2);
        pt.pos = vec3( 0,0,0 );
        pt.dir = randVec3()-0.5f;
        ptcl.push_back( pt );
    }
}

vec4 cApp::dfBm( float x, float y, float z, float w, float turb, int mOctaves )
{
    vec4 result;
    
    for( uint8_t i = 0; i < mOctaves; i++ ) {
        float scale = (1.0 / 2.0) * pow(2.0, float(i));
        float noiseScale = pow(turb, float(i));
        if(turb == 0.0 && i == 0){
            noiseScale = 1.0;
        }
        float rate = pow(2.0, float(i));
        result += mPln.dnoise( x*rate, y*rate, z*rate, w*rate ) * noiseScale * scale;
    }
    
    return result;
}

void cApp::update_particle(){
    
    function<void(int i)> func = [&](int i){
        Particle & pt = ptcl[i];
        pt.life--;
        
        float noiseScale = 0.002;
        float timeScale  = 0.001;
        
        vec3 ps = pt.pos * noiseScale;
        
        vec3 yad = vec3(123.4, 129845.6, -1239.1);
        vec3 zad = vec3(-9519.0, 9051.0, -123.0);
        
        vec3 xx = ps;
        vec3 yy = ps + yad;
        vec3 zz = ps + zad;
        
        int octave = 4;
        float turb = 0.8f;
        vec4 nx = dfBm( xx.x, xx.y, xx.z, 10+frame*timeScale, turb, octave );
        vec4 ny = dfBm( yy.x, yy.y, yy.z, 10+frame*timeScale, turb, octave );
        vec4 nz = dfBm( zz.x, zz.y, zz.z, 10+frame*timeScale, turb, octave );
        
        vec3 nvel;
        nvel.x = nz.y-ny.z;
        nvel.y = nx.z-nz.x;
        nvel.z = ny.x-nx.y;

        pt.vel += nvel*0.1f;
        pt.pos += pt.vel*0.1f;
        pt.pos += pt.dir*0.01f;
    
    };
    
    if(1){
        parallel_for( blocked_range<size_t>(0,  ptcl.size()), [&](const blocked_range<size_t>r){
            for( int i=r.begin(); i!=r.end(); i++){
                func(i);
            }
        });
    }else{
        for( int i=0; i<ptcl.size(); i++){
            func(i);
        }
    }
}


void cApp::update_vbo(){
    
    vbo.resetPos();
    vbo.resetCol();
    vbo.resetVbo();
    
    for (int i=0; i<ptcl.size(); i++) {
        if( ptcl[i].life > 0){
            //vbo.addPos( vec3(ptcl[i].pos.x, ptcl[i].pos.y, 0) );
            vbo.addPos( ptcl[i].pos );
            vbo.addCol( ColorAf( ptcl[i].life*0.0006,0,0, ptcl[i].life*0.0006) );
        }
    }
    
    if(vbo.vbo){
        vbo.updateVboPos();
        vbo.updateVboCol();
    }else{
        vbo.init( GL_POINTS );
    }
}

void cApp::draw(){
    glPointSize(1);
    
    gl::enableAlphaBlending();
    
    gl::pushMatrices();
    gl::setMatrices( mCamUi.getCamera() );
    gl::clear( Color(0,0,0) );
    gl::color(1,0,0);
    bShader ? vbo.drawShader() : vbo.draw();
    gl::popMatrices();
    
    gl::pushMatrices();
    gl::color(1, 1, 1);
    gl::drawString("fps      " + to_string( getAverageFps()),   vec2(20,20) );
    gl::drawString("particke " + to_string( ptcl.size() ),      vec2(20,35) );
    gl::drawString("particke " + to_string( vbo.vbo->getNumVertices() ), vec2(20,50) );
    gl::popMatrices();
    frame+=1;
}


void cApp::keyDown( KeyEvent event )
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




/*
 
 
        This is for making 4D perlin noise

        see http://www.iquilezles.org/www/articles/morenoise/morenoise.htm
 
*/
vector<string> stlerp( string t, vector<string> a, vector<string> b){
    vector<string> ret;
    
    for ( auto & s: a ) {
        ret.push_back( s );
        ret.push_back( "-"+t+s );
    }
    
    for ( auto & s: b ) {
        ret.push_back( t+s );
    }
    
    return ret;
}

void cApp::expand_func(){
    vector<string> res =
    stlerp("w",
           stlerp("z",
                  stlerp( "y", stlerp("x",{"a"},{"b"}), stlerp("x",{"c"},{"d"})),
                  stlerp( "y", stlerp("x",{"e"},{"f"}), stlerp("x",{"g"},{"h"}))),
           stlerp("z",
                  stlerp( "y", stlerp("x",{"i"},{"j"}), stlerp("x",{"k"},{"l"})),
                  stlerp( "y", stlerp("x",{"m"},{"n"}), stlerp("x",{"o"},{"p"})))
           );
    
    for( auto &rs : res){
        string s = rs;
        int num_minus = std::count(s.begin(), s.end(), '-');
        if( num_minus!=0){
            int pos = s.find("-");
            while (pos!=string::npos) {
                s.erase( pos, 1 );
                pos = s.find("-");
            }
        }
        
        if(num_minus%2 == 1)
            s.insert( 0, "-" );
        else
            s.insert( 0, "+" );
        
        sort(s.begin(), s.end() );
        
        rs = s;
    }
    
    sort(res.begin(), res.end() );
    
    cout << "total " << res.size() << endl;
    
    vector<string> prt;
    for( auto &s : res){
        
        if( s.find( "wxyz" ) != s.npos ){
            //s.erase(s.find( "wxyz" ), 4);
            //prt.push_back( s );
        }else if( s.find( "xyz" ) != s.npos ){
            //s.erase(s.find( "xyz" ), 3);
            //prt.push_back( s );
        }else if( s.find( "wxy" ) != s.npos ){
            //s.erase(s.find( "wxy" ), 3);
            //prt.push_back( s );
        }else if( s.find( "wxz" ) != s.npos ){
            //s.erase(s.find( "wxz" ), 3);
            //prt.push_back( s );
        }else if( s.find( "wyz" ) != s.npos ){
            //s.erase(s.find( "wyz" ), 3);
            //prt.push_back( s );
        }else if( s.find("xy") != s.npos ){
            //s.erase(s.find( "xy" ), 2);
            //prt.push_back( s );
        }else if( s.find("xz") != s.npos ){
            //s.erase(s.find( "xz" ), 2);
            //prt.push_back( s );
        }else if( s.find("wx") != s.npos ){
            //s.erase(s.find( "wx" ), 2);
            //prt.push_back( s );
        }else if( s.find("yz") != s.npos ){
            //s.erase(s.find( "yz" ), 2);
            //prt.push_back( s );
        }else if( s.find("wy") != s.npos ){
            //s.erase(s.find( "wy" ), 2);
            //prt.push_back( s );
        }else if( s.find("wz") != s.npos ){
            //s.erase(s.find( "wz" ), 2);
            //prt.push_back( s );
        }else if( s.find("x") != s.npos ){
            //s.erase(s.find( "x" ), 1);
            //prt.push_back( s );
        }else if( s.find("y") != s.npos ){
            //s.erase(s.find( "y" ), 1);
            //prt.push_back( s );
        }else if( s.find("z") != s.npos ){
            //s.erase(s.find( "z" ), 1);
            //prt.push_back( s );
        }else if( s.find("w") != s.npos ){
            s.erase(s.find( "w" ), 1);
            prt.push_back( s );
        }
    }
    
    for( auto &p : prt){
        cout << p << " ";
    }
    cout << endl;

}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 ) ) )
