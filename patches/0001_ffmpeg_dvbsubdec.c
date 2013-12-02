diff --git a/libs/libtorc-av/libavcodec/avcodec.h b/libs/libtorc-av/libavcodec/avcodec.h
index c4db4d7..fed70c8 100644
--- a/libs/libtorc-av/libavcodec/avcodec.h
+++ b/libs/libtorc-av/libavcodec/avcodec.h
@@ -3477,7 +3477,10 @@ typedef struct AVSubtitleRect {
     int w;         ///< width            of pict, undefined when pict is not set
     int h;         ///< height           of pict, undefined when pict is not set
     int nb_colors; ///< number of colors in pict, undefined when pict is not set
-
+    int display_x; ///< top left corner  of region into which pict is displayed
+    int display_y; ///< top left corner  of region into which pict is displayed
+    int display_w; ///< width            of region into which pict is displayed
+    int display_h; ///< height           of region into which pict is displayed
     /**
      * data+linesize for the bitmap of this subtitle.
      * can be set for text/ass as well once they where rendered
diff --git a/libs/libtorc-av/libavcodec/dvbsubdec.c b/libs/libtorc-av/libavcodec/dvbsubdec.c
index 955925a..18cb595 100644
--- a/libs/libtorc-av/libavcodec/dvbsubdec.c
+++ b/libs/libtorc-av/libavcodec/dvbsubdec.c
@@ -1403,6 +1403,18 @@ static int dvbsub_display_end_segment(AVCodecContext *avctx, const uint8_t *buf,
         rect->y = display->y_pos + offset_y;
         rect->w = region->width;
         rect->h = region->height;
+        if (display_def) {
+            rect->display_x = display_def->x;
+            rect->display_y = display_def->y;
+            rect->display_w = display_def->width;
+            rect->display_h = display_def->height;
+        }
+        else {
+            // per ETS 300 743 - assume 720x576 if Display Definition not provided
+            rect->display_w = 720;
+            rect->display_h = 576;
+        }
+
         rect->nb_colors = (1 << region->depth);
         rect->type      = SUBTITLE_BITMAP;
         rect->pict.linesize[0] = region->width;
