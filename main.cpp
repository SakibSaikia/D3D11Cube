#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>

#include <array>
#include <cassert>
#include <fstream>
#include <memory>
#include <stdexcept>

#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "D3DCompiler.lib")

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct MyVertex
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT4 color;
};


struct MyConstants
{
	DirectX::XMMATRIX worldTransform;
	DirectX::XMMATRIX viewTransform;
	DirectX::XMMATRIX projectionTransform;
};

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
HWND g_wndHandle = nullptr;
Microsoft::WRL::ComPtr<ID3D11Device> g_device;
Microsoft::WRL::ComPtr<ID3D11DeviceContext> g_deviceContext;
Microsoft::WRL::ComPtr<IDXGIFactory2> g_dxgiFactory;
Microsoft::WRL::ComPtr<IDXGISwapChain1> g_swapChain;
Microsoft::WRL::ComPtr<ID3D11RenderTargetView> g_backBufferView;
Microsoft::WRL::ComPtr<ID3D11Texture2D> g_depthBuffer;
Microsoft::WRL::ComPtr<ID3D11DepthStencilView> g_depthBufferView;
Microsoft::WRL::ComPtr<ID3D11VertexShader> g_vertexShader;
Microsoft::WRL::ComPtr<ID3D11PixelShader> g_pixelShader;
Microsoft::WRL::ComPtr<ID3D11InputLayout> g_inputLayout;
Microsoft::WRL::ComPtr<ID3D11Buffer> g_vertexBuffer;
Microsoft::WRL::ComPtr<ID3D11Buffer> g_indexBuffer;
DirectX::XMMATRIX g_worldTransform;
DirectX::XMMATRIX g_viewTransform;
DirectX::XMMATRIX g_projectionTransform;

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void InitializeWindow(HINSTANCE hInstance, int nShowCmd);
int Run();
void InitializeD3D(HWND hWnd);
void DestroyD3D();
void DrawSomething(HWND hWnd);

//--------------------------------------------------------------------------------------
// Program entry point
//--------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nShowCmd)
{
	try
	{
		InitializeWindow(hInstance, nShowCmd);
		Run();
	}
	catch (std::exception& e)
	{
		::OutputDebugStringA(e.what());
	}

	return 1;
}

void InitializeWindow(HINSTANCE hInstance, int nShowCmd)
{
	WNDCLASS desc;
	desc.style = CS_HREDRAW | CS_VREDRAW;
	desc.lpfnWndProc = WndProc;
	desc.cbClsExtra = 0;
	desc.cbWndExtra = 0;
	desc.hInstance = hInstance;
	desc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	desc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	desc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW);
	desc.lpszMenuName = nullptr;
	desc.lpszClassName = L"AppWindow";

	if (!RegisterClass(&desc))
	{
		throw std::invalid_argument("*** ERROR: Failed to register WNDCLASS ***\n");
	}

	g_wndHandle = CreateWindowA(
		"AppWindow",
		"D3D11Cube",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		1280,
		720,
		nullptr,
		nullptr,
		hInstance,
		nullptr);

	if (g_wndHandle == nullptr)
	{
		throw std::invalid_argument("*** ERROR: Failed to create window ***\n");
	}

	ShowWindow(g_wndHandle, nShowCmd);
}

int Run()
{
	MSG msg = { nullptr };

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			InvalidateRect(g_wndHandle, nullptr, FALSE);
			UpdateWindow(g_wndHandle);
		}
	}

	return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		try
		{
			InitializeD3D(hWnd);
		}
		catch (std::exception& e)
		{
			DestroyD3D();
			DestroyWindow(hWnd);
			::OutputDebugStringA(e.what());
		}

		break;
	case WM_PAINT:
		DrawSomething(hWnd);
		break;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			DestroyWindow(hWnd);
		}
		break;
	case WM_DESTROY:
		DestroyD3D();
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

//--------------------------------------------------------------------------------------
// D3D Utils
//--------------------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3DBlob> LoadBlob(const std::string& filename)
{
	std::string filepath = R"(CompiledShaders\)" + filename;
	std::ifstream fileHandle(filepath, std::ios::binary);
	assert(fileHandle.good() && L"Error opening file");

	// file size
	fileHandle.seekg(0, std::ios::end);
	std::ifstream::pos_type size = fileHandle.tellg();
	fileHandle.seekg(0, std::ios::beg);

	// serialize bytecode
	Microsoft::WRL::ComPtr<ID3DBlob> blob;
	HRESULT hr = D3DCreateBlob(size, blob.GetAddressOf());
	if (hr != S_OK)
	{
		throw std::invalid_argument("*** ERROR: Failed to create shader blob ***\n");
	}

	fileHandle.read(static_cast<char*>(blob->GetBufferPointer()), size);

	fileHandle.close();

	return blob;
}

//--------------------------------------------------------------------------------------
// D3D Initialization
//--------------------------------------------------------------------------------------
void InitializeD3D(HWND hWnd)
{
	HRESULT hr;

	// Device & DeviceContext
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1 };
	hr = D3D11CreateDevice(
		nullptr, 
		D3D_DRIVER_TYPE_HARDWARE, 
		nullptr, 
		D3D11_CREATE_DEVICE_DEBUG, 
		featureLevels, 
		ARRAYSIZE(featureLevels),
		D3D11_SDK_VERSION, 
		&g_device, 
		nullptr, 
		&g_deviceContext);

	if (hr != S_OK)
	{
		throw std::invalid_argument("*** ERROR: Device creation failed ***\n");
	}

	// SwapChain
	hr = CreateDXGIFactory1(IID_PPV_ARGS(g_dxgiFactory.GetAddressOf()));

	if (hr != S_OK)
	{
		throw std::runtime_error("*** ERROR: Failed to create DXGI factory ***\n");
	}

	DXGI_SWAP_CHAIN_DESC1 sd = {};
	sd.Width = 1280;
	sd.Height = 720;
	sd.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 1;

	hr = g_dxgiFactory->CreateSwapChainForHwnd(
		g_device.Get(), 
		hWnd,
		&sd, 
		nullptr, 
		nullptr, 
		g_swapChain.GetAddressOf());

	if (hr != S_OK)
	{
		throw std::invalid_argument("*** ERROR: Failed to create swap chain ***\n");
	}

	// Back buffer
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	hr = g_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
	if (hr != S_OK)
	{
		throw std::runtime_error("*** ERROR: Failed to get back buffer ***\n");
	}

	hr = g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, g_backBufferView.GetAddressOf());
	if (hr != S_OK)
	{
		throw std::runtime_error("*** ERROR: Failed create back buffer view ***\n");
	}

	// DepthBuffer
	D3D11_TEXTURE2D_DESC descDepth = {};
	descDepth.Width = 1280;
	descDepth.Height = 720;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_device->CreateTexture2D(&descDepth, nullptr, g_depthBuffer.GetAddressOf());
	if (hr != S_OK)
	{
		throw std::runtime_error("*** ERROR: Failed create depth buffer ***\n");
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_device->CreateDepthStencilView(g_depthBuffer.Get(), &descDSV, g_depthBufferView.GetAddressOf());
	if (hr != S_OK)
	{
		throw std::runtime_error("*** ERROR: Failed create depth buffer view ***\n");
	}

	// Shaders
	Microsoft::WRL::ComPtr<ID3DBlob> vsBlob = LoadBlob("vertex_shader.cso");
	hr = g_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, g_vertexShader.GetAddressOf());
	if (hr != S_OK)
	{
		throw std::runtime_error("*** ERROR: Failed create vertex shader ***\n");
	}

	Microsoft::WRL::ComPtr<ID3DBlob> psBlob = LoadBlob("pixel_shader.cso");
	hr = g_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, g_pixelShader.GetAddressOf());
	if (hr != S_OK)
	{
		throw std::runtime_error("*** ERROR: Failed create pixel shader ***\n");
	}

	// VertexBuffer
	using namespace DirectX;
	MyVertex cubeVerts[] =
	{
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) },
	};

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(cubeVerts);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = cubeVerts;
	hr = g_device->CreateBuffer(&bd, &initData, g_vertexBuffer.GetAddressOf());
	if (hr != S_OK)
	{
		throw std::runtime_error("*** ERROR: Failed create vertex buffer ***\n");
	}

	// IndexBuffer
	WORD indices[] =
	{
		3,1,0,
		2,1,3,

		0,5,4,
		1,5,0,

		3,4,7,
		0,4,3,

		1,6,5,
		2,6,1,

		2,7,6,
		3,7,2,

		6,4,5,
		7,4,6,
	};

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(indices);
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	initData.pSysMem = indices;
	hr = g_device->CreateBuffer(&bd, &initData, g_indexBuffer.GetAddressOf());
	if (hr != S_OK)
	{
		throw std::runtime_error("*** ERROR: Failed create index buffer ***\n");
	}

	// InputLayout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	hr = g_device->CreateInputLayout(
		layout,
		ARRAYSIZE(layout),
		vsBlob->GetBufferPointer(),
		vsBlob->GetBufferSize(),
		g_inputLayout.GetAddressOf());

	if (hr != S_OK)
	{
		throw std::runtime_error("*** ERROR: Failed create input layout ***\n");
	}
}

//--------------------------------------------------------------------------------------
// Main Draw
//--------------------------------------------------------------------------------------
void DrawSomething(HWND hWnd)
{
	// set viewport
	D3D11_VIEWPORT viewport = { 1280.f, 720.f, 0.f, 1.f, 0, 0 };
	g_deviceContext->RSSetViewports(1, &viewport);

	// set back buffer
	ID3D11RenderTargetView* renderTargets[] = { g_backBufferView.Get() };
	g_deviceContext->OMSetRenderTargets(ARRAYSIZE(renderTargets), renderTargets, g_depthBufferView.Get());

	// clear
	g_deviceContext->ClearRenderTargetView(g_backBufferView.Get(), DirectX::Colors::DimGray);

	// bind geometry
	ID3D11Buffer* vertexBuffers[] = { g_vertexBuffer.Get() };
	UINT vertexBufferStrides[] = { sizeof(MyVertex) };
	UINT vertexBufferOffsets[] = { 0 };
	g_deviceContext->IASetVertexBuffers(0, ARRAYSIZE(vertexBuffers), vertexBuffers, vertexBufferStrides, vertexBufferOffsets);
	g_deviceContext->IASetIndexBuffer(g_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

	// Set the input layout
	g_deviceContext->IASetInputLayout(g_inputLayout.Get());

	// set topology
	g_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// bind shaders
	g_deviceContext->VSSetShader(g_vertexShader.Get(), nullptr, 0);
	g_deviceContext->PSSetShader(g_pixelShader.Get(), nullptr, 0);

	// w00t
	g_deviceContext->DrawIndexed(36 /*12 tris*/, 0, 0);

	// present to display!
	g_swapChain->Present(0, 0);
}

//--------------------------------------------------------------------------------------
// D3D Cleanup
//--------------------------------------------------------------------------------------
void DestroyD3D()
{
	g_device.Reset();
	g_deviceContext.Reset();
	g_dxgiFactory.Reset();
	g_swapChain.Reset();
	g_backBufferView.Reset();
	g_depthBuffer.Reset();
	g_depthBufferView.Reset();
	g_vertexShader.Reset();
	g_pixelShader.Reset();
	g_inputLayout.Reset();
	g_vertexBuffer.Reset();
	g_indexBuffer.Reset();
}