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
 * @file orxSoundSystem.c
 * @date 26/09/2021
 * @author iarwain@orx-project.org
 *
 * MiniAudio sound system plugin implementation
 *
 */


#include "orxPluginAPI.h"

#define STB_VORBIS_HEADER_ONLY
#define FILE                              void
#include "stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY
#undef FILE

#ifdef __APPLE__
  #define MA_NO_RUNTIME_LINKING
#endif /* __APPLE__ */
#ifdef APIENTRY
  #undef APIENTRY
#endif /* APIENTRY */
#define MA_NO_FLAC
#define MA_NO_GENERATION
#define MA_ON_THREAD_ENTRY                rpmalloc_thread_initialize();
#define MA_ON_THREAD_EXIT                 rpmalloc_thread_finalize(1);
#define MINIAUDIO_IMPLEMENTATION

#include "rpmalloc.h"
#include "miniaudio.h"

#undef MA_NO_RUNTIME_LINKING
#undef MA_NO_FLAC
#undef MA_NO_GENERATION
#undef MA_ON_THREAD_ENTRY
#undef MA_ON_THREAD_EXIT
#undef MINIAUDIO_IMPLEMENTATION

#define FILE                              void
#define fopen(NAME, MODE)                 orxResource_Open(NAME, orxFALSE)
#define fread(BUFFER, SIZE, COUNT, FILE)  (orxResource_Read(FILE, SIZE * COUNT, BUFFER, orxNULL, orxNULL) / (SIZE))
#define fgetc(FILE)                       (orxResource_Read(FILE, 1, &c, orxNULL, orxNULL) <= 0) ? EOF : c & 0xFF // Context-sensitive, single call site in stb_vorbis
#define ftell(FILE)                       (orxU32)orxResource_Tell(FILE)
#define fseek(FILE, OFFSET, WHENCE)       (orxResource_Seek(FILE, OFFSET, (orxSEEK_OFFSET_WHENCE)WHENCE) < 0) ? 1 : 0
#define fclose(FILE)                      orxResource_Close(FILE)

#define malloc(SIZE)                      orxMemory_Allocate((orxU32)SIZE, orxMEMORY_TYPE_AUDIO)
#define realloc(MEMORY, SIZE)             orxMemory_Reallocate(MEMORY, (orxU32)SIZE, orxMEMORY_TYPE_AUDIO)
#define free(MEMORY)                      orxMemory_Free(MEMORY)

#undef __STDC_WANT_SECURE_LIB__           /* Do not use fopen_s on Win32 */

#ifndef __orxDEBUG__
  #undef NDEBUG
  #define NDEBUG
#endif /* !__orxDEBUG__ */

#include "stb_vorbis.c"

#undef FILE
#undef fopen
#undef fread
#undef fgetc
#undef ftell
#undef fseek
#undef fclose

#undef malloc
#undef realloc
#undef free


/** Module flags
 */
#define orxSOUNDSYSTEM_KU32_STATIC_FLAG_NONE              0x00000000 /**< No flags */

#define orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY             0x00000001 /**< Ready flag */
#define orxSOUNDSYSTEM_KU32_STATIC_FLAG_RECORDING         0x00000002 /**< Recording flag */

#define orxSOUNDSYSTEM_KU32_STATIC_MASK_ALL               0xFFFFFFFF /**< All mask */


/** Misc defines
 */
#define orxSOUNDSYSTEM_KU32_BANK_SIZE                     128
#define orxSOUNDSYSTEM_KS32_DEFAULT_STREAM_BUFFER_NUMBER  4
#define orxSOUNDSYSTEM_KS32_DEFAULT_STREAM_BUFFER_SIZE    4096
#define orxSOUNDSYSTEM_KS32_DEFAULT_RECORDING_FREQUENCY   48000
#define orxSOUNDSYSTEM_KS32_DEFAULT_FREQUENCY             48000
#define orxSOUNDSYSTEM_KS32_DEFAULT_CHANNEL_NUMBER        2
#define orxSOUNDSYSTEM_KE_DEFAULT_LOG_LEVEL               MA_LOG_LEVEL_WARNING
#define orxSOUNDSYSTEM_KE_DEFAULT_FORMAT                  ma_format_f32
#define orxSOUNDSYSTEM_KF_DEFAULT_DIMENSION_RATIO         orx2F(0.01f)
#define orxSOUNDSYSTEM_KF_DEFAULT_THREAD_SLEEP_TIME       orx2F(0.001f)
#define orxSOUNDSYSTEM_KZ_THREAD_NAME                     "Sound"


/***************************************************************************
 * Structure declaration                                                   *
 ***************************************************************************/

/** Internal sample structure
 */
struct __orxSOUNDSYSTEM_SAMPLE_t
{
};

/** Internal sound structure
 */
struct __orxSOUNDSYSTEM_SOUND_t
{
  orxHANDLE               hUserData;
  ma_sound                stSound;
  ma_node_base            stStreamNode;
  ma_uint32               u32InputChannelCount;
  ma_uint32               u32OutputChannelCount;
};

/** Static structure
 */
typedef struct __orxSOUNDSYSTEM_STATIC_t
{
  orxSOUND_EVENT_PAYLOAD  stRecordingPayload; /**< Recording payload */
  ma_log                  stLog;              /**< Log */
  ma_log_callback         stLogCallback;      /**< Log callback */
  ma_vfs_callbacks        stCallbacks;        /**< Resource callbacks */
  ma_context              stContext;          /**< Context */
  ma_resource_manager     stResourceManager;  /**< Resource manager */
  ma_engine               stEngine;           /**< Engine */
  ma_node_vtable          stStreamNodeVTable; /**< Stream node VTable */
  ma_decoding_backend_vtable stVorbisVTable;  /**< Vorbis decoding backend VTable */
  ma_decoding_backend_vtable *apstVTable[1];  /**< Decoding backend VTable */
  orxBANK                *pstSampleBank;      /**< Sound bank */
  orxBANK                *pstSoundBank;       /**< Sound bank */
  orxFLOAT               *afStreamBuffer;     /**< Stream buffer */
  orxFLOAT               *afRecordingBuffer;  /**< Recording buffer */
  orxFLOAT                fDimensionRatio;    /**< Dimension ration */
  orxFLOAT                fRecDimensionRatio; /**< Reciprocal dimension ratio */
  orxS32                  s32StreamBufferSize;/**< Stream buffer size */
  orxS32                  s32StreamBufferCount;/**< Stream buffer number */
  orxU32                  u32WorkerThread;    /**< Worker thread */
  orxU32                  u32Flags;           /**< Status flags */

} orxSOUNDSYSTEM_STATIC;


/***************************************************************************
 * Static variables                                                        *
 ***************************************************************************/

/** Static data
 */
static orxSOUNDSYSTEM_STATIC sstSoundSystem;


/***************************************************************************
 * Private functions                                                       *
 ***************************************************************************/

static void orxSoundSystem_MiniAudio_ProcessStream(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    // Do some processing of ppFramesIn (one stream of audio data per input bus)
    const float* pFramesIn_0 = ppFramesIn[0]; // Input bus @ index 0.
    float* pFramesOut_0 = ppFramesOut[0];     // Output bus @ index 0.

    // Do some processing. On input, `pFrameCountIn` will be the number of input frames in each
    // buffer in `ppFramesIn` and `pFrameCountOut` will be the capacity of each of the buffers
    // in `ppFramesOut`. On output, `pFrameCountIn` should be set to the number of input frames
    // your node consumed and `pFrameCountOut` should be set the number of output frames that
    // were produced.
    //
    // You should process as many frames as you can. If your effect consumes input frames at the
    // same rate as output frames (always the case, unless you're doing resampling), you need
    // only look at `ppFramesOut` and process that exact number of frames. If you're doing
    // resampling, you'll need to be sure to set both `pFrameCountIn` and `pFrameCountOut`
    // properly.
    ma_channel_map_apply_f32(ppFramesOut[0], NULL, ma_engine_get_channels(&(sstSoundSystem.stEngine)), ppFramesIn[0], NULL, ma_node_get_input_channels(pNode, 0), *pFrameCountIn, ma_channel_mix_mode_simple);
}

/*
 * This function's logic has been lifted straight from 'ma_stbvorbis_init_file', bypassing the use of the pushdata API.
 */
static ma_result SoundSystem_MiniAudio_InitVorbis(ma_read_proc _pfnRead, ma_seek_proc _pfnSeek, ma_tell_proc _pfnTell, void *_pReadSeekTellUserData, const ma_decoding_backend_config *_pstConfig, const ma_allocation_callbacks *_pstAllocationCallbacks, ma_stbvorbis *_pstVorbis)
{
  ma_result hResult;

  /* Checks */
  orxASSERT(_pfnRead != NULL);
  orxASSERT(_pfnSeek != NULL);

  /* Inits internals */
  hResult = ma_stbvorbis_init_internal(_pstConfig, _pstVorbis);

  /* Success? */
  if(hResult == MA_SUCCESS)
  {
    /* Inits callbacks */
    _pstVorbis->onRead                = _pfnRead;
    _pstVorbis->onSeek                = _pfnSeek;
    _pstVorbis->onTell                = _pfnTell;
    _pstVorbis->pReadSeekTellUserData = _pReadSeekTellUserData;
    ma_allocation_callbacks_init_copy(&(_pstVorbis->allocationCallbacks), _pstAllocationCallbacks);

    /* Inits vorbis decoder */
    _pstVorbis->stb = stb_vorbis_open_file(((ma_decoder*)_pReadSeekTellUserData)->data.vfs.file, FALSE, NULL, NULL);

    /* Success? */
    if(_pstVorbis->stb != NULL)
    {
      /* Updates status */
      _pstVorbis->usingPushMode = MA_FALSE;

      /* Executes post-init */
      hResult = ma_stbvorbis_post_init(_pstVorbis);

      /* Failure? */
      if(hResult != MA_SUCCESS)
      {
        /* Closes decoder */
        stb_vorbis_close(_pstVorbis->stb);
      }
    }
    else
    {
      /* Updates result */
      hResult = MA_INVALID_FILE;
    }
  }

  /* Done! */
  return hResult;
}

/*
 * This function's logic has been lifted straight from 'ma_decoding_backend_init__stbvorbis', replacing the internal call to 'ma_stbvorbis_init' with 'SoundSystem_MiniAudio_InitVorbis'.
 */
static ma_result SoundSystem_MiniAudio_InitVorbisBackend(void *_pUserData, ma_read_proc _pfnRead, ma_seek_proc _pfnSeek, ma_tell_proc _pfnTell, void *_pReadSeekTellUserData, const ma_decoding_backend_config *_pstConfig, const ma_allocation_callbacks *_pstAllocationCallbacks, ma_data_source **_ppstBackend)
{
  ma_result     hResult;
  ma_stbvorbis *pstVorbis;

  /* Allocates the decoder backend */
  pstVorbis = (ma_stbvorbis *)ma_malloc(sizeof(ma_stbvorbis), _pstAllocationCallbacks);

  /* Success? */
  if(pstVorbis != NULL)
  {
    /* Inits decoder backend */
    hResult = SoundSystem_MiniAudio_InitVorbis(_pfnRead, _pfnSeek, _pfnTell, _pReadSeekTellUserData, _pstConfig, _pstAllocationCallbacks, pstVorbis);

    /* Success? */
    if(hResult == MA_SUCCESS)
    {
      /* Stores it */
      *_ppstBackend = pstVorbis;
    }
    else
    {
      /* Frees decoder backend */
      ma_free(pstVorbis, _pstAllocationCallbacks);
    }
  }
  else
  {
    /* Updates result */
    hResult = MA_OUT_OF_MEMORY;
  }

  /* Done! */
  return hResult;
}

static void SoundSystem_MiniAudio_UninitVorbisBackend(void *_pUserData, ma_data_source *_pstBackend, const ma_allocation_callbacks *_pstAllocationCallbacks)
{
  /* Uninits decoder backend */
  ma_stbvorbis_uninit((ma_stbvorbis *)_pstBackend, _pstAllocationCallbacks);

  /* Frees it */
  ma_free(_pstBackend, _pstAllocationCallbacks);

  /* Done! */
  return;
}

static orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_ProcessTask(void *_pContext)
{
  /* Processes next job */
  if(ma_resource_manager_process_next_job((ma_resource_manager *)_pContext) == MA_NO_DATA_AVAILABLE)
  {
    /* Sleeps */
    orxSystem_Delay(orxSOUNDSYSTEM_KF_DEFAULT_THREAD_SLEEP_TIME);
  }

  /* Done! */
  return orxSTATUS_SUCCESS;
}

static void orxSoundSystem_MiniAudio_Log(void *_pUserData, ma_uint32 _u32Level, const char *_zMessage)
{
  /* Warning or error? */
  if(_u32Level <= orxSOUNDSYSTEM_KE_DEFAULT_LOG_LEVEL)
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_SOUND, orxANSI_KZ_COLOR_FG_YELLOW "[%s]" orxANSI_KZ_COLOR_FG_DEFAULT " %s", ma_log_level_to_string(_u32Level), _zMessage);
  }

  /* Done! */
  return;
}

static void *orxSoundSystem_MiniAudio_Allocate(size_t _sSize, void *_pContext)
{
  /* Done! */
  return orxMemory_Allocate((orxU32)_sSize, orxMEMORY_TYPE_AUDIO);
}

static void *orxSoundSystem_MiniAudio_Reallocate(void *_pMem, size_t _sSize, void *_pContext)
{
  /* Done! */
  return orxMemory_Reallocate(_pMem, (orxU32)_sSize, orxMEMORY_TYPE_AUDIO);
}

static void orxSoundSystem_MiniAudio_Free(void *p, void *_pContext)
{
  /* Done! */
  orxMemory_Free(p);
}

static ma_result SoundSystem_MiniAudio_Open(ma_vfs *_pstVFS, const char *_zFilename, ma_uint32 _u32OpenMode, ma_vfs_file *_pstFile)
{
    orxHANDLE hResource;
    ma_result hResult;

    /* Opens resource */
    hResource = orxResource_Open(_zFilename, orxFALSE);

    /* Success? */
    if(hResource != orxHANDLE_UNDEFINED)
    {
      /* Stores file */
      *_pstFile = (ma_vfs_file)hResource;

      /* Updates result */
      hResult = MA_SUCCESS;
    }
    else
    {
      /* Updates result */
      hResult = MA_ACCESS_DENIED;
    }

    /* Done! */
    return hResult;
}

static ma_result SoundSystem_MiniAudio_Close(ma_vfs *_pstVFS, ma_vfs_file _stFile)
{
  /* Closes resource */
  orxResource_Close((orxHANDLE)_stFile);

  /* Done! */
  return MA_SUCCESS;
}

static ma_result SoundSystem_MiniAudio_Read(ma_vfs *_pstVFS, ma_vfs_file _stFile, void *_pDst, size_t _sSizeInBytes, size_t *_psBytesRead)
{
  /* Reads data */
  *_psBytesRead = (size_t)orxResource_Read((orxHANDLE)_stFile, (orxS64)_sSizeInBytes, _pDst, orxNULL, orxNULL);

  /* Done! */
  return (*_psBytesRead == 0) ? MA_AT_END : MA_SUCCESS;
}

static ma_result SoundSystem_MiniAudio_Write(ma_vfs *_pstVFS, ma_vfs_file _stFile, const void *_pSrc, size_t _sSizeInBytes, size_t *_psBytesWritten)
{
  /* Writes data */
  *_psBytesWritten = (size_t)orxResource_Write((orxHANDLE)_stFile, _sSizeInBytes, _pSrc, orxNULL, orxNULL);

  /* Done! */
  return (*_psBytesWritten != _sSizeInBytes) ? MA_ACCESS_DENIED : MA_SUCCESS;
}

static ma_result SoundSystem_MiniAudio_Seek(ma_vfs *_pstVFS, ma_vfs_file _stFile, ma_int64 _s64Offset, ma_seek_origin _eOrigin)
{
  ma_result hResult;

  /* Seeks */
  hResult = (orxResource_Seek((orxHANDLE)_stFile, _s64Offset, (orxSEEK_OFFSET_WHENCE)_eOrigin) >= 0) ? MA_SUCCESS : MA_ERROR;

  /* Done! */
  return hResult;
}

static ma_result SoundSystem_MiniAudio_Tell(ma_vfs *_pstVFS, ma_vfs_file _stFile, ma_int64 *_ps64Cursor)
{
  orxS64    s64Cursor;
  ma_result hResult;

  /* Tells */
  s64Cursor = orxResource_Tell((orxHANDLE)_stFile);

  /* Valid? */
  if(s64Cursor >= 0)
  {
    /* Updates cursor */
    *_ps64Cursor = s64Cursor;

    /* Updates result */
    hResult = MA_SUCCESS;
  }
  else
  {
    /* Updates result */
    hResult = MA_ERROR;
  }

  /* Done! */
  return hResult;
}

static ma_result SoundSystem_MiniAudio_Info(ma_vfs *_pstVFS, ma_vfs_file _stFile, ma_file_info *_pstInfo)
{
  /* Gets size */
  _pstInfo->sizeInBytes = (ma_uint64)orxResource_GetSize((orxHANDLE)_stFile);

  /* Done! */
  return MA_SUCCESS;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_Init()
{
  orxSTATUS eResult = orxSTATUS_FAILURE;

  /* Was already initialized? */
  if(!(sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY))
  {
    ma_resource_manager_config  stResourceManagerConfig;
    ma_result                   hResult;

    /* Cleans static controller */
    orxMemory_Zero(&sstSoundSystem, sizeof(orxSOUNDSYSTEM_STATIC));

    /* Inits vorbis decoding backend VTable */
    sstSoundSystem.stVorbisVTable.onInit                  = &SoundSystem_MiniAudio_InitVorbisBackend;
    sstSoundSystem.stVorbisVTable.onUninit                = &SoundSystem_MiniAudio_UninitVorbisBackend;
    sstSoundSystem.apstVTable[0]                          = &(sstSoundSystem.stVorbisVTable);

    /* Inits stream node VTable */
    sstSoundSystem.stStreamNodeVTable.onProcess           = &orxSoundSystem_MiniAudio_ProcessStream;
    sstSoundSystem.stStreamNodeVTable.inputBusCount       = 1;
    sstSoundSystem.stStreamNodeVTable.outputBusCount      = 1;
    sstSoundSystem.stStreamNodeVTable.flags               = MA_NODE_FLAG_CONTINUOUS_PROCESSING | MA_NODE_FLAG_DIFFERENT_PROCESSING_RATES;

    /* Inits resource callbacks */
    sstSoundSystem.stCallbacks.onOpen                     = &SoundSystem_MiniAudio_Open;
    sstSoundSystem.stCallbacks.onClose                    = &SoundSystem_MiniAudio_Close;
    sstSoundSystem.stCallbacks.onRead                     = &SoundSystem_MiniAudio_Read;
    sstSoundSystem.stCallbacks.onWrite                    = &SoundSystem_MiniAudio_Write;
    sstSoundSystem.stCallbacks.onSeek                     = &SoundSystem_MiniAudio_Seek;
    sstSoundSystem.stCallbacks.onTell                     = &SoundSystem_MiniAudio_Tell;
    sstSoundSystem.stCallbacks.onInfo                     = &SoundSystem_MiniAudio_Info;

    /* Inits resource manager configuration */
    stResourceManagerConfig                               = ma_resource_manager_config_init();
    stResourceManagerConfig.decodedFormat                 = orxSOUNDSYSTEM_KE_DEFAULT_FORMAT;
    stResourceManagerConfig.decodedSampleRate             = orxSOUNDSYSTEM_KS32_DEFAULT_FREQUENCY;
    stResourceManagerConfig.jobThreadCount                = 0;
    stResourceManagerConfig.flags                         = MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING;
    stResourceManagerConfig.ppCustomDecodingBackendVTables= sstSoundSystem.apstVTable;
    stResourceManagerConfig.customDecodingBackendCount    = orxARRAY_GET_ITEM_COUNT(sstSoundSystem.apstVTable);
    stResourceManagerConfig.pVFS                          = &(sstSoundSystem.stCallbacks);
    stResourceManagerConfig.allocationCallbacks.onMalloc  = &orxSoundSystem_MiniAudio_Allocate;
    stResourceManagerConfig.allocationCallbacks.onRealloc = &orxSoundSystem_MiniAudio_Reallocate;
    stResourceManagerConfig.allocationCallbacks.onFree    = &orxSoundSystem_MiniAudio_Free;

    /* Inits log system */
    hResult                                               = ma_log_init(&(stResourceManagerConfig.allocationCallbacks), &(sstSoundSystem.stLog));
    orxASSERT(hResult == MA_SUCCESS);
    sstSoundSystem.stLogCallback                          = ma_log_callback_init(&orxSoundSystem_MiniAudio_Log, NULL);
    hResult                                               = ma_log_register_callback(&(sstSoundSystem.stLog), sstSoundSystem.stLogCallback);
    orxASSERT(hResult == MA_SUCCESS);
    stResourceManagerConfig.pLog                          = &(sstSoundSystem.stLog);

    /* Inits resource manager */
    hResult = ma_resource_manager_init(&stResourceManagerConfig, &(sstSoundSystem.stResourceManager));

    /* Success? */
    if(hResult == MA_SUCCESS)
    {
      ma_context_config stContextConfig;

      /* Inits context */
      stContextConfig       = ma_context_config_init();
      stContextConfig.pLog  = &(sstSoundSystem.stLog);
      hResult               = ma_context_init(NULL, 0, &stContextConfig, &(sstSoundSystem.stContext));

      /* Success? */
      if(hResult == MA_SUCCESS)
      {
        ma_engine_config stEngineConfig;

        /* Inits engine */
        stEngineConfig                  = ma_engine_config_init();
        stEngineConfig.pContext         = &(sstSoundSystem.stContext);
        stEngineConfig.pLog             = &(sstSoundSystem.stLog);
        stEngineConfig.pResourceManager = &(sstSoundSystem.stResourceManager);
        stEngineConfig.sampleRate       = orxSOUNDSYSTEM_KS32_DEFAULT_FREQUENCY;
        stEngineConfig.channels         = orxSOUNDSYSTEM_KS32_DEFAULT_CHANNEL_NUMBER;
        ma_allocation_callbacks_init_copy(&(stEngineConfig.allocationCallbacks), &(stResourceManagerConfig.allocationCallbacks));
        hResult                         = ma_engine_init(&stEngineConfig, &(sstSoundSystem.stEngine));

        /* Success? */
        if(hResult == MA_SUCCESS)
        {
          /* Pushes config section */
          orxConfig_PushSection(orxSOUNDSYSTEM_KZ_CONFIG_SECTION);

          /* Has stream buffer size? */
          if(orxConfig_HasValue(orxSOUNDSYSTEM_KZ_CONFIG_STREAM_BUFFER_SIZE) != orxFALSE)
          {
            /* Stores it */
            sstSoundSystem.s32StreamBufferSize = orxConfig_GetU32(orxSOUNDSYSTEM_KZ_CONFIG_STREAM_BUFFER_SIZE) & 0xFFFFFFFC;
          }
          else
          {
            /* Uses default one */
            sstSoundSystem.s32StreamBufferSize = orxSOUNDSYSTEM_KS32_DEFAULT_STREAM_BUFFER_SIZE;
          }

          /* Has stream buffer number? */
          if(orxConfig_HasValue(orxSOUNDSYSTEM_KZ_CONFIG_STREAM_BUFFER_NUMBER) != orxFALSE)
          {
            /* Gets stream number */
            sstSoundSystem.s32StreamBufferCount = orxMAX(2, orxConfig_GetU32(orxSOUNDSYSTEM_KZ_CONFIG_STREAM_BUFFER_NUMBER));
          }
          else
          {
            /* Uses default one */
            sstSoundSystem.s32StreamBufferCount = orxSOUNDSYSTEM_KS32_DEFAULT_STREAM_BUFFER_NUMBER;
          }

          /* Creates banks */
          sstSoundSystem.pstSampleBank  = orxBank_Create(orxSOUNDSYSTEM_KU32_BANK_SIZE, sizeof(orxSOUNDSYSTEM_SAMPLE), orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_AUDIO);
          sstSoundSystem.pstSoundBank   = orxBank_Create(orxSOUNDSYSTEM_KU32_BANK_SIZE, sizeof(orxSOUNDSYSTEM_SOUND), orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_AUDIO);

          /* Valid? */
          if((sstSoundSystem.pstSampleBank != orxNULL) && (sstSoundSystem.pstSoundBank != orxNULL))
          {
            orxFLOAT fRatio;

            /* Allocates stream buffers */
            sstSoundSystem.afStreamBuffer     = (orxFLOAT *)orxMemory_Allocate(sstSoundSystem.s32StreamBufferSize * sizeof(orxFLOAT), orxMEMORY_TYPE_AUDIO);
            sstSoundSystem.afRecordingBuffer  = (orxFLOAT *)orxMemory_Allocate(sstSoundSystem.s32StreamBufferSize * sizeof(orxFLOAT), orxMEMORY_TYPE_AUDIO);

            /* Valid? */
            if((sstSoundSystem.afStreamBuffer != orxNULL) && (sstSoundSystem.afRecordingBuffer != orxNULL))
            {
              /* Adds job thread */
              sstSoundSystem.u32WorkerThread = orxThread_Start(&orxSoundSystem_MiniAudio_ProcessTask, orxSOUNDSYSTEM_KZ_THREAD_NAME, &(sstSoundSystem.stResourceManager));

              /* Valid? */
              if(sstSoundSystem.u32WorkerThread != orxU32_UNDEFINED)
              {
                /* Retrieves dimension ratio */
                fRatio                          = orxConfig_GetFloat(orxSOUNDSYSTEM_KZ_CONFIG_RATIO);
                sstSoundSystem.fDimensionRatio  = (fRatio > orxFLOAT_0) ? fRatio : orxSOUNDSYSTEM_KF_DEFAULT_DIMENSION_RATIO;

                /* Stores ratio */
                orxConfig_SetFloat(orxSOUNDSYSTEM_KZ_CONFIG_RATIO, sstSoundSystem.fDimensionRatio);

                /* Gets reciprocal dimension ratio */
                sstSoundSystem.fRecDimensionRatio = orxFLOAT_1 / sstSoundSystem.fDimensionRatio;

                /* Updates status */
                orxFLAG_SET(sstSoundSystem.u32Flags, orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY, orxSOUNDSYSTEM_KU32_STATIC_MASK_ALL);

                /* Updates result */
                eResult = orxSTATUS_SUCCESS;
              }
              else
              {
                /* Deletes buffers */
                orxMemory_Free(sstSoundSystem.afStreamBuffer);
                orxMemory_Free(sstSoundSystem.afRecordingBuffer);
                sstSoundSystem.afStreamBuffer   = orxNULL;
                sstSoundSystem.afRecordingBuffer= orxNULL;

                /* Deletes banks */
                orxBank_Delete(sstSoundSystem.pstSampleBank);
                orxBank_Delete(sstSoundSystem.pstSoundBank);
                sstSoundSystem.pstSampleBank  = orxNULL;
                sstSoundSystem.pstSoundBank   = orxNULL;

                /* Uninits engine */
                ma_engine_uninit(&(sstSoundSystem.stEngine));

                /* Uninits context */
                ma_context_uninit(&(sstSoundSystem.stContext));

                /* Uninits resource manager */
                ma_resource_manager_uninit(&(sstSoundSystem.stResourceManager));

                /* Uninits log */
                ma_log_uninit(&(sstSoundSystem.stLog));
              }
            }
            else
            {
              /* Deletes banks */
              orxBank_Delete(sstSoundSystem.pstSampleBank);
              orxBank_Delete(sstSoundSystem.pstSoundBank);
              sstSoundSystem.pstSampleBank  = orxNULL;
              sstSoundSystem.pstSoundBank   = orxNULL;

              /* Uninits engine */
              ma_engine_uninit(&(sstSoundSystem.stEngine));

              /* Uninits context */
              ma_context_uninit(&(sstSoundSystem.stContext));

              /* Uninits resource manager */
              ma_resource_manager_uninit(&(sstSoundSystem.stResourceManager));

              /* Uninits log */
              ma_log_uninit(&(sstSoundSystem.stLog));
            }
          }
          else
          {
            /* Uninits engine */
            ma_engine_uninit(&(sstSoundSystem.stEngine));

            /* Uninits context */
            ma_context_uninit(&(sstSoundSystem.stContext));

            /* Uninits resource manager */
            ma_resource_manager_uninit(&(sstSoundSystem.stResourceManager));

            /* Uninits log */
            ma_log_uninit(&(sstSoundSystem.stLog));
          }

          /* Pops config section */
          orxConfig_PopSection();
        }
        else
        {
          /* Uninits context */
          ma_context_uninit(&(sstSoundSystem.stContext));

          /* Uninits resource manager */
          ma_resource_manager_uninit(&(sstSoundSystem.stResourceManager));

          /* Uninits log */
          ma_log_uninit(&(sstSoundSystem.stLog));
        }
      }
      else
      {
        /* Uninits resource manager */
        ma_resource_manager_uninit(&(sstSoundSystem.stResourceManager));

        /* Uninits log */
        ma_log_uninit(&(sstSoundSystem.stLog));
      }
    }
    else
    {
      /* Uninits log */
      ma_log_uninit(&(sstSoundSystem.stLog));
    }
  }

  /* Done! */
  return eResult;
}

void orxFASTCALL orxSoundSystem_MiniAudio_Exit()
{
  /* Was initialized? */
  if(sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY)
  {
    /* Stops any recording */
    orxSoundSystem_StopRecording();

    /* Joins worker thread */
    orxThread_Join(sstSoundSystem.u32WorkerThread);

    /* Uninits engine */
    ma_engine_uninit(&(sstSoundSystem.stEngine));

    /* Uninits context */
    ma_context_uninit(&(sstSoundSystem.stContext));

    /* Uninits resource manager */
    ma_resource_manager_uninit(&(sstSoundSystem.stResourceManager));

    /* Uninits log */
    ma_log_uninit(&(sstSoundSystem.stLog));

    /* Deletes buffers */
    orxMemory_Free(sstSoundSystem.afStreamBuffer);
    orxMemory_Free(sstSoundSystem.afRecordingBuffer);

    /* Deletes banks */
    orxBank_Delete(sstSoundSystem.pstSampleBank);
    orxBank_Delete(sstSoundSystem.pstSoundBank);

    /* Cleans static controller */
    orxMemory_Zero(&sstSoundSystem, sizeof(orxSOUNDSYSTEM_STATIC));
  }

  /* Done! */
  return;
}

orxSOUNDSYSTEM_SAMPLE *orxFASTCALL orxSoundSystem_MiniAudio_CreateSample(orxU32 _u32ChannelNumber, orxU32 _u32FrameNumber, orxU32 _u32SampleRate)
{
  orxSOUNDSYSTEM_SAMPLE *pstResult = orxNULL;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);

  //! TODO

  /* Done! */
  return pstResult;
}

orxSOUNDSYSTEM_SAMPLE *orxFASTCALL orxSoundSystem_MiniAudio_LoadSample(const orxSTRING _zFilename)
{
  orxSOUNDSYSTEM_SAMPLE *pstResult = orxNULL;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_zFilename != orxNULL);

  //! TODO

  /* Done! */
  return pstResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_DeleteSample(orxSOUNDSYSTEM_SAMPLE *_pstSample)
{
  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSample != orxNULL);

  //! TODO

  /* Done! */
  return orxSTATUS_SUCCESS;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_GetSampleInfo(const orxSOUNDSYSTEM_SAMPLE *_pstSample, orxU32 *_pu32ChannelNumber, orxU32 *_pu32FrameNumber, orxU32 *_pu32SampleRate)
{
  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSample != orxNULL);
  orxASSERT(_pu32ChannelNumber != orxNULL);
  orxASSERT(_pu32FrameNumber != orxNULL);
  orxASSERT(_pu32SampleRate != orxNULL);

  //! TODO

  /* Done! */
  return orxSTATUS_SUCCESS;
}

orxSOUNDSYSTEM_SOUND *orxFASTCALL orxSoundSystem_MiniAudio_CreateFromSample(orxHANDLE _hUserData, const orxSOUNDSYSTEM_SAMPLE *_pstSample)
{
  orxSOUNDSYSTEM_SOUND *pstResult = orxNULL;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT((_hUserData != orxNULL) && (_hUserData != orxHANDLE_UNDEFINED));
  orxASSERT(_pstSample != orxNULL);

  //! TODO

  /* Done! */
  return pstResult;
}

orxSOUNDSYSTEM_SOUND *orxFASTCALL orxSoundSystem_MiniAudio_CreateStream(orxHANDLE _hUserData, orxU32 _u32ChannelNumber, orxU32 _u32SampleRate)
{
  orxSOUNDSYSTEM_SOUND *pstResult = orxNULL;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT((_hUserData != orxNULL) && (_hUserData != orxHANDLE_UNDEFINED));

  //! TODO

  /* Done! */
  return pstResult;
}

orxSOUNDSYSTEM_SOUND *orxFASTCALL orxSoundSystem_MiniAudio_CreateStreamFromFile(orxHANDLE _hUserData, const orxSTRING _zFilename)
{
  orxSOUNDSYSTEM_SOUND *pstResult;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT((_hUserData != orxNULL) && (_hUserData != orxHANDLE_UNDEFINED));
  orxASSERT(_zFilename != orxNULL);

  /* Allocates sound */
  pstResult = (orxSOUNDSYSTEM_SOUND *)orxBank_Allocate(sstSoundSystem.pstSoundBank);

  /* Valid? */
  if(pstResult != orxNULL)
  {
    const orxSTRING zResourceLocation;

    /* Clears it */
    orxMemory_Zero(pstResult, sizeof(orxSOUNDSYSTEM_SOUND));

    /* Locates resource */
    zResourceLocation = orxResource_Locate(orxSOUND_KZ_RESOURCE_GROUP, _zFilename);

    /* Success? */
    if(zResourceLocation != orxNULL)
    {
      ma_result       hResult;
      ma_sound_config stSoundConfig;

      /* Inits sound's config */
      stSoundConfig             = ma_sound_config_init();
      stSoundConfig.pFilePath   = zResourceLocation;
      stSoundConfig.channelsOut = MA_SOUND_SOURCE_CHANNEL_COUNT;
      stSoundConfig.flags       = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT;

      /* Creates sound */
      hResult = ma_sound_init_ex(&(sstSoundSystem.stEngine), &stSoundConfig, &(pstResult->stSound));

      /* Success? */
      if(hResult == MA_SUCCESS)
      {
        ma_node_config stNodeConfig;

        /* Inits stream node's config */
        stNodeConfig                    = ma_node_config_init();
        hResult                         = ma_sound_get_data_format(&(pstResult->stSound), NULL, &(pstResult->u32InputChannelCount), NULL, NULL, 0);
        pstResult->u32OutputChannelCount= ma_engine_get_channels(&(sstSoundSystem.stEngine));
        stNodeConfig.pInputChannels     = &(pstResult->u32InputChannelCount);
        stNodeConfig.pOutputChannels    = &(pstResult->u32OutputChannelCount);
        stNodeConfig.vtable             = &(sstSoundSystem.stStreamNodeVTable);
        orxASSERT(hResult == MA_SUCCESS);

        /* Creates stream node */
        hResult = ma_node_init(ma_engine_get_node_graph(&(sstSoundSystem.stEngine)), &stNodeConfig, &(sstSoundSystem.stEngine.allocationCallbacks), &(pstResult->stStreamNode));
        orxASSERT(hResult == MA_SUCCESS);

        /* Chains them */
        hResult = ma_node_attach_output_bus(&(pstResult->stStreamNode), 0, ma_engine_get_endpoint(&(sstSoundSystem.stEngine)), 0);
        orxASSERT(hResult == MA_SUCCESS);
        hResult = ma_node_attach_output_bus(&(pstResult->stSound), 0, &(pstResult->stStreamNode), 0);
        orxASSERT(hResult == MA_SUCCESS);
      }
      else
      {
        /* Logs message */
        orxDEBUG_PRINT(orxDEBUG_LEVEL_SOUND, "Can't load sound stream <%s>: can't process resource [%s].", _zFilename, zResourceLocation);

        /* Deletes sound */
        orxBank_Free(sstSoundSystem.pstSoundBank, pstResult);

        /* Updates result */
        pstResult = orxNULL;
      }
    }
    else
    {
      /* Logs message */
      orxDEBUG_PRINT(orxDEBUG_LEVEL_SOUND, "Can't load sound stream <%s>: can't locate resource.", _zFilename);

      /* Deletes sound */
      orxBank_Free(sstSoundSystem.pstSoundBank, pstResult);

      /* Updates result */
      pstResult = orxNULL;
    }
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_SOUND, "Can't load sound stream <%s>: can't allocate sound structure.", _zFilename);
  }

  /* Done! */
  return pstResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_Delete(orxSOUNDSYSTEM_SOUND *_pstSound)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  /* Uninits sound */
  ma_sound_uninit(&(_pstSound->stSound));

  /* Deletes sound */
  orxBank_Free(sstSoundSystem.pstSoundBank, _pstSound);

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_Play(orxSOUNDSYSTEM_SOUND *_pstSound)
{
  orxSTATUS eResult;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  /* Plays sound */
  eResult = (ma_sound_start(&(_pstSound->stSound)) == MA_SUCCESS) ? orxSTATUS_SUCCESS : orxSTATUS_FAILURE;

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_Pause(orxSOUNDSYSTEM_SOUND *_pstSound)
{
  orxSTATUS eResult;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  /* Pauses sound */
  eResult = (ma_sound_stop(&(_pstSound->stSound)) == MA_SUCCESS) ? orxSTATUS_SUCCESS : orxSTATUS_FAILURE;

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_Stop(orxSOUNDSYSTEM_SOUND *_pstSound)
{
  orxSTATUS eResult;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  /* Pauses sound */
  if(ma_sound_stop(&(_pstSound->stSound)) == MA_SUCCESS)
  {
    /* Rewinds it */
    eResult = (ma_sound_seek_to_pcm_frame(&(_pstSound->stSound), 0) == MA_SUCCESS) ? orxSTATUS_SUCCESS : orxSTATUS_FAILURE;
  }
  else
  {
    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_StartRecording(const orxSTRING _zName, orxBOOL _bWriteToFile, orxU32 _u32SampleRate, orxU32 _u32ChannelNumber)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_zName != orxNULL);

  //! TODO

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_StopRecording()
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);

  //! TODO

  /* Done! */
  return eResult;
}

orxBOOL orxFASTCALL orxSoundSystem_MiniAudio_HasRecordingSupport()
{
  orxBOOL bResult = orxFALSE;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);

  //! TODO

  /* Done! */
  return bResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_SetVolume(orxSOUNDSYSTEM_SOUND *_pstSound, orxFLOAT _fVolume)
{
  orxSTATUS eResult;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  /* Sets volume */
  eResult = (ma_sound_set_volume(&(_pstSound->stSound), _fVolume) == MA_SUCCESS) ? orxSTATUS_SUCCESS : orxSTATUS_FAILURE;

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_SetPitch(orxSOUNDSYSTEM_SOUND *_pstSound, orxFLOAT _fPitch)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  /* Sets pitch */
  ma_sound_set_pitch(&(_pstSound->stSound), _fPitch);

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_SetTime(orxSOUNDSYSTEM_SOUND *_pstSound, orxFLOAT _fTime)
{
  orxSTATUS eResult;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  /* Sets time */
  eResult = (ma_sound_seek_to_pcm_frame(&(_pstSound->stSound), orxF2U(_fTime * orxU2F(ma_engine_get_sample_rate(ma_sound_get_engine(&(_pstSound->stSound)))))) == MA_SUCCESS) ? orxSTATUS_SUCCESS : orxSTATUS_FAILURE;

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_SetPosition(orxSOUNDSYSTEM_SOUND *_pstSound, const orxVECTOR *_pvPosition)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);
  orxASSERT(_pvPosition != orxNULL);

  /* Updates position */
  ma_sound_set_position(&(_pstSound->stSound), _pvPosition->fX * sstSoundSystem.fDimensionRatio, _pvPosition->fY * sstSoundSystem.fDimensionRatio, _pvPosition->fZ * sstSoundSystem.fDimensionRatio);

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_SetAttenuation(orxSOUNDSYSTEM_SOUND *_pstSound, orxFLOAT _fAttenuation)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  //! REMOVE

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_SetReferenceDistance(orxSOUNDSYSTEM_SOUND *_pstSound, orxFLOAT _fDistance)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  //! REMOVE

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_Loop(orxSOUNDSYSTEM_SOUND *_pstSound, orxBOOL _bLoop)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  /* Updates status */
  ma_sound_set_looping(&(_pstSound->stSound), (_bLoop != orxFALSE) ? MA_TRUE : MA_FALSE);

  /* Done! */
  return eResult;
}

orxFLOAT orxFASTCALL orxSoundSystem_MiniAudio_GetVolume(const orxSOUNDSYSTEM_SOUND *_pstSound)
{
  orxFLOAT fResult;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  /* Updates result */
  fResult = ma_node_get_output_bus_volume((ma_node *)&(_pstSound->stSound), 0);

  /* Done! */
  return fResult;
}

orxFLOAT orxFASTCALL orxSoundSystem_MiniAudio_GetPitch(const orxSOUNDSYSTEM_SOUND *_pstSound)
{
  orxFLOAT fResult;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  /* Updates result */
  fResult = _pstSound->stSound.engineNode.pitch;

  /* Done! */
  return fResult;
}

orxFLOAT orxFASTCALL orxSoundSystem_MiniAudio_GetTime(const orxSOUNDSYSTEM_SOUND *_pstSound)
{
  ma_uint64 u64Time;
  orxFLOAT  fResult;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  /* Gets time */
  u64Time = ma_sound_get_time_in_pcm_frames(&(_pstSound->stSound));

  /* Updates result */
  fResult = orxU2F(u64Time) / orxU2F(ma_engine_get_sample_rate(ma_sound_get_engine((ma_sound *)&(_pstSound->stSound))));

  /* Done! */
  return fResult;
}

orxVECTOR *orxFASTCALL orxSoundSystem_MiniAudio_GetPosition(const orxSOUNDSYSTEM_SOUND *_pstSound, orxVECTOR *_pvPosition)
{
  ma_vec3f    vPosition;
  orxVECTOR  *pvResult = _pvPosition;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);
  orxASSERT(_pvPosition != orxNULL);

  /* Gets position */
  vPosition = ma_sound_get_position(&(_pstSound->stSound));

  /* Updates result */
  orxVector_Set(pvResult, sstSoundSystem.fRecDimensionRatio * vPosition.x, sstSoundSystem.fRecDimensionRatio * vPosition.y, sstSoundSystem.fRecDimensionRatio * vPosition.z);

  /* Done! */
  return pvResult;
}

orxFLOAT orxFASTCALL orxSoundSystem_MiniAudio_GetAttenuation(const orxSOUNDSYSTEM_SOUND *_pstSound)
{
  orxFLOAT fResult = orxFLOAT_0;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  //! REMOVE

  /* Done! */
  return fResult;
}

orxFLOAT orxFASTCALL orxSoundSystem_MiniAudio_GetReferenceDistance(const orxSOUNDSYSTEM_SOUND *_pstSound)
{
  orxFLOAT fResult = orxFLOAT_0;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  //! REMOVE

  /* Done! */
  return fResult;
}

orxBOOL orxFASTCALL orxSoundSystem_MiniAudio_IsLooping(const orxSOUNDSYSTEM_SOUND *_pstSound)
{
  orxBOOL bResult;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  /* Updates result */
  bResult = (ma_sound_is_looping(&(_pstSound->stSound)) != MA_FALSE) ? orxTRUE : orxFALSE;

  /* Done! */
  return bResult;
}

orxFLOAT orxFASTCALL orxSoundSystem_MiniAudio_GetDuration(const orxSOUNDSYSTEM_SOUND *_pstSound)
{
  ma_uint64 u64Length;
  ma_result hResult;
  orxFLOAT  fResult = orxFLOAT_0;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  /* Gets length */
  hResult = ma_sound_get_length_in_pcm_frames((ma_sound *)&(_pstSound->stSound), &u64Length);

  /* Success? */
  if(hResult == MA_SUCCESS)
  {
    /* Updates result */
    fResult = orxU2F(u64Length) / orxU2F(ma_engine_get_sample_rate(ma_sound_get_engine((ma_sound *)&(_pstSound->stSound))));
  }

  /* Done! */
  return fResult;
}

orxSOUNDSYSTEM_STATUS orxFASTCALL orxSoundSystem_MiniAudio_GetStatus(const orxSOUNDSYSTEM_SOUND *_pstSound)
{
  orxSOUNDSYSTEM_STATUS eResult = orxSOUNDSYSTEM_STATUS_NONE;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstSound != orxNULL);

  /* Playing? */
  if(ma_sound_is_playing(&(_pstSound->stSound)) != MA_FALSE)
  {
    /* Updates result */
    eResult = orxSOUNDSYSTEM_STATUS_PLAY;
  }
  /* Paused/Stopped */
  else
  {
    /* Updates result */
    eResult = ((ma_sound_get_time_in_pcm_frames(&(_pstSound->stSound)) == 0) || (ma_sound_at_end(&(_pstSound->stSound)) != MA_FALSE)) ? orxSOUNDSYSTEM_STATUS_STOP : orxSOUNDSYSTEM_STATUS_PAUSE;
  }

  /* Done! */
  return eResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_SetGlobalVolume(orxFLOAT _fVolume)
{
  orxSTATUS eResult;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);

  /* Sets volume */
  eResult = (ma_engine_set_volume(&(sstSoundSystem.stEngine), _fVolume) == MA_SUCCESS) ? orxSTATUS_SUCCESS : orxSTATUS_FAILURE;

  /* Done! */
  return eResult;
}

orxFLOAT orxFASTCALL orxSoundSystem_MiniAudio_GetGlobalVolume()
{
  orxFLOAT fResult;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);

  /* Updates result */
  fResult = ma_node_get_output_bus_volume(ma_engine_get_endpoint(&(sstSoundSystem.stEngine)), 0);

  /* Done! */
  return fResult;
}

orxSTATUS orxFASTCALL orxSoundSystem_MiniAudio_SetListenerPosition(const orxVECTOR *_pvPosition)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pvPosition != orxNULL);

  //! TODO

  /* Done! */
  return eResult;
}

orxVECTOR *orxFASTCALL orxSoundSystem_MiniAudio_GetListenerPosition(orxVECTOR *_pvPosition)
{
  orxVECTOR *pvResult = orxNULL;

  /* Checks */
  orxASSERT((sstSoundSystem.u32Flags & orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY) == orxSOUNDSYSTEM_KU32_STATIC_FLAG_READY);
  orxASSERT(_pvPosition != orxNULL);

  //! TODO

  /* Done! */
  return pvResult;
}


/***************************************************************************
 * Plugin related                                                          *
 ***************************************************************************/

orxPLUGIN_USER_CORE_FUNCTION_START(SOUNDSYSTEM);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_Init, SOUNDSYSTEM, INIT);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_Exit, SOUNDSYSTEM, EXIT);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_CreateSample, SOUNDSYSTEM, CREATE_SAMPLE);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_LoadSample, SOUNDSYSTEM, LOAD_SAMPLE);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_DeleteSample, SOUNDSYSTEM, DELETE_SAMPLE);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_GetSampleInfo, SOUNDSYSTEM, GET_SAMPLE_INFO);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_CreateFromSample, SOUNDSYSTEM, CREATE_FROM_SAMPLE);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_CreateStream, SOUNDSYSTEM, CREATE_STREAM);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_CreateStreamFromFile, SOUNDSYSTEM, CREATE_STREAM_FROM_FILE);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_Delete, SOUNDSYSTEM, DELETE);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_Play, SOUNDSYSTEM, PLAY);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_Pause, SOUNDSYSTEM, PAUSE);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_Stop, SOUNDSYSTEM, STOP);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_StartRecording, SOUNDSYSTEM, START_RECORDING);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_StopRecording, SOUNDSYSTEM, STOP_RECORDING);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_HasRecordingSupport, SOUNDSYSTEM, HAS_RECORDING_SUPPORT);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_SetVolume, SOUNDSYSTEM, SET_VOLUME);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_SetPitch, SOUNDSYSTEM, SET_PITCH);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_SetTime, SOUNDSYSTEM, SET_TIME);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_SetPosition, SOUNDSYSTEM, SET_POSITION);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_SetAttenuation, SOUNDSYSTEM, SET_ATTENUATION);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_SetReferenceDistance, SOUNDSYSTEM, SET_REFERENCE_DISTANCE);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_Loop, SOUNDSYSTEM, LOOP);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_GetVolume, SOUNDSYSTEM, GET_VOLUME);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_GetPitch, SOUNDSYSTEM, GET_PITCH);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_GetTime, SOUNDSYSTEM, GET_TIME);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_GetPosition, SOUNDSYSTEM, GET_POSITION);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_GetAttenuation, SOUNDSYSTEM, GET_ATTENUATION);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_GetReferenceDistance, SOUNDSYSTEM, GET_REFERENCE_DISTANCE);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_IsLooping, SOUNDSYSTEM, IS_LOOPING);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_GetDuration, SOUNDSYSTEM, GET_DURATION);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_GetStatus, SOUNDSYSTEM, GET_STATUS);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_SetGlobalVolume, SOUNDSYSTEM, SET_GLOBAL_VOLUME);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_GetGlobalVolume, SOUNDSYSTEM, GET_GLOBAL_VOLUME);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_SetListenerPosition, SOUNDSYSTEM, SET_LISTENER_POSITION);
orxPLUGIN_USER_CORE_FUNCTION_ADD(orxSoundSystem_MiniAudio_GetListenerPosition, SOUNDSYSTEM, GET_LISTENER_POSITION);
orxPLUGIN_USER_CORE_FUNCTION_END();
