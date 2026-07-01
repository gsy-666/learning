# RK TCP Gateway (GM6220 / DM6220)

This folder provides a small TCP server that translates LAN TCP commands into SocketCAN frames using the existing `dm6220_linux.*` code.

## Build (on RK board)

```sh
cd rk_gateway
make
```

## Run

Default: listen on port 6666, use can0, motor_id=0x01, master_id=0x00

```sh
./gm6220_tcp_gateway
```

Custom:

```sh
./gm6220_tcp_gateway --port 9000 --ifname can0 --motor-id 0x01 --master-id 0x00
```

## Test from your PC/phone network

```sh
nc <RK_IP> 6666
PING
ENABLE
VEL 1.0
POSVEL 0.5 1.0
DISABLE
QUIT
```

Commands are line-based (one command per line). Server responds with `OK` / `ERR ...`.
