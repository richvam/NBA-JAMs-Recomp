// R7.2: ROMs on Android. The app's activity is SDL's, plus the system file picker: "Add ROMs"
// (one or more files) and "Add a ROM folder" (a whole folder, with its subfolders). What is
// chosen is copied into the app's roms folder; no storage permission is needed, because
// the picker grants access to exactly what the user chose. The native side is told how
// many files were copied and scans for games again (src/app/main.cpp, nativeRomsImported).
package io.github.richvam.nbajam;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.RouteInfo;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

public class NBAJamActivity extends SDLActivity {
    private static final int PICK_FILES = 0x4e41, PICK_FOLDER = 0x4e42;
    private static String sRomsDir;
    private int mCopied, mFailed;

    public static native void nativeRomsImported(int copied, int failed);

    // Online play: Wi-Fi drops broadcast packets (the home-network search) unless the app
    // holds a multicast lock; it is held while the app runs
    private WifiManager.MulticastLock mMulticast;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        try {
            WifiManager wm = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
            if (wm != null) {
                mMulticast = wm.createMulticastLock("nbajam-lan");
                mMulticast.setReferenceCounted(false);
                mMulticast.acquire();
            }
        } catch (Exception e) {
            // no Wi-Fi: the search still works on other networks where the system allows it
        }
    }

    @Override
    protected void onDestroy() {
        if (mMulticast != null && mMulticast.isHeld()) mMulticast.release();
        super.onDestroy();
    }

    // The router (default gateway) as an IPv4 address in host order, or 0; NAT-PMP and PCP are
    // sent there. An app cannot read /proc/net/route on Android 10 and later.
    // Android's battery saver (R7.6): the picture is kept cheap while it is on
    public static boolean powerSaveMode() {
        try {
            Context c = getContext();
            android.os.PowerManager pm = c == null ? null : (android.os.PowerManager) c.getSystemService(Context.POWER_SERVICE);
            return pm != null && pm.isPowerSaveMode();
        } catch (Exception e) {
            return false;
        }
    }

    public static int defaultGateway() {
        try {
            Context c = getContext();
            if (c == null || Build.VERSION.SDK_INT < 23) return 0;
            ConnectivityManager cm = (ConnectivityManager) c.getSystemService(Context.CONNECTIVITY_SERVICE);
            Network n = cm == null ? null : cm.getActiveNetwork();
            LinkProperties lp = n == null ? null : cm.getLinkProperties(n);
            if (lp == null) return 0;
            for (RouteInfo r : lp.getRoutes()) {
                InetAddress g = r.getGateway();
                if (r.isDefaultRoute() && g instanceof Inet4Address) {
                    byte[] b = g.getAddress();
                    return (b[0] & 255) << 24 | (b[1] & 255) << 16 | (b[2] & 255) << 8 | (b[3] & 255);
                }
            }
        } catch (Exception e) {
            // no permission or no network
        }
        return 0;
    }

    // called from native code (any thread)
    public static void pickRoms(String romsDir, boolean folder) {
        sRomsDir = romsDir;
        final SDLActivity a = mSingleton;
        if (a == null) return;
        a.runOnUiThread(() -> {
            Intent i;
            if (folder) {
                i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            } else {
                i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                i.addCategory(Intent.CATEGORY_OPENABLE);
                i.setType("*/*");
                i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
            }
            try {
                a.startActivityForResult(i, folder ? PICK_FOLDER : PICK_FILES);
            } catch (Exception e) {
                nativeRomsImported(0, -1);   // no file picker on this device
            }
        });
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != PICK_FILES && request != PICK_FOLDER) return;
        if (result != RESULT_OK || data == null || sRomsDir == null) return;   // cancelled
        final ContentResolver cr = getContentResolver();
        final File roms = new File(sRomsDir);
        new Thread(() -> {
            mCopied = 0;
            mFailed = 0;
            roms.mkdirs();
            if (request == PICK_FOLDER) {
                Uri tree = data.getData();
                if (tree != null) copyTree(cr, tree, DocumentsContract.getTreeDocumentId(tree), roms, 0);
            } else if (data.getClipData() != null) {
                for (int k = 0; k < data.getClipData().getItemCount(); k++) {
                    Uri u = data.getClipData().getItemAt(k).getUri();
                    copyOne(cr, u, roms, displayName(cr, u));
                }
            } else if (data.getData() != null) {
                copyOne(cr, data.getData(), roms, displayName(cr, data.getData()));
            }
            nativeRomsImported(mCopied, mFailed);
        }).start();
    }

    private static String displayName(ContentResolver cr, Uri u) {
        try (Cursor c = cr.query(u, new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (c != null && c.moveToFirst()) return c.getString(0);
        } catch (Exception e) {
            // fall through
        }
        String s = u.getLastPathSegment();
        return s == null ? "rom" : s.substring(s.lastIndexOf('/') + 1);
    }

    private void copyTree(ContentResolver cr, Uri tree, String docId, File into, int depth) {
        if (depth > 8) return;
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, docId);
        String[] cols = { DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                          DocumentsContract.Document.COLUMN_MIME_TYPE };
        try (Cursor c = cr.query(children, cols, null, null, null)) {
            while (c != null && c.moveToNext()) {
                String id = c.getString(0), name = c.getString(1), mime = c.getString(2);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                    copyTree(cr, tree, id, new File(into, safe(name)), depth + 1);
                } else {
                    into.mkdirs();
                    copyOne(cr, DocumentsContract.buildDocumentUriUsingTree(tree, id), into, name);
                }
            }
        } catch (Exception e) {
            mFailed++;
        }
    }

    private static String safe(String name) {
        String s = name == null ? "rom" : name.replace('/', '_').replace('\\', '_');
        return s.isEmpty() || s.equals(".") || s.equals("..") ? "rom" : s;
    }

    // never overwrites: the same name and size is taken as already there, another size gets " (2)"
    private void copyOne(ContentResolver cr, Uri u, File dir, String name) {
        name = safe(name);
        long size = -1;
        try (Cursor c = cr.query(u, new String[] { OpenableColumns.SIZE }, null, null, null)) {
            if (c != null && c.moveToFirst() && !c.isNull(0)) size = c.getLong(0);
        } catch (Exception e) {
            // unknown size
        }
        File d = new File(dir, name);
        if (d.exists()) {
            if (size >= 0 && d.length() == size) return;
            int dot = name.lastIndexOf('.');
            String stem = dot > 0 ? name.substring(0, dot) : name, ext = dot > 0 ? name.substring(dot) : "";
            for (int k = 2; d.exists(); k++) d = new File(dir, stem + " (" + k + ")" + ext);
        }
        File tmp = new File(dir, d.getName() + ".part");
        try (InputStream in = cr.openInputStream(u); OutputStream out = new FileOutputStream(tmp)) {
            if (in == null) throw new java.io.IOException("no stream");
            byte[] buf = new byte[1 << 16];
            for (int n; (n = in.read(buf)) > 0;) out.write(buf, 0, n);
        } catch (Exception e) {
            tmp.delete();
            mFailed++;
            return;
        }
        if (tmp.renameTo(d)) mCopied++;
        else { tmp.delete(); mFailed++; }
    }
}
