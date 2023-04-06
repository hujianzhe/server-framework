# server framework
跨平台底层库 https://github.com/hujianzhe/util, 下载后置于BootServer目录下  

简介：本代码只实现节点启动自举与集群节点之间的消息联动基础接口，不包含任何业务代码  
各节点通过读取配置文件加载或更新你自定义的各种服务集群的节点信息  
模块内部由多个线程处理io，另有一个工作线程处理内部消息和收到的网络消息并派遣到你的业务代码逻辑中  
工作线程使用有栈协程进行调度处理  
跨平台，不依赖任何第三方框架  
网络通信支持TCP和可靠UDP  

TODO:  
1、一个好的说明文档，实在没时间写  
2、对脚本语言编写业务逻辑提供支持  

模块介绍:  
1、BootServer：服务节点启动的必备初始化和操作  
2、ServiceTemplate：服务节点代码模板，用来写你的业务逻辑  
3、SoTestClient,SoTestServer：测试节点，用于测试功能  

编译:  
windows直接VS编译
linux下使用make debug 或 make release  

启动:  
编辑好服务节点启动需要的配置文件(具体格式参看附带的配置文件模板)，给每个节点一个配置文件和唯一id，日志标识名，IP和端口号  
windows直接VS打开，工程配置好启动参数  <配置文件>  
linux编译后，sh run.sh <服务进程> <配置文件>  
