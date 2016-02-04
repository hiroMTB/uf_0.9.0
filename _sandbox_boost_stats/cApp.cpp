#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/GeomIO.h"
#include "cinder/ImageIo.h"
#include "cinder/CameraUi.h"
#include "cinder/Rand.h"
#include "cinder/Perlin.h"

#include "tbb/tbb.h"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/moment.hpp>
#include <boost/accumulators/statistics/tail_quantile.hpp>
#include <boost/ref.hpp>
#include <boost/bind.hpp>

#include "Exporter.h"
#include "mtUtil.h"
#include "VboSet.h"

using namespace boost::accumulators;

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace tbb;
using namespace boost;

class cApp : public App {
 public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyDown( KeyEvent e ) override;
    void mouseDown( MouseEvent e) override;
    void mouseDrag( MouseEvent e) override;
    
    void make_data();
    void calc_mean();
    void calc_mean_manual();
    void calc_histogram();
    void calc_histo_manual();
    void calc_quantile();
    
    int frame = 0;
    Perlin mPln;
    VboSet dot;         // points for data
    VboSet hisline;     // lines for histogram
    VboSet hisline2;    // lines for histogram manual
    
    VboSet lines;       // lines for min, max, mean, median
    VboSet qlines;      // lines for quantile value
    
    CameraPersp camP;
    CameraUi camUi;
    
    vector<float> data;
};

void cApp::setup(){
    setWindowSize( 1920, 1080 );
    setWindowPos(0, 0);
    
    mPln.setSeed( mt::getSeed() );
    mPln.setOctaves( 4 );
    
    camP.setPerspective(60, getWindowAspectRatio(), 1, 10000);
    camP.lookAt( vec3(0,0,300), vec3(0,0,0) );
    camUi.setCamera( &camP );
    
    make_data();
    calc_mean();
    
    calc_histogram();
    calc_histo_manual();
    calc_quantile();
}

void cApp::make_data(){
    
    int nd = 100'000;
    for( int i=0; i<nd; i++){
        float n = mPln.noise(0.31+i*2.99f) * 100.0f;
        float r = randFloat(-10.0f, 10.0f);
        float d = n + r + 10.0f;
        data.push_back( d );
    }

    sort( data.begin(), data.end() );
    
    for( int i=0; i<nd; i++){
        float d = data[i];
        float x = (float)i/nd*100.0f;
        float y = d;
        dot.addPos( vec3( y, x, 0) );
        dot.addCol( ColorAf(0,0,0, 0.7) );
    }
    
    dot.init(GL_POINTS);
}

void cApp::calc_mean_manual(){
    
    vector<float> tmp = data;
    sort(tmp.begin(), tmp.end() );

    int nData = data.size();
    float sum = 0;
    for( int i=0; i<nData; i++){
        sum += data[i];
    }
    
    float median;
    if( nData%2 == 0){
        int mPos = (nData-1)/2;
        float m1 = tmp[mPos];
        float m2 = tmp[mPos+1];
        median = (m1 + m2) * 0.5f;
    }else{
        int mPos = (nData-1)/2;
        median = tmp[mPos];
    }

    cout << "manual mean : " << sum/nData << endl;
    cout << "manual median : " << median << endl;
    cout << "manual sum : " << sum << endl;
    cout << endl;
}

void cApp::calc_mean(){

    // push to acc
    accumulator_set<float, stats<tag::min, tag::max, tag::mean, tag::median, tag::sum> > acc;
    std::for_each( data.begin(), data.end(), bind<void>( boost::ref(acc), _1) );
    
    // calc statics
    float rMin = extract::min(acc);
    float rMax = extract::max(acc);
    float rMean = extract::mean(acc);
    float rMedian = extract::median(acc);
    float rSum = extract::sum(acc);
    
    float y = 0;
    float h = +3;
    float z = 0;
    
    lines.addPos( vec3(rMean, y, z) );
    lines.addPos( vec3(rMean, y+h, z) );
    lines.addCol( Colorf(1,0,0) );
    lines.addCol( Colorf(1,0,0) );
    
    lines.addPos( vec3(rMedian, y, z) );
    lines.addPos( vec3(rMedian, y+h, z) );
    lines.addCol( Colorf(0,1,0.5) );
    lines.addCol( Colorf(0,1,0.5) );
    
    lines.addPos( vec3(rMin, y, z) );
    lines.addPos( vec3(rMin, y+h, z) );
    lines.addCol( Colorf(0,0,0) );
    lines.addCol( Colorf(0,0,0) );

    lines.addPos( vec3(rMax, y, z) );
    lines.addPos( vec3(rMax, y+h, z) );
    lines.addCol( Colorf(0,0,0) );
    lines.addCol( Colorf(0,0,0) );

    lines.init( GL_LINES );

    cout << "Min:      " << rMin << endl;
    cout << "Max:      " << rMax << endl;
    cout << "Mean:     " << rMean << endl;
    cout << "Median:   " << rMedian << endl;
    cout << "Sum:      " << rSum << endl;
    
}

void cApp::calc_histogram(){
    
    float yscale = 100 * 5 * 6;

    accumulator_set<float, stats<tag::density >> acc(tag::density::cache_size=data.size(), tag::density::num_bins=600);
    for_each(data.begin(), data.end(), boost::bind(boost::ref(acc), _1));
    
    typedef pair<float, float> value_type;
    typedef boost::iterator_range<
        vector<value_type>::iterator
    > histo_type;
    
    histo_type histo = extract::density( acc );
    
    float sum = 0;
    int hsize = histo.size();
    cout << "histo band size = " << hsize << endl;
    
    for( auto & h:histo){
        float v1 = h.first;
        float amp = h.second;
        float hv = amp * yscale;
        
        hisline.addPos( vec3(v1, 0, 10.0f ) );
        hisline.addPos( vec3(v1, hv, 10.0f ) );
        hisline.addCol( Colorf(0,0,0) );
        hisline.addCol( Colorf(0,0,0) );
        
        //cout << "histo : " << v1 << ", " << amp << endl;
        sum += amp;
    }
    cout << "histo amp sum: " << sum << endl;   // must be 1.0
    
    hisline.init(GL_LINES);
}


void cApp::calc_histo_manual(){
    
    float yscale = 100 * 5;
    
    vector<int> mHisto;
    mHisto.assign(1000, 0);
    
    for( int i=0; i<data.size(); i++){
        float d = data[i];
        mHisto[round(d)+500] += 1;
    }
    
    for( int i=0; i<mHisto.size(); i++){
        float h = mHisto[i] /(float)data.size() * yscale;

        hisline2.addPos( vec3(i-500, 0, 5) );
        hisline2.addPos( vec3(i-500, -h, 5) );
        hisline2.addCol( Colorf(0,0,1) );
        hisline2.addCol( Colorf(0,0,1) );
    }
    hisline2.init( GL_LINES );
}

void cApp::calc_quantile(){
    
    accumulator_set<float, stats<tag::tail_quantile<accumulators::left>>> acc(tag::tail<accumulators::left>::cache_size=data.size() );
    
    for_each(data.begin(), data.end(), bind<void>( boost::ref(acc), _1) );

    float q025 = quantile(acc, quantile_probability = 0.25 );
    float q050 = quantile(acc, quantile_probability = 0.50 );
    float q075 = quantile(acc, quantile_probability = 0.75 );
    
    float y = 0;
    float h = 3;
    float z = 0;

    qlines.addPos( vec3(q025, y, z) );
    qlines.addPos( vec3(q025, y+h, z) );
    qlines.addCol( Colorf(0.1, 0, 0.1) );
    qlines.addCol( Colorf(0.1, 0, 0.1) );

    qlines.addPos( vec3(q050, y, z) );
    qlines.addPos( vec3(q050, y+h, z) );
    qlines.addCol( Colorf(0.1, 0, 0.1) );
    qlines.addCol( Colorf(0.1, 0, 0.1) );
    
    qlines.addPos( vec3(q075, y, z) );
    qlines.addPos( vec3(q075, y+h, z) );
    qlines.addCol( Colorf(0.1, 0, 0.1) );
    qlines.addCol( Colorf(0.1, 0, 0.1) );

    // IQR interquartile range
    qlines.addPos( vec3(q025, y+h/2, z) );
    qlines.addPos( vec3(q075, y+h/2, z) );
    qlines.addCol( Colorf(0.1, 0, 0.1) );
    qlines.addCol( Colorf(0.1, 0, 0.1) );
    
    float step = 0.01;
    for( float q=step; q<1.0f; q+=step){
        float qval = quantile(acc, quantile_probability = q );
        qlines.addPos( vec3(qval, y-h, z) );
        qlines.addPos( vec3(qval, y-h*1.5, z) );
        qlines.addCol( Colorf(0.1, 0, 0.1) );
        qlines.addCol( Colorf(0.1, 0, 0.1) );
    }

    qlines.init(GL_LINES);
}

void cApp::update(){
    
}

void cApp::draw(){
    
    gl::clear( Color(0.9,0.9,0.9) );
    
    glPointSize(1);
    glLineWidth(1);
    gl::enableAlphaBlending();
    
    gl::pushMatrices();
    mt::setMatricesWindow(getWindowWidth(), getWindowHeight(), true, false, -100, 100);
    //gl::setMatrices( camUi.getCamera() );
    {
        //gl::translate( -getWindowWidth()/4, -getWindowHeight()/4, 0 );
        gl::translate( 0, -getWindowHeight()/4, 0 );
        gl::scale(8,8,1);
        
        mt::drawCoordinate(100);

        gl::pointSize(1);
        dot.draw();
        
        gl::lineWidth(1);
        hisline.draw();
        hisline2.draw();

        gl::lineWidth(2);
        gl::translate( 0, -20, 0);
        lines.draw();

        gl::translate( 0, -5, 0);
        qlines.draw();
    }
    gl::popMatrices();
    
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

    switch( event.getCode() ){
        //case KeyEvent::KEY_RIGHT: z++; break;
        //case KeyEvent::KEY_LEFT: z--; break;
            
    }
}

void cApp::mouseDown( MouseEvent e){
    camUi.mouseDown(e);
}

void cApp::mouseDrag( MouseEvent e){
    camUi.mouseDrag(e);
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 ) ) )
