/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sstream>
#include <string>
#include <iostream>

#include "clearcut_backend.h"
#include "clearcutserializer.h"
#include "uploadthread.h"

namespace tuningfork {

ClearcutBackend::~ClearcutBackend() {}

const std::string ClearcutBackend::LOG_SOURCE = "TUNING_FORK";
const char* ClearcutBackend::LOG_TAG = "TuningFork.Clearcut";

bool ClearcutBackend::Process(const ProtobufSerialization &evt_ser) {
    JNIEnv* env;
    //Attach thread
    int envStatus  = vm_->GetEnv((void**)&env, JNI_VERSION_1_6);

    switch(envStatus) {
        case JNI_OK:
            break;
        case JNI_EVERSION:
            __android_log_print(
                    ANDROID_LOG_WARN,
                    LOG_TAG, "JNI Version is not supported, status : %d", envStatus);
            return false;
        case JNI_EDETACHED: {
            int attachStatus = vm_->AttachCurrentThread(&env, (void *) NULL);
            if (attachStatus != JNI_OK) {
                __android_log_print(
                        ANDROID_LOG_WARN,
                        LOG_TAG,
                        "Thread is not attached, status : %d", attachStatus);
                return false;
            }
        }
            break;
        default:
            __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "JNIEnv is not OK, status : %d", envStatus);
            return false;
    }

    //Cast to jbytearray
    jsize length = evt_ser.size();
    jbyteArray  output = env->NewByteArray(length);
    env->SetByteArrayRegion(output, 0, length, reinterpret_cast<const jbyte *>(evt_ser.data()));

    //Send to Clearcut
    jobject newBuilder = env->CallObjectMethod(clearcut_logger_, new_event_, output);
    env->CallVoidMethod(newBuilder, log_method_);
    bool hasException = CheckException(env);

    // Detach thread.
    vm_->DetachCurrentThread();
    __android_log_print(
            ANDROID_LOG_INFO,
            LOG_TAG,
            "Message was sent to clearcut");
    return !hasException;
}

bool ClearcutBackend::Init(JNIEnv *env, jobject activity) {
    env->GetJavaVM(&vm_);
    if(vm_ == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", "JavaVM is null...");
        return false;
    }

    try {
        bool inited = InitWithClearcut(env, activity, false);
        __android_log_print(
            ANDROID_LOG_INFO,
            LOG_TAG,
            "Clearcut status: %s available",
            inited ? "" : "not");
        return  inited;
    } catch (const std::exception& e) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Clearcut status: not available");
        return false;
    }

}

bool ClearcutBackend::GetFidelityParams(ProtobufSerialization &fp_ser, size_t timeout_ms) {
    return true;
}

bool ClearcutBackend::IsGooglePlayServiceAvailable(JNIEnv* env, jobject context) {
    jclass availabilityClass =
            env->FindClass("com/google/android/gms/common/GoogleApiAvailability");
    if(CheckException(env)) return false;

    jmethodID getInstanceMethod = env->GetStaticMethodID(
        availabilityClass,
        "getInstance",
        "()Lcom/google/android/gms/common/GoogleApiAvailability;");
    if(CheckException(env)) return false;

    jobject availabilityInstance = env->CallStaticObjectMethod(
        availabilityClass,
        getInstanceMethod);
    if(CheckException(env)) return false;

    jmethodID isAvailableMethod = env->GetMethodID(
        availabilityClass,
        "isGooglePlayServicesAvailable",
        "(Landroid/content/Context;)I");
    if(CheckException(env)) return false;

    jint jresult = env->CallIntMethod(availabilityInstance, isAvailableMethod, context);
    if(CheckException(env)) return false;

    int result = reinterpret_cast<int>(jresult);

     __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Google Play Services status : %d", result);

    if(result == 0) {
         jfieldID  versionField =
            env->GetStaticFieldID(availabilityClass, "GOOGLE_PLAY_SERVICES_VERSION_CODE", "I");
        if(CheckException(env)) return false;

        jint versionCode = env->GetStaticIntField(availabilityClass, versionField);
        if(CheckException(env)) return false;

        __android_log_print(
            ANDROID_LOG_INFO,
            LOG_TAG,
            "Google Play Services version : %d",
            versionCode);
    }

    return result == 0;
}

bool ClearcutBackend::CheckException(JNIEnv *env) {
    if(env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }
    return false;
}

bool ClearcutBackend::InitWithClearcut(JNIEnv* env, jobject activity, bool anonymousLogging) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Start searching for clearcut...");

    // Get Application Context
    jclass activityClass = env->GetObjectClass(activity);
    if (CheckException(env)) return false;
    jmethodID getContext = env->GetMethodID(
            activityClass,
            "getApplicationContext",
            "()Landroid/content/Context;");
    if (CheckException(env)) return false;
    jobject context = env->CallObjectMethod(activity, getContext);

    //Check if Google Play Services are available
    bool available = IsGooglePlayServiceAvailable(env, context);
    if (!available) {
        __android_log_print(
                ANDROID_LOG_WARN, LOG_TAG,
                "Google Play Service is not available");
        return false;
    }

    // Searching for  classes
    jclass loggerClass = env->FindClass("com/google/android/gms/clearcut/ClearcutLogger");
    if (CheckException(env)) return false;
    jclass stringClass = env->FindClass("java/lang/String");
    if (CheckException(env)) return false;
    jclass builderClass = env->FindClass(
            "com/google/android/gms/clearcut/ClearcutLogger$LogEventBuilder");
    if (CheckException(env)) return false;

    //Searching for all methods
    log_method_ = env->GetMethodID(builderClass, "log", "()V");
    if (CheckException(env)) return false;
    new_event_ = env->GetMethodID(
            loggerClass,
            "newEvent",
            "([B)Lcom/google/android/gms/clearcut/ClearcutLogger$LogEventBuilder;");
    if (CheckException(env)) return false;

    jmethodID anonymousLogger = env->GetStaticMethodID(
            loggerClass,
            "anonymousLogger",
            "(Landroid/content/Context;"
            "Ljava/lang/String;)"
            "Lcom/google/android/gms/clearcut/ClearcutLogger;");
    if (CheckException(env)) return false;

    jmethodID loggerConstructor = env->GetMethodID(
            loggerClass,
            "<init>",
            "(Landroid/content/Context;"
            "Ljava/lang/String;"
            "Ljava/lang/String;)"
            "V");
    if (CheckException(env)) return false;

    //Create logger type
    jstring ccName = env->NewStringUTF(LOG_SOURCE.c_str());
    if (CheckException(env)) return false;

    //Create logger instance
    jobject localClearcutLogger;
    if (anonymousLogging) {
        localClearcutLogger = env->CallStaticObjectMethod(loggerClass, anonymousLogger, context,
                                                          ccName);
    } else {
        localClearcutLogger = env->NewObject(loggerClass, loggerConstructor, context, ccName, NULL);
    }
    if (CheckException(env)) return false;

    clearcut_logger_ = reinterpret_cast<jobject>(env->NewGlobalRef(localClearcutLogger));
    if (CheckException(env)) return false;

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Clearcut is succesfully found.");
    return true;
}
}