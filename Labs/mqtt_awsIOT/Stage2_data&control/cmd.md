mosquitto_pub -h 192.168.1.82 -t imx6ull/command -m "led:on"



mosquitto_sub -h localhost -t "imx6ull/sensor" -v