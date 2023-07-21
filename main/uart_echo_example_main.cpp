/* UART Echo Example

  c语言求 gps 检验值
  #include <stdio.h>
  #include <string.h>
  int main()
    {
	    char a[50] = "PCAS03,0,0,0,0,9,0,0,0,0,0,,,0,0,,,,0";
	    char b;
	    int i;
	    for(i=0;i<strlen(a);i++){
		  printf("%c",a[i]);
		  b = b^a[i];
    }  
   printf("Hello, World! %x \n",b);  
   return 0;
}
*/
#include <stdio.h>
#include <string>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "cJSON.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_storage.h"
/**
 * This is an example which echos any data it receives on configured UART back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: configured UART
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below (See Kconfig)
 */

#define GPS_TXD  19
#define GPS_RXD  18
#define GPS_UART_PORT_NUM      2
#define GPS_UART_BAUD_RATE     9600

#define M5311_TXD  21
#define M5311_RXD  26
#define M5311_UART_PORT_NUM      1
#define M5311_UART_BAUD_RATE     115200
#define M5311_POWE_GPIO     GPIO_NUM_13  

static const adc_channel_t chan = ADC_CHANNEL_7;     //GPIO35 if ADC1
static const adc_bits_width_t wid = ADC_WIDTH_BIT_12;
static const adc_atten_t att = ADC_ATTEN_DB_2_5;
esp_adc_cal_characteristics_t adc_chars;
uint32_t vol = 0;

using namespace std; //使用命名空间 std

uint8_t scope = 1; //1:在范围内
uint8_t new_scope = 1;//用于判断scope有没有发生变化
std::string socpe_wifi="";  //需要监控的wifi
char m5311_con;  // 1：m5311连接onenet  0：断开连接

static const char *TAG = "ESP32_GPS";

//std::string mqtt_set = "AT+MQTTCFG=\"studio-mqtt.heclouds.com\",1883,\"ttty\",60,\"6NudNI7L5R\",\"version=2018-10-31&res=products%2F6NudNI7L5R%2Fdevices%2Fttty&et=4783367387&method=md5&sign=Ew%2BED%2BBDUxPYzx6%2FUGr69g%3D%3D\",1\r\n";
std::string mqtt_set ="AT+MQTTCFG=\"mqtts.heclouds.com\",1883,\"m5311_gps\",60,\"6FqaGR4PX6\",\"version=2018-10-31&res=products%2F6FqaGR4PX6%2Fdevices%2Fm5311_gps&et=2535669105&method=md5&sign=u%2BiGRMuUhIy%2Fn632UST1ew%3D%3D\",1\r\n";
std::string mqtt_con ="AT+MQTTOPEN=1,1,0,0,0,\"\",\"\"\r\n";
//设备属性上报响应
//std::string mqtt_sub1 = "AT+MQTTSUB=$sys/6NudNI7L5R/ttty/thing/property/post/reply\r\n";
//std::string mqtt_sub2 = "AT+MQTTSUB=$sys/6NudNI7L5R/ttty/thing/property/get\r\n";
//std::string mqtt_sub3 = "AT+MQTTSUB=$sys/6NudNI7L5R/ttty/thing/property/set\r\n";

//std::string mqtt_getreply="AT+MQTTPUB=$sys/6NudNI7L5R/ttty/thing/property/get_reply,0,0,0,0,\"{\"msg\":\"success\",\"data\":{location_data},\"code\":200,\"id\":010}\"\r\n";

//std::string SUB_pub_gps = "AT+MQTTPUB=$sys/6NudNI7L5R/ttty/thing/property/post,0,0,0,0,\"{\"params\": {\"location\": {\"value\": mylocat}},\"id\": \"1\"}\"\r\n";
//std::string SUB_pub_wifi = "AT+MQTTPUB=$sys/6NudNI7L5R/ttty/thing/property/post,0,0,0,0,\"{\"params\": {\"$OneNET_LBS_WIFI\": {\"value\": {\"macs\": \"scan_data\"}}},\"id\": \"1\"}\"\r\n";
//设备属性上报响应
std::string mqtt_sub1 = "AT+MQTTSUB=$sys/6FqaGR4PX6/m5311_gps/thing/property/post/reply\r\n";
//设备属性获取响应
std::string mqtt_sub2 = "AT+MQTTSUB=$sys/6FqaGR4PX6/m5311_gps/thing/property/get\r\n";
//设备属性设置响应
std::string mqtt_sub3 = "AT+MQTTSUB=$sys/6FqaGR4PX6/m5311_gps/thing/property/set\r\n";

std::string mqtt_getreply="AT+MQTTPUB=$sys/6FqaGR4PX6/m5311_gps/thing/property/get_reply,0,0,0,0,\"{\"msg\":\"success\",\"data\":{location_data},\"code\":200,\"id\":010}\"\r\n";
//发送gps数据
std::string SUB_pub_gps = "AT+MQTTPUB=$sys/6FqaGR4PX6/m5311_gps/thing/property/post,0,0,0,0,\"{\"params\": {\"gps\": {\"value\": mylocat}},\"id\": \"1\"}\"\r\n";
//发送wifi数据
std::string SUB_pub_wifi = "AT+MQTTPUB=$sys/6FqaGR4PX6/m5311_gps/thing/property/post,0,0,0,0,\"{\"params\": {\"$OneNET_LBS_WIFI\": {\"value\": {\"macs\":\"scan_data\"}}},\"id\": \"1\"}\"\r\n";
//发送附近wifi名称
std::string SUB_pub_wifi_name = "AT+MQTTPUB=$sys/6FqaGR4PX6/m5311_gps/thing/property/post,0,0,0,88,";
//wifi名称需转16进制
std::string wifi_n = "{\"params\": {\"w_name\": {\"value\":\"wifi_name_data\"}},\"id\": \"1\"}";

//发送socp 监控wifi名称
std::string SUB_pub_socp_wifi = "AT+MQTTPUB=$sys/6FqaGR4PX6/m5311_gps/thing/property/post,0,0,0,88,";
//wifi名称需转16进制
std::string socpe_wifi_n="{\"params\": {\"socpe\": {\"value\": {\"monitoring_wifi\":\"socp_wifi\",\"socpe\":int}}},\"id\": \"1\"}";

//std::string scan_d = "";
std::string wifidata="";  //发送的wifi数据
std::string wifi_name_data="";  //发送的wifi数据

//bool wifi_flag=0;
//bool wifi_send=0;
uint8_t wifi_new = 0; //1:wifi数据有更新


std::string gps_data="{\"latitude\":5,\"GPS\":3,\"mac\":6,\"longitude\":4,\"electricity\":vol}";
std::string lat="-1";   //纬度
std::string lon="-1";   //经度
std::string gpsdata=""; //发送的gps数据
uint8_t gps_new = 0; //1:gps数据有更新
uint8_t flag_gps = 0;   //位置有效标志。0=数据无效 1=数据有效

bool m5311_ready = false;
bool start_w = false;   // 开启wifi 扫描 
int real = 0;
int updata = 0;
int wifi = 0;

void rest_m5311();
#define BUF_SIZE (1024)

//将 字符串 转 16进制 在发送中文时需要
string tohex(string str)
    {
    string ret;
    static const char *hex="0123456789ABCDEF";
    for(int i=0;i!=str.size();++i)
        {
        ret.push_back(hex[(str[i]>>4)&0xf]);
        ret.push_back( hex[str[i]&0xf]);
        }
    return ret;
    }


void uart_init(){
    /* gps uart init*/
   uart_config_t uart_gps_config = {
        .baud_rate = GPS_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;
    ESP_ERROR_CHECK(uart_driver_install(GPS_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_PORT_NUM, &uart_gps_config));
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART_PORT_NUM, GPS_TXD, GPS_RXD, -1, -1));
  /*m5311 uart init*/
  uart_config_t uart_m5311_config = {
        .baud_rate = M5311_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    ESP_ERROR_CHECK(uart_driver_install(M5311_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(M5311_UART_PORT_NUM, &uart_m5311_config));
    ESP_ERROR_CHECK(uart_set_pin(M5311_UART_PORT_NUM, M5311_TXD, M5311_RXD, -1, -1));

}

static void gps_task(void *arg)
{
     
     vTaskDelay(1000 / portTICK_RATE_MS);
     std::string a = "$PCAS03,0,0,0,0,9,0,0,0,0,0,,,0,0,,,,0*3B\r\n";
     uart_write_bytes(GPS_UART_PORT_NUM, a.c_str(), a.length());
     vTaskDelay(500 / portTICK_RATE_MS);
     a = "$PCAS04,1*18\r\n";
     uart_write_bytes(GPS_UART_PORT_NUM, a.c_str(), a.length());
     vTaskDelay(500 / portTICK_RATE_MS);

    char *datas = (char *) malloc(BUF_SIZE);
    std::string newgpsdata;
    std::string data;
    uart_flush_input(GPS_UART_PORT_NUM);
    while (1) {
        // Read data from the UART
        int len = uart_read_bytes(GPS_UART_PORT_NUM, datas, (BUF_SIZE - 1), 100 / portTICK_RATE_MS);
        if (len) {
            datas[len] = '\0';
            ESP_LOGI(TAG, "Recv gps: %s", (char *) datas);
            data = datas;
           // data = "$GPRMC,235316.000,A,3004.75047,S,12134.765,E,0.009,75.020,020711,,,A*45";//测试数据
            for (int i = 0; i < 6;i++){
              if (data.find(",") >=0){
                newgpsdata = data.substr(0, data.find(","));
                data = data.substr(data.find(",") + 1, data.length());
                if (i==2){
                  if(newgpsdata == "A"){flag_gps = 1;}
                  else{
                    flag_gps = 0;          
                    break;
                    }
                }
                if (i==3){ //不计算，让app计算
                  //string a = newgpsdata.substr(0, newgpsdata.find("."));
                 // string b = newgpsdata.substr(newgpsdata.find(".") + 1);
                  /*************计算
                  // string c = to_string(atoi((a.substr(2)+b).c_str()));
                  // c = c.substr(0,2) + "." + c.substr(2);
                  // float d = atof(c.c_str())/60;
                  // lat = a.substr(0, 2) + "." + to_string(d).substr(2);
                  ***************/
                 // lat = a.substr(0, 2) + "." + a.substr(2) + b;
                 lat = newgpsdata;
                  ESP_LOGI(TAG, "gps_lat: %s", lat.c_str());
                }
                if (i==5){ //不计算，让app计算
                 // string a = newgpsdata.substr(0, newgpsdata.find("."));
                 // string b = newgpsdata.substr(newgpsdata.find(".") + 1);
                  //  string c = to_string(atoi((a.substr(3)+b).c_str()));
                  // c = c.substr(0,2) + "." + c.substr(2);
                  // float d = atof(c.c_str())/60;
                  // lon = a.substr(0, 3) + "." + to_string(d).substr(2);
                  //lon = a.substr(0, 3) + "." + a.substr(3) + b;
                  lon = newgpsdata;
                  ESP_LOGI(TAG, "gps_lon: %s", lon.c_str());
                }
              }
              else
              {
                break;
              }
            }
          newgpsdata  = gps_data;
          gpsdata = SUB_pub_gps;
          gps_new = 1;
          if(flag_gps == 1){
            newgpsdata  =newgpsdata.replace(newgpsdata.find("3"),1, "1");
            newgpsdata  =newgpsdata.replace(newgpsdata.find("4"),1, lon);
            newgpsdata  =newgpsdata.replace(newgpsdata.find("5"),1, lat);
            newgpsdata  =newgpsdata.replace(newgpsdata.find("6"),1, "0");
            gpsdata  = gpsdata.replace(gpsdata.find("mylocat"),7,newgpsdata);
          }else{
            newgpsdata = newgpsdata.replace(newgpsdata.find("3"),1, "0");
            newgpsdata = newgpsdata.replace(newgpsdata.find("6"),1, "1");
            gpsdata = gpsdata.replace(gpsdata.find("mylocat"),7,newgpsdata);
          }
      //   ESP_LOGI("gps","%s",gpsdata.c_str());
        }
      vTaskDelay(500/portTICK_RATE_MS);
    }
}
/*
    mesg:要发送的数据
    back:发送的数据应该返回的数据
*/
esp_err_t send_m5311_mesg(const char* mesg, char* back){
    esp_err_t re = ESP_FAIL;
    char *datas = (char *) malloc(BUF_SIZE);
    uart_write_bytes(M5311_UART_PORT_NUM,mesg, strlen(mesg));
    // if(m5311_ready == false){
      int len = uart_read_bytes(M5311_UART_PORT_NUM, datas, (BUF_SIZE - 1), 500 / portTICK_RATE_MS);
      if (len) {
        datas[len] = '\0';
        ESP_LOGI("m5311", "%s",datas);
        string mdata = datas;
        if(mdata.find(back) != -1){
          re = ESP_OK;
        }
      }

   //  }   
    free(datas);
    return re;
}

void m5311_init(){
   ESP_LOGI("m5311", "init");
   uint8_t err = 0;
   uint8_t num = 1;
  uint8_t ready = 0;
   while(ready == 0){
      switch (num)      // err 超过5次 将重启 m5311
      {
      case 1:
            while(send_m5311_mesg("AT\r\n","OK")){
              err ++ ;
              if(err>100) {num = 0;ready = 3;break;}
              vTaskDelay(2000/portTICK_RATE_MS);
            }
            err = 0;
            num++;
        break;
      case 2: //发送  AT+SM=LOCK 返回 OK    禁止休眠
            while(send_m5311_mesg("AT+SM=LOCK\r\n","OK")){
              err ++ ;
              if(err>5) {num = 0;ready = 3;break;}
              vTaskDelay(5000/portTICK_RATE_MS);
            }
            err = 0;
            num++;
        break;
      case 3: //发送  AT+CEREG? 返回 +CEREG: 0,1   检查是否连上网
            while(send_m5311_mesg("AT+CEREG?\r\n","1")){
              err ++ ;
              if(err>5) {num = 0;break;}
              vTaskDelay(5000/portTICK_RATE_MS);
            }
            err = 0;
            num++;
        break;
      case 4: //发送 mqtt_set 返回 OK   配置 MQTT 连接参数
            if(send_m5311_mesg("AT+MQTTCFG?\r\n","null") ==ESP_OK){   //是否已 配置 MQTT 连接参数
              while(send_m5311_mesg(mqtt_set.c_str(),"OK")){
                err ++ ;
                if(err>5) {num = 0;ready = 3;break;}
                vTaskDelay(5000/portTICK_RATE_MS);
              }
            };          
            err = 0;
            num++;
        break;
      case 5: //发送 mqtt_con 返回 OK   连接 onemqtt
            while(send_m5311_mesg(mqtt_con.c_str(),"OK")){
              err ++ ;
              if(err>5) {num = 0;ready = 3;break;}
              vTaskDelay(5000/portTICK_RATE_MS);
            }
            err = 0;
            num++;
            vTaskDelay(1000/portTICK_RATE_MS);
        break;
      case 6: //发送 mqtt_sub1 返回 OK   订阅sub1消息
            while(send_m5311_mesg(mqtt_sub1.c_str(),"OK")){
              err ++ ;
              if(err>5) {num = 0;ready = 3;break;}
              vTaskDelay(5000/portTICK_RATE_MS);
            }
            err = 0;
            num++;
            vTaskDelay(1000/portTICK_RATE_MS);
        break;
      case 7: //发送 mqtt_sub2 返回 OK   订阅sub2消息
            while(send_m5311_mesg(mqtt_sub2.c_str(),"OK")){
              err ++ ;
              if(err>5) {num = 0;ready = 3;break;}
              vTaskDelay(5000/portTICK_RATE_MS);
            }
            err = 0;
            num++;
            vTaskDelay(1000/portTICK_RATE_MS);
        break;
      case 8: //发送 mqtt_sub3 返回 OK   订阅sub3消息
            while(send_m5311_mesg(mqtt_sub3.c_str(),"OK")){
              err ++ ;
              if(err>5) {num = 0;ready = 3;break;}
              vTaskDelay(5000/portTICK_RATE_MS);
            }
            err = 0;
            num++;
        break;
      case 9:
          ready = 1;
          m5311_con = 1;
          m5311_ready = true;
          break;
      default:
        break;
      }
   }
   if(ready == 3){
      rest_m5311();
   }
}

esp_err_t  check_m5311(){
     esp_err_t re = ESP_FAIL;
   char *m5311_datas = (char *) malloc(BUF_SIZE);
    uart_flush_input(M5311_UART_PORT_NUM);
   
    char *mesg = "AT+MQTTSTAT?\r\n";
    uart_write_bytes(M5311_UART_PORT_NUM,mesg, strlen(mesg));
    int len = uart_read_bytes(M5311_UART_PORT_NUM, m5311_datas, (BUF_SIZE - 1), 500 / portTICK_RATE_MS);
    if (len) {
      string mdata = m5311_datas;
      if(mdata.find("+MQTTSTAT: 5") != -1){
            re =  ESP_OK;
        }
    free(m5311_datas);
   
  }
       
    return re; 
}


//重启m5311
void rest_m5311(){
    //关闭m5311

    ESP_LOGW("关闭m5311","11");
    gpio_set_level(M5311_POWE_GPIO,1);
    vTaskDelay(9000 / portTICK_RATE_MS);   //拉低9s
    gpio_set_level(M5311_POWE_GPIO,0);     
    vTaskDelay(20000 / portTICK_RATE_MS);   //释放20s

   

      for(int i=0;i<15;i++){  //延时2.5分钟
        vTaskDelay(10000 / portTICK_RATE_MS);   //10s
      }
    
      //开启m5311
      ESP_LOGW("开启m5311","11");
      gpio_set_level(M5311_POWE_GPIO,1);    
      vTaskDelay(1500 / portTICK_RATE_MS);   //拉低1.5s
      gpio_set_level(M5311_POWE_GPIO,0);
      vTaskDelay(1000 / portTICK_RATE_MS);

      m5311_init();
    
   
}

static void m5311_task(void *arg)
{
  
   // uint32_t check_sub=0; //当 25分钟没收到消息 ，检查是否订阅消息
    uint32_t check_rest=0; //25分钟检查时间
    uint32_t m5311_rest = 0;
   
  //  string c_sub = "AT+MQTTSUB?\r\n";  //是否订阅消息
    string rest_mesg = "AT+CCLK?\r\n";//检查m5311时间
    string stop_con_mesg ="AT+MQTTDISC\r\n"; //主动断开连接

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1ULL << M5311_POWE_GPIO;        
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;   
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    //开启m5311
    gpio_set_level(M5311_POWE_GPIO,1);
    vTaskDelay(1500 / portTICK_RATE_MS);
    gpio_set_level(M5311_POWE_GPIO,0);
    vTaskDelay(1000 / portTICK_RATE_MS);
   
   /**** 检查m5311是否已连上onenet 如没有 就 init()****/ 
    while(check_m5311() == ESP_FAIL){
       m5311_init();
    }

    ESP_LOGI("m5311", "reday");
    m5311_con = 1;
    m5311_ready = true;
    char *m5311back_datas = (char *) malloc(BUF_SIZE);
    while (1)
    {   
      if(m5311_ready || (m5311_con==0)){
        int len = uart_read_bytes(M5311_UART_PORT_NUM, m5311back_datas, (BUF_SIZE - 1), 200 / portTICK_RATE_MS);
        if(len){           
            m5311back_datas[len] = '\0';
            string mdata = m5311back_datas;
            ESP_LOGW("back","%s",m5311back_datas);
            // 功能（更新 实时更新 显示wifi）通知
            if(mdata.find("fun") != -1){
             // check_sub =  0;
              string jdata = mdata.substr(mdata.find("{"));
             // ESP_LOGW("json","%s",jdata.c_str());
              cJSON *pJsonRoot = cJSON_Parse(jdata.c_str());
               if (pJsonRoot !=NULL){
                  //  ESP_LOGW("JSON","%s",cJSON_Print(pJsonRoot));
                   cJSON *param = cJSON_GetObjectItem(pJsonRoot, "params");
                   if(param !=NULL){
                      cJSON *fun = cJSON_GetObjectItem(param, "fun");
                      if(fun !=NULL){
                       cJSON *real_1 = cJSON_GetObjectItem(fun,"real");
                       if(real_1 !=NULL){
                        real  = real_1 ->valueint;
                       }
                       cJSON *updata_1 = cJSON_GetObjectItem(fun,"updata");
                      if(updata_1 !=NULL){
                        updata  = updata_1 ->valueint;
                       }
                        cJSON *wifi_1 = cJSON_GetObjectItem(fun,"wifi");
                      if(wifi_1 !=NULL){
                        wifi  = wifi_1 ->valueint;
                       }                     
                        ESP_LOGW("data","real:%d updata:%d wifi:%d",real,updata,wifi);   
                                       
                      } 
                                  
                   }
                 
                cJSON_Delete(pJsonRoot);
               }
            }
            // 设置 监控wifi 通知
            if(mdata.find("socpe") != -1){
            //  check_sub =  0;
               string jdata = mdata.substr(mdata.find("{"));
             // ESP_LOGW("json","%s",jdata.c_str());
              cJSON *pJsonRoot = cJSON_Parse(jdata.c_str());
               if (pJsonRoot !=NULL){
                  //  ESP_LOGW("JSON","%s",cJSON_Print(pJsonRoot));
                   cJSON *param = cJSON_GetObjectItem(pJsonRoot, "params");
                   if(param !=NULL){
                      cJSON *socpe = cJSON_GetObjectItem(param, "socpe");
                      if(socpe !=NULL){
                          cJSON *socpe_wifi_1 = cJSON_GetObjectItem(socpe,"monitoring_wifi");
                          if(socpe_wifi_1 != NULL){
                            esp_err_t ret = esp_storage_set("socpe_wifi", socpe_wifi_1->valuestring, 50);
                            if(ret == ESP_OK){
                                ESP_LOGW("socpe_wifi_strge","OK");
                            }
                            socpe_wifi = socpe_wifi_1->valuestring;
                            ESP_LOGW("socpe_wifi","%s",socpe_wifi.c_str());
                            string n_socpe = socpe_wifi_n;
                            n_socpe.replace(n_socpe.find("socp_wifi"),9,socpe_wifi);
                            n_socpe.replace(n_socpe.find("int"),3,"1");
                            string a = tohex(n_socpe);
                            string socpe_dd = SUB_pub_socp_wifi;
                            socpe_dd = socpe_dd.replace(socpe_dd.find("88"),2,to_string((a.size())/2)) + a +"\r\n";
                            uart_write_bytes(M5311_UART_PORT_NUM,socpe_dd.c_str(), strlen(socpe_dd.c_str()));  //设置socpe_wifi_name     
                          }
                        
                      }
                   }
                    cJSON_Delete(pJsonRoot);
               }  
            }
            //检查 sub
            // if(mdata.find("MQTTSUB?") != -1){
            //   if(mdata.find("property/set") ==-1){   //重新订阅             
            //       uart_write_bytes(M5311_UART_PORT_NUM,mqtt_sub1.c_str(), strlen(mqtt_sub1.c_str())); 
            //       uart_write_bytes(M5311_UART_PORT_NUM,mqtt_sub2.c_str(), strlen(mqtt_sub2.c_str())); 
            //       uart_write_bytes(M5311_UART_PORT_NUM,mqtt_sub3.c_str(), strlen(mqtt_sub3.c_str()));                 
            //   }
            // }
            //掉线
            if(mdata.find("+MQTTDISC: OK") != -1){
                m5311_ready = false;
                ESP_LOGW("loss","loss");
            }
            if(mdata.find("+CCLK:") != -1){
              string m5311_time = mdata.substr(mdata.find(",")+1);
              m5311_time = m5311_time.substr(0,2);
              int tim = stoi(m5311_time);
              if((tim == 7)||(tim == 2)){     //重启
                if(m5311_rest==0){
                  m5311_rest = 1;
                  ESP_LOGW("m5311","rest");
                  m5311_ready = false;
                  rest_m5311();
                 
                }
                
              }else{
                m5311_rest = 0;
              }
            //  if(15<tim && tim<22){    //关闭m5311 断开onenet连接 
              if(12==tim){    //关闭m5311
                if(m5311_con == 1){
                    m5311_con = 0;
                    m5311_ready = false;
                    ESP_LOGW("m5311","stop");
                    uart_write_bytes(M5311_UART_PORT_NUM,stop_con_mesg.c_str(), strlen(stop_con_mesg.c_str())); 
                  }                 
              }else{
                if(m5311_con == 0){  //重启
                    m5311_con = 1;
                    ESP_LOGW("m5311","start");
                    rest_m5311();
                }                 
              }             
               ESP_LOGW("m5311_time","%d",tim);
            }
           // uart_flush(M5311_UART_PORT_NUM);          
        }
      //  check_sub ++;
        check_rest ++;
        //因为一天重启2次 不再检查
        // if(check_sub >7500){  //25分钟检查一次sub  
        //   check_sub = 0;
        //   // string tt = "AT+CGSN=?\r\n";
        //   // uart_write_bytes(M5311_UART_PORT_NUM,tt.c_str(), strlen(tt.c_str())); 
        //   uart_write_bytes(M5311_UART_PORT_NUM,c_sub.c_str(), strlen(c_sub.c_str())); 
        // }
        if(check_rest >7500){  //25分钟检查时间 如果时间是 07 02（真实时间+8 （10点 15点））就重启 m5311 (一天2次)  晚上23点 到 明早 6点 关闭(15 22)
          check_rest = 0;
          uart_write_bytes(M5311_UART_PORT_NUM,rest_mesg.c_str(), strlen(rest_mesg.c_str())); 
        }

       
      }else{
        while(check_m5311() == ESP_FAIL){
          m5311_init();
        }
        m5311_ready = true;
      }    
      vTaskDelay(200 / portTICK_RATE_MS);
    }
    

}


/* Initialize Wi-Fi as sta and set scan method */
static void wifi_scan(void *arg)
{
    string scan_data="";
    string wifi_name="";
     string wifi_name_1="";
    string ssid;
    string rssi;
    char mac[20];
    uint8_t scan_time=0;   //每隔多少分钟主动扫描，检查是否在范围内
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    uint16_t scan_number = 5;     //wifi扫描结果最大数量
    wifi_ap_record_t ap_info[scan_number];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
   

    while(1){

        if(start_w && m5311_con ==1){
          scan_time = 0;
          ESP_ERROR_CHECK(esp_wifi_start());
          esp_wifi_scan_start(NULL, true);
          ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&scan_number, ap_info));
          ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
          ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
          esp_wifi_scan_stop();
          ESP_ERROR_CHECK(esp_wifi_stop());
          new_scope = 0;
          for (int i = 0; (i < scan_number) && (i < ap_count); i++) {
             // ESP_LOGI(TAG, "SSID \t%s RSSI \t%d", ap_info[i].ssid,ap_info[i].rssi);
              ssid =(char *) ap_info[i].ssid;
              if(socpe_wifi == ssid || socpe_wifi ==""){
                new_scope = 1;               
                //ESP_LOGI(TAG, "in_here");
              }
             sprintf(mac,"%x:%x:%x:%x:%x:%x",ap_info[i].bssid[0],ap_info[i].bssid[1],ap_info[i].bssid[2],ap_info[i].bssid[3],ap_info[i].bssid[4],ap_info[i].bssid[5]);             
              rssi =std::to_string(ap_info[i].rssi);
              scan_data  = scan_data + mac + "," +rssi;
              if(scan_number -i >1 )  scan_data = scan_data + "|";  
              wifi_name  = wifi_name + ssid +"," + rssi +";";
          }
          if(new_scope != scope){   //范围更改发送
              scope = new_scope;
              if(socpe_wifi != ""){
                string n_socpe = socpe_wifi_n;
                n_socpe.replace(n_socpe.find("socp_wifi"),9,socpe_wifi);
                n_socpe.replace(n_socpe.find("int"),3,to_string(scope));
                string a = tohex(n_socpe);
                string socpe_dd = SUB_pub_socp_wifi;
                socpe_dd = socpe_dd.replace(socpe_dd.find("88"),2,to_string((a.size())/2)) + a +"\r\n";
                uart_write_bytes(M5311_UART_PORT_NUM,socpe_dd.c_str(), strlen(socpe_dd.c_str()));
     
              }
          }
          scan_number = 5;
          memset(ap_info, 0, sizeof(ap_info));
          

         // wifi_flag=1;
          wifi_new = 1;
          wifidata = SUB_pub_wifi;
          wifidata =  wifidata.replace(wifidata.find("scan_data"),9,scan_data);
          scan_data = "";

         wifi_name_data = SUB_pub_wifi_name;
          wifi_name_1 = wifi_n;
          wifi_name_1 = wifi_name_1.replace(wifi_name_1.find("wifi_name_data"),14,wifi_name);
        //  ESP_LOGI(TAG, "wifi_name_data:%s", wifi_name_1.c_str());
          string a = tohex(wifi_name_1);
         wifi_name_data = wifi_name_data.replace(wifi_name_data.find("88"),2,to_string((a.size())/2)) + a +"\r\n";
         wifi_name  = "";
        // ESP_LOGI(TAG, "wifi__data:%s", wifi_name_data.c_str());
         if(wifi){     //app端显示wifi 才发送
             uart_write_bytes(M5311_UART_PORT_NUM,wifi_name_data.c_str(), strlen(wifi_name_data.c_str()));
         }        
          start_w = false;
          vTaskDelay(1000/portTICK_RATE_MS);
        }     
        scan_time++;
        if(scan_time>=120){     //每隔 2 分钟 主动扫描一次
          start_w = true;
        }
        vTaskDelay(1000/portTICK_RATE_MS);
    }
}


static void fun_task(void *arg){
  uint32_t v;
    while (1)
    {
      //更新一次
      if(updata){
          if(gpsdata.find("vol") != -1){
              esp_adc_cal_get_voltage(chan, &adc_chars, &vol);
              v = vol * 3.25;       
              gpsdata = gpsdata.replace(gpsdata.find("vol"),3,to_string(v));
  
          }
        //是否有gps数据
        if(flag_gps){
          uart_write_bytes(M5311_UART_PORT_NUM,gpsdata.c_str(), strlen(gpsdata.c_str()));        
          updata = 0;
          if(wifi){
            start_w = true;
          }
        }else{
          start_w = true;
          if(wifi_new){
            uart_write_bytes(M5311_UART_PORT_NUM,gpsdata.c_str(), strlen(gpsdata.c_str()));
            uart_write_bytes(M5311_UART_PORT_NUM,wifidata.c_str(), strlen(wifidata.c_str()));
            wifi_new = 0;
            updata = 0;
          }
        }
       
      }
      //实时更新  30s
      while(real){
        esp_adc_cal_get_voltage(chan, &adc_chars, &vol);
        v = vol * 3.25;
        gpsdata = gpsdata.replace(gpsdata.find("vol"),3,to_string(v));
        if(flag_gps){
          uart_write_bytes(M5311_UART_PORT_NUM,gpsdata.c_str(), strlen(gpsdata.c_str()));
          if(wifi){
            start_w = true;
          }
        }else{
          start_w = true;
          if(wifi_new){
            uart_write_bytes(M5311_UART_PORT_NUM,gpsdata.c_str(), strlen(gpsdata.c_str()));
            uart_write_bytes(M5311_UART_PORT_NUM,wifidata.c_str(), strlen(wifidata.c_str()));    
            wifi_new = 0;          
          }
        }       
        vTaskDelay(30000/portTICK_RATE_MS);
      }
      vTaskDelay(2000/portTICK_RATE_MS);
    }
    
}
/******************* C 语言 用下面2个函数 *****************/

/*在字符串中查找字符，返回查到的第一个下标，否则返回-1
参数：pSrc源字符串   pdst要查找的字符串
*/
// int StringFind(const char *pSrc, const char *pDst)
// {
// 	int i, j;
// 	for (i=0; pSrc[i]!='\0'; i++)
// 	{
// 		if(pSrc[i]!=pDst[0])
// 			continue;		
// 		j = 0;
// 		while(pDst[j]!='\0' && pSrc[i+j]!='\0')
// 		{
// 			j++;
// 			if(pDst[j]!=pSrc[i+j])
// 			break;
// 		}
// 		if(pDst[j]=='\0')
// 			return i;
// 	}
// 	return -1;
// }

/*字符串 replace
    参数：original 源字符串
         pattern  要替代的字符串
         replacement 替代的字符串
    返回 字符串
*/
// char * strreplace(char const * const original, 
//     char const * const pattern, char const * const replacement) 
// {
//   size_t const replen = strlen(replacement);
//   size_t const patlen = strlen(pattern);
//   size_t const orilen = strlen(original);

//   size_t patcnt = 0;
//   const char * oriptr;
//   const char * patloc;

//   // find how many times the pattern occurs in the original string
//   for (oriptr = original; (patloc = strstr(oriptr, pattern)); oriptr = patloc + patlen)
//   {
//     patcnt++;
//   }

//   {
//     // allocate memory for the new string
//     size_t const retlen = orilen + patcnt * (replen - patlen);
//     char * const returned = (char *) malloc( sizeof(char) * (retlen + 1) );

//     if (returned != NULL)
//     {
//       // copy the original string, 
//       // replacing all the instances of the pattern
//       char * retptr = returned;
//       for (oriptr = original; (patloc = strstr(oriptr, pattern)); oriptr = patloc + patlen)
//       {
//         size_t const skplen = patloc - oriptr;
//         // copy the section until the occurence of the pattern
//         strncpy(retptr, oriptr, skplen);
//         retptr += skplen;
//         // copy the replacement 
//         strncpy(retptr, replacement, replen);
//         retptr += replen;
//       }
//       // copy the rest of the string.
//       strcpy(retptr, oriptr);
//     }
//     return returned;
//   }	
// }


extern "C" void app_main()
{

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
  ESP_ERROR_CHECK( ret );
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  char socpe_wifi_1[50];
  ret = esp_storage_get("socpe_wifi", socpe_wifi_1, sizeof(socpe_wifi_1));
  if(ret == ESP_OK){
    ESP_LOGW("stroge","%s",socpe_wifi_1);
     socpe_wifi = socpe_wifi_1;
  }

  uart_init();

  adc1_config_width(wid);
  adc1_config_channel_atten((adc1_channel_t)chan, att);

  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, att, wid, 1100, &adc_chars);
  ESP_LOGW("ADC","%d",val_type);
  esp_adc_cal_get_voltage(chan, &adc_chars, &vol);
  ESP_LOGW("ADC_vol","%d",vol);
  xTaskCreate(wifi_scan, "wifi_scan_task", 1024*5, NULL, 10, NULL);
  xTaskCreate(gps_task, "gps_task", 1024*5, NULL, 2, NULL);
  xTaskCreate(m5311_task, "m5311_task", 1024*5, NULL, 15, NULL);
  xTaskCreate(fun_task, "fun_task", 1024*2, NULL, 12, NULL);

 
}
