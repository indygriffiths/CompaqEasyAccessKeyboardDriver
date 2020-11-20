#include <os/log.h>
#include <DriverKit/IOUserServer.h>
#include <DriverKit/IOLib.h>
#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/HIDDriverKit.h>

#include "CompaqEasyAccessKeyboardDriver.h"

struct CompaqEasyAccessKeyboardDriver_IVars
{
    OSArray *elements;
    
    struct {
        OSArray *elements;
    } keyboard;
};

#define _elements   ivars->elements
#define _keyboard   ivars->keyboard

bool CompaqEasyAccessKeyboardDriver::init()
{
    if (!super::init()) {
        return false;
    }
    
    ivars = IONewZero(CompaqEasyAccessKeyboardDriver_IVars, 1);
    if (!ivars) {
        return false;
    }

exit:
    return true;
}

void CompaqEasyAccessKeyboardDriver::free()
{
    if (ivars) {
        OSSafeReleaseNULL(_elements);
        OSSafeReleaseNULL(_keyboard.elements);
    }
    
    IOSafeDeleteNULL(ivars, CompaqEasyAccessKeyboardDriver_IVars, 1);
    super::free();
}

kern_return_t
IMPL(CompaqEasyAccessKeyboardDriver, Start)
{
    kern_return_t ret;

    os_log(OS_LOG_DEFAULT, "CompaqEasyAccessKeyboardDriver: Initializing driver");

    ret = Start(provider, SUPERDISPATCH);
    if (ret != kIOReturnSuccess) {
        Stop(provider, SUPERDISPATCH);
        return ret;
    }
    
    _elements = getElements();
    if (!_elements) {
        os_log(OS_LOG_DEFAULT, "CompaqEasyAccessKeyboardDriver: Failed to get elements");
        Stop(provider, SUPERDISPATCH);
        return kIOReturnError;
    }
    
    _elements->retain();
    
    if (!parseElements(_elements)) {
        os_log(OS_LOG_DEFAULT, "CompaqEasyAccessKeyboardDriver: No supported elements found");
        Stop(provider, SUPERDISPATCH);
        return kIOReturnUnsupported;
    }

    RegisterService();

    os_log(OS_LOG_DEFAULT, "CompaqEasyAccessKeyboardDriver: Driver initiated");
    
    return ret;
}

bool CompaqEasyAccessKeyboardDriver::parseElements(OSArray *elements)
{
    bool result = false;
    
    for (unsigned int i = 0; i < elements->getCount(); i++) {
        IOHIDElement *element = NULL;
        
        element = OSDynamicCast(IOHIDElement, elements->getObject(i));
        
        if (!element) {
            continue;
        }
        
        os_log(OS_LOG_DEFAULT, "CompaqEasyAccessKeyboardDriver: element type: %i usage: %i", element->getType(), element->getUsage());

        if (element->getType() == kIOHIDElementTypeCollection ||
            !element->getUsage()) {
            continue;
        }
        
        if (parseKeyboardElement(element)) {
            result = true;
        }
    }
    
    return result;
}

bool CompaqEasyAccessKeyboardDriver::parseKeyboardElement(IOHIDElement *element)
{
    bool result = false;
    uint32_t usagePage = element->getUsagePage();
    uint32_t usage = element->getUsage();
    
    os_log(OS_LOG_DEFAULT, "CompaqEasyAccessKeyboardDriver: keyboard usagePage: %i usage: %i", usagePage, usage);
    
    // Determine whether the element contains keyboard-related data.
    if (usagePage == kHIDPage_KeyboardOrKeypad) {
        if (usage >= kHIDUsage_KeyboardA &&
            usage <= kHIDUsage_KeyboardRightGUI) {
            
            os_log(OS_LOG_DEFAULT, "CompaqEasyAccessKeyboardDriver: confirmed keyboard at usagePage: %i usage: %i", usagePage, usage);
            result = true;
        }
    } else if (usagePage == kHIDPage_Consumer) {
        // Media keys are under this HID page
        os_log(OS_LOG_DEFAULT, "CompaqEasyAccessKeyboardDriver: confirmed consumer at usagePage: %i usage: %i", usagePage, usage);
        result = true;
    }
    
    if (!result) {
        return false;
    }
    
    if (!_keyboard.elements) {
        _keyboard.elements = OSArray::withCapacity(4);
    }
    
    _keyboard.elements->setObject(element);
    
exit:
    return result;
}

void CompaqEasyAccessKeyboardDriver::handleReport(uint64_t timestamp,
                                     uint8_t *report __unused,
                                     uint32_t reportLength __unused,
                                     IOHIDReportType type,
                                     uint32_t reportID)
{
    if (!_keyboard.elements) {
        return;
    }
    
    // Iterate over the elements that contain keyboard data.
    for (unsigned int i = 0; i < _keyboard.elements->getCount(); i++) {
        IOHIDElement *element = NULL;
        uint64_t elementTimeStamp;
        uint32_t usagePage, usage, value, preValue;
        
        element = OSDynamicCast(IOHIDElement, _keyboard.elements->getObject(i));
        
        if (!element) {
            continue;
        }
        
        // If the element doesn't contain new data, skip it.
        if (element->getReportID() != reportID) {
            continue;
        }
        
        elementTimeStamp = element->getTimeStamp();
        if (timestamp != elementTimeStamp) {
            continue;
        }
        
        // Get the previous value of the element.
        preValue = element->getValue(kIOHIDValueOptionsFlagPrevious) != 0;
        value = element->getValue(0) != 0;
        
        // If the element's value didn't change, skip it.
        if (value == preValue) {
            continue;
        }
        
        usagePage = element->getUsagePage();
        usage = element->getUsage();
        
        // Be careful enabling this log entry, as every key press will be logged
        // os_log(OS_LOG_DEFAULT,
        //       "CompaqEasyAccessKeyboardDriver: Dispatching initial key with usage page: 0x%02x usage: 0x%02x value: %d",
        //       usagePage, usage, value);
        
        switch (usage) {
            case kHIDUsage_Csmr_Loudness:               // Mute button
                usage = kHIDUsage_Csmr_Mute;
                break;
            case kHIDUsage_Csmr_BassBoost:              // Play/pause button
            case kHIDUsage_Csmr_Stop:                   // Stop button (no separate Stop usage key that macOS doesn't re-use for Mute)
                usage = kHIDUsage_Csmr_PlayOrPause;
                break;
            case kHIDUsage_Csmr_ScanPreviousTrack:      // Previous trackbutton
                usage = kHIDUsage_Csmr_Rewind;
                break;
            case kHIDUsage_Csmr_ScanNextTrack:          // Next track button
                usage = kHIDUsage_Csmr_FastForward;
                break;
            case kHIDUsage_Csmr_BassIncrement:          // Volume up button
                usage = kHIDUsage_Csmr_VolumeIncrement;
                break;
            case kHIDUsage_Csmr_BassDecrement:          // Volume down button
                usage = kHIDUsage_Csmr_VolumeDecrement;
                break;
            case kHIDUsage_Csmr_ACForward:              // Shop button
                usagePage = kHIDPage_KeyboardOrKeypad;
                usage = kHIDUsage_KeyboardF16;
                break;
        }
        
        // Be careful enabling this log entry, as every key press will be logged
        // os_log(OS_LOG_DEFAULT,
        //       "CompaqEasyAccessKeyboardDriver: Dispatching final key with usage page: 0x%02x usage: 0x%02x value: %d",
        //       usagePage, usage, value);
        
        dispatchKeyboardEvent(timestamp, usagePage, usage, value, 0, true);
    }
    
exit:
    return;
}
