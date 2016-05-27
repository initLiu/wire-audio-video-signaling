/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_waz_call_FlowManager */

#ifndef _Included_com_waz_call_FlowManager
#define _Included_com_waz_call_FlowManager
#ifdef __cplusplus
extern "C" {
#endif
#undef com_waz_call_FlowManager_MCAT_NORMAL
#define com_waz_call_FlowManager_MCAT_NORMAL 0L
#undef com_waz_call_FlowManager_MCAT_HOLD
#define com_waz_call_FlowManager_MCAT_HOLD 1L
#undef com_waz_call_FlowManager_MCAT_PLAYBACK
#define com_waz_call_FlowManager_MCAT_PLAYBACK 2L
#undef com_waz_call_FlowManager_MCAT_CALL
#define com_waz_call_FlowManager_MCAT_CALL 3L
#undef com_waz_call_FlowManager_MCAT_CALL_VIDEO
#define com_waz_call_FlowManager_MCAT_CALL_VIDEO 4L
#undef com_waz_call_FlowManager_AUSRC_INTMIC
#define com_waz_call_FlowManager_AUSRC_INTMIC 0L
#undef com_waz_call_FlowManager_AUSRC_EXTMIC
#define com_waz_call_FlowManager_AUSRC_EXTMIC 1L
#undef com_waz_call_FlowManager_AUSRC_HEADSET
#define com_waz_call_FlowManager_AUSRC_HEADSET 2L
#undef com_waz_call_FlowManager_AUSRC_BT
#define com_waz_call_FlowManager_AUSRC_BT 3L
#undef com_waz_call_FlowManager_AUSRC_LINEIN
#define com_waz_call_FlowManager_AUSRC_LINEIN 4L
#undef com_waz_call_FlowManager_AUSRC_SPDIF
#define com_waz_call_FlowManager_AUSRC_SPDIF 5L
#undef com_waz_call_FlowManager_AUPLAY_EARPIECE
#define com_waz_call_FlowManager_AUPLAY_EARPIECE 0L
#undef com_waz_call_FlowManager_AUPLAY_SPEAKER
#define com_waz_call_FlowManager_AUPLAY_SPEAKER 1L
#undef com_waz_call_FlowManager_AUPLAY_HEADSET
#define com_waz_call_FlowManager_AUPLAY_HEADSET 2L
#undef com_waz_call_FlowManager_AUPLAY_BT
#define com_waz_call_FlowManager_AUPLAY_BT 3L
#undef com_waz_call_FlowManager_AUPLAY_LINEOUT
#define com_waz_call_FlowManager_AUPLAY_LINEOUT 4L
#undef com_waz_call_FlowManager_AUPLAY_SPDIF
#define com_waz_call_FlowManager_AUPLAY_SPDIF 5L
#undef com_waz_call_FlowManager_VIDEO_SEND_NONE
#define com_waz_call_FlowManager_VIDEO_SEND_NONE 0L
#undef com_waz_call_FlowManager_VIDEO_PREVIEW
#define com_waz_call_FlowManager_VIDEO_PREVIEW 1L
#undef com_waz_call_FlowManager_VIDEO_SEND
#define com_waz_call_FlowManager_VIDEO_SEND 2L
#undef com_waz_call_FlowManager_VIDEO_STATE_STOPPED
#define com_waz_call_FlowManager_VIDEO_STATE_STOPPED 0L
#undef com_waz_call_FlowManager_VIDEO_STATE_STARTED
#define com_waz_call_FlowManager_VIDEO_STATE_STARTED 1L
#undef com_waz_call_FlowManager_VIDEO_REASON_NORMAL
#define com_waz_call_FlowManager_VIDEO_REASON_NORMAL 0L
#undef com_waz_call_FlowManager_VIDEO_REASON_BAD_CONNECTION
#define com_waz_call_FlowManager_VIDEO_REASON_BAD_CONNECTION 1L
#undef com_waz_call_FlowManager_AUDIO_INTERRUPTION_STOPPED
#define com_waz_call_FlowManager_AUDIO_INTERRUPTION_STOPPED 0L
#undef com_waz_call_FlowManager_AUDIO_INTERRUPTION_STARTED
#define com_waz_call_FlowManager_AUDIO_INTERRUPTION_STARTED 1L
#undef com_waz_call_FlowManager_LOG_LEVEL_DEBUG
#define com_waz_call_FlowManager_LOG_LEVEL_DEBUG 0L
#undef com_waz_call_FlowManager_LOG_LEVEL_INFO
#define com_waz_call_FlowManager_LOG_LEVEL_INFO 1L
#undef com_waz_call_FlowManager_LOG_LEVEL_WARN
#define com_waz_call_FlowManager_LOG_LEVEL_WARN 2L
#undef com_waz_call_FlowManager_LOG_LEVEL_ERROR
#define com_waz_call_FlowManager_LOG_LEVEL_ERROR 3L
/*
 * Class:     com_waz_call_FlowManager
 * Method:    audioPlayDeviceChanged
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_com_waz_call_FlowManager_audioPlayDeviceChanged
  (JNIEnv *, jobject, jint);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    audioSourceDeviceChanged
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_com_waz_call_FlowManager_audioSourceDeviceChanged
  (JNIEnv *, jobject, jint);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    mediaCategoryChanged
 * Signature: (Ljava/lang/String;I)I
 */
JNIEXPORT jint JNICALL Java_com_waz_call_FlowManager_mediaCategoryChanged
  (JNIEnv *, jobject, jstring, jint);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    attach
 * Signature: (Landroid/content/Context;J)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_attach
  (JNIEnv *, jobject, jobject, jlong);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    detach
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_detach
  (JNIEnv *, jobject);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    refreshAccessToken
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_refreshAccessToken
  (JNIEnv *, jobject, jstring, jstring);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    setSelfUser
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_setSelfUser
  (JNIEnv *, jobject, jstring);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    response
 * Signature: (ILjava/lang/String;[BJ)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_response
  (JNIEnv *, jobject, jint, jstring, jbyteArray, jlong);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    event
 * Signature: (Ljava/lang/String;[B)Z
 */
JNIEXPORT jboolean JNICALL Java_com_waz_call_FlowManager_event
  (JNIEnv *, jobject, jstring, jbyteArray);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    acquireFlows
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_com_waz_call_FlowManager_acquireFlows
  (JNIEnv *, jobject, jstring, jstring);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    releaseFlowsNative
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_releaseFlowsNative
  (JNIEnv *, jobject, jstring);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    setActive
 * Signature: (Ljava/lang/String;Z)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_setActive
  (JNIEnv *, jobject, jstring, jboolean);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    addUser
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_addUser
  (JNIEnv *, jobject, jstring, jstring, jstring);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    setLogHandler
 * Signature: (Lcom/waz/log/LogHandler;)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_setLogHandler
  (JNIEnv *, jobject, jobject);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    setMute
 * Signature: (Z)I
 */
JNIEXPORT jint JNICALL Java_com_waz_call_FlowManager_setMute
  (JNIEnv *, jobject, jboolean);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    getMute
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_com_waz_call_FlowManager_getMute
  (JNIEnv *, jobject);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    setEnableLogging
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_setEnableLogging
  (JNIEnv *, jobject, jboolean);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    setEnableMetrics
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_setEnableMetrics
  (JNIEnv *, jobject, jboolean);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    setSessionId
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_setSessionId
  (JNIEnv *, jobject, jstring, jstring);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    callInterruption
 * Signature: (Ljava/lang/String;Z)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_callInterruption
  (JNIEnv *, jobject, jstring, jboolean);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    networkChanged
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_networkChanged
  (JNIEnv *, jobject);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    setLogLevel
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_setLogLevel
  (JNIEnv *, jclass, jint);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    sortConferenceParticipants
 * Signature: ([Ljava/lang/String;)[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_com_waz_call_FlowManager_sortConferenceParticipants
  (JNIEnv *, jclass, jobjectArray);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    canSendVideo
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_com_waz_call_FlowManager_canSendVideo
  (JNIEnv *, jobject, jstring);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    setVideoSendStateNative
 * Signature: (Ljava/lang/String;I)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_setVideoSendStateNative
  (JNIEnv *, jobject, jstring, jint);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    setVideoPreviewNative
 * Signature: (Ljava/lang/String;Landroid/view/View;)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_setVideoPreviewNative
  (JNIEnv *, jobject, jstring, jobject);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    setVideoView
 * Signature: (Ljava/lang/String;Ljava/lang/String;Landroid/view/View;)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_setVideoView
  (JNIEnv *, jobject, jstring, jstring, jobject);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    vmStartRecord
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_vmStartRecord
  (JNIEnv *, jobject, jstring);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    vmStopRecord
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_vmStopRecord
  (JNIEnv *, jobject);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    vmGetLength
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_waz_call_FlowManager_vmGetLength
  (JNIEnv *, jobject, jstring);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    vmStartPlay
 * Signature: (Ljava/lang/String;I)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_vmStartPlay
  (JNIEnv *, jobject, jstring, jint);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    vmStopPlay
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_vmStopPlay
  (JNIEnv *, jobject);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    vmApplyChorus
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_vmApplyChorus
  (JNIEnv *, jobject, jstring, jstring);

/*
 * Class:     com_waz_call_FlowManager
 * Method:    vmApplyReverb
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_waz_call_FlowManager_vmApplyReverb
  (JNIEnv *, jobject, jstring, jstring);

#ifdef __cplusplus
}
#endif
#endif
