#include "../foxui.h"

#include <d3d11.h>
#include <dxgi1_2.h>

struct Foxui_D3D11 {
    ID3D11Device           *device;
    ID3D11DeviceContext    *context;
    IDXGISwapChain1        *swap_chain;
    ID3D11RenderTargetView *render_target;
    
    s32 width;
    s32 height;
};

#if defined(FOXUI_DEBUG)
#define D3D11_DEBUG D3D11_CREATE_DEVICE_DEBUG
#else
#define D3D11_DEBUG 0
#endif

FOXUI_INTERNAL bool foxui_d3d11_create_device(Foxui_D3D11 *d3d) {
    UINT flags = 0;
    flags |= D3D11_DEBUG;
    
    D3D_FEATURE_LEVEL feature_levels[] = {
        // note(sora): apparently, on older machines 11_1 might make problems, causing
        //             E_INVALIDARG... so if this *actually* becomes a problem, make sure
        //             to maybe just use 11_0
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    
    D3D_FEATURE_LEVEL selected_feature_level = {};
    HRESULT result = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        feature_levels,
        FOXUI_ARRAY_COUNT(feature_levels),
        D3D11_SDK_VERSION,
        &d3d->device,
        &selected_feature_level,
        &d3d->context
    );
    
    return SUCCEEDED(result);
}

FOXUI_INTERNAL bool foxui_d3d11_create_swap_chain(Foxui_D3D11 *d3d, HWND hwnd) {
    IDXGIDevice   *dxgi_device = nullptr;
    IDXGIAdapter  *adapter     = nullptr;
    IDXGIFactory2 *factory     = nullptr;
    
    HRESULT result = d3d->device->QueryInterface(
        __uuidof(IDXGIDevice),
        (void **)&dxgi_device
    );
    
    if (FAILED(result)) {
        // todo(sora): introduce logging
        return false;
    }
    
    result = dxgi_device->GetAdapter(&adapter);
        if (FAILED(result)) {
        // todo(sora): introduce logging
        dxgi_device->Release();
        return false;
    }
    
    result = adapter->GetParent(
        __uuidof(IDXGIFactory2),
        (void **)&factory
    );
    
    if (FAILED(result)) {
        // todo(sora): introduce logging
        adapter->Release();
        dxgi_device->Release();
        return false;
    }
    
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    // note(sora): swap chain -> display surface.
    //             shader and stuff can still use floating point.
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    
    // note(sora): msdn says don't use `CreateSwapChain` anymore
    result = factory->CreateSwapChainForHwnd(
        d3d->device,
        hwnd,
        &desc,
        nullptr,
        nullptr,
        &d3d->swap_chain
    );
    
    // todo(sora): probably we need to introduce a defer macro... 
    //             clearing up COM objects is causing me brain aneurysm
    factory->Release();
    adapter->Release();
    dxgi_device->Release();
    
    return SUCCEEDED(result);
}

FOXUI_INTERNAL bool foxui_d3d11_create_render_target(Foxui_D3D11 *d3d) {
    ID3D11Texture2D *back_buffer = nullptr;
    
    HRESULT result = d3d->swap_chain->GetBuffer(
        0,
        __uuidof(ID3D11Texture2D),
        (void **)&back_buffer
    );
    
    if(FAILED(result)) {
        return false;
    }
    
    result = d3d->device->CreateRenderTargetView(
        back_buffer,
        nullptr,
        &d3d->render_target
    );
    
    back_buffer->Release();
    return SUCCEEDED(result);
}

FOXUI_INTERNAL bool foxui_d3d11_create(Foxui_D3D11 *d3d, Foxui_Window *window) {
    // todo(sora): perhaps, those should be asserts instead.
    if(!d3d || !window || !window->native_window) {
        return false;
    }
    
    if(!foxui_d3d11_create_device(d3d)) {
        // todo(sora): add logging
        return false;
    }
    
    if(!foxui_d3d11_create_swap_chain(d3d, (HWND)window->native_window)) {
        // todo(sora): add logging
        return false;
    }
    
    if(!foxui_d3d11_create_render_target(d3d)) {
        // todo(sora): add logging
        return false;
    }
    
    return true;
}

FOXUI_INTERNAL void foxui_d3d11_destroy(Foxui_D3D11 *d3d) {
    if(!d3d) return;
    
    if(d3d->render_target) d3d->render_target->Release();
    if(d3d->swap_chain) d3d->swap_chain->Release();
    if(d3d->context) {
        d3d->context->ClearState();
        d3d->context->Release();
    }
    if(d3d->device) d3d->device->Release();
    
    *d3d = {0};
}

FOXUI_INTERNAL void foxui_d3d11_draw(Foxui_D3D11 *d3d) {
    D3D11_VIEWPORT viewport = {};
    viewport.Width = (f32)d3d->width;
    viewport.Height = (f32)d3d->height;
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;
    
    d3d->context->RSSetViewports(1, &viewport);
    d3d->context->OMSetRenderTargets(1, &d3d->render_target, nullptr);
    
    f32 clear_color[4] = {
        0x2a / 255.f,
        0x29 / 255.f,
        0x47 / 255.f,
        1.f
    };
    
    d3d->context->ClearRenderTargetView(d3d->render_target, clear_color);
    d3d->swap_chain->Present(1, 0);
}

FOXUI_INTERNAL bool foxui_d3d11_resize(Foxui_D3D11 *d3d, s32 width, s32 height) {
    // todo(sora): perhaps, those should be asserts instead.
    if(!d3d || !d3d->swap_chain || width == 0 || height == 0) {
        return false;
    }
    
    d3d->context->OMSetRenderTargets(0, nullptr, nullptr);
    
    if(d3d->render_target) {
        d3d->render_target->Release();
        d3d->render_target = nullptr;
    }
    
    HRESULT result = d3d->swap_chain->ResizeBuffers(
        0,
        width,
        height,
        DXGI_FORMAT_UNKNOWN,
        0
    );
    
    if(FAILED(result)) {
        // todo(sora): logging
        return false;
    }
    
    d3d->width = width;
    d3d->height = height;
    
    return foxui_d3d11_create_render_target(d3d);
}
