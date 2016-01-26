#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/GeomIO.h"
#include "cinder/ImageIo.h"
#include "cinder/CameraUi.h"
#include "cinder/Rand.h"
#include "cinder/Perlin.h"
#include "Exporter.h"
#include "tbb/tbb.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace tbb;

class cApp : public App {
 public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyDown( KeyEvent event ) override;

    void test_tie();
    void test_lambda();
    void callee(int i);
    
    int frame = 0;
    
    Perlin mPln;
    Exporter mExp;
    
};

void cApp::setup(){
    mExp.setup(1080*3, 1920, 0, 100-1, GL_RGB, mt::getRenderPath(), 0, "test" );
    setWindowSize( 1080*3*0.7, 1920*0.7 );
    setWindowPos(0, 0);
    
    mPln.setSeed( 345 );
    mPln.setOctaves( 4 );
    
    //test_tie();
    test_lambda();
}


struct Particle {
    Particle(string _name, int _age, vec3 _pos):name(_name), age(_age), pos(_pos){};
    std::string name;
    int age;
    vec3 pos;
    void print(){
        cout << name << " " << pos << "" << age << " " << endl;
    }
};

/*
 
        easy comparare multiple object with std::tie
 
 */
void cApp::test_tie(){
    
    vector<Particle> ps{ Particle("eee", 100, vec3(555,2,3) ), Particle("aaa", 10, vec3(222,333,444) ), Particle("eee", 1010, vec3(111,2,3) ) };
    
    int i=0;
    for( auto &p : ps){ cout << ++i << " "; p.print(); }
    
    sort(ps.begin(), ps.end(), [](auto&r, auto &l){
        return tie(r.name, r.pos.x, r.pos.y, r.pos.z, r.age ) < tie(l.name, l.pos.x, l.pos.y, l.pos.z, l.age );
    });
    
    cout << endl << "after sort" << endl;
    int i2=0;
    for( auto &p : ps){ cout << ++i2 << " "; p.print(); }
    
}

/*
 
        sandbox with lambda, bind, function, tbb
 
 */
void cApp::test_lambda(){
    int cnt = 0;
    
    function<void(int)> f = [&](int i){
        cnt += i;
        callee(cnt);
    };

    vector<function<void()>> funcs;
    for(int i=0; i<10; i++){
        funcs.push_back( bind(f, i) );
    }
    
    int n = funcs.size();
    if(0){
        parallel_for( blocked_range<size_t>(0, n), [&](const blocked_range<size_t>r){
            for( int i=r.begin(); i!=r.end(); i++){ funcs[i]; }
        });
    }else{
        for(int i=0; i<n; i++){
            funcs[i]();             // <- do not forget () to call function
        }
    }
}

void cApp::callee(int i){
    cout << "I am " << i << endl;
}

void cApp::update(){
    
    
}

void cApp::draw(){
    glPointSize(1);
    glLineWidth(1);
    gl::enableAlphaBlending();
    
    mExp.beginPersp();
    {
        gl::clear( Color(0,0,0) );
        gl::color(1,0,0);
        gl::drawSolidCircle(vec2(100,100), 50);
    }
    mExp.end();
    
    mExp.draw();
    
    gl::pushMatrices();
    {
        gl::color(1, 1, 1);
        gl::drawString("fps      " + to_string( getAverageFps()),   vec2(20,20) );
        gl::drawString("frame    " + to_string( frame ),   vec2(20,35) );
    }
    gl::popMatrices();
    
    frame+=1;
}

void cApp::keyDown( KeyEvent event ){
    switch( event.getChar() ){
        case 'S': mExp.startRender(); break;
        case 's': mExp.snapShot(); break;
    }
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 ) ) )
