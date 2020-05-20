# server framework

1、底层库https://github.com/hujianzhe/util, 下载后置于BootServer目录下  
2、纯C实现，不依赖任何第三方框架  
3、跨平台服务器母机，可具体配置启动参数  
4、通过加载动态链接库实现不同的业务逻辑  
5、支持异步回调、协程、消息3种RPC调度机制，基于红黑树实现的定时器  
6、网络通信支持TCP和可靠UDP，支持自定义消息编解码  
7、服务发现中心，集群节点建立  
8、支持按照一致性hash环，hash取余数，依次轮询三种方式选取目标节点  

TODO：  
1、一个好的说明文档，实在没时间写  
2、日志替换成底层库中的文件日志  
3、对脚本语言提供支持  
4、有状态服务节点的扩容与缩容  
5、利用本地消息表实现消息幂等  

启动：  
编辑好服务启动需要的配置文件之后  
windows直接VS打开，工程配置好启动参数  <配置文件> <加载模块路径>    
linux下进入BootServer目录，sh run.sh <配置文件> <加载模块路径>  
