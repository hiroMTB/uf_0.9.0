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

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/moment.hpp>
#include <boost/accumulators/statistics/tail_quantile.hpp>
#include <boost/ref.hpp>
#include <boost/bind.hpp>

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace boost::accumulators;
using namespace boost;


float offsetx = 60;
float offsety = 60;

const int w = 1920;
const int h = 1080;

float ww = w-offsetx*2;
float hh = h-offsety*2;
float xs = ww;
float ys;
float ys1;
float ys2;


class Gdata{
    
public:
    
    Gdata( string _name, float _in, float _out, float _logLevel, int _id, ColorAf c):
    name(_name), in(_in), out(_out), logLevel(_logLevel), id(_id), color(c), show(true){}
    
    
    void makeStats(){
        {
            accumulator_set<float, stats<tag::min, tag::max, tag::mean, tag::median, tag::sum> > acc;
            std::for_each( data.begin(), data.end(), bind<void>( boost::ref(acc), _1) );
            min      = extract::min(acc);
            max      = extract::max(acc);
            mean     = extract::mean(acc);
            median   = extract::median(acc);
            sum      = extract::sum(acc);
        }
        
        
        {
            accumulator_set<float, stats<tag::tail_quantile<accumulators::left>>> acc(tag::tail<accumulators::left>::cache_size=data.size() );
            for_each(data.begin(), data.end(), bind<void>( boost::ref(acc), _1) );
            q025 = quantile(acc, quantile_probability = 0.25 );
            q075 = quantile(acc, quantile_probability = 0.75 );
        }

        printf("prm name        : %s\n", name.c_str() );
        printf("min             : %e\n", min );
        printf("0.25 quantile   : %e\n", q025 );
        printf("0.50 quantile   : %e\n", median );
        printf("0.75 quantile   : %e\n", q075 );
        printf("1.00 quantile   : %e\n", max );
        printf("mean            : %e\n", mean );
        printf("sum             : %e\n", sum );
        
        std::copy( data.begin(), data.end(), back_inserter(data_sort) );
        std::sort(data_sort.begin(), data_sort.end());
    }

    
    void makeVboMap( float xs, float ys){
        
        for( int i=0; i<data.size(); i++ ) {
            
            float x = data[i];
            float y = i*ys;
            float map = lmap(x, min, max, 0.0f, 1.0f);
            map *= xs;
            vMap.addPos( vec3( map, y, 0) );
            vMap.addCol( ColorAf( 0,0,0,1) );
        }
        vMap.init(GL_POINTS);
    }

    void makeVboMapSort( float xs, float ys){
        
        for( int i=0; i<data_sort.size(); i++ ) {
            float x = data_sort[i];
            float y = i*ys;
            float map = lmap(x, min, max, 0.0f, 1.0f);
            map *= xs;
            vMapSort.addPos( vec3( map, y, 0) );
            vMapSort.addCol( color);
        }
        vMapSort.init(GL_LINE_STRIP);
    }

    void makeVboLog(float xs, float ys, int res){
        
        for( int i=0; i<data.size(); i++ ) {
            
            if(i%res!=0) continue;
            
            float x = data[i];
            float y = i*ys;
            float map = lmap(x, min, max, 1.0f, pow(10.0f, logLevel));
            float log = log10(map) / logLevel;
            log *= xs;
            vLog.addPos( vec3( log, y, 0) );
            vLog.addCol( color );
        }
        vLog.init(GL_POINTS);
    }
    
    void makeVboLogSort(float xs, float ys){
        
        for( int i=0; i<data_sort.size(); i++ ) {
            float x = data_sort[i];
            float y = i*ys;
            float map = lmap(x, min, max, 1.0f, pow(10.0f, logLevel));
            float log = log10(map) / logLevel;
            log *= xs;
            vLogSort.addPos( vec3( log, y, 0) );
            vLogSort.addCol( color );
        }
        vLogSort.init(GL_LINE_STRIP);
    }
    
    void makeVboLine( float xs, float ys){
        
        float yst = 0;
        float len = -20;
        
        // zero
        if(0){
            float zero = lmap(0.0f, min, max, 0.0f, 1.0f);
            vLine.addPos(vec3( zero*xs, 0, 0) );
            vLine.addPos(vec3( zero*xs, hh, 0) );
            vLine.addCol(color);
            vLine.addCol(ColorAf(1,0,0,1));
        }
        
        // mean
        if(0){
            float m = lmap(mean, min, max, 0.0f, 1.0f);
            vLine.addPos(vec3( m*xs, yst, 0) );
            vLine.addPos(vec3( m*xs, yst-len, 0) );
            vLine.addCol(ColorAf(1,0,0,1));
            vLine.addCol(ColorAf(1,0,0,1));
        }
        
        
        // median
        {
            float med = lmap(median, min, max, 0.0f, 1.0f);
            vLine.addPos(vec3( med*xs, yst, 0) );
            vLine.addPos(vec3( med*xs, yst-len, 0) );
            vLine.addCol(ColorAf(1,0,0.2,1));
            vLine.addCol(ColorAf(1,0,0.2,1));
        }
        
        // min
        {
            float q = lmap(min, min, max, 0.0f, 1.0f);
            vLine.addPos(vec3( q*xs, yst, 0) );
            vLine.addPos(vec3( q*xs, yst-len, 0) );
            vLine.addCol(ColorAf(0,0,0,1));
            vLine.addCol(ColorAf(0,0,0,1));
        }
        
        // max
        {
            float q = lmap(max, min, max, 0.0f, 1.0f);
            vLine.addPos(vec3( q*xs, yst, 0) );
            vLine.addPos(vec3( q*xs, yst-len, 0) );
            vLine.addCol(ColorAf(0,0,0,1));
            vLine.addCol(ColorAf(0,0,0,1));
        }
        
        // q025
        {
            float q = lmap(q025, min, max, 0.0f, 1.0f);
            vLine.addPos(vec3( q*xs, yst, 0) );
            vLine.addPos(vec3( q*xs, yst-len, 0) );
            vLine.addCol(ColorAf(0,0,0,1));
            vLine.addCol(ColorAf(0,0,0,1));
        }
        
        // q075
        {
            float q = lmap(q075, min, max, 0.0f, 1.0f);
            vLine.addPos(vec3( q*xs, yst, 0) );
            vLine.addPos(vec3( q*xs, yst-len, 0) );
            vLine.addCol(ColorAf(0,0,0,1));
            vLine.addCol(ColorAf(0,0,0,1));
        }
    }

    void makeVboLineLog( float xs, float ys){
    
        float yst = 0;
        float len = -20;
        
        // zero log
        if(0){
            float zero = lmap(0.0f, min, max, 1.0f, pow(10.0f, logLevel));
            zero = log10(zero) / logLevel;
            vLineLog.addPos(vec3( zero*xs, 0, 0) );
            vLineLog.addPos(vec3( zero*xs, hh, 0) );
            vLineLog.addCol(color);
            vLineLog.addCol(color);
        }
        
        
        // mean log
        if(0){
            float m = lmap(mean, min, max, 1.0f, pow(10.0f, logLevel));
            m = log10(m) / logLevel;
            vLineLog.addPos(vec3( m*xs, yst, 0) );
            vLineLog.addPos(vec3( m*xs, yst-len, 0) );
            vLineLog.addCol(color);
            vLineLog.addCol(color);
        }
        
        // median log
        {
            float med = lmap(median, min, max, 1.0f, pow(10.0f, logLevel));
            med = log10(med) / logLevel;
            vLineLog.addPos(vec3( med*xs, yst, 0) );
            vLineLog.addPos(vec3( med*xs, yst-len, 0) );
            vLineLog.addCol(color);
            vLineLog.addCol(color);
        }
        
        // min log
        {
            float q = lmap(min, min, max, 1.0f, pow(10.0f, logLevel));
            q = log10(q) / logLevel;
            vLineLog.addPos(vec3( q*xs, yst, 0) );
            vLineLog.addPos(vec3( q*xs, yst-len, 0) );
            vLineLog.addCol(color);
            vLineLog.addCol(color);
        }
        
        // max log
        {
            float q = lmap(max, min, max, 1.0f, pow(10.0f, logLevel));
            q = log10(q) / logLevel;
            vLineLog.addPos(vec3( q*xs, yst, 0) );
            vLineLog.addPos(vec3( q*xs, yst-len, 0) );
            vLineLog.addCol(color);
            vLineLog.addCol(color);
        }
        
        
        // q025 log
        {
            float q = lmap(q025, min, max, 1.0f, pow(10.0f, logLevel));
            q = log10(q) / logLevel;
            vLineLog.addPos(vec3( q*xs, yst, 0) );
            vLineLog.addPos(vec3( q*xs, yst-len, 0) );
            vLineLog.addCol(color);
            vLineLog.addCol(color);
        }
        
        // q075 log
        {
            float q = lmap(q075, min, max, 1.0f, pow(10.0f, logLevel));
            q = log10(q) / logLevel;
            vLineLog.addPos(vec3( q*xs, yst, 0) );
            vLineLog.addPos(vec3( q*xs, yst-len, 0) );
            vLineLog.addCol(color);
            vLineLog.addCol(color);
        }
        
        vLineLog.init(GL_LINES);
    }
    
    void makeVboHisto( float xs, float ys){
        histo_res = xs*0.5f;
        
        vector<float> histo_m;
        histo_m.assign(histo_res, 0.0f);
        for( int i=0; i<data.size(); i++ ) {
            float d = data[i];
            float map = lmap(d, min, max, 0.0f, (float)histo_res-0.55556f);
            int index = round(map);
            histo_m[index] += 1;
        }
        
        int size = histo_m.size();
        for( int i=0; i<size; i++){
            
            float x = i/(float)size * xs;
            float y = histo_m[i]*ys;
            vHisto.addPos(vec3( x, y+1, 0) );
            vHisto.addPos(vec3( x, 0, 0) );
            vHisto.addCol( color );
            vHisto.addCol( color );
        }
        vHisto.init( GL_LINES );
    }
    
    void makeVboHistoLog( float xs, float ys, float ymax){
        histo_res = xs*0.5f;
        
        vector<float> histo_m;
        histo_m.assign(histo_res, 0.0f);
        for( int i=0; i<data.size(); i++ ) {
            float d = data[i];
            d = lmap(d, min, max, 1.0f, pow(10.0f, logLevel));
            float log = log10(d) / logLevel;
            float map = lmap(log, 0.0f, 1.0f, 0.0f, (float)histo_res-0.55556f);
            int index = round(map);
            histo_m[index] += 1;
        }
        
        int size = histo_m.size();
        for( int i=0; i<size; i++){
            
            float x = i/(float)size * xs;
            float y = histo_m[i]*ys;
            y = MIN(ymax, y);
            vHistoLog.addPos(vec3( x, y+1, 0) );
            vHistoLog.addPos(vec3( x, 0, 0) );
            vHistoLog.addCol( color );
            vHistoLog.addCol( color );
        }
        vHistoLog.init( GL_LINES );
    }

    
    vector<vec3>    pos;
    vector<float>   data, data_sort;
    string          name;

    VboSet          vMap,     vLog;
    VboSet          vMapSort, vLogSort;
    VboSet          vLine,    vLineLog;
    VboSet          vHisto,   vHistoLog;
    
    ColorAf         color;
    bool            show;
    int             id;
    float           in;
    float           out;
    float           min;
    float           max;
    float           mean;
    float           median;
    float           sum;
    float           q025;
    float           q075;

    float           logLevel;
    int             histo_res;
    
    typedef pair<float, float> histo_v;
    typedef boost::iterator_range<
        vector<histo_v>::iterator
    > Histo;
    
    Histo histo;
  
};












//
//
//      cApp
//
//
class cApp : public App {
    
public:
    void setup();
    void update();
    void draw();
    void mouseDown( MouseEvent event );
    void mouseDrag( MouseEvent event );
    void keyDown( KeyEvent event );

    void loadBin( fs::path path, vector<float> & array );
    void resize();
    
    void makeVbo( Gdata & gd);
    
    fs::path assetDir;
    CameraUi camUi;
    CameraPersp cam;
    Perlin mPln;
    Exporter mExp;
    
    float fmax = numeric_limits<float>::max();
    float fmin = numeric_limits<float>::min();
    
    vector<Gdata> gds;
    bool bStart     = false;
    bool bShowStats = false;
    bool bLog       = true;
    
    //string frameName = "rdr_00468_l17.hydro";
    //string frameName = "rdr_01027_l17.hydro";
    string frameName = "rdr_01398_l17.hydro";
};

void cApp::setup(){
    setWindowPos( 0, 0 );
    setWindowSize( w, h );
    mExp.setup( w, h, 0, 10, GL_RGB, mt::getRenderPath(), 0);
    
    assetDir = mt::getAssetPath();
    
#ifdef RENDER
    mExp.startRender();
#endif

    gds.push_back( Gdata( "pos",        0.0f, 1.0f, 0, 0, ColorAf(0,0,0)) );
    gds.push_back( Gdata( "vel_length", 0.5f, 1.0f, 10, 1, ColorAf(1,0,0, 0.6f)) );
    gds.push_back( Gdata( "rho",        0.3f, 1.0f, 10, 2, ColorAf(0,1,0, 0.6f)) );
    gds.push_back( Gdata( "N",          0.0f, 1.0f, 0, 3, ColorAf(0,0,0)) );
    gds.push_back( Gdata( "mass",       0.5f, 1.0f, 10, 4, ColorAf(0,0,1, 0.6f)) );
    gds.push_back( Gdata( "K",          0.8f, 1.0f, 50, 5, ColorAf(0,0,0)) );
    
    
    vector<int> which = {1, 2, 4};

    for( int i=0; i<which.size(); i++){
        int id = which[i];
        string prmName = gds[id].name;
        loadBin(assetDir/"sim"/"garaxy"/"bin"/prmName/(frameName+"_"+ prmName + ".bin"), gds[id].data );
        gds[id].makeStats();
    }

    ys = hh/(float)gds[1].data.size();
    ys1 = ys * 0.25f;
    
    float hs = 60;

    for( int i=0; i<which.size(); i++){
        int id = which[i];

        if( bLog ){
            gds[id].makeVboLog( xs, ys1, 20 );
            gds[id].makeVboLogSort( xs, ys1 );
            gds[id].makeVboLineLog( xs, ys );
            gds[id].makeVboHistoLog( xs, ys1*hs, hh*0.3f );
        }else{
            gds[id].makeVboMap( xs, ys1 );
            gds[id].makeVboMapSort( xs, ys1 );
            gds[id].makeVboLine( xs, ys );
            gds[id].makeVboHisto( xs, ys1*hs );
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

    if(1){
        int res = 10;
        vector<float> prx;
        for(int i=0; i<array.size(); i++){
            if(i%res!=0) continue;
            prx.push_back(array[i]);
        }
        array.clear();
        array = prx;
    }
}


void cApp::update(){
}

void cApp::draw(){
    
    //mExp.begin( camUi.getCamera() );{
    mExp.beginOrtho(false, false);{
        
        gl::translate( w, 0, 0);
        gl::scale(-1, 1, 1);
        
        gl::clear( ColorA(1,1,1) );
        gl::enableAlphaBlending();
        gl::lineWidth( 1 );
        gl::pointSize( 1 );
        
        gl::translate(offsetx,offsety);
        
        for( int i=0; i<gds.size(); i++ ){
            if( gds[i].show ){

                if( bLog ){
                    gl::pushMatrices();
                    
                    gl::translate(0,0,-1);
                    gds[i].vLineLog.draw();
                    
                    gl::translate(0,hh*0.05,1);
                    gds[i].vHistoLog.draw();

                    gl::translate(0,hh*0.33,0);
                    gds[i].vLogSort.draw();

                    gl::translate(0,hh*0.33,0);
                    gds[i].vLog.draw();
                    gl::popMatrices();
                }else{
                    gl::pushMatrices();
                    gds[i].vLine.draw();
                    gl::translate(0,hh*0.05,0);
                    gds[i].vHisto.draw();
                    
                    gl::translate(0,hh*0.33,0);
                    gds[i].vMapSort.draw();
                    
                    gl::translate(0,hh*0.33,0);
                    gds[i].vMap.draw();
                    gl::popMatrices();
                }
            }
        }
        
        if(0){
            gl::translate(0,0,-1);
            gl::color(0.5, 0.5, 0.5);
            gl::drawLine(vec3(-offsetx/2,0,0), vec3(w-offsetx*2, 0, 0));
            gl::drawLine(vec3(0,-offsety/2,0), vec3(0, h-offsety*2, 0));
        }

    }mExp.end();
    
    mExp.draw();
    
}

void cApp::keyDown( KeyEvent event ) {
    switch ( event.getChar() ) {
        case 'S': mExp.startRender();       break;
        case 's': mExp.snapShot( frameName+".png" ); break;
        case 't': bShowStats = !bShowStats; break;
        case '0': gds[0].show=!gds[0].show; break;  // pos, no vbo
        case '1': gds[1].show=!gds[1].show; break;  // vel
        case '2': gds[2].show=!gds[2].show; break;  // rho
        case '3': gds[3].show=!gds[3].show; break;  // N,   no vbo
        case '4': gds[4].show=!gds[4].show; break;  // mass
        case '5': gds[5].show=!gds[5].show; break;  // K
    }
}

void cApp::mouseDown( MouseEvent event ){

    float x = event.getPos().x;
    float wid = w - offsetx*2;
    float pc = lmap(x, offsetx, offsetx+wid, 1.0f, 0.0f);
    cout << pc << endl;
}

void cApp::mouseDrag( MouseEvent event ){
}

void cApp::resize(){
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 ) ) )
