# Soulcalibur: Broken Destiny recomp module: the translated game (GEN: tools/patch-gen.py's copy of prepare-game.sh's C, with the
# module's changes) and the module's own code, on the SDK's runtime for the Brick.
cmake_minimum_required(VERSION 3.20)
project(soulcalibur_broken_destiny C CXX)
include(sdk/cmake/nakagawa.cmake)
nakagawa_add_runtime(nk_runtime BACKEND brick)
nakagawa_add_game(Soulcalibur_Broken_Destiny_recomp_module RUNTIME nk_runtime GEN "${GEN}" SOURCES "${CMAKE_SOURCE_DIR}/src/module.c")
