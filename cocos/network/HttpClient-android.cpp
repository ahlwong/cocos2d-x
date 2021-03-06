/****************************************************************************
 Copyright (c) 2012      greathqy
 Copyright (c) 2012      cocos2d-x.org
 Copyright (c) 2013-2017 Chukong Technologies Inc.
 
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

#include "platform/CCPlatformConfig.h"
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)

#include "network/HttpClient.h"

#include <queue>
#include <sstream>
#include <stdio.h>
#include <errno.h>

#include "base/CCDirector.h"
#include "platform/CCFileUtils.h"
#include "platform/android/jni/JniHelper.h"

#include "base/ccUTF8.h"

// AWFramework addition
#include <jni.h>

NS_CC_BEGIN

namespace network {
    
typedef std::vector<std::string> HttpRequestHeaders;
typedef HttpRequestHeaders::iterator HttpRequestHeadersIter;
typedef std::vector<std::string> HttpCookies;
typedef HttpCookies::iterator HttpCookiesIter;

static HttpClient* _httpClient = nullptr; // pointer to singleton

struct CookiesInfo
{
    std::string domain;
    bool tailmatch;
    std::string path;
    bool secure;
    std::string key;
    std::string value;
    std::string expires;
};

//static size_t writeData(void *ptr, size_t size, size_t nmemb, void *stream)
static size_t writeData(void* buffer, size_t sizes, HttpResponse* response)
{
    std::vector<char> * recvBuffer = (std::vector<char>*)response->getResponseData();
    recvBuffer->clear();
    recvBuffer->insert(recvBuffer->end(), (char*)buffer, ((char*)buffer) + sizes);
    return sizes;
} 

//static size_t writeHeaderData(void *ptr, size_t size, size_t nmemb, void *stream)
size_t writeHeaderData(void* buffer, size_t sizes,HttpResponse* response)
{
    std::vector<char> * recvBuffer = (std::vector<char>*) response->getResponseHeader();
    recvBuffer->clear();
    recvBuffer->insert(recvBuffer->end(), (char*)buffer, (char*)buffer + sizes);
    return sizes;
}

#pragma mark - HttpURLConnection

class HttpURLConnection
{

#pragma mark - Instance Variables

private:

    HttpClient* _client;
    std::string _requestMethod;
    std::string _responseCookies;
    std::string _cookieFileName;
    std::string _url;

    bool _receivedResponse;
    int _responseCode;
    char* _responseHeaders;
    char* _responseContent;
    int _responseContentLength;
    char* _responseMessage;

    std::condition_variable_any _conditionVariable;
    std::mutex _mutex;

public:

    void dispatchResponseCallback(int responseCode, char* responseHeaders, char* responseContent, int responseContentLength, char* responseMessage)
    {
        _receivedResponse = true;
        _responseCode = responseCode;
        _responseHeaders = responseHeaders;
        _responseContent = responseContent;
        _responseContentLength = responseContentLength;
        _responseMessage = responseMessage;

        _conditionVariable.notify_one();
    }
    
    size_t saveResponseCookies(const char* responseCookies, size_t count)
    {
        if (nullptr == responseCookies || strlen(responseCookies) == 0 || count == 0) {
            return 0;
        }
        
        if (_cookieFileName.empty()) {
            _cookieFileName = FileUtils::getInstance()->getWritablePath() + "cookieFile.txt";
        }
        
        FILE* fp = fopen(_cookieFileName.c_str(), "w");
        if (nullptr == fp) {
            CCLOG("can't create or open response cookie files");
            return 0;
        }
        
        fwrite(responseCookies, sizeof(char), count, fp);
        
        fclose(fp);
        
        return count;
    }
    
    std::string getResponseHeaderByKey(HttpResponse* response, const std::string& key)
    {
        const std::vector<char>* header = response->getResponseHeader();
        std::string headerString(header->begin(), header->end());

        auto it = std::search(headerString.begin(), headerString.end(), key.begin(), key.end(), [](char ch1, char ch2) {
            return std::toupper(ch1) == std::toupper(ch2);
        });
        if (it != headerString.end()) {
            size_t startPos = it - headerString.begin();
            size_t endPos = headerString.find_first_of("\n", startPos);
            if (endPos == std::string::npos) {
                return headerString.substr(startPos);
            }
            else {
                return headerString.substr(startPos, endPos - startPos);
            }
        }

        return "";
    }

    const std::string& getCookieFileName() const
    {
        return _cookieFileName;
    }
    
    void setCookieFileName(std::string& filename)
    {
        _cookieFileName = filename;
    }

    void populateResponse(HttpResponse* response, char* responseMessage)
    {
        // Response code
        response->setResponseCode(_responseCode);

        // Headers
        if (_responseHeaders) {
            writeHeaderData(_responseHeaders, strlen(_responseHeaders), response);
        }
        free(_responseHeaders);

        // Cookies
        std::string cookies = getResponseHeaderByKey(response, "set-cookie");
        if (!cookies.empty()) {
            saveResponseCookies(cookies.c_str(), cookies.size());
        }

        // Content
        if (_responseContent) {
            std::vector<char> * recvBuffer = (std::vector<char>*)response->getResponseData();
            recvBuffer->clear();
            recvBuffer->insert(recvBuffer->begin(), (char*)_responseContent, ((char*)_responseContent) + _responseContentLength);
        }
        free(_responseContent);

        // Message
        if (_responseMessage) {
            strcpy(responseMessage, _responseMessage);
            free(_responseMessage);
        }

        if (_responseCode == -1) {
            response->setSucceed(false);
            response->setErrorBuffer(responseMessage);
        }
        else {
            response->setSucceed(true);
        }
    }

    void wait()
    {
        while(!_receivedResponse) {
            _conditionVariable.wait(_mutex);
        }
    }

private:

    void createHttpURLConnection(HttpRequest* request)
    {
        JniMethodInfo methodInfo;
        if (JniHelper::getStaticMethodInfo(methodInfo,
            "org.cocos2dx.lib.Cocos2dxHttpURLConnection",
            "createHttpURLConnection",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/util/Map;[BIIJJLjava/lang/String;)V"))
        {
            // Method
            setupRequestMethod(request);
            jstring jMethod = methodInfo.env->NewStringUTF(_requestMethod.c_str());

            // URL
            _url = request->getUrl();
            jstring jURL = methodInfo.env->NewStringUTF(_url.c_str());

            // Headers
            std::map<std::string, std::string> header;
            setupRequestHeader(request, header);

            jclass hashMapClass = methodInfo.env->FindClass("java/util/HashMap");
            jmethodID hashMapInit = methodInfo.env->GetMethodID(hashMapClass, "<init>", "(I)V");
            jobject hashMapObj = methodInfo.env->NewObject(hashMapClass, hashMapInit, header.size());
            jmethodID hashMapPut = methodInfo.env->GetMethodID(hashMapClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
            for (auto it : header) {
                jstring key = methodInfo.env->NewStringUTF(it.first.c_str());
                jstring value = methodInfo.env->NewStringUTF(it.second.c_str());
                methodInfo.env->CallObjectMethod(hashMapObj, hashMapPut, key, value);
                methodInfo.env->DeleteLocalRef(key);
                methodInfo.env->DeleteLocalRef(value);
            }

            // Data
            jbyteArray jData;
            ssize_t dataSize = request->getRequestDataSize();
            jData = methodInfo.env->NewByteArray(dataSize);
            methodInfo.env->SetByteArrayRegion(jData, 0, dataSize, (const jbyte*)request->getRequestData());

            // Read and connect timeouts
            jint jReadTimeout = _client ? _client->getTimeoutForRead() * 1000 : 30 * 1000; // Defaults to 30 seconds
            jint jConnectTimeout = _client ? _client->getTimeoutForConnect() * 1000 : 30 * 1000; // Defaults to 30 seconds

            // Request pointer
            jlong jRequestPointer = (jlong)(intptr_t)request;

            // Connection pointer
            jlong jConnectionPointer = (jlong)(intptr_t)this;

            // SSL filename
            std::string sslFilename = _client && !_client->getSSLVerification().empty() ? FileUtils::getInstance()->fullPathForFilename(_client->getSSLVerification()) : "";
            jstring jSSLFilename = methodInfo.env->NewStringUTF(sslFilename.c_str());

            // Call
            methodInfo.env->CallStaticVoidMethod(methodInfo.classID, methodInfo.methodID, jMethod, jURL, hashMapObj, jData, jReadTimeout, jConnectTimeout, jRequestPointer, jConnectionPointer, jSSLFilename);

            // Cleanup
            methodInfo.env->DeleteLocalRef(jMethod);
            methodInfo.env->DeleteLocalRef(jURL);
            methodInfo.env->DeleteLocalRef(hashMapObj);
            methodInfo.env->DeleteLocalRef(jData);
            methodInfo.env->DeleteLocalRef(jSSLFilename);
            methodInfo.env->DeleteLocalRef(methodInfo.classID);
        }
    }

    void setupRequestMethod(HttpRequest* request)
    {
        HttpRequest::Type requestType = request->getRequestType();

        if (HttpRequest::Type::GET != requestType &&
            HttpRequest::Type::POST != requestType &&
            HttpRequest::Type::PUT != requestType &&
            HttpRequest::Type::DELETE != requestType)
        {
            CCASSERT(true, "CCHttpClient: unknown request type, only GET、POST、PUT、DELETE are supported");
            return;
        }

        switch (requestType) {

            case HttpRequest::Type::GET:
                _requestMethod = "GET";
                break;

            case HttpRequest::Type::POST:
                _requestMethod = "POST";
                break;

            case HttpRequest::Type::PUT:
                _requestMethod = "PUT";
                break;

            case HttpRequest::Type::DELETE:
                _requestMethod = "DELETE";
                break;

            default:
                break;
        }
    }

    void setupRequestHeader(HttpRequest* request, std::map<std::string, std::string>& headerMap)
    {
        HttpRequestHeaders headers = request->getHeaders();
        if (!headers.empty()) {
            for (auto& header : headers) {
                int len = header.length();
                int pos = header.find(':');
                if (-1 == pos || pos >= len) {
                    continue;
                }
                std::string key = header.substr(0, pos);
                std::string value = header.substr(pos + 1, len - pos - 1);
                headerMap[key] = value;
            }
        }

        setupCookieRequestHeader(headerMap);
    }

    void setupCookieRequestHeader(std::map<std::string, std::string>& headerMap)
    {
        if (_client->getCookieFilename().empty()) {
            return;
        }
        
        _cookieFileName = FileUtils::getInstance()->fullPathForFilename(_client->getCookieFilename());
        
        std::string cookiesInfo = FileUtils::getInstance()->getStringFromFile(_cookieFileName);
        
        if (cookiesInfo.empty()) {
            return;
        }
        
        HttpCookies cookiesVec;
        cookiesVec.clear();
        
        std::stringstream  stream(cookiesInfo);
        std::string item;
        while (std::getline(stream, item, '\n')) {
            cookiesVec.push_back(item);
        }
        
        if (cookiesVec.empty()) {
            return;
        }
        
        std::vector<CookiesInfo> cookiesInfoVec;
        cookiesInfoVec.clear();

        for (auto& cookies : cookiesVec) {
            if (cookies.find("#HttpOnly_") != std::string::npos) {
                cookies = cookies.substr(10);
            }
            
            if (cookies.at(0) == '#') {
                continue;
            }
            
            CookiesInfo co;
            std::stringstream streamInfo(cookies);
            std::string item;
            std::vector<std::string> elems;
            
            while (std::getline(streamInfo, item, '\t')) {
                elems.push_back(item);
            }
            
            co.domain = elems[0];
            if (co.domain.at(0) == '.') {
                co.domain = co.domain.substr(1);
            }
            co.tailmatch = strcmp("TRUE", elems.at(1).c_str())?true: false;
            co.path   = elems.at(2);
            co.secure = strcmp("TRUE", elems.at(3).c_str())?true: false;
            co.expires = elems.at(4);
            co.key = elems.at(5);
            co.value = elems.at(6);
            cookiesInfoVec.push_back(co);
        }

        std::string sendCookiesInfo = "";
        int cookiesCount = 0;
        for (auto& cookieInfo : cookiesInfoVec) {
            if (_url.find(cookieInfo.domain) != std::string::npos) {
                std::string keyValue = cookieInfo.key;
                keyValue.append("=");
                keyValue.append(cookieInfo.value);
                if (cookiesCount != 0) {
                    sendCookiesInfo.append(";");
                }
                
                sendCookiesInfo.append(keyValue);
            }
            cookiesCount++;
        }

        headerMap["Cookie"] = sendCookiesInfo;
    }

#pragma mark - Constructors

public:

    HttpURLConnection(HttpClient* httpClient)

    : _client(httpClient)
    , _requestMethod("")
    , _responseCookies("")
    , _cookieFileName("")
    , _receivedResponse(false)
    , _responseCode(-1)
    , _responseHeaders(nullptr)
    , _responseContent(nullptr)
    , _responseContentLength(0)
    , _responseMessage(nullptr)

    {

    }

    ~HttpURLConnection()
    {

    }

    bool init(HttpRequest* request)
    {
        if (nullptr == _client) {
            return false;
        }

        createHttpURLConnection(request);

        return true;
    }

};

#pragma mark - HttpClient

// Process Response
void HttpClient::processResponse(HttpResponse* response, char* responseMessage)
{
    auto request = response->getHttpRequest();
    HttpRequest::Type requestType = request->getRequestType();
    if (HttpRequest::Type::GET != requestType &&
        HttpRequest::Type::POST != requestType &&
        HttpRequest::Type::PUT != requestType &&
        HttpRequest::Type::DELETE != requestType)
    {
        CCASSERT(true, "CCHttpClient: unknown request type, only GET、POST、PUT、DELETE are supported");
        return;
    }

    HttpURLConnection urlConnection(this);
    if (!urlConnection.init(request)) {
        response->setSucceed(false);
        response->setErrorBuffer("HttpURLConnetcion init failed");
        return;
    }

    urlConnection.wait();

    urlConnection.populateResponse(response, responseMessage);
}

// Worker thread
void HttpClient::networkThread()
{    
    increaseThreadCount();

    while (true) 
    {
        HttpRequest *request;

        // step 1: send http request if the requestQueue isn't empty
        {
            std::lock_guard<std::mutex> lock(_requestQueueMutex);
            while (_requestQueue.empty()) {
                _sleepCondition.wait(_requestQueueMutex);
            }
            request = _requestQueue.at(0);
            _requestQueue.erase(0);
        }

        if (request == _requestSentinel) {
            break;
        }
        
        // Create a HttpResponse object, the default setting is http access failed
        HttpResponse *response = new (std::nothrow) HttpResponse(request);
        processResponse(response, _responseMessage);
        
        // add response packet into queue
        _responseQueueMutex.lock();
        _responseQueue.pushBack(response);
        _responseQueueMutex.unlock();
        
        _schedulerMutex.lock();
        if (nullptr != _scheduler)
        {
            _scheduler->performFunctionInCocosThread(CC_CALLBACK_0(HttpClient::dispatchResponseCallbacks, this));
        }
        _schedulerMutex.unlock();
    }
    
    // cleanup: if worker thread received quit signal, clean up un-completed request queue
    _requestQueueMutex.lock();
    _requestQueue.clear();
    _requestQueueMutex.unlock();
    
    _responseQueueMutex.lock();
    _responseQueue.clear();
    _responseQueueMutex.unlock();

    decreaseThreadCountAndMayDeleteThis();    
}

// Worker thread
void HttpClient::networkThreadAlone(HttpRequest* request, HttpResponse* response)
{
    increaseThreadCount();

    char responseMessage[RESPONSE_BUFFER_SIZE] = { 0 };
    processResponse(response, responseMessage);

    _schedulerMutex.lock();
    if (_scheduler != nullptr)
    {
        _scheduler->performFunctionInCocosThread([this, response, request]{
            const ccHttpRequestCallback& callback = request->getCallback();
            Ref* pTarget = request->getTarget();
            SEL_HttpResponse pSelector = request->getSelector();

            if (callback != nullptr)
            {
                callback(this, response);
            }
            else if (pTarget && pSelector)
            {
                (pTarget->*pSelector)(this, response);
            }
            response->release();
            // do not release in other thread
            request->release();
        });
    }
    _schedulerMutex.unlock();
    decreaseThreadCountAndMayDeleteThis();
}

// HttpClient implementation
HttpClient* HttpClient::getInstance()
{
    if (_httpClient == nullptr) 
    {
        _httpClient = new (std::nothrow) HttpClient();
    }
    
    return _httpClient;
}

void HttpClient::destroyInstance()
{
    if (_httpClient == nullptr)
    {
        CCLOG("HttpClient singleton is nullptr");
        return;
    }

    CCLOG("HttpClient::destroyInstance ...");

    auto thiz = _httpClient;
    _httpClient = nullptr;

    thiz->_scheduler->unscheduleAllForTarget(thiz);

    thiz->_schedulerMutex.lock();
    thiz->_scheduler = nullptr;
    thiz->_schedulerMutex.unlock();

    {
        std::lock_guard<std::mutex> lock(thiz->_requestQueueMutex);
        thiz->_requestQueue.pushBack(thiz->_requestSentinel);
    }
    thiz->_sleepCondition.notify_one();

    thiz->decreaseThreadCountAndMayDeleteThis();
    CCLOG("HttpClient::destroyInstance() finished!");
}

void HttpClient::enableCookies(const char* cookieFile) 
{
    std::lock_guard<std::mutex> lock(_cookieFileMutex);
    if (cookieFile)
    {
        _cookieFilename = std::string(cookieFile);
    }
    else
    {
        _cookieFilename = (FileUtils::getInstance()->getWritablePath() + "cookieFile.txt");
    }
}
    
void HttpClient::setSSLVerification(const std::string& caFile)
{
    std::lock_guard<std::mutex> lock(_sslCaFileMutex);
    _sslCaFilename = caFile;
}

#pragma mark - Constructors

HttpClient::HttpClient()
: _isInited(false)
, _timeoutForConnect(30)
, _timeoutForRead(60)
, _threadCount(0)
, _cookie(nullptr)
, _requestSentinel(new HttpRequest())
{
    CCLOG("In the constructor of HttpClient!");
    increaseThreadCount();
    _scheduler = Director::getInstance()->getScheduler();
}

HttpClient::~HttpClient()
{
    CCLOG("In the destructor of HttpClient!");
    CC_SAFE_RELEASE(_requestSentinel);
}

//Lazy create semaphore & mutex & thread
bool HttpClient::lazyInitThreadSemaphore()
{
    if (_isInited)
    {
        return true;
    }
    else
    {
        auto t = std::thread(CC_CALLBACK_0(HttpClient::networkThread, this));
        t.detach();
        _isInited = true;
    }

    return true;
}

//Add a get task to queue
void HttpClient::send(HttpRequest* request)
{    
    if (!lazyInitThreadSemaphore()) 
    {
        return;
    }
    
    if (nullptr == request)
    {
        return;
    }
        
    request->retain();

    _requestQueueMutex.lock();
    _requestQueue.pushBack(request);
    _requestQueueMutex.unlock();

    // Notify thread start to work
    _sleepCondition.notify_one();
}

void HttpClient::sendImmediate(HttpRequest* request)
{
    if(nullptr == request)
    {
        return;
    }

    request->retain();
    // Create a HttpResponse object, the default setting is http access failed
    HttpResponse *response = new (std::nothrow) HttpResponse(request);

    auto t = std::thread(&HttpClient::networkThreadAlone, this, request, response);
    t.detach();
}

// Poll and notify main thread if responses exists in queue
void HttpClient::dispatchResponseCallbacks()
{
    // log("CCHttpClient::dispatchResponseCallbacks is running");
    //occurs when cocos thread fires but the network thread has already quited
    HttpResponse* response = nullptr;
    
    _responseQueueMutex.lock();

    if (!_responseQueue.empty())
    {
        response = _responseQueue.at(0);
        _responseQueue.erase(0);
    }

    _responseQueueMutex.unlock();
    
    if (response)
    {
        HttpRequest *request = response->getHttpRequest();
        const ccHttpRequestCallback& callback = request->getCallback();
        Ref* pTarget = request->getTarget();
        SEL_HttpResponse pSelector = request->getSelector();

        if (callback != nullptr)
        {
            callback(this, response);
        }
        else if (pTarget && pSelector)
        {
            (pTarget->*pSelector)(this, response);
        }
        
        response->release();
        // do not release in other thread
        request->release();
    }
}

void HttpClient::increaseThreadCount()
{
    _threadCountMutex.lock();
    ++_threadCount;
    _threadCountMutex.unlock();
}

void HttpClient::decreaseThreadCountAndMayDeleteThis()
{
    bool needDeleteThis = false;
    _threadCountMutex.lock();
    --_threadCount;
    if (0 == _threadCount)
    {
        needDeleteThis = true;
    }
    
    _threadCountMutex.unlock();
    if (needDeleteThis)
    {
        delete this;
    }
}

void HttpClient::setTimeoutForConnect(int value)
{
    std::lock_guard<std::mutex> lock(_timeoutForConnectMutex);
    _timeoutForConnect = value;
}
    
int HttpClient::getTimeoutForConnect()
{
    std::lock_guard<std::mutex> lock(_timeoutForConnectMutex);
    return _timeoutForConnect;
}
    
void HttpClient::setTimeoutForRead(int value)
{
    std::lock_guard<std::mutex> lock(_timeoutForReadMutex);
    _timeoutForRead = value;
}
    
int HttpClient::getTimeoutForRead()
{
    std::lock_guard<std::mutex> lock(_timeoutForReadMutex);
    return _timeoutForRead;
}
    
const std::string& HttpClient::getCookieFilename()
{
    std::lock_guard<std::mutex> lock(_cookieFileMutex);
    return _cookieFilename;
}
    
const std::string& HttpClient::getSSLVerification()
{
    std::lock_guard<std::mutex> lock(_sslCaFileMutex);
    return _sslCaFilename;
}

// AWFramework addition
extern "C" {

    static char* getBufferFromJString(jstring jstr, JNIEnv* env)
    {
        if (nullptr == jstr)
        {
            return nullptr;
        }
        std::string strValue = cocos2d::StringUtils::getStringUTFCharsJNI(env, jstr);
        return strdup(strValue.c_str());
    }

    static int getCStrFromJByteArray(jbyteArray jba, JNIEnv* env, char** ppData)
    {
        if (nullptr == jba)
        {
            *ppData = nullptr;
            return 0;
        }

        int len = env->GetArrayLength(jba);
        char* str = (char*)malloc(sizeof(char)*len);
        env->GetByteArrayRegion(jba, 0, len, (jbyte*)str);

        *ppData = str;
        return len;
    }

    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxHttpURLConnection_nativeDispatchProgress(JNIEnv* env, jobject thiz, jlong requestPointer, jlong totalBytesWritten, jlong totalBytesExpectedToWrite)
    {
        HttpRequest* request = reinterpret_cast<HttpRequest*>((intptr_t)requestPointer);
        request->dispatchUploadProgressCallback((long)totalBytesWritten, (long)totalBytesExpectedToWrite);
    }

    JNIEXPORT void JNICALL Java_org_cocos2dx_lib_Cocos2dxHttpURLConnection_nativeDispatchResponse(JNIEnv* env, jobject thiz, jlong connectionPointer, jint responseCode, jstring responseHeaders, jbyteArray responseContent, jstring responseMessage)
    {
        HttpURLConnection* connection = reinterpret_cast<HttpURLConnection*>((intptr_t)connectionPointer);

        char* content = nullptr;
        int contentLength = getCStrFromJByteArray(responseContent, env, &content);
        
        connection->dispatchResponseCallback((int)responseCode, getBufferFromJString(responseHeaders, env), content, contentLength, getBufferFromJString(responseMessage, env));
    }
}

}

NS_CC_END

#endif // #if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
