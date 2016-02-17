cmake_minimum_required(VERSION 2.6)
project(zyncoder)

add_executable(zyncoder main.cpp)

install(TARGETS zyncoder RUNTIME DESTINATION bin)
