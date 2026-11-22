# Portable domain logic: no ESP-IDF or LVGL headers allowed here (host-tested).
idf_component_register(SRCS "text.cpp" "dedupe.cpp" "search.cpp" INCLUDE_DIRS "include")
