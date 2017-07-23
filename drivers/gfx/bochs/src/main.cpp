
#include <assert.h>
#include <stdio.h>
#include <deque>
#include <experimental/optional>
#include <functional>
#include <iostream>
#include <memory>

#include <arch/bits.hpp>
#include <arch/register.hpp>
#include <arch/io_space.hpp>
#include <async/result.hpp>
#include <boost/intrusive/list.hpp>
#include <cofiber.hpp>
#include <frigg/atomic.hpp>
#include <frigg/arch_x86/machine.hpp>
#include <frigg/memory.hpp>
#include <helix/ipc.hpp>
#include <helix/await.hpp>
#include <protocols/hw/client.hpp>
#include <protocols/mbus/client.hpp>
#include <protocols/usb/usb.hpp>
#include <protocols/usb/api.hpp>
#include <protocols/usb/server.hpp>

#include "bochs.hpp"

std::vector<std::shared_ptr<GfxDevice>> globalDevices;

// ----------------------------------------------------------------
// Helper Funcs.
// ----------------------------------------------------------------

void fillBuffer(void* frame_buffer, int width, int height){
	char* ptr = (char*)frame_buffer;
	for(int j = 0; j < height; j++) {
		for(int i = 0; i < width; i++) {
			// RGBX
			ptr[4 * (j * width + i) + 0] = 0xCE;
			ptr[4 * (j * width + i) + 1] = 0x13;
			ptr[4 * (j * width + i) + 2] = 0x95;
			ptr[4 * (j * width + i) + 3] = 0x00;
		}
	}
};

// ----------------------------------------------------------------
// GfxDevice.
// ----------------------------------------------------------------

GfxDevice::GfxDevice(void* frame_buffer)
: _frameBuffer{frame_buffer} {
	uintptr_t ports[] = { 0x01CE, 0x01CF, 0x01D0 };
	HelHandle handle;
	HEL_CHECK(helAccessIo(ports, 3, &handle));
	HEL_CHECK(helEnableIo(handle));
	
	_operational = arch::global_io;
}

COFIBER_ROUTINE(cofiber::no_future, GfxDevice::initialize(), ([=] {
	_operational.store(regs::index, (uint16_t)RegisterIndex::id);
	auto version = _operational.load(regs::data); 
	if(version < 0xB0C2) {
		std::cout << "gfx/bochs: Device version 0x" << std::hex << version << std::dec
				<< " may be unsupported!" << std::endl;
	}

	_operational.store(regs::index, (uint16_t)RegisterIndex::enable);
	_operational.store(regs::data, enable_bits::noMemClear || enable_bits::lfb);
	
	_operational.store(regs::index, (uint16_t)RegisterIndex::virtWidth);
	_operational.store(regs::data, 1024);
	_operational.store(regs::index, (uint16_t)RegisterIndex::virtHeight);
	_operational.store(regs::data, 768);
	_operational.store(regs::index, (uint16_t)RegisterIndex::bpp);
	_operational.store(regs::data, 32);

	_operational.store(regs::index, (uint16_t)RegisterIndex::resX);
	_operational.store(regs::data, 1024);
	_operational.store(regs::index, (uint16_t)RegisterIndex::resY);
	_operational.store(regs::data, 768);
	_operational.store(regs::index, (uint16_t)RegisterIndex::offX);
	_operational.store(regs::data, 0);
	_operational.store(regs::index, (uint16_t)RegisterIndex::offY);
	_operational.store(regs::data, 0);
	
	_operational.store(regs::index, (uint16_t)RegisterIndex::enable);
	_operational.store(regs::data, enable_bits::enable 
			|| enable_bits::noMemClear || enable_bits::lfb);
	
	fillBuffer(_frameBuffer, 1024, 768);
}))

// ----------------------------------------------------------------
// Freestanding PCI discovery functions.
// ----------------------------------------------------------------

COFIBER_ROUTINE(cofiber::no_future, bindController(mbus::Entity entity), ([=] {
	protocols::hw::Device pci_device(COFIBER_AWAIT entity.bind());
	auto info = COFIBER_AWAIT pci_device.getPciInfo();
	assert(info.barInfo[0].ioType == protocols::hw::IoType::kIoTypeMemory);
	auto bar = COFIBER_AWAIT pci_device.accessBar(0);
	
	void *actual_pointer;
	HEL_CHECK(helMapMemory(bar.getHandle(), kHelNullHandle, nullptr,
			0, info.barInfo[0].length, kHelMapReadWrite | kHelMapShareAtFork, &actual_pointer));

	auto gfx_device = std::make_shared<GfxDevice>(actual_pointer);
	gfx_device->initialize();
	globalDevices.push_back(std::move(gfx_device));
}))

COFIBER_ROUTINE(cofiber::no_future, observeControllers(), ([] {
	auto root = COFIBER_AWAIT mbus::Instance::global().getRoot();

	auto filter = mbus::Conjunction({
		mbus::EqualsFilter("pci-vendor", "1234")
	});
	COFIBER_AWAIT root.linkObserver(std::move(filter),
			[] (mbus::AnyEvent event) {
		if(event.type() == typeid(mbus::AttachEvent)) {
			std::cout << "gfx/bochs: Detected device" << std::endl;
			bindController(boost::get<mbus::AttachEvent>(event).getEntity());
		}else{
			throw std::runtime_error("Unexpected event type");
		}
	});
}))

int main() {
	printf("gfx/bochs: Starting driver\n");
	
	observeControllers();

	while(true)
		helix::Dispatcher::global().dispatch();
	
	return 0;
}
