package imgui.example.android;

import android.app.NativeActivity;
import android.os.Bundle;
import android.content.Context;
import android.view.inputmethod.InputMethodManager;
import android.view.KeyEvent;
import java.util.concurrent.LinkedBlockingQueue;
import android.util.Log;

public class MainActivity extends NativeActivity {
    @Override
    public  void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    void showSoftInput() {
        InputMethodManager inputMethodManager = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
        inputMethodManager.showSoftInput(this.getWindow().getDecorView(), 0);
    }

    void hideSoftInput() {
        InputMethodManager inputMethodManager = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE) ;
        inputMethodManager.hideSoftInputFromWindow(getWindow().getDecorView().getWindowToken(), 0);
    }

    // Queue for the Unicode characters to be polled from native code (via pollUnicodeChar())
    private LinkedBlockingQueue<Integer> unicodeCharacterQueue = new LinkedBlockingQueue();

    // We assume dispatchKeyEvent() of the NativeActivity is actually called for every
    // KeyEvent and not consumed by any View before it reaches here
    @Override 
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            unicodeCharacterQueue.offer(event.getUnicodeChar(event.getMetaState()));
        }
        return super.dispatchKeyEvent(event);
    }

    public int pollUnicodeChar() {
        Integer i = unicodeCharacterQueue.poll();
        return i != null? i : 0;
    }
}
