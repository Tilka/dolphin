add_library(use_pch pch.h pch.cpp)

# fmt/format.h is included in the PCH
target_link_libraries(use_pch PUBLIC fmt::fmt)

target_precompile_headers(use_pch PRIVATE pch.h)
