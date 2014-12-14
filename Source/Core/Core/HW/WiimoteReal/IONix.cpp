// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifndef _WIN32
#include <unistd.h>
#endif
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>
#include <libusb.h>

#include "Common/Common.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteReal
{

class WiimoteLibusb final : public Wiimote
{
public:
	WiimoteLibusb(libusb_device* device, u8 interface, u8 endpoint_in);
	~WiimoteLibusb() override;

protected:
	bool ConnectInternal() override;
	void DisconnectInternal() override;
	bool IsConnected() const override;
	void IOWakeup() override;
	int IORead(u8* buf) override;
	int IOWrite(u8 const* buf, size_t len) override;

private:
	libusb_device* m_device;
	libusb_device_handle* m_handle = nullptr;
	u8 m_interface;
	u8 m_endpoint_in;
};

WiimoteLibusb::WiimoteLibusb(libusb_device* device, u8 interface, u8 endpoint_in)
	: m_interface(interface)
	, m_endpoint_in(endpoint_in)
{
	m_device = libusb_ref_device(device);
}

WiimoteLibusb::~WiimoteLibusb()
{
	Shutdown();
	libusb_unref_device(m_device);
	m_device = nullptr;
}

bool WiimoteLibusb::ConnectInternal()
{
	libusb_open(m_device, &m_handle);
	if (!m_handle)
	{
		ERROR_LOG(WIIMOTE, "Failed to open device");
		return false;
	}

	int result = libusb_detach_kernel_driver(m_handle, m_interface);
	if (result < 0 && result != LIBUSB_ERROR_NOT_FOUND)
	{
		ERROR_LOG(WIIMOTE, "Failed to detach kernel driver (%s)", libusb_error_name(result));
		return false;
	}

	if (libusb_claim_interface(m_handle, m_interface) < 0)
	{
		ERROR_LOG(WIIMOTE, "Failed to claim interface");
		return false;
	}

	unsigned char string[64];
	if (libusb_get_string_descriptor_ascii(m_handle, 2, string, sizeof(string)) > 0)
		INFO_LOG(WIIMOTE, "Got string descriptor \"%s\"", string);

	return true;
}

void WiimoteLibusb::DisconnectInternal()
{
	if (m_handle)
	{
		libusb_release_interface(m_handle, 0);
		libusb_close(m_handle);
		m_handle = nullptr;
	}
}

bool WiimoteLibusb::IsConnected() const
{
	return !!m_handle;
}

void WiimoteLibusb::IOWakeup()
{
	ERROR_LOG(WIIMOTE, "TODO: WiimoteLibusb::IOWakeup(interface=%02x)", m_interface);
}

int WiimoteLibusb::IORead(u8* buf)
{
	int bytes_read = 0;
	int result = libusb_interrupt_transfer(m_handle, m_endpoint_in, buf + 1, MAX_PAYLOAD, &bytes_read, WIIMOTE_DEFAULT_TIMEOUT);
	if (result < 0 && result != LIBUSB_ERROR_TIMEOUT)
	{
		ERROR_LOG(WIIMOTE, "Failed to read (%s)", libusb_error_name(result));
		return -1;
	}
	buf[0] = WM_SET_REPORT | WM_BT_INPUT;
	return bytes_read + 1;
}

int WiimoteLibusb::IOWrite(const u8* buf, size_t len)
{
	_assert_(buf[0] == (WM_SET_REPORT | WM_BT_OUTPUT));

	u8 request_type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
	u8 bRequest = LIBUSB_REQUEST_SET_CONFIGURATION;
	u16 wValue = (WM_BT_OUTPUT << 8) | buf[1];
	u16 wIndex = 0;
	u8* data = const_cast<u8*>(buf + 1);
	size_t wLength = len - 1;

	int result = libusb_control_transfer(m_handle, request_type, bRequest, wValue, wIndex, data, wLength, WIIMOTE_DEFAULT_TIMEOUT);

	if (result < 0)
	{
		ERROR_LOG(WIIMOTE, "Failed to write (%s)", libusb_error_name(result));
		return -1;
	}
	if ((size_t)result < wLength)
		ERROR_LOG(WIIMOTE, "Failed to complete write");
	return result + 1;
}

void FindLibusbWiimotes(std::vector<Wiimote*>* found_wiimotes)
{
	libusb_context* context;
	libusb_init(&context);

	libusb_device** devices;
	if (libusb_get_device_list(nullptr, &devices) < 0)
	{
		ERROR_LOG(WIIMOTE, "Failed to get device list");
		return;
	}
	int i = 0;
	libusb_device* device;
	while ((device = devices[i++]))
	{
		libusb_device_descriptor device_descriptor;
		if (libusb_get_device_descriptor(device, &device_descriptor) < 0)
		{
			ERROR_LOG(WIIMOTE, "Failed to get device descriptor");
			continue;
		}
		if (device_descriptor.idVendor  == 0x057e &&
		    device_descriptor.idProduct == 0x0306)
		{
			libusb_config_descriptor* config_descriptor;
			if (libusb_get_active_config_descriptor(device, &config_descriptor) < 0)
			{
				ERROR_LOG(WIIMOTE, "Failed to get config descriptor");
				continue;
			}
			for (u8 interface_index = 0; interface_index < config_descriptor->bNumInterfaces; interface_index++)
			{
				const libusb_interface* interface = &config_descriptor->interface[interface_index];
				_assert_(interface->num_altsetting == 1);
				const libusb_interface_descriptor* interface_descriptor = &interface->altsetting[0];
				_assert_(interface_descriptor->bNumEndpoints == 1);

				u8 interface_number = interface_descriptor->bInterfaceNumber;
				u8 endpoint_in = interface_descriptor->endpoint[0].bEndpointAddress;
				_assert_(endpoint_in & LIBUSB_ENDPOINT_IN);

				NOTICE_LOG(WIIMOTE, "Found USB wiimote at device=%04x:%04x interface=%02x endpoint=%02x",
				           device_descriptor.idVendor, device_descriptor.idProduct, interface_number, endpoint_in);

				Wiimote* wiimote = new WiimoteLibusb(device, interface_number, endpoint_in);
				found_wiimotes->push_back(wiimote);
			}
			libusb_free_config_descriptor(config_descriptor);
		}
	}
	libusb_free_device_list(devices, 1);

	libusb_exit(context);
}

class WiimoteLinux final : public Wiimote
{
public:
	WiimoteLinux(bdaddr_t bdaddr);
	~WiimoteLinux() override;

protected:
	bool ConnectInternal() override;
	void DisconnectInternal() override;
	bool IsConnected() const override;
	void IOWakeup() override;
	int IORead(u8* buf) override;
	int IOWrite(u8 const* buf, size_t len) override;

private:
	bdaddr_t m_bdaddr;   // Bluetooth address
	int m_cmd_sock;      // Command socket
	int m_int_sock;      // Interrupt socket
	int m_wakeup_pipe_w;
	int m_wakeup_pipe_r;
};

WiimoteScanner::WiimoteScanner()
	: device_id(-1)
	, device_sock(-1)
{
	// Get the id of the first Bluetooth device.
	device_id = hci_get_route(nullptr);
	if (device_id < 0)
	{
		NOTICE_LOG(WIIMOTE, "Bluetooth not found.");
		return;
	}

	// Create a socket to the device
	device_sock = hci_open_dev(device_id);
	if (device_sock < 0)
	{
		ERROR_LOG(WIIMOTE, "Unable to open Bluetooth.");
		return;
	}
}

bool WiimoteScanner::IsReady() const
{
	return device_sock > 0;
}

WiimoteScanner::~WiimoteScanner()
{
	if (IsReady())
		close(device_sock);
}

void WiimoteScanner::Update()
{}

void WiimoteScanner::FindWiimotes(std::vector<Wiimote*> & found_wiimotes, Wiimote* & found_board)
{
	FindLibusbWiimotes(&found_wiimotes);

	// supposedly 1.28 seconds
	int const wait_len = 1;

	int const max_infos = 255;
	inquiry_info scan_infos[max_infos] = {};
	auto* scan_infos_ptr = scan_infos;
	found_board = nullptr;

	// Scan for Bluetooth devices
	int const found_devices = hci_inquiry(device_id, wait_len, max_infos, nullptr, &scan_infos_ptr, IREQ_CACHE_FLUSH);
	if (found_devices < 0)
	{
		ERROR_LOG(WIIMOTE, "Error searching for Bluetooth devices.");
		return;
	}

	DEBUG_LOG(WIIMOTE, "Found %i Bluetooth device(s).", found_devices);

	// Display discovered devices
	for (int i = 0; i < found_devices; ++i)
	{
		ERROR_LOG(WIIMOTE, "found a device...");

		// BT names are a maximum of 248 bytes apparently
		char name[255] = {};
		if (hci_read_remote_name(device_sock, &scan_infos[i].bdaddr, sizeof(name), name, 1000) < 0)
		{
			ERROR_LOG(WIIMOTE, "name request failed");
			continue;
		}

		ERROR_LOG(WIIMOTE, "device name %s", name);
		if (IsValidBluetoothName(name))
		{
			bool new_wiimote = true;

			// TODO: do this

			// Determine if this Wiimote has already been found.
			//for (int j = 0; j < MAX_WIIMOTES && new_wiimote; ++j)
			//{
			//	if (wm[j] && bacmp(&scan_infos[i].bdaddr,&wm[j]->bdaddr) == 0)
			//		new_wiimote = false;
			//}

			if (new_wiimote)
			{
				// Found a new device
				char bdaddr_str[18] = {};
				ba2str(&scan_infos[i].bdaddr, bdaddr_str);

				Wiimote* wm = new WiimoteLinux(scan_infos[i].bdaddr);
				if (IsBalanceBoardName(name))
				{
					found_board = wm;
					NOTICE_LOG(WIIMOTE, "Found balance board (%s).", bdaddr_str);
				}
				else
				{
					found_wiimotes.push_back(wm);
					NOTICE_LOG(WIIMOTE, "Found Wiimote (%s).", bdaddr_str);
				}
			}
		}
	}

}

WiimoteLinux::WiimoteLinux(bdaddr_t bdaddr) : Wiimote(), m_bdaddr(bdaddr)
{
	m_cmd_sock = -1;
	m_int_sock = -1;

	int fds[2];
	if (pipe(fds))
	{
		ERROR_LOG(WIIMOTE, "pipe failed");
		abort();
	}
	m_wakeup_pipe_w = fds[1];
	m_wakeup_pipe_r = fds[0];
}

WiimoteLinux::~WiimoteLinux()
{
	Shutdown();
	close(m_wakeup_pipe_w);
	close(m_wakeup_pipe_r);
}

// Connect to a Wiimote with a known address.
bool WiimoteLinux::ConnectInternal()
{
	sockaddr_l2 addr = {};
	addr.l2_family = AF_BLUETOOTH;
	addr.l2_bdaddr = m_bdaddr;
	addr.l2_cid = 0;

	// Output channel
	addr.l2_psm = htobs(WM_OUTPUT_CHANNEL);
	if ((m_cmd_sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) == -1 ||
	                  connect(m_cmd_sock, (sockaddr*)&addr, sizeof(addr)) < 0)
	{
		WARN_LOG(WIIMOTE, "Unable to open output socket to Wiimote: %s", strerror(errno));
		close(m_cmd_sock);
		m_cmd_sock = -1;
		return false;
	}

	// Input channel
	addr.l2_psm = htobs(WM_INPUT_CHANNEL);
	if ((m_int_sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) == -1 ||
	                  connect(m_int_sock, (sockaddr*)&addr, sizeof(addr)) < 0)
	{
		WARN_LOG(WIIMOTE, "Unable to open input socket from Wiimote: %s", strerror(errno));
		close(m_int_sock);
		close(m_cmd_sock);
		m_int_sock = m_cmd_sock = -1;
		return false;
	}

	return true;
}

void WiimoteLinux::DisconnectInternal()
{
	close(m_cmd_sock);
	close(m_int_sock);

	m_cmd_sock = -1;
	m_int_sock = -1;
}

bool WiimoteLinux::IsConnected() const
{
	return m_cmd_sock != -1;// && int_sock != -1;
}

void WiimoteLinux::IOWakeup()
{
	char c = 0;
	if (write(m_wakeup_pipe_w, &c, 1) != 1)
	{
		ERROR_LOG(WIIMOTE, "Unable to write to wakeup pipe.");
	}
}

// positive = read packet
// negative = didn't read packet
// zero = error
int WiimoteLinux::IORead(u8* buf)
{
	// Block select for 1/2000th of a second

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(m_int_sock, &fds);
	FD_SET(m_wakeup_pipe_r, &fds);

	if (select(m_int_sock + 1, &fds, nullptr, nullptr, nullptr) == -1)
	{
		ERROR_LOG(WIIMOTE, "Unable to select Wiimote %i input socket.", m_index + 1);
		return -1;
	}

	if (FD_ISSET(m_wakeup_pipe_r, &fds))
	{
		char c;
		if (read(m_wakeup_pipe_r, &c, 1) != 1)
		{
			ERROR_LOG(WIIMOTE, "Unable to read from wakeup pipe.");
		}
		return -1;
	}

	if (!FD_ISSET(m_int_sock, &fds))
		return -1;

	// Read the pending message into the buffer
	int r = read(m_int_sock, buf, MAX_PAYLOAD);
	if (r == -1)
	{
		// Error reading data
		ERROR_LOG(WIIMOTE, "Receiving data from Wiimote %i.", m_index + 1);

		if (errno == ENOTCONN)
		{
			// This can happen if the Bluetooth dongle is disconnected
			ERROR_LOG(WIIMOTE, "Bluetooth appears to be disconnected.  "
					"Wiimote %i will be disconnected.", m_index + 1);
		}

		r = 0;
	}

	return r;
}

int WiimoteLinux::IOWrite(u8 const* buf, size_t len)
{
	return write(m_int_sock, buf, (int)len);
}

}; // WiimoteReal
