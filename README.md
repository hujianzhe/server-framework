# server framework
跨平台底层库 https://github.com/hujianzhe/util, 下载后置于BootServer目录下  

简介：
代码只实现服务节点启动自举、任务调度、基本模块描述，不包含任何业务代码，纯C实现，一般编译成动态库使用  
功能方面保持极度克制，实现了一些常见通用协议流  
支持了C++20无栈协程的扩展，并且和纯C动态库部分代码隔离，防止C++污染到框架层,影响动态库的生成  
util中的代码负责跨平台，除了它不需要安装任何第三方库  

运作流程简介：  
业务进程调用该代码编译成的动态库，并调用相应接口使用，调用流程详见测试节点示例  
模块内部由多个线程处理网络io读写，单独的accept接入线程，初始时会开启一个工作线程处理内部消息和收到的网络消息并派遣到你的业务代码逻辑中  
~~工作线程使用有栈协程进行调度处理（未使用无栈协程的原因见下文）~~  
工作线程默认使用有栈协程进行调度处理以保持最高兼容度，也可以很方便的使用C++20无栈协程(util库中实现了一个)去调度，有栈协程与无栈协程可以共存  

TODO:  
1、一个好的说明文档，实在没时间写  
2、对脚本语言编写业务逻辑提供支持  

模块介绍与示例代码:  
1、BootServer：主要代码部分，服务节点的必备初始化和操作  
2、ServiceTemplate：服务节点代码模板，用来写你的业务逻辑  
3、SoTestClient,SoTestServer：测试节点，编写了一些示例代码  
4、Cpp20-SoTestServer：测试节点，编写了一些示例代码，main.cpp中启用了C++20无栈协程(使用util库中的一个C++20无栈协程调度器)  

编译:  
windows直接VS编译  
linux/Mac OS X下使用make debug 或 make release  

启动:  
编辑好服务节点启动需要的配置文件(具体格式参看附带的配置文件模板)，给每个节点一个配置文件和唯一id，日志标识名，IP和端口号  
windows直接VS打开，工程配置好启动参数  <配置文件>  
linux/mac编译后，sh run.sh <服务进程> <配置文件>  

一些设计原因与见解：  
~~Q:为何不使用无栈协程而采用有栈协程？~~  
~~A:纯C实现无栈协程是容易的（详细代码可以看util库中用纯C实现的无栈协程调度器），但资源回收与持久化（尤其是栈上变量在协程重入后的情形）是极度困难的~~  
  ~~想顺手的使用无栈协程还是得靠编译器支持，这点C++20已经做到，util库中也有完整的C++20无栈协程调度器实现~~  
  
Q:为何无栈协程采用头文件形式来提供这个扩展功能？  
A:1.因为无栈协程具有代码侵入性，会改变大量函数签名形式  
  2.包含了C++对象，这些在C中都是无法识别的，无法顺利导出动态库  
  3.头文件形式给出，将无栈协程的启用权限交给应用层，是我目前想到的对纯C动态库部分没有任何污染的应对方式  
  
Q:可以替换成其他调度器么？  
A:可以替换工作线程的调度器，工作线程被设计成调度器的运行载体，框架内部网络线程与工作线程的交互可通过接口hook对应行为  

Q:可以替换成其他网络库么？  
A:1.目前不可以替换成其他网络库，但设计之处已经考虑了这个问题，网络部分和任务调度部分是彻底分离的，将来会提供类似工作线程hook对应行为进行替换  
  2.虽然第三方网络库很多，但其侧重点各不相同。有用于应用开发的tcp，udp库，也有恨不得包含整个宇宙的库，也有应用层实现整个网络协议栈的库，所以其实这一块并不是统一的  
  3.如果将来有一套网络库进入了标准，那么我会替换的  
  
Q:为何不直接用C++做框架？  
A:1.写这套代码时候，C++20还没出现，那时候相对最成熟的协程方案就是有栈协程，这个用C通过调用对应平台系统API就可以做到  
  2.此类框架要实现的功能已经固化，资源的生命周期都是流程固化的，用纯C实现恰好足够（之前有过一个C++实现的版本，感觉反而代码变得复杂了）  
  3.如果动态库被其他模块调用，还是需要C封一层接口  
  4.C++导出class具有传染性，且ABI不统一  
  
Q:业务层能继续使用纯C开发么？  
A:非常不建议，因为异步流程的编写，以及高级语言编写的模块中抛出了异常，这些都让资源销毁的时机变得无法确定，这时仍用纯C去人为手动控制资源是极度困难的  
  应该使用更高级的语言去处理这些事情，比如可以用C++开发上层业务代码，它的RAII机制可以确保对应资源的释放  

Q:老项目的"回调"想改造成"协程"，是不是把类似调度器部分和业务代码中的对应调用处替换就可以了？  
A:没有那么简单，首先是工作量问题，另外不论是callback形式还是协程形式，本质都是发出请求等待结果，这期间变量的生命周期是个必须要解决的问题，需要全盘仔细考虑，如果是原始指针，那么几乎可以说是无法改造的  
  如果项目已经使用诸如std::shared_ptr之类的去拉长变量生命周期，那么改造成为"有栈协程"的版本难度会少一些  
  如果要改造成使用C++20无栈协程，工作量是巨大的无异于重写项目（因为无栈协程具有强烈的代码侵入性）因此老项目建议不改造  

Q:为何目前不允许协程迁移到不同线程中执行？  
A:迁移协程可以很容易做到，但不提供的原因是  
  1.协程之间执行的任务是不确定的，可能是会造成io与计算混在同一个调度线程中  
  2.迁移后同一个协程过程就可能会运行在不同线程上，这时必须保证你的代码不依赖任何线程本地变量，但你无法确保第三方库没有使用线程本地变量  
