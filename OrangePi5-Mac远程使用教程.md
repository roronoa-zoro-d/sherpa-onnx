# Orange Pi 5 + Mac 远程使用教程

> 适用：Orange Pi 5（OPI5 8G，RK3588S）+ 在家 Wi‑Fi + Mac 终端开发  
> 仓库目录：与本文件同级的 `sherpa-onnx/`（含 `run-kws-microphone.sh`）  
> 实测成功：板子 hostname **`orangepi5`**，家里 IP **`192.168.1.50`**，Wi‑Fi **`CMCC-Amf2`**

---

## 1. 你要实现什么

```text
板子上电 → 自动连家里 Wi‑Fi → Mac 同 Wi‑Fi → ssh 登录 → rsync 传代码 → 板子上编译运行
```

**无显示器时** 全程在 Mac 终端完成（板子第一次配置需要接显示器，在公司或借显示器做一次即可）。

---

## 2. 硬件清单

| 物品 | 说明 |
|------|------|
| Orange Pi 5 + SD 卡（已烧系统） | 主设备 |
| 5V 电源（建议 4A） | Type-C 供电 |
| Wi‑Fi 天线 ×2 | 必须接好 |
| Mac | 与板子同一 Wi‑Fi |
| HDMI + 键盘 | **仅首次配网时需要** |
| 网线 | 可选，Wi‑Fi 不稳时插光猫 LAN |

---

## 3. 账号与 Wi‑Fi（自己记，勿提交 git）

| 项目 | 说明 |
|------|------|
| SSH 用户名 | 常见 `orangepi`（板子终端 `whoami`） |
| SSH 密码 | 登录板子桌面 / sudo 用的密码 |
| 家里 Wi‑Fi SSID | `CMCC-Amf2` |
| Wi‑Fi 密码 | Mac 钥匙串或光猫 `192.168.1.1` 查看 |

Mac 查已保存 Wi‑Fi 密码：

```bash
security find-generic-password -D "AirPort network password" -a "CMCC-Amf2" -wa
```

---

## 4. 在公司（或借显示器）一次性配置

> **只做一次**。配好后关机带回家，回家上电即可。

### 4.1 接线

```text
电源 + HDMI + 键盘 + 天线 → 板子
```

### 4.2 删掉所有旧 Wi‑Fi 配置（关键）

多条 `autoconnect` 会互相抢，板子开机先去连不存在的网络（如旧 CMCC、测试 zoro3），**永远连不上家里 Wi‑Fi**。

```bash
nmcli connection show
# 逐个删掉不需要的（名字按实际改，报错可忽略）
sudo nmcli connection delete "CMCC-Amf2" 2>/dev/null
sudo nmcli connection delete "CMCC-ZEwa" 2>/dev/null
sudo nmcli connection delete "home-wifi" 2>/dev/null
sudo nmcli connection delete "zoro" 2>/dev/null
sudo nmcli connection delete "zoro1" 2>/dev/null
sudo nmcli connection delete "zoro2" 2>/dev/null
sudo nmcli connection delete "zoro3" 2>/dev/null
sudo nmcli connection delete "zoro 1" 2>/dev/null
```

### 4.3 只添加 **一条** 家里 Wi‑Fi

```bash
sudo nmcli connection add type wifi con-name "home-wifi" ifname wlan0 \
  ssid "CMCC-Amf2" \
  wifi-sec.key-mgmt wpa-psk \
  wifi-sec.psk "你的家里WiFi密码" \
  connection.autoconnect yes \
  connection.autoconnect-priority 100 \
  ipv4.method auto
```

验证（Orange Pi 上请用下面命令，不要用 `-f 802-11-wireless.ssid`）：

```bash
nmcli -f NAME,AUTOCONNECT connection show
nmcli connection show "home-wifi" | grep -E "autoconnect|ssid"
```

必须：**只剩 `home-wifi`，且 `AUTOCONNECT` 为 `yes`**。

### 4.4 开启 SSH

```bash
sudo apt update
sudo apt install -y openssh-server
sudo systemctl enable --now ssh
sudo ss -tlnp | grep :22
```

应看到 `0.0.0.0:22`。

### 4.5 关机带回家

```bash
sudo poweroff
```

---

## 5. 可选：用手机热点模拟回家（在公司验证）

逻辑与回家相同：**预设 Wi‑Fi → 关板子 → 开热点（模拟家里路由器）→ 板子上电**。

1. 显示器上 **Connect to Hidden Wi-Fi Network...** 填测试 SSID/密码（如 `zoro3`），**或** 用 `nmcli add` 写入。  
2. **删掉其它 Wi‑Fi**，只留测试 SSID，`autoconnect yes`。  
3. 拔显示器，关板子。  
4. **手机先开同名热点**，再板子上电，等 5～10 分钟。  
5. 手机热点应显示 **1 台设备**（板子）→ 模拟成功。  
6. 通过后删掉测试配置，再按 **第 4 节** 只留 `home-wifi`（CMCC-Amf2）。

---

## 6. 回家后：Mac 连接板子

### 6.1 上电等待

1. 板子插电（天线接好），**可不插显示器**。  
2. 等 **5 分钟**（Wi‑Fi 关联 + 启动）。  
3. Mac 连 **`CMCC-Amf2`**。

### 6.2 找板子 IP

**方法 A：光猫后台**

浏览器 `http://192.168.1.1` → LAN 侧地址 → 找 **`orangepi5`**（实测 IP **`192.168.1.50`**）。

**方法 B：Mac 扫描 + 断电对比（最准）**

```bash
ipconfig getifaddr en0

# 板子断电 30 秒
nmap -sn 192.168.1.0/24 | grep "Nmap scan report" > /tmp/off.txt
# 板子上电，等 5 分钟
nmap -sn 192.168.1.0/24 | grep "Nmap scan report" > /tmp/on.txt
diff /tmp/off.txt /tmp/on.txt
```

多出来的 IP 就是板子。

**怎么读 nmap 结果：**

| IP | 通常是什么 |
|----|------------|
| `192.168.1.1` | 光猫 |
| `192.168.1.10` | Mac（以你机器为准） |
| `192.168.1.50` | **板子 orangepi5**（DHCP 可能变，不要写死） |
| 其它 | 手机、电视等 |

### 6.3 SSH 登录

```bash
export BOARD_IP=192.168.1.50    # 改成你扫到的 IP
export BOARD_USER=orangepi

ping -c 3 $BOARD_IP
ssh ${BOARD_USER}@${BOARD_IP}
```

首次输入 `yes`，再输密码。

| 报错 | 含义 |
|------|------|
| `Connection refused` | SSH 未开 → 接显示器执行第 4.4 节 |
| `Permission denied` | 用户名或密码错 |
| `Operation timed out` | IP 不对或板子未联网 |

### 6.4（可选）Mac SSH 快捷方式

编辑 `~/.ssh/config`：

```
Host opi5
    HostName 192.168.1.50
    User orangepi
    StrictHostKeyChecking accept-new
```

以后：`ssh opi5`（IP 变了就改 `HostName`）。

---

## 7. Mac 传代码到板子

**前提：SSH 已成功。**

```bash
# Mac 上
cd /Users/zoro/Program/audio/asr/k2_zoro/sherpa-onnx

export BOARD_IP=192.168.1.50
export BOARD_USER=orangepi

rsync -avz --progress \
  --exclude 'build/' \
  --exclude 'depends/' \
  --exclude 'depends_bak/' \
  --exclude '.git/' \
  ./ ${BOARD_USER}@${BOARD_IP}:~/sherpa-onnx/
```

传模型（若本地有）：

```bash
rsync -avz --progress ./models/ ${BOARD_USER}@${BOARD_IP}:~/sherpa-onnx/models/
```

---

## 8. 板子上编译 KWS

SSH 登录板子后：

```bash
cd ~/sherpa-onnx

# 首次：安装依赖
sudo apt update
sudo apt install -y build-essential cmake git wget curl pkg-config libasound2-dev

# 编译（CPU 版，先用 wav 测，无需麦克风）
rm -rf build && mkdir -p build && cd build
cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=./install \
  -DSHERPA_ONNX_ENABLE_RKNN=OFF \
  -DSHERPA_ONNX_ENABLE_PORTAUDIO=OFF \
  -DSHERPA_ONNX_ENABLE_BINARY=ON \
  ..
cmake --build . -j$(nproc)
```

验证：

```bash
ls ~/sherpa-onnx/build/bin/sherpa-onnx-keyword-spotter
~/sherpa-onnx/build/bin/sherpa-onnx-keyword-spotter --help
```

用 wav 测试（无需麦克风）：

```bash
cd ~/sherpa-onnx
MODEL_DIR=~/sherpa-onnx/models/sherpa-onnx-kws-zipformer-wenetspeech-3.3M-2024-01-01-mobile
./build/bin/sherpa-onnx-keyword-spotter \
  --tokens="${MODEL_DIR}/tokens.txt" \
  --encoder="${MODEL_DIR}/encoder-epoch-12-avg-2-chunk-16-left-64.int8.onnx" \
  --decoder="${MODEL_DIR}/decoder-epoch-12-avg-2-chunk-16-left-64.onnx" \
  --joiner="${MODEL_DIR}/joiner-epoch-12-avg-2-chunk-16-left-64.int8.onnx" \
  --keywords-file="${MODEL_DIR}/test_wavs/test_keywords.txt" \
  "${MODEL_DIR}/test_wavs/"*.wav
```

---

## 9. 日常开发流程

```text
1. 板子上电，等 3～5 分钟
2. Mac 连 CMCC-Amf2
3. nmap 或光猫找 BOARD_IP
4. ssh orangepi@BOARD_IP
5. Mac rsync 改动的代码
6. 板子 build 目录：cmake --build . -j$(nproc)
7. 运行测试
```

---

## 10. 故障排查

| 现象 | 原因 | 处理 |
|------|------|------|
| 光猫列表无 orangepi5 | 租约缓存 / 未连上 | 用 nmap + 断电 diff |
| 以前 `.49` 不是板子 | 别的设备 | 以 **orangepi5** 或断电 diff 为准 |
| 扫不到板子 IP | 多条 Wi‑Fi autoconnect 冲突 | 只留一条 `home-wifi` |
| 图形界面配 Wi‑Fi 后仍不自动连 | 未设 autoconnect | `nmcli modify ... autoconnect yes` |
| 用了「Create New Wi-Fi Network」 | 板子当 AP/adhoc，不是连路由器 | 改用「Connect to Hidden Wi-Fi Network」或 nmcli |
| ssh Connection refused | sshd 未开 | `systemctl enable --now ssh` |
| 只有 2 个 nmap IP | 板子还没进网 | 多等 5 分钟；查天线 |
| 编译重启 | 电源 3A 不够 | 换 5V/4A 或 `make -j2` |

---

## 11. 命令速查

```bash
# === 板子（首次配置）===
nmcli connection show
sudo nmcli connection delete "旧连接名"
sudo nmcli connection add type wifi con-name "home-wifi" ifname wlan0 \
  ssid "CMCC-Amf2" wifi-sec.key-mgmt wpa-psk wifi-sec.psk "密码" \
  connection.autoconnect yes connection.autoconnect-priority 100 ipv4.method auto
sudo systemctl enable --now ssh

# === Mac（每次上电）===
ipconfig getifaddr en0
nmap -sn 192.168.1.0/24
ssh orangepi@192.168.1.50

# === Mac（传代码）===
cd /Users/zoro/Program/audio/asr/k2_zoro/sherpa-onnx
rsync -avz --exclude build --exclude depends_bak --exclude .git \
  ./ orangepi@192.168.1.50:~/sherpa-onnx/

# === 板子（编译）===
cd ~/sherpa-onnx/build && cmake --build . -j$(nproc)
```

---

## 12. 经验总结（踩坑记录）

1. **只保留一条 `autoconnect yes` 的 Wi‑Fi**（`home-wifi` → CMCC-Amf2）。多条旧配置会导致开机先去连不存在的网络。  
2. **图形「连接隐藏网络」只保存 SSID/密码**，必须在终端设 `autoconnect yes`。  
3. **不要用「Create New Wi-Fi Network」** 配家里 Wi‑Fi（那是板子自建网络，不是连路由器）。  
4. **光猫设备名可能是 `orangepi5`**，不再是 `--`；IP 用 nmap 或光猫查，不要猜。  
5. **手机热点模拟**：先开热点，再板子上电；测试完删掉测试 SSID，只留 CMCC-Amf2。  
6. **SSH 必须在公司或显示器上 `enable` 一次**，否则 Mac 上 `Connection refused`。

---

*文档路径：`sherpa-onnx/OrangePi5-Mac远程使用教程.md`*
