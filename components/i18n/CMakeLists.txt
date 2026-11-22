# Strings live in strings.csv; the header/table are generated at configure time.
set(gen_dir "${CMAKE_CURRENT_BINARY_DIR}/gen")
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    if(NOT PYTHON)
        set(PYTHON python3)
    endif()
    execute_process(
        COMMAND ${PYTHON} "${CMAKE_CURRENT_LIST_DIR}/gen_strings.py" "${CMAKE_CURRENT_LIST_DIR}/strings.csv" "${gen_dir}"
        RESULT_VARIABLE rc)
    if(rc)
        message(FATAL_ERROR "gen_strings.py failed")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/strings.csv")
endif()
idf_component_register(SRCS "i18n.cpp" "${gen_dir}/strings.gen.cpp" INCLUDE_DIRS "include" "${gen_dir}")
