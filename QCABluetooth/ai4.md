管道是创建成功了，但是好像读写失败，日志如下：
[    4.716233]: tommybt: init in
[    4.716239]: tommybt: probe in
[    4.716245]: tommybt: start in
[    4.716253]: tommybt: findAndOpenInterface (compatible)
[    4.716263]: tommybt: attempt reset config to force interfaces
[    4.716651]: tommybt: setConfiguration applied 1
[    4.716655]: tommybt: scanning child objects for IOUSBHostInterface
[    4.716657]: tommybt: fallback to manual parsing of configuration descriptor
[    4.716658]: tommybt: config wTotalLength = 185
[    4.716660]: tommybt: parsed endpoint at offset 26: addr=0x81 dir=0x80 num=1 type=3 maxPkt=16
[    4.716662]: tommybt: selected INT IN endpoint 0x81 (type=3)
[    4.716664]: tommybt: parsed endpoint at offset 33: addr=0x82 dir=0x80 num=2 type=2 maxPkt=64
[    4.716666]: tommybt: parsed endpoint at offset 40: addr=0x02 dir=0x00 num=2 type=2 maxPkt=64
[    4.716669]: tommybt: selected BULK OUT endpoint 0x02 (type=2)
[    4.717056]: tommybt: created pipes from descriptors: intIn=0x81 bulkOut=0x02
[    4.717063]: tommybt: failed to submit event read: 0xe00002d8

读写的函数是：
void QCABluetooth::allocateEventRead() {
    const int bufSize = 260; // 足够存放 HCI event
    _eventBuffer = IOBufferMemoryDescriptor::withCapacity(bufSize, kIODirectionIn);
    if (!_eventBuffer) {
        IOLog("tommybt: failed to allocate event buffer\n");
        return;
    }

    IOUSBHostCompletion completion;
    completion.owner     = this;
    completion.action    = sEventReadComplete;
    completion.parameter = nullptr; // use member _eventBuffer

    IOReturn ret = _intInPipe->io(_eventBuffer, bufSize, &completion, 0);
    if (ret != kIOReturnSuccess) {
        IOLog("tommybt: failed to submit event read: 0x%x\n", ret);
    } else {
        IOLog("tommybt: submitted event read\n");
    }
}
看看怎么解决？
