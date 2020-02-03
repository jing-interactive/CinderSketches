#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "CinderARKit.h"
#include "Config.hpp"

#include "cinder/gl/Fbo.h"
#include "cinder/GeomIo.h"
#include "cinder/Perlin.h"
#include "cinder/Rand.h"
#include "cinder/Camera.h"
#include "cmath"


using namespace ci;
using namespace ci::app;
using namespace std;

const int    FBO_WIDTH = 2048;
const int    FBO_HEIGHT = 2048;

class BlackHoleARApp : public App {
  public:
	void setup() override;
	void touchesBegan( TouchEvent event ) override;
	void update() override;
	void draw() override;
    
    ARKit::Session mARSession;
    
private:
    void updateShadowMap();
    void updateEnvMap();

    gl::GlslProgRef mRenderProg;
    gl::GlslProgRef mUpdateProg;
    gl::GlslProgRef mParticleProg;
    gl::GlslProgRef mEnvProg;
    gl::GlslProgRef mFloorProg;
    
    // Descriptions of particle data layout.
    gl::VaoRef          mAttributes[2];
    // Buffers holding raw particle data on GPU.
    gl::VboRef          mParticleBuffer[2];
    
    std::uint32_t       mSourceIndex        = 0;
    std::uint32_t       mDestinationIndex   = 1;
    
    gl::FboRef              mFbo;
    gl::FboRef              mFboParticle;
    gl::FboRef              mFboEnv;
    gl::Texture2dRef        mShadowMapTex;
    gl::TextureRef          mEnvTex;
    
    CameraPersp             mLightCam;
    CameraOrtho             mCamParticle;
    
    vec3                    mLightPos;
    gl::BatchRef            mSphere;
    gl::BatchRef            mEnv;
    gl::BatchRef            mFloor;
    Perlin                  mPerlin;
    
    mat4                    mMtxScale;
    mat4                    mMtxTouch;
    mat4                    mMtxIdentity;
    mat4                    mMtxModel;
    ARKit::AnchorID         aID;
    

    float mSeed = randFloat(1000.0f);
    
    float offset = 0.0f;
    float targetOffset = 0.0f;
};

struct Particle
{
    vec3    pos;
    vec3    vel;
    vec3    posOrg;
    vec3    random;
    float   life;
};


void prepareSettings( BlackHoleARApp::Settings *settings) {
    settings->setHighDensityDisplayEnabled(); // try removing this line
}

void BlackHoleARApp::setup()
{
    float s = 0.1f;
    mMtxScale = glm::scale(glm::mat4(1.0f), glm::vec3(s));
    
    gl::enableDepthRead();
    gl::enableDepthWrite();
    
    auto config = ARKit::SessionConfiguration()
                        .trackingType( ARKit::TrackingType::WorldTracking )
                        .planeDetection( ARKit::PlaneDetection::Horizontal );
    
    mARSession.runConfiguration( config );
    
    console() << "Number of particles : " << Config::getInstance().NUM_PARTICLES << endl;
    // init particles
    vector<Particle> particles;
    particles.assign( Config::getInstance().NUM_PARTICLES, Particle() );
    float zRange = 0.1f;
    
    
    for( int i =0; i<particles.size(); i++) {
        float a = randFloat() * M_PI * 2.0;
        float r = 3.0;
        float _x = cos(a) * r;
        float _y = sin(a) * r;
        float z = randFloat(-zRange, zRange);
        
        float s = mPerlin.fBm(_x, _y, z) * 0.2;
        r = randFloat(2.0, 2.5);
        float x = cos(a) * r * ( 1.0 + s);
        float y = sin(a) * r * ( 1.0 + s);
        
        auto &p = particles.at( i );
        
        p.pos = vec3(x, y, z);
        p.posOrg = vec3(x, y, z);
        p.life = Rand::randFloat(0.01f, 1.0f);
        p.random = vec3(randFloat(), randFloat(), randFloat());
    }
    
    
    mParticleBuffer[mSourceIndex]       = gl::Vbo::create( GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), particles.data(), GL_STATIC_DRAW );
    mParticleBuffer[mDestinationIndex]  = gl::Vbo::create( GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), nullptr, GL_STATIC_DRAW );
    
    for( int i = 0; i < 2; ++i )
    {    // Describe the particle layout for OpenGL.
        mAttributes[i] = gl::Vao::create();
        gl::ScopedVao vao( mAttributes[i] );
        
        // Define attributes as offsets into the bound particle buffer
        gl::ScopedBuffer buffer( mParticleBuffer[i] );
        gl::enableVertexAttribArray( 0 );
        gl::enableVertexAttribArray( 1 );
        gl::enableVertexAttribArray( 2 );
        gl::enableVertexAttribArray( 3 );
        gl::enableVertexAttribArray( 4 );
        gl::vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, pos) );
        gl::vertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, vel) );
        gl::vertexAttribPointer( 2, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, posOrg) );
        gl::vertexAttribPointer( 3, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, random) );
        gl::vertexAttribPointer( 4, 1, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, life) );
        
    }
    
    
    mRenderProg = gl::GlslProg::create( gl::GlslProg::Format().vertex( loadAsset( "renderES3.vert" ) ).fragment( loadAsset("renderES3.frag"))
       .attribLocation( "ciPosition", 0 )
       .attribLocation( "iPositionOrg", 2 )
       .attribLocation( "iRandom", 3 )
       .attribLocation( "iLife", 4 )
    );
    
    
    mUpdateProg = gl::GlslProg::create( gl::GlslProg::Format().vertex( loadAsset( "updateES3.vert" ) ).fragment( loadAsset( "no_op_es3.frag" ) )
        .feedbackFormat( GL_INTERLEAVED_ATTRIBS )
        .feedbackVaryings( { "position", "velocity", "positionOrg", "random", "life"} )
        .attribLocation( "iPosition", 0 )
        .attribLocation( "iVelocity", 1 )
        .attribLocation( "iPositionOrg", 2 )
        .attribLocation( "iRandom", 3 )
        .attribLocation( "iLife", 4 )
        );
    
    // shadow mapping
    float scale = 0.035f;
    mLightPos = vec3( 0.0f * scale, 10.0f * scale, 4.0f * scale);
    gl::Texture2d::Format depthFormat;
    depthFormat.setInternalFormat( GL_DEPTH_COMPONENT32F );
    depthFormat.setCompareMode( GL_COMPARE_REF_TO_TEXTURE );
    depthFormat.setMagFilter( GL_LINEAR );
    depthFormat.setMinFilter( GL_LINEAR );
    depthFormat.setWrap( GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE );
    depthFormat.setCompareFunc( GL_LEQUAL );
    mShadowMapTex = gl::Texture2d::create( FBO_WIDTH, FBO_HEIGHT, depthFormat );
    
    gl::Fbo::Format fboFormat;
    fboFormat.attachment( GL_DEPTH_ATTACHMENT, mShadowMapTex );
    mFbo = gl::Fbo::create( FBO_WIDTH, FBO_HEIGHT, fboFormat );
    
    gl::Fbo::Format fboFormatParticle;
    mFboParticle = gl::Fbo::create( 64, 64, fboFormatParticle.colorTexture() );
    
    gl::Fbo::Format fboFormatEnv;
    mFboEnv = gl::Fbo::create( FBO_WIDTH, FBO_HEIGHT, fboFormatEnv.colorTexture() );
    
    // Set up camera from the light's viewpoint
    mLightCam.setPerspective( 100.0f, mFbo->getAspectRatio(), 0.1f, 10.0f );
    mLightCam.lookAt( mLightPos, vec3( 0.0f ) );
    
    // floor
    mFloorProg = gl::GlslProg::create( gl::GlslProg::Format().vertex( loadAsset( "floor.vert" ) ).fragment( loadAsset("floor.frag")));
    auto plane = gl::VboMesh::create( geom::Plane().size(vec2(10.0f)));
    mFloor = gl::Batch::create(plane, mFloorProg);
    
    //  particle texture
    mParticleProg = gl::GlslProg::create( gl::GlslProg::Format().vertex( loadAsset( "particle.vert" ) ).fragment( loadAsset("particle.frag")));
    auto sphere = gl::VboMesh::create( geom::Icosphere() );
    mSphere = gl::Batch::create( sphere, mParticleProg );
    
    float r = 1.0f;
    mCamParticle.setOrtho(-r, r, -r, r, .1f, 10.0f);
    mCamParticle.lookAt(vec3(0.0, 0.0, 5.0f), vec3(0.0, 0.0, 0.0));
    
    gl::ScopedFramebuffer fbo( mFboParticle );
    gl::ScopedViewport viewport( vec2( 0.0f ), mFboParticle->getSize() );
    
    gl::clear( Color( 0, 0, 0 ) );
    gl::setMatrices( mCamParticle );
    gl::enableDepthRead();
    gl::enableDepthWrite();

    gl::ScopedGlslProg prog( mParticleProg );
    mParticleProg->uniform("uLightPos", mLightPos);
    
    mSphere->draw();
}

void BlackHoleARApp::touchesBegan( TouchEvent event )
{
    aID = mARSession.addAnchorRelativeToCamera( vec3(0.0f, 0.0f, -2.0f) );
    console() << aID << endl;
    
    targetOffset += 1.0;
    
    // next step : reset
    if(targetOffset > 2.0) {
        auto anchors = mARSession.getAnchors();
        console() << anchors.size() << endl;
        while( anchors.size() > 1) {
            anchors.erase(anchors.begin());
            console() << anchors.size() << endl;
        }
        offset = 0.0f;
        targetOffset = 1.0;
    }
    
}

void BlackHoleARApp::update()
{
    
    if(targetOffset > 0.5) {
        offset += 0.005f;
        if(offset > 1.0) {
            offset = 1.0;
        }
    }
    // Update particles on the GPU
    gl::ScopedGlslProg prog( mUpdateProg );
    gl::ScopedState rasterizer( GL_RASTERIZER_DISCARD, true );    // turn off fragment stage

    // mUpdateProg->uniform("uCenter", getWindowCenter());
    mUpdateProg->uniform("uTime", float(getElapsedSeconds()) + mSeed);
    mUpdateProg->uniform("uOffset", offset);
    float t =(targetOffset > 1.0f) ? 1.0f : 0.0f;
    mUpdateProg->uniform("uIsClosing", t);
    

    // Bind the source data (Attributes refer to specific buffers).
    gl::ScopedVao source( mAttributes[mSourceIndex] );
    // Bind destination as buffer base.
    gl::bindBufferBase( GL_TRANSFORM_FEEDBACK_BUFFER, 0, mParticleBuffer[mDestinationIndex] );
    gl::beginTransformFeedback( GL_POINTS );

    // Draw source into destination, performing our vertex transformations.
    gl::drawArrays( GL_POINTS, 0, Config::getInstance().NUM_PARTICLES );

    gl::endTransformFeedback();

    // Swap source and destination for next loop
    std::swap( mSourceIndex, mDestinationIndex );
}

void BlackHoleARApp::updateShadowMap() {
    gl::ScopedFramebuffer fbo( mFbo );
    gl::ScopedViewport viewport( vec2( 0.0f ), mFbo->getSize() );
    
    gl::clear( Color( 0, 0, 0 ) );
    gl::ScopedMatrices matScp;
    
    gl::setMatrices( mLightCam );
    gl::enableDepthRead();
    gl::enableDepthWrite();
    gl::setModelMatrix(mMtxScale);

    gl::ScopedGlslProg prog( mRenderProg );
    
    mRenderProg->uniform("uViewport", vec2(getWindowSize()));
    mRenderProg->uniform("uTranslateMatrix", mMtxIdentity);
    mRenderProg->uniform("uTouchMatrix", mMtxTouch);
    
    gl::ScopedVao vao( mAttributes[mSourceIndex] );
    gl::context()->setDefaultShaderVars();
    gl::drawArrays( GL_POINTS, 0, Config::getInstance().NUM_PARTICLES );
}


void BlackHoleARApp::updateEnvMap() {
    gl::ScopedFramebuffer fbo( mFboEnv );
    gl::ScopedViewport viewport( vec2( 0.0f ), mFboEnv->getSize() );
    gl::clear( Color( 0, 0, 0 ) );
    
    gl::ScopedMatrices matScp;
    
    mARSession.drawRGBCaptureTexture(getWindowBounds());
}

void BlackHoleARApp::draw()
{
    if(mARSession.getAnchors().size() <= 0) {
        mMtxTouch = mARSession.getProjectionMatrix() * mARSession.getViewMatrix();
        updateEnvMap();
    } else {
        updateShadowMap();
    }
    
	gl::clear( Color( 0, 0, 0 ) );
    gl::ScopedMatrices matScp;
    
    gl::disableDepthRead();
    gl::disableDepthWrite();
    
    
    
    if(mARSession.getAnchors().size() <= 0) {
        gl::setMatricesWindow( toPixels( getWindowSize() ) );
        gl::draw( mFboEnv->getColorTexture(), Rectf( 0, 0, getWindowWidth() * 2, getWindowHeight() * 2 ) );
    } else {
        mARSession.drawRGBCaptureTexture(getWindowBounds());
    }
    
    gl::setMatricesWindow( toPixels( getWindowSize() ) );
    
    if( mARSession.getAnchors().size() <= 0) {
        return;
    }
    
//    console() << "Number of Anchors : " <<  mARSession.getAnchors()[0] << endl;
    auto anchors = mARSession.getAnchors();
    auto anchor = anchors[0];
    
    
    
    gl::enableDepthRead();
    gl::enableDepthWrite();
    
    gl::setViewMatrix( mARSession.getViewMatrix() );
    gl::setProjectionMatrix( mARSession.getProjectionMatrix() );
    gl::setModelMatrix(mMtxScale);

    gl::ScopedGlslProg prog( mRenderProg );
    
    mat4 shadowMatrix = mLightCam.getProjectionMatrix() * mLightCam.getViewMatrix();
    mRenderProg->uniform("uViewport", vec2(getWindowSize()));
    mRenderProg->uniform("uOffset", offset);
    mRenderProg->uniform("uTranslateMatrix", anchor.mTransform);
    mRenderProg->uniform("uShadowMatrix", shadowMatrix);
    mRenderProg->uniform("uTouchMatrix", mMtxTouch);
    
    gl::ScopedTextureBind texScope( mShadowMapTex, (uint8_t) 0 );
    mRenderProg->uniform( "uShadowMap", 0 );
    
    gl::ScopedTextureBind texScopeParticle( mFboParticle->getColorTexture(), (uint8_t) 1 );
    mRenderProg->uniform( "uParticleMap", 1 );
    
    gl::ScopedTextureBind texScopeEnv( mFboEnv->getColorTexture(), (uint8_t) 2 );
    mRenderProg->uniform( "uEnvMap", 2 );
    
    gl::ScopedVao vao( mAttributes[mSourceIndex] );
    gl::context()->setDefaultShaderVars();
    gl::drawArrays( GL_POINTS, 0, Config::getInstance().NUM_PARTICLES );
    
    
    gl::setViewMatrix( mARSession.getViewMatrix() );
    gl::setProjectionMatrix( mARSession.getProjectionMatrix() );
    gl::setModelMatrix(mMtxScale);
    
        
    
//    gl::setMatricesWindow( toPixels( getWindowSize() ) );
//    int s = 128 * 2;
//    gl::draw( mFbo->getDepthTexture(), Rectf( 0, 0, s, s ) );
    
}

CINDER_APP( BlackHoleARApp, RendererGl( RendererGl::Options().msaa( 4 ) ), prepareSettings )
