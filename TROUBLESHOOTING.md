# 1.#error "Unknown Slave Target"
  查看版本：echo $env:ESP_IDF_VERSION
  修改版本：$env:ESP_IDF_VERSION="5.5"
  操作：
  修改版本：$env:ESP_IDF_VERSION="5.5"
  删除旧文件：Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
  Remove-Item sdkconfig,sdkconfig.old -Force -ErrorAction SilentlyContinue
  原因是工程里面实际目录地址不一样，如下：managed_components/espressif__esp_wifi_remote/idf_v5.5/
# 2.A fatal error occurred: bootloader/bootloader.bin requires chip revision in range [v3.1 - v3.99] (this chip is revision v1.3)
  操作如下：
  进入SDK配置界面：idf.py menuconfig
  修改配置：
  Component config
        >Hardware Setting
             >Chip revision
                  >Select ESP32-P4
  退出保存
  Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
  idf.py reconfigure
  idf.py build flash monitor



