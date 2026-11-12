# ImmortalWrt by MYOS

MYOS 是基于 ImmortalWrt 的 OpenWrt 固件。先确认路由器的完整型号，再选对应的源码分支或固件。六款机型的成品都在 [Nightly 发布页](https://github.com/561410590/immortalwrt-by-MYOS/releases/tag/nightly)。

| 路由器型号 | 源码分支 | OpenWrt 设备配置 |
| --- | --- | --- |
| C8-688 | [main](https://github.com/561410590/immortalwrt-by-MYOS/tree/main) | `nradio_c8-688` |
| C5800-688 | [main](https://github.com/561410590/immortalwrt-by-MYOS/tree/main) | `nradio_c5800-688` |
| NBCPE-688 | [main](https://github.com/561410590/immortalwrt-by-MYOS/tree/main) | `nradio_nbcpe-688` |
| C2000MAX | [C2000MAX](https://github.com/561410590/immortalwrt-by-MYOS/tree/C2000MAX) | `nradio_c2000-max` |
| C2000-Ultra | [C2000-Ultra](https://github.com/561410590/immortalwrt-by-MYOS/tree/C2000-Ultra) | `nradio_c2000-ultra` |
| N5000 | [N5000](https://github.com/561410590/immortalwrt-by-MYOS/tree/N5000) | `nradio_n5000` |

## 在 GitHub 上编译

GitHub Actions 定时任务设为每天北京时间 12:00（UTC 04:00）。任务拉取最新上游源码和软件包配方，合入对应机型的 MYOS 源码，然后并行编译六款固件。GitHub 排队时，实际开始时间会晚于计划时间。

1. 打开 [MYOS daily firmware](https://github.com/561410590/immortalwrt-by-MYOS/actions/workflows/nightly.yml)。有仓库写权限的维护者可点 **Run workflow**，选 `main` 和目标机型。`all` 编译六款。只想补跑部分机型时，在 `models` 输入逗号分隔的完整型号，例如 `C8-688,C5800-688`。
2. 点开运行记录，再点机型名称。页面会显示源码下载、工具链、内核、软件包、镜像装配和权限检查分别进行到哪一步。第一次构建要准备工具链，可能需要数小时，之后的构建会尝试复用缓存。
3. 任务成功后，**Artifacts** 中有 `firmware-机型` 和 `logs-机型`。发布任务会把成品放到 [Nightly 发布页](https://github.com/561410590/immortalwrt-by-MYOS/releases/tag/nightly)，并更新 `nightly.json`。发布页的表格列出每款固件的版本、构建时间和源码提交，某款本轮失败时会保留它之前成功的版本。

## 在自己的电脑上编译

下列命令复用仓库的云端构建脚本。建议使用 x86_64 的 Ubuntu 22.04、普通用户账户和至少 60 GB 可用空间。Linux 用户可直接从安装依赖开始。Windows 用户先在管理员终端安装 WSL 的 Ubuntu。

```powershell
wsl --install -d Ubuntu-22.04
```

接下来打开 **Ubuntu 22.04**，后续命令都在这个窗口里运行。把源码放在 Ubuntu 的 `~` 下，避免使用 `/mnt/c` 存放构建目录。

### 1. 安装依赖

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential ccache clang flex bison g++ gawk gcc-multilib gettext git \
  libelf-dev libncurses-dev libssl-dev python3 python3-distutils python3-setuptools \
  rsync swig unzip zlib1g-dev file wget curl patch perl tar xz-utils zstd jq \
  libfuse-dev libtool autoconf automake cmake ninja-build device-tree-compiler squashfs-tools
```

### 2. 下载源码和机型构建输入

下面以 C8-688 为例。换机型时，把 `MODEL` 和 `BRANCH` 改成上表同一行里的型号和源码分支。每款机型单独使用一个新的工作目录。

```bash
export MODEL=C8-688
export BRANCH=main
repo=https://github.com/561410590/immortalwrt-by-MYOS.git

mkdir "$HOME/myos-build-$MODEL"
cd "$HOME/myos-build-$MODEL"
git clone --depth 1 --single-branch --branch main --filter=blob:none --sparse "$repo" automation
git -C automation sparse-checkout set .github/ci
git clone --depth 1 --single-branch --branch "$BRANCH" "$repo" source
```

公开仓库提供源码和机型配置。部分机型还需要板级二进制、参考镜像或装配文件，见对应分支的 `BINARY_INPUTS.json`。这些文件由维护者的私有构建仓库提供。没有访问权限时，可以查看公开源码和 Actions 日志，或直接使用 Nightly 发布页的成品。

有私有仓库读取权限的维护者可用已配置的 GitHub SSH 身份取出脚本指定的输入版本。

```bash
inputs_ref=$(jq -r .commit automation/.github/ci/inputs.json)
git init private-inputs
git -C private-inputs fetch --depth 1 git@github.com:561410590/MYOS-build-inputs.git "$inputs_ref"
git -C private-inputs checkout --detach FETCH_HEAD
```

`prepare` 会解包所需文件，然后清理这个工作目录里的 `private-inputs` 副本。请使用上面新建的副本。

### 3. 获取上游源码并构建

在含有 `automation`、`source` 和 `private-inputs` 的工作目录里运行。构建脚本会按 `MODEL` 设置设备配置，自动建立 feeds 索引并执行 `make defconfig`。

```bash
export SOURCE_COMMIT=$(git -C source rev-parse HEAD)
export BUILD_STAMP=$(TZ=Asia/Shanghai date +%Y%m%d%H%M)
export CCACHE_DIR="$PWD/source/.ccache"

(
  set -e
  python3 automation/.github/ci/upstream.py --source source --output logs/upstream.json
  for stage in prepare download tools toolchain kernel packages; do
    python3 automation/.github/ci/build.py "$stage"
  done
  case "$MODEL" in
    C8-688|NBCPE-688) ;; # 这两款在 assemble 阶段使用自己的镜像配方
    *) python3 automation/.github/ci/build.py compile ;;
  esac
  python3 automation/.github/ci/build.py assemble
)
```

`tools` 和 `toolchain` 准备编译程序，`kernel` 和 `packages` 分别编译内核和软件包，`assemble` 组装最终固件并读取权限元数据。首次下载可能较大，请等每一步完成。默认按可用 CPU 数量并行编译。

### 4. 找到固件和日志

- `output/MYOS-<机型>-<版本>-<内核>-sysupgrade.bin` 是最终升级固件。文件名中的机型须和设备完整型号一致。
- `logs/upstream.json` 记录本次拉取的上游提交。各阶段的详细日志也在 `logs/`，例如 `packages.log`。
- `logs/<机型>-permissions/summary.json` 汇总了实际软件包、组装目录和最终镜像的权限读取结果。同目录还保留详细清单，包含文件模式、属主属组及符号链接。
- 如果失败，先打开报错阶段对应的日志，找出失败的包名或文件。依赖安装失败看 `apt` 的错误，软件包失败看 `packages.log`，镜像组装失败看机型对应的装配日志。修正后可在同一工作目录重跑编译或装配步骤。如果 `prepare` 未完成，请从新的工作目录重新准备源码和输入。
