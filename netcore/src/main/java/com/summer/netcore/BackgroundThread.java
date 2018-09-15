package com.summer.netcore;

import android.os.Handler;
import android.os.HandlerThread;

/**
 * Created by summer on 04/09/2018.
 */

class BackgroundThread {

    private Handler mH;
    private HandlerThread mT;

    private static class Holder{
        static BackgroundThread sIns = new BackgroundThread();
    }

    public static BackgroundThread get(){
        return Holder.sIns;
    }

    private BackgroundThread(){
        mT = new HandlerThread("core-background");
        mT.start();
        mH = new Handler(mT.getLooper());
    }

    public boolean schedule(Runnable task){
        return mH.post(task);
    }

    public void cancel(Runnable task){
        mH.removeCallbacks(task);
    }


}
