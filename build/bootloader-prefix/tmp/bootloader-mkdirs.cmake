# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/hung-le/esp/esp-idf/components/bootloader/subproject"
  "/home/hung-le/Test_ethernet/esp_lte_a7677s_ppp/build/bootloader"
  "/home/hung-le/Test_ethernet/esp_lte_a7677s_ppp/build/bootloader-prefix"
  "/home/hung-le/Test_ethernet/esp_lte_a7677s_ppp/build/bootloader-prefix/tmp"
  "/home/hung-le/Test_ethernet/esp_lte_a7677s_ppp/build/bootloader-prefix/src/bootloader-stamp"
  "/home/hung-le/Test_ethernet/esp_lte_a7677s_ppp/build/bootloader-prefix/src"
  "/home/hung-le/Test_ethernet/esp_lte_a7677s_ppp/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/hung-le/Test_ethernet/esp_lte_a7677s_ppp/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/hung-le/Test_ethernet/esp_lte_a7677s_ppp/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
