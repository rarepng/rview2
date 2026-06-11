// clean little containment translation unit
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include <SpoutDX.h>

#include <iostream>
#include "obs_bridge.hpp"

namespace obs_bridge {

// inline LONG WINAPI DiagnosticVEH(EXCEPTION_POINTERS* ep) {
//     std::cerr << "EXCEPTION CODE:    0x" << std::hex << ep->ExceptionRecord->ExceptionCode << "\n";
//     std::cerr << "FAULTING ADDRESS:  0x" << std::hex << (uintptr_t)ep->ExceptionRecord->ExceptionAddress << "\n";
//     if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
//         std::cerr << "ACCESS VIOLATION:  "
//                   << (ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "READ" : "WRITE")
//                   << " at 0x" << std::hex << ep->ExceptionRecord->ExceptionInformation[1] << "\n";
//     }
//     return EXCEPTION_CONTINUE_SEARCH;
// }


static const GUID IID_IDXGIResource1_Local = {
	0x30961379, 0x4609, 0x4a41,
	{ 0x99, 0x8e, 0x54, 0xfe, 0x56, 0x7e, 0xe0, 0xc1 }
};
static ID3D11Device* g_d3d11Device = nullptr;
static ID3D11Texture2D* g_d3d11Texture = nullptr;
static ID3D12Device* g_d3d12Device = nullptr;
static ID3D12Resource* g_d3d12Texture = nullptr;
static HANDLE g_sharedHandle = nullptr;
static spoutDX* g_spout = nullptr;

static VkImportMemoryWin32HandleInfoKHR g_importWin32Info{};

void* get_vulkan_import_info(void* shared_handle) {
	g_importWin32Info.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
	g_importWin32Info.pNext = nullptr;
	g_importWin32Info.handleType = use_d3d12 ?
	                               VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT_KHR :
	                               VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
	g_importWin32Info.handle = shared_handle;
	g_importWin32Info.name = nullptr;

	return static_cast<void*>(&g_importWin32Info);
}

void* create_d3d_shared_texture(uint32_t width, uint32_t height, const uint8_t* device_luid) {
	if (!device_luid) return nullptr;

	LUID vkLuid;
	memcpy(&vkLuid, device_luid, sizeof(LUID));

	IDXGIFactory4* factory = nullptr;

	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return nullptr;

	IDXGIAdapter1* targetAdapter = nullptr;
	IDXGIAdapter1* adapter = nullptr;

	for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.AdapterLuid.LowPart == vkLuid.LowPart && desc.AdapterLuid.HighPart == vkLuid.HighPart) {
			targetAdapter = adapter;
			break;
		}

		adapter->Release();
	}

	factory->Release();

	if (!targetAdapter) {
		std::cerr << "mo adapter matching Vulkan LUID." << std::endl;
		return nullptr;
	}

	if constexpr (use_d3d12) {
		// future d3d12
		std::cerr << "d3d12 not yet implemented." << std::endl;
		targetAdapter->Release();
		return nullptr;
	} else {
		HRESULT hr = D3D11CreateDevice(
		                 targetAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
		                 0, nullptr, 0, D3D11_SDK_VERSION,
		                 &g_d3d11Device, nullptr, nullptr
		             );
		targetAdapter->Release();

		if (FAILED(hr)) {
			std::cerr << "D3D11CreateDevice failed." << std::endl;
			return nullptr;
		}

		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;

		hr = g_d3d11Device->CreateTexture2D(&desc, nullptr, &g_d3d11Texture);

		if (FAILED(hr)) {
			std::cerr << "failed to create D3D11 shared texture." << std::endl;
			return nullptr;
		}

		IDXGIResource1* resource = nullptr;
		hr = g_d3d11Texture->QueryInterface(IID_IDXGIResource1_Local, (void**)&resource);

		if (FAILED(hr)) {
			std::cerr << "IDXGIResource1 query failed." << std::endl;
			return nullptr;
		}

		hr = resource->CreateSharedHandle(
		         nullptr,
		         DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
		         nullptr,
		         &g_sharedHandle
		     );
		resource->Release();

		if (FAILED(hr)) {
			std::cerr << "CreateSharedHandle failed with: " << hr << std::endl; // RARE.. this is usually a hard segfault.
			return nullptr;
		}

		return g_sharedHandle;
	}
}

void send_spout() {
	if constexpr (use_d3d12) {
		return; // future d3d12
	} else {
		if (!g_d3d11Texture || !g_d3d11Device) return;

		if (!g_spout) {
			g_spout = new spoutDX();
			g_spout->OpenDirectX11(g_d3d11Device);
			g_spout->SetSenderName("Rview_Spout");
			std::cout << "SPOUTDX [[SUCCESS]].\n";
		}

		g_spout->SendTexture(g_d3d11Texture);
	}
}

void cleanup() {
	if (g_spout) {
		g_spout->ReleaseSender();
		delete g_spout;
		g_spout = nullptr;
	}

	if (g_sharedHandle) {
		CloseHandle(g_sharedHandle);
		g_sharedHandle = nullptr;
	}

	if (g_d3d11Texture) {
		g_d3d11Texture->Release();
		g_d3d11Texture = nullptr;
	}

	if (g_d3d11Device)  {
		g_d3d11Device->Release();
		g_d3d11Device = nullptr;
	}

	if (g_d3d12Texture) {
		g_d3d12Texture->Release();
		g_d3d12Texture = nullptr;
	}

	if (g_d3d12Device)  {
		g_d3d12Device->Release();
		g_d3d12Device = nullptr;
	}
}

//                                                                                                //
//        VVVVVVVVVVVVV[[ OLD ]](VULKAN -> DX)       (VULKAN <- DX)[[ NEW ]]^^^^^^^^^^^^^^^^^     //
//                                                                                                //


//     static ID3D12Device*   g_d3d12Device  = nullptr;
//     static ID3D12Resource* g_d3d12Texture = nullptr;
//     void* extract_vulkan_win32_handle(VkDevice device, VkDeviceMemory memory) {
//         VkMemoryGetWin32HandleInfoKHR handleInfo{
//             VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR};
//         handleInfo.memory     = memory;
//         handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT_KHR;
//         auto fp = (PFN_vkGetMemoryWin32HandleKHR)vkGetDeviceProcAddr(
//             device, "vkGetMemoryWin32HandleKHR");
//         if (!fp) {
//             std::cerr << "[Fatal] vkGetMemoryWin32HandleKHR not found — "
//                          "VK_KHR_external_memory_win32 not enabled?\n";
//             return nullptr;
//         }
//         HANDLE h = nullptr;
//         if (fp(device, &handleInfo, &h) != VK_SUCCESS) return nullptr;
//         return static_cast<void*>(h);
//     }
//     bool init_obs_bridge(
//         void*          shared_handle,
//         uint32_t       width,
//         uint32_t       height,
//         const uint8_t* device_luid)
//     {
//         AddVectoredExceptionHandler(1, DiagnosticVEH);
//         std::cerr << "VEH_INSTALLED\n";
//         if (!shared_handle || !device_luid) return false;
//         LUID vkLuid;
//         memcpy(&vkLuid, device_luid, sizeof(LUID));
//         IDXGIFactory4* factory = nullptr;
//         if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return false;
//         IDXGIAdapter1* target  = nullptr;
//         IDXGIAdapter1* adapter = nullptr;
//         for (UINT i = 0;
//              factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
//         {
//             DXGI_ADAPTER_DESC1 desc;
//             adapter->GetDesc1(&desc);
//             if (desc.AdapterLuid.LowPart  == vkLuid.LowPart &&
//                 desc.AdapterLuid.HighPart == vkLuid.HighPart) {
//                 target = adapter;
//                 break;
//             }
//             adapter->Release();
//         }
//         factory->Release();
//         if (!target) {
//             std::cerr << "[Fatal] OBS Bridge: No adapter matching Vulkan LUID.\n";
//             return false;
//         }
//         HRESULT hr = D3D12CreateDevice(
//             target, D3D_FEATURE_LEVEL_12_0, IID_ID3D12Device_Local,
//             reinterpret_cast<void**>(&g_d3d12Device));
//         target->Release();
//         UINT nodeCount = g_d3d12Device->GetNodeCount();
//         std::cerr << "D3D12 device node count: " << nodeCount << "\n";
//         if (FAILED(hr) || !g_d3d12Device) {
//             std::cerr << "[Fatal] OBS Bridge: D3D12CreateDevice failed: 0x"
//                       << std::hex << hr << "\n";
//             return false;
//         }
//         HANDLE nt_handle = static_cast<HANDLE>(shared_handle);
//         DWORD  flags     = 0;
//         if (!GetHandleInformation(nt_handle, &flags)) {
//             std::cerr << "[Fatal] OBS Bridge: NT handle invalid before OpenSharedHandle."
//                          " GetLastError=" << GetLastError() << "\n";
//             g_d3d12Device->Release();
//             g_d3d12Device = nullptr;
//             return false;
//         }
//         std::cout << "[Debug] OBS Bridge: NT handle valid, flags=0x"
//                   << std::hex << flags << "\n";
//         void** vtable = *reinterpret_cast<void***>(g_d3d12Device);
//         for (int i = 29; i <= 35; ++i) {
//             std::cerr << "vtable[" << i << "] = 0x" << std::hex << (uintptr_t)vtable[i] << "\n";
//         }
//         HRESULT nhr = E_FAIL;
//         static const GUID IID_ID3D12Heap_Explicit =
//             { 0x6b3b2502, 0x6e51, 0x45b3,
//             { 0x90, 0xee, 0x98, 0x84, 0x26, 0x5e, 0x8d, 0xf3 } };
//         ID3D12Heap* heap = nullptr;
//         nhr = g_d3d12Device->OpenSharedHandle(
//             nt_handle,
//             IID_ID3D12Heap_Explicit,
//             reinterpret_cast<void**>(&heap)
//         );
//         std::cerr << "Heap import HRESULT: 0x" << std::hex << hr << "\n";
//         if (FAILED(nhr) || !g_d3d12Texture) {
//             std::cerr << "[Fatal] OBS Bridge: OpenSharedHandle failed: 0x"
//                       << std::hex << nhr << "\n";
//             //   0x80070005  E_ACCESSDENIED
//             //   0x80070057  E_INVALIDARG
//             //   0x887a002b  DXGI_ERROR_UNSUPPORTED
//             g_d3d12Device->Release();
//             g_d3d12Device = nullptr;
//             return false;
//         }
//         std::cout << "[Success] OBS Bridge: D3D12 resource imported from Vulkan memory.\n";
//         return true;
//     }
//     static SECURITY_ATTRIBUTES            g_sa{};
//     static VkExportMemoryWin32HandleInfoKHR g_exportWin32Info{};
//     void* get_os_export_memory_info() {
//         g_sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
//         g_sa.bInheritHandle       = FALSE;           // TRUE is unnecessary and noisy
//         g_sa.lpSecurityDescriptor = nullptr;
//         g_exportWin32Info.sType       = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
//         g_exportWin32Info.pNext       = nullptr;
//         g_exportWin32Info.pAttributes = &g_sa;
//         g_exportWin32Info.dwAccess    = 0xC0000000;
//         return static_cast<void*>(&g_exportWin32Info);
//     }

}
#endif