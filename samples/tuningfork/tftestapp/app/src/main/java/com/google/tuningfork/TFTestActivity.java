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

package com.google.tuningfork;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.Choreographer;
import com.google.android.gms.common.GoogleApiAvailability;

public class TFTestActivity extends AppCompatActivity implements Choreographer.FrameCallback {


    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        CheckGMS();
        nInit();
    }

    @Override
    protected void onStart() {
        super.onStart();
        Choreographer.getInstance().postFrameCallback(this);
    }

    @Override
    public void doFrame(long t) {

        nOnChoreographer(t);
        Choreographer.getInstance().postFrameCallback(this);
    }

    private void CheckGMS() {

        GoogleApiAvailability availability = GoogleApiAvailability.getInstance();
        int status = availability.isGooglePlayServicesAvailable(this.getApplicationContext());
        int version = GoogleApiAvailability.GOOGLE_PLAY_SERVICES_VERSION_CODE;

        Log.i("Tuningfork Clearcut", "CheckGMS status: " + status);
        Log.i("TUningfork Clearcut", "CheckGMS version: " + version);
    }

    public native void nInit();
    public native void nOnChoreographer(long frameTimeNanos);

}
