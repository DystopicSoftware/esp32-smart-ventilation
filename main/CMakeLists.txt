idf_component_register(SRCS "main.c" 
                            "mocks/mock_sensors.c" 
                            "mocks/mock_fan.c"
                            "tasks/task_sensor.c"
                            "tasks/task_control.c"
                            "storage/config_manager.c"
                            "network/wifi_station.c"
                            "web/web_server.c"          # <--- NUEVO
                       INCLUDE_DIRS "include"
                       REQUIRES nvs_flash esp_wifi esp_event esp_netif lwip esp_http_server json) # <--- NUEVO