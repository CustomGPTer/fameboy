/* Copyright 2026 Jonathan Jackson. SPDX-License-Identifier: MPL-2.0 */
package uk.jj.fameboy;
import android.graphics.Bitmap;
final class Native {
 static { System.loadLibrary("fameboy"); }
 static final Object LOCK=new Object();
 static native int open(String rom,String save);
 static native int frame(Bitmap bitmap,short[] samples,int keys);
 static native double fps();
 static native boolean flush();
 static native void close();
 static native boolean state(String path,boolean load);
 static native String entries(String path);
 static native boolean extract(String archive,String entry,String target);
 static native int probe(String path);
 static native byte[] snapshot();
 static native boolean restore(byte[] state);
 static native int cheats(String[] codes);
}
