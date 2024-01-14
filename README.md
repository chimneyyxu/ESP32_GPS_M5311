# ESP32_GPS_M5311
使用 wifi can ,用AT指令和M5311 GPS 通信

支持 esp_idf v5.1

新增 wifi scan1任务，1小时定时唤醒一次检查是否在wifi范围内，如果在则进入休眠，否则（或者3小时一次）连接onnet上传数据


## How to use example

### Hardware Required


