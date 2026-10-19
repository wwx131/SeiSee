# seisee-test Linux x86_64 测试版

包含当前 Metrics、FK 频谱及滤波功能，辅助线默认隐藏。

构建目标在 SeiSeeMp/SeiSeeMp.pro 中设置为 `unix:!macx: TARGET = seisee-test`。

面向 x86_64 桌面 Linux，glibc 2.31 或更高版本（例如 Ubuntu 20.04 及更新版本）。附带 Qt 运行库和插件；系统仍需桌面显示服务、显卡驱动及基础系统库。不能在 ARM、Alpine/musl、CentOS 7/较旧 glibc 上直接运行。

## 单文件运行

```sh
chmod +x seisee-test
./seisee-test
```

系统没有 FUSE 时：

```sh
./seisee-test --appimage-extract-and-run
```

也可使用无需 FUSE 的目录版：

```sh
tar -xzf seisee-test-linux-x86_64.tar.gz
cd seisee-test-portable
./seisee-test
```

打开带空格的路径：

```sh
./seisee-test "/path with spaces/data.sgy"
```

请保留目录版的全部文件。SSH 服务器若无桌面，需要配置图形显示后才能显示窗口。

验证项目：Linux 原生构建、数值与界面回归、便携目录和 AppImage 的离屏启动；这些检查不等同于已在每种发行版及真实桌面环境测试。
