/****************************************************************************
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 
 http://www.cocos2d-x.org
 
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
 ****************************************************************************/

#include "base/CCIMEDispatcher.h"
#include "base/CCDirector.h"
#include "base/CCEventType.h"
#include "base/CCEventCustom.h"
#include "base/CCEventDispatcher.h"
#include "platform/CCApplication.h"
#include "platform/CCFileUtils.h"
#include "platform/android/jni/JniHelper.h"
#include <jni.h>
#include <base/CCIMEDelegate.h>

#include "base/ccUTF8.h"

using namespace cocos2d;

extern "C" {

    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender(JNIEnv* env) {
        cocos2d::Director::getInstance()->mainLoop();
    }

    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnPause() {
        if (Director::getInstance()->getOpenGLView()) {
                Application::getInstance()->applicationDidEnterBackground();
                cocos2d::EventCustom backgroundEvent(EVENT_COME_TO_BACKGROUND);
                cocos2d::Director::getInstance()->getEventDispatcher()->dispatchEvent(&backgroundEvent);
        }
    }

    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnResume() {
        static bool firstTime = true;
        if (Director::getInstance()->getOpenGLView()) {
            // don't invoke at first to keep the same logic as iOS
            // can refer to https://github.com/cocos2d/cocos2d-x/issues/14206
            if (!firstTime)
                Application::getInstance()->applicationWillEnterForeground();

            cocos2d::EventCustom foregroundEvent(EVENT_COME_TO_FOREGROUND);
            cocos2d::Director::getInstance()->getEventDispatcher()->dispatchEvent(&foregroundEvent);

            firstTime = false;
        }
    }

    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInsertText(JNIEnv* env, jobject thiz, jstring text) {
        std::string  strValue = cocos2d::StringUtils::getStringUTFCharsJNI(env, text);
        const char* pszText = strValue.c_str();
        cocos2d::IMEDispatcher::sharedDispatcher()->dispatchInsertText(pszText, strlen(pszText));
    }

    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeDeleteBackward(JNIEnv* env, jobject thiz) {
        cocos2d::IMEDispatcher::sharedDispatcher()->dispatchDeleteBackward();
    }

    // AWFramework addition
    JNIEXPORT jboolean JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeShouldChangeText(JNIEnv* env, jobject thiz, jint start, jint length, jstring text) {
        std::string  strValue = cocos2d::StringUtils::getStringUTFCharsJNI(env, text);
        return cocos2d::IMEDispatcher::sharedDispatcher()->dispatchShouldChangeText(AWTextRange(start, start + length), strValue);
    }

    // AWFramework addition
    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeReplaceText(JNIEnv* env, jobject thiz, jint start, jint length, jstring text) {
        std::string  strValue = cocos2d::StringUtils::getStringUTFCharsJNI(env, text);
        cocos2d::IMEDispatcher::sharedDispatcher()->dispatchReplaceText(AWTextRange(start, start + length), strValue);
    }

    // AWFramework addition
    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeKeyboardWillShow(JNIEnv* env, jobject thiz, jfloat duration, jint beginX, jint beginY, jint beginWidth, jint beginHeight, jint endX, jint endY, jint endWidth, jint endHeight) {

        Director* director = Director::getInstance();
        GLView* glView = director->getOpenGLView();
        const Size& size = director->getWinSize();
        float nodeOverViewScale = size.width / (glView->getFrameSize().width / glView->getContentScaleFactor());

        auto info = IMEKeyboardNotificationInfo();
        info.duration = duration;
        float bHeight = (float)beginHeight * nodeOverViewScale;
        info.begin = Rect((float)beginX * nodeOverViewScale, size.height - (float)beginY * nodeOverViewScale - bHeight, (float)beginWidth * nodeOverViewScale, bHeight);
        float eHeight = (float)endHeight * nodeOverViewScale;
        info.end = Rect((float)endX * nodeOverViewScale, size.height - (float)endY * nodeOverViewScale - eHeight, (float)endWidth * nodeOverViewScale, eHeight);

        cocos2d::IMEDispatcher::sharedDispatcher()->dispatchKeyboardWillShow(info);
    }

    // AWFramework addition
    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeKeyboardDidShow(JNIEnv* env, jobject thiz, jfloat duration, jint beginX, jint beginY, jint beginWidth, jint beginHeight, jint endX, jint endY, jint endWidth, jint endHeight) {

        Director* director = Director::getInstance();
        GLView* glView = director->getOpenGLView();
        const Size& size = director->getWinSize();
        float nodeOverViewScale = size.width / (glView->getFrameSize().width / glView->getContentScaleFactor());

        auto info = IMEKeyboardNotificationInfo();
        info.duration = duration;
        float bHeight = (float)beginHeight * nodeOverViewScale;
        info.begin = Rect((float)beginX * nodeOverViewScale, size.height - (float)beginY * nodeOverViewScale - bHeight, (float)beginWidth * nodeOverViewScale, bHeight);
        float eHeight = (float)endHeight * nodeOverViewScale;
        info.end = Rect((float)endX * nodeOverViewScale, size.height - (float)endY * nodeOverViewScale - eHeight, (float)endWidth * nodeOverViewScale, eHeight);

        cocos2d::IMEDispatcher::sharedDispatcher()->dispatchKeyboardDidShow(info);
    }

    // AWFramework addition
    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeKeyboardWillHide(JNIEnv* env, jobject thiz, jfloat duration, jint beginX, jint beginY, jint beginWidth, jint beginHeight, jint endX, jint endY, jint endWidth, jint endHeight) {

        Director* director = Director::getInstance();
        GLView* glView = director->getOpenGLView();
        const Size& size = director->getWinSize();
        float nodeOverViewScale = size.width / (glView->getFrameSize().width / glView->getContentScaleFactor());

        auto info = IMEKeyboardNotificationInfo();
        info.duration = duration;
        float bHeight = (float)beginHeight * nodeOverViewScale;
        info.begin = Rect((float)beginX * nodeOverViewScale, size.height - (float)beginY * nodeOverViewScale - bHeight, (float)beginWidth * nodeOverViewScale, bHeight);
        float eHeight = (float)endHeight * nodeOverViewScale;
        info.end = Rect((float)endX * nodeOverViewScale, size.height - (float)endY * nodeOverViewScale - eHeight, (float)endWidth * nodeOverViewScale, eHeight);

        cocos2d::IMEDispatcher::sharedDispatcher()->dispatchKeyboardWillHide(info);
    }

    // AWFramework addition
    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeKeyboardDidHide(JNIEnv* env, jobject thiz, jfloat duration, jint beginX, jint beginY, jint beginWidth, jint beginHeight, jint endX, jint endY, jint endWidth, jint endHeight) {

        Director* director = Director::getInstance();
        GLView* glView = director->getOpenGLView();
        const Size& size = director->getWinSize();
        float nodeOverViewScale = size.width / (glView->getFrameSize().width / glView->getContentScaleFactor());

        auto info = IMEKeyboardNotificationInfo();
        info.duration = duration;
        float bHeight = (float)beginHeight * nodeOverViewScale;
        info.begin = Rect((float)beginX * nodeOverViewScale, size.height - (float)beginY * nodeOverViewScale - bHeight, (float)beginWidth * nodeOverViewScale, bHeight);
        float eHeight = (float)endHeight * nodeOverViewScale;
        info.end = Rect((float)endX * nodeOverViewScale, size.height - (float)endY * nodeOverViewScale - eHeight, (float)endWidth * nodeOverViewScale, eHeight);

        cocos2d::IMEDispatcher::sharedDispatcher()->dispatchKeyboardDidHide(info);
    }

    JNIEXPORT jstring JNICALL Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeGetContentText() {
        JNIEnv * env = 0;

        if (JniHelper::getJavaVM()->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK || ! env) {
            return 0;
        }
        std::string pszText = cocos2d::IMEDispatcher::sharedDispatcher()->getContentText();
        return cocos2d::StringUtils::newStringUTFJNI(env, pszText);
    }
}
