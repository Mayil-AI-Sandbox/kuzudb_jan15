add_library(kuzu_processor_operator_npy_reader
        OBJECT
        npy_reader.cpp)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:kuzu_processor_operator_npy_reader>
        PARENT_SCOPE)
