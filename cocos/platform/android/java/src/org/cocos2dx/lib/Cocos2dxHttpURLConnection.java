/****************************************************************************
Copyright (c) 2010-2014 cocos2d-x.org

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
package org.cocos2dx.lib;

import android.util.Log;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.KeyStore;
import java.security.cert.Certificate;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Map.Entry;
import java.util.zip.GZIPInputStream;
import java.util.zip.InflaterInputStream;

import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManagerFactory;

import android.os.AsyncTask;

public class Cocos2dxHttpURLConnection
{
    private static final String POST_METHOD = "POST" ;
    private static final String PUT_METHOD = "PUT" ;

    static void createHttpURLConnection(String method, String url, Map<String, String> headers, byte[] data, int readTimeout, int connectTimeout, long requestPointer, long connectionPointer, String sslFilename) {

        SSLContext sslContext = null;
        if (null != sslFilename && !sslFilename.isEmpty()) {
            sslContext = generateSSLContext(sslFilename);
        }

        AWHttpPostAsyncTask asyncTask = new AWHttpPostAsyncTask(method, url, headers, data, readTimeout, connectTimeout, requestPointer, connectionPointer, sslContext);
        asyncTask.execute();
    }

    private static SSLContext generateSSLContext(String sslFilename) {
        SSLContext context = null;
        try {
            InputStream caInput = null;
            if (sslFilename.startsWith("/")) {
                caInput = new BufferedInputStream(new FileInputStream(sslFilename));
            }
            else {
                String assetString = "assets/";
                String assetsfilenameString = sslFilename.substring(assetString.length());
                caInput = new BufferedInputStream(Cocos2dxHelper.getActivity().getAssets().open(assetsfilenameString));
            }

            CertificateFactory cf = CertificateFactory.getInstance("X.509");
            Certificate ca;
            ca = cf.generateCertificate(caInput);
            System.out.println("ca=" + ((X509Certificate) ca).getSubjectDN());
            caInput.close();

            // Create a KeyStore containing our trusted CAs
            String keyStoreType = KeyStore.getDefaultType();
            KeyStore keyStore = KeyStore.getInstance(keyStoreType);
            keyStore.load(null, null);
            keyStore.setCertificateEntry("ca", ca);

            // Create a TrustManager that trusts the CAs in our KeyStore
            String tmfAlgorithm = TrustManagerFactory.getDefaultAlgorithm();
            TrustManagerFactory tmf = TrustManagerFactory.getInstance(tmfAlgorithm);
            tmf.init(keyStore);

            // Create an SSLContext that uses our TrustManager
            context = SSLContext.getInstance("TLS");
            context.init(null, tmf.getTrustManagers(), null);
        }
        catch (Exception e) {
            Log.e("Cocos2dxHttpURLConnection generateSSLContext exception", e.toString());
        }
        return context;
    }

    // AWFramework addition
    private static native void nativeDispatchProgress(long requestPointer, long totalBytesWritten, long totalBytesExpectedToWrite);
    private static native void nativeDispatchResponse(long connectionPointer, int responseCode, String responseHeaders, byte[] responseContent, String responseMessage);

    // AWFramework addition
    private static class AWHttpPostAsyncTask extends AsyncTask<String, Void, Void>
    {
        private String method;
        private String url;
        private Map<String, String> headers;
        private byte[] data;
        private int readTimeout;
        private int connectTimeout;
        private long requestPointer;
        private long connectionPointer;
        private SSLContext sslContext;

        public AWHttpPostAsyncTask(String method, String url, Map<String, String> headers, byte[] data, int readTimeout, int connectTimeout, long requestPointer, long connectionPointer, SSLContext sslContext) {
            this.method = method;
            this.url = url;
            this.headers = headers;
            this.data = data;
            this.readTimeout = readTimeout;
            this.connectTimeout = connectTimeout;
            this.requestPointer = requestPointer;
            this.connectionPointer = connectionPointer;
            this.sslContext = sslContext;
        }

        // This is a function that we are overriding from AsyncTask. It takes Strings as parameters because that is what we defined for the parameters of our async task
        @Override
        protected Void doInBackground(String... params) {

            try {

                // URL
                URL url = new URL(this.url);
                HttpURLConnection urlConnection = (HttpURLConnection) url.openConnection();

                // Input
                urlConnection.setRequestProperty("Accept-Encoding", "identity");
                urlConnection.setDoInput(true);

                // Timeouts
                urlConnection.setReadTimeout(this.readTimeout);
                urlConnection.setConnectTimeout(this.connectTimeout);

                // Headers
                if (null != this.headers) {
                    for (Map.Entry<String, String> entry : this.headers.entrySet()) {
                        urlConnection.setRequestProperty(entry.getKey(), entry.getValue());
                    }
                }

                // Verify SSL
                if (null != this.sslContext && (urlConnection instanceof HttpsURLConnection)) {
                    try {
                        HttpsURLConnection httpsURLConnection = (HttpsURLConnection) urlConnection;
                        httpsURLConnection.setSSLSocketFactory(this.sslContext.getSocketFactory());
                    }
                    catch (Exception e) {
                        Log.e("Cocos2dxHttpURLConnection verify ssl exception", e.toString());
                    }
                }

                // Output
                urlConnection.setRequestMethod(this.method);
                if(POST_METHOD.equalsIgnoreCase(this.method) || PUT_METHOD.equalsIgnoreCase(this.method)) {
                    urlConnection.setDoOutput(true);

                    // Data
                    try {
                        if (null != this.data) {
                            urlConnection.setFixedLengthStreamingMode(this.data.length);
                        }

                        OutputStream out = urlConnection.getOutputStream();
                        if (null != this.data) {
                            int progress = 0;
                            int bytesRead = 0;
                            int chunkSize = 1024;
                            while (progress < this.data.length) {
                                bytesRead = Math.min(chunkSize, this.data.length - progress);
                                out.write(this.data, progress, bytesRead);
                                out.flush();
                                progress += bytesRead;

                                // Dispatch progress
                                nativeDispatchProgress(this.requestPointer, progress, this.data.length);
                            }
                        }
                        out.close();
                    } catch (IOException e) {
                        Log.e("Cocos2dxHttpURLConnection output stream exception", e.toString());
                    }
                }

                // Response code
                int responseCode = getResponseCode(urlConnection);

                // Response headers
                String responseHeaders = getResponseHeaders(urlConnection);

                // Response content
                byte[] responseContent = getResponseContent(urlConnection);

                // Response message
                String responseMessage = getResponseMessage(urlConnection);

                // Disconnect
                urlConnection.disconnect();

                // Dispatch response
                nativeDispatchResponse(this.connectionPointer, responseCode, responseHeaders, responseContent, responseMessage);
            }
            catch (Exception e) {
                Log.d("AWHttpPostAsyncTask", e.getLocalizedMessage());
            }

            return null;
        }

        private int getResponseCode(HttpURLConnection http) {
            int code = 0;
            try {
                code = http.getResponseCode();
            } catch (IOException e) {
                Log.e("Cocos2dxHttpURLConnection getResponseCode exception", e.toString());
            }
            return code;
        }

        private String getResponseHeaders(HttpURLConnection http) {
            Map<String, List<String>> headers = http.getHeaderFields();
            if (null == headers) {
                return null;
            }

            String header = "";

            for (Entry<String, List<String>> entry: headers.entrySet()) {
                String key = entry.getKey();
                if (null == key) {
                    header += listToString(entry.getValue(), ",") + "\n";
                }
                else if ("set-cookie".equalsIgnoreCase(key)) {
                    header += key + ":" + combineCookies(entry.getValue(), http.getURL().getHost()) + "\n";
                }
                else {
                    header += key + ":" + listToString(entry.getValue(), ",") + "\n";
                }
            }

            return header;
        }

        private byte[] getResponseContent(HttpURLConnection http) {
            InputStream in;
            try {
                in = http.getInputStream();
                String contentEncoding = http.getContentEncoding();
                if (contentEncoding != null) {
                    if(contentEncoding.equalsIgnoreCase("gzip")){
                        in = new GZIPInputStream(http.getInputStream()); //reads 2 bytes to determine GZIP stream!
                    }
                    else if(contentEncoding.equalsIgnoreCase("deflate")){
                        in = new InflaterInputStream(http.getInputStream());
                    }
                }
            } catch (IOException e) {
                in = http.getErrorStream();
            } catch (Exception e) {
                Log.e("Cocos2dxHttpURLConnection getResponseContent stream exception", e.toString());
                return null;
            }

            try {
                byte[] buffer = new byte[1024];
                int size   = 0;
                ByteArrayOutputStream bytestream = new ByteArrayOutputStream();
                while((size = in.read(buffer, 0 , 1024)) != -1)
                {
                    bytestream.write(buffer, 0, size);
                }
                byte retbuffer[] = bytestream.toByteArray();
                bytestream.close();
                return retbuffer;
            } catch (Exception e) {
                Log.e("Cocos2dxHttpURLConnection getResponseContent buffer exception", e.toString());
            }

            return null;
        }

        private String getResponseMessage(HttpURLConnection http) {
            String msg;
            try {
                msg = http.getResponseMessage();
            } catch (IOException e) {
                msg = e.toString();
                Log.e("Cocos2dxHttpURLConnection getResponseMessage exception", msg);
            }

            return msg;
        }

        private static String listToString(List<String> list, String strInterVal) {
            if (list == null) {
                return null;
            }
            StringBuilder result = new StringBuilder();
            boolean flag = false;
            for (String str : list) {
                if (flag) {
                    result.append(strInterVal);
                }
                if (null == str) {
                    str = "";
                }
                result.append(str);
                flag = true;
            }
            return result.toString();
        }

        private static String combineCookies(List<String> list, String hostDomain) {
            StringBuilder sbCookies = new StringBuilder();
            String domain    = hostDomain;
            String tailmatch = "FALSE";
            String path      = "/";
            String secure    = "FALSE";
            String key = null;
            String value = null;
            String expires = null;
            for (String str : list) {
                String[] parts = str.split(";");
                for (String part : parts) {
                    int firstIndex = part.indexOf("=");
                    if (-1 == firstIndex)
                        continue;

                    String[] item =  {part.substring(0, firstIndex), part.substring(firstIndex + 1)};
                    if ("expires".equalsIgnoreCase(item[0].trim())) {
                        expires = str2Seconds(item[1].trim());
                    } else if("path".equalsIgnoreCase(item[0].trim())) {
                        path = item[1];
                    } else if("secure".equalsIgnoreCase(item[0].trim())) {
                        secure = item[1];
                    } else if("domain".equalsIgnoreCase(item[0].trim())) {
                        domain = item[1];
                    } else if("version".equalsIgnoreCase(item[0].trim()) || "max-age".equalsIgnoreCase(item[0].trim())) {
                        //do nothing
                    } else {
                        key = item[0];
                        value = item[1];
                    }
                }

                if (null == domain) {
                    domain = "none";
                }

                sbCookies.append(domain);
                sbCookies.append('\t');
                sbCookies.append(tailmatch);  //access
                sbCookies.append('\t');
                sbCookies.append(path);      //path
                sbCookies.append('\t');
                sbCookies.append(secure);    //secure
                sbCookies.append('\t');
                sbCookies.append(expires);   //expires
                sbCookies.append("\t");
                sbCookies.append(key);       //key
                sbCookies.append("\t");
                sbCookies.append(value);     //value
                sbCookies.append('\n');
            }

            return sbCookies.toString();
        }

        private static String str2Seconds(String strTime) {
            Calendar c = Calendar.getInstance();
            long milliseconds = 0;

            try {
                c.setTime(new SimpleDateFormat("EEE, dd-MMM-yy hh:mm:ss zzz", Locale.US).parse(strTime));
                milliseconds = c.getTimeInMillis() / 1000;
            } catch (ParseException ex) {

                // AWFramework addition, fixes expected format
                try {
                    c.setTime(new SimpleDateFormat("EEE, dd MMM yyyy hh:mm:ss zzz", Locale.US).parse(strTime));
                    milliseconds = c.getTimeInMillis() / 1000;
                } catch (ParseException e) {
                    Log.e("Cocos2dxHttpURLConnection str2Seconds exception", e.toString());
                }
            }

            return Long.toString(milliseconds);
        }
    }
}