# rtsp_server

#### 配置免密ssh

1. windows上 ssh-keygen，然后一直回车
2. C:\Users\用户\.ssh目录下的id_rsa.pub复制到服务器.ssh文件夹，然后运行cat id_rsa.pub >> authorized_keys就可以直接登录