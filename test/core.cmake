# Core sources shared by the host tests and the simulator. Expects ROOT and creates `panel_core`.
set(ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
# cJSON comes from ESP-IDF's json component; remembered after the first configure.
if(NOT CJSON_DIR)
    if(NOT DEFINED ENV{IDF_PATH})
        message(FATAL_ERROR "Set IDF_PATH (cJSON is taken from ESP-IDF's json component)")
    endif()
    set(CJSON_DIR $ENV{IDF_PATH}/components/json/cJSON CACHE PATH "cJSON sources")
endif()

set(GEN ${CMAKE_BINARY_DIR}/gen)
execute_process(COMMAND python3 ${ROOT}/components/i18n/gen_strings.py ${ROOT}/components/i18n/strings.csv ${GEN}
                RESULT_VARIABLE rc)
if(rc)
    message(FATAL_ERROR "gen_strings.py failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ROOT}/components/i18n/strings.csv)

set(CORE_SOURCES
    ${CJSON_DIR}/cJSON.c
    ${ROOT}/components/core/text.cpp
    ${ROOT}/components/core/dedupe.cpp
    ${ROOT}/components/core/search.cpp
    ${ROOT}/components/img/artwork.cpp
    ${ROOT}/components/pairing/pairing.cpp
    ${ROOT}/components/backend/ma_session.cpp
    ${ROOT}/components/backend/ma_commands.cpp
    ${ROOT}/components/backend/mock_backend.cpp
    ${ROOT}/components/i18n/i18n.cpp
    ${GEN}/strings.gen.cpp
    ${ROOT}/components/app/app.cpp
    ${ROOT}/components/app/features.cpp
    ${ROOT}/components/app/features/players.cpp
    ${ROOT}/components/app/features/playback.cpp
    ${ROOT}/components/app/features/queue.cpp
    ${ROOT}/components/app/features/search.cpp
    ${ROOT}/components/app/features/favourites.cpp
    ${ROOT}/components/app/features/preferences.cpp
    ${ROOT}/components/app/features/playlists.cpp
    ${ROOT}/components/app/features/browse.cpp
    ${ROOT}/components/app/features/setup.cpp
    ${ROOT}/components/app/features/panel.cpp
    ${ROOT}/components/app/features/update.cpp)
add_library(panel_core STATIC ${CORE_SOURCES})
target_include_directories(panel_core PUBLIC
    ${CJSON_DIR}
    ${ROOT}/components/core/include
    ${ROOT}/components/img/include
    ${ROOT}/components/pairing/include
    ${ROOT}/components/backend/include
    ${ROOT}/components/i18n/include ${GEN}
    ${ROOT}/components/app/include ${ROOT}/components/app)
