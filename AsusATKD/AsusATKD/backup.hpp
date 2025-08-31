//
//  backup.hpp
//  AsusATKD
//
//  Created by lymos on 2025/8/29.
//

#ifndef _ASUSATKD_HPP
#define _ASUSATKD_HPP

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDDevice.h>


// Vendor / Product (from your Windows observation)
#define ASUS_VENDOR_ID  0x0B05
#define ASUS_PRODUCT_ID 0x1854

class AsusATKD : public IOService
{
    OSDeclareDefaultStructors(AsusATKD)

private:
    IOWorkLoop*    _workLoop;
    IOCommandGate* _cmdGate;
    IOService*     _provider;

public:
    // lifecycle
    virtual bool init(OSDictionary* dict = nullptr) override;
    virtual void free(void) override;

    virtual IOService* probe(IOService* provider, SInt32* score) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;

    // placeholder callback signature - actual registration depends on provider API
    static void InputReportCallback(void* target,
                                    IOReturn status,
                                    void* refCon,
                                    UInt32 reportType,
                                    UInt32 reportID,
                                    const void* reportData,
                                    UInt32 reportLength);

    // helper: safe logging (only usage page / usage / numeric value)
    void safeLogUsage(uint32_t usagePage, uint32_t usage, int64_t value);
};

#endif /* _ASUSATKD_HPP */
