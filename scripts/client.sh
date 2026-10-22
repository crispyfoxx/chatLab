#!/usr/bin/env bash
set -euo pipefail 
#-e：某条命令执行失败时，脚本尽快退出。
#-u：使用了“未定义变量”时直接报错退出。
#-o pipefail：管道命令中，只要任意一段失败，整个管道就算失败。

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"      # 就是 ~/chat_lab。$0是自身路径，pwd获得绝对路径，最后赋值给POJECT_DIR

CONTAINER_NAME="${1:?Usage: $0 <container-name> [server-host] [server-port]}" # ${1:?错误信息} 第一个参数 container-name 是必填项，如果没传，就告诉用户正确用法。
USER_NAME="${2:?Usage: $0 <container-name> <username> [server-host] [server-port]}" # ${2:?错误信息} 第二个参数 username 是必填项，如果没传，就告诉用户正确用法。
SERVER_HOST="${3:-host.docker.internal}" # ${2:-默认值} 如果 $2 有值就用 $2，否则使用默认值。
SERVER_PORT="${4:-9000}"

docker run --rm -it \
  --name "$CONTAINER_NAME" \
  --network chatnet \
  --add-host=host.docker.internal:host-gateway \
  -v "${PROJECT_DIR}:/work" \
  -w /work \
  chatlab:dev ./build/basic_multithread "$USER_NAME" "$SERVER_HOST" "$SERVER_PORT"

#5 docker开始运行，--rm退出后自动删除容器，-i保持交互，-t分配伪终端
#6 --network指定网络，这个网络一般需要提前存在，例如docker network create chatnet。如果 server 也运行在 chatnet 中，那么这些容器之间可以通过 Docker 网络通信。
#7 --add-host添加host.docker.internal映射到宿主机网关
#8 -v挂载项目目录到容器的/work目录，/work就相当于/chatLab
#9 -w指定工作目录为/work
#10 chatlab:dev是镜像名，./build/basic_multithread是要运行的程序

# chmod +x scripts/client.sh

# 执行脚本：
# bash client.sh client1 Alice
# bash client.sh client2 Bob