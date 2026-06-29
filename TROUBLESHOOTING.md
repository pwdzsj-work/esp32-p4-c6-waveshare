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
  指令操作：
  Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
  idf.py reconfigure
  idf.py build flash monitor

# 3.ninja failed with exit code 1, output of the command is in the
  main/CMakeLists.txt添加自定义组件
  操作如下：
  idf_component_register(SRCS ${SOURCES}
  PRIV_REQUIRES 你自己的组件
  )
# 4.生成esp32-c6固件操作指令
  cd D:\p\work\ESP32-P4\soft

    idf.py create-project-from-example "espressif/esp_hosted:slave"
    cd slave
    idf.py set-target esp32c6
    idf.py menuconfig
    idf.py build
  如果报错，执行如下指令： 
  Get-ChildItem Env:IDF_TARGET
  如果显示 IDF_TARGET    esp32p4
  执行指令：
  Remove-Item Env:IDF_TARGET -ErrorAction SilentlyContinue
  Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
  Remove-Item sdkconfig,sdkconfig.old,dependencies.lock -Force -ErrorAction SilentlyContinue

  烧录遇到问题，按住esp32-p4的复位或EN键


