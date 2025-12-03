graph TD
    subgraph "Hardware Abstraction Layer (HAL)"
        M_TEMP[Mock Temp] -->|I_Temp| T_SENS[Sensor Task]
        M_PIR[Mock PIR] -->|I_PIR| T_SENS
    end

    subgraph "Core Logic"
        T_SENS -->|Queue: SensorData| T_CTRL[Control Task]
        NTP[NTP Service] -->|Time| T_CTRL
        NVS[NVS Storage] <-->|Config| T_CTRL
        WEB[Web Server] -->|Queue: ConfigUpdate| T_CTRL
    end

    subgraph "Actuators"
        T_CTRL -->|Queue: PWMCommand| T_ACT[Actuator Task]
        T_ACT -->|I_Fan| M_FAN[Mock Fan]
    end