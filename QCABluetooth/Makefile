# 固件路径
RAMPATCH = ar3k/ramps_0x31010100_40.dfu
MAINFW   = ar3k/AthrBT_0x31010100.dfu
FWHEADER = qca_firmware.h

# 默认目标
all: $(FWHEADER) build

# 生成头文件
$(FWHEADER): $(RAMPATCH) $(MAINFW) gen_qca_fw.sh
	@echo "==> 生成固件头文件: $@"
	@bash ./gen_qca_fw.sh $(RAMPATCH) $(MAINFW) $(FWHEADER)
	
# 编译目标 (示例，你可以替换成 xcodebuild 或 clang)
build:
    @echo "==> 编译 KEXT..."
    # xcodebuild -project QCABluetooth.xcodeproj -configuration Release

# 清理
clean:
	@rm -f $(FWHEADER)
    @echo "==> 清理完成"
