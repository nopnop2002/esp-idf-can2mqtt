set(srcs "main.c" "mqtt_pub.c" "mqtt_sub.c")

if (IDF_VERSION_MAJOR STREQUAL "5")
    list(APPEND srcs "twai_task_v5.c")
elseif (IDF_VERSION_MAJOR STREQUAL "6")
    list(APPEND srcs "twai_task_v6.c")
endif()

idf_component_register(SRCS ${srcs} INCLUDE_DIRS "." EMBED_TXTFILES root_cert.pem)
