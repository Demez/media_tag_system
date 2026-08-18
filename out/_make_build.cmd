mkdir _new_build
mkdir _new_build\icons
mkdir _new_build\shaders

copy "icons"   "_new_build\icons"
copy "shaders" "_new_build\shaders"

copy "brotlicommon.dll" "_new_build/brotlicommon.dll"
copy "brotlidec.dll"    "_new_build/brotlidec.dll"
copy "brotlienc.dll"    "_new_build/brotlienc.dll"
copy "FreeImage.dll"    "_new_build/FreeImage.dll"
copy "kdl.dll"    "_new_build/kdl.dll"
copy "heif.dll"         "_new_build/heif.dll"
copy "jxl.dll"          "_new_build/jxl.dll"
copy "jxl_cms.dll"      "_new_build/jxl_cms.dll"
copy "jxl_threads.dll"  "_new_build/jxl_threads.dll"
copy "libmpv-2.dll"     "_new_build/libmpv-2.dll"
copy "SDL3.dll"         "_new_build/SDL3.dll"
copy "spng.dll"         "_new_build/spng.dll"
copy "turbojpeg.dll"    "_new_build/turbojpeg.dll"

copy "media_tag_system.com" "_new_build/media_tag_system.com"
copy "media_tag_system.exe" "_new_build/media_tag_system.exe"

copy "config_default.kdl" "_new_build/config_default.kdl"
copy "seguiemj.ttf"       "_new_build/seguiemj.ttf"

pause
