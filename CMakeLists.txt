cmake_minimum_required(VERSION 3.15)
project(BusinessFX)

include(GenerateG3A)
find_package(Gint 2.1 REQUIRED)

set(SOURCES
  src/main.c
  src/ui.c
  src/calc.c
  src/formulas.c
  src/content.c
  src/charts.c
)

add_executable(businessfx ${SOURCES})
target_compile_options(businessfx PRIVATE -Wall -Wextra -Os)
target_link_libraries(businessfx Gint::Gint)

if("${FXSDK_PLATFORM_LONG}" STREQUAL fxCG50)
  generate_g3a(TARGET businessfx OUTPUT "BusinessFX.g3a"
    NAME "Business FX" ICONS assets-cg/icon-uns.png assets-cg/icon-sel.png)
endif()
