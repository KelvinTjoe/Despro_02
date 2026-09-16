gunakan library yang tertera pada lib_deps dengan inisialisasi board esp32dev pada platform.io dalam visual studio code

[env]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
	sandeepmistry/LoRa@^0.8.0
	tinyu-zhao/TinyGPSPlus-ESP32@^0.0.2

[env:field]
build_src_filter = -<*> +<LoraFieldNode.cpp>

[env:relay]
build_src_filter = -<*> +<LoraRelayNode.cpp>

[env:command]
build_src_filter = -<*> +<LoraCommandNode.cpp>