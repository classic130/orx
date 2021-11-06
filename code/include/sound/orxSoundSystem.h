/* Orx - Portable Game Engine
 *
 * Copyright (c) 2008-2021 Orx-Project
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *    1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 *    2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 *    3. This notice may not be removed or altered from any source
 *    distribution.
 */

/**
 * @file orxSoundSystem.h
 * @date 13/07/2008
 * @author iarwain@orx-project.org
 *
 * @todo
 */

/**
 * @addtogroup orxSoundSystem
 *
 * Sound system plugin module
 * Plugin module that handles sound system
 *
 * @{
 */


#ifndef _orxSOUNDSYSTEM_H_
#define _orxSOUNDSYSTEM_H_

#include "orxInclude.h"
#include "math/orxVector.h"
#include "plugin/orxPluginCore.h"


/***************************************************************************
 * Structure declaration                                                   *
 ***************************************************************************/

/** Abstract sound structures
 */
typedef struct __orxSOUNDSYSTEM_SOUND_t   orxSOUNDSYSTEM_SOUND;
typedef struct __orxSOUNDSYSTEM_SAMPLE_t  orxSOUNDSYSTEM_SAMPLE;

/** Sound system status enum
 */
typedef enum __orxSOUNDSYSTEM_STATUS_t
{
  orxSOUNDSYSTEM_STATUS_PLAY = 0,
  orxSOUNDSYSTEM_STATUS_PAUSE,
  orxSOUNDSYSTEM_STATUS_STOP,

  orxSOUNDSYSTEM_STATUS_NUMBER,

  orxSOUNDSYSTEM_STATUS_NONE = orxENUM_NONE

} orxSOUNDSYSTEM_STATUS;


/** Config defines
 */
#define orxSOUNDSYSTEM_KZ_CONFIG_SECTION              "SoundSystem"
#define orxSOUNDSYSTEM_KZ_CONFIG_RATIO                "DimensionRatio"
#define orxSOUNDSYSTEM_KZ_CONFIG_LISTENERS            "Listeners"


/***************************************************************************
 * Functions directly implemented by orx core
 ***************************************************************************/

/** Sound system module setup
 */
extern orxDLLAPI void orxFASTCALL                     orxSoundSystem_Setup();


/***************************************************************************
 * Functions extended by plugins
 ***************************************************************************/

/** Inits the sound system module
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_Init();

/** Exits from the sound system module
 */
extern orxDLLAPI void orxFASTCALL                     orxSoundSystem_Exit();

/** Creates an empty sample
 * @param[in]   _u32ChannelNumber                     Number of channels of the sample
 * @param[in]   _u32FrameNumber                       Number of frame of the sample (number of "samples" = number of frames * number of channels)
 * @param[in]   _u32SampleRate                        Sampling rate of the sample (ie. number of frames per second)
 * @return orxSOUNDSYSTEM_SAMPLE / orxNULL
 */
extern orxDLLAPI orxSOUNDSYSTEM_SAMPLE *orxFASTCALL   orxSoundSystem_CreateSample(orxU32 _u32ChannelNumber, orxU32 _u32FrameNumber, orxU32 _u32SampleRate);

/** Loads a sound sample from file (cannot be played directly)
 * @param[in]   _zFilename                            Name of the file to load as a sample (completely loaded in memory, useful for sound effects)
 * @return orxSOUNDSYSTEM_SAMPLE / orxNULL
 */
extern orxDLLAPI orxSOUNDSYSTEM_SAMPLE *orxFASTCALL   orxSoundSystem_LoadSample(const orxSTRING _zFilename);

/** Deletes a sound sample
 * @param[in]   _pstSample                            Concerned sample
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_DeleteSample(orxSOUNDSYSTEM_SAMPLE *_pstSample);

/** Gets sample info
 * @param[in]   _pstSample                            Concerned sample
 * @param[in]   _pu32ChannelNumber                    Number of channels of the sample
 * @param[in]   _pu32FrameNumber                      Number of frame of the sample (number of "samples" = number of frames * number of channels)
 * @param[in]   _pu32SampleRate                       Sampling rate of the sample (ie. number of frames per second)
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_GetSampleInfo(const orxSOUNDSYSTEM_SAMPLE *_pstSample, orxU32 *_pu32ChannelNumber, orxU32 *_pu32FrameNumber, orxU32 *_pu32SampleRate);

/** Sets sample data
 * @param[in]   _pstSample                            Concerned sample
 * @param[in]   _afData                               Data to set (samples are expected to be signed/normalized)
 * @param[in]   _u32SampleNumber                      Number of samples in the data array
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_SetSampleData(orxSOUNDSYSTEM_SAMPLE *_pstSample, const orxFLOAT *_afData, orxU32 _u32SampleNumber);

/** Creates a sound from preloaded sample (can be played directly)
 * @param[in]   _hUserData                            User data to associate with this sound
 * @param[in]   _pstSample                            Concerned sample
 * @return orxSOUNDSYSTEM_SOUND / orxNULL
 */
extern orxDLLAPI orxSOUNDSYSTEM_SOUND *orxFASTCALL    orxSoundSystem_CreateFromSample(orxHANDLE _hUserData, const orxSOUNDSYSTEM_SAMPLE *_pstSample);

/** Creates an empty stream
 * @param[in]   _hUserData                            User data to associate with this sound
 * @param[in]   _u32ChannelNumber                     Number of channels for the stream
 * @param[in]   _u32SampleRate                        Sampling rate of the stream (ie. number of frames per second)
 * @return orxSOUNDSYSTEM_SOUND / orxNULL
 */
extern orxDLLAPI orxSOUNDSYSTEM_SOUND *orxFASTCALL    orxSoundSystem_CreateStream(orxHANDLE _hUserData, orxU32 _u32ChannelNumber, orxU32 _u32SampleRate);

/** Creates a streamed sound from file (can be played directly)
 * @param[in]   _hUserData                            User data to associate with this sound
 * @param[in]   _zFilename                            Name of the file to load as a stream (won't be completely loaded in memory, useful for musics)
 * @return orxSOUNDSYSTEM_SOUND / orxNULL
 */
extern orxDLLAPI orxSOUNDSYSTEM_SOUND *orxFASTCALL    orxSoundSystem_CreateStreamFromFile(orxHANDLE _hUserData, const orxSTRING _zFilename);

/** Deletes a sound
 * @param[in]   _pstSound                             Concerned sound
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_Delete(orxSOUNDSYSTEM_SOUND *_pstSound);

/** Plays a sound
 * @param[in]   _pstSound                             Concerned sound
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_Play(orxSOUNDSYSTEM_SOUND *_pstSound);

/** Pauses a sound
 * @param[in]   _pstSound                             Concerned sound
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_Pause(orxSOUNDSYSTEM_SOUND *_pstSound);

/** Stops a sound
 * @param[in]   _pstSound                             Concerned sound
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_Stop(orxSOUNDSYSTEM_SOUND *_pstSound);

/** Starts recording
 * @param[in]   _zName                                Name for the recorded sound/file
 * @param[in]   _bWriteToFile                         Should write to file?
 * @param[in]   _u32SampleRate                        Sample rate, 0 for default rate (48000Hz)
 * @param[in]   _u32ChannelNumber                     Channel number, 0 for default mono channel
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_StartRecording(const orxCHAR *_zName, orxBOOL _bWriteToFile, orxU32 _u32SampleRate, orxU32 _u32ChannelNumber);

/** Stops recording
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_StopRecording();

/** Is recording possible on the current system?
 * @return orxTRUE / orxFALSE
 */
extern orxDLLAPI orxBOOL orxFASTCALL                  orxSoundSystem_HasRecordingSupport();

/** Sets a sound volume
 * @param[in]   _pstSound                             Concerned sound
 * @param[in]   _fVolume                              Volume to set [0, 1]
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_SetVolume(orxSOUNDSYSTEM_SOUND *_pstSound, orxFLOAT _fVolume);

/** Sets a sound pitch
 * @param[in]   _pstSound                             Concerned sound
 * @param[in]   _fPitch                               Pitch to set
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_SetPitch(orxSOUNDSYSTEM_SOUND *_pstSound, orxFLOAT _fPitch);

/** Sets a sound time (ie. cursor/play position from beginning)
 * @param[in]   _pstSound                             Concerned sound
 * @param[in]   _fTime                                Time, in seconds
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_SetTime(orxSOUNDSYSTEM_SOUND *_pstSound, orxFLOAT _fTime);

/** Sets a sound position
 * @param[in]   _pstSound                             Concerned sound
 * @param[in]   _pvPosition                           Position to set
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_SetPosition(orxSOUNDSYSTEM_SOUND *_pstSound, const orxVECTOR *_pvPosition);

/** Sets a sound spatialization, with gain decreasing between the minimum and maximum distances, when enabled
 * @param[in] _pstSound                               Concerned Sound
 * @param[in] _fMinDistance                           Min distance, inside which the max gain will be used, strictly negative value to disable spatialization entirely
 * @param[in] _fMaxDistance                           Max distance, outside which the gain will stop decreasing, strictly negative value to disable spatialization entirely
 * @param[in] _fMinGain                               Min gain in [0.0f - 1.0f]
 * @param[in] _fMaxGain                               Max gain in [0.0f - 1.0f]
 * @param[in] _fRollOff                               RollOff factor applied when interpolating the gain between inner and outer distances, defaults to 1.0f
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_SetSpatialization(orxSOUNDSYSTEM_SOUND *_pstSound, orxFLOAT _fMinDistance, orxFLOAT _fMaxDistance, orxFLOAT _fMinGain, orxFLOAT _fMaxGain, orxFLOAT _fRollOff);

/** Sets a sound panning
 * @param[in] _pstSound panning
 * @param[in] _fPanning                               Sound panning, -1.0f for full left, 0.0f for center, 1.0f for full right
 * @param[in] _bMix                                   Left/Right channels will be mixed if orxTRUE or act like a balance otherwise
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_SetPanning(orxSOUNDSYSTEM_SOUND *_pstSound, orxFLOAT _fPanning, orxBOOL _bMix);

/** Loops a sound
 * @param[in]   _pstSound                             Concerned sound
 * @param[in]   _bLoop                                Loop / no loop
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_Loop(orxSOUNDSYSTEM_SOUND *_pstSound, orxBOOL _bLoop);

/** Gets a sound volume
 * @param[in]   _pstSound                             Concerned sound
 * @return Sound's volume
 */
extern orxDLLAPI orxFLOAT orxFASTCALL                 orxSoundSystem_GetVolume(const orxSOUNDSYSTEM_SOUND *_pstSound);

/** Gets a sound pitch
 * @param[in]   _pstSound                             Concerned sound
 * @return Sound's pitch
 */
extern orxDLLAPI orxFLOAT orxFASTCALL                 orxSoundSystem_GetPitch(const orxSOUNDSYSTEM_SOUND *_pstSound);

/** Gets a sound's time (ie. cursor/play position from beginning)
 * @param[in]   _pstSound                             Concerned sound
 * @return Sound's time (cursor/play position), in seconds
 */
extern orxDLLAPI orxFLOAT orxFASTCALL                 orxSoundSystem_GetTime(const orxSOUNDSYSTEM_SOUND *_pstSound);

/** Gets a sound position
 * @param[in]   _pstSound                             Concerned sound
 * @param[out]  _pvPosition                           Position to get
 * @return Sound's position
 */
extern orxDLLAPI orxVECTOR *orxFASTCALL               orxSoundSystem_GetPosition(const orxSOUNDSYSTEM_SOUND *_pstSound, orxVECTOR *_pvPosition);

/** Gets a sound spatialization information
 * @param[in] _pstSound                               Concerned Sound
 * @param[out] _pfMinDistance                         Min distance, inside which the max gain will be used, will be strictly negative if the sound isn't spatialized
 * @param[out] _pfMaxDistance                         Max distance, outside which the gain will stop decreasing, will be strictly negative if the sound isn't spatialized
 * @param[out] _pfMinGain                             Min gain in [0.0f - 1.0f]
 * @param[out] _pfMaxGain                             Max gain in [0.0f - 1.0f]
 * @param[out] _pfRollOff                             RollOff factor applied when interpolating the gain between inner and outer distances, defaults to 1.0f
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_GetSpatialization(const orxSOUNDSYSTEM_SOUND *_pstSound, orxFLOAT *_pfMinDistance, orxFLOAT *_pfMaxDistance, orxFLOAT *_pfMinGain, orxFLOAT *_pfMaxGain, orxFLOAT *_pfRollOff);

/** Gets a sound panning
 * @param[in] _pstSound                               Concerned Sound
 * @param[out] _pfPanning                             Sound panning, -1.0f for full left, 0.0f for center, 1.0f for full right
 * @param[out] _pbMix                                 Left/Right channels are be mixed if orxTRUE or act like a balance otherwise
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_GetPanning(const orxSOUNDSYSTEM_SOUND *_pstSound, orxFLOAT *_pfPanning, orxBOOL *_pbMix);

/** Is sound looping?
 * @param[in]   _pstSound                             Concerned sound
 * @return orxTRUE if looping, orxFALSE otherwise
 */
extern orxDLLAPI orxBOOL orxFASTCALL                  orxSoundSystem_IsLooping(const orxSOUNDSYSTEM_SOUND *_pstSound);

/** Gets a sound duration
 * @param[in]   _pstSound                             Concerned sound
 * @return Sound's duration (seconds)
 */
extern orxDLLAPI orxFLOAT orxFASTCALL                 orxSoundSystem_GetDuration(const orxSOUNDSYSTEM_SOUND *_pstSound);

/** Gets a sound status (play/pause/stop)
 * @param[in]   _pstSound                             Concerned sound
 * @return orxSOUNDSYSTEM_STATUS
 */
extern orxDLLAPI orxSOUNDSYSTEM_STATUS orxFASTCALL    orxSoundSystem_GetStatus(const orxSOUNDSYSTEM_SOUND *_pstSound);

/** Sets global volume
 * @param[in] _fGlobalVolume                          Global volume to set
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_SetGlobalVolume(orxFLOAT _fGlobalVolume);

/** Gets global volume
 * @return Gobal volume
 */
extern orxDLLAPI orxFLOAT orxFASTCALL                 orxSoundSystem_GetGlobalVolume();

/** Sets listener position
 * @param[in] _u32Index                               Listener index, 0-based
 * @param[in] _pvPosition                             Desired position
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL                orxSoundSystem_SetListenerPosition(orxU32 _u32Index, const orxVECTOR *_pvPosition);

/** Gets listener position
 * @param[in] _u32Index                               Listener index, 0-based
 * @param[out] _pvPosition                            Listener's position
 * @return orxVECTOR / orxNULL
 */
extern orxDLLAPI orxVECTOR *orxFASTCALL               orxSoundSystem_GetListenerPosition(orxU32 _u32Index, orxVECTOR *_pvPosition);

#endif /* _orxSOUNDSYSTEM_H_ */

/** @} */
