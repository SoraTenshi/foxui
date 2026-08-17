#include "../foxui.h"

#include <math.h>

#include <d3d11.h>
#include <dxgi1_2.h>

#include <d3dcompiler.h>

#define FOXUI_D3D11_VERTEX_CAP 4096
#define FOXUI_D3D11_INDEX_CAP 8192

// todo(sora): maybe this should be in a seperate file?
const char *shader =
    "cbuffer VS_Constants : register(b0) {"
    "    float2 viewport_size;"
    "    float2 padding;"
    "};"

    "struct VS_Input {"
    "    float2 position : POSITION;"
    "    float4 color : COLOR;"
    "};"

    "struct VS_Output {"
    "    float4 position : SV_POSITION;"
    "    float4 color : COLOR;"
    "};"

    "VS_Output vs_main(VS_Input input) {"
    "    VS_Output output;"
    "    float2 p;"
    // note(sora): this just converts to NDC coordinates
    "    p.x = input.position.x / viewport_size.x * 2.0 - 1.0;"
    "    p.y = 1.0 - input.position.y / viewport_size.y * 2.0;"
    "    output.position = float4(p, 0.0, 1.0);"
    "    output.color = input.color;"
    "    return output;"
    "}"

    "float4 ps_main(VS_Output input) : SV_TARGET {"
    "    return input.color;"
    "}";
    
// struct Foxui_D3D11_Vertex {
    // f32 x, y;
    // f32 r, g, b, a;
// };

struct Foxui_D3D11_Vertex_Constants {
    f32 width;
    f32 height;
    f32 padding[2];
};

struct Foxui_D3D11 {
    ID3D11Device           *device;
    ID3D11DeviceContext    *context;
    IDXGISwapChain1        *swap_chain;
    ID3D11RenderTargetView *render_target;
    ID3D11VertexShader     *vertex_shader;
    ID3D11PixelShader      *pixel_shader;
    ID3D11InputLayout      *input_layout;
    ID3D11RasterizerState  *rasterizer_state;
    
    ID3D11Buffer           *vertex_buffer;
    ID3D11Buffer           *index_buffer;
    ID3D11Buffer           *vertex_constants;

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

FOXUI_INTERNAL bool foxui_d3d11_create_vertex_buffer(Foxui_D3D11 *d3d) {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(Foxui_Vertex) * FOXUI_D3D11_VERTEX_CAP;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT result = d3d->device->CreateBuffer(
        &desc,
        nullptr,
        &d3d->vertex_buffer
    );

    return SUCCEEDED(result);
}

FOXUI_INTERNAL bool foxui_d3d11_create_index_buffer(Foxui_D3D11 *d3d) {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(u32) * FOXUI_D3D11_INDEX_CAP;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT result = d3d->device->CreateBuffer(
        &desc,
        nullptr,
        &d3d->index_buffer
    );

    return SUCCEEDED(result);
}

FOXUI_INTERNAL bool foxui_d3d11_create_vertex_constants(Foxui_D3D11 *d3d) {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(Foxui_D3D11_Vertex_Constants);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    HRESULT result = d3d->device->CreateBuffer(
        &desc,
        nullptr,
        &d3d->vertex_constants
    );

    return SUCCEEDED(result);
}

FOXUI_INTERNAL bool foxui_d3d11_create_shaders(Foxui_D3D11 *d3d) {
    ID3DBlob *vertex_blob = nullptr;
    ID3DBlob *pixel_blob  = nullptr;
    ID3DBlob *error_blob  = nullptr;

    HRESULT result = D3DCompile(
        shader,
        strlen(shader),
        nullptr,
        nullptr,
        nullptr,
        "vs_main",
        "vs_5_0",
        0,
        0,
        &vertex_blob,
        &error_blob
    );

    if(FAILED(result)) {
        if(error_blob) {
            OutputDebugStringA((char *)error_blob->GetBufferPointer());
            error_blob->Release();
        }

        return false;
    }

    if(error_blob) {
        error_blob->Release();
        error_blob = nullptr;
    }

    result = D3DCompile(
        shader,
        strlen(shader),
        nullptr,
        nullptr,
        nullptr,
        "ps_main",
        "ps_5_0",
        0,
        0,
        &pixel_blob,
        &error_blob
    );

    if(FAILED(result)) {
        if(error_blob) {
            OutputDebugStringA((char *)error_blob->GetBufferPointer());
            error_blob->Release();
        }

        vertex_blob->Release();
        return false;
    }

    result = d3d->device->CreateVertexShader(
        vertex_blob->GetBufferPointer(),
        vertex_blob->GetBufferSize(),
        nullptr,
        &d3d->vertex_shader
    );

    if(FAILED(result)) {
        pixel_blob->Release();
        vertex_blob->Release();
        return false;
    }

    result = d3d->device->CreatePixelShader(
        pixel_blob->GetBufferPointer(),
        pixel_blob->GetBufferSize(),
        nullptr,
        &d3d->pixel_shader
    );

    if(FAILED(result)) {
        pixel_blob->Release();
        vertex_blob->Release();
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC input_elements[] = {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32_FLOAT,
            0,
            offsetof(Foxui_Vertex, point),
            D3D11_INPUT_PER_VERTEX_DATA,
            0,
        },
        {
            "COLOR",
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            0,
            offsetof(Foxui_Vertex, color),
            D3D11_INPUT_PER_VERTEX_DATA,
            0,
        },
    };

    result = d3d->device->CreateInputLayout(
        input_elements,
        FOXUI_ARRAY_COUNT(input_elements),
        vertex_blob->GetBufferPointer(),
        vertex_blob->GetBufferSize(),
        &d3d->input_layout
    );

    pixel_blob->Release();
    vertex_blob->Release();

    return SUCCEEDED(result);
}

FOXUI_INTERNAL bool foxui_d3d11_create_rasterizer_state(Foxui_D3D11 *d3d) {
    D3D11_RASTERIZER_DESC desc = {};
    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = D3D11_CULL_NONE;
    desc.ScissorEnable = TRUE;
    desc.DepthClipEnable = TRUE;

    HRESULT result = d3d->device->CreateRasterizerState(&desc, &d3d->rasterizer_state);

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
    
    RECT client_rect = {};
    GetClientRect((HWND)window->native_window, &client_rect);
    window->client_rect = Foxui_Rect{
        .left = (f32)client_rect.left,
        .top = (f32)client_rect.top,
        .right = (f32)client_rect.right,
        .bottom = (f32)client_rect.bottom,
    };
    d3d->width  = client_rect.right - client_rect.left;
    d3d->height = client_rect.bottom - client_rect.top;
    
    if(!foxui_d3d11_create_render_target(d3d)) {
        // todo(sora): add logging
        return false;
    }
    
    if(!foxui_d3d11_create_vertex_buffer(d3d)) {
        // todo(sora): logging
        return false;
    }
    
    if(!foxui_d3d11_create_index_buffer(d3d)) {
        // todo(sora): logging
        return false;
    }
    
    if(!foxui_d3d11_create_vertex_constants(d3d)) {
        // todo(sora): logging
        return false;
    }
    
    if(!foxui_d3d11_create_rasterizer_state(d3d)) {
        // todo(sora): logging
        return false;
    }
    
    if(!foxui_d3d11_create_shaders(d3d)) {
        // todo(sora): logging
        return false;
    }
    
    return true;
}

FOXUI_INTERNAL void foxui_d3d11_destroy(Foxui_D3D11 *d3d) {
    if(!d3d) return;
    
    if(d3d->context) {
        d3d->context->ClearState();
        d3d->context->Release();
    }
    if(d3d->render_target)    d3d->render_target->Release();
    if(d3d->swap_chain)       d3d->swap_chain->Release();
    if(d3d->device)           d3d->device->Release();
    if(d3d->vertex_constants) d3d->vertex_constants->Release();
    if(d3d->index_buffer)     d3d->index_buffer->Release();
    if(d3d->vertex_buffer)    d3d->vertex_buffer->Release();
    if(d3d->input_layout)     d3d->input_layout->Release();
    if(d3d->pixel_shader)     d3d->pixel_shader->Release();
    if(d3d->vertex_shader)    d3d->vertex_shader->Release();
    
    *d3d = {0};
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

FOXUI_INTERNAL void foxui_d3d11_begin_frame(Foxui_D3D11 *d3d) {
    D3D11_VIEWPORT viewport = {};
    viewport.Width = (f32)d3d->width;
    viewport.Height = (f32)d3d->height;
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;
    
    Foxui_D3D11_Vertex_Constants constants {
        .width = (f32)d3d->width,
        .height = (f32)d3d->height,
    };
    d3d->context->UpdateSubresource(d3d->vertex_constants, 0, nullptr, &constants, 0, 0);
    d3d->context->VSSetConstantBuffers(0, 1, &d3d->vertex_constants);

    d3d->context->RSSetViewports(1, &viewport);
    d3d->context->OMSetRenderTargets(1, &d3d->render_target, nullptr);
    
    d3d->context->RSSetState(d3d->rasterizer_state);

    // todo(sora): maybe not a static color? who knows lol
    f32 clear_color[] = {
        0x2a / 255.f,
        0x29 / 255.f,
        0x47 / 255.f,
        1.f,
    };

    d3d->context->ClearRenderTargetView(
        d3d->render_target,
        clear_color
    );
}

FOXUI_INTERNAL bool foxui_d3d11_submit(Foxui_D3D11 *d3d, Foxui_Draw_List *list) {
    if(list->vertex_count == 0 || list->index_count == 0) {
        return true;
    }
    
    D3D11_MAPPED_SUBRESOURCE vertices = {};
    HRESULT result = d3d->context->Map(
        d3d->vertex_buffer,
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &vertices
    );
    
    if(FAILED(result)) {
        // todo(sora): logging
        return false;
    }
    
    memcpy(vertices.pData, list->vertices, sizeof(Foxui_Vertex) * list->vertex_count);
    d3d->context->Unmap(d3d->vertex_buffer, 0);
    
    D3D11_MAPPED_SUBRESOURCE indices = {};
    result = d3d->context->Map(
        d3d->index_buffer,
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &indices
    );

    if(FAILED(result)) {
        // todo(sora): logging
        return false;
    }

    memcpy(indices.pData, list->indices, sizeof(u32) * list->index_count);
    d3d->context->Unmap(d3d->index_buffer, 0);
    
    UINT stride = sizeof(Foxui_Vertex);
    UINT offset = 0;
    
    d3d->context->IASetInputLayout(d3d->input_layout);
    d3d->context->IASetVertexBuffers(
        0,
        1,
        &d3d->vertex_buffer,
        &stride,
        &offset
    );
    d3d->context->IASetIndexBuffer(d3d->index_buffer, DXGI_FORMAT_R32_UINT, 0);
    d3d->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d3d->context->VSSetShader(d3d->vertex_shader, nullptr, 0);
    d3d->context->PSSetShader(d3d->pixel_shader, nullptr, 0);
     
    for(u32 i = 0; i < list->command_count; ++i) {
        Foxui_Draw_Command *command = &list->commands[i];

        D3D11_RECT scissor = {
            .left = (LONG)command->clip_rect.left,
            .top = (LONG)command->clip_rect.top,
            .right = (LONG)command->clip_rect.right,
            .bottom = (LONG)command->clip_rect.bottom,
        };
        d3d->context->RSSetScissorRects(1, &scissor);
        d3d->context->DrawIndexed(list->index_count, 0, 0);
    }
    
    return true;
}

FOXUI_INTERNAL bool foxui_d3d11_end_frame(
    Foxui_D3D11     *d3d,
    Foxui_Window    *window,
    Foxui_Draw_List *list
) {
    if(!foxui_d3d11_submit(d3d, list)) {
        // todo(sora):  logging
        return false;
    }
    UINT sync_interval = window->flags.is_sizing ? 0 : 1;
    HRESULT result = d3d->swap_chain->Present(sync_interval, 0);
    
    return SUCCEEDED(result);
}
