/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2014 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

#include "OgreStableHeaders.h"

#include "Compositor/OgreCompositorShadowNodeDef.h"
#include "Compositor/Pass/PassScene/OgreCompositorPassSceneDef.h"

#include "OgreLight.h"

#include "OgreStringConverter.h"
#include "OgreLogManager.h"

namespace Ogre
{
    IdString CompositorShadowNodeDef::addTextureSourceName( const String &name, size_t index,
                                                            TextureSource textureSource )
    {
        if( textureSource == TEXTURE_INPUT )
        {
            OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS, "Shadow Nodes don't support input channels!"
                            " Shadow Node: '" + mNameStr + "'",
                            "OgreCompositorShadowNodeDef::addTextureSourceName" );
        }

        return CompositorNodeDef::addTextureSourceName( name, mShadowMapTexDefinitions.size() + index,
                                                        textureSource );
    }
    //-----------------------------------------------------------------------------------
    void CompositorShadowNodeDef::addBufferInput( size_t inputChannel, IdString name )
    {
        OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS, "Shadow Nodes don't support input channels!"
                        " Shadow Node: '" + mNameStr + "'",
                        "OgreCompositorShadowNodeDef::addBufferInput" );
    }
    //-----------------------------------------------------------------------------------
    void CompositorShadowNodeDef::setNumShadowTextureDefinitions( size_t numTex )
    {
        mShadowMapTexDefinitions.reserve( numTex );
    }
    //-----------------------------------------------------------------------------------
    ShadowTextureDefinition* CompositorShadowNodeDef::addShadowTextureDefinition(
            size_t lightIdx, size_t split, const String &name, uint8 mrtIndex,
            const Vector2 &uvOffset, const Vector2 &uvLength, uint8 arrayIdx )
    {
        if( name.empty() )
        {
            OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS,
                            "Shadow maps used as atlas can't have empty names."
                            " Light index #" + StringConverter::toString( lightIdx ),
                            "CompositorShadowNodeDef::addShadowTextureDefinition" );
        }
        if( name.find( "global_" ) == 0 )
        {
            OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS,
                            "Shadow maps cannot reference global textures!"
                            " Light index #" + StringConverter::toString( lightIdx ),
                            "CompositorShadowNodeDef::addShadowTextureDefinition" );
        }

        ShadowMapTexDefVec::const_iterator itor = mShadowMapTexDefinitions.begin();
        ShadowMapTexDefVec::const_iterator end  = mShadowMapTexDefinitions.end();

        size_t newLight = 1;

        while( itor != end )
        {
            if( itor->light == lightIdx )
                newLight = 0;

            if( itor->light == lightIdx && itor->split == split )
            {
                OGRE_EXCEPT( Exception::ERR_DUPLICATE_ITEM, "There's already a texture for light index #"
                                + StringConverter::toString( lightIdx ),
                                "OgreCompositorShadowNodeDef::addShadowTextureDefinition" );
            }
            ++itor;
        }

        mNumLights += newLight;

        mShadowMapTexDefinitions.push_back( ShadowTextureDefinition( mDefaultTechnique, name, mrtIndex,
                                                                     uvOffset, uvLength, arrayIdx,
                                                                     lightIdx, split ) );

        return &mShadowMapTexDefinitions.back();
    }
    //-----------------------------------------------------------------------------------
    void CompositorShadowNodeDef::postInitializePassDef( CompositorPassDef *passDef )
    {
        //Shadow nodes usually should be unaffected by these masks.
        passDef->mExecutionMask         = 0xFF;
        passDef->mViewportModifierMask  = 0x00;
    }
    //-----------------------------------------------------------------------------------
    void CompositorShadowNodeDef::_validateAndFinish(void)
    {
        mLightTypesMask.resize( mNumLights, 0u );

        CompositorTargetDefVec::iterator itor = mTargetPasses.begin();
        CompositorTargetDefVec::iterator end  = mTargetPasses.end();

        while( itor != end )
        {
            const CompositorPassDefVec &passDefVec = itor->getCompositorPasses();
            CompositorPassDefVec::const_iterator it = passDefVec.begin();
            CompositorPassDefVec::const_iterator en = passDefVec.end();

            while( it != en )
            {
                CompositorPassDef *pass = *it;

                //All passes in shadow nodes can't include overlays.
                if( pass->mIncludeOverlays )
                {
                    LogManager::getSingleton().logMessage( "WARNING: All Passes in a Shadow Node "
                                    "can't include overlays. Turning them off. ShadowNode: '" +
                                    mName.getFriendlyText() + "'" );
                }

                pass->mIncludeOverlays = false;

                if( pass->mShadowMapIdx < mShadowMapTexDefinitions.size() )
                {
                    const ShadowTextureDefinition &texDef = mShadowMapTexDefinitions[pass->mShadowMapIdx];

                    if( itor->getRenderTargetName() == texDef.getTextureName() &&
                        !pass->mShadowMapFullViewport )
                    {
                        //Only force the viewport settings to the passes
                        //that directly rendering into the atlas
                        pass->mVpLeft   = static_cast<float>( texDef.uvOffset.x );
                        pass->mVpTop    = static_cast<float>( texDef.uvOffset.y );
                        pass->mVpWidth  = static_cast<float>( texDef.uvLength.x );
                        pass->mVpHeight = static_cast<float>( texDef.uvLength.y );

                        pass->mVpScissorLeft   = pass->mVpLeft;
                        pass->mVpScissorTop    = pass->mVpTop;
                        pass->mVpScissorWidth  = pass->mVpWidth;
                        pass->mVpScissorHeight = pass->mVpHeight;
                    }

                    if( texDef.shadowMapTechnique == SHADOWMAP_PSSM )
                    {
                        //PSSM only supports directional lights. This is for sure.
                        itor->setShadowMapSupportedLightTypes( 1u << Light::LT_DIRECTIONAL );
                    }
                    else if( itor->getShadowMapSupportedLightTypes() == 0 )
                    {
                        OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS,
                            "Pass in shadow node " + mNameStr + " is assigned to shadow "
                            "maps but says it does not support any light type. "
                            "Did you forget to call setShadowMapSupportedLightTypes?",
                            "CompositorShadowNodeDef::_validateAndFinish" );
                    }

                    //Accumulate the types of lights this shadow map supports
                    //based on the passes that claim to be compatible with it.
                    const size_t lightIdx = mShadowMapTexDefinitions[pass->mShadowMapIdx].light;
                    mLightTypesMask[lightIdx] |= itor->getShadowMapSupportedLightTypes();
                }

                if( pass->getType() == PASS_SCENE )
                {
                    //assert( dynamic_cast<CompositorPassSceneDef*>( pass ) );
                    CompositorPassSceneDef *passScene = static_cast<CompositorPassSceneDef*>( pass );
                    mMinRq = std::min<size_t>( mMinRq, passScene->mFirstRQ );
                    mMaxRq = std::max<size_t>( mMaxRq, passScene->mLastRQ );

                    //Regular nodes calculate the LOD values, we just use them.
                    if( passScene->mLodCameraName == IdString() )
                        passScene->mUpdateLodLists = false;

                    //Set to only render casters
                    passScene->mVisibilityMask |= VisibilityFlags::LAYER_SHADOW_CASTER;

                    //Nested shadow maps are not allowed. Sorry!
                    passScene->mShadowNode              = IdString();
                    passScene->mShadowNodeRecalculation = SHADOW_NODE_CASTER_PASS;
                }

                ++it;
            }

            ++itor;
        }

        //See which shadow maps can share the camera setup
        ShadowMapTexDefVec::iterator it1 = mShadowMapTexDefinitions.begin();
        ShadowMapTexDefVec::iterator en1 = mShadowMapTexDefinitions.end();

        while( it1 != en1 )
        {
            if( it1->split != 0 && it1->shadowMapTechnique != SHADOWMAP_PSSM )
            {
                OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS,
                    "Trying to use a split with non-PSSM shadow map techniques.",
                    "CompositorShadowNodeDef::_validateAndFinish");
            }

#ifndef OGRE_REMOVE_PSSM_SPLIT_LIMIT
            if( it1->numSplits > 5 )
            {
                //The risk is that, because of how Ogre deals with constant params, we have no way
                //to tell whether the shader has enough room to hold all the floats we'll sent.
                //At 5 splits, we send 4 floats (i.e. a float4). If there were 6 splits, we would
                //send 5 floats, which means the variable needs to be declared as float [5] or
                //float4 [2], float4x2 etc. If you're sure the shader has enough room, you
                //can define this macro to remove the limit.
                it1->numSplits = 5;
                LogManager::getSingleton().logMessage( "WARNING: Limiting the number of PSSM splits per "
                            "light to 5. If you wish to use more & understand the risks, recompile Ogre "
                            "defining the macro OGRE_REMOVE_PSSM_SPLIT_LIMIT." );
            }
#endif

            bool bShared = false;

            ShadowMapTexDefVec::const_iterator it2 = mShadowMapTexDefinitions.begin();
            ShadowMapTexDefVec::const_iterator en2 = it1;

            while( it2 != en2 && !bShared )
            {
                if( it2->light == it1->light )
                {
                    if( it2->split == it1->split )
                    {
                        //Do not share the setups, the user may be trying to do tricky stuff
                        //(like comparing two shadow mapping techniques on the same light)
                        LogManager::getSingleton().logMessage( "WARNING: Two shadow maps refer to the "
                            "same light & split. Ignore this if it is intentional" );
                    }
                    else if( it2->split != it1->split )
                    {
                        if( it2->numSplits != it1->numSplits )
                        {
                            LogManager::getSingleton().logMessage( "WARNING: All pssm shadow maps with "
                                    "the same light but different split must have the same number of "
                                    "splits. Attempting to fix. ShadowNode: '" +
                                    mName.getFriendlyText() + "'." );
                            it1->numSplits = it2->numSplits;
                        }

                        it1->_setSharesSetupWithIdx( it2 - mShadowMapTexDefinitions.begin() );
                        bShared = true;
                    }
                }
                else if( it2->shadowMapTechnique == it1->shadowMapTechnique )
                {
                    it1->_setSharesSetupWithIdx( it2 - mShadowMapTexDefinitions.begin() );
                    bShared = true;
                }
                ++it2;
            }

            ++it1;
        }
    }
}
