# 基于 Qt 和 MySQL 的医院门诊挂号与药品管理系统

本项目是一个 Linux 环境下运行的毕业设计项目骨架，包含两个端：

- `client`：Qt Widgets 客户端，面向挂号员、医生、药房、财务和管理员。
- `server`：Qt TCP 服务端，负责业务接口、权限校验和 MySQL 数据访问。
- `common`：客户端和服务端共享的通信协议、通用类型。
- `database`：MySQL 建库脚本。
- `docs`：模块划分和接口说明。

## 当前框架亮点

- 客户端和服务器端分离，均使用 Qt/C++，适合 Linux 下部署。
- MySQL 表结构已按毕设要求预留，当前包含 17 张业务表。
- 功能模块超过 8 个：登录权限、患者管理、挂号管理、医生排班、医生接诊、处方管理、药品库存、收费结算、费用统计。
- 使用 CMake 组织工程，后续可直接扩展页面、接口和数据库访问。
- 挂号管理、医生排班、药品入库支持动态数据；患者端新增挂号后，医院端挂号管理可刷新查看。
- 列表页支持手动刷新、按模块频率自动刷新和分页显示。

## 当前数据模式

默认配置 `config/server.example.ini` 中：

```ini
[database]
enabled=true
driver=QODBC
```

这表示项目默认通过 Qt 自带的 `QODBC` 驱动连接 MySQL。启动服务端前需要先保证 MySQL 服务已启动，并且已经创建数据库：

```bash
mysql -u root -p < database/schema.sql
```

当前默认 MySQL 账号密码按你的环境设置为 `root / admin`。如果数据库账号密码不同，请修改 `server.ini` 或 `config/server.example.ini`：

```ini
[database]
enabled=true
driver=QODBC
host=127.0.0.1
port=3306
name=hospital_outpatient
user=root
password=你的MySQL密码
```

Windows 下如果提示 ODBC 驱动缺失，请安装 64 位 `MySQL Connector/ODBC 8.0`。如果你自己已经补好了 Qt 的 `qsqlmysql.dll`，也可以把 `driver=QODBC` 改成 `driver=QMYSQL`。

Linux 下使用 `config/server.linux.example.ini`，默认走 Qt MySQL 驱动：

```ini
[database]
enabled=true
driver=QMYSQL
```

Linux 服务端脚本已经默认读取这个配置，不需要安装 Windows ODBC 驱动。

目前已接入 MySQL 的核心写入：患者挂号、医生排班号源保存、药品扫码/手动入库。查询和写入前会检查连接，如果 MySQL 临时断开会自动尝试重连。

如果只是课堂演示、暂时没有 MySQL，也可以把 `enabled=false` 切回演示模式。

## 三种目标环境

本项目使用 CMake 统一管理源码，目标是同一套工程可以在 VS Code、Qt Creator、银河麒麟系统中打开、编译和运行。

### 1. VS Code

直接打开项目目录：

```text
D:\bs\HospitalOutpatientSystem
```

Windows 下已配置 Qt 路径，构建目录为 `build-windows`：

```text
D:\Qt\6.11.0\mingw_64
D:\Qt\Tools\mingw1310_64
```

在 VS Code 中按 `Ctrl+Shift+P`：

- 选择 `Tasks: Run Task`
- 运行 `Windows: Run All`

也可以在“运行和调试”里选择 `Run All`，它会自动启动服务端和客户端。

如果 IntelliSense 仍然红线，执行：

- `C/C++: Reset IntelliSense Database`
- `Developer: Reload Window`

### 2. Qt Creator

1. 打开 Qt Creator。
2. 选择 `Open Project`，打开本目录下的 `CMakeLists.txt`。
3. Windows 下 Kit 选择 Qt 6 MinGW。
4. 银河麒麟下 Kit 选择系统 Qt/GCC。
5. 推荐运行 `hospital_launcher`，它会自动启动服务端和客户端。

Qt Creator 使用的是同一个 `CMakeLists.txt`，不需要 `.pro` 文件。

### 3. 银河麒麟系统

推荐使用银河麒麟桌面版运行 Qt 客户端；服务器版也可以运行服务端。

把整个 `HospitalOutpatientSystem` 目录复制到麒麟系统后，在终端执行：

```bash
cd HospitalOutpatientSystem
bash scripts/kylin_install_deps.sh
bash scripts/kylin_configure.sh
bash scripts/kylin_build.sh
mysql -u root -p < database/schema.sql
```

启动服务端：

```bash
cd HospitalOutpatientSystem
bash scripts/kylin_run_server.sh
```

另开一个终端启动客户端：

```bash
cd HospitalOutpatientSystem
bash scripts/kylin_run_client.sh
```

如果麒麟系统的软件源没有 Qt6，项目会使用 Qt5 编译；当前代码兼容 Qt5/Qt6。

Ubuntu 或其他普通 Linux 虚拟机可以使用对应的 Linux 脚本：

```bash
cd HospitalOutpatientSystem
bash scripts/linux_install_deps_ubuntu.sh
bash scripts/linux_configure.sh
bash scripts/linux_build.sh
mysql -u root -p < database/schema.sql
bash scripts/linux_run_server.sh
```

另开一个终端：

```bash
cd HospitalOutpatientSystem
bash scripts/linux_run_client.sh
```

如果需要桌面快捷方式：

```bash
cd HospitalOutpatientSystem
bash scripts/linux_create_shortcuts.sh
```

## 数据库初始化

```bash
mysql -u root -p < database/schema.sql
```

脚本可重复执行，会重建 `hospital_outpatient` 库内的业务表并写入演示数据。

默认登录账号：

- 用户名：`admin`
- 密码：`123456`

## 启动示例

Windows PowerShell：

```powershell
cd D:\bs\HospitalOutpatientSystem
powershell -ExecutionPolicy Bypass -File scripts\windows_configure.ps1
powershell -ExecutionPolicy Bypass -File scripts\windows_build.ps1
powershell -ExecutionPolicy Bypass -File scripts\windows_run_all.ps1
```

银河麒麟：

```bash
bash scripts/kylin_run_server.sh
```

另开一个终端启动客户端：

```bash
bash scripts/kylin_run_client.sh
```

如果数据库账号密码不是 `root / admin`，请修改 `config/server.linux.example.ini`。

## 目录结构

```text
HospitalOutpatientSystem/
  client/      Qt Widgets 客户端
  common/      共享协议和通用数据结构
  config/      服务端配置模板
  database/    MySQL 建库脚本
  docs/        模块设计说明
  server/      Qt TCP 服务端
```
