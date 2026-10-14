package com.diamon.tecladobios;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executors;

/**
 * Extracts bundled Arduino firmware files (.hex, .ino, guides) from assets/firmware/
 * and saves them to the device's Download/TecladoBIOS_Firmware folder using MediaStore (API 29+)
 * or standard File API with fallback.
 */
public class FirmwareExporter {

    private static final String TAG = "FirmwareExporter";
    private static final String ASSETS_DIR = "firmware";

    public interface ExportCallback {
        void onSuccess(String targetDirectory, List<String> exportedFiles);
        void onError(String errorMessage);
    }

    public static void exportFirmwareAsync(Context context, ExportCallback callback) {
        Executors.newSingleThreadExecutor().execute(() -> {
            try {
                String[] assetFiles = context.getAssets().list(ASSETS_DIR);
                if (assetFiles == null || assetFiles.length == 0) {
                    callback.onError(context.getString(R.string.firmware_no_assets_found, ASSETS_DIR));
                    return;
                }

                List<String> exported = new ArrayList<>();
                String targetPathDesc;

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    // Use MediaStore.Downloads for Android 10+ (Scoped Storage compliant)
                    ContentResolver resolver = context.getContentResolver();
                    targetPathDesc = "Download";

                    for (String fileName : assetFiles) {
                        Uri collection = MediaStore.Downloads.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);
                        ContentValues values = new ContentValues();
                        values.put(MediaStore.Downloads.DISPLAY_NAME, fileName);
                        values.put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS);
                        values.put(MediaStore.Downloads.IS_PENDING, 1);

                        // Determine MIME type
                        if (fileName.endsWith(".hex")) {
                            values.put(MediaStore.Downloads.MIME_TYPE, "application/octet-stream");
                        } else if (fileName.endsWith(".ino") || fileName.endsWith(".txt") || fileName.endsWith(".md")) {
                            values.put(MediaStore.Downloads.MIME_TYPE, "text/plain");
                        } else {
                            values.put(MediaStore.Downloads.MIME_TYPE, "application/octet-stream");
                        }

                        Uri itemUri = resolver.insert(collection, values);
                        if (itemUri != null) {
                            try (InputStream is = context.getAssets().open(ASSETS_DIR + "/" + fileName);
                                 OutputStream os = resolver.openOutputStream(itemUri)) {
                                if (os != null) {
                                    byte[] buffer = new byte[8192];
                                    int read;
                                    while ((read = is.read(buffer)) != -1) {
                                        os.write(buffer, 0, read);
                                    }
                                }
                            }
                            values.clear();
                            values.put(MediaStore.Downloads.IS_PENDING, 0);
                            resolver.update(itemUri, values, null, null);
                            exported.add(fileName);
                        }
                    }
                } else {
                    // Legacy File API for Android 9 and lower
                    File downloadDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
                    if (!downloadDir.exists()) {
                        downloadDir.mkdirs();
                    }
                    targetPathDesc = downloadDir.getAbsolutePath();

                    for (String fileName : assetFiles) {
                        File destFile = new File(downloadDir, fileName);
                        try (InputStream is = context.getAssets().open(ASSETS_DIR + "/" + fileName);
                             OutputStream os = new FileOutputStream(destFile)) {
                            byte[] buffer = new byte[8192];
                            int read;
                            while ((read = is.read(buffer)) != -1) {
                                os.write(buffer, 0, read);
                            }
                        }
                        exported.add(fileName);
                    }
                }

                callback.onSuccess(targetPathDesc, exported);

            } catch (Exception e) {
                Log.e(TAG, "Error exporting firmware", e);
                callback.onError(e.getMessage());
            }
        });
    }
}
