idf_component_register(
    SRCS "src/ltemc.c" "src/ltemc-atcmd.c" "src/ltemc-files.c" "src/ltemc-geo.c" "src/ltemc-gnss.c" "src/ltemc-gpio.c" "src/ltemc-http.c" "src/ltemc-iop.c"
          "src/ltemc-mqtt.c" "src/ltemc-network.c" "src/ltemc-nxp-sc16is.c" "src/ltemc-quectel-bg.c" "src/ltemc-sckt.c" "src/ltemc-tls.c" 
    INCLUDE_DIRS "src" 
    REQUIRES "freertos"
)