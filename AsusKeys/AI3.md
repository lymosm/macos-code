以下是AsusSMC对键盘背光的patch，分平台的。你帮我分别放进每一个方法里。
broadwell：
Method (SKBV, 1, NotSerialized)\n
{\n
    ^^PCI0.LPCB.EC0.WRAM (0x04B1, Arg0)\n
    Return (Arg0)\n
}\n
coffeelake：
Method (SKBV, 1, NotSerialized)\n
{\n
    ^^PCI0.LPCB.EC0.WRAM (0xBA, 0xC9F0, ^^KBLV)\n
    ^^PCI0.LPCB.EC0.ST9E (0x1F, 0xFF, Arg0)\n
    Return (Arg0)\n
}\n
icelake：
Method (SKBV, 1, NotSerialized)\n
{\n
    ^^PCI0.LPCB.EC0.ST9E (0x1F, 0xFF, Arg0)\n
    Return (Arg0)\n
}\n
ivybridge：
Method (SKBV, 1, NotSerialized)\n
{\n
    ^^PCI0.LPCB.EC0.WRAM (0x044B, Arg0)\n
    Return (Arg0)\n
}\n
kabylake：
Method (SKBV, 1, NotSerialized)\n
{\n
    ^^PCI0.LPCB.EC0.WRAM (0x09F0, ^^KBLV)\n
    ^^PCI0.LPCB.EC0.ST9E (0x1F, 0xFF, Arg0)\n
    Return (Arg0)\n
}\n
tuf：
Method (SKBV, 1, NotSerialized)\n
{\n
    Arg0 = (Arg0 / 16) | 0x80\n
    SLKI(Arg0)\n
    Return (One)\n
}\n
whiskeylake：
Method (SKBV, 1, NotSerialized)\n
{\n
    ^^PCI0.LPCB.EC0.ST9E (0x1F, 0xFF, Arg0)\n
    Return (Arg0)\n
}\n


我想一个一个调用调式：
比如：
DefinitionBlock ("", "SSDT", 2, "ACDT", "SSDTKB", 0x00000000)
{
    External (_SB_.ATKD, DeviceObj)
    Scope (\_SB.ATKD)
    {
        Method (KB1, 1, NotSerialized)
        {
            ^^PCI0.LPCB.EC0.WRAM (0x04B1, Arg0)
            Return (Arg0)
        }
        Method (KB2,
        Method (KB3,
        等等依次写下去。把可能的方案尝试都写下，除了那些平台的，还有你的猜测也可以加上。我想调式出键盘背光控制，你能帮我吗？
    }
}

注意：我的CPU：I7-7700HQ EC0中没有ST9E方法，WRAM只支持2个参数，所以有些也不能照搬，需求调整修改。
