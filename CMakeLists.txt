cmake_minimum_required (VERSION 3.10.2)

set(pluginname gipFFmpegVideo)
#if(NOT DEFINED pluginname)

#endif()

set(PLUGIN_DIR ${PLUGINS_DIR}/${pluginname})
set(PREBUILT_DIR ${PLUGIN_DIR}/prebuilts)


list(APPEND PLUGIN_SRCS
			${PLUGIN_DIR}/src/gipFFmpegVideo.cpp
			${PLUGIN_DIR}/src/gipFFmpegUtils.cpp
			${PLUGIN_DIR}/src/gVideoFrameRingBuffer.cpp
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
	set(PLUGIN_LIBS_DIR "${PREBUILT_DIR}/lib/win32")
elseif(LINUX)
	set(PLUGIN_LIBS_DIR "${PREBUILT_DIR}/lib/linux")
endif()

list(APPEND PLUGIN_LINKDIRS ${PLUGIN_LIBS_DIR})
list(APPEND PLUGIN_LIBDIRS ${PLUGIN_LIBS_DIR})

if(WIN32 OR LINUX)
	list(APPEND PLUGIN_LINKLIBS
		avcodec
		avformat
		avfilter
		avdevice
		avutil
		swscale
	)
endif()
