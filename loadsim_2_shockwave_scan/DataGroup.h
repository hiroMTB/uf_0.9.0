#pragma once

#include "cinder/app/App.h"
#include "cinder/Rand.h"
#include "cinder/Utilities.h"
#include "cinder/gl/Vbo.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class DataGroup{
    
public:
    
    void createDot( const vector<vec3> &v, const vector<ColorAf> &c, float aThreshold ){
        
        posRef = make_shared<vector<vec3>>(v);
        colRef = make_shared<vector<ColorAf>>(c);

        //posRef = std::move(v);
        //colRef = std::make(c);

        mThreshold = aThreshold;
        
        gl::VboMesh::Layout play, clay;
        play.usage(GL_STATIC_DRAW).attrib(geom::POSITION,3);
        clay.usage(GL_STATIC_DRAW).attrib(geom::COLOR, 4);

        mDot.reset();
        mDot = gl::VboMesh::create( v.size(), GL_POINTS, {play, clay} );

        auto itrp = mDot->mapAttrib3f(geom::POSITION);
        auto itrc = mDot->mapAttrib4f(geom::COLOR);
        
        for(int i=0; i<mDot->getNumVertices(); i++){
            *itrp = v[i];
            *itrc = c[i];
            ++itrp;
            ++itrc;
        }
        itrp.unmap();
        itrc.unmap();
        
        char m[255];
        sprintf(m, "create vbo DOT    threshold %0.4f  %10zu verts", mThreshold, mDot->getNumVertices() );
        cout << m << endl;
    }
    

    void createLine( const vector<vec3> &v, const vector<ColorAf> &c ){

        vector<vec3> sv;
        vector<ColorAf> sc;
        
        for( int i=0; i<v.size(); i++ ){
            
            int id1 = i;
            int id2 = randInt(0, v.size() );
            
            vec3 v1 = v[id1];
            vec3 v2 = v[id2];

            if( distance(v2,v2) < 60 ){
                sv.push_back( v1 );
                sv.push_back( v2 );
                sc.push_back( c[id1] );
                sc.push_back( c[id2] );

            
                for( int j=0; j<3; j++ ){
                    sv.push_back( v1 + vec3(randFloat(), randFloat(), randFloat())*2.0f );
                    sv.push_back( v2 + vec3(randFloat(), randFloat(), randFloat())*2.0f );
                    sc.push_back( c[id1] );
                    sc.push_back( c[id2] );
                }
            }
        }

        gl::VboMesh::Layout play, clay;
        play.usage(GL_STATIC_DRAW).attrib(geom::POSITION,3);
        clay.usage(GL_STATIC_DRAW).attrib(geom::COLOR, 4);
        
        mLine.reset();
        mLine = gl::VboMesh::create( sv.size(), GL_LINES, {play, clay} );
        auto itrp = mLine->mapAttrib3f(geom::POSITION);
        auto itrc = mLine->mapAttrib4f(geom::COLOR);
        
        for( int i=0; i<sv.size(); i++ ){
            *itrp = sv[i];
            *itrc = sc[i];
            ++itrp;
            ++itrc;
        }
        itrp.unmap();
        itrc.unmap();
        
        char m[255];
        sprintf(m, "create vbo Line   threshold %0.4f  %10lu lines", mThreshold, mLine->getNumVertices()/2 );
        cout << m << endl;

    }
    
    
    void draw_imediate(){
        gl::begin( GL_POINTS );
        for(int i=0; i<posRef->size(); i++){
            ColorAf & c = colRef->at(i);
            vec3 & v = posRef->at(i);
            gl::color( c );
            gl::vertex( v );
        }
        gl::end();
    }
    
    void clear(){
        mDot.reset();
        mLine.reset();
    }
    
    gl::VboMeshRef mDot;
    gl::VboMeshRef mLine;
    std::shared_ptr<vector<vec3>> posRef;
    std::shared_ptr<vector<ColorAf>> colRef;
    
    float       mThreshold;
    
};