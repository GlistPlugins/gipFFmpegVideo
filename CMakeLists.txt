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

if(WIN32)
	add_library(avcodec SHARED IMPORTED GLOBAL)
	set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION "${PLUGIN_LIBS_DIR}/avcodec-60.dll")
	set_target_properties(avcodec PROPERTIES IMPORTED_IMPLIB "${PLUGIN_LIBS_DIR}/libavcodec.dll.a")

	add_library(avdevice SHARED IMPORTED GLOBAL)
	set_target_properties(avdevice PROPERTIES IMPORTED_LOCATION "${PLUGIN_LIBS_DIR}/avdevice-60.dll")
	set_target_properties(avdevice PROPERTIES IMPORTED_IMPLIB "${PLUGIN_LIBS_DIR}/libavdevice.dll.a")

	add_library(avfilter SHARED IMPORTED GLOBAL)
	set_target_properties(avfilter PROPERTIES IMPORTED_LOCATION "${PLUGIN_LIBS_DIR}/avfilter-9.dll")
	set_target_properties(avfilter PROPERTIES IMPORTED_IMPLIB "${PLUGIN_LIBS_DIR}/libavfilter.dll.a")

	add_library(avformat SHARED IMPORTED GLOBAL)
	set_target_properties(avformat PROPERTIES IMPORTED_LOCATION "${PLUGIN_LIBS_DIR}/avformat-60.dll")
	set_target_properties(avformat PROPERTIES IMPORTED_IMPLIB "${PLUGIN_LIBS_DIR}/libavformat.dll.a")

	add_library(avutil SHARED IMPORTED GLOBAL)
	set_target_properties(avutil PROPERTIES IMPORTED_LOCATION "${PLUGIN_LIBS_DIR}/avutil-58.dll")
	set_target_properties(avutil PROPERTIES IMPORTED_IMPLIB "${PLUGIN_LIBS_DIR}/libavutil.dll.a")

	add_library(swresample SHARED IMPORTED GLOBAL)
	set_target_properties(swresample PROPERTIES IMPORTED_LOCATION "${PLUGIN_LIBS_DIR}/swresample-4.dll")
	set_target_properties(swresample PROPERTIES IMPORTED_IMPLIB "${PLUGIN_LIBS_DIR}/libswresample.dll.a")

	add_library(swscale SHARED IMPORTED GLOBAL)
	set_target_properties(swscale PROPERTIES IMPORTED_LOCATION "${PLUGIN_LIBS_DIR}/swscale-7.dll")
	set_target_properties(swscale PROPERTIES IMPORTED_IMPLIB "${PLUGIN_LIBS_DIR}/libswscale.dll.a")
endif()
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
