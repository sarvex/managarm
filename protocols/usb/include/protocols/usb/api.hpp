#pragma once

#include <memory>

#include <arch/dma_structs.hpp>
#include <async/result.hpp>
#include <frg/expected.hpp>

#include "usb.hpp"

enum class UsbError {
	none,
	stall,
	babble,
	timeout,
	unsupported,
	other
};

enum class DeviceSpeed {
	lowSpeed,
	fullSpeed,
	highSpeed,
	superSpeed
};

enum XferFlags {
	kXferToDevice = 1,
	kXferToHost = 2
};

struct ControlTransfer {
	ControlTransfer(XferFlags flags, arch::dma_object_view<SetupPacket> setup,
			arch::dma_buffer_view buffer)
	: flags{flags}, setup{setup}, buffer{buffer} { }

	XferFlags flags;
	arch::dma_object_view<SetupPacket> setup;
	arch::dma_buffer_view buffer;
};

struct InterruptTransfer {
	InterruptTransfer(XferFlags flags, arch::dma_buffer_view buffer)
	: flags{flags}, buffer{buffer},
			allowShortPackets{false}, lazyNotification{false} { }

	XferFlags flags;
	arch::dma_buffer_view buffer;
	bool allowShortPackets;
	bool lazyNotification;
};

struct BulkTransfer {
	BulkTransfer(XferFlags flags, arch::dma_buffer_view buffer)
	: flags{flags}, buffer{buffer},
			allowShortPackets{false}, lazyNotification{false} { }

	XferFlags flags;
	arch::dma_buffer_view buffer;
	bool allowShortPackets;
	bool lazyNotification;
};

enum class PipeType {
	null, in, out, control
};

// ----------------------------------------------------------------------------
// EndpointData
// ----------------------------------------------------------------------------

struct EndpointData {
protected:
	~EndpointData() = default;

public:
	virtual async::result<frg::expected<UsbError>> transfer(ControlTransfer info) = 0;
	virtual async::result<frg::expected<UsbError, size_t>> transfer(InterruptTransfer info) = 0;
	virtual async::result<frg::expected<UsbError, size_t>> transfer(BulkTransfer info) = 0;
};


struct Endpoint {
	Endpoint(std::shared_ptr<EndpointData> state);

	async::result<frg::expected<UsbError>> transfer(ControlTransfer info) const;
	async::result<frg::expected<UsbError, size_t>> transfer(InterruptTransfer info) const;
	async::result<frg::expected<UsbError, size_t>> transfer(BulkTransfer info) const;

private:
	std::shared_ptr<EndpointData> _state;
};

// ----------------------------------------------------------------------------
// InterfaceData
// ----------------------------------------------------------------------------

struct InterfaceData {
protected:
	~InterfaceData() = default;

public:
	virtual async::result<frg::expected<UsbError, Endpoint>>
	getEndpoint(PipeType type, int number) = 0;
};

struct Interface {
	Interface(std::shared_ptr<InterfaceData> state);

	async::result<frg::expected<UsbError, Endpoint>>
	getEndpoint(PipeType type, int number) const;

private:
	std::shared_ptr<InterfaceData> _state;
};


// ----------------------------------------------------------------------------
// ConfigurationData
// ----------------------------------------------------------------------------

struct ConfigurationData {
protected:
	~ConfigurationData() = default;

public:
	virtual async::result<frg::expected<UsbError, Interface>>
	useInterface(int number, int alternative) = 0;
};

struct Configuration {
	Configuration(std::shared_ptr<ConfigurationData> state);

	async::result<frg::expected<UsbError, Interface>>
	useInterface(int number, int alternative) const;

private:
	std::shared_ptr<ConfigurationData> _state;
};

// ----------------------------------------------------------------------------
// DeviceData
// ----------------------------------------------------------------------------

struct DeviceData {
protected:
	~DeviceData() = default;

public:
	virtual arch::dma_pool *setupPool() = 0;
	virtual arch::dma_pool *bufferPool() = 0;

	virtual async::result<frg::expected<UsbError, std::string>> configurationDescriptor() = 0;
	virtual async::result<frg::expected<UsbError, Configuration>> useConfiguration(int number) = 0;
	virtual async::result<frg::expected<UsbError>> transfer(ControlTransfer info) = 0;
};

struct Device {
	Device(std::shared_ptr<DeviceData> state);

	arch::dma_pool *setupPool() const;
	arch::dma_pool *bufferPool() const;

	async::result<frg::expected<UsbError, std::string>> configurationDescriptor() const;
	async::result<frg::expected<UsbError, Configuration>> useConfiguration(int number) const;
	async::result<frg::expected<UsbError>> transfer(ControlTransfer info) const;

	std::shared_ptr<DeviceData> state() const {
		return _state;
	}

private:
	std::shared_ptr<DeviceData> _state;
};

// ----------------------------------------------------------------
// BaseController.
// ----------------------------------------------------------------

struct Hub;

struct BaseController {
protected:
	~BaseController() = default;

public:
	virtual async::result<void> enumerateDevice(std::shared_ptr<Hub> hub, int port, DeviceSpeed speed) = 0;
};
