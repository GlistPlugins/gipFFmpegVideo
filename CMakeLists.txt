cmake_minimum_required (VERSION 3.10.2)

set(pluginname gipFFmpegVideo)
#if(NOT DEFINED pluginname)
    
#endif()

set(PLUGIN_DIR ${PLUGINS_DIR}/${pluginname})
set(PREBUILT_DIR ${PLUGIN_DIR}/prebuilts)


list(APPEND PLUGIN_SRCS
			${PLUGIN_DIR}/src/gipFFmpegVideo.cpp
			${PLUGIN_DIR}/src/gipFFmpegUtils.cpp
			${PLUGIN_DIR}/src/gipAVSound.cpp
			${PLUGIN_DIR}/src/gRingBuffer.cpp
)

list(APPEND PLUGIN_INCLUDES
			${PLUGIN_DIR}/src
			${PREBUILT_DIR}/include
			${PREBUILT_DIR}/include/libavcodec
			${PREBUILT_DIR}/include/libavutil
			${PREBUILT_DIR}/include/libavfilter
			${PREBUILT_DIR}/include/libavformat
			${PREBUILT_DIR}/include/libavdevice
			${PREBUILT_DIR}/include/libswresample
			${PREBUILT_DIR}/include/libswscale
)

if(WIN32)
list(APPEND PLUGIN_LINKDIRS
			${PREBUILT_DIR}/lib/win32
			${PREBUILT_DIR}/bin
)

#set(PLUGIN_LIBDIRS ${PREBUILT_DIR}/lib/win32)
list(APPEND PLUGIN_LIBDIRS
			${PREBUILT_DIR}/lib/win32
)
endif(WIN32)

					
if(WIN32)
	list(APPEND PLUGIN_LINKLIBS
				avcodec
				avformat
				avfilter
				avdevice
				avutil
				swscale
				portaudio
				)
elseif(LINUX)
	list(APPEND PLUGIN_LINKLIBS
				avcodec
				avformat
				avfilter
				avdevice
				avutil
				swscale
				portaudio
				)
elseif(APPLE)
	list(APPEND PLUGIN_LINKLIBS
				)
endif(WIN32)
