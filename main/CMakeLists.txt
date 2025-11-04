file(GLOB_RECURSE SOURCES "*.c")
idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES cert/ca.crt cert/client.crt cert/client.key)