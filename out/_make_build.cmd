mkdir _new_build
mkdir _new_build\icons
mkdir _new_build\shaders

copy "icons"   "_new_build\icons"
copy "shaders" "_new_build\shaders"

copy "brotlicommon.dll" "_new_build/brotlicommon.dll"
copy "brotlidec.dll"    "_new_build/brotlidec.dll"
copy "brotlienc.dll"    "_new_build/brotlienc.dll"
copy "FreeImage.dll"    "_new_build/FreeImage.dll"
copy "kdl.dll"          "_new_build/kdl.dll"
copy "heif.dll"         "_new_build/heif.dll"
copy "jxl.dll"          "_new_build/jxl.dll"
copy "jxl_cms.dll"      "_new_build/jxl_cms.dll"
copy "jxl_threads.dll"  "_new_build/jxl_threads.dll"
copy "libmpv-2.dll"     "_new_build/libmpv-2.dll"
copy "SDL3.dll"         "_new_build/SDL3.dll"
copy "spng.dll"         "_new_build/spng.dll"
copy "turbojpeg.dll"    "_new_build/turbojpeg.dll"

copy "avcodec-63.dll"   "_new_build/avcodec-63.dll"
copy "avdevice-63.dll"  "_new_build/avdevice-63.dll"
copy "avfilter-12.dll"  "_new_build/avfilter-12.dll"
copy "avformat-63.dll"  "_new_build/avformat-63.dll"
copy "avutil-61.dll"    "_new_build/avutil-61.dll"
copy "swresample-7.dll" "_new_build/swresample-7.dll"
copy "swscale-10.dll"   "_new_build/swscale-10.dll"


copy "foto_falcon.dll" "_new_build/foto_falcon.dll"
copy "foto_falcon.com" "_new_build/foto_falcon.com"
copy "foto_falcon.exe" "_new_build/foto_falcon.exe"

copy "config_default.kdl" "_new_build/config_default.kdl"
copy "seguiemj.ttf"       "_new_build/seguiemj.ttf"

pause
