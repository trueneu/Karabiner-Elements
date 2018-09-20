#include "logger.hpp"
#include "thread_utility.hpp"
#include <CoreGraphics/CoreGraphics.h>
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/file_sinks.h>
#include <sstream>
//#include <spdlog.h>

auto keypress_logger = spdlog::basic_logger_mt("keypress_counter", "/var/log/karabiner/keypresses.log");
const int flush_rate = 100;
int c = 0;

namespace {
    CFMachPortRef eventtap_;

    CGEventRef callback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
        std::cout << "CGEventGetFlags 0x" << std::hex << CGEventGetFlags(event) << std::dec << std::endl;

        if ((CGEventGetFlags(event) & kCGEventFlagMaskAlternate)
            | (CGEventGetFlags(event) & kCGEventFlagMaskShift)
            | (CGEventGetFlags(event) & kCGEventFlagMaskCommand)
            | (CGEventGetFlags(event) & kCGEventFlagMaskControl)) {
            keypress_logger->info("modifier_pressed");
        }
        std::string keytype;
        std::ostringstream stringStream;
        long int keycode = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        switch (type) {
            case kCGEventTapDisabledByTimeout:
                krbn::logger::get_logger().info("Re-enable event_tap_ by kCGEventTapDisabledByTimeout");
                CGEventTapEnable(eventtap_, true);
                break;

            case kCGEventKeyDown:
                std::cout << "kCGEventKeyDown" << std::endl;
                switch (keycode) {
                    case 123: case 124: case 125: case 126:
                        keytype.assign("arrow");
                        break;
                    case 115: case 116: case 119: case 121:
                        keytype.assign("pg");
                        break;
                    case 117:
                    case 51:
                        keytype.assign("bksp");
                        break;
                    default:
                        keytype.assign("other");
                };
                std::cout << "Keycode: " << keycode << std::endl;
                stringStream << "regular_key_down_" << keytype;
                keypress_logger->info(stringStream.str());
                break;

            case kCGEventKeyUp:
                std::cout << "kCGEventKeyUp" << std::endl;

            default:
                std::cout << "callback:" << type << std::endl;
                break;
        }
        if (c++ > flush_rate) {
            keypress_logger->flush();
            c = 0;
        }
        return event;
    }
} // namespace

int main(int argc, const char *argv[]) {
    krbn::thread_utility::register_main_thread();

    if (getuid() != 0) {
        krbn::logger::get_logger().error("eventtap requires root privilege to use kCGHIDEventTap.");
        return 0;
    }

    if (auto source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState)) {
        std::cout << "CGEventSourceKeyboardType:" << CGEventSourceGetKeyboardType(source) << std::endl;
        CFRelease(source);
    }

    auto mask = CGEventMaskBit(kCGEventFlagsChanged) |
                CGEventMaskBit(kCGEventKeyDown) |
                CGEventMaskBit(kCGEventKeyUp) |
                CGEventMaskBit(kCGEventLeftMouseDown) |
                CGEventMaskBit(kCGEventLeftMouseUp) |
                CGEventMaskBit(kCGEventRightMouseDown) |
                CGEventMaskBit(kCGEventRightMouseUp) |
                CGEventMaskBit(kCGEventMouseMoved) |
                CGEventMaskBit(kCGEventLeftMouseDragged) |
                CGEventMaskBit(kCGEventRightMouseDragged) |
                CGEventMaskBit(kCGEventScrollWheel) |
                CGEventMaskBit(kCGEventOtherMouseDown) |
                CGEventMaskBit(kCGEventOtherMouseUp) |
                CGEventMaskBit(kCGEventOtherMouseDragged);

    eventtap_ = CGEventTapCreate(kCGHIDEventTap,
                                 kCGTailAppendEventTap,
                                 kCGEventTapOptionListenOnly,
                                 mask,
                                 callback,
                                 nullptr);

    auto run_loop_source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventtap_, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source, kCFRunLoopCommonModes);
    CGEventTapEnable(eventtap_, true);
    CFRelease(run_loop_source);

    CFRunLoopRun();
    keypress_logger->flush();
    return 0;
}
