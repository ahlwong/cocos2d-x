/****************************************************************************
Copyright (c) 2010-2011 cocos2d-x.org

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

import android.app.Activity;
import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.Handler;
import android.os.Message;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

// AWFramework additions
import android.util.Pair;
import android.text.InputType;
import android.view.inputmethod.EditorInfo;
import android.view.ViewTreeObserver;
import android.graphics.Rect;

public class Cocos2dxGLSurfaceView extends GLSurfaceView {
    // ===========================================================
    // Constants
    // ===========================================================

    private static final String TAG = Cocos2dxGLSurfaceView.class.getSimpleName();

    private final static int HANDLER_OPEN_IME_KEYBOARD = 2;
    private final static int HANDLER_CLOSE_IME_KEYBOARD = 3;

    // AWFramework additions
    private final static int HANDLER_SET_SELECTED_TEXT_RANGE = 4;
    private final static int HANDLER_SET_KEYBOARD_INPUT_TYPE_IME_OPTIONS = 5;

    // ===========================================================
    // Fields
    // ===========================================================

    // TODO Static handler -> Potential leak!
    private static Handler sHandler;

    private static Cocos2dxGLSurfaceView mCocos2dxGLSurfaceView;
    private static Cocos2dxTextInputWrapper sCocos2dxTextInputWraper;

    private Cocos2dxRenderer mCocos2dxRenderer;
    private Cocos2dxEditBox mCocos2dxEditText;

    private boolean mSoftKeyboardShown = false;
    private boolean mMultipleTouchEnabled = true;

    public boolean isSoftKeyboardShown() {
        return mSoftKeyboardShown;
    }

    public void setSoftKeyboardShown(boolean softKeyboardShown) {
        this.mSoftKeyboardShown = softKeyboardShown;
    }

    public boolean isMultipleTouchEnabled() {
        return mMultipleTouchEnabled;
    }

    public void setMultipleTouchEnabled(boolean multipleTouchEnabled) {
        this.mMultipleTouchEnabled = multipleTouchEnabled;
    }

    // AWFramework additions
    private boolean mKeyboardVisible = false;
    private int mKeyboardDiscrepancy = 0;
    private Rect mKeyboardRect;

    // ===========================================================
    // Constructors
    // ===========================================================

    public Cocos2dxGLSurfaceView(final Context context) {
        super(context);

        this.initView();
    }

    public Cocos2dxGLSurfaceView(final Context context, final AttributeSet attrs) {
        super(context, attrs);
        
        this.initView();
    }

    protected void initView() {
        this.setFocusableInTouchMode(true);

        Cocos2dxGLSurfaceView.mCocos2dxGLSurfaceView = this;
        Cocos2dxGLSurfaceView.sCocos2dxTextInputWraper = new Cocos2dxTextInputWrapper(this);

        Cocos2dxGLSurfaceView.sHandler = new Handler() {
            @Override
            public void handleMessage(final Message msg) {
                switch (msg.what) {
                    case HANDLER_OPEN_IME_KEYBOARD:
                        if (null != Cocos2dxGLSurfaceView.this.mCocos2dxEditText && Cocos2dxGLSurfaceView.this.mCocos2dxEditText.requestFocus()) {
                            Cocos2dxGLSurfaceView.this.mCocos2dxEditText.removeTextChangedListener(Cocos2dxGLSurfaceView.sCocos2dxTextInputWraper);
                            Cocos2dxGLSurfaceView.this.mCocos2dxEditText.setText("");
                            final String text = (String) msg.obj;
                            Cocos2dxGLSurfaceView.this.mCocos2dxEditText.append(text);
                            Cocos2dxGLSurfaceView.sCocos2dxTextInputWraper.setOriginText(text);
                            Cocos2dxGLSurfaceView.this.mCocos2dxEditText.addTextChangedListener(Cocos2dxGLSurfaceView.sCocos2dxTextInputWraper);
                            final InputMethodManager imm = (InputMethodManager) Cocos2dxGLSurfaceView.mCocos2dxGLSurfaceView.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
                            imm.showSoftInput(Cocos2dxGLSurfaceView.this.mCocos2dxEditText, 0);
                            Log.d("GLSurfaceView", "showSoftInput");
                        }
                        break;

                    case HANDLER_CLOSE_IME_KEYBOARD:
                        if (null != Cocos2dxGLSurfaceView.this.mCocos2dxEditText) {
                            Cocos2dxGLSurfaceView.this.mCocos2dxEditText.removeTextChangedListener(Cocos2dxGLSurfaceView.sCocos2dxTextInputWraper);
                            final InputMethodManager imm = (InputMethodManager) Cocos2dxGLSurfaceView.mCocos2dxGLSurfaceView.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
                            imm.hideSoftInputFromWindow(Cocos2dxGLSurfaceView.this.mCocos2dxEditText.getWindowToken(), 0);
                            Cocos2dxGLSurfaceView.this.requestFocus();
                            // can take effect after GLSurfaceView has focus
                            ((Cocos2dxActivity)Cocos2dxGLSurfaceView.mCocos2dxGLSurfaceView.getContext()).hideVirtualButton();
                            Log.d("GLSurfaceView", "HideSoftInput");
                        }
                        break;

                    // AWFramework addition
                    case HANDLER_SET_SELECTED_TEXT_RANGE:
                        if (null != Cocos2dxGLSurfaceView.this.mCocos2dxEditText) {
                            final Pair pair = (Pair) msg.obj;

                            // Update the contents on edit text before computing selected text range because native code may have changed text
                            final String text = mCocos2dxRenderer.getContentText();
                            if (!Cocos2dxGLSurfaceView.this.mCocos2dxEditText.getText().toString().equals(text)) {
                                Cocos2dxGLSurfaceView.this.mCocos2dxEditText.removeTextChangedListener(Cocos2dxGLSurfaceView.sCocos2dxTextInputWraper);
                                Cocos2dxGLSurfaceView.this.mCocos2dxEditText.setText(text);
                                Cocos2dxGLSurfaceView.sCocos2dxTextInputWraper.setOriginText(text);
                                Cocos2dxGLSurfaceView.this.mCocos2dxEditText.addTextChangedListener(Cocos2dxGLSurfaceView.sCocos2dxTextInputWraper);
                            }

                            int length = Cocos2dxGLSurfaceView.this.mCocos2dxEditText.getText().length();
                            int first = ((Integer)pair.first).intValue();
                            first = Math.min(first, length);
                            int second = ((Integer)pair.second).intValue();
                            second = Math.min(second, length);
                            Cocos2dxGLSurfaceView.this.mCocos2dxEditText.setSelection(first, second);
                            Log.d("GLSurfaceView", "SetSelectedTextRange");
                        }
                        break;

                    // AWFramework addition
                    case HANDLER_SET_KEYBOARD_INPUT_TYPE_IME_OPTIONS:
                        if (null != Cocos2dxGLSurfaceView.this.mCocos2dxEditText) {
                            final Pair pair = (Pair) msg.obj;
                            int length = Cocos2dxGLSurfaceView.this.mCocos2dxEditText.getText().length();
                            int inputType = ((Integer)pair.first).intValue();
                            int imeOptions = ((Integer)pair.second).intValue();
                            Cocos2dxGLSurfaceView.this.mCocos2dxEditText.setInputType(inputType);
                            Cocos2dxGLSurfaceView.this.mCocos2dxEditText.setImeOptions(imeOptions);
                            Log.d("GLSurfaceView", "SetInputTypeAndIMEOptions");
                        }
                        break;
                }
            }
        };

        // AWFramework addition
        final Activity activity = (Activity)this.getContext();
        getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
            @Override
            public void onGlobalLayout() {
                Rect frame = new Rect();
                View rootview = activity.getWindow().getDecorView();
                rootview.getWindowVisibleDisplayFrame(frame);

                int screenHeight = rootview.getRootView().getHeight() - frame.top;
                int keyboardHeight = screenHeight - frame.height() - (mKeyboardDiscrepancy == 1 ? 0 : mKeyboardDiscrepancy);
                if (mKeyboardDiscrepancy == 0) {
                    mKeyboardDiscrepancy = keyboardHeight;
                    if (keyboardHeight == 0) {
                        mKeyboardDiscrepancy = 1;
                    }
                }

                boolean visible = keyboardHeight > 100; // arbitrary threshold
                if (mKeyboardVisible != visible) {
                    float duration = 0.3f;
                    if (mKeyboardVisible) {
                        Rect begin = mKeyboardRect;
                        Rect end = new Rect(frame.left, screenHeight, frame.right, screenHeight);
                        Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleKeyboardWillHide(duration, begin, end);
                        Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleKeyboardDidHide(duration, begin, end);
                    }
                    else {
                        Rect begin = new Rect(frame.left, screenHeight, frame.right, screenHeight);
                        Rect end = new Rect(frame.left, frame.bottom - frame.top, frame.right, screenHeight);
                        Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleKeyboardWillShow(duration, begin, end);
                        Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleKeyboardDidShow(duration, begin, end);
                        mKeyboardRect = end;
                    }
                    mKeyboardVisible = visible;
                }
            }
        });
    }

    // ===========================================================
    // Getter & Setter
    // ===========================================================


       public static Cocos2dxGLSurfaceView getInstance() {
       return mCocos2dxGLSurfaceView;
       }

       public static void queueAccelerometer(final float x, final float y, final float z, final long timestamp) {   
       mCocos2dxGLSurfaceView.queueEvent(new Runnable() {
        @Override
            public void run() {
                Cocos2dxAccelerometer.onSensorChanged(x, y, z, timestamp);
        }
        });
    }

    public void setCocos2dxRenderer(final Cocos2dxRenderer renderer) {
        this.mCocos2dxRenderer = renderer;
        this.setRenderer(this.mCocos2dxRenderer);
    }

    private String getContentText() {
        return this.mCocos2dxRenderer.getContentText();
    }

    public Cocos2dxEditBox getCocos2dxEditText() {
        return this.mCocos2dxEditText;
    }

    public void setCocos2dxEditText(final Cocos2dxEditBox pCocos2dxEditText) {
        this.mCocos2dxEditText = pCocos2dxEditText;

        // AWFramework addition
        this.mCocos2dxEditText.setSingleLine(false);
        this.mCocos2dxEditText.setMultilineEnabled(true);
        this.mCocos2dxEditText.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE);

        if (null != this.mCocos2dxEditText && null != Cocos2dxGLSurfaceView.sCocos2dxTextInputWraper) {
            this.mCocos2dxEditText.setOnEditorActionListener(Cocos2dxGLSurfaceView.sCocos2dxTextInputWraper);
            this.requestFocus();
        }
    }

    // ===========================================================
    // Methods for/from SuperClass/Interfaces
    // ===========================================================

    @Override
    public void onResume() {
        super.onResume();
        this.setRenderMode(RENDERMODE_CONTINUOUSLY);
        this.queueEvent(new Runnable() {
            @Override
            public void run() {
                Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleOnResume();
            }
        });
    }

    @Override
    public void onPause() {
        this.queueEvent(new Runnable() {
            @Override
            public void run() {
                Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleOnPause();
            }
        });
        this.setRenderMode(RENDERMODE_WHEN_DIRTY);
        //super.onPause();
    }

    @Override
    public boolean onTouchEvent(final MotionEvent pMotionEvent) {
        // these data are used in ACTION_MOVE and ACTION_CANCEL
        final int pointerNumber = pMotionEvent.getPointerCount();
        final int[] ids = new int[pointerNumber];
        final float[] xs = new float[pointerNumber];
        final float[] ys = new float[pointerNumber];

        if (mSoftKeyboardShown){
            InputMethodManager imm = (InputMethodManager)this.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
            View view = ((Activity)this.getContext()).getCurrentFocus();
            imm.hideSoftInputFromWindow(view.getWindowToken(),0);
            this.requestFocus();
            mSoftKeyboardShown = false;
        }

        for (int i = 0; i < pointerNumber; i++) {
            ids[i] = pMotionEvent.getPointerId(i);
            xs[i] = pMotionEvent.getX(i);
            ys[i] = pMotionEvent.getY(i);
        }

        switch (pMotionEvent.getAction() & MotionEvent.ACTION_MASK) {
            case MotionEvent.ACTION_POINTER_DOWN:
                final int indexPointerDown = pMotionEvent.getAction() >> MotionEvent.ACTION_POINTER_INDEX_SHIFT;
                if (!mMultipleTouchEnabled && indexPointerDown != 0) {
                    break;
                }
                final int idPointerDown = pMotionEvent.getPointerId(indexPointerDown);
                final float xPointerDown = pMotionEvent.getX(indexPointerDown);
                final float yPointerDown = pMotionEvent.getY(indexPointerDown);

                this.queueEvent(new Runnable() {
                    @Override
                    public void run() {
                        Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleActionDown(idPointerDown, xPointerDown, yPointerDown);
                    }
                });
                break;

            case MotionEvent.ACTION_DOWN:
                // there are only one finger on the screen
                final int idDown = pMotionEvent.getPointerId(0);
                final float xDown = xs[0];
                final float yDown = ys[0];

                this.queueEvent(new Runnable() {
                    @Override
                    public void run() {
                        Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleActionDown(idDown, xDown, yDown);
                    }
                });
                break;

            case MotionEvent.ACTION_MOVE:
                if (!mMultipleTouchEnabled) {
                    // handle only touch with id == 0
                    for (int i = 0; i < pointerNumber; i++) {
                        if (ids[i] == 0) {
                            final int[] idsMove = new int[]{0};
                            final float[] xsMove = new float[]{xs[i]};
                            final float[] ysMove = new float[]{ys[i]};
                            this.queueEvent(new Runnable() {
                                @Override
                                public void run() {
                                    Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleActionMove(idsMove, xsMove, ysMove);
                                }
                            });
                            break;
                        }
                    }
                } else {
                    this.queueEvent(new Runnable() {
                        @Override
                        public void run() {
                            Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleActionMove(ids, xs, ys);
                        }
                    });
                }
                break;

            case MotionEvent.ACTION_POINTER_UP:
                final int indexPointUp = pMotionEvent.getAction() >> MotionEvent.ACTION_POINTER_INDEX_SHIFT;
                if (!mMultipleTouchEnabled && indexPointUp != 0) {
                    break;
                }
                final int idPointerUp = pMotionEvent.getPointerId(indexPointUp);
                final float xPointerUp = pMotionEvent.getX(indexPointUp);
                final float yPointerUp = pMotionEvent.getY(indexPointUp);

                this.queueEvent(new Runnable() {
                    @Override
                    public void run() {
                        Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleActionUp(idPointerUp, xPointerUp, yPointerUp);
                    }
                });
                break;

            case MotionEvent.ACTION_UP:
                // there are only one finger on the screen
                final int idUp = pMotionEvent.getPointerId(0);
                final float xUp = xs[0];
                final float yUp = ys[0];

                this.queueEvent(new Runnable() {
                    @Override
                    public void run() {
                        Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleActionUp(idUp, xUp, yUp);
                    }
                });
                break;

            case MotionEvent.ACTION_CANCEL:
                if (!mMultipleTouchEnabled) {
                    // handle only touch with id == 0
                    for (int i = 0; i < pointerNumber; i++) {
                        if (ids[i] == 0) {
                            final int[] idsCancel = new int[]{0};
                            final float[] xsCancel = new float[]{xs[i]};
                            final float[] ysCancel = new float[]{ys[i]};
                            this.queueEvent(new Runnable() {
                                @Override
                                public void run() {
                                    Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleActionCancel(idsCancel, xsCancel, ysCancel);
                                }
                            });
                            break;
                        }
                    }
                } else {
                    this.queueEvent(new Runnable() {
                        @Override
                        public void run() {
                            Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleActionCancel(ids, xs, ys);
                        }
                    });
                }
                break;
        }

        /*
        if (BuildConfig.DEBUG) {
            Cocos2dxGLSurfaceView.dumpMotionEvent(pMotionEvent);
        }
        */
        return true;
    }

    /*
     * This function is called before Cocos2dxRenderer.nativeInit(), so the
     * width and height is correct.
     */
    @Override
    protected void onSizeChanged(final int pNewSurfaceWidth, final int pNewSurfaceHeight, final int pOldSurfaceWidth, final int pOldSurfaceHeight) {
        if(!this.isInEditMode()) {
            this.mCocos2dxRenderer.setScreenWidthAndHeight(pNewSurfaceWidth, pNewSurfaceHeight);
        }
    }

    @Override
    public boolean onKeyDown(final int pKeyCode, final KeyEvent pKeyEvent) {
        switch (pKeyCode) {
            case KeyEvent.KEYCODE_BACK:
                Cocos2dxVideoHelper.mVideoHandler.sendEmptyMessage(Cocos2dxVideoHelper.KeyEventBack);
            case KeyEvent.KEYCODE_MENU:
            case KeyEvent.KEYCODE_DPAD_LEFT:
            case KeyEvent.KEYCODE_DPAD_RIGHT:
            case KeyEvent.KEYCODE_DPAD_UP:
            case KeyEvent.KEYCODE_DPAD_DOWN:
            case KeyEvent.KEYCODE_ENTER:
            case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
            case KeyEvent.KEYCODE_DPAD_CENTER:
                this.queueEvent(new Runnable() {
                    @Override
                    public void run() {
                        Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleKeyDown(pKeyCode);
                    }
                });
                return true;
            default:
                return super.onKeyDown(pKeyCode, pKeyEvent);
        }
    }

    @Override
    public boolean onKeyUp(final int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BACK:
            case KeyEvent.KEYCODE_MENU:
            case KeyEvent.KEYCODE_DPAD_LEFT:
            case KeyEvent.KEYCODE_DPAD_RIGHT:
            case KeyEvent.KEYCODE_DPAD_UP:
            case KeyEvent.KEYCODE_DPAD_DOWN:
            case KeyEvent.KEYCODE_ENTER:
            case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
            case KeyEvent.KEYCODE_DPAD_CENTER:
                this.queueEvent(new Runnable() {
                    @Override
                    public void run() {
                        Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleKeyUp(keyCode);
                    }
                });
                return true;
            default:
                return super.onKeyUp(keyCode, event);
        }
    }

    // ===========================================================
    // Methods
    // ===========================================================

    // ===========================================================
    // Inner and Anonymous Classes
    // ===========================================================

    public static void openIMEKeyboard() {
        final Message msg = new Message();
        msg.what = Cocos2dxGLSurfaceView.HANDLER_OPEN_IME_KEYBOARD;
        msg.obj = Cocos2dxGLSurfaceView.mCocos2dxGLSurfaceView.getContentText();
        Cocos2dxGLSurfaceView.sHandler.sendMessage(msg);
    }

    public static void closeIMEKeyboard() {
        final Message msg = new Message();
        msg.what = Cocos2dxGLSurfaceView.HANDLER_CLOSE_IME_KEYBOARD;
        Cocos2dxGLSurfaceView.sHandler.sendMessage(msg);
    }

    public void insertText(final String pText) {
        this.queueEvent(new Runnable() {
            @Override
            public void run() {
                Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleInsertText(pText);
            }
        });
    }

    public void deleteBackward() {
        this.queueEvent(new Runnable() {
            @Override
            public void run() {
                Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleDeleteBackward();
            }
        });
    }

    private static void dumpMotionEvent(final MotionEvent event) {
        final String names[] = { "DOWN", "UP", "MOVE", "CANCEL", "OUTSIDE", "POINTER_DOWN", "POINTER_UP", "7?", "8?", "9?" };
        final StringBuilder sb = new StringBuilder();
        final int action = event.getAction();
        final int actionCode = action & MotionEvent.ACTION_MASK;
        sb.append("event ACTION_").append(names[actionCode]);
        if (actionCode == MotionEvent.ACTION_POINTER_DOWN || actionCode == MotionEvent.ACTION_POINTER_UP) {
            sb.append("(pid ").append(action >> MotionEvent.ACTION_POINTER_INDEX_SHIFT);
            sb.append(")");
        }
        sb.append("[");
        for (int i = 0; i < event.getPointerCount(); i++) {
            sb.append("#").append(i);
            sb.append("(pid ").append(event.getPointerId(i));
            sb.append(")=").append((int) event.getX(i));
            sb.append(",").append((int) event.getY(i));
            if (i + 1 < event.getPointerCount()) {
                sb.append(";");
            }
        }
        sb.append("]");
        Log.d(Cocos2dxGLSurfaceView.TAG, sb.toString());
    }

    // AWFramework addition
    public static void setSelectedTextRange(final int start, final int end) {
        final Message msg = new Message();
        msg.what = Cocos2dxGLSurfaceView.HANDLER_SET_SELECTED_TEXT_RANGE;
        msg.obj = Pair.create(new Integer(start), new Integer(end));
        Cocos2dxGLSurfaceView.sHandler.sendMessage(msg);
    }

    // AWFramework addition
    public void replaceText(final int start, final int length, final String text) {
        this.queueEvent(new Runnable() {
            @Override
            public void run() {
                if (Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleShouldChangeText(start, length, text)) {
                    Cocos2dxGLSurfaceView.this.mCocos2dxRenderer.handleReplaceText(start, length, text);
                }
            }
        });
    }

    // AWFramework addition
    public static void setKeyboardConfiguration(final int type, final int autocapitalizationType, final int autocorrectionType, final int spellcheckingType, final int appearance, final int returnKeyType) {
        int inputType = 0;
        int imeOptions = 0;

        // KeyboardType
        switch(type) {
            case 0: // KeyboardType_DEFAULT
                inputType |= InputType.TYPE_CLASS_TEXT;
                break;
            case 1: // KeyboardType_ASCII_CAPABLE
                inputType |= InputType.TYPE_CLASS_TEXT; // Default
                break;
            case 2: // KeyboardType_NUMBERS_AND_PUNCTUATION
                inputType |= InputType.TYPE_CLASS_NUMBER; // Default
                break;
            case 3: // KeyboardType_URL
                inputType |= InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI;
                break;
            case 4: // KeyboardType_NUMBER_PAD
                inputType |= InputType.TYPE_CLASS_NUMBER;
                break;
            case 5: // KeyboardType_PHONE_PAD
                inputType |= InputType.TYPE_CLASS_PHONE;
                break;
            case 6: // KeyboardType_NAME_PHONE_PAD
                inputType |= InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PERSON_NAME;
                break;
            case 7: // KeyboardType_EMAIL_ADDRESS
                inputType |= InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
                break;
            case 8: // KeyboardType_DECIMAL_PAD
                inputType |= InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL;
                break;
            case 9: // KeyboardType_TWITTER
                inputType |= InputType.TYPE_CLASS_TEXT; // Default
                break;
            case 10: // KeyboardType_WEB_SEARCH
                inputType |= InputType.TYPE_CLASS_TEXT; // Default
                break;
            case 11: // KeyboardType_ASCII_CAPABLE_NUMBER_PAD
                inputType |= InputType.TYPE_CLASS_NUMBER; // Default
                break;
            case 12: // KeyboardType_ALPHABET
                inputType |= InputType.TYPE_CLASS_TEXT; // Default
                break;
            case 13: // KeyboardType_PASSWORD
                inputType |= InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD;
                break;
        }

        if ((inputType & InputType.TYPE_CLASS_TEXT) > 0) {

            // KeyboardAutocapitalizationType
            switch(autocapitalizationType) {
                case 0: // KeyboardAutocapitalizationType_DEFAULT
                    inputType |= InputType.TYPE_TEXT_FLAG_CAP_SENTENCES;
                    break;
                case 1: // KeyboardAutocapitalizationType_NONE
                    // Do nothing
                    break;
                case 2: // KeyboardAutocapitalizationType_ALL_CHARACTERS
                    inputType |= InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS;
                    break;
                case 3: // KeyboardAutocapitalizationType_SENTENCES
                    inputType |= InputType.TYPE_TEXT_FLAG_CAP_SENTENCES;
                    break;
                case 4: // KeyboardAutocapitalizationType_WORDS
                    inputType |= InputType.TYPE_TEXT_FLAG_CAP_WORDS;
                    break;
            }
        }

        if ((inputType & InputType.TYPE_CLASS_TEXT) > 0) {

            // KeyboardAutocorrectionType
            switch (autocorrectionType) {
                case 0: // KeyboardAutocorrectionType_DEFAULT
                    inputType |= InputType.TYPE_TEXT_FLAG_AUTO_CORRECT;
                    break;
                case 1: // KeyboardAutocorrectionType_ON
                    inputType |= InputType.TYPE_TEXT_FLAG_AUTO_CORRECT;
                    break;
                case 2: // KeyboardAutocorrectionType_OFF
                    inputType |= InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
                    break;
            }
        }

        if ((inputType & InputType.TYPE_CLASS_TEXT) > 0) {

            // KeyboardSpellcheckingType
            switch (spellcheckingType) {
                case 0: // KeyboardSpellcheckingType_DEFAULT
                    // Do nothing
                    break;
                case 1: // KeyboardSpellcheckingType_ON
                    // Do nothing
                    break;
                case 2: // KeyboardSpellcheckingType_OFF
                    // Do nothing
                    break;
            }
        }

        // KeyboardAppearance
        switch(appearance) {
            case 0: // KeyboardAppearance_DEFAULT
                // Do nothing
                break;
            case 1: // KeyboardAppearance_DARK
                // Do nothing
                break;
            case 2: // KeyboardAppearance_LIGHT
                // Do nothing
                break;
            case 3: // KeyboardAppearance_ALERT
                // Do nothing
                break;
        }

        // KeyboardReturnKeyType
        switch(returnKeyType) {
            case 0: // KeyboardReturnKeyType_DEFAULT
                imeOptions |= EditorInfo.IME_ACTION_UNSPECIFIED;
                break;
            case 1: // KeyboardReturnKeyType_GO
                imeOptions |= EditorInfo.IME_ACTION_GO;
                break;
            case 2: // KeyboardReturnKeyType_DONE
                imeOptions |= EditorInfo.IME_ACTION_DONE;
                break;
            case 3: // KeyboardReturnKeyType_JOIN
                imeOptions |= EditorInfo.IME_ACTION_GO;
                break;
            case 4: // KeyboardReturnKeyType_NEXT
                imeOptions |= EditorInfo.IME_ACTION_NEXT;
                break;
            case 5: // KeyboardReturnKeyType_SEND
                imeOptions |= EditorInfo.IME_ACTION_SEND;
                break;
            case 6: // KeyboardReturnKeyType_ROUTE
                imeOptions |= EditorInfo.IME_ACTION_GO;
                break;
            case 7: // KeyboardReturnKeyType_SEARCH
                imeOptions |= EditorInfo.IME_ACTION_SEARCH;
                break;
            case 8: // KeyboardReturnKeyType_CONTINUE
                imeOptions |= EditorInfo.IME_ACTION_NEXT;
                break;
        }

        final Message msg = new Message();
        msg.what = Cocos2dxGLSurfaceView.HANDLER_SET_KEYBOARD_INPUT_TYPE_IME_OPTIONS;
        msg.obj = Pair.create(new Integer(inputType), new Integer(imeOptions));
        Cocos2dxGLSurfaceView.sHandler.sendMessage(msg);
    }
}
