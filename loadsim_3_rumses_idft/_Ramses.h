#pragma once

#include <iostream>
#include <fstream>
#include "mtUtil.h"

using namespace ci;
using namespace ci::app;

class Ramses{
    
public:
    
    Ramses( int _simType, int _prmType){
        eSimType = _simType;
        ePrm = _prmType;
        
        bShow = false;
        bAutoMinMax = false;
        extrude = 200.0f;
        scale = 100;
        in_min = -9.0;
        in_max = 3.0;
        visible_thresh = 0.1f;
        eStretch = 1;
        xoffset = yoffset = zoffset = 0;
        loadPlotData();
    }

    
    void loadPlotData(){
        if( pR.size() != 0) return;
            
        string simu_name = simType[eSimType];
        
        // load .r, .thata
        vector<string> exts = { ".r", ".theta"};
        for( auto & ext : exts ){            
            string path = (simRootDir/"plot"/(simu_name + ext)).string();
            cout << "start loading " << ext << " file : " << path << "...";
            std::ifstream is( path, std::ios::binary );
            if(is){ cout << " done" << endl;
            }else{ cout << " ERROR" << endl;}
            
            is.seekg (0, is.end);
            int fileSize = is.tellg();
            is.seekg (0, is.beg);
            int arraySize = arraySize = fileSize / sizeof(double);
            
            vector<double> & vec = (ext == ".r")? pR : pTheta;
            vec.clear();
            vec.assign(arraySize, double(0));
            is.read(reinterpret_cast<char*>(&vec[0]), fileSize);
            is.close();
            
            printf( "arraySize %d, %e~%e\n\n", (int)vec.size(), vec[0], vec[vec.size()-1]);
        }
        
        boxelx = pR.size();
        boxely = pTheta.size();
    }
    
    void loadSimData( int frame ){
        
        if( frame>endFrame[eSimType]){
            return;
        }
        
        string fileName = (simType[eSimType] + "_polar_" +stretch[eStretch]+ "_" + prm[ePrm] + "_00" + to_string(frame) + ".bin");
        fs::path dir = simRootDir/simType[eSimType]/stretch[eStretch]/prm[ePrm];
        fs::path path = dir/fileName;
        
        std::ifstream is( path.string(), std::ios::binary );
        if(is){
            // cout << " done" << endl;
        }else{
            cout << " ERROR" << endl;
            return;
        }
        
        // get length of file:
        is.seekg (0, is.end);
        int fileSize = is.tellg();
        is.seekg (0, is.beg);
        
        arraySize = arraySize = fileSize / sizeof(double);

        //data.clear();
        if(data.size()==0)
            data.assign(arraySize, double(0));
        is.read(reinterpret_cast<char*>(&data[0]), fileSize);
        is.close();
    }
    
    void updateVbo(){
        if(data.size()==0)return;
        
        if( bAutoMinMax ){
            in_min = numeric_limits<double>::max();
            in_max = numeric_limits<double>::min();
            
            for( auto d : data ){
                in_min = min( in_min, d );
                in_max = max( in_max, d );
            }
        }
        
        vector<vec3> points;
        vector<ColorAf> colors;
        
        for( int j=0; j<boxely; j++ ){
            for( int i=0; i<boxelx; i++ ){
                
                int index = j + i*boxely;
                
                double rho_raw = data[index];
                float rho_map = lmap(rho_raw, in_min, in_max, 0.0, 1.0);
                
                //if( visible_thresh<rho_map && rho_map <1.00 ){
                if( 0.0<rho_map && rho_map <=1.00 ){
                    float hue = lmap( rho_map, visible_thresh, 1.0f, 0.83f, 0.0f);
                    
                    bool polar = true;
                    if( polar ){
                        double r = pR[i];
                        double theta = pTheta[j];
                        double x = r * cos( theta );
                        double y = r * sin( theta );

                        //rho_map -= in_min;
                        points.push_back( vec3( x*scale+xoffset, y*scale+yoffset, rho_map*extrude + zoffset) * globalScale );
                    }else{
                        points.push_back( vec3(i, j, 0) * scale);
                    }
                    
                    ColorAf color(CM_HSV, hue, 0.8f, 0.7f);
                    colors.push_back( color );
                }
            }
        }
        
        gl::VboMesh::Layout play, clay;
        play.usage(GL_STATIC_DRAW).attrib(geom::POSITION,3);
        clay.usage(GL_STATIC_DRAW).attrib(geom::COLOR,4);
        
        vbo.reset();
        vbo = gl::VboMesh::create( points.size(), GL_POINTS, {play, clay} );
        {
            auto itr = vbo->mapAttrib3f( geom::POSITION );
            for( int i=0; i<points.size(); i++ ){ *itr++ = points[i]; }
            itr.unmap();
        }
        {
            auto itr = vbo->mapAttrib4f( geom::COLOR);
            for( int i=0; i<colors.size(); i++ ){ *itr++ = colors[i];}
            itr.unmap();
        }
        
        nParticle = vbo->getNumVertices();
        visible_rate = nParticle/(float)arraySize*100.0f;
       
    }
    
    
    void draw(){
        if(vbo && bShow ){
            gl::draw(vbo);
        }
    }

    vector<double> data;
    
    bool bShow;
    bool bAutoMinMax;
    float extrude;

    float xoffset;
    float yoffset;
    float zoffset;
    float scale;
    double in_min;
    double in_max;
    int nParticle;
    int eSimType;
    int ePrm;
    int eStretch;
    float visible_thresh;
    float visible_rate;
    int arraySize;
    
    gl::VboMeshRef vbo;
    
    const fs::path simRootDir = mt::getAssetPath()/fs::path("sim/supernova");
    static vector<string> simType;
    static vector<string> stretch;
    static vector<string> prm;
    static vector<int> endFrame;

    static vector<double> pR, pTheta;
    static int boxelx, boxely;
    static float globalScale;
};


vector<string>  Ramses::simType = {"simu_1", "simu_2", "simu_3","simu_4","simu_5" };
vector<string>  Ramses::stretch = { "linear", "log" };
vector<string>  Ramses::prm = {"rho", "vx", "vy", "S", "P", "c"};
vector<double>  Ramses::pR;
vector<double>  Ramses::pTheta;
vector<int>     Ramses::endFrame = { 650, 350, 452, 514, 750 };
int             Ramses::boxelx = -123;
int             Ramses::boxely = -123;
float           Ramses::globalScale = 1.0f;

