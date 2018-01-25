/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "d3d11_common.h"

#include <dynamic/dylib.h>

static dylib_t d3d11_dll;

HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
      IDXGIAdapter*   pAdapter,
      D3D_DRIVER_TYPE DriverType,
      HMODULE         Software,
      UINT            Flags,
      CONST D3D_FEATURE_LEVEL* pFeatureLevels,
      UINT                     FeatureLevels,
      UINT                     SDKVersion,
      CONST DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
      IDXGISwapChain**            ppSwapChain,
      ID3D11Device**              ppDevice,
      D3D_FEATURE_LEVEL*          pFeatureLevel,
      ID3D11DeviceContext**       ppImmediateContext)
{
   static PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN fp;

   if (!d3d11_dll)
      d3d11_dll = dylib_load("d3d11.dll");

   if (!d3d11_dll)
      return TYPE_E_CANTLOADLIBRARY;

   if (!fp)
      fp = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)dylib_proc(
            d3d11_dll, "D3D11CreateDeviceAndSwapChain");

   if (!fp)
      return TYPE_E_CANTLOADLIBRARY;

   return fp(
         pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
         pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
}

void d3d11_init_texture(D3D11Device device, d3d11_texture_t* texture)
{
   Release(texture->handle);
   Release(texture->staging);
   Release(texture->view);

   //   .Usage = D3D11_USAGE_DYNAMIC,
   //   .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,

   texture->desc.MipLevels          = 1;
   texture->desc.ArraySize          = 1;
   texture->desc.SampleDesc.Count   = 1;
   texture->desc.SampleDesc.Quality = 0;
   texture->desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
   texture->desc.CPUAccessFlags =
         texture->desc.Usage == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0;
   texture->desc.MiscFlags = 0;
   D3D11CreateTexture2D(device, &texture->desc, NULL, &texture->handle);

   {
      D3D11_SHADER_RESOURCE_VIEW_DESC view_desc = {
         .Format                    = texture->desc.Format,
         .ViewDimension             = D3D_SRV_DIMENSION_TEXTURE2D,
         .Texture2D.MostDetailedMip = 0,
         .Texture2D.MipLevels       = -1,
      };
      D3D11CreateTexture2DShaderResourceView(device, texture->handle, &view_desc, &texture->view);
   }

   {
      D3D11_TEXTURE2D_DESC desc = texture->desc;
      desc.BindFlags            = 0;
      desc.Usage                = D3D11_USAGE_STAGING;
      desc.CPUAccessFlags       = D3D11_CPU_ACCESS_WRITE;
      D3D11CreateTexture2D(device, &desc, NULL, &texture->staging);
   }
}

void d3d11_update_texture(
      D3D11DeviceContext ctx,
      int                width,
      int                height,
      int                pitch,
      DXGI_FORMAT        format,
      const void*        data,
      d3d11_texture_t*   texture)
{
   D3D11_MAPPED_SUBRESOURCE mapped_texture;

   D3D11MapTexture2D(ctx, texture->staging, 0, D3D11_MAP_WRITE, 0, &mapped_texture);

   dxgi_copy(
         width, height, format, pitch, data, texture->desc.Format, mapped_texture.RowPitch,
         mapped_texture.pData);

   D3D11UnmapTexture2D(ctx, texture->staging, 0);

   if (texture->desc.Usage == D3D11_USAGE_DEFAULT)
      texture->dirty = true;
}

DXGI_FORMAT
d3d11_get_closest_match(D3D11Device device, DXGI_FORMAT desired_format, UINT desired_format_support)
{
   DXGI_FORMAT* format = dxgi_get_format_fallback_list(desired_format);
   UINT         format_support;
   while (*format != DXGI_FORMAT_UNKNOWN)
   {
      if (SUCCEEDED(D3D11CheckFormatSupport(device, *format, &format_support)) &&
          ((format_support & desired_format_support) == desired_format_support))
         break;
      format++;
   }
   assert(*format);
   return *format;
}
