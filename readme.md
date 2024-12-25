# 简介

网易云音乐会员专属格式的解码。源码部分参考了最早的ncmdump（anonymous5l/ncmdump，原作者已经删库），感谢前辈的付出！

使用c++17重写，修复了一些跨平台编译问题。引入openmp、cryptopp等高性能库，理论上是现在开源版本中速度最快的。

# 编译

```shell
xmake
xmake install
```

# 使用

Windows:编译后打开".\build\windows\x64\release"目录，双击"fastncmdump.exe"。

Linux:编译后打开".\build\linux\x64\release"目录，终端输入"./fastncmdump"。

软件启动后有中文提示，祝你使用愉快！

# 免责声明

软件仅供学习交流，请勿用于商业及非法用途，如产生法律纠纷与本人无关。

如果您认为本软件对您的合法权益产生了侵害，请在issues留言。
