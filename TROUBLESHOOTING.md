# 1.#error "Unknown Slave Target"
  查看版本：echo $env:ESP_IDF_VERSION
  修改版本：$env:ESP_IDF_VERSION="5.5"
  操作：
  修改版本：$env:ESP_IDF_VERSION="5.5"
  删除旧文件：Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
  Remove-Item sdkconfig,sdkconfig.old -Force -ErrorAction SilentlyContinue
  原因是工程里面实际目录地址不一样，如下：managed_components/espressif__esp_wifi_remote/idf_v5.5/
# 2.
