# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "E:/esp/esp-idf/components/bootloader/subproject"
  "F:/esp_project/ESP32_GPS_M5311/build/bootloader"
  "F:/esp_project/ESP32_GPS_M5311/build/bootloader-prefix"
  "F:/esp_project/ESP32_GPS_M5311/build/bootloader-prefix/tmp"
  "F:/esp_project/ESP32_GPS_M5311/build/bootloader-prefix/src/bootloader-stamp"
  "F:/esp_project/ESP32_GPS_M5311/build/bootloader-prefix/src"
  "F:/esp_project/ESP32_GPS_M5311/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "F:/esp_project/ESP32_GPS_M5311/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
