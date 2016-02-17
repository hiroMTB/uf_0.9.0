#include "cinder/app/App.h"
#include "cinder/Rand.h"
#include "cinder/Utilities.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/Texture.h"
#include "cinder/Camera.h"
#include "cinder/CameraUi.h"
#include "cinder/Perlin.h"

#include "cinder/audio/dsp/Fft.h"
#include "cinder/audio/SamplePlayerNode.h"

#include "ConsoleColor.h"
#include "Exporter.h"
#include "mtUtil.h"
#include "SoundWriter.h"
#include "VboSet.h"
#include "Dft.h"

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
    
    void procIfft();
    void fftCheck();
    void dftCheck();
    
    CameraUi camUi;
    Perlin mPln;
    Exporter mExp;
    
    bool bOrtho = false;
    CameraPersp cam;
    
    VboSet vWav;
    
    VboSet vMag;
    VboSet vSpec;
    VboSet vWav_inv;

};

void cApp::setup(){
    setWindowPos( 0, 0 );

    float w = 1920;
    float h = 1080;
    
    setWindowSize( w, h );
    
    cam = CameraPersp(w, h, 55.0f, 1, 10000 );
    cam.lookAt( vec3(0,0,1200), vec3(0,0,0) );
    cam.setLensShift( 0,0 );
    camUi.setCamera( &cam );
    
//    fftCheck();
    dftCheck();
}

void cApp::dftCheck(){
    int samplingRate = 44100;
    int fftSize = 1024*15;
    fftSize = Dft::findBiggestDftSize(fftSize);
    
    float xscale = getWindowWidth()/(float)fftSize * 0.8;
    
    //
    //      1. make sine wave
    //
    bool loadAudioFile = true;
    audio::Buffer wave(fftSize);
    
    if( !loadAudioFile ){
        float * f = wave.getChannel(0);
        
        vector<float> freq = {4000.0f};
        
        for( int j=0; j<freq.size(); j++){
            
            float stepRad = freq[j]*2.0f*pi / (float)samplingRate;
            for( int i=0; i<wave.getSize(); i++){
                float s = sin( i * stepRad ) * 0.1f;
                f[i] += s;
            }
        }
    }else{
        
        fs::path path = mt::getAssetPath()/"snd"/"data2wav"/"hamosaic_slope1.tif-80db.wav";
        audio::SourceFileRef src = audio::load( loadFile(path));
        audio::BufferRef buf = src->loadBuffer();
        
        int st = buf->getNumFrames()*0.5;
        for( int i=0; i<fftSize; i++){
            wave.getChannel(0)[i] = buf->getChannel(0)[i+st];
        }
    }
    
    for( int i=0; i<wave.getSize(); i++){
        float s = wave.getChannel(0)[i];
        vWav.addPos( vec3(i*xscale, s*100.0f, 0) );
        vWav.addCol( ColorAf(1,1,1,1) );
    }
    vWav.init(GL_POINTS);
    
    
    Dft dft;
    audio::BufferSpectral spec = audio::BufferSpectral( fftSize );
    float fftxscale = 0.1f;
    
    float fftRes =  samplingRate/(float)fftSize;
    cout << "fft Hz resolution : " << fftRes << endl;
    dft.forward(&wave, &spec, fftSize );
    
    float magScale = 1.0f / fftSize;
    float * real = spec.getReal();
    float * imag = spec.getImag();
    
    float max_v = 0;
    int max_index = -1;
    for( size_t i = 0; i<fftSize/2; i++ ) {
        float re = real[i];
        float im = imag[i];
        float m =  std::sqrt( re*re + im*im ) * magScale;
        vMag.addPos( vec3( i*fftxscale, 0.1f+m*300.0f, 0) );
        vMag.addPos( vec3( i*fftxscale, 0, 0) );
        vMag.addCol( ColorAf(0,1,0,1) );
        vMag.addCol( ColorAf(0,1,0,1) );
        
        vSpec.addPos( vec3(re, im, 0.0f)*10.0f);
        vSpec.addCol(ColorAf(1,0,0,0.7f));
        
        if( max_v<m ){
            max_v = m;
            max_index = i;
        }
    }
    
    float max_freq = ((float)max_index-0.5f) * fftRes;
    cout << "bin: " << max_index << ", freq: " << max_freq << " Hz, amp: " << max_v << endl;
    vMag.init(GL_LINES);
    vSpec.init(GL_POINTS);
    
    //
    //      3. idft
    //
    wave.zero();
    dft.inverse(&spec, &wave, fftSize );
    
    for( int i=0; i<wave.getSize(); i++){
        float d = wave[i];
        vWav_inv.addPos( vec3(i*xscale, d*100.0f, 0) );
        vWav_inv.addCol( ColorAf(1,1,1,1) );
    }
    vWav_inv.init(GL_POINTS);

}

void cApp::fftCheck(){

    int samplingRate = 44100;
    int fftSize = 512;
    float xscale = getWindowWidth()/(float)fftSize * 0.8;
    
    //
    //      1. make sine wave
    //
    audio::Buffer wave(fftSize);
    float * f = wave.getChannel(0);
    
    vector<float> freq = {4000.0f};
    
    for( int j=0; j<freq.size(); j++){
        
        float stepRad = freq[j]*2.0f*pi / (float)samplingRate;
        for( int i=0; i<wave.getSize(); i++){
            float s = sin( i * stepRad ) * 0.1f;
            f[i] += s;
        }
    }
    
    //
    //      2. fft
    
    //      2.1 windowing
    if(0){
        int windowSize = fftSize;
        audio::AlignedArrayPtr	windowTable = audio::makeAlignedArray<float>( windowSize );
        audio::dsp::WindowType	windowType = audio::dsp::WindowType::BLACKMAN;
        audio::dsp::generateWindow(windowType, windowTable.get(), windowSize);
        audio::dsp::mul( wave.getData(), windowTable.get(), wave.getData(), windowSize );
    }
    
    for( int i=0; i<wave.getSize(); i++){
        float s = f[i];
        vWav.addPos( vec3(i*xscale, s*100.0f, 0) );
        vWav.addCol( ColorAf(1,1,1,1) );
    }
    vWav.init(GL_POINTS);
    
    
    //      2.2 fft
    unique_ptr<audio::dsp::Fft> fft = unique_ptr<audio::dsp::Fft>( new audio::dsp::Fft( fftSize ) );
    audio::BufferSpectral spec = audio::BufferSpectral( fftSize );
    float fftxscale = 0.1f;

    float fftRes =  samplingRate/(float)fftSize;
    cout << "fft Hz resolution : " << fftRes << endl;
    fft->forward( &wave, &spec);
    float magScale = 1.0f / fft->getSize();
    float * real = spec.getReal();
    float * imag = spec.getImag();
    
    float max_v = 0;
    int max_index = -1;
    for( size_t i = 0; i<fftSize/2; i++ ) {
        float re = real[i];
        float im = imag[i];
        float m =  std::sqrt( re*re + im*im ) * magScale;
        vMag.addPos( vec3( i*fftxscale, 0.1f+m*300.0f, 0) );
        vMag.addPos( vec3( i*fftxscale, 0, 0) );
        vMag.addCol( ColorAf(0,1,0,1) );
        vMag.addCol( ColorAf(0,1,0,1) );
        
        if( max_v<m ){
            max_v = m;
            max_index = i;
        }
    }

    float max_freq = ((float)max_index-0.5f) * fftRes;
    cout << "bin: " << max_index << ", freq: " << max_freq << " Hz, amp: " << max_v << endl;

    vMag.init(GL_LINES);
    
    
    //
    //      3. ifft
    //
    wave.zero();
    fft->inverse(&spec, &wave);
    
    for( int i=0; i<wave.getSize(); i++){
        float d = wave[i];
        vWav_inv.addPos( vec3(i*xscale, d*100.0f, 0) );
        vWav_inv.addCol( ColorAf(1,1,1,1) );
    }
    vWav_inv.init(GL_POINTS);
}

void cApp::update(){
}

void cApp::draw(){
    gl::clear();
    gl::enableAlphaBlending();
    gl::setMatrices( camUi.getCamera() );

    gl::translate( -getWindowWidth()*0.4f, 0, 0);

    gl::pointSize(1);
    gl::translate(0, 400, 0);
    gl::color(1, 1, 1);
    gl::drawLine(vec3(0,100,0), vec3(0,-100,0) );
    vWav.draw();

    gl::pointSize(3);
    gl::translate(0, -200, 0);
    vMag.draw();
    
    gl::pointSize(1);
    gl::translate(0, -200, 0);
    gl::drawLine(vec3(0,100,0), vec3(0,-100,0) );
    vWav_inv.draw();
    
    gl::translate(0, -400, 0);
    gl::pointSize(2);
    gl::drawLine(vec3(-200,0,0), vec3(200,0,0) );
    gl::drawLine(vec3(0,200,0), vec3(0,-200,0) );
    vSpec.draw();

}

void cApp::keyDown( KeyEvent event ) {
}

void cApp::mouseDown( MouseEvent event ){
    camUi.mouseDown( event.getPos() );
}

void cApp::mouseDrag( MouseEvent event ){
    camUi.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
}

void cApp::resize(){
    CameraPersp & cam = const_cast<CameraPersp&>(camUi.getCamera());
    cam.setAspectRatio( getWindowAspectRatio() );
    camUi.setCamera( &cam );
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 )) )
