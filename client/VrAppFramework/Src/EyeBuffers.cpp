/************************************************************************************

Filename    :   EyeBuffers.cpp
Content     :   Handling of different eye buffer formats
Created     :   March 8, 2014
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "EyeBuffers.h"

#include <math.h>

#include "Framebuffer.h"
#include "SystemClock.h"

namespace OVR
{

ovrEyeBuffers::ovrEyeBuffers() :
	LogEyeSceneGpuTime(),
	DiscardInsteadOfClear( true ),
	Framebuffers(),
	BufferCount( 2 )
{
}

ovrEyeBuffers::~ovrEyeBuffers()
{
	for ( int eye = 0; eye < BufferCount; eye++ )
	{
		delete Framebuffers[eye];
	}
}

void ovrEyeBuffers::Initialize( const ovrEyeBufferParms & appBufferParms, const bool useMultiview_ )
{
	BufferCount = ( useMultiview_ ) ? 1 : 2;

	const ovrEyeBufferParms bufferParms_ = appBufferParms;

	// Update the buffers if parameters have changed
	if ( Framebuffers[0] != NULL &&
			BufferParms.resolutionWidth == bufferParms_.resolutionWidth &&
			BufferParms.resolutionHeight == bufferParms_.resolutionHeight &&
			BufferParms.multisamples == bufferParms_.multisamples &&
			BufferParms.colorFormat == bufferParms_.colorFormat &&
			BufferParms.depthFormat == bufferParms_.depthFormat )
	{
		return;
	}

	/*
	* Changes a buffer set from one (possibly uninitialized) state to a new
	* resolution / multisample / color depth state.
	*
	* Various GL binding points are modified, as well as the clear color and scissor rect.
	*
	* Note that in OpenGL 3.0 and ARB_framebuffer_object, frame buffer objects are NOT
	* shared across context share groups, so this work must be done in the rendering thread,
	* not the timewarp thread.  The texture that is bound to the FBO is properly shared.
	*
	* TODO: fall back to simpler cases on failure and mark a flag in bufferData?
	*/
	OVR_LOG( "Reallocating buffers" );

	OVR_LOG( "Allocate FBO: res=%ix%i color=%i depth=%i ms=%i",
			bufferParms_.resolutionWidth, bufferParms_.resolutionHeight, 
			bufferParms_.colorFormat, bufferParms_.depthFormat, bufferParms_.multisamples );

	GL_CheckErrors( "Before framebuffer creation" );

	int64_t colorFormat = GL_RGBA8;
	switch ( bufferParms_.colorFormat )
	{
		case COLOR_565: 		colorFormat = GL_RGB565; break;
		case COLOR_5551:		colorFormat = GL_RGB5_A1; break;
		case COLOR_4444:		colorFormat = GL_RGBA4; break;
		case COLOR_8888:		colorFormat = GL_RGBA8; break;
		case COLOR_8888_sRGB:	colorFormat = GL_SRGB8_ALPHA8; break;
		case COLOR_RGBA16F:		colorFormat = GL_RGBA16F; break;
		default: 				OVR_FAIL( "Unknown colorFormat %i", bufferParms_.colorFormat );
	}

	// GL_DEPTH_COMPONENT16 is the only strictly legal thing in unextended GL ES 2.0
	// The GL_OES_depth24 extension allows GL_DEPTH_COMPONENT24_OES.
	// The GL_OES_packed_depth_stencil extension allows GL_DEPTH24_STENCIL8_OES.
	int64_t depthFormat = GL_DEPTH_COMPONENT24;
	switch ( bufferParms_.depthFormat )
	{
		case DEPTH_16:				depthFormat = GL_DEPTH_COMPONENT16; break;
		case DEPTH_24:				depthFormat = GL_DEPTH_COMPONENT24; break;
		case DEPTH_24_STENCIL_8:	depthFormat = GL_DEPTH24_STENCIL8; break;
		default: 					OVR_FAIL( "Unknown depthFormat %i", bufferParms_.depthFormat );
	}

	for ( int eye = 0; eye < BufferCount; eye++ )
	{
		delete Framebuffers[eye];
		Framebuffers[eye] = new ovrFramebuffer( colorFormat, depthFormat,
												bufferParms_.resolutionWidth, bufferParms_.resolutionHeight,
												bufferParms_.multisamples, bufferParms_.resolveDepth,
												useMultiview_ );
	}

	GL_CheckErrors( "after framebuffer creation" );

	// Save the current buffer parms
	BufferParms = bufferParms_;
}

void ovrEyeBuffers::BeginFrame()
{
	if ( LogEyeSceneGpuTime.IsEnabled() )
	{
		static double lastReportTime = 0;
		const double timeNow = floor( SystemClock::GetTimeInSeconds() );
		if ( timeNow > lastReportTime )
		{
			OVR_LOG( "Eyes GPU time: %3.1f ms", LogEyeSceneGpuTime.GetTotalTime() );
			lastReportTime = timeNow;
		}
	}
}

void ovrEyeBuffers::EndFrame()
{
	for ( int eye = 0; eye < BufferCount; eye++ )
	{
		Framebuffers[eye]->Advance();
	}
}

void ovrEyeBuffers::BeginRenderingEye( const int eyeNum )
{
	if ( eyeNum >= BufferCount )
	{
		return;
	}

	LogEyeSceneGpuTime.Begin( eyeNum );

	Framebuffers[eyeNum]->Bind();

	glViewport( 0, 0, BufferParms.resolutionWidth, BufferParms.resolutionHeight );
	glScissor( 0, 0, BufferParms.resolutionWidth, BufferParms.resolutionHeight );
	glDepthMask( GL_TRUE );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );

#if !defined( OVR_OS_ANDROID )
	// On Android, we do this during timewarp rendering.
	if ( BufferParms.colorFormat == COLOR_8888_sRGB )
	{
		glEnable( GL_FRAMEBUFFER_SRGB_EXT );
	}
#endif

	if ( DiscardInsteadOfClear )
	{
		GL_InvalidateFramebuffer( INV_FBO, true, true );
		glClear( GL_DEPTH_BUFFER_BIT );
	}
	else
	{
		glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}
}

void ovrEyeBuffers::EndRenderingEye( const int eyeNum )
{
	if ( eyeNum >= BufferCount )
	{
		return;
	}

	Framebuffers[eyeNum]->Resolve();

#if !defined( OVR_OS_ANDROID )
	// On Android, we do this during timewarp rendering.
	if ( BufferParms.colorFormat == COLOR_8888_sRGB )
	{
		glDisable( GL_FRAMEBUFFER_SRGB_EXT );
	}
#endif

	LogEyeSceneGpuTime.End( eyeNum );

	// Left to themselves, tiled GPU drivers will avoid starting rendering
	// until they are absolutely forced to by a swap or read of a buffer,
	// because poorly implemented applications may switch away from an
	// FBO (say, to render a shadow buffer), then switch back to it and
	// continue rendering, which would cause a wasted resolve and unresolve
	// from real memory if drawing started automatically on leaving an FBO.
	//
	// We are not going to do any such thing, and we want the drawing of the
	// first eye to overlap with the command generation for the second eye
	// if the GPU has idled, so explicitly flush the pipeline.
	//
	// Adreno and Mali Do The Right Thing on glFlush, but PVR is
	// notorious for ignoring flushes to optimize naive apps.  The preferred
	// solution there is to use EGL_SYNC_FLUSH_COMMANDS_BIT_KHR on a
	// KHR sync object, but several versions of the Adreno driver have
	// had broken implementations of this that performed a full finish
	// instead of just a flush.
	glFlush();
}

ovrFrameTextureSwapChains ovrEyeBuffers::GetCurrentFrameTextureSwapChains()
{
	ovrFrameTextureSwapChains eyes = {};

	for ( int eye = 0; eye < 2; eye++ )
	{
		eyes.ColorTextureSwapChain[eye] = Framebuffers[( BufferCount == 1 ) ? 0 : eye]->GetColorTextureSwapChain();
	}
	eyes.TextureSwapChainIndex = Framebuffers[0]->GetTextureSwapChainIndex();

	return eyes;
}

} // namespace OVR
