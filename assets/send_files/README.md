# 测试发送文件目录

将需要通过右侧 **Send file** 发送的普通文件放在此目录。

在客户端的 `File path` 中可使用工程相对路径，例如：

```text
assets/send_files/hello.txt
```

也支持绝对 WSL 路径，例如：

```text
/home/alan/michat/lvgl_chat/assets/send_files/hello.txt
```

Emoji 请继续放在 `assets/emojis/`，并通过 **Emoji** 按钮选择发送。收到的文件会保存到 `build/runtime/received/`。
