# SaltyEgine

通过opengl实现的路径追踪demo，cpu端对顶点数据做了bvh优化

## 构建

依赖glfw，glm
推荐使用xmake构建，xamke配置脚本已经写好
xmake命令：

```lua
xmake project -k vsxmake
```

## 简单说明

cpu构建bvh加速结构，gpu在片段着色器进行路径追踪运算。

需要三个pass，pass1计算光线，pass2开启颜色缓冲更新到pass1，pass3进行toneMapping等后处理。

正确渲染的效果如图：

<img src="D:\4prj\SaltyEgine\README.assets\image-20220929112448377.png" alt="image-20220929112448377" style="zoom:50%;" />
