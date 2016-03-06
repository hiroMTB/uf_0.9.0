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
        bPolar = true;
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

        // delete hole
        if(0){
            double Rmin = pR[0];
            double Rmax = pR[pR.size()-1];
            for( auto& p : pR ){
                p = lmap(p, Rmin, Rmax, 0.0, 1.0 );
            }
        }
        
        boxelx = pR.size();
        boxely = pTheta.size();
    }
    
    bool loadSimData( int frame ){
        
        if( frame>endFrame[eSimType]){
            return false;
        }
        
        string fileName = (simType[eSimType] + "_polar_" +stretch[eStretch]+ "_" + prm[ePrm] + "_00" + to_string(frame) + ".bin");
        fs::path dir = simRootDir/simType[eSimType]/stretch[eStretch]/prm[ePrm];
        fs::path path = dir/fileName;
        
        std::ifstream is( path.string(), std::ios::binary );
        if(is){
            // cout << " done" << endl;
        }else{
            cout << " ERROR" << endl;
            return false;
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
        return true;
    }
    
    void updateVbo(vec3 eye){
        if(data.size()==0)return;
        
        if( bAutoMinMax ){
            in_min = numeric_limits<double>::max();
            in_max = numeric_limits<double>::min();
            
            for( auto d : data ){
                in_min = min( in_min, d );
                in_max = max( in_max, d );
            }
        }
        
        pos.clear();
        col.clear();
        
        for( int j=0; j<boxely; j+=1 ){
            for( int i=0; i<boxelx; i+=1 ){
                
                int index = j + i*boxely;
                
                double rho_raw = data[index];
                float rho_map = lmap(rho_raw, in_min, in_max, 0.0, 1.0);
                
                //if( visible_thresh<rho_map && rho_map <1.00 ){
                if( 0.0<rho_map && rho_map <=1.00 ){
                    float hue = lmap( rho_map, visible_thresh, 1.0f, 0.83f, 0.0f);
                    if( bPolar ){
                        double r = pR[i];
                        double theta = pTheta[j];
                        double x = r * cos( theta );
                        double y = r * sin( theta );

                        rho_map -= visible_thresh;
                        vec3 v = vec3( x, y, rho_map );
                        pos.push_back( v );
                    }else{
                        pos.push_back( vec3(i, j, 0) * scale);
                    }
                    
                    //ColorAf color(CM_HSV, hue, 0.8f, 0.7f);
                    //ColorAf color(0.8f, 0.8f, 0.8f, 0.7f);
                    //ColorAf color(hue, hue, hue, hue);
                    float w = 0.1+rho_map;
                    ColorAf color(w,w,w,w);
                    col.push_back( color );
                }
            }
        }
        
        std::sort( pos.begin(), pos.end(),
                  [&eye](const vec3 &l, const vec3 &r){ return glm::distance(eye, l) > glm::distance(eye, r); }
                  );
        
        
        gl::VboMesh::Layout play, clay;
        play.usage(GL_STATIC_DRAW).attrib(geom::POSITION,3);
        clay.usage(GL_STATIC_DRAW).attrib(geom::COLOR,4);
        
        vbo.reset();
        vbo = gl::VboMesh::create( pos.size(), GL_POINTS, {play, clay} );
        {
            auto itr = vbo->mapAttrib3f( geom::POSITION );
            for( int i=0; i<pos.size(); i++ ){
                
                vec3 p = pos[i];
                p.x = p.x*scale+xoffset;
                p.y = p.y*scale+yoffset;
                p.z = p.z*extrude + zoffset;
                p *= globalScale;
                *itr++ = p;
            }
            itr.unmap();
        }
        {
            auto itr = vbo->mapAttrib4f( geom::COLOR);
            for( int i=0; i<col.size(); i++ ){ *itr++ = col[i];}
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
    vector<vec3> pos;
    vector<ColorAf> col;
    
    
    bool bShow;
    bool bAutoMinMax;
    bool bPolar;
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
    int boxelx, boxely;
    
    gl::VboMeshRef vbo;
    
    const fs::path simRootDir = mt::getAssetPath()/fs::path("sim/supernova");
    static vector<string> simType;
    static vector<string> stretch;
    static vector<string> prm;
    static vector<int> endFrame;

    vector<double> pR, pTheta;
    static float globalScale;
};


vector<string>  Ramses::simType = {"simu_1", "simu_2", "simu_3","simu_4","simu_5" };
vector<string>  Ramses::stretch = { "linear", "log" };
vector<string>  Ramses::prm = {"rho", "vx", "vy", "S", "P", "c"};
vector<int>     Ramses::endFrame = { 650, 350, 452, 514, 750 };
float           Ramses::globalScale = 1.0f;
