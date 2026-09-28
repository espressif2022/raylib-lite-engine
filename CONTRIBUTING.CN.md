# 参与 Raylib Lite Engine 开发

[English](CONTRIBUTING.EN.md) · [文档索引](docs/README.CN.md)

先按[快速入门](docs/quickstart.CN.md)跑通 Host，再从[构建路径](docs/build-matrix.CN.md)选择目标。玩法模型与共享视图保持可移植；板级服务放在平台或产品适配层。增加游戏私有 helper 前，先查[能力目录（English）](components/README.md)中的现有公共接口。

提交改动时说明触发条件、最终行为和受影响目标。修改像素、资产格式或命令协议时，配独立正确性检查。运行 `python3 tools/check_markdown_links.py`、`git diff --check` 和相关测试；[AGENTS.md](AGENTS.md)按改动类型列出检查。Host 耗时与板端测量分开记录。不要提交生成的 `build/`、`managed_components/`、`sdkconfig` 或本地 `docs/debug/` 文件。

新源码写明 SPDX 标识，并遵循所在组件的 C/Python 风格。不要复制其他引擎的实现；算法和数据布局的原创选择应在提交说明中可供审查。
