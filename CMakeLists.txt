cmake_minimum_required (VERSION 3.10.2)

set(pluginname gipFFmpegVideo)
#if(NOT DEFINED pluginname)
    
#endif()

set(PLUGIN_DIR ${PLUGINS_DIR}/${pluginname})
set(PREBUILT_DIR ${PLUGIN_DIR}/prebuilts)


list(APPEND PLUGIN_SRCS
			${PLUGIN_DIR}/src/gipFFmpegVideo.cpp
			${PLUGIN_DIR}/src/gipFFmpegUtils.cpp
)

list(APPEND PLUGIN_INCLUDES
			${PLUGIN_DIR}/src
)

list(APPEND PLUGIN_INCLUDES
	${PREBUILT_DIR}/include
	${PREBUILT_DIR}/include/libavcodec
	${PREBUILT_DIR}/include/libavutil
	${PREBUILT_DIR}/include/libavfilter
	${PREBUILT_DIR}/include/libavformat
	${PREBUILT_DIR}/include/libavdevice
	${PREBUILT_DIR}/include/libswresample
	${PREBUILT_DIR}/include/libswscale
)

list(APPEND PLUGIN_LIBDIRS ${PREBUILT_DIR}/lib/win32)

list(APPEND PLUGIN_LINKDIRS ${PREBUILT_DIR}/bin)

if(WIN32)
	list(APPEND PLUGIN_LINKLIBS
				avcodec
				avdevice
				avfilter
				avformat
				avutil
				swresample
				swscale )
elseif(LINUX)
	list(APPEND PLUGIN_LINKLIBS
				)
elseif(APPLE)
	list(APPEND PLUGIN_LINKLIBS
				)
endif(WIN32)