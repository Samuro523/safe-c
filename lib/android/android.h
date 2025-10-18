
// android.h

//----------------------------------------------------------------------------------------
#begin unsafe
//----------------------------------------------------------------------------------------

const int EVENT_INITIALIZE   =   1;
const int EVENT_RENDER_FRAME =  10;
const int EVENT_TERMINATE    = 999;

//----------------------------------------------------------------------------------------

// must be initialized at program start
long g_assetManager;

//----------------------------------------------------------------------------------------

byte[]^ android_load_asset (string filename);
byte* android_load_asset2 (string filename, out int size);

//----------------------------------------------------------------------------------------

const int AASSET_MODE_UNKNOWN   = 0;
const int AASSET_MODE_RANDOM    = 1;
const int AASSET_MODE_STREAMING = 2;
const int AASSET_MODE_BUFFER    = 3;
  
[extern "libandroid.so"]
long AAssetManager_open (long assetManager, char* filename, int mode);

[extern "libandroid.so"]
byte* AAsset_getBuffer (long aset);

[extern "libandroid.so"]
long AAsset_getLength64 (long aset);

[extern "libandroid.so"]
void AAsset_close(long aset);

//----------------------------------------------------------------------------------------
typedef GameActivity;
//----------------------------------------------------------------------------------------

struct ARect
{
  /// Minimum X coordinate of the rectangle.
  int4 left;
  /// Minimum Y coordinate of the rectangle.
  int4 top;
  /// Maximum X coordinate of the rectangle.
  int4 right;
  /// Maximum Y coordinate of the rectangle.
  int4 bottom;
}

//----------------------------------------------------------------------------------------

typedef int4  int32_t;
typedef uint4 uint32_t;
typedef int8  int64_t;
typedef byte ANativeWindow;  // opaque type

//----------------------------------------------------------------------------------------

const int GAME_ACTIVITY_POINTER_INFO_AXIS_COUNT = 48;

struct GameActivityPointerAxes 
{
    int32_t id;
    int32_t toolType;
    float   axisValues[GAME_ACTIVITY_POINTER_INFO_AXIS_COUNT];
    float   rawX;
    float   rawY;
}

//----------------------------------------------------------------------------------------

const int GAMEACTIVITY_MAX_NUM_POINTERS_IN_MOTION_EVENT = 8;

struct GameActivityMotionEvent 
{
  int32_t deviceId;
  int32_t source;
  int32_t action;

  int64_t eventTime;  // time when event occured
  int64_t downTime;   // time when pointer was pressed down

  int32_t flags;
  int32_t metaState;

  int32_t actionButton;
  int32_t buttonState;
  int32_t classification;
  int32_t edgeFlags;     // Flags can indicate touches near the left, right, top, or bottom screen edges.

  uint32_t pointerCount;
  GameActivityPointerAxes pointers[GAMEACTIVITY_MAX_NUM_POINTERS_IN_MOTION_EVENT];

  int    historySize;
  long*  historicalEventTimes;
  float* historicalAxisValues;

  float  precisionX;
  float  precisionY;
}

//----------------------------------------------------------------------------------------

struct GameActivityKeyEvent 
{
  int32_t deviceId;
  int32_t source;
  int32_t action;

  int64_t eventTime;
  int64_t downTime;

  int32_t flags;
  int32_t metaState;

  int32_t modifiers;
  int32_t repeatCount;
  int32_t keyCode;
  int32_t scanCode;
  int32_t unicodeChar;
}

//----------------------------------------------------------------------------------------

struct android_input_buffer 
{
    /**
     * Pointer to a read-only array of GameActivityMotionEvent.
     * Only the first motionEventsCount events are valid.
     */
    GameActivityMotionEvent* motionEvents;

    /**
     * The number of valid motion events in `motionEvents`.
     */
    int8 motionEventsCount;

    /**
     * The size of the `motionEvents` buffer.
     */
    int8 motionEventsBufferSize;

    /**
     * Pointer to a read-only array of GameActivityKeyEvent.
     * Only the first keyEventsCount events are valid.
     */
    GameActivityKeyEvent*  keyEvents;

    /**
     * The number of valid "Key" events in `keyEvents`.
     */
    int8 keyEventsCount;

    /**
     * The size of the `keyEvents` buffer.
     */
    int8 keyEventsBufferSize;
}

//----------------------------------------------------------------------------------------

struct GameTextInputSpan 
{
  int32_t start;/** The start of the region (inclusive). */
  int32_t endi;/** The end of the region (exclusive). */
}

struct GameTextInputState 
{
  /**
   * Text owned by the state, as a modified UTF-8 string. Null-terminated.
   * https://en.wikipedia.org/wiki/UTF-8#Modified_UTF-8
   */
  char* text_UTF8;
  /**
   * Length in bytes of text_UTF8, *not* including the null at end.
   */
  int32_t text_length;
  /**
   * A selection defined on the text.
   */
  GameTextInputSpan selection;
  /**
   * A composing region defined on the text.
   */
  GameTextInputSpan composingRegion;
}

//----------------------------------------------------------------------------------------

typedef [callback] void SaveInstanceStateRecallback (char* bytes, int len, byte* context);

typedef [callback] void ONSTART   (GameActivity* activity);
typedef [callback] void ONRESUME  (GameActivity* activity);
typedef [callback] void ONSAVEINSTANCESTATE(GameActivity* activity, SaveInstanceStateRecallback recallback, byte* context);
typedef [callback] void ONPAUSE   (GameActivity* activity);
typedef [callback] void ONSTOP    (GameActivity* activity);
typedef [callback] void ONDESTROY(GameActivity* activity);
typedef [callback] void ONWINDOWFOCUSCHANGED  (GameActivity* activity, bool hasFocus);
typedef [callback] void ONNATIVEWINDOWCREATED (GameActivity* activity, ANativeWindow* window);
typedef [callback] void ONNATIVEWINDOWRESIZED (GameActivity* activity, ANativeWindow* window, int32_t newWidth, int32_t newHeight);
typedef [callback] void ONNATIVEWINDOWREDRAWNEEDED (GameActivity* activity, ANativeWindow* window);
typedef [callback] void ONNATIVEWINDOWDESTROYED    (GameActivity* activity, ANativeWindow* window);
typedef [callback] void ONCONFIGURATIONCHANGED     (GameActivity* activity);
typedef [callback] void ONTRIMMEMORY (GameActivity* activity, int level);
typedef [callback] bool ONTOUCHEVENT (GameActivity* activity, GameActivityMotionEvent* event);
typedef [callback] bool ONKEYDOWN    (GameActivity* activity, GameActivityKeyEvent* event);
typedef [callback] bool ONKEYUP      (GameActivity* activity, GameActivityKeyEvent* event);
typedef [callback] void ONTEXTINPUTEVENT      (GameActivity* activity, GameTextInputState* state);
typedef [callback] void ONWINDOWINSETSCHANGED (GameActivity* activity);

//----------------------------------------------------------------------------------------

struct GameActivityCallbacks 
{
  /* GameActivity has started.  See Java documentation for Activity.onStart() for more information. */
  ONSTART onStart;

  /* GameActivity has resumed.  See Java documentation for Activity.onResume() for more information. */
  ONRESUME onResume;

  /**
   * The framework is asking GameActivity to save its current instance state.
   * See the Java documentation for Activity.onSaveInstanceState() for more
   * information. The user should call the recallback with their data, its
   * length and the provided context; they retain ownership of the data. Note
   * that the saved state will be persisted, so it can not contain any active
   * entities (pointers to memory, file descriptors, etc).
   */
  ONSAVEINSTANCESTATE onSaveInstanceState;

  /* GameActivity has paused.  See Java documentation for Activity.onPause() for more information. */
  ONPAUSE onPause;

  /* GameActivity has stopped.  See Java documentation for Activity.onStop() for more information. */
  ONSTOP onStop;

  /* GameActivity is being destroyed.  See Java documentation for Activity.onDestroy() for more information. */
  ONDESTROY onDestroy;

  /* Focus has changed in this GameActivity's window.  to pause a game when it loses input focus. */
  ONWINDOWFOCUSCHANGED onWindowFocusChanged;

  /* The drawing window for this native activity has been created.  You can use the given native window object to start drawing. */
  ONNATIVEWINDOWCREATED onNativeWindowCreated;

  /* The drawing window for this native activity has been resized.  
     You should retrieve the new size from the window and ensure that your rendering in it now matches. */
  ONNATIVEWINDOWRESIZED onNativeWindowResized;

  /* The drawing window for this native activity needs to be redrawn.  
   * To avoid transient artifacts during screen changes (such resizing after rotation), 
   * applications should not return from this function until they have finished drawing their window in its current state. */
  ONNATIVEWINDOWREDRAWNEEDED onNativeWindowRedrawNeeded;

  /* The drawing window for this native activity is going to be destroyed.
   * You MUST ensure that you do not touch the window object after returning
   * from this function: in the common case of drawing to the window from
   * another thread, that means the implementation of this callback must
   * properly synchronize with the other thread to stop its drawing before
   * returning from here.
   */
  ONNATIVEWINDOWDESTROYED onNativeWindowDestroyed;

  /* The current device AConfiguration has changed.  The new configuration can be retrieved from assetManager. */
  ONCONFIGURATIONCHANGED onConfigurationChanged;

  /* The system is running low on memory.  Use this callback to release resources you do not need, 
   * to help the system avoid killing more important processes. */
  ONTRIMMEMORY onTrimMemory;

  /* Callback called for every MotionEvent done on the GameActivity SurfaceView. 
   * Ownership of `event` is maintained by the library and it is only valid during the callback. */
  ONTOUCHEVENT onTouchEvent;

  /* Callback called for every key down event on the GameActivity SurfaceView.
   * Ownership of `event` is maintained by the library and it is only valid during the callback. */
  ONKEYDOWN onKeyDown;

  /* Callback called for every key up event on the GameActivity SurfaceView.
   * Ownership of `event` is maintained by the library and it is only valid during the callback. */
  ONKEYUP onKeyUp;

  /* Callback called for every soft-keyboard text input event.
   * Ownership of `state` is maintained by the library and it is only valid during the callback. */
  ONTEXTINPUTEVENT onTextInputEvent;

  /* Callback called when WindowInsets of the main app window have changed.
   * Call GameActivity_getWindowInsets to retrieve the insets themselves. */
  ONWINDOWINSETSCHANGED onWindowInsetsChanged;
}

//----------------------------------------------------------------------------------------

typedef JavaVM;
typedef JNIEnv;
typedef int jint;
typedef byte* jclass;
typedef byte* jmethodID;
typedef byte* jobject;
typedef byte* jstring;
typedef int jboolean;

typedef jint ATTACHCURRENTTHREAD (JavaVM* javavm, JNIEnv** jnienv, byte* extra);
typedef jint DETACHCURRENTTREAD  (JavaVM* javavm);

struct JNIInvokeInterface
{
  byte*       reserved0;
  byte*       reserved1;
  byte*       reserved2;
  byte*v3;   // jint        (*DestroyJavaVM)(JavaVM*);
  ATTACHCURRENTTHREAD AttachCurrentThread;
  DETACHCURRENTTREAD  DetachCurrentThread;
  byte*v6;   // jint        (*GetEnv)(JavaVM*, byte**, jint);
  byte*v7;   // jint        (*AttachCurrentThreadAsDaemon)(JavaVM*, JNIEnv**, byte*);
}

//---------------------------------------------------------------------

typedef JNIInvokeInterface* JavaVM;

//---------------------------------------------------------------------

typedef jclass    FINDCLASS   (JNIEnv* jnienv, char* name);
typedef jmethodID GETMETHODID (JNIEnv* jnienv, jclass jc, char* name1, char* name2);
typedef jobject   CALLOBJECTMETHOD (JNIEnv* jnienv, jobject jo, jmethodID jm);

typedef jmethodID GETSTATICMETHODID (JNIEnv* jnienv, jclass jc, char* name1, char* name2);
typedef jobject   CALLSTATICOBJECTMETHOD (JNIEnv* jnienv, jclass jc, jmethodID jm);

typedef char* GETSTRINGUTFCHARS (JNIEnv* jnienv, jstring js, jboolean* jb);
typedef void  RELEASESTRINGUTFCHARS (JNIEnv* jnienv, jstring js, char* str);


struct JNINativeInterface
{
  byte*       reserved0;
  byte*       reserved1;
  byte*       reserved2;
  byte*       reserved3;
  byte*v4;   // jint        (*GetVersion)(JNIEnv *);
  byte*v5;   // jclass      (*DefineClass)(JNIEnv*, const char*, jobject, const jbyte*, jsize);

  FINDCLASS FindClass;

  byte*v7;   // jmethodID   (*FromReflectedMethod)(JNIEnv*, jobject);
  byte*v8;   // jfieldID    (*FromReflectedField)(JNIEnv*, jobject);
    /* spec doesn't show jboolean parameter */
  byte*v9;   // jobject     (*ToReflectedMethod)(JNIEnv*, jclass, jmethodID, jboolean);
  byte*v10;   // jclass      (*GetSuperclass)(JNIEnv*, jclass);
  byte*v11;   // jboolean    (*IsAssignableFrom)(JNIEnv*, jclass, jclass);

    /* spec doesn't show jboolean parameter */
  byte*v12;   // jobject     (*ToReflectedField)(JNIEnv*, jclass, jfieldID, jboolean);
  byte*v13;   // jint        (*Throw)(JNIEnv*, jthrowable);
  byte*v14;   // jint        (*ThrowNew)(JNIEnv *, jclass, const char *);
  byte*v15;   // jthrowable  (*ExceptionOccurred)(JNIEnv*);
  byte*v16;   // void        (*ExceptionDescribe)(JNIEnv*);
  byte*v17;   //    void        (*ExceptionClear)(JNIEnv*);
  byte*v18;   //    void        (*FatalError)(JNIEnv*, const char*);

  byte*v19;   //    jint        (*PushLocalFrame)(JNIEnv*, jint);
  byte*v20;   //    jobject     (*PopLocalFrame)(JNIEnv*, jobject);

  byte*v21;   //    jobject     (*NewGlobalRef)(JNIEnv*, jobject);
  byte*v22;   //    void        (*DeleteGlobalRef)(JNIEnv*, jobject);
  byte*v23;   //    void        (*DeleteLocalRef)(JNIEnv*, jobject);
  byte*v24;   //    jboolean    (*IsSameObject)(JNIEnv*, jobject, jobject);

  byte*v25;   //    jobject     (*NewLocalRef)(JNIEnv*, jobject);
  byte*v26;   //    jint        (*EnsureLocalCapacity)(JNIEnv*, jint);

  byte*v27;   //    jobject     (*AllocObject)(JNIEnv*, jclass);
  byte*v28;   //    jobject     (*NewObject)(JNIEnv*, jclass, jmethodID, ...);
  byte*v29;   //    jobject     (*NewObjectV)(JNIEnv*, jclass, jmethodID, va_list);
  byte*v30;   //    jobject     (*NewObjectA)(JNIEnv*, jclass, jmethodID, const jvalue*);

  byte*v31;   //    jclass      (*GetObjectClass)(JNIEnv*, jobject);
  byte*v32;   //    jboolean    (*IsInstanceOf)(JNIEnv*, jobject, jclass);

  GETMETHODID GetMethodID;
  CALLOBJECTMETHOD CallObjectMethod;

  byte*v35;   //    jobject     (*CallObjectMethodV)(JNIEnv*, jobject, jmethodID, va_list);
  byte*v36;   //    jobject     (*CallObjectMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
  byte*v37;   //    jboolean    (*CallBooleanMethod)(JNIEnv*, jobject, jmethodID, ...);
  byte*v38;   //    jboolean    (*CallBooleanMethodV)(JNIEnv*, jobject, jmethodID, va_list);
  byte*v39;   //    jboolean    (*CallBooleanMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
  byte*v40;   //    jbyte       (*CallByteMethod)(JNIEnv*, jobject, jmethodID, ...);
  byte*v41;   //    jbyte       (*CallByteMethodV)(JNIEnv*, jobject, jmethodID, va_list);
  byte*v42;   //    jbyte       (*CallByteMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
  byte*v43;   //    jchar       (*CallCharMethod)(JNIEnv*, jobject, jmethodID, ...);
  byte*v44;   //    jchar       (*CallCharMethodV)(JNIEnv*, jobject, jmethodID, va_list);
  byte*v45;   //    jchar       (*CallCharMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
  byte*v46;   //    jshort      (*CallShortMethod)(JNIEnv*, jobject, jmethodID, ...);
  byte*v47;   //    jshort      (*CallShortMethodV)(JNIEnv*, jobject, jmethodID, va_list);
  byte*v48;   //    jshort      (*CallShortMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
  byte*v49;   //    jint        (*CallIntMethod)(JNIEnv*, jobject, jmethodID, ...);
  byte*v50;   //    jint        (*CallIntMethodV)(JNIEnv*, jobject, jmethodID, va_list);
  byte*v51;   //    jint        (*CallIntMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
  byte*v52;   //    jlong       (*CallLongMethod)(JNIEnv*, jobject, jmethodID, ...);
  byte*v53;   //    jlong       (*CallLongMethodV)(JNIEnv*, jobject, jmethodID, va_list);
  byte*v54;   //    jlong       (*CallLongMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
  byte*v55;   //    jfloat      (*CallFloatMethod)(JNIEnv*, jobject, jmethodID, ...);
  byte*v56;   //    jfloat      (*CallFloatMethodV)(JNIEnv*, jobject, jmethodID, va_list);
  byte*v57;   //    jfloat      (*CallFloatMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
  byte*v58;   //    jdouble     (*CallDoubleMethod)(JNIEnv*, jobject, jmethodID, ...);
  byte*v59;   //    jdouble     (*CallDoubleMethodV)(JNIEnv*, jobject, jmethodID, va_list);
  byte*v60;   //    jdouble     (*CallDoubleMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
  byte*v61;   //    void        (*CallVoidMethod)(JNIEnv*, jobject, jmethodID, ...);
  byte*v62;   //    void        (*CallVoidMethodV)(JNIEnv*, jobject, jmethodID, va_list);
  byte*v63;   //    void        (*CallVoidMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
  byte*v64;   //    jobject     (*CallNonvirtualObjectMethod)(JNIEnv*, jobject, jclass,  jmethodID, ...);
  byte*v65;   //    jobject     (*CallNonvirtualObjectMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
  byte*v66;   //    jobject     (*CallNonvirtualObjectMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
  byte*v67;   //    jboolean    (*CallNonvirtualBooleanMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
  byte*v68;   //    jboolean    (*CallNonvirtualBooleanMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
  byte*v69;   //    jboolean    (*CallNonvirtualBooleanMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
  byte*v70;   //    jbyte       (*CallNonvirtualByteMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
  byte*v71;   //    jbyte       (*CallNonvirtualByteMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
  byte*v72;   //    jbyte       (*CallNonvirtualByteMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
  byte*v73;   //    jchar       (*CallNonvirtualCharMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
  byte*v74;   //    jchar       (*CallNonvirtualCharMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
  byte*v75;   //    jchar       (*CallNonvirtualCharMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
  byte*v76;   //    jshort      (*CallNonvirtualShortMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
  byte*v77;   //    jshort      (*CallNonvirtualShortMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
  byte*v78;   //    jshort      (*CallNonvirtualShortMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
  byte*v79;   //    jint        (*CallNonvirtualIntMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
  byte*v80;   //    jint        (*CallNonvirtualIntMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
  byte*v81;   //    jint        (*CallNonvirtualIntMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
  byte*v82;   //    jlong       (*CallNonvirtualLongMethod)(JNIEnv*, jobject, jclass,  jmethodID, ...);
  byte*v83;   //    jlong       (*CallNonvirtualLongMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
  byte*v84;   //    jlong       (*CallNonvirtualLongMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
  byte*v85;   //    jfloat      (*CallNonvirtualFloatMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
  byte*v86;   //    jfloat      (*CallNonvirtualFloatMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
  byte*v87;   //    jfloat      (*CallNonvirtualFloatMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
  byte*v88;   //    jdouble     (*CallNonvirtualDoubleMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
  byte*v89;   //    jdouble     (*CallNonvirtualDoubleMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
  byte*v90;   //    jdouble     (*CallNonvirtualDoubleMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
  byte*v91;   //    void        (*CallNonvirtualVoidMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
  byte*v92;   //    void        (*CallNonvirtualVoidMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
  byte*v93;   //    void        (*CallNonvirtualVoidMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
  byte*v94;   //    jfieldID    (*GetFieldID)(JNIEnv*, jclass, const char*, const char*);
  byte*v95;   //    jobject     (*GetObjectField)(JNIEnv*, jobject, jfieldID);
  byte*v96;   //    jboolean    (*GetBooleanField)(JNIEnv*, jobject, jfieldID);
  byte*v97;   //    jbyte       (*GetByteField)(JNIEnv*, jobject, jfieldID);
  byte*v98;   //    jchar       (*GetCharField)(JNIEnv*, jobject, jfieldID);
  byte*v99;   //    jshort      (*GetShortField)(JNIEnv*, jobject, jfieldID);
  byte*v100;   //    jint        (*GetIntField)(JNIEnv*, jobject, jfieldID);
  byte*v101;   //    jlong       (*GetLongField)(JNIEnv*, jobject, jfieldID);
  byte*v102;   //    jfloat      (*GetFloatField)(JNIEnv*, jobject, jfieldID);
  byte*v103;   //    jdouble     (*GetDoubleField)(JNIEnv*, jobject, jfieldID);
  byte*v104;   //    void        (*SetObjectField)(JNIEnv*, jobject, jfieldID, jobject);
  byte*v105;   //    void        (*SetBooleanField)(JNIEnv*, jobject, jfieldID, jboolean);
  byte*v106;   //    void        (*SetByteField)(JNIEnv*, jobject, jfieldID, jbyte);
  byte*v107;   //    void        (*SetCharField)(JNIEnv*, jobject, jfieldID, jchar);
  byte*v108;   //    void        (*SetShortField)(JNIEnv*, jobject, jfieldID, jshort);
  byte*v109;   //    void        (*SetIntField)(JNIEnv*, jobject, jfieldID, jint);
  byte*v110;   //    void        (*SetLongField)(JNIEnv*, jobject, jfieldID, jlong);
  byte*v111;   //    void        (*SetFloatField)(JNIEnv*, jobject, jfieldID, jfloat);
  byte*v112;   //    void        (*SetDoubleField)(JNIEnv*, jobject, jfieldID, jdouble);

  GETSTATICMETHODID GetStaticMethodID;
  CALLSTATICOBJECTMETHOD CallStaticObjectMethod;

  byte*v115;   //    jobject     (*CallStaticObjectMethodV)(JNIEnv*, jclass, jmethodID, va_list);
  byte*v116;   //    jobject     (*CallStaticObjectMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
  byte*v117;   //    jboolean    (*CallStaticBooleanMethod)(JNIEnv*, jclass, jmethodID, ...);
  byte*v118;   //    jboolean    (*CallStaticBooleanMethodV)(JNIEnv*, jclass, jmethodID, va_list);
  byte*v119;   //    jboolean    (*CallStaticBooleanMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
  byte*v120;   //    jbyte       (*CallStaticByteMethod)(JNIEnv*, jclass, jmethodID, ...);
  byte*v121;   //    jbyte       (*CallStaticByteMethodV)(JNIEnv*, jclass, jmethodID, va_list);
  byte*v122;   //    jbyte       (*CallStaticByteMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
  byte*v123;   //    jchar       (*CallStaticCharMethod)(JNIEnv*, jclass, jmethodID, ...);
  byte*v124;   //    jchar       (*CallStaticCharMethodV)(JNIEnv*, jclass, jmethodID, va_list);
  byte*v125;   //    jchar       (*CallStaticCharMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
  byte*v126;   //    jshort      (*CallStaticShortMethod)(JNIEnv*, jclass, jmethodID, ...);
  byte*v127;   //    jshort      (*CallStaticShortMethodV)(JNIEnv*, jclass, jmethodID, va_list);
  byte*v128;   //    jshort      (*CallStaticShortMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
  byte*v129;   //    jint        (*CallStaticIntMethod)(JNIEnv*, jclass, jmethodID, ...);
  byte*v130;   //    jint        (*CallStaticIntMethodV)(JNIEnv*, jclass, jmethodID, va_list);
  byte*v131;   //    jint        (*CallStaticIntMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
  byte*v132;   //    jlong       (*CallStaticLongMethod)(JNIEnv*, jclass, jmethodID, ...);
  byte*v133;   //    jlong       (*CallStaticLongMethodV)(JNIEnv*, jclass, jmethodID, va_list);
  byte*v134;   //    jlong       (*CallStaticLongMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
  byte*v135;   //    jfloat      (*CallStaticFloatMethod)(JNIEnv*, jclass, jmethodID, ...);
  byte*v136;   //    jfloat      (*CallStaticFloatMethodV)(JNIEnv*, jclass, jmethodID, va_list);
  byte*v137;   //    jfloat      (*CallStaticFloatMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
  byte*v138;   //    jdouble     (*CallStaticDoubleMethod)(JNIEnv*, jclass, jmethodID, ...);
  byte*v139;   //    jdouble     (*CallStaticDoubleMethodV)(JNIEnv*, jclass, jmethodID, va_list);
  byte*v140;   //    jdouble     (*CallStaticDoubleMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
  byte*v141;   //    void        (*CallStaticVoidMethod)(JNIEnv*, jclass, jmethodID, ...);
  byte*v142;   //    void        (*CallStaticVoidMethodV)(JNIEnv*, jclass, jmethodID, va_list);
  byte*v143;   //    void        (*CallStaticVoidMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
  byte*v144;   //    jfieldID    (*GetStaticFieldID)(JNIEnv*, jclass, const char*, const char*);
  byte*v145;   //    jobject     (*GetStaticObjectField)(JNIEnv*, jclass, jfieldID);
  byte*v146;   //    jboolean    (*GetStaticBooleanField)(JNIEnv*, jclass, jfieldID);
  byte*v147;   //    jbyte       (*GetStaticByteField)(JNIEnv*, jclass, jfieldID);
  byte*v148;   //    jchar       (*GetStaticCharField)(JNIEnv*, jclass, jfieldID);
  byte*v149;   //    jshort      (*GetStaticShortField)(JNIEnv*, jclass, jfieldID);
  byte*v150;   //    jint        (*GetStaticIntField)(JNIEnv*, jclass, jfieldID);
  byte*v151;   //    jlong       (*GetStaticLongField)(JNIEnv*, jclass, jfieldID);
  byte*v152;   //    jfloat      (*GetStaticFloatField)(JNIEnv*, jclass, jfieldID);
  byte*v153;   //    jdouble     (*GetStaticDoubleField)(JNIEnv*, jclass, jfieldID);
  byte*v154;   //    void        (*SetStaticObjectField)(JNIEnv*, jclass, jfieldID, jobject);
  byte*v155;   //    void        (*SetStaticBooleanField)(JNIEnv*, jclass, jfieldID, jboolean);
  byte*v156;   //    void        (*SetStaticByteField)(JNIEnv*, jclass, jfieldID, jbyte);
  byte*v157;   //    void        (*SetStaticCharField)(JNIEnv*, jclass, jfieldID, jchar);
  byte*v158;   //    void        (*SetStaticShortField)(JNIEnv*, jclass, jfieldID, jshort);
  byte*v159;   //    void        (*SetStaticIntField)(JNIEnv*, jclass, jfieldID, jint);
  byte*v160;   //    void        (*SetStaticLongField)(JNIEnv*, jclass, jfieldID, jlong);
  byte*v161;   //    void        (*SetStaticFloatField)(JNIEnv*, jclass, jfieldID, jfloat);
  byte*v162;   //    void        (*SetStaticDoubleField)(JNIEnv*, jclass, jfieldID, jdouble);
  byte*v163;   //    jstring     (*NewString)(JNIEnv*, const jchar*, jsize);
  byte*v164;   //    jsize       (*GetStringLength)(JNIEnv*, jstring);
  byte*v165;   //    const jchar* (*GetStringChars)(JNIEnv*, jstring, jboolean*);
  byte*v166;   //    void        (*ReleaseStringChars)(JNIEnv*, jstring, const jchar*);
  byte*v167;   //    jstring     (*NewStringUTF)(JNIEnv*, const char*);
  byte*v168;   //    jsize       (*GetStringUTFLength)(JNIEnv*, jstring);

    /* JNI spec says this returns const jbyte*, but that's inconsistent */
  GETSTRINGUTFCHARS GetStringUTFChars;
  RELEASESTRINGUTFCHARS ReleaseStringUTFChars;


#if 0
    jsize       (*GetArrayLength)(JNIEnv*, jarray);
    jobjectArray (*NewObjectArray)(JNIEnv*, jsize, jclass, jobject);
    jobject     (*GetObjectArrayElement)(JNIEnv*, jobjectArray, jsize);
    void        (*SetObjectArrayElement)(JNIEnv*, jobjectArray, jsize, jobject);

    jbooleanArray (*NewBooleanArray)(JNIEnv*, jsize);
    jbyteArray    (*NewByteArray)(JNIEnv*, jsize);
    jcharArray    (*NewCharArray)(JNIEnv*, jsize);
    jshortArray   (*NewShortArray)(JNIEnv*, jsize);
    jintArray     (*NewIntArray)(JNIEnv*, jsize);
    jlongArray    (*NewLongArray)(JNIEnv*, jsize);
    jfloatArray   (*NewFloatArray)(JNIEnv*, jsize);
    jdoubleArray  (*NewDoubleArray)(JNIEnv*, jsize);

    jboolean*   (*GetBooleanArrayElements)(JNIEnv*, jbooleanArray, jboolean*);
    jbyte*      (*GetByteArrayElements)(JNIEnv*, jbyteArray, jboolean*);
    jchar*      (*GetCharArrayElements)(JNIEnv*, jcharArray, jboolean*);
    jshort*     (*GetShortArrayElements)(JNIEnv*, jshortArray, jboolean*);
    jint*       (*GetIntArrayElements)(JNIEnv*, jintArray, jboolean*);
    jlong*      (*GetLongArrayElements)(JNIEnv*, jlongArray, jboolean*);
    jfloat*     (*GetFloatArrayElements)(JNIEnv*, jfloatArray, jboolean*);
    jdouble*    (*GetDoubleArrayElements)(JNIEnv*, jdoubleArray, jboolean*);

    void        (*ReleaseBooleanArrayElements)(JNIEnv*, jbooleanArray,
                        jboolean*, jint);
    void        (*ReleaseByteArrayElements)(JNIEnv*, jbyteArray,
                        jbyte*, jint);
    void        (*ReleaseCharArrayElements)(JNIEnv*, jcharArray,
                        jchar*, jint);
    void        (*ReleaseShortArrayElements)(JNIEnv*, jshortArray,
                        jshort*, jint);
    void        (*ReleaseIntArrayElements)(JNIEnv*, jintArray,
                        jint*, jint);
    void        (*ReleaseLongArrayElements)(JNIEnv*, jlongArray,
                        jlong*, jint);
    void        (*ReleaseFloatArrayElements)(JNIEnv*, jfloatArray,
                        jfloat*, jint);
    void        (*ReleaseDoubleArrayElements)(JNIEnv*, jdoubleArray,
                        jdouble*, jint);

    void        (*GetBooleanArrayRegion)(JNIEnv*, jbooleanArray,
                        jsize, jsize, jboolean*);
    void        (*GetByteArrayRegion)(JNIEnv*, jbyteArray,
                        jsize, jsize, jbyte*);
    void        (*GetCharArrayRegion)(JNIEnv*, jcharArray,
                        jsize, jsize, jchar*);
    void        (*GetShortArrayRegion)(JNIEnv*, jshortArray,
                        jsize, jsize, jshort*);
    void        (*GetIntArrayRegion)(JNIEnv*, jintArray,
                        jsize, jsize, jint*);
    void        (*GetLongArrayRegion)(JNIEnv*, jlongArray,
                        jsize, jsize, jlong*);
    void        (*GetFloatArrayRegion)(JNIEnv*, jfloatArray,
                        jsize, jsize, jfloat*);
    void        (*GetDoubleArrayRegion)(JNIEnv*, jdoubleArray,
                        jsize, jsize, jdouble*);

    /* spec shows these without const; some jni.h do, some don't */
    void        (*SetBooleanArrayRegion)(JNIEnv*, jbooleanArray,
                        jsize, jsize, const jboolean*);
    void        (*SetByteArrayRegion)(JNIEnv*, jbyteArray,
                        jsize, jsize, const jbyte*);
    void        (*SetCharArrayRegion)(JNIEnv*, jcharArray,
                        jsize, jsize, const jchar*);
    void        (*SetShortArrayRegion)(JNIEnv*, jshortArray,
                        jsize, jsize, const jshort*);
    void        (*SetIntArrayRegion)(JNIEnv*, jintArray,
                        jsize, jsize, const jint*);
    void        (*SetLongArrayRegion)(JNIEnv*, jlongArray,
                        jsize, jsize, const jlong*);
    void        (*SetFloatArrayRegion)(JNIEnv*, jfloatArray,
                        jsize, jsize, const jfloat*);
    void        (*SetDoubleArrayRegion)(JNIEnv*, jdoubleArray,
                        jsize, jsize, const jdouble*);

    jint        (*RegisterNatives)(JNIEnv*, jclass, const JNINativeMethod*,
                        jint);
    jint        (*UnregisterNatives)(JNIEnv*, jclass);
    jint        (*MonitorEnter)(JNIEnv*, jobject);
    jint        (*MonitorExit)(JNIEnv*, jobject);
    jint        (*GetJavaVM)(JNIEnv*, JavaVM**);

    void        (*GetStringRegion)(JNIEnv*, jstring, jsize, jsize, jchar*);
    void        (*GetStringUTFRegion)(JNIEnv*, jstring, jsize, jsize, char*);

    byte*       (*GetPrimitiveArrayCritical)(JNIEnv*, jarray, jboolean*);
    void        (*ReleasePrimitiveArrayCritical)(JNIEnv*, jarray, byte*, jint);

    const jchar* (*GetStringCritical)(JNIEnv*, jstring, jboolean*);
    void        (*ReleaseStringCritical)(JNIEnv*, jstring, const jchar*);

    jweak       (*NewWeakGlobalRef)(JNIEnv*, jobject);
    void        (*DeleteWeakGlobalRef)(JNIEnv*, jweak);

    jboolean    (*ExceptionCheck)(JNIEnv*);

    jobject     (*NewDirectByteBuffer)(JNIEnv*, byte*, jlong);
    byte*       (*GetDirectBufferAddress)(JNIEnv*, jobject);
    jlong       (*GetDirectBufferCapacity)(JNIEnv*, jobject);

    /* added in JNI 1.6 */
    jobjectRefType (*GetObjectRefType)(JNIEnv*, jobject);
#endif
}

typedef JNINativeInterface* JNIEnv;

//---------------------------------------------------------------------

/**
 * This structure defines the native side of an android.app.GameActivity.
 * It is created by the framework, and handed to the application's native
 * code as it is being launched.
 */

struct GameActivity
{
  /**
   * Pointer to the callback function table of the native application.
   * You can set the functions here to your own callbacks.  The callbacks
   * pointer itself here should not be changed; it is allocated and managed
   * for you by the framework.
   */
  GameActivityCallbacks* callbacks;

  /**
   * The global handle on the process's Java VM.
   */
  JavaVM* vm;

  /**
   * JNI context for the main thread of the app.  Note that this field
   * can ONLY be used from the main thread of the process; that is, the
   * thread that calls into the GameActivityCallbacks.
   */
  JNIEnv* env;

  /**
   * The GameActivity object handle.
   */
  byte* javaGameActivity;    // *jobject

  /**
   * Path to this application's internal data directory.
   */
  char* internalDataPath;

  /**
   * Path to this application's external (removable/mountable) data directory.
   */
  char* externalDataPath;

  /**
   * The platform's SDK version code.
   */
  int4 sdkVersion;

  /**
   * This is the native instance of the application.  It is not used by
   * the framework, but can be set by the application to its own instance
   * state.
   */
  byte* instance;

  /**
   * Pointer to the Asset Manager instance for the application.  The
   * application uses this to access binary assets bundled inside its own .apk
   * file.
   */
  long  assetManager;   // AAssetManager

  /**
   * Available starting with Honeycomb: path to the directory containing
   * the application's OBB files (if any).  If the app doesn't have any
   * OBB files, this directory may not exist.
   */
  char* obbPath;
}

//----------------------------------------------------------------------------------------
typedef android_app;
typedef android_poll_source;
typedef [callback] void PROCESS (android_app* app, android_poll_source* source);

typedef [callback] bool android_key_event_filter (GameActivityKeyEvent* event);
typedef [callback] bool android_motion_event_filter (GameActivityMotionEvent* event);
//----------------------------------------------------------------------------------------

struct android_poll_source 
{
    /**
     * The identifier of this source.  May be LOOPER_ID_MAIN or
     * LOOPER_ID_INPUT.
     */
    int4 id;

    /** The android_app this ident is associated with. */
    android_app* app;

    /**
     * Function to call to perform the standard processing of data from
     * this source.
     */
    PROCESS process;
}

//----------------------------------------------------------------------------------------

struct android_app
{
  /**
   * An optional pointer to application-defined state.
   */
  byte* userData;

  /**
   * A required callback for processing main app commands (`APP_CMD_*`).
   * This is called each frame if there are app commands that need processing.
   */
  byte*onAppCmd; // void (*onAppCmd)(struct android_app* app, int32_t cmd);

  /** The GameActivity object instance that this app is running in. */
  GameActivity* activity;

  /** The current configuration the app is running in. */
  byte* config;  // AConfiguration

  /**
   * The last activity saved state, as provided at creation time.
   * It is NULL if there was no state.  You can use this as you need; the
   * memory will remain around until you call android_app_exec_cmd() for
   * APP_CMD_RESUME, at which point it will be freed and savedState set to
   * NULL. These variables should only be changed when processing a
   * APP_CMD_SAVE_STATE, at which point they will be initialized to NULL and
   * you can malloc your state and place the information here.  In that case
   * the memory will be freed for you later.
   */
  byte* savedState;

  /**
   * The size of the activity saved state. It is 0 if `savedState` is NULL.
   */
  int8 savedStateSize;

  /** The ALooper associated with the app's thread. */
  byte* looper;   // ALooper

  /** When non-NULL, this is the window surface that the app can draw in. */
  byte* window;  // ANativeWindow

  /**
   * Current content rectangle of the window; this is the area where the
   * window's content should be placed to be seen by the user.
   */
  ARect contentRect;

  /**
   * Current state of the app's activity.  May be either APP_CMD_START,
   * APP_CMD_RESUME, APP_CMD_PAUSE, or APP_CMD_STOP.
   */
  int activityState;

  /**
   * This is non-zero when the application's GameActivity is being
   * destroyed and waiting for the app thread to complete.
   */
  int destroyRequested;

  /**
   * This is used for buffering input from GameActivity. Once ready, the
   * application thread switches the buffers and processes what was
   * accumulated.
   */
  android_input_buffer inputBuffers[2];

  int currentInputBuffer;

  /**
   * 0 if no text input event is outstanding, 1 if it is.
   * Use `GameActivity_getTextInputState` to get information
   * about the text entered by the user.
   */
  int textInputState;

  // Below are "private" implementation of the glue code.
  /** @cond INTERNAL */

  long mutex;  // pthread_mutex_t
  long cond;   // pthread_cond_t

  int msgread;
  int msgwrite;

  long thread;  // pthread_t

  android_poll_source cmdPollSource;

  int running;
  int stateSaved;
  int destroyed;
  int redrawNeeded;
  
  byte* pendingWindow;       // ANativeWindow
  ARect pendingContentRect;

  android_key_event_filter keyEventFilter;
  android_motion_event_filter motionEventFilter;
}

//----------------------------------------------------------------------------------------

void init_android (android_app *app);

android_app *g_app;

//----------------------------------------------------------------------------------------
#end unsafe
//----------------------------------------------------------------------------------------
