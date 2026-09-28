# Agent 命令接口

[English](agent-cli.EN.md) · [快速入门](quickstart.CN.md)

`tools/game_cli.py` 的有限操作支持在**子命令后**传 `--json`：`create`、`sim --headless`、`build`。成功或失败时 stdout 只输出一个 `mosaico-game-cli/v1` JSON 对象；编译日志、资源诊断和用法说明走 stderr。交互式 `sim` 持续提供网页预览，不支持 `--json`。未传 `--json` 时保留既有输出方式。

| 退出码 | Agent 应如何处理 |
| --- | --- |
| 0 | 操作成功；读取 `status`、`command` 和结果字段 |
| 1 | 子进程构建/仿真失败；查看 stderr，修复源码或配置，再重试 |
| 2 | 参数或项目配置不合法；修改命令，不要原样重试 |
| 3 | 缺少工具、IDF 环境或文件系统不可用；补齐环境后重试 |
| 4 | Host 输出不符合机器协议；保存 stderr 和命令以排查协议问题 |

`sim --json` 的结果在 `result` 下；`build --json` 额外返回子工具原始 `tool_exit_code`。CLI 不会执行烧录、安装或发布，也没有能跨这些动作授权的 `--yes`。创建目录、构建输出、设备烧录/安装和发布应按各自作用范围分别授权；不要把 `build` 成功当作设备验收或发布完成。

```sh
python3 tools/game_cli.py sim examples/raylib_shooter --headless --frames 30 --json
python3 tools/game_cli.py build /path/to/module --target elf --toolchain /path/to/toolchain.cmake --json
```
