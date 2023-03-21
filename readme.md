### 资源获取
1. [课程官网](https://www.cs.cmu.edu/afs/cs.cmu.edu/academic/class/15213-f15/www/index.html) (2015)
2. [CSAPP官网](http://csapp.cs.cmu.edu/3e/home.html)
3. [实验资料](https://csapp.cs.cmu.edu/3e/labs.html)
4. [实验总览](https://hansimov.gitbook.io/csapp/labs/labs-overview)
5. [Lab0 环境搭建](https://ziyang.moe/article/csapplab0.html)
6. [CSAPP书籍答案](https://dreamanddead.github.io/CSAPP-3e-Solutions/)
7. [课程视频](https://www.bilibili.com/video/BV1iW411d7hd?spm_id_from=333.999.0.0&vd_source=810e3dc9707586eb86e45ed37548b720)

#### 上课时遇到的问题

(1). 在 attack lab Part I 中，不能覆写函数栈帧中的当前函数返回地址之上的部分，详情可以参考这里
https://stackoverflow.com/questions/53255874/buffer-overflow-attack-the-attack-lab-phase-2

(2). 在 64 bit 的 ubuntu 系统中，编译 32 bit 的程序需要安装一些库，推荐安装库 gcc-multilib
```shell
sudo apt install gcc-multilib
```
详情可以参考这里
https://stackoverflow.com/questions/22355436/how-to-compile-32-bit-apps-on-64-bit-ubuntu

(3). 在 cache lab Part B 中，如果系统上没有安装 valgrind，则测试程序 test-trans 会报错，此时直接安装 valgrind 即可
```shell 
sudo apt install valgrind
```

(4). cache lab Part B 中的 driver.py 使用 python2 运行，如果未安装 python2，则使用
```shell
sudo apt install python2
```
安装即可

(5). malloc lab 中原始实验包中测试用的trace不存在，下载测试文件的链接如下
```
https://github.com/Ethan-Yan27/CSAPP-Labs/tree/master/yzf-malloclab-handout/traces
```

