# gst-yuvconvert
gstreamer插件，通过libyuv进行缩放及像素格式转换

* 支持nv12/i420输入，i420/argb输出。

* 先缩放后转换。

# 参考命令
gst-launch-1.0 -v filesrc location=./d_1080.mp4 ! qtdemux ! h264parse ! avdec_h264ftomx ! yuvconvert ! ximagesink
