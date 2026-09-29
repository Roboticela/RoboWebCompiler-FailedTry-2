# CMake generated Testfile for 
# Source directory: D:/Muhammad/RoboHTMLCompiler/Tests
# Build directory: D:/Muhammad/RoboHTMLCompiler/build/Tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[RoboHTMLCompilerTests]=] "D:/Muhammad/RoboHTMLCompiler/build/bin/RoboHTMLCompilerTests.exe")
set_tests_properties([=[RoboHTMLCompilerTests]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/Muhammad/RoboHTMLCompiler/Tests/CMakeLists.txt;40;add_test;D:/Muhammad/RoboHTMLCompiler/Tests/CMakeLists.txt;0;")
add_test([=[RoboHTMLCompilerExample]=] "D:/Muhammad/RoboHTMLCompiler/build/bin/RoboHTMLCompiler.exe" "--name" "BasicApp" "--output" "D:/Muhammad/RoboHTMLCompiler/build/example" "D:/Muhammad/RoboHTMLCompiler/Examples/BasicApp")
set_tests_properties([=[RoboHTMLCompilerExample]=] PROPERTIES  WORKING_DIRECTORY "D:/Muhammad/RoboHTMLCompiler/build" _BACKTRACE_TRIPLES "D:/Muhammad/RoboHTMLCompiler/Tests/CMakeLists.txt;43;add_test;D:/Muhammad/RoboHTMLCompiler/Tests/CMakeLists.txt;0;")
